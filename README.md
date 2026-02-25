# IEGPerTest Simulator

IEG/EDR 客户端模拟器：用于模拟客户端注册、心跳、日志发送与白名单上传的 WPF 桌面工具（.NET 8）。

## 📚 核心文档

- **项目看板**：[project_dashboard.html](project_dashboard.html) - 可视化进度看板（启动：`.\scripts\start_dashboard.ps1`）
- **实施文档**：[docs/项目实施文档.md](docs/项目实施文档.md)
- **经验教训**：[docs/经验教训-日志类型实现.md](docs/经验教训-日志类型实现.md)

## 🚀 快速开始

### 开发环境运行

```powershell
# 构建并运行（需要 .NET 8 SDK）
dotnet build
dotnet run --project src/SimulatorApp
```

### 发布独立可执行文件

```powershell
# 发布所有组件
.\scripts\publish_all.ps1

# 或单独发布
.\scripts\publish_simulatorapp.ps1      # 主应用
.\scripts\publish_simulatorrunner.ps1   # 运行器
.\scripts\publish_testreceiver.ps1      # 测试接收器
.\scripts\publish_devrunner.ps1         # 开发工具
```

发布产物位于 `artifacts/*Publish/` 目录。

### 部署到无 .NET 环境的机器

发布的应用是**自包含**的，但需要拷贝以下文件：

```
SimulatorApp.exe                    # 主程序（146 MB）
D3DCompiler_47_cor3.dll            # WPF 依赖
PenImc_cor3.dll
PresentationNative_cor3.dll
vcruntime140_cor3.dll
wpfgfx_cor3.dll
```

保持这些文件在同一目录即可在任何 Windows x64 机器上运行。

## ✅ 当前状态（v2.1.0）

**已完成功能：**
- ✅ 客户端注册、心跳、白名单上传（PT/HTTP/HTTPS）
- ✅ **11 种日志类型**完整实现并验证（详见下方列表）
- ✅ UI 界面完善（17 种日志分类、心跳选项、项目类型选择）
- ✅ 单文件发布流程（含 WPF 原生依赖）
- ✅ 持久化存储（`Clients.log`、`config.json`）

**已实现的 11 种日志类型：**
1. 非法程序启动（非白名单）
2. 白名单防篡改
3. 进程审计事件
4. 病毒告警事件
5. 漏洞预警
6. 系统资源异常告警
7. 违规外联
8. 威胁数据采集-进程启动事件
9. 文件保护（HostDefence）
10. 注册表保护（HostDefence）
11. 强制访问控制（HostDefence）

**待实现日志类型（剩余 6 种）：**
- USB 设备认证
- USB 访问告警（IEG）
- U 盘插拔事件（IEG）
- 软件安装异常（EDR）
- 防火墙事件（EDR）
- 客户端操作（EDR）

## 📖 下一步建议

**日志类型扩展：**
- 参考 [经验教训文档](docs/经验教训-日志类型实现.md) 实现剩余 6 种日志类型
- 核心经验：**数据优先于代码**，先对比原项目实际日志格式

**UI 增强：**
- 任务状态表格（开始时间、持续时间、状态）
- 更细粒度的进度展示

**协议完善：**
- 日志路由规则细化（HTTPS/UDP 选择逻辑）
- 心跳超时与重连机制

## 🛠️ 技术栈

- .NET 8.0 (WPF)
- 自包含发布（win-x64）
- 持久化：JSON 文件
- 网络协议：HTTP/HTTPS（客户端模拟）

## 📁 项目结构

```
src/
   SimulatorApp/          # WPF 主应用
   SimulatorLib/          # 核心业务逻辑
   SimulatorRunner/       # CLI 运行器
   TestReceiver/          # 测试接收服务器
scripts/                     # 构建和发布脚本
docs/                        # 文档和经验总结
artifacts/                   # 发布产物输出
```

## 📝 重要说明

- `Clients.log` 和 `config.json` 在程序首次运行时自动生成
- 修改代码后需重新执行 `publish` 脚本更新 exe
- Git 代理配置（如需要）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- 版本标签：v2.0.0（UI完善）、v2.1.0（11个日志类型）
