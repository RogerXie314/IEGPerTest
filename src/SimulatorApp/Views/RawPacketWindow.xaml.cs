using System.Windows;
using SimulatorApp.ViewModels;

namespace SimulatorApp.Views
{
    public partial class RawPacketWindow : Window
    {
        private readonly RawPacketViewModel _vm;

        public RawPacketWindow()
        {
            InitializeComponent();
            _vm = new RawPacketViewModel();
            _vm.OwnerWindow = this;
            DataContext = _vm;
            Loaded += async (_, _) => await _vm.InitializeAsync();
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            _vm.OnWindowClosing();
            base.OnClosing(e);
        }
    }
}
