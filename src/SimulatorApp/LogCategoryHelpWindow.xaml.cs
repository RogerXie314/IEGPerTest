using System.Windows;

namespace SimulatorApp
{
    public partial class LogCategoryHelpWindow : Window
    {
        public LogCategoryHelpWindow()
        {
            InitializeComponent();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
