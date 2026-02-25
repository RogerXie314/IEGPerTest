using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Text.Json;
using System.Text;
using System.Threading;
using System.Threading.Channels;
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
        /// 并发注册一批客户端：对每个客户端通过 HTTPS 调用 USM 的 /USM/clientLogin.do 注册，
        /// 解析返回的 devid/tcpPort 并持久化到 Clients.log。
        /// 支持并发限制、重试与超时；默认允许自签名证书（便于测试环境）。
        /// </summary>
        public async Task<RegistrationSummary> RegisterAsync(string clientIdPrefix, int startIndex, int count, string startIp = "192.168.0.1", string host = "localhost", int port = 8441, int concurrency = 10, int retry = 3, int timeoutMs = 3000, CancellationToken ct = default)
        {
            var sem = new SemaphoreSlim(concurrency);
            var tasks = new List<Task>();

            // 并发安全的统计计数器
            int successCount = 0;
            int failureCount = 0;
            var failureReasons = new ConcurrentDictionary<string, int>(StringComparer.Ordinal);

            // 用 Channel 解耦注册速度与文件写入速度：
            // 注册任务 HTTP 完成后立即投递记录到 channel 并释放并发门控，
            // 独立的消费者 Task 从 channel 读取并串行写入 Clients.log。
            // 这样文件写入的快慢不会阻塞下一批注册任务的启动。
            var writeChannel = Channel.CreateUnbounded<ClientRecord>(
                new UnboundedChannelOptions { SingleReader = true, SingleWriter = false });

            // 后台消费者：串行写入，保证文件安全；channel 关闭后将剩余记录全部刷完再退出
            var writerTask = Task.Run(async () =>
            {
                await foreach (var rec in writeChannel.Reader.ReadAllAsync(CancellationToken.None)
                                                              .ConfigureAwait(false))
                {
                    try { await ClientsPersistence.AppendAsync(rec).ConfigureAwait(false); }
                    catch { /* 单条写入失败不影响其余记录 */ }
                }
            });

            using var http = CreateHttpClient(timeoutMs);
            var loginUrl = new Uri($"https://{host}:{port}/USM/clientLogin.do");
            var resultUrl = new Uri($"https://{host}:{port}/USM/clientResult.do");

            for (int i = 0; i < count; i++)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                int idx = startIndex + i;
                int ipOffset = i;
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var clientId = clientIdPrefix + idx.ToString();
                        var ip = IncrementIPv4(startIp, ipOffset);
                        var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(ip);

                        bool ok = false;
                        uint deviceId = 0;
                        int tcpPort = 0;
                        string lastFailReason = "未知失败";

                        // 对齐原项目 WLJsonParse::SetUp_GetJson + AccessServer.cpp 的调用参数
                        var reqJson = UsmRegistrationJsonBuilder.BuildClientLoginJson(
                            computerId: clientId,
                            username: "SysAdmin",
                            computerName: clientId,
                            computerIp: ip,
                            computerMac: mac,
                            windowsVersion: OsInfo.GetWindowsVersionName(),
                            windowsX64: Environment.Is64BitOperatingSystem,
                            proxying: 0,
                            licenseRecycle: false,
                            clientType: 0,
                            clientVersion: "V300R011C01B030");

                        // 对齐原项目 WlSetUPApp.cpp 的重试策略：
                        //   WL_CONNECT_USM_RETRY_COUNTS = 3（最多3次）
                        //   WL_SETUP_DOPOST_RETRY_TIME  = 1000ms（固定间隔，给服务器恢复时间）
                        // clientLogin.do 和 clientResult.do 均在同一重试循环内，
                        // 任意一步失败都消耗一次重试计数并等待 1000ms 后重试整个流程，
                        // 对应原项目 USM_BUSY（errCode=8）等繁忙场景。
                        const int RetryDelayMs = 1000;

                        for (int attempt = 1; attempt <= retry && !ok && !ct.IsCancellationRequested; attempt++)
                        {
                            try
                            {
                                // Step 1: clientLogin.do（对应原项目 SetUp_DoPost）
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
                                    // errCode=8 → USM_BUSY（服务器繁忙），固定等待 1000ms 后重试
                                    lastFailReason = ErrCodeToReason(errCode);
                                    await Task.Delay(RetryDelayMs, ct).ConfigureAwait(false);
                                    continue;
                                }

                                if (UsmResponseParsers.TryParseConfigInfo(respText, out var cfg))
                                {
                                    deviceId = cfg.DeviceId;
                                    tcpPort = cfg.TcpPort;
                                }

                                // Step 2: clientResult.do（对应原项目 Setup_InstallEnd）
                                // 原项目同样纳入重试循环：失败时 iRetry-- + Sleep(1000ms) + continue
                                var resultJson = UsmRegistrationJsonBuilder.BuildClientResultJson(
                                    computerId: clientId,
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

                        // 更新统计计数
                        if (ok)
                        {
                            Interlocked.Increment(ref successCount);
                        }
                        else
                        {
                            Interlocked.Increment(ref failureCount);
                            failureReasons.AddOrUpdate(lastFailReason, 1, (_, c) => c + 1);
                        }

                        var status = ok ? "Registered" : "Failed";
                        var rec = new ClientRecord(clientId, ip, DateTime.UtcNow, status)
                        {
                            DeviceId = deviceId,
                            TcpPort = tcpPort,
                        };
                        // HTTP 完成后立即投入 channel，sem 随后立刻释放，
                        // 不再等待文件写入完成，保证注册速度不受磁盘 I/O 影响。
                        writeChannel.Writer.TryWrite(rec);
                    }
                    catch (OperationCanceledException) { }
                    catch { }
                    finally
                    {
                        sem.Release();
                    }
                }, ct));
            }

            // 等待所有注册任务完成，再告知 channel 写端关闭
            await Task.WhenAll(tasks).ConfigureAwait(false);
            writeChannel.Writer.Complete();

            // 等待消费者把 channel 中剩余记录全部刷写完毕
            await writerTask.ConfigureAwait(false);

            return new RegistrationSummary
            {
                Total = count,
                Success = successCount,
                Failed = failureCount,
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
