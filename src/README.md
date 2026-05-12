# ⚠️ src/ 目录在 `main` 分支上已不再维护

本目录下所有 C# / C++ 工程（`SimulatorApp`、`SimulatorLib`、`NativeEngine`、`NativeRunner`、`NativeSender`、`RawPacketEngine`、`SimulatorRunner`、`TestReceiver`、`DevTools`）都属于 **SimulatorApp** 工具链。

`main` 分支主推 **WLServerTest**（MFC 工具，源码在 [external/IEG_Code/code/WLServerTest/](../external/IEG_Code/code/WLServerTest/)），`src/` 下的代码不再随 `main` 演进。

## 想编译 / 修改 SimulatorApp？

请切到对应架构分支：

| 分支 | 架构 | 最新版本 |
|---|---|---|
| [`simulator-subprocess`](../../../tree/simulator-subprocess) | WPF + NativeRunner.exe 子进程（stdio 管道 IPC） | v3.9.7 |
| [`simulator-inproc-dll`](../../../tree/simulator-inproc-dll) | WPF + NativeSender.dll 同进程（P/Invoke） | v3.7.31 |

那两条分支上有完整的 `IEGPerTest.sln` / `build_all.bat` / `scripts/publish_simulatorapp.ps1`，能直接编译发布。

## 为什么 main 上还留着这些代码？

便于在 `main` 上对照查阅，避免每次研究架构差异都要切分支。相关的根目录构建脚本已搬到 [`archive/simulator-app/`](../archive/simulator-app/)。

详见仓库根 [README.md](../README.md)。
