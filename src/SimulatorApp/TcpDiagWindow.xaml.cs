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
        private PortAdvancedWindow? _portAdvWindow;

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

            // ── 3. 更新界面 ───────────────────────────────────────────────────
            TxtSampleTime.Text = DateTime.Now.ToString("HH:mm:ss");


            // 连接状态区
            TxtEstablished.Text  = cntEstablished.ToString("N0");
            TxtTimeWaitCount.Text = cntTimeWait.ToString("N0");
            TxtOtherCount.Text   = cntOther.ToString("N0");
            TxtPortsUsed.Text    = $"{portsUsed:N0} / {portCount:N0}";

            TxtPortUsageBar.Value  = Math.Min(portUsagePct, 100);
            TxtPortUsageLabel.Text = $"{portUsagePct:F1}%";

            // 如果子窗口已开启，一并刷新
            if (_portAdvWindow?.IsVisible == true) _portAdvWindow.RefreshFromParent();

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

        private void BtnPortAdvanced_Click(object sender, RoutedEventArgs e)
        {
            if (_portAdvWindow == null || !_portAdvWindow.IsLoaded)
            {
                _portAdvWindow = new PortAdvancedWindow(_multiIpMode, _ipCount) { Owner = this };
                _portAdvWindow.Show();
            }
            else
            {
                _portAdvWindow.Activate();
            }
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
                TxtSshLogPath.Text      = !string.IsNullOrEmpty(cfg.SshLogPath) ? cfg.SshLogPath : "/root/logs";
                TxtSshThreshold.Text    = cfg.SshSizeThresholdMb > 0 ? cfg.SshSizeThresholdMb.ToString() : "50";
                var defEnd   = DateTime.Now;
                var defStart = defEnd.AddMinutes(-90);
                TxtSshTimeFrom.Text = (DateTime.TryParse(cfg.SshCollectFrom, out var cfgFrom) ? cfgFrom : defStart).ToString("yyyy-MM-dd HH:mm");
                TxtSshTimeTo.Text   = (DateTime.TryParse(cfg.SshCollectTo,   out var cfgTo)   ? cfgTo   : defEnd  ).ToString("yyyy-MM-dd HH:mm");
            }
            catch { /* 加载失败时保留默认值 */ }
        }

        private async void BtnCollectLogs_Click(object sender, RoutedEventArgs e)
        {
            string host          = TxtSshHost.Text.Trim();
            string user          = TxtSshUser.Text.Trim();
            string password      = PbSshPassword.Password;
            string remoteLogPath = TxtSshLogPath.Text.Trim();
            if (!int.TryParse(TxtSshPort.Text,         out int sshPort))  sshPort  = 22;
            if (!int.TryParse(TxtSshThreshold.Text,    out int threshMb)) threshMb = 50;
            long threshBytes = (long)threshMb * 1024 * 1024;

            if (!DateTime.TryParse(TxtSshTimeFrom.Text.Trim(), out DateTime localStart))
            { AppendSshLog("❌ 开始时间格式错误，请填写如：2026-03-04 14:00"); return; }
            if (!DateTime.TryParse(TxtSshTimeTo.Text.Trim(), out DateTime localEnd))
                localEnd = DateTime.Now;

            if (string.IsNullOrEmpty(host))     { AppendSshLog("❌ 请填写平台主机地址"); return; }
            if (string.IsNullOrEmpty(user))     { AppendSshLog("❌ 请填写 SSH 用户名");   return; }
            if (string.IsNullOrEmpty(password)) { AppendSshLog("❌ 请填写密码（sudo 提权需要密码）"); return; }
            if (localEnd <= localStart)         { AppendSshLog("❌ 结束时间必须晚于开始时间"); return; }

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
                cfg.SshCollectFrom     = localStart.ToString("yyyy-MM-dd HH:mm");
                cfg.SshCollectTo       = localEnd.ToString("yyyy-MM-dd HH:mm");
                await AppConfig.SaveAsync(cfg).ConfigureAwait(true);
            }
            catch { /* non-fatal */ }

            var localNow  = DateTime.Now;
            _sshOutputDir = Path.Combine(AppContext.BaseDirectory, $"debug_collect_{localNow:yyyyMMdd_HHmmss}");
            Directory.CreateDirectory(_sshOutputDir);

            string localRangeDesc = $"{localStart:yyyy-MM-dd HH:mm:ss}  ~  {localEnd:yyyy-MM-dd HH:mm:ss}";
            AppendSshLog($"输出目录：{_sshOutputDir}");
            AppendSshLog($"本机时间范围：{localRangeDesc}");
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
                    // su root 会把 "Password:" 提示混入输出，取最后一个非空行作为实际结果
                    string whoamiLast = whoamiOut.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                        .LastOrDefault(l => !l.TrimStart().StartsWith("Password"))?.Trim() ?? whoamiOut;
                    if (whoamiLast == "root")
                    {
                        AppendSshLog("✅ 已切换到 root 账户");
                    }
                    else
                    {
                        AppendSshLog($"⚠ su root 返回：{whoamiLast}（将继续尝试，若命令失败请确认密码）");
                    }
                    AppendSshLog("");

                    // ── 1c. 计算平台侧时间范围（修正时钟偏差）──────────────────
                    // 平台时间 = 本机时间 - offsetSec
                    var remoteStart       = localStart.AddSeconds(-offsetSec);
                    long remoteStartEpoch = new DateTimeOffset(remoteStart).ToUnixTimeSeconds();
                    string remoteStartStr = remoteStart.ToString("yyyy-MM-dd HH:mm:ss");
                    var remoteEnd         = localEnd.AddSeconds(-offsetSec);
                    long remoteEndEpoch   = new DateTimeOffset(remoteEnd).ToUnixTimeSeconds();
                    string remoteEndStr   = remoteEnd.ToString("yyyy-MM-dd HH:mm:ss");
                    AppendSshLog($"平台时间范围：{remoteStartStr}  ~  {remoteEndStr}");
                    AppendSshLog("");

                    // ── 1d. 建临时目录，find -newer 递归定位范围内的文件 ────────
                    string tmpBase       = $"/tmp/dbg_{localNow:yyyyMMddHHmmss}";
                    string remoteLogRoot = remoteLogPath.TrimEnd('/');
                    SshRunAsRoot(ssh, password, $"mkdir -p {tmpBase}");
                    SshRunAsRoot(ssh, password, $"touch -d @{remoteStartEpoch} {tmpBase}/.ts_start");
                    SshRunAsRoot(ssh, password, $"touch -d @{remoteEndEpoch}   {tmpBase}/.ts_end");
                    // 不加 -maxdepth，递归搜索所有子目录下时间区间内的文件
                    string findOut = SshRunAsRoot(ssh, password,
                        $"find {remoteLogRoot} -type f -newer {tmpBase}/.ts_start ! -newer {tmpBase}/.ts_end 2>/dev/null");

                    var remoteFiles = findOut.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                        .Select(s => s.Trim()).Where(s => s.StartsWith("/"))
                        .OrderBy(s => s).ToList();

                    AppendSshLog($"目录 {remoteLogRoot}（含子目录）：{remoteFiles.Count} 个文件在时间范围内");

                    // ── 1d2. 若指定目录为空，自动探测常见平台日志目录 ──────────
                    if (remoteFiles.Count == 0)
                    {
                        AppendSshLog("  ℹ 指定目录无文件，自动扫描常见平台日志目录…");
                        // 列出各候选目录最近修改的 .log 文件，帮助用户确认实际路径
                        string discoverCmd =
                            $"find /opt /var/log /home /root /data /srv /usr/local -maxdepth 4" +
                            $" -type f -name '*.log' -newer {tmpBase}/.ts_start ! -newer {tmpBase}/.ts_end 2>/dev/null" +
                            $" | head -30";
                        string discovered = SshRunAsRoot(ssh, password, discoverCmd);
                        var discoveredFiles = discovered.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                            .Select(s => s.Trim()).Where(s => s.StartsWith("/")).ToList();
                        if (discoveredFiles.Count > 0)
                        {
                            AppendSshLog($"  找到 {discoveredFiles.Count} 个候选文件：");
                            foreach (var f in discoveredFiles)
                                AppendSshLog($"    {f}");
                            AppendSshLog("  → 请在上方'平台日志目录'中填入对应路径后重新收集");
                            // 把第一个文件的父目录作为建议写回 UI
                            string suggestedDir = discoveredFiles[0][..discoveredFiles[0].LastIndexOf('/')]; 
                            Dispatcher.BeginInvoke(new Action(() => TxtSshLogPath.Text = suggestedDir));
                        }
                        else
                        {
                            AppendSshLog("  未发现时间范围内的 .log 文件（平台可能用非 .log 扩展名）");
                            // 退而求其次：列出最近修改的任意文件（不过滤扩展名）
                            string anyCmd =
                                $"find /opt /var/log /home /root -maxdepth 5" +
                                $" -type f -newer {tmpBase}/.ts_start ! -newer {tmpBase}/.ts_end 2>/dev/null" +
                                $" | head -20";
                            string anyOut = SshRunAsRoot(ssh, password, anyCmd);
                            var anyFiles = anyOut.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                                .Select(s => s.Trim()).Where(s => s.StartsWith("/")).ToList();
                            if (anyFiles.Count > 0)
                            {
                                AppendSshLog($"  时间范围内任意文件（含非.log）：");
                                foreach (var f in anyFiles) AppendSshLog($"    {f}");
                            }
                            else
                            {
                                AppendSshLog("  该时间段无任何文件被修改（时钟偏差过大？或平台日志延迟写入）");
                            }
                        }
                    }

                    // ── 1e. 逐文件：小文件直接复制，大文件 awk 过滤（保留子目录结构）────────
                    sftp.Connect();
                    foreach (var remoteFile in remoteFiles)
                    {
                        // 每个文件独立 try-catch，单文件失败不影响其他文件
                        try
                        {
                            // 计算相对路径（保留子目录层级），用于本地存储和 tmp 镜像
                            string relPath = remoteFile.StartsWith(remoteLogRoot + "/")
                                ? remoteFile.Substring(remoteLogRoot.Length + 1)
                                : Path.GetFileName(remoteFile);
                            string tmpFile    = $"{tmpBase}/{relPath}";
                            string tmpFileDir = tmpFile.Contains('/') ? tmpFile[..tmpFile.LastIndexOf('/')] : tmpBase;

                            // 在 /tmp 中建立镜像子目录
                            SshRunAsRoot(ssh, password, $"mkdir -p {tmpFileDir}");

                            long fileSize = 0;
                            string wcOut = SshRunAsRoot(ssh, password, $"wc -c < {remoteFile} 2>/dev/null").Trim();
                            long.TryParse(wcOut.Split(' ', StringSplitOptions.RemoveEmptyEntries)[0], out fileSize);

                            if (fileSize <= threshBytes)
                            {
                                // 小文件：root cp → chmod 644 → SFTP 下载
                                AppendSshLog($"  ↓ {relPath}  ({fileSize / 1024.0:F0} KB) 直接下载");
                                SshRunAsRoot(ssh, password, $"cp {remoteFile} {tmpFile} && chmod 644 {tmpFile}");
                            }
                            else
                            {
                                // 大文件：root awk 按时间戳过滤（适配 [YYYY-MM-DD HH:MM:SS:ms] 格式）
                                AppendSshLog($"  🔍 {relPath}  ({fileSize / 1024.0 / 1024.0:F1} MB) 服务端过滤 {remoteStartStr} ~ {remoteEndStr}");
                                string awkCmd = $"awk -v cutoff='{remoteStartStr}' -v endtime='{remoteEndStr}' " +
                                    $@"'/^\[20[0-9][0-9]-[0-9][0-9]-/{{ts=substr($0,2,19); keep=(ts>=cutoff && ts<=endtime)}} keep{{print}}' " +
                                    $"{remoteFile} > {tmpFile} && chmod 644 {tmpFile}";
                                SshRunAsRoot(ssh, password, awkCmd);
                            }

                            // 通过 SFTP 下载，本地保留相同子目录结构
                            var localDst = Path.Combine(_sshOutputDir,
                                relPath.Replace('/', Path.DirectorySeparatorChar));
                            Directory.CreateDirectory(Path.GetDirectoryName(localDst)!);
                            using var fs = File.OpenWrite(localDst);
                            sftp.DownloadFile(tmpFile, fs);
                            long localSize = new FileInfo(localDst).Length;
                            AppendSshLog($"     → 已下载 {localSize / 1024.0:F0} KB");
                            remoteCount++;
                        }
                        catch (Exception exFile)
                        {
                            AppendSshLog($"  ⚠ {Path.GetFileName(remoteFile)} 跳过: {exFile.Message}");
                        }
                    }

                    // ── 1f. /var/log/messages（系统级 socket / fd 错误）────────────
                    try
                    {
                        AppendSshLog("\n收集 /var/log/messages…");
                        string msgSrc = "/var/log/messages";
                        string msgTmp = $"{tmpBase}/_messages";
                        // messages 时间戳格式 "Mar  4 18:00:01" 无年份，无法精确过滤，直接 cp 整个文件
                        string cpOut = SshRunAsRoot(ssh, password,
                            $"cp {msgSrc} {msgTmp} 2>/dev/null && chmod 644 {msgTmp} && echo OK");
                        if (cpOut.Contains("OK"))
                        {
                            var localMsg = Path.Combine(_sshOutputDir, "var_log_messages");
                            using var fsMsg = File.OpenWrite(localMsg);
                            sftp.DownloadFile(msgTmp, fsMsg);
                            long msgSize = new FileInfo(localMsg).Length;
                            AppendSshLog($"  ↓ var_log_messages  ({msgSize / 1024.0 / 1024.0:F1} MB)");
                            remoteCount++;
                        }
                        else
                        {
                            // 兜底：尝试 journalctl（systemd 系统无 /var/log/messages）
                            AppendSshLog("  ⚠ /var/log/messages 不存在或无权限，尝试 journalctl…");
                            try
                            {
                                string jSince = remoteStart.ToString("yyyy-MM-dd HH:mm:ss");
                                string jUntil = remoteEnd.ToString("yyyy-MM-dd HH:mm:ss");
                                string jCmd   = $"journalctl --since='{jSince}' --until='{jUntil}' --no-pager -q 2>/dev/null | head -2000";
                                string jOut   = SshRunAsRoot(ssh, password, jCmd);
                                // 过滤 Password: 行
                                string jClean = string.Join("\n",
                                    jOut.Split('\n').Where(l => !l.TrimStart().StartsWith("Password")));
                                if (!string.IsNullOrWhiteSpace(jClean))
                                {
                                    string jLocal = Path.Combine(_sshOutputDir, "journalctl.log");
                                    File.WriteAllText(jLocal, jClean);
                                    AppendSshLog($"  ↓ journalctl.log  ({jClean.Length / 1024.0:F0} KB，前2000行)");
                                    remoteCount++;
                                }
                                else
                                {
                                    AppendSshLog("  journalctl 也无内容（权限不足或 systemd 未运行）");
                                }
                            }
                            catch (Exception exJ) { AppendSshLog($"  journalctl 失败: {exJ.Message}"); }
                        }
                    }
                    catch (Exception exMsg) { AppendSshLog($"  ⚠ /var/log/messages: {exMsg.Message}"); }

                    sftp.Disconnect();

                    // ── 1g. 清理临时目录 ──────────────────────────────────
                    try { SshRunAsRoot(ssh, password, $"rm -rf {tmpBase}"); }
                    catch { /* 清理失败不影响结果 */ }
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

                // 2a. exe 目录下的 heartbeat_monitor_*.log
                try
                {
                    foreach (var f in Directory.GetFiles(AppContext.BaseDirectory, "heartbeat_monitor_*.log")
                        .Where(f => File.GetLastWriteTime(f) >= localStart))
                    {
                        File.Copy(f, Path.Combine(_sshOutputDir, Path.GetFileName(f)), true);
                        AppendSshLog($"  ✓ {Path.GetFileName(f)}");
                        localCount++;
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ heartbeat log: {ex.Message}"); }

                // 2b. logs/ 子目录下所有 *.log（logsend / logsend-packets / logsend-failures 等）
                try
                {
                    var logDir = Path.Combine(AppContext.BaseDirectory, "logs");
                    if (Directory.Exists(logDir))
                    {
                        var localLogsDst = Path.Combine(_sshOutputDir, "logs");
                        Directory.CreateDirectory(localLogsDst);
                        foreach (var f in Directory.GetFiles(logDir, "*.log")
                            .Where(f => File.GetLastWriteTime(f) >= localStart))
                        {
                            File.Copy(f, Path.Combine(localLogsDst, Path.GetFileName(f)), true);
                            AppendSshLog($"  ✓ logs/{Path.GetFileName(f)}");
                            localCount++;
                        }
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ logs/: {ex.Message}"); }

                // 2c. Clients.log（注册结果/CMDID，不按时间过滤，直接快照）
                try
                {
                    var clientsLog = Path.Combine(AppContext.BaseDirectory, "Clients.log");
                    if (File.Exists(clientsLog))
                    {
                        File.Copy(clientsLog, Path.Combine(_sshOutputDir, "Clients.log"), true);
                        AppendSshLog("  ✓ Clients.log（注册快照）");
                        localCount++;
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ Clients.log: {ex.Message}"); }

                // 2d. config.json 快照（SshPassword 脱敏，保留全部业务配置）
                try
                {
                    var cfgPath = Path.Combine(AppContext.BaseDirectory, "config.json");
                    if (File.Exists(cfgPath))
                    {
                        var cfgJson = File.ReadAllText(cfgPath);
                        var cfgObj  = JsonSerializer.Deserialize<AppConfig>(cfgJson);
                        if (cfgObj != null)
                        {
                            cfgObj.SshPassword = cfgObj.SshPassword.Length > 0
                                ? "***（已脱敏）" : "";
                            var redacted = JsonSerializer.Serialize(cfgObj,
                                new JsonSerializerOptions { WriteIndented = true });
                            File.WriteAllText(
                                Path.Combine(_sshOutputDir, "config_snapshot.json"),
                                redacted, System.Text.Encoding.UTF8);
                            AppendSshLog("  ✓ config_snapshot.json（平台地址/注册网段/客户端前缀等，密码已脱敏）");
                            localCount++;
                        }
                    }
                }
                catch (Exception ex) { AppendSshLog($"  ⚠ config_snapshot.json: {ex.Message}"); }

                // ── 3. 写说明文件（时间偏差 + 测试环境摘要）───────────────────────────────────
                try
                {
                    // 读 config 补充测试环境信息（复用已解析的 cfgObj 不可用时回退到重新读取）
                    AppConfig? cfgSnap = null;
                    try
                    {
                        var cfgPath2 = Path.Combine(AppContext.BaseDirectory, "config.json");
                        if (File.Exists(cfgPath2))
                            cfgSnap = JsonSerializer.Deserialize<AppConfig>(File.ReadAllText(cfgPath2));
                    }
                    catch { /* 读取失败就不写环境摘要 */ }

                    var sb = new System.Text.StringBuilder();
                    sb.AppendLine("# 日志收集说明");
                    sb.AppendLine();
                    sb.AppendLine("## 时间信息");
                    sb.AppendLine($"  收集时刻（本机）: {localNow:yyyy-MM-dd HH:mm:ss}");
                    sb.AppendLine($"  本机时间范围:     {localRangeDesc}");
                    sb.AppendLine($"  平台时间偏差:     {(offsetSec >= 0 ? "+" : "")}{offsetSec} 秒（本机 - 平台）");
                    sb.AppendLine($"  平台侧时间范围:   {localStart.AddSeconds(-offsetSec):yyyy-MM-dd HH:mm:ss}  ~  {localEnd.AddSeconds(-offsetSec):yyyy-MM-dd HH:mm:ss}");
                    sb.AppendLine();
                    sb.AppendLine("  注：分析时间线时，平台日志时间戳 + 偏差秒数 ≈ 本机时间。");
                    sb.AppendLine();

                    if (cfgSnap != null)
                    {
                        sb.AppendLine("## 测试环境配置（来自 config.json）");
                        sb.AppendLine($"  平台地址:         {cfgSnap.PlatformHost}:{cfgSnap.PlatformPort}");
                        if (cfgSnap.UseLogServer)
                            sb.AppendLine($"  日志服务器:       {cfgSnap.LogHost}:{cfgSnap.LogPort}（已启用）");
                        else
                            sb.AppendLine($"  日志服务器:       未启用");
                        sb.AppendLine();
                        sb.AppendLine($"  客户端 OS 类型:   {cfgSnap.ClientOsType}");
                        sb.AppendLine($"  客户端名称前缀:   {cfgSnap.RegClientPrefix}");
                        sb.AppendLine($"  注册起始序号:     {cfgSnap.RegStartIndex}");
                        sb.AppendLine($"  注册起始 IP:      {cfgSnap.RegStartIp}");
                        sb.AppendLine($"  注册客户端数:     {cfgSnap.RegCount}");
                        sb.AppendLine($"  心跳间隔:         {cfgSnap.HeartbeatIntervalMs / 1000} 秒");
                        sb.AppendLine();
                        sb.AppendLine($"  日志发送 HTTPS:   客户端 {cfgSnap.LogHttpsClientCount} 个  ×  {cfgSnap.LogHttpsEps} EPS  ×  {cfgSnap.LogMessagesPerClient} 条");
                        sb.AppendLine($"  日志发送威胁检测: 客户端 {cfgSnap.LogThreatClientCount} 个  ×  {cfgSnap.LogThreatEps} EPS  ×  {cfgSnap.LogMessagesPerClient} 条");
                        sb.AppendLine();
                        sb.AppendLine($"  SSH 日志路径:     {cfgSnap.SshLogPath}");
                        sb.AppendLine($"  SSH 大文件阈值:   {cfgSnap.SshSizeThresholdMb} MB");
                    }

                    File.WriteAllText(Path.Combine(_sshOutputDir, "_收集说明.txt"),
                        sb.ToString(), System.Text.Encoding.UTF8);
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

        /// <summary>通过 echo PASSWORD | su root -c '...' 切换 root 执行命令，返回 stdout+stderr（已过滤 Password: 提示行）</summary>
        private static string SshRunAsRoot(SshClient ssh, string password, string command)
        {
            string cmdLine = $"echo {ShellQuote(password)} | su root -c {ShellQuote(command)} 2>&1";
            using var cmd  = ssh.RunCommand(cmdLine);
            // su 会把 "Password:" 提示混入输出，过滤掉避免污染下游解析
            var lines = cmd.Result.Split('\n')
                .Where(l => !l.TrimStart().StartsWith("Password:"))
                .ToArray();
            return string.Join("\n", lines);
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