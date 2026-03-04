using System;
using System.Diagnostics;
using System.Windows;

namespace SimulatorApp
{
    /// <summary>
    /// SSH 日志收集进度窗口——独立子窗口，保持可见直到用户手动关闭。
    /// </summary>
    public partial class SshLogWindow : Window
    {
        private string? _outputDir;
        private bool _forceClose;

        public SshLogWindow()
        {
            InitializeComponent();
        }

        // 关闭 X 按钮时改为隐藏，避免用户误关导致看不到还在进行中的收集
        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (!_forceClose)
            {
                e.Cancel = true;
                Hide();
            }
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            _forceClose = true;
            Close();
        }

        private void BtnClear_Click(object sender, RoutedEventArgs e)
        {
            TxtLog.Clear();
        }

        private void BtnOpenDir_Click(object sender, RoutedEventArgs e)
        {
            if (!string.IsNullOrEmpty(_outputDir) && System.IO.Directory.Exists(_outputDir))
                Process.Start(new ProcessStartInfo { FileName = _outputDir, UseShellExecute = true });
        }

        // ── 外部调用 API ─────────────────────────────────────────────────────

        /// <summary>清空日志区（线程安全）</summary>
        public void Clear()
        {
            if (!CheckAccess()) { Dispatcher.BeginInvoke(new Action(Clear)); return; }
            TxtLog.Clear();
        }

        /// <summary>向日志区追加一行（线程安全）</summary>
        public void AppendLine(string text)
        {
            if (!CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(() => AppendLine(text)));
                return;
            }
            TxtLog.AppendText(text + "\n");
            TxtLog.ScrollToEnd();
        }

        /// <summary>更新底部状态文本（线程安全）</summary>
        public void SetStatus(string text)
        {
            if (!CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(() => SetStatus(text)));
                return;
            }
            TxtStatus.Text = text;
        }

        /// <summary>收集完成后显示"打开目录"按钮</summary>
        public void SetOutputDir(string dir)
        {
            if (!CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(() => SetOutputDir(dir)));
                return;
            }
            _outputDir = dir;
            BtnOpenDir.Visibility = string.IsNullOrEmpty(dir) ? Visibility.Collapsed : Visibility.Visible;
        }

        /// <summary>将窗口置顶显示（若已隐藏则重新显示）</summary>
        public void ShowAndActivate()
        {
            if (!CheckAccess())
            {
                Dispatcher.BeginInvoke(new Action(ShowAndActivate));
                return;
            }
            if (!IsVisible) Show();
            Activate();
        }
    }
}
