using System;
using System.ComponentModel;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using SimulatorLib.Network;
using SimulatorLib.Workers;
using SimulatorLib.Persistence;

namespace SimulatorApp.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        private string _platformHost = "localhost";
        private int _platformPort = 8441;
        private string _logHost = "localhost";
        private int _logPort = 4565;
        private bool _useLogServer = true;
        private string _regPrefix = "Client-";
        private int _regStart = 1;
        private int _regCount = 5;
        private int _hbInterval = 1000;

        private int _logClientCount = 5;
        private int _logMessagesPerClient = 50;
        private int _logMessagesPerSecondPerClient = 10;
        private int _logTotalMessages;
        private int _logSuccess;
        private int _logFailed;

        private bool _catClientOps = true;
        private bool _catVulnProtect;
        private bool _catProcessControl;
        private bool _catOs;
        private bool _catOutbound;
        private bool _catThreat = true;
        private bool _catNonWhitelist;
        private bool _catWhitelistTamper;
        private bool _catFileProtect;
        private bool _catMandatoryAccess;
        private bool _catProcessAudit;
        private bool _catVirusAlert;

        private string _whitelistFilePath = string.Empty;
        private int _whitelistClientCount = 5;
        private int _whitelistConcurrency = 4;
        private int _uploadTotal;
        private int _uploadSuccess;
        private int _uploadFailed;
        private string _statusLog = string.Empty;
        private int _hbTotal;
        private int _hbTcpOk;
        private int _hbTcpFail;
        private int _hbUdpOk;
        private int _hbUdpFail;

        private CancellationTokenSource? _hbCts;
        private CancellationTokenSource? _logCts;
        private CancellationTokenSource? _uploadCts;

        private readonly SynchronizationContext? _uiContext;

        public string PlatformHost { get => _platformHost; set { _platformHost = value; OnProp(); } }
        public int PlatformPort { get => _platformPort; set { _platformPort = value; OnProp(); } }
        public string LogHost { get => _logHost; set { _logHost = value; OnProp(); } }
        public int LogPort { get => _logPort; set { _logPort = value; OnProp(); } }
        public bool UseLogServer { get => _useLogServer; set { _useLogServer = value; OnProp(); } }
        public string RegPrefix { get => _regPrefix; set { _regPrefix = value; OnProp(); } }
        public int RegStart { get => _regStart; set { _regStart = value; OnProp(); } }
        public int RegCount { get => _regCount; set { _regCount = value; OnProp(); } }
        public int HbInterval { get => _hbInterval; set { _hbInterval = value; OnProp(); } }

        public int LogClientCount { get => _logClientCount; set { _logClientCount = value; OnProp(); } }
        public int LogMessagesPerClient { get => _logMessagesPerClient; set { _logMessagesPerClient = value; OnProp(); } }
        public int LogMessagesPerSecondPerClient { get => _logMessagesPerSecondPerClient; set { _logMessagesPerSecondPerClient = value; OnProp(); } }
        public int LogTotalMessages { get => _logTotalMessages; set { _logTotalMessages = value; OnProp(); } }
        public int LogSuccess { get => _logSuccess; set { _logSuccess = value; OnProp(); } }
        public int LogFailed { get => _logFailed; set { _logFailed = value; OnProp(); } }

        public bool CatClientOps { get => _catClientOps; set { _catClientOps = value; OnProp(); } }
        public bool CatVulnProtect { get => _catVulnProtect; set { _catVulnProtect = value; OnProp(); } }
        public bool CatProcessControl { get => _catProcessControl; set { _catProcessControl = value; OnProp(); } }
        public bool CatOs { get => _catOs; set { _catOs = value; OnProp(); } }
        public bool CatOutbound { get => _catOutbound; set { _catOutbound = value; OnProp(); } }
        public bool CatThreat { get => _catThreat; set { _catThreat = value; OnProp(); } }
        public bool CatNonWhitelist { get => _catNonWhitelist; set { _catNonWhitelist = value; OnProp(); } }
        public bool CatWhitelistTamper { get => _catWhitelistTamper; set { _catWhitelistTamper = value; OnProp(); } }
        public bool CatFileProtect { get => _catFileProtect; set { _catFileProtect = value; OnProp(); } }
        public bool CatMandatoryAccess { get => _catMandatoryAccess; set { _catMandatoryAccess = value; OnProp(); } }
        public bool CatProcessAudit { get => _catProcessAudit; set { _catProcessAudit = value; OnProp(); } }
        public bool CatVirusAlert { get => _catVirusAlert; set { _catVirusAlert = value; OnProp(); } }

        public string WhitelistFilePath { get => _whitelistFilePath; set { _whitelistFilePath = value; OnProp(); } }
        public int WhitelistClientCount { get => _whitelistClientCount; set { _whitelistClientCount = value; OnProp(); } }
        public int WhitelistConcurrency { get => _whitelistConcurrency; set { _whitelistConcurrency = value; OnProp(); } }
        public int UploadTotal { get => _uploadTotal; set { _uploadTotal = value; OnProp(); } }
        public int UploadSuccess { get => _uploadSuccess; set { _uploadSuccess = value; OnProp(); } }
        public int UploadFailed { get => _uploadFailed; set { _uploadFailed = value; OnProp(); } }

        public string StatusLog { get => _statusLog; set { _statusLog = value; OnProp(); } }

        public int HbTotal { get => _hbTotal; set { _hbTotal = value; OnProp(); } }
        public int HbTcpOk { get => _hbTcpOk; set { _hbTcpOk = value; OnProp(); } }
        public int HbTcpFail { get => _hbTcpFail; set { _hbTcpFail = value; OnProp(); } }
        public int HbUdpOk { get => _hbUdpOk; set { _hbUdpOk = value; OnProp(); } }
        public int HbUdpFail { get => _hbUdpFail; set { _hbUdpFail = value; OnProp(); } }

        public ICommand RegisterCommand { get; }
        public ICommand StartHeartbeatCommand { get; }
        public ICommand StopHeartbeatCommand { get; }
        public ICommand PortTestCommand { get; }
        public ICommand StartLogSendCommand { get; }
        public ICommand StopLogSendCommand { get; }
        public ICommand BrowseWhitelistFileCommand { get; }
        public ICommand StartWhitelistUploadCommand { get; }
        public ICommand StopWhitelistUploadCommand { get; }

        public MainViewModel()
        {
            _uiContext = SynchronizationContext.Current;
            RegisterCommand = new RelayCommand(async _ => await RegisterAsync());
            StartHeartbeatCommand = new RelayCommand(async _ => await StartHeartbeatAsync());
            StopHeartbeatCommand = new RelayCommand(_ => StopHeartbeat());
            PortTestCommand = new RelayCommand(async _ => await PortTestAsync());
            StartLogSendCommand = new RelayCommand(async _ => await StartLogSendAsync());
            StopLogSendCommand = new RelayCommand(_ => StopLogSend());
            BrowseWhitelistFileCommand = new RelayCommand(_ => BrowseWhitelistFile());
            StartWhitelistUploadCommand = new RelayCommand(async _ => await StartWhitelistUploadAsync());
            StopWhitelistUploadCommand = new RelayCommand(_ => StopWhitelistUpload());
            _ = LoadConfigAsync();
        }

        private void OnProp([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        private async Task LoadConfigAsync()
        {
            var cfg = await AppConfig.LoadAsync().ConfigureAwait(false);
            RunOnUi(() =>
            {
                PlatformHost = cfg.PlatformHost;
                PlatformPort = cfg.PlatformPort;
                LogHost = cfg.LogHost;
                LogPort = cfg.LogPort;
                UseLogServer = cfg.UseLogServer;
                RegPrefix = cfg.RegClientPrefix;
                RegStart = cfg.RegStartIndex;
                RegCount = cfg.RegCount;
                HbInterval = cfg.HeartbeatIntervalMs;

                LogClientCount = cfg.LogClientCount;
                LogMessagesPerClient = cfg.LogMessagesPerClient;
                LogMessagesPerSecondPerClient = cfg.LogMessagesPerSecondPerClient;

                WhitelistFilePath = cfg.WhitelistFilePath;
                WhitelistClientCount = cfg.WhitelistClientCount;
                WhitelistConcurrency = cfg.WhitelistConcurrency;

                AppendStatus("配置已加载");
            });
        }

        private async Task SaveConfigAsync()
        {
            var cfg = new AppConfig
            {
                PlatformHost = PlatformHost,
                PlatformPort = PlatformPort,
                LogHost = LogHost,
                LogPort = LogPort,
                UseLogServer = UseLogServer,
                RegClientPrefix = RegPrefix,
                RegStartIndex = RegStart,
                RegCount = RegCount,
                HeartbeatIntervalMs = HbInterval,
                LogClientCount = LogClientCount,
                LogMessagesPerClient = LogMessagesPerClient,
                LogMessagesPerSecondPerClient = LogMessagesPerSecondPerClient,
                WhitelistFilePath = WhitelistFilePath,
                WhitelistClientCount = WhitelistClientCount,
                WhitelistConcurrency = WhitelistConcurrency
            };
            await AppConfig.SaveAsync(cfg).ConfigureAwait(false);
            RunOnUi(() => AppendStatus("配置已保存"));
        }

        private void AppendStatus(string text)
        {
            StatusLog += $"[{DateTime.Now:HH:mm:ss}] {text}\r\n";
        }

        private void RunOnUi(Action action)
        {
            if (_uiContext == null)
            {
                action();
                return;
            }

            _uiContext.Post(_ =>
            {
                try { action(); }
                catch { }
            }, null);
        }

        private async Task RegisterAsync()
        {
            try
            {
                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() =>
                    {
                        AppendStatus("输入校验失败: " + reason);
                    });
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                var sender = new TcpSender();
                var reg = new RegistrationWorker(sender);
                RunOnUi(() => AppendStatus($"开始注册 {RegCount} 个客户端..."));
                await reg.RegisterAsync(RegPrefix, RegStart, RegCount, host: PlatformHost, port: PlatformPort, concurrency: 4, retry: 3, timeoutMs: 1500).ConfigureAwait(false);
                RunOnUi(() => AppendStatus("注册任务完成"));
            }
            catch (Exception ex)
            {
                RunOnUi(() => AppendStatus("注册异常: " + ex.Message));
            }
        }

        private async Task StartHeartbeatAsync()
        {
            try
            {
                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() => AppendStatus("输入校验失败: " + reason));
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                _hbCts = new CancellationTokenSource();
                var tcp = new TcpSender();
                var udp = new UdpSender();
                var hb = new HeartbeatWorker(tcp, udp);
                RunOnUi(() => AppendStatus("开始心跳任务..."));
                _ = Task.Run(async () =>
                {
                    var progress = new System.Progress<SimulatorLib.Workers.HeartbeatWorker.HeartbeatStats>(s =>
                    {
                        RunOnUi(() =>
                        {
                            HbTotal = s.Total;
                            HbTcpOk = s.SuccessTcp;
                            HbTcpFail = s.FailTcp;
                            HbUdpOk = s.SuccessUdp;
                            HbUdpFail = s.FailUdp;
                            AppendStatus($"心跳汇总: Total={s.Total} TCP_OK={s.SuccessTcp} TCP_FAIL={s.FailTcp} UDP_OK={s.SuccessUdp} UDP_FAIL={s.FailUdp}");
                        });
                    });
                    await hb.StartAsync(HbInterval, useLogServer: UseLogServer, platformHost: PlatformHost, platformPort: PlatformPort, logHost: LogHost, logPort: LogPort, concurrency: 6, ct: _hbCts.Token, progress: progress).ConfigureAwait(false);
                });
            }
            catch (Exception ex)
            {
                RunOnUi(() => AppendStatus("心跳启动异常: " + ex.Message));
            }
        }

        private void StopHeartbeat()
        {
            if (_hbCts != null && !_hbCts.IsCancellationRequested)
            {
                _hbCts.Cancel();
                RunOnUi(() => AppendStatus("已请求停止心跳任务"));
            }
        }

        private async Task PortTestAsync()
        {
            RunOnUi(() => AppendStatus("开始端口连通测试..."));
            bool tcpOk = await TestTcpAsync(PlatformHost, PlatformPort, 1500);
            RunOnUi(() => AppendStatus($"TCP {PlatformHost}:{PlatformPort} -> {(tcpOk ? "OK" : "FAIL")}"));

            bool logTcpOk = await TestTcpAsync(LogHost, LogPort, 1500);
            RunOnUi(() => AppendStatus($"TCP {LogHost}:{LogPort} -> {(logTcpOk ? "OK" : "FAIL")}"));

            bool udpOk = await TestUdpAsync(LogHost, LogPort, 1000);
            RunOnUi(() => AppendStatus($"UDP {LogHost}:{LogPort} -> {(udpOk ? "OK" : "WARN/NO-ACK")}"));

            bool httpOk = await TestHttpAsync(PlatformHost, PlatformPort, 2000);
            RunOnUi(() => AppendStatus($"HTTP http://{PlatformHost}:{PlatformPort}/ -> {(httpOk ? "OK" : "FAIL")}"));
        }

        private (bool ok, string reason) ValidateInputs()
        {
            if (string.IsNullOrWhiteSpace(PlatformHost)) return (false, "PlatformHost 为空");
            if (UseLogServer && string.IsNullOrWhiteSpace(LogHost)) return (false, "LogHost 为空");
            if (PlatformPort <= 0 || PlatformPort > 65535) return (false, "PlatformPort 不在有效范围");
            if (UseLogServer && (LogPort <= 0 || LogPort > 65535)) return (false, "LogPort 不在有效范围");
            if (RegCount <= 0) return (false, "RegCount 必须大于 0");
            if (LogClientCount <= 0) return (false, "LogClientCount 必须大于 0");
            if (LogMessagesPerClient <= 0) return (false, "LogMessagesPerClient 必须大于 0");
            if (LogMessagesPerSecondPerClient < 0) return (false, "LogMessagesPerSecondPerClient 不能为负数");
            if (WhitelistClientCount <= 0) return (false, "WhitelistClientCount 必须大于 0");
            if (WhitelistConcurrency <= 0) return (false, "WhitelistConcurrency 必须大于 0");
            return (true, string.Empty);
        }

        private string[] GetSelectedCategories()
        {
            var list = new System.Collections.Generic.List<string>();
            if (CatClientOps) list.Add("客户端操作");
            if (CatVulnProtect) list.Add("漏洞防护");
            if (CatProcessControl) list.Add("进程控制");
            if (CatOs) list.Add("操作系统");
            if (CatOutbound) list.Add("非法外联");
            if (CatThreat) list.Add("威胁检测");
            if (CatNonWhitelist) list.Add("非白名单");
            if (CatWhitelistTamper) list.Add("白名单防篡改");
            if (CatFileProtect) list.Add("文件保护");
            if (CatMandatoryAccess) list.Add("强制访问控制");
            if (CatProcessAudit) list.Add("进程审计");
            if (CatVirusAlert) list.Add("病毒告警");
            return list.Count == 0 ? new[] { "Default" } : list.ToArray();
        }

        private async Task StartLogSendAsync()
        {
            try
            {
                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() => AppendStatus("输入校验失败: " + reason));
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                _logCts = new CancellationTokenSource();

                var tcp = new TcpSender();
                var udp = new UdpSender();
                var worker = new LogWorker(tcp, udp);
                var cats = GetSelectedCategories();

                RunOnUi(() =>
                {
                    LogTotalMessages = 0;
                    LogSuccess = 0;
                    LogFailed = 0;
                    AppendStatus($"开始日志发送：客户端数={LogClientCount} 每客户端总条数={LogMessagesPerClient} 每秒条数={LogMessagesPerSecondPerClient} 分类={cats.Length}");
                });

                _ = Task.Run(async () =>
                {
                    var progress = new Progress<SimulatorLib.Workers.LogSendStats>(s =>
                    {
                        RunOnUi(() =>
                        {
                            LogTotalMessages = s.TotalMessages;
                            LogSuccess = s.Success;
                            LogFailed = s.Failed;
                            AppendStatus($"日志汇总: Total={s.TotalMessages} OK={s.Success} FAIL={s.Failed}");
                        });
                    });

                    await worker.StartAsync(
                        messagesPerClient: LogMessagesPerClient,
                        messagesPerSecondPerClient: LogMessagesPerSecondPerClient <= 0 ? null : LogMessagesPerSecondPerClient,
                        maxClients: LogClientCount,
                        categories: cats,
                        useLogServer: UseLogServer,
                        platformHost: PlatformHost,
                        platformPort: PlatformPort,
                        logHost: LogHost,
                        logPort: LogPort,
                        concurrency: 6,
                        ct: _logCts.Token,
                        progress: progress).ConfigureAwait(false);
                });
            }
            catch (Exception ex)
            {
                RunOnUi(() => AppendStatus("日志发送启动异常: " + ex.Message));
            }
        }

        private void StopLogSend()
        {
            if (_logCts != null && !_logCts.IsCancellationRequested)
            {
                _logCts.Cancel();
                RunOnUi(() => AppendStatus("已请求停止日志发送任务"));
            }
        }

        private void BrowseWhitelistFile()
        {
            try
            {
                var dlg = new Microsoft.Win32.OpenFileDialog
                {
                    Title = "选择白名单文件",
                    Filter = "All Files (*.*)|*.*"
                };
                var res = dlg.ShowDialog();
                if (res == true)
                {
                    WhitelistFilePath = dlg.FileName;
                    _ = SaveConfigAsync();
                    AppendStatus("已选择白名单文件: " + System.IO.Path.GetFileName(WhitelistFilePath));
                }
            }
            catch (Exception ex)
            {
                AppendStatus("选择文件异常: " + ex.Message);
            }
        }

        private async Task StartWhitelistUploadAsync()
        {
            try
            {
                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() => AppendStatus("输入校验失败: " + reason));
                    return;
                }

                if (string.IsNullOrWhiteSpace(WhitelistFilePath) || !System.IO.File.Exists(WhitelistFilePath))
                {
                    RunOnUi(() => AppendStatus("白名单文件不存在，请先选择文件"));
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                _uploadCts = new CancellationTokenSource();
                var tcp = new TcpSender();
                var worker = new WhitelistUploadWorker(tcp);

                RunOnUi(() =>
                {
                    UploadTotal = 0;
                    UploadSuccess = 0;
                    UploadFailed = 0;
                    AppendStatus($"开始白名单上传：文件={System.IO.Path.GetFileName(WhitelistFilePath)} 客户端数={WhitelistClientCount} 并发={WhitelistConcurrency}");
                });

                _ = Task.Run(async () =>
                {
                    var progress = new Progress<SimulatorLib.Workers.UploadStats>(s =>
                    {
                        RunOnUi(() =>
                        {
                            UploadTotal = s.Total;
                            UploadSuccess = s.Success;
                            UploadFailed = s.Failed;
                            AppendStatus($"上传汇总: Total={s.Total} OK={s.Success} FAIL={s.Failed}");
                        });
                    });

                    await worker.StartAsync(
                        filePath: WhitelistFilePath,
                        clientCount: WhitelistClientCount,
                        platformHost: PlatformHost,
                        platformPort: PlatformPort,
                        concurrency: WhitelistConcurrency,
                        ct: _uploadCts.Token,
                        progress: progress).ConfigureAwait(false);
                });
            }
            catch (Exception ex)
            {
                RunOnUi(() => AppendStatus("白名单上传启动异常: " + ex.Message));
            }
        }

        private void StopWhitelistUpload()
        {
            if (_uploadCts != null && !_uploadCts.IsCancellationRequested)
            {
                _uploadCts.Cancel();
                RunOnUi(() => AppendStatus("已请求停止白名单上传任务"));
            }
        }

        

        private async Task<bool> TestTcpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var tcp = new System.Net.Sockets.TcpClient();
                var t = tcp.ConnectAsync(host, port);
                var task = await Task.WhenAny(t, Task.Delay(timeoutMs)).ConfigureAwait(false);
                if (task != t) return false;
                return tcp.Connected;
            }
            catch { return false; }
        }

        private async Task<bool> TestUdpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var udp = new System.Net.Sockets.UdpClient();
                var addrs = System.Net.Dns.GetHostAddresses(host);
                if (addrs == null || addrs.Length == 0) return false;
                var endpoint = new System.Net.IPEndPoint(addrs[0], port);
                var data = Encoding.UTF8.GetBytes("PING");
                await udp.SendAsync(data, data.Length, endpoint).ConfigureAwait(false);
                return true;
            }
            catch { return false; }
        }

        private async Task<bool> TestHttpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var c = new System.Net.Http.HttpClient { Timeout = TimeSpan.FromMilliseconds(timeoutMs) };
                var url = $"http://{host}:{port}/";
                var resp = await c.GetAsync(url).ConfigureAwait(false);
                return resp.IsSuccessStatusCode;
            }
            catch { return false; }
        }
    }
}
