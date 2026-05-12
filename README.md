# IEGPerTest — WLServerTest 压测工具（`main` 分支）

`main` 分支主推 **WLServerTest** —— 基于老 IEG 代码树持续翻新的 MFC 桌面压测工具（x64 单 exe），用于模拟客户端注册、TCP 心跳、HTTPS 日志、白名单上传。

> 仓库还有两条 SimulatorApp（C# WPF）架构分支，见下方[「仓库分支结构」](#-仓库分支结构重要)。

---

## 🆕 WLServerTest **V5.5**（2026-05-11，latest）

发布路径：`artifacts/WLServerTestPublish/`。

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
- [scripts/estimate_client_limit.ps1](scripts/estimate_client_limit.ps1) — 单机客户端上限评估

---

## 🌿 仓库分支结构（重要）

`main` 上的代码 ≠ 全部架构。三套压测/模拟实现分别活在不同分支，**互不合并**，便于研究对比：

| 分支 | 主推工具 | 架构 | 最新版本 |
|---|---|---|---|
| **`main`**（当前） | **WLServerTest** | MFC + 纯原生 C++（基于老 IEG 代码树优化） | **V5.5** |
| [`simulator-subprocess`](../../tree/simulator-subprocess) | SimulatorApp | C# WPF + NativeRunner.exe 子进程（stdio 管道 IPC，C++ 与 .NET GC 隔离） | v3.9.7 |
| [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) | SimulatorApp | C# WPF + NativeSender.dll 同进程（P/Invoke） | v3.7.31 |

要编译/修改 SimulatorApp，请切到对应架构分支：那里有完整的 `IEGPerTest.sln`、`build_all.bat`、`src/SimulatorApp/`、文档等。`main` 分支不再保留 SimulatorApp 的源码和构建脚本。

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

docs/                           # 文档（聊天记录 / 项目实施文档 / 项目看板 / WLServerTest UI 改造进度 等）
tools/                          # 通用：Python 白名单解析工具（CLI + GUI）
scripts/
  └── estimate_client_limit.ps1 # 单机客户端上限评估

archive/
  └── simulator-app/            # SimulatorApp 历史构建脚本归档（sln/bat/ps1/RELEASE_NOTES 等）
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
- **版本号需手动改**：`WLServerTest.rc`（main）/ `SimulatorApp.csproj`（其它分支），脚本不自增

## 📋 历史版本

- **WLServerTest**（main）：V5.5（计数器原子化+卡片化+布局重排）/ V5.2（崩溃修复+心跳时长）/ V5.1 / V5.0
- **SimulatorApp**（其它分支）：详见 [`simulator-subprocess`](../../tree/simulator-subprocess) 与 [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) 自带 CHANGELOG

详见各 `CHANGELOG_*.md` 与 [archive/simulator-app/RELEASE_NOTES_v3.9.7.md](archive/simulator-app/RELEASE_NOTES_v3.9.7.md)。

