using System.Windows;

namespace SimulatorApp.Views
{
    public partial class VersionManagementWindow : Window
    {
        public VersionManagementWindow()
        {
            InitializeComponent();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
