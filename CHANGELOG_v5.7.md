# V5.7 更新记录 (WLServerTest)

发布日期：2026-05-14

## 🐛 缺陷修复

### 网口插拔 / 外设控制 / U盘插拔 短连接漏发（关键）
- **现象**：V5.6 单独勾选这三类时，平台收不到任何短连接日志（必须同时勾选"病毒预警"才偶尔有数据）
- **根因**：[WLServerTestDlg.cpp:8882](external/IEG_Code/code/WLServerTest/WLServerTestDlg.cpp#L8882) 日志线程 `CLIENT_MSGLOG_Virus` 块缺一对闭合 `}`，导致 NETADAPTER / EXTDEV / UDISKPLUG 三段分发被嵌套到 Virus 内部
- **修复**：在 `SendClientVirusLogToServer(...)` 后补 `}`，删除 UDISKPLUG 之后多余的 `}`，四类分发同级独立

## 🎨 日志输出优化

### 时间戳加日期
- `[HH:MM:SS.mmm]` → `[YYYY-MM-DD HH:MM:SS.mmm]`
- 便于长时间压测时区分跨天日志

### 移除冗余 debug 日志
- 移除每个日志线程启动时刷的 `[LOG-DIAG] Thread started, SelectedLogType=...` 块
- 移除 `[IEG/EDR] ApplyProjectTypeSelection called` 调试输出

## 版本号

- `FILEVERSION` / `PRODUCTVERSION`：`5,7,0,0`
- `FileVersion` / `ProductVersion` 字符串：`5, 7, 0, 0`
- 主窗口标题：`WLServerTest V5.7 - 注册/心跳/日志/白名单 压测工具`

## 构建产物

- `artifacts/WLServerTestPublish/WLServerTest.exe`（x64 Release）
- `artifacts/WLServerTestPublish/WLNetComm.dll`
- `artifacts/WLServerTestPublish/RawPacketEngine.dll`
- `artifacts/WLServerTestPublish/WLServerTest.ini`
