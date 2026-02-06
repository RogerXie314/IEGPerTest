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
        }

        private async void BtnRegister_Click(object sender, RoutedEventArgs e)
        {
            BtnRegister.IsEnabled = false;
            AppendStatus("开始并发注册 5 个客户端（UI 触发）...");
            var senderNet = new TcpSender();
            var reg = new RegistrationWorker(senderNet);
            try
            {
                await reg.RegisterAsync("UIClient-", 100, 5, host: "localhost", port: 8441, concurrency: 3, retry: 2, timeoutMs: 1500);
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
                _ = Task.Run(async () =>
                {
                    await hb.StartAsync(1000, useLogServer: true, platformHost: "localhost", platformPort: 8441, logHost: "localhost", logPort: 4565, concurrency: 5, ct: _hbCts.Token);
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
    }
}

