using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

class Program
{
    static async Task Main()
    {
        Console.WriteLine("TestReceiver 启动：TCP 8441 / UDP 4565");

        var tcpTask = RunTcpListener(8441);
        var udpTask = RunUdpListener(4565);

        await Task.WhenAll(tcpTask, udpTask);
    }

    static async Task RunTcpListener(int port)
    {
        var listener = new TcpListener(IPAddress.Any, port);
        listener.Start();
        Console.WriteLine($"TCP 侦听中 {port}");
        while (true)
        {
            try
            {
                var client = await listener.AcceptTcpClientAsync();
                _ = HandleTcpClient(client);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"TCP 侦听异常: {ex.Message}");
            }
        }
    }

    static async Task HandleTcpClient(TcpClient client)
    {
        try
        {
            using var stream = client.GetStream();
            var buffer = new byte[4096];
            var len = await stream.ReadAsync(buffer, 0, buffer.Length);
            var s = Encoding.UTF8.GetString(buffer, 0, len);
            Console.WriteLine($"[TCP 收到] {s}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"处理 TCP 客户端异常: {ex.Message}");
        }
        finally
        {
            client.Close();
        }
    }

    static async Task RunUdpListener(int port)
    {
        using var udp = new UdpClient(port);
        Console.WriteLine($"UDP 侦听中 {port}");
        while (true)
        {
            try
            {
                var res = await udp.ReceiveAsync();
                var s = Encoding.UTF8.GetString(res.Buffer);
                Console.WriteLine($"[UDP 收到] {s} from {res.RemoteEndPoint}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"UDP 侦听异常: {ex.Message}");
            }
        }
    }
}
