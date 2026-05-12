# IEGPerTest Simulator v3.9.7

发布日期：2026-04-29（NativeRunner 行为对齐补丁）

## 概述

v3.9.7 是 v3.9.6 的补丁版本，聚焦于将 NativeRunner.exe 的心跳/日志行为完全对齐老工具 `ConsoleMode.cpp`（WLServerTest），消除遗留的行为差异，确保在多主机并发场景下 EPS 稳定。

---

## 🐛 Bug 修复

### [关键] NativeRunner 心跳 send 失败时不应重连
- **问题**：v3.9.6 中 HB send 失败时会立即 `closesocket` + `CreateConnection` + 重试，而老工具 `ConsoleHeartbeatThread` 从不在 send 失败时主动重连，只依赖下一轮心跳间隔后自然重建连接
- **影响**：重连逻辑打断了心跳节奏，与老工具行为不一致，在网络抖动时反而加剧连接状态抖动
- **修复**：去掉 HB send 失败时的整个 close+reconnect+retry 块，send 失败仅计数 `s_hbSendFail++`，直接进入 `RecvHeartbeatReply`，与老工具完全一致

### [关键] NativeRunner 日志 send 失败时不应计为失败
- **问题**：v3.9.6 中 log `SendAll` 失败时走 `s_logSendFail++`，而老工具 `ConsoleLogThread` 不检查 `SendThreatLog_ToserverTCP` 的返回值，直接 `sentCount++`，即永远计为已发送
- **影响**：GUI 显示的失败数虚高，且行为与老工具不完全一致
- **修复**：去掉 `if/else` 检查，`SendAll` 返回值直接丢弃，`s_logSendOk++` 无条件执行，完全对齐老工具

### [优化] 白名单上传 filepath 参数去除冗余处理
- **问题**：`UploadOneClientAsync` 在已取文件名（`Path.GetFileName`）后仍调用 `BuildModifiedPath`，而该函数只处理 `:` 和 `\`，对纯文件名是空操作，属于历史遗留冗余代码
- **修复**：直接 `Base64UrlEncode(fileName)`，去掉多余的 `BuildModifiedPath` 调用，逻辑更清晰，行为与 v3.9.3 以来完全一致

---

## ✨ 新增（NativeRunner 内部优化，对外行为不变）

### 全局原子计数器 + O(1) OutputStats
- 新增 `g_nHBSending`、`g_nHBServerAck`、`g_nLogActive` 三个全局 `volatile long` 计数器
- 通过 `SlotSetConnected` / `SlotSetLastReplyOk` 辅助函数在 `InterlockedExchange` 时同步更新全局计数
- `OutputStats()` 直接读取三个全局计数，不再遍历 `g_clients[]`，从 O(n) 降为 O(1)
- 对齐老工具 `InterlockedIncrement/Decrement` 全局计数器模式

### Stats 间隔恢复 2 秒
- `StatsThreadProc` 的 `Sleep` 从 60s 恢复为 2s（对齐老工具输出频率）

### cmdId==0 不关闭 socket
- recv 返回 cmdId=0（超时/无数据）时，保持 socket 打开，`lastReplyOk` 不变，下轮继续发送
- 对齐老工具 `ConsoleHeartbeatThread` 无 else 分支的行为

---

## 与老工具差异对比（v3.9.7 后）

| 行为 | 老工具 ConsoleMode.cpp | v3.9.7 NativeRunner |
|---|---|---|
| HB send 失败 | 直接调 recv，不重连 | ✅ 相同 |
| Log send 失败 | 不检查返回值，sentCount++ | ✅ 相同 |
| cmdId==0 | socket 保持，继续 Sleep | ✅ 相同 |
| 掉线重连时机 | recv 返回 cmdId=18 | ✅ 相同 |
| Stats 全局计数 | InterlockedIncrement/Decrement | ✅ 相同 |
| Stats 输出频率 | 固定周期 | ✅ 2s（对齐） |

---

## 📦 部署说明

升级方式：替换 `NativeRunner.exe` 和 `SimulatorApp.exe`，其余文件无需更新。

| 文件 | 说明 |
|------|------|
| `SimulatorApp.exe` | 主程序（含白名单上传修复） |
| `NativeRunner.exe` | C++ 子进程（完全对齐老工具行为） |
