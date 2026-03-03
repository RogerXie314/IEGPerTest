using System;
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
    /// 与心跳共用的同一个 <c>CWLTcpConnect</c> 实例，日志发送直接在心跳 socket 上 WriteAsync。
    /// </para>
    /// <para>
    /// 本工具的对齐实现：HeartbeatWorker 每次成功建立 TCP 连接后，将
    /// <c>clientId → NetworkStream</c> 注册到此表；连接断开时注销。
    /// LogWorker 威胁检测通道发送前先从此表取 stream，若有则直接写入，
    /// 避免为同一 clientId 新建额外 TCP 连接导致平台踢掉心跳 session。
    /// </para>
    /// <para>
    /// 线程安全：<c>NetworkStream.WriteAsync</c> 不允许并发调用（心跳写 + 日志写会互相
    /// 穿插导致包体损坏）。每个 clientId 对应一把 <c>SemaphoreSlim(1,1)</c> 写锁，
    /// HeartbeatWorker 和 LogWorker 在 WriteAsync 前均须通过 <c>AcquireWriteLockAsync</c>
    /// 获取锁，操作完成后调用 <c>ReleaseWriteLock</c> 释放。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>每个 clientId 独占一把写锁，防止心跳写与日志写并发损坏包体。</summary>
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _writeLocks = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值）。</summary>
        public void Register(string clientId, NetworkStream stream)
        {
            _streams[clientId] = stream;
            // 写锁按需创建，重连时复用（锁本身与连接生命周期无关）
            _writeLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
        }

        /// <summary>注销心跳连接（连接关闭时调用）。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);
        // 注意：_writeLocks 的 SemaphoreSlim 故意不删除，避免重连期间等锁的一方出现 KeyNotFoundException。

        // ── 读取 stream ────────────────────────────────────────────────────────

        /// <summary>尝试获取心跳 stream；不存在时返回 null。</summary>
        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 写锁 API ───────────────────────────────────────────────────────────

        /// <summary>
        /// 获取指定 clientId 的写锁（异步，最长等待 timeoutMs 毫秒）。
        /// 返回 true 表示成功获取；false 表示超时或锁不存在（此时不得写入 stream）。
        /// 调用方须在 finally 块中调用 <c>ReleaseWriteLock</c>，无论是否获取成功均安全。
        /// </summary>
        public async Task<bool> AcquireWriteLockAsync(string clientId, int timeoutMs, CancellationToken ct)
        {
            if (!_writeLocks.TryGetValue(clientId, out var sem)) return false;
            try
            {
                return await sem.WaitAsync(timeoutMs, ct).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                return false;
            }
        }

        /// <summary>
        /// 释放指定 clientId 的写锁。如果未持有锁（获取时返回 false），依然可以安全调用。
        /// </summary>
        public void ReleaseWriteLock(string clientId, bool acquired)
        {
            if (!acquired) return;
            if (_writeLocks.TryGetValue(clientId, out var sem))
                sem.Release();
        }

        public int Count => _streams.Count;
    }
}
