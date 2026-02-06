using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public record UploadStats(int Total, int Success, int Failed);

    public class WhitelistUploadWorker
    {
        private readonly INetworkSender _tcpSender;

        public WhitelistUploadWorker(INetworkSender tcpSender)
        {
            _tcpSender = tcpSender;
        }

        /// <summary>
        /// 将白名单文件并发上传到前 N 个客户端（由 Clients.log 提供目标）。
        /// 简化实现：通过 TCP 将整个文件作为 payload 发送到目标的指定端口（模拟上传）。
        /// </summary>
        public async Task StartAsync(string filePath, int clientCount, string platformHost, int platformPort, int concurrency, CancellationToken ct, IProgress<UploadStats>? progress = null)
        {
            var statsSuccess = 0;
            var statsFail = 0;

            if (!File.Exists(filePath)) throw new FileNotFoundException("白名单文件不存在", filePath);
            var payload = await File.ReadAllBytesAsync(filePath, ct).ConfigureAwait(false);

            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            var targets = clients.Take(clientCount).ToList();

            var sem = new SemaphoreSlim(concurrency);
            var tasks = new List<Task>();

            foreach (var c in targets)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var ok = await _tcpSender.SendTcpAsync(platformHost, platformPort, payload, 5000, ct).ConfigureAwait(false);
                        if (ok) Interlocked.Increment(ref statsSuccess);
                        else Interlocked.Increment(ref statsFail);
                    }
                    catch { Interlocked.Increment(ref statsFail); }
                    finally { sem.Release(); }
                }, ct));
            }

            await Task.WhenAll(tasks).ConfigureAwait(false);

            var stats = new UploadStats(targets.Count, statsSuccess, statsFail);
            try { progress?.Report(stats); } catch { }

            try
            {
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                var logPath = Path.Combine(logDir, $"upload-{DateTime.UtcNow:yyyyMMdd}.log");
                var line = $"{DateTime.UtcNow:o} File={Path.GetFileName(filePath)} Total={stats.Total} Success={stats.Success} Fail={stats.Failed}";
                await File.AppendAllTextAsync(logPath, line + Environment.NewLine).ConfigureAwait(false);
            }
            catch { }
        }
    }
}
