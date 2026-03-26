# IEGPerTest Simulator

IEG/EDR 客户端模拟器：用于模拟客户端注册、心跳、日志发送与白名单上传的 WPF 桌面工具（.NET 8）。

## 📚 核心文档

- **实施文档**：[docs/项目实施文档.md](docs/项目实施文档.md)（含完整变更记录、调试技巧、协议路由表）
- **经验教训**：[docs/经验教训-日志类型实现.md](docs/经验教训-日志类型实现.md)
- **项目看板**：[docs/project_dashboard.html](docs/project_dashboard.html)

## 🚀 快速开始

### 开发环境运行

```powershell
# 构建并运行（需要 .NET 8 SDK）
dotnet build
dotnet run --project src/SimulatorApp
```

### 发布独立可执行文件

```powershell
# 常规发布（每次发版用这个）：自动 bump 版本号 + 构建 C++ DLL + 打包单文件 + git commit/push
.\scripts\publish_simulatorapp.ps1

# 全量发布（仅当 SimulatorRunner / TestReceiver / DevRunner 有代码改动时才用）
.\scripts\publish_all.ps1
```

发布产物位于 `artifacts/SimulatorAppPublish/` 目录。

### 部署到无 .NET 环境的机器

发布的应用是**完全自包含单文件 EXE**（NativeEngine.dll + NativeSender.dll 内嵌），只需拷贝一个文件：

```
SimulatorApp.exe    # 主程序（约 73 MB，含 .NET 运行时 + WPF 原生库 + C++ DLL）
```

在任何 Windows x64 机器上双击即可运行，无需安装 .NET 或额外 DLL。

> 注意：程序首次运行时会在同目录生成 `config.json` 和 `Clients.log`。

## ✅ 当前状态（v3.8.7）

### 核心架构

- **NativeEngine C++ DLL（v3.7.39+）**：心跳和威胁日志发送的热路径完全由 C++ 实现（非阻塞 socket + OS 线程 + 纯 C++ JSON/zlib/PT 打包），零 P/Invoke 热路径，对齐老工具 WLServerTest 行为
- **双引擎自动切换**：检测到 NativeEngine.dll 时走 C++ 引擎，否则回退 C# TcpClient 路径
- **Channel 架构（v3.7.16）→ 直写架构（v3.7.27）→ NativeEngine（v3.7.39）**：经历三代架构演进，彻底消除锁竞争和 EPS 振荡

### 已完成功能

- ✅ 客户端注册、心跳、白名单上传（PT/HTTP/HTTPS）
- ✅ **TCP 心跳稳定在线**（NativeEngine 非阻塞 socket，每客户端 1 个 OS 线程，对齐老工具 `AfxBeginThread + Sleep(500)` 节奏）
- ✅ 心跳断线原因监控：`logs/heartbeat_monitor_*.log`（7 种原因：服务端关闭/写失败/连接失败/超时/Session超时/锁竞争跳过/未注册重注册）
- ✅ **23 种日志类型**完整实现（详见下方列表），JSON 字段名对齐平台 `WLJsonParse.cpp`
- ✅ **威胁检测 5 种子类**（TCP 长连接，CMDID=21）：JSON+zlib+PT 打包全在 C++ 内完成，EPS 对齐老工具 ~1050
- ✅ **威胁检测命中率对齐**：`bHit=1/71`（每 71 包 1 包命中），miss 数据使用无害进程名
- ✅ **外设控制 9 种子分类**（USB接口/手机平板/光驱/无线网卡/USB网卡/软盘/蓝牙/串口/并口）
- ✅ **日志发送 EPS 精确控制**（deadline 模式）
- ✅ **防 TLS 握手风暴**：>100 客户端时错峰窗口扩大到 10s
- ✅ **压测模式**：连接复用 + 信号量门控，单机支持 3000 客户端 / 6000+ EPS
- ✅ **单文件发布**（约 73 MB，NativeEngine.dll + NativeSender.dll 内嵌 EXE）
- ✅ 持久化存储（`Clients.log`、`config.json`）
- ✅ **分通道独立配置**：HTTPS 与威胁检测各自独立配置客户端数和 EPS
- ✅ **任务面板（TaskPanel）**：所有后台任务统一 DataGrid，实时显示状态/进度/成功/失败计数
- ✅ **策略接收（PolicyReceiveWorker）**：TCP cmdId=17 触发 HTTPS 拉取策略 → 回 ACK；NativeEngine 模式通过回调支持
- ✅ **白名单轮换**：随机轮换，15 分钟周期，HTTPS multipart 上传 + clientScanStatus 通知
- ✅ **注册版本选择**：ComboBox 选择客户端版本（默认 V300R011C01B030）
- ✅ **客户端 OS 类型切换**：注册/心跳时可选 Windows / Linux
- ✅ **多IP模式运行时连接计数**：每IP实际建连次数原子统计
- ✅ **连接参数诊断**：一键推荐设置（端口范围/TIME_WAIT），`netsh show` 正则解析（中英文兼容）
- ✅ **SSH 平台日志一键收集**：SSH → su root → RTT 偏差补偿 → find 递归 → awk 过滤 → /var/log/messages
- ✅ **SshLogWindow 独立浮窗**：SSH 收集进度独立展示
- ✅ **日志分类说明弹窗**：「？分类说明」按钮，含项目类型/颜色含义/USB 四类区分/典型场景
- ✅ **注册高级设置子窗口**（RegAdvancedWindow）：并发数/超时/轮间隔独立配置
- ✅ **端口&EPS 高级设置子窗口**（PortAdvancedWindow）：TCP 配置/EPS/参数优化独立配置
- ✅ **失败日志按文件大小限制（50 MB）**，日志统一输出到 `logs/` 目录

### 已实现的全部日志类型（23 种主类 + 9 种外设子类）

*通用/IEG 类（HTTPS 通道）：*
1. 非法程序启动（非白名单）
2. 白名单防篡改
3. 进程审计事件
4. 病毒告警事件
5. 漏洞预警
6. 系统资源异常告警
7. 违规外联
8. USB 设备认证
9. USB 访问告警（IEG）
10. U盘插拔事件（IEG）
11. 网口Up/Down（IEG，CMDVER=4）
12. U盘告警（老版本，UsbType=2）

*威胁检测（TCP 长连接，CMDID=21）：*
13. 威胁数据采集-进程启动
14. 威胁数据采集-注册表访问
15. 威胁数据采集-文件访问
16. 威胁数据采集-系统日志
17. 威胁数据采集-DLL 加载

*EDR 专属（HTTPS，DP/SysGuard 通道）：*
18. 文件保护（DP/HostDefence）
19. 注册表保护（RegProtect/HostDefence）
20. 强制访问控制（HostDefence）
21. 系统防护（SysGuard）
22. 软件安装异常（SafetyStore）
23. 防火墙事件（FireWall）
24. 客户端操作（Admin）

*外设控制子类（9 种，共享 CMDID=204，区别在 UsbType 字段）：*
25~33. 禁用 USB接口 / 手机平板 / CDROM / 无线网卡 / USB网卡 / 软盘 / 蓝牙 / 串口 / 并口

## 🔥 压力测试场景

### 目标

| 指标 | 目标值 |
|---|---|
| 模拟在线客户端数 | 3000 |
| 日志发送速率 | 6000 条/秒（每客户端 2/s）|

### 推荐方案：3 台主机分工

每台主机负责 1000 个客户端，三台合计达到目标。

| 主机 | 起始序号 | 数量 | IP 段 | EPS |
|---|---|---|---|---|
| 主机 A | 1 | 1000 | 192.168.0.1 | 2000 |
| 主机 B | 1001 | 1000 | 192.168.1.1 | 2000 |
| 主机 C | 2001 | 1000 | 192.168.2.1 | 2000 |

> ⚠️ 三台主机必须注册**不同序号范围**的客户端，否则同一 ClientId 的心跳会互相踢掉 session。

### 每台主机操作步骤

```
1. 配置"平台地址"为目标平台 IP（三台相同）
2. 注册区：按上表设置起始序号 / 数量，执行"并发注册"（一次性，结果保存在 Clients.log）
3. 启动 TCP 心跳，保持 1000 个客户端在线
4. 日志发送设置：
     客户端个数     = 1000
     每客户端每秒   = 2
     每客户端总条数 = 视测试时长（10 分钟填 1200，持续测试填 999999）
     并发连接数     = 50（可逐步调高，观察失败率）
     ☑ 压测模式
5. 三台同时点"添加任务"
```

### 日志发送模式说明

| | 模拟模式（默认） | 压测模式（勾选）|
|---|---|---|
| **连接方式** | 每条日志新建独立 TLS 连接 | 连接复用，多条日志共享连接 |
| **单机上限** | ~50 客户端（~100 EPS） | 3000+ 客户端（6000+ EPS）|
| **用途** | 验证与真实客户端行为兼容性 | 测试平台日志处理吞吐上限 |
| **平台感知** | — | 请求内容完全相同，平台无法区分 |

### 失败排查

| 日志文件 | 内容 |
|---|---|
| `logs/logsend-failures-*.log` | 日志发送失败分类统计（≤ 50 MB） |
| `logs/logsend-failures-https-*.log` | HTTPS 失败详情（含状态码和响应体） |
| `logs/heartbeat_monitor_*.log` | 心跳断线原因（7 种） |
| `logs/throughput-*.log` | 可选：实测 EPS 统计（需启用 `Metrics.Enable()`） |
| SSH 收集 | TcpDiagWindow → "SSH 日志收集" → 输出到 `debug_collect_yyyyMMdd_HHmmss/` |

---

## 📖 下一步建议

**协议完善：**
- Win2012R2 兼容性（见 `feature/win2012r2-compat` 分支）
- 策略字段覆盖率验证（对齐平台后端期望的全部 JSON 字段）

**稳定性 & 压测：**
- `scripts/estimate_client_limit.ps1` 辅助评估单机客户端上限

**UI 增强：**
- 任务面板列宽自适应 / 支持导出 CSV
- 实时折线图（EPS、心跳在线数随时间变化）

## 🛠️ 技术栈

- .NET 8.0 (WPF)
- C++ DLL（NativeEngine：非阻塞 Winsock + OS 线程 + zlib，NativeSender：PT 协议打包）
- CMake（C++ 构建，MSVC x64）
- 自包含单文件发布（win-x64，NativeEngine.dll + NativeSender.dll 内嵌）
- 持久化：JSON 文件
- 网络协议：TCP（PT 协议心跳/威胁日志）、HTTPS（注册/通用日志/白名单/策略）

## 📁 项目结构

```
src/
   SimulatorApp/          # WPF 主应用（含子窗口：RegAdvanced/PortAdvanced/TcpDiag/SshLog/LogCategoryHelp）
   SimulatorLib/          # 核心业务逻辑（Workers/Protocol/Models/Network/Persistence）
   NativeEngine/          # C++ DLL：非阻塞 socket 心跳 + 威胁日志发送引擎
   NativeSender/          # C++ DLL：PT 协议打包
   SimulatorRunner/       # CLI 运行器
   TestReceiver/          # 测试接收服务器
   DevTools/              # 开发辅助工具
scripts/                  # 构建和发布脚本
docs/                     # 文档和经验总结
artifacts/                # 发布产物输出
external/                 # IEG 原始 C++ 源码参考（含 zlibstat.lib）
```

## 📝 重要说明

- **默认开发分支：`main`**
- `Clients.log` 和 `config.json` 在程序首次运行时自动生成
- 修改代码后需重新执行 `publish_simulatorapp.ps1` 更新 exe（脚本自动 bump 版本号 + 构建 C++ DLL + 打包 + git commit/push）
- Git 代理配置（如需要）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- 版本演进脉络：`v2.0.0`（UI完善）→ `v2.1.0`（11种日志）→ `v2.2.0`（断线监控）→ `v2.3.0`（外设/EPS/压测模式）→ `v2.4.0`（多IP/诊断窗口）→ `v3.0.0`（威胁检测/任务面板/策略接收）→ `v3.1.0`（TCP流复用/17+日志类型）→ `v3.2.0`（写锁）→ `v3.3.0`（锁超时修复）→ `v3.4.0`（SSH诊断/OS切换）→ `v3.5.0`（协议对齐）→ `v3.6.x`（TCP可靠性/日志分类说明/网口事件/UI清理）→ `v3.7.x`（Channel→直写→NativeEngine C++ 引擎，彻底解决 EPS 振荡和心跳稳定性）→ **`v3.8.x`**（C++ 热路径零 P/Invoke、策略回调、单文件打包、日志线程重连恢复、漏洞防护攻击方向修正，当前）
