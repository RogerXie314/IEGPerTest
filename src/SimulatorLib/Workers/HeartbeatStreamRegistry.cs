using System.Collections.Concurrent;
using System.Net.Sockets;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream 注册表。
    /// <para>
    /// 原项目（WLData.cpp）：威胁检测日志通过 <c>SetWLTcpConnectInstance</c> 注入
    /// 与心跳共用的同一个 <c>CWLTcpConnect</c> 实例，日志发送直接在心跳 socket 上 WriteAsync。
    /// </para>
    /// <para>
    /// 本工具的对齐实现：HeartbeatWorker 每次成功建立 TCP 连接后，将
    /// <c>clientId → NetworkStream</c> 注册到此表；连接断开时注销。
    /// LogWorker 威胁检测通道发送前先从此表取 stream，若有则直接写入，
    /// 避免为同一 clientId 新建额外 TCP 连接导致平台踢掉心跳 session。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值）。</summary>
        public void Register(string clientId, NetworkStream stream)
            => _streams[clientId] = stream;

        /// <summary>注销心跳连接（连接关闭时调用）。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);

        /// <summary>尝试获取心跳 stream；不存在或连接已关闭返回 null。</summary>
        public NetworkStream? TryGet(string clientId)
        {
            if (_streams.TryGetValue(clientId, out var s))
                return s;
            return null;
        }

        public int Count => _streams.Count;
    }
}
