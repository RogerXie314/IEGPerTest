# 分析：1000 EPS 日志发送导致心跳掉线的可能原因

> **背景**：用 C# 工具往平台打 1000 EPS 日志会造成心跳掉线；
> 而用另一个 C++ 工具（串行发送逻辑）打到 5000 EPS 却非常稳定，没有掉线。
>
> 本文从代码层面分析可能的原因，**不涉及任何代码修改**。

---

## 一、两种工具的发送模型本质差异

| 维度 | C# 工具（本项目） | C++ 工具 |
|---|---|---|
| 发送方式 | **并发**：1000 个客户端各自独立 Task，几乎同时发送 | **串行**：全局一个接一个发送，发完一条才发下一条 |
| 写 socket 的节奏 | 每条日志发完立即进入下一条（接近连续写） | 两条日志之间存在天然间隔（上一条发完，下一条才开始） |
| 心跳与日志共用 socket | 是（同一 TCP stream，通过 `SemaphoreSlim` 写锁串行化） | 是（同一 `g_sock[i]`，串行逻辑天然保证写入顺序） |

---

## 二、C# 工具中最可能的根本原因

### 原因 1：写锁长时间被日志占用，心跳无法及时获取（**最主要原因**）

**机制说明**

本工具中，`HeartbeatWorker` 和 `LogWorker` **共用同一条 TCP 连接**
（`HeartbeatStreamRegistry` 中的 `NetworkStream`）。
为防止两者并发写入导致字节交叉，使用 `SemaphoreSlim(1,1)` 写锁。

关键代码（`HeartbeatWorker.cs`）：
```csharp
bool lockAcq = await _streamRegistry.AcquireWriteLockAsync(
    c.ClientId, 4000, sendCts.Token);  // 心跳：等锁最长 4000ms
if (!lockAcq)
{
    lastReason[idx] = Reason.LockBusy;  // 锁超时，本次心跳跳过
    continue;
}
```

关键代码（`LogWorker.cs`）：
```csharp
bool lockAcq = await _streamRegistry.AcquireWriteLockAsync(
    clientId, 200, ct);  // 日志：等锁最长 200ms
```

**高 EPS 时的问题**

当日志 EPS = 1000，且每条日志通过 TCP 发送（ThreatData 通道），
每个客户端的 `intervalMs = 1000 / EPS_per_client`。
假设 1000 个客户端、每客户端 1 条/秒，则每个客户端的写锁占用时间
= 一次 `WriteAsync + FlushAsync` 的实际耗时（通常 < 5ms）。

但在并发发送模式下，1000 个客户端的写操作**近乎连续地**抢占写锁：

```
时间轴（单个 clientId 的写锁）：
  t=0ms    LogWorker 客户端 #1 获取锁 → 写 5ms → 释放
  t=5ms    LogWorker 客户端 #1 立即再次尝试获取锁（下一条日志）
  t=5ms    HeartbeatWorker 也在等锁...
  t=10ms   ...LogWorker 再次抢到锁
  ...
  t=4000ms HeartbeatWorker 等待超时 → LockBusy → 本次心跳跳过
```

每个 `intervalMs` 周期内（如 1000ms），`LogWorker` 有机会多次写入同一客户端，
写完一条后几乎立即开始下一条，使得写锁几乎没有空闲时间。
`HeartbeatWorker` 等待 4000ms 仍无法获取锁，触发 `Reason.LockBusy`，
**连续多个周期都无法发出心跳**，平台侧判定心跳超时，将连接踢掉。

**C++ 工具为何不存在此问题**

C++ 工具采用全局串行发送：当前客户端的日志发完，才发下一个客户端的日志。
这意味着任意一个 `g_sock[i]` 在两次日志写入之间，存在明确的空闲窗口，
心跳写入总能在下次日志写入前完成。总 EPS 再高，到单个 socket 的写锁争用也不会恶化。

---

### 原因 2：`intervalMs` 计算精度问题导致实际发送速率超过预期

**机制说明**

```csharp
// LogWorker.cs
int intervalMs = 0;
if (messagesPerSecondPerClient.HasValue && messagesPerSecondPerClient.Value > 0)
    intervalMs = 1000 / messagesPerSecondPerClient.Value;  // 整数除法，向下取整
```

例如，设定 `messagesPerSecondPerClient = 3`，则 `intervalMs = 333ms`，
实际 EPS ≈ 3.003 条/秒（稍微超标）。在 1000 个客户端的叠加下，
实际总 EPS 略高于配置值，导致锁争用比预期更激烈。

这是次要因素，但在高 EPS 场景下会放大原因 1 的效应。

---

### 原因 3：心跳写锁等待超时后，TCP 连接被错误地 Dispose（历史 Bug，v3.3.0 已修复）

> ⚠️ 此问题在 v3.3.0 已修复，列出供参考。

**历史机制（v3.2.0 及之前）**

早期版本中，`AcquireWriteLockAsync` 超时会抛出 `TimeoutException`，
被外层 `catch` 误判为写失败（`WriteFailed`），进而执行：
```csharp
tcp?.Dispose();  // TCP 连接被销毁
alive = false;
```

结果：TCP 本来还有效，却被主动关闭，HeartbeatWorker 触发重连，
重连期间平台认为设备离线。

**v3.3.0 的修复**

改为检测 `lockAcq == false` 时**仅跳过本次心跳**（`continue`），
不 Dispose TCP。但原因 1 描述的锁饥饿问题在 v3.3.0 中仍然存在——
跳过心跳的频率增加，最终还是导致平台超时踢人。

---

### 原因 4：1000 个并发任务的线程池调度压力

**机制说明**

C# 中 `Task.Run` 启动 1000 个并发任务，`TaskScheduler`（线程池）需要调度这些任务。
当 EPS 较高时，线程池中大量任务处于就绪状态，可能引发：

- **调度延迟**：某些客户端的任务被推迟执行，导致 `HeartbeatWorker` 的心跳
  `Task.Delay(intervalMs)` 到期后无法及时执行写操作。
- **上下文切换开销**：大量任务频繁切换增加 CPU 开销，进一步加剧调度延迟。

C++ 工具为单线程串行模型，不存在此问题：所有操作在一个线程中顺序执行，
无调度抖动。

---

### 原因 5：TCP 发送缓冲区背压

**机制说明**

当日志发送速率超过平台的处理能力，TCP 发送缓冲区可能被打满。
此时 `WriteAsync` 会阻塞等待缓冲区腾出空间，占用写锁的时间变长（原来 5ms → 可能几十 ms），
进一步加剧原因 1 的锁争用。

C++ 串行工具因为两次写之间存在天然间隔，缓冲区有时间排空，不容易触发背压。

---

## 三、原因优先级总结

| 优先级 | 原因 | 影响程度 |
|---|---|---|
| ⭐⭐⭐ 最主要 | 写锁长时间被 LogWorker 饥饿占用，HeartbeatWorker 4000ms 超时后跳过心跳，平台踢连接 | 直接导致掉线 |
| ⭐⭐ 次要 | TCP 发送缓冲区背压，WriteAsync 阻塞时间变长，放大锁饥饿效应 | 恶化主要原因 |
| ⭐⭐ 次要 | 线程池调度抖动，心跳无法在精确的 intervalMs 时刻触发 | 间接加剧掉线 |
| ⭐ 次要 | intervalMs 整数除法导致实际 EPS 略超配置值 | 轻微放大 |
| ℹ️ 历史 | 锁超时误判为写失败，Dispose 有效 TCP（v3.3.0 已修复） | 历史原因 |

---

## 四、C++ 工具为何 5000 EPS 仍稳定

1. **串行写入**：每个 socket 的两次写之间有明确空闲窗口，心跳总能插入。
2. **单线程调度**：无线程池调度抖动，心跳时序精确。
3. **背压自然缓解**：串行发送速率受前一条发送耗时限制，不会瞬间打满 TCP 缓冲区。
4. **EPS 的分布不同**：C++ 的 5000 EPS 是全局总量，单 socket 上的写频率 = 5000 / 客户端数；
   C# 的 1000 EPS 可能来自更高的"每客户端 EPS × 客户端数"组合，
   对单个 socket 的写压力反而更高。

---

## 五、关键代码位置

| 文件 | 行为 |
|---|---|
| `src/SimulatorLib/Workers/HeartbeatStreamRegistry.cs` | 写锁 `SemaphoreSlim` 管理，`AcquireWriteLockAsync(timeout=4000ms)` |
| `src/SimulatorLib/Workers/HeartbeatWorker.cs` `Reason.LockBusy = 6` | 记录锁超时跳过心跳事件 |
| `src/SimulatorLib/Workers/LogWorker.cs` `AcquireWriteLockAsync(timeout=200ms)` | 日志写锁，200ms 快速失败 |
| `heartbeat_monitor_*.log` | 运行时输出，可观察 `⚡锁竞争跳过:N` 字段确认问题是否出现 |
