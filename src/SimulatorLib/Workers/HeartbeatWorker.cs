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
            IProgress<HeartbeatStats>? progress = null,
            string? osVersion = null)
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
            // 专用事件日志队列：实时写入 heartbeat-events-YYYYMMDD.log（500ms 刷新，独立于 monitor 快照）
            var hbEventLog = new ConcurrentQueue<string>();
            // 每客户端最近断线时间戳（ms），用于计算 RECONNECT 延迟
            var evtDisconnAtMs = new long[clients.Count];

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
                    bool           hasConnectedBefore = false; // 区分首次 CONNECT 与 RECONNECT

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
                                // 事件日志：首次 CONNECT 或断线后 RECONNECT
                                {
                                    long latMs = hasConnectedBefore && evtDisconnAtMs[idx] > 0
                                        ? DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() - evtDisconnAtMs[idx] : -1;
                                    hbEventLog.Enqueue($"{DateTime.UtcNow:o} {(hasConnectedBefore ? "RECONNECT" : "CONNECT  ")} client={c.ClientId}{(latMs >= 0 ? $" latencyMs={latMs}" : "")}");
                                    hasConnectedBefore = true;
                                }

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
                                                Interlocked.Exchange(ref evtDisconnAtMs[capturedIdx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                                hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={capturedC.ClientId} reason=服务端关闭");
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
                                                    Interlocked.Exchange(ref evtDisconnAtMs[capturedIdx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                                    hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={capturedC.ClientId} reason=服务端关闭");
                                                    break;
                                                }
                                                Interlocked.Exchange(ref lastReplyTimeMs[capturedIdx],
                                                    DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                            }
                                        }
                                    }
                                    catch (Exception exDrain)
                                    {
                                        alive = false;
                                        capturedRegistry?.Unregister(capturedC.ClientId);
                                        Interlocked.Exchange(ref evtDisconnAtMs[capturedIdx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                        // 记录具体异常类型，区分网络错误/ObjectDisposed/OperationCanceled
                                        string drainReason = exDrain is ObjectDisposedException ? "连接已释放" : exDrain.GetType().Name;
                                        hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={capturedC.ClientId} reason=drain异常:{drainReason}");
                                    }
                                }, ct);
                            }
                            catch (OperationCanceledException) when (ct.IsCancellationRequested) { return; }
                            catch (OperationCanceledException)
                            {
                                // ConnectAsync 2s 超时（CancelAfter 触发）
                                // 对应原始：connect timeout → 下一个心跳周期重试，节奏由 intervalMs 控制
                                lastResult[idx]  = 0;
                                lastReason[idx]  = Reason.ConnectTimeout;
                                Interlocked.Exchange(ref evtDisconnAtMs[idx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={c.ClientId} reason=连接超时");
                                try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { return; }
                                continue;
                            }
                            catch
                            {
                                // 连接被拒绝 / 网络不通：等一个心跳周期后重试（与原始行为一致）
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.ConnectFailed;
                                Interlocked.Exchange(ref evtDisconnAtMs[idx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={c.ClientId} reason=连接失败");
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
                            var json       = HeartbeatJsonBuilder.BuildV3R7C02(c.ClientId, domainName, c.IP, mac, osVersion);
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
                                // 不跳过整个心跳周期（老工具 send() 由 OS 排队，心跳包一定发出）：
                                // 短暂等待 200ms 让 LogWorker 完成写操作后，立即补发心跳。
                                // 补发失败才按 WriteFailed 处理（而非静默跳过）。
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.LockBusy;
                                try { await Task.Delay(200, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { break; }
                                // 等待后重试获取锁（500ms），写完即释放
                                bool retryLock = _streamRegistry != null
                                    ? await _streamRegistry.AcquireWriteLockAsync(c.ClientId, 500, ct).ConfigureAwait(false)
                                    : true;
                                if (!retryLock)
                                {
                                    // 仍抢不到：本次心跳确实跳过，等下一周期，但不等完整 intervalMs
                                    continue;
                                }
                                // 重新赋值 lockAcq 供 finally 块正确释放
                                lockAcq = retryLock;
                                // 继续走下方 try { WriteAsync } finally { ReleaseWriteLock }
                            }
                            try
                            {
                                await stream!.WriteAsync(payload, 0, payload.Length, sendCts.Token).ConfigureAwait(false);
                                await stream.FlushAsync(sendCts.Token).ConfigureAwait(false);
                                // 防止 LogWorker 因 write_ex 调用 Unregister 后 TCP 仍存活、
                                // 导致注册表永久空缺（stream_null 持续至连接自然死亡）。
                                // 必须在 ReleaseWriteLock 之前注册：确保锁释放时注册表已有值，
                                // 等锁的 LogWorker 拿到锁后能立即 TryGet 成功，消除竞态窗口。
                                _streamRegistry?.Register(c.ClientId, stream!);
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
                            Interlocked.Exchange(ref evtDisconnAtMs[idx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                            hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={c.ClientId} reason=写入失败");
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
                                // Session 被平台静默踢出：主动关闭 TCP，下轮会重新连接。
                                // 加随机 jitter（0~5s）打散重连潮：同批注册的客户端可能同时触发
                                // SessionStale，若同时重连则 270 个 ConnectAsync 并发冲击平台，
                                // 导致更多拒绝 → 雪崩。
                                tcp?.Dispose();
                                tcp    = null;
                                stream = null;
                                alive  = false;
                                Interlocked.Exchange(ref connectedFlags[idx], 0);
                                lastResult[idx] = 0;
                                lastReason[idx] = Reason.SessionStale;
                                _streamRegistry?.Unregister(c.ClientId);
                                // snapshot 一次，避免 drain task 并发修改导致双重读取结果不一致
                                long snapReply = Interlocked.Read(ref lastReplyTimeMs[idx]);
                                string snapReplyStr = snapReply > 0
                                    ? DateTimeOffset.FromUnixTimeMilliseconds(snapReply).LocalDateTime.ToString("HH:mm:ss")
                                    : "从未";
                                eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 平台静默踢session 客户端#{idx} {c.ClientId} 上次回包:{snapReplyStr} 连接建立:{DateTimeOffset.FromUnixTimeMilliseconds(connectedAtMs).LocalDateTime:HH:mm:ss}");
                                Interlocked.Exchange(ref evtDisconnAtMs[idx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={c.ClientId} reason=平台踢session 上次回包:{snapReplyStr}");
                                // 随机 jitter 0~5000ms，打散多客户端同时重连对平台的冲击
                                int reconnJitter = Random.Shared.Next(0, 5000);
                                if (reconnJitter > 0)
                                {
                                    try { await Task.Delay(reconnJitter, ct).ConfigureAwait(false); }
                                    catch (OperationCanceledException) { break; }
                                }
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

            // 事件日志任务：实时将 hbEventLog 写入 heartbeat-events-YYYYMMDD.log（500ms 刷新）
            var hbEventLogFile = Path.Combine(AppContext.BaseDirectory, $"heartbeat-events-{DateTime.UtcNow:yyyyMMdd}.log");
            _ = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { await Task.Delay(500, ct).ConfigureAwait(false); }
                    catch { break; }
                    if (!hbEventLog.IsEmpty)
                    {
                        var esb = new StringBuilder();
                        while (hbEventLog.TryDequeue(out var ev)) esb.AppendLine(ev);
                        try { await File.AppendAllTextAsync(hbEventLogFile, esb.ToString(), CancellationToken.None).ConfigureAwait(false); } catch { }
                    }
                }
                // 取消后 drain 残留事件，确保最后一批不丢失
                if (!hbEventLog.IsEmpty)
                {
                    var esb = new StringBuilder();
                    while (hbEventLog.TryDequeue(out var ev)) esb.AppendLine(ev);
                    try { File.AppendAllText(hbEventLogFile, esb.ToString()); } catch { }
                }
            });

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
        // 不经过 PT 协议加密，用于 Linux 客户端场景。
        // 若 useLogServer=true 且 udpSender 不为 null，则每次 HTTPS 成功后
        // 同时以 UDP 将相同 payload 发往日志服务器（与 TCP 心跳的附加 UDP 逻辑对齐）。
        // ──────────────────────────────────────────────────────────────────────
        public async Task StartHttpsAsync(
            int intervalMs,
            string platformHost,
            int platformPort,
            CancellationToken ct,
            IProgress<HeartbeatStats>? progress = null,
            string? osVersion = null,
            bool useLogServer = false,
            string? logHost = null,
            int logPort = 0,
            IUdpSender? udpSender = null)
        {
            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            if (clients.Count == 0) return;

            var lastResult    = new int[clients.Count];
            var lastUdpResult = new int[clients.Count];
            for (int j = 0; j < clients.Count; j++) { lastResult[j] = -1; lastUdpResult[j] = -1; }

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
                                                c.ClientId, domainName, c.IP, mac, osVersion);

                            using var content = new StringContent(
                                json, Encoding.UTF8, "application/json");
                            using var resp = await http.PostAsync(url, content, ct)
                                               .ConfigureAwait(false);

                            lastResult[idx] = resp.IsSuccessStatusCode ? 1 : 0;

                            // 策略接收：解析响应体中的策略 JSON 并回包
                            // 对应老工具 ParseRevData → SendExecResult 流程
                            if (resp.IsSuccessStatusCode && _policyWorker != null)
                            {
                                try
                                {
                                    var body = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
                                    if (!string.IsNullOrWhiteSpace(body))
                                        _ = _policyWorker.ProcessHttpsResponseBodyAsync(body, c.ClientId, ct);
                                }
                                catch { }
                            }

                            // 可选 UDP 心跳到日志服务器（与 TCP 心跳的附加逻辑对齐）
                            if (useLogServer && udpSender != null && !string.IsNullOrEmpty(logHost)
                                && resp.IsSuccessStatusCode)
                            {
                                var jsonBytes = TrimTrailingNewline(Encoding.UTF8.GetBytes(json));
                                var payload   = PtProtocol.Pack(jsonBytes, cmdId: 1, deviceId: c.DeviceId);
                                var udpOk     = await udpSender.SendUdpAsync(logHost, logPort, payload, 1500, ct)
                                                               .ConfigureAwait(false);
                                lastUdpResult[idx] = udpOk ? 1 : 0;
                            }
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

                    int ok = 0, fail = 0, udpOk = 0, udpFail = 0;
                    for (int j = 0; j < clients.Count; j++)
                    {
                        if      (lastResult[j] == 1) ok++;
                        else if (lastResult[j] == 0) fail++;
                        if      (lastUdpResult[j] == 1) udpOk++;
                        else if (lastUdpResult[j] == 0) udpFail++;
                    }
                    // HTTPS 心跳：Connected = HTTP 响应成功数；SuccessTcp/FailTcp 复用为 HTTPS ok/fail
                    try { progress?.Report(new HeartbeatStats(clients.Count, ok, ok, fail, udpOk, udpFail)); } catch { }
                }
            }, ct);

            await Task.WhenAll(clientTasks).ConfigureAwait(false);
        }
    }
}
