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

        private void ShowTcpDiag_Click(object sender, RoutedEventArgs e)
        {
            var vm  = (ViewModels.MainViewModel)DataContext;
            var win = new TcpDiagWindow(vm: vm) { Owner = this };
            win.ShowDialog();
        }
    }
}

