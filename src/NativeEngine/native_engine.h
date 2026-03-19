#pragma once

#ifdef NATIVEENGINE_EXPORTS
#define NE_API __declspec(dllexport)
#else
#define NE_API __declspec(dllimport)
#endif

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ---------- 客户端信息（C# 传入）----------
typedef struct {
    char     clientId[64];        // 如 "Client1-1"
    int32_t  deviceId;
    char     ip[32];              // 如 "10.254.197.1"
    int32_t  tcpPort;             // 心跳/日志 TCP端口 (默认=platformPort)
} NE_ClientInfo;

// ---------- 引擎配置 ----------
typedef struct {
    char     platformHost[256];   // 平台 IP
    int32_t  platformPort;        // 平台 TCP 端口 (如 8441)
    int32_t  hbIntervalMs;        // 心跳间隔 (ms), 默认 30000
    int32_t  connectGateMs;       // 连接串行化间隔 (ms), 默认 500
} NE_Config;

// ---------- 运行时统计 ----------
typedef struct {
    int32_t  hbConnected;         // 当前已连接数
    int32_t  hbTotal;             // 总客户端数
    int32_t  hbSendOk;            // HB 发送成功累计
    int32_t  hbSendFail;          // HB 发送失败累计
    int32_t  hbRecvOk;            // 收到平台正常回包(cmdId=1)
    int32_t  hbRecvNoReg;         // 收到未注册回包(cmdId=18)
    int32_t  hbReplied;           // 当前有回包的客户端数（最近一次HB收到回包）
    int32_t  disconnects;         // 断线次数
    int32_t  reconnects;          // 重连次数
    int64_t  logSendOk;           // Log 发送成功累计
    int64_t  logSendFail;         // Log 发送失败累计
} NE_Stats;

// ---------- 回调：通知 C# 某客户端需要重注册 ----------
typedef void (__stdcall *NE_NeedReregisterCallback)(const char* clientId);

// ---------- 回调：通知 C# 构建心跳 payload ----------
// C# 填充 outBuf，返回写入的字节数；返回 0 表示失败
typedef int32_t (__stdcall *NE_BuildHBPayloadCallback)(
    const char* clientId, int32_t deviceId,
    uint8_t* outBuf, int32_t outBufSize);

// ---------- API ----------

// 初始化引擎。成功返回0。
NE_API int32_t NE_Init(
    const NE_Config* config,
    const NE_ClientInfo* clients,
    int32_t clientCount,
    NE_NeedReregisterCallback onNeedReregister,
    NE_BuildHBPayloadCallback onBuildHBPayload);

// 启动心跳线程（每客户端1个OS线程）。
NE_API int32_t NE_StartHeartbeat();

// 启动日志发送线程。
// payloadTemplates: 预打包好的 payload 数组（每种日志类型一个）
// payloadSizes: 每个 payload 的大小
// typeCount: 日志类型数量
// logClientCount: 发日志的客户端数（从第0个开始）
// intervalMs: 每轮日志间隔（ms）
// totalMessages: 每客户端发送总条数（0=无限）
// sleepBetweenTypesMs: 类型间 Sleep (老工具=50)
NE_API int32_t NE_StartLogSend(
    const uint8_t** payloadTemplates,
    const int32_t* payloadSizes,
    int32_t typeCount,
    int32_t logClientCount,
    int32_t intervalMs,
    int32_t totalMessages,
    int32_t sleepBetweenTypesMs);

// 停止所有线程（阻塞等待全部退出）。
NE_API void NE_StopAll();

// 仅停止日志发送。
NE_API void NE_StopLogSend();

// 获取实时统计。
NE_API void NE_GetStats(NE_Stats* out);

// 检查日志发送线程是否还在运行。返回1=运行中，0=已结束。
NE_API int32_t NE_IsLogSendRunning();

// 释放资源。
NE_API void NE_Shutdown();

#ifdef __cplusplus
}
#endif
