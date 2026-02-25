using System.Collections.Generic;
using System.Text;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 一次注册任务的汇总结果，包含成功数、失败数以及失败原因分布。
    /// </summary>
    public class RegistrationSummary
    {
        public int Total { get; init; }
        public int Success { get; init; }
        public int Failed { get; init; }

        /// <summary>
        /// 失败原因 → 出现次数。
        /// key 示例：「服务器繁忙(8)」「HTTP错误(500)」「超时/网络异常」等。
        /// </summary>
        public IReadOnlyDictionary<string, int> FailureReasons { get; init; }
            = new Dictionary<string, int>();

        /// <summary>
        /// 将汇总结果格式化为多行文本，便于写入日志。
        /// </summary>
        public string ToLogString()
        {
            var sb = new StringBuilder();
            sb.AppendLine($"[注册统计] 总数={Total}  成功={Success}  失败={Failed}");
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
