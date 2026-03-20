// NativeSender.cpp — Winsock 同步发送层实现
// 100% 对齐 C++ 老工具 SendInfoToServer 的网络行为。

#define NATIVESENDER_EXPORTS
#include "NativeSender.h"

#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

// ── 内部辅助 ─────────────────────────────────────────────────────────

// 精确接收 expectLen 字节（对齐老工具 RecvData）
static int recv_exact(SOCKET s, char* buf, int expectLen, int timeoutMs)
{
    if (timeoutMs > 0)
    {
        DWORD tv = (DWORD)timeoutMs;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    }

    int total = 0;
    while (total < expectLen)
    {
        int n = recv(s, buf + total, expectLen - total, 0);
        if (n <= 0) return total; // 断开或错误
        total += n;
    }

    // 恢复无限等待
    if (timeoutMs > 0)
    {
        DWORD tv = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    }

    return total;
}

// 1KB 分块发送（对齐老工具 while(nProtocalLen - nSendCount > 0) { send(1024) }）
static int send_chunked(SOCKET s, const char* data, int dataLen)
{
    int sent = 0;
    while (sent < dataLen)
    {
        int chunk = dataLen - sent;
        if (chunk > 1024) chunk = 1024;

        int n = send(s, data + sent, chunk, 0);
        if (n == SOCKET_ERROR) return -1;
        sent += n;
    }
    return 0;
}

// ── 公开 API 实现 ────────────────────────────────────────────────────

NS_API int NS_Init(void)
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}

NS_API void NS_Cleanup(void)
{
    WSACleanup();
}

NS_API unsigned __int64 NS_CreateConnection(const char* host, int port)
{
    // 100% 对齐老工具 CSendInfoToServer::CreateConnection：
    // socket(AF_INET, SOCK_STREAM, 0) → 非阻塞 connect → select(2s) → 切回阻塞

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return (unsigned __int64)INVALID_SOCKET;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    // inet_pton for IPv4
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        closesocket(s);
        return (unsigned __int64)INVALID_SOCKET;
    }

    // 切非阻塞
    unsigned long ul = 1;
    ioctlsocket(s, FIONBIO, &ul);

    int nRes = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (nRes == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
    {
        fd_set writeSet, exceptSet;
        struct timeval tv;
        tv.tv_sec = 2;  // 2 秒超时，对齐老工具
        tv.tv_usec = 0;

        FD_ZERO(&writeSet);
        FD_SET(s, &writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(s, &exceptSet);

        nRes = select(0, NULL, &writeSet, &exceptSet, &tv);
        if (nRes <= 0 || FD_ISSET(s, &exceptSet) || !FD_ISSET(s, &writeSet))
        {
            closesocket(s);
            return (unsigned __int64)INVALID_SOCKET;
        }
    }
    else if (nRes == SOCKET_ERROR)
    {
        closesocket(s);
        return (unsigned __int64)INVALID_SOCKET;
    }

    // 切回阻塞模式（对齐老工具）
    ul = 0;
    ioctlsocket(s, FIONBIO, &ul);

    // 注意：老工具不设 TCP_NODELAY 也不设 SO_KEEPALIVE
    // 这里100%对齐，不加任何额外 socket option

    return (unsigned __int64)s;
}

NS_API void NS_CloseConnection(unsigned __int64 sock)
{
    if (sock != (unsigned __int64)INVALID_SOCKET)
    {
        closesocket((SOCKET)sock);
    }
}

NS_API int NS_SendData(unsigned __int64 sock, const unsigned char* data, int dataLen)
{
    if (sock == (unsigned __int64)INVALID_SOCKET || data == NULL || dataLen <= 0)
        return -1;
    return send_chunked((SOCKET)sock, (const char*)data, dataLen);
}

NS_API int NS_RecvData(unsigned __int64 sock, unsigned char* buf, int expectLen, int timeoutMs)
{
    if (sock == (unsigned __int64)INVALID_SOCKET || buf == NULL || expectLen <= 0)
        return 0;
    return recv_exact((SOCKET)sock, (char*)buf, expectLen, timeoutMs);
}

NS_API unsigned int NS_SendHeartbeatAndRecv(
    unsigned __int64 sock,
    const unsigned char* hbPacket, int hbPacketLen,
    int recvTimeoutMs)
{
    if (sock == (unsigned __int64)INVALID_SOCKET) return 0;

    // 1. 发送心跳包
    if (send_chunked((SOCKET)sock, (const char*)hbPacket, hbPacketLen) != 0)
        return 0;

    // 2. 接收 PT 头（48 字节）
    // 对齐老工具 RecvHeartbeat：读 header → IsValidHeader → GetProtacalBodyLen → GetProtacalCmd → 读 body
    unsigned char header[48];
    int n = recv_exact((SOCKET)sock, (char*)header, 48, recvTimeoutMs);
    if (n < 48) return 0;

    // 验证 PT 标志
    if (header[0] != 'P' || header[1] != 'T') return 0;

    // 提取 nBodyLen（offset 4, uint16 BE）
    unsigned short bodyLen = ((unsigned short)header[4] << 8) | header[5];

    // 提取 nCmdID（offset 32, uint32 BE）
    unsigned int cmdId = ((unsigned int)header[32] << 24)
                       | ((unsigned int)header[33] << 16)
                       | ((unsigned int)header[34] << 8)
                       | header[35];

    // 3. 读取 body（丢弃，对齐老工具 RecvData(sockRecv, saBufBody, nBodyLen)）
    if (bodyLen > 0 && bodyLen < 65535)
    {
        // 用栈上/小堆分配读取并丢弃
        char discard[4096];
        int remaining = (int)bodyLen;
        while (remaining > 0)
        {
            int chunk = remaining > (int)sizeof(discard) ? (int)sizeof(discard) : remaining;
            int r = recv_exact((SOCKET)sock, (char*)discard, chunk, recvTimeoutMs);
            if (r < chunk) return 0;
            remaining -= chunk;
        }
    }

    return cmdId;
}

NS_API int NS_SendThreatBatch(
    unsigned __int64 sock,
    const unsigned char* pkt1, int pkt1Len,
    const unsigned char* pkt2, int pkt2Len,
    const unsigned char* pkt3, int pkt3Len)
{
    if (sock == (unsigned __int64)INVALID_SOCKET) return -1;

    // File (EventType=30)
    if (send_chunked((SOCKET)sock, (const char*)pkt1, pkt1Len) != 0)
        return -1;

    Sleep(50); // 对齐老工具 Sleep(50)

    // ProcStart (EventType=60)
    if (send_chunked((SOCKET)sock, (const char*)pkt2, pkt2Len) != 0)
        return -1;

    Sleep(50); // 对齐老工具 Sleep(50)

    // Reg (EventType=40)
    if (send_chunked((SOCKET)sock, (const char*)pkt3, pkt3Len) != 0)
        return -1;

    return 0;
}
