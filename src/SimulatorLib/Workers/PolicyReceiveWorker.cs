using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Protocol;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 策略下发接收与回包处理。
    /// 平台通过已建立的 TCP 长连接（心跳连接）向客户端推送策略命令，
    /// 客户端（本工具）须按协议回包，平台才显示"策略已生效"。
    ///
    /// 使用方式：
    ///   在 HeartbeatWorker 的 drain 循环中调用 ProcessStreamAsync，
    ///   它会持续读取并解析 PT 包，识别下行策略命令后自动回包。
    /// </summary>
    public class PolicyReceiveWorker
    {
        private static readonly Random _rng = new();

        // 已知平台侧推送策略的 cmdId 范围（可按实际协议扩展）
        // 0x65 = 101：策略下发（参考原始 C++ WLMainControl 推送命令字）
        // 0x66 = 102：白名单下发
        // 0x67 = 103：黑名单下发
        // 0+   ：凡非心跳（cmdId!=1）的下行命令一律视为策略，均回包
        private const uint CmdHeartbeat = 1;

        /// <summary>统计：收到策略数量</summary>
        public int ReceivedCount => _receivedCount;
        private int _receivedCount;

        /// <summary>统计：已回包数量</summary>
        public int RepliedCount => _repliedCount;
        private int _repliedCount;

        /// <summary>
        /// 流式读取并处理 TCP 流上的下行 PT 包，遇到策略命令时自动回包。
        /// 返回 false 表示连接已关闭（n==0），返回 true 表示因取消而退出。
        /// </summary>
        public async Task<bool> ProcessStreamAsync(
            NetworkStream stream,
            string clientId,
            uint deviceId,
            CancellationToken ct)
        {
            // 用于累积不完整 TCP 片段的缓冲区
            var buf = new byte[65536];
            var accumulated = new System.IO.MemoryStream();

            try
            {
                while (!ct.IsCancellationRequested)
                {
                    int n = await stream.ReadAsync(buf, 0, buf.Length, ct).ConfigureAwait(false);
                    if (n == 0) return false; // 服务端 FIN

                    accumulated.Write(buf, 0, n);

                    // 尝试从 accumulated 中解析完整的 PT 包
                    await TryProcessPackets(accumulated, stream, clientId, deviceId, ct).ConfigureAwait(false);
                }
            }
            catch (OperationCanceledException) { }
            catch { return false; }

            return true;
        }

        // ── 私有实现 ──────────────────────────────────────────────────────────

        private async Task TryProcessPackets(
            System.IO.MemoryStream buf,
            NetworkStream stream,
            string clientId,
            uint deviceId,
            CancellationToken ct)
        {
            buf.Position = 0;
            var data = buf.ToArray();
            int offset = 0;

            while (offset < data.Length)
            {
                // 需要至少 HeaderLength(48) 字节才能读包头
                if (data.Length - offset < PtProtocol.HeaderLength) break;

                // 校验 PT Magic
                if (data[offset] != (byte)'P' || data[offset + 1] != (byte)'T')
                {
                    // 非法数据，跳过 1 字节继续
                    offset++;
                    continue;
                }

                // 读取 bodyLen (offset+4, uint16 big-endian)
                ushort bodyLen = BinaryPrimitives.ReadUInt16BigEndian(data.AsSpan(offset + 4, 2));
                int totalLen = PtProtocol.HeaderLength + bodyLen;

                if (data.Length - offset < totalLen) break; // 包不完整，等待更多数据

                // 读取 cmdId (offset+32, uint32 big-endian)
                uint cmdId = BinaryPrimitives.ReadUInt32BigEndian(data.AsSpan(offset + 32, 4));

                if (cmdId != CmdHeartbeat && cmdId != 0)
                {
                    // 非心跳的下行命令 → 视为策略推送，需要回包
                    Interlocked.Increment(ref _receivedCount);
                    await HandlePolicyCommandAsync(stream, clientId, deviceId, cmdId, ct).ConfigureAwait(false);
                }

                offset += totalLen;
            }

            // 重置 MemoryStream，仅保留未处理的尾部数据
            var remaining = data.AsSpan(offset).ToArray();
            buf.SetLength(0);
            if (remaining.Length > 0) buf.Write(remaining, 0, remaining.Length);
        }

        private async Task HandlePolicyCommandAsync(
            NetworkStream stream,
            string clientId,
            uint deviceId,
            uint cmdId,
            CancellationToken ct)
        {
            try
            {
                // 模拟客户端处理策略的延迟（500ms ~ 2000ms）
                int delay;
                lock (_rng) { delay = _rng.Next(500, 2001); }
                await Task.Delay(delay, ct).ConfigureAwait(false);

                // 构建 Result 回包：告知平台策略已应用（dealResult=0 表示成功）
                var resultPayload = BuildPolicyResultJson(clientId, cmdId, dealResult: 0);
                var resultBytes   = Encoding.UTF8.GetBytes(resultPayload);
                var packet        = PtProtocol.Pack(resultBytes, cmdId: cmdId, deviceId: deviceId);

                using var writeCts = System.Threading.CancellationTokenSource.CreateLinkedTokenSource(ct);
                writeCts.CancelAfter(5000);
                await stream.WriteAsync(packet, 0, packet.Length, writeCts.Token).ConfigureAwait(false);
                await stream.FlushAsync(writeCts.Token).ConfigureAwait(false);

                Interlocked.Increment(ref _repliedCount);
            }
            catch { /* 回包失败不影响主流程 */ }
        }

        private static string BuildPolicyResultJson(string clientId, uint cmdId, int dealResult)
        {
            // 参考 UsmRegistrationJsonBuilder.BuildClientResultJson 的结构
            var root = new JsonArray
            {
                new JsonObject
                {
                    ["ComputerID"] = clientId,
                    ["CMDTYPE"]    = 0x01,
                    ["CMDID"]      = (int)cmdId,
                    ["DealResult"] = dealResult,
                    ["Timestamp"]  = DateTimeOffset.UtcNow.ToUnixTimeSeconds()
                }
            };
            return root.ToJsonString() + "\n";
        }
    }
}
