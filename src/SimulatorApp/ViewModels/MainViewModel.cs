using System;
using System.ComponentModel;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Input;
using SimulatorLib.Network;
using SimulatorLib.Workers;
using SimulatorLib.Persistence;
using System.Threading.Tasks;

namespace SimulatorApp.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        private string _platformHost = "localhost";
        private int _platformPort = 8441;
        private string _logHost = "localhost";
        private int _logPort = 4565;
        private string _regPrefix = "Client-";
        private int _regStart = 1;
        private int _regCount = 5;
        private int _hbInterval = 1000;
        private string _statusLog = string.Empty;

        private CancellationTokenSource? _hbCts;

        public string PlatformHost { get => _platformHost; set { _platformHost = value; OnProp(); } }
        public int PlatformPort { get => _platformPort; set { _platformPort = value; OnProp(); } }
        public string LogHost { get => _logHost; set { _logHost = value; OnProp(); } }
        public int LogPort { get => _logPort; set { _logPort = value; OnProp(); } }
        public string RegPrefix { get => _regPrefix; set { _regPrefix = value; OnProp(); } }
        public int RegStart { get => _regStart; set { _regStart = value; OnProp(); } }
        public int RegCount { get => _regCount; set { _regCount = value; OnProp(); } }
        public int HbInterval { get => _hbInterval; set { _hbInterval = value; OnProp(); } }

        public string StatusLog { get => _statusLog; set { _statusLog = value; OnProp(); } }

        public ICommand RegisterCommand { get; }
        public ICommand StartHeartbeatCommand { get; }
        public ICommand StopHeartbeatCommand { get; }
        public ICommand PortTestCommand { get; }

        public MainViewModel()
        {
            RegisterCommand = new RelayCommand(async _ => await RegisterAsync());
            StartHeartbeatCommand = new RelayCommand(async _ => await StartHeartbeatAsync());
            StopHeartbeatCommand = new RelayCommand(_ => StopHeartbeat());
            PortTestCommand = new RelayCommand(async _ => await PortTestAsync());
            _ = LoadConfigAsync();
        }

        private void OnProp([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        private async Task LoadConfigAsync()
        {
            var cfg = await AppConfig.LoadAsync().ConfigureAwait(false);
            PlatformHost = cfg.PlatformHost;
            PlatformPort = cfg.PlatformPort;
            LogHost = cfg.LogHost;
            LogPort = cfg.LogPort;
            RegPrefix = cfg.RegClientPrefix;
            RegStart = cfg.RegStartIndex;
            RegCount = cfg.RegCount;
            HbInterval = cfg.HeartbeatIntervalMs;
            AppendStatus("配置已加载");
        }

        private async Task SaveConfigAsync()
        {
            var cfg = new AppConfig
            {
                PlatformHost = PlatformHost,
                PlatformPort = PlatformPort,
                LogHost = LogHost,
                LogPort = LogPort,
                RegClientPrefix = RegPrefix,
                RegStartIndex = RegStart,
                RegCount = RegCount,
                HeartbeatIntervalMs = HbInterval
            };
            await AppConfig.SaveAsync(cfg).ConfigureAwait(false);
            AppendStatus("配置已保存");
        }

        private void AppendStatus(string text)
        {
            StatusLog += $"[{DateTime.Now:HH:mm:ss}] {text}\r\n";
        }

        private async Task RegisterAsync()
        {
            try
            {
                await SaveConfigAsync().ConfigureAwait(false);
                var sender = new TcpSender();
                var reg = new RegistrationWorker(sender);
                AppendStatus($"开始注册 {RegCount} 个客户端...");
                await reg.RegisterAsync(RegPrefix, RegStart, RegCount, host: PlatformHost, port: PlatformPort, concurrency: 4, retry: 3, timeoutMs: 1500).ConfigureAwait(false);
                AppendStatus("注册任务完成");
            }
            catch (Exception ex)
            {
                AppendStatus("注册异常: " + ex.Message);
            }
            OnProp(nameof(StatusLog));
        }

        private async Task StartHeartbeatAsync()
        {
            try
            {
                await SaveConfigAsync().ConfigureAwait(false);
                _hbCts = new CancellationTokenSource();
                var tcp = new TcpSender();
                var udp = new UdpSender();
                var hb = new HeartbeatWorker(tcp, udp);
                AppendStatus("开始心跳任务...");
                _ = Task.Run(async () =>
                {
                    await hb.StartAsync(HbInterval, useLogServer: true, platformHost: PlatformHost, platformPort: PlatformPort, logHost: LogHost, logPort: LogPort, concurrency: 6, ct: _hbCts.Token).ConfigureAwait(false);
                });
            }
            catch (Exception ex)
            {
                AppendStatus("心跳启动异常: " + ex.Message);
            }
            OnProp(nameof(StatusLog));
        }

        private void StopHeartbeat()
        {
            if (_hbCts != null && !_hbCts.IsCancellationRequested)
            {
                _hbCts.Cancel();
                AppendStatus("已请求停止心跳任务");
                OnProp(nameof(StatusLog));
            }
        }

        private async Task PortTestAsync()
        {
            AppendStatus("开始端口连通测试...");
            bool tcpOk = await TestTcpAsync(PlatformHost, PlatformPort, 1500);
            AppendStatus($"TCP {PlatformHost}:{PlatformPort} -> {(tcpOk ? "OK" : "FAIL")}");
            bool udpOk = await TestUdpAsync(PlatformHost, PlatformPort, 1000);
            AppendStatus($"UDP {PlatformHost}:{PlatformPort} -> {(udpOk ? "OK" : "WARN/NO-ACK")}");
            bool httpOk = await TestHttpAsync(PlatformHost, PlatformPort, 2000);
            AppendStatus($"HTTP http://{PlatformHost}:{PlatformPort}/ -> {(httpOk ? "OK" : "FAIL")}");
            OnProp(nameof(StatusLog));
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
