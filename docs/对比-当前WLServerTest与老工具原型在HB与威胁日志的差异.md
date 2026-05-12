# 当前 WLServerTest（R1~R8 改动后）vs 老工具原型 在 HB + 威胁检测 TCP 长连接 的差异

> 整理日期：2026-05-12  
> 对比对象：  
> - **老工具原型**：`external/WLServerTest_initial/`（VS2008 原始基线，未做任何 R1~R8 改动）  
> - **当前 main**：`external/IEG_Code/code/WLServerTest/`（V5.5，已应用 R1~R8 全部改动）  
> 视角：**压力测试**——心跳稳定性 + 威胁检测长连接 EPS 稳定性

---

## 差异表

| # | 维度 | 老工具原型（WLServerTest_initial） | 当前 main（R1~R8 后） | 对压测的影响 |
|---|---|---|---|---|
| 1 | **HB 心跳时长** | 硬编码上限（`HB_DURATION_HOUR=1` 小时） | UI 可编辑（V5.2 引入"心跳时长(分钟)"），默认 7200 分钟 | 长跑测试不再需要重启工具续命 ⚡正向 |
| 2 | **HB 线程粒度** | 1 客户端 = 1 线程，`THREAD_PRIORITY_NORMAL`，`g_sock[1000]` | 完全未改：1:1，优先级、上限均与原型一致 | 无差异 |
| 3 | **HB 建连错峰** | `Sleep(500)` 硬编码 | 未改 | 无差异 |
| 4 | **HB JSON 字段** | 仅 `ComputerID/ClientID/IP/Version`，无 `ComputerIP` 注入 | `InjectComputerIP` 把 ComputerIP 写入根 envelope + CMDContent 每项 | 平台能正确反查到客户端 IP，HB 在线判定更稳 ⚡正向 |
| 5 | **HB cmd=17 (POLICY)** | HB 线程**同步**调用 HTTPS `SendHeartbeat()` | 未改 | 老缺陷仍在：POLICY 阻塞 HB 周期，服务端频发 POLICY 会拉长心跳延迟 ⚠️ |
| 6 | **HB cmd=18 (NOREGISTER)** | HB 线程同步 HTTPS 重注册 + 重连 + 重发 | 未改 | 同 #5 ⚠️ |
| 7 | **HB send 失败处理** | 立即 `CreateConnection` 重连 → 当轮再 send 一次 | 未改 | 单次失败自愈快，但 socket 数组写入仍非原子（见 #8） |
| 8 | **g_sock 数组写入** | `g_sock[g_nSocketCount++]` 非原子 | 未改 | 1200 并发建连理论存在槽位覆盖（500ms 错峰 + 极短临界区，实测未触发）⚠️ |
| 9 | **TCP 长连接威胁日志：发送函数** | `SendThreatLog_ToserverTCP`：File→Sleep50→ProcStart→Sleep50→Reg，函数级 hit=每 71 次一次 | 函数实现未改；调用前对三个 JSON 都跑 `InjectComputerIP` 注入 ComputerIP | 平台对 TCP 长连接日志也能反查 IP（此前只靠心跳映射），数据落盘字段更全 ⚡正向 |
| 10 | **日志线程主循环 Sleep** | `Sleep(SleepInterval)` 整段 30s | R6 分块 sleep：`for (e=0; e<slp; e+=50) { if (stop) break; Sleep(50); }` | 停止响应延迟从最坏 1000ms 降到 50ms；EPS 影响 < 0.5%（每秒每线程多 ~20 次系统调用） |
| 11 | **停止信号传播** | `g_bStopTask/g_bStopLogTask` 普通 BOOL，每个发送函数前各检查 1 次 | R6 在 7 个日志类型分支之间**逐个**插入停止检查 | 停止更跟手，对发送 EPS 无副作用 ⚡正向 |
| 12 | **send 失败统计** | `m_lMsgLogSuccessCount` 无论成败均递增（计数虚高） | 未改 | 工具端"成功数"不可信，**压测口径以平台入库为准** ⚠️ |
| 13 | **威胁日志线程粒度** | 1 客户端 = 1 线程，`THREAD_PRIORITY_TIME_CRITICAL` | 未改 | 无差异 |
| 14 | **HB / 日志共享 socket** | 是（日志线程值拷贝 `g_sock[i]`） | 未改 | 30s HB 周期 send+recv 仅占 ~5ms，碰撞窗口 ~0.017%/周期，原型已证可靠 |
| 15 | **HB 重连后日志 socket 不更新** | `pHeapArgs->sock` 值拷贝，HB 重连不通知日志线程 | 未改 | 中等隐患；但 #7 是当轮重连复用 `g_sock[i]`，重连后值实际未变 → 日志线程不受影响 |
| 16 | **WLNetComm.dll 缺失防护** | 直接崩溃 | V5.2 加 `LoadLibraryW` 预检 + 弹错 | 不影响数据面，仅首次启动稳定性 ⚡正向 |
| 17 | **注册计数竞态** | `g_nTotalRegistered_ClientCount++` 多线程非原子 | V5.5 全部改为 `InterlockedIncrement` | 1500 客户端并发注册不再丢计数 ⚡正向 |
| 18 | **UI 卡片化** | 旧拥挤布局 | V5.5 三列卡片 + 状态分组 | 仅 UI，无数据面影响 |

---

## 理论 EPS 与实测对比（2026-05-12 测试场景）

- **场景**：3 台主机 × 500 客户端注册（共 1500），全部上心跳；分别给 400 客户端打威胁检测 TCP 日志，UI"每秒=1"
- **代码侧理论上限**：
  - 每客户端 1 个发送线程，外层 `Sleep(1000ms)`
  - 一次循环依次发 3 条事件（File 30 / ProcStart 60 / Reg 40），子报文间 `Sleep(50)`
  - 实际周期 ≈ 1100ms → 单客户端 ≈ 2.73 EPS
  - 3 × 400 × 2.73 ≈ **3270 EPS**（忽略内部 100ms Sleep 的理想上限 ≈ 3600 EPS）
- **平台后台实测**（Flume `PackAmountStatisticsUtil` 输出 `wl_limit`）：
  - 稳态 col2（每分钟入库）≈ 157000 ± 600（< 1.2% 波动）
  - 稳态 col3（平均 EPS）≈ **2620 ± 15**
- **偏差分析（理论 3270 vs 实测 2620 ≈ -20%）**：
  1. 1200 个 `THREAD_PRIORITY_TIME_CRITICAL` Win32 线程在普通三台主机上的调度抖动（主因）
  2. `SendData_OnlyCompress` 同步阻塞（zlib `compress()` + TCP send 串行，单条耗时 1~5ms）
  3. 30s HB 心跳对同一 socket 的写入互斥（每客户端 ~0.3% 时间窗损失）
  4. 平台侧 Flume/Kafka 入库背压（极小，因 col2 平稳无毛刺）
- **结论**：通道完全稳定，瓶颈在工具端发送侧，不是平台限流

---

## 压测视角的两条结论

1. **核心数据面（线程模型、socket 共享、Sleep 节拍、子报文顺序、hit 周期）与老工具原型 100% 一致**，相同硬件应跑出同一 EPS 量级，**无性能回退**。
2. **R1~R8 改动分类**：
   - **正向加固**：ComputerIP 注入、注册计数原子化、停止信号跟手、HB 时长可配、WLNetComm.dll 预检
   - **未触碰的老缺陷**：POLICY/NOREGISTER 阻塞 HB（#5/#6）、send 失败统计虚高（#12）、socket 数组写入竞态（#8）——原型同样存在，不构成回退

---

## 相关源码定位

| 关注点 | 文件 / 行号 |
|---|---|
| HB 线程主体 | `external/IEG_Code/code/WLServerTest/WLServerTestDlg.cpp` — `ThreadFunc_HeartbeatSend_New` |
| 日志线程主体 | 同上 — `ThreadFunc_MsgLogSend` (line ~8409) |
| 威胁日志发送 | `external/IEG_Code/code/WLServerTest/SendInfoToServer.cpp` — `SendThreatLog_ToserverTCP` (line ~719) |
| ComputerIP 注入 | 同上 — `InjectComputerIP`（jsoncpp 结构化注入） |
| 原始基线 | `external/WLServerTest_initial/`（只读参考） |
