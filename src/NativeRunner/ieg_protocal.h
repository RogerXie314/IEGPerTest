#pragma once
#include <stdint.h>

// AES加密协议打包（对齐老工具 SendData -> CProtocal::GetPortocal 默认AES+zlib路径）
// 返回打包后总字节数，失败返回 -1
// outBuf 必须足够大（建议 jsonLen * 2 + 64）
int PackPT_HB(const char* json, int jsonLen,
              uint32_t cmdId, uint32_t deviceId,
              uint8_t* outBuf, int outBufCapacity);

// zlib-only协议打包（对齐老工具 SendData_OnlyCompress -> CProtocal::GetPortocal zlib+none路径）
int PackPT_Log(const char* json, int jsonLen,
               uint32_t cmdId, uint32_t deviceId,
               uint8_t* outBuf, int outBufCapacity);

// 生成心跳JSON（对齐老工具 HeartBeat_GetJson 9-arg版本）
// 返回 std::string，末尾含FastWriter的'\n'，调用方传 size()-1 给 PackPT_HB
#include <string>
std::string IEG_HeartBeat_GetJson(const wchar_t* computerID,
                                  const wchar_t* computerName,
                                  const wchar_t* computerIP,
                                  int cpu, int mem);
