using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Security.Cryptography;
using SimulatorLib.Network;
using SimulatorLib.Persistence;
using SimulatorLib.Models;
using System.IO;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using SimulatorLib.Protocol;
using System.Buffers.Binary;
using System.Net.Http;

namespace SimulatorLib.Workers
{
    public record LogSendStats(int TotalMessages, int Success, int Failed, string? MultiIpSummary = null);

    public class LogWorker
    {
        private readonly INetworkSender _tcpSender;
        private readonly IUdpSender? _udpSender;

        private readonly ConcurrentDictionary<string, TcpConnState> _tcpStates = new();
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _tcpStateLocks = new();

        private readonly ConcurrentDictionary<string, int> _debugSendCount = new();
        private const int MaxDebugSendPerClient = 3;

        // Aligned with external/IEG_Code/code/include/format/commurl.h and interface documentation
        private const string HttpsPathProcess = "/USM/clientAlertlog.do";
        private const string HttpsPathUsb = "/USM/clientULog.do";
        private const string HttpsPathUsbWarning = "/USM/clientUSBLog.do"; // USB访问告警
        private const string HttpsPathUDiskPlug = "/USM/hotplugDevLog.do"; // U盘插拔
        // external/IEG_Code/code/include/format/commurl.h: URL_LOG_VUL
        // 注意：clientVulDefslog.do 是漏洞库同步接口，日志上报应使用 loopholeProtectLog.do
        private const string HttpsPathVulDefense = "/USM/loopholeProtectLog.do";
        private const string HttpsPathAdmin = "/USM/clientAdminLog.do";
        private const string HttpsPathHostDefence = "/USM/hostDefenceWarning.do";
        private const string HttpsPathFireWall = "/USM/clientFirewallLog.do";
        private const string HttpsPathIllegalConnect = "/USM/illegalConnectLog.do";
        private const string HttpsPathVirusProtect = "/USM/virusProtectLog.do";
        private const string HttpsPathThreatFake = "/USM/threatFakeLog.do";
        private const string HttpsPathDataGuard = "/USM/docGuardLog.do";
        private const string HttpsPathSysGuard = "/USM/sysGuardLog.do";
        private const string HttpsPathOsResource = "/USM/resourceMessageLog.do";
        private const string HttpsPathSafetyStore = "/USM/clientSafetyAppInstallLog.do";

        private const int MaxDebugHttps = 10;
        private int _httpsDebugCount = 0;

        // 失败 HTTPS 响应详情日志（不受 MaxDebugHttps 限制，单独写入 logsend-failures-https-*.log）
        // 按文件大小限制：单个失败日志文件最大 50 MB，超出后停止追加（防止磁盘撑爆）
        private const long MaxFailLogBytes = 50L * 1024 * 1024; // 50 MB

        // 失败追踪：记录每个日志分类的失败次数，输出到 logsend-failures-YYYYMMDD.log
        private readonly ConcurrentDictionary<string, int> _failByCategory = new();

        // 共享心跳流注册表（对齐原项目 SetWLTcpConnectInstance 复用同一 TCP 连接）
        private readonly HeartbeatStreamRegistry? _streamRegistry;

        public LogWorker(INetworkSender tcpSender, IUdpSender? udpSender = null, HeartbeatStreamRegistry? streamRegistry = null)
        {
            _tcpSender = tcpSender;
            _udpSender = udpSender;
            _streamRegistry = streamRegistry;
        }

        /// <summary>
        /// 发送日志：按每客户端发送指定条数的日志消息。若 useLogServer=true 则采用 UDP 发送到日志服务器，否则通过 TCP 发送到平台。
        /// </summary>
        public async Task StartAsync(int messagesPerClient, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, CancellationToken ct, IProgress<LogSendStats>? progress = null)
        {
            await StartCoreAsync(
                messagesPerClient: messagesPerClient,
                messagesPerSecondPerClient: null,
                maxClients: null,
                categories: null,
                useLogServer: useLogServer,
                platformHost: platformHost,
                platformPort: platformPort,
                logHost: logHost,
                logPort: logPort,
                concurrency: concurrency,
                stressMode: false,
                localIps: null,
                ct: ct,
                progress: progress).ConfigureAwait(false);
        }

        /// <summary>
        /// 增强版：支持
        /// - maxClients：仅对前 N 个客户端发日志（由 Clients.log 顺序决定）
        /// - messagesPerSecondPerClient：每客户端每秒条数（<=0 或 null 表示不做速率控制）
        /// - categories：日志分类（为空则用 Default）；会按消息序号轮转
        /// </summary>
        public async Task StartAsync(int messagesPerClient, int? messagesPerSecondPerClient, int? maxClients, IReadOnlyList<string>? categories, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, bool stressMode, IReadOnlyList<string>? localIps, CancellationToken ct, IProgress<LogSendStats>? progress = null)
        {
            await StartCoreAsync(
                messagesPerClient: messagesPerClient,
                messagesPerSecondPerClient: messagesPerSecondPerClient,
                maxClients: maxClients,
                categories: categories,
                useLogServer: useLogServer,
                platformHost: platformHost,
                platformPort: platformPort,
                logHost: logHost,
                logPort: logPort,
                concurrency: concurrency,
                stressMode: stressMode,
                localIps: localIps,
                ct: ct,
                progress: progress).ConfigureAwait(false);
        }

        private async Task StartCoreAsync(int messagesPerClient, int? messagesPerSecondPerClient, int? maxClients, IReadOnlyList<string>? categories, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, bool stressMode, IReadOnlyList<string>? localIps, CancellationToken ct, IProgress<LogSendStats>? progress)
        {
            var clientsAll = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            var clients = (maxClients.HasValue && maxClients.Value > 0)
                ? clientsAll.Take(maxClients.Value).ToList()
                : clientsAll;

            if (clients.Count == 0)
            {
                var emptyStats = new LogSendStats(0, 0, 0);
                try { progress?.Report(emptyStats); } catch { }

                try
                {
                    var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                    var logPath = Path.Combine(logDir, $"logsend-{DateTime.UtcNow:yyyyMMdd}.log");
                    var line = $"{DateTime.UtcNow:o} TotalMessages=0 Success=0 Fail=0 Reason=NoClients ClientsFile={ClientsPersistence.GetPath()}";
                    await File.AppendAllTextAsync(logPath, line + Environment.NewLine, ct).ConfigureAwait(false);
                }
                catch { }

                return;
            }

            var totalMessages = clients.Count * messagesPerClient;
            var success = 0;
            var fail = 0;

            // 任务启动即上报一次，避免 UI 长时间显示 0。
            try { progress?.Report(new LogSendStats(totalMessages, 0, 0)); } catch { }

            var sw = Stopwatch.StartNew();
            long lastReportMs = 0;

            void MaybeReportProgress()
            {
                if (progress == null) return;

                var now = sw.ElapsedMilliseconds;
                var last = Interlocked.Read(ref lastReportMs);
                if (now - last < 1000) return;
                if (Interlocked.CompareExchange(ref lastReportMs, now, last) != last) return;

                try { progress.Report(new LogSendStats(totalMessages, success, fail)); } catch { }
            }

            // 计算发送间隔：每个客户端独立按此间隔发送。
            // 语义：messagesPerSecondPerClient=2 → 每个客户端每秒2条 → 1000客户端 = 2000 EPS（单机）
            int intervalMs = 0;
            if (messagesPerSecondPerClient.HasValue && messagesPerSecondPerClient.Value > 0)
                intervalMs = 1000 / messagesPerSecondPerClient.Value;

            var categoryList = (categories != null && categories.Count > 0)
                ? categories
                : new[] { "Default" };

            // 原始客户端（libcurl）：连接超时6s，请求超时30s，每次请求独立TLS连接。
            // 压测模式：允许连接复用，MaxConnectionsPerServer=concurrency，消除端口耗尽问题。
            // 多IP模式：解析本地IP列表，短连接时通过 ConnectCallback 轮转绑定本地IP，每IP独立端口池。
            var parsedLocalIps = ParseLocalIps(localIps);
            // 每个IP的实际连接计数（原子递增），用于结束后验证分布
            var ipCounters = parsedLocalIps.Count > 0 ? new long[parsedLocalIps.Count] : null;
            using var http = stressMode
                ? CreateHttpClientStress(concurrency, connectTimeoutMs: 6000, requestTimeoutMs: 30000)
                : parsedLocalIps.Count > 0
                    ? CreateHttpClientMultiIp(parsedLocalIps, ipCounters!, connectTimeoutMs: 6000, requestTimeoutMs: 30000)
                    : CreateHttpClient(connectTimeoutMs: 6000, requestTimeoutMs: 30000);

            // 压测模式下用信号量限制同时飞行的 HTTPS 请求数，避免连接池内锁竞争。
            // 门控容量 == MaxConnectionsPerServer，进门即有连接，零等待。
            using var httpsGate = stressMode
                ? new SemaphoreSlim(Math.Max(1, concurrency), Math.Max(1, concurrency))
                : null;

            // 错峰启动：在 spreadMs 内均匀分散各客户端的首次发送，避免瞬间冲击。
            // 1000客户端 × 2/s → 若全部同时启动，瞬间 2000 TLS 握手/s 直接打垮服务端。
            // 扩大为 10s 启动窗口 → 100 握手/s，服务端可承受。
            // 最大不超过 30s，单客户端不超过 1 轮 interval。
            int spreadMs = clients.Count > 1 ? Math.Min(Math.Max(intervalMs > 0 ? intervalMs : 1000, clients.Count > 100 ? 10000 : 3000), 30000) : 0;

            var clientTasks = new List<Task>(clients.Count);
            for (int ci = 0; ci < clients.Count; ci++)
            {
                var c = clients[ci];
                var startDelay = clients.Count > 1 ? (int)((long)ci * spreadMs / clients.Count) : 0;

                clientTasks.Add(Task.Run(async () =>
                {
                    // 每客户端独立 serial（序列号语义是 per-connection，无需全局唯一）
                    uint clientSerial = 0;

                    if (startDelay > 0)
                    {
                        try { await Task.Delay(startDelay, ct).ConfigureAwait(false); }
                        catch (OperationCanceledException) { return; }
                    }

                    // deadline 模式：目标时刻按固定步长递增，HTTP 耗时从等待中扣除，不叠加。
                    // 超期分两种情况处理：
                    //   慢响应（耗时 >= fastFailThresholdMs）：正常压力大，重置基准直接继续。
                    //   快速失败（耗时 < fastFailThresholdMs，如 TLS 握手被拒 ~1ms）：
                    //     额外等 intervalMs，防止多主机叠加产生握手风暴打垮服务端。
                    var intervalTicks = intervalMs > 0 ? (long)(intervalMs * (double)Stopwatch.Frequency / 1000.0) : 0L;
                    const int fastFailThresholdMs = 50; // 低于此值视为快速失败，需要 floor 限速
                    var nextDeadlineTick = Stopwatch.GetTimestamp(); // 第1条立即发送
                    long lastSendElapsedMs = intervalMs; // 首条假设"正常耗时"

                    for (int msgIdx = 0; msgIdx < messagesPerClient && !ct.IsCancellationRequested; msgIdx++)
                    {
                        if (msgIdx > 0 && intervalTicks > 0)
                        {
                            nextDeadlineTick += intervalTicks;
                            var remainTicks = nextDeadlineTick - Stopwatch.GetTimestamp();
                            if (remainTicks > 0)
                            {
                                var waitMs = (int)(remainTicks * 1000L / Stopwatch.Frequency);
                                if (waitMs > 0)
                                {
                                    try { await Task.Delay(waitMs, ct).ConfigureAwait(false); }
                                    catch (OperationCanceledException) { return; }
                                }
                            }
                            else
                            {
                                // 超期：重置基准，不累积欠债。
                                nextDeadlineTick = Stopwatch.GetTimestamp();
                                // 仅快速失败时加 floor 等待，正常慢响应直接继续。
                                if (lastSendElapsedMs < fastFailThresholdMs)
                                {
                                    try { await Task.Delay(intervalMs, ct).ConfigureAwait(false); }
                                    catch (OperationCanceledException) { return; }
                                }
                            }
                        }

                        var cat = categoryList[msgIdx % categoryList.Count];
                        var (socketCmd, json) = BuildLogByCategory(cat, c, messageIndex: msgIdx);
                        bool ok;

                        var sendSw = Stopwatch.StartNew();
                        try
                        {
                            if (IsThreatDataCategory(cat))
                            {
                                var src = GetThreatDataJsonBytes(json);
                                uint serial = unchecked(++clientSerial);
                                var pt = PtProtocol.Pack(
                                    src,
                                    cmdId: socketCmd,
                                    compressType: PtCompressType.Zlib,
                                    encryptType: useLogServer ? PtEncryptType.Aes : PtEncryptType.None,
                                    deviceId: c.DeviceId,
                                    serialNumber: serial);
                                TryDebugWriteSentPacket(c.ClientId, pt, json);

                                if (useLogServer)
                                    ok = _udpSender != null && !string.IsNullOrEmpty(logHost)
                                        ? await _udpSender.SendUdpAsync(logHost, logPort, pt, 2000, ct).ConfigureAwait(false)
                                        : false;
                                else
                                {
                                    var tcpPort = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                    ok = await SendLogTcpLikeExternalAsync(c.ClientId, platformHost, tcpPort, pt, ct).ConfigureAwait(false);
                                }
                            }
                            else if (useLogServer)
                            {
                                if (IsHttpsOnlyCategory(cat))
                                {
                                    var path = GetHttpsPathForCategory(cat);
                                    ok = await PostLogHttpsGatedAsync(http, httpsGate, platformHost, platformPort, path, json, cat, ct).ConfigureAwait(false);
                                }
                                else
                                {
                                    var src = GetSocketJsonBytes(json);
                                    uint serial = unchecked(++clientSerial);
                                    var pt = PtProtocol.Pack(src, cmdId: socketCmd, deviceId: c.DeviceId, serialNumber: serial);
                                    TryDebugWriteSentPacket(c.ClientId, pt, json);

                                    ok = _udpSender != null && !string.IsNullOrEmpty(logHost)
                                        ? await _udpSender.SendUdpAsync(logHost, logPort, pt, 2000, ct).ConfigureAwait(false)
                                        : false;
                                }
                            }
                            else
                            {
                                var path = GetHttpsPathForCategory(cat);
                                ok = await PostLogHttpsGatedAsync(http, httpsGate, platformHost, platformPort, path, json, cat, ct).ConfigureAwait(false);
                            }

                            if (ok) Interlocked.Increment(ref success);
                            else { Interlocked.Increment(ref fail); TrackFailure(cat, "non_ok_response"); }
                        }
                        catch (OperationCanceledException) { return; }
                        catch (Exception ex)
                        {
                            Interlocked.Increment(ref fail);
                            TrackFailure(cat, ex.GetType().Name + ": " + ex.Message);
                        }
                        lastSendElapsedMs = sendSw.ElapsedMilliseconds;

                        MaybeReportProgress();
                    }
                }, ct));
            }

            try
            {
                await Task.WhenAll(clientTasks).ConfigureAwait(false);
            }
            finally
            {
                await CloseAllTcpAsync().ConfigureAwait(false);
            }

            WriteFailureSummary();

            // 多IP分布摘要
            string? multiIpSummary = null;
            if (ipCounters != null && parsedLocalIps.Count > 0)
            {
                var parts = parsedLocalIps.Select((ip, i) => $"{ip}: {ipCounters[i]:N0}次").ToArray();
                multiIpSummary = "多IP连接分布 → " + string.Join("  ", parts);
            }

            var stats = new LogSendStats(totalMessages, success, fail, multiIpSummary);
            try { progress?.Report(stats); } catch { }

            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-{DateTime.UtcNow:yyyyMMdd}.log");
                var line = $"{DateTime.UtcNow:o} TotalMessages={stats.TotalMessages} Success={stats.Success} Fail={stats.Failed} ClientsAll={clientsAll.Count} ClientsSelected={clients.Count} ClientsFile={ClientsPersistence.GetPath()} UseLogServer={useLogServer} Platform={platformHost}:{platformPort} LogServer={logHost}:{logPort}";
                await File.AppendAllTextAsync(logPath, line + Environment.NewLine).ConfigureAwait(false);
            }
            catch { }
        }

        private static byte[] GetSocketJsonBytes(string json)
        {
            // external: SendBySocketClient(strJson.c_str(), strJson.length() + 1, nCmd)
            // 但在打 PT 包时通常会传 len-1（不包含末尾 '\0'）。
            // 因此这里返回：UTF-8(JSON + '\n')，不附加 '\0'。
            if (string.IsNullOrEmpty(json)) return Array.Empty<byte>();

            if (!json.EndsWith("\n", StringComparison.Ordinal)) json += "\n";
            return Encoding.UTF8.GetBytes(json);
        }

        private static byte[] GetThreatDataJsonBytes(string json)
        {
            // external/WLCData/WLData.cpp: SendLogToServer(GetPortocal(..., nSrcLen=strJson.length()-1))
            // 这里不强制追加换行，并在末尾存在换行时去掉，避免 srcLen/CRC 受影响。
            if (string.IsNullOrEmpty(json)) return Array.Empty<byte>();
            if (json.EndsWith("\n", StringComparison.Ordinal)) json = json.Substring(0, json.Length - 1);
            return Encoding.UTF8.GetBytes(json);
        }

        private static bool IsThreatDataCategory(string category)
        {
            var cat = LogCategoryHelper.ParseDisplayName(category);
            return cat.HasValue && LogCategoryHelper.IsThreatDataCategory(cat.Value);
        }

        private static bool IsHttpsOnlyCategory(string category)
        {
            // external/WLMainDataHandle/AccessServer.cpp::SendLog2Server
            // DP/SysGuard are only posted via HTTPS; the non-https(LogServer socket) branch does not include them.
            var cat = LogCategoryHelper.ParseDisplayName(category);
            return cat.HasValue && LogCategoryHelper.IsHttpsOnlyCategory(cat.Value);
        }

        private static string GetHttpsPathForCategory(string category)
        {
            // Best-effort mapping from simulator categories to external commurl.h.
            // If a category is unknown, fall back to the closest general endpoint.
            
            // 特殊处理：文件保护、注册表保护、强制访问控制都使用HostDefence endpoint
            if (category == "文件保护" || category == "注册表保护" || category == "强制访问控制")
            {
                return HttpsPathHostDefence;
            }
            
            var cat = LogCategoryHelper.ParseDisplayName(category);
            if (!cat.HasValue) return HttpsPathProcess;

            return cat.Value switch
            {
                LogCategory.Admin => HttpsPathAdmin,
                LogCategory.Process => HttpsPathProcess,
                LogCategory.VulDefense => HttpsPathVulDefense,
                LogCategory.Illegal => HttpsPathIllegalConnect,
                LogCategory.VPScan => HttpsPathVirusProtect,
                LogCategory.OSResource => HttpsPathOsResource,
                LogCategory.DP => HttpsPathDataGuard,
                LogCategory.SysGuard => HttpsPathSysGuard,
                LogCategory.RegProtect => HttpsPathHostDefence,
                LogCategory.HostDefence => HttpsPathHostDefence,
                LogCategory.Usb => HttpsPathUsb,
                LogCategory.UsbWarning => HttpsPathUsbWarning,
                LogCategory.UDiskPlug => HttpsPathUDiskPlug,
                LogCategory.SafetyStore => HttpsPathSafetyStore,
                LogCategory.FireWall => HttpsPathFireWall,
                LogCategory.TFWarning => HttpsPathThreatFake,
                // 外设控制展开：全部使用 /USM/clientULog.do（ExtDevLog）
                LogCategory.ExtDevUsbPort or LogCategory.ExtDevWpd or LogCategory.ExtDevCdrom
                    or LogCategory.ExtDevWlan or LogCategory.ExtDevUsbEthernet or LogCategory.ExtDevFloppy
                    or LogCategory.ExtDevBluetooth or LogCategory.ExtDevSerial or LogCategory.ExtDevParallel
                    => HttpsPathUsb,
                _ => HttpsPathProcess,
            };
        }

        private static List<IPAddress> ParseLocalIps(IReadOnlyList<string>? localIps)
        {
            var result = new List<IPAddress>();
            if (localIps == null) return result;
            foreach (var s in localIps)
            {
                var ip = s.Trim();
                if (ip.Length == 0) continue;
                if (IPAddress.TryParse(ip, out var addr))
                    result.Add(addr);
            }
            return result;
        }

        /// <summary>
        /// 多IP模式 HttpClient：短连接（用完即弃），通过 ConnectCallback 轮转绑定本地 IP。
        /// 每个本地 IP 拥有独立的端口池（约 64500 个），N 个 IP × (端口数 / TIME_WAIT) = N × EPS上限。
        /// </summary>
        private static HttpClient CreateHttpClientMultiIp(List<IPAddress> localIps, long[] ipCounters, int connectTimeoutMs, int requestTimeoutMs)
        {
            int ipIndex = -1;
            var handler = new SocketsHttpHandler
            {
                SslOptions = new System.Net.Security.SslClientAuthenticationOptions
                {
                    RemoteCertificateValidationCallback = (_, _, _, _) => true,
                },
                PooledConnectionLifetime = TimeSpan.Zero, // 短连接：用完即弃
                ConnectTimeout           = TimeSpan.FromMilliseconds(connectTimeoutMs),
                ConnectCallback          = async (context, cancellationToken) =>
                {
                    // 轮转选择本地 IP，并累计每IP的实际连接次数
                    int idx = Math.Abs(Interlocked.Increment(ref ipIndex) % localIps.Count);
                    var localIp = localIps[idx];
                    Interlocked.Increment(ref ipCounters[idx]);

                    var socket = new Socket(localIp.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                    socket.NoDelay = true;
                    // 绑定到指定本地 IP，端口 0 = OS 自动从当前 IP 的端口池中分配
                    socket.Bind(new IPEndPoint(localIp, 0));
                    try
                    {
                        await socket.ConnectAsync(context.DnsEndPoint.Host, context.DnsEndPoint.Port, cancellationToken).ConfigureAwait(false);
                        return new NetworkStream(socket, ownsSocket: true);
                    }
                    catch
                    {
                        socket.Dispose();
                        throw;
                    }
                },
            };
            return new HttpClient(handler)
            {
                Timeout               = TimeSpan.FromMilliseconds(requestTimeoutMs),
                DefaultRequestVersion = System.Net.HttpVersion.Version11,
                DefaultVersionPolicy  = HttpVersionPolicy.RequestVersionExact,
            };
        }

        private static HttpClient CreateHttpClient(int connectTimeoutMs, int requestTimeoutMs)
        {
            // 模拟原始客户端（WLNetComm/WLCurl.cpp）行为：
            // - 原始客户端每次请求 curl_easy_init() 新建连接，无连接复用
            // - PooledConnectionLifetime=Zero：每次请求完成后丢弃连接，与原始行为一致
            // - HTTP/1.1：原始客户端从未启用 HTTP/2
            // - 超时参数与原始一致：连接6s，请求30s
            var handler = new SocketsHttpHandler
            {
                SslOptions = new System.Net.Security.SslClientAuthenticationOptions
                {
                    RemoteCertificateValidationCallback = (_, _, _, _) => true,
                },
                PooledConnectionLifetime = TimeSpan.Zero, // 短连接：用完即弃，模拟原始客户端
                ConnectTimeout           = TimeSpan.FromMilliseconds(connectTimeoutMs),
            };
            return new HttpClient(handler)
            {
                Timeout               = TimeSpan.FromMilliseconds(requestTimeoutMs),
                DefaultRequestVersion = System.Net.HttpVersion.Version11,
                DefaultVersionPolicy  = HttpVersionPolicy.RequestVersionExact,
            };
        }

        /// <summary>
        /// 压测模式专用 HttpClient：连接复用，MaxConnectionsPerServer=concurrency。
        /// 消除短连接的端口耗尽问题，单机可支撑 3000+ 客户端 / 6000+ EPS。
        /// </summary>
        private static HttpClient CreateHttpClientStress(int concurrency, int connectTimeoutMs, int requestTimeoutMs)
        {
            var handler = new SocketsHttpHandler
            {
                SslOptions = new System.Net.Security.SslClientAuthenticationOptions
                {
                    RemoteCertificateValidationCallback = (_, _, _, _) => true,
                },
                // 连接复用：不设 PooledConnectionLifetime=Zero，连接可长期保持。
                // 限制对平台的最大并发连接数，与信号量门控数量保持一致。
                MaxConnectionsPerServer = Math.Max(1, concurrency),
                ConnectTimeout          = TimeSpan.FromMilliseconds(connectTimeoutMs),
            };
            return new HttpClient(handler)
            {
                Timeout               = TimeSpan.FromMilliseconds(requestTimeoutMs),
                DefaultRequestVersion = System.Net.HttpVersion.Version11,
                DefaultVersionPolicy  = HttpVersionPolicy.RequestVersionExact,
            };
        }

        /// <summary>
        /// 带信号量门控的 HTTPS 发送：
        /// 压测模式下先获取信号量再发起请求，保证同时飞行的请求数 == 连接池大小，消除池内锁竞争。
        /// 非压测模式（gate=null）直接透传，行为与原来一致。
        /// </summary>
        private async Task<bool> PostLogHttpsGatedAsync(HttpClient http, SemaphoreSlim? gate, string host, int port, string path, string json, string category, CancellationToken ct)
        {
            if (gate == null)
                return await PostLogHttpsAsync(http, host, port, path, json, category, ct).ConfigureAwait(false);

            await gate.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                return await PostLogHttpsAsync(http, host, port, path, json, category, ct).ConfigureAwait(false);
            }
            finally
            {
                gate.Release();
            }
        }

        private async Task<bool> PostLogHttpsAsync(HttpClient http, string host, int port, string path, string json, string category, CancellationToken ct)
        {
            try
            {
                // external Json::FastWriter 末尾通常有换行；这里也尽量贴近。
                if (!json.EndsWith("\n", StringComparison.Ordinal)) json += "\n";

                if (string.IsNullOrWhiteSpace(path)) path = HttpsPathProcess;
                if (!path.StartsWith("/", StringComparison.Ordinal)) path = "/" + path;
                var url = new Uri($"https://{host}:{port}{path}");
                using var content = new StringContent(json, Encoding.UTF8, "application/json");
                using var resp = await http.PostAsync(url, content, ct).ConfigureAwait(false);
                var respText = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);

                TryDebugWriteHttps(host, port, path, resp.StatusCode, respText);
                if (!resp.IsSuccessStatusCode)
                    WriteHttpsFailureDetail(category, host, port, path, (int)resp.StatusCode, respText);
                return resp.IsSuccessStatusCode;
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch (Exception ex)
            {
                // 记录内层异常，便于诊断（如 远程主机强迫关闭连接 等）
                var inner = ex.InnerException;
                var msg = ex.GetType().Name + ": " + ex.Message
                    + (inner != null ? " | Inner: " + inner.GetType().Name + ": " + inner.Message : string.Empty);
                TryDebugWriteHttps(host, port, path, null, msg);
                WriteHttpsFailureDetail(category, host, port, path, -1, msg);
                return false;
            }
        }

        /// <summary>
        /// 记录HTTPS失败响应详情（状态码+响应体），写入 logsend-failures-https-YYYYMMDD.log。
        /// 不受 MaxDebugHttps 限制，按文件大小限制（最大 50 MB）。
        /// </summary>
        private void WriteHttpsFailureDetail(string category, string host, int port, string path, int statusCode, string respBody)
        {
            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-failures-https-{DateTime.UtcNow:yyyyMMdd}.log");
                if (File.Exists(logPath) && new FileInfo(logPath).Length >= MaxFailLogBytes) return;
                var preview = respBody ?? string.Empty;
                if (preview.Length > 500) preview = preview.Substring(0, 500);
                preview = preview.Replace("\r", "\\r").Replace("\n", "\\n");
                var line = $"{DateTime.UtcNow:o} FAIL category={category} host={host}:{port} path={path} status={statusCode} body={preview}";
                File.AppendAllText(logPath, line + Environment.NewLine);
            }
            catch { }
        }

        private void TryDebugWriteHttps(string host, int port, string path, System.Net.HttpStatusCode? status, string text)
        {
            try
            {
                var n = Interlocked.Increment(ref _httpsDebugCount);
                if (n > MaxDebugHttps) return;

                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-https-{DateTime.UtcNow:yyyyMMdd}.log");

                var preview = text ?? string.Empty;
                if (preview.Length > 500) preview = preview.Substring(0, 500);
                preview = preview.Replace("\r", "\\r").Replace("\n", "\\n");

                var code = status.HasValue ? ((int)status.Value).ToString() : "-";
                var line = $"{DateTime.UtcNow:o} HTTPS host={host}:{port} path={path} status={code} preview={preview}";
                File.AppendAllText(logPath, line + Environment.NewLine);
            }
            catch { }
        }

        /// <summary>
        /// 记录单次失败（分类+原因），写入 logsend-failures 日志，并累计分类计数。
        /// </summary>
        private void TrackFailure(string category, string reason)
        {
            _failByCategory.AddOrUpdate(category, 1, (_, v) => v + 1);

            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-failures-{DateTime.UtcNow:yyyyMMdd}.log");
                if (File.Exists(logPath) && new FileInfo(logPath).Length >= MaxFailLogBytes) return;
                var line = $"{DateTime.UtcNow:o} FAIL category={category} reason={reason}";
                File.AppendAllText(logPath, line + Environment.NewLine);
            }
            catch { }
        }

        /// <summary>
        /// 任务结束时将分类失败汇总写入日志，便于分析哪类日志失败率高。
        /// </summary>
        private void WriteFailureSummary()
        {
            if (_failByCategory.IsEmpty) return;
            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-failures-{DateTime.UtcNow:yyyyMMdd}.log");
                var sb = new StringBuilder();
                sb.AppendLine($"{DateTime.UtcNow:o} === FAIL SUMMARY ===");
                foreach (var kv in _failByCategory.OrderByDescending(x => x.Value))
                    sb.AppendLine($"  {kv.Key}: {kv.Value} 次");
                File.AppendAllText(logPath, sb.ToString());
            }
            catch { }
        }

        private static (uint SocketCmd, string Json) BuildLogByCategory(string category, ClientRecord client, int messageIndex)
        {
            // UI 分类 -> external em_LogType + (SOCKET_CMD_LOG_*) + JSON结构
            // 未覆盖/未知的分类，优先回落到 ProcessAlert（平台通常能展示）。

            static int ClampPort(int port)
            {
                if (port < 1) return 1;
                if (port > 65535) return 65535;
                return port;
            }

            var userName = $"user_{Math.Abs(client.DeviceId.GetHashCode()) % 10000:D4}";
            var baseOctet = (Math.Abs(client.DeviceId.GetHashCode()) % 200) + 1;
            var dstIp = $"10.0.{baseOctet}.{(messageIndex % 254) + 1}";
            var srcPort = ClampPort(10000 + (messageIndex % 50000));
            var dstPort = (messageIndex % 3) switch
            {
                0 => 80,
                1 => 443,
                _ => 3389,
            };
            var exeName = (messageIndex % 4) switch
            {
                0 => "cmd.exe",
                1 => "powershell.exe",
                2 => "notepad.exe",
                _ => "calc.exe",
            };
            var fullPath = $"C:\\Windows\\System32\\{exeName}";
            var parent = (messageIndex % 2 == 0) ? "explorer.exe" : "services.exe";

            // 特殊处理：文件保护使用HostDefence类型，Level2=1
            if (category == "文件保护")
            {
                return (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"C:\\Temp\\{client.ClientId}\\protected-{messageIndex}.txt",
                    processName: exeName,
                    userName: userName,
                    logContent: ((messageIndex % 5) + 1).ToString(),  // 原项目使用简单数字字符串："1", "2", "3" etc
                    detailLogTypeLevel2: 1,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_FILEROTECT
                    clientIp: client.IP,
                    clientName: client.ClientId,
                    machineCode: client.ClientId,
                    os: "Windows 10",
                    blocked: true));
            }

            // 特殊处理：注册表保护使用HostDefence类型，Level2=2
            if (category == "注册表保护")
            {
                return (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"HKLM\\Software\\Demo\\Key{messageIndex % 10}",
                    processName: exeName,
                    userName: userName,
                    logContent: ((messageIndex % 5) + 1).ToString(),  // 原项目使用简单数字字符串："1", "2", "3" etc
                    detailLogTypeLevel2: 2,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_REGPROTECT
                    clientIp: client.IP,
                    clientName: client.ClientId,
                    machineCode: client.ClientId,
                    os: "Windows 10",
                    blocked: false));
            }

            // 特殊处理：强制访问控制使用HostDefence类型，Level2=4
            if (category == "强制访问控制")
            {
                return (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"C:\\Temp\\{client.ClientId}\\demo-{messageIndex}.txt",
                    processName: exeName,
                    userName: userName,
                    logContent: ((messageIndex % 5) + 1).ToString(),  // 原项目使用简单数字字符串："1", "2", "3" etc
                    detailLogTypeLevel2: 4,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_MACPROTECT
                    clientIp: client.IP,
                    clientName: client.ClientId,
                    machineCode: client.ClientId,
                    os: "Windows 10",
                    blocked: true));
            }

            // White-list related display names are handled by dedicated builders
            if (category == "非白名单" || category == "非白名单告警")
            {
                return (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildWhitelistAlertLog(
                    client.ClientId,
                    fullPath: fullPath,
                    parentProcess: parent,
                    userName: userName,
                    holdBack: (messageIndex % 7 == 0) ? 1 : 0,
                    companyName: "Microsoft Corporation",
                    productName: "Windows",
                    version: Environment.OSVersion.Version.ToString(),
                    hash: ComputeFakeSha1(fullPath + client.ClientId + messageIndex),
                    iegHash: ComputeFakeSha1("ieg" + client.ClientId + messageIndex),
                    defIntegrity: ComputeFakeSha1(fullPath + "-def" + messageIndex),
                    clientIp: client.IP,
                    clientName: client.ClientId,
                    machineCode: client.ClientId,
                    os: "Windows 10"
                ));
            }

            if (category == "白名单防篡改")
            {
                return (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildWhitelistModifyLog(
                    client.ClientId,
                    fullPath: fullPath,
                    parentProcess: parent,
                    userName: userName,
                    companyName: "Microsoft Corporation",
                    productName: "Windows",
                    version: Environment.OSVersion.Version.ToString(),
                    hash: ComputeFakeSha1(fullPath + client.ClientId + messageIndex),
                    iegHash: ComputeFakeSha1("ieg" + client.ClientId + messageIndex),
                    defIntegrity: ComputeFakeSha1(fullPath + "-def" + messageIndex),
                    clientIp: client.IP,
                    clientName: client.ClientId,
                    machineCode: client.ClientId,
                    os: "Windows 10"
                ));
            }

            // 进程审计: Type=2 (OPTYPE_PWL_AUDIT), SubType=6 (SUBTYPE_EXECUTEFILE)
            // 严格匹配checkbox名称，不要与其他分类混淆
            if (category == "进程审计")
            {
                var (type, subType) = LogTypeMapper.GetProcessAuditType();
                return (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildProcessAlertLog(
                    client.ClientId,
                    fullPath: fullPath,
                    parentProcess: parent,
                    userName: userName,
                    holdBack: 0, // 审计模式不阻止
                    integrityCheck: 0,
                    certCheck: 0,
                    type: type,
                    subType: subType,
                    companyName: "Microsoft Corporation",
                    productName: "Windows",
                    version: Environment.OSVersion.Version.ToString(),
                    hash: ComputeFakeSha1(fullPath + client.ClientId + messageIndex),
                    iegHash: ComputeFakeSha1("ieg" + client.ClientId + messageIndex),
                    defIntegrity: ComputeFakeSha1(fullPath + "-def" + messageIndex),
                    os: "Windows 10",
                    category: LogFieldHelper.LogCategory.ProcessControl
                ));
            }

            var parsedCategory = LogCategoryHelper.ParseDisplayName(category) ?? LogCategory.Admin;

            // 外设控制子类（禁USB/CDROM/蓝牙等，共享同一URL和CMDID，仅UsbType字段不同）
            if (LogCategoryHelper.IsExtDevCategory(parsedCategory))
            {
                var usbType = LogCategoryHelper.GetExtDevUsbType(parsedCategory) ?? 2;
                var devName = LogCategoryHelper.GetDisplayName(parsedCategory);
                return (CmdWords.SocketCmd.LogUsb, LogJsonBuilder.BuildUsbDeviceLog(
                    client.ClientId,
                    deviceType: devName,
                    deviceName: $"{devName}-event-{messageIndex % 50}",
                    state: 0,
                    usbType: usbType));
            }

            return parsedCategory switch
            {
                LogCategory.Admin => (CmdWords.SocketCmd.LogAdmin, LogJsonBuilder.BuildClientAdminLog(client.ClientId, userName, $"Simulated client operation idx={messageIndex}", success: true)),

                // LogCategory.Process 已由上面的专门if分支处理（进程审计、非白名单、白名单防篡改）

                LogCategory.VulDefense => (CmdWords.SocketCmd.LogVulDefense, LogJsonBuilder.BuildVulDefenseLog(client.ClientId, srcIp: client.IP, srcPort: srcPort, dstIp: dstIp, dstPort: dstPort)),

                LogCategory.Illegal => (CmdWords.SocketCmd.LogIllegal, LogJsonBuilder.BuildIllegalConnectLog(client.ClientId, host: $"test{messageIndex % 100}.example.com", ip: "93.184.216.34", state: 1)),

                LogCategory.Usb => (CmdWords.SocketCmd.LogUsb, LogJsonBuilder.BuildUsbDeviceLog(client.ClientId, deviceType: "USB-Storage", deviceName: $"USB-Device-{messageIndex % 100}", state: (messageIndex % 2))),

                LogCategory.UsbWarning => (CmdWords.SocketCmd.LogUsbWarning, LogJsonBuilder.BuildUsbWarningLog(client.ClientId, filePath: $"E:\\{client.ClientId}\\file-{messageIndex}.exe", operation: "Copy", userName: userName)),

                LogCategory.UDiskPlug => (CmdWords.SocketCmd.LogUsbDiskPlug, LogJsonBuilder.BuildUDiskPlugLog(client.ClientId, diskType: "Removable", diskName: $"UDisk-{messageIndex % 50}", action: (messageIndex % 2))),

                LogCategory.SafetyStore => (CmdWords.SocketCmd.LogSafetyAppStore, LogJsonBuilder.BuildSafetyStoreLog(client.ClientId, softwareName: $"TestApp{messageIndex % 20}", softwarePath: $"C:\\Program Files\\TestApp{messageIndex % 20}\\app.exe", installType: (messageIndex % 3))),

                LogCategory.DP => (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildDataProtectLog(
                    client.ClientId,
                    @operator: userName,
                    operationObject: $"C:\\Temp\\{client.ClientId}\\demo-{messageIndex}.txt",
                    action: 1,
                    result: 0)),

                LogCategory.SysGuard => (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildSysGuardLog(
                    client.ClientId,
                    subject: fullPath,
                    @object: $"C:\\Temp\\{client.ClientId}\\system-{messageIndex}.cfg",
                    action: 1,
                    result: 0)),

                // 注册表保护和强制访问控制已在前面特殊处理，此处不再列出
                // LogCategory.RegProtect 和 LogCategory.HostDefence 已在上面if分支中处理

                LogCategory.TFWarning => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventProcStartLog(
                    client.ClientId,
                    processId: 2000 + (Math.Abs(client.DeviceId.GetHashCode()) % 20000) + (messageIndex % 1000),
                    processGuid: Guid.NewGuid().ToString("B"),
                    processPath: fullPath,
                    commandLine: $"{exeName} /c whoami idx={messageIndex}")),

                LogCategory.ThreatDllLoad => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventDllLoadLog(
                    client.ClientId,
                    processId: 2000 + (Math.Abs(client.DeviceId.GetHashCode()) % 20000) + (messageIndex % 1000),
                    processGuid: Guid.NewGuid().ToString("B"),
                    processPath: fullPath,
                    targetDll: $"C:\\Windows\\System32\\malware-{messageIndex % 50}.dll")),

                LogCategory.ThreatFileAccess => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventFileAccessLog(
                    client.ClientId,
                    processId: 2000 + (Math.Abs(client.DeviceId.GetHashCode()) % 20000) + (messageIndex % 1000),
                    processGuid: Guid.NewGuid().ToString("B"),
                    processPath: fullPath,
                    filePath: $"C:\\Sensitive\\{client.ClientId}\\doc-{messageIndex % 200}.docx")),

                LogCategory.ThreatRegAccess => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventRegAccessLog(
                    client.ClientId,
                    processId: 2000 + (Math.Abs(client.DeviceId.GetHashCode()) % 20000) + (messageIndex % 1000),
                    processGuid: Guid.NewGuid().ToString("B"),
                    processPath: fullPath,
                    regKey: $"HKLM\\SOFTWARE\\{client.ClientId}\\Config\\Key{messageIndex % 100}")),

                LogCategory.ThreatOsEvent => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventOsEventLog(
                    client.ClientId,
                    eventId: 4624 + (messageIndex % 10),
                    logName: (messageIndex % 3) == 0 ? "Security" : ((messageIndex % 3) == 1 ? "System" : "Application"),
                    message: $"Simulated OS event idx={messageIndex} client={client.ClientId}")),

                LogCategory.VPScan => (CmdWords.SocketCmd.LogVirus, LogJsonBuilder.BuildVirusLog(client.ClientId, virusPath: $"C:\\Temp\\{client.ClientId}\\eicar-{messageIndex}.com", virusName: "EICAR-Test-File")),

                LogCategory.OSResource => (CmdWords.SocketCmd.LogOsResource, LogJsonBuilder.BuildOsResourceLog(client.ClientId, message: $"CPU usage high idx={messageIndex} client={client.ClientId}", resourceType: (messageIndex % 3) + 1)),

                LogCategory.FireWall => (CmdWords.SocketCmd.LogFireWall, LogJsonBuilder.BuildFireWallLog(client.ClientId, logContent: $"Firewall event idx={messageIndex} blocked connection to {dstIp}:{dstPort}", type: (messageIndex % 2))),

                // 未知分类fallback到客户端操作日志
                _ => (CmdWords.SocketCmd.LogAdmin, LogJsonBuilder.BuildClientAdminLog(client.ClientId, userName, $"Fallback: unknown category={category} idx={messageIndex}", success: true)),
            };
        }

        private async Task<bool> SendLogTcpLikeExternalAsync(string clientId, string host, int port, byte[] payload, CancellationToken ct)
        {
            // 优先复用心跳已建立的 TCP stream（对齐原项目 SetWLTcpConnectInstance：
            // WLData.cpp 始终使用与心跳相同的 CWLTcpConnect 实例发送威胁检测日志，
            // 避免同一 ComputerID 出现两条 TCP 连接被平台踢掉心跳的问题）
            var hbStream = _streamRegistry?.TryGet(clientId);
            if (hbStream != null)
            {
                // 加写锁：防止与 HeartbeatWorker 并发写同一 NetworkStream 导致包体互相穿插损坏
                bool lockAcq = await _streamRegistry!.AcquireWriteLockAsync(clientId, 2000, ct).ConfigureAwait(false);
                try
                {
                    // 重新取一次 stream（锁等待期间可能重连，stream 已更换）
                    hbStream = _streamRegistry.TryGet(clientId);
                    if (hbStream != null && lockAcq)
                    {
                        using var cts2 = CancellationTokenSource.CreateLinkedTokenSource(ct);
                        cts2.CancelAfter(2000);
                        await hbStream.WriteAsync(payload, 0, payload.Length, cts2.Token).ConfigureAwait(false);
                        await hbStream.FlushAsync(cts2.Token).ConfigureAwait(false);
                        return true;
                    }
                }
                catch
                {
                    // 心跳流已断开，降级走独立 TCP 连接路径
                }
                finally
                {
                    _streamRegistry?.ReleaseWriteLock(clientId, lockAcq);
                }
            }

            // external: SendData失败 -> CloseSocket -> CreateSocket -> 重发
            if (await SendLogTcpOnceAsync(clientId, host, port, payload, ct).ConfigureAwait(false))
            {
                return true;
            }

            await CloseTcpAsync(clientId).ConfigureAwait(false);
            return await SendLogTcpOnceAsync(clientId, host, port, payload, ct).ConfigureAwait(false);
        }

        private static string ComputeFakeSha1(string input)
        {
            using var sha1 = SHA1.Create();
            var bytes = Encoding.UTF8.GetBytes(input ?? string.Empty);
            var hash = sha1.ComputeHash(bytes);
            var sb = new StringBuilder(hash.Length * 2);
            foreach (var b in hash)
            {
                sb.Append(b.ToString("x2"));
            }
            return sb.ToString().ToUpperInvariant();
        }

        private async Task<bool> SendLogTcpOnceAsync(string clientId, string host, int port, byte[] payload, CancellationToken ct)
        {
            var gate = _tcpStateLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
            await gate.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                var state = await GetOrCreateTcpStateAsync(clientId, host, port, ct).ConfigureAwait(false);
                if (state == null) return false;

                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(TimeSpan.FromMilliseconds(2000));

                await state.Stream.WriteAsync(payload, 0, payload.Length, cts.Token).ConfigureAwait(false);
                await state.Stream.FlushAsync(cts.Token).ConfigureAwait(false);
                return true;
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch
            {
                return false;
            }
            finally
            {
                gate.Release();
            }
        }

        private async Task<TcpConnState?> GetOrCreateTcpStateAsync(string clientId, string host, int port, CancellationToken ct)
        {
            if (_tcpStates.TryGetValue(clientId, out var existing))
            {
                if (existing.IsConnectedTo(host, port))
                {
                    return existing;
                }

                await CloseTcpAsync(clientId).ConfigureAwait(false);
            }

            try
            {
                var tcp = new TcpClient
                {
                    NoDelay = true,
                };

                using (var cts = CancellationTokenSource.CreateLinkedTokenSource(ct))
                {
                    cts.CancelAfter(TimeSpan.FromMilliseconds(2000));
                    await tcp.ConnectAsync(host, port, cts.Token).ConfigureAwait(false);
                }

                var stream = tcp.GetStream();
                var state = new TcpConnState(clientId, host, port, tcp, stream);
                state.StartReceiveLoop();
                _tcpStates[clientId] = state;
                return state;
            }
            catch
            {
                return null;
            }
        }

        private Task CloseTcpAsync(string clientId)
        {
            if (_tcpStates.TryRemove(clientId, out var state))
            {
                state.Close();
            }
            return Task.CompletedTask;
        }

        private Task CloseAllTcpAsync()
        {
            foreach (var key in _tcpStates.Keys)
            {
                _ = CloseTcpAsync(key);
            }
            _tcpStates.Clear();
            return Task.CompletedTask;
        }

        private sealed class TcpConnState
        {
            public string ClientId { get; }
            public string Host { get; }
            public int Port { get; }
            public TcpClient Tcp { get; }
            public NetworkStream Stream { get; }

            private readonly CancellationTokenSource _cts = new();
            private Task? _receiveTask;

            private int _recvLogged = 0;
            private const int MaxRecvLog = 20;

            public TcpConnState(string clientId, string host, int port, TcpClient tcp, NetworkStream stream)
            {
                ClientId = clientId;
                Host = host;
                Port = port;
                Tcp = tcp;
                Stream = stream;
            }

            public bool IsConnectedTo(string host, int port)
            {
                if (!Tcp.Connected) return false;
                if (!string.Equals(Host, host, StringComparison.OrdinalIgnoreCase)) return false;
                return Port == port;
            }

            public void StartReceiveLoop()
            {
                _receiveTask = Task.Run(() => ReceiveLoopAsync(_cts.Token));
            }

            public void Close()
            {
                try { _cts.Cancel(); } catch { }
                try { Stream.Close(); } catch { }
                try { Tcp.Close(); } catch { }
                try { _receiveTask?.Wait(200); } catch { }
            }

            private async Task ReceiveLoopAsync(CancellationToken ct)
            {
                var header = new byte[PtProtocol.HeaderLength];
                try
                {
                    while (!ct.IsCancellationRequested)
                    {
                        bool okHeader = await ReadExactAsync(Stream, header, header.Length, ct).ConfigureAwait(false);
                        if (!okHeader) break;

                        if (header[0] != (byte)'P' || header[1] != (byte)'T')
                        {
                            // 非 PT：尽量继续读（这里直接退出，避免错位导致死循环）
                            break;
                        }

                        ushort bodyLen = BinaryPrimitives.ReadUInt16BigEndian(header.AsSpan(4, 2));
                        if (bodyLen == 0 || bodyLen > 65535) break;

                        var body = new byte[bodyLen];
                        bool okBody = await ReadExactAsync(Stream, body, body.Length, ct).ConfigureAwait(false);
                        if (!okBody) break;

                        // 仅记录少量回包，避免日志暴涨。
                        if (Interlocked.Increment(ref _recvLogged) <= MaxRecvLog)
                        {
                            TryDebugWriteReceivedPacket(ClientId, header, body);
                        }
                    }
                }
                catch { }
            }

            private static async Task<bool> ReadExactAsync(NetworkStream stream, byte[] buffer, int len, CancellationToken ct)
            {
                int readTotal = 0;
                while (readTotal < len)
                {
                    int read = await stream.ReadAsync(buffer, readTotal, len - readTotal, ct).ConfigureAwait(false);
                    if (read <= 0) return false;
                    readTotal += read;
                }
                return true;
            }
        }

        private void TryDebugWriteSentPacket(string clientId, byte[] ptPacket, string json)
        {
            try
            {
                int cur = _debugSendCount.AddOrUpdate(clientId, 1, (_, v) => v + 1);
                if (cur > MaxDebugSendPerClient) return;

                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-packets-{DateTime.UtcNow:yyyyMMdd}.log");

                var header = ptPacket.AsSpan(0, PtProtocol.HeaderLength);
                ushort bodyLen = BinaryPrimitives.ReadUInt16BigEndian(header.Slice(4, 2));
                byte enc = header[6];
                byte comp = header[7];
                uint serial = BinaryPrimitives.ReadUInt32BigEndian(header.Slice(12, 4));
                uint crc = BinaryPrimitives.ReadUInt32BigEndian(header.Slice(16, 4));
                uint cmd = BinaryPrimitives.ReadUInt32BigEndian(header.Slice(32, 4));
                uint devid = BinaryPrimitives.ReadUInt32BigEndian(header.Slice(36, 4));
                ushort srcLen = BinaryPrimitives.ReadUInt16BigEndian(header.Slice(40, 2));

                var jsonPreview = json.Length > 300 ? json.Substring(0, 300) : json;
                var line = $"{DateTime.UtcNow:o} SEND client={clientId} cmd={cmd} devid={devid} serial={serial} srcLen={srcLen} bodyLen={bodyLen} enc={enc} comp={comp} crc32=0x{crc:X8} jsonPreview={jsonPreview.Replace("\r", "\\r").Replace("\n", "\\n")}";
                File.AppendAllText(logPath, line + Environment.NewLine);
            }
            catch { }
        }

        private static void TryDebugWriteReceivedPacket(string clientId, byte[] header, byte[] body)
        {
            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logrecv-{DateTime.UtcNow:yyyyMMdd}.log");

                uint serial = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(12, 4));
                uint cmd = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(32, 4));
                uint devid = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(36, 4));
                ushort srcLen = BinaryPrimitives.ReadUInt16BigEndian(header.AsSpan(40, 2));

                string srcPreview;
                try
                {
                    var packet = new byte[PtProtocol.HeaderLength + body.Length];
                    Buffer.BlockCopy(header, 0, packet, 0, PtProtocol.HeaderLength);
                    Buffer.BlockCopy(body, 0, packet, PtProtocol.HeaderLength, body.Length);
                    var src = PtProtocol.Unpack(packet);
                    var s = Encoding.UTF8.GetString(src);
                    srcPreview = s.Length > 300 ? s.Substring(0, 300) : s;
                }
                catch (Exception ex)
                {
                    srcPreview = "<unpack_failed:" + ex.GetType().Name + ">";
                }

                var line = $"{DateTime.UtcNow:o} RECV client={clientId} cmd={cmd} devid={devid} serial={serial} srcLen={srcLen} srcPreview={srcPreview.Replace("\r", "\\r").Replace("\n", "\\n")}";
                File.AppendAllText(logPath, line + Environment.NewLine);
            }
            catch { }
        }
    }
}
