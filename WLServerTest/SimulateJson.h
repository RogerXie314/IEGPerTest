#pragma once
#include "../include/WLUtilities/WLJsonParse.h"
#include "..\WLLoginUI\crypto\sha1.h"


#define REG_DEFAULT_VALUE			L"(Default)"  

//added by lzq:MAY17
//WinEvent 结构体
/*
[
{
"WinEventLog.FilterRTID": "86547",
"factoryid": "3",
"WinEventLog.Keywords": "0x8020000000000000",
"assetIp": "192.168.7.73",
"WinEventLog.Level": "0",
"WinEventLog.ProviderName": "Microsoft-Windows-Security-Auditing",
"WinEventLog.ThreadID": "6204",
"WinEventLog.SourceAddress": "192.168.7.76",
"WinEventLog.ProviderGuid": "{54849625-5478-4994-a5ba-3e3b0328c30d}",
"WinEventLog.Application": "\\device\\harddiskvolume3\\windows\\system32\\svchost.exe",
"WinEventLog.RemoteMachineID": "S-1-0-0",
"WinEventLog.Task": "12810",
"WinEventLog.Version": "1",
"WinEventLog.DestPort": "5353",
"clientip": "192.168.7.73",
"WinEventLog.SystemTime": "2023-05-17T06:49:51.6998291Z",
"WinEventLog.Channel": "Security",
"WinEventLog.LayerRTID": "44",
"WinEventLog.TimeCreated": 1684306192,
"WinEventLog.EventRecordID": "2005088",
"caffectedip": "192.168.7.73",
"WinEventLog.SourcePort": "5353",
"WinEventLog.Direction": "%%14592",
"WinEventLog.EventID": 5156,
"ComputerID": "041E9F69win10-hch000C29C17F9C",
"WinEventLog.DestAddress": "224.0.0.251",
"WinEventLog.System": "5156101281000x80200000000000002005088Securitywin10-hch",
"WinEventLog.LayerName": "%%14610",
"WinEventLog.RemoteUserID": "S-1-0-0",
"WinEventLog.Computer": "win10-hch",
"WinEventLog.Protocol": "17",
"WinEventLog.Opcode": "0",
"WinEventLog.ProcessID": "1504",
"threat": true
}
]
*/
typedef struct _THREAT_EVENT_WINEVENT
{
	WCHAR FilterRTID[GUID_STRING_SIZE];
	WCHAR factoryid[MAX_PATH];
	WCHAR Keywords[MAX_DEVICE_PATH];
	WCHAR assetIp[MAX_PATAMETERS_LENGTH];

	WCHAR Level[GUID_STRING_SIZE];
	WCHAR ProviderName[MAX_PATH];
	WCHAR ThreadID[MAX_DEVICE_PATH];
	WCHAR SourceAddress[MAX_PATAMETERS_LENGTH];
	WCHAR ProviderGuid[GUID_STRING_SIZE];
	WCHAR Application[MAX_PATH];
	WCHAR RemoteMachineID[MAX_DEVICE_PATH];
	WCHAR Task[MAX_PATAMETERS_LENGTH];
	WCHAR Version[GUID_STRING_SIZE];
	WCHAR DestPort[MAX_PATH];
	WCHAR clientip[MAX_DEVICE_PATH];
	WCHAR SystemTime[MAX_PATAMETERS_LENGTH];
	WCHAR Channel[GUID_STRING_SIZE];
	WCHAR LayerRTID[MAX_PATH];
	WCHAR TimeCreated[MAX_DEVICE_PATH];
	WCHAR EventRecordID[MAX_PATAMETERS_LENGTH];
	WCHAR caffectedip[GUID_STRING_SIZE];
	WCHAR SourcePort[MAX_PATH];
	WCHAR Direction[MAX_DEVICE_PATH];
	WCHAR EventID[MAX_PATAMETERS_LENGTH];
	WCHAR ComputerID[GUID_STRING_SIZE];
	WCHAR DestAddress[MAX_PATH];
	WCHAR System[MAX_DEVICE_PATH];
	WCHAR LayerName[MAX_PATAMETERS_LENGTH];
	WCHAR RemoteUserID[GUID_STRING_SIZE];
	WCHAR Computer[MAX_PATH];
	WCHAR Protocol[MAX_DEVICE_PATH];
	WCHAR Opcode[MAX_PATAMETERS_LENGTH];
	WCHAR ProcessID[MAX_PATAMETERS_LENGTH];
	bool threat;
}THREAT_EVENT_WINEVENT, *PTHREAT_EVENT_WINEVENT;

class WLSimulateJson
{
public:
   WLSimulateJson(void);

  ~WLSimulateJson(void);

  //only this one added by lzq  //暂时不用！
	  std::string ThreatLog_WinEvent_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_WINEVENT pWinEvent);

	  UINT AddJsonCmdID(IN std::string strJson, IN UINT iCmdID,char** SendBuf);//CNC

	  std::string  UnicodeToUTF8(const std::wstring &str);
	  std::wstring UTF8ToUnicode(const std::string &szAnsi );

	  //需要分别实现   五个SimulateJson中分别调用  格式需要符合心跳那几个字段
	  std::string ThreatLog_ProcStart_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_PROC_START pProcStartEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr);
	  std::string ThreatLog_Reg_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_REG pRegEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr);
	  std::string ThreatLog_File_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_FILE pFileEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr);
	  std::string ThreatLog_Proc_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_PROC pProcEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr);
	  std::string ThreatLog_WinEvent_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_WINEVENT pWinEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr);

public:
	     UINT ThreatLog_SimulateJson_File_ReturnBuf(__in tstring ComputerID,char** RstBuf);

    std::string ThreatLog_SimulateJson_File(__in tstring ComputerID, BOOL bHit = TRUE);
	std::string ThreatLog_SimulateJson_ProcStart(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP, BOOL bHit = TRUE);
	std::string ThreatLog_SimulateJson_WinEventLog(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP);
	std::string ThreatLog_SimulateJson_Reg(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP, BOOL bHit = TRUE);
	std::string ThreatLog_SimulateJson_Proc(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP);


	std::string OptLog_SimulateJson_Nwl(__in tstring ComputerID);

};