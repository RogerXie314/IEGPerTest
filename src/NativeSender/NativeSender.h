#pragma once

// NativeSender.dll — 精简的 Winsock 同步发送层
// 对齐 C++ 老工具 SendInfoToServer 的网络行为：
//   socket() → non-blocking connect(2s) → blocking mode → send(1KB chunks) → recv
// C# 端负责 PT 协议打包（PtProtocol.Pack），本 DLL 只做字节级 TCP 收发。

#ifdef NATIVESENDER_EXPORTS
#define NS_API __declspec(dllexport)
#else
#define NS_API __declspec(dllimport)
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── 连接管理 ──────────────────────────────────────────────────────────
// 返回 SOCKET（uint64），INVALID_SOCKET 表示失败。
// 对齐老工具 CreateConnection：非阻塞 connect + select(2s timeout) → 切回阻塞模式。
// 注意：老工具不设 TCP_NODELAY 也不设 SO_KEEPALIVE，这里 100% 对齐。
NS_API unsigned __int64 NS_CreateConnection(const char* host, int port);

// 关闭连接。
NS_API void NS_CloseConnection(unsigned __int64 sock);

// ── 发送 ──────────────────────────────────────────────────────────────
// 同步 send()，1KB 分块，对齐老工具 SendData/SendData_OnlyCompress 的 while 循环。
// data 是 C# 端已打好的 PT 协议包（48 字节头 + body）。
// 返回 0 成功，-1 失败。
NS_API int NS_SendData(unsigned __int64 sock, const unsigned char* data, int dataLen);

// ── 接收 ──────────────────────────────────────────────────────────────
// 同步 recv()，精确接收 expectLen 字节。用于读 PT 头和 body。
// timeoutMs > 0 时设 SO_RCVTIMEO；0 = 无限等待。
// 返回实际收到字节数，< expectLen 表示失败/断开。
NS_API int NS_RecvData(unsigned __int64 sock, unsigned char* buf, int expectLen, int timeoutMs);

// ── 心跳发送 + 收回包（一次完整交互）──────────────────────────────────
// 发送 C# 端打好的 HB PT 包，然后同步等待回包（对齐老工具 RecvHeartbeat 逻辑）。
// 返回平台下发的 cmdId（>0），0 表示失败/超时。
NS_API unsigned int NS_SendHeartbeatAndRecv(
    unsigned __int64 sock,
    const unsigned char* hbPacket, int hbPacketLen,
    int recvTimeoutMs);

// ── 威胁日志批量发送（3 种类型，对齐 SendThreatLog_ToserverTCP）────────
// packets[0..2] 是 C# 端打好的 3 个 PT 协议包。
// 每种之间 Sleep(50ms)，对齐老工具。任一种失败立即返回 -1。
// 返回 0 成功，-1 失败。
NS_API int NS_SendThreatBatch(
    unsigned __int64 sock,
    const unsigned char* pkt1, int pkt1Len,
    const unsigned char* pkt2, int pkt2Len,
    const unsigned char* pkt3, int pkt3Len);

// ── 初始化/清理 Winsock ──────────────────────────────────────────────
NS_API int NS_Init(void);
NS_API void NS_Cleanup(void);

#ifdef __cplusplus
}
#endif
