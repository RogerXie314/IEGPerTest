/*
 * Option B: 直接从老工具源文件提取 ThreatLog JSON 生成逻辑
 *
 * 来源：
 *   WLJsonParse.cpp  : ThreatLog_File_GetJson / ThreatLog_ProcStart_GetJson / ThreatLog_Reg_GetJson
 *   SimulateJson.cpp : ThreatLog_SimulateJson_File / ThreatLog_SimulateJson_ProcStart / ThreatLog_SimulateJson_Reg
 *   ThreatLogStruct.h: 结构体定义
 *
 * 修改说明：
 *   - 去掉类前缀（CWLJsonParse:: / WLSimulateJson::），改为独立函数
 *   - 内联必要的常量和辅助函数（原文不变）
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <ctime>
#include "ieg_simulate_json.h"

// JsonCpp amalgamated — 从老工具 WLUtilities/jsoncpp.cpp 编译
#include "../../external/IEG_Code/code/include/WLUtilities/json/json.h"

// ─────────────────────────────────────────────────────────────
// 常量（来自 ThreatLogStruct.h / public_def.h）
// ─────────────────────────────────────────────────────────────
#ifndef GUID_STRING_SIZE
#define GUID_STRING_SIZE  (40)
#endif
#ifndef MAX_TEXT_SIZE
#define MAX_TEXT_SIZE     (1024)
#endif
#ifndef MAX_REGPATH_SIZE
#define MAX_REGPATH_SIZE  (1024)
#endif
#ifndef MAX_DEVICE_PATH
#define MAX_DEVICE_PATH   (292)
#endif
#ifndef MAX_PATAMETERS_LENGTH
#define MAX_PATAMETERS_LENGTH (MAX_DEVICE_PATH * 2)
#endif

#define THREAT_EVENT_TYPE_FILE      (30)
#define THREAT_EVENT_TYPE_REG       (40)
#define THREAT_EVENT_TYPE_PROCSTART (60)
#define THREAT_EVENT_UPLOAD_CMDID   (21)
#define THREATLOG_TYPE_FILE_CREATE  (2)

// ─────────────────────────────────────────────────────────────
// 结构体（原文来自 ThreatLogStruct.h）
// ─────────────────────────────────────────────────────────────
typedef struct _THREAT_PROC_PROCINFO {
    ULONG ulPid;
    WCHAR ProcGuid[GUID_STRING_SIZE];
    WCHAR ProcFileName[MAX_PATH];
    WCHAR ProcPath[MAX_DEVICE_PATH];
    WCHAR ProcCMDLine[MAX_PATAMETERS_LENGTH];
} THREAT_PROC_PROCINFO, *PTHREAT_PROC_PROCINFO;

typedef struct _THREAT_PROC_USERINFO {
    WCHAR ProcUserName[MAX_PATH];
    WCHAR ProcUserSid[MAX_PATH];
    ULONG ulTerminalSessionId;
} THREAT_PROC_USERINFO, *PTHREAT_PROC_USERINFO;

typedef struct _THREAT_PROC_FILEINFO {
    WCHAR FileVersion[MAX_PATH];
    WCHAR Description[MAX_PATH];
    WCHAR Product[MAX_PATH];
    WCHAR Company[MAX_PATH];
    WCHAR OriginalFileName[MAX_PATH];
} THREAT_PROC_FILEINFO, *PTHREAT_PROC_FILEINFO;

typedef struct _THREAT_EVENT_PROC_START {
    UCHAR bStart;
    LONGLONG llTimeStamp;
    THREAT_PROC_PROCINFO  stProcInfo;
    THREAT_PROC_USERINFO  stProcUserInfo;
    THREAT_PROC_FILEINFO  stProcFileInfo;
    THREAT_PROC_PROCINFO  stParentProcInfo;
    THREAT_PROC_USERINFO  stParentProcUserInfo;
} THREAT_EVENT_PROC_START, *PTHREAT_EVENT_PROC_START;

typedef struct _THREAT_REG_INFO {
    WCHAR RegKeyPath[MAX_REGPATH_SIZE];
    WCHAR RegTargetKeyPath[MAX_REGPATH_SIZE];
    WCHAR RegValueName[MAX_REGPATH_SIZE];
    WCHAR RegValue[MAX_REGPATH_SIZE];
} THREAT_REG_INFO, *PTHREAT_REG_INFO;

typedef struct _THREAT_EVENT_REG {
    USHORT Operation;
    LONGLONG llTimeStamp;
    THREAT_PROC_PROCINFO  stProcInfo;
    THREAT_PROC_USERINFO  stProcUserInfo;
    THREAT_PROC_FILEINFO  stProcFileInfo;
    THREAT_REG_INFO       stRegInfo;
} THREAT_EVENT_REG, *PTHREAT_EVENT_REG;

typedef struct _THREAT_FILE_INFO {
    WCHAR FileName[MAX_DEVICE_PATH];
    WCHAR FileExtention[MAX_PATH];
    WCHAR FileFolder[MAX_DEVICE_PATH];
    WCHAR FilePath[MAX_DEVICE_PATH];
} THREAT_FILE_INFO, *PTHREAT_FILE_INFO;

typedef struct _THREAT_EVENT_FILE {
    USHORT Operation;
    LONGLONG llTimeStamp;
    THREAT_PROC_PROCINFO  stProcInfo;
    THREAT_PROC_USERINFO  stProcUserInfo;
    THREAT_FILE_INFO      stFileInfo;
    THREAT_FILE_INFO      stTargetFileInfo;
} THREAT_EVENT_FILE, *PTHREAT_EVENT_FILE;

// ─────────────────────────────────────────────────────────────
// 辅助函数（原文来自 WLJsonParse.cpp / SimulateJson.cpp）
// ─────────────────────────────────────────────────────────────

// 原文：CWLJsonParse::UnicodeToUTF8（WLJsonParse.cpp line 135）
static std::string UnicodeToUTF8(const std::wstring& str)
{
    int nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    CHAR* szUtf8 = new CHAR[nLen + 1];
    memset(szUtf8, 0, nLen + 1);
    nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, szUtf8, nLen + 1, NULL, NULL);
    std::string strUtf8 = szUtf8;
    delete[] szUtf8;
    return strUtf8;
}

// 原文：ReturnCurrentTimeWstr（SimulateJson.cpp line 136）
static std::wstring ReturnCurrentTimeWstr()
{
    time_t tt = time(NULL);
    tm* t = localtime(&tt);
    int year = t->tm_year + 1900;
    int mon  = t->tm_mon + 1;
    int day  = t->tm_mday;
    int hour = t->tm_hour;
    int min  = t->tm_min;
    int sec  = t->tm_sec;

    std::wostringstream tmp;
    tmp << year << L"-" << mon << L"-" << day << L" "
        << hour << L":" << min << L":" << sec;
    return tmp.str();
}

// ─────────────────────────────────────────────────────────────
// GetJson 函数（原文来自 WLJsonParse.cpp，去掉 CWLJsonParse:: 前缀）
// ─────────────────────────────────────────────────────────────

// 原文：CWLJsonParse::ThreatLog_ProcStart_GetJson（line 2326）
static std::string ThreatLog_ProcStart_GetJson(const std::wstring& ComputerID,
                                               PTHREAT_EVENT_PROC_START pProcStartEvent)
{
    std::string sJsonPacket = "";
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    if (NULL == pProcStartEvent) return sJsonPacket;

    CMDContent["a"] = 0;
    CMDContent.clear();

    person["ComputerID"] = UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = 200;
    person["CMDID"]   = THREAT_EVENT_UPLOAD_CMDID;
    CMDContent["EventType"]                  = THREAT_EVENT_TYPE_PROCSTART;
    CMDContent["Process.TimeStamp"]          = pProcStartEvent->llTimeStamp;
    CMDContent["Process.ProcessId"]          = (Json::Int64)pProcStartEvent->stProcInfo.ulPid;
    CMDContent["Process.ProcessGuid"]        = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcGuid);
    CMDContent["Process.ProcessFileName"]    = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcFileName);
    CMDContent["Process.ProcessName"]        = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcPath);
    CMDContent["Process.CommandLine"]        = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcCMDLine);
    CMDContent["Process.User"]               = UnicodeToUTF8(pProcStartEvent->stProcUserInfo.ProcUserName);
    CMDContent["Process.UserSid"]            = UnicodeToUTF8(pProcStartEvent->stProcUserInfo.ProcUserSid);
    CMDContent["Process.TerminalSessionId"]  = (Json::Int64)pProcStartEvent->stProcUserInfo.ulTerminalSessionId;
    CMDContent["Process.FileVersion"]        = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.FileVersion);
    CMDContent["Process.Description"]        = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Description);
    CMDContent["Process.Product"]            = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Product);
    CMDContent["Process.Company"]            = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Company);
    CMDContent["Process.OriginalFileName"]   = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.OriginalFileName);
    CMDContent["Process.ParentProcessId"]    = (Json::Int64)pProcStartEvent->stParentProcInfo.ulPid;
    CMDContent["Process.ParentProcessGuid"]  = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcGuid);
    CMDContent["Process.ParentProcessFileName"] = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcFileName);
    CMDContent["Process.ParentProcessName"]  = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcPath);
    CMDContent["Process.ParentCommandLine"]  = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcCMDLine);
    CMDContent["Process.ParentUser"]         = UnicodeToUTF8(pProcStartEvent->stParentProcUserInfo.ProcUserName);
    person["CMDContent"] = CMDContent;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

// 原文：CWLJsonParse::ThreatLog_Reg_GetJson（line 2376）
static std::string ThreatLog_Reg_GetJson(const std::wstring& ComputerID,
                                         PTHREAT_EVENT_REG pRegEvent)
{
    std::string sJsonPacket = "";
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    if (NULL == pRegEvent) return sJsonPacket;

    CMDContent["a"] = 0;
    CMDContent.clear();

    person["ComputerID"] = UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = 200;
    person["CMDID"]   = THREAT_EVENT_UPLOAD_CMDID;
    CMDContent["EventType"]                   = THREAT_EVENT_TYPE_REG;
    CMDContent["Registry.Operation"]          = pRegEvent->Operation;
    CMDContent["Registry.TimeStamp"]          = pRegEvent->llTimeStamp;
    CMDContent["Registry.ProcessId"]          = (Json::Int64)pRegEvent->stProcInfo.ulPid;
    CMDContent["Registry.ProcessGuid"]        = UnicodeToUTF8(pRegEvent->stProcInfo.ProcGuid);
    CMDContent["Registry.ProcessName"]        = UnicodeToUTF8(pRegEvent->stProcInfo.ProcPath);
    CMDContent["Registry.ProcessFileName"]    = UnicodeToUTF8(pRegEvent->stProcInfo.ProcFileName);
    CMDContent["Registry.CommandLine"]        = UnicodeToUTF8(pRegEvent->stProcInfo.ProcCMDLine);
    CMDContent["Registry.User"]               = UnicodeToUTF8(pRegEvent->stProcUserInfo.ProcUserName);
    CMDContent["Registry.UserSid"]            = UnicodeToUTF8(pRegEvent->stProcUserInfo.ProcUserSid);
    CMDContent["Registry.TerminalSessionId"]  = (Json::Int64)pRegEvent->stProcUserInfo.ulTerminalSessionId;
    CMDContent["Registry.KeyPath"]            = UnicodeToUTF8(pRegEvent->stRegInfo.RegKeyPath);
    CMDContent["Registry.TargetKeyPath"]      = UnicodeToUTF8(pRegEvent->stRegInfo.RegTargetKeyPath);
    CMDContent["Registry.ValueName"]          = UnicodeToUTF8(pRegEvent->stRegInfo.RegValueName);
    CMDContent["Registry.Value"]              = UnicodeToUTF8(pRegEvent->stRegInfo.RegValue);
    CMDContent["Registry.FileVersion"]        = UnicodeToUTF8(pRegEvent->stProcFileInfo.FileVersion);
    CMDContent["Registry.Description"]        = UnicodeToUTF8(pRegEvent->stProcFileInfo.Description);
    CMDContent["Registry.Product"]            = UnicodeToUTF8(pRegEvent->stProcFileInfo.Product);
    CMDContent["Registry.Company"]            = UnicodeToUTF8(pRegEvent->stProcFileInfo.Company);
    CMDContent["Registry.OriginalFileName"]   = UnicodeToUTF8(pRegEvent->stProcFileInfo.OriginalFileName);
    person["CMDContent"] = CMDContent;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

// 原文：CWLJsonParse::ThreatLog_File_GetJson（line 2427）
static std::string ThreatLog_File_GetJson(const std::wstring& ComputerID,
                                          PTHREAT_EVENT_FILE pFileEvent)
{
    std::string sJsonPacket = "";
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    if (NULL == pFileEvent) return sJsonPacket;

    CMDContent["a"] = 0;
    CMDContent.clear();

    person["ComputerID"] = UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = 200;
    person["CMDID"]   = THREAT_EVENT_UPLOAD_CMDID;
    CMDContent["EventType"]                      = THREAT_EVENT_TYPE_FILE;
    CMDContent["FileAccess.Operation"]           = pFileEvent->Operation;
    CMDContent["FileAccess.TimeStamp"]           = pFileEvent->llTimeStamp;
    CMDContent["FileAccess.ProcessId"]           = (Json::Int64)pFileEvent->stProcInfo.ulPid;
    CMDContent["FileAccess.ProcessGuid"]         = UnicodeToUTF8(pFileEvent->stProcInfo.ProcGuid);
    CMDContent["FileAccess.ProcessName"]         = UnicodeToUTF8(pFileEvent->stProcInfo.ProcPath);
    CMDContent["FileAccess.ProcessFileName"]     = UnicodeToUTF8(pFileEvent->stProcInfo.ProcFileName);
    CMDContent["FileAccess.CommandLine"]         = UnicodeToUTF8(pFileEvent->stProcInfo.ProcCMDLine);
    CMDContent["FileAccess.User"]                = UnicodeToUTF8(pFileEvent->stProcUserInfo.ProcUserName);
    CMDContent["FileAccess.UserSid"]             = UnicodeToUTF8(pFileEvent->stProcUserInfo.ProcUserSid);
    CMDContent["FileAccess.TerminalSessionId"]   = (Json::Int64)pFileEvent->stProcUserInfo.ulTerminalSessionId;
    CMDContent["FileAccess.FileName"]            = UnicodeToUTF8(pFileEvent->stFileInfo.FileName);
    CMDContent["FileAccess.FileExtention"]       = UnicodeToUTF8(pFileEvent->stFileInfo.FileExtention);
    CMDContent["FileAccess.FilePath"]            = UnicodeToUTF8(pFileEvent->stFileInfo.FileFolder);
    CMDContent["FileAccess.FullFileName"]        = UnicodeToUTF8(pFileEvent->stFileInfo.FilePath);
    CMDContent["FileAccess.TargetFileName"]      = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileName);
    CMDContent["FileAccess.TargetFileExtention"] = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileExtention);
    CMDContent["FileAccess.TargetFilePath"]      = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileFolder);
    CMDContent["FileAccess.TargetFullFileName"]  = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FilePath);
    person["CMDContent"] = CMDContent;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

// ─────────────────────────────────────────────────────────────
// SimulateJson 函数（原文来自 SimulateJson.cpp，去掉 WLSimulateJson:: 前缀）
// ─────────────────────────────────────────────────────────────

// 原文：WLSimulateJson::ThreatLog_SimulateJson_File（line 297）
std::string IEG_SimulateJson_File(const std::wstring& ComputerID, bool bHit)
{
    PTHREAT_EVENT_FILE pEvt = NULL;

    std::wstring wsRand = ReturnCurrentTimeWstr();

    std::wstring wsProcName, wsProcPath, wsProcCmdLine;
    if (bHit) {
        wsProcName    = L"dns.exe";
        wsProcPath    = L"D:\\dns.exe";
        wsProcCmdLine = L"D:\\dns.exe set";
    } else {
        wsProcName    = L"e.exe";
        wsProcPath    = L"D:\\e.exe";
        wsProcCmdLine = L"D:\\e.exe";
    }

    std::wstring wsUserName     = L"WIN-913056QNOGK\\DELL";
    std::wstring wsUserSid      = L"S-1-5-21-3782372158-3025124834-3246284786-1000";
    std::wstring wsFilePath     = L"\\device\\harddiskvolume3\\windows\\system32\\mimilsa.log";
    std::wstring wsFileFolder   = L"\\device\\harddiskvolume3\\windows\\system32";
    std::wstring wsFileName     = L"mimilsa.log";
    std::wstring wsFileExt      = L"log";

    std::string strJson = "";
    pEvt = (PTHREAT_EVENT_FILE)malloc(sizeof(THREAT_EVENT_FILE));
    if (NULL == pEvt) return strJson;
    memset(pEvt, 0, sizeof(THREAT_EVENT_FILE));

    pEvt->Operation  = THREATLOG_TYPE_FILE_CREATE;
    pEvt->llTimeStamp = (LONGLONG)rand();
    pEvt->stProcInfo.ulPid = rand();
    memcpy(pEvt->stProcInfo.ProcGuid,    wsRand.c_str(),      wsRand.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcFileName, wsProcName.c_str(),  wsProcName.length()  * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcPath,    wsProcPath.c_str(),   wsProcPath.length()  * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcCmdLine.c_str(),wsProcCmdLine.length()* sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserName, wsUserName.c_str(), wsUserName.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserSid,  wsUserSid.c_str(),  wsUserSid.length()  * sizeof(WCHAR));
    pEvt->stProcUserInfo.ulTerminalSessionId = rand() % 10;
    memcpy(pEvt->stFileInfo.FilePath,      wsFilePath.c_str(),   wsFilePath.length()   * sizeof(WCHAR));
    memcpy(pEvt->stFileInfo.FileFolder,    wsFileFolder.c_str(), wsFileFolder.length() * sizeof(WCHAR));
    memcpy(pEvt->stFileInfo.FileName,      wsFileName.c_str(),   wsFileName.length()   * sizeof(WCHAR));
    memcpy(pEvt->stFileInfo.FileExtention, wsFileExt.c_str(),    wsFileExt.length()    * sizeof(WCHAR));
    memcpy(pEvt->stTargetFileInfo.FilePath,      wsFilePath.c_str(),   wsFilePath.length()   * sizeof(WCHAR));
    memcpy(pEvt->stTargetFileInfo.FileFolder,    wsFileFolder.c_str(), wsFileFolder.length() * sizeof(WCHAR));
    memcpy(pEvt->stTargetFileInfo.FileName,      wsFileName.c_str(),   wsFileName.length()   * sizeof(WCHAR));
    memcpy(pEvt->stTargetFileInfo.FileExtention, wsFileExt.c_str(),    wsFileExt.length()    * sizeof(WCHAR));

    strJson = ThreatLog_File_GetJson(ComputerID, pEvt);
    free(pEvt);
    return strJson;
}

// 原文：WLSimulateJson::ThreatLog_SimulateJson_ProcStart（line 403）
std::string IEG_SimulateJson_ProcStart(const std::wstring& ComputerID, bool bHit)
{
    PTHREAT_EVENT_PROC_START pEvt = NULL;

    std::wstring wsRand             = ReturnCurrentTimeWstr();
    std::wstring wsParentGuidString = L"{89382C912EDF43A5BA884 DC13577427}";

    std::wstring wsFileName, wsProcName, wsProcPath, wsProcCmdLine, wsProcPara;
    std::wstring wsParentProcPath, wsParentProcCmdLine;
    if (bHit) {
        wsFileName        = L"cmd.exe";
        wsProcName        = L"C:\\Windows\\System32\\cmd.exe";
        wsProcPath        = L"C:\\Windows\\System32\\cmd.exe";
        wsProcCmdLine     = L"\"cmd.exe\" /c \"reg add \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\" /v set dir NoSetTaskbar /t REG_DWORD /d 1 /f\"";
        wsProcPara        = L"cmd.exe /c dir test set";
        wsParentProcPath     = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
        wsParentProcCmdLine  = L"\"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\" -exec bypass";
    } else {
        wsFileName        = L"a.exe";
        wsProcName        = L"C:\\a.exe";
        wsProcPath        = L"C:\\a.exe";
        wsProcCmdLine     = L"\"a.exe\"";
        wsProcPara        = L"a.exe";
        wsParentProcPath     = L"C:\\b.exe";
        wsParentProcCmdLine  = L"\"C:\\b.exe\"";
    }

    std::wstring wsUserName          = L"WIN-913056QNOGK\\DELL";
    std::wstring wsUserSid           = L"S-1-5-21-3782372158-3025124834-3246284786-1000";
    std::wstring wsParentProcUserName = L"WIN-9PARENTCGK\\DELL";
    std::wstring wsParentProcUserSid  = L"S-1-5-21-3782372158-3025124834-3246284786-1234";
    std::wstring FileVersion  = L"6.1.7601.17514 (win7sp1_rtm.101119-1850)";
    std::wstring Description  = L"Host Process for Windows Services";
    std::wstring Product      = L"Microsoft Windows Operating System";
    std::wstring Company      = L"Microsoft Corporation";
    std::wstring OriginalFileName = L"cmd.exe";

    ULONG uProcId = rand(), uParentProcId = rand();
    std::string strJson = "";

    pEvt = (PTHREAT_EVENT_PROC_START)malloc(sizeof(THREAT_EVENT_PROC_START));
    if (NULL == pEvt) return strJson;
    memset(pEvt, 0, sizeof(THREAT_EVENT_PROC_START));

    pEvt->bStart = 'X';
    pEvt->llTimeStamp = (LONGLONG)rand();
    pEvt->stProcInfo.ulPid = uProcId;
    memcpy(pEvt->stProcInfo.ProcGuid,    wsRand.c_str(),    wsRand.length()    * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcFileName, wsFileName.c_str(), wsFileName.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcPath,    wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcPara.c_str(), wsProcPara.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserName, wsUserName.c_str(), wsUserName.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserSid,  wsUserSid.c_str(),  wsUserSid.length()  * sizeof(WCHAR));
    pEvt->stProcUserInfo.ulTerminalSessionId = rand() % 10;
    memcpy(pEvt->stProcFileInfo.FileVersion,      FileVersion.c_str(),      FileVersion.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Description,      Description.c_str(),      Description.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Product,          Product.c_str(),          Product.length()          * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Company,          Company.c_str(),          Company.length()          * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.OriginalFileName, OriginalFileName.c_str(), OriginalFileName.length() * sizeof(WCHAR));
    pEvt->stParentProcInfo.ulPid = uParentProcId;
    memcpy(pEvt->stParentProcInfo.ProcGuid,   wsParentGuidString.c_str(),  wsParentGuidString.length()  * sizeof(WCHAR));
    memcpy(pEvt->stParentProcInfo.ProcPath,   wsParentProcPath.c_str(),    wsParentProcPath.length()    * sizeof(WCHAR));
    memcpy(pEvt->stParentProcInfo.ProcCMDLine,wsParentProcCmdLine.c_str(), wsParentProcCmdLine.length() * sizeof(WCHAR));
    memcpy(pEvt->stParentProcUserInfo.ProcUserName, wsParentProcUserName.c_str(), wsParentProcUserName.length() * sizeof(WCHAR));
    memcpy(pEvt->stParentProcUserInfo.ProcUserSid,  wsParentProcUserSid.c_str(),  wsParentProcUserSid.length()  * sizeof(WCHAR));
    pEvt->stParentProcUserInfo.ulTerminalSessionId = rand() % 10;

    strJson = ThreatLog_ProcStart_GetJson(ComputerID, pEvt);
    free(pEvt);
    return strJson;
}

// 原文：WLSimulateJson::ThreatLog_SimulateJson_Reg（line 647）
std::string IEG_SimulateJson_Reg(const std::wstring& ComputerID, bool bHit)
{
    PTHREAT_EVENT_REG pEvt = NULL;

    std::wstring wsRand = ReturnCurrentTimeWstr();

    std::wstring wsFileName, wsProcName, wsProcPath, wsProcCmdLine;
    std::wstring wsParentProcPath, wsParentProcCmdLine;
    if (bHit) {
        wsFileName    = L"test_reg.exe";
        wsProcName    = L"test_reg.exe";
        wsProcPath    = L"C:\\Windows\\system32\\test_reg.exe";
        wsProcCmdLine = L"C:\\Windows\\system32\\test_reg.exe for test parameters...";
        wsParentProcPath     = L"C:\\Windows\\system32\\parenttest.exe";
        wsParentProcCmdLine  = L"C:\\Windows\\system32\\parenttest.exe for parent paraments!";
    } else {
        wsFileName    = L"c.exe";
        wsProcName    = L"c.exe";
        wsProcPath    = L"C:\\c.exe";
        wsProcCmdLine = L"C:\\c.exe";
        wsParentProcPath     = L"C:\\d.exe";
        wsParentProcCmdLine  = L"C:\\d.exe!";
    }

    std::wstring wsUserName = L"WIN-913056QNOGK\\DELL";
    std::wstring wsUserSid  = L"S-1-5-21-3782372158-3025124834-3246284786-1000";
    std::wstring FileVersion      = L"6.1.7601.17514 (win7sp1_rtm.101119-1850)";
    std::wstring Description      = L"Host Process for Windows Services";
    std::wstring Product          = L"Microsoft Windows Operating System";
    std::wstring Company          = L"Microsoft Corporation";
    std::wstring OriginalFileName = L"vdsldr.exe";
    std::wstring wsHKeyReplaced   = L"HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\services\\TVqQAAMAAAAEAAAA";
    std::wstring wsRegValue       = L"TVqQAAMAAAAEAAAA";
    const WCHAR* REG_DEFAULT_VALUE = L"(Default)";

    ULONG uProcId = rand();
    std::string strJson = "";

    pEvt = (PTHREAT_EVENT_REG)malloc(sizeof(THREAT_EVENT_REG));
    if (NULL == pEvt) return strJson;
    memset(pEvt, 0, sizeof(THREAT_EVENT_REG));

    pEvt->Operation = THREAT_EVENT_TYPE_REG;
    pEvt->llTimeStamp = (LONGLONG)rand();
    pEvt->stProcInfo.ulPid = uProcId;
    memcpy(pEvt->stProcInfo.ProcGuid,     wsRand.c_str(),      wsRand.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcFileName, wsFileName.c_str(),  wsFileName.length()  * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcPath,     wsProcPath.c_str(),  wsProcPath.length()  * sizeof(WCHAR));
    memcpy(pEvt->stProcInfo.ProcCMDLine,  wsProcCmdLine.c_str(),wsProcCmdLine.length()* sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserName, wsUserName.c_str(), wsUserName.length() * sizeof(WCHAR));
    memcpy(pEvt->stProcUserInfo.ProcUserSid,  wsUserSid.c_str(),  wsUserSid.length()  * sizeof(WCHAR));
    pEvt->stProcUserInfo.ulTerminalSessionId = rand() % 10;
    memcpy(pEvt->stProcFileInfo.FileVersion,      FileVersion.c_str(),      FileVersion.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Description,      Description.c_str(),      Description.length()      * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Product,          Product.c_str(),          Product.length()          * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.Company,          Company.c_str(),          Company.length()          * sizeof(WCHAR));
    memcpy(pEvt->stProcFileInfo.OriginalFileName, OriginalFileName.c_str(), OriginalFileName.length() * sizeof(WCHAR));
    memcpy(pEvt->stRegInfo.RegKeyPath,       wsHKeyReplaced.c_str(),  wsHKeyReplaced.length()  * sizeof(WCHAR));
    memcpy(pEvt->stRegInfo.RegTargetKeyPath, wsHKeyReplaced.c_str(),  wsHKeyReplaced.length()  * sizeof(WCHAR));
    memcpy(pEvt->stRegInfo.RegValueName,     REG_DEFAULT_VALUE,        wcslen(REG_DEFAULT_VALUE)* sizeof(WCHAR));
    memcpy(pEvt->stRegInfo.RegValue,         wsRegValue.c_str(),       wsRegValue.length()       * sizeof(WCHAR));

    strJson = ThreatLog_Reg_GetJson(ComputerID, pEvt);
    free(pEvt);
    return strJson;
}
