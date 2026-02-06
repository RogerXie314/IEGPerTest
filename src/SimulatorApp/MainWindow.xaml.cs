using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using SimulatorLib.Network;
using SimulatorLib.Workers;

namespace SimulatorApp
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource? _hbCts;

        public MainWindow()
        {
            InitializeComponent();
            _ = LoadConfigAsync();
        }

        private async System.Threading.Tasks.Task LoadConfigAsync()
        {
            try
            {
                var cfg = await SimulatorLib.Persistence.AppConfig.LoadAsync().ConfigureAwait(false);
                // UI thread update
                TxtPlatformHost.Dispatcher.Invoke(() => TxtPlatformHost.Text = cfg.PlatformHost);
                TxtPlatformPort.Dispatcher.Invoke(() => TxtPlatformPort.Text = cfg.PlatformPort.ToString());
                TxtLogHost.Dispatcher.Invoke(() => TxtLogHost.Text = cfg.LogHost);
                TxtLogPort.Dispatcher.Invoke(() => TxtLogPort.Text = cfg.LogPort.ToString());

                TxtRegPrefix.Dispatcher.Invoke(() => TxtRegPrefix.Text = cfg.RegClientPrefix);
                TxtRegStart.Dispatcher.Invoke(() => TxtRegStart.Text = cfg.RegStartIndex.ToString());
                TxtRegCount.Dispatcher.Invoke(() => TxtRegCount.Text = cfg.RegCount.ToString());

                TxtHbInterval.Dispatcher.Invoke(() => TxtHbInterval.Text = cfg.HeartbeatIntervalMs.ToString());
                AppendStatus("配置已加载");
            }
            catch (Exception ex)
            {
                AppendStatus("加载配置失败: " + ex.Message);
            }
        }

        private async System.Threading.Tasks.Task SaveConfigAsync()
        {
            try
            {
                var cfg = new SimulatorLib.Persistence.AppConfig();
                _ = TxtPlatformHost.Dispatcher.Invoke(() => cfg.PlatformHost = TxtPlatformHost.Text);
                _ = TxtPlatformPort.Dispatcher.Invoke(() => cfg.PlatformPort = int.TryParse(TxtPlatformPort.Text, out var p) ? p : 8441);
                _ = TxtLogHost.Dispatcher.Invoke(() => cfg.LogHost = TxtLogHost.Text);
                _ = TxtLogPort.Dispatcher.Invoke(() => cfg.LogPort = int.TryParse(TxtLogPort.Text, out var lp) ? lp : 4565);

                _ = TxtRegPrefix.Dispatcher.Invoke(() => cfg.RegClientPrefix = TxtRegPrefix.Text);
                _ = TxtRegStart.Dispatcher.Invoke(() => cfg.RegStartIndex = int.TryParse(TxtRegStart.Text, out var rs) ? rs : 1);
                _ = TxtRegCount.Dispatcher.Invoke(() => cfg.RegCount = int.TryParse(TxtRegCount.Text, out var rc) ? rc : 5);

                _ = TxtHbInterval.Dispatcher.Invoke(() => cfg.HeartbeatIntervalMs = int.TryParse(TxtHbInterval.Text, out var hi) ? hi : 1000);

                await SimulatorLib.Persistence.AppConfig.SaveAsync(cfg).ConfigureAwait(false);
                AppendStatus("配置已保存");
            }
            catch (Exception ex)
            {
                AppendStatus("保存配置失败: " + ex.Message);
            }
        }

        private async void BtnRegister_Click(object sender, RoutedEventArgs e)
        {
            BtnRegister.IsEnabled = false;
            AppendStatus("开始并发注册 5 个客户端（UI 触发）...");
            var senderNet = new TcpSender();
            var reg = new RegistrationWorker(senderNet);
            try
            {
                // save config then use values
                await SaveConfigAsync().ConfigureAwait(false);
                var host = TxtPlatformHost.Dispatcher.Invoke(() => TxtPlatformHost.Text);
                var port = int.TryParse(TxtPlatformPort.Dispatcher.Invoke(() => TxtPlatformPort.Text), out var pp) ? pp : 8441;
                var prefix = TxtRegPrefix.Dispatcher.Invoke(() => TxtRegPrefix.Text);
                var start = int.TryParse(TxtRegStart.Dispatcher.Invoke(() => TxtRegStart.Text), out var si) ? si : 100;
                var count = int.TryParse(TxtRegCount.Dispatcher.Invoke(() => TxtRegCount.Text), out var c) ? c : 5;
                await reg.RegisterAsync(prefix, start, count, host: host, port: port, concurrency: 3, retry: 2, timeoutMs: 1500).ConfigureAwait(false);
                AppendStatus("注册任务完成，已写入 Clients.log（如果成功）");
            }
            catch (Exception ex)
            {
                AppendStatus("注册失败: " + ex.Message);
            }
            finally
            {
                BtnRegister.IsEnabled = true;
            }
        }

        private async void BtnStartHb_Click(object sender, RoutedEventArgs e)
        {
            BtnStartHb.IsEnabled = false;
            BtnStopHb.IsEnabled = true;
            AppendStatus("开始心跳任务（UI 触发）...");

            _hbCts = new CancellationTokenSource();
            var tcp = new TcpSender();
            var udp = new UdpSender();
            var hb = new HeartbeatWorker(tcp, udp);

            try
            {
                // Run heartbeat loop in background
                await SaveConfigAsync().ConfigureAwait(false);
                var host = TxtPlatformHost.Dispatcher.Invoke(() => TxtPlatformHost.Text);
                var port = int.TryParse(TxtPlatformPort.Dispatcher.Invoke(() => TxtPlatformPort.Text), out var pp) ? pp : 8441;
                var logHost = TxtLogHost.Dispatcher.Invoke(() => TxtLogHost.Text);
                var logPort = int.TryParse(TxtLogPort.Dispatcher.Invoke(() => TxtLogPort.Text), out var lp) ? lp : 4565;
                var interval = int.TryParse(TxtHbInterval.Dispatcher.Invoke(() => TxtHbInterval.Text), out var hi) ? hi : 1000;

                _ = Task.Run(async () =>
                {
                    await hb.StartAsync(interval, useLogServer: true, platformHost: host, platformPort: port, logHost: logHost, logPort: logPort, concurrency: 5, ct: _hbCts.Token);
                });
            }
            catch (Exception ex)
            {
                AppendStatus("心跳启动异常: " + ex.Message);
                BtnStartHb.IsEnabled = true;
                BtnStopHb.IsEnabled = false;
            }
        }

        private void BtnStopHb_Click(object sender, RoutedEventArgs e)
        {
            if (_hbCts != null && !_hbCts.IsCancellationRequested)
            {
                _hbCts.Cancel();
                AppendStatus("已请求停止心跳任务。");
            }
            BtnStartHb.IsEnabled = true;
            BtnStopHb.IsEnabled = false;
        }

        private void AppendStatus(string text)
        {
            var line = $"[{DateTime.Now:HH:mm:ss}] {text}" + Environment.NewLine;
            TxtStatus.Dispatcher.Invoke(() => TxtStatus.AppendText(line));
        }

        private async void BtnPortTest_Click(object sender, RoutedEventArgs e)
        {
            AppendStatus("开始端口连通测试...");
            var host = TxtPlatformHost.Dispatcher.Invoke(() => TxtPlatformHost.Text);
            var port = int.TryParse(TxtPlatformPort.Dispatcher.Invoke(() => TxtPlatformPort.Text), out var pp) ? pp : 8441;

            var results = new System.Text.StringBuilder();
            var tcpOk = await TestTcpAsync(host, port, 1500);
            results.AppendLine($"TCP {host}:{port} -> {(tcpOk ? "OK" : "FAIL")}");

            var udpOk = await TestUdpAsync(host, port, 1000);
            results.AppendLine($"UDP {host}:{port} -> {(udpOk ? "OK" : "WARN/NO-ACK")}");

            var httpOk = await TestHttpAsync(host, port, 2000);
            results.AppendLine($"HTTP http://{host}:{port}/ -> {(httpOk ? "OK" : "FAIL")}");

            AppendStatus(results.ToString());
        }

        private async System.Threading.Tasks.Task<bool> TestTcpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var tcp = new System.Net.Sockets.TcpClient();
                var t = tcp.ConnectAsync(host, port);
                var task = await System.Threading.Tasks.Task.WhenAny(t, System.Threading.Tasks.Task.Delay(timeoutMs));
                if (task != t) return false;
                return tcp.Connected;
            }
            catch { return false; }
        }

        private async System.Threading.Tasks.Task<bool> TestUdpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var udp = new System.Net.Sockets.UdpClient();
                var addrs = System.Net.Dns.GetHostAddresses(host);
                if (addrs == null || addrs.Length == 0) return false;
                var endpoint = new System.Net.IPEndPoint(addrs[0], port);
                var data = System.Text.Encoding.UTF8.GetBytes("PING");
                await udp.SendAsync(data, data.Length, endpoint);
                return true; // UDP is connectionless: treat send success as best-effort
            }
            catch { return false; }
        }

        private async System.Threading.Tasks.Task<bool> TestHttpAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var c = new System.Net.Http.HttpClient { Timeout = TimeSpan.FromMilliseconds(timeoutMs) };
                var url = $"http://{host}:{port}/";
                var resp = await c.GetAsync(url);
                return resp.IsSuccessStatusCode;
            }
            catch { return false; }
        }
    }
}

