using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using SimulatorLib.Models;
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
        private bool _useLogServer = false;
        private string _regPrefix = "Client-";
        private int _regStart = 1;
        private string _regStartIp = string.Empty;
        private int _regCount = 5;
        private int _hbInterval = 30000;
        private string _projectType = "IEG";

        private int _logMessagesPerClient = 50;
        private int _logHttpsClientCount = 5;
        private int _logHttpsEps = 10;
        private int _logThreatClientCount = 5;
        private int _logThreatEps = 1;
        private long _logTotalMessages;
        private long _logSuccess;
        private long _logFailed;

        private bool _catClientOps = true;
        private bool _catVulnProtect;
        private bool _catProcessControl;
        private bool _catOs;
        private bool _catOutbound;
        // 威胁检测5种事件（均通过 TCP 长连接）
        private bool _catThreatProcStart;   // 进程启动（EDR）
        private bool _catThreatRegAccess;    // 注册表访问（EDR）
        private bool _catThreatFileAccess;   // 文件访问（EDR）
        private bool _catThreatOsEvent;      // 操作系统日志（IEG）
        private bool _catThreatDllLoad;      // DLL加载（EDR）
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
        // 外设控制子类
        private bool _catExtDevUsbPort;
        private bool _catExtDevWpd;
        private bool _catExtDevCdrom;
        private bool _catExtDevWlan;
        private bool _catExtDevUsbEthernet;
        private bool _catExtDevFloppy;
        private bool _catExtDevBluetooth;
        private bool _catExtDevSerial;
        private bool _catExtDevParallel;
        // 插拔 & 网口事件子类（共用 CMDID=204，CMDVER 区分）
        private bool _catNetAdapterEvent;

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
        private int _whitelistCycleIntervalSec = 900; // 15分钟
        private long _uploadTotal;
        private long _uploadSuccess;
        private long _uploadFailed;
        private string _statusLog = string.Empty;

        // 操作系统类型和客户端版本（注册时影响平台功能可用性）
        private string _osType = "Windows"; // "Windows" 或 "Linux"
        private string _regClientVersion = "V300R011C01B090";
        private List<string> _clientVersionList = new()
        {
            "V300R011C01B090",   // Windows 当前版本
            "V300R011C01B030",   // Windows 旧版
            "V300R006C02B090",   // 老版（支持白名单上传）
        };

        // 策略下发接收统计
        private int _policyReceived;
        private int _policyReplied;
        private PolicyReceiveWorker? _policyWorker;
        private HeartbeatStreamRegistry? _hbStreamRegistry;   // 共享心跳流注册表
        private bool _enablePolicyReceive = true;

        // 任务面板
        public ObservableCollection<TaskRecord> TaskRecords { get; } = new();
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

        public int LogMessagesPerClient { get => _logMessagesPerClient; set { _logMessagesPerClient = value; OnProp(); } }
        /// <summary>HTTPS 短连接通道：客户端个数</summary>
        public int LogHttpsClientCount { get => _logHttpsClientCount; set { _logHttpsClientCount = value; OnProp(); } }
        /// <summary>HTTPS 短连接日志：每客户端每秒条数（平台规格 ≤100 EPS）</summary>
        public int LogHttpsEps { get => _logHttpsEps; set { _logHttpsEps = value; OnProp(); } }
        /// <summary>威胁检测 TCP 长连接通道：客户端个数</summary>
        public int LogThreatClientCount { get => _logThreatClientCount; set { _logThreatClientCount = value; OnProp(); } }
        /// <summary>威胁检测 TCP 长连接日志：每客户端每秒条数（平台规格 6000 EPS）</summary>
        public int LogThreatEps { get => _logThreatEps; set { _logThreatEps = value; OnProp(); } }
        public long LogTotalMessages { get => _logTotalMessages; set { _logTotalMessages = value; OnProp(); } }
        public long LogSuccess { get => _logSuccess; set { _logSuccess = value; OnProp(); } }
        public long LogFailed { get => _logFailed; set { _logFailed = value; OnProp(); } }

        public bool CatClientOps { get => _catClientOps; set { _catClientOps = value; OnProp(); } }
        public bool CatVulnProtect { get => _catVulnProtect; set { _catVulnProtect = value; OnProp(); } }
        public bool CatProcessControl { get => _catProcessControl; set { _catProcessControl = value; OnProp(); } }
        public bool CatOs { get => _catOs; set { _catOs = value; OnProp(); } }
        public bool CatOutbound { get => _catOutbound; set { _catOutbound = value; OnProp(); } }
        // 威胁检测5种事件
        public bool CatThreatProcStart { get => _catThreatProcStart; set { _catThreatProcStart = value; OnProp(); } }
        public bool CatThreatRegAccess { get => _catThreatRegAccess; set { _catThreatRegAccess = value; OnProp(); } }
        public bool CatThreatFileAccess { get => _catThreatFileAccess; set { _catThreatFileAccess = value; OnProp(); } }
        public bool CatThreatOsEvent { get => _catThreatOsEvent; set { _catThreatOsEvent = value; OnProp(); } }
        public bool CatThreatDllLoad { get => _catThreatDllLoad; set { _catThreatDllLoad = value; OnProp(); } }
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
        // 外设控制子类
        public bool CatExtDevUsbPort { get => _catExtDevUsbPort; set { _catExtDevUsbPort = value; OnProp(); } }
        public bool CatExtDevWpd { get => _catExtDevWpd; set { _catExtDevWpd = value; OnProp(); } }
        public bool CatExtDevCdrom { get => _catExtDevCdrom; set { _catExtDevCdrom = value; OnProp(); } }
        public bool CatExtDevWlan { get => _catExtDevWlan; set { _catExtDevWlan = value; OnProp(); } }
        public bool CatExtDevUsbEthernet { get => _catExtDevUsbEthernet; set { _catExtDevUsbEthernet = value; OnProp(); } }
        public bool CatExtDevFloppy { get => _catExtDevFloppy; set { _catExtDevFloppy = value; OnProp(); } }
        public bool CatExtDevBluetooth { get => _catExtDevBluetooth; set { _catExtDevBluetooth = value; OnProp(); } }
        public bool CatExtDevSerial { get => _catExtDevSerial; set { _catExtDevSerial = value; OnProp(); } }
        public bool CatExtDevParallel { get => _catExtDevParallel; set { _catExtDevParallel = value; OnProp(); } }
        // 插拔 & 网口事件子类
        public bool CatNetAdapterEvent { get => _catNetAdapterEvent; set { _catNetAdapterEvent = value; OnProp(); } }

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
        public int WhitelistCycleIntervalSec { get => _whitelistCycleIntervalSec; set { _whitelistCycleIntervalSec = value; OnProp(); } }
        public long UploadTotal { get => _uploadTotal; set { _uploadTotal = value; OnProp(); } }
        public long UploadSuccess { get => _uploadSuccess; set { _uploadSuccess = value; OnProp(); } }
        public long UploadFailed { get => _uploadFailed; set { _uploadFailed = value; OnProp(); } }

        // 注册版本
        public string RegClientVersion
        {
            get => _regClientVersion;
            set { _regClientVersion = value; OnProp(); OnProp(nameof(OsInfoText)); }
        }
        /// <summary>注册版本下拉列表（随操作系统类型切换而变化）</summary>
        public IReadOnlyList<string> ClientVersionList => _clientVersionList;

        // 操作系统类型切换
        public bool IsOsWindows
        {
            get => _osType == "Windows";
            set { if (value && _osType != "Windows") { _osType = "Windows"; OnOsTypeChanged(); } }
        }
        public bool IsOsLinux
        {
            get => _osType == "Linux";
            set { if (value && _osType != "Linux") { _osType = "Linux"; OnOsTypeChanged(); } }
        }
        /// <summary>注册时填入 WindowsVersion 字段的实际值</summary>
        public string RegWindowsVersion =>
            _osType == "Linux" ? "Linux centos7" : SimulatorLib.Protocol.OsInfo.GetWindowsVersionName();
        /// <summary>在 UI 上显示当前 OS 信息的提示文本</summary>
        public string OsInfoText =>
            _osType == "Linux"
            ? $"OS: Linux centos7   版本: {RegClientVersion}"
            : $"OS: {SimulatorLib.Protocol.OsInfo.GetWindowsVersionName()}   版本: {RegClientVersion}";

        /// <summary>心跳设置面板展示的协议模式标签（随 OS 类型自动切换）</summary>
        public string HeartbeatModeLabel =>
            _osType == "Linux" ? "HTTPS（Linux）" : "TCP（Windows）";

        private void OnOsTypeChanged()
        {
            if (_osType == "Linux")
            {
                _clientVersionList = new List<string> { "V300R011C11B060-Redhat7.x-x64" };
                _regClientVersion  = "V300R011C11B060-Redhat7.x-x64";
            }
            else
            {
                _clientVersionList = new List<string>
                {
                    "V300R011C01B090",
                    "V300R011C01B030",
                    "V300R006C02B090",
                };
                _regClientVersion  = "V300R011C01B090";
            }
            OnProp(nameof(ClientVersionList));
            OnProp(nameof(RegClientVersion));
            OnProp(nameof(IsOsWindows));
            OnProp(nameof(IsOsLinux));
            OnProp(nameof(OsInfoText));
            OnProp(nameof(HeartbeatModeLabel));
        }

        // 策略接收
        public bool EnablePolicyReceive
        {
            get => _enablePolicyReceive;
            set { _enablePolicyReceive = value; OnProp(); }
        }
        public int PolicyReceived { get => _policyReceived; set { _policyReceived = value; OnProp(); } }
        public int PolicyReplied  { get => _policyReplied;  set { _policyReplied  = value; OnProp(); } }

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

        /// <summary>向任务面板追加一条记录（线程安全，自动切换到 UI 线程）。</summary>
        private TaskRecord AddTaskRecord(string type, int clientCount, int intervalSec = 0)
        {
            var r = new TaskRecord(type, clientCount, intervalSec);
            RunOnUi(() => TaskRecords.Add(r));
            return r;
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

                LogMessagesPerClient = cfg.LogMessagesPerClient;
                LogHttpsClientCount = cfg.LogHttpsClientCount;
                LogHttpsEps = cfg.LogHttpsEps;
                LogThreatClientCount = cfg.LogThreatClientCount;
                LogThreatEps = cfg.LogThreatEps;

                WhitelistFilePath = cfg.WhitelistFilePath;
                WhitelistClientCount = cfg.WhitelistClientCount;
                WhitelistConcurrency = cfg.WhitelistConcurrency;

                // 操作系统类型（加载后触发版本列表更新）
                if (!string.IsNullOrEmpty(cfg.ClientOsType) && cfg.ClientOsType != _osType)
                {
                    _osType = cfg.ClientOsType;
                    OnOsTypeChanged();
                }

                AppendStatus("配置已加载");
                ApplyProjectTypeSelection(); // 按当前项目类型（默认IEG）恢复日志分类勾选
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
                LogMessagesPerClient = LogMessagesPerClient,
                LogHttpsClientCount = LogHttpsClientCount,
                LogHttpsEps = LogHttpsEps,
                LogThreatClientCount = LogThreatClientCount,
                LogThreatEps = LogThreatEps,
                WhitelistFilePath = WhitelistFilePath,
                WhitelistClientCount = WhitelistClientCount,
                WhitelistConcurrency = WhitelistConcurrency,
                ClientOsType = _osType,
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
                // IEG如属：威胁检测中只有操作系统日志
                CatThreatOsEvent = true;
                CatThreatProcStart = false;
                CatThreatRegAccess = false;
                CatThreatFileAccess = false;
                CatThreatDllLoad = false;
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
                // EDR如属：威胁检测全部5种事件（EDR 4种 + IEG操作系统日志）
                CatThreatProcStart = true;
                CatThreatRegAccess = true;
                CatThreatFileAccess = true;
                CatThreatDllLoad = true;
                CatThreatOsEvent = true;
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
                    clientVersion: RegClientVersion,
                    windowsVersion: RegWindowsVersion,
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
                // 防重入：相同任务只允许一个实例运行
                if (_hbCts != null && !_hbCts.IsCancellationRequested)
                {
                    RunOnUi(() => AppendStatus("⚠ 心跳任务已在运行，请先点击【停止】再重新开始"));
                    return;
                }

                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() => AppendStatus("输入校验失败: " + reason));
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                _hbCts = new CancellationTokenSource();
                _lastHbLogTime = DateTime.Now;

                var hbTaskRec = AddTaskRecord("心跳", RegCount, HbInterval / 1000);

                if (_osType == "Linux")
                {
                    // ── Linux 路径：HTTPS 心跳 + 可选 UDP 到日志服务器 ────────────────────
                    var udpSender = UseLogServer ? new UdpSender() : null;
                    var hb = new HeartbeatWorker(new TcpSender(), udpSender);
                    RunOnUi(() => AppendStatus($"开始心跳任务（HTTPS 模式，目标: https://{PlatformHost}:{PlatformPort}/USM/clientHeartbeat.do）" +
                        (UseLogServer ? $"  + UDP 日志服务器 {LogHost}:{LogPort}" : "") + "..."));
                    _ = Task.Run(async () =>
                    {
                        var progress = new System.Progress<SimulatorLib.Workers.HeartbeatWorker.HeartbeatStats>(s =>
                        {
                            RunOnUi(() =>
                            {
                                HbTotal     = s.Total;
                                HbConnected = s.Connected;
                                HbTcpOk     = s.SuccessTcp;   // HTTPS 成功数复用此字段
                                HbTcpFail   = s.FailTcp;      // HTTPS 失败数复用此字段
                                HbUdpOk     = s.SuccessUdp;
                                HbUdpFail   = s.FailUdp;
                            });
                        });
                        await hb.StartHttpsAsync(
                            HbInterval,
                            platformHost:  PlatformHost,
                            platformPort:  PlatformPort,
                            ct:            _hbCts.Token,
                            progress:      progress,
                            osVersion:     "Linux centos7",
                            useLogServer:  UseLogServer,
                            logHost:       LogHost,
                            logPort:       LogPort,
                            udpSender:     udpSender).ConfigureAwait(false);
                        hbTaskRec.MarkStopped();
                    });
                }
                else
                {
                    // ── Windows 路径：TCP 长连接心跳 + 可选 UDP 到日志服务器 ───────────────
                    var tcp = new TcpSender();
                    var udp = new UdpSender();
                    _policyWorker = EnablePolicyReceive ? new PolicyReceiveWorker(PlatformHost, PlatformPort) : null;
                    _hbStreamRegistry = new HeartbeatStreamRegistry();
                    var hb = new HeartbeatWorker(tcp, udp, _policyWorker, _hbStreamRegistry);
                    RunOnUi(() => AppendStatus("开始心跳任务（TCP 模式）" +
                        (EnablePolicyReceive ? "（策略接收已开启）" : string.Empty) +
                        (UseLogServer ? $"  + UDP 日志服务器 {LogHost}:{LogPort}" : "") + "..."));
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

                                // 仅在出现异常时输出日志；同一状态每 30s 最多一条，避免刷屏
                                {
                                    int offline = s.Total - s.Connected;
                                    bool hasOffline = offline > 0;
                                    bool hasReasons = s.RsnSessionStale > 0 || s.RsnConnFailed > 0 ||
                                                         s.RsnConnTimeout  > 0 || s.RsnServerClosed > 0 ||
                                                         s.RsnWriteFailed  > 0 || s.RsnLockBusy > 0;
                                    bool hasIssue = hasOffline || hasReasons;

                                    if (hasIssue && (DateTime.Now - _lastHbLogTime).TotalSeconds >= 30)
                                    {
                                        _lastHbLogTime = DateTime.Now;
                                        var reasons = new System.Collections.Generic.List<string>();
                                        if (s.RsnSessionStale > 0) reasons.Add($"平台踢session:{s.RsnSessionStale}");
                                        if (s.RsnConnFailed   > 0) reasons.Add($"连接拒绝:{s.RsnConnFailed}");
                                        if (s.RsnConnTimeout  > 0) reasons.Add($"连接超时:{s.RsnConnTimeout}");
                                        if (s.RsnServerClosed > 0) reasons.Add($"服务端关闭:{s.RsnServerClosed}");
                                        if (s.RsnWriteFailed  > 0) reasons.Add($"写入失败:{s.RsnWriteFailed}");
                                        if (s.RsnLockBusy     > 0) reasons.Add($"⚡锁竞争跳过:{s.RsnLockBusy}");
                                        string reasonStr = reasons.Count > 0
                                            ? "  离线原因: " + string.Join(", ", reasons)
                                            : string.Empty;
                                        AppendStatus(
                                            $"[心跳] ⚠ 总:{s.Total}  TCP连接:{s.Connected}  平台回包:{s.ServerReplied}  TCP离线:{offline}↓" +
                                            reasonStr);
                                    }
                                }
                            });
                        });
                        await hb.StartAsync(HbInterval, useLogServer: UseLogServer, platformHost: PlatformHost, platformPort: PlatformPort, logHost: LogHost, logPort: LogPort, concurrency: 500, ct: _hbCts.Token, progress: progress,
                            osVersion: null).ConfigureAwait(false);
                        hbTaskRec.MarkStopped();
                    });
                    // 定时将策略接收统计同步到 UI
                    _ = Task.Run(async () =>
                    {
                        while (_hbCts != null && !_hbCts.IsCancellationRequested)
                        {
                            if (_policyWorker != null)
                            {
                                RunOnUi(() =>
                                {
                                    PolicyReceived = _policyWorker.ReceivedCount;
                                    PolicyReplied  = _policyWorker.RepliedCount;
                                    if (hbTaskRec.Status == SimulatorLib.Models.TaskStatus.Running)
                                        hbTaskRec.Detail = $"策略收到:{PolicyReceived} 已回包:{PolicyReplied}";
                                });
                            }
                            try { await Task.Delay(2000, _hbCts.Token).ConfigureAwait(false); }
                            catch { break; }
                        }
                    });
                }
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
            // 兼容旧路径（直接调用 HTTPS 命令时产生的 CTS）
            if (_httpsCts != null && !_httpsCts.IsCancellationRequested)
                _httpsCts.Cancel();
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
                            HbHttpsOk    = s.Connected;          // HTTPS 响应成功数
                            HbHttpsFail  = s.Total - s.Connected; // 其余为失败/未响应
                        });
                    });
                    await hb.StartHttpsAsync(
                        HbInterval,
                        platformHost: PlatformHost,
                        platformPort: PlatformPort,
                        ct: _httpsCts.Token,
                        progress: progress,
                        osVersion: _osType == "Linux" ? "Linux centos7" : null).ConfigureAwait(false);
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
            if (LogMessagesPerClient <= 0) return (false, "LogMessagesPerClient 必须大于 0");
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
            // 威胁检测 5 种事件（TCP 长连接）
            if (CatThreatProcStart) list.Add("威胁检测-进程启动");
            if (CatThreatRegAccess) list.Add("威胁检测-注册表访问");
            if (CatThreatFileAccess) list.Add("威胁检测-文件访问");
            if (CatThreatOsEvent) list.Add("威胁检测-系统日志");
            if (CatThreatDllLoad) list.Add("威胁检测-DLL加载");
            if (CatNonWhitelist) list.Add("非白名单");
            if (CatWhitelistTamper) list.Add("白名单防篡改");
            if (CatFileProtect) list.Add("文件保护");
            if (CatRegProtect) list.Add("注册表保护");
            if (CatMandatoryAccess) list.Add("强制访问控制");
            if (CatProcessAudit) list.Add("进程审计");
            if (CatVirusAlert) list.Add("病毒告警");
            if (CatUsb) list.Add("U盘告警(老版本)");
            if (CatUsbWarning) list.Add("USB访问告警");
            if (CatUDiskPlug) list.Add("U盘插拔");
            if (CatFirewall) list.Add("防火墙");
            if (CatSysGuard) list.Add("系统防护");
            // 外设控制子类
            if (CatExtDevUsbPort) list.Add("禁USB接口");
            if (CatExtDevWpd) list.Add("禁手机平板");
            if (CatExtDevCdrom) list.Add("禁CDROM");
            if (CatExtDevWlan) list.Add("禁无线网卡");
            if (CatExtDevUsbEthernet) list.Add("禁USB网卡");
            if (CatExtDevFloppy) list.Add("禁软盘");
            if (CatExtDevBluetooth) list.Add("禁蓝牙");
            if (CatExtDevSerial) list.Add("禁串口");
            if (CatExtDevParallel) list.Add("禁并口");
            // 插拔 & 网口事件子类
            if (CatNetAdapterEvent) list.Add("网口Up/Down");
            return list.Count == 0 ? new[] { "Default" } : list.ToArray();
        }

        private static bool IsThreatCategoryByName(string category) =>
            category.StartsWith("威胁检测-", System.StringComparison.Ordinal);

        private async Task StartLogSendAsync()
        {
            try
            {
                // 防重入：相同任务只允许一个实例运行
                if (_logCts != null && !_logCts.IsCancellationRequested)
                {
                    RunOnUi(() => AppendStatus("⚠ 日志发送任务已在运行，请先点击【结束任务】再重新开始"));
                    return;
                }

                var (ok, reason) = ValidateInputs();
                if (!ok)
                {
                    RunOnUi(() => AppendStatus("输入校验失败: " + reason));
                    return;
                }

                await SaveConfigAsync().ConfigureAwait(false);
                _logCts = new CancellationTokenSource();

                var cats = GetSelectedCategories();
                var threatCats = cats.Where(IsThreatCategoryByName).ToArray();
                var httpsCats  = cats.Where(c => !IsThreatCategoryByName(c)).ToArray();

                var totalHttps  = (httpsCats.Length  > 0 && LogHttpsClientCount  > 0) ? (long)LogHttpsClientCount  * LogMessagesPerClient : 0;
                var totalThreat = (threatCats.Length > 0 && LogThreatClientCount > 0) ? (long)LogThreatClientCount * LogMessagesPerClient : 0;

                // 创建任务面板记录（客户数取两个通道之和，间隔=0 表示一次性任务）
                int logDisplayClients = (httpsCats.Length  > 0 && LogHttpsClientCount  > 0 ? LogHttpsClientCount  : 0)
                                      + (threatCats.Length > 0 && LogThreatClientCount > 0 ? LogThreatClientCount : 0);
                var logTaskRec = AddTaskRecord("日志发送", logDisplayClients, 0);

                RunOnUi(() =>
                {
                    LogTotalMessages = totalHttps + totalThreat;
                    LogSuccess = 0;
                    LogFailed  = 0;

                    var httpsDesc  = httpsCats.Length  > 0 ? $"HTTPS({httpsCats.Length}种, {LogHttpsClientCount}客户端, {LogHttpsEps} EPS/客户端)" : "";
                    var threatDesc = threatCats.Length > 0 ? $"威胁检测TCP({threatCats.Length}种, {LogThreatClientCount}客户端, {LogThreatEps} EPS/客户端)" : "";
                    var channels = string.Join(" + ",
                        new[] { httpsDesc, threatDesc }.Where(s => s.Length > 0));
                    AppendStatus($"开始日志发送：每客户端条数={LogMessagesPerClient} 通道=[{channels}]");
                });

                _ = Task.Run(async () =>
                {
                    if ((httpsCats.Length == 0 || LogHttpsClientCount <= 0) && (threatCats.Length == 0 || LogThreatClientCount <= 0))
                    {
                        RunOnUi(() => AppendStatus("⚠ HTTPS 和威胁检测通道均未起用（客户端数均为0）"));
                        logTaskRec.MarkStopped();
                        return;
                    }

                    // 两个通道各自维护 success/fail 计数，合并上报 UI 和任务面板
                    long httpsOk = 0L, httpsFail = 0L;
                    long threatOk = 0L, threatFail = 0L;

                    void ReportCombined()
                    {
                        RunOnUi(() =>
                        {
                            LogSuccess = httpsOk  + threatOk;
                            LogFailed  = httpsFail + threatFail;
                            logTaskRec.SuccessCount = LogSuccess;
                            logTaskRec.FailCount    = LogFailed;
                        });
                    }

                    var workerTasks = new List<Task>();

                    // ── HTTPS 通道（短连接，EPS ≤100）──────────────────────────
                    // 客户端数=0 表示禁用此通道
                    if (httpsCats.Length > 0 && LogHttpsClientCount > 0)
                    {
                        var httpsWorker = new LogWorker(new TcpSender(), new UdpSender(), _hbStreamRegistry);
                        var httpsProgress = new Progress<SimulatorLib.Workers.LogSendStats>(s =>
                        {
                            System.Threading.Interlocked.Exchange(ref httpsOk,   s.Success);
                            System.Threading.Interlocked.Exchange(ref httpsFail, s.Failed);
                            ReportCombined();
                        });
                        workerTasks.Add(httpsWorker.StartAsync(
                            messagesPerClient:          LogMessagesPerClient,
                            messagesPerSecondPerClient: LogHttpsEps  <= 0 ? null : (int?)LogHttpsEps,
                            maxClients:                 LogHttpsClientCount,
                            categories:                 httpsCats,
                            useLogServer:               UseLogServer,
                            platformHost:               PlatformHost,
                            platformPort:               PlatformPort,
                            logHost:                    LogHost,
                            logPort:                    LogPort,
                            concurrency:                1,
                            stressMode:                 false,
                            localIps:                   null,
                            ct:                         _logCts.Token,
                            progress:                   httpsProgress));
                    }

                    // ── 威胁检测 TCP 长连接通道（EPS 可达 6000）────────────────
                    // 客户端数=0 表示禁用此通道
                    if (threatCats.Length > 0 && LogThreatClientCount > 0)
                    {
                        var threatWorker = new LogWorker(new TcpSender(), new UdpSender(), _hbStreamRegistry);
                        var threatProgress = new Progress<SimulatorLib.Workers.LogSendStats>(s =>
                        {
                            System.Threading.Interlocked.Exchange(ref threatOk,   s.Success);
                            System.Threading.Interlocked.Exchange(ref threatFail, s.Failed);
                            ReportCombined();
                        });
                        workerTasks.Add(threatWorker.StartAsync(
                            messagesPerClient:          LogMessagesPerClient,
                            messagesPerSecondPerClient: LogThreatEps <= 0 ? null : (int?)LogThreatEps,
                            maxClients:                 LogThreatClientCount,
                            categories:                 threatCats,
                            useLogServer:               UseLogServer,
                            platformHost:               PlatformHost,
                            platformPort:               PlatformPort,
                            logHost:                    LogHost,
                            logPort:                    LogPort,
                            concurrency:                1,
                            stressMode:                 false,
                            localIps:                   null,
                            ct:                         _logCts.Token,
                            progress:                   threatProgress));
                    }

                    try
                    {
                        await Task.WhenAll(workerTasks).ConfigureAwait(false);
                    }
                    finally
                    {
                        logTaskRec.MarkStopped();
                    }
                    RunOnUi(() =>
                        AppendStatus($"日志发送完成: 总数={LogTotalMessages} 成功={LogSuccess} 失败={LogFailed}"));
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
                // 防重入：相同任务只允许一个实例运行
                if (_uploadCts != null && !_uploadCts.IsCancellationRequested)
                {
                    RunOnUi(() => AppendStatus("⚠ 白名单上传任务已在运行，请先点击【结束任务】再重新开始"));
                    return;
                }

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

                // 取消上一次任务（重新 AddTask = 重置已上传集合）
                _uploadCts?.Cancel();
                _uploadCts = new CancellationTokenSource();
                var tcp    = new TcpSender();
                var worker = new WhitelistUploadWorker(tcp);

                // 创建任务面板记录
                var taskRec = AddTaskRecord("白名单上传", WhitelistClientCount, WhitelistCycleIntervalSec);

                RunOnUi(() =>
                {
                    UploadTotal = 0;
                    UploadSuccess = 0;
                    UploadFailed = 0;
                    AppendStatus($"开始白名单上传（随机轮转）：文件={System.IO.Path.GetFileName(WhitelistFilePath)}" +
                                 $" 每轮客户端数={WhitelistClientCount} 并发={WhitelistConcurrency}" +
                                 $" 轮间隔={WhitelistCycleIntervalSec}s");
                });

                _ = Task.Run(async () =>
                {
                    try
                    {
                        // 订阅 TaskRecord 属性变化，同步更新 UI 统计数值
                        taskRec.PropertyChanged += (_, e) =>
                        {
                            if (e.PropertyName == nameof(TaskRecord.SuccessCount) ||
                                e.PropertyName == nameof(TaskRecord.FailCount))
                            {
                                RunOnUi(() =>
                                {
                                    UploadSuccess = taskRec.SuccessCount;
                                    UploadFailed  = taskRec.FailCount;
                                    UploadTotal   = taskRec.SuccessCount + taskRec.FailCount;
                                });
                            }
                            if (e.PropertyName == nameof(TaskRecord.Detail))
                                RunOnUi(() => AppendStatus("[白名单] " + taskRec.Detail));
                        };

                        await worker.RunRotatingAsync(
                            filePath:        WhitelistFilePath,
                            clientCount:     WhitelistClientCount,
                            platformHost:    PlatformHost,
                            platformPort:    PlatformPort,
                            concurrency:     WhitelistConcurrency,
                            cycleIntervalMs: WhitelistCycleIntervalSec * 1000,
                            record:          taskRec,
                            ct:              _uploadCts.Token).ConfigureAwait(false);

                        RunOnUi(() => AppendStatus(
                            $"白名单上传结束: 成功={taskRec.SuccessCount} 失败={taskRec.FailCount} 状态={taskRec.StatusText}"));
                    }
                    catch (Exception ex)
                    {
                        taskRec.Detail = "异常: " + ex.Message;
                        taskRec.Status = SimulatorLib.Models.TaskStatus.Error;
                        RunOnUi(() => AppendStatus("白名单上传异常: " + ex.Message));
                    }
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
