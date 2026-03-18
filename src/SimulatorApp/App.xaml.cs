using System.Threading;
using System.Windows;

namespace SimulatorApp
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // 400+ 客户端 × (HB task + drain task + LogWorker task) = 1200+ 并发异步操作。
            // .NET ThreadPool 默认 MinThreads = CPU核数（~8），每 500ms 才创建 1 个新线程。
            // 如果不提前设置，Task.Delay / WriteAsync 的回调排不上线程，
            // 导致 HB 超时 → 平台关连接 → LogWorker 空转紧循环 → CPU/内存炸裂 → 雪崩。
            ThreadPool.SetMinThreads(500, 500);
            base.OnStartup(e);
        }
    }
}
