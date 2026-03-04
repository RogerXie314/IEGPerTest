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
        private SshLogWindow? _sshLogWindow;

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
                TxtSshHost.Text        = !string.IsNullOrEmpty(cfg.PlatformHost) ? cfg.PlatformHost : "192.168.125.20";
                TxtSshPort.Text        = cfg.SshPort > 0 ? cfg.SshPort.ToString() : "22";
                TxtSshUser.Text        = !string.IsNullOrEmpty(cfg.SshUser) ? cfg.SshUser : "sysadmin";
                PbSshPassword.Password = cfg.SshPassword;
                TxtSshLogPath.Text     = !string.IsNullOrEmpty(cfg.SshLogPath) ? cfg.SshLogPath : "/root/logs";
                TxtSshThreshold.Text   = cfg.SshSizeThresholdMb > 0 ? cfg.SshSizeThresholdMb.ToString() : "50";
            }
            catch { /* 加载失败时保留默认值 */ }
        }

        private async void BtnCollectLogs_Click(object sender, RoutedEventArgs e)
        {
            string host          = TxtSshHost.Text.Trim();
            string user          = TxtSshUser.Text.Trim();
            string password      = PbSshPassword.Password;
            string remoteLogPath = TxtSshLogPath.Text.Trim();
            if (!int.TryParse(TxtSshPort.Text,      out int sshPort))  sshPort  = 22;
            if (!int.TryParse(TxtSshMinutes.Text,   out int minutes))  minutes  = 90;
            if (!int.TryParse(TxtSshThreshold.Text, out int threshMb)) threshMb = 50;
            long threshBytes = (long)threshMb * 1024 * 1024;

            if (string.IsNullOrEmpty(host))     { AppendSshLog("❌ 请填写平台主机地址"); return; }
            if (string.IsNullOrEmpty(user))     { AppendSshLog("❌ 请填写 SSH 用户名");   return; }
            if (string.IsNullOrEmpty(password)) { AppendSshLog("❌ 请填写密码（sudo 提权需要密码）"); return; }

            BtnCollectLogs.IsEnabled    = false;
            BtnOpenOutputDir.Visibility = Visibility.Collapsed;
            TxtSshProgress.Text         = "收集中…";
            TxtTimeSkew.Text            = "检测中…";
            TxtTimeSkew.Foreground      = System.Windows.Media.Brushes.DarkOrange;

            // 打开 / 显示 SSH 日志子窗口
            if (_sshLogWindow == null || !_sshLogWindow.IsLoaded)
                _sshLogWindow = new SshLogWindow { Owner = this };
            _sshLogWindow.Clear();
            _sshLogWindow.SetStatus("收集中…");
            _sshLogWindow.SetOutputDir("");
            _sshLogWindow.ShowAndActivate();

            // 保存配置
            try
            {
                var cfg = await AppConfig.LoadAsync().ConfigureAwait(true);
                cfg.SshPort            = sshPort;
                cfg.SshUser            = user;
                cfg.SshPassword        = password;
                cfg.SshLogPath         = remoteLogPath;
                cfg.SshSizeThresholdMb = threshMb;
                await AppConfig.SaveAsync(cfg).ConfigureAwait(true);
            }
            catch { /* non-fatal */ }

            var localNow    = DateTime.Now;
            var localCutoff = localNow.AddMinutes(-minutes);
            _sshOutputDir   = Path.Combine(AppContext.BaseDirectory, $"debug_collect_{localNow:yyyyMMdd_HHmmss}");
            Directory.CreateDirectory(_sshOutputDir);

            AppendSshLog($"输出目录：{_sshOutputDir}");
            AppendSshLog($"本机截止时间：{localCutoff:yyyy-MM-dd HH:mm:ss}");
            AppendSshLog("");

            int remoteCount = 0, localCount = 0;
            long offsetSec = 0;

            await Task.Run(() =>
            {
                // ── 1. SSH 收集平台日志 ────────────────────────────────────────
                try
                {
                    AppendSshLog($"正在连接 {user}@{host}:{sshPort} …");
                    var auth     = new PasswordAuthenticationMethod(user, password);
                    var connInfo = new ConnectionInfo(host, sshPort, user, auth) { Timeout = TimeSpan.FromSeconds(15) };

                    using var ssh  = new SshClient(connInfo);
                    using var sftp = new SftpClient(connInfo);
                    ssh.Connect();
                    AppendSshLog("✅ SSH 连接成功");

                    // ── 1a. 检测时间偏差（RTT 半程补偿）──────────────────────
                    long t0 = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                    string remoteEpochStr = SshRun(ssh, "date +%s").Trim();
                    long t1 = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                    string skewDesc = "（检测失败）";
                    var skewFg = System.Windows.Media.Brushes.Gray;
                    if (long.TryParse(remoteEpochStr, out long remoteEpoch))
                    {
                        long localEpoch = (t0 + t1) / 2;
                        offsetSec = localEpoch - remoteEpoch;  // 正 = 本机超前平台
                        skewDesc = offsetSec == 0 ? "无偏差"
                            : $"{(offsetSec > 0 ? "+" : "")}{offsetSec}s（本机{(offsetSec > 0 ? "超前" : "落后")}平台）";
                        skewFg = Math.Abs(offsetSec) > 30
                            ? System.Windows.Media.Brushes.DarkRed
                            : System.Windows.Media.Brushes.DarkGreen;
                        AppendSshLog($"⏱ 时间偏差：{skewDesc}");
                    }
                    var capturedFg = skewFg;
                    Dispatcher.BeginInvoke(new Action(() =>
                    {
                        TxtTimeSkew.Text       = skewDesc;
                        TxtTimeSkew.Foreground = capturedFg;
                    }));

                    // ── 1b. 切换 root（密码与 SSH 登录密码相同）──────────────
                    AppendSshLog("正在切换 root 账户…");
                    string whoamiOut = SshRunAsRoot(ssh, password, "whoami").Trim();
                    if (whoamiOut == "root")
                    {
                        AppendSshLog("✅ 已切换到 root 账户");
                    }
                    else
                    {
                        AppendSshLog($"⚠ su root 返回：{whoamiOut}（将继续尝试，若命令失败请确认密码）");
                    }
                    AppendSshLog("");

                    // ── 1c. 计算平台侧截止时间（修正偏差）─────────────────────
                    // 平台时间 = 本机时间 - offsetSec
                    var remoteCutoff       = localCutoff.AddSeconds(-offsetSec);
                    long remoteCutoffEpoch = new DateTimeOffset(remoteCutoff).ToUnixTimeSeconds();
                    string remoteCutoffStr = remoteCutoff.ToString("yyyy-MM-dd HH:mm:ss");
                    AppendSshLog($"平台截止：{remoteCutoffStr}");
                    AppendSshLog("");

                    // ── 1d. 建临时目录，find -newer 定位范围内的文件 ────────
                    string tmpBase = $"/tmp/dbg_{localNow:yyyyMMddHHmmss}";
                    SshRunAsRoot(ssh, password, $"mkdir -p {tmpBase}");
                    SshRunAsRoot(ssh, password, $"touch -d @{remoteCutoffEpoch} {tmpBase}/.ts_marker");
                    string findOut = SshRunAsRoot(ssh, password,
                        $"find {remoteLogPath} -maxdepth 1 -type f -newer {tmpBase}/.ts_marker 2>/dev/null");

                    var remoteFiles = findOut.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                        .Select(s => s.Trim()).Where(s => s.StartsWith("/"))
                        .OrderBy(s => s).ToList();

                    AppendSshLog($"目录 {remoteLogPath}：{remoteFiles.Count} 个文件在时间范围内");

                    // ── 1e. 逐文件：小文件直接复制，大文件 awk 过滤 ────────
                    sftp.Connect();
                    foreach (var remoteFile in remoteFiles)
                    {
                        string fname   = Path.GetFileName(remoteFile);
                        string tmpFile = $"{tmpBase}/{fname}";

                        long fileSize = 0;
                        string wcOut = SshRunAsRoot(ssh, password, $"wc -c < {remoteFile} 2>/dev/null").Trim();
                        long.TryParse(wcOut.Split(' ', StringSplitOptions.RemoveEmptyEntries)[0], out fileSize);

                        if (fileSize <= threshBytes)
                        {
                            // 小文件：root cp → chmod 644 → SFTP 下载
                            AppendSshLog($"  ↓ {fname}  ({fileSize / 1024.0:F0} KB) 直接下载");
                            SshRunAsRoot(ssh, password, $"cp {remoteFile} {tmpFile} && chmod 644 {tmpFile}");
                        }
                        else
                        {
                            // 大文件：root awk 按时间戳过滤（适配 [YYYY-MM-DD HH:MM:SS:ms] 格式）
                            AppendSshLog($"  🔍 {fname}  ({fileSize / 1024.0 / 1024.0:F1} MB) 服务端过滤 ≥ {remoteCutoffStr}");
                            // awk: 遇到 [YYYY-MM-DD 开头行提取前19字符为时间戳；在范围内则打印（包括后续续行）
                            string awkCmd = $"awk -v cutoff='{remoteCutoffStr}' " +
                                @"'/^\[20[0-9][0-9]-[0-9][0-9]-/{ts=substr($0,2,19); keep=(ts>=cutoff)} keep{print}' " +
                                $"{remoteFile} > {tmpFile} && chmod 644 {tmpFile}";
                            SshRunAsRoot(ssh, password, awkCmd);
                        }

                        // 通过 SFTP 下载 /tmp 中的文件
                        try
                        {
                            var localDst = Path.Combine(_sshOutputDir, fname);
                            using var fs = File.OpenWrite(localDst);
                            sftp.DownloadFile(tmpFile, fs);
                            long localSize = new FileInfo(localDst).Length;
                            AppendSshLog($"     → 已下载 {localSize / 1024.0:F0} KB");
                            remoteCount++;
                        }
                        catch (Exception exDl) { AppendSshLog($"     ⚠ 下载失败: {exDl.Message}"); }
                    }

                    sftp.Disconnect();

                    // ── 1f. 清理临时目录 ──────────────────────────────────
                    SshRunAsRoot(ssh, password, $"rm -rf {tmpBase}");
                    ssh.Disconnect();
                    AppendSshLog($"\n✅ 平台日志收集完成：{remoteCount} 个文件");
                }
                catch (Exception ex)
                {
                    AppendSshLog($"❌ SSH 异常：{ex.Message}");
                    AppendSshLog("提示：平台日志已跳过，本地日志正常备份。");
                    Dispatcher.BeginInvoke(new Action(() =>
                    {
                        TxtTimeSkew.Text       = "连接失败";
                        TxtTimeSkew.Foreground = System.Windows.Media.Brushes.DarkRed;
                    }));
                }

                AppendSshLog("");

                // ── 2. 备份本地调试日志（按本机时间）──────────────────────────
                AppendSshLog("本地调试日志备份…");
                try
                {
                    foreach (var f in Directory.GetFiles(AppContext.BaseDirectory, "heartbeat_monitor_*.log")
                        .Where(f => File.GetLastWriteTime(f) >= localCutoff))
                    {
                        File.Copy(f, Path.Combine(_sshOutputDir, Path.GetFileName(f)), true);
                        AppendSshLog($"  ✓ {Path.GetFileName(f)}");
                        localCount++;
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ heartbeat log: {ex.Message}"); }

                try
                {
                    var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    if (Directory.Exists(logDir))
                        foreach (var f in Directory.GetFiles(logDir, "logsend-packets-*.log")
                            .Where(f => File.GetLastWriteTime(f) >= localCutoff))
                        {
                            File.Copy(f, Path.Combine(_sshOutputDir, Path.GetFileName(f)), true);
                            AppendSshLog($"  ✓ {Path.GetFileName(f)}");
                            localCount++;
                        }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ logsend log: {ex.Message}"); }

                // ── 3. 写时间偏差说明文件 ───────────────────────────────────
                try
                {
                    string note = "# 日志收集说明\n" +
                        $"收集时刻（本机）: {localNow:yyyy-MM-dd HH:mm:ss}\n" +
                        $"本机截止时间:     {localCutoff:yyyy-MM-dd HH:mm:ss}\n" +
                        $"平台时间偏差:     {(offsetSec >= 0 ? "+" : "")}{offsetSec} 秒（本机 - 平台）\n" +
                        $"平台侧截止时间:   {localCutoff.AddSeconds(-offsetSec):yyyy-MM-dd HH:mm:ss}\n\n" +
                        "分析时间线时请注意：平台日志时间戳 + 偏差秒数 ≈ 本机时间。\n";
                    File.WriteAllText(Path.Combine(_sshOutputDir, "_时间偏差说明.txt"), note,
                        System.Text.Encoding.UTF8);
                }
                catch { /* non-fatal */ }

                AppendSshLog($"✅ 本地日志：{localCount} 个文件");
            });

            TxtSshProgress.Text         = $"完成！平台 {remoteCount} 个  +  本地 {localCount} 个";
            BtnCollectLogs.IsEnabled    = true;
            BtnOpenOutputDir.Visibility = Visibility.Visible;
            _sshLogWindow?.SetStatus($"完成！平台 {remoteCount} 个 + 本地 {localCount} 个");
            _sshLogWindow?.SetOutputDir(_sshOutputDir);
        }

        /// <summary>通过 SSH exec channel 运行命令，返回 stdout</summary>
        private static string SshRun(SshClient ssh, string command)
        {
            using var cmd = ssh.RunCommand(command);
            return cmd.Result;
        }

        /// <summary>通过 echo PASSWORD | sudo -S bash -c '...' 运行需要提权的命令，返回 stdout+stderr</summary>
        private static string SshRunSudo(SshClient ssh, string password, string command)
        {
            string cmdLine = $"echo {ShellQuote(password)} | sudo -S bash -c {ShellQuote(command)} 2>&1";
            using var cmd  = ssh.RunCommand(cmdLine);
            return cmd.Result;
        }

        /// <summary>通过 echo PASSWORD | su root -c '...' 切换 root 执行命令，返回 stdout+stderr</summary>
        private static string SshRunAsRoot(SshClient ssh, string password, string command)
        {
            string cmdLine = $"echo {ShellQuote(password)} | su root -c {ShellQuote(command)} 2>&1";
            using var cmd  = ssh.RunCommand(cmdLine);
            return cmd.Result;
        }

        /// <summary>Shell 单引号安全转义：包裹在 ' 中，将内部的 ' 转义为 '\'  '</summary>
        private static string ShellQuote(string s)
            => "'" + s.Replace("'", "'\\''") + "'";

        private void BtnOpenOutputDir_Click(object sender, RoutedEventArgs e)
        {
            if (!string.IsNullOrEmpty(_sshOutputDir) && Directory.Exists(_sshOutputDir))
                Process.Start(new ProcessStartInfo { FileName = _sshOutputDir, UseShellExecute = true });
        }

        private void AppendSshLog(string text)
        {
            _sshLogWindow?.AppendLine(text);
        }    }
}