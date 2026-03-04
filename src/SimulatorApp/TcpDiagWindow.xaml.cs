using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.NetworkInformation;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using Microsoft.Win32;
using Renci.SshNet;
using SimulatorLib.Persistence;

namespace SimulatorApp
{
    public partial class TcpDiagWindow : Window
    {
        private readonly bool _multiIpMode;
        private readonly int  _ipCount;
        private readonly ViewModels.MainViewModel? _vm;
        private string _sshOutputDir = "";

        public TcpDiagWindow(bool multiIpMode = false, string localIps = "", ViewModels.MainViewModel? vm = null)
        {
            InitializeComponent();
            _multiIpMode = multiIpMode;
            _ipCount = multiIpMode ? CountValidIps(localIps) : 1;
            _vm = vm;
            Loaded += Window_Loaded;
            RefreshData();
        }

        private static int CountValidIps(string localIps) =>
            localIps.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                     .Count(s => System.Net.IPAddress.TryParse(s, out _));

        private void RefreshData()
        {
            // ── 1. 读取系统 TCP 配置 ──────────────────────────────────────────
            int portStart = 49152;   // Windows Vista+ 默认
            int portCount = 16384;
            int timeWaitSec = -1;
            bool portRangeModified = false;
            bool timeWaitFromReg   = false;

            // 用 netsh show 读取实际生效的动态端口范围（比注册表更可靠）
            try
            {
                var psi = new ProcessStartInfo("netsh", "int ipv4 show dynamicport tcp")
                {
                    UseShellExecute        = false,
                    RedirectStandardOutput = true,
                    CreateNoWindow         = true,
                };
                using var proc = Process.Start(psi)!;
                string output = proc.StandardOutput.ReadToEnd();
                proc.WaitForExit(3000);

                // 解析输出（中英文系统均适用：取冒号后的数字，第1个=起始端口，第2个=数量）
                var numbers = new System.Collections.Generic.List<int>();
                foreach (var line in output.Split('\n'))
                {
                    var m = System.Text.RegularExpressions.Regex.Match(line, @":\s*(\d+)");
                    if (m.Success && int.TryParse(m.Groups[1].Value, out int v))
                        numbers.Add(v);
                }
                if (numbers.Count >= 2)
                {
                    portStart = numbers[0];
                    portCount = numbers[1];
                    portRangeModified = (portStart != 49152 || portCount != 16384);
                }
            }
            catch
            {
                // netsh 调用失败，保持默认值
            }

            // TIME_WAIT 超时从注册表读（重启后生效，注册表才是真实来源）
            try
            {
                using var key = Registry.LocalMachine.OpenSubKey(
                    @"SYSTEM\CurrentControlSet\Services\Tcpip\Parameters");
                if (key != null)
                {
                    var twDelay = key.GetValue("TcpTimedWaitDelay");
                    if (twDelay != null)
                    {
                        timeWaitSec    = Convert.ToInt32(twDelay);
                        timeWaitFromReg = true;
                    }
                }
            }
            catch
            {
                // 权限不足时忽略
            }

            string portRangeSrc = portRangeModified ? "已优化" : "系统默认";

            int portEnd           = portStart + portCount - 1;
            int effectiveTimeWait = timeWaitSec > 0 ? timeWaitSec : 120;
            string timeWaitSrc    = timeWaitFromReg ? "注册表已配置" : "未配置，使用系统默认 (~120s)";

            // ── 2. 实时采样当前 TCP 连接 ──────────────────────────────────────
            int cntEstablished = 0, cntTimeWait = 0, cntOther = 0;
            var usedLocalPorts  = new HashSet<int>();

            try
            {
                var props       = IPGlobalProperties.GetIPGlobalProperties();
                var connections = props.GetActiveTcpConnections();

                foreach (var conn in connections)
                {
                    switch (conn.State)
                    {
                        case TcpState.Established: cntEstablished++; break;
                        case TcpState.TimeWait:    cntTimeWait++;    break;
                        default:                   cntOther++;       break;
                    }

                    // 只统计落在动态端口范围内的本地端口
                    int lp = conn.LocalEndPoint.Port;
                    if (lp >= portStart && lp <= portEnd)
                        usedLocalPorts.Add(lp);
                }
            }
            catch
            {
                // 采样失败时各值保持为 0
            }

            int    portsUsed        = usedLocalPorts.Count;
            double portUsagePct     = portCount > 0 ? (double)portsUsed / portCount * 100.0 : 0;
            double theoreticalEps   = (double)portCount / effectiveTimeWait;

            // ── 3. 更新界面 ───────────────────────────────────────────────────
            TxtSampleTime.Text = DateTime.Now.ToString("HH:mm:ss");

            // 系统配置区
            TxtPortRange.Text = $"{portStart} – {portEnd}  (共 {portCount:N0} 个端口)  [{portRangeSrc}]";
            TxtTimeWait.Text  = timeWaitFromReg
                ? $"{timeWaitSec} 秒  [{timeWaitSrc}]"
                : timeWaitSrc;

            // 连接状态区
            TxtEstablished.Text  = cntEstablished.ToString("N0");
            TxtTimeWaitCount.Text = cntTimeWait.ToString("N0");
            TxtOtherCount.Text   = cntOther.ToString("N0");
            TxtPortsUsed.Text    = $"{portsUsed:N0} / {portCount:N0}";

            TxtPortUsageBar.Value  = Math.Min(portUsagePct, 100);
            TxtPortUsageLabel.Text = $"{portUsagePct:F1}%";

            // 理论 EPS 区
            if (_multiIpMode && _ipCount > 1)
            {
                double multiEps = theoreticalEps * _ipCount;
                TxtTheoreticalEps.Text  = $"{multiEps:N0} EPS";
                TxtFormula.Text         = $"{_ipCount} × ({portCount:N0} ÷ {effectiveTimeWait}s) = {multiEps:N0}";
                TxtMultiIpEps.Text      = $"单IP: {theoreticalEps:N0} EPS";
                TxtMultiIpEps.Visibility = Visibility.Visible;
                PanelMultiIp.Visibility  = Visibility.Visible;
                TxtMultiIpDesc.Text      = $"{_ipCount} 个 IP，上限扩大 {_ipCount} 倍";
                TxtMultiIpFormula.Text   = $"{_ipCount} × {theoreticalEps:N0} = {multiEps:N0} EPS";
            }
            else
            {
                TxtTheoreticalEps.Text  = $"{theoreticalEps:N0} EPS";
                TxtFormula.Text         = $"{portCount:N0} ÷ {effectiveTimeWait}s = {theoreticalEps:N0}";
                TxtMultiIpEps.Visibility = Visibility.Collapsed;
                PanelMultiIp.Visibility  = Visibility.Collapsed;
            }
            TxtCurrentUsagePct.Text = $"{portUsagePct:F1}%（端口池已占用）";
            TxtMultiHostTip.Text    = _multiIpMode
                ? $"当前已启用多IP模式（{_ipCount} 个IP）；端口上限已扩大 {_ipCount} 倍"
                : "若需多台主机：绑定 N 个 IP = 上限 × N；开启压测模式（长连接）可突破端口瓶颈";

            PopulateConnParams();
        }

        /// <summary>
        /// 刷新展示心跳 / 日志连接参数区域
        /// </summary>
        private void PopulateConnParams()
        {
            if (_vm == null)
            {
                ConnParamsPanel.Visibility = Visibility.Collapsed;
                return;
            }

            ConnParamsPanel.Visibility = Visibility.Visible;

            // 读取第一个已注册客户端的 TcpPort（心跳实际使用的端口）
            int hbTcpPort = _vm.PlatformPort; // fallback
            bool portFromReg = false;
            try
            {
                string filePath = ClientsPersistence.GetPath();
                if (File.Exists(filePath))
                {
                    foreach (var line in File.ReadLines(filePath))
                    {
                        if (string.IsNullOrWhiteSpace(line)) continue;
                        var rec = JsonSerializer.Deserialize<ClientRecord>(line);
                        if (rec?.TcpPort > 0)
                        {
                            hbTcpPort   = rec.TcpPort;
                            portFromReg = true;
                            break;
                        }
                    }
                }
            }
            catch { /* 读取失败时用 fallback */ }

            // 心跳 TCP
            TxtHbTarget.Text    = $"{_vm.PlatformHost}:{hbTcpPort}";
            TxtHbConnected.Text = $"{_vm.HbConnected:N0} / {_vm.HbTotal:N0} 个";
            TxtHbPortNote.Text  = portFromReg
                ? $"注册响应下发（心跳配置端口 {_vm.PlatformPort} 仅用于安全加速 / HTTPS）"
                : $"心跳配置端口（客户端尚未注册或注册响应无 tcpPort 字段）";

            // 日志 TCP
            if (_vm.UseLogServer)
            {
                TxtLogTcpMode.Text   = "独立日志服务器";
                TxtLogTcpTarget.Text = $"{_vm.LogHost}:{_vm.LogPort}";
            }
            else
            {
                TxtLogTcpMode.Text   = "借用心跳流（长连接 CMDID=21）";
                TxtLogTcpTarget.Text = $"{_vm.PlatformHost}:{hbTcpPort}";
            }

            // 日志 HTTPS
            TxtLogHttpsTarget.Text = $"{_vm.PlatformHost}:{_vm.PlatformPort}";
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => RefreshData();

        private void BtnClose_Click(object sender, RoutedEventArgs e) => Close();

        // ── 一键推荐设置：端口 1025–65535（64511个），TIME_WAIT=30s ──────────
        private async void BtnOneClickOptimize_Click(object sender, RoutedEventArgs e)
        {
            TxtEditPortStart.Text = "1025";
            TxtEditPortCount.Text = "64511";
            TxtEditTimeWait.Text  = "30";
            bool ok1 = ApplyPortRange(1025, 64511);
            bool ok2 = ApplyTimeWait(30);
            if (ok1 && ok2)
                ShowStatus("✅ 一键优化完成：端口 1025–65535（64511个），TIME_WAIT=30s。\nTIME_WAIT 设置重启后生效，端口范围立即生效。", success: true);
            if (ok1) await System.Threading.Tasks.Task.Delay(600);
            RefreshData();
        }

        private async void BtnApplyPortRange_Click(object sender, RoutedEventArgs e)
        {
            if (!int.TryParse(TxtEditPortStart.Text.Trim(), out int start) || start < 1025 || start > 60000)
            { ShowStatus("❌ 端口起始值无效（范围 1025–60000）", success: false); return; }
            if (!int.TryParse(TxtEditPortCount.Text.Trim(), out int count) || count < 1000 || start + count - 1 > 65535)
            { ShowStatus($"❌ 端口数量无效（至少 1000，且起始+数量≤65535）", success: false); return; }
            bool ok = ApplyPortRange(start, count);
            if (ok) await System.Threading.Tasks.Task.Delay(600);
            RefreshData();
        }

        private void BtnApplyTimeWait_Click(object sender, RoutedEventArgs e)
        {
            if (!int.TryParse(TxtEditTimeWait.Text.Trim(), out int tw) || tw < 30 || tw > 300)
            { ShowStatus("❌ TIME_WAIT 值无效（范围 30–300 秒）", success: false); return; }
            ApplyTimeWait(tw);
            RefreshData();
        }

        /// <summary>
        /// 调用 netsh 设置动态端口范围（立即生效，需管理员权限，会弹 UAC）
        /// </summary>
        private bool ApplyPortRange(int start, int count)
        {
            try
            {
                // netsh int ipv4 set dynamicport tcp start=X num=Y
                var psi = new ProcessStartInfo
                {
                    FileName        = "netsh",
                    Arguments       = $"int ipv4 set dynamicport tcp start={start} num={count}",
                    UseShellExecute = true,
                    Verb            = "runas",   // 请求 UAC 提权
                    WindowStyle     = ProcessWindowStyle.Hidden,
                    CreateNoWindow  = true,
                };
                using var proc = Process.Start(psi);
                proc?.WaitForExit(8000);
                int exit = proc?.ExitCode ?? -1;
                if (exit == 0)
                {
                    ShowStatus($"✅ 端口范围已设置：{start}–{start + count - 1}（共 {count:N0} 个），立即生效。", success: true);
                    return true;
                }
                else
                {
                    ShowStatus($"⚠ netsh 返回 {exit}，请以管理员身份手动运行：\nnetsh int ipv4 set dynamicport tcp start={start} num={count}", success: false);
                    return false;
                }
            }
            catch (Exception ex)
            {
                ShowStatus($"⚠ 无法执行 netsh（{ex.Message}）\n请以管理员身份手动运行：\nnetsh int ipv4 set dynamicport tcp start={start} num={count}", success: false);
                return false;
            }
        }

        /// <summary>
        /// 写注册表设置 TcpTimedWaitDelay（重启后生效，需管理员权限）
        /// </summary>
        private bool ApplyTimeWait(int seconds)
        {
            try
            {
                using var key = Registry.LocalMachine.OpenSubKey(
                    @"SYSTEM\CurrentControlSet\Services\Tcpip\Parameters", writable: true);
                if (key == null) throw new Exception("无法打开注册表项（权限不足）");
                key.SetValue("TcpTimedWaitDelay", seconds, RegistryValueKind.DWord);
                ShowStatus($"✅ TIME_WAIT 已写入注册表（{seconds}s），重启后生效。", success: true);
                return true;
            }
            catch (Exception ex)
            {
                ShowStatus($"⚠ 写入注册表失败（{ex.Message}）\n请以管理员身份运行，或手动修改：\nHKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\n  TcpTimedWaitDelay = {seconds}（DWORD）", success: false);
                return false;
            }
        }

        private void ShowStatus(string msg, bool success)
        {
            TxtOptimizeStatus.Text       = msg;
            TxtOptimizeStatus.Foreground = success
                ? System.Windows.Media.Brushes.DarkGreen
                : System.Windows.Media.Brushes.DarkOrange;
            TxtOptimizeStatus.Visibility = Visibility.Visible;
        }

        // ═════════════════════════════════════════════════════
        // SSH 日志收集
        // ═════════════════════════════════════════════════════

        private async void Window_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                var cfg = await AppConfig.LoadAsync().ConfigureAwait(true);
                TxtSshHost.Text    = !string.IsNullOrEmpty(cfg.PlatformHost) ? cfg.PlatformHost : "192.168.125.20";
                TxtSshPort.Text    = cfg.SshPort > 0 ? cfg.SshPort.ToString() : "22";
                TxtSshUser.Text    = !string.IsNullOrEmpty(cfg.SshUser) ? cfg.SshUser : "root";
                PbSshPassword.Password = cfg.SshPassword;
                TxtSshLogPath.Text = !string.IsNullOrEmpty(cfg.SshLogPath) ? cfg.SshLogPath : "/root/logs";
            }
            catch { /* 加载失败时保留默认值 */ }
        }

        private async void BtnCollectLogs_Click(object sender, RoutedEventArgs e)
        {
            string host          = TxtSshHost.Text.Trim();
            string user          = TxtSshUser.Text.Trim();
            string password      = PbSshPassword.Password;
            string remoteLogPath = TxtSshLogPath.Text.Trim();
            if (!int.TryParse(TxtSshPort.Text,    out int sshPort))    sshPort    = 22;
            if (!int.TryParse(TxtSshMinutes.Text, out int minutes)) minutes = 90;

            if (string.IsNullOrEmpty(host)) { AppendSshLog("❌ 请填写平台主机地址"); return; }
            if (string.IsNullOrEmpty(user)) { AppendSshLog("❌ 请填写 SSH 用户名");   return; }

            BtnCollectLogs.IsEnabled            = false;
            BtnOpenOutputDir.Visibility         = Visibility.Collapsed;
            TxtSshProgress.Text                 = "收集中…";
            TxtSshLog.Text                      = "";

            // 保存 SSH 配置
            try
            {
                var cfg = await AppConfig.LoadAsync().ConfigureAwait(true);
                cfg.SshPort     = sshPort;
                cfg.SshUser     = user;
                cfg.SshPassword = password;
                cfg.SshLogPath  = remoteLogPath;
                await AppConfig.SaveAsync(cfg).ConfigureAwait(true);
            }
            catch { /* non-fatal */ }

            var cutoff = DateTime.Now.AddMinutes(-minutes);
            _sshOutputDir = Path.Combine(AppContext.BaseDirectory,
                $"debug_collect_{DateTime.Now:yyyyMMdd_HHmmss}");
            Directory.CreateDirectory(_sshOutputDir);

            AppendSshLog($"输出目录：{_sshOutputDir}");
            AppendSshLog($"采集范围：{cutoff:HH:mm:ss} 之后再修改的文件");
            AppendSshLog("");

            int remoteCount = 0, localCount = 0;

            await Task.Run(() =>
            {
                // ── 1. SSH 收集平台日志 ──────────────────────────────
                try
                {
                    AppendSshLog($"正在连接 SSH {user}@{host}:{sshPort} …");
                    var authMethod = new PasswordAuthenticationMethod(user, password);
                    var connInfo   = new ConnectionInfo(host, sshPort, user, authMethod);
                    connInfo.Timeout = TimeSpan.FromSeconds(15);

                    using var sftp = new SftpClient(connInfo);
                    sftp.Connect();
                    AppendSshLog("✅ SSH 连接成功");

                    var remoteFiles = sftp.ListDirectory(remoteLogPath)
                        .Where(f => !f.IsDirectory && f.LastWriteTime >= cutoff)
                        .OrderBy(f => f.LastWriteTime)
                        .ToList();

                    AppendSshLog($"目录 {remoteLogPath}：共 {remoteFiles.Count} 个文件在范围内");

                    foreach (var f in remoteFiles)
                    {
                        var localFile = Path.Combine(_sshOutputDir, f.Name);
                        AppendSshLog($"  ↓ {f.Name}  ({f.Length / 1024.0:F1} KB)");
                        using var fs = File.OpenWrite(localFile);
                        sftp.DownloadFile(f.FullName, fs);
                        remoteCount++;
                    }

                    sftp.Disconnect();
                    AppendSshLog($"✅ 平台日志：{remoteCount} 个文件下载完成");
                }
                catch (Exception ex)
                {
                    AppendSshLog($"❌ SSH 错误：{ex.Message}");
                    AppendSshLog("提示：平台日志跳过，本地日志正常备份。");
                }
                AppendSshLog("");

                // ── 2. 备份本地日志 ────────────────────────────────
                AppendSshLog("本地调试日志备份…");

                // heartbeat_monitor 日志
                try
                {
                    foreach (var f in Directory.GetFiles(AppContext.BaseDirectory, "heartbeat_monitor_*.log")
                        .Where(f => File.GetCreationTime(f) >= cutoff))
                    {
                        var dst = Path.Combine(_sshOutputDir, Path.GetFileName(f));
                        File.Copy(f, dst, true);
                        AppendSshLog($"  ✓ local: {Path.GetFileName(f)}");
                        localCount++;
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ heartbeat log: {ex.Message}"); }

                // logsend-packets 日志
                try
                {
                    var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    if (Directory.Exists(logDir))
                    {
                        foreach (var f in Directory.GetFiles(logDir, "logsend-packets-*.log")
                            .Where(f => File.GetLastWriteTime(f) >= cutoff))
                        {
                            var dst = Path.Combine(_sshOutputDir, Path.GetFileName(f));
                            File.Copy(f, dst, true);
                            AppendSshLog($"  ✓ local: {Path.GetFileName(f)}");
                            localCount++;
                        }
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ logsend log: {ex.Message}"); }

                AppendSshLog($"✅ 本地日志：{localCount} 个文件");
            });

            TxtSshProgress.Text         = $"完成！平台 {remoteCount} 个  +  本地 {localCount} 个";
            BtnCollectLogs.IsEnabled    = true;
            BtnOpenOutputDir.Visibility = Visibility.Visible;
        }

        private void BtnOpenOutputDir_Click(object sender, RoutedEventArgs e)
        {
            if (!string.IsNullOrEmpty(_sshOutputDir) && Directory.Exists(_sshOutputDir))
                Process.Start(new ProcessStartInfo { FileName = _sshOutputDir, UseShellExecute = true });
        }

        private void AppendSshLog(string text)
        {
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(() => AppendSshLog(text)));
                return;
            }
            TxtSshLog.Text += text + "\n";
            SshLogScroll.ScrollToEnd();
        }    }
}