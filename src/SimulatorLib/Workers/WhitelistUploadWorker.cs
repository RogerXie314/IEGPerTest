using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Security;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Models;
using SimulatorLib.Network;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public record UploadStats(int Total, int Success, int Failed);

    /// <summary>
    /// 白名单上传 Worker — 随机轮转策略：
    ///   • 每轮从已注册客户端中，随机选取尚未上传过的 N 台进行上传
    ///   • 每台客户端在一个任务生命周期内只上传一次（除非重新 AddTask）
    ///   • 每 cycleIntervalMs（默认15分钟）自动切换到下一批
    ///   • 所有客户端都上传完后自动结束
    ///
    /// 协议对齐 SendInfoToServer.cpp::Send_FileLog_WL_ToServer：
    ///   ① 上传前：POST /USM/clientScanStatus.do  SolidifyStatus=11（WL_SOLIDIFY_UPLOAD）
    ///   ② 文件上传：POST https://{host}:{port}/USM/upLoadSysWhiteFile.do?id={b64url(computerID)}&filepath={b64url(modPath)}
    ///      multipart/form-data，字段名 "file"，使用用户选择的白名单原始文件
    ///   ③ 上传成功后：POST /USM/clientScanStatus.do  SolidifyStatus=10（WL_SOLIDIFY_UPDATE）
    /// </summary>
    public class WhitelistUploadWorker
    {
        // DATA_TO_SERVER_SCANSTATUS = 211（WLCmdWordDef.h）
        private const int CmdTypeScanStatus = 200;
        private const int CmdIdScanStatus   = 211;
        // WL_SOLIDIFY_UPLOAD = 11, WL_SOLIDIFY_UPDATE = 10（WLServerTest.h）
        private const int SolidifyUploading = 11;
        private const int SolidifyUpdated   = 10;

        private readonly INetworkSender _tcpSender; // 保留以兼容构造签名
        private static readonly Random _rng = new();

        public WhitelistUploadWorker(INetworkSender tcpSender)
        {
            _tcpSender = tcpSender;
        }

        /// <summary>
        /// 随机轮转上传模式（主要入口）：
        ///   每 cycleIntervalMs 毫秒选取新一批，直到全部上传完或取消。
        /// </summary>
        public async Task RunRotatingAsync(
            string filePath, int clientCount,
            string platformHost, int platformPort,
            int concurrency,
            int cycleIntervalMs,
            TaskRecord record,
            CancellationToken ct)
        {
            if (!File.Exists(filePath)) throw new FileNotFoundException("白名单文件不存在", filePath);

            // 本次 AddTask 的已上传集合（ClientId 去重）
            var uploadedSet = new HashSet<string>(StringComparer.Ordinal);
            int cycleNo = 0;

            while (!ct.IsCancellationRequested)
            {
                cycleNo++;

                // 读取当前已注册客户端列表
                var allClients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                var remaining  = allClients
                    .Where(c => c.Status == "Registered" && !uploadedSet.Contains(c.ClientId))
                    .ToList();

                if (remaining.Count == 0)
                {
                    record.Detail = $"全部客户端已上传完成（共 {uploadedSet.Count} 台），任务结束";
                    record.MarkCompleted();
                    return;
                }

                // 随机打乱后取前 N 台
                Shuffle(remaining);
                var targets = remaining.Take(Math.Max(1, clientCount)).ToList();

                record.Detail = $"第{cycleNo}轮：选取 {targets.Count} 台（已上传 {uploadedSet.Count}，剩余可选 {remaining.Count}）";

                var stats = await RunBatchAsync(filePath, targets, platformHost, platformPort, concurrency, ct)
                    .ConfigureAwait(false);

                // 将本轮发送的客户端（成功或失败）均加入已上传集合，避免重复
                foreach (var t in targets)
                    uploadedSet.Add(t.ClientId);

                record.SuccessCount += stats.Success;
                record.FailCount    += stats.Failed;

                await WriteLogAsync(filePath, stats, cycleNo).ConfigureAwait(false);

                // 等待下一轮（支持提前取消）
                try
                {
                    record.Detail = $"第{cycleNo}轮完成（成功{stats.Success}/失败{stats.Failed}），{cycleIntervalMs / 1000}s 后开始下一轮…";
                    await Task.Delay(cycleIntervalMs, ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
            }

            record.MarkStopped();
        }

        // ── 内部实现 ────────────────────────────────────────────────────────────

        private static HttpClient CreateHttpClient()
        {
            var handler = new HttpClientHandler
            {
                // 平台自签名证书，忽略验证（对齐老工具 libcurl 关闭证书校验）
                ServerCertificateCustomValidationCallback = (_, _, _, _) => true,
                AllowAutoRedirect = false,
            };
            return new HttpClient(handler, disposeHandler: true)
            {
                Timeout = TimeSpan.FromSeconds(30),
            };
        }

        private async Task<UploadStats> RunBatchAsync(
            string filePath, List<ClientRecord> targets,
            string platformHost, int platformPort,
            int concurrency, CancellationToken ct)
        {
            var sem = new SemaphoreSlim(Math.Max(1, concurrency));
            int statsSuccess = 0, statsFail = 0;
            var tasks = new List<Task>(targets.Count);

            // 每批共享一个 HttpClient（连接复用），跨批不复用（避免连接池积压）
            using var http = CreateHttpClient();

            var fileName = Path.GetFileName(filePath);

            foreach (var c in targets)
            {
                var client = c; // 闭包捕获
                await sem.WaitAsync(ct).ConfigureAwait(false);
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var ok = await UploadOneClientAsync(
                            http, client, filePath, fileName,
                            platformHost, platformPort, ct).ConfigureAwait(false);
                        if (ok) Interlocked.Increment(ref statsSuccess);
                        else    Interlocked.Increment(ref statsFail);
                    }
                    catch { Interlocked.Increment(ref statsFail); }
                    finally { sem.Release(); }
                }, ct));
            }

            await Task.WhenAll(tasks).ConfigureAwait(false);
            return new UploadStats(targets.Count, statsSuccess, statsFail);
        }

        /// <summary>
        /// 对齐 Send_FileLog_WL_ToServer：
        ///   1. 上报状态 UPLOADING(11)
        ///   2. multipart 上传白名单文件
        ///   3. 上传成功后上报状态 UPDATED(10)
        /// </summary>
        private static async Task<bool> UploadOneClientAsync(
            HttpClient http,
            ClientRecord c,
            string filePath, string fileName,
            string host, int port,
            CancellationToken ct)
        {
            var baseUrl = $"https://{host}:{port}";

            // ① 上传前状态通知
            await SendScanStatusAsync(http, baseUrl, c.ClientId, SolidifyUploading, ct)
                .ConfigureAwait(false);

            // ② 构造上传 URL（对齐 C++：去掉 ':'，'\\' → '-'，再 Base64URL 编码）
            var idB64       = Base64UrlEncode(c.ClientId);
            var modPath     = BuildModifiedPath(filePath);
            var pathB64     = Base64UrlEncode(modPath);
            var uploadUrl   = $"{baseUrl}/USM/upLoadSysWhiteFile.do?id={idB64}&filepath={pathB64}";

            // ③ multipart/form-data 上传（对齐 libcurl puploadFile）
            bool uploadOk = false;
            try
            {
                await using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read, 65536, useAsync: true);
                using var multipart = new MultipartFormDataContent();
                using var sc = new StreamContent(fs);
                multipart.Add(sc, "file", fileName);

                var resp = await http.PostAsync(uploadUrl, multipart, ct).ConfigureAwait(false);
                uploadOk = resp.IsSuccessStatusCode;
            }
            catch { /* 上传异常计为失败，不再上报 UPDATED */ }

            // ④ 上传成功后状态通知
            if (uploadOk)
            {
                await SendScanStatusAsync(http, baseUrl, c.ClientId, SolidifyUpdated, ct)
                    .ConfigureAwait(false);
            }

            return uploadOk;
        }

        /// <summary>
        /// 对应 sendScanStatus(lpGuid, WL_SOLIDIFY_xxx)：
        ///   POST /USM/clientScanStatus.do
        ///   Body: [{"ComputerID":"...","CMDTYPE":200,"CMDID":211,"CMDContent":{"SolidifyStatus":N,"SolidifySpeed":0,"Time":"..."}}]
        /// </summary>
        private static async Task SendScanStatusAsync(
            HttpClient http, string baseUrl, string computerId,
            int solidifyStatus, CancellationToken ct)
        {
            try
            {
                var time = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
                var json = JsonSerializer.Serialize(new[]
                {
                    new
                    {
                        ComputerID = computerId,
                        CMDTYPE    = CmdTypeScanStatus,
                        CMDID      = CmdIdScanStatus,
                        CMDContent = new
                        {
                            SolidifyStatus = solidifyStatus,
                            SolidifySpeed  = 0,
                            Time           = time,
                        }
                    }
                });
                using var content = new StringContent(json, Encoding.UTF8, "application/json");
                await http.PostAsync($"{baseUrl}/USM/clientScanStatus.do", content, ct)
                    .ConfigureAwait(false);
            }
            catch { /* 状态通知失败不影响主流程 */ }
        }

        /// <summary>
        /// 对齐 C++：去掉 ':'，把 '\\' 替换为 '-'（原路径用于服务端记录）。
        /// </summary>
        private static string BuildModifiedPath(string path)
        {
            var sb = new StringBuilder(path.Length);
            foreach (var ch in path)
            {
                if (ch == ':') continue;
                sb.Append(ch == '\\' ? '-' : ch);
            }
            return sb.ToString();
        }

        /// <summary>Base64URL 编码（无 padding），对应 C++ CBase64::Base64URLEncode。</summary>
        private static string Base64UrlEncode(string value)
        {
            var bytes = Encoding.UTF8.GetBytes(value);
            return Convert.ToBase64String(bytes)
                .Replace('+', '-')
                .Replace('/', '_')
                .TrimEnd('=');
        }

        private static async Task WriteLogAsync(string filePath, UploadStats stats, int cycle = 0)
        {
            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"upload-{DateTime.UtcNow:yyyyMMdd}.log");
                var cycleTag = cycle > 0 ? $" Cycle={cycle}" : string.Empty;
                var line = $"{DateTime.UtcNow:o}{cycleTag} File={Path.GetFileName(filePath)}" +
                           $" Total={stats.Total} Success={stats.Success} Fail={stats.Failed}";
                await File.AppendAllTextAsync(logPath, line + Environment.NewLine, Encoding.UTF8)
                    .ConfigureAwait(false);
            }
            catch { }
        }

        private static void Shuffle<T>(List<T> list)
        {
            for (int i = list.Count - 1; i > 0; i--)
            {
                int j = _rng.Next(i + 1);
                (list[i], list[j]) = (list[j], list[i]);
            }
        }
    }
}
