using System;
using System.Net;

namespace SimulatorLib.Models
{
    public enum TaskStatus { Running, Completed, Failed, Stopped }

    public class TaskModel
    {
        public Guid Id { get; set; } = Guid.NewGuid();
        public string Type { get; set; } = string.Empty; // register|heartbeat|log|upload
        public DateTime StartTime { get; set; }
        public int ClientCount { get; set; }
        public int IntervalMs { get; set; }
        public int DurationMinutes { get; set; }
        public TaskStatus Status { get; set; }
        public int ActiveClientsCount { get; set; }
        public int FailedCount { get; set; }
    }
}
