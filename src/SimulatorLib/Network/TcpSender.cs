using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Network
{
    public class TcpSender : INetworkSender
    {
        /// <summary>
        /// 尝试建立 TCP 连接并发送 payload，超时或异常返回 false。
        /// </summary>
        public async Task<bool> SendTcpAsync(string host, int port, byte[] payload, int timeoutMs, CancellationToken ct)
        {
            try
            {
                using var tcp = new TcpClient();
                var connectTask = tcp.ConnectAsync(host, port);
                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(timeoutMs);
                var completed = await Task.WhenAny(connectTask, Task.Delay(Timeout.Infinite, cts.Token)).ConfigureAwait(false);
                if (!tcp.Connected) return false;

                using var stream = tcp.GetStream();
                await stream.WriteAsync(payload, 0, payload.Length, ct).ConfigureAwait(false);
                await stream.FlushAsync(ct).ConfigureAwait(false);
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
