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
using System.Net.Sockets;
using SimulatorLib.Protocol;
using System.Buffers.Binary;
using System.Net.Http;

namespace SimulatorLib.Workers
{
    public record LogSendStats(int TotalMessages, int Success, int Failed);

    public class LogWorker
    {
        private readonly INetworkSender _tcpSender;
        private readonly IUdpSender? _udpSender;

        private readonly ConcurrentDictionary<string, TcpConnState> _tcpStates = new();
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _tcpStateLocks = new();

        private uint _serialCounter = 0;

        private readonly ConcurrentDictionary<string, int> _debugSendCount = new();
        private const int MaxDebugSendPerClient = 3;

        // Aligned with external/IEG_Code/code/include/format/commurl.h
        private const string HttpsPathProcess = "/USM/clientAlertlog.do";
        private const string HttpsPathUsb = "/USM/clientULog.do";
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

        private const int MaxDebugHttps = 10;
        private int _httpsDebugCount = 0;

        public LogWorker(INetworkSender tcpSender, IUdpSender? udpSender = null)
        {
            _tcpSender = tcpSender;
            _udpSender = udpSender;
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
                ct: ct,
                progress: progress).ConfigureAwait(false);
        }

        /// <summary>
        /// 增强版：支持
        /// - maxClients：仅对前 N 个客户端发日志（由 Clients.log 顺序决定）
        /// - messagesPerSecondPerClient：每客户端每秒条数（<=0 或 null 表示不做速率控制）
        /// - categories：日志分类（为空则用 Default）；会按消息序号轮转
        /// </summary>
        public async Task StartAsync(int messagesPerClient, int? messagesPerSecondPerClient, int? maxClients, IReadOnlyList<string>? categories, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, CancellationToken ct, IProgress<LogSendStats>? progress = null)
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
                ct: ct,
                progress: progress).ConfigureAwait(false);
        }

        private async Task StartCoreAsync(int messagesPerClient, int? messagesPerSecondPerClient, int? maxClients, IReadOnlyList<string>? categories, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, CancellationToken ct, IProgress<LogSendStats>? progress)
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

            // 用户需求：每秒发送1条已勾选的不同的日志类型；直到遍历完已勾选的日志分类；然后再次循环，直到满足总条数
            // 全局串行发送：totalMessages = clientCount * messagesPerClient
            // 每次发送：选择 client (轮询) + category (全局轮换)
            var intervalMs = 1000; // 每秒1条

            var categoryList = (categories != null && categories.Count > 0)
                ? categories
                : new[] { "Default" };

            using var http = CreateHttpClient(timeoutMs: 4000);

            try
            {
                // 全局串行发送所有消息
                for (int globalIndex = 0; globalIndex < totalMessages && !ct.IsCancellationRequested; globalIndex++)
                {
                    if (globalIndex > 0)
                    {
                        await Task.Delay(intervalMs, ct).ConfigureAwait(false);
                    }

                    // 轮询选择客户端
                    var clientIndex = globalIndex % clients.Count;
                    var c = clients[clientIndex];

                    // 全局轮换日志类别
                    var categoryIndex = globalIndex % categoryList.Count;
                    var cat = categoryList[categoryIndex];

                    var (socketCmd, json) = BuildLogByCategory(cat, c, messageIndex: globalIndex);
                    bool ok;

                    try
                    {
                        if (IsThreatDataCategory(cat))
                        {
                            // 对齐原项目 external/WLCData/WLThreatLog.cpp + WLData.cpp：
                            // - 默认（未启用日志服务器）：TCP/PT → 平台（zlib + 不加密）
                            // - 启用日志服务器：UDP/PT → 日志服务器（zlib + AES）
                            var src = GetThreatDataJsonBytes(json);
                            uint serial = unchecked(++_serialCounter);
                            var pt = PtProtocol.Pack(
                                src,
                                cmdId: socketCmd,
                                compressType: PtCompressType.Zlib,
                                encryptType: useLogServer ? PtEncryptType.Aes : PtEncryptType.None,
                                deviceId: c.DeviceId,
                                serialNumber: serial);
                            TryDebugWriteSentPacket(c.ClientId, pt, json);

                            if (useLogServer)
                            {
                                if (_udpSender != null && !string.IsNullOrEmpty(logHost))
                                {
                                    ok = await _udpSender.SendUdpAsync(logHost, logPort, pt, 2000, ct).ConfigureAwait(false);
                                }
                                else
                                {
                                    ok = false;
                                }
                            }
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
                                // external/WLMainDataHandle/AccessServer.cpp:
                                // em_LogDP/em_LogSysGuard only supported via HTTPS; non-https LogServer path does not cover them.
                                var path = GetHttpsPathForCategory(cat);
                                ok = await PostLogHttpsAsync(http, platformHost, platformPort, path, json, ct).ConfigureAwait(false);
                            }
                            else
                            {
                                // 对齐原项目：LogServer/type=udp 时，日志走 UDP → 日志服务器。
                                var src = GetSocketJsonBytes(json);
                                uint serial = unchecked(++_serialCounter);
                                var pt = PtProtocol.Pack(src, cmdId: socketCmd, deviceId: c.DeviceId, serialNumber: serial);
                                TryDebugWriteSentPacket(c.ClientId, pt, json);

                                if (_udpSender != null && !string.IsNullOrEmpty(logHost))
                                {
                                    ok = await _udpSender.SendUdpAsync(logHost, logPort, pt, 2000, ct).ConfigureAwait(false);
                                }
                                else
                                {
                                    ok = false;
                                }
                            }
                        }
                        else
                        {
                            // 对齐原项目：LogServer/type=https(默认) 时，日志走 HTTPS → 平台。
                            var path = GetHttpsPathForCategory(cat);
                            ok = await PostLogHttpsAsync(http, platformHost, platformPort, path, json, ct).ConfigureAwait(false);
                        }

                        if (ok) Interlocked.Increment(ref success);
                        else Interlocked.Increment(ref fail);
                    }
                    catch
                    {
                        Interlocked.Increment(ref fail);
                    }

                    MaybeReportProgress();
                }

            }
            finally
            {
                await CloseAllTcpAsync().ConfigureAwait(false);
            }

            var stats = new LogSendStats(totalMessages, success, fail);
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
                LogCategory.RegProtect => HttpsPathSysGuard, // 注册表保护使用相同endpoint
                LogCategory.HostDefence => HttpsPathHostDefence,
                LogCategory.Usb => HttpsPathUsb,
                LogCategory.FireWall => HttpsPathFireWall,
                LogCategory.TFWarning => HttpsPathThreatFake,
                _ => HttpsPathProcess,
            };
        }

        private static HttpClient CreateHttpClient(int timeoutMs)
        {
            var handler = new HttpClientHandler
            {
                ServerCertificateCustomValidationCallback = HttpClientHandler.DangerousAcceptAnyServerCertificateValidator,
            };
            return new HttpClient(handler)
            {
                Timeout = TimeSpan.FromMilliseconds(Math.Max(500, timeoutMs)),
            };
        }

        private async Task<bool> PostLogHttpsAsync(HttpClient http, string host, int port, string path, string json, CancellationToken ct)
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
                return resp.IsSuccessStatusCode;
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch (Exception ex)
            {
                TryDebugWriteHttps(host, port, path, null, ex.GetType().Name + ":" + ex.Message);
                return false;
            }
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

            // 特殊处理：文件保护也使用HostDefence类型，但Level2=1
            if (category == "文件保护")
            {
                return (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"C:\\Temp\\{client.ClientId}\\protected-{messageIndex}.txt",
                    processName: exeName,
                    userName: userName,
                    logContent: $"文件[C:\\Temp\\{client.ClientId}\\protected-{messageIndex}.txt]被{exeName}非法修改",
                    detailLogTypeLevel2: 1,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_FILEROTECT
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
                    machineCode: client.ClientId
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
                    machineCode: client.ClientId
                ));
            }

            var parsedCategory = LogCategoryHelper.ParseDisplayName(category) ?? LogCategory.Process;
            return parsedCategory switch
            {
                LogCategory.Admin => (CmdWords.SocketCmd.LogAdmin, LogJsonBuilder.BuildClientAdminLog(client.ClientId, userName, $"Simulated client operation idx={messageIndex}", success: true)),

                LogCategory.Process =>
                (
                    CmdWords.SocketCmd.LogProcess,
                    // Fill realistic-looking fake fields so platform displays as real
                    LogJsonBuilder.BuildProcessAlertLog(
                        client.ClientId,
                        fullPath: fullPath,
                        parentProcess: parent,
                        userName: userName,
                        holdBack: (messageIndex % 7 == 0) ? 1 : 0,
                        integrityCheck: 0,
                        certCheck: 0,
                        type: 0,
                        subType: 0,
                        companyName: "Microsoft Corporation",
                        productName: "Windows",
                        version: Environment.OSVersion.Version.ToString(),
                        hash: ComputeFakeSha1(fullPath + client.ClientId + messageIndex),
                        iegHash: ComputeFakeSha1("ieg" + client.ClientId + messageIndex),
                        defIntegrity: ComputeFakeSha1(fullPath + "-def" + messageIndex)
                    )
                ),

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

                LogCategory.RegProtect => (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"HKLM\\Software\\Demo\\Key{messageIndex % 10}",
                    processName: exeName,
                    userName: userName,
                    logContent: $"注册表项[HKLM\\Software\\Demo\\Key{messageIndex % 10}]被{exeName}修改",
                    detailLogTypeLevel2: 2,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_REGPROTECT
                    blocked: false)),

                LogCategory.HostDefence => (CmdWords.SocketCmd.LogHostDefence, LogJsonBuilder.BuildHostDefenceLog(
                    client.ClientId,
                    fullPath: $"C:\\Temp\\{client.ClientId}\\demo-{messageIndex}.txt",
                    processName: exeName,
                    userName: userName,
                    logContent: $"强制访问控制: 进程{exeName}访问文件被阻止",
                    detailLogTypeLevel2: 4,  // WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_MACPROTECT
                    blocked: true)),

                LogCategory.TFWarning => (CmdWords.SocketCmd.LogThreat, LogJsonBuilder.BuildThreatEventProcStartLog(
                    client.ClientId,
                    processId: 2000 + (Math.Abs(client.DeviceId.GetHashCode()) % 20000) + (messageIndex % 1000),
                    processGuid: Guid.NewGuid().ToString("B"),
                    processPath: fullPath,
                    commandLine: $"{exeName} /c whoami idx={messageIndex}")),

                LogCategory.VPScan => (CmdWords.SocketCmd.LogVirus, LogJsonBuilder.BuildVirusLog(client.ClientId, virusPath: $"C:\\Temp\\{client.ClientId}\\eicar-{messageIndex}.com", virusName: "EICAR-Test-File")),

                LogCategory.OSResource => (CmdWords.SocketCmd.LogOsResource, LogJsonBuilder.BuildOsResourceLog(client.ClientId, message: $"CPU usage high idx={messageIndex} client={client.ClientId}", resourceType: (messageIndex % 3) + 1)),

                LogCategory.FireWall => (CmdWords.SocketCmd.LogFireWall, LogJsonBuilder.BuildFireWallLog(client.ClientId, logContent: $"Firewall event idx={messageIndex} blocked connection to {dstIp}:{dstPort}", type: (messageIndex % 2))),

                _ => (CmdWords.SocketCmd.LogProcess, LogJsonBuilder.BuildProcessAlertLog(
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
                    machineCode: client.ClientId
                )),
            };
        }

        private async Task<bool> SendLogTcpLikeExternalAsync(string clientId, string host, int port, byte[] payload, CancellationToken ct)
        {
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
