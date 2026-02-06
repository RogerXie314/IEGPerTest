# IEGPerTest Simulator

项目原型：用于模拟客户端注册、心跳、日志发送与白名单上传的桌面工具（WPF + .NET 7）。

快速开始：

- 打开命令行，进入项目根目录（本仓库）
- 若机器已安装 .NET SDK，可构建并运行：

```powershell
cd src/SimulatorApp
dotnet build
dotnet run
```

- `Clients.log` 将位于程序运行目录（用于持久化已注册客户端列表），运行时不要将该文件纳入版本控制（已在 `.gitignore` 中忽略）。

下一步：实现 UI 控件、`RegistrationWorker` / `HeartbeatWorker`、网络抽象层与测试用例。

自动化与 CI
----------------
本仓库已添加本地自动化脚本和 GitHub Actions CI：

- 本地脚本：`scripts/build_and_run.ps1`，用于构建并运行演示程序；可传入 `-RunBackground` 参数后台运行。
- CI：`.github/workflows/ci.yml`，在 `push`/`pull_request` 时于 `windows-latest` 上构建并运行一次 `SimulatorRunner` 做快速校验。

说明：如果需要自动化 `git push`，请在使用脚本的机器上配置凭据（SSH key 或 PAT）。
