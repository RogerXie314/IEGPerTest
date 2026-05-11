// ieg_protocal.cpp
// 从老工具 WLProtocal/Protocal.cpp 移植的协议打包实现
// 对外暴露 PackPT_HB（AES加密）和 PackPT_Log（zlib-only）两个函数
// 以及 IEG_HeartBeat_GetJson（对齐老工具 9-arg HeartBeat_GetJson）
//
// 依赖：wlModeEncrypt.c, Aescrypt.c, Aeskey.c, Aestab.c, aes_modes.c, sha1.c（均纯C）

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>

// zlib
#include "../../dependencies_export/include/zlib/zlib.h"

// AES / sha1（纯C，来自老工具 common/）
// sha1.h 不依赖 brg_types.h/stdint.h，可以安全 include
// AES 封装通过 ieg_aes_wrap.c（纯C）隔离，避免 common/stdint.h 与 MSVC <type_traits> 冲突
extern "C" {
#include "../../external/IEG_Code/code/common/sha1.h"
}
#include "ieg_aes_wrap.h"

// jsoncpp（已在CMakeLists引入）
#include "../../external/IEG_Code/code/include/WLUtilities/json/json.h"

#include "ieg_protocal.h"

// ============================================================
//  常量（来自老工具 Protocal.cpp）
// ============================================================

#define SHA1_LEN    20
#define KEY_LEN     32

static const char* g_strEncryptKey = "4&k7w4&P588%e5r684k$bj@JB$Jf8bik";
static const unsigned short nProtocalVer = 1;

// ============================================================
//  WL_PORTOCAL_HEAD（来自老工具 include/WLProtocal/Protocal.h）
// ============================================================

#pragma pack(1)
typedef struct {
    char          szFlag[2];       // "PT"
    unsigned char cProtoVer;
    unsigned char cSource;         // em_portocal_Windows_IEG = 3
    unsigned short nBodyLen;       // big-endian，body（压缩/加密后）长度
    unsigned char nEncryptType;    // 0=none, 1=AES
    unsigned char nCompressType;   // 0=none, 1=zlib
    unsigned char nFillLen;        // AES padding字节数
    unsigned char nReserve;
    unsigned short nRandomKey;     // big-endian
    unsigned int   nSerialNumber;
    unsigned int   CheckSum;       // big-endian CRC32
    unsigned int   nSessionID;     // SessionID（预留TCP用，置0）
    long long      nTime;          // big-endian unix时间戳
    unsigned int   nCmdID;         // big-endian cmdId
    unsigned int   nDeviceID;      // big-endian deviceId
    unsigned short nSrcLen;        // big-endian 原始JSON长度
    char           reserved2[6];
} WL_PORTOCAL_HEAD_IEG;
#pragma pack()

// ============================================================
//  字节序工具（来自老工具 Protocal.cpp）
// ============================================================

static __int64 ieg_hton64(__int64 val)
{
    // 判断是否大端
    union { unsigned int i; unsigned char s[4]; } c;
    c.i = 0x12345678;
    if ((unsigned char)0x12 == c.s[0]) return val;  // 大端，不转换

    long high = (long)((val >> 32) & 0xFFFFFFFF);
    long low  = (long)(val & 0xFFFFFFFF);
    low  = htonl(low);
    high = htonl(high);
    __int64 ret = ((__int64)low << 32) | (unsigned int)high;
    return ret;
}

// ============================================================
//  AES工具函数（来自老工具 CProtocal::GetRandomKey / GetEncryptKey）
// ============================================================

static unsigned short IEG_GetRandomKey()
{
    return (unsigned short)(rand() % 0xFFFF);
}

static void IEG_GetEncryptKey(unsigned short nRandomKey, char* pEncryptKey)
{
    sha1_context sha1Ctx;
    std::ostringstream strTemp;
    unsigned char hashCode[SHA1_LEN] = {0};

    strTemp << g_strEncryptKey << nRandomKey;
    std::string s = strTemp.str();

    sha1_starts(&sha1Ctx);
    sha1_update(&sha1Ctx, (uint8*)s.c_str(), (uint32)s.length());
    sha1_finish(&sha1Ctx, hashCode);

    memcpy(pEncryptKey, hashCode, SHA1_LEN);
    // KEY_LEN=32，sha1只有20字节，剩余清零（与老工具 ZeroMemory 后 memcpy 一致）
    memset(pEncryptKey + SHA1_LEN, 0, KEY_LEN - SHA1_LEN);
}

// ============================================================
//  FillPortocalHead（来自老工具 CProtocal::FillPortocalHead）
// ============================================================

static void IEG_FillPortocalHead(WL_PORTOCAL_HEAD_IEG& head,
                                 unsigned short nBodyLen,
                                 unsigned char encryptType,
                                 unsigned char compressType,
                                 unsigned char nFillLen,
                                 unsigned short nRandomKey,
                                 unsigned long crc,
                                 unsigned short nSrcLen,
                                 unsigned int nCmd,
                                 unsigned int deviceId)
{
    memset(&head, 0, sizeof(head));
    head.szFlag[0] = 'P';
    head.szFlag[1] = 'T';
    head.cProtoVer       = nProtocalVer;
    head.cSource         = 3;   // em_portocal_Windows_IEG
    head.nBodyLen        = htons(nBodyLen);
    head.nEncryptType    = encryptType;
    head.nCompressType   = compressType;
    head.nFillLen        = nFillLen;
    head.nReserve        = 0;
    head.nRandomKey      = htons(nRandomKey);
    head.nSerialNumber   = 0;
    head.CheckSum        = htonl(crc);
    head.nTime           = ieg_hton64((__int64)time(NULL));
    head.nSrcLen         = htons(nSrcLen);
    head.nCmdID          = htonl(nCmd);
    head.nDeviceID       = htonl(deviceId);
}

// ============================================================
//  PackPT_HB：zlib压缩 + AES加密（对齐老工具 SendData 路径）
// ============================================================

int PackPT_HB(const char* json, int jsonLen,
              uint32_t cmdId, uint32_t deviceId,
              uint8_t* outBuf, int outBufCapacity)
{
    if (!json || jsonLen <= 0) return -1;

    // 1. CRC32
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)json, (uInt)jsonLen);

    // 2. zlib 压缩
    uLong compBound = compressBound((uLong)jsonLen);
    std::string compBuf(compBound, '\0');
    uLong compLen = compBound;
    if (compress((Bytef*)&compBuf[0], &compLen, (const Bytef*)json, (uLong)jsonLen) != Z_OK)
        return -1;

    // 3. AES 加密（对齐老工具 Encrypt：padding到16倍数）
    unsigned short nRandomKey = IEG_GetRandomKey();
    char encKey[KEY_LEN] = {0};
    IEG_GetEncryptKey(nRandomKey, encKey);

    unsigned char nFillLen = (unsigned char)(16 - (compLen % 16));
    uLong totalLen = compLen + nFillLen;

    std::string paddingBuf(totalLen, '\0');
    memcpy(&paddingBuf[0], compBuf.data(), compLen);
    // padding bytes已是0（与老工具 memset 0 后 memcpy 一致）

    ieg_aes_ecb_encrypt(encKey, KEY_LEN, &paddingBuf[0], (unsigned int)totalLen);

    // 4. 组装协议头
    if ((int)(sizeof(WL_PORTOCAL_HEAD_IEG) + totalLen) > outBufCapacity) return -1;

    WL_PORTOCAL_HEAD_IEG head;
    IEG_FillPortocalHead(head,
                         (unsigned short)totalLen,
                         1,  // AES
                         1,  // zlib
                         nFillLen,
                         nRandomKey,
                         crc,
                         (unsigned short)jsonLen,
                         cmdId,
                         deviceId);

    memcpy(outBuf, &head, sizeof(head));
    memcpy(outBuf + sizeof(head), paddingBuf.data(), totalLen);

    return (int)(sizeof(head) + totalLen);
}

// ============================================================
//  PackPT_Log：zlib压缩 + 不加密（对齐老工具 SendData_OnlyCompress 路径）
// ============================================================

int PackPT_Log(const char* json, int jsonLen,
               uint32_t cmdId, uint32_t deviceId,
               uint8_t* outBuf, int outBufCapacity)
{
    if (!json || jsonLen <= 0) return -1;

    // 1. CRC32
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)json, (uInt)jsonLen);

    // 2. zlib 压缩
    uLong compBound = compressBound((uLong)jsonLen);
    if ((int)(sizeof(WL_PORTOCAL_HEAD_IEG) + compBound) > outBufCapacity) return -1;

    uint8_t* compBuf = outBuf + sizeof(WL_PORTOCAL_HEAD_IEG);
    uLong compLen = compBound;
    if (compress(compBuf, &compLen, (const Bytef*)json, (uLong)jsonLen) != Z_OK)
        return -1;

    // 3. 协议头（无加密，nFillLen=0，nRandomKey=0）
    WL_PORTOCAL_HEAD_IEG head;
    IEG_FillPortocalHead(head,
                         (unsigned short)compLen,
                         0,  // none
                         1,  // zlib
                         0,  // nFillLen
                         0,  // nRandomKey
                         crc,
                         (unsigned short)jsonLen,
                         cmdId,
                         deviceId);

    memcpy(outBuf, &head, sizeof(head));

    return (int)(sizeof(head) + compLen);
}

// ============================================================
//  UnicodeToUTF8（与 ieg_simulate_json.cpp 中相同，局部实现）
// ============================================================

static std::string ieg_wstr_to_utf8(const wchar_t* ws)
{
    if (!ws) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], len, NULL, NULL);
    return s;
}

// ============================================================
//  IEG_HeartBeat_GetJson（对齐老工具 HeartBeat_GetJson 9-arg）
// ============================================================

std::string IEG_HeartBeat_GetJson(const wchar_t* computerID,
                                  const wchar_t* computerName,
                                  const wchar_t* computerIP,
                                  int cpu, int mem)
{
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    CMDContent["a"] = 0;
    CMDContent.clear();

    person["ComputerID"] = ieg_wstr_to_utf8(computerID);
    person["CMDTYPE"]    = 200;   // CMDTYPE_CMD
    person["CMDID"]      = 1;     // DATA_TO_SERVER_HEARTBEAT
    person["Domain"]     = "test.com";

    CMDContent["dwCPU"]          = cpu;
    CMDContent["dwMem"]          = mem;
    CMDContent["WindowsVersion"] = "Windows 7";
    CMDContent["ComputerName"]   = ieg_wstr_to_utf8(computerName);
    CMDContent["ComputerIP"]     = ieg_wstr_to_utf8(computerIP);

    person["CMDContent"] = CMDContent;
    person["clientLanguage"] = "zh";   // 对齐老工具 dwLanguage==0 → "zh"

    root.append(person);
    return writer.write(root);  // FastWriter末尾含\n，调用方传 size()-1
}
