#pragma once
// ClientConfigUploader.h
// 客户端配置上传 - 固定目录配置模板加载模块
// 读取 exe 目录下 clientConfig.json；缺失或解析失败时回退内置默认模板。

#include <string>

class CClientConfigUploader
{
public:
    // 读取 exe 目录 clientConfig.json 并返回 CMDContent 级 JSON 字符串（不含 ComputerID/CMDTYPE/CMDID）。
    // 成功返回 TRUE；文件缺失/解析失败时回退内置默认模板并返回 FALSE（strErrMsg 说明原因）。
    static BOOL LoadClientConfigTemplate(std::string& strCmdContentJson, CString& strErrMsg);

    // 获取内置默认模板（CMDContent 级 JSON 字符串）。
    static const char* GetDefaultTemplate();
};