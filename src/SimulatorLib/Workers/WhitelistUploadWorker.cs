using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
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
    /// </summary>
    public class WhitelistUploadWorker
    {
        private readonly INetworkSender _tcpSender;
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

        private async Task<UploadStats> RunBatchAsync(
            string filePath, List<ClientRecord> targets,
            string platformHost, int platformPort,
            int concurrency, CancellationToken ct)
        {
            var payload = await File.ReadAllBytesAsync(filePath, ct).ConfigureAwait(false);
            var sem = new SemaphoreSlim(Math.Max(1, concurrency));
            int statsSuccess = 0, statsFail = 0;
            var tasks = new List<Task>(targets.Count);

            foreach (var c in targets)
            {
                await sem.WaitAsync(ct).ConfigureAwait(false);
                tasks.Add(Task.Run(async () =>
                {
                    try
                    {
                        var ok = await _tcpSender.SendTcpAsync(platformHost, platformPort, payload, 5000, ct)
                            .ConfigureAwait(false);
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
