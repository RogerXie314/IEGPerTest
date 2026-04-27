using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using SimulatorLib.Models;
using SimulatorLib.Network;
using SimulatorLib.Workers;
using SimulatorLib.Persistence;
using SimulatorApp.Workers;

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
        private int _logHttpsClientCount = 0;
        private int _logHttpsEps = 0;
        private int _logThreatClientCount = 50;
        private int _logThreatEps = 1;
        private int _logThreatHitEvery = 71;
        private long _logTotalMessages;
        private long _logSuccess;
        private long _logFailed;

        private bool _catClientOps = true;
        private bool _catVulnProtect;
        private bool _catProcessControl;
        private bool _catOs;
        private bool _catOutbound;
        // 威胁检测5种事件（均通过 TCP 长连接）
        private bool _catThreatProcStart = true;   // 进程启动（EDR）
        private bool _catThreatRegAccess  = true;   // 注册表访问（EDR）
        private bool _catThreatFileAccess = true;   // 文件访问（EDR）
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
        private int _registeredClientCount;  // 已注册客户端总数
        private string _regFailureDetail = string.Empty;

        private string _whitelistFilePath = string.Empty;
        private bool _enableWhitelistOnReg = false;
        private int _whitelistConcurrency = 4;
        private long _uploadTotal;
        private long _uploadSuccess;
        private long _uploadFailed;
        private string _statusLog = string.Empty;

        // 操作系统类型和客户端版本（注册时影响平台功能可用性）
        private string _osType = "Windows"; // "Windows" 或 "Linux"
        private string _regClientVersion = "V300R011C01B090";
        private List<string> _clientVersionList = new();

        // 策略下发接收统计
        private int _policyReceived;
        private int _policyReplied;
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
        private ProcessEngine? _processEngine;               // 进程引擎（子进程方式）
        private TaskRecord? _hbTaskRec;                      // ProcessEngine 路径的心跳任务面板记录
        private readonly SemaphoreSlim _reregLock = new SemaphoreSlim(1, 1);  // 防止并发重注册竞争 Clients.log
        private readonly HashSet<string> _reregInFlight = new HashSet<string>(StringComparer.OrdinalIgnoreCase);  // 正在重注册的客户端集合

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
        /// <summary>威胁命中轮比：每 N 轮仅第 1 轮发 hit 包，其余发 miss 包（对齐老工具 bHit=1/71）；0 或 1 = 每轮均 hit</summary>
        public int LogThreatHitEvery { get => _logThreatHitEvery; set { _logThreatHitEvery = value; OnProp(); } }
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
        public int RegSuccess { get => _regSuccess; set { _regSuccess = value; OnProp(); CommandManager.InvalidateRequerySuggested(); } }
        public int RegFailed { get => _regFailed; set { _regFailed = value; OnProp(); } }
        public string RegFailureDetail { get => _regFailureDetail; set { _regFailureDetail = value; OnProp(); } }

        // 已注册客户端总数（包括历史注册的，用于判断心跳按钮是否可用）
        public int RegisteredClientCount { get => _registeredClientCount; set { _registeredClientCount = value; OnProp(); CommandManager.InvalidateRequerySuggested(); } }

        public string WhitelistFilePath { get => _whitelistFilePath; set { _whitelistFilePath = value; OnProp(); } }
        /// <summary>对齐老工具：注册完成后自动对全部已注册客户端上传一次白名单</summary>
        public bool EnableWhitelistOnReg { get => _enableWhitelistOnReg; set { _enableWhitelistOnReg = value; OnProp(); } }
        public int WhitelistConcurrency { get => _whitelistConcurrency; set { _whitelistConcurrency = value; OnProp(); } }
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

        /// <summary>加载客户端版本列表</summary>
        private void LoadClientVersions()
        {
            var config = SimulatorLib.Config.ClientVersionConfig.Load();
            if (_osType == "Linux")
            {
                _clientVersionList = config.LinuxVersions.ToList();
                _regClientVersion = _clientVersionList.FirstOrDefault() ?? "V300R011C11B060-Redhat7.x-x64";
            }
            else
            {
                _clientVersionList = config.WindowsVersions.ToList();
                _regClientVersion = _clientVersionList.FirstOrDefault() ?? "V300R011C01B090";
            }
        }

        private void OnOsTypeChanged()
        {
            LoadClientVersions();
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
        public int HbConnected { get => _hbConnected; set { _hbConnected = value; OnProp(); CommandManager.InvalidateRequerySuggested(); } }
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
        public ICommand PreviewWhitelistCommand { get; }
        public ICommand StartWhitelistUploadCommand { get; }
        public ICommand StopWhitelistUploadCommand { get; }

        public MainViewModel()
        {
            _uiContext = SynchronizationContext.Current;
            
            // 加载客户端版本列表
            LoadClientVersions();
            
            RegisterCommand = new RelayCommand(async _ => await RegisterAsync());
            StartHeartbeatCommand = new RelayCommand(
                async _ => await StartHeartbeatAsync(),
                _ => RegisteredClientCount > 0);  // 只要有已注册客户端就可以开始心跳
            StopHeartbeatCommand = new RelayCommand(_ => StopHeartbeat());
            PortTestCommand = new RelayCommand(async _ => await PortTestAsync());
            StartLogSendCommand = new RelayCommand(
                async _ => await StartLogSendAsync(),
                _ => HbTotal > 0 && HbConnected >= HbTotal);
            StopLogSendCommand = new RelayCommand(_ => StopLogSend());
            BrowseWhitelistFileCommand = new RelayCommand(_ => BrowseWhitelistFile());
            PreviewWhitelistCommand = new RelayCommand(_ => PreviewWhitelist(), _ => !string.IsNullOrEmpty(WhitelistFilePath) && File.Exists(WhitelistFilePath));
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
            
            // 加载已注册客户端数量
            var allClients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
            var registeredCount = allClients.Count(c => c.Status == "Registered");
            
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
                LogThreatHitEvery = cfg.LogThreatHitEvery;

                WhitelistFilePath = cfg.WhitelistFilePath;
                EnableWhitelistOnReg = cfg.EnableWhitelistOnReg;
                WhitelistConcurrency = cfg.WhitelistConcurrency;

                // 设置已注册客户端数量
                RegisteredClientCount = registeredCount;

                // 操作系统类型（加载后触发版本列表更新）
                if (!string.IsNullOrEmpty(cfg.ClientOsType) && cfg.ClientOsType != _osType)
                {
                    _osType = cfg.ClientOsType;
                    OnOsTypeChanged();
                }

                AppendStatus($"配置已加载，已注册客户端：{registeredCount} 台");
                ApplyProjectTypeSelection(); // 按当前项目类型（默认IEG）恢复日志分类勾选
            });
        }

        public async Task ResetRegistrationAsync()
        {
            // 清空 Clients.log
            await ClientsPersistence.WriteAllAsync(new System.Collections.Generic.List<SimulatorLib.Persistence.ClientRecord>()).ConfigureAwait(false);

            RunOnUi(() =>
            {
                // 重置计数
                RegisteredClientCount = 0;
                // 恢复注册表单默认值（对齐老工具 Reset 行为）
                RegPrefix  = "Client-";
                RegStart   = 1;
                RegStartIp = "192.168.0.1";
                RegCount   = 5;
                AppendStatus("已重置：客户端列表已清空，注册设置已恢复默认值");
            });

            await SaveConfigAsync().ConfigureAwait(false);
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
                LogThreatHitEvery = LogThreatHitEvery,
                WhitelistFilePath = WhitelistFilePath,
                EnableWhitelistOnReg = EnableWhitelistOnReg,
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
                // IEG如属：威胁检测进程启动/注册表/文件访问（默认不勾选操作系统日志）
                CatThreatOsEvent = false;
                CatThreatProcStart = true;
                CatThreatRegAccess = true;
                CatThreatFileAccess = true;
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
                // EDR如属：威胁检测全部4种EDR事件（默认不勾选操作系统日志）
                CatThreatProcStart = true;
                CatThreatRegAccess = true;
                CatThreatFileAccess = true;
                CatThreatDllLoad = true;
                CatThreatOsEvent = false;
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
                    // 更新已注册客户端总数
                    RegisteredClientCount = summary.Success;
                    AppendStatus($"注册任务完成（共{summary.Rounds}轮）：成功={summary.Success}  失败={summary.Failed}");
                    if (summary.FailureReasons.Count > 0)
                        System.Diagnostics.Debug.WriteLine("[注册失败原因] " + detailSb.ToString().TrimEnd());
                });

                // 将统计追加写入 RegistrationStats.log 文件
                try
                {
                    var statsLogDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    Directory.CreateDirectory(statsLogDir);
                    var statsPath = Path.Combine(statsLogDir, "RegistrationStats.log");
                    var line = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {summary.ToLogString()}";
                    await File.AppendAllTextAsync(statsPath, line + Environment.NewLine, Encoding.UTF8).ConfigureAwait(false);
                }
                catch { /* 日志写入失败不影响主流程 */ }

                // 对齐老工具：注册完成后自动上传白名单（勾选了“注册后自动上传”时）
                if (EnableWhitelistOnReg &&
                    !string.IsNullOrWhiteSpace(WhitelistFilePath) &&
                    System.IO.File.Exists(WhitelistFilePath) &&
                    summary.Success > 0)
                {
                    var allClients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                    var autoRec = AddTaskRecord("白名单上传(自动)", allClients.Count, 0);
                    RunOnUi(() => AppendStatus(
                        $"注册完成，自动开始白名单上传（全量 {allClients.Count} 台，并发={WhitelistConcurrency}）…"));
                    _ = Task.Run(async () =>
                    {
                        try
                        {
                            var uploadWorker = new WhitelistUploadWorker(new TcpSender());
                            await uploadWorker.RunAllOnceAsync(
                                WhitelistFilePath, allClients,
                                PlatformHost, PlatformPort,
                                WhitelistConcurrency,
                                autoRec,
                                CancellationToken.None).ConfigureAwait(false);
                            RunOnUi(() => AppendStatus(
                                $"[白名单自动上传] 完成：成功={autoRec.SuccessCount} 失败={autoRec.FailCount}"));
                        }
                        catch (Exception uploadEx)
                        {
                            RunOnUi(() => AppendStatus("白名单自动上传异常: " + uploadEx.Message));
                        }
                    });
                }
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
                    // ── Windows 路径：使用 ProcessEngine 子进程方式 ───────────────
                    var clients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                    if (clients.Count == 0) { RunOnUi(() => AppendStatus("⚠ 未找到已注册客户端")); return; }

                    // 检查 NativeRunner.exe 是否存在
                    var wlTestPath = Path.Combine(AppContext.BaseDirectory, "NativeRunner.exe");
                    if (!File.Exists(wlTestPath))
                    {
                        RunOnUi(() => AppendStatus($"⚠ 未找到 NativeRunner.exe，无法启动心跳。路径: {wlTestPath}"));
                        return;
                    }

                    _processEngine?.Dispose();
                    _processEngine = new ProcessEngine(wlTestPath, "config.ini");
                    
                    // 订阅事件
                    _processEngine.OnStatsUpdated += () =>
                    {
                        RunOnUi(() =>
                        {
                            HbTotal = _processEngine.RegisteredTotal;
                            HbConnected = _processEngine.HBServerAck;   // 服务器确认上线（非18回包）
                            HbTcpOk = _processEngine.HBSending;         // TCP连接数（含未被服务器确认的）
                            HbTcpFail = _processEngine.HBNotSending;
                            
                            if (hbTaskRec.Status == SimulatorLib.Models.TaskStatus.Running)
                                hbTaskRec.Detail = $"在线:{_processEngine.HBServerAck}/{_processEngine.RegisteredTotal}";
                        });
                    };
                    
                    _processEngine.OnInfoMessage += (msg) =>
                    {
                        RunOnUi(() => AppendStatus(msg));
                    };
                    
                    _processEngine.OnErrorMessage += (msg) =>
                    {
                        RunOnUi(() => AppendStatus($"⚠ {msg}"));
                    };

                    // NOREGISTER 重注册：NativeRunner 收到 cmdId=18 时通知 C#，
                    // C# 用 HTTPS 重注册并将新 DeviceId 写回 NativeRunner（对齐 NativeEngine.dll 的 g_onNeedReregister 回调）
                    var capturedEngine = _processEngine;
                    _processEngine.OnReregisterRequest += async (reregIdx, clientId) =>
                    {
                        // 防止同一客户端重复重注册
                        await _reregLock.WaitAsync().ConfigureAwait(false);
                        bool skip;
                        try { skip = !_reregInFlight.Add(clientId); }
                        finally { _reregLock.Release(); }
                        if (skip) return;

                        try
                        {
                            var allClients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                            var rec = allClients.FirstOrDefault(c => c.ClientId == clientId);
                            if (rec == null) return;

                            var newRec = await SimulatorLib.Workers.HeartbeatWorker.ReregisterClientAsync(
                                rec, PlatformHost, PlatformPort, RegClientVersion,
                                _hbCts?.Token ?? CancellationToken.None).ConfigureAwait(false);
                            if (newRec == null) return;

                            // 更新 Clients.log
                            await _reregLock.WaitAsync().ConfigureAwait(false);
                            try
                            {
                                var latest  = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);
                                var updated = latest.Select(c => c.ClientId == clientId ? newRec : c).ToList();
                                await ClientsPersistence.WriteAllAsync(updated).ConfigureAwait(false);
                            }
                            finally { _reregLock.Release(); }

                            // 通知 NativeRunner 更新内存 DeviceId
                            capturedEngine?.SendDeviceIdUpdate(reregIdx, newRec.DeviceId);
                        }
                        finally
                        {
                            await _reregLock.WaitAsync().ConfigureAwait(false);
                            try { _reregInFlight.Remove(clientId); }
                            finally { _reregLock.Release(); }
                        }
                    };

                    // 策略下发接收：NativeRunner 收到 cmdId=17 时通知 C#，C# 通过 HTTPS 拉取策略并回包
                    var policyWorker = new SimulatorLib.Workers.PolicyReceiveWorker(PlatformHost, PlatformPort);
                    _processEngine.OnPolicyRequest += (policyIdx, clientId) =>
                    {
                        RunOnUi(() => PolicyReceived++);
                        _ = policyWorker.HandleTcpPolicyCmdAsync(17, clientId,
                                _hbCts?.Token ?? CancellationToken.None)
                            .ContinueWith(t =>
                            {
                                if (t.IsCompletedSuccessfully)
                                    RunOnUi(() => PolicyReplied = policyWorker.RepliedCount);
                            }, TaskScheduler.Default);
                    };

                    // 启动进程（使用已注册客户端的配置）
                    var firstClient = clients.First();
                    var startNum = int.Parse(firstClient.ClientId.Replace(RegPrefix, ""));
                    
                    // TcpPort 由注册时服务器返回（如 4575），与注册端口（如 8441）不同
                    int hbTcpPort = clients.FirstOrDefault(c => c.TcpPort > 0)?.TcpPort ?? PlatformPort;
                    // 计算威胁日志类型数（NativeRunner 按 0..typeCount-1 顺序发送：0=进程启动 1=注册表访问 2=文件访问）
                    // 日志线程由用户点击"添加任务"时通过 SendStartLog 按需启动，心跳启动时不启动日志

                    bool success = _processEngine.Start(
                        serverIP: PlatformHost,
                        serverPort: PlatformPort,
                        serverHBPort: hbTcpPort,
                        clientIDPrefix: RegPrefix,
                        clientStartIP: firstClient.IP,
                        clientStartNum: startNum,
                        clientCount: clients.Count,
                        hbInterval: HbInterval,
                        hbTotalMinutes: 0,   // 0 = 无限运行，由用户手动停止
                        logClientCount: 0,
                        logEachClientTotalItems: 0,
                        logEachClientPerSecondItems: 1,
                        logSelectedTypes: 0,   // 心跳启动时不启动日志线程
                        logHitEvery: 71);

                    if (!success)
                    {
                        RunOnUi(() => AppendStatus("⚠ ProcessEngine 启动失败"));
                        return;
                    }

                    string logDesc = "";
                    _hbTaskRec = hbTaskRec;  // 保存引用供 StopHeartbeat 调用 MarkStopped
                    RunOnUi(() => AppendStatus($"开始心跳任务（ProcessEngine 子进程模式，{clients.Count} 客户端，间隔 {HbInterval}ms{logDesc}）"));
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

            // 停止 ProcessEngine
            if (_processEngine != null)
            {
                // 同步停止日志任务（子进程退出后日志监控也应随之结束）
                StopLogSend();

                var engineToStop = _processEngine;
                var hbRec = _hbTaskRec;
                _processEngine = null;
                _hbTaskRec = null;
                Task.Run(() =>
                {
                    try { engineToStop.Stop(); } catch { }
                    try { engineToStop.Dispose(); } catch { }
                });
                RunOnUi(() =>
                {
                    hbRec?.MarkStopped();
                    AppendStatus("心跳子进程已停止");
                });
            }
        }

        /// <summary>关闭主窗口时调用，强制清理子进程</summary>
        public void Cleanup()
        {
            try { _hbCts?.Cancel(); } catch { }
            try { _httpsCts?.Cancel(); } catch { }
            try { _logCts?.Cancel(); } catch { }
            if (_processEngine != null)
            {
                try { _processEngine.Stop(); } catch { }
                try { _processEngine.Dispose(); } catch { }
                _processEngine = null;
                _hbTaskRec = null;
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
            RunOnUi(() => AppendStatus("连接测试..."));
            bool tcpOk = await TestTcpAsync(PlatformHost, PlatformPort, 1500);
            bool httpOk = await TestHttpAsync(PlatformHost, PlatformPort, 2000);
            
            RunOnUi(() => AppendStatus($"TCP {PlatformHost}:{PlatformPort} -> {(tcpOk ? "✓" : "✗")}  HTTPS -> {(httpOk ? "✓" : "✗")}"));
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

        private static string GetDomainNameSafe()
        {
            try { return Environment.UserDomainName ?? string.Empty; }
            catch { return string.Empty; }
        }

        private static byte[] TrimTrailingNewline(byte[] bytes)
        {
            if (bytes.Length >= 2 && bytes[^2] == (byte)'\r' && bytes[^1] == (byte)'\n')
                return bytes.AsSpan(0, bytes.Length - 2).ToArray();
            if (bytes.Length >= 1 && bytes[^1] == (byte)'\n')
                return bytes.AsSpan(0, bytes.Length - 1).ToArray();
            return bytes;
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
                        var httpsWorker = new LogWorker(new TcpSender(), new UdpSender(), null);
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
                    // 通过 stdin 发送 STARTLOG 命令启动 NativeRunner 子进程中的日志线程，
                    // 然后添加监控任务持续从 STATS 读取进度并更新 UI，直到用户点击【结束任务】。
                    if (threatCats.Length > 0 && LogThreatClientCount > 0)
                    {
                        var capturedEngine = _processEngine;
                        if (capturedEngine != null)
                        {
                            int availClients = capturedEngine.RegisteredTotal > 0
                                ? capturedEngine.RegisteredTotal : LogThreatClientCount;
                            int logClients = Math.Min(LogThreatClientCount, availClients);
                            capturedEngine.SendStartLog(
                                logClients,
                                LogThreatEps > 0 ? LogThreatEps : 1,
                                LogMessagesPerClient,
                                threatCats.Length,
                                LogThreatHitEvery > 0 ? LogThreatHitEvery : 71);
                            RunOnUi(() => AppendStatus("威胁检测日志正在子进程中运行，统计数据每2秒更新一次..."));
                            var capturedCts = _logCts!;
                            workerTasks.Add(Task.Run(async () =>
                            {
                                try
                                {
                                    while (!capturedCts.IsCancellationRequested)
                                    {
                                        threatOk   = capturedEngine.LogSendOk;
                                        threatFail = capturedEngine.LogSendFail;
                                        ReportCombined();
                                        await Task.Delay(2000, capturedCts.Token).ConfigureAwait(false);
                                    }
                                }
                                catch (OperationCanceledException) { }
                                // 最终统计
                                threatOk   = capturedEngine.LogSendOk;
                                threatFail = capturedEngine.LogSendFail;
                                ReportCombined();
                            }));
                        }
                        else
                        {
                            RunOnUi(() => AppendStatus("⚠ 子进程未运行，威胁日志无法统计（请先启动心跳）"));
                        }
                    }

                    try
                    {
                        await Task.WhenAll(workerTasks).ConfigureAwait(false);
                    }
                    finally
                    {
                        logTaskRec.MarkStopped();
                        _logCts?.Dispose();
                        _logCts = null;  // 任务自然结束后释放，允许下次直接添加任务
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
            _processEngine?.SendStopLog();  // 停止子进程中的日志线程
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

        private void PreviewWhitelist()
        {
            try
            {
                if (string.IsNullOrEmpty(WhitelistFilePath))
                {
                    AppendStatus("⚠ 请先选择白名单文件");
                    return;
                }

                if (!File.Exists(WhitelistFilePath))
                {
                    AppendStatus($"⚠ 文件不存在: {WhitelistFilePath}");
                    return;
                }

                var previewWindow = new Views.WhitelistPreviewWindow(WhitelistFilePath)
                {
                    Owner = Application.Current.MainWindow
                };
                previewWindow.ShowDialog();
            }
            catch (Exception ex)
            {
                AppendStatus($"预览白名单失败: {ex.Message}");
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
                var taskRec = AddTaskRecord("白名单上传", 0, 0);

                RunOnUi(() =>
                {
                    UploadTotal = 0;
                    UploadSuccess = 0;
                    UploadFailed = 0;
                    AppendStatus($"开始白名单上传（全量一次，对齐老工具）：文件={System.IO.Path.GetFileName(WhitelistFilePath)}" +
                                 $" 并发={WhitelistConcurrency}");
                });

                _ = Task.Run(async () =>
                {
                    try
                    {
                        // 读取所有已注册客户端（全量上传，对齐老工具逻辑）
                        var allClients = await ClientsPersistence.ReadAllAsync().ConfigureAwait(false);

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

                        await worker.RunAllOnceAsync(
                            filePath:     WhitelistFilePath,
                            clients:      allClients,
                            platformHost: PlatformHost,
                            platformPort: PlatformPort,
                            concurrency:  WhitelistConcurrency,
                            record:       taskRec,
                            ct:           _uploadCts.Token).ConfigureAwait(false);

                        RunOnUi(() => AppendStatus(
                            $"白名单上传结束: 成功={taskRec.SuccessCount} 失败={taskRec.FailCount} 状态={taskRec.StatusText}"));
                    }
                    catch (Exception ex)
                    {
                        taskRec.Detail = "异常: " + ex.Message;
                        taskRec.Status = SimulatorLib.Models.TaskStatus.Error;
                        RunOnUi(() => AppendStatus("白名单上传异常: " + ex.Message));
                    }
                    finally
                    {
                        _uploadCts?.Dispose();
                        _uploadCts = null;  // 任务自然结束后释放，允许下次直接添加任务
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
