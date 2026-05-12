# IEGPerTest — IEG/EDR 压测工具集

本仓库包含两套**互补的**客户端压测工具，针对不同测试场景：

| 工具 | 形态 | 适用场景 | 当前版本 |
|---|---|---|---|
| **WLServerTest** | MFC 桌面工具（x64 单 exe） | 异机部署、传统压测、单机 500 客户端、人工/可视化操作 | **V5.5**（2026-05-11）|
| **SimulatorApp** | C# WPF + NativeRunner 子进程 | 多机分布式压测、3000 客户端联机、CI/脚本化、攻击报文发送 | v3.9.6（2026-04-27）|

二者都模拟客户端注册、TCP 心跳、HTTPS 日志、白名单上传，但实现路径完全独立，**互不依赖**：
- WLServerTest = 老 IEG 代码树（`external/IEG_Code/`）的 MFC 工具，逐版本翻新
- SimulatorApp  = 全新 .NET 8 WPF 应用 + C++ 子进程引擎（NativeRunner.exe）

---

## 🆕 WLServerTest **V5.5**（2026-05-11，主推）

独立 MFC 压测工具，发布路径：`artifacts/WLServerTestPublish/`。

**V5.5 亮点**：
- 修复多线程注册时"已注册"计数器丢更新的概率 bug（`InterlockedIncrement`）
- "任务与状态"区改为 4 张卡片式分组（注册/心跳/日志/白名单）
- 主窗口三列布局优化：左列收窄、中列左移 25 DLU、右列加宽，给"任务面板/日志输出"更多空间
- 修复异机 500 客户端启动约 1 分钟崩溃（`WLNetComm.dll` 加载失败防御）
- 心跳设置新增"心跳时长(分钟)"输入框，默认 7200

**部署**（无需安装运行时）：把 `artifacts/WLServerTestPublish/` 整目录拷贝到目标机器，双击 `WLServerTest.exe`。

详见：
- [CHANGELOG_v5.5.md](CHANGELOG_v5.5.md)
- [CHANGELOG_v5.2.md](CHANGELOG_v5.2.md)
- [docs/项目实施文档.md](docs/项目实施文档.md)

### 编译 WLServerTest

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  external\IEG_Code\code\WLServerTest\WLServerTest.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m
```

输出在 `external/IEG_Code/code/WLServerTest/x64/Release/WLServerTest.exe`。发布时连同 `WLNetComm.dll`（仓库 `external/IEG_Code/code/bin/Release/x64/`）、`RawPacketEngine.dll`、`WLServerTest.ini` 一起拷至发布目录。

---

## 🧪 SimulatorApp（C# WPF + NativeRunner 子进程）

适合分布式压测、CI 集成、攻击报文发送等场景。当前版本 v3.9.6。

### 进程分离架构

```
SimulatorApp.exe (C# WPF 界面)
    ↕ stdin/stdout 管道
NativeRunner.exe (C++ 心跳/日志引擎，独立进程)
RawPacketEngine.dll (C++ 原始报文发送，主进程加载)
```

- **C# WPF**：界面、配置、状态显示、白名单解析、SSH 收集
- **C++ NativeRunner**：心跳 TCP 长连接 + 威胁日志 + HTTPS 通用日志，原生运行，零 GC 干扰
- **故障隔离**：子进程崩溃不影响主界面；主界面 GC 暂停不影响 C++ 心跳

### 快速开始

```powershell
# 一键构建（C++ DLL + .NET publish + 拷贝产物）
.\build_all.bat

# 或：仅发布 SimulatorApp 到 artifacts/SimulatorAppPublish/
.\scripts\publish_simulatorapp.ps1
```

### 部署到无 .NET 环境的机器

发布产物为**自包含 EXE**（含 .NET 8 运行时），将 `artifacts/SimulatorAppPublish/` 整体拷贝即可：

```
SimulatorApp.exe              # 主程序（约 155 MB）
NativeRunner.exe              # C++ 心跳/日志子进程
NativeSender.dll              # PT 协议打包
RawPacketEngine.dll           # 原始报文发送（需 Npcap）
wpfgfx_cor3.dll …             # WPF 原生渲染库
config.json                   # 配置文件
```

> 攻击报文发送需额外安装 [Npcap](https://npcap.com/)；未安装时弹友好提示，其他功能不受影响。

### 核心能力

- ✅ 23 种日志类型 + 9 种外设子类（USB/光驱/无线/蓝牙/串并口 等）
- ✅ 威胁检测（TCP 长连接，CMDID=21，~6000 EPS）
- ✅ 攻击报文发送（MS08-067 / MS17-010 / MS20-796 内置 + `.etc`/`.pcap` 导入）
- ✅ 任务面板：所有后台任务统一 DataGrid，实时显示状态/进度/计数
- ✅ 客户端版本管理（增删改 + 持久化 JSON）
- ✅ 白名单 V2/V3/V4 预览 + CSV 导出
- ✅ SSH 一键收集平台日志 + RTT 偏差补偿
- ✅ TCP 诊断（端口范围 / TIME_WAIT 推荐）
- ✅ stdout 多线程互斥（500 线程并发安全）

详见：[docs/PROCESS_ARCHITECTURE.md](docs/PROCESS_ARCHITECTURE.md)、[docs/QUICK_START.md](docs/QUICK_START.md)、[docs/BUILD_INSTRUCTIONS.md](docs/BUILD_INSTRUCTIONS.md)

---

## 🔥 推荐压测方案：3 台主机 × 1000 客户端 = 3000 在线

| 主机 | 起始序号 | 数量 | IP 段 | EPS |
|---|---|---|---|---|
| 主机 A | 1 | 1000 | 192.168.0.1 | 2000 |
| 主机 B | 1001 | 1000 | 192.168.1.1 | 2000 |
| 主机 C | 2001 | 1000 | 192.168.2.1 | 2000 |

> ⚠️ 三台必须用**不同序号范围**，否则同 ClientId 心跳会互踢 session。

每台主机：
1. 配平台地址（三机相同）
2. 注册区按表设置 → "并发注册"
3. 启动 TCP 心跳保持 1000 客户端在线
4. 日志：客户端 1000 / 每秒每端 2 / 总条数视测试时长（10 分钟 = 1200）
5. 三机同时点"添加任务"

---

## 📚 文档与工具

- [docs/项目实施文档.md](docs/项目实施文档.md) — 完整变更记录、协议路由表、调试技巧
- [docs/经验教训-日志类型实现.md](docs/经验教训-日志类型实现.md)
- [docs/project_dashboard.html](docs/project_dashboard.html) — 项目看板
- [tools/README.md](tools/README.md) — Python 白名单解析工具（CLI + GUI）
- [scripts/estimate_client_limit.ps1](scripts/estimate_client_limit.ps1) — 单机客户端上限评估

---

## 🛠️ 技术栈

| 模块 | 技术 |
|---|---|
| WLServerTest | MFC + VS2022 + C++17 + WLNetComm.dll(x64) |
| SimulatorApp | .NET 8 WPF（自包含发布 win-x64）|
| NativeEngine / NativeRunner / NativeSender | C++ + 非阻塞 Winsock + zlib + PT 协议 |
| RawPacketEngine | C++ + Npcap / pcap |
| 持久化 | JSON 文件（`Clients.log`、`config.json`、`client_versions.json`）|

---

## 🌿 仓库分支结构（重要）

`main` 上的代码 ≠ 全部架构。三套压测/模拟实现分别活在不同分支：

| 分支 | 主推工具 | 架构 | 最新版本 |
|---|---|---|---|
| **`main`**（当前） | **WLServerTest** | MFC + 纯原生 C++（基于老 IEG 代码树优化） | **V5.5** |
| [`simulator-subprocess`](../../tree/simulator-subprocess) | SimulatorApp | WPF + NativeRunner.exe 子进程（stdio 管道 IPC） | v3.9.7 |
| [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) | SimulatorApp | WPF + NativeSender.dll 同进程（P/Invoke） | v3.7.31 |

> `main` 上根目录虽然还能看到 `src/`（SimulatorApp 代码），但这些代码**仅作为对照参考**，不再随 `main` 演进。要编译/修改 SimulatorApp，请切到对应架构分支。SimulatorApp 相关的构建脚本（sln / build_all.bat / publish 脚本等）已搬到 [`archive/simulator-app/`](archive/simulator-app/)。

---

## 📁 仓库结构（main 分支）

```
README.md                       # 本文件
CHANGELOG_v5.5.md               # WLServerTest V5.5 变更日志
CHANGELOG_v5.2.md               # WLServerTest V5.2 变更日志
config.ini.example              # WLServerTest 配置样例

external/                       # WLServerTest 源码 + 老 IEG 代码参考库
  ├── IEG_Code/code/
  │     ├── WLServerTest/       # ★ 主推工具：MFC 压测工具源码
  │     └── （其余 IEG 模块作为依赖参考）
  ├── 攻击报文/                  # 抓包样本
  └── xiaobing/                  # 第三方参考

artifacts/
  └── WLServerTestPublish/      # WLServerTest V5.5 发布产物（exe + dll + ini）

docs/                           # 文档（聊天记录 / 项目实施文档 / 项目看板 等）
tools/                          # 通用：Python 白名单解析工具（CLI + GUI）
scripts/
  └── estimate_client_limit.ps1 # 单机客户端上限评估

src/                            # ⚠ SimulatorApp 代码，main 上不再维护
                                # 编译请切到 simulator-subprocess 或 simulator-inproc-dll 分支
                                # 见 src/README.md

archive/
  └── simulator-app/            # SimulatorApp 历史构建脚本归档
        ├── IEGPerTest.sln
        ├── build_all.bat
        ├── export_dependencies.ps1
        ├── publish_simulatorapp.ps1
        ├── dependencies_export/
        └── RELEASE_NOTES_v3.9.7.md
```

---

## 📝 重要说明

- **分支策略**：`main` 长期主推 WLServerTest；SimulatorApp 两种架构活在专用分支上，互不合并
- **Git 代理**（如需）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- **版本号需手动改**：`WLServerTest.rc`（main）/ `SimulatorApp.csproj`（其它分支），脚本不自增

## 📋 历史版本

- **WLServerTest**：V5.5（卡片化+布局重排）/ V5.2（崩溃修复+心跳时长）/ V5.1 / V5.0
- **SimulatorApp**：v3.9.6（NativeRunner 进程架构 + stdout 互斥）/ v3.9.5（版本管理+Server 识别）/ v3.9.3（白名单预览）/ v3.9.2（Npcap 友好提示）

详见各 `CHANGELOG_*.md` 与 [archive/simulator-app/RELEASE_NOTES_v3.9.7.md](archive/simulator-app/RELEASE_NOTES_v3.9.7.md)。

