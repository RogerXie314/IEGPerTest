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

发布的应用是**完全自包含单文件**，只需拷贝一个文件：

```
SimulatorApp.exe    # 主程序（约 155 MB，含 .NET 运行时 + WPF 原生库）
```

在任何 Windows x64 机器上双击即可运行，无需安装 .NET 或额外 DLL。

> 注意：程序首次运行时会在同目录生成 `config.json` 和 `Clients.log`。

## ✅ 当前状态（v2.3.0）

**已完成功能：**
- ✅ 客户端注册、心跳、白名单上传（PT/HTTP/HTTPS）
- ✅ **TCP 心跳 1000 客户端全部稳定在线**（独立 Task + bool alive 重连机制）
- ✅ 心跳断线原因监控：自动写 `heartbeat_monitor_*.log`（5 种原因：服务端关闭/写失败/连接失败/超时/正常）
- ✅ **11 种日志类型**完整实现并验证（详见下方列表），JSON 字段名对齐平台 `WLJsonParse.cpp`
- ✅ UI 布局优化（左列注册+日志分类加宽，右列心跳+白名单+日志发送，2×2 统计面板）
- ✅ **外设控制子分类**：9 种（USB接口/手机平板/光驱/无线网卡/USB网卡/软盘/蓝牙/串口/并口）
- ✅ **日志发送 EPS 精确控制**（deadline 模式：HTTP 耗时从间隔中扣除，超期重置基准而非累积欠债）
- ✅ **防 TLS 握手风暴**：>100 客户端时错峰窗口扩大到 10s（≤100 TLS/s 均匀分散）
- ✅ **对齐原始客户端 HTTP 行为**（HTTP/1.1、每次请求独立连接 `PooledConnectionLifetime=Zero`、连接超时 6s/请求超时 30s）
- ✅ **日志发送每客户端独立并发**，失败单独写分类 debug 日志
- ✅ HTTPS 失败响应写独立 log（记录状态码和响应体，便于诊断平台下发策略）
- ✅ 启动时默认全选 IEG 分类、默认不勾选日志服务器
- ✅ **压测模式**：连接复用+信号量门控，单机支持 3000 客户端 / 6000+ EPS（并发连接数可配置，默认 50）
- ✅ 单文件发布（154.6 MB，含 WPF 原生依赖，无需目标机安装 .NET）
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
- 版本标签脉络：`v2.0.0`（UI完善）→ `v2.1.0`（11种日志/文档）→ `v2.0-tcp-heartbeat-stable`（TCP心跳1000客户端稳定，里程碑标记）→ `v2.2.0`（UI布局优化+断线原因监控日志）→ **`v2.3.0`**（外设控制分类、EPS精确控制、防TLS风暴、压测模式，当前）
