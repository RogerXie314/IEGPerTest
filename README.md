# IEGPerTest — WLServerTest 压测工具（`main` 分支）

`main` 分支主推 **WLServerTest** —— 基于老 IEG 代码树持续翻新的 MFC 桌面压测工具（x64 单 exe），用于模拟客户端注册、TCP 心跳、TCP/HTTPS 威胁日志上报、白名单上传。

> 仓库还有两条 SimulatorApp（C# WPF）架构分支，见下方[「仓库分支结构」](#-仓库分支结构重要)。

---

## 🆕 WLServerTest **V6.5**（2026-07-20，latest）

发布路径：`artifacts/WLServerTestPublish/`。

### V6.5 核心功能

**客户端模拟**

| 功能 | 说明 |
|------|------|
| 客户端注册 | 并发批量注册，devid 从服务端获取后持久化到 INI，重启自动恢复 |
| TCP 心跳 | 按 ClientID 保活，独立控制启停，不干扰日志发送任务 |
| 威胁日志发送 | TCP/HTTPS 双通道，File/ProcStart/Reg/DLL Load/WinEventLog 五种类型可分别勾选过滤 |
| 白名单上传 | 批量上报文件白名单到平台 |
| 攻击报文回放 | 从抓包样本加载、按 stream 拆分回放 |

**V6.5 版本特性**

- **devid 按客户端持久化**：注册 devid 写入 `[DevID]` 段 INI，重启免重注册；协议头自动注入
- **威胁日志类型复选框过滤**：五种日志类型可按位掩码单独开关，未勾选的零开销跳过
- **ComputerIP 注入**：所有威胁日志 JSON `CMDContent` 中包含对应客户端 IP
- **Debug 日志开关**：界面复选框控制 `OutputDebugString` 诊断输出，不勾选零性能影响
- **"结束任务"精准控制**：仅停止日志发送任务行，不误伤共用列表的心跳任务
- **协议健壮性修复**：JSON `\D` 非法转义 / DLL Load 截断缺 `]` / 注册 devid=0 等问题全部修复

### V6.3 重要修复

- DLL Load JSON fastjson 转义兼容
- 协议头 `nDeviceID` 正确取值
- JSON 边界截断问题

**部署**（无需安装运行时）：把 `artifacts/WLServerTestPublish/` 整目录拷贝到目标机器，双击 `WLServerTest.exe`。

Tooltips 速查：

- **Debug 日志**：配合 Sysinternals DebugView（`Dbgview.exe`，`Capture → Capture Global Win32`），勾选复选框后实时查看 JSON 注入诊断
- **注册**：`[控制] → [注册]` 选"并发注册"，devid 自动写入 `[DevID]` 段 INI；Reset 后清空 devid 缓存
- **日志发送**：可同时勾选多种类型（Exec/Script/Reg/DLL/WinEvent），`dwSubTypes` 位掩码过滤

---

### 编译 WLServerTest

**推荐：一键脚本**（自动探测 VS、自动拷贝产物到 `artifacts/WLServerTestPublish/`）

```powershell
pwsh -File scripts\build_wlservertest.ps1
# 选项：-Configuration Debug | -SkipPublish | -Clean
# 环境变量：$env:MSBUILD_EXE / $env:WLNETCOMM_DLL 可覆盖默认探测
```

脚本会自动按以下优先级查找 MSBuild：`$env:MSBUILD_EXE` → PATH → `vswhere` (VS Installer 标准位置，覆盖 2017/2019/2022 + Community/Pro/Enterprise/BuildTools) → 8 个常见绝对路径兜底。仓库路径基于脚本自身位置解析，与 cwd 无关。

**手动 MSBuild**（备用）：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  external\IEG_Code\code\WLServerTest\WLServerTest.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m
```

输出在 `external/IEG_Code/code/WLServerTest/x64/Release/WLServerTest.exe`。发布时连同 `WLNetComm.dll`（仓库 `external/IEG_Code/code/bin/Release/x64/`）、`RawPacketEngine.dll`、`WLServerTest.ini` 一起拷至发布目录。

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
- [docs/分析-WLServerTest与NativeRunner代码差异及压力稳定性评估.md](docs/分析-WLServerTest与NativeRunner代码差异及压力稳定性评估.md)
- [docs/project_dashboard.html](docs/project_dashboard.html) — 项目看板
- [tools/README.md](tools/README.md) — Python 白名单解析工具（CLI + GUI）

---

## 🌿 仓库分支结构（重要）

`main` 上的代码 ≠ 全部架构。三套压测/模拟实现分别活在不同分支，**互不合并**，便于研究对比：

| 分支 | 主推工具 | 架构 | 最新版本 |
|---|---|---|---|
| **`main`**（当前） | **WLServerTest** | MFC + 纯原生 C++（基于老 IEG 代码树优化） | **V6.5** |
| [`simulator-subprocess`](../../tree/simulator-subprocess) | SimulatorApp | C# WPF + NativeRunner.exe 子进程（stdio 管道 IPC，C++ 与 .NET GC 隔离） | v3.9.7 |
| [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) | SimulatorApp | C# WPF + NativeSender.dll 同进程（P/Invoke） | v3.7.31 |

要编译/修改 SimulatorApp，请切到对应架构分支：那里有完整的 `IEGPerTest.sln`、`build_all.bat`、`src/SimulatorApp/`、文档等。`main` 分支不再保留 SimulatorApp 的源码和构建脚本。

---

## 📁 仓库结构（main 分支）

```
README.md                       # 本文件

external/                       # WLServerTest 源码 + 老 IEG 代码参考库
  ├── IEG_Code/code/
  │     ├── WLServerTest/       # ★ 主推工具：MFC 压测工具源码
  │     └── （其余 IEG 模块作为依赖参考）
  ├── 攻击报文/                  # 抓包样本
  └── xiaobing/                  # 第三方参考

artifacts/
  └── WLServerTestPublish/      # WLServerTest V6.5 发布产物（exe + dll + ini）

docs/                           # 文档（项目实施文档 / 项目看板 等）
tools/                          # 通用：Python 白名单解析工具（CLI + GUI）
scripts/
  └── build_wlservertest.ps1    # 一键编译脚本

archive/
  └── simulator-app/            # SimulatorApp 历史构建脚本归档
```

---

## 🛠️ 技术栈（main 分支）

| 模块 | 技术 |
|---|---|
| WLServerTest | MFC + VS2022 + C++17 + WLNetComm.dll(x64) + RawPacketEngine |
| 持久化 | INI / Clients.log |

---

## 📝 重要说明

- **分支策略**：`main` 长期主推 WLServerTest；SimulatorApp 两种架构活在专用分支上
- **Git 代理**（如需）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- **版本号需手动改**：`WLServerTest.rc`，脚本不自增

## 📋 历史版本

- **WLServerTest**（main）：V6.5（devid 持久化+威胁日志过滤+ComputerIP 注入+Debug 开关）/ V6.3（DLL Load 转义+devid 协议头+JSON 截断修复）/ V6.2（清理+文档整理）/ V6.1（攻击报文多 stream 修复）/ V6.0（注册 doPost 弹窗移除+RawPacketEngine 入库）/ V5.9（OPT 调度大修+日志路由对齐）/ V5.8（攻击报文 UI 复刻）/ V5.7（短连接漏发修复）/ V5.6（IEG/EDR 联动）/ V5.5（计数器原子化+卡片化）/ V5.2（崩溃修复+心跳时长）/ V5.1 / V5.0
- **SimulatorApp**（其它分支）：详见 [`simulator-subprocess`](../../tree/simulator-subprocess) 与 [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) 自带 CHANGELOG

详见 [docs/项目实施文档.md](docs/项目实施文档.md) 与 [archive/simulator-app/RELEASE_NOTES_v3.9.7.md](archive/simulator-app/RELEASE_NOTES_v3.9.7.md)。
