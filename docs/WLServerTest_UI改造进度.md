# WLServerTest UI 改造进度

> 全部 C++/MFC。完全对齐 SimulatorApp（WPF）功能。心跳/TCP函数一行不改。
> **规则**：完成一项打 ✅，开始一项打 🔄，阻塞打 ❌。

---

## Phase 0：准备

| # | 任务 | 状态 |
|---|------|------|
| 0.1 | 创建本进度文件 | ✅ |

---

## Phase 1：resource.h — 新增 76 个控件/对话框 ID

| # | 任务 | 状态 |
|---|------|------|
| 1.1 | 31 个日志分类 CheckBox ID（IDC_CAT_CLIENT_OPS=1070 … IDC_CAT_THREAT_OS=1100） | 🔄 |
| 1.2 | 服务器/OS/注册/心跳/白名单/日志/统计/输出 控件 ID（1101–1144） | 🔄 |
| 1.3 | 子对话框 ID（IDD_LOG_HELP=200, IDD_WL_PREVIEW=201, IDD_RAWPACKET=202, IDD_VER_MGMT=203） | 🔄 |
| 1.4 | 更新 `_APS_NEXT_CONTROL_VALUE=1145`，`_APS_NEXT_DIALOG_VALUE=204` | 🔄 |

---

## Phase 2：WLServerTest.rc — 主对话框完全重写（1040×520 DLU）

| # | 任务 | 状态 |
|---|------|------|
| 2.1 | 对话框尺寸 900×540 DLU，WS_THICKFRAME 保持可缩放 | ⬜ |
| 2.2 | 左列：服务器设置 GroupBox（含日志服务器 CheckBox） | ⬜ |
| 2.3 | 左列：OS 类型 GroupBox（Windows/Linux RadioButton） | ⬜ |
| 2.4 | 左列：注册设置 GroupBox（版本 ComboBox + 管理版本按钮） | ⬜ |
| 2.5 | 左列：日志分类 GroupBox（项目类型Combo + "?分类说明" + 31 CheckBox 4子区域） | ⬜ |
| 2.6 | 右列：心跳设置 GroupBox（间隔/策略接收/开始/停止/模式Badge） | ⬜ |
| 2.7 | 右列：白名单上传 GroupBox（路径/选择/自动上传/并发度/上传/结束/预览） | ⬜ |
| 2.8 | 右列：日志发送设置 GroupBox（总条数/HTTPS双参数/TCP三参数/添加/结束） | ⬜ |
| 2.9 | 右列：统计 GroupBox（2×2格：注册/心跳/日志/白名单） | ⬜ |
| 2.10 | 右列：心跳任务 CListCtrl + 日志/WL任务 CListCtrl（保持双列表） | ⬜ |
| 2.11 | 右列：日志输出 EDITTEXT（ES_MULTILINE ES_READONLY） | ⬜ |
| 2.12 | 追加 4 个子对话框定义（IDD_LOG_HELP/WL_PREVIEW/RAWPACKET/VER_MGMT） | ⬜ |
| 2.13 | 旧 Combo/Button 保留为隐藏控件（NOT WS_VISIBLE，代码兼容） | ⬜ |

---

## Phase 3：WLServerTestDlg.h — 追加新成员

| # | 任务 | 状态 |
|---|------|------|
| 3.1 | 服务器/OS 类型控件变量（editLogHost/editLogPort/chkUseLogServer/radioOsWin/radioOsLinux） | ⬜ |
| 3.2 | 版本/项目类型 ComboBox（comboClientVersion, comboProjectType） | ⬜ |
| 3.3 | 31 个日志分类 CButton 成员（m_catClientOps … m_catThreatOs） | ⬜ |
| 3.4 | 心跳/白名单/日志发送控件变量（~15 个） | ⬜ |
| 3.5 | 14 个统计 CStatic 成员 + m_editLogOutput + m_staticHbBadge | ⬜ |
| 3.6 | 方法声明（AppendLogOutput/UpdateStatsDisplay/LoadClientVersionCombo/SendLinuxHeartbeat_HTTPS + 14 个按钮处理） | ⬜ |

---

## Phase 4：WLServerTestDlg.cpp — 逻辑层（仅追加，旧函数体一行不改）

| # | 任务 | 状态 |
|---|------|------|
| 4.1 | DoDataExchange 追加 ~60 行新 DDX_Control/DDX_Text 绑定 | ⬜ |
| 4.2 | MESSAGE_MAP 追加 ~14 行 ON_BN_CLICKED + ON_WM_TIMER | ⬜ |
| 4.3 | OnInitDialog 末尾追加：ComboBox 填充/默认值/SetTimer(1,1000,NULL) | ⬜ |
| 4.4 | OnBnClickedOsWindows/Linux：切版本列表/Badge文字/OsInfo文字 | ⬜ |
| 4.5 | LoadClientVersionCombo：从 ProfileConfig 读版本列表，填充 Combo | ⬜ |
| 4.6 | OnBnClickedHbStart：Windows→现有TCP路径；Linux→HTTPS心跳线程 | ⬜ |
| 4.7 | OnBnClickedHbStop/LogAdd/LogStop/WlUpload/WlStop2 | ⬜ |
| 4.8 | 弹窗处理：WlPreview/VerMgmt/LogHelp/RawPacket | ⬜ |
| 4.9 | SendLinuxHeartbeat_HTTPS（WinHTTP POST，JSON完全对齐 HeartbeatJsonBuilder.cs） | ⬜ |
| 4.10 | AppendLogOutput（线程安全）/ UpdateStatsDisplay / OnTimer | ⬜ |

---

## Phase 5a：SendInfoToServer — 新增 HTTPS 日志发送

| # | 任务 | 状态 |
|---|------|------|
| 5a.1 | 新增 `SendHttpsLog_ByType(client&, int logType)` 统一分发函数 | ⬜ |
| 5a.2 | 26 类 HTTPS 日志路由（威胁检测5类走 TCP，其余26类走 HTTPS） | ⬜ |

---

## Phase 5b：Linux HTTPS 心跳精确实现

| # | 任务 | 状态 |
|---|------|------|
| 5b.1 | WinHTTP POST `https://{host}:{port}/USM/clientHeartbeat.do` | ⬜ |
| 5b.2 | JSON Body：CMDTYPE=200, MAC=02-00-{IP4Bytes}，WindowsVersion="Linux centos7" | ⬜ |
| 5b.3 | 忽略证书错误（SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS） | ⬜ |
| 5b.4 | 心跳线程：按 HbInterval 定时循环，支持停止信号 | ⬜ |

---

## Phase 6：子对话框（4 个，无 AdvancedRegDlg）

| # | 任务 | 状态 |
|---|------|------|
| 6.1 | **VersionManagementDlg**（IDD_VER_MGMT=203）：OS RadioBtn + ListBox版本列表 + 添加/删除(确认)/恢复默认(确认)/保存 | ⬜ |
| 6.2 | **LogCategoryHelpDlg**（IDD_LOG_HELP=200）：CListCtrl 3列（类别/发送通道/说明），31行静态数据 | ⬜ |
| 6.3 | **WhitelistPreviewDlg**（IDD_WL_PREVIEW=201）：CListCtrl 读 WL 文件，显示路径+哈希 | ⬜ |
| 6.4 | **RawPacketDlg**（IDD_RAWPACKET=202）：LoadLibrary RPE.dll + NIC Combo + .etc文件 ListBox + 发送/停止 | ⬜ |

---

## Phase 7：ProfileConfig — 新增 INI 持久化方法

| # | 任务 | 状态 |
|---|------|------|
| 7.1 | LogHost/LogPort/UseLogServer/OsType/ProjectType | ⬜ |
| 7.2 | HbInterval/WlConcurrency/ClientVersion | ⬜ |
| 7.3 | WindowsVersionList/LinuxVersionList（多版本用 \| 分隔存 INI） | ⬜ |

---

## Phase 8：WLServerTest.vcxproj — 注册新文件

| # | 任务 | 状态 |
|---|------|------|
| 8.1 | 追加 4 个 ClCompile（VersionManagementDlg.cpp 等） | ⬜ |

---

## Phase 9：构建 & 验证

| # | 任务 | 状态 |
|---|------|------|
| 9.1 | MSBuild x64 Release — 零错误 | ⬜ |
| 9.2 | 启动验证：对话框约 1300×820px，布局与 SimulatorApp 截图对比 | ⬜ |
| 9.3 | Windows TCP 心跳（端到端服务器回包确认） | ⬜ |
| 9.4 | Linux HTTPS 心跳（向真实服务器发包确认回包） | ⬜ |
| 9.5 | 版本管理弹窗：添加/删除/恢复默认/保存 验证 | ⬜ |
| 9.6 | 白名单预览弹窗验证 | ⬜ |
| 9.7 | 攻击报文弹窗（选 NIC → 选 ms08067 → Burst 100包） | ⬜ |
| 9.8 | 31类日志分类 → 添加任务验证 | ⬜ |
| 9.9 | 重启验证 INI 持久化 | ⬜ |

---

## 关键技术参数

| 参数 | 值 |
|---|---|
| 对话框尺寸 | 900×540 DLU（约 1350×876px @ 96DPI） |
| Linux 心跳 URL | `https://{host}:{port}/USM/clientHeartbeat.do` |
| MAC 生成规则 | `02-00-{IP四字节HEX}` |
| 默认 Windows 版本 | `V300R011C01B090` |
| 默认 Linux 版本 | `V300R011C11B060-Redhat7.x-x64` |
| 默认注册并发（写死） | 10（AdvancedRegDlg已去掉） |
| RawPacketEngine 加载 | LoadLibrary 动态加载 |
| 排除功能 | TcpDiagWindow（连接参数诊断） |
