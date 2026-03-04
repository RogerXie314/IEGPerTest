using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Windows;
using Microsoft.Win32;

namespace SimulatorApp
{
    public partial class PortAdvancedWindow : Window
    {
        private readonly bool _multiIpMode;
        private readonly int  _ipCount;

        public PortAdvancedWindow(bool multiIpMode = false, int ipCount = 1)
        {
            InitializeComponent();
            _multiIpMode = multiIpMode;
            _ipCount = ipCount;
            Loaded += (_, _) => RefreshData();
        }

        private void RefreshData()
        {
            // ── 1. 读取系统 TCP 配置 ──────────────────────────────────────────
            int portStart = 49152;
            int portCount = 16384;
            int timeWaitSec = -1;
            bool portRangeModified = false;
            bool timeWaitFromReg   = false;

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

                var numbers = new List<int>();
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
            catch { }

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
            catch { }

            string portRangeSrc    = portRangeModified ? "已优化" : "系统默认";
            int    portEnd         = portStart + portCount - 1;
            int    effectiveTimeWait = timeWaitSec > 0 ? timeWaitSec : 120;
            string timeWaitSrc     = timeWaitFromReg ? "注册表已配置" : "未配置，使用系统默认 (~120s)";

            // ── 2. 读取端口占用率（用于 EPS 计算参考）────────────────────────
            int cntEstablished = 0;
            var usedLocalPorts = new HashSet<int>();
            try
            {
                var props       = IPGlobalProperties.GetIPGlobalProperties();
                var connections = props.GetActiveTcpConnections();
                foreach (var conn in connections)
                {
                    if (conn.State == TcpState.Established) cntEstablished++;
                    int lp = conn.LocalEndPoint.Port;
                    if (lp >= portStart && lp <= portEnd)
                        usedLocalPorts.Add(lp);
                }
            }
            catch { }

            int    portsUsed     = usedLocalPorts.Count;
            double portUsagePct  = portCount > 0 ? (double)portsUsed / portCount * 100.0 : 0;
            double theoreticalEps = (double)portCount / effectiveTimeWait;

            // ── 3. 更新界面 ───────────────────────────────────────────────────
            TxtSampleTime.Text = DateTime.Now.ToString("HH:mm:ss");

            // TCP 配置区
            TxtPortRange.Text = $"{portStart} – {portEnd}  (共 {portCount:N0} 个端口)  [{portRangeSrc}]";
            TxtTimeWait.Text  = timeWaitFromReg
                ? $"{timeWaitSec} 秒  [{timeWaitSrc}]"
                : timeWaitSrc;

            // 理论 EPS 区
            if (_multiIpMode && _ipCount > 1)
            {
                double multiEps = theoreticalEps * _ipCount;
                TxtTheoreticalEps.Text   = $"{multiEps:N0}";
                TxtFormula.Text          = $"{_ipCount} × ({portCount:N0} ÷ {effectiveTimeWait}s) = {multiEps:N0}";
                TxtMultiIpEps.Text       = $"单IP: {theoreticalEps:N0} EPS";
                TxtMultiIpEps.Visibility  = Visibility.Visible;
                PanelMultiIp.Visibility   = Visibility.Visible;
                TxtMultiIpDesc.Text       = $"{_ipCount} 个 IP，上限扩大 {_ipCount} 倍";
                TxtMultiIpFormula.Text    = $"{_ipCount} × {theoreticalEps:N0} = {multiEps:N0} EPS";
            }
            else
            {
                TxtTheoreticalEps.Text   = $"{theoreticalEps:N0}";
                TxtFormula.Text          = $"{portCount:N0} ÷ {effectiveTimeWait}s = {theoreticalEps:N0}";
                TxtMultiIpEps.Visibility  = Visibility.Collapsed;
                PanelMultiIp.Visibility   = Visibility.Collapsed;
            }
            TxtCurrentUsagePct.Text = $"{portUsagePct:F1}%（端口池已占用）";
            TxtMultiHostTip.Text    = _multiIpMode
                ? $"当前已启用多IP模式（{_ipCount} 个IP）；端口上限已扩大 {_ipCount} 倍"
                : "若需多台主机：绑定 N 个 IP = 上限 × N；开启压测模式（长连接）可突破端口瓶颈";
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => RefreshData();

        /// <summary>Allow parent TcpDiagWindow to push a refresh.</summary>
        public void RefreshFromParent() => RefreshData();

        private void BtnClose_Click(object sender, RoutedEventArgs e) => Close();

        // ── 一键推荐 ────────────────────────────────────────────────────────
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

        private bool ApplyPortRange(int start, int count)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName        = "netsh",
                    Arguments       = $"int ipv4 set dynamicport tcp start={start} num={count}",
                    UseShellExecute = true,
                    Verb            = "runas",
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
    }
}
