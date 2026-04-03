using System;
using System.IO;
using System.Threading;
using System.Windows;
using SimulatorLib.Network;

namespace SimulatorApp
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // 检测必需的 C++ DLL，缺失则报错退出
            var baseDir = AppContext.BaseDirectory;
            var requiredDlls = new[]
            {
                "NativeEngine.dll",
                "NativeSender.dll",
                "RawPacketEngine.dll"
            };

            var missingDlls = new System.Collections.Generic.List<string>();
            foreach (var dll in requiredDlls)
            {
                var path = Path.Combine(baseDir, dll);
                if (!File.Exists(path))
                    missingDlls.Add(dll);
            }

            if (missingDlls.Count > 0)
            {
                var msg = $"缺少必需的 DLL 文件，程序无法启动：\n\n{string.Join("\n", missingDlls)}\n\n" +
                          $"请确保以下文件与 SimulatorApp.exe 位于同一目录：\n" +
                          $"- NativeEngine.dll（心跳和长连接日志引擎）\n" +
                          $"- NativeSender.dll（PT 协议打包）\n" +
                          $"- RawPacketEngine.dll（攻击报文发送引擎）";
                MessageBox.Show(msg, "启动失败", MessageBoxButton.OK, MessageBoxImage.Error);
                Shutdown(1);
                return;
            }

            // 初始化 Winsock（NativeSender DLL 需要）
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
