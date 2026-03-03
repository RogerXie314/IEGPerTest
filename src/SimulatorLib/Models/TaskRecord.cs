using System;
using System.ComponentModel;

namespace SimulatorLib.Models
{
    /// <summary>
    /// 统一任务面板的记录模型，每个任务（心跳/日志/白名单/策略接收）对应一条记录。
    /// </summary>
    public class TaskRecord : INotifyPropertyChanged
    {
        private static int _idCounter = 0;

        public event PropertyChangedEventHandler? PropertyChanged;

        private TaskStatus _status = TaskStatus.Running;
        private int    _successCount;
        private int    _failCount;
        private string _detail = string.Empty;
        private DateTime? _endTime;

        public int      Id          { get; } = System.Threading.Interlocked.Increment(ref _idCounter);
        public string   TaskType    { get; }
        public DateTime StartTime   { get; } = DateTime.Now;
        public int      ClientCount { get; set; }
        /// <summary>循环间隔（秒）；0 = 不适用（一次性任务）</summary>
        public int      IntervalSec { get; set; }

        public TaskStatus Status
        {
            get => _status;
            set { _status = value; Notify(); Notify(nameof(StatusText)); Notify(nameof(StatusColor)); }
        }

        public int SuccessCount
        {
            get => _successCount;
            set { _successCount = value; Notify(); }
        }

        public int FailCount
        {
            get => _failCount;
            set { _failCount = value; Notify(); }
        }

        public string Detail
        {
            get => _detail;
            set { _detail = value; Notify(); }
        }

        public DateTime? EndTime
        {
            get => _endTime;
            set { _endTime = value; Notify(); Notify(nameof(DurationText)); }
        }

        // ── 派生显示属性 ──────────────────────────────────────────────────────────

        public string StatusText => _status switch
        {
            TaskStatus.Running   => "运行中",
            TaskStatus.Stopped   => "已停止",
            TaskStatus.Completed => "已完成",
            TaskStatus.Error     => "异常",
            _                    => "未知"
        };

        /// <summary>WPF 颜色 Brush 名（可通过 Converter 转换）</summary>
        public string StatusColor => _status switch
        {
            TaskStatus.Running   => "#27AE60",  // 绿
            TaskStatus.Stopped   => "#888888",  // 灰
            TaskStatus.Completed => "#2980B9",  // 蓝
            TaskStatus.Error     => "#E74C3C",  // 红
            _                    => "#888888"
        };

        public string DurationText
        {
            get
            {
                var end = _endTime ?? DateTime.Now;
                var span = end - StartTime;
                if (span.TotalHours >= 1)
                    return $"{(int)span.TotalHours}h{span.Minutes:D2}m";
                if (span.TotalMinutes >= 1)
                    return $"{(int)span.TotalMinutes}m{span.Seconds:D2}s";
                return $"{(int)span.TotalSeconds}s";
            }
        }

        public string IntervalText => IntervalSec > 0 ? $"{IntervalSec}s" : "—";

        public TaskRecord(string taskType, int clientCount = 0, int intervalSec = 0)
        {
            TaskType    = taskType;
            ClientCount = clientCount;
            IntervalSec = intervalSec;
        }

        /// <summary>任务停止：更新状态与结束时间</summary>
        public void MarkStopped()
        {
            EndTime = DateTime.Now;
            Status  = TaskStatus.Stopped;
        }

        /// <summary>任务自然完成</summary>
        public void MarkCompleted()
        {
            EndTime = DateTime.Now;
            Status  = TaskStatus.Completed;
        }

        private void Notify([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}
