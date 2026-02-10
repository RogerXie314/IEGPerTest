using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text.Json;
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
        /// 并发注册一批客户端：对每个客户端通过 HTTPS 调用 USM 的 /USM/clientLogin.do 注册，
        /// 解析返回的 devid/tcpPort 并持久化到 Clients.log。
        /// 支持并发限制、重试与超时；默认允许自签名证书（便于测试环境）。
        /// </summary>
        public async Task RegisterAsync(string clientIdPrefix, int startIndex, int count, string startIp = "192.168.0.1", string host = "localhost", int port = 8441, int concurrency = 10, int retry = 3, int timeoutMs = 3000, CancellationToken ct = default)
        {
            var sem = new SemaphoreSlim(concurrency);
            var tasks = new List<Task>();

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

                        bool ok = false;
                        uint deviceId = 0;
                        int tcpPort = 0;

                        for (int attempt = 1; attempt <= retry && !ok && !ct.IsCancellationRequested; attempt++)
                        {
                            try
                            {
                                using var content = new StringContent(reqJson, Encoding.UTF8, "application/json");
                                using var resp = await http.PostAsync(loginUrl, content, ct).ConfigureAwait(false);
                                var respText = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);

                                if (!resp.IsSuccessStatusCode)
                                {
                                    await Task.Delay(200 * attempt, ct).ConfigureAwait(false);
                                    continue;
                                }

                                if (!UsmResponseParsers.TryCheckSetupResult(respText, out var errCode, out _))
                                {
                                    await Task.Delay(200 * attempt, ct).ConfigureAwait(false);
                                    continue;
                                }

                                if (UsmResponseParsers.TryParseConfigInfo(respText, out var cfg))
                                {
                                    deviceId = cfg.DeviceId;
                                    tcpPort = cfg.TcpPort;
                                }

                                ok = true;

                                // 按原项目 Register 完还会调用 clientResult.do，上报安装结束结果
                                var resultJson = UsmRegistrationJsonBuilder.BuildClientResultJson(
                                    computerId: clientId,
                                    username: "SysAdmin",
                                    cmdType: UsmRegistrationJsonBuilder.SetupCmdType,
                                    cmdId: UsmRegistrationJsonBuilder.CmdClientRegistry,
                                    dealResult: 0);
                                _ = TryPostFireAndForget(http, resultUrl, resultJson, ct);
                            }
                            catch (OperationCanceledException) { throw; }
                            catch
                            {
                                await Task.Delay(200 * attempt, ct).ConfigureAwait(false);
                            }
                        }

                        var status = ok ? "Registered" : "Failed";
                        var rec = new ClientRecord(clientId, ip, DateTime.UtcNow, status)
                        {
                            DeviceId = deviceId,
                            TcpPort = tcpPort,
                        };
                        await ClientsPersistence.AppendAsync(rec).ConfigureAwait(false);
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

        private static async Task TryPostFireAndForget(HttpClient http, Uri url, string json, CancellationToken ct)
        {
            try
            {
                using var content = new StringContent(json, Encoding.UTF8, "application/json");
                using var _ = await http.PostAsync(url, content, ct).ConfigureAwait(false);
            }
            catch { }
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
