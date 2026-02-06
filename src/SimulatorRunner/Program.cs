using System;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Workers;

class Program
{
    static async Task Main()
    {
        Console.WriteLine("SimulatorRunner 启动");

        var reg = new RegistrationWorker();
        Console.WriteLine("开始注册 5 个客户端到 Clients.log...");
        await reg.RegisterAsync("Client-", 1, 5);
        Console.WriteLine("注册完成。");

        var hb = new HeartbeatWorker();

        using var cts = new CancellationTokenSource();
        // 启动心跳循环，5 次后取消
        var task = hb.StartAsync(1000, cts.Token);

        await Task.Delay(5500);
        cts.Cancel();
        await task.ConfigureAwait(false);

        Console.WriteLine("心跳演示结束");
    }
}
