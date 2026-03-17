using System.Collections.Concurrent;
using System.Net.Sockets;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream 注册表。
    /// <para>
    /// 原项目（WLData.cpp）：威胁检测日志通过 <c>SetWLTcpConnectInstance</c> 注入
    /// 与心跳共用的同一个 <c>CWLTcpConnect</c> 实例，日志发送直接在心跳 socket 上调用同步 send()。
    /// </para>
    /// <para>
    /// 本工具的对齐实现：HeartbeatWorker 每次成功建立 TCP 连接后，将
    /// <c>clientId → NetworkStream</c> 注册到此表；连接断开时注销。
    /// LogWorker 威胁检测通道发送前先从此表取 stream，若有则直接写入，
    /// 避免为同一 clientId 新建额外 TCP 连接导致平台踢掉心跳 session。
    /// </para>
    /// <para>
    /// 线程安全：每个 clientId 对应一个普通 <c>object</c> 写锁，
    /// HeartbeatWorker 和 LogWorker 在 <c>Write()</c> 前均须 <c>lock(GetWriteLock(clientId))</c>。
    /// 使用同步 <c>lock</c> + <c>Write()</c>（非 async），完全对齐老工具 C++ 行为：
    /// OS TCP 层自动串行化并发 send()，无应用层超时/跳过机制，背压时阻塞线程而非丢弃心跳。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>每个 clientId 独占一个写锁对象，用于 lock() 块。重连时复用（锁与连接生命周期无关）。</summary>
        private readonly ConcurrentDictionary<string, object> _writeLocks = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值）。</summary>
        public void Register(string clientId, NetworkStream stream)
        {
            _streams[clientId] = stream;
            _writeLocks.GetOrAdd(clientId, _ => new object());
        }

        /// <summary>注销心跳连接（连接关闭时调用）。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);

        // ── 读取 stream ────────────────────────────────────────────────────────

        /// <summary>尝试获取心跳 stream；不存在时返回 null。</summary>
        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 写锁 API ───────────────────────────────────────────────────────────

        /// <summary>
        /// 获取指定 clientId 的写锁对象，供调用方在 <c>lock()</c> 块中使用。
        /// 重连时锁对象不变（稳定标识），无需担心拿到过期引用。
        /// </summary>
        public object? GetWriteLock(string clientId)
        {
            _writeLocks.TryGetValue(clientId, out var lck);
            return lck;
        }

        public int Count => _streams.Count;
    }
}
