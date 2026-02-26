using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
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
        /// Connected     = 当前持有有效 TCP 连接的客户端数
        /// SuccessTcp    = 上一报告周期内发送成功次数（WriteAsync 未抛异常）
        /// FailTcp       = 上一报告周期内发送失败次数（包含连接失败）
        /// ServerReplied = 最近 3个周期内有服务端回包的客户端数
        ///                 （滚动时间窗口：超过 3×intervalMs 没回包就计为「未回包」，
        ///                  可发现平台静默踢人而 TCP 不断的情况）
        /// RsnServerClosed/WriteFailed/ConnFailed/ConnTimeout = 各断线原因客户端数
        /// </summary>
        public record HeartbeatStats(
            int Total, int Connected, int SuccessTcp, int FailTcp,
            int SuccessUdp, int FailUdp, int ServerReplied = 0,
            int RsnServerClosed = 0, int RsnWriteFailed = 0,
            int RsnConnFailed = 0, int RsnConnTimeout = 0);

        // 断线原因代码（存入 lastReason[] 数组，供监控日志读取）
        static class Reason
        {
            public const int Ok             = 0; // 正常在线
            public const int ServerClosed   = 1; // 服务端主动关闭（FIN，drain n==0）
            public const int WriteFailed    = 2; // WriteAsync 抛异常
            public const int ConnectFailed  = 3; // ConnectAsync 抛异常（含拒绝连接）
            public const int ConnectTimeout = 4; // ConnectAsync 3s 超时
        }

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

            // lastReplyTimeMs[i]: 最近一次收到服务端回包的 Unix毫秒时间戳。0=未收到过。
            // 统计时判断最近 3×intervalMs 内是否有回包，超过窗口就计为「未回包」。
            // 这能发现平台静默踢人而 TCP 不断的情况（之前的 int 标志会将这种情况误报为回包）。
            var lastReplyTimeMs = new long[clients.Count];

            // lastReason[i]: 最近一次断线/失败原因（见 Reason 常量）
            var lastReason = new int[clients.Count];

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
                    bool           alive  = false; // drain Task 检测到 n=0 时置 false，主循环据此重连

                    while (!ct.IsCancellationRequested)
                    {
                        //  1. 建立连接（连接不存在，或 drain 检测到服务端关闭时）
                        if (tcp == null || !alive)
                        {
                            tcp?.Dispose();
                            tcp    = null;
                            stream = null;
                            alive  = false;
                            Interlocked.Exchange(ref connectedFlags[idx], 0);
                            Interlocked.Exchange(ref lastReplyTimeMs[idx], 0L); // 重建连接时清零回包时间戳

                            try
                            {
                                int tcpPort = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                var newTcp  = new TcpClient { NoDelay = true };
                                newTcp.Client.SetSocketOption(
                                    SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                                using var connCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                                connCts.CancelAfter(3000);
                                await newTcp.ConnectAsync(platformHost, tcpPort, connCts.Token).ConfigureAwait(false);

                                tcp    = newTcp;
                                stream = tcp.GetStream();
                                alive  = true;
                                Interlocked.Exchange(ref connectedFlags[idx], 1);

                                // 后台 drain：读取并丢弃服务端推送（对应原始 C++ HeartBeatRecvThread）。
                                // 当服务端主动关闭连接时（n==0），置 alive=false，
                                // 主循环下一轮检测到后会重连。
                                var drainStream  = stream;
                                var capturedIdx  = idx;
                                _ = Task.Run(async () =>
                                {
                                    var buf = new byte[4096];
                                    try
                                    {
                                        while (!ct.IsCancellationRequested)
                                        {
                                            int n = await drainStream.ReadAsync(buf, 0, buf.Length, ct).ConfigureAwait(false);
                                            if (n == 0) { alive = false; lastReason[capturedIdx] = Reason.ServerClosed; break; } // 服务端 FIN
                                            // n > 0：服务端有数据返回。记录回包时间戳，统计时以滚动窗口判断是否连续在回包。
                                            Interlocked.Exchange(ref lastReplyTimeMs[capturedIdx],
                                                DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                        }
                                    }
                                    catch { alive = false; }
                                }, ct);
                            }
                            catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
                            catch (OperationCanceledException)
                            {
                                // ConnectAsync 3s 超时（CancelAfter 触发）
                                lastResult[idx]  = 0;
                                lastReason[idx]  = Reason.ConnectTimeout;
                                // 抖动上限 min(intervalMs/2, 5000)：避免大量客户端同时重连冲击服务端，
                                // 同时不超过 5s，保证断线后能快速恢复，不依赖 intervalMs 的长短。
                                int jitter;
                                lock (_rng) { jitter = _rng.Next(0, Math.Min(intervalMs / 2, 5000)); }
                                try { await Task.Delay(jitter, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { return; }
                                continue;
                            }
                            catch
                            {
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.ConnectFailed;
                                // 连接失败：随机抖动后重试（同上限 5s）
                                int jitter;
                                lock (_rng) { jitter = _rng.Next(0, Math.Min(intervalMs / 2, 5000)); }
                                try { await Task.Delay(jitter, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { return; }
                                continue;
                            }
                        }

                        //  2. 发送心跳包
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

                            lastResult[idx] = 1;
                            lastReason[idx] = Reason.Ok;

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
                            // 写失败 = 连接已死，下一轮重连（不立即 continue，先等 intervalMs）
                            tcp?.Dispose();
                            tcp    = null;
                            stream = null;
                            alive  = false;
                            Interlocked.Exchange(ref connectedFlags[idx], 0);
                            lastResult[idx] = 0;
                            lastReason[idx] = Reason.WriteFailed;
                        }

                        // 等待下一个心跳周期
                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }
                    }

                    tcp?.Dispose();
                }, ct));
            }

            // 监控日志：定时写文件，记录离线数量与断线原因分布（供开发分析）
            int monitorIntervalMs = Math.Max(60_000, intervalMs * 10);
            var monitorFile = Path.Combine(AppContext.BaseDirectory,
                $"heartbeat_monitor_{DateTime.Now:yyyyMMdd_HHmmss}.log");
            var monitorTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(monitorIntervalMs, ct).ConfigureAwait(false); }
                    catch { break; }

                    int total = clients.Count;
                    int conn = 0, ok = 0, fail = 0, replied = 0;
                    int rServerClosed = 0, rWriteFailed = 0, rConnFailed = 0, rConnTimeout = 0;
                    long replyWindowMs2 = (long)intervalMs * 3;
                    long nowMs2 = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                    for (int j = 0; j < total; j++)
                    {
                        if (connectedFlags[j] == 1) conn++;
                        if (lastResult[j]     == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                        long t2 = Interlocked.Read(ref lastReplyTimeMs[j]);
                        if (t2 > 0 && (nowMs2 - t2) <= replyWindowMs2) replied++;

                        if (connectedFlags[j] == 0)
                        {
                            switch (lastReason[j])
                            {
                                case Reason.ServerClosed:   rServerClosed++;   break;
                                case Reason.WriteFailed:    rWriteFailed++;    break;
                                case Reason.ConnectFailed:  rConnFailed++;     break;
                                case Reason.ConnectTimeout: rConnTimeout++;    break;
                            }
                        }
                    }
                    int offline = total - conn;

                    var sb = new StringBuilder();
                    sb.AppendLine($"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] 心跳监控");
                    sb.AppendLine($"  总客户端:{total}  已连接:{conn}  离线:{offline}  已发送:{ok}  发送失败:{fail}");
                    sb.AppendLine($"  平台回包({replyWindowMs2/1000}s窗口):{replied}  TCP连接中但未回包:{conn - replied}←平台静默踢局则在此是现");
                    if (offline > 0)
                    {
                        sb.AppendLine($"  断线原因 | 服务端关闭:{rServerClosed}  写入失败:{rWriteFailed}  连接失败:{rConnFailed}  连接超时:{rConnTimeout}");
                    }
                    sb.AppendLine();

                    try { await File.AppendAllTextAsync(monitorFile, sb.ToString(), ct).ConfigureAwait(false); }
                    catch { /* 写文件失败不影响主流程 */ }
                }
            }, ct);

            // 统计上报：扫描 lastResult / connectedFlags 数组（gauge，无累计溢出）
            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(Math.Max(1000, intervalMs), ct).ConfigureAwait(false); }
                    catch { break; }

                    int conn = 0, ok = 0, fail = 0, udpOk = 0, udpFail = 0, replied = 0;
                    int rSC = 0, rWF = 0, rCF = 0, rCT = 0;
                    long replyWindowMs = (long)intervalMs * 3; // 3个周期内有回包才算在线
                    long nowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                    for (int j = 0; j < clients.Count; j++)
                    {
                        if (connectedFlags[j] == 1)   conn++;
                        if (lastResult[j]     == 1)   ok++;
                        else if (lastResult[j] == 0)  fail++;
                        if (lastUdpResult[j]  == 1)   udpOk++;
                        else if (lastUdpResult[j]==0)  udpFail++;
                        // 仅当最近 3×intervalMs 内收到过回包，才计入 replied
                        long t = Interlocked.Read(ref lastReplyTimeMs[j]);
                        if (t > 0 && (nowMs - t) <= replyWindowMs) replied++;
                        // 仅统计真正离线（未连接）客户端的断线原因
                        if (connectedFlags[j] == 0)
                        {
                            switch (lastReason[j])
                            {
                                case Reason.ServerClosed:   rSC++; break;
                                case Reason.WriteFailed:    rWF++; break;
                                case Reason.ConnectFailed:  rCF++; break;
                                case Reason.ConnectTimeout: rCT++; break;
                            }
                        }
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, conn, ok, fail, udpOk, udpFail, replied, rSC, rWF, rCF, rCT)); } catch { }
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
