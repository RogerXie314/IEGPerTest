using System.Reflection;
using System.Windows;

namespace SimulatorApp
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext = new ViewModels.MainViewModel();

            var ver = Assembly.GetExecutingAssembly().GetName().Version;
            Title = ver != null
                ? $"IEG 模拟器 v{ver.Major}.{ver.Minor}.{ver.Build}"
                : "IEG 模拟器";
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            // 关闭主窗口时确保子进程被强制终止
            if (DataContext is ViewModels.MainViewModel vm)
                vm.Cleanup();
            base.OnClosing(e);
        }

        private void ShowTcpDiag_Click(object sender, RoutedEventArgs e)
        {
            var vm  = (ViewModels.MainViewModel)DataContext;
            var win = new TcpDiagWindow(vm: vm) { Owner = this };
            win.ShowDialog();
        }

        private void ShowLogCategoryHelp_Click(object sender, RoutedEventArgs e)
        {
            new LogCategoryHelpWindow { Owner = this }.ShowDialog();
        }

        private void ShowRegAdvanced_Click(object sender, RoutedEventArgs e)
        {
            new RegAdvancedWindow { Owner = this, DataContext = DataContext }.ShowDialog();
        }

        private async void RegReset_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show(
                "将清空已注册客户端列表（Clients.log），\n并将注册设置恢复为默认值。\n\n确认重置？",
                "确认重置",
                MessageBoxButton.OKCancel,
                MessageBoxImage.Warning);
            if (result != MessageBoxResult.OK) return;
            if (DataContext is ViewModels.MainViewModel vm)
                await vm.ResetRegistrationAsync();
        }

        private void BtnRawPacket_Click(object sender, RoutedEventArgs e)
        {
            var win = new SimulatorApp.Views.RawPacketWindow();
            win.Show();
        }

        private void ManageVersions_Click(object sender, RoutedEventArgs e)
        {
            var win = new Views.VersionManagementWindow { Owner = this };
            var dialogResult = win.ShowDialog();
            
            // 窗口关闭后重新加载版本列表
            if (DataContext is ViewModels.MainViewModel vm)
            {
                // 通过反射调用私有方法 LoadClientVersions
                var method = vm.GetType().GetMethod("LoadClientVersions", 
                    System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                method?.Invoke(vm, null);
                
                // 触发属性更新
                vm.GetType().GetMethod("OnProp", 
                    System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)?
                    .Invoke(vm, new object?[] { "ClientVersionList" });
            }
        }
    }
}

