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
        /// 三阶段并发注册：
        ///   1. 预生成 — 构造所有客户端记录（状态=Generated），一次性写入 Clients.log；
        ///   2. 并发注册 — 以 SemaphoreSlim(concurrency) 控制并发，同时发起最多 concurrency 条 HTTP 注册请求；
        ///   3. 回写 — 注册完成后，用最终状态（Registered/Failed）覆写 Clients.log。
        ///
        /// 这样即使注册中途崩溃，Clients.log 中也保留了预生成的完整客户端列表。
        /// </summary>
        public async Task<RegistrationSummary> RegisterAsync(string clientIdPrefix, int startIndex, int count,
            string startIp = "192.168.0.1", string host = "localhost", int port = 8441,
            int concurrency = 20, int retry = 3, int timeoutMs = 3000, CancellationToken ct = default)
        {
            // ── Phase 1: 预生成所有客户端记录并持久化 ────────────────────────────────
            var preRecords = new List<ClientRecord>(count);
            for (int i = 0; i < count; i++)
            {
                var clientId = clientIdPrefix + (startIndex + i).ToString();
                var ip = IncrementIPv4(startIp, i);
                preRecords.Add(new ClientRecord(clientId, ip, DateTime.UtcNow, "Generated"));
            }
            await ClientsPersistence.WriteAllAsync(preRecords).ConfigureAwait(false);

            // ── Phase 2: 并发 HTTP 注册 ───────────────────────────────────────────
            var sem = new SemaphoreSlim(concurrency);
            int successCount = 0;
            int failureCount = 0;
            var failureReasons = new ConcurrentDictionary<string, int>(StringComparer.Ordinal);

            // 用数组记录每个客户端的最终 record（可以更新 DeviceId/TcpPort/Status）
            var finalRecords = new ClientRecord[count];
            for (int i = 0; i < count; i++) finalRecords[i] = preRecords[i];

            using var http = CreateHttpClient(timeoutMs);
            var loginUrl  = new Uri($"https://{host}:{port}/USM/clientLogin.do");
            var resultUrl = new Uri($"https://{host}:{port}/USM/clientResult.do");

            var tasks = new List<Task>(count);
            for (int i = 0; i < count; i++)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                int capturedI = i;
                var preRec = preRecords[capturedI];

                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(preRec.IP);
                        var reqJson = UsmRegistrationJsonBuilder.BuildClientLoginJson(
                            computerId: preRec.ClientId,
                            username: "SysAdmin",
                            computerName: preRec.ClientId,
                            computerIp: preRec.IP,
                            computerMac: mac,
                            windowsVersion: OsInfo.GetWindowsVersionName(),
                            windowsX64: Environment.Is64BitOperatingSystem,
                            proxying: 0,
                            licenseRecycle: false,
                            clientType: 0,
                            clientVersion: "V300R011C01B030");

                        bool ok = false;
                        uint deviceId = 0;
                        int tcpPort = 0;
                        string lastFailReason = "未知失败";

                        // 对齐原项目重试策略：固定 1000ms 间隔
                        const int RetryDelayMs = 1000;

                        for (int attempt = 1; attempt <= retry && !ok && !ct.IsCancellationRequested; attempt++)
                        {
                            try
                            {
                                // Step 1: clientLogin.do
                                using var content = new StringContent(reqJson, Encoding.UTF8, "application/json");
                                using var resp = await http.PostAsync(loginUrl, content, ct).ConfigureAwait(false);
                                var respText = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);

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

                                // Step 2: clientResult.do
                                var resultJson = UsmRegistrationJsonBuilder.BuildClientResultJson(
                                    computerId: preRec.ClientId,
                                    username: "SysAdmin",
                                    cmdType: UsmRegistrationJsonBuilder.SetupCmdType,
                                    cmdId: UsmRegistrationJsonBuilder.CmdClientRegistry,
                                    dealResult: 0);

                                using var resultContent = new StringContent(resultJson, Encoding.UTF8, "application/json");
                                using var resultResp = await http.PostAsync(resultUrl, resultContent, ct).ConfigureAwait(false);

                                if (!resultResp.IsSuccessStatusCode)
                                {
                                    lastFailReason = $"结果上报失败(HTTP{(int)resultResp.StatusCode})";
                                    await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                                    continue;
                                }

                                ok = true;
                            }
                            catch (OperationCanceledException) { throw; }
                            catch
                            {
                                lastFailReason = "超时/网络异常";
                                await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                            }
                        }

                        // 记录统计
                        if (ok)
                        {
                            Interlocked.Increment(ref successCount);
                            finalRecords[capturedI] = preRec with
                            {
                                Status = "Registered",
                                RegisteredAt = DateTime.UtcNow,
                                DeviceId = deviceId,
                                TcpPort = tcpPort,
                            };
                        }
                        else
                        {
                            Interlocked.Increment(ref failureCount);
                            failureReasons.AddOrUpdate(lastFailReason, 1, (_, c) => c + 1);
                            finalRecords[capturedI] = preRec with { Status = "Failed" };
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

            // ── Phase 3: 用最终状态覆写 Clients.log ──────────────────────────────
            await ClientsPersistence.WriteAllAsync(finalRecords).ConfigureAwait(false);

            return new RegistrationSummary
            {
                Total   = count,
                Success = successCount,
                Failed  = failureCount,
                FailureReasons = new Dictionary<string, int>(failureReasons),
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
