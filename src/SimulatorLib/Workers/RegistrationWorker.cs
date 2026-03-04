using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Protocol;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class RegistrationWorker
    {
        public RegistrationWorker(INetworkSender? sender = null)
        {
            _ = sender;
        }

        /// <summary>
        /// 多轮并发注册：
        ///   Phase 1 — 预生成所有客户端记录（status=Generated），一次性写入 Clients.log；
        ///   Phase 2 — 外层轮次循环：
        ///               每轮并发注册当前 pending 列表（SemaphoreSlim 限制并发数）；
        ///               本轮失败的客户端进入下一轮，等待 retryIntervalMs 后再次并发注册全部失败客户端；
        ///               直到 pending 为空（全部成功）或用户取消；
        ///   Phase 3 — 用最终状态覆写 Clients.log。
        /// </summary>
        public async Task<RegistrationSummary> RegisterAsync(
            string clientIdPrefix, int startIndex, int count,
            string startIp = "192.168.0.1", string host = "localhost", int port = 8441,
            int concurrency = 20, int retry = 3, int timeoutMs = 3000,
            int retryIntervalMs = 30000,
            string clientVersion = "V300R011C01B090",
            string? windowsVersion = null,
            IProgress<RegistrationRoundProgress>? roundProgress = null,
            CancellationToken ct = default)
        {
            // ── Phase 1: 预生成 ────────────────────────────────────────────────────
            var preRecords = new List<ClientRecord>(count);
            for (int i = 0; i < count; i++)
            {
                var clientId = clientIdPrefix + (startIndex + i).ToString();
                var ip = IncrementIPv4(startIp, i);
                preRecords.Add(new ClientRecord(clientId, ip, DateTime.UtcNow, "Generated"));
            }
            await ClientsPersistence.WriteAllAsync(preRecords).ConfigureAwait(false);

            // ── Phase 2: 多轮并发注册 ──────────────────────────────────────────────
            using var http = CreateHttpClient(timeoutMs);
            var loginUrl  = new Uri($"https://{host}:{port}/USM/clientLogin.do");
            var resultUrl = new Uri($"https://{host}:{port}/USM/clientResult.do");

            // 以 clientId 为键，维护所有客户端的最终状态
            var finalDict = new Dictionary<string, ClientRecord>(count, StringComparer.Ordinal);
            foreach (var r in preRecords) finalDict[r.ClientId] = r;

            var allFailureReasons = new ConcurrentDictionary<string, int>(StringComparer.Ordinal);
            int totalSuccess = 0;
            int round = 0;
            var pendingList = new List<ClientRecord>(preRecords);

            while (pendingList.Count > 0 && !ct.IsCancellationRequested)
            {
                round++;
                var sem = new SemaphoreSlim(concurrency);
                var roundFailedBag    = new ConcurrentBag<ClientRecord>();
                var roundReasonsBag   = new ConcurrentDictionary<string, int>(StringComparer.Ordinal);
                int roundSuccess = 0;
                int roundFailed  = 0;

                var tasks = new List<Task>(pendingList.Count);
                foreach (var rec in pendingList)
                {
                    await sem.WaitAsync(ct).ConfigureAwait(false);
                    var capturedRec = rec;
                    tasks.Add(Task.Run(async () =>
                    {
                        try
                        {
                            var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(capturedRec.IP);
                            var reqJson = UsmRegistrationJsonBuilder.BuildClientLoginJson(
                                computerId: capturedRec.ClientId,
                                username: "SysAdmin",
                                computerName: capturedRec.ClientId,
                                computerIp: capturedRec.IP,
                                computerMac: mac,
                                windowsVersion: windowsVersion ?? OsInfo.GetWindowsVersionName(),
                                windowsX64: Environment.Is64BitOperatingSystem,
                                proxying: 0,
                                licenseRecycle: false,
                                clientType: 0,
                                clientVersion: clientVersion);

                            bool ok = false;
                            uint deviceId = 0;
                            int  tcpPort  = 0;
                            string lastFailReason = "未知失败";
                            const int RetryDelayMs = 1000;

                            for (int attempt = 1; attempt <= retry && !ok && !ct.IsCancellationRequested; attempt++)
                            {
                                try
                                {
                                    using var content = new StringContent(reqJson, Encoding.UTF8, "application/json");
                                    using var resp    = await http.PostAsync(loginUrl, content, ct).ConfigureAwait(false);
                                    var respText      = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);

                                    if (!resp.IsSuccessStatusCode)
                                    {
                                        lastFailReason = $"HTTP错误({(int)resp.StatusCode})";
                                        await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                                        continue;
                                    }

                                    if (!UsmResponseParsers.TryCheckSetupResult(respText, out var errCode, out _))
                                    {
                                        lastFailReason = ErrCodeToReason(errCode);
                                        await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                                        continue;
                                    }

                                    if (UsmResponseParsers.TryParseConfigInfo(respText, out var cfg))
                                    {
                                        deviceId = cfg.DeviceId;
                                        tcpPort  = cfg.TcpPort;
                                    }

                                    var resultJson = UsmRegistrationJsonBuilder.BuildClientResultJson(
                                        computerId: capturedRec.ClientId,
                                        username: "SysAdmin",
                                        cmdType: UsmRegistrationJsonBuilder.SetupCmdType,
                                        cmdId: UsmRegistrationJsonBuilder.CmdClientRegistry,
                                        dealResult: 0);

                                    using var resultContent = new StringContent(resultJson, Encoding.UTF8, "application/json");
                                    using var resultResp    = await http.PostAsync(resultUrl, resultContent, ct).ConfigureAwait(false);

                                    if (!resultResp.IsSuccessStatusCode)
                                    {
                                        lastFailReason = $"结果上报失败(HTTP{(int)resultResp.StatusCode})";
                                        await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                                        continue;
                                    }

                                    ok = true;
                                }
                                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                                {
                                    // 用户主动取消，向上传播
                                    throw;
                                }
                                catch (OperationCanceledException)
                                {
                                    // HttpClient 内部超时（TaskCanceledException），不是用户取消
                                    // 当作超时/网络异常处理，继续重试
                                    lastFailReason = "超时/网络异常";
                                    try { await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false); } catch { }
                                }
                                catch
                                {
                                    lastFailReason = "超时/网络异常";
                                    try { await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false); } catch { }
                                }
                            }

                            if (ok)
                            {
                                Interlocked.Increment(ref roundSuccess);
                                var successRec = capturedRec with
                                {
                                    Status = "Registered",
                                    RegisteredAt = DateTime.UtcNow,
                                    DeviceId = deviceId,
                                    TcpPort  = tcpPort,
                                };
                                // 线程安全更新字典（同一 clientId 不会被两个线程同时写）
                                lock (finalDict) { finalDict[capturedRec.ClientId] = successRec; }
                            }
                            else
                            {
                                Interlocked.Increment(ref roundFailed);
                                roundFailedBag.Add(capturedRec with { Status = "Failed" });
                                roundReasonsBag.AddOrUpdate(lastFailReason, 1, (_, c) => c + 1);
                                lock (finalDict) { finalDict[capturedRec.ClientId] = capturedRec with { Status = "Failed" }; }
                            }
                        }
                        catch (OperationCanceledException) { }
                        catch { }
                        finally
                        {
                            sem.Release();
                        }
                    }, ct));
                }

                await Task.WhenAll(tasks).ConfigureAwait(false);

                Interlocked.Add(ref totalSuccess, roundSuccess);
                foreach (var kv in roundReasonsBag)
                    allFailureReasons.AddOrUpdate(kv.Key, kv.Value, (_, c) => c + kv.Value);

                pendingList = new List<ClientRecord>(roundFailedBag);

                // 每轮结束后报告进度（供 UI 实时更新）
                roundProgress?.Report(new RegistrationRoundProgress(
                    Round: round,
                    RoundSuccess: roundSuccess,
                    RoundFailed: roundFailed,
                    TotalSuccess: totalSuccess,
                    Remaining: pendingList.Count));

                if (pendingList.Count > 0 && !ct.IsCancellationRequested)
                {
                    // 持久化当前状态（包含已成功+仍失败的记录），便于查看中间进度
                    List<ClientRecord> snapshot;
                    lock (finalDict) { snapshot = new List<ClientRecord>(finalDict.Values); }
                    await ClientsPersistence.WriteAllAsync(snapshot).ConfigureAwait(false);

                    // 等待重试间隔
                    await Task.Delay(retryIntervalMs, ct).ConfigureAwait(false);
                }
            }

            // ── Phase 3: 最终状态写回 Clients.log ──────────────────────────────────
            // 按原始生成顺序输出，保持文件有序
            var orderedFinal = new List<ClientRecord>(count);
            foreach (var r in preRecords)
                orderedFinal.Add(finalDict.TryGetValue(r.ClientId, out var fr) ? fr : r);
            await ClientsPersistence.WriteAllAsync(orderedFinal).ConfigureAwait(false);

            // 用「总数 - 累计成功」计算最终失败数，避免「Generated/消失」状态被误算
            int finalFailed = count - totalSuccess;

            return new RegistrationSummary
            {
                Total   = count,
                Success = totalSuccess,
                Failed  = finalFailed,
                Rounds  = round,
                FailureReasons = new Dictionary<string, int>(allFailureReasons),
            };
        }

        /// <summary>
        /// 将 USM 返回的 errCode 映射为人类可读的失败原因描述，对应原项目 WLMainDataHandle 中的错误码枚举。
        /// </summary>
        private static string ErrCodeToReason(int errCode) => errCode switch
        {
            1  => "连接服务器失败(1)",
            2  => "节点数为空(2)",
            3  => "JSON解析失败(3)",
            4  => "许可证错误(4)",
            6  => "IP重复(6)",
            8  => "服务器繁忙(8)",
            12 => "资产信息错误(12)",
            _  => $"服务端拒绝({errCode})",
        };

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

        private static string IncrementIPv4(string startIp, int offset)
        {
            // startIp 在 UI 层已校验；这里仍尽量做防御性处理
            if (string.IsNullOrWhiteSpace(startIp)) startIp = "192.168.0.1";
            var parts = startIp.Split('.');
            if (parts.Length != 4) return startIp;

            if (!byte.TryParse(parts[0], out var a)) return startIp;
            if (!byte.TryParse(parts[1], out var b)) return startIp;
            if (!byte.TryParse(parts[2], out var c)) return startIp;
            if (!byte.TryParse(parts[3], out var d)) return startIp;

            uint v = ((uint)a << 24) | ((uint)b << 16) | ((uint)c << 8) | d;
            v = unchecked(v + (uint)Math.Max(0, offset));

            var na = (byte)(v >> 24);
            var nb = (byte)(v >> 16);
            var nc = (byte)(v >> 8);
            var nd = (byte)(v);
            return $"{na}.{nb}.{nc}.{nd}";
        }
    }
}
