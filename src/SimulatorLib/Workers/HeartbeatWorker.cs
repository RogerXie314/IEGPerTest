using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Network;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class HeartbeatWorker
    {
        private readonly INetworkSender _tcpSender;
        private readonly IUdpSender? _udpSender;

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
                            var payload = Encoding.UTF8.GetBytes($"HEARTBEAT:{c.ClientId}:{c.IP}");
                            // 发送 TCP 到平台
                            var tcpOk = await _tcpSender.SendTcpAsync(platformHost, platformPort, payload, 2000, ct).ConfigureAwait(false);
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
                    await File.AppendAllTextAsync(logPath, line + Environment.NewLine).ConfigureAwait(false);
                }
                catch { }

                try
                {
                    await Task.Delay(intervalMs, ct).ConfigureAwait(false);
                }
                catch (TaskCanceledException) { break; }
            }
        }
    }
}
