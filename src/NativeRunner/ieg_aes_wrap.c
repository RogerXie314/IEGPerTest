// ieg_aes_wrap.c
// 纯C文件，隔离 common/stdint.h 与 MSVC C++ <type_traits> 的冲突
// 只在此文件中 include Aes/wlModeEncrypt 头文件

/* 防止 common/stdint.h 被解析后与 MSVC 标准库冲突 */
#include "../../external/IEG_Code/code/common/wlModeEncrypt.h"

#include "ieg_aes_wrap.h"

void ieg_aes_ecb_encrypt(const char* key, unsigned int keyLen, char* data, unsigned int len)
{
    WL_MODE_CIPHER_CTX ctx;
    wlModeCipherCtxInit_Len(&ctx, key, keyLen);
    wlEcbEncryptData(&ctx, data, data, len);
}
