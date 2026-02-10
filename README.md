# IEGPerTest Simulator

项目原型：用于模拟客户端注册、心跳、日志发送与白名单上传的桌面工具（WPF + .NET 8）。

实施文档（快速恢复版）：[docs/项目实施文档.md](docs/项目实施文档.md)

快速开始：

- 打开命令行，进入项目根目录（本仓库）
- 若机器已安装 .NET SDK，可构建并运行：

```powershell
dotnet build
dotnet run --project src/SimulatorApp
```

说明：

- `dotnet run` / `dotnet build` 运行的是源码构建产物（总是最新）。
- `artifacts/*Publish/*.exe` 是“发布产物”，不会因为你改了代码自动更新；需要重新 `publish`。

发布最新 exe（可选）：

```powershell
./scripts/publish_all.ps1
```

分别发布：

```powershell
./scripts/publish_simulatorapp.ps1
./scripts/publish_simulatorrunner.ps1
./scripts/publish_testreceiver.ps1
./scripts/publish_devrunner.ps1
```

- `Clients.log` 将位于程序运行目录（用于持久化已注册客户端列表），运行时不要将该文件纳入版本控制（已在 `.gitignore` 中忽略）。

已实现（基础版）：注册、心跳、日志发送、白名单上传（均基于 `Clients.log`）；并有基础单元测试覆盖 ViewModel 与配置。

下一步（建议迭代）：
- UI：按需求补齐“任务状态表格”（开始时间/持续时间/状态等）与更细粒度的进度展示
- 协议：按需求规则补齐“除威胁数据采集外的日志走 HTTPS（未勾选日志服务器）”等协议差异（当前日志发送为简化版示例 payload）
- 数据：补齐 `Clients.log` 的导入/导出/清理与并发访问防护策略

- 自动化与 CI
----------------
本仓库已添加本地自动化脚本和 GitHub Actions CI：

- 本地脚本：`scripts/build_and_run.ps1`，用于构建并运行演示程序；可传入 `-RunBackground` 参数后台运行。
- 快捷方式创建：`scripts/create_shortcut.ps1`，用于在当前用户桌面创建指向 `artifacts/DevRunner/DevRunner.exe` 的快捷方式；可选在仓库 `tools/` 目录生成一份复制。示例：

```powershell
# 在桌面创建快捷方式（默认目标 artifacts/DevRunner/DevRunner.exe）
.\scripts\create_shortcut.ps1

# 同时在仓库 tools/ 目录也创建快捷方式
.\scripts\create_shortcut.ps1 -CreateInRepoTools
```
- CI：`.github/workflows/ci.yml`，在 `push`/`pull_request` 时于 `windows-latest` 上构建并运行一次 `SimulatorRunner` 做快速校验。

说明：如果需要自动化 `git push`，请在使用脚本的机器上配置凭据（SSH key 或 PAT）。
