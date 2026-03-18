using System.Collections.Concurrent;
using System.Net.Sockets;
using System.Threading.Channels;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream 注册表 + 每客户端日志包队列。
    /// <para>
    /// <b>架构原则（对齐老工具 C++ 行为）：</b>
    /// HeartbeatWorker 是每个客户端 TCP 连接的唯一写入方。
    /// LogWorker 通过 <c>TryEnqueueLog</c> 将日志包投入每客户端的
    /// <c>Channel&lt;byte[]&gt;</c>，再由 HeartbeatWorker 在心跳间隙
    /// （写完 HB 之后到下次 HB 截止前 100ms）统一 drain 并写入 TCP。
    /// </para>
    /// <para>
    /// 这彻底消除并发 TCP 写：无锁、无超时、无 LockBusy。
    /// 与老工具对应：HeartbeatThread + MsgLogThread 都用 g_sock[i]，
    /// OS Winsock 在内核层串行化；本工具由 HeartbeatWorker 统一串行写，效果等价。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>每个 clientId 的日志包队列：LogWorker 生产，HeartbeatWorker 消费。</summary>
        private readonly ConcurrentDictionary<string, Channel<byte[]>> _logQueues = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream（每次重连时覆盖旧值），同时确保日志队列已存在。</summary>
        public void Register(string clientId, NetworkStream stream)
        {
            _streams[clientId] = stream;
            _logQueues.GetOrAdd(clientId, _ => Channel.CreateBounded<byte[]>(
                new BoundedChannelOptions(2000)
                {
                    FullMode                      = BoundedChannelFullMode.DropOldest,
                    SingleReader                  = true,   // HeartbeatWorker 单消费者
                    SingleWriter                  = false,  // LogWorker 多线程并发入队
                    AllowSynchronousContinuations = false,
                }));
        }

        /// <summary>注销心跳连接（连接关闭时调用）。日志队列保留，避免重连期间入队失败。</summary>
        public void Unregister(string clientId)
            => _streams.TryRemove(clientId, out _);

        // ── Stream 读取（HeartbeatWorker 内部用）────────────────────────────────

        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 日志队列 API ───────────────────────────────────────────────────────

        /// <summary>
        /// LogWorker 调用：将日志包非阻塞投入队列，立即返回。
        /// 返回 null：成功；"no_queue"：clientId 尚未注册（客户端还未完成首次握手）。
        /// 队列已满时自动丢弃最旧条目（BoundedChannelFullMode.DropOldest），永不阻塞。
        /// </summary>
        public string? TryEnqueueLog(string clientId, byte[] payload)
        {
            // 客户端断线期间 _streams 中无该条目：静默丢弃，对齐老工具 C++ 行为——
            // 老工具 Log 线程握有重连前的 stale socket，send() 直接返回 SOCKET_ERROR，
            // 相当于断线窗口内所有包静默失败，重连后无积压 burst，平台不触发 FIN。
            if (!_streams.ContainsKey(clientId)) return "disconnected";
            if (!_logQueues.TryGetValue(clientId, out var ch)) return "no_queue";
            ch.Writer.TryWrite(payload); // DropOldest 模式下必然成功
            return null;
        }

        /// <summary>
        /// HeartbeatWorker 调用：获取日志队列读端，用于 drain 循环。
        /// </summary>
        public ChannelReader<byte[]>? GetLogReader(string clientId)
        {
            _logQueues.TryGetValue(clientId, out var ch);
            return ch?.Reader;
        }

        public int Count => _streams.Count;
    }
}
