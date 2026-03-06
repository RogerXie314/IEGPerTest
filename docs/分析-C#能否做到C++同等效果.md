# 分析：C# 能否做到像 C++ 那样的稳定效果？

> **背景**：C++ 老工具（WLServerTest）在相同平台上配置相同 EPS 可以稳定运行；
> 照着 C++ 逻辑改写的 C# 串行调度器却仍然不稳定。
> 用户问：是不是 C# 做不到？另外，如果"根本原因在平台侧"，C++ 怎么就能工作？

---

## 0. 结论（先说）

**C# 能做到，但之前的分析有两处错误需要纠正。**

| 之前说法 | 实际情况 |
|---|---|
| "C# 串行已对齐 C++" | ❌ 错：async WriteAsync（~2ms）≠ C++ SendData（<0.1ms），一轮耗时超过 intervalMs 导致零等待 |
| "根本原因在平台侧" | ⚠️ 不准确：平台有约束，但 C++ 自然遵守，C# 当前实现违反——**修复点在客户端** |

---

## 1. 矛盾的正确解释

**用户质疑**：如果"根本原因在平台侧"，那么 C++ 和 C# 面对同一个平台，C++ 怎么能稳定？

**解答**：这句话是对的——之前说"根本原因在平台侧"措辞不准确。

更准确的描述是：

> 平台存在一个客观约束：**HB handler 的处理时间不能被日志 I/O 持续饥饿超过 90s**。
>
> - **C++ 工具**：天然遵守这个约束——`SendData()` 极速（<0.1ms/客户端），
>   加上 `Sleep(N)` OS 级真实停发，每轮发完后平台有 940ms 的安静窗口处理 HB。
>
> - **C# 当前实现**：违反这个约束——`WriteAsync` ~2ms/客户端，
>   600 客户端一轮 1200ms > intervalMs(1000ms)，轮间等待时间为 0，
>   向平台持续不断地发送，HB handler 被饥饿 90s 后触发 PolicyDrain。
>
> **修复点在客户端**，不是平台。C# 只要产生与 C++ 相同的发送波形（burst + 真实停发），
> 就能复现 C++ 的稳定性。

---

## 2. C++ 老工具的真实发送结构

来自 `WLServerTestDlg.cpp`（文档记录）：

```
独立日志线程（全局，处理所有 g_sock[]）：
    while (true) {
        for i in 0..N-1:
            SendData(g_sock[i], payload)  // 写入 OS TCP 内核缓冲区，< 0.1ms
        Sleep(N)                          // OS 内核级阻塞，真实停发 N ms
    }
```

关键数字（600 客户端，1 EPS）：
```
一轮发送时间 = 600 × 0.1ms = 60ms
Sleep(N)     ≈ 1000ms - 60ms = 940ms
发送波形     = [60ms 突发] [940ms 安静] [60ms 突发] [940ms 安静] ...
```

平台在每个周期有 **940ms 真实安静窗口**，HB handler 从容处理，90s watchdog 从未触发。

---

## 3. C# async 版本为什么失败

```
一轮发送时间 = 600 × ~2ms (WriteAsync async overhead) = 1200ms
intervalMs   = 1000ms
等待时间     = 1000 - 1200 = -200ms → 立即开始下一轮，零等待

发送波形     = [持续不断的 ~500 包/秒 drip，无任何停顿]
```

平台看到的是连续数据流，HB handler 被持续饥饿，90s 内无法处理 HB → PolicyDrain。

**"串行"只是逻辑顺序——async await 链的协程暂停不等于 OS TCP 停发。**

---

## 4. 正确的 C# 实现（已提交到本分支）

### 三项关键对齐

| C++ | 之前 C# async | 本次修复 C# |
|---|---|---|
| `SendData()` OS 阻塞调用，<0.1ms | `await WriteAsync/FlushAsync`，~2ms async 开销 | `Socket.Send()`，<0.1ms，对应 C++ `SendData()` |
| `Sleep(N)` OS 内核级阻塞 | `await Task.Delay(N)`，协程暂停，TCP 仍在发 | `ct.WaitHandle.WaitOne(N)` OS 级阻塞，TCP 真实停发 |
| 独立 OS 线程 | `Task.Run` 线程池 Task | `new Thread(...)`，专用非线程池 OS 线程 |

### 实现位置

`LogWorker.cs` — `StartCoreAsync` 方法中的 `[CppStyleDispatcher]` 块：

```csharp
// 激活条件：!useLogServer && intervalMs > 0 && _streamRegistry != null
// 即：TCP 模式 + 设置了 EPS + HB stream 可用（复用 HB socket，与 C++ g_sock[i] 一致）

var thread = new Thread(() =>
{
    while (!ct.IsCancellationRequested)
    {
        var roundStartMs = Environment.TickCount64;

        for (int ci = 0; ci < clients.Count; ci++)
        {
            bool ok = TrySendLogTcpSync(c.ClientId, pt); // Socket.Send，<0.1ms
            ...
        }

        // OS 级真实等待：对应 C++ Sleep(N)
        var waitMs = intervalMs - (int)(Environment.TickCount64 - roundStartMs);
        if (waitMs > 1)
            ct.WaitHandle.WaitOne(waitMs); // 真实停发，平台 HB handler 得到呼吸窗口
    }
});
thread.IsBackground = true;
thread.Name = "LogSerialDispatcher";
thread.Start();
```

`TrySendLogTcpSync`（同步发送，<0.1ms）：

```csharp
private bool TrySendLogTcpSync(string clientId, byte[] payload)
{
    bool lockAcq = _streamRegistry.TryAcquireWriteLock(clientId); // timeout=0，立即
    try
    {
        var hbStream = _streamRegistry.TryGet(clientId);
        if (hbStream == null || !lockAcq) return false;
        hbStream.Socket.Send(payload); // 同步，< 0.1ms
        return true;
    }
    finally { _streamRegistry.ReleaseWriteLock(clientId, lockAcq); }
}
```

### 预期效果

```
一轮发送时间 = 600 × ~0.1ms = ~60ms
OS 级等待    ≈ 940ms（ct.WaitHandle.WaitOne(940)）
发送波形     = [60ms 突发] [940ms 真实安静] [60ms 突发] [940ms 真实安静] ...
                ↑ 与 C++ 完全一致
```

---

## 5. 触发条件说明

`[CppStyleDispatcher]` 自动激活，条件：
1. `useLogServer == false`（使用 TCP 直连平台，而非日志服务器）
2. `messagesPerSecondPerClient > 0`（配置了 EPS）
3. `_streamRegistry != null`（HeartbeatStreamRegistry 已注入，有 HB 共享 stream）

不满足任一条件（如 HTTPS 模式、日志服务器模式、无 EPS 限速），自动回退到原有并行 Task 模式。

---

## 6. 一句话总结

> C# 能做到 C++ 的效果，关键在于用 **`Socket.Send` + 专用 OS 线程 + `WaitHandle.WaitOne`** 代替
> **`await WriteAsync` + 线程池 Task + `await Task.Delay`**。
>
> "根本原因在平台侧"是不准确的表述。平台有约束（HB 不能被饥饿 90s），
> C++ 天然遵守，C# async 版本违反。修复是客户端的代码改动，不需要等待平台修复。

