using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using SimulatorLib.Network;
using SimulatorLib.Workers;

namespace SimulatorApp
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource? _hbCts;

        public MainWindow()
        {
            using System.Windows;
            using SimulatorApp.ViewModels;

            namespace SimulatorApp
            {
                public partial class MainWindow : Window
                {
                    public MainWindow()
                    {
                        InitializeComponent();
                        DataContext = new MainViewModel();
                    }
                }
            }
            }
            catch { return false; }
        }
    }
}

