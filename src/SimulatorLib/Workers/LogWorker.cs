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

            var totalMessages = clients.Count * messagesPerClient;
            var success = 0;
            var fail = 0;

            var intervalMs = 0;
            if (messagesPerSecondPerClient.HasValue && messagesPerSecondPerClient.Value > 0)
            {
                intervalMs = (int)Math.Max(1, Math.Round(1000.0 / messagesPerSecondPerClient.Value));
            }

            var categoryList = (categories != null && categories.Count > 0)
                ? categories
                : new[] { "Default" };

            var sem = new SemaphoreSlim(Math.Max(1, concurrency));
            var tasks = new List<Task>(clients.Count);

            foreach (var c in clients)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        for (int i = 0; i < messagesPerClient && !ct.IsCancellationRequested; i++)
                        {
                            if (intervalMs > 0)
                            {
                                await Task.Delay(intervalMs, ct).ConfigureAwait(false);
                            }

                            var cat = categoryList[i % categoryList.Count];
                            var payload = Encoding.UTF8.GetBytes($"LOG:{cat}:{c.ClientId}:{DateTime.UtcNow:o}:SampleLogMessage");
                            bool ok;
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
                    }
                    catch
                    {
                        // 发生异常视为失败（尽量不影响其他客户端发送）
                        Interlocked.Increment(ref fail);
                    }
                    finally
                    {
                        sem.Release();
                    }
                }, ct));
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
