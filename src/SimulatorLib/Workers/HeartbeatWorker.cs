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
    /// 心跳 Worker：为每个已注册客户端维护一条独立的长连接 TCP socket，
    /// 周期性发送心跳包。每个 Task 独占自己的 TcpClient，无需共享，无需加锁。
    /// 连接断开或发送失败  立即 Dispose  下轮循环重新连接。
    /// </summary>
    public class HeartbeatWorker
    {
        private readonly IUdpSender? _udpSender;

        public HeartbeatWorker(INetworkSender tcpSender, IUdpSender? udpSender = null)
        {
            _udpSender = udpSender;
        }

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

            var successTcp = 0;
            var failTcp    = 0;
            var successUdp = 0;
            var failUdp    = 0;

            // 错峰启动：1000 个客户端的首次 connect 在 spreadMs 内均匀错开，
            // 避免瞬间发起大量并发 TCP 连接压垮服务端 accept 队列。
            int spreadMs = Math.Min(intervalMs, 3000);

            var clientTasks = new List<Task>(clients.Count);
            for (int i = 0; i < clients.Count; i++)
            {
                var c = clients[i];
                int startDelay = clients.Count > 1 ? (int)((long)i * spreadMs / clients.Count) : 0;

                clientTasks.Add(Task.Run(async () =>
                {
                    // 错峰延迟
                    if (startDelay > 0)
                    {
                        try { await Task.Delay(startDelay, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { return; }
                    }

                    // 每个 Task 独占自己的 socket，绝对不跨 Task 共享，无需加锁
                    TcpClient?     tcp    = null;
                    NetworkStream? stream = null;

                    while (!ct.IsCancellationRequested)
                    {
                        try
                        {
                            //  1. 确保连接存活 
                            if (tcp == null || !tcp.Connected)
                            {
                                tcp?.Dispose();
                                stream = null;

                                int tcpPort  = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                var newTcp   = new TcpClient { NoDelay = true };

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

                            //  3. 非阻塞 drain ACK，防止服务端发送缓冲区堆积 
                            if (stream.DataAvailable)
                            {
                                var drainBuf = new byte[256];
                                while (stream.DataAvailable)
                                    await stream.ReadAsync(drainBuf, 0, drainBuf.Length, ct).ConfigureAwait(false);
                            }

                            Interlocked.Increment(ref successTcp);

                            //  4. 可选 UDP 心跳 
                            if (useLogServer && _udpSender != null && !string.IsNullOrEmpty(logHost))
                            {
                                var udpOk = await _udpSender.SendUdpAsync(logHost, logPort, payload, 1500, ct).ConfigureAwait(false);
                                if (udpOk) Interlocked.Increment(ref successUdp);
                                else       Interlocked.Increment(ref failUdp);
                            }
                        }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested)
                        {
                            break;
                        }
                        catch
                        {
                            // 连接失败或发送失败  丢弃 socket，下轮自动重连
                            tcp?.Dispose();
                            tcp    = null;
                            stream = null;
                            Interlocked.Increment(ref failTcp);
                        }

                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }
                    }

                    // 退出时清理
                    tcp?.Dispose();

                }, ct));
            }

            // 统计上报：每个报告周期恰好 Exchange 一次，与心跳发送严格对齐
            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(Math.Max(1000, intervalMs), ct).ConfigureAwait(false); }
                    catch { break; }

                    var stats = new HeartbeatStats(
                        clients.Count,
                        Interlocked.Exchange(ref successTcp, 0),
                        Interlocked.Exchange(ref failTcp, 0),
                        Interlocked.Exchange(ref successUdp, 0),
                        Interlocked.Exchange(ref failUdp, 0));
                    try { progress?.Report(stats); } catch { }
                }
            }, ct);

            await Task.WhenAll(clientTasks).ConfigureAwait(false);
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
