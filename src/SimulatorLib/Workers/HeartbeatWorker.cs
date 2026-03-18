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
    ///   - 相位对齐重连：错峰启动让各客户端占据不同时间槽，断线后等到自己的下一个自然时刻再重连
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
            int RsnSessionStale = 0, int RsnLockBusy = 0, int RsnNoRegister = 0);

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
            public const int NoRegister     = 7; // 平台返回 cmdId==18（未注册），需要 HTTPS 重注册后重连
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
            string? osVersion = null,
            string? clientVersion = null)
        {
            var clientList = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            if (clientList.Count == 0) return;
            var clients = clientList; // 仅用于长度/索引（read-only 语义）
            // 可变当前记录：重注册后更新 DeviceId/TcpPort 并持久化到 Clients.log
            var currentClients = clientList.ToList();
            // 异步安全的写锁（保护 currentClients 并发修改 + WriteAllAsync）
            var clientsLock = new SemaphoreSlim(1, 1);

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

            // 错峰启动：在整个 intervalMs 窗口内均匀错开首次连接（对齐老工具 500ms×50批≈25s 的启动散布）。
            // 上限等于 intervalMs 而非固定 3s，确保 500 个客户端的相位散布覆盖完整心跳周期，
            // 重连时相位天然不重叠，不需要额外 jitter。
            int spreadMs = intervalMs;

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
                    // cmdId==18 标志：drain 收到平台返回“未注册”时置 true，主循环在重连前先执行 HTTPS 重注册
                    bool           needReregister = false;
                    // 立即唤醒信号：drain 检测到断连或 NOREGISTER 时 Release，使主循环提前退出 intervalMs 等待
                    var wakeupSem = new SemaphoreSlim(0, 1);
                    // 相位对齐重连门控：记录本客户端"自然下一次 HB 时刻"（Unix ms），
                    // 每次 HB 写入成功后更新为 now+intervalMs。断线后重连时等到此刻，
                    // 保持各客户端重连时刻的相位差，天然散布在整个 intervalMs 窗口内。
                    long nextCycleTargetMs = 0L;

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

                            // re-register：对应老工具 HEARTBEAT_CMD_NOREGISTER 分支
                            // ― CloseConnection → RegisterClientToServer (HTTPS) → CreateConnection
                            if (needReregister)
                            {
                                needReregister = false;
                                lastReason[idx] = Reason.NoRegister;
                                var reregResult = await ReregisterClientAsync(
                                    c, platformHost, platformPort, clientVersion, ct).ConfigureAwait(false);
                                if (reregResult != null)
                                {
                                    c = reregResult; // 更新局部 c（DeviceId/TcpPort 可能变化）
                                    await clientsLock.WaitAsync(ct).ConfigureAwait(false);
                                    try
                                    {
                                        currentClients[idx] = reregResult;
                                        await ClientsPersistence.WriteAllAsync(currentClients).ConfigureAwait(false);
                                    }
                                    finally { clientsLock.Release(); }
                                    hbEventLog.Enqueue($"{DateTime.UtcNow:o} REREGISTER OK   client={c.ClientId} deviceId={c.DeviceId}");
                                }
                                else
                                {
                                    hbEventLog.Enqueue($"{DateTime.UtcNow:o} REREGISTER FAIL client={c.ClientId} 将使用旧凭据重连");
                                }
                            }

                            // 相位对齐重连：等到本客户端"自然下一次 HB 时刻"再发起 ConnectAsync。
                            // 原理：500 个客户端的 startDelay 均匀散布在 [0, intervalMs) 内，
                            // 每次 HB 写入后 nextCycleTargetMs = hbTime+intervalMs，各客户端值不同。
                            // 平台批量发 FIN 时，各客户端等待剩余时间各异，重连自然散布在整个周期窗口，
                            // 等价于老工具 C++ Sleep(30000) 保留的相位差，无需随机 jitter。
                            if (hasConnectedBefore && nextCycleTargetMs > 0)
                            {
                                long waitMs = nextCycleTargetMs - DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                                if (waitMs > 0)
                                {
                                    try { await Task.Delay((int)Math.Min(waitMs, intervalMs), ct).ConfigureAwait(false); }
                                    catch (OperationCanceledException) { return; }
                                }
                            }

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
                                // Fix 4: 精细化 TCP KeepAlive 探测参数（SIO_KEEPALIVE_VALS）。
                                // 老工具用 CURLOPT_LOW_SPEED_LIMIT/TIME 感知死连接；.NET WriteAsync(无CT) 需要
                                // OS 层面检测才能在合理时间内抛 IOException，而非永久阻塞。
                                // 5s 空闲后开始探测，每 2s 一次，OS 默认 3~5 次 = 死连接约 11~15s 内触发。
                                try
                                {
                                    var ka = new byte[12];
                                    BitConverter.GetBytes(1u).CopyTo(ka, 0);     // enable=1
                                    BitConverter.GetBytes(5000u).CopyTo(ka, 4);  // idle_ms=5000
                                    BitConverter.GetBytes(2000u).CopyTo(ka, 8);  // interval_ms=2000
                                    newTcp.Client.IOControl(IOControlCode.KeepAliveValues, ka, null);
                                }
                                catch { /* 不支持时忽略，基础 KeepAlive 已由 SetSocketOption 开启 */ }
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
                                                    DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()),
                                                onNeedReregister: () =>
                                                {
                                                    // cmdId==18：平台通知未注册，标记重注册并立即唤醒主循环
                                                    needReregister = true;
                                                    alive = false;
                                                    capturedRegistry?.Unregister(capturedC.ClientId);
                                                    Interlocked.Exchange(ref evtDisconnAtMs[capturedIdx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                                    hbEventLog.Enqueue($"{DateTime.UtcNow:o} NOREGISTER     client={capturedC.ClientId} 触发重注册");
                                                    if (wakeupSem.CurrentCount == 0) wakeupSem.Release();
                                                })
                                                .ConfigureAwait(false);
                                            if (!serverAlive)
                                            {
                                                alive = false;
                                                lastReason[capturedIdx] = Reason.ServerClosed;
                                                capturedRegistry?.Unregister(capturedC.ClientId);
                                                eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 服务端关闭(PolicyDrain) 客户端#{capturedIdx} {capturedC.ClientId}");
                                                Interlocked.Exchange(ref evtDisconnAtMs[capturedIdx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                                hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={capturedC.ClientId} reason=服务端关闭");
                                                if (wakeupSem.CurrentCount == 0) wakeupSem.Release();
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
                                                    if (wakeupSem.CurrentCount == 0) wakeupSem.Release();
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
                                        if (wakeupSem.CurrentCount == 0) wakeupSem.Release();
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

                            // 直写架构：HB 也走写锁，和 LogWorker DirectSendAsync 共享同一 SemaphoreSlim(1,1)。
                            // HB 1次/30s vs Log 3次/s → 冲突概率 < 0.5%，微秒级持有时间。
                            bool lockAcquired = false;
                            if (_streamRegistry != null)
                                lockAcquired = await _streamRegistry.AcquireWriteLockAsync(c.ClientId, 2000, ct).ConfigureAwait(false);
                            try
                            {
                                await stream!.WriteAsync(payload, 0, payload.Length).ConfigureAwait(false);
                                await stream.FlushAsync().ConfigureAwait(false);
                            }
                            finally
                            {
                                if (lockAcquired) _streamRegistry!.ReleaseWriteLock(c.ClientId);
                            }
                            _streamRegistry?.Register(c.ClientId, stream!);
                            // 更新相位目标：下次 HB（或重连）时刻 = 本次写入时刻 + intervalMs
                            nextCycleTargetMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + intervalMs;

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
                            // 写失败 = 连接已死，立即重连（对齐老工具在同一周期内 CreateConnection 重试）
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
                            nextCycleTargetMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + intervalMs; // 相位门控：重连等待一个完整周期
                            continue; // 重连等待由循环顶部的 nextCycleTargetMs 门控处理
                        }

                        // HB 间隙：直写架构下 LogWorker 自行写 TCP，drain 仅处理 Channel fallback 积压。
                        // 简化为：快速清空 Channel 残留（若有），然后 wakeupSem 等待到下次 HB。
                        if (alive && stream != null)
                        {
                            var logReader = _streamRegistry?.GetLogReader(c.ClientId);
                            if (logReader != null)
                            {
                                // 快速清空 fallback Channel 积压（非 allThreatTcp 路径可能残留少量包）
                                while (alive && !ct.IsCancellationRequested && logReader.TryRead(out var logPayload))
                                {
                                    try
                                    {
                                        bool drainLock = false;
                                        if (_streamRegistry != null)
                                            drainLock = await _streamRegistry.AcquireWriteLockAsync(c.ClientId, 2000, ct).ConfigureAwait(false);
                                        try
                                        {
                                            using var writeCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                                            writeCts.CancelAfter(3000);
                                            await stream.WriteAsync(logPayload, 0, logPayload.Length, writeCts.Token).ConfigureAwait(false);
                                        }
                                        finally
                                        {
                                            if (drainLock) _streamRegistry!.ReleaseWriteLock(c.ClientId);
                                        }
                                    }
                                    catch (OperationCanceledException) when (ct.IsCancellationRequested) { break; }
                                    catch
                                    {
                                        tcp?.Dispose();
                                        tcp    = null;
                                        stream = null;
                                        alive  = false;
                                        Interlocked.Exchange(ref connectedFlags[idx], 0);
                                        _streamRegistry?.Unregister(c.ClientId);
                                        lastResult[idx] = 0;
                                        lastReason[idx] = Reason.WriteFailed;
                                        eventQueue.Enqueue($"[{DateTime.Now:HH:mm:ss.fff}] 写入失败(LogDrain) 客户端#{idx} {c.ClientId}");
                                        Interlocked.Exchange(ref evtDisconnAtMs[idx], DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
                                        hbEventLog.Enqueue($"{DateTime.UtcNow:o} DISCONNECT client={c.ClientId} reason=日志写入失败");
                                        break;
                                    }
                                }
                            }

                            // 等待下次 HB 周期（wakeupSem 可被 drain task 断连事件提前唤醒）
                            if (alive)
                            {
                                try { await wakeupSem.WaitAsync(intervalMs, ct).ConfigureAwait(false); }
                                catch (OperationCanceledException) { break; }
                            }
                        }

                        //  4. Session 超时检测：平台静默踢人而 TCP 不断的场景
                        //  触发条件：曾经收到过平台回包（lastReply>0），但最近 staleMs 内没有再收到。
                        //  对齐老工具 C++ 行为：RecvHeartBeatBack_TCP 2s 超时后直接继续下一周期，
                        //  从不因"未收到回包"主动断线。只有曾有回包、后来停止时才处理。
                        //  !! lastReply==0（本次连接从未收到回包）= 平台忙/容量限制，不等于 session 失效。
                        //  旧逻辑（refTime=connectedAtMs 当 lastReply==0 时）会在 90s 后触发断线，
                        //  造成 ~2min 振荡周期（90s等待 + 30s重连），这是之前版本 EPS 振荡的根因。
                        if (alive)
                        {
                            long staleMs   = Math.Max((long)intervalMs * 3, 30_000L);
                            long nowCheck  = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                            long lastReply = Interlocked.Read(ref lastReplyTimeMs[idx]);
                            // 只有收到过回包、且最近又停止回包，才判定 session 失效（对齐老工具）
                            if (lastReply > 0 && (nowCheck - lastReply) > staleMs)
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
                                // 相位对齐：重连由循环顶部的 nextCycleTargetMs 门控，无需额外随机 jitter
                            }
                        }
                    }

                    tcp?.Dispose();
                }, ct));
            }

            // 监控日志：定时写文件，记录离线数量与断线原因分布（供开发分析）
            // 间隔：max(30s, intervalMs)，确保即使心跳间隔30s、测试刚开始3分钟内也能捕获异常
            int monitorIntervalMs = Math.Max(30_000, intervalMs);
            var hbLogDir = Path.Combine(AppContext.BaseDirectory, "logs");
            Directory.CreateDirectory(hbLogDir);
            var monitorFile = Path.Combine(hbLogDir,
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
                    int rServerClosed = 0, rWriteFailed = 0, rConnFailed = 0, rConnTimeout = 0, rSessionStale = 0, rLockBusy = 0, rNoRegister = 0;
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
                            case Reason.NoRegister:     rNoRegister++; break; // 包含已重连的，更能反映历史重注册次数
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
                    if (rNoRegister > 0)
                        sb.AppendLine($"  平台返回未注册(NoRegister/历史次数):{rNoRegister} ⇒ 已触发 HTTPS 重注册+重连");
                    sb.AppendLine();

                    prevReplied = replied;
                    prevConn    = conn;

                    try { await File.AppendAllTextAsync(monitorFile, sb.ToString(), ct).ConfigureAwait(false); }
                    catch { /* 写文件失败不影响主流程 */ }
                }
            }, ct);

            // 事件日志任务：实时将 hbEventLog 写入 heartbeat-events-YYYYMMDD.log（500ms 刷新）
            var hbEventLogFile = Path.Combine(hbLogDir, $"heartbeat-events-{DateTime.UtcNow:yyyyMMdd}.log");
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
                    int rSC = 0, rWF = 0, rCF = 0, rCT = 0, rSS = 0, rLB = 0, rNR = 0;
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
                            case Reason.NoRegister:     rNR++; break;
                            case Reason.LockBusy:       rLB++; break; // 连接有效但心跳被跳过
                        }
                    }
                    try { progress?.Report(new HeartbeatStats(clients.Count, conn, ok, fail, udpOk, udpFail, replied, rSC, rWF, rCF, rCT, rSS, rLB, rNR)); } catch { }
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

        /// <summary>
        /// 对单个客户端执行 HTTPS 重注册。对应老工具 HEARTBEAT_CMD_NOREGISTER 分支：
        ///   CloseConnection → RegisterClientToServer(HTTPS) → 返回更新后的 ClientRecord。
        /// 若注册失败返回 null，调用方保留旧记录继续重连（避免永久离线）。
        /// </summary>
        private static async Task<ClientRecord?> ReregisterClientAsync(
            ClientRecord client, string host, int port, string? clientVersion, CancellationToken ct)
        {
            var handler = new HttpClientHandler
            {
                ServerCertificateCustomValidationCallback = HttpClientHandler.DangerousAcceptAnyServerCertificateValidator,
            };
            using var http = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(10) };
            var loginUrl  = new Uri($"https://{host}:{port}/USM/clientLogin.do");
            var resultUrl = new Uri($"https://{host}:{port}/USM/clientResult.do");

            var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(client.IP);
            var reqJson = UsmRegistrationJsonBuilder.BuildClientLoginJson(
                computerId: client.ClientId,
                username: "SysAdmin",
                computerName: client.ClientId,
                computerIp: client.IP,
                computerMac: mac,
                windowsVersion: OsInfo.GetWindowsVersionName(),
                windowsX64: Environment.Is64BitOperatingSystem,
                proxying: 0,
                licenseRecycle: false,
                clientType: 0,
                clientVersion: clientVersion ?? "V300R011C01B090");
            try
            {
                using var content = new StringContent(reqJson, Encoding.UTF8, "application/json");
                using var resp    = await http.PostAsync(loginUrl, content, ct).ConfigureAwait(false);
                var respText      = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
                if (!resp.IsSuccessStatusCode) return null;
                if (!UsmResponseParsers.TryCheckSetupResult(respText, out _, out _)) return null;

                uint deviceId = 0;
                int  tcpPort  = 0;
                if (UsmResponseParsers.TryParseConfigInfo(respText, out var cfg))
                {
                    deviceId = cfg.DeviceId;
                    tcpPort  = cfg.TcpPort;
                }

                var resultJson = UsmRegistrationJsonBuilder.BuildClientResultJson(
                    computerId: client.ClientId,
                    username: "SysAdmin",
                    cmdType: UsmRegistrationJsonBuilder.SetupCmdType,
                    cmdId: UsmRegistrationJsonBuilder.CmdClientRegistry,
                    dealResult: 0);
                using var rc = new StringContent(resultJson, Encoding.UTF8, "application/json");
                using var rr = await http.PostAsync(resultUrl, rc, ct).ConfigureAwait(false);
                if (!rr.IsSuccessStatusCode) return null;

                return client with { Status = "Registered", RegisteredAt = DateTime.UtcNow, DeviceId = deviceId, TcpPort = tcpPort };
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                throw; // 用户主动取消，向上传播
            }
            catch
            {
                return null; // 超时/网络问题，返回 null 由调用方决定是否用旧凭据重连
            }
        }
    }
}
