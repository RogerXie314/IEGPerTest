using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using SimulatorLib.Models;

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
        public int LogNotSending { get; private set; }
        
        // 事件：状态更新
        public event Action? OnStatsUpdated;
        public event Action<string>? OnInfoMessage;
        public event Action<string>? OnErrorMessage;
        
        public ProcessEngine(string exePath = "WLServerTest.exe", string configPath = "config.ini")
        {
            _exePath = exePath;
            _configPath = configPath;
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
            int logSelectedTypes)
        {
            try
            {
                // 1. 写配置文件
                WriteConfigFile(
                    serverIP, serverPort, serverHBPort,
                    clientIDPrefix, clientStartIP, clientStartNum, clientCount,
                    hbInterval, hbTotalMinutes,
                    logClientCount, logEachClientTotalItems, logEachClientPerSecondItems,
                    logSelectedTypes);
                
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
                        StandardErrorEncoding = Encoding.UTF8
                    }
                };
                
                _process.OutputDataReceived += OnOutputDataReceived;
                _process.ErrorDataReceived += OnErrorDataReceived;
                
                _process.Start();
                _process.BeginOutputReadLine();
                _process.BeginErrorReadLine();
                
                OnInfoMessage?.Invoke($"C++ process started: PID={_process.Id}");
                return true;
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"Failed to start C++ process: {ex.Message}");
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
                        OnInfoMessage?.Invoke("C++ process killed (timeout)");
                    }
                    else
                    {
                        OnInfoMessage?.Invoke("C++ process stopped gracefully");
                    }
                }
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"Failed to stop C++ process: {ex.Message}");
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
            int logSelectedTypes)
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
            sb.AppendLine("SkipRegistration=true");  // 使用C#已注册的客户端
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
            sb.AppendLine();
            
            File.WriteAllText(_configPath, sb.ToString(), Encoding.UTF8);
            OnInfoMessage?.Invoke($"Config file written: {_configPath}");
        }
        
        /// <summary>
        /// 处理stdout输出
        /// </summary>
        private void OnOutputDataReceived(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e.Data)) return;
            
            try
            {
                // 解析输出格式：STATS|key=value|key=value...
                if (e.Data.StartsWith("STATS|"))
                {
                    ParseStats(e.Data);
                    OnStatsUpdated?.Invoke();
                }
                else if (e.Data.StartsWith("INFO|"))
                {
                    OnInfoMessage?.Invoke(e.Data.Substring(5));
                }
                else if (e.Data.StartsWith("ERROR|"))
                {
                    OnErrorMessage?.Invoke(e.Data.Substring(6));
                }
                else
                {
                    // 其他输出
                    OnInfoMessage?.Invoke(e.Data);
                }
            }
            catch (Exception ex)
            {
                OnErrorMessage?.Invoke($"Failed to parse output: {ex.Message}");
            }
        }
        
        /// <summary>
        /// 处理stderr输出
        /// </summary>
        private void OnErrorDataReceived(object sender, DataReceivedEventArgs e)
        {
            if (!string.IsNullOrEmpty(e.Data))
            {
                OnErrorMessage?.Invoke(e.Data);
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
                        case "LogNotSending":
                            LogNotSending = int.Parse(value);
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
