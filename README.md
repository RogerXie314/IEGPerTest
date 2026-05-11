# IEGPerTest Simulator

IEG/EDR 客户端模拟器：用于模拟客户端注册、心跳、日志发送与白名单上传的 WPF 桌面工具（.NET 8）。

## � 最新版本：WLServerTest **V5.5**（2026-05-11）

独立 MFC 压测工具 WLServerTest 已迭代至 V5.5，发布路径：`artifacts/WLServerTestPublish/`。

**V5.5 亮点**：
- 修复多线程注册时"已注册"计数器丢更新的概率 bug（改用 `InterlockedIncrement`）
- "任务与状态"区改为 4 张卡片式分组（注册/心跳/日志/白名单），信息密度更高
- 主窗口三列布局优化：左列收窄、中列左移 25 DLU、右列加宽，给"任务面板/日志输出"更多空间
- 修复异机 500 客户端启动约 1 分钟崩溃（`WLNetComm.dll` 加载失败防御）
- 心跳设置新增"心跳时长(分钟)"输入框，默认 7200

详见 [CHANGELOG_v5.5.md](CHANGELOG_v5.5.md)、[CHANGELOG_v5.2.md](CHANGELOG_v5.2.md)。

## 🎯 进程分离架构（SimulatorApp）

**v4.0 采用全新的进程分离架构，彻底解决300客户端高并发不稳定问题！**

### 架构对比

| 架构 | v3.x（DLL方式） | v4.0（进程方式） |
|------|----------------|-----------------|
| **稳定性** | ❌ 300客户端不稳定 | ✅ 100%稳定 |
| **GC影响** | ❌ C#的GC暂停影响C++线程 | ✅ 完全隔离，无影响 |
| **故障隔离** | ❌ 崩溃相互影响 | ✅ 进程独立 |
| **调试** | ❌ 困难 | ✅ 容易 |

### 新架构说明

```
SimulatorApp.exe (C# WPF界面)
    ↓ 配置文件 + stdout
WLServerTest.exe (C++压测内核，独立进程)
```

- **C# WPF**：负责界面、配置、状态显示
- **C++ 进程**：负责实际压测（注册、心跳、日志），原生运行，无GC干扰

详见：
- [进程架构说明](docs/PROCESS_ARCHITECTURE.md)
- [快速开始](docs/QUICK_START.md)
- [编译说明](docs/BUILD_INSTRUCTIONS.md)

## 📚 核心文档

- **实施文档**：[docs/项目实施文档.md](docs/项目实施文档.md)（含完整变更记录、调试技巧、协议路由表）
- **经验教训**：[docs/经验教训-日志类型实现.md](docs/经验教训-日志类型实现.md)
- **项目看板**：[docs/project_dashboard.html](docs/project_dashboard.html)
- **白名单解析工具**：[tools/README.md](tools/README.md)（.wl文件解析和预览工具，已验证 ✅）

## 🚀 快速开始

### 一键编译（推荐）

```powershell
# 编译C++和C#，并复制到deploy目录
.\build_all.bat
```

### 测试C++控制台模式

```powershell
# 测试5个客户端，运行1分钟
.\test_console.bat
```

### 开发环境运行

```powershell
# 构建并运行（需要 .NET 8 SDK）
dotnet build
dotnet run --project src/SimulatorApp
```

### 发布独立可执行文件

```powershell
# 常规发布：构建 C++ + 打包 + 复制到 artifacts/SimulatorAppPublish/
.\scripts\publish_simulatorapp.ps1

# 全量发布（仅当 SimulatorRunner / TestReceiver / DevRunner 有代码改动时才用）
.\scripts\publish_all.ps1
```

发布产物位于 `artifacts/SimulatorAppPublish/` 目录。**版本号需手动修改 `SimulatorApp.csproj`**，脚本不自动递增。

### 部署到无 .NET 环境的机器

发布的应用是**自包含 EXE**，将以下文件整体拷贝到目标机器：

```
SimulatorApp.exe              # 主程序（约 155 MB，含 .NET 运行时）
WLServerTest.exe              # C++ 压测内核（独立进程）
NativeSender.dll              # C++ PT 协议打包
RawPacketEngine.dll           # C++ 攻击报文发送引擎（需 Npcap 驱动）
wpfgfx_cor3.dll               # WPF 原生渲染库（及同目录其他 _cor3.dll）
config.json                   # 配置文件
```

在任何 Windows x64 机器上双击即可运行，无需安装 .NET。

> **注意**：攻击报文发送功能需额外安装 [Npcap](https://npcap.com/) 驱动；未安装时会弹出友好提示，其余功能不受影响。

> 程序首次运行时会在同目录生成 `config.json` 和 `Clients.log`。

## ✅ 当前状态（v3.9.6）

### 核心架构

- **NativeRunner C++ 独立进程（v3.9.6）**：心跳和威胁日志发送迁移到独立子进程 `NativeRunner.exe`，与 SimulatorApp.exe 通过 stdin/stdout 管道通信，彻底隔离 .NET GC 对 C++ 线程的影响
- **心跳与日志解耦**：心跳和日志发送可独立启动/停止，互不干扰
- **stdout 多线程互斥（v3.9.6 修复）**：NativeRunner 所有 stdout 输出通过 `CRITICAL_SECTION` 保护，防止 500 线程并发写出时字节级交错导致 C# parse 失败

### 已完成功能

- ✅ **NativeRunner 独立进程架构**（v3.9.6）：C++ 子进程负责心跳+日志，通过 stdin/stdout 管道与 C# 通信
- ✅ **心跳/日志解耦**（v3.9.6）：心跳启动后可独立开始/停止日志发送，不影响心跳连接
- ✅ **stdout 竞争 BUG 修复**（v3.9.6）：修复 500 线程并发写 REREGISTER/STATS 时字节交错导致 C# parse 失败、客户端永久掉线的问题
- ✅ **注册重置按钮**（v3.9.6）：清空 Clients.log，恢复注册表单默认值
- ✅ **客户端版本管理**（v3.9.5）：支持增删改客户端版本列表，配置保存到 JSON 文件，Windows/Linux 版本分别管理，支持恢复默认版本
- ✅ **Windows Server 识别修复**（v3.9.5）：修复 Windows Server 2012 R2 等系统显示为版本号的问题，支持识别 Server 2022/2019/2016/2012 R2/2012/2008 R2
- ✅ **白名单文件预览**（v3.9.3）：点击"预览白名单"按钮可查看.wl文件内容，显示白名单数量、列表、支持搜索和导出，支持V2/V3/V4格式
- ✅ **白名单文件解析工具**（tools/ 目录）：Python 工具，可读取 .wl 文件并显示白名单列表及数量，支持命令行和 GUI 两种界面，支持搜索和导出功能
- ✅ **攻击报文发送**（RawPacketEngine C++ DLL + WPF 独立子窗口 v3.9.2）：内置 MS08-067/MS17-010/MS20-796 三种漏洞利用报文，支持导入 `.etc`/`.pcap`、字段编辑、源IP变化规则（FieldRule）、多 Stream Round-Robin 发送、PPS/间隔/最大速率控制；界面采用步骤引导两栏布局 + 紧凑统计卡片，RawPacketEngine.dll 与 EXE 同目录部署
- ✅ **Npcap 未安装时友好提示**：检测失败时显示错误对话框并附下载地址，不再崩溃
- ✅ 客户端注册、心跳、白名单上传（PT/HTTP/HTTPS）
- ✅ **TCP 心跳稳定在线**（NativeRunner 非阻塞 socket，每客户端 1 个 OS 线程）
- ✅ **23 种日志类型**完整实现（详见下方列表），JSON 字段名对齐平台 `WLJsonParse.cpp`
- ✅ **威胁检测 5 种子类**（TCP 长连接，CMDID=21）：JSON+zlib+PT 打包全在 C++ 内完成
- ✅ **威胁检测命中率对齐**：`bHit=1/71`（每 71 包 1 包命中），miss 数据使用无害进程名
- ✅ **外设控制 9 种子分类**（USB接口/手机平板/光驱/无线网卡/USB网卡/软盘/蓝牙/串口/并口）
- ✅ **日志发送 EPS 精确控制**（deadline 模式）
- ✅ **防 TLS 握手风暴**：>100 客户端时错峰窗口扩大到 10s
- ✅ **自包含发布**（EXE ~155 MB + WPF 原生 DLL + NativeRunner.exe + RawPacketEngine.dll，同目录部署，无需安装 .NET）
- ✅ 持久化存储（`Clients.log`、`config.json`）
- ✅ **分通道独立配置**：HTTPS 与威胁检测各自独立配置客户端数和 EPS
- ✅ **任务面板（TaskPanel）**：所有后台任务统一 DataGrid，实时显示状态/进度/成功/失败计数
- ✅ **策略接收（PolicyReceiveWorker）**：TCP cmdId=17 触发 HTTPS 拉取策略 → 回 ACK
- ✅ **白名单轮换**：随机轮换，15 分钟周期，HTTPS multipart 上传 + clientScanStatus 通知
- ✅ **注册版本选择**：ComboBox 选择客户端版本（默认 V300R011C01B030）
- ✅ **客户端 OS 类型切换**：注册/心跳时可选 Windows / Linux
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
5. 三台同时点"添加任务"
```

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
- Win2012R2 兼容性
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
- 自包含发布（win-x64，EXE + WPF 原生 DLL + C++ DLL 同目录）
- C++ DLL（RawPacketEngine：Npcap/pcap 原始报文发送引擎）
- 持久化：JSON 文件
- 网络协议：TCP（PT 协议心跳/威胁日志）、HTTPS（注册/通用日志/白名单/策略）

## 📁 项目结构

```
src/
   SimulatorApp/          # WPF 主应用（含子窗口：RegAdvanced/PortAdvanced/TcpDiag/SshLog/LogCategoryHelp/RawPacket）
   SimulatorLib/          # 核心业务逻辑（Workers/Protocol/Models/Network/Persistence/RawPacket）
   NativeEngine/          # C++ DLL：非阻塞 socket 心跳 + 威胁日志发送引擎
   NativeSender/          # C++ DLL：PT 协议打包
   RawPacketEngine/       # C++ DLL：Npcap 原始报文发送引擎（攻击报文发送模块）
   SimulatorRunner/       # CLI 运行器
   TestReceiver/          # 测试接收服务器
   DevTools/              # 开发辅助工具
tools/                    # 白名单文件解析工具（Python）
scripts/                  # 构建和发布脚本
docs/                     # 文档和经验总结
artifacts/                # 发布产物输出
```

## 📝 重要说明

- **唯一分支：`main`**
- `Clients.log` 和 `config.json` 在程序首次运行时自动生成
- 修改代码后执行 `publish_simulatorapp.ps1` 重新构建（构建 C++ DLL + dotnet publish + 复制 DLL），**版本号手动修改 `SimulatorApp.csproj`**
- Git 代理配置（如需要）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- 版本演进脉络：... → **`v3.9.x`**（v3.9.6 NativeRunner 进程架构 + stdout 竞争修复；v3.9.5 客户端版本管理 + Windows Server 识别修复；v3.9.3 白名单预览功能；v3.9.2 Npcap 崩溃修复 + 打包脚本修复 WPF DLL 丢失，当前）

## 📋 版本历史

### v3.9.6 — 2026-04-27：NativeRunner 进程架构 + 心跳稳定性修复

**架构变更**：
- **迁移到 NativeRunner 独立进程**：C++ 心跳+威胁日志引擎从 DLL 迁移为独立可执行文件 `NativeRunner.exe`，通过 stdin/stdout 管道与 C# 通信，彻底隔离 .NET GC 对 C++ 线程的影响
- **心跳与日志解耦**：心跳启动后可独立开始/停止日志发送，互不干扰；UI 上"开始心跳"和日志任务分开操作

**BUG 修复**：
- **stdout 多线程竞争修复（关键）**：NativeRunner 500 个心跳线程同时向 stdout 写 `REREGISTER` 消息时，字节级交错导致 C# 收到合并行（如 `STATS|...|NoReg=460REREGISTER|5|clientId`），`int.Parse` 抛异常，REREGISTER 事件丢失，客户端永久无法重注册。修复方案：全局 `CRITICAL_SECTION g_csStdout` + `PrintLine()` 包装所有 stdout 输出
- **LogSendOk/Fail 类型溢出**：从 `int` 改为 `long`（C#）/ `std::atomic<int64_t>`（C++），防止高 EPS 长时间运行溢出
- **注册统计在 STARTLOG 时重置**：每次开始日志任务时清零 LogSendOk/Fail 计数

**新增功能**：
- **注册重置按钮**：注册设置区"高级…"旁新增"重置"按钮，清空 Clients.log 并恢复注册表单默认值（前缀 `Client-`、起始编号 `1`、起始IP `192.168.0.1`、数量 `5`）



### v3.9.5 — 2026-04-17：客户端版本管理 + Windows Server 识别修复

**新增功能**：
- **客户端版本管理**：新增版本管理窗口，支持增删改客户端版本列表，配置保存到 JSON 文件（%AppData%/SimulatorApp/client_versions.json），Windows/Linux 版本分别管理，支持恢复默认版本
- **Windows Server 识别修复**：修复 Windows Server 2012 R2 显示为 "Microsoft Windows 6.3.9600" 的问题，增强 OsInfo.GetWindowsVersionName() 方法，支持识别 Server 2022/2019/2016/2012 R2/2012/2008 R2

**技术实现**：
- 新增 ClientVersionConfig 类负责版本配置读写
- 新增 VersionManagementWindow 和 VersionManagementViewModel
- MainViewModel 从配置文件加载版本列表，不再硬编码
- 使用 Environment.OSVersion.Version 识别 Windows Server 版本号

### v3.9.3 — 2026-04-16：白名单预览功能 + UI布局修复

**新增功能**：
- **白名单预览窗口**：新增 WhitelistPreviewWindow 和 WhitelistPreviewViewModel，支持读取 V2/V3/V4 格式白名单文件，显示白名单总数、文件路径、版本信息，支持搜索过滤（路径/哈希）和导出为 CSV 格式
- **白名单上传功能增强**：上传完成后在状态 JSON 中添加 WLFileCount 字段，平台可显示白名单数量；修复文件名保留问题
- **心跳功能优化**：新增 RegisteredClientCount 属性，从持久化数据加载已注册客户端数量，修复重启后"开始心跳"按钮不可用的问题

**UI 修复**：
- 修复右侧列布局 bug（DockPanel 从 Grid.Column=1 改为 Grid.Column=2）
- 白名单上传设置按钮优化（添加"预览白名单"按钮，调整按钮宽度和字体大小）

**文档**：
- 新增 docs/白名单预览功能说明.md
- 新增 docs/白名单预览功能测试指南.md
