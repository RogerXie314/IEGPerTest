using System;
using System.Collections.Generic;
using System.Net.Http;
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
    /// 实现逻辑与原始 C++ 完全对齐：
    ///   - 长连接，写失败立即 Dispose，下轮重连（原始 CloseSocket  CreateSocket）
    ///   - 不依赖 Connected 属性（不可靠），不用 Poll（时序不稳）
    ///   - 开启 TCP KeepAlive，由 OS 自动探测网络层死连接
    ///   - 重连时加随机抖动，避免大量连接同时断开后同时重连形成冲击波
    /// </summary>
    public class HeartbeatWorker
    {
        private readonly IUdpSender? _udpSender;
        private static readonly Random _rng = new();

        public HeartbeatWorker(INetworkSender tcpSender, IUdpSender? udpSender = null)
        {
            _udpSender = udpSender;
        }

        /// <summary>
        /// Connected  = 当前持有有效 TCP 连接的客户端数
        /// SuccessTcp = 上一报告周期内发送成功次数（gauge，上限 = Total）
        /// FailTcp    = 上一报告周期内发送失败次数（包含连接失败）
        /// </summary>
        public record HeartbeatStats(int Total, int Connected, int SuccessTcp, int FailTcp, int SuccessUdp, int FailUdp);

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

            // lastResult[i]: 1=上轮成功, 0=上轮失败, -1=尚未发送
            var lastResult    = new int[clients.Count];
            var lastUdpResult = new int[clients.Count];
            for (int j = 0; j < clients.Count; j++) { lastResult[j] = -1; lastUdpResult[j] = -1; }

            // connectedFlags[i]: 1=当前有活跃 TCP 连接, 0=断开中
            var connectedFlags = new int[clients.Count];

            // 错峰启动：在 spreadMs 内均匀错开首次连接
            int spreadMs = Math.Min(intervalMs, 3000);

            var clientTasks = new List<Task>(clients.Count);
            for (int i = 0; i < clients.Count; i++)
            {
                var c          = clients[i];
                var idx        = i;
                int startDelay = clients.Count > 1 ? (int)((long)i * spreadMs / clients.Count) : 0;

                clientTasks.Add(Task.Run(async () =>
                {
                    // 错峰初始延迟
                    if (startDelay > 0)
                    {
                        try { await Task.Delay(startDelay, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { return; }
                    }

                    TcpClient?     tcp    = null;
                    NetworkStream? stream = null;

                    while (!ct.IsCancellationRequested)
                    {
                        //  1. 建立连接（若尚未连接）
                        if (tcp == null)
                        {
                            try
                            {
                                int tcpPort = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                var newTcp  = new TcpClient { NoDelay = true };

                                // TCP KeepAlive：由 OS 探测网络层死连接（约 2 分钟后触发）
                                newTcp.Client.SetSocketOption(
                                    SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);

                                using var connCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                                connCts.CancelAfter(3000); // 与原始 C++ select 超时 2s 对齐
                                await newTcp.ConnectAsync(platformHost, tcpPort, connCts.Token).ConfigureAwait(false);

                                tcp    = newTcp;
                                stream = tcp.GetStream();
                                Interlocked.Exchange(ref connectedFlags[idx], 1);

                                // 后台接收 drain：持续读取并丢弃服务端推送数据（策略、命令、ACK 等），
                                // 防止服务端 TCP 发送缓冲区满而阻塞，导致平台无法更新心跳状态。
                                // 与原始 C++ HeartBeatRecvThread 等价（只 drain，不处理命令）。
                                var drainStream = stream;
                                _ = Task.Run(async () =>
                                {
                                    var buf = new byte[4096];
                                    try
                                    {
                                        while (!ct.IsCancellationRequested)
                                        {
                                            int n = await drainStream.ReadAsync(buf, 0, buf.Length, ct).ConfigureAwait(false);
                                            if (n == 0) break; // 服务端主动关闭
                                        }
                                    }
                                    catch { /* ObjectDisposedException = tcp被Dispose；OperationCanceledException = 停止 */ }
                                }, ct);
                            }
                            catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
                            catch
                            {
                                // 连接失败：标记失败，随机抖动后重试（避免雷同重连）
                                tcp?.Dispose();
                                tcp    = null;
                                stream = null;
                                Interlocked.Exchange(ref connectedFlags[idx], 0);
                                lastResult[idx] = 0;

                                // 随机抖动 0~intervalMs，防止所有失败连接同时重试
                                int jitter;
                                lock (_rng) { jitter = _rng.Next(0, intervalMs); }
                                try { await Task.Delay(jitter, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { return; }
                                continue; // 重新尝试连接，不等完整 intervalMs
                            }
                        }

                        //  2. 发送心跳包（连接存在就直接发，失败即视为连接死亡）
                        try
                        {
                            var domainName = GetDomainNameSafe();
                            var mac        = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(c.IP);
                            var json       = HeartbeatJsonBuilder.BuildV3R7C02(c.ClientId, domainName, c.IP, mac);
                            var jsonBytes  = TrimTrailingNewline(Encoding.UTF8.GetBytes(json));
                            var payload    = PtProtocol.Pack(jsonBytes, cmdId: 1, deviceId: c.DeviceId);

                            using var sendCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                            sendCts.CancelAfter(5000);
                            await stream!.WriteAsync(payload, 0, payload.Length, sendCts.Token).ConfigureAwait(false);
                            await stream.FlushAsync(sendCts.Token).ConfigureAwait(false);

                            lastResult[idx] = 1; // 本轮成功

                            //  3. 可选 UDP 心跳 
                            if (useLogServer && _udpSender != null && !string.IsNullOrEmpty(logHost))
                            {
                                var payload2 = PtProtocol.Pack(jsonBytes, cmdId: 1, deviceId: c.DeviceId);
                                var udpOk    = await _udpSender.SendUdpAsync(logHost, logPort, payload2, 1500, ct).ConfigureAwait(false);
                                lastUdpResult[idx] = udpOk ? 1 : 0;
                            }
                        }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested) { break; }
                        catch
                        {
                            // 发送失败 = 连接已死（与原始 C++ SendData 失败逻辑相同）
                            // Dispose  下轮从步骤1重新 Connect
                            tcp?.Dispose();
                            tcp    = null;
                            stream = null;
                            Interlocked.Exchange(ref connectedFlags[idx], 0);
                            lastResult[idx] = 0;
                            // 注意：直接 continue，不等 intervalMs，立即重连
                            continue;
                        }

                        // 等待下一个心跳周期
                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }
                    }

                    tcp?.Dispose();
                }, ct));
            }

            // 统计上报：扫描 lastResult / connectedFlags 数组（gauge，无累计溢出）
            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(Math.Max(1000, intervalMs), ct).ConfigureAwait(false); }
                    catch { break; }

                    int conn = 0, ok = 0, fail = 0, udpOk = 0, udpFail = 0;
                    for (int j = 0; j < clients.Count; j++)
                    {
                        if (connectedFlags[j] == 1)   conn++;
                        if (lastResult[j]     == 1)   ok++;
                        else if (lastResult[j] == 0)  fail++;
                        if (lastUdpResult[j]  == 1)   udpOk++;
                        else if (lastUdpResult[j]==0)  udpFail++;
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, conn, ok, fail, udpOk, udpFail)); } catch { }
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

        // ──────────────────────────────────────────────────────────────────────
        // HTTPS 心跳：POST https://{host}:{port}/USM/clientHeartbeat.do
        // 不经过 PT 协议加密，用于诊断 TCP 心跳是否是协议/加密问题。
        // ──────────────────────────────────────────────────────────────────────
        public async Task StartHttpsAsync(
            int intervalMs,
            string platformHost,
            int platformPort,
            CancellationToken ct,
            IProgress<HeartbeatStats>? progress = null)
        {
            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            if (clients.Count == 0) return;

            var lastResult = new int[clients.Count];
            for (int j = 0; j < clients.Count; j++) lastResult[j] = -1;

            int spreadMs = Math.Min(intervalMs, 3000);

            var handler = new HttpClientHandler
            {
                ServerCertificateCustomValidationCallback =
                    HttpClientHandler.DangerousAcceptAnyServerCertificateValidator,
            };
            using var http = new HttpClient(handler)
            {
                Timeout = TimeSpan.FromMilliseconds(Math.Max(intervalMs, 10000)),
            };
            var url = $"https://{platformHost}:{platformPort}/USM/clientHeartbeat.do";

            var clientTasks = new List<Task>(clients.Count);
            for (int i = 0; i < clients.Count; i++)
            {
                var c   = clients[i];
                var idx = i;
                int startDelay = clients.Count > 1
                    ? (int)((long)i * spreadMs / clients.Count) : 0;

                clientTasks.Add(Task.Run(async () =>
                {
                    if (startDelay > 0)
                    {
                        try { await Task.Delay(startDelay, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { return; }
                    }

                    while (!ct.IsCancellationRequested)
                    {
                        try
                        {
                            var domainName = GetDomainNameSafe();
                            var mac        = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(c.IP);
                            var json       = HeartbeatJsonBuilder.BuildV3R7C02(
                                                c.ClientId, domainName, c.IP, mac);

                            using var content = new StringContent(
                                json, Encoding.UTF8, "application/json");
                            using var resp = await http.PostAsync(url, content, ct)
                                               .ConfigureAwait(false);

                            lastResult[idx] = resp.IsSuccessStatusCode ? 1 : 0;
                        }
                        catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
                        catch { lastResult[idx] = 0; }

                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }
                    }
                }, ct));
            }

            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(Math.Max(1000, intervalMs), ct).ConfigureAwait(false); }
                    catch { break; }

                    int ok = 0, fail = 0;
                    for (int j = 0; j < clients.Count; j++)
                    {
                        if      (lastResult[j] == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, 0, ok, fail, 0, 0)); } catch { }
                }
            }, ct);

            await Task.WhenAll(clientTasks).ConfigureAwait(false);
        }
    }
}
