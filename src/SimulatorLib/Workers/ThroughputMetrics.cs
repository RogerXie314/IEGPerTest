using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 可选的日志发送吞吐量统计器。
    ///
    /// <para>
    /// 默认不启用。调用 <see cref="Enable"/> 后，在后台定时打印：<br/>
    ///   • 已发送总数 / 每秒 / 每分钟<br/>
    ///   • 失败总数   / 每秒 / 每分钟<br/>
    /// 速率采用"快照差分"算法：每个上报周期结束时与上一快照对比，
    /// 除以实际间隔秒数，避免累计倾斜。
    /// </para>
    ///
    /// <para>
    /// 典型用法：
    /// <code>
    ///   logWorker.Metrics.Enable(reportIntervalSeconds: 30);
    ///   await logWorker.StartAsync(...);
    /// </code>
    /// </para>
    /// </summary>
    public sealed class ThroughputMetrics
    {
        private long _totalSuccess;
        private long _totalFail;
        private bool _enabled;
        private int _reportIntervalSeconds = 30;
        private string? _logFilePath;

        /// <summary>是否已启用统计。</summary>
        public bool Enabled => _enabled;

        /// <summary>当前累计成功发送数（线程安全读）。</summary>
        public long TotalSuccess => Interlocked.Read(ref _totalSuccess);

        /// <summary>当前累计失败数（线程安全读）。</summary>
        public long TotalFail => Interlocked.Read(ref _totalFail);

        /// <summary>
        /// 启用吞吐量统计。
        /// </summary>
        /// <param name="reportIntervalSeconds">定时上报间隔（秒），默认 30 秒。最小 1 秒。</param>
        /// <param name="logFilePath">
        ///   可选：将每次统计行追加写入此文件。
        ///   若为 null，自动写入 logs/throughput-{yyyyMMdd}.log。
        ///   若为空字符串 ""，则不写文件（只输出到控制台）。
        /// </param>
        public void Enable(int reportIntervalSeconds = 30, string? logFilePath = null)
        {
            _reportIntervalSeconds = Math.Max(1, reportIntervalSeconds);
            _logFilePath = logFilePath; // null = 自动路径；"" = 不写文件
            _enabled = true;
        }

        /// <summary>
        /// 记录 1 次成功发送。高并发热路径，零分配。
        /// </summary>
        public void RecordSuccess() => Interlocked.Increment(ref _totalSuccess);

        /// <summary>
        /// 记录 1 次发送失败。高并发热路径，零分配。
        /// </summary>
        public void RecordFail() => Interlocked.Increment(ref _totalFail);

        /// <summary>
        /// 获取当前累计快照（线程安全）。
        /// </summary>
        public ThroughputSnapshot GetSnapshot(Stopwatch sw) =>
            new(Interlocked.Read(ref _totalSuccess),
                Interlocked.Read(ref _totalFail),
                sw.Elapsed.TotalSeconds);

        /// <summary>
        /// 启动定时上报后台循环。未调用 <see cref="Enable"/> 时立即返回。
        /// 建议以 <c>CancellationTokenSource.CreateLinkedTokenSource(outerCt)</c> 的 Token 驱动，
        /// 发送任务完成后取消，此时会额外输出一次最终汇总行。
        /// </summary>
        public async Task RunReportLoopAsync(Stopwatch sw, CancellationToken ct)
        {
            if (!_enabled) return;

            var intervalMs = _reportIntervalSeconds * 1000;
            ThroughputSnapshot? prev = null;

            try
            {
                while (!ct.IsCancellationRequested)
                {
                    await Task.Delay(intervalMs, ct).ConfigureAwait(false);

                    var now = GetSnapshot(sw);
                    var line = FormatPeriodicReport(now, prev, _reportIntervalSeconds);
                    Console.WriteLine(line);
                    await TryAppendLogAsync(line).ConfigureAwait(false);
                    prev = now;
                }
            }
            catch (OperationCanceledException) { }

            // 任务结束时：输出最终汇总（无论是主动取消还是外部 ct 触发）
            var final = GetSnapshot(sw);
            var finalLine = FormatFinalReport(final);
            Console.WriteLine(finalLine);
            await TryAppendLogAsync(finalLine).ConfigureAwait(false);
        }

        // ── 格式化 ──────────────────────────────────────────────────────────

        private static string FormatPeriodicReport(
            ThroughputSnapshot now, ThroughputSnapshot? prev, int intervalSec)
        {
            var ts = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");

            double deltaSec;
            long deltaSuccess;
            long deltaFail;

            if (prev == null)
            {
                // 第一次上报：用总运行时间计算平均速率
                deltaSec = Math.Max(0.1, now.ElapsedSeconds);
                deltaSuccess = now.TotalSuccess;
                deltaFail = now.TotalFail;
            }
            else
            {
                deltaSec = Math.Max(0.1, now.ElapsedSeconds - prev.ElapsedSeconds);
                deltaSuccess = now.TotalSuccess - prev.TotalSuccess;
                deltaFail = now.TotalFail - prev.TotalFail;
            }

            var successPerSec = deltaSuccess / deltaSec;
            var failPerSec = deltaFail / deltaSec;
            var successPerMin = successPerSec * 60.0;
            var failPerMin = failPerSec * 60.0;

            return
                $"[{ts}] [吞吐统计]  " +
                $"发送: 总计 {now.TotalSuccess,9:N0} 条  " +
                $"{successPerSec,8:N1}/秒  {successPerMin,9:N0}/分钟  |  " +
                $"失败: 总计 {now.TotalFail,6:N0} 条  " +
                $"{failPerSec,7:N1}/秒  {failPerMin,8:N0}/分钟  " +
                $"(本轮 {deltaSec:F1}s)";
        }

        private static string FormatFinalReport(ThroughputSnapshot final)
        {
            var ts = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
            var elapsed = Math.Max(0.1, final.ElapsedSeconds);
            var total = final.TotalSuccess + final.TotalFail;
            var overallPerSec = total / elapsed;
            var overallPerMin = overallPerSec * 60.0;
            var failRate = total > 0 ? (double)final.TotalFail / total * 100.0 : 0.0;

            return
                $"[{ts}] [吞吐汇总]  " +
                $"总发送 {total,9:N0} 条  " +
                $"平均 {overallPerSec,8:N1}/秒  {overallPerMin,9:N0}/分钟  |  " +
                $"失败 {final.TotalFail,6:N0} 条  失败率 {failRate,5:F2}%  " +
                $"(总耗时 {elapsed:F1}s)";
        }

        // ── 文件写入 ─────────────────────────────────────────────────────────

        private async Task TryAppendLogAsync(string line)
        {
            string? path;

            if (_logFilePath == null)
            {
                // 自动路径：logs/throughput-{yyyyMMdd}.log（与 logsend-*.log 同目录）
                var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                path = Path.Combine(logDir, $"throughput-{DateTime.Now:yyyyMMdd}.log");
            }
            else if (_logFilePath.Length == 0)
            {
                return; // 显式传空字符串 = 不写文件
            }
            else
            {
                path = _logFilePath;
            }

            try
            {
                var dir = Path.GetDirectoryName(path);
                if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                    Directory.CreateDirectory(dir);
                await File.AppendAllTextAsync(path, line + Environment.NewLine).ConfigureAwait(false);
            }
            catch { /* 统计写入失败不影响主流程 */ }
        }
    }

    /// <summary>
    /// 某时刻的吞吐量快照，用于差分计算速率。
    /// </summary>
    public record ThroughputSnapshot(long TotalSuccess, long TotalFail, double ElapsedSeconds);
}
