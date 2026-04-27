// NativeRunner — 独立 C++ 可执行文件，用于心跳 + 日志发送
//
// 核心逻辑直接复用 NativeEngine.cpp 中已验证的算法：
//   - 每客户端独立 deviceId（修正 WLServerTest.exe 的全局静态 m_nUniqueID 竞态问题）
//   - PackPT：zlib 压缩 + 48 字节 PT 头（无 MFC 依赖）
//   - 1 线程/客户端，socket 生命周期由 HB 线程独立管理
//
// 使用方式：NativeRunner.exe --config <config.ini path>
// 输出协议（stdout）：
//   INFO|...        关键里程碑日志
//   ERROR|...       错误
//   STATS|...       实时统计，每 2 秒一次
//   WARN|...        非致命警告
// 停止：向 stdin 发送 "quit\n"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "zlib.h"

#pragma comment(lib, "ws2_32.lib")

// ============================================================
//  配置结构
// ============================================================

struct RunnerConfig {
    // [Server]
    std::string platformHost;
    int32_t     platformPort;    // HBPort：心跳/日志 TCP 端口（如 4575）

    // [Heartbeat]
    int32_t     hbIntervalMs;    // 心跳间隔 ms（默认 30000）
    int32_t     connectGateMs;   // 建连错峰间隔 ms（默认 500，对齐老工具）
    int32_t     hbTotalMinutes;  // 运行时长（0=无限）

    // [Client]
    std::string clientsLogPath;  // Clients.log 完整路径

    // [Log]
    int32_t     logClientCount;         // 发日志客户端数（0=不发日志）
    int32_t     logEachClientTotalItems;// 每客户端发送总条数（0=无限）
    int32_t     logEachClientPerSecond; // 每秒条数（用于计算 intervalMs）
    int32_t     logSelectedTypes;       // 日志类型数量（0..3）
    int32_t     logHitEvery;            // 每 N 轮第 1 轮发 hit（默认 71）
    int32_t     logSleepBetweenTypesMs; // 类型间 Sleep ms（默认 50）
};

// ============================================================
//  内部数据结构（对齐 NativeEngine.cpp）
// ============================================================

struct ClientSlot {
    char     clientId[64];
    char     computerIdTemplate[64];
    int32_t  deviceId;
    char     ip[32];
    int32_t  tcpPort;
    HANDLE   hbThread;
    HANDLE   logThread;
    volatile long connected;
    volatile long lastReplyOk;
};

struct HBGroupArg {
    int startIdx;
    int count;
};

struct LogSendConfig {
    int32_t hitEvery;
    int32_t typeCount;
    int32_t logClientCount;
    int32_t intervalMs;
    int32_t totalMessages;
    int32_t sleepBetweenTypesMs;
};

// ============================================================
//  全局状态
// ============================================================

static RunnerConfig              g_cfg;
static std::vector<ClientSlot>   g_clients;
static int32_t                   g_clientCount = 0;

// 对齐老工具：独立全局 socket 数组（供日志线程共享读）
static SOCKET           g_sock[2000]    = {};
static volatile long    g_nSocketCount  = 0;

static volatile long    g_stopHB  = 0;
static volatile long    g_stopLog = 0;

static LogSendConfig    g_logCfg  = {};

// 统计
static std::atomic<int32_t> s_hbSendOk{0};
static std::atomic<int32_t> s_hbSendFail{0};
static std::atomic<int32_t> s_hbRecvOk{0};
static std::atomic<int32_t> s_hbRecvNoReg{0};
static std::atomic<int32_t> s_disconnects{0};
static std::atomic<int32_t> s_reconnects{0};
static std::atomic<int64_t> s_logSendOk{0};
static std::atomic<int64_t> s_logSendFail{0};

// ============================================================
//  stdout 互斥锁（防止多线程并发输出时字节级交错）
// ============================================================

static CRITICAL_SECTION g_csStdout;

// 所有 stdout 输出必须通过此函数，保证原子性
static void PrintLine(const std::string& line)
{
    EnterCriticalSection(&g_csStdout);
    std::cout << line << '\n';
    std::cout.flush();
    LeaveCriticalSection(&g_csStdout);
}

// ============================================================
//  INI 解析（对齐 ConsoleMode.cpp）
// ============================================================

static std::string GetINI(const std::string& content,
                          const std::string& section,
                          const std::string& key,
                          const std::string& def)
{
    std::istringstream iss(content);
    std::string line;
    bool inSection = false;
    std::string hdr = "[" + section + "]";

    while (std::getline(iss, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        size_t e = line.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        line = line.substr(s, e - s + 1);

        if (line == hdr) { inSection = true; continue; }
        if (!line.empty() && line[0] == '[') { inSection = false; continue; }
        if (!inSection) continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string k = line.substr(0, pos);
        std::string v = line.substr(pos + 1);
        k.erase(k.find_last_not_of(" \t") + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        if (k == key) return v;
    }
    return def;
}

static bool LoadConfig(const char* path, RunnerConfig& out)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "ERROR|Cannot open config: " << path << std::endl;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string c = ss.str();

    out.platformHost    = GetINI(c, "Server",    "IP",               "127.0.0.1");
    out.platformPort    = std::stoi(GetINI(c, "Server", "HBPort",    "4575"));
    out.hbIntervalMs    = std::stoi(GetINI(c, "Heartbeat", "Interval", "30000"));
    out.connectGateMs   = std::stoi(GetINI(c, "Heartbeat", "ConnectGateMs", "500"));
    out.hbTotalMinutes  = std::stoi(GetINI(c, "Heartbeat", "TotalMinutes",  "0"));
    out.clientsLogPath  = GetINI(c, "Client",    "ClientsLogPath",   "");

    out.logClientCount         = std::stoi(GetINI(c, "Log", "ClientCount",              "0"));
    out.logEachClientTotalItems= std::stoi(GetINI(c, "Log", "EachClientTotalItems",     "0"));
    out.logEachClientPerSecond = std::stoi(GetINI(c, "Log", "EachClientPerSecondItems", "0"));
    out.logSelectedTypes       = std::stoi(GetINI(c, "Log", "SelectedTypes",            "0"));
    out.logHitEvery            = std::stoi(GetINI(c, "Log", "HitEvery",                 "71"));
    out.logSleepBetweenTypesMs = std::stoi(GetINI(c, "Log", "SleepBetweenTypesMs",      "50"));
    return true;
}

// ============================================================
//  Clients.log 解析（JSONL 格式，对齐 ConsoleMode.cpp）
// ============================================================

static bool LoadClientsLog(const std::string& path, std::vector<ClientSlot>& clients)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "WARN|Cannot open Clients.log: " << path << std::endl;
        return false;
    }

    auto extractStr = [](const std::string& line, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = line.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = line.find('"', pos);
        return (end == std::string::npos) ? "" : line.substr(pos, end - pos);
    };

    auto extractUInt = [](const std::string& line, const std::string& key) -> uint32_t {
        std::string search = "\"" + key + "\":";
        size_t pos = line.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.size();
        size_t end = line.find_first_of(",}", pos);
        if (end == std::string::npos) return 0;
        try { return (uint32_t)std::stoul(line.substr(pos, end - pos)); }
        catch (...) { return 0; }
    };

    std::string line;
    int lineNum = 0;
    while (std::getline(f, line)) {
        ++lineNum;
        if (line.empty()) continue;

        std::string clientId = extractStr(line, "ClientId");
        std::string ip       = extractStr(line, "IP");
        uint32_t    deviceId = extractUInt(line, "DeviceId");
        int32_t     tcpPort  = (int32_t)extractUInt(line, "TcpPort");

        if (clientId.empty() || ip.empty()) {
            PrintLine("WARN|Line " + std::to_string(lineNum) + ": missing ClientId or IP, skipped");
            continue;
        }

        ClientSlot slot = {};
        strncpy_s(slot.clientId,            clientId.c_str(), _TRUNCATE);
        strncpy_s(slot.computerIdTemplate,  clientId.c_str(), _TRUNCATE);
        slot.deviceId   = (int32_t)deviceId;
        strncpy_s(slot.ip, ip.c_str(), _TRUNCATE);
        slot.tcpPort    = tcpPort;
        slot.hbThread   = NULL;
        slot.logThread  = NULL;
        slot.connected  = 0;
        slot.lastReplyOk= 0;
        clients.push_back(slot);

        if (clients.size() <= 5) {
            std::ostringstream oss;
            oss << "INFO|Client[" << clients.size() << "]: "
                << "ClientId=" << clientId
                << " IP=" << ip
                << " DeviceId=" << deviceId
                << " TcpPort=" << tcpPort;
            PrintLine(oss.str());
        }
    }
    return !clients.empty();
}

// ============================================================
//  底层 Socket 操作（直接翻译自 NativeEngine.cpp）
// ============================================================

static SOCKET CreateConnection(const char* host, int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    unsigned long ul = 1;
    ioctlsocket(s, FIONBIO, &ul);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    int nRes = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (nRes == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            fd_set writeSet, exceptSet;
            FD_ZERO(&writeSet);  FD_SET(s, &writeSet);
            FD_ZERO(&exceptSet); FD_SET(s, &exceptSet);
            timeval tm = {2, 0};
            nRes = select(0, NULL, &writeSet, &exceptSet, &tm);
            if (nRes <= 0 || FD_ISSET(s, &exceptSet) || !FD_ISSET(s, &writeSet)) {
                closesocket(s); return INVALID_SOCKET;
            }
        } else {
            closesocket(s); return INVALID_SOCKET;
        }
    }
    ioctlsocket(s, FIONBIO, &ul);
    return s;
}

static bool SendAll(SOCKET s, const uint8_t* data, int len)
{
    int sent = 0;
    while (sent < len) {
        int chunk = (len - sent > 1024) ? 1024 : (len - sent);
        int n = send(s, (const char*)(data + sent), chunk, 0);
        if (n == SOCKET_ERROR) return false;
        sent += n;
    }
    return true;
}

static bool RecvAll(SOCKET s, uint8_t* buf, int len)
{
    int recved = 0, retries = 0;
    while (recved < len) {
        int n = recv(s, (char*)(buf + recved), len - recved, 0);
        if (n == SOCKET_ERROR || n == 0) {
            if (++retries > 100) return false;
            Sleep(20);
        } else {
            recved += n;
            retries = 0;
        }
    }
    return true;
}

// 返回 cmdId：1=正常, 17=策略, 18=未注册, 0=失败
static uint32_t RecvHeartbeatReply(SOCKET s)
{
    uint8_t header[48];
    if (!RecvAll(s, header, 48)) return 0;

    uint16_t bodyLen = ((uint16_t)header[4] << 8) | header[5];
    uint32_t cmdId   = ((uint32_t)header[32] << 24)
                     | ((uint32_t)header[33] << 16)
                     | ((uint32_t)header[34] <<  8)
                     |  (uint32_t)header[35];

    if (bodyLen > 0 && bodyLen < 1024 * 1024) {
        std::vector<uint8_t> body(bodyLen);
        RecvAll(s, body.data(), bodyLen);
    }
    return cmdId;
}

// ============================================================
//  PT 协议打包（对齐 NativeEngine.cpp PackPT）
// ============================================================

static inline uint16_t be16(uint16_t v) { return htons(v); }
static inline uint32_t be32(uint32_t v) { return htonl(v); }
static uint64_t be64(uint64_t v) {
    return ((uint64_t)htonl((uint32_t)(v >> 32))) |
           ((uint64_t)htonl((uint32_t)(v & 0xFFFFFFFF)) << 32);
}

static int PackPT(const char* json, int jsonLen,
                  uint32_t cmdId, uint32_t deviceId,
                  uint8_t* outBuf, int outBufCapacity)
{
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)json, (uInt)jsonLen);

    uLong compBound = compressBound((uLong)jsonLen);
    if ((int)(48 + compBound) > outBufCapacity) return -1;

    uint8_t* compBuf = outBuf + 48;
    uLong compLen = compBound;
    if (compress(compBuf, &compLen, (const Bytef*)json, (uLong)jsonLen) != Z_OK) return -1;

    uint8_t* h = outBuf;
    memset(h, 0, 48);
    h[0] = 'P'; h[1] = 'T';
    h[2] = 1;   // cProtoVer
    h[3] = 3;   // cSource = em_portocal_Windows_IEG

    uint16_t bodyLenBE = be16((uint16_t)compLen);
    memcpy(h + 4, &bodyLenBE, 2);

    h[6] = 0;   // nEncryptType  = none
    h[7] = 1;   // nCompressType = zlib

    uint32_t crcBE = be32((uint32_t)crc);
    memcpy(h + 16, &crcBE, 4);

    uint64_t timeBE = be64((uint64_t)time(NULL));
    memcpy(h + 24, &timeBE, 8);

    uint32_t cmdIdBE = be32(cmdId);
    memcpy(h + 32, &cmdIdBE, 4);

    uint32_t devIdBE = be32(deviceId);
    memcpy(h + 36, &devIdBE, 4);

    uint16_t srcLenBE = be16((uint16_t)jsonLen);
    memcpy(h + 40, &srcLenBE, 2);

    return (int)(48 + compLen);
}

// ============================================================
//  心跳 — 单客户端发送+接收（对齐 NativeEngine.cpp HBDoSendRecv）
// ============================================================

static void HBDoSendRecv(int idx, ClientSlot& slot, SOCKET& localSock)
{
    // 不在发送开始时清零 lastReplyOk，避免 STATS 采样到发送中间状态导致在线数假性波动
    // lastReplyOk 只在真正失败时清零，在收到成功回包时置1

    char hbJson[768];
    int hbJsonLen = snprintf(hbJson, sizeof(hbJson),
        "[{\"ComputerID\":\"%s\",\"CMDTYPE\":100,\"CMDID\":1"
        ",\"Domain\":\"test.com\""
        ",\"CMDContent\":{"
            "\"dwCPU\":%d,\"dwMem\":%d"
            ",\"WindowsVersion\":\"Windows 10\""
            ",\"ComputerName\":\"%s\""
            ",\"ComputerIP\":\"%s\""
        "}"
        ",\"clientLanguage\":\"zh\"}]",
        slot.clientId,
        rand() % 100, rand() % 100,
        slot.clientId,
        slot.ip[0] ? slot.ip : "10.0.0.1");
    if (hbJsonLen <= 0 || hbJsonLen >= (int)sizeof(hbJson)) return;

    uint8_t hbBuf[4096];
    int32_t hbLen = PackPT(hbJson, hbJsonLen, 1, (uint32_t)slot.deviceId, hbBuf, sizeof(hbBuf));
    if (hbLen <= 0) return;

    bool sendOk = SendAll(localSock, hbBuf, hbLen);
    if (!sendOk) {
        s_hbSendFail++;
        closesocket(localSock);
        localSock = CreateConnection(g_cfg.platformHost.c_str(),
                                     slot.tcpPort > 0 ? slot.tcpPort : g_cfg.platformPort);
        if (localSock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            s_reconnects++;
            if (SendAll(localSock, hbBuf, hbLen)) {
                s_hbSendOk++;
            } else {
                s_hbSendFail++;
                closesocket(localSock);
                localSock = INVALID_SOCKET;
                InterlockedExchange(&slot.connected, 0);
                s_disconnects++;
                return;
            }
        } else {
            InterlockedExchange(&slot.connected, 0);
            s_disconnects++;
            return;
        }
    } else {
        s_hbSendOk++;
    }

    uint32_t cmdId = RecvHeartbeatReply(localSock);
    if (cmdId == 1 || cmdId == 17) {
        s_hbRecvOk++;
        InterlockedExchange(&slot.lastReplyOk, 1);
        if (cmdId == 17) {
            // 策略下发通知：通知 C# 发起 HTTPS 策略拉取（对齐 NativeEngine DLL 的 g_onPolicyNotify 回调）
            PrintLine("POLICY|" + std::to_string(idx) + "|" + std::string(slot.clientId));
        }
    } else if (cmdId == 18) {
        s_hbRecvNoReg++;
        InterlockedExchange(&slot.lastReplyOk, 0);  // NOREGISTER：标记为离线
        closesocket(localSock);
        localSock = INVALID_SOCKET;
        InterlockedExchange(&slot.connected, 0);
        s_disconnects++;
        // NOREGISTER：通知 C# 重注册（对齐 NativeEngine DLL 的 g_onNeedReregister 回调）
        // C# 重注册完成后通过 stdin 发回 DEVICEID|idx|newDeviceId，更新内存 DeviceId 后下轮生效
        PrintLine("REREGISTER|" + std::to_string(idx) + "|" + std::string(slot.clientId));
        // 重连等待 C# 重注册（最多等 15s，之后用旧 DeviceId 重试，避免永久阻塞）
        for (int w = 0; w < 150; w++) {
            Sleep(100);
            if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
            // 简单标志：C# 会通过 stdin DEVICEID 更新 deviceId，我们检测到变化即可
            // 由于 deviceId 写在 StdinThreadProc，这里只等固定时间
        }
        localSock = CreateConnection(g_cfg.platformHost.c_str(),
                                     slot.tcpPort > 0 ? slot.tcpPort : g_cfg.platformPort);
        if (localSock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            s_reconnects++;
            if (SendAll(localSock, hbBuf, hbLen)) s_hbSendOk++;
            else {
                s_hbSendFail++;
                closesocket(localSock);
                localSock = INVALID_SOCKET;
                InterlockedExchange(&slot.connected, 0);
                s_disconnects++;
            }
        }
    } else {
        // RecvHeartbeatReply 失败（cmdId==0）：关闭 socket，下轮重连
        // 同时清零 lastReplyOk，避免长期显示已离线客户端仍"在线"
        InterlockedExchange(&slot.lastReplyOk, 0);
        closesocket(localSock);
        localSock = INVALID_SOCKET;
        InterlockedExchange(&slot.connected, 0);
        s_disconnects++;
    }
}

// ============================================================
//  HB 线程（每线程 1 个客户端，对齐 NativeEngine.cpp HBThreadProc）
// ============================================================

static DWORD WINAPI HBThreadProc(LPVOID param)
{
    HBGroupArg* args = (HBGroupArg*)param;
    int idx = args->startIdx;
    delete args;

    ClientSlot& slot = g_clients[idx];
    SOCKET localSock = INVALID_SOCKET;

    // 1. 初始建连
    if (!InterlockedCompareExchange(&g_stopHB, 0, 0)) {
        localSock = CreateConnection(g_cfg.platformHost.c_str(),
                                     slot.tcpPort > 0 ? slot.tcpPort : g_cfg.platformPort);
        if (localSock != INVALID_SOCKET) {
            InterlockedExchange(&slot.connected, 1);
            // 写入全局 socket 数组（日志线程会读取）
            int si = InterlockedIncrement(&g_nSocketCount) - 1;
            if (si < 2000) g_sock[si] = localSock;
            HBDoSendRecv(idx, slot, localSock);
        }
    }

    // 2. 心跳主循环
    while (!InterlockedCompareExchange(&g_stopHB, 0, 0)) {
        Sleep(g_cfg.hbIntervalMs);
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;

        // 断线重连
        if (localSock == INVALID_SOCKET) {
            localSock = CreateConnection(g_cfg.platformHost.c_str(),
                                         slot.tcpPort > 0 ? slot.tcpPort : g_cfg.platformPort);
            if (localSock == INVALID_SOCKET) {
                InterlockedExchange(&slot.connected, 0);
                s_disconnects++;
                continue;
            }
            InterlockedExchange(&slot.connected, 1);
            s_reconnects++;
        }

        HBDoSendRecv(idx, slot, localSock);
    }

    if (localSock != INVALID_SOCKET) closesocket(localSock);
    InterlockedExchange(&slot.connected, 0);
    return 0;
}

// ============================================================
//  日志 JSON 构建（对齐 NativeEngine.cpp BuildThreatJson）
// ============================================================

static void FormatTimeGuid(char* buf, int size)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, size, "%04d-%d-%d %d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
}

static const uint32_t THREAT_CMDID = 21;

static int BuildThreatJson(int typeIdx, bool isHit,
                           const char* computerID,
                           char* buf, int bufSize)
{
    char timeGuid[32];
    FormatTimeGuid(timeGuid, sizeof(timeGuid));
    unsigned int pid = (unsigned int)(rand() % 60000 + 1000);
    long long ts = (long long)time(NULL);

    int written = 0;
    if (typeIdx == 0) {
        const char* procName = isHit
            ? "C:\\\\Windows\\\\System32\\\\cmd.exe"
            : "C:\\\\Windows\\\\System32\\\\a.exe";
        written = snprintf(buf, bufSize,
            "[{\"ComputerID\":\"%s\",\"CMDTYPE\":200,\"CMDID\":21,\"CMDContent\":{"
            "\"EventType\":60,"
            "\"Process.TimeStamp\":%lld,"
            "\"Process.ProcessId\":%u,"
            "\"Process.ProcessGuid\":\"%s\","
            "\"Process.ProcessName\":\"%s\","
            "\"Process.CommandLine\":\"\\\"%s\\\" /c whoami\","
            "\"Process.ParentProcessName\":\"C:\\\\Windows\\\\System32\\\\WindowsPowerShell\\\\v1.0\\\\powershell.exe\","
            "\"threat\":%s}}]",
            computerID, ts, pid, timeGuid, procName, procName,
            isHit ? "true" : "false");
    } else if (typeIdx == 1) {
        const char* procExe = isHit
            ? "C:\\\\Windows\\\\System32\\\\cmd.exe"
            : "C:\\\\Windows\\\\System32\\\\a.exe";
        written = snprintf(buf, bufSize,
            "[{\"ComputerID\":\"%s\",\"CMDTYPE\":200,\"CMDID\":21,\"CMDContent\":{"
            "\"EventType\":40,"
            "\"Registry.TimeStamp\":%lld,"
            "\"Registry.ProcessId\":%u,"
            "\"Registry.ProcessGuid\":\"%s\","
            "\"Registry.ProcessName\":\"%s\","
            "\"Registry.RegistryKey\":\"HKLM\\\\SOFTWARE\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\\\\TVqQAAMAAAAEAAAA\","
            "\"threat\":%s}}]",
            computerID, ts, pid, timeGuid, procExe,
            isHit ? "true" : "false");
    } else {
        const char* filePath = isHit
            ? "\\\\device\\\\harddiskvolume3\\\\windows\\\\system32\\\\mimilsa.log"
            : "\\\\device\\\\harddiskvolume3\\\\windows\\\\system32\\\\a.log";
        const char* procExe = isHit ? "D:\\\\dns.exe" : "D:\\\\a.exe";
        written = snprintf(buf, bufSize,
            "[{\"ComputerID\":\"%s\",\"CMDTYPE\":200,\"CMDID\":21,\"CMDContent\":{"
            "\"EventType\":30,"
            "\"FileAccess.TimeStamp\":%lld,"
            "\"FileAccess.ProcessId\":%u,"
            "\"FileAccess.ProcessGuid\":\"%s\","
            "\"FileAccess.ProcessName\":\"%s\","
            "\"FileAccess.FilePath\":\"%s\","
            "\"threat\":%s}}]",
            computerID, ts, pid, timeGuid, procExe, filePath,
            isHit ? "true" : "false");
    }
    return (written > 0 && written < bufSize) ? written : -1;
}

// ============================================================
//  日志线程（对齐 NativeEngine.cpp LogThreadProc）
// ============================================================

static DWORD WINAPI LogThreadProc(LPVOID param)
{
    int idx = (int)(intptr_t)param;
    ClientSlot& slot = g_clients[idx];

    // 等待 HB 线程建连 + 首次认证成功（对齐 NativeEngine.cpp）
    SOCKET sock = INVALID_SOCKET;
    while (!InterlockedCompareExchange(&g_stopLog, 0, 0)) {
        if (idx < g_nSocketCount) sock = g_sock[idx];
        if (sock != INVALID_SOCKET && InterlockedCompareExchange(&slot.lastReplyOk, 1, 1)) break;
        Sleep(100);
    }
    if (InterlockedCompareExchange(&g_stopLog, 0, 0)) return 0;

    char localComputerID[64];
    strncpy_s(localComputerID, sizeof(localComputerID),
              slot.computerIdTemplate[0] ? slot.computerIdTemplate : slot.clientId,
              _TRUNCATE);
    uint32_t localDeviceId = (uint32_t)slot.deviceId;

    int msgCount   = 0;
    int totalMsg   = g_logCfg.totalMessages;
    int hitEvery   = (g_logCfg.hitEvery > 1) ? g_logCfg.hitEvery : 1;

    char    jsonBuf[4096];
    std::vector<uint8_t> ptBuf(8192);

    while (!InterlockedCompareExchange(&g_stopLog, 0, 0)) {
        if (totalMsg > 0 && msgCount >= totalMsg) break;

        bool isHit = (msgCount % hitEvery == 0);

        for (int t = 0; t < g_logCfg.typeCount; t++) {
            if (InterlockedCompareExchange(&g_stopLog, 0, 0)) break;

            int jsonLen = BuildThreatJson(t, isHit, localComputerID, jsonBuf, sizeof(jsonBuf));
            if (jsonLen <= 0) { s_logSendFail++; break; }

            int ptLen = PackPT(jsonBuf, jsonLen, THREAT_CMDID,
                               localDeviceId, ptBuf.data(), (int)ptBuf.size());
            if (ptLen <= 0) { s_logSendFail++; break; }

            if (SendAll(sock, ptBuf.data(), ptLen)) {
                s_logSendOk++;
            } else {
                s_logSendFail++;
                // 对齐老工具：send 失败后直接 break，不关闭 socket，不重连
                // socket 生命周期 100% 由 HB 线程管理，Log 线程只负责发送
                break;
            }

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
//  STATS 输出（每 2 秒，格式与 ConsoleMode.cpp 完全一致）
// ============================================================

static void OutputStats()
{
    int connected = 0, replied = 0;
    for (int i = 0; i < g_clientCount; i++) {
        if (g_clients[i].connected)  connected++;
        if (g_clients[i].lastReplyOk) replied++;
    }
    int notConnected = g_clientCount - connected;

    // 计算 LogNotSending（还没完成的日志线程数）
    int logCount = (g_logCfg.logClientCount > g_clientCount)
                   ? g_clientCount : g_logCfg.logClientCount;
    int logActive = 0;
    for (int i = 0; i < logCount; i++) {
        if (g_clients[i].logThread) {
            DWORD ec;
            if (GetExitCodeThread(g_clients[i].logThread, &ec) && ec == STILL_ACTIVE)
                logActive++;
        }
    }

    std::ostringstream statsOss;
    statsOss << "STATS"
             << "|RegisteredTotal=" << g_clientCount
             << "|HBSending="       << connected
             << "|HBNotSending="    << notConnected
             << "|HBServerAck="     << replied
             << "|LogNotSending="   << (logCount - logActive)
             << "|LogSendOk="       << s_logSendOk.load()
             << "|LogSendFail="     << s_logSendFail.load()
             << "|NoReg="           << s_hbRecvNoReg.load();
    PrintLine(statsOss.str());
}

static DWORD WINAPI StatsThreadProc(LPVOID param)
{
    int totalSec = (int)(intptr_t)param;  // 0 = 无限
    int iterations = (totalSec > 0) ? (totalSec / 2) : INT_MAX;
    for (int i = 0; i < iterations; i++) {
        Sleep(2000);
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
        OutputStats();
    }
    return 0;
}

// ============================================================
//  stdin 读取线程（监听 "quit" 命令）
// ============================================================

static DWORD WINAPI StdinThreadProc(LPVOID /*param*/)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "q") {
            InterlockedExchange(&g_stopHB, 1);
            InterlockedExchange(&g_stopLog, 1);
            break;
        // DEVICEID|idx|newDeviceId — C# 重注册完成后发回的新 DeviceId
        } else if (line.size() > 9 && line.compare(0, 9, "DEVICEID|") == 0) {
            size_t p1 = line.find('|', 9);
            if (p1 != std::string::npos) {
                try {
                    int     cidx   = std::stoi(line.substr(9, p1 - 9));
                    int32_t devId  = (int32_t)std::stoul(line.substr(p1 + 1));
                    if (cidx >= 0 && cidx < g_clientCount) {
                        g_clients[cidx].deviceId = devId;
                        PrintLine("INFO|DeviceId updated: client[" + std::to_string(cidx) + "]=" + std::to_string(devId));
                    }
                } catch (...) {}
            }
        // STOPLOG — C# 点击"结束任务"时发送，停止日志线程
        } else if (line == "STOPLOG") {
            InterlockedExchange(&g_stopLog, 1);
            PrintLine("INFO|Log threads stopping");
        // STARTLOG|clientCount|eps|totalItems|types|hitEvery — C# 点击"添加任务"时发送
        } else if (line.size() > 9 && line.compare(0, 9, "STARTLOG|") == 0) {
            // 检查是否已有日志线程在运行（防止重复启动）
            bool logRunning = false;
            for (int i = 0; i < g_clientCount; i++) {
                if (g_clients[i].logThread) {
                    DWORD ec;
                    if (GetExitCodeThread(g_clients[i].logThread, &ec) && ec == STILL_ACTIVE) {
                        logRunning = true; break;
                    }
                }
            }
            if (logRunning) {
                PrintLine("WARN|Log threads already running, STARTLOG ignored");
            } else {
                try {
                    std::string params = line.substr(9);
                    size_t pos = 0, next;
                    auto np = [&](int32_t& out) {
                        next = params.find('|', pos);
                        std::string tok = (next == std::string::npos)
                            ? params.substr(pos) : params.substr(pos, next - pos);
                        pos = (next == std::string::npos) ? params.size() : next + 1;
                        out = std::stoi(tok);
                    };
                    int32_t lClientCount=0, lEps=0, lTotal=0, lTypes=0, lHit=71;
                    np(lClientCount); np(lEps); np(lTotal); np(lTypes); np(lHit);
                    if (lClientCount <= 0 || lEps <= 0 || lTypes <= 0) {
                        PrintLine("WARN|STARTLOG invalid params, log not started");
                    } else {
                        InterlockedExchange(&g_stopLog, 0);
                        // 重置统计计数器，确保每次任务统计从0开始
                        s_logSendOk.store(0);
                        s_logSendFail.store(0);
                        g_logCfg.hitEvery            = (lHit > 1) ? lHit : 71;
                        g_logCfg.typeCount           = lTypes;
                        g_logCfg.logClientCount      = lClientCount;
                        g_logCfg.intervalMs          = 1000 / lEps;
                        g_logCfg.totalMessages       = lTotal;
                        g_logCfg.sleepBetweenTypesMs = 50;  // 对齐老工具：类型间隔50ms
                        int logCount = (lClientCount > g_clientCount) ? g_clientCount : lClientCount;
                        for (int i = 0; i < logCount; i++) {
                            // 清理旧句柄（线程已退出）
                            if (g_clients[i].logThread) {
                                CloseHandle(g_clients[i].logThread);
                                g_clients[i].logThread = NULL;
                            }
                            HANDLE h = CreateThread(NULL, 0, LogThreadProc, (LPVOID)(intptr_t)i, 0, NULL);
                            if (h) SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
                            g_clients[i].logThread = h;
                        }
                        PrintLine("INFO|Log threads started (" + std::to_string(logCount) + " clients)");
                    }
                } catch (...) {
                    PrintLine("ERROR|STARTLOG parse failed");
                }
            }
        }
    }
    // stdin 关闭（父进程终止）也触发停止
    InterlockedExchange(&g_stopHB, 1);
    InterlockedExchange(&g_stopLog, 1);
    return 0;
}

// ============================================================
//  main
// ============================================================

int main(int argc, char* argv[])
{
    // 关闭 stdout 缓冲，确保每行立即被 C# 收到
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    InitializeCriticalSection(&g_csStdout);

    // 解析 --config <path>（兼容 --console 参数）
    const char* configPath = "config.ini";
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            configPath = argv[i + 1];
            break;
        }
    }

    PrintLine("INFO|NativeRunner starting, config=" + std::string(configPath));

    if (!LoadConfig(configPath, g_cfg)) return 1;

    // WSA 初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "ERROR|WSAStartup failed" << std::endl;
        return 1;
    }

    // 初始化全局 socket 数组
    for (int i = 0; i < 2000; i++) g_sock[i] = INVALID_SOCKET;
    g_nSocketCount = 0;
    InterlockedExchange(&g_stopHB, 0);
    InterlockedExchange(&g_stopLog, 0);

    // 加载 Clients.log
    if (g_cfg.clientsLogPath.empty()) {
        std::cerr << "ERROR|ClientsLogPath not set in config" << std::endl;
        WSACleanup();
        return 1;
    }

    if (!LoadClientsLog(g_cfg.clientsLogPath, g_clients) || g_clients.empty()) {
        std::cerr << "ERROR|No clients loaded from Clients.log: " << g_cfg.clientsLogPath << std::endl;
        WSACleanup();
        return 1;
    }

    g_clientCount = (int32_t)g_clients.size();
    PrintLine("INFO|Loaded " + std::to_string(g_clientCount) + " clients from Clients.log");

    // 启动诊断摘要（始终输出，便于 UI 侧确认配置是否正确）
    {
        int32_t firstDev  = g_clients.empty() ? 0 : g_clients[0].deviceId;
        int32_t firstPort = g_clients.empty() ? 0 : g_clients[0].tcpPort;
        std::ostringstream startOss;
        startOss << "WARN|startup config: server=" << g_cfg.platformHost
                 << ":" << g_cfg.platformPort
                 << " interval=" << g_cfg.hbIntervalMs
                 << "ms clients=" << g_clientCount
                 << " client[0]=" << (g_clients.empty() ? "" : g_clients[0].clientId)
                 << " devId[0]=" << firstDev
                 << " port[0]=" << firstPort;
        PrintLine(startOss.str());
    }

    // 启动 stdin 监听线程
    HANDLE hStdin = CreateThread(NULL, 0, StdinThreadProc, NULL, 0, NULL);

    // 启动 STATS 输出线程
    int totalSec = g_cfg.hbTotalMinutes * 60;
    HANDLE hStats = CreateThread(NULL, 0, StatsThreadProc,
                                 (LPVOID)(intptr_t)totalSec, 0, NULL);

    // 启动心跳线程（每 connectGateMs 创建一个，对齐老工具建连错峰）
    PrintLine("INFO|Starting heartbeat threads (" + std::to_string(g_clientCount)
              + " clients, interval=" + std::to_string(g_cfg.hbIntervalMs) + "ms)...");

    for (int i = 0; i < g_clientCount; i++) {
        if (InterlockedCompareExchange(&g_stopHB, 0, 0)) break;
        HBGroupArg* args = new HBGroupArg{i, 1};
        HANDLE h = CreateThread(NULL, 0, HBThreadProc, (LPVOID)args, 0, NULL);
        g_clients[i].hbThread = h;
        if (g_cfg.connectGateMs > 0) Sleep(g_cfg.connectGateMs);
    }

    PrintLine("INFO|Heartbeat threads started");

    // 启动日志线程（如果配置了）
    if (g_cfg.logClientCount > 0 && g_cfg.logSelectedTypes > 0
        && g_cfg.logEachClientPerSecond > 0)
    {
        g_logCfg.hitEvery            = (g_cfg.logHitEvery > 1) ? g_cfg.logHitEvery : 71;
        g_logCfg.typeCount           = g_cfg.logSelectedTypes;
        g_logCfg.logClientCount      = g_cfg.logClientCount;
        g_logCfg.intervalMs          = 1000 / g_cfg.logEachClientPerSecond;
        g_logCfg.totalMessages       = g_cfg.logEachClientTotalItems;
        g_logCfg.sleepBetweenTypesMs = g_cfg.logSleepBetweenTypesMs;

        int logCount = (g_cfg.logClientCount > g_clientCount)
                       ? g_clientCount : g_cfg.logClientCount;
        for (int i = 0; i < logCount; i++) {
            HANDLE h = CreateThread(NULL, 0, LogThreadProc, (LPVOID)(intptr_t)i, 0, NULL);
            if (h) SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
            g_clients[i].logThread = h;
        }
        PrintLine("INFO|Log threads started (" + std::to_string(logCount) + " clients)");
    }

    PrintLine("INFO|Console mode is running");

    // 等待停止信号（定时或 stdin quit）
    if (totalSec > 0) {
        // 超时自动停止
        Sleep(totalSec * 1000);
        InterlockedExchange(&g_stopHB, 1);
        InterlockedExchange(&g_stopLog, 1);
    } else {
        // 无限等待 stdin quit
        WaitForSingleObject(hStdin, INFINITE);
    }

    PrintLine("INFO|Stopping all threads...");
    InterlockedExchange(&g_stopHB, 1);
    InterlockedExchange(&g_stopLog, 1);

    // 等待所有线程退出（最多 60s）
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

    // 输出最终统计
    OutputStats();

    if (hStats) { WaitForSingleObject(hStats, 3000); CloseHandle(hStats); }
    if (hStdin) CloseHandle(hStdin);

    WSACleanup();
    PrintLine("INFO|NativeRunner stopped");
    DeleteCriticalSection(&g_csStdout);
    return 0;
}
