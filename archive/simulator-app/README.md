# archive/simulator-app/ — SimulatorApp 历史构建脚本归档

这里收纳的是 `main` 分支上**已不再维护**的 SimulatorApp（WPF 工具）相关根目录文件。
搬迁原因：`main` 分支主推 WLServerTest（MFC 工具），这些脚本/sln 容易让 clone 仓库的人误判主推工具。

## 内容

| 文件 | 用途 | 现归属 |
|---|---|---|
| `IEGPerTest.sln` | SimulatorApp 的 Visual Studio 解决方案（不含 WLServerTest 工程） | SimulatorApp |
| `build_all.bat` | 一键构建 C++ DLL + .NET publish | SimulatorApp |
| `export_dependencies.ps1` | NativeEngine 依赖导出脚本 | SimulatorApp |
| `dependencies_export/` | 导出的 zlib 头/库 | SimulatorApp |
| `publish_simulatorapp.ps1` | SimulatorApp 发布脚本（原 `scripts/`） | SimulatorApp |
| `RELEASE_NOTES_v3.9.7.md` | SimulatorApp v3.9.7 发布说明 | SimulatorApp |

## 想真正使用 / 编译 SimulatorApp？

请切到对应架构分支：

- [`simulator-subprocess`](../../../../tree/simulator-subprocess) — **v3.9.7**，WPF + NativeRunner.exe 子进程架构（推荐）
- [`simulator-inproc-dll`](../../../../tree/simulator-inproc-dll) — **v3.7.31**，WPF + NativeSender.dll 同进程旧架构

那些分支上的根目录结构（`IEGPerTest.sln` / `build_all.bat` / `src/` 等）才是 SimulatorApp 的正常工作目录。

## 为什么不直接删？

- 历史可追溯：`main` 分支保留这些文件的搬迁记录而非 `git rm`
- 偶尔需要在 `main` 上对照查阅 SimulatorApp 历史脚本时无需切分支
- 若未来想把 SimulatorApp 重新作为 `main` 主推工具，从 archive 还原即可
