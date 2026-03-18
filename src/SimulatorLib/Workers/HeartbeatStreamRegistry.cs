using System.Collections.Concurrent;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Channels;
using SimulatorLib.Network;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream 注册表 + 直写锁 + 每客户端日志包队列（fallback）。
    /// <para>
    /// <b>架构原则（v3.7.27 直写架构，对齐老工具 C++ 行为）：</b>
    /// LogWorker 和 HeartbeatWorker 各自通过轻量 SemaphoreSlim(1,1) 写锁直接写 TCP stream，
    /// 还原 C++ 两线程各自 send(g_sock[i]) + OS 内核串行化的原始模型。
    /// 锁持有时间仅为单包 WriteAsync（NoDelay=true，微秒级），竞争概率 &lt; 0.5%。
    /// </para>
    /// <para>
    /// Channel 保留作为 fallback（非 allThreatTcp 路径仍可用），但主路径不再经过 Channel。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>Native SOCKET 句柄注册表（v3.7.30 NativeSender DLL 路径）。</summary>
        private readonly ConcurrentDictionary<string, ulong> _nativeSockets = new();

        /// <summary>每客户端 TCP 写锁：HeartbeatWorker + LogWorker 共用，SemaphoreSlim(1,1) 异步互斥。</summary>
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _writeLocks = new();

        /// <summary>每个 clientId 的日志包队列（fallback 用）：LogWorker 生产，HeartbeatWorker 消费。</summary>
        private readonly ConcurrentDictionary<string, Channel<byte[]>> _logQueues = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值），同时确保写锁和日志队列已存在。</summary>
        public void Register(string clientId, NetworkStream stream)
        {
            _streams[clientId] = stream;
            _writeLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
            _logQueues.GetOrAdd(clientId, _ => Channel.CreateBounded<byte[]>(
                new BoundedChannelOptions(2000)
                {
                    FullMode                      = BoundedChannelFullMode.DropOldest,
                    SingleReader                  = true,
                    SingleWriter                  = false,
                    AllowSynchronousContinuations = false,
                }));
        }

        /// <summary>注销心跳连接（连接关闭时调用）。写锁和日志队列保留，避免重连期间 NPE。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);

        // ── Native Socket 注册/注销（v3.7.30 NativeSender DLL）────────────────

        /// <summary>注册 native SOCKET 句柄（每次重连时覆盖旧值），同时确保写锁已存在。</summary>
        public void RegisterNative(string clientId, ulong sock)
        {
            _nativeSockets[clientId] = sock;
            _writeLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
            _logQueues.GetOrAdd(clientId, _ => Channel.CreateBounded<byte[]>(
                new BoundedChannelOptions(2000)
                {
                    FullMode                      = BoundedChannelFullMode.DropOldest,
                    SingleReader                  = true,
                    SingleWriter                  = false,
                    AllowSynchronousContinuations = false,
                }));
        }

        /// <summary>注销 native SOCKET（连接关闭时调用）。</summary>
        public void UnregisterNative(string clientId)
            => _nativeSockets.TryRemove(clientId, out _);

        /// <summary>获取 native SOCKET 句柄，不存在返回 INVALID_SOCKET。</summary>
        public ulong GetNativeSocket(string clientId)
            => _nativeSockets.TryGetValue(clientId, out var s) ? s : NativeSenderInterop.INVALID_SOCKET;

        // ── Stream 读取 ────────────────────────────────────────────────────────

        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 写锁 API（直写架构核心）────────────────────────────────────────────

        /// <summary>
        /// 获取指定客户端的写锁。调用方必须在 finally 中调用 <see cref="ReleaseWriteLock"/>。
        /// 超时返回 false（调用方应跳过本次写入，不断连接）。
        /// </summary>
        public async Task<bool> AcquireWriteLockAsync(string clientId, int timeoutMs, CancellationToken ct)
        {
            if (!_writeLocks.TryGetValue(clientId, out var sem)) return false;
            return await sem.WaitAsync(timeoutMs, ct).ConfigureAwait(false);
        }

        /// <summary>释放写锁。</summary>
        public void ReleaseWriteLock(string clientId)
        {
            if (_writeLocks.TryGetValue(clientId, out var sem))
            {
                try { sem.Release(); } catch (SemaphoreFullException) { /* 防御性 */ }
            }
        }

        // ── 直写 API ──────────────────────────────────────────────────────────

        /// <summary>
        /// 直接将 payload 写入 TCP stream（获取写锁 → WriteAsync → 释放写锁）。
        /// 返回 null 表示成功，非 null 字符串为失败原因。
        /// 对齐 C++ Log 线程直接 send(sock) 的数据路径，消除 Channel 间接层延迟。
        /// </summary>
        public async Task<string?> DirectSendAsync(string clientId, byte[] payload, CancellationToken ct)
        {
            // ── Native DLL 路径优先（v3.7.30）──
            if (_nativeSockets.TryGetValue(clientId, out var nativeSock))
            {
                if (!_writeLocks.TryGetValue(clientId, out var nsem)) return "no_lock";
                if (!await nsem.WaitAsync(2000, ct).ConfigureAwait(false)) return "lock_timeout";
                try
                {
                    return NativeSenderInterop.NS_SendData(nativeSock, payload, payload.Length) == 0
                        ? null : "send_failed";
                }
                finally
                {
                    try { nsem.Release(); } catch (SemaphoreFullException) { }
                }
            }

            // ── 托管 NetworkStream 路径（fallback）──
            if (!_streams.TryGetValue(clientId, out var stream)) return "disconnected";
            if (!_writeLocks.TryGetValue(clientId, out var sem)) return "no_lock";

            // 写锁超时 2s：正常情况 < 1ms（单小包 NoDelay），超时说明连接异常或 HB 正在写
            if (!await sem.WaitAsync(2000, ct).ConfigureAwait(false)) return "lock_timeout";
            try
            {
                // !! 不传 CancellationToken 给 WriteAsync：CT 取消会导致 .NET RST socket，
                // 对齐 v3.7.3 Fix 1（去除 WriteAsync/FlushAsync 的 CT，背压时不再 RST socket）。
                // 若连接已死，TCP KeepAlive（idle=5s, interval=2s）会在 ~15s 内触发 IOException。
                await stream.WriteAsync(payload, 0, payload.Length).ConfigureAwait(false);
                return null;
            }
            catch (Exception ex)
            {
                return ex.GetType().Name;
            }
            finally
            {
                try { sem.Release(); } catch (SemaphoreFullException) { }
            }
        }

        // ── 日志队列 API（fallback，非 allThreatTcp 路径使用）──────────────────

        /// <summary>
        /// drain 速率间隔（ms/包）：已弃用（直写架构下不再需要）。保留以兼容旧调用。
        /// </summary>
        public volatile int LogDrainIntervalMs = 0;

        /// <summary>
        /// LogWorker 调用（fallback 路径）：将日志包非阻塞投入队列，立即返回。
        /// </summary>
        public string? TryEnqueueLog(string clientId, byte[] payload)
        {
            if (!_streams.ContainsKey(clientId)) return "disconnected";
            if (!_logQueues.TryGetValue(clientId, out var ch)) return "no_queue";
            ch.Writer.TryWrite(payload);
            return null;
        }

        /// <summary>
        /// HeartbeatWorker 调用：获取日志队列读端，用于 drain 循环（处理 fallback 积压）。
        /// </summary>
        public ChannelReader<byte[]>? GetLogReader(string clientId)
        {
            _logQueues.TryGetValue(clientId, out var ch);
            return ch?.Reader;
        }

        public int Count => _streams.Count + _nativeSockets.Count;
    }
}
