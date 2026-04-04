# 线程模型正确理解（2026-04-04）

## 老工具（WLServerTest）的线程模型

### 心跳线程
- **每客户端1个持久的OS线程**
- 线程一直运行，30秒周期发送心跳
- 负责socket的创建、管理、重连
- 代码：`HB_CLIENTCOUNT_PER_THREAD = 1`

### 日志线程
- **每个发送日志的客户端创建1个临时的OS线程**
- 线程发送完指定数量的日志后自动退出
- 复用心跳线程创建的socket（`pHeapArgsForAllThread_MsgLog->sock = g_sock[i]`）
- 代码：`AfxBeginThread((AFX_THREADPROC)ThreadFunc_MsgLogSend,...)`
- 退出条件：`if (iThisClient_CurrentSuccessCount == pHeapArgs->iMsgLog_EachClientTotalCount) break;`

### 线程数观察
- 500个客户端启动心跳：500个心跳线程 + 15个系统线程 = **515个线程**
- 100个客户端发送日志：
  - 启动时：515 + 100 = **615个线程**
  - 发送完毕后：日志线程退出，回到 **515个线程**

## NativeEngine.dll 的实现

### 完全对齐老工具

#### 心跳线程
```cpp
// HBThreadProc - 持久运行
while (!g_stopHB) {
    Sleep(g_config.hbIntervalMs);  // 30秒
    HBDoSendRecv(slot);  // 发送心跳
}
```

#### 日志线程
```cpp
// LogThreadProc - 发送完退出
while (!g_stopLog) {
    if (totalMsg > 0 && msgCount >= totalMsg) break;  // 退出条件
    // ... 发送日志 ...
    msgCount++;
    Sleep(g_logCfg.intervalMs);
}
return 0;  // 线程退出
```

#### Socket共享
```cpp
struct ClientSlot {
    SOCKET sock;        // 心跳线程创建和管理
    HANDLE hbThread;    // 心跳线程句柄（持久）
    HANDLE logThread;   // 日志线程句柄（临时）
};

// 日志线程复用心跳线程的socket
SOCKET sock = slot.sock;  // 对齐老工具 g_sock[i]
```

## 文档描述建议

### 简洁版（用于README）
- 每客户端1个持久心跳线程 + 临时日志线程（发送完退出）
- 日志线程复用心跳线程的socket

### 详细版（用于技术文档）
- **心跳线程**：每客户端1个OS线程，持久运行，30秒周期发送心跳，负责socket生命周期管理
- **日志线程**：每个发送日志的客户端创建1个临时OS线程，发送完指定数量后自动退出
- **Socket共享**：日志线程复用心跳线程创建的socket（对齐老工具`g_sock[]`机制）
- **线程数**：500客户端 = 500个持久心跳线程；启动100客户端发日志时短暂增加100个临时线程

## 之前理解的错误

❌ **错误**：每客户端2个持久的OS线程（心跳 + 日志）
✅ **正确**：每客户端1个持久的心跳线程 + 临时的日志线程（发送完退出）

这解释了为什么观察到的线程数是515而不是615：
- 如果日志线程是持久的，应该是500心跳 + 100日志 = 600 + 15 = 615
- 实际是515，说明日志线程已经发送完毕并退出了
