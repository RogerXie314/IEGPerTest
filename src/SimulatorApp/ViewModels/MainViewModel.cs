using System;
using System.ComponentModel;
using System.IO;
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
        private string _regStartIp = string.Empty;
        private int _regCount = 5;
        private int _hbInterval = 30000;
        private string _projectType = "IEG";

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
        private bool _catRegProtect;
        private bool _catMandatoryAccess;
        private bool _catProcessAudit;
        private bool _catVirusAlert;
        private bool _catUsb;
        private bool _catUsbWarning;
        private bool _catUDiskPlug;
        private bool _catFirewall;
        private bool _catSysGuard;

        private int _regConcurrency = 20;
        private int _regRetryIntervalSec = 30;
        private int _regTimeoutMs = 60000;
        private int _regRound;
        private int _regTotal;
        private int _regSuccess;
        private int _regFailed;
        private string _regFailureDetail = string.Empty;

        private string _whitelistFilePath = string.Empty;
        private int _whitelistClientCount = 5;
        private int _whitelistConcurrency = 4;
        private int _uploadTotal;
        private int _uploadSuccess;
        private int _uploadFailed;
        private string _statusLog = string.Empty;
        private int _hbTotal;
        private int _hbConnected;
        private int _hbTcpOk;
        private int _hbTcpFail;
        private int _hbServerReplied;
        private int _hbUdpOk;
        private int _hbUdpFail;
        private int _hbHttpsTotal;
        private int _hbHttpsOk;
        private int _hbHttpsFail;
        private int _hbHttpsUdpOk;
        private int _hbHttpsUdpFail;

        private CancellationTokenSource? _hbCts;
        private CancellationTokenSource? _httpsCts;
        private CancellationTokenSource? _logCts;
        private CancellationTokenSource? _uploadCts;
        private DateTime _lastHbLogTime = DateTime.MinValue; // 心跳状态日志节流

        private readonly SynchronizationContext? _uiContext;

        public string PlatformHost { get => _platformHost; set { _platformHost = value; OnProp(); } }
        public int PlatformPort { get => _platformPort; set { _platformPort = value; OnProp(); } }
        public string LogHost { get => _logHost; set { _logHost = value; OnProp(); } }
        public int LogPort { get => _logPort; set { _logPort = value; OnProp(); } }
        public bool UseLogServer { get => _useLogServer; set { _useLogServer = value; OnProp(); } }
        public string ProjectType 
        { 
            get => _projectType; 
            set 
            { 
                _projectType = value; 
                OnProp();
                // 项目类型改变时，自动勾选对应的日志分类
                ApplyProjectTypeSelection();
            } 
        }
        public string RegPrefix { get => _regPrefix; set { _regPrefix = value; OnProp(); } }
        public int RegStart { get => _regStart; set { _regStart = value; OnProp(); } }
        public string RegStartIp { get => _regStartIp; set { _regStartIp = value; OnProp(); } }
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
        public bool CatRegProtect { get => _catRegProtect; set { _catRegProtect = value; OnProp(); } }
        public bool CatMandatoryAccess { get => _catMandatoryAccess; set { _catMandatoryAccess = value; OnProp(); } }
        public bool CatProcessAudit { get => _catProcessAudit; set { _catProcessAudit = value; OnProp(); } }
        public bool CatVirusAlert { get => _catVirusAlert; set { _catVirusAlert = value; OnProp(); } }
        public bool CatUsb { get => _catUsb; set { _catUsb = value; OnProp(); } }
        public bool CatUsbWarning { get => _catUsbWarning; set { _catUsbWarning = value; OnProp(); } }
        public bool CatUDiskPlug { get => _catUDiskPlug; set { _catUDiskPlug = value; OnProp(); } }
        public bool CatFirewall { get => _catFirewall; set { _catFirewall = value; OnProp(); } }
        public bool CatSysGuard { get => _catSysGuard; set { _catSysGuard = value; OnProp(); } }

        public int RegConcurrency { get => _regConcurrency; set { _regConcurrency = value; OnProp(); } }
        public int RegRetryIntervalSec { get => _regRetryIntervalSec; set { _regRetryIntervalSec = value; OnProp(); } }
        public int RegTimeoutMs { get => _regTimeoutMs; set { _regTimeoutMs = value; OnProp(); } }
        public int RegRound { get => _regRound; set { _regRound = value; OnProp(); } }
        public int RegTotal { get => _regTotal; set { _regTotal = value; OnProp(); } }
        public int RegSuccess { get => _regSuccess; set { _regSuccess = value; OnProp(); } }
        public int RegFailed { get => _regFailed; set { _regFailed = value; OnProp(); } }
        public string RegFailureDetail { get => _regFailureDetail; set { _regFailureDetail = value; OnProp(); } }

        public string WhitelistFilePath { get => _whitelistFilePath; set { _whitelistFilePath = value; OnProp(); } }
        public int WhitelistClientCount { get => _whitelistClientCount; set { _whitelistClientCount = value; OnProp(); } }
        public int WhitelistConcurrency { get => _whitelistConcurrency; set { _whitelistConcurrency = value; OnProp(); } }
        public int UploadTotal { get => _uploadTotal; set { _uploadTotal = value; OnProp(); } }
        public int UploadSuccess { get => _uploadSuccess; set { _uploadSuccess = value; OnProp(); } }
        public int UploadFailed { get => _uploadFailed; set { _uploadFailed = value; OnProp(); } }

        public string StatusLog { get => _statusLog; set { _statusLog = value; OnProp(); } }

        public int HbTotal { get => _hbTotal; set { _hbTotal = value; OnProp(); } }
        public int HbConnected { get => _hbConnected; set { _hbConnected = value; OnProp(); } }
        public int HbTcpOk { get => _hbTcpOk; set { _hbTcpOk = value; OnProp(); } }
        public int HbTcpFail { get => _hbTcpFail; set { _hbTcpFail = value; OnProp(); } }
        /// <summary>服务端有回包的客户端数（最接近「平台真实在线」的指标）</summary>
        public int HbServerReplied { get => _hbServerReplied; set { _hbServerReplied = value; OnProp(); } }
        public int HbUdpOk { get => _hbUdpOk; set { _hbUdpOk = value; OnProp(); } }
        public int HbUdpFail { get => _hbUdpFail; set { _hbUdpFail = value; OnProp(); } }
        public int HbHttpsTotal { get => _hbHttpsTotal; set { _hbHttpsTotal = value; OnProp(); } }
        public int HbHttpsOk { get => _hbHttpsOk; set { _hbHttpsOk = value; OnProp(); } }
        public int HbHttpsFail { get => _hbHttpsFail; set { _hbHttpsFail = value; OnProp(); } }
        public int HbHttpsUdpOk { get => _hbHttpsUdpOk; set { _hbHttpsUdpOk = value; OnProp(); } }
        public int HbHttpsUdpFail { get => _hbHttpsUdpFail; set { _hbHttpsUdpFail = value; OnProp(); } }

        public ICommand RegisterCommand { get; }
        public ICommand StartHeartbeatCommand { get; }
        public ICommand StopHeartbeatCommand { get; }
        public ICommand StartHttpsHeartbeatCommand { get; }
        public ICommand StopHttpsHeartbeatCommand { get; }
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
            StartHttpsHeartbeatCommand = new RelayCommand(async _ => await StartHttpsHeartbeatAsync());
            StopHttpsHeartbeatCommand  = new RelayCommand(_ => StopHttpsHeartbeat());
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
                RegStartIp = cfg.RegStartIp;
                RegCount = cfg.RegCount;
                RegConcurrency = cfg.RegConcurrency;
                RegRetryIntervalSec = cfg.RegRetryIntervalSec;
                RegTimeoutMs = cfg.RegTimeoutMs;
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
                RegStartIp = RegStartIp,
                RegCount = RegCount,
                RegConcurrency = RegConcurrency,
                RegRetryIntervalSec = RegRetryIntervalSec,
                RegTimeoutMs = RegTimeoutMs,
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
        
        private void ApplyProjectTypeSelection()
        {
            // 根据项目类型自动勾选对应的日志分类
            if (_projectType == "IEG")
            {
                // IEG专属分类
                CatVulnProtect = true;
                CatProcessAudit = true;
                CatNonWhitelist = true;
                CatWhitelistTamper = true;
                CatRegProtect = true;
                CatUsb = true;
                CatUsbWarning = true;
                CatUDiskPlug = true;
                
                // 通用分类
                CatClientOps = true;
                // CatProcessControl 已移除
                CatOs = true;
                CatOutbound = true;
                CatThreat = false; // IEG的威胁检测只有系统数据采集，不单独发送威胁数据采集
                CatFileProtect = true;
                CatMandatoryAccess = true;
                CatVirusAlert = true;
                
                // 取消EDR专属
                CatFirewall = false;
                CatSysGuard = false;
            }
            else if (_projectType == "EDR")
            {
                // EDR专属分类
                CatFirewall = true;
                CatSysGuard = true;
                
                // 通用分类
                CatClientOps = true;
                // CatProcessControl 已移除
                CatOs = true;
                CatOutbound = true;
                CatThreat = true; // EDR的威胁检测包含威胁数据采集
                CatFileProtect = true;
                CatMandatoryAccess = true;
                CatVirusAlert = true;
                
                // 取消IEG专属
                CatVulnProtect = false;
                CatProcessAudit = false;
                CatNonWhitelist = false;
                CatWhitelistTamper = false;
                CatRegProtect = false;
                CatUsb = false;
                CatUsbWarning = false;
                CatUDiskPlug = false;
            }
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

                // 重置上次统计
                RunOnUi(() =>
                {
                    RegTotal = RegCount;
                    RegSuccess = 0;
                    RegFailed = 0;
                    RegRound = 0;
                    RegFailureDetail = string.Empty;
                    AppendStatus($"开始注册 {RegCount} 个客户端（并发={RegConcurrency}，超时={RegTimeoutMs}ms，轮间隔={RegRetryIntervalSec}s），预生成数据中...");
                });

                var summary = await reg.RegisterAsync(
                    RegPrefix, RegStart, RegCount,
                    startIp: RegStartIp, host: PlatformHost, port: PlatformPort,
                    concurrency: RegConcurrency, retry: 3, timeoutMs: RegTimeoutMs,
                    retryIntervalMs: RegRetryIntervalSec * 1000,
                    roundProgress: new Progress<SimulatorLib.Workers.RegistrationRoundProgress>(p =>
                    {
                        RunOnUi(() =>
                        {
                            RegRound = p.Round;
                            RegSuccess = p.TotalSuccess;
                            RegFailed = p.Remaining;
                            if (p.Remaining > 0)
                                AppendStatus($"第{p.Round}轮完成：本轮成功={p.RoundSuccess} 失败={p.RoundFailed}，等待{RegRetryIntervalSec}s后重试剩余{p.Remaining}个...");
                            else
                                AppendStatus($"第{p.Round}轮完成：本轮成功={p.RoundSuccess}，全部注册成功！");
                        });
                    })).ConfigureAwait(false);

                // 构建失败原因文本
                var detailSb = new StringBuilder();
                foreach (var kv in summary.FailureReasons)
                    detailSb.AppendLine($"  {kv.Key}: {kv.Value}次");

                RunOnUi(() =>
                {
                    RegTotal = summary.Total;
                    RegSuccess = summary.Success;
                    RegFailed = summary.Failed;
                    RegRound = summary.Rounds;
                    RegFailureDetail = detailSb.ToString().Trim();
                    AppendStatus($"注册任务完成（共{summary.Rounds}轮）：成功={summary.Success}  失败={summary.Failed}");
                    if (summary.FailureReasons.Count > 0)
                        AppendStatus("失败原因：\r\n" + detailSb.ToString().TrimEnd());
                });

                // 将统计追加写入 RegistrationStats.log 文件
                try
                {
                    var statsPath = Path.Combine(AppContext.BaseDirectory, "RegistrationStats.log");
                    var line = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {summary.ToLogString()}";
                    await File.AppendAllTextAsync(statsPath, line + Environment.NewLine, Encoding.UTF8).ConfigureAwait(false);
                }
                catch { /* 日志写入失败不影响主流程 */ }
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
                            HbTotal         = s.Total;
                            HbConnected     = s.Connected;
                            HbTcpOk         = s.SuccessTcp;
                            HbTcpFail       = s.FailTcp;
                            HbServerReplied = s.ServerReplied;
                            HbUdpOk         = s.SuccessUdp;
                            HbUdpFail       = s.FailUdp;

                            // 每 30s 输出一条真实状态日志
                            if ((DateTime.Now - _lastHbLogTime).TotalSeconds >= 30)
                            {
                                _lastHbLogTime = DateTime.Now;
                                int offline = s.Total - s.Connected;
                                // 居前三位原因
                                var reasons = new System.Collections.Generic.List<string>();
                                if (s.RsnSessionStale > 0) reasons.Add($"平台踢session:{s.RsnSessionStale}");
                                if (s.RsnConnFailed  > 0) reasons.Add($"连接拒绝:{s.RsnConnFailed}");
                                if (s.RsnConnTimeout > 0) reasons.Add($"连接超时:{s.RsnConnTimeout}");
                                if (s.RsnServerClosed> 0) reasons.Add($"服务端关闭:{s.RsnServerClosed}");
                                if (s.RsnWriteFailed > 0) reasons.Add($"写入失败:{s.RsnWriteFailed}");
                                string reasonStr = reasons.Count > 0
                                    ? "  离线原因: " + string.Join(", ", reasons)
                                    : string.Empty;
                                int silentDrop = s.Connected - s.ServerReplied; // TCP在线但平台无回包
                                // 只有在已有部分客户端收到过回包（ServerReplied>0）时才告警；
                                // 若全部都没回包（如刚启动前几秒），不误报"被踢"。
                                string silentStr = (silentDrop > 0 && s.ServerReplied > 0)
                                    ? $"  ⚠ TCP在线但无回包:{silentDrop}(平台静默踢出/session超时)"
                                    : string.Empty;
                                AppendStatus(
                                    $"[心跳] 总:{s.Total}  TCP连接:{s.Connected}  平台回包:{s.ServerReplied}  TCP离线:{offline}↓" +
                                    silentStr + reasonStr);
                            }
                        });
                    });
                    await hb.StartAsync(HbInterval, useLogServer: UseLogServer, platformHost: PlatformHost, platformPort: PlatformPort, logHost: LogHost, logPort: LogPort, concurrency: 500, ct: _hbCts.Token, progress: progress).ConfigureAwait(false);
                    // 注意：心跳是持续运行的，不需要记录“完成”日志
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

        private async Task StartHttpsHeartbeatAsync()
        {
            try
            {
                var (ok, reason) = ValidateInputs();
                if (!ok) { RunOnUi(() => AppendStatus("输入校验失败: " + reason)); return; }

                await SaveConfigAsync().ConfigureAwait(false);
                _httpsCts?.Cancel();
                _httpsCts = new CancellationTokenSource();
                var hb = new HeartbeatWorker(new TcpSender());
                RunOnUi(() => AppendStatus("开始 HTTPS 心跳任务..."));
                _ = Task.Run(async () =>
                {
                    var progress = new System.Progress<SimulatorLib.Workers.HeartbeatWorker.HeartbeatStats>(s =>
                    {
                        RunOnUi(() =>
                        {
                            HbHttpsTotal = s.Total;
                            HbHttpsOk    = s.SuccessTcp;   // 复用 TCP 成功字段
                            HbHttpsFail  = s.FailTcp;
                        });
                    });
                    await hb.StartHttpsAsync(
                        HbInterval,
                        platformHost: PlatformHost,
                        platformPort: PlatformPort,
                        ct: _httpsCts.Token,
                        progress: progress).ConfigureAwait(false);
                });
            }
            catch (Exception ex)
            {
                RunOnUi(() => AppendStatus("HTTPS 心跳启动异常: " + ex.Message));
            }
        }

        private void StopHttpsHeartbeat()
        {
            if (_httpsCts != null && !_httpsCts.IsCancellationRequested)
            {
                _httpsCts.Cancel();
                RunOnUi(() => AppendStatus("已请求停止 HTTPS 心跳任务"));
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
            if (RegConcurrency <= 0 || RegConcurrency > 5000) return (false, "并发数必须在 1~5000 之间");
            if (RegRetryIntervalSec < 0) return (false, "轮间隔不能为负数");
            if (RegTimeoutMs < 500) return (false, "单次超时不能低于 500ms");
            if (LogClientCount <= 0) return (false, "LogClientCount 必须大于 0");
            if (LogMessagesPerClient <= 0) return (false, "LogMessagesPerClient 必须大于 0");
            if (LogMessagesPerSecondPerClient < 0) return (false, "LogMessagesPerSecondPerClient 不能为负数");
            if (WhitelistClientCount <= 0) return (false, "WhitelistClientCount 必须大于 0");
            if (WhitelistConcurrency <= 0) return (false, "WhitelistConcurrency 必须大于 0");
            
            // 验证起始IP格式（如果已填写）
            if (!string.IsNullOrWhiteSpace(RegStartIp))
            {
                var parts = RegStartIp.Split('.');
                if (parts.Length != 4)
                    return (false, "起始IP格式错误，应为 x.x.x.x 格式");
                foreach (var part in parts)
                {
                    if (!byte.TryParse(part, out _))
                        return (false, "起始IP格式错误，每段应为0-255的数字");
                }
            }
            
            return (true, string.Empty);
        }

        private string[] GetSelectedCategories()
        {
            var list = new System.Collections.Generic.List<string>();
            if (CatClientOps) list.Add("客户端操作");
            if (CatVulnProtect) list.Add("漏洞防护");
            // CatProcessControl 已移除，不存在对应checkbox
            if (CatOs) list.Add("操作系统");
            if (CatOutbound) list.Add("非法外联");
            if (CatThreat) list.Add("威胁数据采集");
            if (CatNonWhitelist) list.Add("非白名单");
            if (CatWhitelistTamper) list.Add("白名单防篡改");
            if (CatFileProtect) list.Add("文件保护");
            if (CatRegProtect) list.Add("注册表保护");
            if (CatMandatoryAccess) list.Add("强制访问控制");
            if (CatProcessAudit) list.Add("进程审计");
            if (CatVirusAlert) list.Add("病毒告警");
            if (CatUsb) list.Add("USB设备");
            if (CatUsbWarning) list.Add("USB访问告警");
            if (CatUDiskPlug) list.Add("U盘插拔");
            if (CatFirewall) list.Add("防火墙");
            if (CatSysGuard) list.Add("系统防护");
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
                            // 只更新状态数值，不记录日志（避免频繁滚动）
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
                    
                    // 任务完成后记录最终结果
                    RunOnUi(() => AppendStatus($"日志发送完成: 总数={LogTotalMessages} 成功={LogSuccess} 失败={LogFailed}"));
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
                            // 只更新状态数值，不记录日志（避免频繁滚动）
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
                    
                    // 任务完成后记录最终结果
                    RunOnUi(() => AppendStatus($"白名单上传完成: 总数={UploadTotal} 成功={UploadSuccess} 失败={UploadFailed}"));
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
