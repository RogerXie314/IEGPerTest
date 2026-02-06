using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Network
{
    public class UdpSender : IUdpSender
    {
        public async Task<bool> SendUdpAsync(string host, int port, byte[] payload, int timeoutMs, CancellationToken ct)
        {
            try
            {
                using var udp = new UdpClient();
                var end = new IPEndPoint(IPAddress.Any, 0);
                var addr = Dns.GetHostAddresses(host);
                if (addr == null || addr.Length == 0) return false;
                var remote = new IPEndPoint(addr[0], port);
                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(timeoutMs);
                await udp.SendAsync(payload, payload.Length, remote).WaitAsync(cts.Token).ConfigureAwait(false);
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
