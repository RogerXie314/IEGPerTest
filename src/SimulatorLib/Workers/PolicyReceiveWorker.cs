using System;
using System.Buffers.Binary;
using System.Net.Http;
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
    /// 客户端（本工具）须解析命令后，通过 HTTPS POST 向平台的
    /// /USM/clientResult.do 上报执行结果，平台才会更新下发状态为"已发送/成功"。
    ///
    /// 使用方式：
    ///   在 HeartbeatWorker 的 drain 循环中调用 ProcessStreamAsync，
    ///   它会持续读取并解析 PT 包，识别下行策略命令后自动通过 HTTPS 上报结果。
    /// </summary>
    public class PolicyReceiveWorker
    {
        private static readonly Random _rng = new();

        // 共享 HttpClient（忽略证书：平台常用自签名证书）
        private static readonly HttpClient _http = CreateHttpClient();

        private readonly string _host;
        private readonly int    _port;

        // 策略结果接口中 CMDTYPE 固定值（接口文档：CMDTYPE=150）
        private const int  CmdTypePolicy  = 150;
        private const uint CmdHeartbeat   = 1;

        /// <summary>统计：收到策略数量</summary>
        public int ReceivedCount => _receivedCount;
        private int _receivedCount;

        /// <summary>统计：已回包数量（HTTPS POST 成功）</summary>
        public int RepliedCount => _repliedCount;
        private int _repliedCount;

        /// <param name="host">平台 HTTPS 主机（与心跳 TCP 主机相同）</param>
        /// <param name="port">平台 HTTPS 端口（通常 8441）</param>
        public PolicyReceiveWorker(string host, int port)
        {
            _host = host;
            _port = port;
        }

        /// <summary>
        /// 流式读取并处理 TCP 流上的下行 PT 包，遇到策略命令时通过 HTTPS 上报结果。
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
                    await TryProcessPackets(accumulated, clientId, ct).ConfigureAwait(false);
                }
            }
            catch (OperationCanceledException) { }
            catch { return false; }

            return true;
        }

        // ── 私有实现 ──────────────────────────────────────────────────────────

        private async Task TryProcessPackets(
            System.IO.MemoryStream buf,
            string clientId,
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
                    // 非心跳的下行命令 → 视为策略推送，通过 HTTPS 上报结果
                    Interlocked.Increment(ref _receivedCount);
                    await HandlePolicyCommandAsync(clientId, cmdId, ct).ConfigureAwait(false);
                }

                offset += totalLen;
            }

            // 重置 MemoryStream，仅保留未处理的尾部数据
            var remaining = data.AsSpan(offset).ToArray();
            buf.SetLength(0);
            if (remaining.Length > 0) buf.Write(remaining, 0, remaining.Length);
        }

        private async Task HandlePolicyCommandAsync(
            string clientId,
            uint cmdId,
            CancellationToken ct)
        {
            try
            {
                // 模拟客户端处理策略的延迟（500ms ~ 2000ms）
                int delay;
                lock (_rng) { delay = _rng.Next(500, 2001); }
                await Task.Delay(delay, ct).ConfigureAwait(false);

                // 通过 HTTPS POST 向平台 /USM/clientResult.do 上报执行结果
                // 接口文档：CMDTYPE=150, CMDID=<策略命令字>, CMDVER=1, CMDContent.RESULT="SUC"
                var resultJson = BuildPolicyResultJson(clientId, (int)cmdId, success: true);
                var resultUrl  = new Uri($"https://{_host}:{_port}/USM/clientResult.do");

                using var content = new StringContent(resultJson, Encoding.UTF8, "application/json");
                using var cts     = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(8000);
                using var resp = await _http.PostAsync(resultUrl, content, cts.Token).ConfigureAwait(false);

                if (resp.IsSuccessStatusCode)
                    Interlocked.Increment(ref _repliedCount);
            }
            catch { /* 回包失败不影响主流程 */ }
        }

        /// <summary>
        /// 构建策略执行结果 JSON。
        /// 接口文档示例（/USM/clientResult.do）：
        /// [{"ComputerID":"FEFOEACD","CMDTYPE":150,"CMDID":247,"CMDVER":1,
        ///   "CMDContent":{"RESULT":"SUC","REASON":""}}]
        /// </summary>
        private static string BuildPolicyResultJson(string clientId, int cmdId, bool success)
        {
            var root = new JsonArray
            {
                new JsonObject
                {
                    ["ComputerID"] = clientId,
                    ["CMDTYPE"]    = CmdTypePolicy,   // 150：策略类命令固定值
                    ["CMDID"]      = cmdId,
                    ["CMDVER"]     = 1,
                    ["CMDContent"] = new JsonObject
                    {
                        ["RESULT"] = success ? "SUC" : "FAIL",
                        ["REASON"] = ""
                    }
                }
            };
            return root.ToJsonString() + "\n";
        }

        private static HttpClient CreateHttpClient()
        {
            var handler = new HttpClientHandler
            {
                ServerCertificateCustomValidationCallback =
                    HttpClientHandler.DangerousAcceptAnyServerCertificateValidator,
            };
            return new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(15) };
        }
    }
}
