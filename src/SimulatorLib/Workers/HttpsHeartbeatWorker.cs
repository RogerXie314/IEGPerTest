using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Persistence;
using SimulatorLib.Protocol;

namespace SimulatorLib.Workers
{
    public class HttpsHeartbeatWorker
    {
        private readonly IUdpSender? _udpSender;

        public HttpsHeartbeatWorker(IUdpSender? udpSender = null)
        {
            _udpSender = udpSender;
        }

        public record HeartbeatStats(int Total, int SuccessHttps, int FailHttps, int SuccessUdp, int FailUdp);

        public async Task StartAsync(int intervalMs, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, CancellationToken ct, IProgress<HeartbeatStats>? progress = null)
        {
            using var http = CreateHttpClient(timeoutMs: 2500);
            var sem = new SemaphoreSlim(Math.Max(1, concurrency));

            while (!ct.IsCancellationRequested)
            {
                var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);

                var successHttps = 0;
                var failHttps = 0;
                var successUdp = 0;
                var failUdp = 0;

                var tasks = new List<Task>(clients.Count);
                foreach (var c in clients)
                {
                    await sem.WaitAsync(ct).ConfigureAwait(false);
                    tasks.Add(Task.Run(async () =>
                    {
                        try
                        {
                            var domainName = GetDomainNameSafe();
                            var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(c.IP);
                            var json = HeartbeatJsonBuilder.BuildV3R7C02(c.ClientId, domainName, c.IP, mac);

                            var ok = await PostHeartbeatAsync(http, platformHost, platformPort, json, ct).ConfigureAwait(false);
                            if (ok) Interlocked.Increment(ref successHttps);
                            else Interlocked.Increment(ref failHttps);

                            if (useLogServer && _udpSender != null && !string.IsNullOrWhiteSpace(logHost))
                            {
                                var payload = PtProtocol.Pack(Encoding.UTF8.GetBytes(json), cmdId: 1, deviceId: c.DeviceId);
                                var udpOk = await _udpSender.SendUdpAsync(logHost, logPort, payload, 1500, ct).ConfigureAwait(false);
                                if (udpOk) Interlocked.Increment(ref successUdp);
                                else Interlocked.Increment(ref failUdp);
                            }
                        }
                        catch (OperationCanceledException) { }
                        catch
                        {
                            Interlocked.Increment(ref failHttps);
                        }
                        finally
                        {
                            sem.Release();
                        }
                    }, ct));
                }

                await Task.WhenAll(tasks).ConfigureAwait(false);

                var stats = new HeartbeatStats(clients.Count, successHttps, failHttps, successUdp, failUdp);
                try { progress?.Report(stats); } catch { }

                try
                {
                    var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                    var logPath = Path.Combine(logDir, $"heartbeat-https-{DateTime.UtcNow:yyyyMMdd}.log");
                    var line = $"{DateTime.UtcNow:o} Total={stats.Total} HttpsOk={stats.SuccessHttps} HttpsFail={stats.FailHttps} UdpOk={stats.SuccessUdp} UdpFail={stats.FailUdp}";
                    await File.AppendAllTextAsync(logPath, line + Environment.NewLine, ct).ConfigureAwait(false);
                }
                catch { }

                try
                {
                    await Task.Delay(Math.Max(200, intervalMs), ct).ConfigureAwait(false);
                }
                catch (TaskCanceledException) { break; }
            }
        }

        private static async Task<bool> PostHeartbeatAsync(HttpClient http, string host, int port, string json, CancellationToken ct)
        {
            var url = new Uri($"https://{host}:{port}/USM/clientHeartbeat.do");
            using var content = new StringContent(json, Encoding.UTF8, "application/json");
            using var resp = await http.PostAsync(url, content, ct).ConfigureAwait(false);
            _ = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
            return resp.IsSuccessStatusCode;
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

        private static string GetDomainNameSafe()
        {
            try
            {
                return Environment.UserDomainName ?? string.Empty;
            }
            catch
            {
                return string.Empty;
            }
        }
    }
}
