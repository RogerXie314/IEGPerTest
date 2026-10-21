# WLServerTest 项目状态文档

> 本文档用于帮助新协作者快速进入开发状态。最后更新：2026-10-20（对应 commit `30124d3`，tag `v6.8.1`）。

---

## 1. 项目概述

WLServerTest 是一款 C++ MFC 桌面工具，用于模拟 IEG 终端安全管控客户端的注册、心跳、策略配置上传、日志上报等核心行为，用于测试服务端接口兼容性。

- **仓库**：`https://github.com/RogerXie314/IEGPerTest.git`
- **当前分支**：`main`
- **当前版本**：`V6.8.1`
- **最新 Tag**：`v6.8.1`

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
