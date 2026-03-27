#pragma once

// RawPacketEngine.dll — 基于 Npcap 的原始报文发送引擎
// 对齐 xb-ether-tester 的发包循环、FieldRule 和校验和更新逻辑。
// C# 端通过 P/Invoke 调用，CallingConvention = Cdecl。

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── RPE_Rule 结构体（对齐 xb-ether-tester t_rule）────────────────────
#pragma pack(push, 1)
typedef struct {
    uint8_t  valid;
    uint32_t flags;
    uint16_t offset;
    uint8_t  width;
    int8_t   bits_from;
    int8_t   bits_len;
    char     rsv[4];
    uint8_t  base_value[8];
    uint8_t  max_value[8];
    uint32_t step_size;
} RPE_Rule;
#pragma pack(pop)

// ── 校验和 flags（对齐 xb-ether-tester t_stream.flags）──────────────
#define RPE_CKSUM_IP    0x01
#define RPE_CKSUM_ICMP  0x02
#define RPE_CKSUM_IGMP  0x04
#define RPE_CKSUM_UDP   0x08
#define RPE_CKSUM_TCP   0x10

// ── 速率类型 ─────────────────────────────────────────────────────────
#define RPE_SPEED_PPS      0   // 包率模式：speedValue = pps
#define RPE_SPEED_INTERVAL 1   // 间隔模式：speedValue = 微秒间隔
#define RPE_SPEED_FASTEST  2   // 最大速率：不做任何延时

// ── 发包模式 ─────────────────────────────────────────────────────────
#define RPE_MODE_CONTINUOUS 0  // 持续发包直到手动停止
#define RPE_MODE_BURST      1  // 定量发包，达到 sndCount 后自动停止

// ── 导出接口 ─────────────────────────────────────────────────────────

// 初始化 Npcap，枚举适配器。返回 0 成功 / -1 失败。
__declspec(dllexport) int32_t RPE_Init();

// 释放所有资源，停止发包线程。
__declspec(dllexport) void RPE_Cleanup();

// 返回枚举到的适配器数量。
__declspec(dllexport) int32_t RPE_GetAdapterCount();

// 获取指定适配器的 pcap 名称和 IPv4 地址字符串。
// 返回 0 成功 / -1 越界。
__declspec(dllexport) int32_t RPE_GetAdapterInfo(
    int32_t index,
    char*   name,    int32_t nameLen,
    char*   ipv4,    int32_t ipv4Len);

// 选择发包适配器。返回 0 成功 / -1 失败。
__declspec(dllexport) int32_t RPE_SelectAdapter(int32_t index);

// 添加一条 Stream（复制 data 字节数组和 rules）。
// 返回 stream_id（>=0）或 -1 失败。
__declspec(dllexport) int32_t RPE_AddStream(
    const uint8_t* data,
    int32_t        len,
    const char*    name,
    const RPE_Rule* rules,
    int32_t        ruleCount,
    uint32_t       checksumFlags);

// 清空所有 Stream（需先停止发包）。
__declspec(dllexport) void RPE_ClearStreams();

// 启用/禁用某条 Stream（运行中立即生效）。
__declspec(dllexport) void RPE_SetStreamEnabled(int32_t streamId, int32_t enabled);

// 设置速率配置。
// speedType: RPE_SPEED_PPS / RPE_SPEED_INTERVAL / RPE_SPEED_FASTEST
// speedValue: PPS 模式时为包率，Interval 模式时为微秒间隔
// sndMode:   RPE_MODE_CONTINUOUS / RPE_MODE_BURST
// sndCount:  Burst 模式时的总发包次数
__declspec(dllexport) void RPE_SetRateConfig(
    int32_t speedType,
    int64_t speedValue,
    int32_t sndMode,
    int64_t sndCount);

// 启动发包循环（DLL 内部线程）。返回 0 成功 / -1 失败。
__declspec(dllexport) int32_t RPE_Start();

// 停止发包循环，阻塞等待线程退出（最多 1000ms）。
__declspec(dllexport) void RPE_Stop();

// 获取当前统计数据（线程安全）。
__declspec(dllexport) void RPE_GetStats(
    uint64_t* sendTotal,
    uint64_t* sendBytes,
    uint64_t* sendFail);

#ifdef __cplusplus
}
#endif
