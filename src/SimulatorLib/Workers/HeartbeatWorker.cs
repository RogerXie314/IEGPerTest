using System;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Protocol;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 Worker：每个客户端独占一个 Task + TcpClient，无共享，无锁。
    /// 失败时 Dispose socket，下轮自动重连。
    /// 统计上报的是"当前轮次在线数"而非累计计数，避免跨窗口重叠计数。
    /// </summary>
    public class HeartbeatWorker
    {
        private readonly IUdpSender? _udpSender;

        public HeartbeatWorker(INetworkSender tcpSender, IUdpSender? udpSender = null)
        {
            _udpSender = udpSender;
        }

        /// <summary>
        /// SuccessTcp / FailTcp 表示上一个报告周期内各自的发送次数（gauge）。
        /// </summary>
        public record HeartbeatStats(int Total, int SuccessTcp, int FailTcp, int SuccessUdp, int FailUdp);

        public async Task StartAsync(
            int intervalMs,
            bool useLogServer,
            string platformHost,
            int platformPort,
            string? logHost,
            int logPort,
            int concurrency,
            CancellationToken ct,
            IProgress<HeartbeatStats>? progress = null)
        {
            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            if (clients.Count == 0) return;

            // 每个客户端用一个 int 记录上轮结果：1=成功，0=失败，-1=未发送
            // 报告任务直接统计这个数组，不受时间窗口对齐影响
            var lastResult = new int[clients.Count];
            for (int j = 0; j < lastResult.Length; j++) lastResult[j] = -1;

            var lastUdpResult = new int[clients.Count];
            for (int j = 0; j < lastUdpResult.Length; j++) lastUdpResult[j] = -1;

            // 错峰启动：在 spreadMs 内均匀错开首次连接，避免瞬间 N 个并发 TCP 连接
            int spreadMs = Math.Min(intervalMs, 3000);

            var clientTasks = new List<Task>(clients.Count);
            for (int i = 0; i < clients.Count; i++)
            {
                var c       = clients[i];
                var idx     = i;
                int startDelay = clients.Count > 1 ? (int)((long)i * spreadMs / clients.Count) : 0;

                clientTasks.Add(Task.Run(async () =>
                {
                    if (startDelay > 0)
                    {
                        try { await Task.Delay(startDelay, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { return; }
                    }

                    TcpClient?     tcp    = null;
                    NetworkStream? stream = null;

                    while (!ct.IsCancellationRequested)
                    {
                        try
                        {
                            //  1. 检查连接是否存活（含服务端关闭检测） 
                            if (tcp == null || IsPeerClosed(tcp))
                            {
                                tcp?.Dispose();
                                stream = null;
                                tcp    = null;

                                int tcpPort = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                var newTcp  = new TcpClient { NoDelay = true };

                                using var connCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                                connCts.CancelAfter(3000);
                                await newTcp.ConnectAsync(platformHost, tcpPort, connCts.Token).ConfigureAwait(false);

                                tcp    = newTcp;
                                stream = tcp.GetStream();
                            }

                            //  2. 构建并发送心跳包 
                            var domainName = GetDomainNameSafe();
                            var mac        = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(c.IP);
                            var json       = HeartbeatJsonBuilder.BuildV3R7C02(c.ClientId, domainName, c.IP, mac);
                            var jsonBytes  = TrimTrailingNewline(Encoding.UTF8.GetBytes(json));
                            var payload    = PtProtocol.Pack(jsonBytes, cmdId: 1, deviceId: c.DeviceId);

                            using var sendCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                            sendCts.CancelAfter(5000);
                            await stream!.WriteAsync(payload, 0, payload.Length, sendCts.Token).ConfigureAwait(false);
                            await stream.FlushAsync(sendCts.Token).ConfigureAwait(false);

                            //  3. 非阻塞 drain ACK 
                            if (stream.DataAvailable)
                            {
                                var drainBuf = new byte[256];
                                while (stream.DataAvailable)
                                    await stream.ReadAsync(drainBuf, 0, drainBuf.Length, ct).ConfigureAwait(false);
                            }

                            lastResult[idx] = 1; // 本轮成功

                            //  4. 可选 UDP 心跳 
                            if (useLogServer && _udpSender != null && !string.IsNullOrEmpty(logHost))
                            {
                                var udpOk = await _udpSender.SendUdpAsync(logHost, logPort, payload, 1500, ct).ConfigureAwait(false);
                                lastUdpResult[idx] = udpOk ? 1 : 0;
                            }
                        }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested)
                        {
                            break;
                        }
                        catch
                        {
                            // 连接或发送失败  丢弃 socket，下轮重连
                            tcp?.Dispose();
                            tcp    = null;
                            stream = null;
                            lastResult[idx] = 0; // 本轮失败
                        }

                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }
                    }

                    tcp?.Dispose();
                }, ct));
            }

            // 统计上报：直接扫描 lastResult 数组，反映"当前有多少客户端上次心跳成功"
            // 与心跳发送时机完全解耦，不存在窗口对齐问题
            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(Math.Max(1000, intervalMs), ct).ConfigureAwait(false); }
                    catch { break; }

                    int ok = 0, fail = 0, udpOk = 0, udpFail = 0;
                    for (int j = 0; j < lastResult.Length; j++)
                    {
                        if      (lastResult[j] == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                        if      (lastUdpResult[j] == 1) udpOk++;
                        else if (lastUdpResult[j] == 0) udpFail++;
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, ok, fail, udpOk, udpFail)); } catch { }
                }
            }, ct);

            await Task.WhenAll(clientTasks).ConfigureAwait(false);
        }

        /// <summary>
        /// 检测对端是否已关闭连接：Poll(SelectRead)+Available==0 是 .NET 中
        /// 检测服务端发送 FIN（半关闭）的标准方法。
        /// TcpClient.Connected 只反映上次 I/O 状态，不能依赖。
        /// </summary>
        private static bool IsPeerClosed(TcpClient tcp)
        {
            if (!tcp.Connected) return true;
            try
            {
                var s = tcp.Client;
                // Poll(0, SelectRead): 若有数据可读 OR 连接关闭则返回 true
                // Available == 0 且 Poll 为 true 意味着对端已关闭（无数据，但连接标记关闭）
                return s.Poll(0, SelectMode.SelectRead) && s.Available == 0;
            }
            catch { return true; }
        }

        private static byte[] TrimTrailingNewline(byte[] bytes)
        {
            if (bytes.Length >= 2 && bytes[^2] == (byte)'\r' && bytes[^1] == (byte)'\n')
                return bytes.AsSpan(0, bytes.Length - 2).ToArray();
            if (bytes.Length >= 1 && bytes[^1] == (byte)'\n')
                return bytes.AsSpan(0, bytes.Length - 1).ToArray();
            return bytes;
        }

        private static string GetDomainNameSafe()
        {
            try { return Environment.UserDomainName ?? string.Empty; }
            catch { return string.Empty; }
        }
    }
}
