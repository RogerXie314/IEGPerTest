# WLServerTest 老工具 vs NativeRunner (v3.9.7) 代码差异及压力稳定性评估

> 分析日期：2026-05-01  
> 对比对象：`external/IEG_Code/code/WLServerTest/` vs `src/NativeRunner/main.cpp`

---

## 一、完整差异表格

### 1. 架构

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 程序形态 | MFC GUI（`CDialog`，有窗口） | 纯 console 可执行文件，无 GUI |
| 框架依赖 | MFC：`CString`、`AfxBeginThread`、`CCriticalSection` | Win32 API + STL，无 MFC |
| 运行方式 | 独立进程，人工操作 GUI | C# `ProcessEngine.cs` 以子进程启动，stdin/stdout 通信 |
| 配置来源 | GUI 控件 + `CProfileConfig` 读 INI（`GetPrivateProfileString`） | `--config <path>` 命令行参数读自定义 INI |
| 客户端数据来源 | GUI 输入前缀/起始号/IP，HTTPS 注册后存入 `g_vecAllClientObjects` | 从 `Clients.log`（JSONL）加载已注册的 `ClientId/IP/DeviceId/TcpPort` |

---

### 2. 心跳线程

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 线程粒度 | 1线程/1客户端（`HB_CLIENTCOUNT_PER_THREAD = 1`，`.h:50`） | 1线程/1客户端 |
| 线程创建函数 | `AfxBeginThread(..., THREAD_PRIORITY_NORMAL)` | `CreateThread(...)` 继承默认优先级 |
| 建连错峰间隔 | `Sleep(500)` 硬编码 | `Sleep(connectGateMs)` 可配置，默认 500ms |
| Socket 数组上限 | `g_sock[1000]` | `g_sock[2000]` |
| **Socket 数组写入** | `g_nSocketCount++`，**非原子操作，多线程竞态** | `InterlockedIncrement(&g_nSocketCount)`，原子操作 |

---

### 3. 心跳发送与回包处理

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| HB JSON OS 版本 | 硬编码 `"Windows 7"` | 硬编码 `"Windows 10"` |
| HB 间隔 | 循环末尾 `Sleep(30000)` 硬编码 | `Sleep(g_cfg.hbIntervalMs)` 可配置，默认 30000ms |
| send 失败处理 | 同一轮次内立即 `CreateConnection` 重连 + 再 send（同步重试） | 记录统计，socket 保持，下轮循环判断 `INVALID_SOCKET` 再重连 |
| cmd=1/17 成功 | 继续循环 | 继续循环，`SlotSetLastReplyOk(1)` |
| **cmd=17 POLICY** | HB 线程内**同步**调用 `SendHeartbeat()`（HTTPS），完成后继续循环（阻塞本线程） | 向 stdout 发 `POLICY|idx|clientId`，C# 负责 HTTPS，HB 线程**不等待**直接继续 |
| cmd=18 NOREGISTER | HB 线程内同步调用 `RegisterClientToServer()`（HTTPS）→ 重连 → 重发，完成后继续（只阻塞本客户端线程） | 向 stdout 发 `REREGISTER|idx|clientId`，轮询等待最多 15s（等 C# 回 `DEVICEID|idx|newId`），再重连继续 |
| recv 失败/cmd=0 | `else { WriteInfo(...) }`，socket 保持，下轮继续 | 无 else 分支，socket 保持，`lastReplyOk` 不变，下轮继续 |

---

### 4. 协议打包

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 打包方式 | 调用外部 `CProtocal::GetPortocal()` 类（`WLProtocal/Protocal.h` DLL） | 内联 `PackPT()`：48字节 PT 头 + zlib，自包含，无外部依赖 |
| HB 打包（cmd=1） | `SendData()` → `protocal.GetPortocal(..., cmdID=1, ...)` | `PackPT(json, len, cmdId=1, deviceId, ...)` |
| 日志打包（cmd=21） | `SendData_OnlyCompress()` → `protocal.GetPortocal(..., em_portocal_compress_zlib, em_portocal_encrypt_none, ...)` | `PackPT(..., THREAT_CMDID=21, ...)` |

---

### 5. 日志线程

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 线程粒度 | 1线程/1客户端，`THREAD_PRIORITY_TIME_CRITICAL` | 1线程/1客户端，`THREAD_PRIORITY_TIME_CRITICAL` |
| 日志类型 | 多种可选（OPT/THT/NWL/BLINE/UKEY/DATAPROTECT/SYSPROTECT/BACKUP/VIRUS 等），GUI 勾选 | 固定 3 种：EventType 60（进程启动）、40（注册表）、30（文件访问） |
| 使用的 socket | `pHeapArgs->sock = g_sock[i]`，直接传入 | 启动后读 `g_sock[idx]`，与 HB 线程共享同一 socket |
| **等待 HB 就绪** | **无等待**，启动后立即发送（socket 可能尚未建连） | 轮询等待 `lastReplyOk = 1`（HB 首次收到服务端 ack）后才开始发送 |
| **hit 触发逻辑** | `if (70 <= nSendCount++) { bHit=TRUE; nSendCount=0; }`：**第71次**开始 hit，之后每71次一次 | `msgCount % hitEvery == 0`，hitEvery 默认 71：**第1次（msgCount=0）即 hit**，之后每71次一次 |
| 类型间 Sleep | `Sleep(50)` 硬编码 | `Sleep(sleepBetweenTypesMs)` 可配置，默认 50ms |
| **send 返回值检查** | 检查返回值，失败则 `goto END` 终止本轮，下一轮重新发送 | **不检查** `SendAll()` 返回值，无论是否成功直接 `s_logSendOk++` |
| 停止信号 | `g_bStopTask` 普通 `BOOL`，loop 入口处检查 | `InterlockedCompareExchange(&g_stopLog, 0, 0)` |
| 动态启停 | GUI 按钮触发 | stdin 命令：`STARTLOG|...` / `STOPLOG` |

---

### 6. 统计与停止

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 统计输出 | 更新 MFC UI 控件（`SetWindowText`），加 `CSingleLock` | 每 2 秒向 stdout 输出 `STATS|RegisteredTotal=N|HBSending=N|...` |
| 统计计算 | 遍历 `g_vecAllClientObjects`，加锁读各 client 标志位 | O(1) 直读全局原子计数器，不遍历 |
| 线程同步原语 | MFC `CCriticalSection` / `CSingleLock` | Win32 `InterlockedIncrement/Decrement/Exchange/CompareExchange` |
| 停止信号可见性 | `g_bStopTask` 普通 `BOOL`，无内存屏障 | `volatile long` + `InterlockedCompareExchange`，保证内存可见性 |
| 父进程退出感知 | N/A | stdin EOF → 自动触发 `g_stopHB/g_stopLog = 1` |

---

## 二、压力稳定性评估

> 问题：在"打日志压力"场景下（心跳 + 日志并发高频发送），哪些差异会影响**心跳稳定**和**日志发送稳定**？

---

### 🔴 高影响 — 直接导致行为差异或数据错误

#### 差异 1：日志线程不等待 HB 就绪（老工具）vs 等待 `lastReplyOk=1`（NativeRunner）

- **老工具**：日志线程启动后立即用 `g_sock[i]` 发送，此时 HB 线程可能还未完成建连，`sock[i]` 可能是 `INVALID_SOCKET`，日志发送直接失败（`SendData_OnlyCompress` 返回 FALSE，`goto END` 跳过本轮）。
- **NativeRunner**：日志线程轮询等待 `lastReplyOk=1`，确保 HB 已建连且服务端已 ack，再开始发日志。
- **影响**：老工具在启动初期可能丢失日志，而 NativeRunner 不会。但老工具失败后下轮会重试，长期稳定性影响有限；初始阶段日志发送量会偏低。

#### 差异 2：hit 触发时机不同（首次行为不一致）

- **老工具**：`if (70 <= nSendCount++)`，`nSendCount` 初始为 0，第 0~69 次不 hit，第 70 次（即第 71 条消息）才第一次 hit。
- **NativeRunner**：`msgCount % hitEvery == 0`，`msgCount` 初始为 0，**第 1 条消息即 hit**。
- **影响**：NativeRunner 第一条日志就发 hit，比老工具多一次 hit。如果服务端对 hit 日志有特殊处理（如触发报警、白名单匹配），在压力初始阶段行为不对齐，影响测试结果的可对比性。

#### 差异 3：send 失败不检查（NativeRunner）vs 失败终止本轮（老工具）

- **老工具**：`SendData_OnlyCompress` 失败 → `goto END`，本轮只发部分类型的日志，下轮继续。
- **NativeRunner**：`SendAll` 返回 false 后仍然 `s_logSendOk++`，统计数字虚高，且继续发下一种类型（在同一个失效 socket 上）。
- **影响**：在网络压力下 socket 出现短暂错误时，NativeRunner 的统计数字不可信（报告成功但实际未发出），且会在失效 socket 上反复尝试发送，**无谓消耗 CPU**，不会自愈。老工具跳出本轮，下轮重新开始，有一定自愈效果。

---

### 🟡 中影响 — 在特定条件下影响稳定性

#### 差异 4：Socket 数组写入竞态（老工具）

- **老工具**：`g_sock[g_nSocketCount++]`，`g_nSocketCount` 非原子递增。如果两个 HB 线程同时触发，可能两个线程读到相同的 `g_nSocketCount` 值，导致两个 socket 写入同一个槽位，其中一个 socket 丢失引用但不关闭（**socket 泄漏**）。
- **NativeRunner**：`InterlockedIncrement(&g_nSocketCount)` 保证原子性，无竞态。
- **影响**：老工具在大量客户端并发建连时（建连错峰间隔 500ms，100 个客户端需要 50s），竞态窗口极短，实际触发概率极低；但一旦触发，对应客户端的日志线程会拿到错误 socket，日志发向错误连接。NativeRunner 无此问题。

#### 差异 5：cmd=17 POLICY 处理时 HB 线程阻塞（老工具）

- **老工具**：收到 POLICY 回包后，HB 线程同步调用 `SendHeartbeat()`（HTTPS），该请求可能耗时数秒。在此期间，该客户端的 HB 循环停止，TCP 心跳连接空闲，服务端可能超时断开。
- **NativeRunner**：向 C# 发通知后立即继续，HB 线程不阻塞。
- **影响**：若服务端在压力测试中频繁下发 POLICY，老工具中 HB 线程会被 HTTPS 请求反复阻塞，实际心跳间隔拉长（超过 30s），可能被服务端判定离线。NativeRunner 无此问题。

#### 差异 6：HB-Socket 与日志线程并发读写（两者均存在，但 NativeRunner 更明显）

- 两者都让 HB 线程持有 socket 并发送 HB，同时日志线程读取同一个 socket 发送日志。HB 和 log 并发 `send()` 同一 socket，TCP 层面 `send()` 是线程安全的（内核序列化），但发出的数据流会交错，服务端收到的包头/包体可能来自两个线程混合写入（如果单次 send 的数据量跨越多个 `send()` 调用）。
- **实际影响**：两个工具的 `SendAll` 实现均以 1024 字节分块发送，小包情况下整包在单次 `send()` 内完成，交错概率低。但在极高压力下（日志包较大）存在包级别交错风险，服务端协议解析可能失败。这是两者**共有的**设计缺陷，非差异点。

---

### 🟢 低影响 — 基本不影响压力稳定性

#### 差异 7：cmd=18 NOREGISTER 处理等待时长

- 老工具同步注册（依赖 HTTPS 响应时间，通常 1~5s）；NativeRunner 等待最多 15s。
- 压力测试中 NOREGISTER 属于异常情况，不影响正常运行时的稳定性。

#### 差异 8：停止信号内存可见性

- 老工具 `g_bStopTask` 无内存屏障，极端情况下线程可能延迟感知停止信号，多跑几轮。
- 不影响发送稳定性，仅影响停止时机的精确性。

#### 差异 9：Socket 数组上限 1000 vs 2000

- 老工具最多支持 1000 个并发连接。超过 1000 客户端时会越界写入，但正常使用不会触发。

---

## 三、结论汇总

| # | 差异 | 影响场景 | 影响方向 |
|---|---|---|---|
| 1 | 日志线程不等待 HB 就绪（老工具） | 启动初期 | 老工具初期丢日志，NativeRunner 不丢 |
| 2 | hit 首条即触发（NativeRunner） | 全程 | NativeRunner 比老工具多一次 hit，影响测试结果可对比性 ✅ 已修复 |
| 3 | send 失败不检查（NativeRunner） | 网络抖动时 | NativeRunner 统计虚高，且在失效 socket 上持续消耗 ✅ 已修复 |
| 4 | Socket 数组竞态（老工具） | 大规模并发建连 | 老工具极小概率 socket 泄漏，日志乱发 |
| 5 | POLICY 同步 HTTPS 阻塞 HB（老工具） | 服务端频繁下发策略时 | 老工具 HB 被阻塞，心跳间隔拉长，可能被服务端判离线 |
| 6 | HB/Log 共享 socket 并发写（两者均有） | 极高压力下大包 | 共有缺陷，包交错风险 |
| 7 | TIME_CRITICAL 日志线程 vs NORMAL 心跳线程（两者均有） | 400-500 客户端极限压力 | 共有设计，见下方分析 |

---

## 四、400-500 客户端崩溃专项分析（v3.9.7 补充，2026-05-01）

> 现象：两台压测机各 300 客户端稳定，各 400-500 客户端出现日志曲线不稳定 + 心跳大量掉线。

### 4.1 线程优先级问题（共有，非 NativeRunner 独有）

**两个工具的线程优先级设置完全相同：**

| 线程类型 | WLServerTest（行号） | NativeRunner（行号） |
|---|---|---|
| 心跳线程 | `THREAD_PRIORITY_NORMAL`（行1883） | `CreateThread` 无 `SetThreadPriority` → 默认 NORMAL（行935） |
| 日志线程 | `THREAD_PRIORITY_TIME_CRITICAL`（行2170） | `SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL)`（行959） |

**重要澄清**：NativeRunner 是 C# SimulatorApp 的子进程，但进程间的线程调度在 Windows 是相互隔离的。C# 父进程的 WPF / GC / 线程池 **不影响** NativeRunner.exe 内部的线程调度。NativeRunner 进程内的情形与老工具完全相同：都是 TIME_CRITICAL 日志线程 + NORMAL 心跳线程共存。

**机制**：Windows 调度器保证 TIME_CRITICAL 线程只要处于"可运行"状态就优先于 NORMAL 线程获得 CPU。在极限压力下（500 个 TIME_CRITICAL 日志线程，每 50ms 或 intervalMs 醒来一次），NORMAL 心跳线程的 `Sleep(30000)` 到期后，可能需要等待所有 TIME_CRITICAL 线程进入 Sleep 才能获得 CPU。实际心跳间隔 = 30s + 等待调度的延迟，若超过服务端超时阈值则被判离线。

**未解的疑问**：若老工具曾稳定运行在 400-500 客户端，则优先级设置相同的两者应表现一致，优先级可能不是主因。**建议对比测试**：在同等条件下测试老工具是否同样在 400-500 出现掉线，以确认是共有限制还是 NativeRunner 特有回归。

### 4.2 RecvAll 无绝对超时（共有）

两工具的 `RecvData` / `RecvAll` 均在收到任意数据时重置 retry 计数。在服务端承压时响应变慢，HB recv 阻塞时间超预期，叠加调度延迟，实际心跳间隔进一步增大。

### 4.3 HB/Log 并发写同一 Socket 无锁保护（共有，高压下概率增大）

两个工具均无 per-socket 发送锁。在 400-500 客户端时，HB 线程和日志线程同时调用 `send()` 的概率随客户端数增加而增大。若单次发送需要多个 `send()` 调用（包体 > 1024 字节），两线程的数据在 TCP 流中交错，服务端协议解析失败，连接被关闭，导致心跳掉线和日志失败同时出现。

### 4.4 v3.9.7 已修复的两项问题（不解决 400-500 崩溃）

| 修复 | 修复内容 | 与 400-500 崩溃的关系 |
|---|---|---|
| Fix 1：hit 计数对齐 | 首次 hit 从第71条开始，与老工具一致 | ❌ 无关 |
| Fix 2：SendAll 失败立即 break | 发送失败不再继续消耗 CPU，统计准确 | 🟡 微小改善，不解决根本 |

### 4.5 建议改动（待验证效果）

将两类线程均调整为 `THREAD_PRIORITY_ABOVE_NORMAL`，使心跳与日志在进程内公平竞争，不再有单方面抢占：

```
TIME_CRITICAL（OS 保留）
ABOVE_NORMAL  ← 心跳线程 + 日志线程（同级，轮转调度）
NORMAL        ← 其他工作线程
```

**风险**：降低日志线程优先级可能略微降低日志发送的 EPS 峰值，但对 30s 间隔的心跳稳定性有明确改善。**是否真正解决 400-500 掉线，仍需测试验证。**
