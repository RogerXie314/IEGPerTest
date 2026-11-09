# IEGPerTest — WLServerTest 压测工具（`main` 分支）

`main` 分支主推 **WLServerTest** —— 基于老 IEG 代码树持续翻新的 MFC 桌面压测工具（x64 单 exe），用于模拟客户端注册、TCP/HTTPS 心跳、威胁日志上报、白名单上传、**策略配置上传**。

> 仓库还有两条 SimulatorApp（C# WPF）架构分支，见下方[「仓库分支结构」](#-仓库分支结构重要)。

---

## 当前版本 **V6.8.1**（2026-10-19）

发布路径：`artifacts/WLServerTest_V6.8.1/`，打包产物 `artifacts/WLServerTest_V6.8.1.zip`（32 个文件）。

### V6.8.1 版本特性

- **Fix13 — 自动上传全量配置修复**：勾选"自动上传配置"后，客户端上线自动上传的配置只有个别模块，与手动点击"上传配置"结果不一致。根因是旧重试线程仅补传主配置（CMDID 201），其余 28 个策略模块失败后永不补齐。现已抽出统一入口 `CWLServerTestDlg::UploadAllCfgModulesToServer`（主配置 + 28 个策略模块），**自动上传 / 手动上传 / 失败重试**三条路径共用，重试线程改为全量重试。

### V6.8 版本特性

- 修复界面乱码（"已注册"计数显示）
- 策略 fallback 对齐 IEG 源码：扫描例外 CMDID 修正为 **152**、安全基线改字符串格式、UDisk 开关字段补全

### V6.7 版本特性（策略配置上传大版本）

- **客户端配置上传全量模拟**：主配置报文补齐 182 项字段，覆盖文件保护 / 强制访问控制 / 注册表 / 信任区 / 隔离区 / 病毒防护 / 漏洞防护 / 安全基线 / 外设管理 / 进程审计 / 数据保护 / 行为监测 / 备份恢复
- **28 个策略模块独立上报**：模板位于 `policies/*.json`（随发布包交付，用户可直接改模板定制报文）
- **双轨上报机制**（关键协议约定，详见 [`策略上传根因分析.md`](策略上传根因分析.md)）：

  | 轨道 | URL | CMDTYPE | CMDID | 内容 |
  |---|---|---|---|---|
  | A 状态总报 | `clientRefresh.do` | **200**（CMDTYPE_CMD） | 201 | 约 108 个开关类扁平字段 |
  | B 规则分报 | `FileProtect.do` 等 | **150**（CMDTYPE_POLICY） | 241/240/243/242/121/181/247/248 等 | 各模块规则明细 |

  > CMDTYPE 填错会被服务端按状态通道校验后静默丢弃，表现为"开关状态上去了、策略内容全空"。
- **自动上传配置**：心跳批量线程做上线边缘检测后触发，后台线程执行不阻塞心跳；失败按指数退避重试（1s→2s→4s→8s，次数与基数可在 INI 的 `[Global]` 段配置）
- **Fix8**：修复消息映射表绑定错误（"上传配置"按钮与"自动上传"复选框此前未绑定到主对话框）

### 保留能力（V6.5 / V6.5.1）

| 功能 | 说明 |
|------|------|
| 客户端注册 | 并发批量注册，devid 从服务端获取后持久化到 INI `[DevID]` 段，重启免重注册 |
| TCP 心跳 | 按 ClientID 保活，独立启停，不干扰日志发送任务 |
| Linux HTTPS 心跳 | WinHTTP POST `/USM/clientHeartbeat.do`，忽略证书错误 |
| 威胁日志发送 | TCP/HTTPS 双通道，31 类分类复选框按位掩码过滤，未勾选零开销跳过 |
| 白名单上传 | 批量上报文件白名单到平台，支持预览 |
| 攻击报文回放 | 从抓包样本加载，按 stream 拆分回放（依赖 `RawPacketEngine.dll`） |

**V6.5.1 修复**：`RegisterClientToServer` 静态变量竞态（300 线程并发注册）与"心跳/日志 devid 不一致"两处缺陷已消除，日志报文 devid 回归 0 与心跳一致；400×1 日志压力下无掉线。

**V6.5 特性**：devid 按客户端持久化、威胁日志类型复选框过滤、ComputerIP 注入、Debug 日志开关、"结束任务"精准停止单行任务、JSON 转义/截断等协议健壮性修复。

---

## 编译 WLServerTest

**推荐：一键脚本**（自动探测 VS，自动拷贝产物到 `artifacts/WLServerTestPublish/`）

```powershell
pwsh -File scripts\build_wlservertest.ps1
# 选项：-Configuration Debug | -SkipPublish | -Clean
# 环境变量：$env:MSBUILD_EXE / $env:WLNETCOMM_DLL 可覆盖默认探测
```

脚本按以下优先级查找 MSBuild：`$env:MSBUILD_EXE` → PATH → `vswhere`（VS 2017/2019/2022 × Community/Pro/Enterprise/BuildTools）→ 8 个常见绝对路径兜底；仓库路径基于脚本自身位置解析，与 cwd 无关。

**备用：源码内批处理**（硬编码 VS2022 Professional 路径，仅本机可用）

```bat
cd external\IEG_Code\code\WLServerTest
build_verify.bat            :: 成功标志 BUILD_EXITCODE=0
```

产物：`external/IEG_Code/code/WLServerTest/x64/Release/WLServerTest.exe`

**手动 MSBuild**：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  external\IEG_Code\code\WLServerTest\WLServerTest.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m
```

> 已知无害警告：`resource.h` / `RawPacketDlg.cpp` 的 C4819 编码警告。源码统一 **GBK（CP936）+ CRLF**，用 Python 改写必须先 `open(...,'wb')` 再 `encode('gbk')`。

---

## 打包发布

```bash
python pack_fix5.py
```

- 输入：`x64/Release/WLServerTest.exe` + `RawPacketEngine.dll` + `clientConfig.json` + `WLServerTest.ini` + `policies/`（28 个 JSON）
- 输出：`artifacts/WLServerTest_V6.8.1/` 与 `artifacts/WLServerTest_V6.8.1.zip`
- 校验：zip 内**文件数应为 32 个**

**部署**（无需安装运行时）：把发布目录整目录拷到目标机器，双击 `WLServerTest.exe`。若启动提示缺 `WLNetComm.dll`，从 `external/IEG_Code/code/bin/Release/x64/` 拷入同目录。

---

## 发版检查清单

版本号需手动同步 **6 处**（脚本不自增），任一遗漏会导致标题栏 / 文件属性 / 包名不一致：

| 位置 | 文件 | 当前值 |
|---|---|---|
| 主界面标题 | `WLServerTest.rc` L108 | `CAPTION "WLServerTest V6.8.1"` |
| 文件版本（数值） | `WLServerTest.rc` L297 | `FILEVERSION 6,8,1,0` |
| 产品版本（数值） | `WLServerTest.rc` L298 | `PRODUCTVERSION 6,8,1,0` |
| 文件版本（字符串） | `WLServerTest.rc` L314 | `VALUE "FileVersion", "6, 8, 1, 0"` |
| 产品版本（字符串） | `WLServerTest.rc` L319 | `VALUE "ProductVersion", "6, 8, 1, 0"` |
| 打包名称 | `pack_fix5.py` L7 | `pack_name = "WLServerTest_V6.8.1"` |

- [ ] `build_verify.bat` 输出 `BUILD_EXITCODE=0`
- [ ] `(Get-Item x64/Release/WLServerTest.exe).VersionInfo.FileVersion` 返回 `6, 8, 1, 0`
- [ ] `python pack_fix5.py` 生成 zip，文件数 32 个
- [ ] zip 内 exe 与 `x64/Release/WLServerTest.exe` 字节级一致
- [ ] 主界面标题栏版本号正确
- [ ] GitHub tag 已推送（`git push origin vX.Y.Z`）

---

## 推荐压测方案：3 台主机 × 1000 客户端 = 3000 在线

| 主机 | 起始序号 | 数量 | IP 段 | EPS |
|---|---|---|---|---|
| 主机 A | 1 | 1000 | 192.168.0.1 | 2000 |
| 主机 B | 1001 | 1000 | 192.168.1.1 | 2000 |
| 主机 C | 2001 | 1000 | 192.168.2.1 | 2000 |

> 三台必须用**不同序号范围**，否则同 ClientId 心跳会互踢 session。

每台主机：

1. 配平台地址（三机相同）
2. 注册区按表设置 → "并发注册"
3. 启动 TCP 心跳保持 1000 客户端在线
4. 日志：客户端 1000 / 每秒每端 2 / 总条数视测试时长（10 分钟 = 1200）
5. 三机同时点"添加任务"

> 承载上限提示：单进程套接字数组为 `g_sock[1000]`，且"1 终端 = 1 线程"，实测 400–500 台/主机即可能出现心跳掉线。单机 2000 台的可行性评估与改造路径见 [`docs/评估-单机2000台终端重构理论可行性.html`](docs/评估-单机2000台终端重构理论可行性.html)。

---

## 文档

- [`PROJECT_STATUS.md`](PROJECT_STATUS.md) — **接手必读**：当前基线、目录结构、版本号规范、编码铁律、线程安全约定、变更日志
- [`策略上传根因分析.md`](策略上传根因分析.md) — 双轨上报机制与 CMDTYPE 200/150 结论、Fix6 修改清单
- [`docs/项目实施文档.md`](docs/项目实施文档.md) — v3.7.x~v5.8 完整变更记录、协议路由表、调试技巧
- [`docs/评估-单机2000台终端重构理论可行性.html`](docs/评估-单机2000台终端重构理论可行性.html) — 10000 台目标的瓶颈定位与重构路径
- [`docs/对比-当前WLServerTest与老工具原型在HB与威胁日志的差异.md`](docs/对比-当前WLServerTest与老工具原型在HB与威胁日志的差异.md) — 与老工具原型 18 项差异表
- [`docs/经验教训-日志类型实现.md`](docs/经验教训-日志类型实现.md) — 日志类型踩坑总结
- [`docs/project_dashboard.html`](docs/project_dashboard.html) — 项目看板
- [`tools/README.md`](tools/README.md) — Python 白名单解析工具（CLI + GUI）

---

## 仓库分支结构（重要）

`main` 上的代码 ≠ 全部架构。三套压测/模拟实现分别活在不同分支，**互不合并**：

| 分支 | 主推工具 | 架构 | 最新版本 |
|---|---|---|---|
| **`main`**（当前） | **WLServerTest** | MFC + 纯原生 C++（基于老 IEG 代码树优化） | **V6.8.1** |
| [`simulator-subprocess`](../../tree/simulator-subprocess) | SimulatorApp | C# WPF + NativeRunner.exe 子进程（stdio 管道 IPC） | v3.9.7 |
| [`simulator-inproc-dll`](../../tree/simulator-inproc-dll) | SimulatorApp | C# WPF + NativeSender.dll 同进程（P/Invoke） | v3.7.31 |

要编译/修改 SimulatorApp，请切到对应架构分支（那里有完整 `IEGPerTest.sln`、`build_all.bat`、`src/SimulatorApp/`）。`main` 分支不保留 SimulatorApp 源码。

---

## 仓库结构（main 分支）

```
README.md                       # 本文件
PROJECT_STATUS.md               # 项目状态 / 变更日志（每次修改必须同步）

external/IEG_Code/code/
  ├── WLServerTest/             # ★ 主推工具源码
  │     ├── WLServerTestDlg.cpp/.h    # 主对话框逻辑（核心修改区）
  │     ├── SendInfoToServer.cpp/.h   # 日志 / 心跳 / 策略上传报文构造
  │     ├── ClientConfigUploader.cpp  # clientConfig.json 模板加载
  │     ├── ProfileConfig.cpp         # INI 持久化
  │     ├── RawPacketDlg.cpp          # 攻击报文窗口
  │     ├── policies/*.json           # 28 个策略模块模板（随包交付）
  │     └── clientConfig.json         # 主配置模板
  └── （其余 IEG 模块作为依赖参考：WLNetComm / WLProtect / common 等）

artifacts/
  ├── WLServerTest_V6.8.1/      # 发布产物目录
  └── WLServerTest_V6.8.1.zip   # 发布包（32 文件）

docs/                           # 项目文档 / 评估报告 / 看板
tools/                          # Python 白名单解析工具（CLI + GUI）
scripts/build_wlservertest.ps1  # 一键编译
pack_fix5.py                    # 打包脚本
archive/                        # SimulatorApp 历史构建脚本归档
```

`.dumate/`、`.tmp/`、`.local-archive/` 为本地开发缓存与备份归档，已在 `.gitignore` 中排除，不入库。

---

## 技术栈（main 分支）

| 模块 | 技术 |
|---|---|
| WLServerTest | MFC + VS2022 + C++17 + 静态链接 WLUtilities/WLProtocal/WLCData 等 + `RawPacketEngine.dll`（攻击报文） |
| 通信 | 自研 `CWLNetCommApi`（TCP 心跳/日志）+ WinHTTP（Linux HTTPS 心跳、策略上传） |
| 持久化 | INI（`WLServerTest.ini`：服务器地址、`[DevID]`、`[RawPacket]`、上传重试参数）/ `Clients.log` |
| 辅助脚本 | Python 3.13（打包、模板生成、批量修复） |

---

## 重要说明

- **分支策略**：`main` 长期主推 WLServerTest；SimulatorApp 两种架构活在专用分支
- **版本号需手动改**：`WLServerTest.rc` 6 处 + `pack_fix5.py`，见上方检查清单
- **修改后必须更新 `PROJECT_STATUS.md`**（第 11 节强制协作规则），并向变更日志追加一条
- **Git 代理**（如需）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`

## 历史版本

| 版本 | 关键内容 |
|---|---|
| **V6.8.1** | Fix13 自动上传全量配置修复（三路径统一全量上传）+ 版本号升级 |
| V6.8 | 界面乱码修复（已注册计数）+ 策略 fallback 对齐 IEG 源码（扫描例外 152 / 基线字符串 / UDisk 开关） |
| V6.7 | 客户端配置上传全量模拟（182 项字段 + 28 个策略模块 + policies 模板随包）+ 自动上传上线触发 + Fix5~Fix11d 系列修复 |
| V6.5.1 | 高并发日志压力掉线修复（devid 竞态 / 心跳日志 devid 一致）+ UI 乱码修复 |
| V6.5 | devid 持久化 + 威胁日志类型过滤 + ComputerIP 注入 + Debug 开关 |
| V6.3 | DLL Load 转义 + devid 协议头 + JSON 截断修复 |
| V6.2 / V6.1 / V6.0 | 清理与文档整理 / 攻击报文多 stream 修复 / 注册 doPost 弹窗移除 |
| V5.9 ~ V5.0 | OPT 调度大修 / 攻击报文 UI 复刻 / 短连接漏发修复 / 计数器原子化 / 崩溃修复 等 |
| SimulatorApp | 详见 `simulator-subprocess`、`simulator-inproc-dll` 分支自带 CHANGELOG |
