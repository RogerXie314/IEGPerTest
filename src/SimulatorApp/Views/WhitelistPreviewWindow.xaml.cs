using System.Windows;
using SimulatorApp.ViewModels;

namespace SimulatorApp.Views
{
    public partial class WhitelistPreviewWindow : Window
    {
        public WhitelistPreviewWindow(string filePath)
        {
            InitializeComponent();
            DataContext = new WhitelistPreviewViewModel(filePath);
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
