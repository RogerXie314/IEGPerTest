using System;
using System.Collections.Concurrent;
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
    /// 实现逻辑与原始 C++ 完全对齐（参考 WLTcpConnect.cpp / WLHeartBeat.cpp）：
    ///   - 长连接，写失败立即 Dispose，下轮重连（对应原始 CloseSocket → CreateSocket）
    ///   - 不依赖 Connected 属性（不可靠），不用 Poll（时序不稳）
    ///   - 开启 TCP KeepAlive，由 OS 自动探测网络层死连接
    ///   - 连接超时 2s（对应原始 stcTime.tv_sec = 2）
    ///   - 连接失败/超时：等待一个心跳周期 intervalMs 后重试（原始靠定时器间隔天然节流）
    ///   - 无额外 jitter / reconnectNotBefore 机制——心跳间隔本身就是重连节奏
    /// </summary>
    public class HeartbeatWorker
    {
        private readonly IUdpSender? _udpSender;
        private readonly PolicyReceiveWorker? _policyWorker;
        private readonly HeartbeatStreamRegistry? _streamRegistry;

        public HeartbeatWorker(INetworkSender tcpSender, IUdpSender? udpSender = null, PolicyReceiveWorker? policyWorker = null, HeartbeatStreamRegistry? streamRegistry = null)
        {
            _udpSender      = udpSender;
            _policyWorker   = policyWorker;
            _streamRegistry = streamRegistry;
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
            int RsnConnFailed = 0, int RsnConnTimeout = 0,
            int RsnSessionStale = 0, int RsnLockBusy = 0);

        // 断线原因代码（存入 lastReason[] 数组，供监控日志读取）
        static class Reason
        {
            public const int Ok             = 0; // 正常在线
            public const int ServerClosed   = 1; // 服务端主动关闭（FIN，drain n==0）
            public const int WriteFailed    = 2; // WriteAsync 抛异常
            public const int ConnectFailed  = 3; // ConnectAsync 抛异常（含拒绝连接）
            public const int ConnectTimeout = 4; // ConnectAsync 3s 超时
            public const int SessionStale   = 5; // 平台静默踢除 session（TCP 不断但不再回包）
            public const int LockBusy       = 6; // 写锁被 LogWorker 占用超时，跳过本次心跳（连接仍然有效）
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

            // 事件驱动日志队列：断连发生时立即入队，监控任务异步写文件（不等下次轮询）
            var eventQueue = new ConcurrentQueue<string>();

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
                    long           connectedAtMs = 0L; // 本次连接建立时的时间戳，用于 session 超时检测

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
                                connCts.CancelAfter(2000); // 2s，对应原始 stcTime.tv_sec = 2
                                await newTcp.ConnectAsync(platformHost, tcpPort, connCts.Token).ConfigureAwait(false);

                                tcp    = newTcp;
                                stream = tcp.GetStream();
                                alive  = true;
                                connectedAtMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                                Interlocked.Exchange(ref connectedFlags[idx], 1);
                                // 注册到共享表，供威胁检测 LogWorker 复用此 stream（对齐原项目 SetWLTcpConnectInstance）
                                _streamRegistry?.Register(c.ClientId, stream);

                                // 后台 drain：读取服务端推送（对应原始 C++ HeartBeatRecvThread）。
                                // 若配置了 PolicyReceiveWorker，则解析下行命令并回包；
                                // 否则仅丢弃数据并记录回包时间戳。
                                var drainStream      = stream;
                                var capturedIdx      = idx;
                                var capturedC        = c;
                                var capturedPolicyWorker = _policyWorker;
                                var capturedRegistry = _streamRegistry;
                                _ = Task.Run(async () =>
                                {
                                    try
                                    {
                                        if (capturedPolicyWorker != null)
                                        {
                                            // 带策略解析的接收模式：每次 ReadAsync 收到数据时，通过回调实时更新回包时间戳
                                            bool serverAlive = await capturedPolicyWorker.ProcessStreamAsync(
                                                drainStream, capturedC.ClientId, capturedC.DeviceId, ct,
                                                onDataReceived: () => Interlocked.Exchange(
                                                    ref lastReplyTimeMs[capturedIdx],
                                                    DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()))
                                                .ConfigureAwait(false);
                                            if (!serverAlive)
                                            {
                                                alive = false;
                                                lastReason[capturedIdx] = Reason.ServerClosed;
                                                capturedRegistry?.Unregister(capturedC.ClientId);
                                                eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 服务端关闭(PolicyDrain) 客户端#{capturedIdx} {capturedC.ClientId}");
                                            }
                                        }
                                        else
                                        {
                                            // 原始 drain 模式：仅读取并丢弃，记录时间戳
                                            var buf = new byte[4096];
                                            while (!ct.IsCancellationRequested)
                                            {
                                                int n = await drainStream.ReadAsync(buf, 0, buf.Length, ct).ConfigureAwait(false);
                                                if (n == 0)
                                                {
                                                    alive = false;
                                                    lastReason[capturedIdx] = Reason.ServerClosed;
                                                    capturedRegistry?.Unregister(capturedC.ClientId);
                                                    eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 服务端关闭(RawDrain) 客户端#{capturedIdx} {capturedC.ClientId}");
                                                    break;
                                                }
                                                Interlocked.Exchange(ref lastReplyTimeMs[capturedIdx],
                                                    DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                            }
                                        }
                                    }
                                    catch { alive = false; capturedRegistry?.Unregister(capturedC.ClientId); }
                                }, ct);
                            }
                            catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
                            catch (OperationCanceledException)
                            {
                                // ConnectAsync 2s 超时（CancelAfter 触发）
                                // 对应原始：connect timeout → 下一个心跳周期重试，节奏由 intervalMs 控制
                                lastResult[idx]  = 0;
                                lastReason[idx]  = Reason.ConnectTimeout;
                                try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { return; }
                                continue;
                            }
                            catch
                            {
                                // 连接被拒绝 / 网络不通：等一个心跳周期后重试（与原始行为一致）
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.ConnectFailed;
                                try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
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

                            // 加写锁：防止与 LogWorker 并发写同一 NetworkStream 导致包体互相穿插损坏
                            bool lockAcq = _streamRegistry != null
                                ? await _streamRegistry.AcquireWriteLockAsync(c.ClientId, 4000, sendCts.Token).ConfigureAwait(false)
                                : true; // 无 registry 时不需要额外锁
                            if (!lockAcq)
                            {
                                // ★ 写锁被 LogWorker 占用超时：TCP 连接本身依然有效，不能 Dispose。
                                // 跳过本次心跳，等下一个周期再试。
                                // （原来 throw TimeoutException 会被 catch 误判为写失败而 Dispose TCP，
                                //   导致 HeartbeatWorker 重连失败而永久离线，但 LogWorker 降级独立 TCP 继续发送。）
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.LockBusy;
                                try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { break; }
                                continue;
                            }
                            try
                            {
                                await stream!.WriteAsync(payload, 0, payload.Length, sendCts.Token).ConfigureAwait(false);
                                await stream.FlushAsync(sendCts.Token).ConfigureAwait(false);
                            }
                            finally
                            {
                                _streamRegistry?.ReleaseWriteLock(c.ClientId, lockAcq);
                            }

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
                            _streamRegistry?.Unregister(c.ClientId);
                            lastResult[idx] = 0;
                            lastReason[idx] = Reason.WriteFailed;
                            eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 写入失败(WriteFailed) 客户端#{idx} {c.ClientId}");
                        }

                        // 等待下一个心跳周期
                        try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { break; }

                        //  4. Session 超时检测：平台静默踢人而 TCP 不断的场景
                        //  如果 TCP 仍连接，但自本次连接建立后一直收不到平台回包，
                        //  说明平台 session 已失效（被踢或连接没经过认证）。
                        //  主动断开重连，否则即使其他主机释放了槽位，我们也永远占着僵尸连接出不来。
                        if (alive)
                        {
                            long staleMs   = Math.Max((long)intervalMs * 3, 30_000L);
                            long nowCheck  = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                            long lastReply = Interlocked.Read(ref lastReplyTimeMs[idx]);
                            // refTime: 收到过回包就用最后回包时间，否则用连接建立时间
                            long refTime   = lastReply > 0 ? lastReply : connectedAtMs;
                            if (refTime > 0 && (nowCheck - refTime) > staleMs)
                            {
                                // Session 被平台静默踢出：主动关闭 TCP，下轮会重新连接
                                tcp?.Dispose();
                                tcp    = null;
                                stream = null;
                                alive  = false;
                                Interlocked.Exchange(ref connectedFlags[idx], 0);
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.SessionStale;
                                _streamRegistry?.Unregister(c.ClientId);
                                eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 平台静默踢session 客户端#{idx} {c.ClientId} 上次回包:{(Interlocked.Read(ref lastReplyTimeMs[idx]) > 0 ? DateTimeOffset.FromUnixTimeMilliseconds(Interlocked.Read(ref lastReplyTimeMs[idx])).LocalDateTime.ToString("HH:mm:ss") : "从未")} 连接建立:{DateTimeOffset.FromUnixTimeMilliseconds(connectedAtMs).LocalDateTime:HH:mm:ss}");
                            }
                        }
                    }

                    tcp?.Dispose();
                }, ct));
            }

            // 监控日志：定时写文件，记录离线数量与断线原因分布（供开发分析）
            // 间隔：max(30s, intervalMs)，确保即使心跳间隔30s、测试刚开始3分钟内也能捕获异常
            int monitorIntervalMs = Math.Max(30_000, intervalMs);
            var monitorFile = Path.Combine(AppContext.BaseDirectory,
                $"heartbeat_monitor_{DateTime.Now:yyyyMMdd_HHmmss}.log");
            var monitorTask = Task.Run(async () =>
            {
                int prevReplied = -1; // 上一周期的回包数，用于检测骤降
                int prevConn    = -1;
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(monitorIntervalMs, ct).ConfigureAwait(false); }
                    catch { break; }

                    int total = clients.Count;
                    int conn = 0, ok = 0, fail = 0, replied = 0;
                    int rServerClosed = 0, rWriteFailed = 0, rConnFailed = 0, rConnTimeout = 0, rSessionStale = 0, rLockBusy = 0;
                    long replyWindowMs2 = (long)intervalMs * 3;
                    long nowMs2 = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                    for (int j = 0; j < total; j++)
                    {
                        if (connectedFlags[j] == 1) conn++;
                        if (lastResult[j]     == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                        long t2 = Interlocked.Read(ref lastReplyTimeMs[j]);
                        if (t2 > 0 && (nowMs2 - t2) <= replyWindowMs2) replied++;

                        switch (lastReason[j])
                        {
                            case Reason.ServerClosed:   if (connectedFlags[j] == 0) rServerClosed++;   break;
                            case Reason.WriteFailed:    if (connectedFlags[j] == 0) rWriteFailed++;    break;
                            case Reason.ConnectFailed:  if (connectedFlags[j] == 0) rConnFailed++;     break;
                            case Reason.ConnectTimeout: if (connectedFlags[j] == 0) rConnTimeout++;    break;
                            case Reason.SessionStale:   if (connectedFlags[j] == 0) rSessionStale++;   break;
                            case Reason.LockBusy:       rLockBusy++; break; // 连接有效但本次心跳被跳过，单独统计（不要求一定离线）
                        }
                    }
                    int offline = total - conn;

                    // 平台回包率骤降检测
                    int silentNow  = conn - replied; // 已连接但未回包数量
                    int silentPrev = prevConn >= 0 ? (prevConn - prevReplied) : -1;
                    string replyTrend = "";
                    if (prevReplied >= 0 && conn > 0)
                    {
                        int drop = prevReplied - replied; // 回包数下降量（正数=下降）
                        if (drop >= 10)
                            replyTrend = $" ⚠⚠ 回包骤降{drop}(上周期:{prevReplied}→本周期:{replied})";
                        else if (drop > 0)
                            replyTrend = $" ↓{drop}(上周期:{prevReplied})";
                        else if (drop < 0)
                            replyTrend = $" ↑{-drop}(上周期:{prevReplied})";
                    }

                    var sb = new StringBuilder();
                    // 先输出累积的事件驱动断连记录
                    while (eventQueue.TryDequeue(out var ev))
                        sb.AppendLine(ev);

                    sb.AppendLine($"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] 心跳监控");
                    sb.AppendLine($"  总客户端:{total}  已连接:{conn}  离线:{offline}  已发送:{ok}  发送失败:{fail}");
                    sb.AppendLine($"  平台回包({replyWindowMs2/1000}s窗口):{replied}{replyTrend}  TCP连接中但未回包:{silentNow}{(silentPrev >= 0 ? $"(上周期:{silentPrev})" : "")}←平台静默踢出则数量偏高");
                    // 始终输出 LockBusy（0也输出），方便确认"fix没有副作用"
                    sb.AppendLine($"  锁竞争跳过(LockBusy):{rLockBusy}{(rLockBusy > 0 ? " ⚠ 心跳被跳过但连接有效，属正常" : " ✓")}");
                    if (offline > 0)
                    {
                        sb.AppendLine($"  断线原因 | 服务端关闭:{rServerClosed}  写入失败:{rWriteFailed}  连接失败:{rConnFailed}  连接超时:{rConnTimeout}  平台踢session:{rSessionStale}");
                    }
                    sb.AppendLine();

                    prevReplied = replied;
                    prevConn    = conn;

                    try { await File.AppendAllTextAsync(monitorFile, sb.ToString(), ct).ConfigureAwait(false); }
                    catch { /* 写文件失败不影响主流程 */ }
                }
            }, ct);

            // 统计上报：扫描 lastResult / connectedFlags 数组（gauge，无累计溢出）
            // 固定 2s 刷新，与心跳间隔解耦：心跳间隔再长（如 30s），UI 也能保持实时响应。
            var reportTask = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(2000, ct).ConfigureAwait(false); }
                    catch { break; }

                    int conn = 0, ok = 0, fail = 0, udpOk = 0, udpFail = 0, replied = 0;
                    int rSC = 0, rWF = 0, rCF = 0, rCT = 0, rSS = 0, rLB = 0;
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
                        switch (lastReason[j])
                        {
                            case Reason.ServerClosed:   if (connectedFlags[j] == 0) rSC++; break;
                            case Reason.WriteFailed:    if (connectedFlags[j] == 0) rWF++; break;
                            case Reason.ConnectFailed:  if (connectedFlags[j] == 0) rCF++; break;
                            case Reason.ConnectTimeout: if (connectedFlags[j] == 0) rCT++; break;
                            case Reason.SessionStale:   if (connectedFlags[j] == 0) rSS++; break;
                            case Reason.LockBusy:       rLB++; break; // 连接有效但心跳被跳过
                        }
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, conn, ok, fail, udpOk, udpFail, replied, rSC, rWF, rCF, rCT, rSS, rLB)); } catch { }
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
                    try { await Task.Delay(2000, ct).ConfigureAwait(false); }
                    catch { break; }

                    int ok = 0, fail = 0;
                    for (int j = 0; j < clients.Count; j++)
                    {
                        if      (lastResult[j] == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                    }
                    // HTTPS 心跳：用 Connected 字段表示当前在线（响应成功）数量
                    try { progress?.Report(new HeartbeatStats(clients.Count, ok, 0, 0, 0, 0)); } catch { }
                }
            }, ct);

            await Task.WhenAll(clientTasks).ConfigureAwait(false);
        }
    }
}
