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
    }
}

