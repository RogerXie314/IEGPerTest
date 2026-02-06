using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Network
{
    public interface IUdpSender
    {
        Task<bool> SendUdpAsync(string host, int port, byte[] payload, int timeoutMs, CancellationToken ct);
    }
}
