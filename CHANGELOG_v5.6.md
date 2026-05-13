# V5.6 更新记录 (WLServerTest)

发布日期：2026-05-13

## 🎨 UI / 布局优化

### 心跳设置区域重排
- 行1：心跳间隔 + 策略接收勾选框
- 行2：心跳时长 + 输入框（与间隔输入框对齐）
- 行3：开始心跳按钮
- 行4：TCP(Windows) 标签（移到按钮右侧同行）

### 日志发送区域调整
- "添加任务"和"结束任务"按钮下移一行，与上方 TCP 客户端数行拉开间距

### 白名单按钮均匀分布
- "立即上传全量"、"结束任务"、"预览"三个按钮等间距均匀排列，不超出分组框

### 注册设置对齐
- "客户端版本"下拉框左对齐上方输入框

### 预览按钮样式统一
- 修复预览按钮视觉风格与其他按钮不一致的问题（SetWindowTheme 列表中补充预览按钮）

## 🚀 功能增强

### IEG/EDR 项目类型联动
- 切换项目类型（IEG/EDR）时自动勾选对应日志分类
- 对齐 C# SimulatorApp 的 ApplyProjectTypeSelection 逻辑
- IEG 专属：非白名单、注册表保护、U盘告警、USB访问告警、白名单防篡改、漏洞保护、进程审计、U盘插拔、网口Up/Down、外设控制9种
- EDR 专属：防火墙、系统守护
- 共有：客户端操作、操作系统、非法外联、文件保护、强制访问控制、病毒预警、威胁检测(进程/注册表/文件)

### 网口插拔 HTTPS 短连接实现
- 新增 `SendClientNetAdapterLogToServer` 方法
- 接口：`/USM/hotplugDevLog.do`，CMDID=204, CMDVER=4, OtherDevType=7
- 定义 `CLIENT_MSGLOG_NETADAPTER = 0x400`

### 外设控制 9 种 HTTPS 短连接实现
- 新增 `SendClientExtDevLogToServer` 方法，支持9种子类型（USB接口、移动设备、CDROM、wifi、USB网卡、软盘、蓝牙、串口、并口）
- 接口：`/USM/clientULog.do`
- LogContent 使用正确的中文格式（如"USB接口使用被禁止"）
- 定义 `CLIENT_MSGLOG_EXTDEV = 0x800`

### 日志输出时间标签
- AppendLogOutput 自动添加 `[HH:MM:SS.mmm]` 时间戳前缀

## 🐛 缺陷修复

### 移除多余"程序报警事件"
- `SendClientNwlLogToServer_FiveType` 中移除 `OPTYPE_PWL_AUTO_APPROVE`（平台无法匹配分类规则）
- 保留：系统完整性检查、非法程序启动、进程审计、白名单篡改事件

### 短连接日志 checkbox 残留状态修复
- `OnBnClickedLogAdd` Channel 1 和 Channel 2 在调用 AddTask 前清除所有 hidden checkbox 残留状态
- 防止上次操作遗留的勾选被误读

### 统计计数补全
- 威胁统计条件中补上 `CLIENT_MSGLOG_Virus`、`CLIENT_MSGLOG_NETADAPTER`、`CLIENT_MSGLOG_EXTDEV`

## 🏗️ 技术变更

### 新增宏定义 (WLServerTestDlg.h)
- `CLIENT_MSGLOG_NETADAPTER = 0x00000400`
- `CLIENT_MSGLOG_EXTDEV = 0x00000800`
- `LOG_SENDER_THREAD_ARG` 新增 `dwExtDevSubTypeMask` 字段
- `CWLServerTestDlg` 新增 `m_dwExtDevSubTypeMask` 成员

### SendInfoToServer 新增方法
- `SendClientNetAdapterLogToServer(LPCTSTR)`
- `SendClientExtDevLogToServer(LPCTSTR, DWORD)`

### 分类说明优化
- LogCategoryHelpDlg 添加扩展样式（整行选择 + 网格线）

## 📦 部署

- `artifacts/WLServerTestPublish/WLServerTest.exe`（x64 Release，3,590,656 字节）
- 依赖：`WLNetComm.dll`、`RawPacketEngine.dll`、`WLServerTest.ini`
