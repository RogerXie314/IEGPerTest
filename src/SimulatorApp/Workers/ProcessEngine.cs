using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Models;
using SimulatorLib.Persistence;

namespace SimulatorApp.Workers
{
    /// <summary>
    /// 进程引擎：通过独立C++进程运行压测，完全隔离C#和C++，避免GC、托管互操作问题
    /// </summary>
    public class ProcessEngine : IDisposable
    {
        private Process? _process;
        private CancellationTokenSource? _outputCts;
        private readonly string _exePath;
        private readonly string _configPath;
        
        // 统计数据
        public int RegisteredTotal { get; private set; }
        public int HBSending { get; private set; }
        public int HBNotSending { get; private set; }
        public int HBServerAck { get; private set; }  // 服务器确认上线（非18回包）
        public int LogNotSending { get; private set; }
        public long LogSendOk { get; private set; }
        public long LogSendFail { get; private set; }
        public int NoReg { get; private set; }  // 收到 NOREGISTER(18) 的累计次数
        
        // 事件：状态更新
        public event Action? OnStatsUpdated;
        public event Action<string>? OnInfoMessage;
        public event Action<string>? OnErrorMessage;
        public event Action<int, string>? OnReregisterRequest;  // (clientIdx, clientId) — NativeRunner 收到 NOREGISTER(18) 时触发
        public event Action<int, string>? OnPolicyRequest;     // (clientIdx, clientId) — NativeRunner 收到 cmdId=17 时触发
        
        public ProcessEngine(string exePath = "NativeRunner.exe", string configPath = "config.ini")
        {
            _exePath = exePath;
            _configPath = configPath;
        }
        
        /// <summary>
        /// 将 C# 重注册后获得的新 DeviceId 发送给 NativeRunner 进程
        /// </summary>
        public void SendDeviceIdUpdate(int idx, uint deviceId)
        {
            try
            {
                if (_process != null && !_process.HasExited)
                    _process.StandardInput.WriteLine($"DEVICEID|{idx}|{deviceId}");
            }
            catch { }
        }

        /// <summary>
        /// 通知 NativeRunner 开始发送威胁日志（用户点击"添加任务"时调用）
        /// </summary>
        public void SendStartLog(int clientCount, int eps, int totalItems, int types, int hitEvery)
        {
            try
            {
                if (_process != null && !_process.HasExited)
                    _process.StandardInput.WriteLine($"STARTLOG|{clientCount}|{eps}|{totalItems}|{types}|{hitEvery}");
            }
            catch { }
        }

        /// <summary>
        /// 通知 NativeRunner 停止发送威胁日志（用户点击"结束任务"时调用）
        /// </summary>
        public void SendStopLog()
        {
            try
            {
                if (_process != null && !_process.HasExited)
                    _process.StandardInput.WriteLine("STOPLOG");
            }
            catch { }
        }

        /// <summary>
        /// 初始化并启动C++进程
        /// </summary>
        public bool Start(
            string serverIP,
            int serverPort,
            int serverHBPort,
            string clientIDPrefix,
            string clientStartIP,
            int clientStartNum,
            int clientCount,
            int hbInterval,
            int hbTotalMinutes,
            int logClientCount,
            int logEachClientTotalItems,
            int logEachClientPerSecondItems,
            int logSelectedTypes,
            int logHitEvery = 71)
        {
            try
            {
                // 1. 写配置文件
                WriteConfigFile(
                    serverIP, serverPort, serverHBPort,
                    clientIDPrefix, clientStartIP, clientStartNum, clientCount,
                    hbInterval, hbTotalMinutes,
                    logClientCount, logEachClientTotalItems, logEachClientPerSecondItems,
                    logSelectedTypes, logHitEvery);
                
                // 2. 启动C++进程
                _process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = _exePath,
                        Arguments = $"--console --config {_configPath}",
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        RedirectStandardInput = true,
                        CreateNoWindow = true,
                        StandardOutputEncoding = Encoding.UTF8,
                        StandardErrorEncoding = Encoding.UTF8,
                        WorkingDirectory = Path.GetDirectoryName(Path.GetFullPath(_exePath)) ?? Environment.CurrentDirectory
                    }
                };
                
                _process.OutputDataReceived += OnOutputDataReceived;
                _process.ErrorDataReceived += OnErrorDataReceived;
                
                _process.Start();
                _process.BeginOutputReadLine();
                _process.BeginErrorReadLine();
                
                return true;
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"子进程启动失败: {ex.Message}");
                return false;
            }
        }
        
        /// <summary>
        /// 停止C++进程
        /// </summary>
        public void Stop()
        {
            try
            {
                if (_process != null && !_process.HasExited)
                {
                    // 尝试优雅关闭（发送Ctrl+C）
                    _process.StandardInput.WriteLine("quit");
                    
                    // 等待3秒
                    if (!_process.WaitForExit(3000))
                    {
                        // 强制关闭
                        _process.Kill();
                        OnInfoMessage?.Invoke("子进程已强制终止");
                    }
                    else
                    {
                        OnInfoMessage?.Invoke("子进程已正常退出");
                    }
                }
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"停止子进程失败: {ex.Message}");
            }
        }
        
        /// <summary>
        /// 写配置文件（INI格式）
        /// </summary>
        private void WriteConfigFile(
            string serverIP,
            int serverPort,
            int serverHBPort,
            string clientIDPrefix,
            string clientStartIP,
            int clientStartNum,
            int clientCount,
            int hbInterval,
            int hbTotalMinutes,
            int logClientCount,
            int logEachClientTotalItems,
            int logEachClientPerSecondItems,
            int logSelectedTypes,
            int logHitEvery = 71)
        {
            var sb = new StringBuilder();
            sb.AppendLine("[Server]");
            sb.AppendLine($"IP={serverIP}");
            sb.AppendLine($"Port={serverPort}");
            sb.AppendLine($"HBPort={serverHBPort}");
            sb.AppendLine();
            
            sb.AppendLine("[Client]");
            sb.AppendLine($"IDPrefix={clientIDPrefix}");
            sb.AppendLine($"StartIP={clientStartIP}");
            sb.AppendLine($"StartNum={clientStartNum}");
            sb.AppendLine($"Count={clientCount}");
            sb.AppendLine("SkipRegistration=true");
            sb.AppendLine($"ClientsLogPath={ClientsPersistence.GetPath()}"); // 使用C#注册生成的真实数据
            sb.AppendLine();
            
            sb.AppendLine("[Heartbeat]");
            sb.AppendLine($"Interval={hbInterval}");
            sb.AppendLine($"TotalMinutes={hbTotalMinutes}");
            sb.AppendLine();
            
            sb.AppendLine("[Log]");
            sb.AppendLine($"ClientCount={logClientCount}");
            sb.AppendLine($"EachClientTotalItems={logEachClientTotalItems}");
            sb.AppendLine($"EachClientPerSecondItems={logEachClientPerSecondItems}");
            sb.AppendLine($"SelectedTypes={logSelectedTypes}");
            sb.AppendLine($"HitEvery={logHitEvery}");
            sb.AppendLine();
            
            // 使用无 BOM 的 UTF-8 写入：Encoding.UTF8 会写入 3 字节 BOM (EF BB BF)，
            // 导致 NativeRunner 的手写 INI 解析器把第一行读成 "\xEF\xBB\xBF[Server]"，
            // 无法匹配 "[Server]"，platformHost 回落为默认 127.0.0.1，连接失败。
            // Windows GetPrivateProfileString (WLServerTest.exe 用的) 会自动跳过 BOM，
            // 但 NativeRunner 的 GetINI 不跳过，所以必须写无 BOM 格式。
            File.WriteAllText(_configPath, sb.ToString(), new System.Text.UTF8Encoding(false));
            // 配置写入无需向GUI上报
        }
        
        private static readonly HashSet<string> _keyMilestones = new(StringComparer.OrdinalIgnoreCase)
        {
            "Heartbeat threads started", "Console mode is running", "Stopping all threads",
            "Registration completed"
        };

        /// <summary>
        /// 处理stdout输出
        /// </summary>
        private int _statsCount = 0;

        private void OnOutputDataReceived(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e.Data)) return;
            
            try
            {
                if (e.Data.StartsWith("REREGISTER|"))
                {
                    var parts = e.Data.Split('|');
                    if (parts.Length >= 3 && int.TryParse(parts[1], out int reregIdx))
                        OnReregisterRequest?.Invoke(reregIdx, parts[2]);
                }
                else if (e.Data.StartsWith("POLICY|"))
                {
                    var parts = e.Data.Split('|');
                    if (parts.Length >= 3 && int.TryParse(parts[1], out int policyIdx))
                        OnPolicyRequest?.Invoke(policyIdx, parts[2]);
                }
                else if (e.Data.StartsWith("STATS|"))
                {
                    ParseStats(e.Data);
                    _statsCount++;
                    // TCP全部失败时告警
                    if (_statsCount == 3 && HBSending == 0 && RegisteredTotal > 0)
                        OnErrorMessage?.Invoke($"TCP连接数为0，检查防火墙/安全软件是否拦截了NativeRunner.exe（目标: {RegisteredTotal}个客户端）");
                    OnStatsUpdated?.Invoke();
                }
                else if (e.Data.StartsWith("ERROR|"))
                {
                    OnErrorMessage?.Invoke(e.Data.Substring(6));
                }
                else if (e.Data.StartsWith("INFO|") || e.Data.StartsWith("WARN|"))
                {
                    string raw = e.Data.Substring(5);
                    string? translated = TranslateNativeMessage(raw);
                    if (translated != null)
                        OnInfoMessage?.Invoke(translated);
                }
                // 其他行（无前缀）静默丢弃
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"解析子进程输出失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 将 NativeRunner 的英文日志翻译为中文，返回 null 表示丢弃该行
        /// </summary>
        private static string? TranslateNativeMessage(string raw)
        {
            // 丢弃：逐客户端详情（前5条，太冗余）
            if (raw.StartsWith("Client[") && raw.Contains("ClientId=")) return null;
            // 丢弃：startup config 诊断行（C# 侧已在启动时记录配置）
            if (raw.StartsWith("startup config:")) return null;
            // 丢弃：DeviceId 更新细节（重注册操作已在 C# 侧记录）
            if (raw.StartsWith("DeviceId updated:")) return null;

            // 翻译：关键里程碑
            if (raw.StartsWith("NativeRunner starting")) return null; // C# 侧已输出"开始心跳任务"
            if (raw.StartsWith("Loaded ") && raw.Contains("clients from Clients.log"))
            {
                var m = System.Text.RegularExpressions.Regex.Match(raw, @"Loaded (\d+) clients");
                return m.Success ? $"已加载 {m.Groups[1].Value} 个客户端" : raw;
            }
            if (raw.StartsWith("Starting heartbeat threads"))
            {
                var m = System.Text.RegularExpressions.Regex.Match(raw, @"Starting heartbeat threads \((\d+) clients, interval=(\d+)ms\)");
                if (m.Success)
                {
                    int sec = int.Parse(m.Groups[2].Value) / 1000;
                    return $"正在启动 {m.Groups[1].Value} 个心跳线程（间隔 {sec}s）...";
                }
                return raw;
            }
            if (raw == "Heartbeat threads started") return "心跳线程已全部就绪";

            if (raw.StartsWith("Log threads started"))
            {
                var m = System.Text.RegularExpressions.Regex.Match(raw, @"Log threads started \((\d+) clients\)");
                return m.Success ? $"威胁日志线程已启动（{m.Groups[1].Value} 个客户端）" : raw;
            }
            if (raw == "Log threads stopping") return "威胁日志线程正在停止...";

            return raw;
        }
        
        /// <summary>
        /// 处理stderr输出
        /// </summary>
        private void OnErrorDataReceived(object sender, DataReceivedEventArgs e)
        {
            if (!string.IsNullOrEmpty(e.Data))
            {
                // stderr 中的 ERROR| 前缀同样剥离
                string msg = e.Data.StartsWith("ERROR|") ? e.Data.Substring(6) : e.Data;
                OnErrorMessage?.Invoke(msg);
            }
        }
        
        /// <summary>
        /// 解析统计数据
        /// 格式：STATS|RegisteredTotal=100|HBSending=50|HBNotSending=50|LogNotSending=100
        /// </summary>
        private void ParseStats(string line)
        {
            var parts = line.Split('|');
            foreach (var part in parts.Skip(1)) // 跳过"STATS"
            {
                var kv = part.Split('=');
                if (kv.Length == 2)
                {
                    var key = kv[0];
                    var value = kv[1];
                    
                    switch (key)
                    {
                        case "RegisteredTotal":
                            RegisteredTotal = int.Parse(value);
                            break;
                        case "HBSending":
                            HBSending = int.Parse(value);
                            break;
                        case "HBNotSending":
                            HBNotSending = int.Parse(value);
                            break;
                        case "HBServerAck":
                            HBServerAck = int.Parse(value);
                            break;
                        case "LogNotSending":
                            LogNotSending = int.Parse(value);
                            break;
                        case "LogSendOk":
                            LogSendOk = long.Parse(value);
                            break;
                        case "LogSendFail":
                            LogSendFail = long.Parse(value);
                            break;
                        case "NoReg":
                            NoReg = int.Parse(value);
                            break;
                    }
                }
            }
        }
        
        public void Dispose()
        {
            Stop();
            _process?.Dispose();
            _outputCts?.Dispose();
        }
    }
}
