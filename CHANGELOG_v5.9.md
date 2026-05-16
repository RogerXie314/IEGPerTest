# V5.9 更新记录 (WLServerTest)

发布日期：2026-05-16

## 🏗️ OPT 调度与日志路由大修

### 10 类日志独立 HTTPS 发送函数
- 新增 10 个按日志分类的 HTTPS 发送函数，每类对接到正确的平台接口
- ClientOps(0x01) → `/USM/clientOpsLog.do`（AdminLog）
- 病毒预警 → `/USM/clientVirusLog.do`
- 系统完整性检查 → `/USM/clientIntegrityLog.do`（IPC 二进制格式，对齐老 FiveType）
- 进程审计(0x0800) → ProcessAlert(Type=2, SubType=6)
- 非白名单 → ProcessAlert(Type=1, SubType=6)
- 白名单篡改 → ProcessAlert(Type=3, SubType=3)
- NWL(0x04) → ProcessAlert(Type=1, SubType=6) 单条发送
- 漏洞保护(0x0400) → `/USM/vulDefenseLog.do`
- 文件/注册表/MAC 防护 → HostDefence 统一端点（LogType 1/2/4）
- DLL加载/WinEvent → TCP 威胁通道 ProcessAlert(0,6)
- SendInfoToServer.h 新增 `SendClientIllegalConnectLogToServer` 声明

### 路由修复
- 修复 Outbound(0x04) 路由到 NWL，停止标志仅影响日志线程不影响心跳
- 修复 VulnProtect(0x0400) 路由到 HTTPS VulDefense 而非 TCP 威胁通道
- 统一文件/注册表/MAC 保护到 HostDefence 端点，对齐 C# 版

## 🐛 ProcessAlert 参数化修复
- ProcessAlert 现在实际使用 type/subType 参数（此前硬编码为 2,6）
- ProcessAlert Type=2 (audit) 而非 Type=1 (non-whitelist)
- WL 篡改 ProcessAlert Type=3, SubType=3；header 中补默认值

## 🐛 系统完整性检查重做
- 系统完整性检查改用 IPC 二进制格式（对齐老 FiveType 逻辑）
- 原"USB(old)"复选框重命名为"系统完整性检查"，补 DDX 映射
- dwTypes 构建器补上 m_catUsb.GetCheck()

## 🐛 TCP 通道修复
- TCP 通道掩码纳入 dwTypes 子类型位（对齐 V5.5 逻辑）
- TCP 通道直接设置 CLIENT_MSGLOG_THREAT（不再依赖隐藏 checkbox）
- TCP 通道 OnBnClickedButton_Lowest_AddTask 加 nHttpsC>0 守卫
- 仅 TCP 通道激活时跳过 HTTPS OnBnClickedButton_Lowest_AddTask

## 🐛 任务停止机制修复
- 同步 g_bStopTask 和 g_bStopLogTask 确保线程实际停止
- 旧日志线程停止前等待 300ms 再清除停止标志

## ✨ UI: 全不选按钮
- 新增"全不选"按钮，一键清除所有日志分类复选框
- 按钮位置调整到分类说明按钮之后

## 🐛 V5.8.x 累积修复
- bRecoveryLicense=FALSE 首次 USM 注册（对齐 C# licenseRecycle=false）
- 修复 OnBnClickedButton_Lowest_AddTask 中类型掩码清除导致日志类型选择丢失
- 短连接参数为 0 时跳过校验（有条件验证）
- 恢复 V5.8.6 中误删的 WL 文件选择按钮 handler
- 使用原始每秒值做校验（绕过强制最小值 1）
- 修复 V5.8.6 删除 dead handler 导致的心跳崩溃
- 清理隐藏控件残留

## 版本号

- `FILEVERSION` / `PRODUCTVERSION`：`5,9,0,0`
- `FileVersion` / `ProductVersion` 字符串：`5, 9, 0, 0`
- 主窗口标题：`WLServerTest V5.9 - 注册/心跳/日志/白名单 压测工具`

## 构建产物

- `artifacts/WLServerTestPublish/WLServerTest.exe`（x64 Release）
- `artifacts/WLServerTestPublish/WLNetComm.dll`
- `artifacts/WLServerTestPublish/RawPacketEngine.dll`
- `artifacts/WLServerTestPublish/WLServerTest.ini`
