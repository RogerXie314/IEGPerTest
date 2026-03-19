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
#include <mutex>

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
    // per-client payloads: payloads[clientIdx * typeCount + typeIdx]
    std::vector<std::vector<uint8_t>> payloads;
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

static DWORD WINAPI HBThreadProc(LPVOID param) {
    int idx = (int)(intptr_t)param;
    ClientSlot& slot = g_clients[idx];

    // 1. 创建连接（对齐老工具：HB线程负责创建 socket）
    slot.sock = CreateConnection(g_config.platformHost,
                                 slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
    if (slot.sock == INVALID_SOCKET) {
        // 失败 → 等一个 HB 周期后在循环里重试
        InterlockedExchange(&slot.connected, 0);
    } else {
        InterlockedExchange(&slot.connected, 1);
        s_reconnects++;

        // 门控内发送首个 HB（对齐老工具线程创建间隔500ms）
        // connectGateMs 由外层 Sleep 控制，这里直接发首 HB
        uint8_t hbBuf[4096];
        int32_t hbLen = 0;
        if (g_onBuildHBPayload) {
            hbLen = g_onBuildHBPayload(slot.clientId, slot.deviceId, hbBuf, sizeof(hbBuf));
        }
        if (hbLen > 0) {
            if (SendAll(slot.sock, hbBuf, hbLen)) {
                s_hbSendOk++;
            } else {
                s_hbSendFail++;
            }
        }
    }

    // 2. 主循环（对齐老工具 do { ... Sleep(30000) } while）
    while (!InterlockedCompareExchange(&g_stopHB, 0, 0)) {
        Sleep(g_config.hbIntervalMs);
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;

        // 如果断线，尝试重连
        if (slot.sock == INVALID_SOCKET || !InterlockedCompareExchange(&slot.connected, 1, 1)) {
            if (slot.sock != INVALID_SOCKET) {
                closesocket(slot.sock);
                slot.sock = INVALID_SOCKET;
            }
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

        // 构建 HB payload（通过回调让 C# 打包）
        uint8_t hbBuf[4096];
        int32_t hbLen = 0;
        if (g_onBuildHBPayload) {
            hbLen = g_onBuildHBPayload(slot.clientId, slot.deviceId, hbBuf, sizeof(hbBuf));
        }
        if (hbLen <= 0) continue;

        // 发送前重置回包标志
        InterlockedExchange(&slot.lastReplyOk, 0);

        // 发送 HB（对齐老工具 SendHeartbeatToserver_TCP）
        bool sendOk = SendAll(slot.sock, hbBuf, hbLen);
        if (!sendOk) {
            s_hbSendFail++;
            // 发送失败 → 重连后再试一次（对齐老工具）
            closesocket(slot.sock);
            slot.sock = CreateConnection(g_config.platformHost,
                                         slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
            if (slot.sock != INVALID_SOCKET) {
                s_reconnects++;
                InterlockedExchange(&slot.connected, 1);
                sendOk = SendAll(slot.sock, hbBuf, hbLen);
                if (sendOk) s_hbSendOk++;
                else { s_hbSendFail++; s_disconnects++; }
            } else {
                InterlockedExchange(&slot.connected, 0);
                s_disconnects++;
            }
            continue;
        }
        s_hbSendOk++;

        // 接收回包（对齐老工具 RecvHeartBeatBack_TCP）
        uint32_t cmdId = RecvHeartbeatReply(slot.sock);
        if (cmdId == 1) {
            // 正常回包
            s_hbRecvOk++;
            InterlockedExchange(&slot.lastReplyOk, 1);
        } else if (cmdId == 18) {
            // NOREGISTER — 关闭连接，通知 C# 重注册
            s_hbRecvNoReg++;
            closesocket(slot.sock);
            slot.sock = INVALID_SOCKET;
            InterlockedExchange(&slot.connected, 0);
            s_disconnects++;
            if (g_onNeedReregister) {
                g_onNeedReregister(slot.clientId);
            }
            // 重新创建连接（对齐老工具：CloseConnection → Register → CreateConnection）
            // 这里只做连接，注册由 C# 回调处理
            Sleep(1000);
            slot.sock = CreateConnection(g_config.platformHost,
                                         slot.tcpPort > 0 ? slot.tcpPort : g_config.platformPort);
            if (slot.sock != INVALID_SOCKET) {
                InterlockedExchange(&slot.connected, 1);
                s_reconnects++;
            }
        } else if (cmdId == 17) {
            // 策略变更 — 忽略（模拟器不需要处理策略）
            s_hbRecvOk++;
            InterlockedExchange(&slot.lastReplyOk, 1);
        } else {
            // 超时或错误 — 老工具 RecvHeartBeatBack_TCP 返回 0 时直接继续下一轮
            // (不断连，不重连，对齐老工具行为)
        }
    }

    // 退出：关闭连接
    if (slot.sock != INVALID_SOCKET) {
        closesocket(slot.sock);
        slot.sock = INVALID_SOCKET;
    }
    InterlockedExchange(&slot.connected, 0);
    return 0;
}

// ============================================================
//  日志发送线程（对齐 ThreadFunc_MsgLogSend）
// ============================================================

static DWORD WINAPI LogThreadProc(LPVOID param) {
    int idx = (int)(intptr_t)param;
    ClientSlot& slot = g_clients[idx];

    int msgCount = 0;
    int totalMsg = g_logCfg.totalMessages;

    while (!InterlockedCompareExchange(&g_stopLog, 0, 0)) {
        if (totalMsg > 0 && msgCount >= totalMsg) break;

        // 每轮间隔（对齐老工具 Sleep(SleepInterval)）
        if (msgCount > 0 && g_logCfg.intervalMs > 0) {
            Sleep(g_logCfg.intervalMs);
        }
        if (InterlockedCompareExchange(&g_stopLog, 0, 0)) break;

        // 检查 socket 是否可用
        if (slot.sock == INVALID_SOCKET || !InterlockedCompareExchange(&slot.connected, 1, 1)) {
            Sleep(1000);  // 没连接，等1s重试
            continue;
        }

        // 发送每种类型（对齐老工具 SendThreatLog_ToserverTCP 的 3 种类型）
        // 每客户端使用自己的 payload（含独立 IP/ClientId）
        bool anyFail = false;
        for (int t = 0; t < g_logCfg.typeCount; t++) {
            if (InterlockedCompareExchange(&g_stopLog, 0, 0)) break;
            if (slot.sock == INVALID_SOCKET) { anyFail = true; break; }

            const auto& payload = g_logCfg.payloads[idx * g_logCfg.typeCount + t];
            bool ok = SendAll(slot.sock, payload.data(), (int)payload.size());
            if (ok) {
                s_logSendOk++;
            } else {
                s_logSendFail++;
                anyFail = true;
                // 对齐 C++: 一种失败就 goto END (break)
                break;
            }

            // 类型间 Sleep（对齐老工具 Sleep(50)）
            if (t < g_logCfg.typeCount - 1 && g_logCfg.sleepBetweenTypesMs > 0) {
                Sleep(g_logCfg.sleepBetweenTypesMs);
            }
        }

        if (anyFail) {
            // 发送失败 — 标记 socket 无效，等 HB 线程重连
            // 对齐老工具：SendThreatLog 失败后回主循环 Sleep
            // 注意：这里不关闭 socket，因为 HB 线程管理连接生命周期
            // 但需要通知 HB 线程该连接有问题
            InterlockedExchange(&slot.connected, 0);
            s_disconnects++;
            if (slot.sock != INVALID_SOCKET) {
                closesocket(slot.sock);
                slot.sock = INVALID_SOCKET;
            }
            Sleep(g_logCfg.intervalMs > 0 ? g_logCfg.intervalMs : 1000);
        }

        msgCount++;
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

    for (int i = 0; i < g_clientCount; i++) {
        g_clients[i].hbThread = CreateThread(
            NULL, 0, HBThreadProc, (LPVOID)(intptr_t)i, 0, NULL);

        // 对齐老工具：线程间隔 500ms 创建
        if (g_config.connectGateMs > 0 && i < g_clientCount - 1) {
            Sleep(g_config.connectGateMs);
        }
    }
    return 0;
}

NE_API int32_t NE_StartLogSend(
    const uint8_t** payloadTemplates,
    const int32_t* payloadSizes,
    int32_t typeCount,
    int32_t logClientCount,
    int32_t intervalMs,
    int32_t totalMessages,
    int32_t sleepBetweenTypesMs)
{
    InterlockedExchange(&g_stopLog, 0);

    g_logCfg.payloads.clear();
    g_logCfg.typeCount = typeCount;
    g_logCfg.logClientCount = logClientCount;
    g_logCfg.intervalMs = intervalMs;
    g_logCfg.totalMessages = totalMessages;
    g_logCfg.sleepBetweenTypesMs = sleepBetweenTypesMs;

    // payloadTemplates 是 [logClientCount * typeCount] 的扁平数组
    // payloadTemplates[clientIdx * typeCount + typeIdx]
    int totalPayloads = logClientCount * typeCount;
    for (int p = 0; p < totalPayloads; p++) {
        g_logCfg.payloads.emplace_back(
            payloadTemplates[p],
            payloadTemplates[p] + payloadSizes[p]);
    }

    int count = (logClientCount > g_clientCount) ? g_clientCount : logClientCount;
    for (int i = 0; i < count; i++) {
        g_clients[i].logThread = CreateThread(
            NULL, 0, LogThreadProc, (LPVOID)(intptr_t)i, 0, NULL);
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
