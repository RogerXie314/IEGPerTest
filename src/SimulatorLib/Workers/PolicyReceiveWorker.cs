using System;
using System.Buffers.Binary;
using System.Linq;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Persistence;
using SimulatorLib.Protocol;

namespace SimulatorLib.Workers
{
    /// <summary>
    /// 策略下发接收与回包处理。
    ///
    /// 对齐老工具 WLServerTestDlg.cpp 策略流程：
    ///   TCP 心跳响应 cmdId==1  → 正常 ACK，忽略。
    ///   TCP 心跳响应 cmdId==17（HEARTBEAT_CMD_POLCY）→ 平台"有策略"通知，
    ///     客户端须立即发 HTTPS 心跳（/USM/clientHeartbeat.do），
    ///     从响应体中读取策略 JSON，再 POST /USM/clientResult.do 回报结果。
    ///   HTTPS 心跳路径（Linux/ThreatLog）：每次 HTTPS 心跳成功后，
    ///     HeartbeatWorker 调用 ProcessHttpsResponseBodyAsync 传入响应体，
    ///     使两条路径都能捕获策略下发。
    /// </summary>
    public class PolicyReceiveWorker
    {
        private static readonly Random _rng = new();

        // 共享 HttpClient（忽略证书：平台常用自签名证书）
        private static readonly HttpClient _http = CreateHttpClient();

        private readonly string _host;
        private readonly int    _port;

        // TCP 心跳响应 cmdId 常量（WLServerTest.h / WLCmdWordDef.h）
        private const uint CmdHeartbeat             = 1;   // HEARTBEAT_CMD_BACK
        private const uint CmdHeartbeatPolicyNotify = 17;  // HEARTBEAT_CMD_POLCY
        private const uint CmdHeartbeatNoRegister   = 18;  // HEARTBEAT_CMD_NOREGISTER（平台通知客户端未注册，需重新注册）

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
        /// <param name="onDataReceived">
        /// 每次 ReadAsync 成功读到数据时触发的回调（可为 null）。
        /// 调用方（HeartbeatWorker）用此更新 lastReplyTimeMs，以便准确统计平台回包数。
        /// </param>
        public async Task<bool> ProcessStreamAsync(
            NetworkStream stream,
            string clientId,
            uint deviceId,
            CancellationToken ct,
            Action? onDataReceived = null,
            Action? onNeedReregister = null)
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

                    // 每次收到数据立即通知调用方（用于更新回包时间戳，保证统计准确）
                    onDataReceived?.Invoke();

                    accumulated.Write(buf, 0, n);

                    // 尝试从 accumulated 中解析完整的 PT 包
                    await TryProcessPackets(accumulated, clientId, ct, onNeedReregister).ConfigureAwait(false);
                }
            }
            catch (OperationCanceledException) { }
            catch { return false; }

            return true;
        }

        // ── 公开辅助：供 HeartbeatWorker HTTPS 路径调用 ──────────────────────────

        /// <summary>
        /// 解析 HTTPS 心跳响应体中的策略命令并回包。
        /// 由 HeartbeatWorker.StartHttpsAsync 在每次 HTTPS 心跳成功后调用，
        /// 与 TCP cmdId==17 触发的 FetchPolicyViaHttpsAsync 路径对称。
        /// </summary>
        public Task ProcessHttpsResponseBodyAsync(string responseBody, string clientId, CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(responseBody)) return Task.CompletedTask;
            return ProcessPolicyJsonAsync(responseBody, clientId, ct);
        }

        // ── 私有实现 ──────────────────────────────────────────────────────────

        private Task TryProcessPackets(
            System.IO.MemoryStream buf,
            string clientId,
            CancellationToken ct,
            Action? onNeedReregister = null)
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
                    offset++;
                    continue;
                }

                // 读取 bodyLen (offset+4, uint16 big-endian)
                ushort bodyLen = BinaryPrimitives.ReadUInt16BigEndian(data.AsSpan(offset + 4, 2));
                int totalLen = PtProtocol.HeaderLength + bodyLen;

                if (data.Length - offset < totalLen) break; // 包不完整，等待更多数据

                // 读取 PT 头中的 cmdId（用于判断是否是心跳：心跳 cmdId=1）
                uint headerCmdId = BinaryPrimitives.ReadUInt32BigEndian(data.AsSpan(offset + 32, 4));

                if (headerCmdId == CmdHeartbeatPolicyNotify)
                {
                    // cmdId==17（HEARTBEAT_CMD_POLCY）：平台策略变更通知。
                    // TCP 包本身无策略内容；须发 HTTPS 心跳从响应体拿策略 JSON。
                    // 对应老工具：g_WLServerTestDlg->SendHeartbeat(*cclient) → ParseRevData。
                    _ = FetchPolicyViaHttpsAsync(clientId, ct);
                }
                else if (headerCmdId == CmdHeartbeatNoRegister)
                {
                    // cmdId==18（HEARTBEAT_CMD_NOREGISTER）：平台通知该客户端未注册。
                    // 对应老工具：CloseConnection → RegisterClientToServer → CreateConnection → SendHeartbeatToserver_TCP。
                    // 收到后立即通知 HeartbeatWorker 执行重注册+重连（不等下一个心跳周期）。
                    onNeedReregister?.Invoke();
                }
                // cmdId==1（HEARTBEAT_CMD_BACK）及其他值均忽略

                offset += totalLen;
            }

            // 重置 MemoryStream，仅保留未处理的尾部数据
            var remaining = data.AsSpan(offset).ToArray();
            buf.SetLength(0);
            if (remaining.Length > 0) buf.Write(remaining, 0, remaining.Length);
            return Task.CompletedTask;
        }

        /// <summary>
        /// 当 TCP 收到 cmdId==17（HEARTBEAT_CMD_POLCY）时，主动发 HTTPS 心跳拉取策略。
        /// 对应老工具：SendHeartbeat(*cclient) → pdoHeartBeat → ParseRevData → SendExecResult。
        /// </summary>
        private async Task FetchPolicyViaHttpsAsync(string clientId, CancellationToken ct)
        {
            try
            {
                var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                var record  = clients.FirstOrDefault(c => c.ClientId == clientId);
                var ip      = record?.IP ?? "127.0.0.1";
                var mac     = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(ip);
                var json    = HeartbeatJsonBuilder.BuildV3R7C02(
                    clientId, Environment.UserDomainName ?? "", ip, mac);

                using var content = new StringContent(json, Encoding.UTF8, "application/json");
                using var cts     = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(8000);
                using var resp = await _http.PostAsync(
                    $"https://{_host}:{_port}/USM/clientHeartbeat.do", content, cts.Token)
                    .ConfigureAwait(false);

                if (resp.IsSuccessStatusCode)
                {
                    var body = await resp.Content.ReadAsStringAsync(cts.Token).ConfigureAwait(false);
                    await ProcessPolicyJsonAsync(body, clientId, ct).ConfigureAwait(false);
                }
            }
            catch { }
        }

        /// <summary>
        /// 解析策略 JSON 数组，对每条命令递增计数并发起回包。
        /// 格式：[{"ComputerID":"...","CMDTYPE":150,"CMDID":247,"CMDContent":{...}}]
        /// 对应老工具 ParseRevData + SendExecResult 流程。
        /// </summary>
        private async Task ProcessPolicyJsonAsync(string json, string clientId, CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(json)) return;
            try
            {
                var doc = System.Text.Json.JsonDocument.Parse(json);
                var arr = doc.RootElement;
                if (arr.ValueKind != System.Text.Json.JsonValueKind.Array) return;
                for (int i = 0; i < arr.GetArrayLength(); i++)
                {
                    var item = arr[i];
                    if (!item.TryGetProperty("CMDTYPE", out var ctEl) ||
                        !item.TryGetProperty("CMDID",   out var ciEl)) continue;
                    Interlocked.Increment(ref _receivedCount);
                    await HandlePolicyCommandAsync(clientId, ctEl.GetInt32(), ciEl.GetInt32(), ct)
                        .ConfigureAwait(false);
                }
            }
            catch { }
        }

        private async Task HandlePolicyCommandAsync(
            string clientId,
            int cmdType,
            int cmdId,
            CancellationToken ct)
        {
            try
            {
                // 模拟客户端处理策略的延迟（500ms ~ 2000ms）
                int delay;
                lock (_rng) { delay = _rng.Next(500, 2001); }
                await Task.Delay(delay, ct).ConfigureAwait(false);

                // 通过 HTTPS POST 向平台 /USM/clientResult.do 上报执行结果。
                // CMDTYPE 和 CMDID 直接使用平台下发包体 JSON 里的值，原样回报，
                // 与原始 C++ sendExecResult(CMDID, nDealResult) 保持一致。
                var resultJson = BuildPolicyResultJson(clientId, cmdType, cmdId, success: true);
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
        /// 构建策略执行结果 JSON，CMDTYPE/CMDID 直接使用从平台下发包解析出的值，
        /// 与原始 C++ Result_GetJsonByDealResult 的字段对齐：
        /// [{"ComputerID":"...","CMDTYPE":150,"CMDID":247,"CMDVER":1,
        ///   "CMDContent":{"RESULT":"SUC","REASON":""}}]
        /// </summary>
        private static string BuildPolicyResultJson(string clientId, int cmdType, int cmdId, bool success)
        {
            var root = new JsonArray
            {
                new JsonObject
                {
                    ["ComputerID"] = clientId,
                    ["CMDTYPE"]    = cmdType,
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
