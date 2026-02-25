using System.Collections.Generic;
using System.Text;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 每完成一轮注册后向调用方报告的进度快照。
    /// </summary>
    public record RegistrationRoundProgress(
        int Round,          // 当前是第几轮
        int RoundSuccess,   // 本轮新增成功数
        int RoundFailed,    // 本轮仍失败数（将进入下一轮）
        int TotalSuccess,   // 累计成功数
        int Remaining);     // 剩余待注册数（即本轮失败数）

    /// <summary>
    /// 一次完整注册任务（所有轮次结束）后的汇总结果。
    /// </summary>
    public class RegistrationSummary
    {
        public int Total { get; init; }
        public int Success { get; init; }
        public int Failed { get; init; }
        public int Rounds { get; init; }

        /// <summary>失败原因 → 累计出现次数（跨所有轮次）。</summary>
        public IReadOnlyDictionary<string, int> FailureReasons { get; init; }
            = new Dictionary<string, int>();

        public string ToLogString()
        {
            var sb = new StringBuilder();
            sb.AppendLine($"[注册统计] 总数={Total}  成功={Success}  失败={Failed}  共{Rounds}轮");
            if (FailureReasons.Count > 0)
            {
                sb.AppendLine("  失败原因明细：");
                foreach (var kv in FailureReasons)
                    sb.AppendLine($"    {kv.Key}: {kv.Value} 次");
            }
            return sb.ToString().TrimEnd();
        }
    }
}
