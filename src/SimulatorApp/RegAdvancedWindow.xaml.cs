using System.Windows;

namespace SimulatorApp
{
    public partial class RegAdvancedWindow : Window
    {
        public RegAdvancedWindow()
        {
            InitializeComponent();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
