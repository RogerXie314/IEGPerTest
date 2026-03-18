using System.Threading;
using System.Windows;
using SimulatorLib.Network;

namespace SimulatorApp
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // v3.7.30: 初始化 Winsock（NativeSender DLL 需要）
            NativeSenderInterop.NS_Init();

            // 400 客户端 × (HB task + LogWorker task) = 800+ 并发任务，P/Invoke 阻塞调用需要足够线程。
            ThreadPool.SetMinThreads(1000, 1000);
            base.OnStartup(e);
        }

        protected override void OnExit(ExitEventArgs e)
        {
            NativeSenderInterop.NS_Cleanup();
            base.OnExit(e);
        }
    }
}
