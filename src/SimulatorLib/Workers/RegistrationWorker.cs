using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class RegistrationWorker
    {
        private readonly INetworkSender _sender;

        public RegistrationWorker(INetworkSender? sender = null)
        {
            _sender = sender ?? new TcpSender();
        }

        /// <summary>
        /// 并发注册一批客户端：对每个客户端尝试通过 TCP 发送注册数据，注册成功后持久化到 Clients.log。
        /// 支持并发限制、重试与超时。
        /// </summary>
        public async Task RegisterAsync(string clientIdPrefix, int startIndex, int count, string host = "localhost", int port = 8441, int concurrency = 10, int retry = 3, int timeoutMs = 3000, CancellationToken ct = default)
        {
            var sem = new SemaphoreSlim(concurrency);
            var tasks = new List<Task>();

            for (int i = 0; i < count; i++)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                int idx = startIndex + i;
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var clientId = clientIdPrefix + idx.ToString();
                        var ip = $"192.168.0.{idx % 254 + 1}";
                        var payload = Encoding.UTF8.GetBytes($"REGISTER:{clientId}:{ip}");

                        bool ok = false;
                        for (int attempt = 1; attempt <= retry && !ok && !ct.IsCancellationRequested; attempt++)
                        {
                            ok = await _sender.SendTcpAsync(host, port, payload, timeoutMs, ct).ConfigureAwait(false);
                            if (!ok)
                                await Task.Delay(200 * attempt, ct).ConfigureAwait(false);
                        }

                        var status = ok ? "Registered" : "Failed";
                        var rec = new ClientRecord(clientId, ip, DateTime.UtcNow, status);
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
    }
}
