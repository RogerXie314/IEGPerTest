#include "StdAfx.h"
#include "SimulateJson.h"
#include "../include/WLProtocal/Protocal.h"


WLSimulateJson::WLSimulateJson(void)
	{
	}

WLSimulateJson::~WLSimulateJson(void)
	{
	}

UINT WLSimulateJson::AddJsonCmdID(IN std::string strJson, IN UINT iCmdID,char** SendBuf)//CNC
{
	static CProtocal Portocal;

	std::wstring wsError = _T("");

	UINT  nProtocalLen		= 0;
	char* pProtocalData		= NULL;
	int iRet				= 0;

	if (!Portocal.GetPortocal(strJson.c_str(), strJson.length()-1, iCmdID, pProtocalData, nProtocalLen, &wsError))
	{
		return FALSE;
	}

	*SendBuf = pProtocalData;

/*
	if (NULL == m_TcpConnectInstance)
	{
		return FALSE;
	}

	if (!m_TcpConnectInstance->IsConnect())
	{
		if (!m_TcpConnectInstance->CreateSocket())
		{
			return FALSE;
		}
	}

	iRet = m_TcpConnectInstance->SendData(pSendBuf, nProtocalLen, &wsError);
	if (NO_ERROR == iRet)
	{
		bRet = TRUE;
	}

	if (NULL != pProtocalData)
	{
		delete []pProtocalData;
		pProtocalData = NULL;
	}
*/
	return nProtocalLen;
}

std::wstring WLSimulateJson::UTF8ToUnicode(const std::string &szAnsi )
{
	int nLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, szAnsi.c_str(), -1, NULL, 0);
	WCHAR* wszAnsi = new WCHAR[nLen + 1];
	memset(wszAnsi, 0, sizeof(WCHAR) * (nLen + 1));
	nLen = MultiByteToWideChar(CP_UTF8, 0, szAnsi.c_str(), -1, wszAnsi, nLen + 1);
	std::wstring strRet;
	strRet = wszAnsi;
	delete []wszAnsi;
	return strRet;
}

std::string  WLSimulateJson::UnicodeToUTF8(const std::wstring &str)
{
    //wchar_t  wszAnsi[1024*40]={0};
    //wsprintfW(wszAnsi, L"%s", str.c_str());

    int nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    CHAR* szUtf8 = new CHAR[nLen + 1];
    memset(szUtf8, 0, nLen + 1);
    nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, szUtf8, nLen + 1, NULL, NULL);
    std::string strUtf8 = szUtf8;	
    delete []szUtf8;
    return strUtf8;
}

//added by lzq:20230522 添加三个函数 生成随机字符串！
static unsigned long RandString_get_stage()
{
	ULONG secs = time(NULL);
	//secs = (secs / 300) * 300; -- 5分钟内随机码不变化，修改为直接取秒数 chennian 20181204
	return secs;
}

static void RandString_formatTime(string & tmStr, time_t time1)  
{  
	struct tm tm1;  
	char szTime[200];

	tm1 = *localtime(&time1);  

	sprintf(szTime, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d",  
		tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday,  
		tm1.tm_hour, tm1.tm_min,tm1.tm_sec);  
	tmStr = szTime;
	return;
}  

//有问题，暂时不可调用
int RandString_getRandString(std::wstring & randString)
{
	int retval = -1;
	sha1_context ctx;
	string strTime;
	wstring harddiskId;
	wstring strError;
	unsigned char digest[20];
	WCHAR data[20] = {0};


	sha1_starts(&ctx);

	RandString_formatTime(strTime, (time_t)RandString_get_stage());
	sha1_update(&ctx, (unsigned char *)strTime.c_str(), strTime.length());

	CCredentialMgr::GetMgr()->GetHardDiskID(harddiskId, &strError);
	sha1_update(&ctx, (unsigned char *)harddiskId.c_str(), harddiskId.length());

	sha1_finish(&ctx, digest);

	_snwprintf(data, _countof(data), L"%02x%02x%02x", digest[0], digest[1], digest[2]);

	randString = data;
	return 0;
}

std::wstring ReturnCurrentTimeWstr()
{
	time_t tt = time(NULL);//tt是一个时间戳
	tm* t= localtime(&tt);
	int year = t->tm_year + 1900;
	int mon = t->tm_mon + 1;
	int day = t->tm_mday;
	int hour = t->tm_hour;
	int min = t->tm_min;
	int sec= t->tm_sec;

	wostringstream tmp;
	tmp<<year<<L"-"<<mon<<L"-"<<day<<L" "<<hour<<L":"<<min<<L":"<<sec;
	std::wstring wtime = tmp.str();
	return wtime;
}
//暂时未用;没有调用
UINT WLSimulateJson::ThreatLog_SimulateJson_File_ReturnBuf(__in tstring ComputerID,char** RstBuf)
{
	PTHREAT_EVENT_FILE pEvt = NULL;

	std::wstring wsGuidString = _T("{THREAT306CC45482AA3DC7 3D1F34E624}");

	std::wstring wsProcName = _T("test.exe");
	std::wstring wsProcPath = _T("C:\\Windows\\system32\\test.exe");
	std::wstring wsProcCmdLine = _T("C:\\Windows\\system32\\test.exe for test parameters...");


	std::wstring wsUserName = _T("WIN-913056QNOGK\\DELL");
	std::wstring wsUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1000");
	std::wstring wsUserSessionID = _T("1");

	WCHAR szFilePath[MAX_DEVICE_PATH]=_T("C:\\Windows\\Prefetch\\AUDIODG.EXE");
	WCHAR szTargetFilePath[MAX_DEVICE_PATH]=_T("C:\\Windows\\Prefetch\\AUDIODG.EXE-D0D776AC.pf");

	std::string MD5String = ""; 

	std::wstring wsFilePath = _T("\\device\\harddiskvolume3\\windows\\system32\\svchost.exe");
	std::wstring wsFileFolder = _T("\\device\\harddiskvolume3\\windows\\system32");
	std::wstring wsFileName = _T("svchost.exe");
	std::wstring wsFileExtension = _T(".exe");

	static CWLJsonParse json;

	std::string strJson = "";

	ULONG  RandPid;


	pEvt = (PTHREAT_EVENT_FILE)malloc(sizeof(THREAT_EVENT_FILE));
	if (NULL == pEvt)
	{
		return NULL;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_FILE));

	RandPid = rand();


	//开始填充pEvt结构体：

	//USHORT Operation;
	pEvt->Operation = THREAT_EVENT_TYPE_FILE;

	//LONGLONG llTimeStamp;
	pEvt->llTimeStamp = (LONGLONG)rand();

	//THREAT_PROC_PROCINFO stProcInfo;
	////ULONG ulPid;
	pEvt->stProcInfo.ulPid = RandPid;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stProcInfo.ProcGuid, wsGuidString.c_str(), wsGuidString.length() * sizeof(WCHAR));

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcFileName, wsProcName.c_str(), wsProcName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stProcInfo.ProcPath, wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcCmdLine.c_str(), wsProcCmdLine.length() * sizeof(WCHAR));

	//THREAT_PROC_USERINFO stProcUserInfo;
	memcpy(pEvt->stProcUserInfo.ProcUserName,wsUserName.c_str(),wsUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stProcUserInfo.ProcUserSid,wsUserSid.c_str(),wsUserSid.length()*sizeof(WCHAR));
	pEvt->stProcUserInfo.ulTerminalSessionId = rand()%10;

	//THREAT_PROC_FILEINFO stFileInfo;
	memcpy(pEvt->stFileInfo.FilePath, wsFilePath.c_str(),wsFilePath.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileFolder, wsFileFolder.c_str(),wsFileFolder.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileName, wsFileName.c_str(),wsFileName.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileExtention, wsFileExtension.c_str(),wsFileExtension.length()  * sizeof(WCHAR));

	//THREAT_PROC_FILEINFO stTargetFileInfo;
	memcpy(pEvt->stTargetFileInfo.FilePath, wsFilePath.c_str(),wsFilePath.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileFolder, wsFileFolder.c_str(),wsFileFolder.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileName, wsFileName.c_str(),wsFileName.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileExtention, wsFileExtension.c_str(),wsFileExtension.length()  * sizeof(WCHAR));


	//    strJson = ThreatLog_File_GetJson_HBP(ComputerID, pEvt,CMDTYPE_POLICY,DATA_TO_SERVER_CLIENT_ADMIN_LOG,_T("DomainTest_File"),_T("WinTest_ProcStart"),wstrClientID,wstrClientIP);
	strJson = json.ThreatLog_File_GetJson(ComputerID, pEvt);

	char* pSendBuf=NULL;

	UINT proLen = AddJsonCmdID(strJson,THREAT_EVENT_UPLOAD_CMDID,&pSendBuf);  //NC,记得释放

	if(pSendBuf != NULL)
	{
		//DbgBreakPoint();
		*RstBuf = pSendBuf;
	}
	else
	{
		//DbgBreakPoint();
	}


	free(pEvt);
	pEvt = NULL;

	return proLen;
}

std::string WLSimulateJson::OptLog_SimulateJson_Nwl(__in tstring ComputerID)
{
	  //

	std::string sTmp =  "Not implemented in this func";
	return sTmp;

}

/*
rule:2251
关联 
*事件 
&& AND 
文件访问-文件名 = mimilsa.log 
文件访问-进程文件名 = lsass.exe 



2374

关联 
*事件 
&& AND 
文件访问-进程文件名 = dns.exe 
|| OR 
文件访问-操作类型 = 创建 
文件访问-操作类型 = 删除 
文件访问-操作类型 = 写 
文件访问-文件名 != dns.log		

*/


//当前实际调用：
std::string WLSimulateJson::ThreatLog_SimulateJson_File(__in tstring ComputerID, BOOL bHit)
{
	PTHREAT_EVENT_FILE pEvt = NULL;

	std::wstring wsGuidString = _T("{THREAT306CC45482AA3DC7 3D1F34E624}");
	std::wstring wsRand = ReturnCurrentTimeWstr();

	std::wstring wsProcName = _T("");
	std::wstring wsProcPath = _T("");
	std::wstring wsProcCmdLine = _T("");
	if (bHit)
	{
		wsProcName = _T("dns.exe");
		wsProcPath = _T("D:\\dns.exe");
		wsProcCmdLine = _T("D:\\dns.exe set");
	}
	else
	{
		wsProcName = _T("e.exe");
		wsProcPath = _T("D:\\e.exe");
		wsProcCmdLine = _T("D:\\e.exe");
	}

	std::wstring wsUserName = _T("WIN-913056QNOGK\\DELL");
	std::wstring wsUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1000");
	std::wstring wsUserSessionID = _T("1");

	WCHAR szFilePath[MAX_DEVICE_PATH]=_T("C:\\Windows\\Prefetch\\mimilsa.log");
	WCHAR szTargetFilePath[MAX_DEVICE_PATH]=_T("C:\\Windows\\Prefetch\\mimilsa.log");

	std::string MD5String = ""; 

	std::wstring wsFilePath = _T("\\device\\harddiskvolume3\\windows\\system32\\mimilsa.log");
	std::wstring wsFileFolder = _T("\\device\\harddiskvolume3\\windows\\system32");
	std::wstring wsFileName = _T("mimilsa.log");
	std::wstring wsFileExtension = _T("log");

	static CWLJsonParse json;

	std::string strJson = "";

	ULONG  RandPid;


	pEvt = (PTHREAT_EVENT_FILE)malloc(sizeof(THREAT_EVENT_FILE));
	if (NULL == pEvt)
	{
		return NULL;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_FILE));

	RandPid = rand();


	//开始填充pEvt结构体：

	//USHORT Operation;
	pEvt->Operation = THREATLOG_TYPE_FILE_CREATE;  // THREAT_EVENT_TYPE_FILE;

	//LONGLONG llTimeStamp;
	pEvt->llTimeStamp = (LONGLONG)rand();

	//THREAT_PROC_PROCINFO stProcInfo;
	////ULONG ulPid;
	pEvt->stProcInfo.ulPid = RandPid;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stProcInfo.ProcGuid, wsRand.c_str(), wsRand.length() * sizeof(WCHAR));//sjh

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcFileName, wsProcName.c_str(), wsProcName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stProcInfo.ProcPath, wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcCmdLine.c_str(), wsProcCmdLine.length() * sizeof(WCHAR));

	//THREAT_PROC_USERINFO stProcUserInfo;
	memcpy(pEvt->stProcUserInfo.ProcUserName,wsUserName.c_str(),wsUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stProcUserInfo.ProcUserSid,wsUserSid.c_str(),wsUserSid.length()*sizeof(WCHAR));
	pEvt->stProcUserInfo.ulTerminalSessionId = rand()%10;

	//THREAT_PROC_FILEINFO stFileInfo;
	memcpy(pEvt->stFileInfo.FilePath, wsFilePath.c_str(),wsFilePath.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileFolder, wsFileFolder.c_str(),wsFileFolder.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileName, wsFileName.c_str(),wsFileName.length()  * sizeof(WCHAR));
	memcpy(pEvt->stFileInfo.FileExtention, wsFileExtension.c_str(),wsFileExtension.length()  * sizeof(WCHAR));

	//THREAT_PROC_FILEINFO stTargetFileInfo;
	memcpy(pEvt->stTargetFileInfo.FilePath, wsFilePath.c_str(),wsFilePath.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileFolder, wsFileFolder.c_str(),wsFileFolder.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileName, wsFileName.c_str(),wsFileName.length()  * sizeof(WCHAR));
	memcpy(pEvt->stTargetFileInfo.FileExtention, wsFileExtension.c_str(),wsFileExtension.length()  * sizeof(WCHAR));

	
	 //    strJson = ThreatLog_File_GetJson_HBP(ComputerID, pEvt,CMDTYPE_POLICY,DATA_TO_SERVER_CLIENT_ADMIN_LOG,_T("DomainTest_File"),_T("WinTest_ProcStart"),wstrClientID,wstrClientIP);
	strJson = json.ThreatLog_File_GetJson(ComputerID, pEvt);

	free(pEvt);
	pEvt = NULL;

	return strJson;
}

std::string WLSimulateJson::ThreatLog_SimulateJson_ProcStart(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP, BOOL bHit)
{
	PTHREAT_EVENT_PROC_START pEvt = NULL;

	std::wstring wsGuidString = _T("{THREATC912EDF43A5BA884 DC13577427}");
	std::wstring wsRand = ReturnCurrentTimeWstr();

	std::wstring wsParentGuidString = _T("{89382C912EDF43A5BA884 DC13577427}");

	std::wstring wsFileName = _T("");  
	std::wstring wsProcName = _T("");
	std::wstring wsProcPath = _T("");
	std::wstring wsProcCmdLine = _T("");//rule 
	std::wstring wsProcPara    = _T("");
	std::wstring wsParentProcPath = _T("");
	std::wstring wsParentProcCmdLine = _T("");

	if (bHit)
	{
		wsFileName = _T("cmd.exe");  
		wsProcName = _T("C:\\Windows\\System32\\cmd.exe");
		wsProcPath = _T("C:\\Windows\\System32\\cmd.exe");
		wsProcCmdLine = _T("\"cmd.exe\" /c \"reg add \"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\" /v set dir NoSetTaskbar /t REG_DWORD /d 1 /f\"");//rule 
		wsProcPara    = _T("cmd.exe /c dir test set");

		wsParentProcPath = _T("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
		wsParentProcCmdLine = _T("\"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe\" -exec bypass");
	}
	else// 不命中
	{
		wsFileName = _T("a.exe");  
		wsProcName = _T("C:\\a.exe");
		wsProcPath = _T("C:\\a.exe");
		wsProcCmdLine = _T("\"a.exe\"");//rule 
		wsProcPara    = _T("a.exe");
		wsParentProcPath = _T("C:\\b.exe");
		wsParentProcCmdLine = _T("\"C:\\b.exe\"");
	}
	
	std::wstring wsUserName = _T("WIN-913056QNOGK\\DELL");
	std::wstring wsUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1000");
	//std::wstring wsUserSessionID = _T("1");

	std::wstring wsParentProcUserName = _T("WIN-9PARENTCGK\\DELL");
	std::wstring wsParentProcUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1234");
	//std::wstring wsParentProcUserSessionID = _T("2");

	std::wstring wsComputerID = ComputerID;
	std::string strJson = "";

	std::wstring FileVersion = _T("6.1.7601.17514 (win7sp1_rtm.101119-1850)");
	std::wstring Description =_T("Host Process for Windows Services");
	std::wstring Product = _T("Microsoft Windows Operating System");
	std::wstring Company = _T("Microsoft Corporation");
	std::wstring OriginalFileName = _T("cmd.exe");

	ULONG uProcId=rand();
	ULONG uParentProcId=rand();

	static CWLJsonParse json;


	pEvt = (PTHREAT_EVENT_PROC_START)malloc(sizeof(THREAT_EVENT_PROC_START));
	if (NULL == pEvt)
	{
		return strJson;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_PROC_START));


	//UCHAR bStart;
	pEvt->bStart = 'X';

	//LONGLONG llTimeStamp;
	pEvt->llTimeStamp = (LONGLONG)rand();

	//THREAT_PROC_PROCINFO stProcInfo;
	////ULONG ulPid;
	pEvt->stProcInfo.ulPid = uProcId;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stProcInfo.ProcGuid, wsRand.c_str(), wsRand.length() * sizeof(WCHAR));

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcFileName, wsFileName.c_str(), wsFileName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stProcInfo.ProcPath, wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcPara.c_str(), wsProcPara.length() * sizeof(WCHAR));


	//THREAT_PROC_USERINFO stProcUserInfo;
	memcpy(pEvt->stProcUserInfo.ProcUserName,wsUserName.c_str(),wsUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stProcUserInfo.ProcUserSid,wsUserSid.c_str(),wsUserSid.length()*sizeof(WCHAR));
	pEvt->stProcUserInfo.ulTerminalSessionId = rand()%10;



	//THREAT_PROC_FILEINFO stProcFileInfo;
	memcpy(pEvt->stProcFileInfo.FileVersion, FileVersion.c_str(), FileVersion.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Description, Description.c_str(), Description.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Product, Product.c_str(), Product.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Company, Company.c_str(), Company.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.OriginalFileName, OriginalFileName.c_str(), OriginalFileName.length() * sizeof(WCHAR));

	//THREAT_PROC_PROCINFO stParentProcInfo;
	////ULONG ulPid;
	pEvt->stParentProcInfo.ulPid = uParentProcId;
	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stParentProcInfo.ProcGuid, wsParentGuidString.c_str(), wsParentGuidString.length() * sizeof(WCHAR));
	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stParentProcInfo.ProcPath, wsParentProcPath.c_str(), wsParentProcPath.length() * sizeof(WCHAR));
	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stParentProcInfo.ProcCMDLine, wsParentProcCmdLine.c_str(), wsParentProcCmdLine.length() * sizeof(WCHAR));

	//THREAT_PROC_USERINFO stParentProcUserInfo;
	memcpy(pEvt->stParentProcUserInfo.ProcUserName,wsParentProcUserName.c_str(),wsParentProcUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stParentProcUserInfo.ProcUserSid,wsParentProcUserSid.c_str(),wsParentProcUserSid.length()*sizeof(WCHAR));
	pEvt->stParentProcUserInfo.ulTerminalSessionId = rand()%10;

	//strJson = ThreatLog_ProcStart_GetJson_HBP(ComputerID, pEvt,CMDTYPE_CMD,DATA_TO_SERVER_CLIENT_ADMIN_LOG,_T("DomainTest_ProcStart"),_T("WinTest"),wstrClientID,wstrClientIP);
	strJson = json.ThreatLog_ProcStart_GetJson(ComputerID, pEvt);


	free(pEvt);
	pEvt = NULL;

	return strJson;
}
std::string WLSimulateJson::ThreatLog_SimulateJson_WinEventLog(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP)
{
	PTHREAT_EVENT_WINEVENT pEvt = NULL;


	std::wstring wsComputerID = _T("GetComputerInfo WinEvent");
	std::string  strJson = "";

	int uProcId = rand();
	int uParentProcId = rand();
	
	CString strRand;
	strRand.Format(_T("Random guid xxxxx field:%d"),uProcId);
  

	static CWLJsonParse json;

	std::wstring  FilterRTID = _T("8");
	std::wstring  factoryid = _T("218");
	std::wstring  Keywords = _T("0x8020000000000000");
	std::wstring  assetIp = _T("123.123.123.123");

	std::wstring  Level = _T("0");
	std::wstring  ProviderName = _T("Microsoft-Windows-Security-Auditing");
	std::wstring  ThreadID = _T("6204");
	std::wstring  SourceAddress = _T("123.123.123.123");
	std::wstring  ProviderGuid = ReturnCurrentTimeWstr(); //sjh

	
	std::wstring  Application = _T("\\device\\harddiskvolume3\\windows\\system32\\svchost.exe");
	std::wstring  RemoteMachineID = _T("S-1-0-0");
	std::wstring  Task = _T("12810");
	std::wstring  Version = _T("1");
	std::wstring  DestPort = _T("5353");
	std::wstring  clientip = _T("123.123.123.123");
	std::wstring  SystemTime = _T("2023-05-17T06:49:51.6998291Z");
	std::wstring  Channel = _T("Security");
	std::wstring  LayerRTID = _T("44");
	std::wstring  TimeCreated = _T("1684306192");
	std::wstring  EventRecordID = _T("2005088");
	std::wstring  caffectedip = _T("123.123.123.123");
	std::wstring  SourcePort = _T("5353");
	std::wstring  Direction = _T("%%14592");
	std::wstring  EventID = _T("4656");
	std::wstring  ComputerIDA = _T("041E9F69win10-hch000C29C17F9C");
	std::wstring  DestAddress = _T("123.123.123.123");
	std::wstring  System = _T("5156101281000x80200000000000002005088Securitywin10-hch");
	std::wstring  LayerName = _T("%%14610");
	std::wstring  RemoteUserID = _T("S-1-0-0");
	std::wstring  Computer = _T("win10-hch");
	std::wstring  Protocol = _T("17");
	std::wstring  Opcode = _T("0");
	std::wstring  ProcessID = _T("1504");
	//std::wstring  threat = false;


	pEvt = (PTHREAT_EVENT_WINEVENT)malloc(sizeof(THREAT_EVENT_WINEVENT));
	if (NULL == pEvt)
	{
		return strJson;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_WINEVENT));


	//UCHAR bStart;
	memcpy(pEvt->FilterRTID, FilterRTID.c_str(), FilterRTID.length() * sizeof(WCHAR));
	memcpy(pEvt->factoryid,factoryid.c_str(),factoryid.length()* sizeof(WCHAR));
	memcpy(pEvt->Keywords,Keywords.c_str(),Keywords.length()* sizeof(WCHAR));
	memcpy(pEvt->assetIp,assetIp.c_str(),assetIp.length()* sizeof(WCHAR));
	memcpy(pEvt->Level,Level.c_str(),Level.length()* sizeof(WCHAR));
	memcpy(pEvt->ProviderName,ProviderName.c_str(),ProviderName.length()* sizeof(WCHAR));
	memcpy(pEvt->ThreadID,ThreadID.c_str(),ThreadID.length()* sizeof(WCHAR));
	memcpy(pEvt->SourceAddress,SourceAddress.c_str(),SourceAddress.length()* sizeof(WCHAR));
	memcpy(pEvt->ProviderGuid,ProviderGuid.c_str(),ProviderGuid.length()* sizeof(WCHAR));
	memcpy(pEvt->Application,Application.c_str(),Application.length()* sizeof(WCHAR));
	memcpy(pEvt->RemoteMachineID,RemoteMachineID.c_str(),RemoteMachineID.length()* sizeof(WCHAR));
	memcpy(pEvt->Task,Task.c_str(),Task.length()* sizeof(WCHAR));
	memcpy(pEvt->Version,Version.c_str(),Version.length()* sizeof(WCHAR));
	memcpy(pEvt->DestPort,DestPort.c_str(),DestPort.length()* sizeof(WCHAR));
	memcpy(pEvt->clientip,clientip.c_str(),clientip.length()* sizeof(WCHAR));
	memcpy(pEvt->SystemTime,SystemTime.c_str(),SystemTime.length()* sizeof(WCHAR));
	memcpy(pEvt->Channel,Channel.c_str(),Channel.length()* sizeof(WCHAR));
	memcpy(pEvt->LayerRTID,LayerRTID.c_str(),LayerRTID.length()* sizeof(WCHAR));
	memcpy(pEvt->TimeCreated,TimeCreated.c_str(),TimeCreated.length()* sizeof(WCHAR));
	memcpy(pEvt->EventRecordID,EventRecordID.c_str(),EventRecordID.length()* sizeof(WCHAR));
	memcpy(pEvt->caffectedip,caffectedip.c_str(),caffectedip.length()* sizeof(WCHAR));
	memcpy(pEvt->SourcePort,SourcePort.c_str(),SourcePort.length()* sizeof(WCHAR));
	memcpy(pEvt->Direction,Direction.c_str(),Direction.length()* sizeof(WCHAR));
	memcpy(pEvt->EventID,EventID.c_str(),EventID.length()* sizeof(WCHAR));
	memcpy(pEvt->ComputerID,ComputerIDA.c_str(),ComputerIDA.length()* sizeof(WCHAR));
	memcpy(pEvt->DestAddress,DestAddress.c_str(),DestAddress.length()* sizeof(WCHAR));
	memcpy(pEvt->System,System.c_str(),System.length()* sizeof(WCHAR));
	memcpy(pEvt->LayerName,LayerName.c_str(),LayerName.length()* sizeof(WCHAR));
	memcpy(pEvt->RemoteUserID,RemoteUserID.c_str(),RemoteUserID.length()* sizeof(WCHAR));
	memcpy(pEvt->Computer,Computer.c_str(),Computer.length()* sizeof(WCHAR));
	memcpy(pEvt->Protocol,Protocol.c_str(),Protocol.length()* sizeof(WCHAR));
	memcpy(pEvt->Opcode,Opcode.c_str(),Opcode.length()* sizeof(WCHAR));
	memcpy(pEvt->ProcessID,ProcessID.c_str(),ProcessID.length()* sizeof(WCHAR));
	pEvt->threat = false;


	// strJson = ThreatLog_WinEvent_GetJson_HBP(ComputerID, pEvt,CMDTYPE_POLICY,DATA_TO_SERVER_HEARTBEAT,_T("DomainTest_WinEvent"),_T("WinTest_WinEvent"),wstrClientID,wstrClientIP);
	strJson = ThreatLog_WinEvent_GetJson(ComputerID, pEvt);

	
	free(pEvt);
	pEvt = NULL; 

	return strJson;
}

std::string WLSimulateJson::ThreatLog_SimulateJson_Reg(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP, BOOL bHit)
{
	PTHREAT_LOG_REG pLogReg;
	LONGLONG llTime;

	PTHREAT_EVENT_REG pEvt = NULL;

	std::wstring wsGuidString = _T("THREAT2003ACD446E87C331B6AAC3C203");
	std::wstring wsRand = ReturnCurrentTimeWstr();

	std::wstring wsParentGuidString = _T("");

	std::wstring wsFileName = _T("");
	std::wstring wsProcName = _T("");
	std::wstring wsProcPath = _T("");
	std::wstring wsProcCmdLine = _T("");
	std::wstring wsProcPara    = _T("");

	std::wstring wsParentProcPath = _T("");
	std::wstring wsParentProcCmdLine = _T("");
	if (bHit)
	{
		wsFileName = _T("test_reg.exe");
		wsProcName = _T("test_reg.exe");
		wsProcPath = _T("C:\\Windows\\system32\\test_reg.exe");
		wsProcCmdLine = _T("C:\\Windows\\system32\\test_reg.exe for test parameters...");
		wsProcPara    = _T("for test parameters...");

		wsParentProcPath = _T("C:\\Windows\\system32\\parenttest.exe");
		wsParentProcCmdLine = _T("C:\\Windows\\system32\\parenttest.exe for parent paraments!");
	}
	else //不命中
	{
		wsFileName = _T("c.exe");
		wsProcName = _T("c.exe");
		wsProcPath = _T("C:\\c.exe");
		wsProcCmdLine = _T("C:\\c.exe");
	    wsProcPara    = _T("xxxxx");

		wsParentProcPath = _T("C:\\d.exe");
		wsParentProcCmdLine = _T("C:\\d.exe!");
	}

	

	std::wstring wsUserName = _T("WIN-913056QNOGK\\DELL");
	std::wstring wsUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1000");
	//std::wstring wsUserSessionID = _T("1");

	std::wstring wsParentProcUserName = _T("WIN-9PARENTCGK\\DELL");
	std::wstring wsParentProcUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1234");
	//std::wstring wsParentProcUserSessionID = _T("2");

	std::string strJson = "";

	std::wstring FileVersion = _T("6.1.7601.17514 (win7sp1_rtm.101119-1850)");
	std::wstring Description =_T("Host Process for Windows Services");
	std::wstring Product = _T("Microsoft Windows Operating System");
	std::wstring Company = _T("Microsoft Corporation");
	std::wstring OriginalFileName = _T("vdsldr.exe");

	ULONG uProcId=rand();
	ULONG uParentProcId=rand();

	std::wstring wsHKeyReplaced = _T("HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\services\\TVqQAAMAAAAEAAAA");
	std::wstring wsRegValue = _T("TVqQAAMAAAAEAAAA");

	DWORD dwRegValue = 0;

	CWLJsonParse json;



	pEvt = (PTHREAT_EVENT_REG)malloc(sizeof(THREAT_EVENT_REG));
	if (NULL == pEvt)
	{
		return strJson;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_REG));

	//USHORT Operation;
	pEvt->Operation = THREAT_EVENT_TYPE_REG;

	//LONGLONG llTimeStamp;
	pEvt->llTimeStamp = (LONGLONG) rand();

	//THREAT_PROC_PROCINFO stProcInfo;
	////ULONG ulPid;
	pEvt->stProcInfo.ulPid = uProcId;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stProcInfo.ProcGuid, wsRand.c_str(), wsRand.length() * sizeof(WCHAR));

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcFileName, wsFileName.c_str(), wsFileName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stProcInfo.ProcPath, wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcCmdLine.c_str(), wsProcCmdLine.length() * sizeof(WCHAR));

	//THREAT_PROC_USERINFO stProcUserInfo;
	memcpy(pEvt->stProcUserInfo.ProcUserName,wsUserName.c_str(),wsUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stProcUserInfo.ProcUserSid,wsUserSid.c_str(),wsUserSid.length()*sizeof(WCHAR));
	pEvt->stProcUserInfo.ulTerminalSessionId = rand()%10;

	//THREAT_PROC_FILEINFO stProcFileInfo;
	memcpy(pEvt->stProcFileInfo.FileVersion, FileVersion.c_str(), FileVersion.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Description, Description.c_str(), Description.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Product, Product.c_str(), Product.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Company, Company.c_str(), Company.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.OriginalFileName, OriginalFileName.c_str(), OriginalFileName.length() * sizeof(WCHAR));

	//THREAT_REG_INFO stRegInfo;
	memcpy(pEvt->stRegInfo.RegKeyPath, wsHKeyReplaced.c_str(), wsHKeyReplaced.length() * sizeof(WCHAR));

	memcpy(pEvt->stRegInfo.RegTargetKeyPath, wsHKeyReplaced.c_str(), wsHKeyReplaced.length() * sizeof(WCHAR));

	memcpy(pEvt->stRegInfo.RegValueName, REG_DEFAULT_VALUE, wcslen(REG_DEFAULT_VALUE) * sizeof(WCHAR));

	memcpy(pEvt->stRegInfo.RegValue, wsRegValue.c_str(), wsRegValue.length() * sizeof(WCHAR));


	//strJson = ThreatLog_Reg_GetJson_HBP(ComputerID, pEvt,CMDTYPE_POLICY,DATA_TO_SERVER_HOSTDEFENCE,_T("DomainTest_Reg"),_T("WinTest_Reg"),wstrClientID,wstrClientIP);
	strJson = json.ThreatLog_Reg_GetJson(ComputerID, pEvt);


	free(pEvt);
	pEvt = NULL;


	return strJson;
}

std::string WLSimulateJson::ThreatLog_SimulateJson_Proc(__in tstring ComputerID,__in wstring wstrClientID,__in wstring wstrClientIP)
{
	LONGLONG llTime;

	std::wstring wsRand = ReturnCurrentTimeWstr();
	
	PTHREAT_EVENT_PROC pEvt = NULL;

	std::wstring wsFileName = _T("test_proc.exe");
	std::wstring wsProcName = _T("test_proc.exe");
	std::wstring wsProcPath = _T("C:\\Windows\\system32\\test_proc.exe");
	std::wstring wsProcCmdLine = _T("C:\\Windows\\system32\\test_proc.exe for test parameters...");
	std::wstring wsProcPara    = _T("for test parameters...");

	std::wstring wsParentProcPath = _T("C:\\Windows\\system32\\parenttest.exe");

	std::wstring wsTargetProcPath = _T("C:\\Windows\\system32\\tarvad.exe");
	std::wstring wsTargetProcParameter = _T("tarvad.exe parameter tests.");

	std::wstring wsParentProcCmdLine = _T("C:\\Windows\\system32\\parenttest.exe for parent paraments!");


	std::wstring wsUserName = _T("WIN-913056QNOGK\\DELL");
	std::wstring wsUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1000");
	//std::wstring wsUserSessionID = _T("1");

	std::wstring wsParentProcUserName = _T("WIN-9PARENTCGK\\DELL");
	std::wstring wsParentProcUserSid = _T("S-1-5-21-3782372158-3025124834-3246284786-1234");
	//std::wstring wsParentProcUserSessionID = _T("2");

	std::wstring FileVersion = _T("6.1.7601.17514 (win7sp1_rtm.101119-1850)");
	std::wstring Description =_T("Host Process for Windows Services");
	std::wstring Product = _T("Microsoft Windows Operating System");
	std::wstring Company = _T("Microsoft Corporation");
	std::wstring OriginalFileName = _T("vdsldr.exe");
	std::wstring wsGuidString = _T("THREAT625-5478-4994-a5ba-3e3b0328c30d");
	std::wstring wsTargetGuidString = _T("041E9F69win10-hch000C29C17F9C");
	std::wstring wsComputerID = _T("GetComputerInfo Proc!");
	std::string strJson = "";
	std::string MD5String = "";

	ULONG uProcId=rand();
	ULONG uTargetProcId=rand();


	static CWLJsonParse json;

	pEvt = (PTHREAT_EVENT_PROC)malloc(sizeof(THREAT_EVENT_PROC));
	if (NULL == pEvt)
	{
		return strJson;
	}

	memset(pEvt, 0, sizeof(THREAT_EVENT_PROC));

	//LONGLONG llTimeStamp;
	pEvt->llTimeStamp = (LONGLONG)rand();

	//THREAT_PROC_PROCINFO stProcInfo;
	////ULONG ulPid;
	pEvt->stProcInfo.ulPid = uProcId;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stProcInfo.ProcGuid, wsRand.c_str(), wsRand.length() * sizeof(WCHAR));

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcFileName, wsFileName.c_str(), wsFileName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stProcInfo.ProcPath, wsProcPath.c_str(), wsProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stProcInfo.ProcCMDLine, wsProcPara.c_str(), wsProcPara.length()* sizeof(WCHAR));

	//THREAT_PROC_USERINFO stProcUserInfo;
	memcpy(pEvt->stProcUserInfo.ProcUserName,wsUserName.c_str(),wsUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stProcUserInfo.ProcUserSid,wsUserSid.c_str(),wsUserSid.length()*sizeof(WCHAR));
	pEvt->stProcUserInfo.ulTerminalSessionId = rand()%10;

	//THREAT_PROC_FILEINFO stProcFileInfo;
	memcpy(pEvt->stProcFileInfo.FileVersion, FileVersion.c_str(), FileVersion.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Description, Description.c_str(), Description.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Product, Product.c_str(), Product.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.Company, Company.c_str(), Company.length() * sizeof(WCHAR));
	memcpy(pEvt->stProcFileInfo.OriginalFileName, OriginalFileName.c_str(), OriginalFileName.length() * sizeof(WCHAR));

	//THREAT_PROC_PROCINFO stParentProcInfo;
	////ULONG ulPid;
	pEvt->stTargetProcInfo.ulPid = uTargetProcId;

	////WCHAR ProcGuid[GUID_STRING_SIZE];
	memcpy(pEvt->stTargetProcInfo.ProcGuid, wsTargetGuidString.c_str(), wsTargetGuidString.length() * sizeof(WCHAR));

	////WCHAR ProcFileName[MAX_PATH];
	memcpy(pEvt->stTargetProcInfo.ProcFileName, wsFileName.c_str(), wsFileName.length() * sizeof(WCHAR));

	////WCHAR ProcPath[MAX_DEVICE_PATH];
	memcpy(pEvt->stTargetProcInfo.ProcPath, wsTargetProcPath.c_str(),wsTargetProcPath.length() * sizeof(WCHAR));

	////WCHAR ProcCMDLine[MAX_PATH];
	memcpy(pEvt->stTargetProcInfo.ProcCMDLine, wsTargetProcParameter.c_str(), wsTargetProcParameter.length() * sizeof(WCHAR));

	//THREAT_PROC_USERINFO stParentProcUserInfo;
	memcpy(pEvt->stTargetProcUserInfo.ProcUserName,wsParentProcUserName.c_str(),wsParentProcUserName.length()*sizeof(WCHAR));
	memcpy(pEvt->stTargetProcUserInfo.ProcUserSid,wsParentProcUserSid.c_str(),wsParentProcUserSid.length()*sizeof(WCHAR));
	pEvt->stTargetProcUserInfo.ulTerminalSessionId = rand()%10;

	//THREAT_PROC_ACCESS stProcAccess;
	pEvt->stProcAccess.bCreateProcess = PROCESS_QUERY_LIMITED_INFORMATION;
	
	//ThreatLog_Proc_GetJson_HBP已经不是成员函数,坚持使用原来的GetJson函数！！
    //strJson = ThreatLog_Proc_GetJson_HBP(ComputerID, pEvt,CMDTYPE_CMD,DATA_TO_SERVER_HEARTBEAT,_T("DomainTest_Proc"),_T("WinTest_Proc"),wstrClientID,wstrClientIP);
	strJson = json.ThreatLog_Proc_GetJson(ComputerID, pEvt);

	free(pEvt);
	pEvt = NULL;  

	return strJson;  
}









std::string WLSimulateJson::ThreatLog_WinEvent_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_WINEVENT pWinEvent)
{
	std::string sJsonPacket = "";

	Json::Value root;  
	Json::FastWriter writer;
	Json::Value person,CMDContent;

	if (NULL == pWinEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_SYSTEM;


	CMDContent["FilterRTID"] = UnicodeToUTF8(pWinEvent->FilterRTID);
	CMDContent["factoryid"] = UnicodeToUTF8(pWinEvent->factoryid);
	CMDContent["Keywords"] = UnicodeToUTF8(pWinEvent->Keywords);
	CMDContent["assetIp"] = UnicodeToUTF8(pWinEvent->assetIp);
	CMDContent["Level"] = UnicodeToUTF8(pWinEvent->Level); 
	CMDContent["ProviderName"] = UnicodeToUTF8(pWinEvent->ProviderName);
	CMDContent["ThreadID"] = UnicodeToUTF8(pWinEvent->ThreadID); 
	CMDContent["SourceAddress"] = UnicodeToUTF8(pWinEvent->SourceAddress);
	CMDContent["ProviderGuid"] = UnicodeToUTF8(pWinEvent->ProviderGuid);
	CMDContent["Application"] = UnicodeToUTF8(pWinEvent->Application);
	CMDContent["RemoteMachineID"] = UnicodeToUTF8(pWinEvent->RemoteMachineID); 
	CMDContent["Task"] = UnicodeToUTF8(pWinEvent->Task);
	CMDContent["Version"] = UnicodeToUTF8(pWinEvent->Version); 
	CMDContent["DestPort"] = UnicodeToUTF8(pWinEvent->DestPort);
	CMDContent["clientip"] = UnicodeToUTF8(pWinEvent->clientip); 
	CMDContent["SystemTime"] = UnicodeToUTF8(pWinEvent->SystemTime);
	CMDContent["Channel"] = UnicodeToUTF8(pWinEvent->Channel); 
	CMDContent["LayerRTID"] = UnicodeToUTF8(pWinEvent->LayerRTID);
	CMDContent["TimeCreated"] = UnicodeToUTF8(pWinEvent->TimeCreated); 
	CMDContent["EventRecordID"] = UnicodeToUTF8(pWinEvent->EventRecordID);
	CMDContent["caffectedip"] = UnicodeToUTF8(pWinEvent->caffectedip); 
	CMDContent["SourcePort"] = UnicodeToUTF8(pWinEvent->SourcePort);
	CMDContent["Direction"] = UnicodeToUTF8(pWinEvent->Direction); 
	CMDContent["EventID"] = UnicodeToUTF8(pWinEvent->EventID);
	CMDContent["ComputerID"] = UnicodeToUTF8(pWinEvent->ComputerID); 
	CMDContent["DestAddress"] = UnicodeToUTF8(pWinEvent->DestAddress);
	CMDContent["System"] = UnicodeToUTF8(pWinEvent->System); 
	CMDContent["LayerName"] = UnicodeToUTF8(pWinEvent->LayerName);
	CMDContent["RemoteUserID"] = UnicodeToUTF8(pWinEvent->RemoteUserID); 
	CMDContent["Computer"] = UnicodeToUTF8(pWinEvent->Computer);
	CMDContent["Protocol"] = UnicodeToUTF8(pWinEvent->Protocol); 
	CMDContent["Opcode"] = UnicodeToUTF8(pWinEvent->Opcode); 
	CMDContent["ProcessID"] = UnicodeToUTF8(pWinEvent->ProcessID);
	CMDContent["threat"] = (BOOLEAN)pWinEvent->threat;

	person["CMDContent"] = CMDContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket; 
}

std::string WLSimulateJson::ThreatLog_ProcStart_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_PROC_START pProcStartEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";
	std::wstring strRand = L"";

	RandString_getRandString(strRand); 

	Json::Value root;  
	Json::FastWriter writer;
	Json::Value person, CMDContent;


	if (NULL == pProcStartEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	//person["CMDTYPE"] = (int)cmdType;
	//person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_PROCSTART;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

   
	CMDContent["Process.TimeStamp"] = pProcStartEvent->llTimeStamp;
	CMDContent["Process.ProcessId"] = (Json::Int64)pProcStartEvent->stProcInfo.ulPid;
	CMDContent["Process.ProcessGuid"] = UnicodeToUTF8(strRand); //随机化的字段
	CMDContent["Process.ProcessFileName"] = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcFileName);
	CMDContent["Process.ProcessName"] = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcPath);
	CMDContent["Process.CommandLine"] = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcCMDLine);
	CMDContent["Process.User"] = UnicodeToUTF8(pProcStartEvent->stProcUserInfo.ProcUserName);
	CMDContent["Process.UserSid"] = UnicodeToUTF8(pProcStartEvent->stProcUserInfo.ProcUserSid);
	CMDContent["Process.TerminalSessionId"] = (Json::Int64)pProcStartEvent->stProcUserInfo.ulTerminalSessionId;

	CMDContent["Process.FileVersion"] = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.FileVersion);
	CMDContent["Process.Description"] = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Description);
	CMDContent["Process.Product"] = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Product);
	CMDContent["Process.Company"] = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.Company);
	CMDContent["Process.OriginalFileName"] = UnicodeToUTF8(pProcStartEvent->stProcFileInfo.OriginalFileName);
	CMDContent["Process.ParentProcessId"] = (Json::Int64)pProcStartEvent->stParentProcInfo.ulPid;
	CMDContent["Process.ParentProcessGuid"] = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcGuid);
	CMDContent["Process.ParentProcessFileName"] = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcFileName);
	CMDContent["Process.ParentProcessName"] = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcPath);
	CMDContent["Process.ParentCommandLine"] = UnicodeToUTF8(pProcStartEvent->stParentProcInfo.ProcCMDLine);
	CMDContent["Process.ParentUser"] = UnicodeToUTF8(pProcStartEvent->stParentProcUserInfo.ProcUserName);

	person["CMDContent"] = CMDContent;

	DWORD dwLanguage = WLUtils::GetLanguageType();

	if( dwLanguage == 0 )
	{
		person["clientLanguage"]= "zh";
	}
	else
	{
		person["clientLanguage"]= "en";
	}

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;

}

std::string WLSimulateJson::ThreatLog_Reg_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_REG pRegEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";
	std::wstring strRand = L"";

	RandString_getRandString(strRand); //lzq:这种随机化方法有问题!

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	if (NULL == pRegEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	//person["CMDTYPE"] = (int)cmdType;
	//person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_REG;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

	CMDContent["Registry.Operation"] = pRegEvent->Operation;
	CMDContent["Registry.TimeStamp"] = pRegEvent->llTimeStamp;
	CMDContent["Registry.ProcessId"] = (Json::Int64)pRegEvent->stProcInfo.ulPid;;
	CMDContent["Registry.ProcessGuid"] = UnicodeToUTF8(strRand);//随机化自字段
	CMDContent["Registry.ProcessName"] = UnicodeToUTF8(pRegEvent->stProcInfo.ProcPath);
	CMDContent["Registry.ProcessFileName"] = UnicodeToUTF8(pRegEvent->stProcInfo.ProcFileName);
	CMDContent["Registry.CommandLine"] = UnicodeToUTF8(pRegEvent->stProcInfo.ProcCMDLine);
	CMDContent["Registry.User"] = UnicodeToUTF8(pRegEvent->stProcUserInfo.ProcUserName);
	CMDContent["Registry.UserSid"] = UnicodeToUTF8(pRegEvent->stProcUserInfo.ProcUserSid);
	CMDContent["Registry.TerminalSessionId"] = (Json::Int64)pRegEvent->stProcUserInfo.ulTerminalSessionId;

	CMDContent["Registry.KeyPath"] = UnicodeToUTF8(pRegEvent->stRegInfo.RegKeyPath);
	CMDContent["Registry.TargetKeyPath"] = UnicodeToUTF8(pRegEvent->stRegInfo.RegTargetKeyPath);
	CMDContent["Registry.ValueName"] = UnicodeToUTF8(pRegEvent->stRegInfo.RegValueName);
	CMDContent["Registry.Value"] = UnicodeToUTF8(pRegEvent->stRegInfo.RegValue);
	CMDContent["Registry.FileVersion"] = UnicodeToUTF8(pRegEvent->stProcFileInfo.FileVersion);
	CMDContent["Registry.Description"] = UnicodeToUTF8(pRegEvent->stProcFileInfo.Description);
	CMDContent["Registry.Product"] = UnicodeToUTF8(pRegEvent->stProcFileInfo.Product);
	CMDContent["Registry.Company"] = UnicodeToUTF8(pRegEvent->stProcFileInfo.Company);
	CMDContent["Registry.OriginalFileName"] = UnicodeToUTF8(pRegEvent->stProcFileInfo.OriginalFileName);

	person["CMDContent"] = CMDContent;
	DWORD dwLanguage = WLUtils::GetLanguageType();
	if( dwLanguage == 0 )
	{
		person["clientLanguage"]= "zh";
	}
	else
	{
		person["clientLanguage"]= "en";
	}


	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;

}
std::string WLSimulateJson::ThreatLog_File_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_FILE pFileEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";

	std::wstring strRand = L"";

	RandString_getRandString(strRand);

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	if (NULL == pFileEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	//person["CMDTYPE"] = (int)cmdType;
	//person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_FILE;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

	CMDContent["FileAccess.Operation"] = pFileEvent->Operation;
	CMDContent["FileAccess.TimeStamp"] = pFileEvent->llTimeStamp;
	CMDContent["FileAccess.ProcessId"] = (Json::Int64)pFileEvent->stProcInfo.ulPid;;
	CMDContent["FileAccess.ProcessGuid"] = UnicodeToUTF8(strRand);//随机化字段
	CMDContent["FileAccess.ProcessName"] = UnicodeToUTF8(pFileEvent->stProcInfo.ProcPath);
	CMDContent["FileAccess.ProcessFileName"] = UnicodeToUTF8(pFileEvent->stProcInfo.ProcFileName);
	CMDContent["FileAccess.CommandLine"] = UnicodeToUTF8(pFileEvent->stProcInfo.ProcCMDLine);
	CMDContent["FileAccess.User"] = UnicodeToUTF8(pFileEvent->stProcUserInfo.ProcUserName);
	CMDContent["FileAccess.UserSid"] = UnicodeToUTF8(pFileEvent->stProcUserInfo.ProcUserSid);
	CMDContent["FileAccess.TerminalSessionId"] = (Json::Int64)pFileEvent->stProcUserInfo.ulTerminalSessionId;

	CMDContent["FileAccess.FileName"] = UnicodeToUTF8(pFileEvent->stFileInfo.FileName);
	CMDContent["FileAccess.FileExtention"] = UnicodeToUTF8(pFileEvent->stFileInfo.FileExtention);
	CMDContent["FileAccess.FilePath"] = UnicodeToUTF8(pFileEvent->stFileInfo.FileFolder);
	CMDContent["FileAccess.FullFileName"] = UnicodeToUTF8(pFileEvent->stFileInfo.FilePath);

	CMDContent["FileAccess.TargetFileName"] = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileName);
	CMDContent["FileAccess.TargetFileExtention"] = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileExtention);
	CMDContent["FileAccess.TargetFilePath"] = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FileFolder);
	CMDContent["FileAccess.TargetFullFileName"] = UnicodeToUTF8(pFileEvent->stTargetFileInfo.FilePath);

	person["CMDContent"] = CMDContent;
	DWORD dwLanguage = WLUtils::GetLanguageType();
	if( dwLanguage == 0 )
	{
		person["clientLanguage"]= "zh";
	}
	else
	{
		person["clientLanguage"]= "en";
	}

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;

}
std::string WLSimulateJson::ThreatLog_Proc_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_PROC pProcEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";

	std::wstring strRand = L"";

	RandString_getRandString(strRand);

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	if (NULL == pProcEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	//person["CMDTYPE"] = (int)cmdType;
	//person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_PROC;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

	CMDContent["ProcessAccess.TimeStamp"] = pProcEvent->llTimeStamp;
	CMDContent["ProcessAccess.SourceProcessId"] = (Json::Int64)pProcEvent->stProcInfo.ulPid;
	CMDContent["ProcessAccess.SourceProcessGuid"] = UnicodeToUTF8(strRand); //随机化的字段
	CMDContent["ProcessAccess.SourceProcessFileName"] = UnicodeToUTF8(pProcEvent->stProcInfo.ProcFileName);
	CMDContent["ProcessAccess.SourceProcessName"] = UnicodeToUTF8(pProcEvent->stProcInfo.ProcPath);
	CMDContent["ProcessAccess.SourceCommandLine"] = UnicodeToUTF8(pProcEvent->stProcInfo.ProcCMDLine);
	CMDContent["ProcessAccess.SourceUser"] = UnicodeToUTF8(pProcEvent->stProcUserInfo.ProcUserName);
	CMDContent["ProcessAccess.SourceUserSid"] = UnicodeToUTF8(pProcEvent->stProcUserInfo.ProcUserSid);
	//CMDContent["ProcessAccess.SourceTerminalSessionId"] = (Json::Int64)pProcEvent->stProcUserInfo.ulTerminalSessionId;

	CMDContent["ProcessAccess.TargetProcessId"] = (Json::Int64)pProcEvent->stTargetProcInfo.ulPid;
	CMDContent["ProcessAccess.TargetProcessGuid"] = UnicodeToUTF8(pProcEvent->stTargetProcInfo.ProcGuid);
	CMDContent["ProcessAccess.TargetProcessFileName"] = UnicodeToUTF8(pProcEvent->stTargetProcInfo.ProcFileName);
	CMDContent["ProcessAccess.TargetProcessName"] = UnicodeToUTF8(pProcEvent->stTargetProcInfo.ProcPath);
	CMDContent["ProcessAccess.TargetCommandLine"] = UnicodeToUTF8(pProcEvent->stTargetProcInfo.ProcCMDLine);
	CMDContent["ProcessAccess.TargetUser"] = UnicodeToUTF8(pProcEvent->stTargetProcUserInfo.ProcUserName);
	CMDContent["ProcessAccess.TargetUserSid"] = UnicodeToUTF8(pProcEvent->stTargetProcUserInfo.ProcUserSid);
	//CMDContent["ProcessAccess.TargetTerminalSessionId"] = (Json::Int64)pProcEvent->stTargetProcUserInfo.ulTerminalSessionId;

	CMDContent["ProcessAccess.ProcessCreateThread"] = (BOOLEAN)pProcEvent->stProcAccess.bCreateThread;
	CMDContent["ProcessAccess.ProcessCreateProcess"] = (BOOLEAN)pProcEvent->stProcAccess.bCreateProcess;
	CMDContent["ProcessAccess.ProcessVmOperation"] = (BOOLEAN)pProcEvent->stProcAccess.bVmOperation;  
	CMDContent["ProcessAccess.ProcessVmRead"] = (BOOLEAN)pProcEvent->stProcAccess.bVmRead;
	CMDContent["ProcessAccess.ProcessVmWrite"] = (BOOLEAN)pProcEvent->stProcAccess.bVmWrite;  
	CMDContent["ProcessAccess.ProcessTerminate"] = (BOOLEAN)pProcEvent->stProcAccess.bTerminate;
	CMDContent["ProcessAccess.ProcessSuspendResume"] = (BOOLEAN)pProcEvent->stProcAccess.bSuspendResume;
	CMDContent["ProcessAccess.ProcessDupHandle"] = (BOOLEAN)pProcEvent->stProcAccess.bDupHandle;
	//CMDContent["ProcessAccess.ProcessQueryInformation"] = (BOOLEAN)pProcEvent->stProcAccess.bQueryInformation;
	//CMDContent["ProcessAccess.ProcessQueryLimitedInformation"] = (BOOLEAN)pProcEvent->stProcAccess.bQueryLimitedInformation;
	CMDContent["ProcessAccess.Synchronize"] = (BOOLEAN)pProcEvent->stProcAccess.bSynchronize;
	CMDContent["ProcessAccess.ProcessSetInfomation"] = (BOOLEAN)pProcEvent->stProcAccess.bSetInfomation;
	CMDContent["ProcessAccess.ProcessSetQuota"] = (BOOLEAN)pProcEvent->stProcAccess.bSetQuota;
	CMDContent["ProcessAccess.ProcessSetLimitedInfomation"] = (BOOLEAN)pProcEvent->stProcAccess.bSetLimitedInfomation;

	person["CMDContent"] = CMDContent;
	DWORD dwLanguage = WLUtils::GetLanguageType();
	if( dwLanguage == 0 )
	{
		person["clientLanguage"]= "zh";
	}
	else
	{
		person["clientLanguage"]= "en";
	}

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;  

}
std::string WLSimulateJson::ThreatLog_WinEvent_GetJson_HBP(__in tstring ComputerID, __in PTHREAT_EVENT_WINEVENT pWinEvent,__in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";

	std::wstring strRand = L"";

	RandString_getRandString(strRand);

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person,CMDContent;

	if (NULL == pWinEvent)
	{
		return sJsonPacket;
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	//person["CMDTYPE"] = (int)cmdType;
	//person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);

	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_SYSTEM;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

	// person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;  需要保留 CMDID ?

	CMDContent["FilterRTID"] = UnicodeToUTF8(pWinEvent->FilterRTID);
	CMDContent["factoryid"] = UnicodeToUTF8(pWinEvent->factoryid);  
	CMDContent["Keywords"] = UnicodeToUTF8(pWinEvent->Keywords);
	CMDContent["assetIp"] = UnicodeToUTF8(pWinEvent->assetIp);
	CMDContent["Level"] = UnicodeToUTF8(pWinEvent->Level); 
	CMDContent["ProviderName"] = UnicodeToUTF8(pWinEvent->ProviderName);
	CMDContent["ThreadID"] = UnicodeToUTF8(pWinEvent->ThreadID); 
	CMDContent["SourceAddress"] = UnicodeToUTF8(pWinEvent->SourceAddress);
	CMDContent["ProviderGuid"] = UnicodeToUTF8(strRand); //随机化的字段
	CMDContent["Application"] = UnicodeToUTF8(pWinEvent->Application);
	CMDContent["RemoteMachineID"] = UnicodeToUTF8(pWinEvent->RemoteMachineID); 
	CMDContent["Task"] = UnicodeToUTF8(pWinEvent->Task);
	CMDContent["Version"] = UnicodeToUTF8(pWinEvent->Version); 
	CMDContent["DestPort"] = UnicodeToUTF8(pWinEvent->DestPort);
	CMDContent["clientip"] = UnicodeToUTF8(pWinEvent->clientip); 
	CMDContent["SystemTime"] = UnicodeToUTF8(pWinEvent->SystemTime);
	CMDContent["Channel"] = UnicodeToUTF8(pWinEvent->Channel); 
	CMDContent["LayerRTID"] = UnicodeToUTF8(pWinEvent->LayerRTID);
	CMDContent["TimeCreated"] = UnicodeToUTF8(pWinEvent->TimeCreated); 
	CMDContent["EventRecordID"] = UnicodeToUTF8(pWinEvent->EventRecordID);
	CMDContent["caffectedip"] = UnicodeToUTF8(pWinEvent->caffectedip); 
	CMDContent["SourcePort"] = UnicodeToUTF8(pWinEvent->SourcePort);
	CMDContent["Direction"] = UnicodeToUTF8(pWinEvent->Direction); 
	CMDContent["EventID"] = UnicodeToUTF8(pWinEvent->EventID);
	CMDContent["ComputerID"] = UnicodeToUTF8(pWinEvent->ComputerID); 
	CMDContent["DestAddress"] = UnicodeToUTF8(pWinEvent->DestAddress);
	CMDContent["System"] = UnicodeToUTF8(pWinEvent->System); 
	CMDContent["LayerName"] = UnicodeToUTF8(pWinEvent->LayerName);
	CMDContent["RemoteUserID"] = UnicodeToUTF8(pWinEvent->RemoteUserID); 
	CMDContent["Computer"] = UnicodeToUTF8(pWinEvent->Computer);
	CMDContent["Protocol"] = UnicodeToUTF8(pWinEvent->Protocol); 
	CMDContent["Opcode"] = UnicodeToUTF8(pWinEvent->Opcode); 
	CMDContent["ProcessID"] = UnicodeToUTF8(pWinEvent->ProcessID);
	CMDContent["threat"] = (BOOLEAN)pWinEvent->threat;

	person["CMDContent"] = CMDContent;
	DWORD dwLanguage = WLUtils::GetLanguageType();
	if( dwLanguage == 0 )
	{
		person["clientLanguage"]= "zh";
	}
	else
	{
		person["clientLanguage"]= "en";
	}

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}