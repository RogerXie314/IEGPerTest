using System.Collections.Concurrent;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream 注册表。
    /// <para>
    /// 原项目（WLData.cpp）：威胁检测日志通过 <c>SetWLTcpConnectInstance</c> 注入
    /// 与心跳共用的同一个 <c>CWLTcpConnect</c> 实例，日志发送直接在心跳 socket 上 send()。
    /// </para>
    /// <para>
    /// 本工具的对齐实现：HeartbeatWorker 每次成功建立 TCP 连接后，将
    /// <c>clientId → NetworkStream</c> 注册到此表；连接断开时注销。
    /// LogWorker 威胁检测通道发送前先从此表取 stream，若有则直接写入，
    /// 避免为同一 clientId 新建额外 TCP 连接导致平台踢掉心跳 session。
    /// </para>
    /// <para>
    /// 线程安全：每个 clientId 对应一把 <c>SemaphoreSlim(1,1)</c> 写锁。
    /// 获取方式：<c>await AcquireWriteLockAsync(clientId, ct)</c>（不设超时）。
    /// 背压时异步等待（不阻塞线程池线程）；写完后在 finally 块释放。
    /// 对齐 C++ 互斥语义：心跳或日志被延迟，而非被丢弃（无 LockBusy skip）。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>每个 clientId 独占一把异步写锁，重连时复用（锁与连接生命周期无关）。</summary>
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _writeLocks = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值）。</summary>
        public void Register(string clientId, NetworkStream stream)
        {
            _streams[clientId] = stream;
            _writeLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
        }

        /// <summary>注销心跳连接（连接关闭时调用）。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);
        // _writeLocks 的 SemaphoreSlim 故意不删除，避免重连期间等锁的一方出现 KeyNotFoundException。

        // ── 读取 stream ────────────────────────────────────────────────────────

        /// <summary>尝试获取心跳 stream；不存在时返回 null。</summary>
        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 写锁 API ───────────────────────────────────────────────────────────

        /// <summary>
        /// 异步获取指定 clientId 的写锁（不设超时，等到能拿为止）。
        /// 异步等待不阻塞线程池线程，背压时本 Task 挂起，其他 Task 继续运行。
        /// 调用方须在 finally 块中调用 <c>ReleaseWriteLock</c>。
        /// </summary>
        public async Task AcquireWriteLockAsync(string clientId, CancellationToken ct)
        {
            if (!_writeLocks.TryGetValue(clientId, out var sem)) return;
            await sem.WaitAsync(ct).ConfigureAwait(false);
        }

        /// <summary>释放指定 clientId 的写锁。</summary>
        public void ReleaseWriteLock(string clientId)
        {
            if (_writeLocks.TryGetValue(clientId, out var sem))
                sem.Release();
        }

        public int Count => _streams.Count;
    }
}
