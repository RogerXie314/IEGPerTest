using System;
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
            Loaded += async (_, _) =>
            {
                try { await _vm.InitializeAsync(); }
                catch (Exception ex)
                {
                    MessageBox.Show(
                        $"攻击报文模块初始化失败：{ex.Message}\n\n请确认 Npcap 已安装（https://npcap.com/）",
                        "初始化失败", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            };
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            _vm.OnWindowClosing();
            base.OnClosing(e);
        }
    }
}
