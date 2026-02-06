using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Persistence;
using System.IO;

namespace SimulatorLib.Workers
{
    public record LogSendStats(int TotalMessages, int Success, int Failed);

    public class LogWorker
    {
        private readonly INetworkSender _tcpSender;
        private readonly IUdpSender? _udpSender;

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
            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            var totalMessages = clients.Count * messagesPerClient;

            var success = 0;
            var fail = 0;

            var sem = new SemaphoreSlim(concurrency);
            var tasks = new List<Task>();

            foreach (var c in clients)
            {
                for (int i = 0; i < messagesPerClient; i++)
                {
                    await sem.WaitAsync(ct).ConfigureAwait(false);
                    tasks.Add(Task.Run(async () =>
                    {
                        try
                        {
                            var payload = Encoding.UTF8.GetBytes($"LOG:{c.ClientId}:{DateTime.UtcNow:o}:SampleLogMessage");
                            bool ok = false;
                            if (useLogServer && _udpSender != null && !string.IsNullOrEmpty(logHost))
                            {
                                ok = await _udpSender.SendUdpAsync(logHost, logPort, payload, 2000, ct).ConfigureAwait(false);
                            }
                            else
                            {
                                ok = await _tcpSender.SendTcpAsync(platformHost, platformPort, payload, 2000, ct).ConfigureAwait(false);
                            }

                            if (ok) Interlocked.Increment(ref success);
                            else Interlocked.Increment(ref fail);
                        }
                        catch { Interlocked.Increment(ref fail); }
                        finally { sem.Release(); }
                    }, ct));
                }
            }

            await Task.WhenAll(tasks).ConfigureAwait(false);

            var stats = new LogSendStats(totalMessages, success, fail);
            try { progress?.Report(stats); } catch { }

            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"logsend-{DateTime.UtcNow:yyyyMMdd}.log");
                var line = $"{DateTime.UtcNow:o} TotalMessages={stats.TotalMessages} Success={stats.Success} Fail={stats.Failed}";
                await File.AppendAllTextAsync(logPath, line + Environment.NewLine).ConfigureAwait(false);
            }
            catch { }
        }
    }
}
