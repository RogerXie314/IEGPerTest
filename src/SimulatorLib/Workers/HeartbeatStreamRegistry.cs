using System.Collections.Concurrent;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Channels;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 心跳 TCP 连接的 stream/socket 注册表 + 每客户端日志包队列（fallback）。
    /// <para>
    /// <b>架构原则（v3.7.35 无锁直写，完全对齐老工具 C++ 行为）：</b>
    /// LogWorker 和 HeartbeatWorker 各自直接调用 Socket.Send()（阻塞式 Winsock send），
    /// OS 内核保证同一 socket 上并发 send() 的字节不交错（与 C++ send(g_sock[i]) 完全等价）。
    /// 无应用层锁，无超时，无 NetworkStream.WriteAsync（后者非线程安全）。
    /// </para>
    /// <para>
    /// Channel 保留作为 fallback（非 allThreatTcp 路径仍可用），但主路径不再经过 Channel。
    /// </para>
    /// </summary>
    public sealed class HeartbeatStreamRegistry
    {
        private readonly ConcurrentDictionary<string, NetworkStream> _streams = new();

        /// <summary>每客户端底层 Socket：用于 Send() 直写，绕过 NetworkStream 的非线程安全限制。</summary>
        private readonly ConcurrentDictionary<string, Socket> _sockets = new();

        /// <summary>每个 clientId 的日志包队列（fallback 用）：LogWorker 生产，HeartbeatWorker 消费。</summary>
        private readonly ConcurrentDictionary<string, Channel<byte[]>> _logQueues = new();

        // ── 注册 / 注销 ────────────────────────────────────────────────────────

        /// <summary>注册心跳连接的 stream + socket（每次重连时覆盖旧值），同时确保日志队列已存在。</summary>
        public void Register(string clientId, NetworkStream stream, Socket socket)
        {
            _streams[clientId] = stream;
            _sockets[clientId] = socket;
            _logQueues.GetOrAdd(clientId, _ => Channel.CreateBounded<byte[]>(
                new BoundedChannelOptions(2000)
                {
                    FullMode                      = BoundedChannelFullMode.DropOldest,
                    SingleReader                  = true,
                    SingleWriter                  = false,
                    AllowSynchronousContinuations = false,
                }));
        }

        /// <summary>注销心跳连接（连接关闭时调用）。日志队列保留，避免重连期间 NPE。</summary>
        public void Unregister(string clientId)
        {
            _streams.TryRemove(clientId, out _);
            _sockets.TryRemove(clientId, out _);
        }

        // ── Stream 读取 ────────────────────────────────────────────────────────

        public NetworkStream? TryGet(string clientId)
        {
            _streams.TryGetValue(clientId, out var s);
            return s;
        }

        // ── 直写 API（无锁，Socket.Send 内核串行化）─────────────────────────

        /// <summary>
        /// 直接将 payload 写入底层 Socket（阻塞式 Winsock send）。
        /// 返回 null 表示成功，非 null 字符串为失败原因。
        /// <para>
        /// v3.7.35: 彻底去掉应用层写锁，改用 Socket.Send()。
        /// Winsock send() 在 TCP 流式 socket 上是线程安全的 — OS 内核保证并发 send()
        /// 的字节不交错，与老工具 C++ send(g_sock[i]) 行为完全一致。
        /// 对于小包（PT 包 &lt; 2KB），send() 立即拷入内核缓冲区（微秒级），无背压阻塞。
        /// </para>
        /// </summary>
        public string? DirectSend(string clientId, byte[] payload)
        {
            if (!_sockets.TryGetValue(clientId, out var socket)) return "disconnected";
            try
            {
                socket.Send(payload, SocketFlags.None);
                return null;
            }
            catch (Exception ex)
            {
                return ex.GetType().Name;
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
            if (!_sockets.ContainsKey(clientId)) return "disconnected";
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

        public int Count => _streams.Count;
    }
}
