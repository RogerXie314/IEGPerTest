using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;

namespace SimulatorLib.Persistence
{
    public record ClientRecord(string ClientId, string IP, DateTime RegisteredAt, string Status)
    {
        public uint DeviceId { get; init; } = 0;
        public int TcpPort { get; init; } = 0;
    }

    public static class ClientsPersistence
    {
        private static readonly string FilePath = Path.Combine(AppContext.BaseDirectory, "Clients.log");

        // 兜底保护：防止多处直接调用 AppendAsync 时出现并发冲突。
        // 正常路径下，RegistrationWorker 通过 Channel<ClientRecord> 保证只有单一消费者
        // 调用 AppendAsync，此锁实际上不会产生竞争，但保留作为防御性保障。
        private static readonly SemaphoreSlim _writeLock = new SemaphoreSlim(1, 1);

        public static string GetPath() => FilePath;

        public static async Task AppendAsync(ClientRecord record)
        {
            var line = JsonSerializer.Serialize(record);
            await _writeLock.WaitAsync().ConfigureAwait(false);
            try
            {
                // FileShare.ReadWrite：写入期间允许其他方读取文件，不影响性能监控等工具实时查看日志
                using var fs = new FileStream(FilePath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite);
                using var sw = new StreamWriter(fs);
                await sw.WriteLineAsync(line).ConfigureAwait(false);
            }
            finally
            {
                _writeLock.Release();
            }
        }

        public static async Task<List<ClientRecord>> ReadAllAsync()
        {
            var list = new List<ClientRecord>();
            if (!File.Exists(FilePath)) return list;
            // 允许其他进程读取，但阻止写入时读取可能读到不完整内容
            using var fs = new FileStream(FilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var sr = new StreamReader(fs);
            string? line;
            while ((line = await sr.ReadLineAsync().ConfigureAwait(false)) != null)
            {
                try
                {
                    var rec = JsonSerializer.Deserialize<ClientRecord>(line);
                    if (rec != null) list.Add(rec);
                }
                catch { /* 忽略单行解析错误 */ }
            }
            return list;
        }

        public static bool Exists()
        {
            return File.Exists(FilePath);
        }

        public static void Delete()
        {
            if (File.Exists(FilePath)) File.Delete(FilePath);
        }
    }
}
