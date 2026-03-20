// NativeEngine — 心跳+日志发送 C++ DLL
// 直接翻译老 C++ 工具 WLServerTest 的核心逻辑：
//   非阻塞 socket + OS 线程 + Sleep + send/recv
// payload 打包由 C# (PtProtocol) 完成，DLL 只负责连接管理与发送。

#define NATIVEENGINE_EXPORTS
#include "native_engine.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// ============================================================
//  内部数据结构
// ============================================================

struct ClientSlot {
    char     clientId[64];
    int32_t  deviceId;
    char     ip[32];
    int32_t  tcpPort;
    SOCKET   sock;              // 心跳+日志共享 socket (对齐 g_sock[])
    HANDLE   hbThread;          // 心跳线程句柄
    HANDLE   logThread;         // 日志线程句柄
    volatile long connected;    // 1=已连接
    volatile long lastReplyOk;  // 最近一次HB是否收到回包（1=是）
};

// 对齐老工具 HB_CLIENTCOUNT_PER_THREAD = 10
#define HB_CLIENTS_PER_THREAD  1   // 对齐老工具 HB_CLIENTCOUNT_PER_THREAD = 1（1客户端/线程）

// HB 线程组参数（堆分配，线程启动后 delete）
struct HBGroupArg {
    int startIdx;
    int count;
    int startDelayMs; // 错峰延迟：每个线程自行 Sleep，NE_StartHeartbeat 无需阻塞
};

// 全局状态
static NE_Config             g_config;
static std::vector<ClientSlot> g_clients;
static int32_t               g_clientCount = 0;

// 回调
static NE_NeedReregisterCallback g_onNeedReregister = nullptr;
static NE_BuildHBPayloadCallback g_onBuildHBPayload = nullptr;

// 停止标志
static volatile long g_stopHB  = 0;
static volatile long g_stopLog = 0;

// 统计
static std::atomic<int32_t> s_hbSendOk{0};
static std::atomic<int32_t> s_hbSendFail{0};
static std::atomic<int32_t> s_hbRecvOk{0};
static std::atomic<int32_t> s_hbRecvNoReg{0};
static std::atomic<int32_t> s_disconnects{0};
static std::atomic<int32_t> s_reconnects{0};
static std::atomic<int64_t> s_logSendOk{0};
static std::atomic<int64_t> s_logSendFail{0};

// 日志发送配置
struct LogSendConfig {
    NE_BuildLogPayloadCallback buildPayload;  // 每次发送前回调 C# 动态构建 payload
    int32_t typeCount;
    int32_t logClientCount;
    int32_t intervalMs;
    int32_t totalMessages;
    int32_t sleepBetweenTypesMs;
};
static LogSendConfig g_logCfg;

// WSA 初始化
static bool g_wsaInited = false;

// ============================================================
//  底层 Socket 操作（翻译自 SendInfoToServer.cpp）
// ============================================================

// 创建非阻塞 TCP 连接，对齐 C++ CreateConnection
// 返回 INVALID_SOCKET 表示失败
static SOCKET CreateConnection(const char* host, int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    // 设置非阻塞
    unsigned long ul = 1;
    ioctlsocket(s, FIONBIO, &ul);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    int nRes = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (nRes == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            // 等待连接完成，超时 2 秒（对齐老工具 tv_sec=2）
            fd_set writeSet, exceptSet;
            FD_ZERO(&writeSet);  FD_SET(s, &writeSet);
            FD_ZERO(&exceptSet); FD_SET(s, &exceptSet);
            timeval tm = {2, 0};
            nRes = select(0, NULL, &writeSet, &exceptSet, &tm);
            if (nRes <= 0 || FD_ISSET(s, &exceptSet) || !FD_ISSET(s, &writeSet)) {
                closesocket(s);
                return INVALID_SOCKET;
            }
        } else {
            closesocket(s);
            return INVALID_SOCKET;
        }
    }

    // 连接成功，保持非阻塞（对齐老工具 bug：ul 仍为 1）
    ioctlsocket(s, FIONBIO, &ul);
    return s;
}

// 非阻塞 send — 对齐 C++ SendData 的 send() 循环
// 返回: true=全部发送成功, false=失败（WSAEWOULDBLOCK 等）
static bool SendAll(SOCKET s, const uint8_t* data, int len) {
    int sent = 0;
    while (sent < len) {
        int chunk = (len - sent > 1024) ? 1024 : (len - sent);
        int n = send(s, (const char*)(data + sent), chunk, 0);
        if (n == SOCKET_ERROR) {
            // 非阻塞：WSAEWOULDBLOCK → 立即失败（对齐老工具 goto END）
            return false;
        }
        sent += n;
    }
    return true;
}

// 非阻塞 recv — 对齐 C++ RecvData (100次×20ms=2秒超时)
// 返回: true=收够 len 字节, false=超时或错误
static bool RecvAll(SOCKET s, uint8_t* buf, int len) {
    int recved = 0;
    int retryCount = 0;
    while (recved < len) {
        int n = recv(s, (char*)(buf + recved), len - recved, 0);
        if (n == SOCKET_ERROR || n == 0) {
            retryCount++;
            if (retryCount > 100) return false;  // 100×20ms = 2s
            Sleep(20);
        } else {
            recved += n;
            retryCount = 0;
        }
    }
    return true;
}

// 接收心跳回包 — 对齐 C++ RecvHeartBeatBack_TCP
// 返回 cmdId (1=正常, 17=策略, 18=未注册), 0=失败
//
// WL_PORTOCAL_HEAD = 48 字节 (Big-Endian):
//   [0-1]   szFlag    "PT"
//   [2]     cProtoVer
//   [3]     cSource
//   [4-5]   nBodyLen     (uint16 BE)
//   [6]     nEncryptType
//   [7]     nCompressType
//   [8]     nFillLen
//   [9]     nReserve
//   [10-11] nRandomKey   (uint16 BE)
//   [12-15] nSerialNumber(uint32 BE)
//   [16-19] CheckSum     (uint32 BE)
//   [20-23] nSessionID   (uint32 BE)
//   [24-31] nTime        (int64  BE)
//   [32-35] nCmdID       (uint32 BE) ← 我们要的字段
//   [36-39] nDeviceID    (uint32 BE)
//   [40-41] nSrcLen      (uint16 BE)
//   [42-47] sznReserve
static uint32_t RecvHeartbeatReply(SOCKET s) {
    uint8_t header[48];
    if (!RecvAll(s, header, 48)) return 0;

    // nBodyLen at offset 4, Big-Endian uint16
    uint16_t bodyLen = ((uint16_t)header[4] << 8) | header[5];

    // nCmdID at offset 32, Big-Endian uint32
    uint32_t cmdId = ((uint32_t)header[32] << 24)
                   | ((uint32_t)header[33] << 16)
                   | ((uint32_t)header[34] <<  8)
                   |  (uint32_t)header[35];

    // 读取并丢弃 body
    if (bodyLen > 0 && bodyLen < 1024 * 1024) {
        std::vector<uint8_t> body(bodyLen);
        RecvAll(s, body.data(), bodyLen);
    }

    return cmdId;
}

// ============================================================
//  心跳线程（对齐 ThreadFunc_HeartbeatSend_New）
// ============================================================

// ============================================================
//  HB 单客户端发送+接收（提取为辅助函数）
// ============================================================
static void HBDoSendRecv(ClientSlot& slot) {
    InterlockedExchange(&slot.lastReplyOk, 0);

    uint8_t hbBuf[4096];
    int32_t hbLen = 0;
    if (g_onBuildHBPayload)
        hbLen = g_onBuildHBPayload(slot.clientId, slot.deviceId, hbBuf, sizeof(hbBuf));
    if (hbLen <= 0) return;

    // 对齐老工具：发送失败 → 重连 → 重试发送 → 继续接收（不 return）
    bool sendOk = SendAll(slot.sock, hbBuf, hbLen);
    if (!sendOk) {
        s_hbSendFail++;
        closesocket(slot.sock);
        slot.sock = CreateConnection(g_config.platformHost,
                                     slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
        if (slot.sock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            s_reconnects++;
            if (SendAll(slot.sock, hbBuf, hbLen)) {
                s_hbSendOk++;
                // 继续往下接收（老工具在重连+重发后同样调用 RecvHeartBeatBack_TCP）
            } else {
                s_hbSendFail++;
                closesocket(slot.sock);
                slot.sock = INVALID_SOCKET;
                InterlockedExchange(&slot.connected, 0);
                s_disconnects++;
                return;  // 重试也失败，此轮放弃
            }
        } else {
            InterlockedExchange(&slot.connected, 0);
            s_disconnects++;
            return;  // 重连失败，此轮放弃
        }
    } else {
        s_hbSendOk++;
    }

    uint32_t cmdId = RecvHeartbeatReply(slot.sock);
    if (cmdId == 1 || cmdId == 17) {
        s_hbRecvOk++;
        InterlockedExchange(&slot.lastReplyOk, 1);
    } else if (cmdId == 18) {
        // 对齐老工具：NOREGISTER → 关连 → 回调重注册 → 重连 → 重发 HB
        s_hbRecvNoReg++;
        closesocket(slot.sock);
        slot.sock = INVALID_SOCKET;
        InterlockedExchange(&slot.connected, 0);
        s_disconnects++;
        if (g_onNeedReregister) g_onNeedReregister(slot.clientId);
        slot.sock = CreateConnection(g_config.platformHost,
                                     slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
        if (slot.sock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            s_reconnects++;
            // 老工具在 NOREGISTER 后重连并重发一次 HB（不接收回包，继续下轮）
            if (SendAll(slot.sock, hbBuf, hbLen)) s_hbSendOk++;
            else { s_hbSendFail++; closesocket(slot.sock);
                   slot.sock = INVALID_SOCKET;
                   InterlockedExchange(&slot.connected, 0); s_disconnects++; }
        }
    }
    // cmdId 其他值（0=超时, 策略等）：不断连，直接继续（对齐老工具）
}

// ============================================================
//  HB 线程（每线程管理 1 个客户端，对齐老工具 HB_CLIENTCOUNT_PER_THREAD=1）
//  老工具 ThreadFunc_HeartbeatSend_New：每线程1个客户端
// ============================================================
static DWORD WINAPI HBThreadProc(LPVOID param) {
    HBGroupArg* args = (HBGroupArg*)param;
    int startIdx    = args->startIdx;
    int count       = args->count;
    int startDelay  = args->startDelayMs;
    delete args;

    // 错峰延迟：各线程自行等待，不占用 NE_StartHeartbeat 调用线程
    // 对齐老工具：AfxBeginThread + Sleep(500) 的错峰效果，但不阻塞主线程
    if (startDelay > 0) Sleep(startDelay);

    // 1. 初始建连 + 首个 HB（顺序处理本组所有客户端）
    for (int i = startIdx; i < startIdx + count; i++) {
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
        ClientSlot& slot = g_clients[i];
        slot.sock = CreateConnection(g_config.platformHost,
                                     slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
        if (slot.sock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            // 首次建连不算"重连"，s_reconnects 只在断线后重建时累加
            HBDoSendRecv(slot);
        } else {
            InterlockedExchange(&slot.connected, 0);
        }
    }

    // 2. 主循环：Sleep(interval) → 顺序处理本组所有客户端
    while (!InterlockedCompareExchange(&g_stopHB, 0, 0)) {
        Sleep(g_config.hbIntervalMs);
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;

        for (int i = startIdx; i < startIdx + count; i++) {
            if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
            ClientSlot& slot = g_clients[i];

            // 断线重连
            if (slot.sock == INVALID_SOCKET || !InterlockedCompareExchange(&slot.connected, 1, 1)) {
                if (slot.sock != INVALID_SOCKET) { closesocket(slot.sock); slot.sock = INVALID_SOCKET; }
                slot.sock = CreateConnection(g_config.platformHost,
                                             slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
                if (slot.sock == INVALID_SOCKET) {
                    InterlockedExchange(&slot.connected, 0);
                    s_disconnects++;
                    continue;
                }
                InterlockedExchange(&slot.connected, 1);
                s_reconnects++;
            }

            HBDoSendRecv(slot);
        }
    }

    // 退出：关闭本组所有连接
    for (int i = startIdx; i < startIdx + count; i++) {
        if (g_clients[i].sock != INVALID_SOCKET) {
            closesocket(g_clients[i].sock);
            g_clients[i].sock = INVALID_SOCKET;
        }
        InterlockedExchange(&g_clients[i].connected, 0);
    }
    return 0;
}

// ============================================================
//  日志发送线程（对齐 ThreadFunc_MsgLogSend）
// ============================================================

static DWORD WINAPI LogThreadProc(LPVOID param) {
    int idx = (int)(intptr_t)param;
    ClientSlot& slot = g_clients[idx];

    // 对齐老工具 pHeapArgs->sock 值拷贝语义：
    // 老工具在 AfxBeginThread 创建线程时将 g_sock[i] 值拷贝到 pHeapArgs->sock，
    // 线程内部始终使用这个常量句柄，HB 重连后 g_sock[i] 变化对 log 线程透明。
    // 若 HB 重连并关闭旧 socket，log 线程对旧 handle 的 send() 会失败，
    // 但老工具不检查返回值，继续循环；我们同样不对失败断连，对齐该行为。
    // 等待 socket 就绪（HB 与 Log 同时启动时的竞争窗口，通常 < 几百 ms）。
    SOCKET sock = INVALID_SOCKET;
    while (!InterlockedCompareExchange(&g_stopLog, 0, 0)) {
        sock = slot.sock;
        if (sock != INVALID_SOCKET) break;
        Sleep(100);
    }
    if (InterlockedCompareExchange(&g_stopLog, 0, 0)) return 0;

    int msgCount = 0;
    int totalMsg = g_logCfg.totalMessages;

    // 对齐老工具 ThreadFunc_MsgLogSend：每次循环实时构建 payload（新时间戳、rand PID 等）
    // 使用栈上缓冲区，大小 128KB，足够容纳任意压缩后 payload
    static const int kBufSize = 128 * 1024;
    std::vector<uint8_t> payloadBuf(kBufSize);

    while (!InterlockedCompareExchange(&g_stopLog, 0, 0)) {
        if (totalMsg > 0 && msgCount >= totalMsg) break;

        if (sock == INVALID_SOCKET) {
            if (g_logCfg.intervalMs > 0) Sleep(g_logCfg.intervalMs);
            msgCount++;
            continue;
        }

        for (int t = 0; t < g_logCfg.typeCount; t++) {
            if (InterlockedCompareExchange(&g_stopLog, 0, 0)) break;

            // 每次发送前回调 C# 动态构建 payload（对齐老工具实时 rand()+timestamp）
            int payloadLen = 0;
            if (g_logCfg.buildPayload) {
                payloadLen = g_logCfg.buildPayload(idx, t, msgCount,
                    payloadBuf.data(), kBufSize);
            }
            if (payloadLen <= 0) {
                s_logSendFail++;
                break;
            }

            bool ok = SendAll(sock, payloadBuf.data(), payloadLen);
            if (ok) {
                s_logSendOk++;
            } else {
                s_logSendFail++;
                break;
            }

            // 对齐老工具 SendThreatLog_ToserverTCP：File→Sleep(50)→ProcStart→Sleep(50)→Reg
            if (t < g_logCfg.typeCount - 1 && g_logCfg.sleepBetweenTypesMs > 0)
                Sleep(g_logCfg.sleepBetweenTypesMs);
        }

        msgCount++;

        if (totalMsg > 0 && msgCount >= totalMsg) break;

        if (g_logCfg.intervalMs > 0) Sleep(g_logCfg.intervalMs);
    }
    return 0;
}

// ============================================================
//  导出 API 实现
// ============================================================

NE_API int32_t NE_Init(
    const NE_Config* config,
    const NE_ClientInfo* clients,
    int32_t clientCount,
    NE_NeedReregisterCallback onNeedReregister,
    NE_BuildHBPayloadCallback onBuildHBPayload)
{
    if (!g_wsaInited) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;
        g_wsaInited = true;
    }

    memcpy(&g_config, config, sizeof(NE_Config));
    g_onNeedReregister = onNeedReregister;
    g_onBuildHBPayload = onBuildHBPayload;

    g_clients.clear();
    g_clients.resize(clientCount);
    g_clientCount = clientCount;

    for (int i = 0; i < clientCount; i++) {
        auto& s = g_clients[i];
        strncpy(s.clientId, clients[i].clientId, sizeof(s.clientId) - 1);
        s.deviceId = clients[i].deviceId;
        strncpy(s.ip, clients[i].ip, sizeof(s.ip) - 1);
        s.tcpPort = clients[i].tcpPort;
        s.sock = INVALID_SOCKET;
        s.hbThread = NULL;
        s.logThread = NULL;
        s.connected = 0;
        s.lastReplyOk = 0;
    }

    // 重置统计
    s_hbSendOk = 0; s_hbSendFail = 0;
    s_hbRecvOk = 0; s_hbRecvNoReg = 0;
    s_disconnects = 0; s_reconnects = 0;
    s_logSendOk = 0; s_logSendFail = 0;

    InterlockedExchange(&g_stopHB, 0);
    InterlockedExchange(&g_stopLog, 0);

    return 0;
}

NE_API int32_t NE_StartHeartbeat() {
    InterlockedExchange(&g_stopHB, 0);

    // 对齐老工具：AfxBeginThread → Sleep(500) → AfxBeginThread → Sleep(500) → ...
    // 主循环串行创建线程，每创建一个后 Sleep(connectGateMs)，
    // 使任何时刻的线程数 == 已到时间上线的客户端数（观感与老工具完全一致）。
    // 线程本身内部不再需要 startDelayMs，直接建连即可。
    for (int i = 0; i < g_clientCount; i++) {
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
        HBGroupArg* args = new HBGroupArg{i, 1, 0};  // startDelayMs=0，由外层 Sleep 控制错峰
        HANDLE h = CreateThread(NULL, 0, HBThreadProc, (LPVOID)args, 0, NULL);
        g_clients[i].hbThread = h;
        if (g_config.connectGateMs > 0)
            Sleep(g_config.connectGateMs);  // 对齐 AfxBeginThread 后的 Sleep(500)
    }
    return 0;
}

NE_API int32_t NE_StartLogSend(
    NE_BuildLogPayloadCallback buildPayload,
    int32_t typeCount,
    int32_t logClientCount,
    int32_t intervalMs,
    int32_t totalMessages,
    int32_t sleepBetweenTypesMs)
{
    InterlockedExchange(&g_stopLog, 0);

    g_logCfg.buildPayload       = buildPayload;
    g_logCfg.typeCount          = typeCount;
    g_logCfg.logClientCount     = logClientCount;
    g_logCfg.intervalMs         = intervalMs;
    g_logCfg.totalMessages      = totalMessages;
    g_logCfg.sleepBetweenTypesMs = sleepBetweenTypesMs;

    int count = (logClientCount > g_clientCount) ? g_clientCount : logClientCount;
    for (int i = 0; i < count; i++) {
        HANDLE h = CreateThread(NULL, 0, LogThreadProc, (LPVOID)(intptr_t)i, 0, NULL);
        // 对齐老工具 AfxBeginThread(..., THREAD_PRIORITY_TIME_CRITICAL, ...)（第 2170 行）
        if (h) SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
        g_clients[i].logThread = h;
    }
    return 0;
}

NE_API void NE_StopAll() {
    InterlockedExchange(&g_stopHB, 1);
    InterlockedExchange(&g_stopLog, 1);

    // 等待所有线程退出
    for (int i = 0; i < g_clientCount; i++) {
        if (g_clients[i].hbThread) {
            WaitForSingleObject(g_clients[i].hbThread, 60000);
            CloseHandle(g_clients[i].hbThread);
            g_clients[i].hbThread = NULL;
        }
        if (g_clients[i].logThread) {
            WaitForSingleObject(g_clients[i].logThread, 60000);
            CloseHandle(g_clients[i].logThread);
            g_clients[i].logThread = NULL;
        }
    }
}

NE_API void NE_StopLogSend() {
    InterlockedExchange(&g_stopLog, 1);
    for (int i = 0; i < g_clientCount; i++) {
        if (g_clients[i].logThread) {
            WaitForSingleObject(g_clients[i].logThread, 60000);
            CloseHandle(g_clients[i].logThread);
            g_clients[i].logThread = NULL;
        }
    }
}

NE_API void NE_GetStats(NE_Stats* out) {
    if (!out) return;
    int connCount = 0;
    int repliedCount = 0;
    for (int i = 0; i < g_clientCount; i++) {
        if (g_clients[i].connected) connCount++;
        if (g_clients[i].lastReplyOk) repliedCount++;
    }
    out->hbConnected  = connCount;
    out->hbTotal      = g_clientCount;
    out->hbSendOk     = s_hbSendOk;
    out->hbSendFail   = s_hbSendFail;
    out->hbRecvOk     = s_hbRecvOk;
    out->hbRecvNoReg  = s_hbRecvNoReg;
    out->hbReplied    = repliedCount;
    out->disconnects  = s_disconnects;
    out->reconnects   = s_reconnects;
    out->logSendOk    = s_logSendOk;
    out->logSendFail  = s_logSendFail;
}

NE_API int32_t NE_IsLogSendRunning() {
    int count = (g_logCfg.logClientCount > g_clientCount) ? g_clientCount : g_logCfg.logClientCount;
    for (int i = 0; i < count; i++) {
        if (g_clients[i].logThread != NULL) {
            DWORD exitCode;
            if (GetExitCodeThread(g_clients[i].logThread, &exitCode) && exitCode == STILL_ACTIVE)
                return 1;
        }
    }
    return 0;
}

NE_API void NE_Shutdown() {
    NE_StopAll();
    g_clients.clear();
    g_clientCount = 0;
    if (g_wsaInited) {
        WSACleanup();
        g_wsaInited = false;
    }
}
