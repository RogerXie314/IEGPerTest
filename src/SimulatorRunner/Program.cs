using System;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Workers;

class Program
{
    static async Task Main()
    {
        Console.WriteLine("SimulatorRunner 启动");

        var sender = new SimulatorLib.Network.TcpSender();
        var reg = new RegistrationWorker(sender);
        Console.WriteLine("开始并发注册 5 个客户端到 Clients.log（带重试与超时）...");
        using (var regCts = new CancellationTokenSource())
        {
            await reg.RegisterAsync("Client-", 1, 5, host: "localhost", port: 8441, concurrency: 4, retry: 3, timeoutMs: 1500, ct: regCts.Token);
        }
        Console.WriteLine("注册流程完成（结果写入 Clients.log）。");

        var tcp = new SimulatorLib.Network.TcpSender();
        var udp = new SimulatorLib.Network.UdpSender();
        var hb = new SimulatorLib.Workers.HeartbeatWorker(tcp, udp);

        using var cts = new CancellationTokenSource();
        Console.WriteLine("开始心跳任务演示（每秒一次，10 并发，演示 5 次）...");
        var hbTask = hb.StartAsync(1000, useLogServer: true, platformHost: "localhost", platformPort: 8441, logHost: "localhost", logPort: 4565, concurrency: 10, ct: cts.Token);

        await Task.Delay(5500);
        cts.Cancel();
        await hbTask.ConfigureAwait(false);

        Console.WriteLine("心跳演示结束");
    }
}
