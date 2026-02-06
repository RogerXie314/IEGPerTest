using System;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class HeartbeatWorker
    {
        public async Task RunOnceAsync()
        {
            var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            Console.WriteLine($"心跳任务：读取到 {clients.Count} 条客户端记录");
            foreach (var c in clients)
            {
                // 这里仅模拟心跳发送；后续替换为真实网络发送逻辑
                Console.WriteLine($"发送心跳 -> {c.ClientId} @ {c.IP} (注册时间: {c.RegisteredAt})");
            }
        }

        public async Task StartAsync(int intervalMs, CancellationToken ct)
        {
            while (!ct.IsCancellationRequested)
            {
                await RunOnceAsync().ConfigureAwait(false);
                try
                {
                    await Task.Delay(intervalMs, ct).ConfigureAwait(false);
                }
                catch (TaskCanceledException) { break; }
            }
        }
    }
}
