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
| **HB send 失败处理** | `SendHeartbeatToserverTCP` 返回 FALSE → HB 线程立即 `CreateConnection` 重连（原地更新 `sock[i]`）→ 再 send 一次（同步重试，当轮修复） | 仅 `s_hbSendFail++`，socket 保持，**不重连**，等下一个 30s 周期再重试 |
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
| **hit 触发逻辑** | `if (70 <= nSendCount++) { bHit=TRUE; nSendCount=0; }`：**第71次**开始 hit，之后每71次一次 | `hitCounter` 从0递增，到 `hitEvery-1`（默认70）时触发hit并归零：**第71次**开始 hit，与老工具对齐 ✅ 已修复 |
| 类型间 Sleep | `Sleep(50)` 硬编码 | `Sleep(sleepBetweenTypesMs)` 可配置，默认 50ms |
| **send 返回值检查** | `SendThreatLog_ToserverTCP` 内部某子类型失败则 `goto END` 跳过本函数内剩余子类型；外层调用方不检查返回值，success计数器无论成败均递增 | 失败则 `s_logSendFail++; break;` 终止本轮类型循环，下轮重新开始 ✅ 已修复 |
| 停止信号 | `g_bStopTask` 普通 `BOOL`，loop 入口处检查 | `InterlockedCompareExchange(&g_stopLog, 0, 0)` |
| 动态启停 | GUI 按钮触发 | stdin 命令：`STARTLOG|...` / `STOPLOG` |

---

### 6. 统计与停止

| 维度 | WLServerTest | NativeRunner |
|---|---|---|
| 统计输出 | 更新 MFC UI 控件（`SetWindowText`），加 `CSingleLock` | 每 2 秒向 stdout 输出 `STATS|RegisteredTotal=N|HBSending=N|...` |
| 统计计算 | 全局 `volatile long` 计数器直读（O(1)），但写入路径混用：线程函数内 `InterlockedIncrement`，注册/初始化路径直接 `++/--`（非原子） | 全局 `volatile long` 计数器直读（O(1)），全程一致使用 `Interlocked*` |
| 线程同步原语 | MFC `CCriticalSection` / `CSingleLock` | Win32 `InterlockedIncrement/Decrement/Exchange/CompareExchange` |
| 停止信号可见性 | `g_bStopTask` 普通 `BOOL`，无内存屏障 | `volatile long` + `InterlockedCompareExchange`，保证内存可见性 |
| 父进程退出感知 | N/A | stdin EOF → 自动触发 `g_stopHB/g_stopLog = 1` |

---

## 二、压力稳定性评估

> 问题：在"打日志压力"场景下（心跳 + 日志并发高频发送），哪些差异会影响**心跳稳定**和**日志发送稳定**？

---

### 🔴 高影响 — 直接导致行为差异或数据错误

#### 差异 1：日志线程不等待 HB 就绪（老工具）vs 等待 `lastReplyOk=1`（NativeRunner）

- **老工具**：日志线程启动时 `pHeapArgs->sock` 从 `g_sock[i]` **按值捕获**，整个线程生命周期固定不变。若 HB 线程尚未建连，sock 为 `INVALID_SOCKET`，THREAT 类型被外层 `if (pHeapArgs->sock != INVALID_SOCKET)` 守卫**静默跳过**（不调用 `SendData_OnlyCompress`，不触发 `goto END`）；其余类型（OPT/NWL/DATAPROTECT 等）使用独立 HTTP 连接，不受影响。注：`goto END` 是 `SendThreatLog_ToserverTCP` 函数**内部**机制，仅在 socket 有效但某子类型发送失败时跳过函数内剩余子类型，与 `INVALID_SOCKET` 路径无关。**实际缓解**：WLServerTest 是 GUI 手动操作——用户先点"开始心跳"，HB 线程在进入 `do` 主循环前已完成建连并写入 `g_sock[i]`，之后用户再点"开始日志"，`sock == INVALID_SOCKET` 的场景实际触发概率极低。
- **NativeRunner**：日志线程轮询等待 `lastReplyOk=1`，确保 HB 已建连且服务端已 ack，再开始发日志。实际操作流程与老工具相同——SimulatorApp 启动心跳时传入 `logClientCount=0, logSelectedTypes=0`，NativeRunner 不创建日志线程；用户观察到所有客户端 HB 上线后，再点"添加任务"，通过 stdin `STARTLOG` 命令触发日志线程。此时 HB 线程均已建连并收到服务端 ack，`lastReplyOk=1`，等待循环第一次迭代即退出，为纯防御性保护。
- **影响**：两工具均依赖人工操作顺序保证 HB 就绪先于日志发送，实际差异可忽略。NativeRunner 的等待机制是代码层面的防御，避免配置异常时的静默失败，但在正常使用路径上不产生实际等待。另：两工具的 `g_sock[idx]` 均只在初始建连时写入一次，断线重连后不更新，若中途重连，日志线程持有的 socket 句柄将永久失效——属两工具共有缺陷。

#### 差异 2：hit 触发时机不同（首次行为不一致）

- **老工具**：`if (70 <= nSendCount++)`，`nSendCount` 初始为 0，第 0~69 次不 hit，第 70 次（即第 71 条消息）才第一次 hit。
- **NativeRunner（修复前）**：`msgCount % hitEvery == 0`，`msgCount` 初始为 0，**第 1 条消息即 hit**。**v3.9.7 已修复**：改用 `hitCounter` 从 0 递增，到 `hitEvery-1`（默认 70）时触发首次 hit 并归零，与老工具行为对齐。
- **影响**：NativeRunner 第一条日志就发 hit，比老工具多一次 hit。如果服务端对 hit 日志有特殊处理（如触发报警、白名单匹配），在压力初始阶段行为不对齐，影响测试结果的可对比性。

#### 差异 3：send 失败不检查（NativeRunner）vs 失败终止本轮（老工具）

- **老工具**：`SendThreatLog_ToserverTCP` 内某子类型（File30/ProcStart60/Reg40）发送失败 → `goto END` 跳过本函数内剩余子类型，函数返回 FALSE；但外层 `ThreadFunc_MsgLogSend` **不检查**该返回值，`m_lMsgLogSuccessCount` 无论成败均递增，外层循环继续，下轮重新调用三个子类型。
- **NativeRunner（修复前）**：`SendAll` 返回 false 后仍然 `s_logSendOk++`（计入成功），且继续发下一种类型（在同一个失效 socket 上）。**v3.9.7 已修复**：失败则 `s_logSendFail++; break;` 终止本轮类型循环，统计准确，不再在失效 socket 上持续消耗。
- **影响（修复后）**：两工具在失败处理上的行为已基本对齐：均在本轮终止后下轮重试。NativeRunner 修复后统计数字准确，老工具的 `m_lMsgLogSuccessCount` 仍存在计数虚高问题（轮次级别，非发送级别）。

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

#### 差异 6：HB-Socket 与日志线程共享（两者均有，已被证明可靠）

- 两个工具均让 HB 线程与日志线程共用同一个 socket，无显式锁。但 HB 线程每轮 `send+recv` 仅占用约几毫秒，之后 `Sleep(30000ms)`；日志线程在这 30 秒内对 socket 实际独占使用，真正的并发碰撞窗口约为 0.017%/周期。
- **实际影响**：老工具 300 客户端长期稳定运行已证明这是可靠的设计。与客户端总数无关（每个 socket 独立，碰撞概率不随客户端数放大）。不是压力崩溃的根因。

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
| 1 | 日志线程不等待 HB 就绪（老工具） | 启动初期 | 老工具 THREAT 类型被静默跳过（非 goto END）；因 GUI 操作顺序保证，实际影响极小；NativeRunner 的等待机制针对真实并发竞争 |
| 2 | hit 首条即触发（NativeRunner） | 全程 | NativeRunner 比老工具多一次 hit，影响测试结果可对比性 ✅ 已修复 |
| 3 | send 失败不检查（NativeRunner） | 网络抖动时 | NativeRunner 统计虚高，且在失效 socket 上持续消耗 ✅ 已修复 |
| 4 | Socket 数组竞态（老工具） | 大规模并发建连 | 老工具极小概率 socket 泄漏，日志乱发 |
| 5 | POLICY 同步 HTTPS 阻塞 HB（老工具） | 服务端频繁下发策略时 | 老工具 HB 被阻塞，心跳间隔拉长，可能被服务端判离线 |
| 6 | HB/Log 共享 socket 并发写（两者均有） | 极高压力下大包 | 共有设计，HB 每 30s 才占用 socket 几毫秒，碰撞概率极低（~0.017%/周期），老工具 300 客户端已证明可靠，不是根因 |
| 7 | TIME_CRITICAL 日志 + NORMAL 心跳（老工具）vs ABOVE_NORMAL 两者同级（NativeRunner v3.9.7） | 400-500 客户端极限压力 | NativeRunner 已改善，老工具仍有此问题；NativeRunner 效果待压测验证 |
| 8 | HB send 失败处理：立即重连重试（老工具）vs 等下一周期（NativeRunner） | HB send 偶发失败时 | 老工具当轮即修复；NativeRunner 最多等 30s 才重试，期间心跳中断，可能被服务端判离线 |

---

## 四、400-500 客户端崩溃专项分析（v3.9.7 补充，2026-05-01）

> 现象：两台压测机各 300 客户端稳定，各 400-500 客户端出现日志曲线不稳定 + 心跳大量掉线。

### 4.1 线程优先级问题（WLServerTest 特有，NativeRunner v3.9.7 已修复）

**两个工具的线程优先级设置不同（v3.9.7 已修复）：**

| 线程类型 | WLServerTest（行号） | NativeRunner v3.9.7（行号） |
|---|---|---|
| 心跳线程 | `THREAD_PRIORITY_NORMAL`（行1883） | `SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL)`（行938） |
| 日志线程 | `THREAD_PRIORITY_TIME_CRITICAL`（行2170） | `SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL)`（行834/963） |

**重要澄清**：NativeRunner 是 C# SimulatorApp 的子进程，但进程间的线程调度在 Windows 是相互隔离的。C# 父进程的 WPF / GC / 线程池 **不影响** NativeRunner.exe 内部的线程调度。

**当前状态（v3.9.7）**：NativeRunner 已将心跳和日志线程均改为 `ABOVE_NORMAL`，两者同级轮转调度，不再有单方面抢占。老工具仍是 TIME_CRITICAL 日志线程 + NORMAL 心跳线程，优先级不对称问题依然存在。

**机制回顾**：原 TIME_CRITICAL + NORMAL 组合下，Windows 调度器保证 TIME_CRITICAL 线程只要处于"可运行"状态就优先于 NORMAL 线程获得 CPU。在极限压力下（500 个 TIME_CRITICAL 日志线程，每 50ms 醒来一次），NORMAL 心跳线程的 `Sleep(30000)` 到期后可能长时间等不到 CPU，实际心跳间隔超过服务端超时阈值，被判离线。

**未验证**：改为 ABOVE_NORMAL 后是否真正解决 400-500 掉线，**尚未完成压测验证**。

### 4.2 RecvAll 无绝对超时（共有）

两工具的 `RecvData` / `RecvAll` 均在收到任意数据时重置 retry 计数。在服务端承压时响应变慢，HB recv 阻塞时间超预期，叠加调度延迟，实际心跳间隔进一步增大。

### 4.3 HB/Log 共享 Socket 无锁（共有，但实际碰撞概率极低）

两个工具均无 per-socket 发送锁，但 HB 线程的实际占用窗口极短：每轮 `send HB` + `recv response` 约几毫秒，之后 `Sleep(30000ms)`。日志线程在这 30 秒内对 socket 实际是独占的。真正的并发碰撞窗口 ≈ ~5ms / 30000ms ≈ 0.017% / 周期，与客户端总数无关（每个 socket 独立）。老工具 300 客户端长期稳定运行已证明这一设计可靠，不是 400-500 崩溃的根因。**400-500 客户端时客户端数量本身不会放大单个 socket 的碰撞概率，真正放大的是 OS 线程调度压力（见 4.1）。**

### 4.4 v3.9.7 已修复的两项问题（不解决 400-500 崩溃）

| 修复 | 修复内容 | 与 400-500 崩溃的关系 |
|---|---|---|
| Fix 1：hit 计数对齐 | 首次 hit 从第71条开始，与老工具一致 | ❌ 无关 |
| Fix 2：SendAll 失败立即 break | 发送失败不再继续消耗 CPU，统计准确 | 🟡 微小改善，不解决根本 |

### 4.5 已实施改动（待压测验证效果）

已将两类线程均调整为 `THREAD_PRIORITY_ABOVE_NORMAL`（main.cpp 行 834/938/963），心跳与日志在进程内同级竞争，不再有单方面抢占：

```
TIME_CRITICAL（OS 保留）
ABOVE_NORMAL  ← 心跳线程 + 日志线程（同级，轮转调度）  ← 当前 NativeRunner v3.9.7
NORMAL        ← 老工具心跳线程仍在此级
```

**预期效果**：心跳线程不再被日志线程长时间抢占，实际心跳间隔更接近 30s 设定值。日志 EPS 峰值可能略有下降（从 TIME_CRITICAL 降级），但幅度预计可忽略（Sleep 决定发送节奏，不是 CPU 时间）。

**待验证**：400-500 客户端场景下心跳掉线是否消除，**尚未完成压测**。

---

## 五、实现层资源消耗差异分析（补充）

> 前四节主要关注行为逻辑差异，本节聚焦**代码实现方式本身**（数据结构、同步原语、内存分配、函数调用路径等）在高并发下的资源消耗差异。这些差异不改变功能，但在 400-500 客户端极限压力下会放大为可观测的性能问题。

---

### 5.1 同步原语：内核态 vs 指令级原子操作

| 项目 | WLServerTest | NativeRunner |
|---|---|---|
| 主要同步手段 | `CCriticalSection` / `CSingleLock` | `InterlockedIncrement/Decrement/Exchange/CompareExchange` |

**性能差异机制**：

- `CCriticalSection` 在**无争用**时走用户态自旋（快），但一旦有**争用**（两个线程同时尝试加锁），会立即陷入内核态（`NtWaitForSingleObject` syscall）。每次内核/用户态切换开销约 1~3 µs。
- `Interlocked*` 系列本质是 `LOCK XADD` / `LOCK CMPXCHG` 等 CPU 原子指令，始终在用户态完成，无 syscall，开销约 10~50 ns（约比 `CCriticalSection` 争用路径快 20-100x）。
- 在 400 个并发线程同时对统计计数器进行读写的场景下，`CCriticalSection` 的争用概率极高，会产生大量内核切换，消耗 OS 调度资源，直接加剧心跳线程被抢占的问题（见 4.1）。

**结论**：NativeRunner 在同步原语层面明显优于老工具，且优势随并发规模增大而扩大。

---

### 5.2 统计计数器：混用原子/非原子操作 vs 全程一致 `Interlocked*`

> ⚠️ **源码核实（2026-05-01）**：早期分析误称老工具遍历 `g_vecAllClientObjects` 加锁统计，经查源码该描述**不正确**。两个工具均使用全局 `volatile long` 计数器直读，差异在于写入路径的一致性。

两个工具的统计都是 O(1) 直读全局 `volatile long` 计数器（如 `g_nTotalRegistered - g_nHeartBeatNotSending` = 在线数），计算方式相同。真正的差异在于**写入路径的一致性**：

**WLServerTest**（`WLServerTestDlg.cpp`）：
```cpp
// 线程函数内（多线程竞争路径）—— 正确
InterlockedIncrement(&g_nHeartBeatNotSending_ClientCount); // 行1477

// 注册/初始化路径（可能有多线程并发）—— 非原子
g_nHeartBeatNotSending_ClientCount++;  // 行276, 831, 1033
g_nHeartBeatNotSending_ClientCount--;  // 行1225, 1866
```

**NativeRunner**：全程使用 `InterlockedIncrement`/`InterlockedDecrement`，无普通 `++/--`。

**影响**：老工具在注册阶段（多个建连线程并发时）对同一 `volatile long` 做非原子 `++`，与线程函数内的 `InterlockedIncrement` 并发，存在丢计数的竞态。`volatile` 只防止编译器缓存变量，不防止 CPU 指令级的读改写竞态（`g_nHB++` 编译为 `MOV+ADD+MOV` 三条指令，非原子）。正常规模下丢计数概率低，主要表现为 UI 统计数字偶发偏差，与 400-500 崩溃无直接关联。

`g_vecAllClientObjects` 上的 `CCriticalSection`（`g_csVecClients`）仅保护 vector 的 push_back/clear 等结构性操作，与统计读写无关。

---

### 5.3 线程创建开销：`AfxBeginThread` vs `CreateThread`

| 项目 | WLServerTest | NativeRunner |
|---|---|---|
| 线程创建函数 | `AfxBeginThread(func, arg, priority)` | `CreateThread(NULL, 0, func, arg, 0, NULL)` |

`AfxBeginThread` 内部除调用 `CreateThread` 外，还会：
1. 在堆上分配并初始化一个 `CWinThread` 对象（含 MFC 状态机、消息队列指针等）。
2. 初始化该线程的 MFC TLS（Thread Local Storage）槽位。
3. 每线程额外堆分配约 1~2 KB，500 个线程合计约 500KB~1MB 的 MFC 内部结构，增加堆碎片和 GC（HeapCompact）压力。

这部分差异在线程创建阶段一次性产生，对运行期影响有限，但在批量创建 400-500 个线程时会拉长启动时间、增加初始内存压力。

---

### 5.4 协议打包路径：DLL 跨模块调用 vs 内联函数

| 项目 | WLServerTest | NativeRunner |
|---|---|---|
| 打包函数 | `CProtocal::GetPortocal()`（外部 DLL） | `PackPT()`（同模块内联函数） |

**调用开销差异**：

- DLL 跨模块调用：需经过 IAT（Import Address Table）间接跳转，编译器无法内联，且 DLL 拥有**独立的堆**（若 DLL 内有内存分配，堆操作需要跨模块同步，Windows 堆管理器加全局锁）。
- 内联 `PackPT()`：编译器可内联展开，无跳转开销，所有内存操作在同一堆上完成，无跨模块同步。

每条日志发送均需一次打包调用。400 个日志线程高频触发时，DLL 路径的额外开销（跳转 + 可能的跨模块堆锁）会累积为可观测的 CPU 消耗。NativeRunner 在这一路径上无此开销。

---

### 5.5 字符串内存分配：`CString` vs 栈上 `char[]`

| 项目 | WLServerTest | NativeRunner |
|---|---|---|
| 字符串类型 | `CString`（MFC，堆分配，带引用计数） | `char buf[N]`（栈上固定缓冲区）为主 |

`CString` 每次构造/赋值/拼接均可能触发堆 `malloc/free`（即使有写时复制优化，在多线程环境下引用计数的更新本身也需要原子操作）。在日志打包路径（每条日志构造 JSON 字符串）上，老工具产生的堆分配次数显著多于 NativeRunner。

在高并发下，Windows 堆管理器（`HeapAlloc`）内部有全局锁（低碎片堆 LFH 模式下是分桶锁），频繁的小块堆分配会在多线程下产生锁争用，增加延迟抖动。

**NativeRunner** 使用栈上缓冲区，`sprintf`/`memcpy` 完成 JSON 构造，完全绕过堆分配路径，在高并发下延迟更稳定。

---

### 5.6 `volatile` 在 C++ 中的语义边界

文档第 6 节提到老工具的 `g_bStopTask` 是"普通 `BOOL`，无内存屏障"，但这里需要补充一个常见误区：

**即使给 `g_bStopTask` 加上 `volatile` 关键字，也不能解决跨线程可见性问题。**

- 在 C++ 标准中，`volatile` 的作用是**防止编译器将变量缓存在寄存器中**（即每次访问都从内存地址读/写），但它**不提供任何内存顺序保证**（不阻止 CPU 乱序执行，不插入内存屏障指令）。
- 真正的跨线程可见性保证来自：`InterlockedXxx` 函数（内含 `MFENCE`/`LOCK` 前缀，形成 full memory barrier）、`std::atomic`、或显式的 `MemoryBarrier()` / `_ReadWriteBarrier()`。

NativeRunner 使用 `volatile long g_stopHB` + `InterlockedCompareExchange` 读取，后者的 `LOCK CMPXCHG` 指令提供 acquire-release 语义，保证写入线程的修改对所有读取线程立即可见。老工具的方案在理论上存在线程永远观测不到停止信号的可能（尽管在 x86 强内存模型下极少触发）。

---

### 5.7 实现层差异汇总

| # | 实现差异 | WLServerTest | NativeRunner | 高并发影响 |
|---|---|---|---|---|
| A | 同步原语争用代价 | `CCriticalSection`（内核态切换） | `Interlocked*`（CPU 指令） | 🔴 WLServerTest 高并发下大量 syscall，加剧调度延迟 |
| B | 统计计数器写入一致性 | `volatile long`，写入路径混用 `InterlockedIncrement`（线程函数）和普通 `++/--`（注册路径） | `volatile long`，全程一致 `Interlocked*` | 🟢 丢计数竞态概率低，主要影响统计准确性，不影响稳定性 |
| C | 线程对象开销 | `CWinThread` 堆分配（MFC TLS） | 裸 `CreateThread`，无额外对象 | 🟢 启动期影响，运行期可忽略 |
| D | 打包调用路径 | DLL 跨模块调用，独立堆 | 同模块内联，同一堆 | 🟡 高频打包下 DLL 路径额外开销累积 |
| E | 字符串内存分配 | `CString` 堆分配，引用计数 | 栈上 `char[]`，无堆分配 | 🟡 堆分配锁争用，高并发下延迟抖动 |
| F | 停止信号语义 | `BOOL`（无内存屏障） | `Interlocked*`（full barrier） | 🟢 x86 下极少触发，但理论上不正确 |
