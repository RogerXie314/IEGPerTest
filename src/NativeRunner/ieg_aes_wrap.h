// ieg_aes_wrap.h
// AES加密薄封装，供 ieg_protocal.cpp（C++）调用
// 实现在 ieg_aes_wrap.c（纯C），隔离 common/stdint.h 冲突
#ifndef IEG_AES_WRAP_H
#define IEG_AES_WRAP_H

#ifdef __cplusplus
extern "C" {
#endif

// AES ECB 加密（等同老工具 wlModeCipherCtxInit_Len + wlEcbEncryptData）
// key: KEY_LEN(32) 字节密钥，data: 原地加密缓冲区，len: 必须为16倍数
void ieg_aes_ecb_encrypt(const char* key, unsigned int keyLen, char* data, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif /* IEG_AES_WRAP_H */
