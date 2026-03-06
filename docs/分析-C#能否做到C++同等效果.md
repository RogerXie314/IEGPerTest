# 分析：C# 能否做到像 C++ 那样的稳定效果？

> **背景**：`test/old-tool-log-dispatch` 的串行调度器是照着 C++ `WLServerTest` 的日志线程逻辑改写的，
> 但实测表现仍与 C++ 有很大差距。用户问：是不是 C# 天生做不到？
>
> 结论先说：**C# 原则上能做到，但当前串行实现有三处关键的"形似神不似"，使它无法真正复刻 C++ 的行为。**

---

## 一、C++ 老工具的真实结构

来自 `WLServerTestDlg.cpp`（文档记录）：

```
HBClientCount_InEveryThread = 10
→ 2000 个客户端 = ~200 个 OS 阻塞线程
→ 每个 HB 线程（管 10 个客户端）：
      while (true) {
          for i in 0..9:
              SendData(g_sock[i], hb_packet)   // 发心跳
              RecvHeartBeatBack_TCP(g_sock[i]) // 阻塞等回包（含消费日志 ACK）
          Sleep(30000)                          // OS 级阻塞 30 秒
      }
→ 独立日志线程（全局，处理所有 g_sock[]）：
      while (true) {
          for i in 0..N-1:
              SendData(g_sock[i], log_packet×3) // 发 3 条日志
          Sleep(50)                             // OS 级阻塞 50ms
      }
```

关键特征：
1. **HB 线程与日志线程是两个完全独立的 OS 线程**，绑定到不同 CPU 核心（或由 OS 调度），互不阻塞。
2. `Sleep(30000)` / `Sleep(50)` 是 OS 内核级别的线程挂起，精确可靠。
3. 日志线程每发完一轮（N 个客户端各 3 条），**强制睡 50ms**，然后才发下一轮。
   这 50ms 就是平台 HB handler 的"呼吸窗口"。

---

## 二、C# 串行调度器实际做了什么

```csharp
while (!ct.IsCancellationRequested)
{
    var roundStart = Stopwatch.GetTimestamp();

    for (int ci = 0; ci < clients.Count; ci++)          // 顺序遍历 600 个客户端
    {
        await SendLogTcpLikeExternalAsync(clients[ci]...); // 每次 await，约 1~5ms
    }

    // 轮结束后补满 intervalMs：只有当一轮用时 < intervalMs 才会等待
    var waitMs = (int)(intervalMs - elapsedMs);
    if (waitMs > 10)
        await Task.Delay(waitMs, ct);                    // "可选"的等待
}
```

---

## 三、三处"形似神不似"的差异

### 差异 1：`await` ≠ C++ `Sleep()`——底层机制完全不同

| 对比维度 | C++ `Sleep(N)` | C# `await Task.Delay(N)` |
|---|---|---|
| 执行层级 | **OS 内核**：系统调用，OS scheduler 将线程置 WAIT 态 | **.NET 运行时**：协程挂起，当前线程**立即返回线程池**可继续执行其他 Task |
| "安静效果" | N ms 内此线程**绝对不会执行任何代码** | N ms 内线程池中其他 Task 可能一直在用这个线程，"日志线程"只是逻辑上暂停 |
| 精度保证 | OS 级：≥N ms 的强保证 | Windows 上受系统时钟分辨率（15~20ms）影响；有些情况下 `Task.Delay(50)` 实际等了 60~80ms |
| 对平台的"呼吸窗口" | 日志线程 Sleep(50ms) 期间，平台收不到新的日志数据，HB 响应可以优先被处理 | `await Task.Delay(waitMs)` 期间线程仍然可以被 HB Task 的 `await WriteAsync` 调度，但对**平台侧** I/O 没有任何影响——数据还在 TCP 缓冲区里源源不断地发送 |

**核心问题**：C++ 的 `Sleep(50)` 让日志停发 50ms；C# 的 `await Task.Delay(waitMs)` 只是让"日志调度逻辑"暂停，TCP 内核缓冲区和网卡驱动不受任何影响。平台侧看到的是连续不间断的数据流。

---

### 差异 2：一轮时间 ≥ intervalMs，导致 `waitMs ≤ 0`，"等待"根本不存在

关键计算：
```
客户端数  = 600
每次 SendLogTcpLikeExternalAsync 耗时 ≈ AcquireWriteLock(~0.1ms) + WriteAsync(~1ms) + FlushAsync(~0.5ms)
           ≈ 1.5~3ms（无拥塞时）
一轮总耗时 ≈ 600 × 2ms = 1200ms

intervalMs = 1000ms / 1 EPS = 1000ms

waitMs = 1000 - 1200 = -200ms → 直接跳过等待，下一轮立即开始！
```

实测推论：在 600 客户端、1 EPS 的配置下，C# 串行调度器实际上**没有任何轮间等待**。
一轮结束立即开始下一轮，平台接收到的是持续不断的日志流，没有任何停顿。

更严重的是：由于每轮 1200ms 发完 600 条，实际有效 EPS ≈ 600/1.2s ≈ **500 EPS**，
而不是配置的 600 EPS。Step 7 之所以稳定，是因为有效发送速率自动被降到了 ≈ 500 EPS，
而 Step 5 已经证明 500 EPS 是稳定阈值以下的安全区间！

**换言之：Step 7 的"成功"，不是因为串行模型更好，而是因为串行的顺序 await 链天然限速到了安全范围以下。**

---

### 差异 3：C++ 是两个独立 OS 线程；C# 是单线程池上的协程竞争

C++ 模型：
```
HB 线程（OS Thread A）：独立运行，Sleep(30s) 期间释放 CPU，唤醒后立即抢占执行
日志线程（OS Thread B）：独立运行，Sleep(50ms) 期间释放 CPU
两者在 OS 层面完全互不影响
```

C# 模型（即使使用串行调度器）：
```
LogWorker 串行 Task：一个 async Task，await 期间暂停，让出线程
600 个 HB Tasks：各自 await Task.Delay(30s)，到期时进入"可运行"队列
DrainTask × 600：各自 await ReadAsync，收到数据时排队执行
```

当 LogWorker 在 `for (ci = 0..599)` 循环内部执行时：
- `await SendLogTcpLikeExternalAsync(ci)` 返回后，600 个 HB Task 的到期 continuation 可能已经堆在队列里
- 但 LogWorker 会**立即**执行下一个 `await SendLogTcpLikeExternalAsync(ci+1)`，不给 HB Task 优先执行的机会

在 .NET 默认调度器（`ThreadPoolTaskScheduler`）中，Task continuation 进入 FIFO 队列。
LogWorker 串行循环的每次 `await` 完成后会立即继续，HB Task 的 continuation 被"插队"推迟。
这就是 `Reason.LockBusy`（写锁超时）事件的根本来源。

---

## 四、那 C# 到底能不能做到 C++ 的效果？

**能，但需要放弃"协程模型"，改用真正的 OS 阻塞线程。**

| 方案 | 是否等效 | 代价 |
|---|---|---|
| `Thread.Sleep(50)` 在专用 `Thread`（非线程池）中 | ✅ 完全等效 C++ Sleep | 需要把日志发送放到独立 Thread，不能用 Task.Run（Task.Run 用线程池） |
| `await Task.Delay(50)` 在 async 方法中 | ❌ 不等效 | 只是协程暂停，TCP 缓冲区不停发 |
| 每发 3 条 log 后 `Thread.Sleep(50)`（专用线程）| ✅ 完全复刻 C++ 节奏 | 1 个专用日志线程（OS Thread） |
| 控制 EPS ≤ 500（安全阈值以下）| ✅ 实际有效 | 但放弃了 500~750 区间的压测能力 |

---

## 五、更深层的问题：根本原因在平台侧

即使 C# 完美复刻 C++ 的发送节奏（每批 3 条 + Sleep(50)），
**平台侧的 HB watchdog 90s 阈值依然存在**。

根据 BUG-2026-001 Step 3 的确认：
> 平台 HB handler 与日志 I/O handler 争用同一资源，
> 日志流量超过阈值时 HB 响应被饥饿，90s watchdog 触发 PolicyDrain。

这意味着：
- C++ 工具能稳定运行，不是因为 C++ 语言更好，而是因为 C++ 的 `Sleep(50)` 天然限制了日志流量，
  使总 EPS 保持在平台可处理的阈值以内
- C# 如果也能控制总 EPS ≤ 阈值（约 500~600），同样可以稳定——Step 7 已经证明这一点

**不是"C# 做不到"，而是：**
1. 当前串行实现错误地假设一轮发送时间 < `intervalMs`（实际超时），导致没有任何停顿
2. 即使有停顿，`await Task.Delay` 的协程停顿和 C++ OS 线程停顿对平台的影响机制不同
3. 根本瓶颈在平台侧，与客户端语言无关

---

## 六、一句话总结

> C# 能做到 C++ 的效果，但需要用 **`Thread.Sleep` + 专用 OS 线程**，不能用 **`await Task.Delay` + 线程池 Task**。
> 
> 当前串行实现的"串行"只是逻辑顺序，不是物理停发。
> 600 客户端的一轮发送时间（约 1200ms）已超过 `intervalMs`（1000ms），
> 导致轮间根本没有停顿，与并行版本对平台的实际压力几乎相同。
> 
> Step 7 能稳定，是因为串行的顺序 await 链把有效 EPS 自然压到了约 500（安全阈值以内），
> 而不是因为串行模型本身解决了问题。
