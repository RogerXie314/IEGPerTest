# WLServerTest 项目状态文档

> 本文档用于帮助新协作者快速进入开发状态。最后更新：2026-11-17（分支 `refactor/scale-2000-per-host`，V6.9.2 主对话框 UI 布局调整 + 残留控件引用清理；`main` 仍为 V6.8.1）。

---

## 1. 项目概述

WLServerTest 是一款 C++ MFC 桌面工具，用于模拟 IEG 终端安全管控客户端的注册、心跳、策略配置上传、日志上报等核心行为，用于测试服务端接口兼容性。

- **仓库**：`https://github.com/RogerXie314/IEGPerTest.git`
- **当前分支**：`refactor/scale-2000-per-host`（基线 `origin/main` @ `96e89a1`；`main` 上的 V6.8.1 未改动）
- **当前版本**：`V6.9.2`（仅本分支；= V6.9.0 规模化改造 + V6.9.1 GBK 中文乱码修复与编码守卫 + V6.9.2 主对话框 UI 布局调整与残留控件引用清理；`main` 仍为 V6.8.1）
- **最新 Tag**：`v6.8.1`（本分支尚未打 tag）
- **本分支专项文档**：[`docs/改造-单机2000台规模化实施说明.md`](docs/改造-单机2000台规模化实施说明.md)

---

## 2. 技术栈

| 维度 | 说明 |
|---|---|
| 语言 | C++（MFC 对话框应用） |
| IDE | Visual Studio 2022 Professional |
| 目标平台 | Windows x64 |
| 构建配置 | Release x64 |
| 依赖库 | WLNetComm.dll（外部通信库，单例，线程安全性不可控） |
| 辅助脚本 | Python 3.13（用于打包、策略模板生成、批量修复） |

---

## 3. 目录结构

```
D:\Development\IEGPerTest/
├── external/IEG_Code/code/WLServerTest/   ← 核心源码目录
│   ├── WLServerTest.cpp / .h              ← 程序入口
│   ├── WLServerTestDlg.cpp / .h           ← 主对话框逻辑（核心修改区）
│   ├── SendInfoToServer.cpp               ← 策略上传函数实现（29 个模块）
│   ├── WLServerTest.rc                    ← 资源文件（含版本号、对话框布局）
│   ├── resource.h                         ← 控件 ID 定义
│   ├── policies/                          ← 28 个策略模板 JSON（随发布包交付）
│   ├── clientConfig.json                  ← 客户端默认配置模板
│   ├── WLServerTest.ini                   ← 运行时配置
│   ├── build_verify.bat                   ← Release x64 编译脚本
│   └── x64/Release/                       ← 编译输出目录
├── pack_fix5.py                           ← 打包脚本（生成 zip 发布包）
├── artifacts/                             ← 打包产物输出目录
├── PROJECT_STATUS.md                      ← 本文档
└── README.md                              ← 项目对外说明
```

---

## 4. 构建与打包

### 4.1 编译（Release x64）

```bash
cd external/IEG_Code/code/WLServerTest
cmd //c "build_verify.bat"
```

- **成功标志**：最后输出 `BUILD_EXITCODE=0`
- **编译产物**：`x64/Release/WLServerTest.exe`
- **已知 Warning**：`resource.h` / `RawPacketDlg.cpp` 的 `C4819` 编码警告（已有，不影响构建）

### 4.2 打包

```bash
python pack_fix5.py
```

- **输出**：`artifacts/WLServerTest_V6.8.1.zip`
- **包含内容**：exe、dll、`clientConfig.json`、`WLServerTest.ini`、`policies/`（28 个 JSON 模板）
- **验证**：zip 内文件数应为 32 个（含 28 个策略模板）

---

## 5. 版本号管理规范

每次发版本必须同步更新以下 **6 处**，缺一不可：

| 位置 | 文件 | 示例值 | 说明 |
|---|---|---|---|
| 主界面标题 | `WLServerTest.rc` L108 | `CAPTION "WLServerTest V6.8.1"` | 窗口标题栏 |
| 文件版本（数值） | `WLServerTest.rc` L297 | `FILEVERSION 6,8,1,0` | Win32 VERSIONINFO |
| 产品版本（数值） | `WLServerTest.rc` L298 | `PRODUCTVERSION 6,8,1,0` | Win32 VERSIONINFO |
| 文件版本（字符串） | `WLServerTest.rc` L314 | `VALUE "FileVersion", "6, 8, 1, 0"` | 文件属性页 |
| 产品版本（字符串） | `WLServerTest.rc` L319 | `VALUE "ProductVersion", "6, 8, 1, 0"` | 文件属性页 |
| 打包名称 | `pack_fix5.py` L7 | `pack_name = "WLServerTest_V6.8.1"` | zip 文件名 |

**验证方式**：编译后执行 `powershell -Command "(Get-Item x64/Release/WLServerTest.exe).VersionInfo.FileVersion"`，应返回 `6, 8, 1, 0`。

---

## 6. 编码铁律

### 6.1 源码文件（.cpp / .h / .rc）
- **编码**：GBK（CP936）
- **行尾**：CRLF（`\r\n`）
- **Python 写入时必须**：`open(path, 'wb')` + `data.encode('gbk')`，**禁止**用默认 UTF-8 模式写入

#### 6.1.1 已发生的乱码事故与防复发（必读）
- **现象**：任务面板「状态」列显示 `锟斤拷始锟斤拷`，日志输出显示 `[LOG] TCP 锟斤拷胁通锟斤拷...`（2026-11-17 截图）。
- **根因**：GBK 源码被以 **UTF-8 + errors='replace'** 读取后写回。每个无法解码的 GBK 字节变成 U+FFFD，落盘即 `EF BF BD`；MSVC 按 ANSI/GBK 解释该字节序列，运行时就显示成「锟斤拷」。这不是字体/显示问题，而是**源码字节已损坏**。已确认 `WLServerTestDlg.cpp` 在 Fix9（`903beac`，2026-10-14）由 0 个 `EF BF BD` 突增到 865 个；Fix13 后仍有 759 个，另有 5 个旧文件（注释）也有污染。
- **修复方式**：从干净历史版本（`5a11d74` / `3f13aab` 等 0 污染 blob）按行还原；新增/手工修复 14 行（`请输入客户端信息`、`已停止`、`执行中`、`开始中`、`[LOG] TCP 威胁通道任务已添加` 等）；所有受影响源文件现在 `EF BF BD` 计数为 0，运行时中文串与干净版本逐条一致。
- **防复发（强制）**：
  1. 编译前自动运行 `python tools/check_source_encoding.py`——扫描所有 git 跟踪的 `.cpp/.h/.rc/.bat`，发现 `EF BF BD` 或 UTF-8 BOM 即报错并中止编译。`scripts/build_wlservertest.ps1` 与源码目录 `build_verify.bat` 已接入；`tools/verify_scale_build.py` 也包含该检查。
  2. 任何编辑 **必须**走字节级 GBK 写入（见上方规则）；禁止用默认 UTF-8 的文本编辑器/API 直接保存 `.cpp/.h/.rc`。
  3. 新增中文字符串若担心工具链，优先用 GBK 字节写入，或像 JSON 里那样用 `\xNN` 字节转义（与源文件编码无关）。
  4. 提交前自检：`python tools/check_source_encoding.py` 必须输出 `[OK]`。

### 6.2 Python 辅助脚本
- 代码正文使用 ASCII 半角标点
- 用户输入含全角标点时，先 `replace` 成半角再嵌入源码
- 写 `.sh` 文件前确保 `content.replace(/\r\n/g, "\n")`

### 6.3 路径规范
- Git Bash 命令行路径：`"/c/Users/..."`（正斜杠 + `/c/` 盘符）
- Python/Node 代码内部路径：`"C:/Users/..."`（正斜杠）

---

## 7. 当前代码基线

- **Commit**：`30124d3`
- **Tag**：`v6.8.1`
- **核心变更**：Fix13 — 自动上传全量配置修复 + 版本号升级

### 7.1 Fix13 核心修复摘要

**问题**：勾选"自动上传配置"后，客户端上线自动上传的配置只有个别模块，与手动点击"上传配置"按钮结果不一致。

**根因**：自动上传在客户端上线边缘触发，部分策略模块请求可能因网络/服务器未就绪而失败。旧版重试线程**仅补传主配置（CMDID 201）**，其余 28 个策略模块失败后永不补齐。

**修复**：
1. 新增 `CWLServerTestDlg::UploadAllCfgModulesToServer` 统一全量上传函数（主配置 + 29 个策略模块）
2. 自动上传入口 `UploadClientConfigForClient`、手动上传后台线程、失败重试线程 **三条路径统一调用该函数**
3. 重试线程从"只补主配置"改为"全量重试"，彻底消除模块缺失

### 7.2 涉及的 29 个策略模块

| # | 模块 | CMDID | 说明 |
|---|---|---|---|
| 1 | 主配置 | 201 | 客户端基础配置 |
| 2 | 文件保护 | 241 | FileProtect |
| 3 | 注册表保护 | 240 | RegProtect |
| 4 | 进程审计 | 243 | ProcessAudit |
| 5 | MAC 强制访问控制 | 181 | MacSubObj |
| 6 | 信任进程 | - | TrustProcess |
| 7 | 信任路径 | - | TrustPath |
| 8 | 文件保护例外 | - | FileProtectException |
| 9 | 注册表保护例外 | - | RegProtectException |
| 10 | OS 用户 | - | OSUser |
| 11 | OS 资源 | 361 | OSResource |
| 12 | 扫描例外 | 152 | ScanExclude |
| 13 | 外设白名单 | 122 | UsbWhitelist |
| 14 | UDisk | 120 | UDisk |
| 15 | WiFi | 615 | WiFi |
| 16 | 违规外联 | - | IllegalConnect |
| 17 | 微隔离 | - | Firewall |
| 18 | 安全基线 | 167 | BaseLine |
| 19 | USBKey | 171 | USBKey |
| 20 | 域用户 | 182 | MacDomain |
| 21 | 威胁采集 | - | ThreatDetect |
| 22 | 工单 | - | WorkSheet |
| 23 | 工单结果 | - | WorkSheetResult |
| 24 | MAC 例外 | - | MacException |
| 25 | 信任签名 | - | TrustSigner |
| 26 | 病毒升级 | - | VirusUpgrade |
| 27 | 监视 | - | Surveil |
| 28 | 数据保护 | - | DataGuard |
| 29 | 备份 | - | Backup |

---

## 8. 开发注意事项

### 8.1 线程安全
- `CWLNetCommApi` 是单例，其内部 `pdoPost` 来自外部库 `WLNetComm.dll`，线程安全性不可控
- 后台线程（手动上传、重试）通过 `PostMessage(WM_APP + 101)` 回刷 UI，避免直接操作控件
- `g_csCfgRetryQueue` 互斥锁保护重试队列

### 8.2 策略模板加载
- `CClientConfigUploader::LoadClientConfigTemplate` 加载 `clientConfig.json`
- 各策略模块的 JSON 模板位于 `policies/*.json`，共 28 个文件
- `policy_macsubobj.json` 使用 `items1`/`items2` 结构（非标准 `items`），加载时需兼容

### 8.3 网络通信
- 所有策略上传通过 `CSendInfoToServer` 的 `pdoPost` 发送
- 失败判定：`nRet != CFG_UPLOAD_RET_OK`（其中 `CFG_UPLOAD_RET_OK` 为成功返回值）
- 重试机制：指数退避（1s -> 2s -> 4s -> 8s...），最多 `g_nCfgRetryMaxAttempts` 次

### 8.4 修改禁区
- **禁止**更新 git config
- **禁止**执行 `push --force` 或 `hard reset`
- **禁止**跳过 pre-commit hooks
- 修改前必须先读取现有文件（先读后写原则）

---

## 9. 快速验证 Checklist

发版前必做：

- [ ] `build_verify.bat` 输出 `BUILD_EXITCODE=0`
- [ ] `powershell -Command "(Get-Item x64/Release/WLServerTest.exe).VersionInfo.FileVersion"` 返回目标版本号
- [ ] `python pack_fix5.py` 成功生成 zip，文件数 32 个
- [ ] zip 内 `WLServerTest.exe` 与 `x64/Release/WLServerTest.exe` 字节级一致
- [ ] 主界面标题栏显示正确版本号
- [ ] GitHub Tag 已推送（`git push origin vX.Y.Z`）

---

## 10. 联系方式

- 仓库 Owner：`RogerXie314`
- 项目路径：`D:\Development\IEGPerTest`

---

## 11. 强制协作规则（每次修改必须执行）

> 本规则为项目级强制约定，任何账号、任何会话都必须遵守，用于保证切换账号后能快速接手项目。

- **更新时机**：每次对项目做出任何实质修改（代码、配置、脚本、文档、版本号、构建产物、Git 分支/tag 变更等）后，完成提交前，必须同步更新本文件。
- **更新内容**：在下方「变更日志」追加一条新记录，至少包含：日期、修改人/账号、修改内容摘要、涉及文件路径、影响说明（如需）、下一步计划；并同步刷新第 1 节项目概述、当前版本号、当前基线等受影响字段，使其反映最新状态。
- **口径要求**：状态描述必须与代码、打包产物、远程仓库实际情况一致；版本号以第 1 节记录的当前基线（含 Git tag）为准。
- **增量原则**：变更日志只追加、不覆盖、不删除历史条目，保留完整演进轨迹。
- **交接要求**：本规则本身不得被任何后续修改删除或弱化；若确需调整，须先经仓库 Owner 明确同意。

### 变更日志

- `2026-10-21` | 账号: 当前会话 | 新增第 11 节「强制协作规则」，约定每次修改后必须同步更新本文件并向变更日志追加记录；提交到 main 分支。
- `2026-11-02` | 账号: 当前会话 | 新增评估报告 `docs/评估-单机2000台终端重构理论可行性.html`：面向「5 台主机 × 2000 台 = 10000 台」目标，评估单机承载 2000 台的重构理论可行性（瓶颈定位 / 资源模型 / 6 条重构路径 / 风险边界 / 证据索引）；未改动代码与版本号，仅新增文档。
- `2026-11-02` | 账号: 当前会话 | 修订上述评估报告：新增第 6 节「历史弯路对照与修正」，依据 BUG-2026-001（git e1adb96）、C++ 老工具实测（500/主机、4000 EPS）与历史踩坑记录，把方案定性从「从零重构」修正为「回归＋移植已验证设计」；补充平台 watchdog 90s、短连接端口/TIME_WAIT、发布依赖与 GBK 编码等边界；风险与结论章节顺延为第 7、8 节。
- `2026-11-02` | 账号: 当前会话 | 二次修订上述评估报告（可靠性再评估）：查实 C# 版正是日志压力下落线并被放弃的一方（其在 4000 EPS 量级下被 C++ 老工具反证，修复仅验证到 1000 EPS），据此纠正「把 C# 当已验证参照」的误判——对标对象改为 C++ 老工具；新增第 6.4 节「可靠性判定」，明确「2000 台/主机」为已验证水位（约 500 台/主机）的 4 倍外推、目标吞吐（≈5460 EPS/主机）约为已验证（~1000 EPS/主机）的 5.5 倍，定性为「方向可行、量级高风险、需分阶段实测证伪」。
- `2026-11-09` | 账号: 当前会话 | 仓库整理（**未改动任何源码、未变更版本号**）：① `README.md` 从 V6.5.1 更新到当前基线 V6.8.1——重写版本特性段（V6.7 配置上传 / V6.8 修复 / V6.8.1 Fix13）、补双轨上报表（CMDTYPE 200 vs 150）、补发版检查清单与 `pack_fix5.py` 打包说明、修正产物路径为 `artifacts/WLServerTest_V6.8.1/`、文档索引补 `PROJECT_STATUS.md` 与根因分析；② `.gitignore` 新增 `.dumate/`、`.tmp/`、`.local-archive/` 与源码目录内备份/VS 临时文件规则（DuMate 等本地开发缓存只在本地保留、不入库），并补 `policies/*.json` 可跟踪规则；③ 清理源码目录历史备份：删除 `Backup`~`Backup6`、`.bak_cfg_upload/` 及 `*.bak_fix10`、`*.bak_policy_fix6`、`*.rc.backup`、`*.rc.decoded.txt`、`*.aps`、`*.suo`、`*.user`、`UpgradeLog.htm` 共 16 项（19 个文件），删除前全部打包归档到 `.local-archive/WLServerTest_source_backups_2026-11-09.zip` 并生成 md5 清单 json（已校验条目数与字节数一致；其中 `SendInfoToServer.cpp.bak_policy_fix6` 等 9 个中间态未进过 git，仅存在于该归档）；④ 同步修正 `策略上传根因分析.md` 第 7 节回滚路径，指向归档包与 git 提交 `6c1f544`。影响文件：`README.md`、`.gitignore`、`PROJECT_STATUS.md`、`策略上传根因分析.md`。
- `2026-11-10` | 账号: 当前会话 | 仓库整理收尾（**未改动任何源码、未变更版本号**）：① 源码目录一次性修复脚本 `inspect_dlg.py`/`package_fix3.py`/`patch_dlg2.py`/`patch_dlg_policy.py`/`patch_policy_upload.py` 与 VS 缓存目录 `.vs/` 已清理，脚本先归档到 `.local-archive/WLServerTest_oneshot_scripts_2026-11-10.zip`（5 文件，md5 校验通过）；② `.gitignore` 增加 `.workbuddy/`（工作区记忆目录不入库）；③ 源码目录现仅保留可编译的 26 个 `.cpp/.h`、资源文件、`policies/`（28 个 JSON）与 `x64/Release` 产物。影响文件：`.gitignore`、`PROJECT_STATUS.md`。
- `2026-11-16` | 账号: 当前会话 | **新分支 `refactor/scale-2000-per-host`**（基线 `origin/main` @ `96e89a1`），依据 `docs/评估-单机2000台终端重构理论可行性.html` 实施规模化改造，版本升到 V6.9.0（本分支专用，`main` 不变）。改动：R1 `g_sock[1000]`→`std::vector<SOCKET>` 动态表 + 按客户端索引写入 + 原子计数/越界保护；R2a 心跳由 1 终端 1 线程改为 20 终端/线程（ceil 线程数、末线程实际装载数、启动错峰 500→20ms，保留 `_New` 路径）；R3 日志与心跳统一 `THREAD_PRIORITY_ABOVE_NORMAL`；R4 POLICY 回包不再在 HB 线程同步 HTTPS，改为单工作线程 + 有界队列；R5 注册由每客户端 1 线程改为固定 32 工作线程；R6 重连后发布新 socket + 日志线程每轮刷新 + UI 下拉加 2000。新增 `docs/改造-单机2000台规模化实施说明.md` 与 `tools/verify_scale_build.py`（15 项静态检查全过）。验证：`build_verify.bat` → `BUILD_EXITCODE=0`；exe 文件/产品版本 `6,9,0,0`；`python pack_fix5.py` → `artifacts/WLServerTest_V6.9.0.zip`（32 文件）。**未做实机压测**；R2b 日志池化 / per-socket 发送锁 / R3 实测确认仍为待办，详见专项文档第 5 节。影响文件：`WLServerTestDlg.cpp/.h`、`WLServerTest.rc`、`pack_fix5.py`、`README.md`、`PROJECT_STATUS.md`、新增 2 个文档/脚本。
- `2026-11-17` | 账号: 当前会话 | **修复界面/日志中文乱码（编码回归）**：定位到根因是 GBK 源码被 UTF-8 replace 模式重写，产生 `EF BF BD` 替换字符字节（`锟斤拷`）；首次污染在 Fix9 `903beac`（0→865 个），Fix13 后残留 759 个 + 5 个旧文件注释。按干净历史 blob 逐行还原 111 行、手工修复剩余 14 行，覆盖任务面板状态（`开始中`/`执行中`/`已停止`）、`[LOG] TCP 威胁通道任务已添加`、各 AfxMessageBox 提示与注释；6 个受影响源文件现均无 `EF BF BD`，运行时中文串与干净历史版本逐条比对一致。新增 `tools/check_source_encoding.py` 防复发守卫，接入两个编译脚本与 `verify_scale_build.py`；根因与规则写入第 6.1.1 节。影响文件：`WLServerTestDlg.cpp`、`ProfileConfig.cpp/.h`、`VersionManagementDlg.h`、`client.h`、`RawPacketDlg.cpp`、`build_verify.bat`、`PROJECT_STATUS.md`、新增 `tools/check_source_encoding.py`、`scripts/build_wlservertest.ps1`、`tools/verify_scale_build.py`。
- `2026-11-17` | 账号: 当前会话 | **发布 V6.9.1**（本分支专用，`main` V6.8.1 不变；V6.9.0 产物保留不覆盖，规避运行中 exe 占用）：在 V6.9.0 规模化改造 + 编码乱码修复基础上，版本号整体前进一步——`WLServerTest.rc` 5 处（CAPTION / FILEVERSION / PRODUCTVERSION / StringFileInfo 的 FileVersion、ProductVersion）、`pack_fix5.py` 包名、`tools/verify_scale_build.py` 检查项同步到 `V6.9.1` / `6,9,1,0` / `6, 9, 1, 0`。README 当前 checkout 与版本历史、专项实施说明的版本/验证表同步更新。验证：`BUILD_EXITCODE=0`，exe 版本 `6,9,1,0`，`artifacts/WLServerTest_V6.9.1.zip` 32 文件，`check_source_encoding.py` 与 `verify_scale_build.py` 全通过。影响文件：`WLServerTest.rc`、`pack_fix5.py`、`tools/verify_scale_build.py`、`README.md`、`PROJECT_STATUS.md`、`docs/改造-单机2000台规模化实施说明.md`。
- `2026-11-17` | 账号: 当前会话 | **发布 V6.9.2**（本分支专用，`main` V6.8.1 不变；继 V6.9.1 之后的全部改动，用户用 VS2022 打开 `WLServerTest.vcxproj` 后手动调整界面并打包验证）：① **主对话框 UI 布局调整**——`WLServerTest.rc` 的 `IDD_WLSERVERTEST_DIALOG` 约 42 处控件坐标变化，含日志区按钮下移（`IDC_BUTTON_LOG_ADD` y375→436、`IDC_BUTTON_LOG_STOP` y375→435）、白名单区按钮下移（上传/停止/预览 y232→260、并发输入 y214→236）、心跳区按钮与上传配置按钮微调、多个分类复选框宽度收窄（如 `IDC_CAT_MANDATORY_ACCESS`/`IDC_CAT_OUTBOUND` 87→64 等）；② **VS 资源编辑器规范化**——`WLServerTest.rc` 由 CONTROL 十六进制样式改写为符号样式（BS_AUTOCHECKBOX/LTEXT/GROUPBOX/DEFPUSHBUTTON 等）、段落重排，控件 ID 与标题均保留；VS 同时自动补全 `WLServerTest.vcxproj` 的 manifest 引用与 `WLServerTest.vcxproj.filters` 中 4 个已有 cpp 条目（LogCategoryHelpDlg/RawPacketDlg/WhitelistPreviewDlg/VersionManagementDlg），均为安全变更；③ **清理残留控件引用**——手动删除的 `IDC_STATIC_HB_BADGE`（TCP/Windows 徽标）仍有 1 处 DDX 绑定 + 3 处 `SetWindowText`（`WLServerTestDlg.cpp`）与 1 处成员声明（`WLServerTestDlg.h`），会导致 MFC 启动时绑定不存在控件抛异常，按用户选择从代码移除，共删 5 行；另排查确认 `IDC_LIST_HEARTBEAT_ListShowWindow` 等 4 个缺失 ID 为 V6.9.1 历史遗留、非本次引入；④ **版本号前进一步**——`WLServerTest.rc` 5 处、`pack_fix5.py` 包名、`tools/verify_scale_build.py` 检查项同步到 `V6.9.2` / `6,9,2,0` / `6, 9, 2, 0`，README 当前 checkout 与版本历史、专项实施说明版本引用、本文件头部与当前版本同步。验证：编码守卫 27 文件无 `EF BF BD`/BOM；编译 `BUILD_EXITCODE=0`；exe CAPTION V6.9.2(UTF-16LE)=1、V6.9.1=0、HB_BADGE 引用=0；`artifacts/WLServerTest_V6.9.2.zip` 32 文件；`verify_scale_build.py` 静态检查通过。备份 `.tmp/WLServerTest_*_v692_backup.*`（gitignore，未入库）。影响文件：`WLServerTest.rc`、`WLServerTest.vcxproj`、`WLServerTest.vcxproj.filters`、`WLServerTestDlg.cpp/.h`、`pack_fix5.py`、`tools/verify_scale_build.py`、`README.md`、`PROJECT_STATUS.md`、`docs/改造-单机2000台规模化实施说明.md`（如同步）。**未做实机压测**。
