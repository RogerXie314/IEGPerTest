using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Network
{
    public interface INetworkSender
    {
        /// <summary>
        /// 通过 TCP 发送数据到指定主机端口，返回是否成功。
        /// </summary>
        Task<bool> SendTcpAsync(string host, int port, byte[] payload, int timeoutMs, CancellationToken ct);
    }
}
