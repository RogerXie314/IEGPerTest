using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Protocol;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class HeartbeatWorker
    {
        private readonly INetworkSender _tcpSender;
        private readonly IUdpSender? _udpSender;

        private readonly ConcurrentDictionary<string, TcpConnState> _tcpStates = new();
        private readonly ConcurrentDictionary<string, SemaphoreSlim> _tcpStateLocks = new();

        public HeartbeatWorker(INetworkSender tcpSender, IUdpSender? udpSender = null)
        {
            _tcpSender = tcpSender;
            _udpSender = udpSender;
        }

        /// <summary>
        /// 周期性给所有已注册客户端发送心跳。
        /// - 若 useLogServer 为 true，除 TCP 发送到平台外，会额外向日志服务器发送 UDP 心跳。
        /// - 支持并发控制（concurrency）与心跳间隔（intervalMs）。
        /// </summary>
        public record HeartbeatStats(int Total, int SuccessTcp, int FailTcp, int SuccessUdp, int FailUdp);

        /// <summary>
        /// 增强版：支持通过 `progress` 上报每轮心跳统计；并把每轮统计追加到 `logs/heartbeat-YYYYMMDD.log`。
        /// </summary>
        public async Task StartAsync(int intervalMs, bool useLogServer, string platformHost, int platformPort, string? logHost, int logPort, int concurrency, CancellationToken ct, IProgress<HeartbeatStats>? progress = null)
        {
            var sem = new SemaphoreSlim(concurrency);

            try
            {
                while (!ct.IsCancellationRequested)
                {
                    var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                    Console.WriteLine($"心跳任务：读取到 {clients.Count} 条客户端记录");

                    var sendTasks = new List<Task>();
                    var successTcp = 0;
                    var failTcp = 0;
                    var successUdp = 0;
                    var failUdp = 0;
                    foreach (var c in clients)
                    {
                        await sem.WaitAsync(ct).ConfigureAwait(false);
                        sendTasks.Add(Task.Run(async () =>
                        {
                            try
                            {
                                var domainName = GetDomainNameSafe();
                                var mac = HeartbeatJsonBuilder.GetDeterministicMacFromIpv4(c.IP);
                                var json = HeartbeatJsonBuilder.BuildV3R7C02(c.ClientId, domainName, c.IP, mac);
                                var jsonBytes = GetTcpHeartbeatJsonBytes(json);

                                // external: CProtocal.GetPortocal(json, len-1, cmd=SOCKET_CMD_HEARTBEAT=1)
                                var payload = PtProtocol.Pack(jsonBytes, cmdId: 1, deviceId: c.DeviceId);

                                var tcpPort = c.TcpPort > 0 ? c.TcpPort : platformPort;
                                var tcpOk = await SendHeartbeatTcpLikeExternalAsync(c.ClientId, platformHost, tcpPort, payload, ct).ConfigureAwait(false);
                                if (tcpOk)
                                {
                                    Interlocked.Increment(ref successTcp);
                                    Console.WriteLine($"心跳 TCP 成功 -> {c.ClientId}");
                                }
                                else
                                {
                                    Interlocked.Increment(ref failTcp);
                                    Console.WriteLine($"心跳 TCP 失败 -> {c.ClientId}");
                                }

                                if (useLogServer && _udpSender != null && !string.IsNullOrEmpty(logHost))
                                {
                                    var udpOk = await _udpSender.SendUdpAsync(logHost, logPort, payload, 1500, ct).ConfigureAwait(false);
                                    if (udpOk)
                                    {
                                        Interlocked.Increment(ref successUdp);
                                        Console.WriteLine($"心跳 UDP 成功 -> {c.ClientId}");
                                    }
                                    else
                                    {
                                        Interlocked.Increment(ref failUdp);
                                        Console.WriteLine($"心跳 UDP 失败 -> {c.ClientId}");
                                    }
                                }
                            }
                            catch (OperationCanceledException) { }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"发送心跳时异常: {ex.Message}");
                            }
                            finally
                            {
                                sem.Release();
                            }
                        }, ct));
                    }

                    await Task.WhenAll(sendTasks).ConfigureAwait(false);

                    var stats = new HeartbeatStats(clients.Count, successTcp, failTcp, successUdp, failUdp);
                    try
                    {
                        progress?.Report(stats);
                    }
                    catch { }

                    try
                    {
                        var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                        if (!Directory.Exists(logDir)) Directory.CreateDirectory(logDir);
                        var logPath = Path.Combine(logDir, $"heartbeat-{DateTime.UtcNow:yyyyMMdd}.log");
                        var line = $"{DateTime.UtcNow:o} Total={stats.Total} TcpOk={stats.SuccessTcp} TcpFail={stats.FailTcp} UdpOk={stats.SuccessUdp} UdpFail={stats.FailUdp}";
                        await File.AppendAllTextAsync(logPath, line + Environment.NewLine, ct).ConfigureAwait(false);
                    }
                    catch { }

                    try
                    {
                        await Task.Delay(intervalMs, ct).ConfigureAwait(false);
                    }
                    catch (TaskCanceledException) { break; }
                }
            }
            finally
            {
                await CloseAllTcpAsync().ConfigureAwait(false);
            }
        }

        private static byte[] GetTcpHeartbeatJsonBytes(string json)
        {
            var bytes = Encoding.UTF8.GetBytes(json);
            if (bytes.Length <= 0) return bytes;

            if (bytes.Length >= 2 && bytes[^2] == (byte)'\r' && bytes[^1] == (byte)'\n')
            {
                return bytes.AsSpan(0, bytes.Length - 2).ToArray();
            }

            if (bytes[^1] == (byte)'\n')
            {
                return bytes.AsSpan(0, bytes.Length - 1).ToArray();
            }

            return bytes;
        }

        private async Task<bool> SendHeartbeatTcpLikeExternalAsync(string clientId, string host, int port, byte[] payload, CancellationToken ct)
        {
            // external: SendData失败 -> CloseSocket -> CreateSocket -> 重发
            if (await SendHeartbeatTcpOnceAsync(clientId, host, port, payload, ct).ConfigureAwait(false))
            {
                return true;
            }

            await CloseTcpAsync(clientId).ConfigureAwait(false);
            return await SendHeartbeatTcpOnceAsync(clientId, host, port, payload, ct).ConfigureAwait(false);
        }

        private async Task<bool> SendHeartbeatTcpOnceAsync(string clientId, string host, int port, byte[] payload, CancellationToken ct)
        {
            var gate = _tcpStateLocks.GetOrAdd(clientId, _ => new SemaphoreSlim(1, 1));
            await gate.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                var state = await GetOrCreateTcpStateAsync(clientId, host, port, ct).ConfigureAwait(false);
                if (state == null) return false;

                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(TimeSpan.FromMilliseconds(60000));

                await state.Stream.WriteAsync(payload, 0, payload.Length, cts.Token).ConfigureAwait(false);
                await state.Stream.FlushAsync(cts.Token).ConfigureAwait(false);
                return true;
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch
            {
                return false;
            }
            finally
            {
                gate.Release();
            }
        }

        private async Task<TcpConnState?> GetOrCreateTcpStateAsync(string clientId, string host, int port, CancellationToken ct)
        {
            if (_tcpStates.TryGetValue(clientId, out var existing))
            {
                if (existing.IsConnectedTo(host, port))
                {
                    return existing;
                }

                await CloseTcpAsync(clientId).ConfigureAwait(false);
            }

            try
            {
                var tcp = new TcpClient
                {
                    NoDelay = true,
                };

                using (var cts = CancellationTokenSource.CreateLinkedTokenSource(ct))
                {
                    cts.CancelAfter(TimeSpan.FromMilliseconds(10000));
                    await tcp.ConnectAsync(host, port, cts.Token).ConfigureAwait(false);
                }

                var stream = tcp.GetStream();
                var state = new TcpConnState(clientId, host, port, tcp, stream);

                state.StartReceiveLoop();
                _tcpStates[clientId] = state;
                return state;
            }
            catch
            {
                return null;
            }
        }

        private Task CloseTcpAsync(string clientId)
        {
            if (_tcpStates.TryRemove(clientId, out var state))
            {
                state.Close();
            }
            return Task.CompletedTask;
        }

        private Task CloseAllTcpAsync()
        {
            foreach (var key in _tcpStates.Keys)
            {
                _ = CloseTcpAsync(key);
            }
            _tcpStates.Clear();
            return Task.CompletedTask;
        }

        private sealed class TcpConnState
        {
            private readonly string _clientId;
            private readonly string _host;
            private readonly int _port;
            private readonly TcpClient _client;
            private readonly NetworkStream _stream;
            private readonly CancellationTokenSource _recvCts = new();
            private Task? _recvTask;

            public NetworkStream Stream => _stream;

            public TcpConnState(string clientId, string host, int port, TcpClient client, NetworkStream stream)
            {
                _clientId = clientId;
                _host = host;
                _port = port;
                _client = client;
                _stream = stream;
            }

            public bool IsConnectedTo(string host, int port)
            {
                if (!string.Equals(_host, host, StringComparison.OrdinalIgnoreCase)) return false;
                if (_port != port) return false;
                if (!_client.Connected) return false;
                return true;
            }

            public void StartReceiveLoop()
            {
                _recvTask = Task.Run(() => ReceiveLoopAsync(_recvCts.Token));
            }

            public void Close()
            {
                try { _recvCts.Cancel(); } catch { }
                try { _stream.Close(); } catch { }
                try { _client.Close(); } catch { }
                try { _recvCts.Dispose(); } catch { }
            }

            private async Task ReceiveLoopAsync(CancellationToken ct)
            {
                var header = new byte[PtProtocol.HeaderLength];
                try
                {
                    while (!ct.IsCancellationRequested)
                    {
                        bool okHeader = await ReadExactAsync(_stream, header, header.Length, ct).ConfigureAwait(false);
                        if (!okHeader) break;

                        // PT header: bodyLen at [4..6)
                        int bodyLen = (header[4] << 8) | header[5];
                        if (bodyLen <= 0 || bodyLen > 65535)
                        {
                            break;
                        }

                        var body = new byte[bodyLen];
                        bool okBody = await ReadExactAsync(_stream, body, bodyLen, ct).ConfigureAwait(false);
                        if (!okBody) break;

                        // external recvHeartBeatBack parses response; here we just drain the socket.
                        // Optional decode (best effort) to validate protocol alignment.
                        try
                        {
                            var packet = new byte[PtProtocol.HeaderLength + bodyLen];
                            Buffer.BlockCopy(header, 0, packet, 0, PtProtocol.HeaderLength);
                            Buffer.BlockCopy(body, 0, packet, PtProtocol.HeaderLength, bodyLen);
                            _ = PtProtocol.Unpack(packet);
                        }
                        catch { }
                    }
                }
                catch { }
            }

            private static async Task<bool> ReadExactAsync(NetworkStream stream, byte[] buffer, int len, CancellationToken ct)
            {
                int readTotal = 0;
                while (readTotal < len)
                {
                    int read = await stream.ReadAsync(buffer, readTotal, len - readTotal, ct).ConfigureAwait(false);
                    if (read <= 0) return false;
                    readTotal += read;
                }
                return true;
            }
        }

        private static string GetDomainNameSafe()
        {
            try
            {
                return Environment.UserDomainName ?? string.Empty;
            }
            catch
            {
                return string.Empty;
            }
        }
    }
}
