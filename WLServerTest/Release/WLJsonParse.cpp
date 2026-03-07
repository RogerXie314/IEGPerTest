#include "StdAfx.h"
#include <time.h>
#include <atlconv.h>
#include "../include/format/license.h"
#include "../include/public_def.h"
#include "../include/RemoteUpgrade_def.h"
#include "../include/CmdWord/WLCmdWordDef.h"
#include "../include/WLUtilities/base64.h"
#include "../include/WLUtilities/wntMD5.h"
#include "../include/WLUtilities/json/json.h"
#include "../include/WLUtilities/GetWindowsVersion.h"
#include "../WLBaseModule/WLMetaData.h"
#include "../WLCConfig/WLOSUserManage.h"
#include "../include/WLUtilities/WLJsonParse.h"
//#include "../include/wlSecMod.h"
#include "..\include\wlMiniSecMod.h"
#include "../include/WLCredentialMgr/CredentialStruct.h"
#include "..\include\WLMessageSend\WLClientMessageSender.h"
#include "../include/WLUtilities/StrUtil.h"
#include "../include/WLHostReinforcement/HostDefenceStruct.h"
#include "../include/WLUsbDeviceStruct.h"
#include "../WLHostBaseLine/baseline.h"
#include "../include/WLProcess/WorkSheet.h"
#include <shlwapi.h>
#include "../include/WLProcess/SafeFileCopy.h"
#include <ctime>

#include "../include/defs.h"

#pragma comment(lib, "shlwapi.lib")

#define  UPLOAD_PE_MAX      5  //上传PE的最大值，以M为单位

#define  TYPE_TRUST_PROCESS          "TrustProcess"
#define  TYPE_TRUST_PATH	         "TrustPath"

#define  TYPE_UPGRADE_STATUS_RESULT_SUC     "SUC"
#define  TYPE_UPGRADE_STATUS_RESULT_FAIL    "FAIL"
/////////////////////////////////////////////JSON字段定义：begin///////////////////////////


//防火墙配置协议字段：begin
#define PLY_FW_STRKEY_FIREWALL				"FireWall"
#define PLY_FW_STRKEY_FIREWALLSTATE		"FireWallState"
#define PLY_FW_STRKEY_SYNIPDEFENCE			"SynDefence"
#define PLY_FW_STRKEY_DEFAULT_IN			"FWDefaultIn"
#define PLY_FW_STRKEY_DEFAULT_OUT			"FWDefaultOut"
#define PLY_FW_STRKEY_CONFIGTYPE			"ConfigType"


#define PLY_FW_STRKEY_CMDID				    "CMDID"
#define PLY_FW_STRKEY_CMDTYPE				"CMDTYPE"
#define PLY_FW_STRKEY_COMPUTER_ID			"ComputerID"
#define PLY_FW_STRKEY_CMDCONTENT            "CMDContent"    

#define PLY_FW_STRKEY_ARRAY_FWITEMS		"FwItems"
#define PLY_FW_STRKEY_ITEM_RULENAME		"RuleName"
#define PLY_FW_STRKEY_ITEM_FULLPATH		"FullPath"
#define PLY_FW_STRKEY_ITEM_LOCALIP			"LocalIP"
#define PLY_FW_STRKEY_ITEM_REMOTEIP		"RemoteIP"
#define PLY_FW_STRKEY_ITEM_LOCALPORT		"LocalPort"
#define PLY_FW_STRKEY_ITEM_REMOTEPORT		"RemotePort"
#define PLY_FW_STRKEY_ITEM_PROTOCALTYPE	"ProtocalType"
#define PLY_FW_STRKEY_ITEM_OPERATION		"Operation"
#define PLY_FW_STRKEY_ITEM_DIRECTION		"Direction"
#define PLY_FW_STRKEY_ITEM_TYPE_4_XP		"RuleType"
#define PLY_FW_STRKEY_ITEM_TYPE_4_XP_APP	0
#define PLY_FW_STRKEY_ITEM_TYPE_4_XP_PORT	1
//#define PLY_FW_VALUE_TYPE_CUSTOM			0						//用户定义规则
//#define PLY_FW_VALUE_TYPE_WNT				1						//主机卫士规则
//防火墙配置协议字段：end


//白名单扫描配置字段：start
#define WLSC_KEY_SCANTYPE	"Type"
#define WLSC_KEY_SCANPATH	"Path"
#define WLSC_KEY_SCANSPEED	"Speed"
#define WLSC_KEY_PLYORIGIN	"Origin"
//白名单扫描配置字段：end

#define FETCH_PST_KEY_OSTYPE    "OsType"
#define FETCH_PST_KEY_OSVERSION "OsVersion"
#define FETCH_PST_KEY_OSARCH    "Arch"
#define FETCH_PST_KEY_URL       "URL"
#define FETCH_PST_KEY_MSG       "MSG"

#define EVENT_ID_TAB_BEYOND     L"1000000"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(X)    ((int)(sizeof(X)/sizeof(X[0])))
#endif

/////////////////////////////////////////////JSON字段定义：end////////////////////////////


CWLJsonParse::CWLJsonParse(void)
{
}

CWLJsonParse::~CWLJsonParse(void)
{
}


//宽字节转窄字节
std::string CWLJsonParse::ConvertW2A(const std::wstring &wstr)
{
	setlocale(LC_ALL, ".936");
	size_t nSize = wstr.length() * 2 + 1;
	char *psz = new char[nSize];

	memset(psz, 0, nSize);
	wcstombs(psz, wstr.c_str(), nSize);
	std::string str = psz;
	delete []psz;
	return str;
}


//窄字节转宽字节
std::wstring CWLJsonParse::ConvertA2W(const std::string &str)
{
	setlocale(LC_ALL, ".936");
	size_t nSize = str.length() + 1;
	wchar_t *wpsz = new wchar_t[nSize];

	memset(wpsz, 0, sizeof(wchar_t)*nSize);
	mbstowcs(wpsz, str.c_str(), nSize);
	std::wstring wstr = wpsz;
	delete []wpsz;
	return wstr;
}


std::string CWLJsonParse::UnicodeToUTF8(const std::wstring &str)
{
	int nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
	CHAR* szUtf8 = new CHAR[nLen + 1];
	memset(szUtf8, 0, nLen + 1);
	nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, szUtf8, nLen + 1, NULL, NULL);
	std::string strUtf8 = szUtf8;
	delete []szUtf8;

	return strUtf8;
}


std::wstring CWLJsonParse::UTF8ToUnicode(const std::string &szAnsi )
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

void CWLJsonParse::DWORD_HightToLower(DWORD& dw)
{
	byte  tmp = 0;
	byte* p= (byte*)&dw;

	tmp	 = p[0];
	p[0] = p[3];
	p[3] = tmp;

	tmp = p[1];
	p[1] = p[2];
	p[2] = tmp;
}


std::wstring  CWLJsonParse::convertTimeTToStr (const time_t &time)
{
	std::wstring  wsTime;
	struct tm *tm_time = ::localtime (&time);//把UTC时间转成本地时间。所以在数据库中中保存的时间都是utc时间。
	TCHAR szTime[1024] = {0};
	_stprintf (szTime, _T("%4d-%02d-%02d %02d:%02d:%02d"), tm_time->tm_year + 1900,
		tm_time->tm_mon + 1, tm_time->tm_mday, tm_time->tm_hour,
		tm_time->tm_min, tm_time->tm_sec);
	wsTime = szTime;

	return wsTime;
}

/* time_t 转成string字符串 chennian 20181111 */
std::string  CWLJsonParse::convertTime2Str (const time_t &time)
{
    std::string  strTime;
    struct tm *tm_time = ::localtime (&time);
    char szTime[128] = {0};
    _snprintf (szTime, 128, "%4d-%02d-%02d %02d:%02d:%02d", tm_time->tm_year + 1900,
                tm_time->tm_mon + 1, tm_time->tm_mday, tm_time->tm_hour,
                tm_time->tm_min, tm_time->tm_sec);
    strTime = szTime;

    return strTime;
}
BOOL CWLJsonParse::convertStr2Tm (struct tm &_tm, const std::string &strTime)
{
	//struct tm tm;
	int year, month, day, hour, minute, second;
	if (::sscanf_s(strTime.c_str (), "%4d-%02d-%02d %02d:%02d:%02d", &year,
		&month, &day, &hour, &minute, &second) != 6)
	{
		//TRACE (_T("字符串%s不是标准的时间字符串\r\n"), strTime.c_str ());
		return FALSE;
	}
	_tm.tm_year = year - 1900;
	_tm.tm_mon = month - 1;
	_tm.tm_mday = day;
	_tm.tm_hour = hour;
	_tm.tm_min = minute;
	_tm.tm_sec = second;

	return TRUE;
}

BOOL CWLJsonParse::convertStr2Time (time_t &time, const std::string &strTime)
{
	struct tm tm;
	int year, month, day, hour, minute, second;
	if (::sscanf_s(strTime.c_str (), "%4d-%02d-%02d %02d:%02d:%02d", &year,
		&month, &day, &hour, &minute, &second) != 6)
	{
		//TRACE (_T("字符串%s不是标准的时间字符串\r\n"), strTime.c_str ());
		return TRUE;
	}
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	time = ::mktime (&tm);
	return TRUE;
}


BOOL CWLJsonParse::convertStrToTimeT (time_t &time, const std::wstring &strTime)
{
	struct tm tm;
	int year, month, day, hour, minute, second;
	if (::_stscanf (strTime.c_str (), _T("%4d-%02d-%02d %02d:%02d:%02d"), &year,
		&month, &day, &hour, &minute, &second) != 6)
	{
		//TRACE (_T("字符串%s不是标准的时间字符串\r\n"), strTime.c_str ());
		return TRUE;
	}
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	time = ::mktime (&tm);
	return TRUE;
}


BOOL CWLJsonParse::Base64_Decode(__in std::string sEncrypt, __out char* szDecodeBuf)
{
	CBase64 base64;

	int nLen = (int)sEncrypt.length();

	char *lpSrc  = new char[nLen + 1];
	char *lpDest = new char[nLen + 1];
	if( !lpSrc || !lpDest)
	{
		if(lpSrc)
			delete[] lpSrc;

		if( lpDest)
			delete[] lpDest;

		return FALSE;
	}

	memset(lpSrc, 0, nLen +1);
	memset(lpDest, 0, nLen +1);

	memcpy(lpSrc, sEncrypt.c_str(), nLen);

	base64.Base64Decode(lpSrc, nLen, lpDest, nLen);

	memcpy( szDecodeBuf, lpDest, nLen);


	if(lpSrc)
		delete[] lpSrc;


	if( lpDest)
		delete[] lpDest;


	return TRUE;
}

BOOL CWLJsonParse::Base64_Encode(__in char* szOrginBuf, __in int nLenBuf, __out std::string& sEncypt)
{
	CBase64 base64;

	int nDest = (nLenBuf + 2)/3 * 4 + 1;
	char *lpDest = new char[nDest];
	if(  !lpDest)
	{
		if( lpDest)
			delete[] lpDest;

		return FALSE;
	}

	memset(lpDest, 0, nDest);

	char* p = (char*)base64.Base64Encode(szOrginBuf, nLenBuf, lpDest, nDest);

	sEncypt = lpDest;


	if( lpDest)
		delete[] lpDest;


	return TRUE;
}

#define JSON_DEFAULT_STRING		"-"
std::string CWLJsonParse::WarningLog_GetJsonByVector(
                    __in tstring ComputerID, __in WORD cmdType , 
                    __in WORD cmdID, __in vector<WARNING_LOG_STRUCT*>& vecWarningLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecWarningLog.size();
	if( nCount == 0)
		return "";

	Json::Value root1;
	Json::Value root2;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	for (int i=0; i< nCount; i++)
	{
		WARNING_LOG_STRUCT *pWarningLog = vecWarningLog[i];

		std::wstring wsTemp = convertTimeTToStr((DWORD)pWarningLog->llTime);
		CMDContent["Time"] = UnicodeToUTF8(wsTemp);
		CMDContent["HoldBack"] = (int)pWarningLog->bHoldback;
		CMDContent["IntegrityCheck"] = (int)pWarningLog->bIntegrityCheckFailed;
		CMDContent["CertCheck"] = (int)pWarningLog->bCertCheckFailed;
        CMDContent["Type"] = (int)pWarningLog->nSubType;

		wsTemp = pWarningLog->szFullPath;
		CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);

		wsTemp = pWarningLog->szParentProcess;
		if( wsTemp.length() > 0)
			CMDContent["ParentProcess"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["ParentProcess"] = JSON_DEFAULT_STRING;


		wsTemp = pWarningLog->szCompany;
		if( wsTemp.length()>0)
			CMDContent["CompanyName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["CompanyName"] = JSON_DEFAULT_STRING;


		wsTemp = pWarningLog->szProduct;
		if( wsTemp.length() > 0)
			CMDContent["ProductName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["ProductName"] = JSON_DEFAULT_STRING ;



		wsTemp = pWarningLog->szVersion;
		if( wsTemp.length() > 0)
			CMDContent["Version"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["Version"] = JSON_DEFAULT_STRING;

		wsTemp = pWarningLog->szLogon;
		if( wsTemp.length() > 0)
			CMDContent["UserName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["UserName"] = JSON_DEFAULT_STRING;

		//szHash
		wsTemp = pWarningLog->szIntegrity;
		if (wsTemp.length() > 0)
		{
			CMDContent["Hash"] = UnicodeToUTF8(wsTemp);
		}
		else
			CMDContent["Hash"] = JSON_DEFAULT_STRING;

        //szDefHash
        wsTemp = pWarningLog->szDefIntegrity;
        if (wsTemp.length() > 0)
        {
            CMDContent["IEGHash"] = UnicodeToUTF8(wsTemp);
        }
        else
            CMDContent["IEGHash"] = JSON_DEFAULT_STRING;


		root1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = Json::Value(root1);

	root2.append(person);
	writer.omitEndingLineFeed();
	sJsonPacket = writer.write(root2);
	root2.clear();


	return sJsonPacket;
}

std::string CWLJsonParse::WarningLog_GetJsonByVector(
        __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
        __in std::vector<CWLMetaData*>& vecWarningLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecWarningLog.size();
	if( nCount == 0)
		return "";

	Json::Value root1;
	Json::Value root2;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	for (int i=0; i< nCount; i++)
	{
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecWarningLog[i]->GetData();
		WARNING_LOG_STRUCT* pWarningLog = (WARNING_LOG_STRUCT*)pipclogcomm->data;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pWarningLog->llTime);
		CMDContent["Time"] = UnicodeToUTF8(wsTemp);
		CMDContent["HoldBack"] = (int)pWarningLog->bHoldback;
		CMDContent["IntegrityCheck"] = (int)pWarningLog->bIntegrityCheckFailed;
		CMDContent["CertCheck"] = (int)pWarningLog->bCertCheckFailed;
        CMDContent["Type"] = (int)pWarningLog->nSubType;

		wsTemp = pWarningLog->szFullPath;
		CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);

		wsTemp = pWarningLog->szParentProcess;
		if( wsTemp.length() > 0)
			CMDContent["ParentProcess"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["ParentProcess"] = JSON_DEFAULT_STRING;


		wsTemp = pWarningLog->szCompany;
		if( wsTemp.length()>0)
			CMDContent["CompanyName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["CompanyName"] = JSON_DEFAULT_STRING;


		wsTemp = pWarningLog->szProduct;
		if( wsTemp.length() > 0)
			CMDContent["ProductName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["ProductName"] = JSON_DEFAULT_STRING ;


		wsTemp = pWarningLog->szVersion;
		if( wsTemp.length() > 0)
			CMDContent["Version"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["Version"] = JSON_DEFAULT_STRING;


		wsTemp = pWarningLog->szLogon;
		if( wsTemp.length() > 0)
			CMDContent["UserName"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["UserName"] = JSON_DEFAULT_STRING;

		//szHash
		wsTemp = pWarningLog->szIntegrity;
		if (wsTemp.length() > 0)
		{
			CMDContent["Hash"] = UnicodeToUTF8(wsTemp);
		}
		else
			CMDContent["Hash"] = JSON_DEFAULT_STRING;

        //szDefHash
        wsTemp = pWarningLog->szDefIntegrity;
        if (wsTemp.length() > 0)
        {
            CMDContent["IEGHash"] = UnicodeToUTF8(wsTemp);
        }
        else
            CMDContent["IEGHash"] = JSON_DEFAULT_STRING;


		root1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = Json::Value(root1);

	root2.append(person);
	writer.omitEndingLineFeed();
	sJsonPacket = writer.write(root2);
	root2.clear();


	return sJsonPacket;
}

BOOL CWLJsonParse::WarningLog_GetLogVectorByJson(__in std::string sJson, __out vector<WARNING_LOG_STRUCT*>& vecWarningLog)
{
	Json::Value root;
	Json::Value CMDContent;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";


	tstring ws = _T("");
	//tstring ws  = UTF8ToUnicode(sJson);
	//strValue = ConvertW2A(ws);
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE ;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;

	if( !root[0].isMember("CMDContent"))
	{
		return FALSE;
	}

	//获取数组内容
	CMDContent = (Json::Value)root[0]["CMDContent"];

	nObject = CMDContent.size();
	if( nObject < 1)
		return FALSE;
	//wwdv3
	if (CMDContent.isArray())
	{
		for(int i=0; i<nObject; ++i)
		{
			WARNING_LOG_STRUCT* pWarningLog = new WARNING_LOG_STRUCT;

			ws = UTF8ToUnicode(CMDContent[i]["Time"].asString());
			time_t t = 0;
			convertStrToTimeT(t, ws);

			pWarningLog->llTime = (LONGLONG)t;
			pWarningLog->bHoldback = (BOOLEAN)(DWORD)root[i]["HoldBack"].asInt();
			pWarningLog->bIntegrityCheckFailed = (DWORD)root[i]["IntegrityCheck"].asInt();
			pWarningLog->bCertCheckFailed = (DWORD)root[i]["CertCheck"].asInt();
			pWarningLog->nSubType = (DWORD)root[i]["Type"].asInt();

			std::string s1 = CMDContent[i]["FullPath"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szFullPath, ws.c_str(), ws.length()* sizeof(TCHAR));

			s1 = CMDContent[i]["FullPath"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szParentProcess, ws.c_str(), ws.length()* sizeof(TCHAR));


			s1 = CMDContent[i]["CompanyName"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szCompany, ws.c_str(), ws.length()* sizeof(TCHAR));


			s1 = CMDContent[i]["ProductName"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szProduct, ws.c_str(), ws.length()* sizeof(TCHAR));


			s1 = CMDContent[i]["Version"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szVersion, ws.c_str(), ws.length()* sizeof(TCHAR));

			s1 = CMDContent[i]["UserName"].asString();
			ws = UTF8ToUnicode(s1);
			memcpy(pWarningLog->szLogon, ws.c_str(), ws.length()* sizeof(TCHAR));

			vecWarningLog.push_back(pWarningLog);
		}
	}
	

	return TRUE;
}

std::string CWLJsonParse::UserActionLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<__ADMIN_OPERATION_LOG_STRUCT*>& vecUserActionLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUserActionLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root1;
	Json::Value root2;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	for (int i=0; i< nCount; i++)
	{
		__ADMIN_OPERATION_LOG_STRUCT *pUserActionLog = vecUserActionLog[i];

		std::wstring wsTemp = _T("");

		wsTemp = convertTimeTToStr((DWORD)pUserActionLog->llTime);
		CMDContent["Time"] =  UnicodeToUTF8(wsTemp);

		wsTemp = pUserActionLog->szUserName;
		CMDContent["UserName"] = UnicodeToUTF8(wsTemp);

		wsTemp = pUserActionLog->szLogContent;

		if( wsTemp.length()> 0)
			CMDContent["LogContent"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["LogContent"]=JSON_DEFAULT_STRING;

		CMDContent["dwIsSuccess"]=(int)pUserActionLog->dwIsSuccess;

		root1.append(CMDContent);
	}


	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root2.append(person);
	sJsonPacket = writer.write(root2);
	root2.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::UserActionLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData*>& vecUserActionLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUserActionLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root1;
	Json::Value root2;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	for (int i=0; i< nCount; i++)
	{
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecUserActionLog[i]->GetData();
		__ADMIN_OPERATION_LOG_STRUCT *pUserActionLog = (__ADMIN_OPERATION_LOG_STRUCT*)pipclogcomm->data;


		std::wstring wsTemp = _T("");

		wsTemp = convertTimeTToStr((DWORD)pUserActionLog->llTime);
		CMDContent["Time"] =  UnicodeToUTF8(wsTemp);

		wsTemp = pUserActionLog->szUserName;
		CMDContent["UserName"] = UnicodeToUTF8(wsTemp);

		wsTemp = pUserActionLog->szLogContent;

		if( wsTemp.length()> 0)
			CMDContent["LogContent"] = UnicodeToUTF8(wsTemp);
		else
			CMDContent["LogContent"]=JSON_DEFAULT_STRING;

		CMDContent["dwIsSuccess"]=(int)pUserActionLog->dwIsSuccess;
		root1.append(CMDContent);
	}


	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root2.append(person);
	sJsonPacket = writer.write(root2);
	root2.clear();

	return sJsonPacket;
}


BOOL CWLJsonParse::UserActionLog_GetLogVectorByJson(__in std::string sJson, __out vector<__ADMIN_OPERATION_LOG_STRUCT*>& vecUserActionLog)
{
	Json::Value root;
	Json::Value CMDContent;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";


	tstring ws = _T("");
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE ;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;


	if( !root[0].isMember("CMDContent"))
	{
		return FALSE;
	}

	//获取数组内容
	CMDContent = (Json::Value)root[0]["CMDContent"];

	nObject = CMDContent.size();
	if( nObject < 1)
		return FALSE;
	//wwdv3
	if (CMDContent.isArray())
	{
		for(int i=0; i<nObject; ++i)
		{
			__ADMIN_OPERATION_LOG_STRUCT* pUserAction = new __ADMIN_OPERATION_LOG_STRUCT;
			memset( pUserAction, 0, sizeof(__ADMIN_OPERATION_LOG_STRUCT));

			ws = UTF8ToUnicode(CMDContent[i]["Time"].asString());
			time_t t = 0;
			convertStrToTimeT(t, ws);

			pUserAction->llTime = (LONGLONG)t;

			ws = UTF8ToUnicode(CMDContent[i]["UserName"].asString());
			memcpy( pUserAction->szLogContent, ws.c_str(), ws.length()* sizeof(TCHAR));

			ws = UTF8ToUnicode(CMDContent[i]["LogContent"].asString());
			memcpy( pUserAction->szLogContent, ws.c_str(), ws.length()* sizeof(TCHAR));

			vecUserActionLog.push_back(pUserAction);
		}
	}
	return TRUE;
}


/*
“CMDContent”:[
{
“Time”:”2015-07-02 17:45:30”,
"UDiskType":1, //0 - 未知U盘，1-普通U盘，2-v2.0安全U盘; 3-V3.0U盘。

“serialID”:”abc123XX”,
“FullPath": “F:\\Udiskfilepath\a.txt”,
“ProcessNmae”:"c:\\windows\\system32\\regedit.exe",
“Username”:"Administrator",
“OperationContent”: “1”,	// 操作内容（删除或修改）文件（1：删除2：重命名3：修改 4：读;5是新建文件 6:执行）；
},
]

clientUSBLog.do
----------------------------------
*/
std::string CWLJsonParse::UsbDiskWarningLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<CWLMetaData*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;

	for (int i = 0; i< nCount; i++)
	{
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecUsbLog[i]->GetData();
		UDISK_LOG_STRUCT *pUsbLog = (UDISK_LOG_STRUCT*)pipclogcomm->data;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
		person["Time"] =  UnicodeToUTF8(wsTemp);
		

		wsTemp = pUsbLog->szlogon;
		person["UserName"] = UnicodeToUTF8(wsTemp);

        //V3R2新追加字段
        person["UDiskType"] = (int)pUsbLog->dwUDiskType;

        wsTemp = pUsbLog->szSerial;
        person["serialID"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szFullPath;
        person["FullPath"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szProcessName;
        person["ProcessName"] = UnicodeToUTF8(wsTemp);

        person["OperationContent"] = (int)pUsbLog->dwOptContent;

        person["Block"] = (int)pUsbLog->dwBlock;
       
		CMDContent.append(person);
		person.clear();
	}


	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDVER"] = 2;   //区别与旧版本的上报告警日志JSON规格
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}

std::string CWLJsonParse::UsbDiskWarningLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<UDISK_LOG_STRUCT*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;


	for (int i=0; i< nCount; i++)
	{
		UDISK_LOG_STRUCT *pUsbLog = vecUsbLog[i] ;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
		person["Time"] =  UnicodeToUTF8(wsTemp);

		wsTemp = pUsbLog->szlogon;
		person["UserName"] = UnicodeToUTF8(wsTemp);

        //V3R2新追加字段
        person["UDiskType"] = (int)pUsbLog->dwUDiskType;

        wsTemp = pUsbLog->szSerial;
        person["serialID"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szFullPath;
        person["FullPath"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szProcessName;
        person["ProcessName"] = UnicodeToUTF8(wsTemp);

        person["OperationContent"] = (int)pUsbLog->dwOptContent;

        person["Block"] = (int)pUsbLog->dwBlock;

		CMDContent.append(person);
		person.clear();
	}


	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDVER"] = 2;   //区别与旧版本的上报告警日志JSON规格
	UsbLogJson["CMDContent"] = (Json::Value)CMDContent;

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}
/*
CMDContent”:[
{
“Time”:”2015-07-02 17:45:30”,
"UDiskType":1, //0 - 未知U盘，1-普通U盘，2-v2.0安全U盘; 3-V3.0U盘。

“serialID”:”abc123XX”,
"plugEvent"：1,             //1:插入,0:拔出;2：linux的umount和windows的弹出。
    	"serialID":"12324",           //唯一的ID号。    
    	"registerStatus":0,           //0未注册，1表示已经注册；
“secureUdiskUmountArea”:{“public”:”f:\\”, }//windows 安全U盘弹出public区。

“secureUdiskUmountArea”:{ “secure”:”K:\\”}//windows 安全U盘弹出secure区。

“secureUdiskUmountArea”:{“public”:”/mnt/public”, “secure”:”//mnt/secure”}//linux的格式。
"CommUdiskUmountArea": {//普通U盘在linux系统下umount.信息
		"/dev/sdc1": "/mnt/udisk / sdc1"
	}
"DiskDriverLetter":["E:\","K:\","F:\"]  //盘符，驱动器号

},
]
hotplugDevLog.do

*/

std::string CWLJsonParse::NetAdapterStatus_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<NETWORK_ADAPTER_STATUS*>& vecNetStatus)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecNetStatus.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;
    Json::Value CMDUsbContent;
    Json::Value CMDContentOtherDev;
    
    UsbLogJson["CMDVER"] = 4;   //4.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于其他外设的插拔告警，比3.0版本新增网卡事件

    for (int i=0; i< nCount; i++)
    {
        NETWORK_ADAPTER_STATUS *pNetStatus = vecNetStatus[i] ;

        //  组装其他设备插入信息上传
        std::wstring wsTemp = convertTimeTToStr((DWORD)pNetStatus->llTime);
        person["Time"] =  UnicodeToUTF8(wsTemp);
        
        person["Name"] = UnicodeToUTF8(pNetStatus->Name);
        
        person["IP"] = UnicodeToUTF8(pNetStatus->IPAddress);
        
        person["Mac"] = UnicodeToUTF8(pNetStatus->MacAddress);
        
        person["PlugEvent"] = (int)pNetStatus->dwPlugEvent;
        
        person["OtherDevType"] = (int)7;

        CMDContentOtherDev.append(person);
        person.clear();
    }


    UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    UsbLogJson["CMDTYPE"] = (int)cmdType;
    UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;
    UsbLogJson["CMDUsbContent"] = (Json::Value)CMDUsbContent;
    UsbLogJson["CMDContentOtherDev"] = (Json::Value)CMDContentOtherDev;


    root.append(UsbLogJson);
    sJsonPacket = writer.write(root);
    root.clear();


    //wsJsonPacket = ConvertA2W(sJsonPacket);
    //sJsonPacket = UnicodeToUTF8(ws);
    //return sJsonPacket;

    //std::wstring wsJsonPacket = _T("");
    //wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

    return sJsonPacket;
}
    
std::string CWLJsonParse::UsbDiskPlugLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<UDISK_LOG_STRUCT*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;
	Json::Value CMDUsbContent;
    Json::Value CMDContentOtherDev;

	for (int i=0; i< nCount; i++)
	{
		UDISK_LOG_STRUCT *pUsbLog = vecUsbLog[i] ;
		
		if (pUsbLog->dwLogType == UDISK_LOG_TYPE_USBPORT_INSERT)
		{
			UsbLogJson["CMDVER"] = 2;   //2.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于USB接口的插拔告警

			//  组装USB设备插入信息上传
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcPID;
			person["PID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVID;
			person["VID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcProductDesc;
			person["ProductDesc"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVendorDesc;
			person["VendorDesc"] = UnicodeToUTF8(wsTemp);
			person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
			person["USBClass"] = (int)4;  // pUsbLog->wcUSBClass;
			CMDUsbContent.append(person);
			person.clear();
		}
        else if (pUsbLog->dwLogType == UDISK_LOG_TYPE_OTHER_DEVICE_INSERT_REMOVE)
        {
            if (DEV_TYPE_NET == pUsbLog->dwUDiskType)
            {
                UsbLogJson["CMDVER"] = 4;   //4.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于其他外设的插拔告警，比3.0版本新增网卡事件

                //  组装其他设备插入信息上传
                std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
                person["Time"] =  UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcOtherDevName;
                person["Name"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcProductDesc;//用wcProductDesc字段保存IP
                person["IP"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVendorDesc;//用wcVendorDesc字段保存Mac
                person["Mac"] = UnicodeToUTF8(wsTemp);
                person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
                person["OtherDevType"] = 7;//7在与USM接口中表示网口事件(接口文档16.9)
            }
            else
            {
                UsbLogJson["CMDVER"] = 3;   //3.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于其他外设的插拔告警

                //  组装其他设备插入信息上传
                std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
                person["Time"] =  UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcOtherDevName;
                person["Name"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcPID;
                person["PID"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVID;
                person["VID"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcProductDesc;
                person["ProductDesc"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVendorDesc;
                person["VendorDesc"] = UnicodeToUTF8(wsTemp);
                person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
                person["OtherDevType"] = (int)pUsbLog->dwUDiskType;
            }

            CMDContentOtherDev.append(person);
            person.clear();
        }
		else
		{
		    UsbLogJson["CMDVER"] = 1; //表示安全U盘、普通U盘、移动硬盘的插拔
		    
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);

			//wsTemp = pUsbLog->szlogon;
			//person["UserName"] = UnicodeToUTF8(wsTemp);

			//V3R2新追加字段
			person["UDiskType"] = (int)pUsbLog->dwUDiskType;//0 - 未知U盘，1-普通U盘，2-v2.0安全U盘; 3-V3.0U盘。

			wsTemp = pUsbLog->szSerial;
			person["serialID"] = UnicodeToUTF8(wsTemp);

			person["registerStatus"] = (int)pUsbLog->dwBlock>>16;

			wsTemp = pUsbLog->szFullPath;//"E:\ K:\ T:\ "
			person["DiskDriverLetter"].append(UnicodeToUTF8(wsTemp));
			// person["DiskDriverLetter"] = UnicodeToUTF8(wsTemp);//"DiskDriverLetter":["E:\","K:\","F:\"] 

			person["plugEvent"] = (int)pUsbLog->dwBlock & 0xF ;




			CMDContent.append(person);
			person.clear();
		}
	}


	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
	UsbLogJson["CMDContent"] = (Json::Value)CMDContent;
	UsbLogJson["CMDUsbContent"] = (Json::Value)CMDUsbContent;
    UsbLogJson["CMDContentOtherDev"] = (Json::Value)CMDContentOtherDev;
	

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}

// 设备插拔告警
std::string CWLJsonParse::UsbDiskPlugLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<CWLMetaData*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;
	Json::Value CMDUsbContent;
    Json::Value CMDContentOtherDev;


	for (int i=0; i< nCount; i++)
	{
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecUsbLog[i]->GetData();
		UDISK_LOG_STRUCT *pUsbLog = (UDISK_LOG_STRUCT*)pipclogcomm->data;

		if (pUsbLog->dwLogType == UDISK_LOG_TYPE_USBPORT_INSERT)
		{
			UsbLogJson["CMDVER"] = 2;   //区别与旧版本的上报告警日志JSON规格

			//  组装USB设备插入信息上传
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcPID;
			person["PID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVID;
			person["VID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcProductDesc;
			person["ProductDesc"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVendorDesc;
			person["VendorDesc"] = UnicodeToUTF8(wsTemp);
			person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
			person["USBClass"] = (int)4;  // pUsbLog->wcUSBClass;
			CMDUsbContent.append(person);
			person.clear();
		}
        else if (pUsbLog->dwLogType == UDISK_LOG_TYPE_OTHER_DEVICE_INSERT_REMOVE)
        {
            if (DEV_TYPE_NET == pUsbLog->dwUDiskType) //V3R10新增字段，区分其他外设类型
            {
                UsbLogJson["CMDVER"] = 4;   //4.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于其他外设的插拔告警，比3.0版本新增网卡事件

                //  组装其他设备插入信息上传
                std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
                person["Time"] =  UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcOtherDevName;
                person["Name"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcProductDesc;//用wcProductDesc字段保存IP
                person["IP"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVendorDesc;////用wcVendorDesc字段保存Mac
                person["Mac"] = UnicodeToUTF8(wsTemp);
                person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
                person["OtherDevType"] = 7;//7在与USM接口中表示网口事件(接口文档16.9)
            }
            else
            {
                UsbLogJson["CMDVER"] = 3;   //3.0版本，区别与旧版本的上报插拔告警日志JSON规格，主要用于其他外设的插拔告警

                //  组装其他设备插入信息上传
                std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
                person["Time"] =  UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcOtherDevName;
                person["Name"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcPID;
                person["PID"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVID;
                person["VID"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcProductDesc;
                person["ProductDesc"] = UnicodeToUTF8(wsTemp);
                wsTemp = pUsbLog->wcVendorDesc;
                person["VendorDesc"] = UnicodeToUTF8(wsTemp);
                person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
                person["OtherDevType"] = (int)pUsbLog->dwUDiskType;
            }

            CMDContentOtherDev.append(person);
            person.clear();
        }
		else
		{
		    UsbLogJson["CMDVER"] = 1; //表示安全U盘、普通U盘、移动硬盘的插拔
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);

			//wsTemp = pUsbLog->szlogon;
			//person["UserName"] = UnicodeToUTF8(wsTemp);

			person["UDiskType"] = (int)pUsbLog->dwUDiskType;//0 - 未知U盘，1-普通U盘，2-v2.0安全U盘; 3-V3.0U盘。
			wsTemp = pUsbLog->szSerial;
			person["serialID"] = UnicodeToUTF8(wsTemp);
			person["registerStatus"] = (int)pUsbLog->dwBlock>>16;        
			wsTemp = pUsbLog->szFullPath;//"E: "
			person["DiskDriverLetter"].append(UnicodeToUTF8(wsTemp));
			//person["DiskDriverLetter"] = UnicodeToUTF8(wsTemp);//"DiskDriverLetter":["E:\"] 
			person["plugEvent"] = (int)pUsbLog->dwBlock & 0xF;
			CMDContent.append(person);
			person.clear();
		}
	}
	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;
	UsbLogJson["CMDUsbContent"] = (Json::Value)CMDUsbContent;
    UsbLogJson["CMDContentOtherDev"] = (Json::Value)CMDContentOtherDev;

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}

// 网卡事件
std::string CWLJsonParse::NetAdapterLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<CWLMetaData*>& vecNetLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecNetLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;
    Json::Value CMDUsbContent;
    Json::Value CMDContentOtherDev;

    for (int i=0; i< nCount; i++)
    {
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecNetLog[i]->GetData();
        PNETWORK_ADAPTER_LOG pNetLog = (PNETWORK_ADAPTER_LOG)pipclogcomm->data;

        UsbLogJson["CMDVER"] = 4;   //4.0版本，在原来信息的基础上增加网卡设备接入信息

        //  组装网卡设备插入信息上传
        std::wstring wsTemp = convertTimeTToStr((DWORD)pNetLog->llTime);
        person["Time"] =  UnicodeToUTF8(wsTemp);
        wsTemp = pNetLog->Name;
        person["Name"] = UnicodeToUTF8(wsTemp);
        wsTemp = pNetLog->IPAddress;
        person["IP"] = UnicodeToUTF8(wsTemp);
        person["PlugEvent"] = (int)pNetLog->dwPlugEvent;
        person["OtherDevType"] = (int)pNetLog->dwDevType;

        CMDContentOtherDev.append(person);
        person.clear();
    }
    
    UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    UsbLogJson["CMDTYPE"] = (int)cmdType;
    UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;
    UsbLogJson["CMDUsbContent"] = (Json::Value)CMDUsbContent;
    UsbLogJson["CMDContentOtherDev"] = (Json::Value)CMDContentOtherDev;

    root.append(UsbLogJson);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::NetAdapterLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<NETWORK_ADAPTER_LOG *>& vecNetLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecNetLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;
    Json::Value CMDUsbContent;
    Json::Value CMDContentOtherDev;

    for (int i=0; i< nCount; i++)
    {
        PNETWORK_ADAPTER_LOG pNetLog = (PNETWORK_ADAPTER_LOG)vecNetLog[i];

        UsbLogJson["CMDVER"] = 4;   //4.0版本，在原来信息的基础上增加网卡设备接入信息

        //  组装网卡设备插入信息上传
        std::wstring wsTemp = convertTimeTToStr((DWORD)pNetLog->llTime);
        person["Time"] =  UnicodeToUTF8(wsTemp);
        wsTemp = pNetLog->Name;
        person["Name"] = UnicodeToUTF8(wsTemp);
        wsTemp = pNetLog->IPAddress;
        person["IP"] = UnicodeToUTF8(wsTemp);
        person["PlugEvent"] = (int)pNetLog->dwPlugEvent;
        person["OtherDevType"] = (int)pNetLog->dwDevType;

        CMDContentOtherDev.append(person);
        person.clear();
    }

    UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    UsbLogJson["CMDTYPE"] = (int)cmdType;
    UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;
    UsbLogJson["CMDUsbContent"] = (Json::Value)CMDUsbContent;
    UsbLogJson["CMDContentOtherDev"] = (Json::Value)CMDContentOtherDev;

    root.append(UsbLogJson);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

/*
其他外设使用被禁止时产生告警，客户端组装并上传告警

“CMDContent”:[
{
“Time”:”2015-07-02 17:45:30”,
“UsbType”:4,4，cdrom.5 wifi.6蓝牙，7串口，8并口
“LogContent”:”U盘使用被禁止”,//安全U盘使用被禁止，cdrom使用被禁止，wifi使用被禁止，
 “UserName”:”Administrator”,
},
]
clientULog.do
----------------------------
*/
std::string CWLJsonParse::ExtDevLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<CWLMetaData*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;


	for (int i=0; i< nCount; i++)
	{
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecUsbLog[i]->GetData();
		UDISK_LOG_STRUCT *pUsbLog = (UDISK_LOG_STRUCT*)pipclogcomm->data;

		if (pUsbLog->dwLogType == UDISK_LOG_TYPE_USBPORT_INSERT)
		{
			UsbLogJson["CMDVER"] = 2;   //区别与旧版本的上报告警日志JSON规格

			//  组装USB设备插入信息上传
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcPID;
			person["PID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVID;
			person["VID"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcProductDesc;
			person["ProductDesc"] = UnicodeToUTF8(wsTemp);
			wsTemp = pUsbLog->wcVendorDesc;
			person["VendorDesc"] = UnicodeToUTF8(wsTemp);
			person["PlugEvent"] = (int)pUsbLog->dwPlugEvent;
			person["USBClass"] = (int)4;  // pUsbLog->wcUSBClass;
			CMDContent.append(person);
			person.clear();
		}
		else
		{
			std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
			person["Time"] =  UnicodeToUTF8(wsTemp);
			person["UsbType"] = (int)pUsbLog->dwLogType;
			
			wsTemp = pUsbLog->szLogContent;
			person["LogContent"] = UnicodeToUTF8(wsTemp);

			wsTemp = pUsbLog->szlogon;
			person["UserName"] = UnicodeToUTF8(wsTemp);

            wsTemp = pUsbLog->szFullPath;
            person["FullPath"] = UnicodeToUTF8(wsTemp);

			//V3R2新追加字段
			/*person["UDiskType"] = (int)pUsbLog->dwUDiskType;

			wsTemp = pUsbLog->szSerial;
			person["serialID"] = UnicodeToUTF8(wsTemp);

			wsTemp = pUsbLog->szFullPath;
			person["FullPath"] = UnicodeToUTF8(wsTemp);

			wsTemp = pUsbLog->szProcessName;
			person["ProcessName"] = UnicodeToUTF8(wsTemp);

			person["OperationContent"] = (int)pUsbLog->dwOptContent;

			person["Block"] = (int)pUsbLog->dwBlock;*/
	      
			CMDContent.append(person);
			person.clear();

		}
	}


	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
    UsbLogJson["CMDContent"] = (Json::Value)CMDContent;

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}

// 其他外设使用被禁止时产生告警，客户端组装并上传告警
std::string CWLJsonParse::ExtDevLog_GetJsonByVector(
    __in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
    __in vector<UDISK_LOG_STRUCT*>& vecUsbLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecUsbLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
    Json::Value UsbLogJson;
    Json::Value CMDContent;


	for (int i = 0; i< nCount; i++)
	{
		UDISK_LOG_STRUCT *pUsbLog = vecUsbLog[i] ;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pUsbLog->llTime);
		person["Time"] =  UnicodeToUTF8(wsTemp);
		person["UsbType"] = (int)pUsbLog->dwLogType;//

		wsTemp = pUsbLog->szLogContent;
		person["LogContent"] = UnicodeToUTF8(wsTemp);//插入与拔出

		wsTemp = pUsbLog->szlogon;
		person["UserName"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szFullPath;
        person["FullPath"] = UnicodeToUTF8(wsTemp);
#if 0
        //V3R2新追加字段
        person["UDiskType"] = (int)pUsbLog->dwUDiskType;

        wsTemp = pUsbLog->szSerial;
        person["serialID"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szFullPath;
        person["FullPath"] = UnicodeToUTF8(wsTemp);

        wsTemp = pUsbLog->szProcessName;
        person["ProcessName"] = UnicodeToUTF8(wsTemp);

        person["OperationContent"] = (int)pUsbLog->dwOptContent;

        person["Block"] = (int)pUsbLog->dwBlock;
#endif
		CMDContent.append(person);
		person.clear();
	}


	UsbLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	UsbLogJson["CMDTYPE"] = (int)cmdType;
	UsbLogJson["CMDID"] = (int)cmdID;
	UsbLogJson["CMDContent"] = (Json::Value)CMDContent;

	root.append(UsbLogJson);
	sJsonPacket = writer.write(root);
	root.clear();


	//wsJsonPacket = ConvertA2W(sJsonPacket);
	//sJsonPacket = UnicodeToUTF8(ws);
	//return sJsonPacket;

	//std::wstring wsJsonPacket = _T("");
	//wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

	return sJsonPacket;
}

BOOL CWLJsonParse::UsbLog_GetLogVectorByJson(__in std::string sJson, __out vector<UDISK_LOG_STRUCT*>& vecUsbLog)
{

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";


	//tstring ws  = UTF8ToUnicode(sJson);
	//strValue = ConvertW2A(ws);
	std::wstring ws = _T("");
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;

	if( !root[0].isMember("CMDContent"))
	{
		return FALSE;
	}

	//获取数组内容
	strValue = root[0]["CMDContent"].asString();
	root.clear();

	//再解析一次
	if (!reader.parse(strValue, root))
	{
		return  FALSE;
	}

	nObject = root.size();
	if( nObject < 1)
		return FALSE;
	//wwdv3
	if (root.isArray())
	{
		for(int i=0; i<nObject; ++i)
		{
			UDISK_LOG_STRUCT* pUsbLog = new UDISK_LOG_STRUCT;
			memset( pUsbLog, 0, sizeof(UDISK_LOG_STRUCT));

			ws = UTF8ToUnicode(root[i]["Time"].asString());
			time_t t = 0;
			convertStrToTimeT(t, ws);

			pUsbLog->llTime = (LONGLONG)t;

			pUsbLog->dwLogType =(DWORD) root[i]["UsbType"].asUInt();


			ws = UTF8ToUnicode(root[i]["LogContent"].asString());
			memcpy( pUsbLog->szLogContent, ws.c_str(), ws.length()* sizeof(TCHAR));

			ws = UTF8ToUnicode(root[i]["UserName"].asString());
			memcpy( pUsbLog->szlogon, ws.c_str(), ws.length()* sizeof(TCHAR));

			pUsbLog->dwUDiskType =(DWORD) root[i]["UDiskType"].asUInt();

			ws = UTF8ToUnicode(root[i]["serialID"].asString());
			memcpy( pUsbLog->szSerial, ws.c_str(), ws.length()* sizeof(TCHAR));

			ws = UTF8ToUnicode(root[i]["FullPath"].asString());
			memcpy( pUsbLog->szFullPath, ws.c_str(), ws.length()* sizeof(TCHAR));

			ws = UTF8ToUnicode(root[i]["ProcessNmae"].asString());
			memcpy( pUsbLog->szProcessName, ws.c_str(), ws.length()* sizeof(TCHAR));

			pUsbLog->dwOptContent =(E_AUDIT_USB_OPERATION) root[i]["OperationContent"].asUInt();

			pUsbLog->dwBlock =(DWORD) root[i]["Block"].asUInt();

			vecUsbLog.push_back(pUsbLog);
		}
	}
	

	return TRUE;
}

std::string CWLJsonParse::VulLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData*>& vecVulLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecVulLog.size();
	if( nCount == 0)
		return sJsonPacket;


	Json::FastWriter writer;
	Json::Value root;
	Json::Value vulLogJson;
	Json::Value cmdContent;


	for (int i=0; i< nCount; i++)
	{
		Json::Value logItem;
		IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecVulLog[i]->GetData();
		VULNERABLE_LOG_STRUCT *pVulLog = (VULNERABLE_LOG_STRUCT*)pipclogcomm->data;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pVulLog->llTime);
		logItem["Time"] =  UnicodeToUTF8(wsTemp);
		logItem["VulType"] = (int)pVulLog->dwVulType;
		logItem["VulLevel"] = (int)pVulLog->dwVulLevel;
		logItem["ControlMode"] = (int)pVulLog->dwControlMode;
		logItem["SrcPort"] = (int)pVulLog->usSrcPort;
		logItem["DstPort"] = (int)pVulLog->usDstPort;
		logItem["Protocol"] = pVulLog->dwVulType > 0 ? "TCP":"UDP";

		wsTemp = pVulLog->szSrcIp;
		logItem["SrcIp"] = UnicodeToUTF8(wsTemp);

		wsTemp = pVulLog->szDstIp;
		logItem["DstIp"] = UnicodeToUTF8(wsTemp);

		cmdContent.append(logItem);
	}

	vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	vulLogJson["CMDTYPE"] = (int)cmdType;
	vulLogJson["CMDID"] = (int)cmdID;
	vulLogJson["CMDContent"] = cmdContent;

	root.append(vulLogJson);
	sJsonPacket = writer.write(root);

	root.clear();

	return sJsonPacket;
}


std::string CWLJsonParse::VulLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<VULNERABLE_LOG_STRUCT*>& vecVulLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecVulLog.size();
	if( nCount == 0)
		return sJsonPacket;

	Json::FastWriter writer;
	Json::Value root;
	Json::Value vulLogJson;
	Json::Value cmdContent;


	for (int i=0; i< nCount; i++)
	{
		Json::Value logItem;

		std::wstring wsTemp = convertTimeTToStr((DWORD)vecVulLog[i]->llTime);
		logItem["Time"] =  UnicodeToUTF8(wsTemp);
		logItem["VulType"] = (int)vecVulLog[i]->dwVulType;
		logItem["VulLevel"] = (int)vecVulLog[i]->dwVulLevel;
		logItem["ControlMode"] = (int)vecVulLog[i]->dwControlMode;
		logItem["SrcPort"] = (int)vecVulLog[i]->usSrcPort;
		logItem["DstPort"] = (int)vecVulLog[i]->usDstPort;
		logItem["Protocol"] = vecVulLog[i]->dwVulType > 0 ? "TCP":"UDP";

		wsTemp = vecVulLog[i]->szSrcIp;
		logItem["SrcIp"] = UnicodeToUTF8(wsTemp);

		wsTemp = vecVulLog[i]->szDstIp;
		logItem["DstIp"] = UnicodeToUTF8(wsTemp);

		cmdContent.append(logItem);
	}

	vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
	vulLogJson["CMDTYPE"] = (int)cmdType;
	vulLogJson["CMDID"] = (int)cmdID;
	vulLogJson["CMDContent"] = cmdContent;

	root.append(vulLogJson);
	sJsonPacket = writer.write(root);
	root.clear();

	//sJsonPacket = writer.write(vulLogJson);
	//vulLogJson.clear();

	return sJsonPacket;
}


#if 0
BOOL CWLJsonParse::VulLog_GetLogVectorByJson(__in std::string sJson, __out vector<VULNERABLE_LOG_STRUCT*>& vecVulLog)
{

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";


	//tstring ws  = UTF8ToUnicode(sJson);
	//strValue = ConvertW2A(ws);
	std::wstring ws = _T("");
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;

	if( !root[0].isMember("CMDContent"))
	{
		return FALSE;
	}

	//获取数组内容
	strValue = root[0]["CMDContent"].asString();
	root.clear();

	//再解析一次
	if (!reader.parse(strValue, root))
	{
		return  FALSE;
	}

	nObject = root.size();
	if( nObject < 1)
		return FALSE;

	for(int i=0; i<nObject; ++i)
	{
		VULNERABLE_LOG_STRUCT* pVulLog = new VULNERABLE_LOG_STRUCT;
		memset( pVulLog, 0, sizeof(VULNERABLE_LOG_STRUCT));

		ws = UTF8ToUnicode(root[i]["Time"].asString());
		time_t t = 0;
		convertStrToTimeT(t, ws);

		pVulLog->llTime = (LONGLONG)t;

		pVulLog->dwVulType =(DWORD) root[i]["VulType"].asUInt();


		ws = UTF8ToUnicode(root[i]["SrcIp"].asString());
		memcpy( pVulLog->szSrcIp, ws.c_str(), ws.length()* sizeof(TCHAR));

		ws = UTF8ToUnicode(root[i]["DstIp"].asString());
		memcpy( pVulLog->szDstIp, ws.c_str(), ws.length()* sizeof(TCHAR));

		vecVulLog.push_back(pVulLog);
	}

	return TRUE;
}
#endif

BOOL CWLJsonParse::User_GetPasswordByJson(__in std::string sJson, __out tstring& wsSuperAdminPassword, __out tstring& wsAdminPassword)
{
	BOOL bResult = FALSE;

	std::string s1 = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	if( !root.isMember("CMDContent"))
	{
		goto _exist_;
	}

	root = (Json::Value)root["CMDContent"];


	if(  root.isMember("SuperAdmin") )
	{
		 s1 = root["SuperAdmin"].asString();
		if( s1.length() > 0)
		{
			wsSuperAdminPassword = UTF8ToUnicode(s1);
		}
	}
	else
	{
		goto _exist_;
	}

	if( root.isMember("Admin"))
	{
		s1 = root["Admin"].asString();
		if( s1.length() > 0)
		{
			wsAdminPassword = UTF8ToUnicode(s1);
		}
	}
	else
	{
		goto _exist_;
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

BOOL CWLJsonParse::User_GetPassword2ByJson(__in std::string sJson, __out tstring& wsSuperAdminPassword, __out tstring& wsAdminPassword, __out tstring& wsAuditPassword)
{
	BOOL bResult = FALSE;

	std::string s1 = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";
	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	if( !root.isMember("CMDContent"))
	{
		goto _exist_;
	}

	root = (Json::Value)root["CMDContent"];


	if(  root.isMember("SuperAdmin") )
	{
		s1 = root["SuperAdmin"].asString();
		if( s1.length() > 0)
		{
			wsSuperAdminPassword = UTF8ToUnicode(s1);
		}
	}
	else
	{
		goto _exist_;
	}

	if( root.isMember("Admin"))
	{
		s1 = root["Admin"].asString();
		if( s1.length() > 0)
		{
			wsAdminPassword = UTF8ToUnicode(s1);
		}
	}
	else
	{
		goto _exist_;
	}

	if( root.isMember("Audit"))
	{
		s1 = root["Audit"].asString();
		if( s1.length() > 0)
		{
			wsAuditPassword = UTF8ToUnicode(s1);
		}
	}
	else
	{
		goto _exist_;
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

std::string CWLJsonParse::License_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in std::wstring& wsLicenseFileName, __in std::string& sLicenseFileContentBase64)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

    CMDContent["LicenseFileName"]    = UnicodeToUTF8(wsLicenseFileName);
	CMDContent["LicenseFileContent"] = sLicenseFileContentBase64;

	person["CMDContent"] = (Json::Value)CMDContent;

	person["ComputerID"] = UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::License_GetLicenseContent(__in std::string sJson, __out std::wstring& wsLicenseFileName, __out std::string& sLicenseFileContentBase64)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::FastWriter writer;
	Json::Reader	reader;
	Json::Value person;

	std::string		strValue = "";

	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	if( !root.isMember("CMDContent"))
	{
		goto _exist_;
	}

	root = (Json::Value)root["CMDContent"];
	if( root.isMember("LicenseFileName"))
	{
		wsLicenseFileName = UTF8ToUnicode(root["LicenseFileName"].asString());
	}
	else
	{
		goto _exist_;
	}

	if(root.isMember("LicenseFileContent"))
	{
		sLicenseFileContentBase64 = root["LicenseFileContent"].asString();
	}
	else
	{
		goto _exist_;
	}

	bResult = TRUE;

_exist_:

	return bResult;
}


/*
[{
	"ComputerID":"FEFOEACD",
	"CMDTYPE":200,
	"CMDID":200,
	"Domain":"",
	"clientLanguage":"zh",  // 客户端语言编码
	"CMDContent":
	{
		"bootTime":123454566,
		"bootTimeStr":"Fri 0ct 13 09:13:32 2023",

		"dwCPU":10,
		"dwMem":15,
		"WindowsVersion":"XXX",
		"ComputerName":"XXX",
		"ComputerIP":"XXX",
		"Partiton":      // 逻辑分区名称，使用率和总容量
		[
		   {
			"Drive":"C:",
			"TotalSize":"59.67G",
			 "UsageRate":"XX%"
			},
			{
			   "Drive":"D:",
			   "TotalSize":"99.67G",
			   "UsageRate":"XX%"
			},
			{
			   ……
			}
		 ]
	}
}]
*/
// 添加了	vecDiskInfo vec_DiskInfo 变量，用于给USM上传硬盘信息
//添加了    wstrMac ，用于上传当前通信网卡的mac地址 V3R10 TR6
std::string CWLJsonParse::HeartBeat_GetJson_FromV3R7C02(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
														__in tstring domainName,__in int iCpu, __in int iMemory, 
														const vecDiskInfo &vec_DiskInfo, 
														__in tstring strSystemName,
														__in wstring wsComputerName,
														__in wstring wstrIpAddr,
														__in wstring wstrMac,
														__in time_t bootTimeInSeconds)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent, PartitionArray;
	
	// 【新增Partiton，表示磁盘信息】
	

	int nCount = (int)vec_DiskInfo.size();
	for(int i = 0; i < nCount; i++)
	{
		Json::Value Partiton;

		ULONGLONG dwFreeSize = vec_DiskInfo[i].m_uTotalNumberOfFreeBytes.QuadPart;///1024/1024/1024;
		ULONGLONG dwTotalSize = vec_DiskInfo[i].m_uTotalNumberOfBytes.QuadPart;///1024/1024/1024;
		int DiskUseRate = (int)((dwTotalSize - dwFreeSize) * 100 / dwTotalSize);

		CString strTotalSize;
		if(dwTotalSize > 1024*1024*1024)
		{
			dwTotalSize = dwTotalSize/1024/1024/1024;
			strTotalSize.Format(_T("%d"),dwTotalSize);
			strTotalSize += _T("G");
		}
		else if(dwTotalSize > 1024*1024)
		{
			dwTotalSize = dwTotalSize/1024/1024;
			strTotalSize.Format(_T("%d"),dwTotalSize);
			strTotalSize += _T("M");

		}
		else if(dwTotalSize > 1024)
		{
			dwTotalSize = dwTotalSize/1024;
			strTotalSize.Format(_T("%d"),dwTotalSize);
			strTotalSize += _T("K");
		}
		else if(dwTotalSize )
		{
			strTotalSize.Format(_T("%d"),dwTotalSize);
			strTotalSize += _T("B");
		}

		CString strUsageRate;
		strUsageRate.Format(_T("%d"),DiskUseRate);
		strUsageRate += _T("%");

		WCHAR DriveName[MAX_PATH] = {0};

		_tcsncpy_s(DriveName, _countof(DriveName), vec_DiskInfo[i].szDirve, _countof(DriveName)-1 );

		size_t length = wcslen(DriveName);

		// 检查最后一个字符是否是反斜杠
		if (length > 0 && DriveName[length - 1] == L'\\') 
		{
			// 如果是反斜杠，将其替换为字符串终止符
			DriveName[length - 1] = L'\0';
		}

		Partiton["Drive"] = UnicodeToUTF8(DriveName);	// "C:"
		Partiton["TotalSize"] = UnicodeToUTF8(strTotalSize.GetBuffer());
		Partiton["UsageRate"] = UnicodeToUTF8(strUsageRate.GetBuffer());
		PartitionArray.append(Partiton);
	}

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);
	CMDContent["dwCPU"] = iCpu;
	CMDContent["dwMem"] = iMemory;
	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);
	CMDContent["ComputerMac"] = UnicodeToUTF8(wstrMac);
	CMDContent["bootTime"] = (Json::LargestUInt)bootTimeInSeconds;//系统开机时刻 1970年的秒数。
	CMDContent["bootTimeStr"] = std::asctime(std::localtime(&bootTimeInSeconds));
	CMDContent["Partiton"] = PartitionArray;

	person["CMDContent"] = CMDContent;

	//获取语音类型
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


std::string CWLJsonParse::HeartBeat_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in int iCpu, __in int iMemory, __in tstring strSystemName,__in wstring wsComputerName,__in wstring wstrIpAddr)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);
	CMDContent["dwCPU"] = iCpu;
	CMDContent["dwMem"] = iMemory;
	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wstrIpAddr);

	person["CMDContent"] = CMDContent;

	//获取语音类型
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


std::string CWLJsonParse::HeartBeat_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in tstring domainName,__in int iCpu, __in int iMemory, __in tstring strSystemName)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	CMDContent["a"] = 0;
	CMDContent.clear();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Domain"] = UnicodeToUTF8(domainName);
	CMDContent["dwCPU"] = iCpu;
	CMDContent["dwMem"] = iMemory;
	CMDContent["WindowsVersion"] = UnicodeToUTF8(strSystemName);
	person["CMDContent"] = CMDContent;

    //获取语音类型
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

std::string CWLJsonParse::ThreatLog_ProcStart_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_PROC_START pProcStartEvent)
{
	std::string sJsonPacket = "";

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
	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_PROCSTART;
	CMDContent["Process.TimeStamp"] = pProcStartEvent->llTimeStamp;
	CMDContent["Process.ProcessId"] = (Json::Int64)pProcStartEvent->stProcInfo.ulPid;
	CMDContent["Process.ProcessGuid"] = UnicodeToUTF8(pProcStartEvent->stProcInfo.ProcGuid);
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

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::ThreatLog_Reg_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_REG pRegEvent)
{
	std::string sJsonPacket = "";

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
	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_REG;

	CMDContent["Registry.Operation"] = pRegEvent->Operation;
	CMDContent["Registry.TimeStamp"] = pRegEvent->llTimeStamp;
	CMDContent["Registry.ProcessId"] = (Json::Int64)pRegEvent->stProcInfo.ulPid;;
	CMDContent["Registry.ProcessGuid"] = UnicodeToUTF8(pRegEvent->stProcInfo.ProcGuid);;
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

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::ThreatLog_File_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_FILE pFileEvent)
{
	std::string sJsonPacket = "";

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
	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_FILE;

	CMDContent["FileAccess.Operation"] = pFileEvent->Operation;
	CMDContent["FileAccess.TimeStamp"] = pFileEvent->llTimeStamp;
	CMDContent["FileAccess.ProcessId"] = (Json::Int64)pFileEvent->stProcInfo.ulPid;;
	CMDContent["FileAccess.ProcessGuid"] = UnicodeToUTF8(pFileEvent->stProcInfo.ProcGuid);;
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

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::ThreatLog_Proc_GetJson(__in tstring ComputerID, __in PTHREAT_EVENT_PROC pProcEvent)
{
	std::string sJsonPacket = "";

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
	person["CMDTYPE"] = 200;
	person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
	CMDContent["EventType"] = THREAT_EVENT_TYPE_PROC;
	CMDContent["ProcessAccess.TimeStamp"] = pProcEvent->llTimeStamp;
	CMDContent["ProcessAccess.SourceProcessId"] = (Json::Int64)pProcEvent->stProcInfo.ulPid;
	CMDContent["ProcessAccess.SourceProcessGuid"] = UnicodeToUTF8(pProcEvent->stProcInfo.ProcGuid);
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

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::ThreatLog_NetTcp_GetJson(__in tstring ComputerID, __in PEVENT_NETTCP pEventNetTcp)
{
    std::string sJsonPacket = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    if (NULL == pEventNetTcp)
    {
        return sJsonPacket;
    }

    CMDContent["a"] = 0;
    CMDContent.clear();

    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = 200;
    person["CMDID"] = THREAT_EVENT_UPLOAD_CMDID;
    CMDContent["EventType"] = THREAT_EVENT_TYPE_NETWORK;
    CMDContent["Network.TimeStamp"] = pEventNetTcp->llTimeStamp;
    CMDContent["Network.ProcessId"] = (Json::Int64)pEventNetTcp->stProcInfo.ulPid;
    CMDContent["Network.ProcessGuid"] = UnicodeToUTF8(pEventNetTcp->stProcInfo.ProcGuid);
    CMDContent["Network.ProcessFileName"] = UnicodeToUTF8(pEventNetTcp->stProcInfo.ProcFileName);
    CMDContent["Network.ProcessName"] = UnicodeToUTF8(pEventNetTcp->stProcInfo.ProcPath);
    CMDContent["Network.CommandLine"] = UnicodeToUTF8(pEventNetTcp->stProcInfo.ProcCMDLine);
    CMDContent["Network.User"] = UnicodeToUTF8(pEventNetTcp->stProcUserInfo.ProcUserName);
    CMDContent["Network.UserSid"] = UnicodeToUTF8(pEventNetTcp->stProcUserInfo.ProcUserSid);
    //CMDContent["ProcessAccess.SourceTerminalSessionId"] = (Json::Int64)pProcEvent->stProcUserInfo.ulTerminalSessionId;

    CMDContent["Network.Protocol"] = UnicodeToUTF8(L"tcp");
    
    if (NET_EVENTTYPE_CONNECT == pEventNetTcp->EventType)
    {
        CMDContent["Network.Initiated"] = (BOOLEAN)true;
        
        CMDContent["Network.SourceIsIpv6"] = (BOOLEAN)false;
        CMDContent["Network.SourceIp"] = UnicodeToUTF8(pEventNetTcp->stEventNetTcpInfo.LocalIP);
        CMDContent["Network.SourceHostname"] = UnicodeToUTF8(ComputerID);
        CMDContent["Network.SourcePort"] = (int)pEventNetTcp->stEventNetTcpInfo.LocalPort;
        
        CMDContent["Network.DestinationIsIpv6"] = (BOOLEAN)false;
        CMDContent["Network.DestinationIp"] = UnicodeToUTF8(pEventNetTcp->stEventNetTcpInfo.RemoteIP);
        //CMDContent["Network.DestinationHostname"] = UnicodeToUTF8(ComputerID);
        CMDContent["Network.DestinationPort"] = (int)pEventNetTcp->stEventNetTcpInfo.RemotePort;
    }
    else
    {
        CMDContent["Network.Initiated"] = (BOOLEAN)false;
        
        CMDContent["Network.SourceIsIpv6"] = (BOOLEAN)false;
        CMDContent["Network.SourceIp"] = UnicodeToUTF8(pEventNetTcp->stEventNetTcpInfo.RemoteIP);
        //CMDContent["Network.SourceHostname"] = UnicodeToUTF8(ComputerID);
        CMDContent["Network.SourcePort"] = (int)pEventNetTcp->stEventNetTcpInfo.RemotePort;

        CMDContent["Network.DestinationIsIpv6"] = (BOOLEAN)false;
        CMDContent["Network.DestinationIp"] = UnicodeToUTF8(pEventNetTcp->stEventNetTcpInfo.LocalIP);
        CMDContent["Network.DestinationHostname"] = UnicodeToUTF8(ComputerID);
        CMDContent["Network.DestinationPort"] = (int)pEventNetTcp->stEventNetTcpInfo.LocalPort;
    }

    person["CMDContent"] = CMDContent;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}


/*
 * desc:   get return
 * parm:
 *         sJson, the json of  recv from usm
 * return:
 *         TRUE: if "RESULT" == "SUC"
 *         FALSE: other
 */

BOOL CWLJsonParse::Common_GetResult(__in std::string sJson)
{
	Json::Reader	reader;
	Json::Value 	root;
	Json::Value 	content;
	if (!reader.parse(sJson, root))
	{
		return FALSE;
	}

	if( root.size() < 1 || !root.isArray())
	{
		return FALSE;
	}

	content = (Json::Value)root[0]["CMDContent"];
	if( content == NULL)
	{
		return FALSE;
	}

	if(!content.isMember("RESULT"))
	{
		return FALSE;
	}

	if(!content["RESULT"].isString())
	{
		return FALSE;
	}

	std::string sResult = content["RESULT"].asString();
	if( sResult == "SUC" )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


/*
 * desc:   get json info of scan status
 * parm:
 *         dwScanStatus, scan status
 * return: json of scan command
 */
std::string CWLJsonParse::ScanStatus_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in WORD dwScanStatus,__in WORD dwScanSpeed,__in int iWLFileCount, __in wstring wsTime)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	CMDContent["ScanStatus"] = (int)dwScanStatus;
	CMDContent["ScanSpeed"] = (int)dwScanSpeed;
	CMDContent["Time"]		 = UnicodeToUTF8(wsTime);
	CMDContent["WLFileCount"] = iWLFileCount;

	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] 		= (int)cmdType;
	person["CMDID"] 		= (int)cmdID;
	person["CMDContent"] 	= CMDContent;


	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();


	return sJsonPacket;
}

std::string CWLJsonParse::WhiteListCount_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID,__in int iWLFileCount, __in wstring wsTime)
{
    std::string sJsonPacket = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    CMDContent["Time"]		 = UnicodeToUTF8(wsTime);
    CMDContent["WLFileCount"] = iWLFileCount;

    person["ComputerID"]	= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] 		= (int)cmdType;
    person["CMDID"] 		= (int)cmdID;
    person["CMDContent"] 	= CMDContent;


    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();


    return sJsonPacket;
}

tstring CWLJsonParse::GetErrorInfoById(DWORD id)
{
    tstring tsstrInfo = _T("");

    //获取语音类型
    DWORD dwLanguage = WLUtils::GetLanguageType();

    if(RESULT_ERROR_CODE_FOR_START_SCAN_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单管理程序正在运行，请先关闭。");
        }
        else
        {
            tsstrInfo = _T("Whitelist management program is running, please shut down first.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_OPT_WHITELIST_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在扫描中，不允许操作白名单文件。");
        }
        else
        {
            tsstrInfo = _T("The whitelist is scanning, don't allow operation whitelist files.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_NOT_OPEN_PROGRAM_CONTROL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("请开启程序控制。");
        }
        else
        {
            tsstrInfo = _T("Please open the program control!");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_OPEN_PROCESS_AUDIT_NOT_CONFIG == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("进程审计开启，不允许下发空模板！");
        }
        else
        {
            tsstrInfo = _T("Configuration is empty, are not allowed to open the process audit!");
        }
    }
	else if (RESULT_ERROR_CODE_FOR_ADD_NETWHITELIST == id)
	{
		if( dwLanguage == 0 )
		{
			tsstrInfo = _T("网络白名单部分规则添加失败");
		}
		else
		{
			tsstrInfo = _T("Net Whitelist add fail!");
		}
	}
    else if(RESULT_ERROR_CODE_FOR_IMPORT_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在导入，请稍后再试。");
        }
        else
        {
            tsstrInfo = _T("Whitelist is imported, please try again later.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_ADDDIR_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在追加目录，请稍后再试。");
        }
        else
        {
            tsstrInfo = _T("Whitelist are additional directory, please try again later.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_ADDFILE_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在添加文件，请稍后再试。");
        }
        else
        {
            tsstrInfo = _T("Whitelist is add files, please try again later.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_DELFILE_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在删除文件，请稍后再试。");
        }
        else
        {
            tsstrInfo = _T("Whitelist is deleted files, please try again later.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_SCANINING_FAIL == id)
    {
        if( dwLanguage == 0 )
        {
            tsstrInfo = _T("白名单正在扫描，请稍后再试。");
        }
        else
        {
            tsstrInfo = _T("Whitelist is scanning, please try again later.");
        }
    }
	else if(RESULT_ERROR_CODE_FOR_NOT_IN_WHITELIST == id)
	{
		if( dwLanguage == 0 )
		{
			tsstrInfo = _T("配置中存在非白名单文件、下发进程审计失败。");
		}
		else
		{
			tsstrInfo = _T("There is a non-whitelist file in the configuration, and the delivery process audit fails.");
		}
	}
	else if(RESULT_ERROR_CODE_FOR_IPSEC_NORMAL_ERROR == id)
	{
		if( dwLanguage == 0 )
		{
			tsstrInfo = _T("网络白名单内部失败，请查看日志。");
		}
		else
		{
			tsstrInfo = _T("Net Whitelist Config fail!");
		}
	}
	else if(RESULT_ERROR_CODE_FOR_IPSEC_START_FWS == id)
	{
		if( dwLanguage == 0 )
		{
			tsstrInfo = _T("网络白名单开启服务失败。");
		}
		else
		{
			tsstrInfo = _T("Start firewall service fail!");
		}
	}
	else if(RESULT_ERROR_CODE_FOR_VIRUS_SCAN_NOW == id)
	{
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("正在进行病毒扫描，请等待扫描完成。");
		}
		else
		{
			tsstrInfo = _T("Virus scanning is in progress, please wait for the scanning to complete.");
		}
	}
    else if(RESULT_ERROR_CODE_FOR_FILE_COYP_NOW == id)
    {
        if( dwLanguage == 0)
        {
            tsstrInfo = _T("正在进行文件摆渡，请等待摆渡完成。");
        }
        else
        {
            tsstrInfo = _T("File shuttle is in progress, please wait for the shuttle to complete.");
        }
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_NOT_ENOUGH_SPACE == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("磁盘空间不足。");
		}
		else
		{
		tsstrInfo = _T("Insufficient disk space.");
		}
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_INVALID_BACKUP_LOCATION == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("备份目录所在磁盘不存在。");
		}
		else
		{
			tsstrInfo = _T("The disk where the backup directory is located does not exist.");
		}
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_ADJUST_BACKUP_TIMES == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("备份目录的访问被拒绝。");
		}
		else
		{
			tsstrInfo = _T("Access to the backup directory is denied.");
		}
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_PATH_NOT_ROOTPATH == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("备份目录不允许为磁盘根目录。");
		}
		else
		{
			tsstrInfo = _T("The backup directory is not allowed to be the root directory of the disk.");
		}
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_INVALID_BACKUP_LONGER == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("备份目录的长度超过限制，最多允许包含195个字符。");
		}
		else
		{
			tsstrInfo = _T("The length of the backup directory exceeds the limit, allowing a maximum of 195 characters.");
		}
    }
    else if(RESULT_ERROR_CODE_FOR_ARS_E_PATH_NOT_EMPTY == id)
    {
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("备份路径内有文件，请您选择空目录。");
		}
		else
		{
			tsstrInfo = _T("There are files in the backup path, please select an empty directory.");
		}
    }
	else if (RESULT_ERROR_CODE_FOR_ARS_E_USER_NOT_FOUND == id)
	{
		if( dwLanguage == 0)
		{
			tsstrInfo = _T("主体配置用户本地不存在。");
		}
		else
		{
			tsstrInfo = _T("The principal configuration user does not exist locally.");
		}
	}

    return tsstrInfo;
}

std::string CWLJsonParse::Result_GetJsonByDealResult(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in int nDealResult)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;
    //获取语音类型
    DWORD dwLanguage = WLUtils::GetLanguageType();

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

    //静默升级
    int iCmdID = (int)cmdID;
    if ( iCmdID == CMD_CLIENT_UPGRADE || iCmdID == PLY_CLIENT_ONLY_DOWNLOAD || iCmdID == PLY_CLIENT_SILENT_UPGRADE || iCmdID == PLY_VIRUS_LIB_DOWNLOAD)
    {
        if( nDealResult == 0 || nDealResult == 1)
        {
            CMDContent["RESULT"] = "SUC";
            CMDContent["REASON"] = nDealResult;
        }
        else
        {
            CMDContent["RESULT"] = "FAIL";
            CMDContent["REASON"] = nDealResult;
        }
    }
	else if (PLY_VP_SCAN_COFIG == iCmdID)
	{
		if( nDealResult == 0 )
			CMDContent["RESULT"] = "SUC";
		else
			CMDContent["RESULT"] = "FAIL";

		if(emVP_START_SCAN_RET_WHITELIST_SCAN_NOW == nDealResult)
		{
            if( dwLanguage == 0)
            {
                CMDContent["description"] = UnicodeToUTF8(_T("正在进行白名单扫描，请等待扫描完成。"));
            }
            else
            {
                CMDContent["description"] = UnicodeToUTF8(_T("Whitelist scan in progress,please wait for the scan to complete."));
            }
			
		}
		else if(emVP_START_SCAN_RET_VB_UPDATE_NOW == nDealResult)
		{
            if( dwLanguage == 0)
            {
                CMDContent["description"] = UnicodeToUTF8(_T("正在进行病毒库升级，请等待升级完成。"));
            }
            else
            {
                CMDContent["description"] = UnicodeToUTF8(_T("Virus database upgrade is in progress,please wait for the upgrade to complete."));
            }
		}
        else if (emVP_START_SCAN_RET_VB_FILE_COPY == nDealResult)
        {
            if( dwLanguage == 0)
            {
                CMDContent["description"] = UnicodeToUTF8(_T("正在进行文件摆渡，请等待摆渡完成。"));
            }
            else
            {
                CMDContent["description"] = UnicodeToUTF8(_T("File shuttle is in progress,please wait for the shuttle to complete."));
            }
        }
	}
	else if (CMD_PROCESS_PROTECT_ON    == iCmdID ||
			 CMD_PROCESS_PROTECT_OFF   == iCmdID ||
			 CMD_PROCESS_CTRL_MODE_ON  == iCmdID ||
			 CMD_PROCESS_CTRL_MODE_OFF == iCmdID)
	{
		if( nDealResult == 0 )
			CMDContent["RESULT"] = "SUC";
		else
			CMDContent["RESULT"] = "FAIL";

		if(RESULT_ERROR_CODE_FOR_WHITELIST_NOT_EXIST == nDealResult)
		{
            if( dwLanguage == 0)
            {
                CMDContent["description"] = UnicodeToUTF8(_T("未进行白名单扫描。"));
            }
            else
            {
                CMDContent["description"] = UnicodeToUTF8(_T("No whitelist scan has been performed."));
            }
		}
	}
    else
    {
        if( nDealResult == 0 )
            CMDContent["RESULT"] = "SUC";
        else
            CMDContent["RESULT"] = "FAIL";

        if(0 != nDealResult)
        {
            CMDContent["description"] = UnicodeToUTF8((GetErrorInfoById(nDealResult)));
        }
    }



	person["CMDContent"] = (Json::Value)CMDContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::Setup_GetLicenseContent(__in std::string sJson, __out std::wstring& wsLicenseFileName, __out std::string& sLicenseFileContentBase64)
{

	Json::Value root;
	Json::FastWriter writer;
	Json::Reader	reader;
	Json::Value person;

	std::string		strValue = "";

	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE ;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;


	root = (Json::Value)root[0]["CMDContent"];

	if( root.isMember("LicenseFileName"))
	{
		wsLicenseFileName = UTF8ToUnicode(root["LicenseFileName"].asString());
	}

	if(root.isMember("LicenseFileContent"))
		sLicenseFileContentBase64 = root["LicenseFileContent"].asString();

	return TRUE;
}

BOOL CWLJsonParse::Setup_GetPasswordByJson(__in std::string sJson, __out tstring& wsSuperAdminPassword, __out tstring& wsAdminPassword)
{
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";

	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE ;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;


	root = (Json::Value)root[0]["CMDContent"];


	std::string s1 = root["SuperAdmin"].asString();
	if( s1.length() > 0)
	{
		wsSuperAdminPassword = UTF8ToUnicode(s1);
	}

	s1 = root["Admin"].asString();
	if( s1.length() > 0)
	{
		wsAdminPassword = UTF8ToUnicode(s1);
	}

	return TRUE;
}

BOOL CWLJsonParse::Setup_GetPassword2ByJson(__in std::string sJson, __out tstring& wsSuperAdminPassword, __out tstring& wsAdminPassword, __out tstring& wsAuditPassword)
{
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";

	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		return FALSE ;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
		return FALSE;


	root = (Json::Value)root[0]["CMDContent"];


	std::string s1 = root["SuperAdmin"].asString();
	if( s1.length() > 0)
	{
		wsSuperAdminPassword = UTF8ToUnicode(s1);
	}

	s1 = root["Admin"].asString();
	if( s1.length() > 0)
	{
		wsAdminPassword = UTF8ToUnicode(s1);
	}

	s1 = root["Audit"].asString();
	if( s1.length() > 0)
	{
		wsAuditPassword = UTF8ToUnicode(s1);
	}

	return TRUE;
}

//BOOL CWLJsonParse::Setup_GetIsUpLoadSmallFile(__in std::string& sJson, __out int& nIsUpLoadSmallFile, __out int& nMaxSize)
//{
//	/*
//	李佳鑫 12:35:56
//	“ComputerID”:”FEFOEACD”,
//	“CMDTYPE”:1,
//	“CMDID”:1,
//	“CMDContent”:{
//	“RESULT”:”SUC”
//	“SuperAdmin”:”password”,
//	“Admin”:”password”,
//	“LicenseFileName”:” 20150605104227-USE1-01000111-0005.lcs”,
//	“LicenseFileContent”:”abedgeasXdfweg”4
//	uploadFile：9/10
//
//	*/
//	Json::Value root;
//	Json::FastWriter writer;
//	Json::Reader	reader;
//	Json::Value person;
//
//	std::string		strValue = "";
//
//	strValue = sJson;
//
//	if (!reader.parse(strValue, root))
//	{
//		return FALSE ;
//	}
//
//	int nObject = root.size();
//	if( nObject < 1)
//		return FALSE;
//
//	root = (Json::Value)root[0]["CMDContent"];
//
//
//	if( root.isMember("uploadFile"))
//	{
//		nIsUpLoadSmallFile = root["uploadFile"].asInt();
//	}
//	else
//	{
//		nIsUpLoadSmallFile = 0;
//	}
//
//	//max size
//	if( root.isMember("uploadFileMax") && root["uploadFileMax"].isInt())
//	{
//		nMaxSize = root["uploadFileMax"].asInt();
//	}
//	else
//	{
//		nMaxSize = UPLOAD_PE_MAX;
//	}
//
//	return TRUE;
//}


std::string CWLJsonParse::SetUp_GetJson(__in tstring ComputerID, __in tstring strUserName, __in WORD cmdType , __in WORD cmdID, tstring wsComputerName, tstring wsIP, tstring wsMacAdress, tstring wsWinVer, BOOL bIsBit64, int iProxying, BOOL bRecoveryLicense, int iIEG , tstring wsClientVersion)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	CMDContent["ComputerName"] = UnicodeToUTF8(wsComputerName);
	CMDContent["ComputerIP"] = UnicodeToUTF8(wsIP);

	if(wsMacAdress.length()> 0)
		CMDContent["ComputerMAC"] = UnicodeToUTF8(wsMacAdress);
	else
		CMDContent["ComputerMAC"] = JSON_DEFAULT_STRING;

	CMDContent["WindowsVersion"] = UnicodeToUTF8(wsWinVer);
	CMDContent["WindowsX64"] = (int)bIsBit64;

	//szCurVersion":	"V200R003C10B150",  /* 主机卫士版本号，老版本全部取测 值  */
	//srsVersion ":""   /* 主机加固的版本号  */
    switch (iIEG)
    {
    case LCS_PRODUCT_SRS:
        {
            CMDContent["szCurVersion"] = UnicodeToUTF8(_T(""));
            CMDContent["srsVersion"] = UnicodeToUTF8(wsClientVersion);
        }
        break;

    default:
		CMDContent["szCurVersion"] = UnicodeToUTF8(wsClientVersion);
		CMDContent["srsVersion"] = UnicodeToUTF8(_T(""));
        break;
    }

    CMDContent["licenseRecycle"] = (int)bRecoveryLicense; //licenseRecycle字段，1表示需要USM回收节点，0表示不回收


	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Username"]= UnicodeToUTF8(strUserName);
	person["Proxying"]= iProxying;

	root.append(person);
	sJsonPacket = writer.write(root);

	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::Setup_GetJsonInstallEnd(__in tstring ComputerID, __in tstring strUserName, __in WORD cmdType , __in WORD cmdID,  __in int nDealResult)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	if( nDealResult == 0)
		CMDContent["RESULT"] = "SUC";
	else
		CMDContent["RESULT"] = "FAIL";

	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["Username"]= UnicodeToUTF8(strUserName);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);

	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::Setup_CheckResultByJson(__in std::string sJson, __out int& nErrorCode, __out tstring& wsMessage)
{
	BOOL bResult = FALSE;
	INT  nError = 0;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;

	std::string		strValue = "";
	int cmdid = 0;


	if( sJson.length() == 0)
	{
		nError = -1;
		goto _exist_;
	}


	strValue = sJson;

	if (!reader.parse(strValue, root))
	{
		nError = -2;
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		nError = -3;
		goto _exist_;
	}

	if( root[0].isMember("CMDID") )
	{
		cmdid = root[0]["CMDID"].asInt();
	}

	root = (Json::Value)root[0]["CMDContent"];
	
	if (root.isMember("MESSAGE") && root["MESSAGE"].isString())
	{
        std::string s1 = root["MESSAGE"].asString();
        if( s1.length() > 0)
        {
            wsMessage = UTF8ToUnicode(s1);
        }
	}

	if(1==cmdid  && root.isMember("ERR_CODE"))//cmdid 1注册管理平台
	{
		nError = root["ERR_CODE"].asInt();
		goto _exist_;
	}

	if( root == NULL)
	{
		nError = -4;
		goto _exist_;
	}

	if( root.isMember("RESULT") )
	{
		nError = -5;
		bResult = FALSE;
	}
	else
	{
		bResult = TRUE;
	}

_exist_:

	nErrorCode = nError;
	return bResult;
}

//BOOL CWLJsonParse::Setup_GetUniqueID(__in std::string sJson,  __out DWORD &nUniqueID, tstring *pStrErr)
//{
//	BOOL bRes = FALSE;
//	wostringstream  strTemp;
//	Json::Reader	reader;
//	Json::Value root;
//	int nObject = 0;
//	nUniqueID = 0;
//
//	if( sJson.length() == 0)
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetUniqueID, sJson.length() == 0")<<_T(",");
//		goto END;
//	}
//
//	if (!reader.parse(sJson, root))
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetUniqueID, parse fail")<<_T(",");
//		goto END;
//	}
//
//	nObject = root.size();
//	if( nObject < 1)
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetUniqueID, size=")<<nObject<<_T(",");
//		goto END;
//	}
//
//	if (!root[0].isMember("CMDContent"))
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetUniqueID, CMDContent is not member")<<_T(",");
//		goto END;
//	}
//
//	root = (Json::Value)root[0]["CMDContent"];
//
//	//UniqueID
//	if (root.isMember("devid"))
//	{
//		if(!root["devid"].isUInt())
//		{
//			strTemp << _T("CWLJsonParse::Setup_GetUniqueID, logServerPort fail")<<_T(",");
//			goto END;
//		}
//		nUniqueID = root["devid"].asUInt();
//	}
//
//	bRes = TRUE;
//END:
//	if (pStrErr)
//	{
//		*pStrErr = strTemp.str();
//	}
//
//	return bRes;
//}

//BOOL CWLJsonParse::Setup_GetLogServerByJson(__in std::string sJson, __out tstring &strIP, __out WORD &nPort, tstring *pStrErr)
//{
//	/*
//	获取独立日志服务器的IP和端口，
//	未下发则strIP返回空字符串；
//
//	*/
//	//logServerIp,logServerPort;
//	BOOL bRes = FALSE;
//	wostringstream  strTemp;
//	Json::Reader	reader;
//	Json::Value root;
//	int nObject = 0;
//
//	strIP = _T("");
//	if( sJson.length() == 0)
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, sJson.length() == 0")<<_T(",");
//		goto END;
//	}
//
//	if (!reader.parse(sJson, root))
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, parse fail")<<_T(",");
//		goto END;
//	}
//
//	nObject = root.size();
//	if( nObject < 1)
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, size=")<<nObject<<_T(",");
//		goto END;
//	}
//
//	if (!root[0].isMember("CMDContent"))
//	{
//		strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, CMDContent is not member")<<_T(",");
//		goto END;
//	}
//
//	root = (Json::Value)root[0]["CMDContent"];
//
//	//ip
//	if (root.isMember("logServerIp"))
//	{
//		if(!root["logServerIp"].isString())
//		{
//			strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, logServerIp fail")<<_T(",");
//			goto END;
//		}
//		//检查IP规则
//
//		if(!OSUtils::isIPValid(root["logServerIp"].asString().c_str()))
//		{
//			strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, invalid ip=%S")<<root["logServerIp"].asString().c_str()<<_T(",");
//			goto END;
//		}
//		strIP = CStrUtil::ConvertA2W(root["logServerIp"].asString());
//
//	}
//
//	//port
//	if (root.isMember("logServerPort"))
//	{
//		if(!root["logServerPort"].isUInt())
//		{
//			strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, logServerPort fail")<<_T(",");
//			goto END;
//		}
//
//		//端口规则检查
//		if (root["logServerPort"].asUInt() > 65535)
//		{
//			strTemp << _T("CWLJsonParse::Setup_GetLogServerByJson, invalid port=")<<root["logServerPort"].asUInt()<<_T(",");
//			goto END;
//		}
//
//		nPort = root["logServerPort"].asUInt();
//
//	}
//
//	bRes = TRUE;
//END:
//	if (pStrErr)
//	{
//		*pStrErr = strTemp.str();
//	}
//
//	if (!bRes)
//	{
//		strIP = _T("");
//	}
//	return bRes;
//}


std::string CWLJsonParse::UnInstall_GetJson(__in tstring ComputerID, __in tstring strUserName, __in WORD cmdType , __in WORD cmdID,BOOL unInstall, BOOL unRegister, DWORD dwResult)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	if(unRegister )
	{
        CMDContent["UNREGISTER"]="SUC";
    }

    if(unInstall && dwResult == 0)
    {
        CMDContent["UNINSTALL"]="SUC";
    }
    else
    {
        CMDContent["UNINSTALL"]="FAIL";
    }

    CMDContent["clientType"]= 1;            //IEG, window no SRS.
	CMDContent["RESULT"] = "SUC";			//java代码修改，跟着修改吧；之前NUL，现在不行了，就随便给个值。
	CMDContent["REASON"] = (int)dwResult;   //反注册或反卸载时，上传USM发生错误的值

    person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["Username"]= UnicodeToUTF8(strUserName);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);

	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::Fush_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, CLIENT_STATUS_STRUCT* pClientStatusStuct, __in vecDiskInfo& vec_DiskInfo, __in BOOL bIEG, __in IWLConfig_SystemCtrl::SystemCtrl_LogServer stcLogServer)
{
	/*
	[
		{
			“ComputerID”:”FEFOEACD”,
			“CMDTYPE”:200,
			“CMDID”:201,
			“CMDContent”:{
			“ComputerName”:”Server-PC”,
			“ComputerIP”:”192.168.1.2”,
			“ComputerMAC”:”00-50-56-C0-00-08”
			“WindowsVersion”:”Windows 7”
			“WindowsX64”:1
			“SelfProtectStatus”:1
			“RealtimeAlertStatus”:1
			“SolidifyStatus”:1,
			“ProcessProtectMode”:1,
			“ProcessControlMode”:1
			“UDiskControlMode”:1
			“SafeUDiskControlMode”:1
			 ...
			  key:value
			  ...
					 }
			}
	]
	*/

	std::string sJsonPacket = "";
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	Json::Value arraydisk;
	Json::Value diskchild;
	Json::Value hardDiskInfo;
	//Json::FastWriter writer;
	int nCount = (int)vec_DiskInfo.size();

	for (int i=0; i< nCount; i++)
	{
		std::wstring strDirve = vec_DiskInfo[i].szDirve;
		//std::wstring strTotalSize = vec_DiskInfo[i].szTotalSize;//´ÅÅÌ×ÜÈÝÁ¿
		//std::wstring strFreeSize = vec_DiskInfo[i].szFreeSize;  //´ÅÅÌÊ£ÓàÈÝÁ¿
		ULONGLONG dwFreeSize = vec_DiskInfo[i].m_uTotalNumberOfFreeBytes.QuadPart/1024/1024;
		ULONGLONG dwTotalSize = vec_DiskInfo[i].m_uTotalNumberOfBytes.QuadPart/1024/1024;

		diskchild["letter"]	= UnicodeToUTF8(strDirve.c_str());
		diskchild["capacity"] = dwTotalSize;
		diskchild["remainder"] = dwFreeSize;
		arraydisk.append(diskchild);
	}

	//hardDiskInfo["hardDiskInfo"] = arraydisk;
	//const string& strlog=hardDiskInfo.toStyledString();
	//std::string strValues =  writer.write(hardDiskInfo);


	std::wstring wsTemp = pClientStatusStuct->szComputerName;
	CMDContent["ComputerName"] = UnicodeToUTF8(wsTemp);

	wsTemp = pClientStatusStuct->szComputerIP;
	CMDContent["ComputerIP"] = UnicodeToUTF8(wsTemp);

	wsTemp = pClientStatusStuct->szComputerMAC;
	if(wsTemp.length()> 0)
		CMDContent["ComputerMAC"] = UnicodeToUTF8(wsTemp);
	else
		CMDContent["ComputerMAC"] = JSON_DEFAULT_STRING;

	wsTemp = pClientStatusStuct->szWindowsVersion;
	CMDContent["WindowsVersion"] = UnicodeToUTF8(wsTemp);

	wsTemp = pClientStatusStuct->szCurVersion;
	if(bIEG)
	{
		CMDContent["szCurVersion"] = UnicodeToUTF8(wsTemp);
		CMDContent["srsVersion"] = UnicodeToUTF8(_T(""));
	}
	else
	{
		CMDContent["szCurVersion"] = UnicodeToUTF8(_T(""));
		CMDContent["srsVersion"] = UnicodeToUTF8(wsTemp);
	}

	CMDContent["WindowsX64"] = (int)pClientStatusStuct->dwWindowsX64;
	CMDContent["SelfProtectStatus"] = (int)pClientStatusStuct->dwSelfProtectStatus;
	CMDContent["RealtimeAlertStatus"] = (int)pClientStatusStuct->dwRealtimeAlertStatus;
	CMDContent["SolidifyStatus"] = (int)pClientStatusStuct->dwSolidifyStatus;
	CMDContent["ProcessProtectMode"] = (int)pClientStatusStuct->dwProcessProtectMode;
	CMDContent["ProcessControlMode"] = (int)pClientStatusStuct->dwProcessControlMode;		// 控制模式
    CMDContent["AutoApproval"] = (int)pClientStatusStuct->dwAutoApproval;		            // 自适应开关（V3R7 - IEG）

    //外设
	CMDContent["UDiskControlMode"] = (int)pClientStatusStuct->dwUDiskControlMode;           //V3R2以后不使用，暂时保留
	CMDContent["SafeUDiskControlMode"] = (int)pClientStatusStuct->dwSafeUDiskControlMode;   //V3R2以后不使用，暂时保留

	CMDContent["CDROMControlMode"] = (int)pClientStatusStuct->dwCDROMControlMode;
	CMDContent["WlanControlMode"] = (int)pClientStatusStuct->dwWlanControlMode;
	CMDContent["UsbEthernetControlMode"] = (int)pClientStatusStuct->dwUsbEthernetControlMode;
    CMDContent["NoRegCommonCtrlMode"] = (int)pClientStatusStuct->dwNoRegCommonCtrlMode;
    CMDContent["NoRegSafeCtrlMode"] = (int)pClientStatusStuct->dwNoRegSafeCtrlMode;
    CMDContent["RegUDiskCtrlMode"] = (int)pClientStatusStuct->dwRegUDiskCtrlMode;
    CMDContent["SafeKillVirusCtrlMode"] = (int)pClientStatusStuct->dwSafeKillVirusCtrlMode;
    CMDContent["CommonKillVirusCtrlMode"] = (int)pClientStatusStuct->dwCommonKillVirusCtrlMode;
    CMDContent["BlueToothControlMode"] = (int)pClientStatusStuct->dwBlueToothControlMode;
    CMDContent["SerialPortControlMode"] = (int)pClientStatusStuct->dwSerialPortControlMode;
    CMDContent["ParallelPortControlMode"] = (int)pClientStatusStuct->dwParallelPortControlMode;
	CMDContent["FloppyDiskControlMode"] = (int)pClientStatusStuct->dwFloppyDiskControlMode;
	CMDContent["UsbDeviceControlMode"] = (int)pClientStatusStuct->dwUsbPortControlMode;
	CMDContent["WpdControlMode"] = (int)pClientStatusStuct->dwWpdControlMode;
    CMDContent["UsbStorageCtrl"] = (int)pClientStatusStuct->dwUsbStorageCtrl;
    CMDContent["WifiWhiteList"] = (int)pClientStatusStuct->dwWiFiWhiteListMode;

	CMDContent["dwSystemCheckStatus"] = (int)pClientStatusStuct->dwSystemCheckStatus;

	CMDContent["dwRegProtectStatus"] = (int)pClientStatusStuct->dwRegProtectStatus;
	CMDContent["dwRegProtectAlarmMode"] = (int)pClientStatusStuct->dwRegProtectAlarmMode;

	CMDContent["dwFilePotectStatus"] = (int)pClientStatusStuct->dwFilePotectStatus;
	CMDContent["dwFileProtectAlarmMode"] = (int)pClientStatusStuct->dwFileProtectAlarmMode;

    CMDContent["dwMACStatus"] = (int)pClientStatusStuct->dwMACStatus;

	CMDContent["dwDataPotectStatus"] = (int)pClientStatusStuct->dwDataPotectStatus;

	CMDContent["dwPasswordComplexity"] = (int)pClientStatusStuct->dwPasswordComplexity;
	CMDContent["dwMin_passwd_len"] = (int)pClientStatusStuct->dwMin_passwd_len;
	CMDContent["dwPassword_hist_len"] = (int)pClientStatusStuct->dwPassword_hist_len;
	CMDContent["dwMax_passwd_age"] = (int)pClientStatusStuct->dwMax_passwd_age;
	CMDContent["dwLockout_threshold"] = (int)pClientStatusStuct->dwLockout_threshold;
	CMDContent["dwDisable_guest"] = (int)pClientStatusStuct->dwDisable_guest;
	CMDContent["dwUsrmod3_lockout_observation_window"] = (int)pClientStatusStuct->dwUsrmod3_lockout_observation_window;
	CMDContent["dwUsrmod3_lockout_duration"] = (int)pClientStatusStuct->dwUsrmod3_lockout_duration;

	CMDContent["dwAuditCategoryLogon"] = (int)pClientStatusStuct->dwAuditCategoryLogon;
	CMDContent["dwAuditCategoryAccountManagement"] = (int)pClientStatusStuct->dwAuditCategoryAccountManagement;
	CMDContent["dwAuditCategoryAccountLogon"] = (int)pClientStatusStuct->dwAuditCategoryAccountLogon;
	CMDContent["dwAuditCategoryObjectAccess"] = (int)pClientStatusStuct->dwAuditCategoryObjectAccess;
	CMDContent["dwAuditCategoryPolicyChange"] = (int)pClientStatusStuct->dwAuditCategoryPolicyChange;
	CMDContent["dwAuditCategoryPrivilegeUse"] = (int)pClientStatusStuct->dwAuditCategoryPrivilegeUse;
	CMDContent["dwAuditCategorySystem"] = (int)pClientStatusStuct->dwAuditCategorySystem;
	CMDContent["dwAuditCategoryDetailedTracking"]		= (int)pClientStatusStuct->dwAuditCategoryDetailedTracking;
	CMDContent["dwAuditCategoryDirectoryServiceAccess"] = (int)pClientStatusStuct->dwAuditCategoryDirectoryServiceAccess;

	CMDContent["dwClearPageShutDown"] = (int)pClientStatusStuct->dwClearPageShutDown;
	CMDContent["dwDontDisplayLastUserName"] = (int)pClientStatusStuct->dwDontDisplayLastUserName;
	CMDContent["dwDisableCAD"] = (int)pClientStatusStuct->dwDisableCAD;
	CMDContent["dwRestrictAnonymousSam"] = (int)pClientStatusStuct->dwRestrictAnonymousSam;
	CMDContent["dwRestrictAnonymous"] = (int)pClientStatusStuct->dwRestrictAnonymous;
	CMDContent["dwAutoRun"] = (int)pClientStatusStuct->dwAutoRun;
	CMDContent["dwShare"] = (int)pClientStatusStuct->dwShare;

	CMDContent["dwApplicationLog"] = (int)pClientStatusStuct->dwApplicationLog;
	CMDContent["dwSeclog"] = (int)pClientStatusStuct->dwSeclog;
	CMDContent["dwSystemLog"] = (int)pClientStatusStuct->dwSystemLog;
	CMDContent["dwCachedLogonsCount"] = (int)pClientStatusStuct->dwCachedLogonsCount;
	CMDContent["dwDisableDomainCreds"] = (int)pClientStatusStuct->dwDisableDomainCreds;
	CMDContent["dwEnableUac"] = (int)pClientStatusStuct->dwEnableUac;
	CMDContent["dwForbidAutoLogin"] = (int)pClientStatusStuct->dwForbidAutoLogin;
	CMDContent["dwForbidAutoReboot"] = (int)pClientStatusStuct->dwForbidAutoReboot;
	CMDContent["dwForbidAutoShutdown"] = (int)pClientStatusStuct->dwForbidAutoShutdown;
	CMDContent["dwForbidChangeIp"] = (int)pClientStatusStuct->dwForbidChangeIp;
	CMDContent["dwForbidChangeName"] = (int)pClientStatusStuct->dwForbidChangeName;
	CMDContent["dwForbidConsoleAutoLogin"] = (int)pClientStatusStuct->dwForbidConsoleAutoLogin;
	CMDContent["dwForbidFloppyCopy"] = (int)pClientStatusStuct->dwForbidFloppyCopy;
	CMDContent["dwForbidGethelp"] = (int)pClientStatusStuct->dwForbidGethelp;
	CMDContent["dwForcedLogoff"] = (int)pClientStatusStuct->dwForcedLogoff;
	CMDContent["dwRdpRortNum"] = (int)pClientStatusStuct->dwRdpRortNum;
	CMDContent["dwDepIn"] = (int)pClientStatusStuct->dwDepIn;
	CMDContent["dwDepOut"] = (int)pClientStatusStuct->dwDepOut;

    CMDContent["dwScrnSave"] = (int)pClientStatusStuct->dwScrnSave;
    CMDContent["dwPasswordExpiryWarning"] = (int)pClientStatusStuct->dwPasswordExpiryWarning;
    CMDContent["dwDeleteIpForwardEntry"] = (int)pClientStatusStuct->dwDeleteIpForwardEntry;
    CMDContent["dwForbidAdminToTurnOff"] = (int)pClientStatusStuct->dwForbidAdminToTurnOff;
    CMDContent["dwRemoteHostRDP"] = (int)pClientStatusStuct->dwRemoteHostRDP;
    CMDContent["dwRemoteLoginTime"] = (int)pClientStatusStuct->dwRemoteLoginTime;
    CMDContent["dwForbidDefaultOPTUser"] = (int)pClientStatusStuct->dwForbidDefaultOPTUser;
    CMDContent["dwSynAttackDetectionDesign"] = (int)pClientStatusStuct->dwSynAttackDetectionDesign;

	CMDContent["dwSynIpDefence"] = (int)pClientStatusStuct->dwSynIpDefence;
	CMDContent["dwFireWall"] = (int)pClientStatusStuct->dwFireWall;
	CMDContent["dwFireWallState"] = (int)pClientStatusStuct->dwFireWallState;

	CMDContent["dwIsUpLoadSysLog"] = (int)pClientStatusStuct->dwIsUpLoadSysLog;
	CMDContent["dwUpLoadSysLogCycleDay"] = (int)pClientStatusStuct->dwUpLoadSysLogCycleDay;

	CMDContent["dwIsProcessAudit"] = (int)pClientStatusStuct->dwIsProcessAudit;

	// 增加  上传速度状态 -- add by jian.ding 2020.10.31
	CMDContent["uploadSpeed"] = (int)pClientStatusStuct->dwUploadSpeed;

	// 增加白名单扫描速度状态  	 	  -- add by jian.ding 2020.10.31
	CMDContent["scanControl"] = (int)pClientStatusStuct->dwWhiteListSpeedScanControl;

	// 增加漏洞防护状态  -- add by jian.ding 2020.10.31
	CMDContent["loopholeProtect"] = (int)pClientStatusStuct->dwVulProtection;

    // 检测周期
    CMDContent["SecDetectCheckStatus"] = (int)pClientStatusStuct->dwSecDetectCheckStatus;
    CMDContent["SecDetectTimeType"] = (int)pClientStatusStuct->dwSecDetectTimeType;
    CMDContent["SecDectDay"] = (int)pClientStatusStuct->dwSecDetectDay;
    CMDContent["SecDectHour"] = (int)pClientStatusStuct->dwSecDetectHour;
    CMDContent["SecDectMinute"] = (int)pClientStatusStuct->dwSecDetectMinute;

    // 非法外联
    CMDContent["IllegalConnect"] = (int)pClientStatusStuct->dwIllegalConnect;

	CMDContent["hardDiskInfo"] = arraydisk;
    CMDContent["dwMaintainUkeyStatus"] = (int)pClientStatusStuct->dwMaintainUkeyStatus;

	// 上报系统白名单开关
	CMDContent["UploadSysWLFile"] = (int)pClientStatusStuct->dwSysUploadSwitch;

	// 添加日志服务器信息
	// 当日志服务器类型为https时（即未配置日志服务器），LogServerIP和LogServerPort应该为空
	if (stcLogServer.emType == IWLConfig_SystemCtrl::em_udp)
	{
		std::wstring wsLogServerIP = stcLogServer.szIp;
		CMDContent["LogServerIP"] = UnicodeToUTF8(wsLogServerIP);
		CMDContent["LogServerPort"] = (int)stcLogServer.nPort;
	}
	else
	{
		// 未配置日志服务器时，字段为空
		CMDContent["LogServerIP"] = "";
		CMDContent["LogServerPort"] = 0;
	}

	person["CMDContent"]=(Json::Value)CMDContent;	//ÕâÑù·ûºÏÒªÇó
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}



std::string CWLJsonParse::baseLine_PL_Account_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_ACCOUNT_STRUCT* pBaseLinePlAccount)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	CMDContent["dwPasswordComplexity"] = (int)pBaseLinePlAccount->dwPasswordComplexity;
	CMDContent["dwMin_passwd_len"] = (int)pBaseLinePlAccount->dwMin_passwd_len;
	CMDContent["dwPassword_hist_len"] = (int)pBaseLinePlAccount->dwPassword_hist_len;

	CMDContent["dwMax_passwd_age"] = (int)pBaseLinePlAccount->dwMax_passwd_age;
	CMDContent["dwLockout_threshold"] = (int)pBaseLinePlAccount->dwLockout_threshold;
	CMDContent["dwDisable_guest"] = (int)pBaseLinePlAccount->dwDisable_guest;

	CMDContent["dwUsrmod3_lockout_observation_window"] = (int)pBaseLinePlAccount->dwUsrmod3_lockout_observation_window;
	CMDContent["dwUsrmod3_lockout_duration"] = (int)pBaseLinePlAccount->dwUsrmod3_lockout_duration;

	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_Account_GetValue(__in std::string& sJson, __out BASELINE_PL_ACCOUNT_STRUCT* pBaseLinePlAccount)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	root = (Json::Value)root[0]["CMDContent"];
	if( root == NULL)
	{
		goto _exist_;
	}

	if( root.isMember("dwPasswordComplexity") )
	{
		pBaseLinePlAccount->dwPasswordComplexity = root["dwPasswordComplexity"].asInt();
	}

	if( root.isMember("dwMin_passwd_len") )
	{
		pBaseLinePlAccount->dwMin_passwd_len = root["dwMin_passwd_len"].asInt();
	}

	if( root.isMember("dwPassword_hist_len") )
	{
		pBaseLinePlAccount->dwPassword_hist_len = root["dwPassword_hist_len"].asInt();
	}

	if( root.isMember("dwMax_passwd_age") )
	{
		pBaseLinePlAccount->dwMax_passwd_age = root["dwMax_passwd_age"].asInt();
	}

	if( root.isMember("dwLockout_threshold") )
	{
		pBaseLinePlAccount->dwLockout_threshold = root["dwLockout_threshold"].asInt();
	}

	if( root.isMember("dwDisable_guest") )
	{
		pBaseLinePlAccount->dwDisable_guest = root["dwDisable_guest"].asInt();
	}

	if( root.isMember("dwUsrmod3_lockout_observation_window") )
	{
		pBaseLinePlAccount->dwUsrmod3_lockout_observation_window = root["dwUsrmod3_lockout_observation_window"].asInt();
	}
	if( root.isMember("dwUsrmod3_lockout_duration") )
	{
		pBaseLinePlAccount->dwUsrmod3_lockout_duration = root["dwUsrmod3_lockout_duration"].asInt();
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

// 审核策略
std::string CWLJsonParse::baseLine_PL_Audit_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_AUDIT_STRUCT* pBaseLinePlAduit)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	CMDContent["dwAuditCategoryLogon"]					= (int)pBaseLinePlAduit->dwAuditCategoryLogon;
	CMDContent["dwAuditCategoryAccountManagement"]		= (int)pBaseLinePlAduit->dwAuditCategoryAccountManagement;
	CMDContent["dwAuditCategoryAccountLogon"]			= (int)pBaseLinePlAduit->dwAuditCategoryAccountLogon;
	CMDContent["dwAuditCategoryObjectAccess"]			= (int)pBaseLinePlAduit->dwAuditCategoryObjectAccess;
	CMDContent["dwAuditCategoryPolicyChange"]			= (int)pBaseLinePlAduit->dwAuditCategoryPolicyChange;
	CMDContent["dwAuditCategoryPrivilegeUse"]			= (int)pBaseLinePlAduit->dwAuditCategoryPrivilegeUse;
	CMDContent["dwAuditCategorySystem"]					= (int)pBaseLinePlAduit->dwAuditCategorySystem;
	CMDContent["dwAuditCategoryDetailedTracking"]		= (int)pBaseLinePlAduit->dwAuditCategoryDetailedTracking;
	CMDContent["dwAuditCategoryDirectoryServiceAccess"] = (int)pBaseLinePlAduit->dwAuditCategoryDirectoryServiceAccess;

	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_Audit_GetValue(__in std::string& sJson, __out BASELINE_PL_AUDIT_STRUCT* pBaseLinePlAduit)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	root = (Json::Value)root[0]["CMDContent"];
	if( root == NULL)
	{
		goto _exist_;
	}

	if( root.isMember("dwAuditCategoryLogon") )
	{
		pBaseLinePlAduit->dwAuditCategoryLogon = root["dwAuditCategoryLogon"].asInt();
	}

	if( root.isMember("dwAuditCategoryAccountManagement") )
	{
		pBaseLinePlAduit->dwAuditCategoryAccountManagement = root["dwAuditCategoryAccountManagement"].asInt();
	}

	if( root.isMember("dwAuditCategoryAccountLogon") )
	{
		pBaseLinePlAduit->dwAuditCategoryAccountLogon = root["dwAuditCategoryAccountLogon"].asInt();
	}

	if( root.isMember("dwAuditCategoryObjectAccess") )
	{
		pBaseLinePlAduit->dwAuditCategoryObjectAccess = root["dwAuditCategoryObjectAccess"].asInt();
	}

	if( root.isMember("dwAuditCategoryPolicyChange") )
	{
		pBaseLinePlAduit->dwAuditCategoryPolicyChange = root["dwAuditCategoryPolicyChange"].asInt();
	}

	if( root.isMember("dwAuditCategoryPrivilegeUse") )
	{
		pBaseLinePlAduit->dwAuditCategoryPrivilegeUse = root["dwAuditCategoryPrivilegeUse"].asInt();
	}

	if( root.isMember("dwAuditCategorySystem") )
	{
		pBaseLinePlAduit->dwAuditCategorySystem = root["dwAuditCategorySystem"].asInt();
	}

	if( root.isMember("dwAuditCategoryDetailedTracking") )
	{
		pBaseLinePlAduit->dwAuditCategoryDetailedTracking = root["dwAuditCategoryDetailedTracking"].asInt();
	}
	if( root.isMember("dwAuditCategoryDirectoryServiceAccess") )
	{
		pBaseLinePlAduit->dwAuditCategoryDirectoryServiceAccess = root["dwAuditCategoryDirectoryServiceAccess"].asInt();
	}

	bResult = TRUE;

_exist_:

	return bResult;
}


// 安全选项策略
std::string CWLJsonParse::baseLine_PL_SecurityOptions_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_SECURITY_OPTIONS_STRUCT *pBaseLinePlSecurityOptions)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	CMDContent["dwClearPageShutDown"] = (int)pBaseLinePlSecurityOptions->dwClearPageShutDown;
	CMDContent["dwDontDisplayLastUserName"] = (int)pBaseLinePlSecurityOptions->dwDontDisplayLastUserName;
	CMDContent["dwDisableCAD"] = (int)pBaseLinePlSecurityOptions->dwDisableCAD;

	CMDContent["dwRestrictAnonymousSam"] = (int)pBaseLinePlSecurityOptions->dwRestrictAnonymousSam;
	CMDContent["dwRestrictAnonymous"] = (int)pBaseLinePlSecurityOptions->dwRestrictAnonymous;
	CMDContent["dwAutoRun"] = (int)pBaseLinePlSecurityOptions->dwAutoRun;

	CMDContent["dwShare"] = (int)pBaseLinePlSecurityOptions->dwShare;

	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_SecurityOptions_GetValue(__in std::string& sJson, __out BASELINE_PL_SECURITY_OPTIONS_STRUCT *pBaseLinePlSecurityOptions)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	root = (Json::Value)root[0]["CMDContent"];
	if( root == NULL)
	{
		goto _exist_;
	}

	if( root.isMember("dwClearPageShutDown") )
	{
		pBaseLinePlSecurityOptions->dwClearPageShutDown = root["dwClearPageShutDown"].asInt();
	}

	if( root.isMember("dwDontDisplayLastUserName") )
	{
		pBaseLinePlSecurityOptions->dwDontDisplayLastUserName = root["dwDontDisplayLastUserName"].asInt();
	}

	if( root.isMember("dwDisableCAD") )
	{
		pBaseLinePlSecurityOptions->dwDisableCAD = root["dwDisableCAD"].asInt();
	}

	if( root.isMember("dwRestrictAnonymousSam") )
	{
		pBaseLinePlSecurityOptions->dwRestrictAnonymousSam = root["dwRestrictAnonymousSam"].asInt();
	}

	if( root.isMember("dwRestrictAnonymous") )
	{
		pBaseLinePlSecurityOptions->dwRestrictAnonymous = root["dwRestrictAnonymous"].asInt();
	}

	if( root.isMember("dwAutoRun") )
	{
		pBaseLinePlSecurityOptions->dwAutoRun = root["dwAutoRun"].asInt();
	}

	if( root.isMember("dwShare") )
	{
		pBaseLinePlSecurityOptions->dwShare = root["dwShare"].asInt();
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

// 新安全基线策略
std::string CWLJsonParse::baseLine_PL_New_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_NEW_ST *pstBasePlNew, DWORD dwLevel, std::string *strTime)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	CMDContent["dwPasswordComplexity"]                 = (int)pstBasePlNew->dwPasswordComplexity;
	CMDContent["dwMin_passwd_len"]                     = (int)pstBasePlNew->dwMin_passwd_len;
	CMDContent["dwPassword_hist_len"]                  = (int)pstBasePlNew->dwPassword_hist_len;
	CMDContent["dwMax_passwd_age"]                     = (int)pstBasePlNew->dwMax_passwd_age;
	CMDContent["dwLockout_threshold"]                  = (int)pstBasePlNew->dwLockout_threshold;
	CMDContent["dwDisable_guest"]                      = (int)pstBasePlNew->dwDisable_guest;
	CMDContent["dwUsrmod3_lockout_observation_window"] = (int)pstBasePlNew->dwUsrmod3_lockout_observation_window;
	CMDContent["dwUsrmod3_lockout_duration"]		   = (int)pstBasePlNew->dwUsrmod3_lockout_duration;

	CMDContent["dwAuditCategoryLogon"]                  = (int)pstBasePlNew->dwAuditCategoryLogon;
	CMDContent["dwAuditCategoryAccountManagement"]      = (int)pstBasePlNew->dwAuditCategoryAccountManagement;
	CMDContent["dwAuditCategoryAccountLogon"]           = (int)pstBasePlNew->dwAuditCategoryAccountLogon;
	CMDContent["dwAuditCategoryObjectAccess"]           = (int)pstBasePlNew->dwAuditCategoryObjectAccess;
	CMDContent["dwAuditCategoryPolicyChange"]           = (int)pstBasePlNew->dwAuditCategoryPolicyChange;
	CMDContent["dwAuditCategoryPrivilegeUse"]           = (int)pstBasePlNew->dwAuditCategoryPrivilegeUse;
	CMDContent["dwAuditCategorySystem"]                 = (int)pstBasePlNew->dwAuditCategorySystem;
	CMDContent["dwAuditCategoryDetailedTracking"]		     = (int)pstBasePlNew->dwAuditCategoryDetailedTracking;
	CMDContent["dwAuditCategoryDirectoryServiceAccess"] = (int)pstBasePlNew->dwAuditCategoryDirectoryServiceAccess;

	CMDContent["dwClearPageShutDown"]                   = (int)pstBasePlNew->dwClearPageShutDown;
	CMDContent["dwDontDisplayLastUserName"]             = (int)pstBasePlNew->dwDontDisplayLastUserName;
	CMDContent["dwDisableCAD"]                          = (int)pstBasePlNew->dwDisableCAD;
	CMDContent["dwRestrictAnonymousSam"]                = (int)pstBasePlNew->dwRestrictAnonymousSam;
	CMDContent["dwRestrictAnonymous"]                   = (int)pstBasePlNew->dwRestrictAnonymous;
	CMDContent["dwAutoRun"]                             = (int)pstBasePlNew->dwAutoRun;
	CMDContent["dwShare"]                               = (int)pstBasePlNew->dwShare;

	CMDContent["dwApplicationLog"]         = (int)pstBasePlNew->dwApplicationLog;
	CMDContent["dwSeclog"]                 = (int)pstBasePlNew->dwSeclog;
	CMDContent["dwSystemLog"]              = (int)pstBasePlNew->dwSystemLog;
	CMDContent["dwCachedLogonsCount"]      = (int)pstBasePlNew->dwCachedLogonsCount;
	CMDContent["dwDisableDomainCreds"]     = (int)pstBasePlNew->dwDisableDomainCreds;
	CMDContent["dwEnableUac"]              = (int)pstBasePlNew->dwEnableUac;
	CMDContent["dwForbidAutoLogin"]        = (int)pstBasePlNew->dwForbidAutoLogin;
	CMDContent["dwForbidAutoReboot"]       = (int)pstBasePlNew->dwForbidAutoReboot;
	CMDContent["dwForbidAutoShutdown"]     = (int)pstBasePlNew->dwForbidAutoShutdown;
	CMDContent["dwForbidChangeIp"]         = (int)pstBasePlNew->dwForbidChangeIp;
	CMDContent["dwForbidChangeName"]       = (int)pstBasePlNew->dwForbidChangeName;
	CMDContent["dwForbidConsoleAutoLogin"] = (int)pstBasePlNew->dwForbidConsoleAutoLogin;
	CMDContent["dwForbidFloppyCopy"]       = (int)pstBasePlNew->dwForbidFloppyCopy;
	CMDContent["dwForbidGethelp"]          = (int)pstBasePlNew->dwForbidGethelp;
	CMDContent["dwForcedLogoff"]           = (int)pstBasePlNew->dwForcedLogoff;
	CMDContent["dwRdpRortNum"]             = (int)pstBasePlNew->dwRdpRortNum;
	CMDContent["dwDepIn"]                  = (int)pstBasePlNew->dwDepIn;
	CMDContent["dwDepOut"]                 = (int)pstBasePlNew->dwDepOut;

	// new add by mingming.shi
	CMDContent["dwDeleteIpForwardEntry"]	= (int)pstBasePlNew->dwDeleteIpForwardEntry;
	CMDContent["dwRemoteHostRDP"]			= (int)pstBasePlNew->dwRemoteHostRDP;
	CMDContent["dwRemoteLoginTime"]			= (int)pstBasePlNew->dwRemoteLoginTime;
	CMDContent["dwScrnSave"]				= (int)pstBasePlNew->dwScrnSave;
	//CMDContent["dwFireWall"]				= (int)pstBasePlNew->dwFireWall;
	CMDContent["dwForbidAdminToTurnOff"]	= (int)pstBasePlNew->dwForbidAdminToTurnOff;
	CMDContent["dwSynAttackDetectionDesign"]= (int)pstBasePlNew->dwSynAttackDetectionDesign;
	CMDContent["dwPasswordExpiryWarning"]	= (int)pstBasePlNew->dwPasswordExpiryWarning;
    CMDContent["dwForbidDefaultOPTUser"]    = (int)pstBasePlNew->dwForbidDefaultOPTUser;

	person["CMDContent"] = (Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"] = UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]    = (int)cmdType;
	person["CMDID"]      = (int)cmdID;
	person["LEVEL"]      = (int)dwLevel;

	//if ( !strTime.empty())
	//{
	//	person["LastHardenTime"]      = strTime;
	//}

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::baseLine_PL_New_Status_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID,__in BASELINE_PL_NEW_ST *pSecbStatus, __in BASELINE_PL_NEW_ST *pstBasePlNew, DWORD dwLevel)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	const int maxValLen = 16;
	char szVal[maxValLen] = {0};

    /*%d,%d 状态，勾选状态（有自定义值的项，此处为具体值）*/
	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwPasswordComplexity, (int)pstBasePlNew->dwPasswordComplexity);
	CMDContent["dwPasswordComplexity"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwMin_passwd_len, (int)pstBasePlNew->dwMin_passwd_len);
	CMDContent["dwMin_passwd_len"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwPassword_hist_len, (int)pstBasePlNew->dwPassword_hist_len);
	CMDContent["dwPassword_hist_len"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwMax_passwd_age, (int)pstBasePlNew->dwMax_passwd_age);
	CMDContent["dwMax_passwd_age"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwLockout_threshold, (int)pstBasePlNew->dwLockout_threshold);
	CMDContent["dwLockout_threshold"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDisable_guest, (int)pstBasePlNew->dwDisable_guest);
	CMDContent["dwDisable_guest"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwUsrmod3_lockout_observation_window, (int)pstBasePlNew->dwUsrmod3_lockout_observation_window);
	CMDContent["dwUsrmod3_lockout_observation_window"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwUsrmod3_lockout_duration, (int)pstBasePlNew->dwUsrmod3_lockout_duration);
	CMDContent["dwUsrmod3_lockout_duration"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryLogon, (int)pstBasePlNew->dwAuditCategoryLogon);
	CMDContent["dwAuditCategoryLogon"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryAccountManagement, (int)pstBasePlNew->dwAuditCategoryAccountManagement);
	CMDContent["dwAuditCategoryAccountManagement"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryAccountLogon, (int)pstBasePlNew->dwAuditCategoryAccountLogon);
	CMDContent["dwAuditCategoryAccountLogon"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryObjectAccess, (int)pstBasePlNew->dwAuditCategoryObjectAccess);
	CMDContent["dwAuditCategoryObjectAccess"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryPolicyChange, (int)pstBasePlNew->dwAuditCategoryPolicyChange);
	CMDContent["dwAuditCategoryPolicyChange"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryPrivilegeUse, (int)pstBasePlNew->dwAuditCategoryPrivilegeUse);
	CMDContent["dwAuditCategoryPrivilegeUse"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategorySystem, (int)pstBasePlNew->dwAuditCategorySystem);
	CMDContent["dwAuditCategorySystem"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryDetailedTracking, (int)pstBasePlNew->dwAuditCategoryDetailedTracking);
	CMDContent["dwAuditCategoryDetailedTracking"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAuditCategoryDirectoryServiceAccess, (int)pstBasePlNew->dwAuditCategoryDirectoryServiceAccess);
	CMDContent["dwAuditCategoryDirectoryServiceAccess"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwClearPageShutDown, (int)pstBasePlNew->dwClearPageShutDown);
	CMDContent["dwClearPageShutDown"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDontDisplayLastUserName, (int)pstBasePlNew->dwDontDisplayLastUserName);
	CMDContent["dwDontDisplayLastUserName"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDisableCAD, (int)pstBasePlNew->dwDisableCAD);
	CMDContent["dwDisableCAD"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwRestrictAnonymousSam, (int)pstBasePlNew->dwRestrictAnonymousSam);
	CMDContent["dwRestrictAnonymousSam"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwRestrictAnonymous, (int)pstBasePlNew->dwRestrictAnonymous);
	CMDContent["dwRestrictAnonymous"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwAutoRun, (int)pstBasePlNew->dwAutoRun);
	CMDContent["dwAutoRun"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwShare, (int)pstBasePlNew->dwShare);
	CMDContent["dwShare"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwApplicationLog, (int)pstBasePlNew->dwApplicationLog);
	CMDContent["dwApplicationLog"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwSeclog, (int)pstBasePlNew->dwSeclog);
	CMDContent["dwSeclog"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwSystemLog, (int)pstBasePlNew->dwSystemLog);
	CMDContent["dwSystemLog"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwCachedLogonsCount, (int)pstBasePlNew->dwCachedLogonsCount);
	CMDContent["dwCachedLogonsCount"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDisableDomainCreds, (int)pstBasePlNew->dwDisableDomainCreds);
	CMDContent["dwDisableDomainCreds"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwEnableUac, (int)pstBasePlNew->dwEnableUac);
	CMDContent["dwEnableUac"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidAutoLogin, (int)pstBasePlNew->dwForbidAutoLogin);
	CMDContent["dwForbidAutoLogin"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidAutoReboot, (int)pstBasePlNew->dwForbidAutoReboot);
	CMDContent["dwForbidAutoReboot"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidAutoShutdown, (int)pstBasePlNew->dwForbidAutoShutdown);
	CMDContent["dwForbidAutoShutdown"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidChangeIp, (int)pstBasePlNew->dwForbidChangeIp);
	CMDContent["dwForbidChangeIp"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidChangeName, (int)pstBasePlNew->dwForbidChangeName);
	CMDContent["dwForbidChangeName"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidConsoleAutoLogin, (int)pstBasePlNew->dwForbidConsoleAutoLogin);
	CMDContent["dwForbidConsoleAutoLogin"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidFloppyCopy, (int)pstBasePlNew->dwForbidFloppyCopy);
	CMDContent["dwForbidFloppyCopy"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidGethelp, (int)pstBasePlNew->dwForbidGethelp);
	CMDContent["dwForbidGethelp"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForcedLogoff, (int)pstBasePlNew->dwForcedLogoff);
	CMDContent["dwForcedLogoff"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwRdpRortNum, (int)pstBasePlNew->dwRdpRortNum);
	CMDContent["dwRdpRortNum"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDepIn, (int)pstBasePlNew->dwDepIn);
	CMDContent["dwDepIn"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDepOut, (int)pstBasePlNew->dwDepOut);
	CMDContent["dwDepOut"] = szVal;

	// new add by mingming.shi 2021-09-14
	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwPasswordExpiryWarning, (int)pstBasePlNew->dwPasswordExpiryWarning);
	CMDContent["dwPasswordExpiryWarning"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwDeleteIpForwardEntry, (int)pstBasePlNew->dwDeleteIpForwardEntry);
	CMDContent["dwDeleteIpForwardEntry"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwRemoteHostRDP, (int)pstBasePlNew->dwRemoteHostRDP);
	CMDContent["dwRemoteHostRDP"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwRemoteLoginTime, (int)pstBasePlNew->dwRemoteLoginTime);
	CMDContent["dwRemoteLoginTime"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwScrnSave, (int)pstBasePlNew->dwScrnSave);
	CMDContent["dwScrnSave"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidAdminToTurnOff, (int)pstBasePlNew->dwForbidAdminToTurnOff);
	CMDContent["dwForbidAdminToTurnOff"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwSynAttackDetectionDesign, (int)pstBasePlNew->dwSynAttackDetectionDesign);
	CMDContent["dwSynAttackDetectionDesign"] = szVal;

	_snprintf_s(szVal, maxValLen,  _TRUNCATE, "%d,%d", (int)pSecbStatus->dwForbidDefaultOPTUser, (int)pstBasePlNew->dwForbidDefaultOPTUser);
	CMDContent["dwForbidDefaultOPTUser"] = szVal;

	person["CMDContent"]=(Json::Value)CMDContent;	//这样符合要求
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["LEVEL"] = (int)dwLevel;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_New_Status_GetValue(__in std::string& sJson, __out BASELINE_PL_NEW_ST *pSecbStatus, __out BASELINE_PL_NEW_ST *pSecParam, __out DWORD &dwLevel)
{
    BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root1;
    Json::Value person;

	Json::FastWriter writer;
	Json::Reader reader;

    std::wstring wsContent = _T("");
	std::string	strValue = "";

    std::vector< std::wstring> vecDest;

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("LEVEL"))
	{
		dwLevel =  root[0]["LEVEL"].asInt();
	}

	root = (Json::Value)root[0]["CMDContent"];
	if( root == NULL)
	{
		goto _exist_;
	}

	// Accont
	if( root.isMember("dwPasswordComplexity") )
	{
        wsContent = UTF8ToUnicode(root["dwPasswordComplexity"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwPasswordComplexity = _wtoi(vecDest[0].c_str());
            pSecParam->dwPasswordComplexity = _wtoi(vecDest[1].c_str());
        }
		
	}
	if( root.isMember("dwMin_passwd_len") )
	{
        wsContent = UTF8ToUnicode(root["dwMin_passwd_len"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwMin_passwd_len = _wtoi(vecDest[0].c_str());
            pSecParam->dwMin_passwd_len = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwPassword_hist_len") )
	{
        wsContent = UTF8ToUnicode(root["dwPassword_hist_len"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwPassword_hist_len = _wtoi(vecDest[0].c_str());
            pSecParam->dwPassword_hist_len = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwMax_passwd_age") )
	{
        wsContent = UTF8ToUnicode(root["dwMax_passwd_age"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwMax_passwd_age = _wtoi(vecDest[0].c_str());
            pSecParam->dwMax_passwd_age = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwLockout_threshold") )
	{
        wsContent = UTF8ToUnicode(root["dwLockout_threshold"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwLockout_threshold = _wtoi(vecDest[0].c_str());
            pSecParam->dwLockout_threshold = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDisable_guest") )
	{
        wsContent = UTF8ToUnicode(root["dwDisable_guest"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDisable_guest = _wtoi(vecDest[0].c_str());
            pSecParam->dwDisable_guest = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwUsrmod3_lockout_observation_window") )
	{
        wsContent = UTF8ToUnicode(root["dwUsrmod3_lockout_observation_window"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwUsrmod3_lockout_observation_window = _wtoi(vecDest[0].c_str());
            pSecParam->dwUsrmod3_lockout_observation_window = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwUsrmod3_lockout_duration") )
	{
        wsContent = UTF8ToUnicode(root["dwUsrmod3_lockout_duration"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwUsrmod3_lockout_duration = _wtoi(vecDest[0].c_str());
            pSecParam->dwUsrmod3_lockout_duration = _wtoi(vecDest[1].c_str());
        }
	}

	// Audit
	if( root.isMember("dwAuditCategoryLogon") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryLogon"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryLogon = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryLogon = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryAccountManagement") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryAccountManagement"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryAccountManagement = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryAccountManagement = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryAccountLogon") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryAccountLogon"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryAccountLogon = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryAccountLogon = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryObjectAccess") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryObjectAccess"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryObjectAccess = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryObjectAccess = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryPolicyChange") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryPolicyChange"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryPolicyChange = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryPolicyChange = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryPrivilegeUse") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryPrivilegeUse"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryPrivilegeUse = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryPrivilegeUse = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategorySystem") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategorySystem"].asString()).c_str();
        WLUtils::StringSplit(wsContent, _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategorySystem = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategorySystem = _wtoi(vecDest[1].c_str());
        }
	}

	if( root.isMember("dwAuditCategoryDetailedTracking") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryDetailedTracking"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryDetailedTracking = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryDetailedTracking = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwAuditCategoryDirectoryServiceAccess") )
	{
        wsContent = UTF8ToUnicode(root["dwAuditCategoryDirectoryServiceAccess"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAuditCategoryDirectoryServiceAccess = _wtoi(vecDest[0].c_str());
            pSecParam->dwAuditCategoryDirectoryServiceAccess = _wtoi(vecDest[1].c_str());
        }
	}

	// Security
	if( root.isMember("dwClearPageShutDown") )
	{
        wsContent = UTF8ToUnicode(root["dwClearPageShutDown"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwClearPageShutDown = _wtoi(vecDest[0].c_str());
            pSecParam->dwClearPageShutDown = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDontDisplayLastUserName") )
	{
        wsContent = UTF8ToUnicode(root["dwDontDisplayLastUserName"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDontDisplayLastUserName = _wtoi(vecDest[0].c_str());
            pSecParam->dwDontDisplayLastUserName = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDisableCAD") )
	{
        wsContent = UTF8ToUnicode(root["dwDisableCAD"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDisableCAD = _wtoi(vecDest[0].c_str());
            pSecParam->dwDisableCAD = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwRestrictAnonymousSam") )
	{
        wsContent = UTF8ToUnicode(root["dwRestrictAnonymousSam"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwRestrictAnonymousSam = _wtoi(vecDest[0].c_str());
            pSecParam->dwRestrictAnonymousSam = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwRestrictAnonymous") )
	{
        wsContent = UTF8ToUnicode(root["dwRestrictAnonymous"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwRestrictAnonymous = _wtoi(vecDest[0].c_str());
            pSecParam->dwRestrictAnonymous = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwAutoRun") )
	{
        wsContent = UTF8ToUnicode(root["dwAutoRun"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwAutoRun = _wtoi(vecDest[0].c_str());
            pSecParam->dwAutoRun = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwShare") )
	{
        wsContent = UTF8ToUnicode(root["dwShare"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwShare = _wtoi(vecDest[0].c_str());
            pSecParam->dwShare = _wtoi(vecDest[1].c_str());
        }
	}

	// New
	if( root.isMember("dwForbidAutoReboot") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidAutoReboot"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidAutoReboot = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidAutoReboot = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidAutoShutdown") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidAutoShutdown"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidAutoShutdown = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidAutoShutdown = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidChangeIp") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidChangeIp"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidChangeIp = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidChangeIp = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidChangeName") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidChangeName"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidChangeName = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidChangeName = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidConsoleAutoLogin") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidConsoleAutoLogin"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidConsoleAutoLogin = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidConsoleAutoLogin = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidFloppyCopy") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidFloppyCopy"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidFloppyCopy = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidFloppyCopy = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidGethelp") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidGethelp"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidGethelp = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidGethelp = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForcedLogoff") )
	{
        wsContent = UTF8ToUnicode(root["dwForcedLogoff"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForcedLogoff = _wtoi(vecDest[0].c_str());
            pSecParam->dwForcedLogoff = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwRdpRortNum") )
	{
        wsContent = UTF8ToUnicode(root["dwRdpRortNum"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwRdpRortNum = _wtoi(vecDest[0].c_str());
            pSecParam->dwRdpRortNum = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwApplicationLog") )
	{
        wsContent = UTF8ToUnicode(root["dwApplicationLog"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwApplicationLog = _wtoi(vecDest[0].c_str());
            pSecParam->dwApplicationLog = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwSeclog") )
	{
        wsContent = UTF8ToUnicode(root["dwSeclog"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwSeclog = _wtoi(vecDest[0].c_str());
            pSecParam->dwSeclog = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwSystemLog") )
	{
        wsContent = UTF8ToUnicode(root["dwSystemLog"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwSystemLog = _wtoi(vecDest[0].c_str());
            pSecParam->dwSystemLog = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwCachedLogonsCount") )
	{
        wsContent = UTF8ToUnicode(root["dwCachedLogonsCount"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwCachedLogonsCount = _wtoi(vecDest[0].c_str());
            pSecParam->dwCachedLogonsCount = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDisableDomainCreds") )
	{
        wsContent = UTF8ToUnicode(root["dwDisableDomainCreds"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDisableDomainCreds = _wtoi(vecDest[0].c_str());
            pSecParam->dwDisableDomainCreds = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwEnableUac") )
	{
        wsContent = UTF8ToUnicode(root["dwEnableUac"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwEnableUac = _wtoi(vecDest[0].c_str());
            pSecParam->dwEnableUac = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidAutoLogin") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidAutoLogin"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidAutoLogin = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidAutoLogin = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDepIn") )
	{
        wsContent = UTF8ToUnicode(root["dwDepIn"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDepIn = _wtoi(vecDest[0].c_str());
            pSecParam->dwDepIn = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDepOut") )
	{
        wsContent = UTF8ToUnicode(root["dwDepOut"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDepOut = _wtoi(vecDest[0].c_str());
            pSecParam->dwDepOut = _wtoi(vecDest[1].c_str());
        }
	}

	// new add by mingming.shi 2021-09-14
	if( root.isMember("dwPasswordExpiryWarning") )
	{
        wsContent = UTF8ToUnicode(root["dwPasswordExpiryWarning"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwPasswordExpiryWarning = _wtoi(vecDest[0].c_str());
            pSecParam->dwPasswordExpiryWarning = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwDeleteIpForwardEntry") )
	{
        wsContent = UTF8ToUnicode(root["dwDeleteIpForwardEntry"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwDeleteIpForwardEntry = _wtoi(vecDest[0].c_str());
            pSecParam->dwDeleteIpForwardEntry = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwRemoteHostRDP") )
	{
        wsContent = UTF8ToUnicode(root["dwRemoteHostRDP"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwRemoteHostRDP = _wtoi(vecDest[0].c_str());
            pSecParam->dwRemoteHostRDP = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwRemoteLoginTime") )
	{
        wsContent = UTF8ToUnicode(root["dwRemoteLoginTime"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwRemoteLoginTime = _wtoi(vecDest[0].c_str());
            pSecParam->dwRemoteLoginTime = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwScrnSave") )
	{
        wsContent = UTF8ToUnicode(root["dwScrnSave"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwScrnSave = _wtoi(vecDest[0].c_str());
            pSecParam->dwScrnSave = _wtoi(vecDest[1].c_str());
        }
	}
	/*if( root.isMember("dwFireWall") )
	{
        wsContent = UTF8ToUnicode(root["dwFireWall"].asString()).c_str();
        WLUtils::StringSplit(wsContent, _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwFireWall = _wtoi(vecDest[0].c_str());
            pSecParam->dwFireWall = _wtoi(vecDest[1].c_str());
        }
	}*/
	if( root.isMember("dwForbidAdminToTurnOff") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidAdminToTurnOff"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidAdminToTurnOff = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidAdminToTurnOff = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwSynAttackDetectionDesign") )
	{
        wsContent = UTF8ToUnicode(root["dwSynAttackDetectionDesign"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwSynAttackDetectionDesign = _wtoi(vecDest[0].c_str());
            pSecParam->dwSynAttackDetectionDesign = _wtoi(vecDest[1].c_str());
        }
	}
	if( root.isMember("dwForbidDefaultOPTUser") )
	{
        wsContent = UTF8ToUnicode(root["dwForbidDefaultOPTUser"].asString()).c_str();
        WLUtils::StringSplit(wsContent.c_str(), _T(","), vecDest);

        if ( vecDest.size() != 0)
        {
            pSecbStatus->dwForbidDefaultOPTUser = _wtoi(vecDest[0].c_str());
            pSecParam->dwForbidDefaultOPTUser = _wtoi(vecDest[1].c_str());
        }
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

BOOL CWLJsonParse::baseLine_PL_New_GetValue(__in std::string& sJson, __out BASELINE_PL_NEW_ST *pstBasePlNew, __out DWORD &dwLevel)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root1;
	Json::FastWriter writer;
	Json::Value person;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("LEVEL"))
	{
		dwLevel =  root[0]["LEVEL"].asInt();
	}

	root = (Json::Value)root[0]["CMDContent"];
	if( root == NULL)
	{
		goto _exist_;
	}

	// Accont
	if( root.isMember("dwPasswordComplexity") )
	{
		pstBasePlNew->dwPasswordComplexity = root["dwPasswordComplexity"].asInt();
	}
	if( root.isMember("dwMin_passwd_len") )
	{
		pstBasePlNew->dwMin_passwd_len = root["dwMin_passwd_len"].asInt();
	}
	if( root.isMember("dwPassword_hist_len") )
	{
		pstBasePlNew->dwPassword_hist_len = root["dwPassword_hist_len"].asInt();
	}
	if( root.isMember("dwMax_passwd_age") )
	{
		pstBasePlNew->dwMax_passwd_age = root["dwMax_passwd_age"].asInt();
	}
	if( root.isMember("dwLockout_threshold") )
	{
		pstBasePlNew->dwLockout_threshold = root["dwLockout_threshold"].asInt();
	}
	if( root.isMember("dwDisable_guest") )
	{
		pstBasePlNew->dwDisable_guest = root["dwDisable_guest"].asInt();
	}
	if( root.isMember("dwUsrmod3_lockout_observation_window") )
	{
		pstBasePlNew->dwUsrmod3_lockout_observation_window = root["dwUsrmod3_lockout_observation_window"].asInt();
	}
	if( root.isMember("dwUsrmod3_lockout_duration") )
	{
		pstBasePlNew->dwUsrmod3_lockout_duration = root["dwUsrmod3_lockout_duration"].asInt();
	}

	// Audit
	if( root.isMember("dwAuditCategoryLogon") )
	{
		pstBasePlNew->dwAuditCategoryLogon = root["dwAuditCategoryLogon"].asInt();
	}

	if( root.isMember("dwAuditCategoryAccountManagement") )
	{
		pstBasePlNew->dwAuditCategoryAccountManagement = root["dwAuditCategoryAccountManagement"].asInt();
	}

	if( root.isMember("dwAuditCategoryAccountLogon") )
	{
		pstBasePlNew->dwAuditCategoryAccountLogon = root["dwAuditCategoryAccountLogon"].asInt();
	}

	if( root.isMember("dwAuditCategoryObjectAccess") )
	{
		pstBasePlNew->dwAuditCategoryObjectAccess = root["dwAuditCategoryObjectAccess"].asInt();
	}

	if( root.isMember("dwAuditCategoryPolicyChange") )
	{
		pstBasePlNew->dwAuditCategoryPolicyChange = root["dwAuditCategoryPolicyChange"].asInt();
	}

	if( root.isMember("dwAuditCategoryPrivilegeUse") )
	{
		pstBasePlNew->dwAuditCategoryPrivilegeUse = root["dwAuditCategoryPrivilegeUse"].asInt();
	}

	if( root.isMember("dwAuditCategorySystem") )
	{
		pstBasePlNew->dwAuditCategorySystem = root["dwAuditCategorySystem"].asInt();
	}

	if( root.isMember("dwAuditCategoryDetailedTracking") )
	{
		pstBasePlNew->dwAuditCategoryDetailedTracking = root["dwAuditCategoryDetailedTracking"].asInt();
	}
	if( root.isMember("dwAuditCategoryDirectoryServiceAccess") )
	{
		pstBasePlNew->dwAuditCategoryDirectoryServiceAccess = root["dwAuditCategoryDirectoryServiceAccess"].asInt();
	}

	// Security
	if( root.isMember("dwClearPageShutDown") )
	{
		pstBasePlNew->dwClearPageShutDown = root["dwClearPageShutDown"].asInt();
	}
	if( root.isMember("dwDontDisplayLastUserName") )
	{
		pstBasePlNew->dwDontDisplayLastUserName = root["dwDontDisplayLastUserName"].asInt();
	}
	if( root.isMember("dwDisableCAD") )
	{
		pstBasePlNew->dwDisableCAD = root["dwDisableCAD"].asInt();
	}
	if( root.isMember("dwRestrictAnonymousSam") )
	{
		pstBasePlNew->dwRestrictAnonymousSam = root["dwRestrictAnonymousSam"].asInt();
	}
	if( root.isMember("dwRestrictAnonymous") )
	{
		pstBasePlNew->dwRestrictAnonymous = root["dwRestrictAnonymous"].asInt();
	}
	if( root.isMember("dwAutoRun") )
	{
		pstBasePlNew->dwAutoRun = root["dwAutoRun"].asInt();
	}
	if( root.isMember("dwShare") )
	{
		pstBasePlNew->dwShare = root["dwShare"].asInt();
	}

	// New
	if( root.isMember("dwForbidAutoReboot") )
	{
		pstBasePlNew->dwForbidAutoReboot = root["dwForbidAutoReboot"].asInt();
	}
	if( root.isMember("dwForbidAutoShutdown") )
	{
		pstBasePlNew->dwForbidAutoShutdown = root["dwForbidAutoShutdown"].asInt();
	}
    if( root.isMember("dwForbidChangeIp") )
    {
        //pstBasePlNew->dwForbidChangeIp = root["dwForbidChangeIp"].asInt();
        pstBasePlNew->dwForbidChangeIp = 0; //该项不实现，所以开关状态永远为0
    }
	if( root.isMember("dwForbidChangeName") )
	{
		pstBasePlNew->dwForbidChangeName = root["dwForbidChangeName"].asInt();
	}
	if( root.isMember("dwForbidConsoleAutoLogin") )
	{
		pstBasePlNew->dwForbidConsoleAutoLogin = root["dwForbidConsoleAutoLogin"].asInt();
	}
	if( root.isMember("dwForbidFloppyCopy") )
	{
		pstBasePlNew->dwForbidFloppyCopy = root["dwForbidFloppyCopy"].asInt();
	}
	if( root.isMember("dwForbidGethelp") )
	{
		pstBasePlNew->dwForbidGethelp = root["dwForbidGethelp"].asInt();
	}
	if( root.isMember("dwForcedLogoff") )
	{
		pstBasePlNew->dwForcedLogoff = root["dwForcedLogoff"].asInt();
	}
	if( root.isMember("dwRdpRortNum") )
	{
		pstBasePlNew->dwRdpRortNum = root["dwRdpRortNum"].asInt();
	}
	if( root.isMember("dwApplicationLog") )
	{
		pstBasePlNew->dwApplicationLog = root["dwApplicationLog"].asInt();
	}
	if( root.isMember("dwSeclog") )
	{
		pstBasePlNew->dwSeclog = root["dwSeclog"].asInt();
	}
	if( root.isMember("dwSystemLog") )
	{
		pstBasePlNew->dwSystemLog = root["dwSystemLog"].asInt();
	}
	if( root.isMember("dwCachedLogonsCount") )
	{
		pstBasePlNew->dwCachedLogonsCount = root["dwCachedLogonsCount"].asInt();
	}
	if( root.isMember("dwDisableDomainCreds") )
	{
		pstBasePlNew->dwDisableDomainCreds = root["dwDisableDomainCreds"].asInt();
	}
	if( root.isMember("dwEnableUac") )
	{
		pstBasePlNew->dwEnableUac = root["dwEnableUac"].asInt();
	}
	if( root.isMember("dwForbidAutoLogin") )
	{
		pstBasePlNew->dwForbidAutoLogin = root["dwForbidAutoLogin"].asInt();
	}
	if( root.isMember("dwDepIn") )
	{
		pstBasePlNew->dwDepIn = root["dwDepIn"].asInt();
	}
	if( root.isMember("dwDepOut") )
	{
		pstBasePlNew->dwDepOut = root["dwDepOut"].asInt();
	}

	// new add by mingming.shi 2021-09-14
	if( root.isMember("dwPasswordExpiryWarning") )
	{
		pstBasePlNew->dwPasswordExpiryWarning = root["dwPasswordExpiryWarning"].asInt();
	}
	if( root.isMember("dwDeleteIpForwardEntry") )
	{
		pstBasePlNew->dwDeleteIpForwardEntry = root["dwDeleteIpForwardEntry"].asInt();
	}
	if( root.isMember("dwRemoteHostRDP") )
	{
		pstBasePlNew->dwRemoteHostRDP = root["dwRemoteHostRDP"].asInt();
	}
	if( root.isMember("dwRemoteLoginTime") )
	{
		pstBasePlNew->dwRemoteLoginTime = root["dwRemoteLoginTime"].asInt();
	}
	if( root.isMember("dwScrnSave") )
	{
		pstBasePlNew->dwScrnSave = root["dwScrnSave"].asInt();
	}
	/*if( root.isMember("dwFireWall") )
	{
		pstBasePlNew->dwFireWall = root["dwFireWall"].asInt();
	}*/
	if( root.isMember("dwForbidAdminToTurnOff") )
	{
		pstBasePlNew->dwForbidAdminToTurnOff = root["dwForbidAdminToTurnOff"].asInt();
	}
	if( root.isMember("dwSynAttackDetectionDesign") )
	{
		pstBasePlNew->dwSynAttackDetectionDesign = root["dwSynAttackDetectionDesign"].asInt();
	}
	if( root.isMember("dwForbidDefaultOPTUser") )
	{
		pstBasePlNew->dwForbidDefaultOPTUser = root["dwForbidDefaultOPTUser"].asInt();
	}

	bResult = TRUE;

_exist_:

	return bResult;
}

// IP安全策略
std::string CWLJsonParse::baseLine_PL_SynIP_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_IP_STRUCT* pBaseLinePlIP)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";


	Json::Value root1;
	Json::Value root2;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent_1;
	Json::Value CMDContent_2;


	int nCount = (int)pBaseLinePlIP->vecRuleName_Path.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring s1 = pBaseLinePlIP->vecRuleName_Path[i]->RuleName;
		std::wstring s2 = pBaseLinePlIP->vecRuleName_Path[i]->FullPath;

		CMDContent_1["RuleName"] =  UnicodeToUTF8(s1);
		CMDContent_1["FullPath"] = UnicodeToUTF8(s2);

		root1.append(CMDContent_1);
	}

	nCount = (int)pBaseLinePlIP->vecRuleName_PortType_Port.size();
	for (i=0; i< nCount; i++)
	{
		std::wstring s1 = pBaseLinePlIP->vecRuleName_PortType_Port[i]->RuleName;

		CMDContent_2["RuleName"] = UnicodeToUTF8(s1);
		CMDContent_2["PORT_TYPE"] = (int) pBaseLinePlIP->vecRuleName_PortType_Port[i]->dwPortType;
		CMDContent_2["PORT_NUM"]  = (int) pBaseLinePlIP->vecRuleName_PortType_Port[i]->dwPort;

		root2.append(CMDContent_2);
	}


	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["dwSynIpDefence"] =(int)pBaseLinePlIP->dwSynIpDefence;
	person["dwFireWall"] =(int)pBaseLinePlIP->dwFireWall;
	person["dwFireWallState"] = (int)pBaseLinePlIP->dwFireWallState;

	person["CMDContent_1"] = (Json::Value)root1;
	person["CMDContent_2"] = (Json::Value)root2;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_SynIP_GetValue(__in std::string& sJson, __out  BASELINE_PL_IP_STRUCT* pBaseLinePlIP)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}


	if( root[0].isMember("addOrFull"))
	{
		pBaseLinePlIP->dwAddOrFull=root[0]["addOrFull"].asInt();
	}
	else
	{
		pBaseLinePlIP->dwAddOrFull=UFUM_REPLACE;
	}

	if( root[0].isMember("section"))
	{
		pBaseLinePlIP->dwSection=root[0]["section"].asInt();
	}
	else
	{
		pBaseLinePlIP->dwSection=UFSM_ALL;
	}

	if( root[0].isMember("ComputerID"))
	{
		pBaseLinePlIP->wsComputerID =  UTF8ToUnicode(root[0]["ComputerID"].asString());
	}
	else
	{
		pBaseLinePlIP->wsComputerID = _T("LOCAL");
	}

	if( root[0].isMember("dwSynIpDefence"))
	{
		pBaseLinePlIP->dwSynIpDefence =  root[0]["dwSynIpDefence"].asInt();
	}
	else
	{
		pBaseLinePlIP->dwSynIpDefence = 0;
	}

	if( root[0].isMember("dwFireWall"))
	{
		pBaseLinePlIP->dwFireWall =  root[0]["dwFireWall"].asInt();
	}
	else
	{
		pBaseLinePlIP->dwFireWall = 0;
	}

	if( root[0].isMember("dwFireWallState"))
	{
		pBaseLinePlIP->dwFireWallState =  root[0]["dwFireWallState"].asInt();
	}
	else
	{
		pBaseLinePlIP->dwFireWallState = 0;
	}

	if( root[0].isMember("CMDContent_1"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent_1"]; //进行解析

		// 进行解析
		nObject = root_1.size();
		std::wstring wsRuleName;
		std::wstring wsFullPath;
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				wsRuleName = UTF8ToUnicode(root_1[i]["RuleName"].asString());
				wsFullPath = UTF8ToUnicode(root_1[i]["FullPath"].asString());

				SYN_IP_FIREWARE_STRUCT* p = new SYN_IP_FIREWARE_STRUCT;
				memset(p, 0, sizeof(SYN_IP_FIREWARE_STRUCT));
				_tcscpy(p->RuleName, wsRuleName.c_str());
				_tcscpy(p->FullPath, wsFullPath.c_str());

				pBaseLinePlIP->vecRuleName_Path.push_back(p);
			}
		}
		
	}

	if( root[0].isMember("CMDContent_2"))
	{
		root_2 =  (Json::Value)root[0]["CMDContent_2"]; //进行解析
		nObject = root_2.size();

		std::wstring wsRuleName;
		if (root_2.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				SYN_IP_FIREWARE_STRUCT* p = new SYN_IP_FIREWARE_STRUCT;
				memset(p, 0, sizeof(SYN_IP_FIREWARE_STRUCT));

				wsRuleName = UTF8ToUnicode(root_2[i]["RuleName"].asString());
				_tcscpy(p->RuleName, wsRuleName.c_str());
				p->dwPort = root_2[i]["PORT_NUM"].asInt();
				p->dwPortType = root_2[i]["PORT_TYPE"].asInt();

				pBaseLinePlIP->vecRuleName_PortType_Port.push_back(p);
			}
		}
		
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::baseLine_PL_SynIP_GetExJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BASELINE_PL_IP_STRUCT* pBaseLinePlIP)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";


    Json::Value root1;
    Json::Value root2;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value CMDContent_1;
    Json::Value CMDContent_2;


    int nCount = (int)pBaseLinePlIP->vecRuleName_Path.size();
    int i=0;
    for (i=0; i< nCount; i++)
    {
        std::wstring s1 = pBaseLinePlIP->vecRuleName_Path[i]->RuleName;
        std::wstring s2 = pBaseLinePlIP->vecRuleName_Path[i]->FullPath;

        CMDContent_1["RuleName"] =  UnicodeToUTF8(s1);
        CMDContent_1["FullPath"] = UnicodeToUTF8(s2);

        root1.append(CMDContent_1);
    }

    nCount = (int)pBaseLinePlIP->vecRuleName_PortType_Port.size();
    for (i=0; i< nCount; i++)
    {
        std::wstring s1 = pBaseLinePlIP->vecRuleName_PortType_Port[i]->RuleName;

        CMDContent_2["RuleName"] = UnicodeToUTF8(s1);
        CMDContent_2["PORT_TYPE"] = (int) pBaseLinePlIP->vecRuleName_PortType_Port[i]->dwPortType;
        CMDContent_2["PORT_NUM"]  = (int) pBaseLinePlIP->vecRuleName_PortType_Port[i]->dwPort;

        root2.append(CMDContent_2);
    }


    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;

    person["CMDContent_1"] = (Json::Value)root1;
    person["CMDContent_2"] = (Json::Value)root2;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::baseLine_PL_SynIP_GetExValue(__in std::string& sJson, __out BASELINE_PL_IP_STRUCT* pBaseLinePlIP)
{
    BOOL bResult = FALSE;

    Json::Value root;
    Json::Value root_1;
    Json::Value root_2;
    Json::Reader	reader;
    std::string		strValue = "";

    if( sJson.length() == 0)
    {
        goto _exist_;
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        goto _exist_;
    }


    if( root[0].isMember("addOrFull"))
    {
        pBaseLinePlIP->dwAddOrFull=root[0]["addOrFull"].asInt();
    }
    else
    {
        pBaseLinePlIP->dwAddOrFull=UFUM_REPLACE;
    }

    if( root[0].isMember("section"))
    {
        pBaseLinePlIP->dwSection=root[0]["section"].asInt();
    }
    else
    {
        pBaseLinePlIP->dwSection=UFSM_ALL;
    }

    if( root[0].isMember("ComputerID"))
    {
        pBaseLinePlIP->wsComputerID =  UTF8ToUnicode(root[0]["ComputerID"].asString());
    }
    else
    {
        pBaseLinePlIP->wsComputerID = _T("LOCAL");
    }

    if( root[0].isMember("CMDContent_1"))
    {
        root_1 =  (Json::Value)root[0]["CMDContent_1"]; //进行解析

        // 进行解析
        nObject = root_1.size();
        std::wstring wsRuleName;
        std::wstring wsFullPath;
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				wsRuleName = UTF8ToUnicode(root_1[i]["RuleName"].asString());
				wsFullPath = UTF8ToUnicode(root_1[i]["FullPath"].asString());

				SYN_IP_FIREWARE_STRUCT* p = new SYN_IP_FIREWARE_STRUCT;
				memset(p, 0, sizeof(SYN_IP_FIREWARE_STRUCT));
				_tcscpy(p->RuleName, wsRuleName.c_str());
				_tcscpy(p->FullPath, wsFullPath.c_str());

				pBaseLinePlIP->vecRuleName_Path.push_back(p);
			}
		}
        
    }

    if( root[0].isMember("CMDContent_2"))
    {
        root_2 =  (Json::Value)root[0]["CMDContent_2"]; //进行解析
        nObject = root_2.size();

        std::wstring wsRuleName;
		if (root_2.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				SYN_IP_FIREWARE_STRUCT* p = new SYN_IP_FIREWARE_STRUCT;
				memset(p, 0, sizeof(SYN_IP_FIREWARE_STRUCT));

				wsRuleName = UTF8ToUnicode(root_2[i]["RuleName"].asString());
				_tcscpy(p->RuleName, wsRuleName.c_str());
				p->dwPort = root_2[i]["PORT_NUM"].asInt();
				p->dwPortType = root_2[i]["PORT_TYPE"].asInt();

				pBaseLinePlIP->vecRuleName_PortType_Port.push_back(p);
			}
		}
        
    }

    bResult = TRUE;

_exist_:
    return bResult;
}

std::string CWLJsonParse::baseLine_FireWallLog_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in HOSTBASELINE_IPSEC_LOG_STRUCT* pFireWallLog)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::Value root1;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;



	std::wstring wsTemp =  _T("");
	wsTemp = convertTimeTToStr((DWORD)pFireWallLog->llTime);
	CMDContent["Time"] = UnicodeToUTF8(wsTemp);
	CMDContent["Type"] = (int)pFireWallLog->uType;

	std::wstring s1 = pFireWallLog->szLogContent;
	CMDContent["LogContent"] = UnicodeToUTF8(s1);
	root1.append(CMDContent);

	person["CMDContent"]=(Json::Value)root1;	//这样符合要求,用数组表示
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

// 强制删除文件
std::string CWLJsonParse::secondary_ForceDelFile_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in SECONDARY_PL_FORCE_DEL_FILE* pSecondaryLinePlForceDelFile)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pSecondaryLinePlForceDelFile->vecFile.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring wsTemp = pSecondaryLinePlForceDelFile->vecFile[i];

		CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);

		root1.append(CMDContent);
	}

	//nCount = pSecondaryLinePlForceDelFile->vecDir.size();
	//for (i=0; i< nCount; i++)
	//{
	//	std::wstring wsTemp = pSecondaryLinePlForceDelFile->vecDir[i];

	//	CMDContent["DELETE_FILE_TYPE"] = (int)2;
	//	CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);

	//	root1.append(CMDContent);
	//}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::secondary_ForceDelFile_GetValue(__in std::string& sJson, __out SECONDARY_PL_FORCE_DEL_FILE* pSecondaryLinePlForceDelFile)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;

	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		// 进行解析
		std::wstring strFullPath = _T("");
		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				strFullPath =  UTF8ToUnicode(root_1[i]["FullPath"].asString());
				pSecondaryLinePlForceDelFile->vecFile.push_back(strFullPath);
			}
		}

	}

	bResult = TRUE;

_exist_:
	return bResult;
}

// 主机审计（白名单内，本次列表覆盖上一次列表）
std::string CWLJsonParse::host_AD_Process_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in HOST_AD_PROCESS_STRUCT* pHostAdProcess)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root2;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pHostAdProcess->vecProcess.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring wsTemp =  pHostAdProcess->vecProcess[i];
		CMDContent["FileName"]=UnicodeToUTF8(wsTemp);
		root1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Enable"]=(int)pHostAdProcess->dwIsAudtiProtect;
	person["KeepOldAuditSwitch"]=(int)pHostAdProcess->dwKeepOldAuditSwitch;
	person["JustReload"]=(int)pHostAdProcess->dwJustReload;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::host_AD_Process_GetValue(__in std::string& sJson, __out HOST_AD_PROCESS_STRUCT* pHostAdProcess)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	//Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("addOrFull"))
	{
		pHostAdProcess->dwAddOrFull=root[0]["addOrFull"].asInt();
	}
	else
	{
		pHostAdProcess->dwAddOrFull=UFUM_REPLACE;
	}

	if( root[0].isMember("section"))
	{
		pHostAdProcess->dwSection=root[0]["section"].asInt();
	}
	else
	{
		pHostAdProcess->dwSection=UFSM_ALL;
	}

	if( root[0].isMember("Enable"))
	{
		pHostAdProcess->dwIsAudtiProtect = (DWORD)root[0]["Enable"].asInt();
	}
	else
	{
		pHostAdProcess->dwIsAudtiProtect = 0;
	}

    if( root[0].isMember("KeepOldAuditSwitch"))
	{
		pHostAdProcess->dwKeepOldAuditSwitch = (DWORD)root[0]["KeepOldAuditSwitch"].asInt();
	}
	else
	{
		pHostAdProcess->dwKeepOldAuditSwitch = 0;
	}

    if( root[0].isMember("JustReload"))
	{
		pHostAdProcess->dwJustReload = (DWORD)root[0]["JustReload"].asInt();
	}
	else
	{
		pHostAdProcess->dwJustReload = 0;
	}

	if( root[0].isMember("ComputerID") )
	{
		pHostAdProcess->wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString());
	}
	else
	{
		pHostAdProcess->wsComputerID = _T("LOCAL");
	}


	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		// 进行解析
		std::wstring strFileName = _T("");
		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				strFileName =  UTF8ToUnicode(root_1[i]["FileName"].asString());
				// 新版本与旧版本比较Modify by fanl
				// 如果文件名没有路径，表示是旧版本的数据，则不添加进列表
				//std::wstring wsDisk = strFileName.substr(0,3);

				//if (DRIVE_FIXED != GetDriveType(wsDisk.c_str()))
				//{
				// 路径不是本地磁盘的，都不加入
				//continue;
				//}

				pHostAdProcess->vecProcess.push_back(strFileName);
			}
		}
		
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

// 系统日志
/*
std::string CWLJsonParse::host_AD_SysLog_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in  HOST_AD_SYSLOG_STRUCT* pHostAdSysLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	//每次传递一条
	CMDContent["dwEventClass"] = (int)pHostAdSysLog->dwEventClass;
	CMDContent["dwEventType"] = (int)pHostAdSysLog->dwEventType;
	CMDContent["dwEventID"] = (int)pHostAdSysLog->dwEventID;

	std::wstring wsTemp = convertTimeTToStr((DWORD)pHostAdSysLog->llEventTime);
	CMDContent["EventTime"] = UnicodeToUTF8(wsTemp);


	CMDContent["SourceName"] = UnicodeToUTF8(pHostAdSysLog->wsEventSourceName);

	CMDContent["Description"] = UnicodeToUTF8(pHostAdSysLog->wsEventDescription);
	CMDContent["ComputerName"] =UnicodeToUTF8(pHostAdSysLog->wsEventComputerName);;

	root1.append(CMDContent);


	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}
*/

/*
std::string CWLJsonParse::host_AD_SysLog_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in  std::vector<HOST_AD_SYSLOG_STRUCT*>& vecSysLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = vecSysLog.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		HOST_AD_SYSLOG_STRUCT *pSysLog = vecSysLog[i];

		CMDContent["dwEventClass"] = (int)pSysLog->dwEventClass;
		CMDContent["dwEventType"] = (int)pSysLog->dwEventType;
		CMDContent["dwEventID"] = (int)pSysLog->dwEventID;

		std::wstring wsTemp = convertTimeTToStr((DWORD)pSysLog->llEventTime);
		CMDContent["EventTime"] = UnicodeToUTF8(wsTemp);


		CMDContent["SourceName"] = UnicodeToUTF8(pSysLog->wsEventSourceName);

		CMDContent["Description"] = UnicodeToUTF8(pSysLog->wsEventDescription);
		CMDContent["ComputerName"] =UnicodeToUTF8(pSysLog->wsEventComputerName);;

		root1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}
*/


/*
BOOL CWLJsonParse::host_AD_SysLog_GetValue(__in std::string& sJson, __out std::vector<HOST_AD_SYSLOG_STRUCT>& vecSysLog)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1)
	{
		goto _exist_;
	}

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		std::wstring ws = _T("");
		nObject = root_1.size();
		for (int i=0; i<nObject; i++)
		{
			HOST_AD_SYSLOG_STRUCT sysLog;

			sysLog.dwEventClass = (int)root_1[i]["dwEventClass"].asInt();
			sysLog.dwEventType = (int)root_1[i]["dwEventType"].asInt();
			sysLog.dwEventID = (int)root_1[i]["dwEventID"].asInt();

			ws = UTF8ToUnicode(root_1[i]["EventTime"].asString());
			time_t t = 0;
			convertStrToTimeT(t, ws);
			sysLog.llEventTime = (LONGLONG)t;

			ws = UTF8ToUnicode(root_1[i]["SourceName"].asString());
			_tcscpy(sysLog.wsEventSourceName, ws.c_str());

			ws = UTF8ToUnicode(root_1[i]["Description"].asString());
			_tcscpy(sysLog.wsEventDescription, ws.c_str());

			ws = UTF8ToUnicode(root_1[i]["ComputerName"].asString());
			_tcscpy(sysLog.wsEventComputerName, ws.c_str());

			vecSysLog.push_back(sysLog);
		}
	}
	bResult = TRUE;

_exist_:
	return bResult;
}
*/


BOOL CWLJsonParse::host_AD_SysLog_GetCycle(__in std::string& sJson, __out DWORD& dwIsUpLoad, __out DWORD& dwCycleDay)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	dwIsUpLoad = (DWORD)root[0]["Enable"].asInt();
	root = (Json::Value)root[0]["CMDContent"];
	dwCycleDay = (DWORD)root["dwCycleDay"].asInt();

	bResult = TRUE;
_exist_:
	return bResult;
}

BOOL CWLJsonParse::hostDefence_CheckSysLoadFile_GetValue(__in std::string& sJson, __out DWORD& dwIsCheckSysLoadFile, __out std::wstring& wsComputerID)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString()); //区分是否是客户端
	dwIsCheckSysLoadFile = (DWORD)root[0]["Enable"].asInt(); // 注意 Enable
	root = (Json::Value)root[0]["CMDContent"];

	bResult = TRUE;
_exist_:
	return bResult;
}

BOOL CWLJsonParse::hostDefence_SysUploadSwitch_GetValue(__in std::string& sJson, __out DWORD& dwIsSysUploadSwitch, __out std::wstring& wsComputerID)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString()); //区分是否是客户端
	dwIsSysUploadSwitch = (DWORD)root[0]["Enable"].asInt(); // 注意 Enable
	root = (Json::Value)root[0]["CMDContent"];

	bResult = TRUE;
_exist_:
	return bResult;
}

std::string CWLJsonParse::ProcessWhite_upLoad_CheckSys_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BOOL bEnable)
{
	std::string sJsonPacket = "";

	Json::Value rootContent;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	rootContent.append(CMDContent);

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Enable"]=(int)bEnable;
	person["CMDContent"] = (Json::Value)rootContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

// 上报系统白名单开关的接口
std::string CWLJsonParse::ProcessWhite_upLoad_SysUpload_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BOOL bEnable)
{
	std::string sJsonPacket = "";

	Json::Value rootContent;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	rootContent.append(CMDContent);

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["Enable"]=(int)bEnable;
	person["CMDContent"] = (Json::Value)rootContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

//【主机加固】- 上传系统加载文件
std::string CWLJsonParse::hostDefence_upLoad_CheckSysLoadFile_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, HOST_DEFENCE_SYS_LOADFILE_STRUCT* pHostDefenceSysLoadFile)
{
	std::string sJsonPacket = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pHostDefenceSysLoadFile->vecSysLoadFile.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring wsFileName =  pHostDefenceSysLoadFile->vecSysLoadFile[i];
		CMDContent["FullPath"] = UnicodeToUTF8(wsFileName);
		root1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::hostDefence_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType, __in WORD cmdID,
                       __in vector<CWLMetaData*>& vecHostDefenceLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;
    //SYSTEM_INTEGRALITY_PROTECT_LOG_STRUCT_EX*  pHostDefenceLog = NULL;
	int nCount = (int)vecHostDefenceLog.size();
	if( nCount == 0)
		return sJsonPacket;

    for(int i = 0;i<nCount;i++)
    {
    	//每次传递一条

        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecHostDefenceLog[i]->GetData();
        SYSTEM_INTEGRALITY_PROTECT_LOG_STRUCT *pHostDefenceLog = (SYSTEM_INTEGRALITY_PROTECT_LOG_STRUCT*)pipclogcomm->data;
    	std::wstring wsTemp = _T("");
    	CMDContent["LogType"] = (int)pipclogcomm->dwDetailLogTypeLevel2;// 日志类型 (1：文件，2: 注册表, 3:完整性, 4:强制文件保护);

        //new (2019.07.16)表示是否被阻止
        CMDContent["Block"] = (int)pHostDefenceLog->dwHoldBack;

    	wsTemp = convertTimeTToStr((DWORD)pHostDefenceLog->llTime);
    	CMDContent["Time"] = UnicodeToUTF8(wsTemp);

    	wsTemp = pHostDefenceLog->szProtectPath;
    	CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);


    	wsTemp = pHostDefenceLog->szLogContent;
    	CMDContent["LogContent"] = UnicodeToUTF8(wsTemp);

    	wsTemp = pHostDefenceLog->process;
    	CMDContent["ProcessName"] = UnicodeToUTF8(wsTemp);

    	wsTemp = pHostDefenceLog->szlogon;
    	CMDContent["Username"] = UnicodeToUTF8(wsTemp);

    	root1.append(CMDContent);
    }
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}
std::string CWLJsonParse::hostDefence_FileRegSysLoad_GetJson(__in tstring ComputerID, __in WORD cmdType, __in WORD cmdID, SYSTEM_INTEGRALITY_PROTECT_LOG_STRUCT_EX*  pHostDefenceLog)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	//每次传递一条
	std::wstring wsTemp = _T("");
	CMDContent["LogType"] = (int)pHostDefenceLog->dwLogStyle;

    //new (2019.07.16)表示是否被阻止
    CMDContent["Block"] = (int)pHostDefenceLog->dwHoldBack;

	wsTemp = convertTimeTToStr((DWORD)pHostDefenceLog->llTime);
	CMDContent["Time"] = UnicodeToUTF8(wsTemp);

	wsTemp = pHostDefenceLog->szProtectPath;
	CMDContent["FullPath"] = UnicodeToUTF8(wsTemp);


	wsTemp = pHostDefenceLog->szLogContent;
	CMDContent["LogContent"] = UnicodeToUTF8(wsTemp);

	wsTemp = pHostDefenceLog->process;
	CMDContent["ProcessName"] = UnicodeToUTF8(wsTemp);

	wsTemp = pHostDefenceLog->szlogon;
	CMDContent["Username"] = UnicodeToUTF8(wsTemp);

	root1.append(CMDContent);

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}


BOOL CWLJsonParse::hostDefence_CheckFileProtect_GetValue(__in std::string& sJson, __out HOST_DEFENCE_FILE_STRUCT* pHostDefenceFile)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exit_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	if (!reader.parse(strValue, root))
	{
		goto _exit_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exit_;
	}

	if( root[0].isMember("Enable") ) // 注意 Enable
	{
		pHostDefenceFile->dwEnable = (DWORD)root[0]["Enable"].asInt();
	}
	else
	{
		pHostDefenceFile->dwEnable = 0;
	}

	if( root[0].isMember("alarmMode") )
	{
		pHostDefenceFile->dwAlarmMode = (DWORD)root[0]["alarmMode"].asInt();
	}
	else
	{
		pHostDefenceFile->dwAlarmMode = UFAM_AUDIT_AND_DENY;
	}

	if( root[0].isMember("addOrFull") )
	{
		pHostDefenceFile->dwAddOrFull = (DWORD)root[0]["addOrFull"].asInt();
	}
	else
	{
		pHostDefenceFile->dwAddOrFull = UFUM_REPLACE;
	}

	if( root[0].isMember("section"))
	{
		pHostDefenceFile->dwSection=root[0]["section"].asInt();
	}
	else
	{
		pHostDefenceFile->dwSection=UFSM_ALL;
	}

	if( root[0].isMember("ComputerID") )
	{
		pHostDefenceFile->wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString());
	}
	else
	{
		pHostDefenceFile->wsComputerID = _T("LOCAL");
	}


	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		nObject = root_1.size();
		if (nObject > MAX_FILEPROTECT_COUNT)
		{
			goto _exit_;
		}
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				std::wstring wsFullPath = UTF8ToUnicode(root_1[i]["FullPath"].asString());
				pHostDefenceFile->vecFile.push_back(wsFullPath);
			}
		}
		
	}

	bResult = TRUE;
_exit_:
	return bResult;
}

std::string  CWLJsonParse::hostDefence_CheckFileProtect_GetJson(__in tstring ComputerID, __in WORD cmdType, __in WORD cmdID, __in HOST_DEFENCE_FILE_STRUCT* pHostDefenceFile, BOOL bException)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::Value person;
	Json::Value ObjectRule;

	Json::FastWriter writer;

	/* 处理例外规则信息 */
	int iCount = (int)pHostDefenceFile->vecFile.size();
	for (int i=0; i< iCount; i++)
	{
		ObjectRule["FullPath"]	   = UnicodeToUTF8(pHostDefenceFile->vecFile[i]);

		root1.append(ObjectRule);
	}


	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;

	if(!bException)
	{
		person["Enable"]		= (int)pHostDefenceFile->dwEnable;
		person["alarmMode"]		= (int)pHostDefenceFile->dwAlarmMode;
	}

	person["CMDContent"]  = (Json::Value)root1;
	// BUG ID:29475 - Jian.Ding - 2020.12.29
	person["addOrFull"] = (int)pHostDefenceFile->dwAddOrFull;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}



BOOL CWLJsonParse::hostDefence_CheckRegProtect_GetValue(__in std::string& sJson, __out HOST_DEFENCE_REG_STRUCT* pHostDefenceReg)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("Enable") )
	{
		pHostDefenceReg->dwEnable = (DWORD)root[0]["Enable"].asInt();
	}
	else
	{
		pHostDefenceReg->dwEnable = 0;
	}

	if( root[0].isMember("alarmMode") )
	{
		pHostDefenceReg->dwAlarmMode = (DWORD)root[0]["alarmMode"].asInt();
	}
	else
	{
		pHostDefenceReg->dwAlarmMode = UFAM_AUDIT_AND_DENY;
	}

	if( root[0].isMember("addOrFull") )
	{
		pHostDefenceReg->dwAddOrFull = (DWORD)root[0]["addOrFull"].asInt();
	}
	else
	{
		pHostDefenceReg->dwAddOrFull = UFUM_REPLACE;
	}

	if( root[0].isMember("section"))
	{
		pHostDefenceReg->dwSection=root[0]["section"].asInt();
	}
	else
	{
		pHostDefenceReg->dwSection=UFSM_ALL;
	}

	if( root[0].isMember("ComputerID") )
	{
		pHostDefenceReg->wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString());
	}
	else
	{
		pHostDefenceReg->wsComputerID = _T("LOCAL");
	}


	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				HOST_DEFENCE_REG_ITEM stuItem;
				stuItem.wsPath = UTF8ToUnicode(root_1[i]["RegItem"].asString());
				stuItem.iType = (DWORD)root_1[i]["RegType"].asInt();

				pHostDefenceReg->vecReg.push_back(stuItem);
			}
		}
		
	}

	bResult = TRUE;
_exist_:
	return bResult;
}

std::string CWLJsonParse::hostDefence_CheckRegProtect_GetJson(__in tstring ComputerID, __in WORD cmdType, __in WORD cmdID, __in HOST_DEFENCE_REG_STRUCT* pHostDefenceReg)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::Value person;
	Json::Value ObjectRule;

	Json::FastWriter writer;

	/* 处理例外规则信息 */
	int iCount = (int)pHostDefenceReg->vecReg.size();
	for (int i=0; i< iCount; i++)
	{
		ObjectRule["RegItem"] = UnicodeToUTF8(pHostDefenceReg->vecReg[i].wsPath.c_str());
		ObjectRule["RegType"] = (int) pHostDefenceReg->vecReg[i].iType;

		root1.append(ObjectRule);
	}


	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["addOrFull"]		= (int)pHostDefenceReg->dwAddOrFull;
	person["section"]		= (int)pHostDefenceReg->dwSection;
	person["Enable"]		= (int)pHostDefenceReg->dwEnable;
	person["alarmMode"]		= (int)pHostDefenceReg->dwAlarmMode;

	person["CMDContent"]  = (Json::Value)root1;


	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::hostDefence_CheckDataProtect_GetValue(__in std::string& sJson, __out HOST_DEFENCE_DEP_STRUCT* pHostDefenceData)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("Enable") )
	{
		pHostDefenceData->dwIsDataProtect = (DWORD)root[0]["Enable"].asInt();
	}
	else
	{
		pHostDefenceData->dwIsDataProtect = 0;
	}

	if( root[0].isMember("ComputerID") )
	{
		pHostDefenceData->wsComputerID = UTF8ToUnicode(root[0]["ComputerID"].asString());
	}
	else
	{
		pHostDefenceData->wsComputerID = _T("LOCAL");
	}

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				std::wstring wsFileName = UTF8ToUnicode(root_1[i]["FileName"].asString());
				pHostDefenceData->vecFileName.push_back(wsFileName);
			}
		}
		
	}

	bResult = TRUE;
_exist_:
	return bResult;
}

std::string CWLJsonParse::hostDefence_CheckDataProtect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in HOST_DEFENCE_DEP_STRUCT* pHostDefenceData)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pHostDefenceData->vecFileName.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring wsFileName =  pHostDefenceData->vecFileName[i];
		CMDContent["FileName"] = UnicodeToUTF8(wsFileName);
		root1.append(CMDContent);
	}

	person["Enable"]=(int)pHostDefenceData->dwIsDataProtect;
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

//【白名单】- 系统完整性检查 - 获取每一项的路径
std::string CWLJsonParse::WMgr_SysCheck_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in std::vector<tstring> vecPath)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value CMDContent;

    tstring wsPath = _T("");

    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;

    if (0 != vecPath.size())
    {
        for (unsigned int i = 0; i < vecPath.size(); i++)
        {
            wsPath = vecPath[i];
            person["CMDContent"].append(UnicodeToUTF8(wsPath));
        }
    }
    else
    {
        person["CMDContent"].append(UnicodeToUTF8(wsPath));
    }

    root.append(person);
    sJson = writer.write(root);
    root.clear();

    return sJson;
}

BOOL CWLJsonParse::WMgr_SysCheck_ParseJson(__in std::string& sJson, __out std::vector<tstring> &vecPath, __out tstring &wsError)
{
    BOOL bResult = FALSE;

    Json::Value root;
    Json::Value CMDContent;
    Json::Reader reader;

    std::string	strValue = "";

    wstringstream wosError;

    if( sJson.length() == 0)
    {
        goto _exist_;
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        wosError << _T("parse failed, json = ") << UTF8ToUnicode(strValue);
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        wosError << _T("root.size() < 1 or root is not array, json = ") << UTF8ToUnicode(strValue);
        goto _exist_;
    }

    if( root[0].isMember("CMDContent"))
    {
        CMDContent =  (Json::Value)root[0]["CMDContent"]; //进行解析

        nObject = CMDContent.size();
        if ( nObject != 0)
        {
			//wwdv3
			if (CMDContent.isArray())
			{
				for (int i = 0; i < nObject; i++)
				{
					std::wstring wsPath = UTF8ToUnicode(CMDContent[i].asString());
					vecPath.push_back(wsPath);
				}
			}
            
        }
        else
        {
            vecPath.clear();
        }
    }

    bResult = TRUE;

_exist_:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}
//【白名单】- 例外路径
BOOL CWLJsonParse::wmg_NoScanPath_GetValue(__in std::string& sJson, __out WMG_NO_SANCE_PATH_STRUCT* pWmgNoScanPath,
                                           BOOL *pbFromClient,std::wstring *strComputerIdRet)
{
	BOOL bResult = FALSE;
    BOOL bFromClient = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;

	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("Enable") )
	{
		pWmgNoScanPath->dwControlScan = (DWORD)root[0]["Enable"].asInt();
	}
	else
	{
		pWmgNoScanPath->dwControlScan = 0;
	}

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				ST_SCAN_EXCEPTION_STRUCT stInfo;
				stInfo.strFullPath = UTF8ToUnicode(root_1[i]["NoScanPath"].asString());

				if( root_1[i].isMember("WarnException") )
				{
					stInfo.bIsWarnExp = (BOOL)(root_1[i]["WarnException"].asInt());
				}
				else
				{
					stInfo.bIsWarnExp = FALSE;
				}
				pWmgNoScanPath->vecStScanExp.push_back(stInfo);
			}
		}
	}

    if( root[0].isMember("ComputerID"))
    {
        std::wstring strComputerId = UTF8ToUnicode(root[0]["ComputerID"].asString());
        if (!strComputerId.compare(_T("LOCAL")))
        {
            bFromClient = TRUE;
        }
        else
        {
            if(strComputerIdRet)
                *strComputerIdRet = strComputerId;
        }
    }

    if (pbFromClient)
    {
        *pbFromClient = bFromClient;
    }

	bResult = TRUE;
_exist_:
	return bResult;
}

std::string CWLJsonParse::wmg_NoScanPath_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __out WMG_NO_SANCE_PATH_STRUCT* pWmgNoScanPath)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root2;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pWmgNoScanPath->vecStScanExp.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring wsTemp =  pWmgNoScanPath->vecStScanExp[i].strFullPath;
		CMDContent["NoScanPath"]=UnicodeToUTF8(wsTemp);
		CMDContent["WarnException"]=(int)pWmgNoScanPath->vecStScanExp[i].bIsWarnExp;
		root1.append(CMDContent);
	}

	person["Enable"]=(int)pWmgNoScanPath->dwControlScan; //1：覆盖，0: 删除 , 2:添加
	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}


//【白名单】- 追加白名单列表 - 指定文件从白名单删除
// 需要指定新版本吗
BOOL CWLJsonParse::whiteList_AppendAndDelete_GetValue(__in std::string& sJson,
                              __out std::vector<WMG_WHITE_FILE_LIST_STRUCT*>& vecWhiteList,
                              __out ENUM_MSG_FROM_TYPE *penFromType)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";
    ENUM_MSG_FROM_TYPE enFromType = MSG_FROM_USM;

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

    if (root[0].isMember("FromType"))
    {
        enFromType = (ENUM_MSG_FROM_TYPE)root[0]["FromType"].asInt();
    }

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		nObject = root_1.size();
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				std::wstring wsPath = UTF8ToUnicode(root_1[i]["FullPath"].asString());
				//pWmgWhiteFileList->vecFile.push_back(wsPath);
				WMG_WHITE_FILE_LIST_STRUCT* pNewStruct = new WMG_WHITE_FILE_LIST_STRUCT;
				// newStruct.dwAddOrDel =  传进来的时候，已经知道了
				// newStruct.dwVersion =  这个需要待商定。
				_tcscpy(pNewStruct->szFullPath, wsPath.c_str());
				pNewStruct->dwFileHashLength = 0;
				pNewStruct->szFileHash[0] = _T('\0');

				if (root_1[i].isMember("hash"))
				{
					std::wstring wsHash = UTF8ToUnicode(root_1[i]["hash"].asString());

					_tcsncpy(pNewStruct->szFileHash, wsHash.c_str(), _countof(pNewStruct->szFileHash));
					pNewStruct->szFileHash[_countof(pNewStruct->szFileHash) - 1] = _T('\0');
					pNewStruct->dwFileHashLength = (DWORD)_tcslen(pNewStruct->szFileHash);
				}

				vecWhiteList.push_back(pNewStruct);
			}
		}
		
	}

    if (penFromType)
    {
        *penFromType = enFromType;
    }

	bResult = TRUE;

_exist_:
	return bResult;
}


BOOL CWLJsonParse::whiteList_AppendWithHash_GetValue(__in std::string& sJson, __out WORK_SHEET_ST& stWorkSheet)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//��ȫ ��������
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("WorkSheetID"))
	{
		 std::wstring strID = UTF8ToUnicode(root[0]["WorkSheetID"].asString());
		 _tcscpy(stWorkSheet.stSheetHead.szWorkSheetId, strID.c_str());
	}
	else
	{
		  _tcscpy(stWorkSheet.stSheetHead.szWorkSheetId, _T(""));
	}

	if( root[0].isMember("WorkSheetName"))
	{
		std::wstring strName = UTF8ToUnicode(root[0]["WorkSheetName"].asString());
		_tcscpy(stWorkSheet.stSheetHead.szWorkSheetName, strName.c_str());
	}
	else
	{
		_tcscpy(stWorkSheet.stSheetHead.szUserName, _T(""));
	}

	if( root[0].isMember("WorkSheetState"))
	{
		stWorkSheet.stSheetHead.dwWorkSheetState = root[0]["WorkSheetState"].asInt();
	}
	else
	{
		stWorkSheet.stSheetHead.dwWorkSheetState = 0;
	}

	if( root[0].isMember("UserName"))
	{
		std::wstring strUserName = UTF8ToUnicode(root[0]["UserName"].asString());
		_tcscpy(stWorkSheet.stSheetHead.szUserName, strUserName.c_str());
	}
	else
	{
		_tcscpy(stWorkSheet.stSheetHead.szUserName, _T(""));
	}

	if( root[0].isMember("StartTime"))
	{
		std::wstring strStartTime = UTF8ToUnicode(root[0]["StartTime"].asString());
		_tcscpy(stWorkSheet.stSheetHead.szStartTime, strStartTime.c_str());
	}
	else
	{
		_tcscpy(stWorkSheet.stSheetHead.szStartTime, _T(""));
	}

	if( root[0].isMember("EndTime"))
	{
		std::wstring strEndTime = UTF8ToUnicode(root[0]["EndTime"].asString());
		_tcscpy(stWorkSheet.stSheetHead.szEndTime, strEndTime.c_str());
	}
	else
	{
		_tcscpy(stWorkSheet.stSheetHead.szEndTime, _T(""));
	}

	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //

		nObject = root_1.size();
		stWorkSheet.stSheetHead.dwItemCount = nObject;
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				std::wstring wsPath		= UTF8ToUnicode(root_1[i]["FileName"].asString());
				std::wstring wsHashVal	= UTF8ToUnicode(root_1[i]["HashValue"].asString());
				std::wstring wsProName  = UTF8ToUnicode(root_1[i]["ProcessName"].asString());
				int iActionType			= root_1[i]["ActionType"].asInt();

				WORK_SHEET_BODY_ST stWorkSheetBody;
				memset(&stWorkSheetBody, 0, sizeof(WORK_SHEET_BODY_ST));

				_tcscpy(stWorkSheetBody.szFullPath, wsPath.c_str());
				_tcscpy(stWorkSheetBody.szFileHash, wsHashVal.c_str());
				_tcscpy(stWorkSheetBody.szProcessPath, wsProName.c_str());
				_tcscpy(stWorkSheetBody.szUpdateTime, _T(""));
				stWorkSheetBody.dwActionType = iActionType;

				stWorkSheet.vecWorkSheetBody.push_back(stWorkSheetBody);
			}
		}
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::whiteList_AppendWithHash_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in WORK_SHEET_ST& stWorkSheet)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;
	
	int iSuccCount = 0;
	int iInsertWLCount = 0;

	int iItemCount = (int)stWorkSheet.vecWorkSheetBody.size();
	int i=0;
	for (i=0; i< iItemCount; i++)
	{
		std::wstring wsFullPath		= stWorkSheet.vecWorkSheetBody[i].szFullPath;
		std::wstring wsFileHash		= stWorkSheet.vecWorkSheetBody[i].szFileHash;
		std::wstring wsExecutable	= stWorkSheet.vecWorkSheetBody[i].szProcessPath;
		std::wstring wsUpdateTime	= stWorkSheet.vecWorkSheetBody[i].szUpdateTime;
		int			 iActioinType	= stWorkSheet.vecWorkSheetBody[i].dwActionType;
		int			 iResult		= stWorkSheet.vecWorkSheetBody[i].dwResult;
		
        if (WORK_SHEET_RESULT_SUCC_INSERT_WL == iResult)
		{
		    iResult = WORK_SHEET_RESULT_SUCC;
		}

		CMDContent["FileName"]	= UnicodeToUTF8(wsFullPath);
		CMDContent["HashValue"]   = UnicodeToUTF8(wsFileHash);
		
        /*if (wsExecutable == _T("是"))
		{
		    CMDContent["executable"] = 1;
		}
		else if (wsExecutable == _T("否"))
		{
		    CMDContent["executable"] = 0;
		}
		else
		{
		    CMDContent["executable"] = 2;
		}*/
        if (wsExecutable.length() == 0 || wsExecutable.length() > 5)
        {
            CMDContent["executable"] = WORK_SHEET_EXEC_FILE_OTHER;
        }
        else
        {
            int iExecutable = _tstoi(wsExecutable.c_str());
            CMDContent["executable"] = iExecutable;
        }
		
		CMDContent["UpdateTime"]  = UnicodeToUTF8(wsUpdateTime);
		CMDContent["ActionType"]	= (int)iActioinType;
		CMDContent["State"]		= (int)iResult;

		root1.append(CMDContent);
		
		if (WORK_SHEET_RESULT_SUCC_INSERT_WL == stWorkSheet.vecWorkSheetBody[i].dwResult)
		{
		    iSuccCount ++;
		    iInsertWLCount ++;
		}
		else if (WORK_SHEET_RESULT_SUCC == stWorkSheet.vecWorkSheetBody[i].dwResult)
		{
		    iSuccCount ++;
		}
	}

	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["WorkSheetID"]	= UnicodeToUTF8(stWorkSheet.stSheetHead.szWorkSheetId);
	person["WorkSheetName"] = UnicodeToUTF8(stWorkSheet.stSheetHead.szWorkSheetName);
	person["WorkSheetState"]= (int)stWorkSheet.stSheetHead.dwWorkSheetState;
	person["UserName"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szUserName);
	person["StartTime"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szStartTime);
	person["EndTime"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szEndTime);
	
	person["Total"]			= (int)iItemCount;
	person["Succeed"]		= (int)iSuccCount;
	person["WLToDB"]		= (int)iInsertWLCount;

	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

std::string CWLJsonParse::SingleWorkSheet_AppendWithHash_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in WORK_SHEET_ST& stFullWorkSheet, __in WORK_SHEET_ST& stSingleWorkSheet)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    Json::Value root1;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value CMDContent;

    int iSuccCount = 0;
    int iInsertWLCount = 0;

    int iItemCount = (int)stFullWorkSheet.vecWorkSheetBody.size();
    int i=0;
    for (i=0; i< iItemCount; i++)
    {
        std::wstring wsFullPath		= stFullWorkSheet.vecWorkSheetBody[i].szFullPath;
        std::wstring wsFileHash		= stFullWorkSheet.vecWorkSheetBody[i].szFileHash;
        std::wstring wsExecutable		= stFullWorkSheet.vecWorkSheetBody[i].szProcessPath;
        std::wstring wsUpdateTime	= stFullWorkSheet.vecWorkSheetBody[i].szUpdateTime;
        int			 iActioinType	= stFullWorkSheet.vecWorkSheetBody[i].dwActionType;
        int			 iResult		= stFullWorkSheet.vecWorkSheetBody[i].dwResult;
        if (WORK_SHEET_RESULT_SUCC_INSERT_WL == iResult)
        {
            iResult = WORK_SHEET_RESULT_SUCC;
        }

        if (WORK_SHEET_RESULT_SUCC_INSERT_WL == stFullWorkSheet.vecWorkSheetBody[i].dwResult)
        {
            iSuccCount ++;
            iInsertWLCount ++;
        }
        else if (WORK_SHEET_RESULT_SUCC == stFullWorkSheet.vecWorkSheetBody[i].dwResult)
        {
            iSuccCount ++;
        }
    }
    
    iItemCount = (int)stSingleWorkSheet.vecWorkSheetBody.size();
    for (i=0; i< iItemCount; i++)
    {
        std::wstring wsFullPath		= stSingleWorkSheet.vecWorkSheetBody[i].szFullPath;
        std::wstring wsFileHash		= stSingleWorkSheet.vecWorkSheetBody[i].szFileHash;
        std::wstring wsExecutable		= stSingleWorkSheet.vecWorkSheetBody[i].szProcessPath;
        std::wstring wsUpdateTime	= stSingleWorkSheet.vecWorkSheetBody[i].szUpdateTime;
        int			 iActioinType	= stSingleWorkSheet.vecWorkSheetBody[i].dwActionType;
        int			 iResult		= stSingleWorkSheet.vecWorkSheetBody[i].dwResult;
        if (WORK_SHEET_RESULT_SUCC_INSERT_WL == iResult)
        {
            iResult = WORK_SHEET_RESULT_SUCC;
        }

        CMDContent["FileName"]	= UnicodeToUTF8(wsFullPath);
        CMDContent["HashValue"]   = UnicodeToUTF8(wsFileHash);
        if (wsExecutable.length() == 0 || wsExecutable.length() > 5)
        {
            CMDContent["executable"] = WORK_SHEET_EXEC_FILE_OTHER;
        }
        else
        {
            int iExecutable = _tstoi(wsExecutable.c_str());
            CMDContent["executable"] = iExecutable;
        }
        CMDContent["UpdateTime"]  = UnicodeToUTF8(wsUpdateTime);
        CMDContent["ActionType"]	= (int)iActioinType;
        CMDContent["State"]		= (int)iResult;

        root1.append(CMDContent);
    }

    person["ComputerID"]	= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"]		= (int)cmdType;
    person["CMDID"]			= (int)cmdID;
    person["WorkSheetID"]	= UnicodeToUTF8(stSingleWorkSheet.stSheetHead.szWorkSheetId);
    person["WorkSheetName"] = UnicodeToUTF8(stSingleWorkSheet.stSheetHead.szWorkSheetName);
    person["WorkSheetState"]= (int)stSingleWorkSheet.stSheetHead.dwWorkSheetState;
    person["UserName"]		= UnicodeToUTF8(stSingleWorkSheet.stSheetHead.szUserName);
    person["StartTime"]		= UnicodeToUTF8(stSingleWorkSheet.stSheetHead.szStartTime);
    person["EndTime"]		= UnicodeToUTF8(stSingleWorkSheet.stSheetHead.szEndTime);

    person["Total"]			= (int)stFullWorkSheet.vecWorkSheetBody.size();
    person["Succeed"]		= (int)iSuccCount;
    person["WLToDB"]		= (int)iInsertWLCount;

    person["CMDContent"] = (Json::Value)root1;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::whiteList_WorkSheet_Result_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in WORK_SHEET_ST& stWorkSheet, __in WORD dwRes)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	wstring wsRes;

	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["WorkSheetID"]	= UnicodeToUTF8(stWorkSheet.stSheetHead.szWorkSheetId);
	person["WorkSheetName"] = UnicodeToUTF8(stWorkSheet.stSheetHead.szWorkSheetName);
	person["WorkSheetState"]= (int)stWorkSheet.stSheetHead.dwWorkSheetState;
	person["UserName"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szUserName);
	person["StartTime"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szStartTime);
	person["EndTime"]		= UnicodeToUTF8(stWorkSheet.stSheetHead.szEndTime);

	if(dwRes)
	{
		wsRes = _T("SUC");
	}
	else
	{
		wsRes = _T("FAIL");
	}

	person["RESULT"]		= UnicodeToUTF8(wsRes.c_str());

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

//WMG_WHITE_FILE_LIST_STRUCT
std::string CWLJsonParse::whiteList_AppendAndDelete_GetJson(__in tstring ComputerID, __in WORD cmdType,
															__in WORD cmdID,
															__in VECTOR_FILEINFOSTRUCT &vecWhiteList,
															__in ENUM_MSG_FROM_TYPE enFromType)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecWhiteList.size();
	if( nCount == 0)
		return "";

	Json::Value root_1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	for (int i=0; i< nCount; i++)
	{
	    FileInfoStruct &stEntry = vecWhiteList[i];
		std::wstring wsFullPath = 	stEntry.szFileFullPath;
		CMDContent["FullPath"] = UnicodeToUTF8(wsFullPath);
		char hex[(INTEGRITY_LENGTH+1) * 2] = {0};
		Bin2Hex(stEntry.bHashCode, INTEGRITY_LENGTH, hex);
		CMDContent["hash"] = hex;
		root_1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	person["dwAddOrDel"]=(int)0;
	person["dwVersion"]=(int)0;
	person["FromType"]= (int)enFromType;

	person["CMDContent"] = Json::Value(root_1);

	root.append(person);
	writer.omitEndingLineFeed();
	sJsonPacket = writer.write(root);
	root.clear();


	return sJsonPacket;
}

std::string CWLJsonParse::whiteList_Config_GetJson(
                            __in ST_WHITELIST_SELECT_CONDITION &stCondition,
                            __in std::vector<ST_WHITELIST_ENTRY> &vecWhiteList)
{

                /*
        json格式：

        //白名单查找获取
        {
            "Condition":
            {
                "MatchType": 1,
                "AppPath": "xxx",
                "Offset": 100,
                "Limit": 100,
                "Audit": 0,
				"SystemFile": 2
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",
                   "HashValue": "xxx",
                   "Time": "xxx",
				   "SystemFile": TRUE
                },

            ]
        }


    */

    int i;
    std::string sJsonPacket = "";
    Json::FastWriter writer;

	Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;


    Condition["MatchType"] = (int)stCondition.enMatchType;
    Condition["AppPath"] = UnicodeToUTF8(stCondition.sAppPath);
    Condition["Offset"] = (int)stCondition.iOffset;
    Condition["Limit"] = (int)stCondition.iLimit;
    Condition["TotalNum"] = (int)stCondition.iTotalNum;
    Condition["TotalPage"] = (int)stCondition.iTotalPage;
    Condition["Audit"] = (int)stCondition.bAudit;
	Condition["SystemFile"] = (int)stCondition.enSystemFile;
    root["Condition"] = Condition;

    for (i = 0; i < (int)vecWhiteList.size(); i++)
    {
        ST_WHITELIST_ENTRY &stEntry = vecWhiteList[i];

        CMDContent_Item["FullPath"] = UnicodeToUTF8(stEntry.sAppPath);
        CMDContent_Item["HashValue"] = UnicodeToUTF8(stEntry.shashvalue);
        CMDContent_Item["Time"] = UnicodeToUTF8(stEntry.sTime);
//        CMDContent_Item["FromType"] = (int)stEntry.iFromType;
		CMDContent_Item["Source"] = (int)stEntry.iSource;
		CMDContent_Item["SystemFile"] = (int)stEntry.bSystemFile;
        CMDContent.append(CMDContent_Item);
    }

    root["CMDContent"] = CMDContent;

    writer.omitEndingLineFeed();
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}



/* 白名单管理 配置json与结构体转换 */
BOOL CWLJsonParse::whiteList_Config_GetValue(__in std::string& sJson,
                                              __out ST_WHITELIST_SELECT_CONDITION &stCondition,
                                              __out std::vector<ST_WHITELIST_ENTRY> &vecWhiteList)
{

   /*
        json格式：

        //白名单查找获取
        {
            "Condition":
            {

                "MatchType": 1,
                "AppPath": "xxx",
                "Offset": 100,
                "Limit": 100,
                "Audit": 0,
				"SystemFile": 2
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",
                   "HashValue": "xxx",
                   "Time": "xxx"
                },

            ]
        }


    */

    unsigned int i = 0;;
    std::string     strValue = sJson;
    Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;
    Json::Reader    reader;
//    ENU_WHITELIST_FROM_TYPE enuFromType;


    if (!reader.parse(strValue, root))
    {
        return FALSE;
    }

    if (root.isMember("Condition"))
    {
        Condition = root["Condition"];

        stCondition.enMatchType = (ENUM_WHITELIST_MATCH_MODE)Condition["MatchType"].asInt();
        stCondition.sAppPath = UTF8ToUnicode(Condition["AppPath"].asString());
        stCondition.iOffset = Condition["Offset"].asInt();
        stCondition.iLimit = Condition["Limit"].asInt();
        stCondition.iTotalNum = Condition["TotalNum"].asInt();
        stCondition.iTotalPage = Condition["TotalPage"].asInt();
        stCondition.bAudit = (BOOL)Condition["Audit"].asInt();
		stCondition.enSystemFile = (ENUM_WHITELIST_SYSTEMFILE)Condition["SystemFile"].asInt();

    }

    if (root.isMember("CMDContent"))
    {
        CMDContent = root["CMDContent"];
        for (i = 0; i < CMDContent.size(); i++)
        {
            ST_WHITELIST_ENTRY stEntry;
            stEntry.llID = 0;
            stEntry.bAudit = 0;
            stEntry.iFileType = 0;
           // stEntry.iFromType = WHITELIST_FROM_OLD_VERSION;
            stEntry.iSubType = 0;


            CMDContent_Item = CMDContent[i];

            stEntry.sAppPath = UTF8ToUnicode(CMDContent_Item["FullPath"].asString());
            stEntry.shashvalue = UTF8ToUnicode(CMDContent_Item["HashValue"].asString());
            stEntry.sTime = UTF8ToUnicode(CMDContent_Item["Time"].asString());
			stEntry.bSystemFile = CMDContent_Item["SystemFile"].asBool();
            //if (CMDContent_Item.isMember("FromType"))
            //{
            //    enuFromType = (ENU_WHITELIST_FROM_TYPE)CMDContent_Item["FromType"].asInt();
            //    if (WHITELIST_FROM_TYPE_VALID(enuFromType))
            //    {
            //        stEntry.iFromType = enuFromType;
            //    }
            //}

			if (CMDContent_Item.isMember("Source"))
			{
				stEntry.iSource = CMDContent_Item["Source"].asInt();
			}

            vecWhiteList.push_back(stEntry);
        }
    }

    return TRUE;
}



std::string CWLJsonParse::whiteList_Result_GetJson(
                            __in ST_WHITELIST_RESULT_SUM &stRet,
                            __in std::vector<ST_WHITELIST_RESULT_ENTRY> &vecWhiteList)
{

                /*
        json格式：

        //白名单查找获取
        {
            "WhiteListResult":
            {
                "Result": 0,      // 0 成功   1失败 不同的失败结果定义不同的错误码
                "Reason": "xxxx",
                "DirWhiteListCount": 2
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",       // 可填可不填
                   "SubReason": "ioctl failed!" // 可填可不填
                },

            ]
        }


    */

    int i;
    std::string sJsonPacket = "";
    Json::FastWriter writer;

	Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;


    Condition["Result"] = (int)stRet.enResult;
    Condition["Reason"] = UnicodeToUTF8(stRet.sReason);
    Condition["DirWhiteListCount"] = (int)stRet.iDirWhiteListCount;
	Condition["AfterDeleteNum"] = (int)stRet.iAfterDeleteNum;
    root["WhiteListResult"] = Condition;

    for (i = 0; i < (int)vecWhiteList.size(); i++)
    {
        ST_WHITELIST_RESULT_ENTRY &stEntry = vecWhiteList[i];

        CMDContent_Item["FullPath"] = UnicodeToUTF8(stEntry.sAppPath);
        CMDContent_Item["HashValue"] = UnicodeToUTF8(stEntry.sSubReason);
        CMDContent.append(CMDContent_Item);
    }

    root["CMDContent"] = CMDContent;

    writer.omitEndingLineFeed();
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}



/* 白名单管理 配置json与结构体转换 */
BOOL CWLJsonParse::whiteList_Result_GetValue(__in std::string& sJson,
                                              __out ST_WHITELIST_RESULT_SUM &stRet,
                                              __out std::vector<ST_WHITELIST_RESULT_ENTRY> &vecWhiteList)
{

   /*
        json格式：

        //白名单查找获取
        {
            "WhiteListResult":
            {
                "Result": 0,      // 0 成功   1失败 不同的失败结果定义不同的错误码
                "Reason": "xxxx",
                "DirWhiteListCount": 2
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",       // 可填可不填
                   "SubReason": "ioctl failed!" // 可填可不填
                },

            ]
        }


    */

    int i;
    std::string     strValue = sJson;
    Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;
    Json::Reader    reader;



    if (!reader.parse(strValue, root))
    {
        return FALSE;
    }

    if (root.isMember("WhiteListResult"))
    {
        Condition = root["WhiteListResult"];

        stRet.enResult = (ENUM_WHITELIST_RESULT_TYPE)Condition["Result"].asInt();
        stRet.sReason = UTF8ToUnicode(Condition["Reason"].asString());
        stRet.iDirWhiteListCount = (int)Condition["DirWhiteListCount"].asInt();
		stRet.iAfterDeleteNum = (int)Condition["AfterDeleteNum"].asInt();

    }

    if (root.isMember("CMDContent"))
    {
        CMDContent = root["CMDContent"];
        for (i = 0; i < CMDContent.size(); i++)
        {
            ST_WHITELIST_RESULT_ENTRY stEntry;

            CMDContent_Item = CMDContent[i];

            stEntry.sAppPath = UTF8ToUnicode(CMDContent_Item["FullPath"].asString());
            stEntry.sSubReason = UTF8ToUnicode(CMDContent_Item["SubReason"].asString());

            vecWhiteList.push_back(stEntry);
        }
    }

    return TRUE;
}

std::string CWLJsonParse::SafeFileCopy_Result_GetJson(
							__in const ST_FILE_COPY_RET &stRet)
{
	/*
	{
	"ret_type": "0",       
	"filepath": "D:/WorkCode/Windows/111.exe"	//可为空
	}
	*/

	Json::Value root;
	Json::FastWriter jsWriter;
	std::string strJson = "";

	root["ret_type"] = (int)stRet.eRetType;

	if(!stRet.wsFullFilePath.empty())
	{
		root["filepath"] = CStrUtil::ConvertW2A(stRet.wsFullFilePath);
	}

	strJson = jsWriter.write(root);

	return strJson;
}

bool CWLJsonParse::SafeFileCopy_Result_GetValue(
					__in const std::string& strJson, __out ST_FILE_COPY_RET& stRet, __out std::wstring& strError)
{
	strError = _T("");
	Json::Reader parser;
	Json::Value root;
	bool bRet = false;

	if (strJson.empty()) 
	{
		strError = _T("strJson is empty");
		return bRet;
	}

	bRet = parser.parse(strJson, root);
	if (!bRet) 
	{
		strError = _T("Failed to parse strJson");
		return bRet;
	}

	// 解析返回类型
	if (root.isMember("ret_type") && 
		root["ret_type"].isInt())
	{
		stRet.eRetType = (ENUM_FILE_COPY_RET_TYPE)root["ret_type"].asInt();
	} 
	else
	{
		strError = _T("ret_type is not int");
		return bRet;
	}

	// 解析文件路径
	if (root.isMember("filepath"))
	{
		if (root["filepath"].isString())
		{
			std::string path = root["filepath"].asString();
			stRet.wsFullFilePath = CStrUtil::ConvertA2W(path);
		}
		else
		{
			strError = _T("source_path is not string");
			return bRet;
		}
	}

	bRet = true;

	return bRet;
}


std::string CWLJsonParse::Software_Result_GetJson(
                            __in ST_SOFTWARE_RESULT &stRet,
                            __in std::vector<ST_SOFTWARE_ENTRY> &vecFileInfo)
{

                /*
        json格式：

        //软件安装与卸载：dlfender返回客户端信息
        {
            "WhiteListResult":
            {
                "Result": 0,      // 0 成功   1失败 不同的失败结果定义不同的错误码
                "Reason": "xxxx",
                "WLFileCount": 10
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",       // 可填可不填
                   "HashValue": "ioctl failed!" // 可填可不填
                },

            ]
        }


    */

    int i;
    std::string sJsonPacket = "";
    Json::FastWriter writer;

	Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;

    Condition["WLFileCount"] = (int)stRet.ulWlFileCount;


    Condition["Result"] = (int)stRet.enResult;
    Condition["Pid"] = (int)stRet.ulPid;
    Condition["Reason"] = UnicodeToUTF8(stRet.sReason);
    root["SoftwareResult"] = Condition;

    for (i = 0; i < (int)vecFileInfo.size(); i++)
    {
        ST_SOFTWARE_ENTRY &stEntry = vecFileInfo[i];

        CMDContent_Item["FullPath"] = UnicodeToUTF8(stEntry.sFilePath);
        CMDContent_Item["CmdLine"] = UnicodeToUTF8(stEntry.sCmdLine);
        CMDContent.append(CMDContent_Item);
    }

    root["CMDContent"] = CMDContent;

    writer.omitEndingLineFeed();
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

BOOL CWLJsonParse::Software_Result_GetValue(__in std::string& sJson,
                                              __out ST_SOFTWARE_RESULT &stRet,
                                              __out std::vector<ST_SOFTWARE_ENTRY> &vecFileInfo)
{

   /*
        json格式：

        //软件安装与卸载：获取
        {
            "WhiteListResult":
            {
                "Result": 0,      // 0 成功   1失败 不同的失败结果定义不同的错误码
                "Reason": "xxxx",
                "DirWhiteListCount": 2
            },
            "CMDContent":
            [
                {
                   "FullPath": "xxxx",       // 可填可不填
                   "SubReason": "ioctl failed!" // 可填可不填
                },

            ]
        }


    */

    int i;
    std::string     strValue = sJson;
    Json::Value Condition;
	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;
    Json::Reader    reader;
    std::wstring wsFilePath;
    std::wstring wsHashCode;


    if (!reader.parse(strValue, root))
    {
        return FALSE;
    }

    if (root.isMember("SoftwareResult"))
    {
        Condition = root["SoftwareResult"];

        stRet.enResult = (ENUM_SOFTWARE_RESULT_TYPE)Condition["Result"].asInt();
        stRet.ulPid = Condition["Pid"].asInt();
        stRet.sReason = UTF8ToUnicode(Condition["Reason"].asString());
        stRet.ulWlFileCount = (ULONG)Condition["WLFileCount"].asInt();
    }

    if (root.isMember("CMDContent"))
    {
        CMDContent = root["CMDContent"];
		//wwdv3
		if (CMDContent.isArray())
		{
			for (i = 0; i < CMDContent.size(); i++)
			{
				ST_SOFTWARE_ENTRY stEntry;

				CMDContent_Item = CMDContent[i];

				stEntry.sFilePath = UTF8ToUnicode(CMDContent_Item["FullPath"].asString());
				stEntry.sCmdLine = UTF8ToUnicode(CMDContent_Item["CmdLine"].asString());

				vecFileInfo.push_back(stEntry);
			}
		}

    }

    return TRUE;
}

//�【白名单】- 上传白名单文件列表
std::string CWLJsonParse::wmg_upLoad_WhiteList_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in std::vector<WMG_WHITE_FILE_LIST_STRUCT*>& vecWhiteList)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	int nCount = (int)vecWhiteList.size();
	if( nCount == 0)
		return "";

	Json::Value root_1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	for (int i=0; i< nCount; i++)
	{
		std::wstring wsFullPath = 	vecWhiteList[i]->szFullPath;
		CMDContent["FullPath"] = UnicodeToUTF8(wsFullPath);
		root_1.append(CMDContent);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	person["dwAddOrDel"]=(int)vecWhiteList[0]->dwAddOrDel;
	person["dwVersion"]=(int)vecWhiteList[0]->dwVersion;

	person["CMDContent"] = Json::Value(root_1);

	root.append(person);
	writer.omitEndingLineFeed();
	sJsonPacket = writer.write(root);
	root.clear();


	return sJsonPacket;
}

//【白名单】- 上传非白名单文件
// 这个由小谢决定
std::string CWLJsonParse::wmg_upLoad_NoWhilteFile_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in SMALL_FILE_STRUCT* pSmallFileStruct)
{
	std::string sJsonPacket = "";
	return sJsonPacket;
}

 //【白名单】- 扫描配置 - JSON 转 结构体
BOOL CWLJsonParse::whitelist_StartScan_GetValue(__in const std::string &sJson, __out WHITELIST_SCAN_CONFIG_ST &stScanConfig)
{
    BOOL bRet = FALSE;
    
    // 重置
    stScanConfig.Reset();

    if (sJson.empty())
    {
        stScanConfig.eScanType = eWST_FullDiskScan;
        stScanConfig.eScanSpeed = eWSS_UniformVelocity;
		stScanConfig.dwSrc = 0;
        return TRUE;
    }
    
    Json::Value root;
    Json::Reader parser;

    bRet = parser.parse(sJson, root);
    if (!bRet)
    {
        return bRet;
    }

    // 兼容USM下发
    if (root.isMember(PLY_FW_STRKEY_CMDCONTENT) &&
        root[PLY_FW_STRKEY_CMDCONTENT].isObject())
    {
        root = root[PLY_FW_STRKEY_CMDCONTENT];
    }

    // 解析扫描速度
    if (root.isMember(WLSC_KEY_SCANSPEED) && 
        root[WLSC_KEY_SCANSPEED].isNumeric())
    {
        stScanConfig.eScanSpeed = WHITELIST_SCAN_SPEED(root[WLSC_KEY_SCANSPEED].asInt());
    }

    // 解析扫描类型
    if (root.isMember(WLSC_KEY_SCANTYPE) && 
        root[WLSC_KEY_SCANTYPE].isNumeric())
    {
        stScanConfig.eScanType = WHITELIST_SCAN_TYPE(root[WLSC_KEY_SCANTYPE].asInt());
    }

    // 解析扫描路径
    stScanConfig.lstScanPath.clear();
    if (root.isMember(WLSC_KEY_SCANPATH) && 
        root[WLSC_KEY_SCANPATH].isArray())
    {
        const Json::Value &arrPaths = root[WLSC_KEY_SCANPATH];
        for (int i = 0; i < arrPaths.size(); ++i)
        {
            std::string path = arrPaths[i].asString();
            std::wstring wsPath = CStrUtil::UTF8ToUnicode(path);
            stScanConfig.lstScanPath.push_back(wsPath);
        }   
    }

	// 解析扫描来源
    if (root.isMember(WLSC_KEY_PLYORIGIN) && 
        root[WLSC_KEY_PLYORIGIN].isNumeric())
    {
		stScanConfig.dwSrc = root[WLSC_KEY_PLYORIGIN].asInt();
    }

    bRet = TRUE;

    return bRet;
}

 //【白名单】- 扫描配置，结构体转JSON
std::string CWLJsonParse::whitelist_StartScan_GetJson(__in const WHITELIST_SCAN_CONFIG_ST &stScanConfig)
{
    std::string strJson;
    Json::Value root;
    Json::FastWriter writer;
    int i = 0;

    root[WLSC_KEY_SCANSPEED] = stScanConfig.eScanSpeed;
    root[WLSC_KEY_SCANTYPE]  = stScanConfig.eScanType;

    std::list<std::wstring>::const_iterator it = stScanConfig.lstScanPath.begin();
    for (;it != stScanConfig.lstScanPath.end(); ++it)
    {
        std::string strPath = CStrUtil::UnicodeToUTF8((*it));
        root[WLSC_KEY_SCANPATH][i] = strPath;
        ++i;
    }
	
	root[WLSC_KEY_PLYORIGIN] = (int)stScanConfig.dwSrc;
    
    writer.omitEndingLineFeed();
    strJson = writer.write(root);
    return strJson;
}

//【白名单】- 切换扫描速度
BOOL CWLJsonParse::whitelist_ChangeScanSpeed_GetValue(__in const std::string &sJson, __out WHITELIST_SCAN_SPEED &emScanSpeed)
{
    BOOL bRet = FALSE;
    if (sJson.empty())
    {
        return bRet;
    }

    Json::Value root;
    Json::Reader parser;

    bRet = parser.parse(sJson, root);
    if (!bRet)
    {
        return bRet;
    }

    if (root.isArray() && root.size() > 0)
    {
        root = root[0];
    }

    // 兼容USM下发
    if (root.isMember(PLY_FW_STRKEY_CMDCONTENT) &&
        root[PLY_FW_STRKEY_CMDCONTENT].isObject())
    {
        root = root[PLY_FW_STRKEY_CMDCONTENT];
    }

    // 解析扫描速度
    if (root.isMember(WLSC_KEY_SCANSPEED) && 
        root[WLSC_KEY_SCANSPEED].isNumeric())
    {
        emScanSpeed = WHITELIST_SCAN_SPEED(root[WLSC_KEY_SCANSPEED].asInt());
    }

    bRet = TRUE;

    return bRet;
}

std::string CWLJsonParse::whitelist_ChangeScanSpeed_GetJson(__in const WHITELIST_SCAN_SPEED &emScanSpeed)
{
    std::string strJson;
    Json::Value root;
    Json::FastWriter writer;
    int i = 0;

    root[WLSC_KEY_SCANSPEED] = emScanSpeed;

    writer.omitEndingLineFeed();
    strJson = writer.write(root);
    return strJson;
}

BOOL CWLJsonParse::plyConfig_GetValue(__in std::string& sJson, __out PLY_CONFIG_STRUCT* pPlyConfig)
{
	//{"EXCLUDESCANFOLDER":1,"REGPROTECT":1,"FILEPROTECT":1,"DEP":1,"IPSEC":1,"PROCESSAUDIT":1} 1:服务策略，0：客户端策略
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("EXCLUDESCANFOLDER") )
	{
		pPlyConfig->dwExcludeScanFolder =(DWORD)root[0]["EXCLUDESCANFOLDER"].asInt();
	}
	else
	{
		pPlyConfig->dwExcludeScanFolder = 0;
	}

	if( root[0].isMember("REGPROTECT") )
	{
		pPlyConfig->dwRegProtect =(DWORD)root[0]["REGPROTECT"].asInt();
	}
	else
	{
		pPlyConfig->dwRegProtect = 0;
	}

	if( root[0].isMember("FILEPROTECT") )
	{
		pPlyConfig->dwFileProtect =(DWORD)root[0]["FILEPROTECT"].asInt();
	}
	else
	{
		pPlyConfig->dwFileProtect = 0;
	}

	if( root[0].isMember("DEP") )
	{
		pPlyConfig->dwDEP =(DWORD)root[0]["DEP"].asInt();
	}
	else
	{
		pPlyConfig->dwDEP = 0;
	}

	if( root[0].isMember("IPSEC") )
	{
		pPlyConfig->dwIPSEC =(DWORD)root[0]["IPSEC"].asInt();
	}
	else
	{
		pPlyConfig->dwIPSEC = 0;
	}

	if( root[0].isMember("PROCESSAUDIT") )
	{
		pPlyConfig->dwProcessAudit =(DWORD)root[0]["PROCESSAUDIT"].asInt();
	}
	else
	{
		pPlyConfig->dwProcessAudit = 0;
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

/*
* @fn           ReadJsonFile
* @brief        读取指定路径Json文档到字符串
* @param[in]    wsFilePath：文件路径
				pErr：错误类型
* @param[out]   
* @return       string: 返回当前运行路径 
*               
* @detail      comment by mingming.shi
* @author      xxx
* @date        
*/
std::string CWLJsonParse::ReadJsonFile(tstring wsFilePath, int *pErr)
{
	std::string sJson = "";

	Wow64RedirectOff wow64RedirectOff;
	int n = PathFileExists(wsFilePath.c_str());
	if( n != 1)
	{
		if (pErr)
		{
			*pErr = -1;
		}

		return sJson;
	}

	//读json
	FILE* fp_read = NULL;
	fp_read = _wfopen(wsFilePath.c_str(), _T("rb"));

	if( fp_read == NULL)
	{
		if (pErr)
		{
			*pErr = -2;
		}
		return sJson;
	}

	fseek(fp_read, 0, SEEK_END);
	long nLen = ftell(fp_read);
	fseek(fp_read, 0, SEEK_SET);

	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);

	fread(pBuf, 1, nLen, fp_read);
	fclose(fp_read);

	sJson = pBuf;
	delete [] pBuf;

	return sJson;
}


BOOL CWLJsonParse::SaveJson(std::string& sJson, tstring wsFilePath, int *pErr)
{
	//static DWORD dwThreadSave = 0;
	//DWORD dwThread = GetCurrentThreadId();
	//if( dwThread != dwThreadSave)
	//{
	//	dwThreadSave = dwThread;
	//	CWLDevCtrlApi::instance()->WLCtrlRemovePriviligeTID(dwThread);
	//}

	Wow64RedirectOff wow64RedirectOff;

	FILE* fp_write = NULL;
	fp_write = _wfopen(wsFilePath.c_str(), _T("wb"));

	if( fp_write == NULL)
	{
		if (pErr)
		{
			_get_errno(pErr);
		}

		return FALSE;
	}

	fwrite(sJson.c_str(), 1, sJson.length(), fp_write);
	fclose(fp_write);

	return TRUE;
}
//-----------------------------------------------------------------------------------------------------------
std::string CWLJsonParse::Credential_USBKeyManager_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in UK_MANAGER_ST* pUSBKeyManager)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";


	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pUSBKeyManager->vecUKBindInfo.size();
	int i=0;
	for (i=0; i< nCount; i++)
	{
		std::wstring s1 = pUSBKeyManager->vecUKBindInfo[i].HID;
		std::wstring s2 = pUSBKeyManager->vecUKBindInfo[i].USBKeyName;
		std::wstring s3 = pUSBKeyManager->vecUKBindInfo[i].USBKeySN;
		std::wstring s4 = pUSBKeyManager->vecUKBindInfo[i].OsUser;
		std::wstring s5 = pUSBKeyManager->vecUKBindInfo[i].OsDomain;
		time_t time		= pUSBKeyManager->vecUKBindInfo[i].time;
		std::wstring s6;
		s6 = convertTimeTToStr(time);


		CMDContent["KeySerialID"]	= UnicodeToUTF8(s1);
		CMDContent["KeyName"]		= UnicodeToUTF8(s2);
		CMDContent["ProductId"]		= UnicodeToUTF8(s3);
		CMDContent["UserName"]		= UnicodeToUTF8(s4);
		CMDContent["Domain"]		= UnicodeToUTF8(s5);
		CMDContent["BoundleTime"]	= UnicodeToUTF8(s6);

		root1.append(CMDContent);
	}


	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["USBKeyAuth"]	=(int)pUSBKeyManager->stUKAuthenInfo.dwUSBKeyDentify;
	person["OsAuth"]		=(int)pUSBKeyManager->stUKAuthenInfo.dwSystemDentify;
	person["Safemode"]		=(int)pUSBKeyManager->stUKAuthenInfo.dwSystemSafeMode;
	person["BindDomain"]	=(int)pUSBKeyManager->dwBindDomain;

	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}
//-----------------------------------------------------------------------------------------------------------
//Credential 管理界面
BOOL CWLJsonParse::Credential_USBKeyManager_GetValue(__in std::string& sJson, __out  UK_MANAGER_ST* pUSBKeyManager)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;

	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}


	if( root[0].isMember("USBKeyAuth"))
	{
		pUSBKeyManager->stUKAuthenInfo.dwUSBKeyDentify =  root[0]["USBKeyAuth"].asInt();
	}
	else
	{
		pUSBKeyManager->stUKAuthenInfo.dwUSBKeyDentify = 0;
	}

	if( root[0].isMember("OsAuth"))
	{
		pUSBKeyManager->stUKAuthenInfo.dwSystemDentify =  root[0]["OsAuth"].asInt();
	}
	else
	{
		pUSBKeyManager->stUKAuthenInfo.dwSystemDentify = 0;
	}

	if( root[0].isMember("Safemode"))
	{
		pUSBKeyManager->stUKAuthenInfo.dwSystemSafeMode =  root[0]["Safemode"].asInt();
	}
	else
	{
		pUSBKeyManager->stUKAuthenInfo.dwSystemSafeMode = 0;
	}

	if( root[0].isMember("BindDomain"))
	{
		pUSBKeyManager->dwBindDomain =  root[0]["BindDomain"].asInt();
	}
	else
	{
		pUSBKeyManager->dwBindDomain = 0;
	}


	if( root[0].isMember("CMDContent"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		// 进行解析
		nObject = root_1.size();
		std::wstring KeySerialID;
		std::wstring KeyName;
		std::wstring ProductId;
		std::wstring UserName;
		std::wstring Domain;
		std::wstring BoundleTime;
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i<nObject; i++)
			{
				KeySerialID = UTF8ToUnicode(root_1[i]["KeySerialID"].asString());
				KeyName		= UTF8ToUnicode(root_1[i]["KeyName"].asString());
				ProductId	= UTF8ToUnicode(root_1[i]["ProductId"].asString());
				UserName	= UTF8ToUnicode(root_1[i]["UserName"].asString());
				Domain		= UTF8ToUnicode(root_1[i]["Domain"].asString());
				BoundleTime = UTF8ToUnicode(root_1[i]["BoundleTime"].asString());

				UK_BINDINFO_ST p ;//= new UK_BINDINFO_ST;
				memset(&p, 0, sizeof(UK_BINDINFO_ST));
				_tcscpy(p.HID, KeySerialID.c_str());
				_tcscpy(p.USBKeyName, KeyName.c_str());
				_tcscpy(p.USBKeySN, ProductId.c_str());
				_tcscpy(p.OsUser, UserName.c_str());
				_tcscpy(p.OsDomain, Domain.c_str());
				convertStrToTimeT(p.time, BoundleTime.c_str());
				//_tcscpy(p->time, BoundleTime.c_str());

				pUSBKeyManager->vecUKBindInfo.push_back(p);
			}
		}
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::USBKey_Certification_Status_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in UK_AUTHENTICATION_ST* pUSBKeyDentify)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;

	DWORD systemDentifyStatus = pUSBKeyDentify->dwSystemDentify;
	DWORD systemSafeModeStatus = pUSBKeyDentify->dwSystemSafeMode;
	DWORD usbKeyDentifyStatus = pUSBKeyDentify->dwUSBKeyDentify;

	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["USBKeyAuth"]	=(int)usbKeyDentifyStatus;
	person["OsAuth"]		=(int)systemDentifyStatus;
	person["Safemode"]		=(int)systemSafeModeStatus;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

/* 解析所有用户的信息
“UserName”:”yangjp”,
“Password”：”ABC@abc123”，
“Group”: “admin”,
“Enable”: 0
“USMCreate”:1
“Domain”:””,
“HardSerialID”: “2CA32E124D011108”,
“KeyName”:” pc1-yangjp”,
“BoundleTime”:” 2018-05-02 17:45:30”,
*/
BOOL CWLJsonParse::OSUser_ParseUsers(Json::Value&       rootContent,VEC_ST_USERS_USM & vecUserUSM)
{
    int iObject = rootContent.size();
    for (int i = 0; i < iObject; i++)
    {
        ST_USER_JSON stOneUser;

        /* user info */
        if (rootContent[i].isMember("UserName"))
        {
			strncpy(stOneUser.stUser.szUserName, rootContent[i]["UserName"].asString().c_str(), MAX_LEN_NAME);

        }

        if (rootContent[i].isMember("Password"))
        {
			strncpy(stOneUser.stUser.szPassword, rootContent[i]["Password"].asString().c_str(), MAX_LEN_PASSWORD);
        }

        if (rootContent[i].isMember("Group"))
        {
           strncpy(stOneUser.stUser.szGroup, rootContent[i]["Group"].asString().c_str(), MAX_LEN_GROUP);
        }

        if (rootContent[i].isMember("Enable"))
        {
            stOneUser.stUser.iEnableStatus  = rootContent[i]["Enable"].asInt();
        }

        if (rootContent[i].isMember("USMCreate"))
        {
            stOneUser.stUser.iUSMCreate     = rootContent[i]["USMCreate"].asInt();
        }

        /* domain */
        if (rootContent[i].isMember("Domain"))
        {
			strncpy(stOneUser.szDomain, rootContent[i]["Domain"].asString().c_str(), sizeof(stOneUser.szDomain));
        }

        /* bind info */
        if (rootContent[i].isMember("HardSerialID"))
        {
			strncpy(stOneUser.stBind.szHID, rootContent[i]["HardSerialID"].asString().c_str(), sizeof(stOneUser.stBind.szHID));
        }

        if (rootContent[i].isMember("KeyName"))
        {
			strncpy(stOneUser.stBind.szUkeyName, rootContent[i]["KeyName"].asString().c_str(), sizeof(stOneUser.stBind.szUkeyName));
        }

        if (rootContent[i].isMember("BoundleTime"))
        {
            std::string tmpTime         = rootContent[i]["BoundleTime"].asString();
            convertStr2Time(stOneUser.stBind.tBindTime, tmpTime);
        }

        if (rootContent[i].isMember("ProductId"))
        {
            strncpy(stOneUser.stBind.szPID, rootContent[i]["ProductId"].asString().c_str(), sizeof(stOneUser.stBind.szPID));
        }

        /* push back */
        vecUserUSM.push_back(stOneUser);
    }

    return TRUE;
}

/* 解析USM下发的用户数据（包含ukey开关和绑定关系）chennian 20181111  */
BOOL CWLJsonParse::OSUser_GetValue(__in std::string& sJson,ST_USERS_INFO_HEAD & stUsersHead, VEC_ST_USERS_USM & vecUserUSM)
{
    BOOL bResult = FALSE;

    Json::Value     root;
    Json::Value     rootContent;
    Json::Reader    reader;
    std::string     strValue = "";

    if (sJson.length() == 0)
    {
        goto END;
    }

    strValue = sJson;

    //补全 按数组解析
    if (strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        goto END;
    }

    int nObject = root.size();
    if (nObject < 1 || !root.isArray())
    {
        goto END;
    }

    /* usbkey 开关 */
    stUsersHead.iUkeyEnable = 0;
    if (root[0].isMember("USBKeyAuth"))
    {
        stUsersHead.iUkeyEnable =  root[0]["USBKeyAuth"].asInt();
    }

    /* 系统认证开关 */
    stUsersHead.iEnableOsAuth = 0;
    if (root[0].isMember("OsAuth"))
    {
        stUsersHead.iEnableOsAuth =  root[0]["OsAuth"].asInt();
    }

    /* 禁用安全模式 */
    stUsersHead.iDisableSafeMode = 0;
    if (root[0].isMember("Safemode"))
    {
        stUsersHead.iDisableSafeMode =  root[0]["Safemode"].asInt();
    }

    /* 禁用本地用户管理 */
    stUsersHead.iDisableLocalUser = 0;
    if (root[0].isMember("DisableLocalUser"))
    {
        stUsersHead.iDisableLocalUser =  root[0]["DisableLocalUser"].asInt();
    }

    /* 消息类型 */
    stUsersHead.iContentType = 0;
    if (root[0].isMember("ContentType"))
    {
        stUsersHead.iContentType =  root[0]["ContentType"].asInt();
    }

    /* 只有头部的情况到此结束，赋值结果为成功 */
    bResult = TRUE;

    /* 用户列表 */
    if (root[0].isMember("CMDContent"))
    {
        rootContent =  (Json::Value)root[0]["CMDContent"];
        bResult = this->OSUser_ParseUsers(rootContent, vecUserUSM);
    }

END:
    return bResult;
}

/* 主机卫士上报给USM的用户列表（包含ukey绑定关系） chennian 20181111 */
std::string CWLJsonParse::OSUser_GetJson(__in tstring ComputerID, ST_USERS_INFO_HEAD & stUsersHead, VEC_ST_USERS_USM & vecUserUSM)
{
    std::string sJsonPacket = "";
    Json::Value root;
    Json::Value rootContent;
    Json::Value head;
    Json::Value oneUser;
    Json::FastWriter writer;

    /*
    “UserName":yangjp”,
    “Password”：”ABC@abc123”，
    “Group”: “admin”,
    “Enable”: 0
    “USMCreate”:1
    “Domain”:””,
    “HardSerialID”: “2CA32E124D011108”,
    “KeyName”:” pc1-yangjp”,
    “BoundleTime”:” 2018-05-02 17:45:30”, */

    int nCount = (int)vecUserUSM.size();
    for (int i = 0; i < nCount; i++)
    {
        ST_USER_JSON *pstOneUser = &vecUserUSM[i];

        time_t time = pstOneUser->stBind.tBindTime;
        std::string strTimeTmp = convertTime2Str(time);

        oneUser["UserName"]      = pstOneUser->stUser.szUserName;
        oneUser["Group"]         = pstOneUser->stUser.szGroup ;
        oneUser["Enable"]        = pstOneUser->stUser.iEnableStatus;
        oneUser["USMCreate"]     = pstOneUser->stUser.iUSMCreate;
        oneUser["Domain"]        = pstOneUser->szDomain;
        oneUser["HardSerialID"]  = pstOneUser->stBind.szHID;
        oneUser["KeyName"]       = pstOneUser->stBind.szUkeyName;

        if(pstOneUser->stBind.tBindTime != 0)
        {
            time_t time = pstOneUser->stBind.tBindTime;
            std::string strTimeTmp = convertTime2Str(time);
            oneUser["BoundleTime"]   = strTimeTmp;
        }

        rootContent.append(oneUser);
    }

    /*
    “ComputerID”:”FEFOEACD”,
    “CMDTYPE”: 200,
    “CMDID”: 171,
    “USBKeyAuth":0,
    “OsAuth”: 1,
    “Safemode”:1，
    “DisableLocalUser”:1，
    “ContentType”: 0,
    “CMDContent”:
    */
    head["ComputerID"]    = UnicodeToUTF8(ComputerID);
    head["CMDTYPE"]       = 200;
    head["CMDID"]         = 171;

    head["USBKeyAuth"]        = stUsersHead.iUkeyEnable;
    head["OsAuth"]            = stUsersHead.iEnableOsAuth;
    head["Safemode"]          = stUsersHead.iDisableSafeMode;
    head["DisableLocalUser"]  = stUsersHead.iDisableLocalUser;
    head["ContentType"]       = stUsersHead.iContentType;

    head["CMDContent"]        = (Json::Value)rootContent;

    root.append(head);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

/* 返回给USM的处理结果 20181111*/
std::string CWLJsonParse::OSUser_Result_GetJson(__in tstring ComputerID, int iRet, std::string strRlt,
                                                            ST_USERS_INFO_HEAD & stUsersHead)
{
    Json::Value CMDContent;
    std::string sJsonPacket = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value head;

	USES_CONVERSION;

    CMDContent["RESULT"] = "SUC";
	if (iRet != 0)
    {
        CMDContent["RESULT"] = "FAIL";
        CMDContent["REASON"] = UnicodeToUTF8(CStrUtil::ConvertA2W(strRlt));//(A2W(strRlt.c_str()));

		//added by xwk 20190909  使用新的返回字段，以便兼容旧的USM
		if (WNT_RET_OTHER_GINA == iRet)//GINA正在被非WNT程序使用
		{
			CMDContent["code"]        = "0";
			CMDContent["description"] = "teg.usbkey.gninerror";
		}else
        {
           CMDContent["description"] = UnicodeToUTF8(CStrUtil::ConvertA2W(strRlt));//(A2W(strRlt.c_str()));
        }
    }

    head["ComputerID"]  = UnicodeToUTF8(ComputerID);
    head["CMDTYPE"]     = PLY_CLIENT_UPDATE_PASSWORD;
    head["CMDID"]       = PLY_SERVER_USBKEY_CERTIFICATION_STATUS;
    head["USBKeyAuth"]  = stUsersHead.iUkeyEnable;
    head["OsAuth"]      = stUsersHead.iEnableOsAuth;
    head["Safemode"]    = stUsersHead.iDisableSafeMode;
    head["DisableLocalUser"]    = stUsersHead.iDisableLocalUser;

    head["CMDContent"]    = CMDContent;

    root.append(head);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}


BOOL CWLJsonParse::USBKey_Certification_Status_GetValue(__in std::string& sJson, __out UK_AUTHENTICATION_ST* pUSBKeyDentify)
{
    BOOL bResult = FALSE;

    Json::Value     root;
    Json::Reader    reader;
    std::string     strValue = "";

    if( sJson.length() == 0)
    {
        goto _exist_;
    }

    strValue = sJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        goto _exist_;
    }

    if( root[0].isMember("USBKeyAuth"))
    {
        pUSBKeyDentify->dwUSBKeyDentify =  root[0]["USBKeyAuth"].asInt();
	}
	else
	{
		pUSBKeyDentify->dwUSBKeyDentify = 0;
	}

	if( root[0].isMember("OsAuth"))
	{
		pUSBKeyDentify->dwSystemDentify =  root[0]["OsAuth"].asInt();
	}
	else
	{
		pUSBKeyDentify->dwSystemDentify = 0;
	}

	if( root[0].isMember("Safemode"))
	{
		pUSBKeyDentify->dwSystemSafeMode =  root[0]["Safemode"].asInt();
	}
	else
	{
		pUSBKeyDentify->dwSystemSafeMode = 0;
	}

	bResult = TRUE;

_exist_:
	return bResult;

}

std::string CWLJsonParse::USBKey_SetStatusResult_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in BOOL bRet, __in UK_AUTHENTICATION_ST* pUSBKeyDentify)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	if (bRet)
	{
		CMDContent["RESULT"] = UnicodeToUTF8(L"SUC");
	}
	else
	{
		CMDContent["RESULT"] = UnicodeToUTF8(L"FAIL");
	}

	DWORD systemDentifyStatus = pUSBKeyDentify->dwSystemDentify;
	DWORD systemSafeModeStatus = pUSBKeyDentify->dwSystemSafeMode;
	DWORD usbKeyDentifyStatus = pUSBKeyDentify->dwUSBKeyDentify;

	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["USBKeyAuth"]	=(int)usbKeyDentifyStatus;
	person["OsAuth"]		=(int)systemDentifyStatus;
	person["Safemode"]		=(int)systemSafeModeStatus;

	person["CMDContent"]    = CMDContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}


BOOL CWLJsonParse::hostDefence_MAC_SubObj_GetValue(__in std::string& sJson, __out HOST_DEFENCE_MAC_ST* pHostDefenceData)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if( root[0].isMember("Enable"))
	{
		pHostDefenceData->header.nIsControlProtect = root[0]["Enable"].asInt();
	}
	else
	{
		pHostDefenceData->header.nIsControlProtect = 0;
	}

	if( root[0].isMember("Operation"))
	{
		pHostDefenceData->nOperation = root[0]["Operation"].asInt();
	}
	else
	{
		pHostDefenceData->nOperation = 0;
	}

	if( root[0].isMember("CMDContent_1"))
	{
		root_1 =  (Json::Value)root[0]["CMDContent_1"]; //进行解析

		// 进行解析
		nObject = root_1.size();
		std::wstring wsUserName;
		std::wstring wsProcessName;
		int			 iDomainID = 0;
		//wwdv3
		if (root_1.isArray())
		{
			for (int i=0; i < nObject; i++)
			{
				wsUserName    = UTF8ToUnicode(root_1[i]["UserName"].asString());
				wsProcessName = UTF8ToUnicode(root_1[i]["ProcessName"].asString());
				iDomainID	      = root_1[i]["DomainID"].asInt();

				if(wsUserName.empty()&&wsProcessName.empty())
				{
					continue;
				}
				HOST_DEFENCE_MAC_USER_ITEM stUserItem;

				_tcscpy(stUserItem.szUserName, wsUserName.c_str());
				_tcscpy(stUserItem.szProcessName, wsProcessName.c_str());
				stUserItem.ulDomainID = iDomainID;

				pHostDefenceData->vecFileAclUser.push_back(stUserItem);
			}
		}
		
	}

	if( root[0].isMember("CMDContent_2"))
	{
		root_2 =  (Json::Value)root[0]["CMDContent_2"]; //进行解析
		nObject = root_2.size();

		std::wstring wsFileName;
		int			 iDomainID;
		if (root_2.isArray())
		{
			for (int i = 0; i < nObject; i++)
			{
				HOST_DEFENCE_MAC_FILE_ITEM stFileItem;

				wsFileName = UTF8ToUnicode(root_2[i]["Filename"].asString());
				iDomainID	   = root_2[i]["DomainID"].asInt();

				_tcscpy(stFileItem.szFileName, wsFileName.c_str());
				stFileItem.ulDomainID = iDomainID;

				pHostDefenceData->vecFileAclFile.push_back(stFileItem);
			}
		}
		
	}

	pHostDefenceData->header.nUserItemCount = (ULONG)pHostDefenceData->vecFileAclUser.size();
	pHostDefenceData->header.nFileItemCount = (ULONG)pHostDefenceData->vecFileAclFile.size();

	if( root[0].isMember("Flag"))
	{
		pHostDefenceData->nIsSwitch = root[0]["Flag"].asInt();
	}
	else
	{
		if ( 0 == pHostDefenceData->header.nFileItemCount &&
			 0 == pHostDefenceData->header.nUserItemCount)
		{
			pHostDefenceData->nIsSwitch = 1;
		}
		else
		{
			pHostDefenceData->nIsSwitch = 0;
		}
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::hostDefence_MAC_SubObj_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in HOST_DEFENCE_MAC_ST* pHostDefenceData)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root2;
	Json::Value root;
	Json::Value person;
	Json::Value CMDContent_1;
	Json::Value CMDContent_2;

	Json::FastWriter writer;


	int nUserCount = (int)pHostDefenceData->vecFileAclUser.size();
	int i=0;
	for (i=0; i< nUserCount; i++)
	{
		std::wstring wsStrUserName = pHostDefenceData->vecFileAclUser[i].szUserName;
		std::wstring wsProcessName = pHostDefenceData->vecFileAclUser[i].szProcessName;
		int			 iDomainID	   = pHostDefenceData->vecFileAclUser[i].ulDomainID;

		CMDContent_1["ProcessName"] = UnicodeToUTF8(wsProcessName);
		CMDContent_1["UserName"]    = UnicodeToUTF8(wsStrUserName);
		CMDContent_1["DomainID"]    = iDomainID;

		root1.append(CMDContent_1);
	}

	int nFileCount = (int)pHostDefenceData->vecFileAclFile.size();
	for (i=0; i< nFileCount; i++)
	{
		std::wstring s1 = pHostDefenceData->vecFileAclFile[i].szFileName;
		int	 iDomainID  = pHostDefenceData->vecFileAclFile[i].ulDomainID;

		CMDContent_2["Filename"] =  UnicodeToUTF8(s1);
		CMDContent_2["DomainID"] = iDomainID;

		root2.append(CMDContent_2);
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]	= (int)cmdType;
	person["CMDID"]		= (int)cmdID;
    person["Flag"]	= (int)pHostDefenceData->nIsSwitch;;
	person["Enable"]	= (int)pHostDefenceData->header.nIsControlProtect;

	person["CMDContent_1"] = (Json::Value)root1;
	person["CMDContent_2"] = (Json::Value)root2;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::hostDefence_MAC_GlobalConfig_GetValue(__in std::string& sJson, __out VECMAC_GLOBALCONFIG &vecMAC_Globalconfig)
{
	BOOL bResult = FALSE;

	Json::Value  root;
	Json::Value  DomainRule;
	Json::Value  ExDomainRule;
	Json::Reader reader;
	std::string	 strValue = "";

	std::wstring wsDomainName;
	int			 iDomainID	     = 0;
	int			 iInDomainAction = 0;
	int			 iExDomainAction = 0;
	int			 iExDomainID	 = 0;
	int			 iAction		 = 0;


	HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST stGlobalConfigItem;
	VECGLOBALCONFIG_EXCEPTIOIN			  vecGlobalConfigEx;

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int iDomainRuleCount = root.size();
	if( iDomainRuleCount < 1 || !root.isArray())
	{
		goto _exist_;
	}


	if( root[0].isMember("CMDContent"))
	{
		DomainRule =  (Json::Value)root[0]["CMDContent"]; //进行解析

		/* 安全域数量 */
		iDomainRuleCount = DomainRule.size();
		//wwdv3
		if (DomainRule.isArray())
		{
			for (int i=0; i < iDomainRuleCount; i++)
			{
				/* 某安全域信息 */
				wsDomainName    = UTF8ToUnicode(DomainRule[i]["DomainName"].asString());
				iDomainID	    = DomainRule[i]["DomainID"].asInt();
				iInDomainAction	= DomainRule[i]["InDomainAction"].asInt();
				iExDomainAction	= DomainRule[i]["ExDomainAction"].asInt();

				memset(&stGlobalConfigItem, 0, sizeof(HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST));

				_tcscpy(stGlobalConfigItem.szDomainName, wsDomainName.c_str());
				stGlobalConfigItem.ulDomainID = iDomainID;
				stGlobalConfigItem.ulInACRight = iInDomainAction;
				stGlobalConfigItem.ulExACRight = iExDomainAction;

				/* 某安全域域间例外信息 */
				if(DomainRule[i].isMember("ExceptionDomainRule"))
				{
					vecGlobalConfigEx.clear();
					ExDomainRule = (Json::Value)DomainRule[i]["ExceptionDomainRule"];//DomainRule[i].isMember("ExceptionDomainRule");

					int iExRuleCount = ExDomainRule.size();
					for(int j = 0; j < iExRuleCount; j++)
					{
						HOST_DEFENCE_MAC_GLOBALCONFIG_EXCEPTION_ITEM_ST stGlobalConfigExItem;
						memset(&stGlobalConfigExItem, 0, sizeof(HOST_DEFENCE_MAC_GLOBALCONFIG_EXCEPTION_ITEM_ST));

						stGlobalConfigExItem.ulSubjectID = (int)ExDomainRule[j]["exDomainID"].asInt();
						stGlobalConfigExItem.ulExACRight = (int)ExDomainRule[j]["Action"].asInt();

						vecGlobalConfigEx.push_back(stGlobalConfigExItem);
					}
				}

				stGlobalConfigItem.ulExCount = (ULONG)vecGlobalConfigEx.size();

				HOST_DEFENCE_MAC_GLOBALCONFIG_ST stMacGlobalConfig;
				memcpy(&stMacGlobalConfig.stGlobalConfigItem, &stGlobalConfigItem, sizeof(HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST));
				stMacGlobalConfig.vecGlobalConfigEx.assign(vecGlobalConfigEx.begin(), vecGlobalConfigEx.end());

				vecMAC_Globalconfig.push_back(stMacGlobalConfig);
			}
		}
		
	}



	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::hostDefence_MAC_GlobalConfig_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in VECMAC_GLOBALCONFIG &vecMAC_Globalconfig)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value DomainRule;
	Json::Value root;
	Json::Value person;
	Json::Value DomainRuleSub;
	Json::Value DomainRuleSubTitle;
	Json::Value ExDomainRule;

	Json::FastWriter writer;

	std::wstring wsDomainName;
	int			 iDomainID	     = 0;
	int			 iInDomainAction = 0;
	int			 iExDomainAction = 0;
	int			 iExDomainID	 = 0;
	int			 iAction		 = 0;


	int iConfigCount = (int)vecMAC_Globalconfig.size();
	for (int i=0; i< iConfigCount; i++)
	{
		HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST stGlobalConfigItem;
		VECGLOBALCONFIG_EXCEPTIOIN			  vecGlobalConfigEx;

		memset(&stGlobalConfigItem, 0, sizeof(HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST));
		memcpy(&stGlobalConfigItem, &vecMAC_Globalconfig[i].stGlobalConfigItem, sizeof(HOST_DEFENCE_MAC_GLOBALCONFIG_ITEM_ST));

		vecGlobalConfigEx.assign(vecMAC_Globalconfig[i].vecGlobalConfigEx.begin(), vecMAC_Globalconfig[i].vecGlobalConfigEx.end());

		/* 添加安全域信息 */
		DomainRuleSub["DomainID"]		= (int)stGlobalConfigItem.ulDomainID;
		DomainRuleSub["DomainName"]		= UnicodeToUTF8(stGlobalConfigItem.szDomainName);
		DomainRuleSub["InDomainAction"] = (int)stGlobalConfigItem.ulInACRight;
		DomainRuleSub["ExDomainAction"] = (int)stGlobalConfigItem.ulExACRight;

		DomainRuleSubTitle.clear();

		/* 添加安全域间例外信息 */
		int	iExCount = (int)vecGlobalConfigEx.size();
		if(0 == iExCount)
		{

			DomainRuleSub["ExceptionDomainRule"] = (Json::Value)DomainRuleSubTitle;
		}
		else
		{

			for(int j = 0; j < iExCount; j++)
			{

				ExDomainRule["exDomainID"] = (int)vecGlobalConfigEx[j].ulSubjectID;
				ExDomainRule["Action"]	   = (int)vecGlobalConfigEx[j].ulExACRight;

				DomainRuleSubTitle.append(ExDomainRule);

				DomainRuleSub["ExceptionDomainRule"] = (Json::Value)DomainRuleSubTitle;
			}
		}

		DomainRule.append(DomainRuleSub);

	}


	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]	= (int)cmdType;
	person["CMDID"]		= (int)cmdID;

	person["CMDContent"] = (Json::Value)DomainRule;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

// 【访问控制】- 强制访问控制-例外规则配置
BOOL CWLJsonParse::hostDefence_MAC_ExCeption_GetValue(__in std::string& sJson, __out VECMAC_EXCEPTION& veMACException,__out BOOL *pbReplace)
{
	BOOL bResult = FALSE;

	Json::Value root;
	Json::Value ExceptionObjectRule;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

	if(root[0].isMember("operation"))
	{
		std::wstring wsOperation;
		wsOperation     = UTF8ToUnicode(root[0]["operation"].asString());
		if(wsOperation.compare(_T("replace")) == 0)
		{
			*pbReplace = TRUE;
		}
		else
		{
			*pbReplace = FALSE;
		}
	}

	if( root[0].isMember("CMDContent"))
	{
		ExceptionObjectRule =  (Json::Value)root[0]["CMDContent"]; //进行解析

		std::wstring wsObjName;
		std::wstring wsSubUserName;
		std::wstring wsProcessName;
		int			 iDomainID = 0;
		int			 iAction   = 0;

		/* 解析例外规则详细信息 */
		nObject = ExceptionObjectRule.size();
		//wwdv3
		if (ExceptionObjectRule.isArray())
		{
			for (int i=0; i < nObject; i++)
			{
				wsObjName     = UTF8ToUnicode(ExceptionObjectRule[i]["Filename"].asString());
				wsSubUserName = UTF8ToUnicode(ExceptionObjectRule[i]["UserName"].asString());
				wsProcessName = UTF8ToUnicode(ExceptionObjectRule[i]["process"].asString());
				iDomainID	  = ExceptionObjectRule[i]["DomainID"].asInt();
				iAction		  = ExceptionObjectRule[i]["Action"].asInt();

				HOST_DEFENCE_MAC_EXCEPTION_ITEM_ST stExceptionItem;
				memset(&stExceptionItem, 0, sizeof(HOST_DEFENCE_MAC_EXCEPTION_ITEM_ST));

				_tcscpy(stExceptionItem.szObjectName, wsObjName.c_str());
				_tcscpy(stExceptionItem.szSubjectUserName, wsSubUserName.c_str());
				_tcscpy(stExceptionItem.szSubjectProcessName, wsProcessName.c_str());
				stExceptionItem.ulDomainID = iDomainID;
				stExceptionItem.ulACRight  = iAction;

				veMACException.push_back(stExceptionItem);
			}
		}
		
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::hostDefence_MAC_ExCeption_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in VECMAC_EXCEPTION& veMACException, __in BOOL bReplace)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value root1;
	Json::Value root;
	Json::Value person;
	Json::Value ExceptionObjectRule;

	Json::FastWriter writer;

	/* 处理例外规则信息 */
	int iExceptionCount = (int)veMACException.size();
	for (int i=0; i< iExceptionCount; i++)
	{
		ExceptionObjectRule["Filename"]	   = UnicodeToUTF8(veMACException[i].szObjectName);
		ExceptionObjectRule["UserName"]	   = UnicodeToUTF8(veMACException[i].szSubjectUserName);
		ExceptionObjectRule["process"]	   = UnicodeToUTF8(veMACException[i].szSubjectProcessName);
		ExceptionObjectRule["DomainID"]	   = (int)veMACException[i].ulDomainID;
		ExceptionObjectRule["Action"]	   = (int)veMACException[i].ulACRight;

		root1.append(ExceptionObjectRule);
	}


	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	if(bReplace)
	{
		person["operation"]		= UnicodeToUTF8(_T("replace"));
	}
	else
	{
		person["operation"]		= UnicodeToUTF8(_T("add"));
	}

	person["CMDContent"]  = (Json::Value)root1;


	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

/*
this function 
user sjon string, process compose to totaljson.
*/

/*
* @fn           IsJsonMember
* @brief        工具函数：查看readRoot中有无str对象，如果有，则写入writeJson
* @param[in]    str: Json字段名
				readRoot: 被读Json对象
* @param[out]   writeRoot: 待写入Json对象
* @return      
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-25
*/
void IsJsonMember(const std::string &str, __in const Json::Value& readRoot,__out Json::Value &writeRoot)
{
	if(writeRoot.isMember(str))
	{
		for (int i = 0; i < readRoot[str].size(); i++)
		{
			writeRoot[str].append((Json::Value)readRoot[str][i]);
		}
	}
	else
	{
		writeRoot[str] = (Json::Value)readRoot[str];
	}
}

/*
* @fn           detect_GetSecDetect_GetJson
* @brief        重载函数：获取安全监测列表数据生成Json字符串
* @param[in]    ComputerID:
				cmdType:
				cmdID:
				vecStrJson: 保存了安全监测Json数据的vec
				bIsServer: 标志位，当前数据传入端是客户\服务
* @param[out]   
* @return       string: 返回安全监测各数据段拼接的Json字符串
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-21
*/
std::string CWLJsonParse::detect_GetSecDetect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, 
													  __in std::map<__WL_SECDETECT_TYPE, std::string> mapDetectUser, BOOL bIsServer, int iSendType)
{
	/*
	// Json 字段内容 每完成一项时更新
	{
	   "ComputeID" :"Admin",
	   "CMDTYPE":200,
	   "CMDID":20,
       "Type":1, //1 等保标准
       "SendType":1 //1 表示立即获取； 2 表示周期上报
	   "CMDContent":{
    		"UserList": [
        		{
            	        "Name":"Admin",
                        "FullName":"",
                        "Status":1, （ 1 - 启用； 0 - 禁用）
                        "UserAttr":2, （2 - 管理员，其他都为普通用户）
                        "LastTime":"2012-12-02 14:39:14"
        		}
            ],
            "Share":[
            ],
			"Service": [
                {
						"?NameType":"WCHAR",
						"?VersionType":"wchar_t",
						"?CompanyType":"wchar_t",
						"?DespType":"wchar_t",  
						"?RunningType": "DWORD（0 - 已停止； 1 - 正在运行）",
                        
                        "Name":"AJRouter",
                        "Version":"6.2.18362.1",
                        "Company":"Microsoft Corporation",
                        "Desp":"AllJoyn Router Service",
                        "Running":0 （0 - 已停止； 1 - 正在运行）
                }
            ],
			"HostInfo":[ （主机信息）
                    {
                        "Host":"JIELUO-PC",
                        "Domain":"WORKGROUP",
                        "OS":"Windows 7 sp1 64bit",
                        "OsVersion":"6.1.7601",
                        "CPU":"Intel(R) Core(TM) i5-6500 CPU @ 3.20GHz",
                        "CPUNum":1, （CPU个数）
                        "CPUFreq":"1696 MHz",
                        "CPUType":"Intel64 Family 6 Model 94 Stepping 3",
                        "BiosProv":"LENOVO",
                        "BiosVer":"ACRSYS - 12f0",
                        "MemorySize":"15.91 GB",
                        "MemoryRev":"7.62 GB"
                    }
                ]

		}
	}
	*/

	std::string sJsonPacket = "";
	Json::Value subRoot;
	Json::Value Root;
	Json::Value root1;
	Json::Value person;
	Json::FastWriter writer;
	Json::Reader reader;

	std::map <__WL_SECDETECT_TYPE ,std::string > ::iterator it;
	
	// 客户端获取的
	for(it = mapDetectUser.begin(); it != mapDetectUser.end(); it++) 
	{
        subRoot.clear();
		if (!reader.parse(it->second, subRoot))
		{
			if(it != mapDetectUser.end())
			{

				continue;
			}
			else
			{
				goto _exist_;
			}
		}

		switch (it->first)
		{
		    case SECDETECT_BASELINE:
				IsJsonMember("SecBaseLine", subRoot, root1);
				break;
    		case SECDETECT_SERVICES:
				IsJsonMember("Service", subRoot, root1);
				break;
    		case SECDETECT_PROGRAM:
				IsJsonMember("Program", subRoot, root1);
    			break;
    		case SECDETECT_PROCESS:
				IsJsonMember("Process", subRoot, root1);
    			break;
    		case SECDETECT_PARTITION:
				IsJsonMember("Partition", subRoot, root1);
    			break;
    		case SECDETECT_SHARE:
				IsJsonMember("Share", subRoot, root1);
    			break;
    		case SECDETECT_USER:
				IsJsonMember("UserList", subRoot, root1);
                break;
            case SECDETECT_WEAK_PASSWORD:
                IsJsonMember("WeakPwd", subRoot, root1);
                break;
            case SECDETECT_DEFENSE:
                IsJsonMember("Defense", subRoot, root1);
                break;
            case SECDETECT_SCORE:
                IsJsonMember("Score", subRoot, root1);
                break;
            case SECDETECT_VUL:
                IsJsonMember("VulInfo", subRoot, root1);
    			break;
    	    case SECDETECT_AUTORUN:
				IsJsonMember("AutoRuns", subRoot, root1);
    			break;
			case SECDETECT_HOSTINFO:
				IsJsonMember("HostInfo", subRoot, root1);
    			break;
			case SECDETECT_PORTLIST:
				IsJsonMember("PortList", subRoot, root1);
				break;
			case SECDETECT_NETCONFIG:
				IsJsonMember("NetConfig", subRoot, root1);
				break;
			case SECDETECT_NETFIREWALL:
				IsJsonMember("NetFireWall", subRoot, root1);
				break;
            case SECDETECT_HARDWARE:
                IsJsonMember("Hardware", subRoot, root1);
                break;
            case SECDETECT_NETCONNECT:
                IsJsonMember("NetConnect", subRoot, root1);
                break;
    		default:
    			continue;
		}

	}


_exist_:

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
    person["CMDVER"] = 1; //无用，暂时加上
	person["CMDID"] = (int)cmdID;
    person["Type"] = 1; //等保标准
    person["SendType"] = iSendType; //1 表示安全检测内容是立即获取；2 表示安全检测内容是周期上报
	person["CMDContent"] = (Json::Value)root1;

	Root.append(person);
	sJsonPacket = writer.write(Root);
	Root.clear();
	root1.clear();
	return sJsonPacket;
}


/*
* @fn           detect_GetSecDetect_GetValue
* @brief        安全基线所有数据的Json字符串拆分
* @param[in]    sJson: 安全基线数据的json字符串
* @param[out]   vecStrJson: 拆分后的Json字符串
* @return       BOOL: 如果拆分成功则返回true,反之返回false
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-20
*/

BOOL CWLJsonParse::detect_GetSecDetect_GetValue(__in std::string sJson, 
             __out std::map<__WL_SECDETECT_TYPE, std::string> &mapStrJson)
{
	BOOL bRet = FALSE;
	Json::Value     root;
	Json::Value     rootContent;
	Json::Reader    reader;
	Json::FastWriter writer;
	std::string     strValue = "";
	std::string		strJson;

	std::map<std::string,__WL_SECDETECT_TYPE> mapIdName; // C++ 98
    std::map<std::string, __WL_SECDETECT_TYPE>::iterator it;

	mapIdName["SecBaseLine"] = SECDETECT_BASELINE;
	mapIdName["UserList"]  = SECDETECT_USER;
	mapIdName["Program"]   = SECDETECT_PROGRAM;
	mapIdName["Process"]   = SECDETECT_PROCESS;
	mapIdName["Service"]   = SECDETECT_SERVICES;
	mapIdName["Share"]     = SECDETECT_SHARE;
	mapIdName["Partition"] = SECDETECT_PARTITION;
    //V300R002 - 2021-09-02
    mapIdName["Defense"] = SECDETECT_DEFENSE;
    mapIdName["Score"] = SECDETECT_SCORE;
    mapIdName["VulInfo"] = SECDETECT_VUL;
	mapIdName["AutoRuns"]  = SECDETECT_AUTORUN;
	mapIdName["HostInfo"]  = SECDETECT_HOSTINFO;
	mapIdName["PortList"]  = SECDETECT_PORTLIST;
	mapIdName["NetConfig"]  = SECDETECT_NETCONFIG;
	mapIdName["NetFireWall"]  = SECDETECT_NETFIREWALL;
    //V300R007 2023-06-02
    mapIdName["Hardware"] = SECDETECT_HARDWARE;
    mapIdName["NetConnect"] = SECDETECT_NETCONNECT;
    //V300R009 2024-08-26
    mapIdName["WeakPwd"] = SECDETECT_WEAK_PASSWORD;

	if( sJson.length() == 0 )
	{
		goto __EXIT;
	}

	strValue = sJson;
	
	if (!reader.parse(sJson, root) || !root.isArray())
	{
		goto __EXIT;
	}
	
	root = root[0]; // 传进来的是一个数组

	// 1. 读取Json
	// 1.1 重写Json对象
	if(root.isMember("CMDContent"))
	{
		root = root["CMDContent"];
	}
	else
	{
		for (it = mapIdName.begin(); it != mapIdName.end(); it++)
		{
			string t = "{\""+ it->first + "\": null}";
			mapStrJson[it->second] = t;
		}
		// 要写入空对象
		goto __EXIT;
	}

	for (it = mapIdName.begin(); it != mapIdName.end(); it++)
	{
		std::string strMemName = it->first;

		if(root.isMember(strMemName))
		{
			rootContent = (Json::Value)root[strMemName];
			strJson = writer.write(rootContent);
		}
		else
		{
			strJson = "{\""+ strMemName + "\": null}";
		}

		mapStrJson[it->second] = strJson;
	}

	bRet = TRUE;

__EXIT:
	
    return bRet;    
}

std::string CWLJsonParse::detect_GetSecDetect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID,
													  __in std::vector<SECDETECT_STRUCT> VecDetect, BOOL bIsServer)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;

	int nCount = (int)VecDetect.size();
	std::wstring strName;
	std::wstring strDesp;
	if (!bIsServer)
	{
		Json::Value CMDContent;
		// 客户端获取的
		for (int i=0; i< nCount; i++)
		{
			strName = VecDetect[i].strName;
			strDesp = VecDetect[i].strDesp;
			CMDContent["Name"] = UnicodeToUTF8(strName);
			CMDContent["Desp"] = UnicodeToUTF8(strDesp);
			root1.append(CMDContent);
		}
	}
	else
	{
		Json::Value CMDContent_BaseLine;
		Json::Value CMDContent_Process;
		Json::Value CMDContent_Program;
		Json::Value CMDContent_Services;
		Json::Value CMDContent_User;
		Json::Value CMDContent_Share;
		Json::Value CMDContent_Partition;
		for (int i=0; i< nCount; i++)
		{
			if(_tcscmp(VecDetect[i].strName,_T("<BaseLine>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					// USM下发的，需要转换
					strName = GetSecDetectKey( VecDetect[i].strName );
					strDesp = VecDetect[i].strDesp;
					CMDContent_BaseLine[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_BaseLine);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<Process>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_Process[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_Process);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<Program>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_Program[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_Program);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<Services>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_Services[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_Services);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<User>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_User[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_User);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<Share>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_Share[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_Share);
			}

			if(_tcscmp(VecDetect[i].strName,_T("<Partition>")) == 0)
			{
				int iCount = _ttoi(VecDetect[i].strDesp);
				i++;
				for( int iNum = 0; iNum<iCount; iNum++)
				{
					strName = VecDetect[i].strName;
					strDesp = VecDetect[i].strDesp;
					CMDContent_Partition[UnicodeToUTF8(strName)] = UnicodeToUTF8(strDesp);
					i++;
				}
				root1.append(CMDContent_Partition);
			}
		}
	}

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	person["CMDContent"] = (Json::Value)root1;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::detect__GetSecDetect_GetValue(__in std::string& sJson, __out std::vector<SECDETECT_STRUCT> &VecDetect)
{
	BOOL bResult = FALSE;

	Json::Value     root;
	Json::Value     rootContent;
	Json::Reader    reader;
	std::string     strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

	strValue = sJson;

	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		goto _exist_;
	}

	if( root[0].isMember("CMDContent"))
	{
		rootContent =  (Json::Value)root[0]["CMDContent"]; //进行解析

		std::wstring wsName;
		std::wstring wsDesp;

		/* 解析详细信息 */
		int  nObject = rootContent.size();
		for (int i=0; i < nObject; i++)
		{
			wsName = UTF8ToUnicode(rootContent[i]["Name"].asString());
			wsDesp = UTF8ToUnicode(rootContent[i]["Desp"].asString());

			SECDETECT_STRUCT stDetectItem;
			_tcscpy(stDetectItem.strName, wsName.c_str());
			_tcscpy(stDetectItem.strDesp, wsDesp.c_str());

			VecDetect.push_back(stDetectItem);
		}
	}

	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::Illegal_Connect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in int nRet, __in int nApplyRet, __in ST_ILLEGAL_INFO* pIllegalInfo)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;


	int nCount = (int)pIllegalInfo->vec_Info.size();
	for (int i=0; i< nCount; i++)
	{
		CMDContent["host"]	= UnicodeToUTF8(pIllegalInfo->vec_Info[i].szIllegalAdress);
		CMDContent["desc"]	= UnicodeToUTF8(pIllegalInfo->vec_Info[i].szIllegalInfo);
		root1.append(CMDContent);
	}
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["Enable"]		= (int)nRet;
    person["ApplyEnable"]  = (int)nApplyRet;
	person["CMDContent"] = (Json::Value)root1;
	root.append(person);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

BOOL CWLJsonParse::Illegal_Connect_ParseJson(__in std::string& sJson, __out ST_ILLEGAL_INFO* pIllegalInfo)
{
	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		return FALSE;
	}


	if( root[0].isMember("addOrFull"))
	{
		pIllegalInfo->dwAddOrFull=root[0]["addOrFull"].asInt();
	}
	else
	{
		pIllegalInfo->dwAddOrFull=UFUM_REPLACE;
	}

	if( root[0].isMember("section"))
	{
		pIllegalInfo->dwSection=root[0]["section"].asInt();
	}
	else
	{
		pIllegalInfo->dwSection=UFSM_ALL;
	}

	if( root[0].isMember("Enable"))
	{
		pIllegalInfo->dwIllegalSwitch=root[0]["Enable"].asInt();
	}
    if ( root[0].isMember("ApplyEnable"))
    {
        pIllegalInfo->bApplySlots = root[0]["ApplyEnable"].asInt();
    }
	if (root[0].isMember("CMDContent"))
	{
		root_Content =  (Json::Value)root[0]["CMDContent"];
		int nObject = root_Content.size();
		for (int i=0; i < nObject; i++)
		{
			ILLEGAL_INFO st_info;
			StrCpyW(st_info.szIllegalAdress,UTF8ToUnicode(root_Content[i]["host"].asString()).c_str());
			StrCpyW(st_info.szIllegalInfo,UTF8ToUnicode(root_Content[i]["desc"].asString()).c_str());
			pIllegalInfo->vec_Info.push_back(st_info);
		}
	}
	return TRUE;
}

std::string CWLJsonParse::Illegal_Connect_GetJsonLog(__in ILLEGAL_LOG* pIllegalLog)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pIllegalLog->vec_log_info.size();
	for (int i=0; i< nCount; i++)
	{
		CMDContent["time"]	= UnicodeToUTF8(pIllegalLog->vec_log_info[i].szTime);
		CMDContent["host"]	= UnicodeToUTF8(pIllegalLog->vec_log_info[i].szHost);
		CMDContent["ip"]	= UnicodeToUTF8(pIllegalLog->vec_log_info[i].szIP);
		CMDContent["state"]	= pIllegalLog->vec_log_info[i].nState;
		root1.append(CMDContent);
	}
	person["ComputerID"]	= UnicodeToUTF8(pIllegalLog->szComputerID);
	person["CMDTYPE"]		= (int)pIllegalLog->nCMDTYPE;
	person["CMDID"]			= (int)pIllegalLog->nCMDID;
	person["CMDContent"] = (Json::Value)root1;
	root.append(person);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

/*
* @fn           OSResource_Connect_GetJsonLog
* @brief        组合Json，用于上传USM系统资源阈值告警日志
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023. create it.
*/
std::string CWLJsonParse::OSResource_Connect_GetJsonLog(__in OSRESOURCE_LOG* pOSResourceLog)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)pOSResourceLog->vec_log_info.size();
	for (int i=0; i< nCount; i++)
	{
		CMDContent["Message"]	= UnicodeToUTF8(pOSResourceLog->vec_log_info[i].szWarnLog);
		//CMDContent["ResourceType"]	= UnicodeToUTF8(pOSResourceLog->vec_log_info[i].TypeLevel2);
		root1.append(CMDContent);
	}
	person["ComputerID"]	= UnicodeToUTF8(pOSResourceLog->szComputerID);
	person["CMDTYPE"]		= (int)pOSResourceLog->nCMDTYPE;
	person["CMDID"]			= (int)pOSResourceLog->nCMDID;
	person["CMDContent"] = (Json::Value)root1;
	root.append(person);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

/*
* @fn           OSResource_GetJsonByVector
* @brief        批量组合Json，用于上传USM系统资源阈值告警日志
* @param[in]    ComputerID - 计算机ID
* @param[in]    cmdType - 命令类型
* @param[in]    cmdID - 命令ID
* @param[in]    vecLogs - 日志数据向量
* @return       JSON字符串
*               
* @author       lll
* @modify：		2025.12.01 create it.
*/
std::string CWLJsonParse::OSResource_GetJsonByVector(__in tstring ComputerID, 
                                                      __in WORD cmdType, __in WORD cmdID, 
                                                      __in vector<CWLMetaData*>& vecLogs)
{
    std::string sJsonPacket;

    if( vecLogs.empty())
    {
        return sJsonPacket;
    }

    Json::FastWriter writer;
    Json::Value root;
    Json::Value logJson;
    Json::Value logs;
    int nCount = (int)vecLogs.size();
    
    for (int i = 0; i < nCount; i++)
    {
        Json::Value item;
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecLogs[i]->GetData();
        OSRESOURCE_LOG_STRUCT *pOSLog = (OSRESOURCE_LOG_STRUCT*)pipclogcomm->data;

        item["Message"] = UnicodeToUTF8(pOSLog->szWarnLog);          // 告警信息
        item["ResourceType"] = (int)pOSLog->TypeLevel2;             // 资源类型（CPU/内存/磁盘）
        item["Time"] = (Json::UInt64)pOSLog->llTime;                // 告警时间戳
        logs.append(item);
    }

    logJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    logJson["CMDTYPE"] = (int)cmdType;
    logJson["CMDID"] = (int)cmdID;
    logJson["CMDContent"] = logs;

    root.append(logJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

std::wstring CWLJsonParse::GetSecDetectKey( __in TCHAR strName[128] )
{
	// 需要给管理平台转换key
	std::wstring strKey;

	// 审核
	if(_tcscmp(strName,_T("AUDITSYSTEM")) == 0)
	{
		strKey = _T("dwAuditCategorySystem");
	}
	if(_tcscmp(strName,_T("AUDITLOGON")) == 0)
	{
		strKey = _T("dwAuditCategoryLogon");
	}
	if(_tcscmp(strName,_T("AUDITOBJECTACCESS")) == 0)
	{
		strKey = _T("dwAuditCategoryObjectAccess");
	}
	if(_tcscmp(strName,_T("AUDITPRIVILEGEUSE")) == 0)
	{
		strKey = _T("dwAuditCategoryPrivilegeUse");
	}
	if(_tcscmp(strName,_T("AUDITDETAILEDTRACKING")) == 0)
	{
		strKey = _T("dwAuditCategoryDetailedTracking");
	}
	if(_tcscmp(strName,_T("AUDITPOLICYCHANGE")) == 0)
	{
		strKey = _T("dwAuditCategoryPolicyChange");
	}
	if(_tcscmp(strName,_T("AUDITACCOUNTMANAGEMENT")) == 0)
	{
		strKey = _T("dwAuditCategoryAccountManagement");
	}
	if(_tcscmp(strName,_T("AUDITDIRECTORYSERVICEACCESS")) == 0)
	{
		strKey = _T("dwAuditCategoryDirectoryServiceAccess");
	}
	if(_tcscmp(strName,_T("AUDITACCOUNTLOGON")) == 0)
	{
		strKey = _T("dwAuditCategoryAccountLogon");
	}

	// 账户策略
	if(_tcscmp(strName,_T("PASSWORDCOMPLEXITY")) == 0)
	{
		strKey = _T("dwPasswordComplexity");
	}
	if(_tcscmp(strName,_T("MINPASSWDLEN")) == 0)
	{
		strKey = _T("dwMin_passwd_len");
	}
	if(_tcscmp(strName,_T("PASSWORDHISTLEN")) == 0)
	{
		strKey = _T("dwPassword_hist_len");
	}
	if(_tcscmp(strName,_T("MAXPASSWDAGE")) == 0)
	{
		strKey = _T("dwMax_passwd_age");
	}
	if(_tcscmp(strName,_T("LOCKOUTTHRESHOLD")) == 0)
	{
		strKey = _T("dwLockout_threshold");
	}
	if(_tcscmp(strName,_T("DISABLEGUEST")) == 0)
	{
		strKey = _T("dwDisable_guest");
	}
	if(_tcscmp(strName,_T("USRMOD3LOCKOUTDURATION")) == 0)
	{
		strKey = _T("dwUsrmod3_lockout_duration");
	}
	if(_tcscmp(strName,_T("USRMOD3LOCKOUTOBSERVATION_WINDOW")) == 0)
	{
		strKey = _T("dwUsrmod3_lockout_observation_window");
	}

	// 安全选项策略
	if(_tcscmp(strName,_T("CLEARPAGESHUTDOWN")) == 0)
	{
		strKey = _T("dwClearPageShutDown");
	}
	if(_tcscmp(strName,_T("DONTDISPLAYLASTUSERNAME")) == 0)
	{
		strKey = _T("dwDontDisplayLastUserName");
	}
	if(_tcscmp(strName,_T("DISABLECAD")) == 0)
	{
		strKey = _T("dwDisableCAD");
	}
	if(_tcscmp(strName,_T("RESTRICTANONYMOUSSAM")) == 0)
	{
		strKey = _T("dwRestrictAnonymousSam");
	}
	if(_tcscmp(strName,_T("RESTRICTANONYMOUS")) == 0)
	{
		strKey = _T("dwRestrictAnonymous");
	}
	if(_tcscmp(strName,_T("AUTORUN")) == 0)
	{
		strKey = _T("dwAutoRun");
	}
	if(_tcscmp(strName,_T("SHARE")) == 0)
	{
		strKey = _T("dwShare");
	}

	// 新安全基线
	if(_tcscmp(strName,_T("SYSTEMLOG")) == 0)
	{
		strKey = _T("dwSystemLog");
	}
	if(_tcscmp(strName,_T("SECLOG")) == 0)
	{
		strKey = _T("dwSeclog");
	}
	if(_tcscmp(strName,_T("APPLICATIONLOG")) == 0)
	{
		strKey = _T("dwApplicationLog");
	}
	if(_tcscmp(strName,_T("FORCEDLOGOFF")) == 0)
	{
		strKey = _T("dwForcedLogoff");
	}
	if(_tcscmp(strName,_T("FORBIDFLOPPYCOPY")) == 0)
	{
		strKey = _T("dwForbidFloppyCopy");
	}
	if(_tcscmp(strName,_T("FORBIDCONSOLEAUTOLOGIN")) == 0)
	{
		strKey = _T("dwForbidConsoleAutoLogin");
	}
	if(_tcscmp(strName,_T("FORBIDAUTOSHUTDOWN")) == 0)
	{
		strKey = _T("dwForbidAutoShutdown");
	}
	if(_tcscmp(strName,_T("CACHEDLOGONSCOUNT")) == 0)
	{
		strKey = _T("dwCachedLogonsCount");
	}
	if(_tcscmp(strName,_T("DISABLEDOMAINCREDS")) == 0)
	{
		strKey = _T("dwDisableDomainCreds");
	}
	if(_tcscmp(strName,_T("FORBIDGETHELP")) == 0)
	{
		strKey = _T("dwForbidGethelp");
	}
	if(_tcscmp(strName,_T("FORBIDAUTOREBOOT")) == 0)
	{
		strKey = _T("dwForbidAutoReboot");
	}
	if(_tcscmp(strName,_T("FORBIDAUTOLOGIN")) == 0)
	{
		strKey = _T("dwForbidAutoLogin");
	}
	if(_tcscmp(strName,_T("FORBIDCHANGEIP")) == 0)
	{
		strKey = _T("dwForbidChangeIp");
	}
	if(_tcscmp(strName,_T("FORBIDCHANGENAME")) == 0)
	{
		strKey = _T("dwForbidChangeName");
	}
	if(_tcscmp(strName,_T("ENABLEUAC")) == 0)
	{
		strKey = _T("dwEnableUac");
	}
	if(_tcscmp(strName,_T("CHANGERDPPORTNUMBER")) == 0)
	{
		strKey = _T("dwRdpRortNum");
	}
	if(_tcscmp(strName,_T("DEPIN")) == 0)
	{
		strKey = _T("dwDepIn");
	}
	if(_tcscmp(strName,_T("DEPOUT")) == 0)
	{
		strKey = _T("dwDepOut");
	}

    //new
    if(_tcscmp(strName,_T("SCRNSAVE")) == 0)
    {
        strKey = _T("dwScrnSave");
    }
    if(_tcscmp(strName,_T("DELETEIPFORWARDENTRY")) == 0)
    {
        strKey = _T("dwDeleteIpForwardEntry");
    }
    if(_tcscmp(strName,_T("REMOTEHOSTRDP")) == 0)
    {
        strKey = _T("dwRemoteHostRDP");
    }
    if(_tcscmp(strName,_T("REMOTELOGINTIME")) == 0)
    {
        strKey = _T("dwRemoteLoginTime");
    }
    if(_tcscmp(strName,_T("FORBIDADMINTURNOFF")) == 0)
    {
        strKey = _T("dwForbidAdminToTurnOff");
    }
    if(_tcscmp(strName,_T("SYN_ATTACK_PROTECT")) == 0)
    {
        strKey = _T("dwSynAttackDetectionDesign");
    }
    if(_tcscmp(strName,_T("USER_PWD_EXPIRE_PROMPT")) == 0)
    {
        strKey = _T("dwPasswordExpiryWarning");
    }
    if(_tcscmp(strName,_T("DISABLE_DEFAULT_OSUSER")) == 0)
    {
        strKey = _T("dwForbidDefaultOPTUser");
    }


	return strKey;
}

std::string CWLJsonParse::DiskInfo_GetJson(__in vecDiskInfo& vec_DiskInfo)
{
	Json::Value root1;
	Json::Value root;
	Json::Value hardDiskInfo;
	Json::FastWriter writer;
	int nCount = (int)vec_DiskInfo.size();

	//for (int i=0; i< nCount; i++)
	//{
	//	std::wstring strDirve = vec_DiskInfo[i].szDirve;
	//	std::wstring strTotalSize = vec_DiskInfo[i].szTotalSize;
	//	std::wstring strFreeSize = vec_DiskInfo[i].szFreeSize;

	//	root["letter"]	= UnicodeToUTF8(strDirve.c_str());
	//	root["capacity"] = UnicodeToUTF8(strTotalSize.c_str());
	//	root["remainder"] = UnicodeToUTF8(strFreeSize.c_str());
	//	root1.append(root);
	//}
	//
	hardDiskInfo["hardDiskInfo"] = root1;
	const string& strlog=hardDiskInfo.toStyledString();
	std::string strValue =  writer.write(hardDiskInfo);

	return strValue;
}


std::string CWLJsonParse::UpLoad_UserInfo_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in ST_USERAUTHORIZE_INFO* pUserAuthorizeInfo)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;
	int nCount = (int)pUserAuthorizeInfo->vec_Info.size();

	for (int i=0; i< nCount; i++)
	{
		std::wstring wsTemp = convertTimeTToStr(::time(NULL));
		CMDContent["Time"] = UnicodeToUTF8(wsTemp);
		CMDContent["UserName"]	= UnicodeToUTF8(pUserAuthorizeInfo->vec_Info[i].szUserAuthorizeUserName);
		CMDContent["Password"]	= UnicodeToUTF8(pUserAuthorizeInfo->vec_Info[i].szUserAuthorizePassWord);
		CMDContent["RoleID"]	= pUserAuthorizeInfo->vec_Info[i].RoleID;
		CMDContent["authority"]	= (int)pUserAuthorizeInfo->vec_Info[i].dwAuthorizeValue;
		CMDContent["IsDefault"] = (int)pUserAuthorizeInfo->vec_Info[i].dwIsDefault; 
		CMDContent["IsFirstLogin"] = (int)pUserAuthorizeInfo->vec_Info[i].dwIsFirstLogin; 
		CMDContent["PwdUpdateTime"] = pUserAuthorizeInfo->vec_Info[i].llPwdUpdateTime; 
		root1.append(CMDContent);
	}
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["operateType"]   = pUserAuthorizeInfo->iDoType;
	person["CMDContent"] = (Json::Value)root1;
	root.append(person);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

BOOL CWLJsonParse::Commond_UserInfo_ParseJson(__in std::string& sJson, __out ST_USERAUTHORIZE_INFO* pUserAuthorizeInfo)
{
	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;
	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		return FALSE;
	}

	if (root[0].isMember("operateType"))
	{
		pUserAuthorizeInfo->iDoType = (int)root[0]["operateType"].asInt();
	}

	if (root[0].isMember("CMDContent"))
	{
		root_Content =  (Json::Value)root[0]["CMDContent"];

		if (root_Content.isArray())
		{
			int nObject = root_Content.size();
			for (int i=0; i < nObject; i++)
			{
				UserAuthorize_Info st_info;
				StrCpyW(st_info.szUserAuthorizeUserName,UTF8ToUnicode(root_Content[i]["UserName"].asString()).c_str());
				StrCpyW(st_info.szUserAuthorizePassWord,UTF8ToUnicode(root_Content[i]["Password"].asString()).c_str());
				st_info.RoleID = (int)root_Content[i]["RoleID"].asInt();
				st_info.dwAuthorizeValue = (DWORD)root_Content[i]["authority"].asInt();
			
				if (root_Content[i].isMember("IsDefault") && !root_Content[i]["IsDefault"].isNull())
				{
				st_info.dwIsDefault = (DWORD)root_Content[i]["IsDefault"].asInt();
				}

				if (root_Content[i].isMember("IsFirstLogin") && !root_Content[i]["IsFirstLogin"].isNull())
				{
					st_info.dwIsFirstLogin = (DWORD)root_Content[i]["IsFirstLogin"].asInt();
				}

				if (root_Content[i].isMember("PwdUpdateTime") && !root_Content[i]["PwdUpdateTime"].isNull())
				{
					st_info.llPwdUpdateTime = (LONGLONG)root_Content[i]["PwdUpdateTime"].asInt64();
				}
			
				pUserAuthorizeInfo->vec_Info.push_back(st_info);
			}
		}

		
	}
	return TRUE;
}

std::string CWLJsonParse::UpLoad_UserErrorInfo_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in ST_USERERROR_INFO* pUserErrorInfo)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;
	int nCount = (int)pUserErrorInfo->vec_ErrorInfo.size();

	for (int i=0; i< nCount; i++)
	{
		std::wstring wsTemp = convertTimeTToStr(::time(NULL));
		CMDContent["Time"] = UnicodeToUTF8(wsTemp);
		CMDContent["UserName"]	= UnicodeToUTF8(pUserErrorInfo->vec_ErrorInfo[i].szUserAuthorizeUserName);
		CMDContent["reason"]	= pUserErrorInfo->vec_ErrorInfo[i].iReason;
		CMDContent["operateType"]	= pUserErrorInfo->vec_ErrorInfo[i].iOperateType;
		root1.append(CMDContent);
	}
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["CMDContent"] = (Json::Value)root1;
	root.append(person);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}
/* 返回给USM的处理结果 2019-10-30
window系统中，发现USM配置的强访主体中有未知的用户，则返回给USM一json信息，告诉它有哪些未知用户。

*/
std::string CWLJsonParse::MACUnknownUserGetJson( __in tstring ComputerID,__in vStrUserName& vecUnknownUserName)
{
    Json::Value CMDContent;
    std::string sJsonPacket = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value head;

    std::string strReason = "UnknownUserName:";
    for(unsigned int i = 0; i<vecUnknownUserName.size(); i++)
    {
        std::wstring wsTemp = _T("");
        wsTemp = vecUnknownUserName[i].c_str();
        strReason += UnicodeToUTF8(wsTemp);
        if(i != vecUnknownUserName.size()-1)
        {
            strReason += ",";
        }
    }
        CMDContent["RESULT"] = "FAIL";
        CMDContent["REASON"] = strReason;

    head["ComputerID"]  = UnicodeToUTF8(ComputerID);
    head["CMDTYPE"]     = PLY_CLIENT_UPDATE_PASSWORD;
    head["CMDID"]       = PLY_CLIENT_SYSTEM_AC_MAC_SUBANDOBJ;

    head["CMDContent"]    = CMDContent;

    root.append(head);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}




BOOL CWLJsonParse::GetLogServerByJson(__in std::string sJson, __out tstring &strIP, __out WORD &nPort, __out WORD &nType, tstring *pStrErr)
{
	/*
	//json格式
	//独立日志服务器或者HTTPS服务器

	{
		"ComputerID":"FEFOEACD",
		"CMDTYPE": 150,
		"CMDID": 168,
		"CMDContent":
		{
			"logServerIp": "192.168.15.6",
			"logServerPort": 12345
			"logServerType":0,1,2   0:http(usm)  1:udp  2:tcp
		}
	}

	获取日志服务器IP和端口，日志服务器类型，




	*/
	//logServerIp,logServerPort;
	BOOL bRes = FALSE;
	wostringstream  strTemp;
	Json::Reader	reader;
	Json::Value root;
	int nObject = 0;
	string KeyName;

	strIP = _T("");
	if( sJson.length() == 0)
	{
		strTemp << _T("CWLJsonParse::GetLogServerByJson, sJson.length() == 0")<<_T(",");
		goto END;
	}
	
	if (!reader.parse(sJson, root))
	{
		strTemp << _T("CWLJsonParse::GetLogServerByJson, parse fail")<<_T(",");
		goto END;
	}

	if (!root.isObject())
	{
		strTemp << _T("CWLJsonParse::GetLogServerByJson, invalid object")<<_T(",");
		goto END;
	}

	if (!root.isMember("CMDContent"))
	{
		strTemp << _T("CWLJsonParse::GetLogServerByJson, CMDContent isnot member")<<_T(",");
		goto END;
	}

	root = (Json::Value)root["CMDContent"];

	//type
	//KeyName = "logServerType";
	//if (root.isMember(KeyName.c_str()) || root[KeyName.c_str()].isUInt())
	//{
	//	nType = root[KeyName.c_str()].asUInt();
	//}
	//else
	//{
	//	strTemp << _T("CWLJsonParse::GetLogServerByJson,  logServerType is not member of CMDContent")<<_T(",");
	//	goto END;
	//}

	//ip
	KeyName = "logServerIp";
	if (root.isMember(KeyName.c_str()) && root[KeyName.c_str()].isString())
	{
		strIP = CStrUtil::ConvertA2W(root[KeyName.c_str()].asString());
	}
	else
	{
		strIP = _T("");
	}

	//port
	KeyName = "logServerPort";
	if (root.isMember(KeyName.c_str()) && root[KeyName.c_str()].isUInt())
	{
		nPort = root[KeyName.c_str()].asUInt();
	}
	else
	{
		nPort = 0;
	}

	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strTemp.str();
	}

	if (!bRes)
	{
		strIP = _T("");
	}
	return bRes;
}


std::string CWLJsonParse::SysLog_GetJson(__in tstring ComputerID,  __in  char *pData, __in int nDataLen)
{
	/*
	系统日志数据先BASE64，然后写入JSON

	json格式：
[
	{
	"ComputerID":"FEFOEACD",
	"CMDContent":
		[
			{"SysLog": "base64 code"},
			{"SysLog": "base64 code"}
		]
	}
]
	*/
	std::string sJsonPacket = "";
	std::string sJsonBody = "";
	Json::Value root;
	Json::FastWriter writer;
	Json::Value Item;

	std::string strBase64;

	//参数检查
	if (!pData || nDataLen <= 0)
	{
		goto END;
	}

	for (int i = 0; i < 1; i++)//目前单条传输，如果将来多条传输，只需修改循环次数
	{
		//base64 二进制数据，方便JSON传输
		if (!CWLJsonParse::Base64_Encode(pData, nDataLen, strBase64))
		{
			goto END;
		}

		Item["SysLog"] = strBase64;
		root.append(Item);
		Item.clear();
	}

	sJsonBody = writer.write(root);
	root.clear();

	Item["ComputerID"]= UnicodeToUTF8(ComputerID);
	//Item["CMDTYPE"] = (int)cmdType;
	//Item["CMDID"] = (int)cmdID;
	Item["CMDContent"] = sJsonBody;

	root.append(Item);
	sJsonPacket = writer.write(root);
	root.clear();

END:
	return sJsonPacket;
}


BOOL CWLJsonParse::GetUploadPeByJson(__in std::string sJson, __out int &nMaxSize, tstring *pStrErr)
{
	/*
	//获取上传PE文件的配置

	//json格式
开启上传

	{
		"ComputerID":"FEFOEACD",
		"CMDTYPE": 150,
		"CMDID": 9,
		"CMDContent":
		{
			"uploadFileMax": 12345
		}
	}


关闭上传

	{
		"ComputerID":"FEFOEACD",
		"CMDTYPE": 150,
		"CMDID": 10,
		"CMDContent":
		{
			"uploadFileMax": 12345
		}
	}

	*/

	BOOL bRes = FALSE;
	wostringstream  strTemp;
	Json::Reader	reader;
	Json::Value root;
	int nObject = 0;

	if( sJson.length() == 0)
	{
		strTemp << _T("CWLJsonParse::GetUploadPeByJson, sJson.length() == 0")<<_T(",");
		goto END;
	}

	if (!reader.parse(sJson, root))
	{
		strTemp << _T("CWLJsonParse::GetUploadPeByJson, parse fail")<<_T(",");
		goto END;
	}

	//if (!root.isArray() || root.size() < 1)
	if (!root.isObject())
	{
		strTemp << _T("CWLJsonParse::GetUploadPeByJson, invalid object")<<_T(",");
		goto END;
	}

	if (!root.isMember("CMDContent"))
	{
		strTemp << _T("CWLJsonParse::GetUploadPeByJson, CMDContent is not member")<<_T(",");
		goto END;
	}

	root = (Json::Value)root["CMDContent"];

	//uploadFile_max
	if (root.isMember("uploadFileMax"))
	{
		if(!root["uploadFileMax"].isInt())
		{
			strTemp << _T("CWLJsonParse::GetLogServerByJson, uploadFileMax invalid")<<_T(",");
			goto END;
		}
		nMaxSize = root["uploadFileMax"].asInt();
	}
	else
	{
		nMaxSize = UPLOAD_PE_MAX;
	}

	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strTemp.str();
	}

	return bRes;
}

string CWLJsonParse::Update_GetJson(tstring ComputerID, tstring *pStrErr)
{
	/*
		升级主机卫士升级json
	 [
		{"ComputerID":"6CBE2F9EWIN-IP26O1UDS48"}
	 ]
	*/

	std::string sJsonPacket = "";
	std::string sJsonBody = "";
	Json::Value root;
	Json::FastWriter writer;
	Json::Value Item;
	wostringstream  strFormat;

	std::string strBase64;

	//参数检查
	if (ComputerID.length() == 0)
	{
		strFormat << _T("Update_GetJson: invalid param")<<_T(", ");
		goto END;
	}

	Item["ComputerID"]= UnicodeToUTF8(ComputerID);

	root.append(Item);
	sJsonPacket = writer.write(root);
	root.clear();

END:

	if (pStrErr && sJsonPacket.length() > 0)
	{
		*pStrErr = strFormat.str();
	}
	return sJsonPacket;
}

BOOL CWLJsonParse::GetConfigInfoByJson(__in const std::string &sJson, __out CONFIG_INFO &stcUpdateInfo, tstring *pStrErr)
{
	/*
	由服务器返回的json获取信息
[
	“ComputerID”:”FEFOEACD”,
	“CMDTYPE”:xxx,
	“CMDID”:xxx,
	“CMDContent”:{

	uploadFile：9/10,  9开启， 10关闭
	uploadFile_max: 10,

	devid:       123
	其他
	}
]
	*/

	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value root;
	int nObject = 0;
	wostringstream  strFormat;
	const int bOn = 9;
	const int bOff = 10;
	string strKeyName;

	if (sJson.length() == 0)
	{
		strFormat << _T("GetConfigInfoByJson: invalid param")<<_T(", ");
		goto END;
	}

	if (!reader.parse(sJson, root))
	{
		strFormat << _T("CWLJsonParse::GetConfigInfoByJson, parse fail")<<_T(",");
		goto END;
	}

	if (!root.isArray() || root.size() < 1)
	{
		strFormat << _T("CWLJsonParse::GetConfigInfoByJson, invalid array")<<_T(",");
		goto END;
	}

	if (!root[0].isMember("CMDContent"))
	{
		strFormat << _T("CWLJsonParse::GetConfigInfoByJson, CMDContent is not member")<<_T(",");
		goto END;
	}

	root = (Json::Value)root[0]["CMDContent"];

	//uploadFile_max
	strKeyName = "uploadFileMax";
	if (root.isMember(strKeyName.c_str()))
	{
		if(!root[strKeyName.c_str()].isInt())
		{
			strFormat << _T("CWLJsonParse::GetConfigInfoByJson, uploadFile_max invalid")<<_T(",");
			goto END;
		}
		stcUpdateInfo.dwUploadMaxSize = root[strKeyName.c_str()].asInt();
	}
	else
	{
		stcUpdateInfo.dwUploadMaxSize = UPLOAD_PE_MAX;
	}

	//uploadfile
	strKeyName = "uploadFile";
	if (!root.isMember(strKeyName.c_str()) || !root[strKeyName.c_str()].isInt())
	{
		strFormat << _T("CWLJsonParse::GetConfigInfoByJson, uploadfile invalid")<<_T(",");
		goto END;
	}
	if (bOn == root[strKeyName.c_str()].asInt())
	{
		stcUpdateInfo.bUpdateState = TRUE;
	}
	else
	{
		stcUpdateInfo.bUpdateState = FALSE;
	}
	/*
	03/29/2021  linlin.li
	添加获取TCP心跳端口
	04/12/2021  linlin.li
	为兼顾老版本USM，没有tcpPort字段不做处理
	*/
	strKeyName = "tcpPort";
	if (root.isMember(strKeyName.c_str()) && root[strKeyName.c_str()].isInt())
	{
		stcUpdateInfo.nTcpPort = root[strKeyName.c_str()].asInt();
	}
	else 
	{
		stcUpdateInfo.nTcpPort = 0;
	}

	////logServerType
	//strKeyName = "logServerType";
	//if (!root.isMember(strKeyName.c_str())  || !root[strKeyName.c_str()].isUInt())
	//{
	//	strFormat << _T("CWLJsonParse::GetLogServerByJson,  logServerType invalid type")<<_T(",");
	//	goto END;
	//}

	//stcUpdateInfo.nLogServerType = root[strKeyName.c_str()].asUInt();

	////logServerIp
	//strKeyName = "logServerIp";
	//if (root.isMember(strKeyName.c_str()) && root[strKeyName.c_str()].isString())
	//{
	//	stcUpdateInfo.strLogServerIP = CStrUtil::ConvertA2W(root[strKeyName.c_str()].asString());
	//}
	//else
	//{
	//	stcUpdateInfo.strLogServerIP = _T("");
	//}


	////logServerPort
	//strKeyName = "logServerPort";
	//if (root.isMember(strKeyName.c_str()) && root[strKeyName.c_str()].isUInt())
	//{
	//	stcUpdateInfo.nPort = root[strKeyName.c_str()].asUInt();
	//}
	//else
	//{
	//	stcUpdateInfo.nPort = 0;
	//}

	//devid (唯一ID)
	strKeyName = "devid";
	if (root.isMember(strKeyName.c_str()) && root[strKeyName.c_str()].isInt())
	{
		stcUpdateInfo.nUniqueID = root[strKeyName.c_str()].asUInt();
	}
	else
	{
		stcUpdateInfo.nUniqueID = 0;
	}

	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}
	return bRes;

}

BOOL CWLJsonParse::GetUpgradeInfoByJson(__in std::string sJson, __out tstring& wsPackageName, __out tstring& wsPackageMD5, __out tstring& wsServerIP, __out int& iPort, __out int& iSize,__out string& sUrl, tstring *pStrErr)
{
    /*数组Json类型
    旧版的：
    [{
        "CMDTYPE": 150,
        "CMDID": 255,
        "CMDVER": 1,
        "ComputerID": "XXX-Pc",
            "Server": "192.168.1.160",
            "Port": "69"
        }
    }]

    新版：
    {
        "CMDContent":
        {
           "PackageMd5":"cc7a7d0c86ab2937b701f5e987d03c56",
           "PackageName":"XXX.exe",
           "Port":69,
           "Server":"192.168.4.207"
           "Size":158765*1024,
		   "URL":"https://192.168.4.160:8440/USM/notLoginDownLoad/downloadByFileName.do?fileName=IEG_V300R003C11B190_ubuntu16-x64.bin"
        },
        "CMDID":255,
        "CMDTYPE":150,
        "ComputerID":"6CBE2F9EWIN-IP26O1UDS48",
        "DOMAIN":null,
        "Username":null
   }

   */

    BOOL bRes = FALSE;

    wostringstream  strTemp;
    Json::Reader	reader;
    Json::Value root;
    int nObject = 0;

    if( sJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, sJson.length() == 0")<<_T(",");
        goto END;
    }

    if (!reader.parse(sJson, root))
    {
        strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, parse fail")<<_T(",");
        goto END;
    }

    //if (!root.isArray())
    if (!root.isObject())
    {
        strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, invalid object")<<_T(",");
        goto END;
    }

    //root = (Json::Value)root[0];

    if (!root.isMember("CMDContent"))
    {
        strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, CMDContent is not member")<<_T(",");
        goto END;
    }


    root = (Json::Value)root["CMDContent"];
    //PackageName
    if (root.isMember("PackageName"))
    {
        if(!root["PackageName"].isString())
        {
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, PackageName invalid")<<_T(",");
            goto END;
        }
        std::string s1 = root["PackageName"].asString();
        if( s1.length() > 0)
        {
            wsPackageName = UTF8ToUnicode(s1);
        }
        else
        {
            wsPackageName = _T("");
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, get <PackageName> info error ")<<_T(",");
            goto END;
        }
    }
    else
    {
        wsPackageName = _T("");
    }

    //PackageMd5
    if (root.isMember("PackageMd5"))
    {
        if(!root["PackageMd5"].isString())
        {
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, PackageMd5 invalid")<<_T(",");
            goto END;
        }
        std::string s2 = root["PackageMd5"].asString();
        if( s2.length() > 0)
        {
            wsPackageMD5 = UTF8ToUnicode(s2);
        }
        else
        {
            wsPackageMD5 = _T("");
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, get <PackageMd5> info error ")<<_T(",");
            goto END;
        }
    }
    else
    {
        wsPackageMD5 = _T("");
    }

    //Server(IP)
    if (root.isMember("Server"))
    {
        if(!root["Server"].isString())
        {
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, Server invalid")<<_T(",");
            goto END;
        }
        std::string s3 = root["Server"].asString();
        if( s3.length() > 0)
        {
            wsServerIP = UTF8ToUnicode(s3);
        }
        else
        {
            wsServerIP = _T("");
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, get <Server> info error ")<<_T(",");
            goto END;
        }
    }
    else
    {
        wsServerIP = _T("");
    }


    //Port
    if (root.isMember("Port"))
    {
        if(!root["Port"].isInt())
        {
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, Port invalid")<<_T(",");
            goto END;
        }

        iPort = root["Port"].asInt();
    }
    else
    {
        iPort = 0;
    }

    //Size
    if (root.isMember("Size"))
    {
        if(!root["Size"].isInt())
        {
            strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, Size invalid")<<_T(",");
            goto END;
        }

        iSize = root["Size"].asInt();
    }
    else
    {
        iSize = 0;
    }

	//URL  新追加参数 2022-09-01
	if (root.isMember("URL"))
	{
		if(!root["URL"].isString())
		{
			strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, <URL> invalid")<<_T(",");
			//bRes = TRUE;
			goto END;
		}

		std::string s4 = root["URL"].asString();
		if( s4.length() > 0)
		{
			sUrl = s4;
		}
		else
		{
			sUrl = "";
			strTemp << _T("CWLJsonParse::GetUpgradeInfoByJson, get <URL> info error ")<<_T(",");
			//bRes = TRUE;
			goto END;
		}
	}

    bRes = TRUE;

END:
    if (pStrErr)
    {
        *pStrErr = strTemp.str();
    }

    return bRes;
}

std::string CWLJsonParse::SilentUpgrateResult_GetJson(__in tstring ComputerID, __in int iReason, __in int iStage, __in DWORD cmdType , __in DWORD cmdID)
{
    std::string sJsonPacket = "";
    Json::Value root1;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    int cmdVer = 1;

    //iReason
    int iRet = iReason;
    if ( iRet == WL_SILENTUPGRADE_SUCCESS || iRet == WL_SILENTUPGRADE_EXEC_SUCCESS)  //1-升级后下载完成  0-升级或下载目录接收完成
    {
        //成功
        Json::Value CMDContent;
        CMDContent["RESULT"]			= TYPE_UPGRADE_STATUS_RESULT_SUC;
        CMDContent["REASON"]			= iRet;
        CMDContent["STAGE"]             = iStage; // enum 其他问题 0 客户端 1  病毒库 2
        person["CMDContent"] = (Json::Value)CMDContent;
    }
    else
    {
        //失败
        Json::Value CMDContent;
        CMDContent["RESULT"]			= TYPE_UPGRADE_STATUS_RESULT_FAIL;
        CMDContent["REASON"]			= iRet;
        CMDContent["STAGE"]             = iStage; // enum 其他问题 0 客户端 1  病毒库 2
        person["CMDContent"] = (Json::Value)CMDContent;
    }


    person["ComputerID"]	= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"]		= (int)cmdType;
    person["CMDID"]			= (int)cmdID;
    person["CMDVER"]        = cmdVer;
    root.append(person);
    const string& strlog = root.toStyledString();
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::DownloadPackageSize_GetJson(__in tstring ComputerID, __in tstring& wsPackageName, __in int iPercent, __in WORD cmdType , __in WORD cmdID)
{
    std::string sJsonPacket = "";
    Json::Value root1;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    int cmdVer = 1;

    //Content:
    Json::Value CMDContent;
    CMDContent["FileName"]	        = UnicodeToUTF8(wsPackageName);
    CMDContent["downloadPer"]		= iPercent;

    person["ComputerID"]	= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"]		= (int)cmdType;
    person["CMDID"]			= (int)cmdID;
    person["CMDVER"]        = cmdVer;
    person["CMDContent"]    = (Json::Value)CMDContent;
    root = person;
    const string& strlog = root.toStyledString();
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

#if 0
//BOOL CWLJsonParse::GetUpdateInfoByJson(const std::string &sJson, CONFIG_INFO &stcUpdateInfo, tstring *pStrErr)
//{
//	/*
//	由服务器返回的升级json获取信息
//[
//	“ComputerID”:”FEFOEACD”,
//	“CMDTYPE”:1,
//	“CMDID”:1,
//	“CMDContent”:{
//
//	uploadFile：9/10,  9开启， 10关闭
//	uploadFile_max: 10,
//	logServerIp: "192.168.3.3",
//	logServerPort: 8441,
//	devid:       123
//
//	}
//]
//	*/
//
//	BOOL bRes = FALSE;
//	Json::Reader reader;
//	Json::Value root;
//	int nObject = 0;
//	wostringstream  strFormat;
//	const int bOn = 9;
//	const int bOff = 10;
//
//	if (sJson.length() == 0)
//	{
//		strFormat << _T("GetUpdateInfoByJson: invalid param")<<_T(", ");
//		goto END;
//	}
//
//	if (!reader.parse(sJson, root))
//	{
//		strFormat << _T("CWLJsonParse::GetUpdateInfoByJson, parse fail")<<_T(",");
//		goto END;
//	}
//
//	if (!root.isArray() || root.size() < 1)
//	{
//		strFormat << _T("CWLJsonParse::GetUpdateInfoByJson, invalid array")<<_T(",");
//		goto END;
//	}
//
//	if (!root[0].isMember("CMDContent"))
//	{
//		strFormat << _T("CWLJsonParse::GetUpdateInfoByJson, CMDContent is not member")<<_T(",");
//		goto END;
//	}
//
//	root = (Json::Value)root[0]["CMDContent"];
//
//	//uploadFile_max
//	if (root.isMember("uploadFile_max"))
//	{
//		if(!root["uploadFile_max"].isInt())
//		{
//			strFormat << _T("CWLJsonParse::GetLogServerByJson, uploadFile_max invalid")<<_T(",");
//			goto END;
//		}
//		stcUpdateInfo.dwUploadMaxSize = root["uploadFile_max"].asInt();
//	}
//	else
//	{
//		stcUpdateInfo.dwUploadMaxSize = UPLOAD_PE_MAX;
//	}
//
//	//uploadfile
//	if (!root.isMember("uploadfile") || !root["uploadfile"].isInt())
//	{
//		strFormat << _T("CWLJsonParse::GetLogServerByJson, uploadfile invalid")<<_T(",");
//		goto END;
//	}
//	if (bOn == root["uploadfile"].asInt())
//	{
//		stcUpdateInfo.bUpdateState = TRUE;
//	}
//	else
//	{
//		stcUpdateInfo.bUpdateState = FALSE;
//	}
//	//stcUpdateInfo.dwUpdateState = root["uploadfile"].asInt();
//
//	//logServerIp
//	if (!root.isMember("logServerIp") || !root["logServerIp"].isString())
//	{
//		strFormat << _T("CWLJsonParse::GetLogServerByJson, uploadfile invalid")<<_T(",");
//		goto END;
//	}
//	stcUpdateInfo.strLogServerIP = CStrUtil::ConvertA2W(root["logServerIp"].asString());
//
//	//logServerPort
//	if (!root.isMember("logServerPort") || !root["logServerPort"].isInt())
//	{
//		strFormat << _T("CWLJsonParse::GetLogServerByJson, logServerPort invalid")<<_T(",");
//		goto END;
//	}
//	stcUpdateInfo.nPort = root["logServerPort"].asUInt();
//
//	//devid (唯一ID)
//	if (!root.isMember("devid") || !root["devid"].isInt())
//	{
//		strFormat << _T("CWLJsonParse::GetLogServerByJson, devid invalid")<<_T(",");
//		goto END;
//	}
//	stcUpdateInfo.nUniqueID = root["devid"].asUInt();
//
//
//	bRes = TRUE;
//END:
//	if (pStrErr)
//	{
//		*pStrErr = strFormat.str();
//	}
//	return bRes;
//
//}
#endif

// 客户端上传信任进程给服务端
std::string  CWLJsonParse::Def2USM_Trust_Process_GetJson(__in const tstring &ComputerID,
														 __in const WORD &cmdType ,
														 __in const WORD &cmdID,
														 __in const VEC_TRUST_PROCESSES& vecTrustProcesses,
														 __in const TRUST_PROCESS_PATH_OPERATION &oper)
{
	/*
	json格式：

	{
		"CMDID":247,
		"CMDTYPE":150,
		"CMDVER":1,
		"Operation":"replace",
		"ComputerID":"xxxxxxxxxx",

		"CMDContent":
			[
				{
					"childProcess":1,
					"FileName":"c:\abc\a.exe",
					"id":2
				},
				{
					"childProcess":0,
					"FileName":"c:\abc\b.exe",
					"id":3
				}
			]

	}


	*/
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value trustProcess;


	//trust process
	int nCount = (int)vecTrustProcesses.size();
	for (int i=0; i< nCount; i++)
	{
		Json::Value CMDContent;
		CMDContent["FileName"] = UnicodeToUTF8(vecTrustProcesses[i].strPath);
		CMDContent["TrustChild"] = Json::Value(vecTrustProcesses[i].bTrushChild);
		CMDContent["id"] = i+1;
		root1.append(CMDContent);
	}

	switch (oper)
	{
	case OPER_ADD:
		trustProcess["Operation"]	= "add";
		break;
	case OPER_DELETE:
		trustProcess["Operation"]	= "delete";
		break;
	case OPER_REPLACE:
		trustProcess["Operation"]	= "replace";
		break;
	default:
		trustProcess["Operation"]	= "unknow";
		break;
	}

	trustProcess["ComputerID"]	= UnicodeToUTF8(ComputerID);
	trustProcess["CMDTYPE"]		= (int)cmdType;
	trustProcess["CMDID"]		= (int)cmdID;

	trustProcess["CMDVER"]		= 1;
	trustProcess["CMDContent"]  = (Json::Value)root1;
	root.append(trustProcess);
	const string& strlog = root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}


BOOL CWLJsonParse::USM2Def_Trust_Process_ParseJson(__in const std::string& sJson,
													__out VEC_TRUST_PROCESSES& vecTrustProcesses,
													__out TRUST_PROCESS_PATH_OPERATION& oper,
													__out tstring *pStrErr)
{

/* //Json格式
//-----------------------------------------
// 信任进程
{
    "CMDID":247,
    "CMDTYPE":150,
    "CMDVER":1,
    "Operation":"replace",
    "ComputerID":"xxxxxxxxxx",

    "CMDContent":
        [
            {
                "TrustChild":1,
                "FileName":"c:\abc\a.exe",
                
            },
            {
                "TrustChild":0,
                "FileName":"c:\abc\b.exe",
             
            }
        ]

}
//-----------------------------------------

*/
	BOOL bRet = FALSE;
	wostringstream  strFormat;
	std::string strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		return FALSE;
	}

	if (!(root[0].isMember("CMDID") &&
		root[0].isMember("CMDTYPE") &&
		root[0].isMember("Operation") &&
		root[0].isMember("CMDContent")) )
	{
		strFormat << _T("Invaild JSON data member, CMD from USM:") << UTF8ToUnicode(sJson) << _T(",");
		goto END;
	}


	int CMDID = root[0]["CMDID"].asInt();
	if (PLY_CONFIG_TRUST_PROCESS != CMDID)
	{
		strFormat << _T("USM2Def_Trust_Process_ParseJson, ERROR: Wrong CMDID, get CMDID = ") << CMDID << _T(",");
		goto END;
	}

	// 判断是否为支持的操作
	const char* usm_operation = root[0]["Operation"].asCString();
	if (0 == strcmp(usm_operation, "replace"))
	{
		oper = OPER_REPLACE;
	}
	else if( 0 == strcmp(usm_operation, "add") )
	{
		oper = OPER_ADD;
	}
	else if( 0 == strcmp(usm_operation, "delete") )
	{
		oper = OPER_DELETE;
	}
	else
	{
		oper = OPER_INVAILD;
		strFormat<< _T("Unsupported operation: ") << UTF8ToUnicode(usm_operation) << _T(",");
		goto END;

	}

	root_Content = (Json::Value)root[0]["CMDContent"];
	if(root_Content.isArray())
	{
		int nObject = root_Content.size();
		for (int i = 0; i < nObject; i++)
		{
			STC_TRUST_PROCESS st_TrustProcess;
			st_TrustProcess.strPath = UTF8ToUnicode(root_Content[i]["FileName"].asString());
			st_TrustProcess.bTrushChild = root_Content[i]["TrustChild"].asInt();

			vecTrustProcesses.push_back(st_TrustProcess);
		}
	}
	
	bRet = TRUE;

END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}
	return bRet;
}


std::string  CWLJsonParse::Def2USM_Trust_Path_GetJson(__in const tstring &ComputerID,
													  __in const WORD &cmdType ,
													  __in const WORD &cmdID,
													  __in const VEC_TRUST_PATH &vecTrustPath,
													  __in const TRUST_PROCESS_PATH_OPERATION &oper)
{
	/*
	json格式：
	{
		"CMDID":248,
		"CMDTYPE":150,
		"CMDVER":1,
		"Operation":"replace",
		"ComputerID":"xxxxxxxxxx",

		"CMDContent":
			[
				{
					"FileName":"c:\abc\",
					"id":1
				}
			]
	}

	*/

	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value trustPath;

	//trust path
	int nCount = (int)vecTrustPath.size();
	for (int i=0; i< nCount; i++)
	{
		Json::Value CMDContent;
		CMDContent["Path"]			= UnicodeToUTF8(vecTrustPath[i].strPath);
		root1.append(CMDContent);
	}

	switch (oper)
	{
	case OPER_ADD:
		trustPath["Operation"]	= "add";
		break;
	case OPER_DELETE:
		trustPath["Operation"]	= "delete";
		break;
	case OPER_REPLACE:
		trustPath["Operation"]	= "replace";
		break;
	default:
		trustPath["Operation"]	= "unknown";
		break;
	}

	trustPath["ComputerID"]	= UnicodeToUTF8(ComputerID);
	trustPath["CMDTYPE"]	= (int)cmdType;
	trustPath["CMDID"]		= (int)cmdID;
	trustPath["CMDContent"] = (Json::Value)root1;
	trustPath["CMDVER"]		= 1;
	root.append(trustPath);
	const string& strlog = root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::USM2Def_Trust_Path_ParseJson(__in const std::string& sJson,
												__out VEC_TRUST_PATH& vecTrustPath,
												__out TRUST_PROCESS_PATH_OPERATION& oper,
												__out tstring *pStrErr)
{
/**
// 信任路径
{
    "CMDID":248,
    "CMDTYPE":150,
    "CMDVER":1,
    "Operation":"replace",
    "ComputerID":"xxxxxxxxxx",

    "CMDContent":
    	[
            {"Path":"c:\abc\"},
            {"Path":"c:\abc\"}
        ]
}
//-----------------------------------------
**/
	BOOL bRet = FALSE;
	wostringstream strFormat;
	std::string strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		return FALSE;
	}

	if (!(root[0].isMember("CMDID") &&
		root[0].isMember("CMDTYPE") &&
		root[0].isMember("Operation") &&
		root[0].isMember("CMDContent")) )
	{
		strFormat << _T("Invaild JSON data member, CMD from USM:") << UTF8ToUnicode(sJson) << _T(",");
		goto END;
	}

	int CMDID = root[0]["CMDID"].asInt();
	if (PLY_CONFIG_TRUST_PATH != CMDID)
	{
		strFormat<< _T("Wrong CMDID, get CMDID = ") << CMDID << _T(",");
		goto END;
	}

	// 判断是否为支持的操作
	const char* usm_operation = root[0]["Operation"].asCString();
	if (0 == strcmp(usm_operation, "replace"))
	{
		oper = OPER_REPLACE;
	}
	else if( 0 == strcmp(usm_operation, "add") )
	{
		oper = OPER_ADD;
	}
	else if( 0 == strcmp(usm_operation, "delete") )
	{
		oper = OPER_DELETE;
	}
	else
	{
		oper = OPER_INVAILD;
		strFormat<< _T("Unsupported operation: ") << UTF8ToUnicode(usm_operation) << _T(",");
		goto END;
	}

	root_Content = (Json::Value)root[0]["CMDContent"];
	if(root_Content.isArray())
	{
		int nObject = root_Content.size();
		for (int i = 0; i < nObject; i++)
		{

			STC_TRUST_PATH stc_TrustPath;
			stc_TrustPath.strPath = UTF8ToUnicode(root_Content[i]["Path"].asString());
			vecTrustPath.push_back(stc_TrustPath);
		}
	}
	
	bRet = TRUE;


END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}
	return bRet;
}

BOOL CWLJsonParse::Trust_Process_ParseJson(__in const std::string& sJson,
										   __out VEC_TRUST_PROCESSES& vecTrustProcesses,
										   __out TRUST_PROCESS_PATH_OPERATION& oper,
										   __out tstring *pStrErr)
{
	/*
		json格式：

		//信任进程
		{
			"Operation":"delete",
			"CMDContent":
			[
				{"Path": "xxxx", "TrustChild": true},
				{ "Path": "xxxx1111", "TrustChild": false},
			]
		}
	*/
	BOOL bRes = FALSE;
	wostringstream  strFormat;
	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{

		strFormat<< _T("invalid json")<<_T(", ");
		goto END;
	}


	if (!root[0].isMember("CMDContent"))
	{
		strFormat<< _T("no CMDContent")<<_T(", ");
		goto END;
	}

	root_Content = (Json::Value)root[0]["CMDContent"];
	int nObject = root_Content.size();
	for (int i = 0; i < nObject; i++)
	{

		STC_TRUST_PROCESS st_TrustProcess;
		st_TrustProcess.strPath = UTF8ToUnicode(root_Content[i]["Path"].asString());
		st_TrustProcess.bTrushChild = root_Content[i]["TrustChild"].asBool();

		vecTrustProcesses.push_back(st_TrustProcess);
	}

	bRes = TRUE;

	// 操作类型（添加，全量替换，删除）判断
	oper = OPER_INVAILD;
	if (root[0].isMember("Operation"))
	{
		std::string operation= root[0]["Operation"].asString();
		if ("add" == operation) oper = OPER_ADD;
		else if("delete" == operation) oper = OPER_DELETE;
		else if("replace" == operation) oper = OPER_REPLACE;
	}
	else
	{
		bRes = FALSE;
	}

END:
	if (!bRes)
	{
		if (pStrErr)
		{
			*pStrErr = strFormat.str();
		}
	}
	return bRes;
}


BOOL CWLJsonParse::Trust_Path_ParseJson(__in const std::string& sJson,
										__out VEC_TRUST_PATH& vecTrustPath,
										__out TRUST_PROCESS_PATH_OPERATION& oper,
										__out tstring *pStrErr)
{
	/*
	json格式：

	//信任路径
	{
		"Oeration": "add",
		"CMDContent":
		[
			{ "Path": "xxxx"},
			{ "Path": "xxxx1111"},
		]
	}

*/
	BOOL bRes = FALSE;
	wostringstream  strFormat;

	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		strFormat<< _T("invalid json")<<_T(", ");
		goto END;
	}

	if (!root[0].isMember("CMDContent"))
	{
		strFormat<< _T("no CMDContent")<<_T(", ");
		goto END;
	}

	root_Content = (Json::Value)root[0]["CMDContent"];
	int nObject = root_Content.size();
	for (int i = 0; i < nObject; i++)
	{
		STC_TRUST_PATH stcTrustPath;
		stcTrustPath.strPath = UTF8ToUnicode(root_Content[i]["Path"].asString());

		vecTrustPath.push_back(stcTrustPath);
	}

	bRes = TRUE;

	// 操作类型（添加，全量替换，删除）判断
	oper = OPER_INVAILD;
	if (root[0].isMember("Operation"))
	{
		std::string operation= root[0]["Operation"].asString();
		if ("add" == operation) oper = OPER_ADD;
		else if("delete" == operation) oper = OPER_DELETE;
		else if("replace" == operation) oper = OPER_REPLACE;
	}
	else
	{
		bRes = FALSE;
	}

END:
	if (!bRes)
	{
		if (pStrErr)
		{
			*pStrErr = strFormat.str();
		}
	}
	return bRes;
}


BOOL CWLJsonParse::Trust_Process_GetJson(__in const VEC_TRUST_PROCESSES& vecTrustProcesses,
										 __in TRUST_PROCESS_PATH_OPERATION oper,
										 __out string &strJson,
										 __out tstring* pStrErr)
{
	/*
		json格式：

		//信任进程
		{
			"Operation":"delete",
			"CMDContent":
			[
				{"Path": "xxxx", "TrustChild": true},
				{ "Path": "xxxx1111", "TrustChild": false},
			]
		}
	*/
	BOOL bRes = FALSE;
	wostringstream  strFormat;

	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)vecTrustProcesses.size();
	for (int i=0; i< nCount; i++)
	{
		CMDContent["Path"]			= UnicodeToUTF8(vecTrustProcesses[i].strPath);
		CMDContent["TrustChild"]	= Json::Value(vecTrustProcesses[i].bTrushChild);
		root1.append(CMDContent);
	}
	person["CMDContent"]    = (Json::Value)root1;

	// 操作类型判断
	std::string operation;
	switch (oper)
	{
	case OPER_ADD:
		operation="add";
		break;
	case OPER_DELETE:
		operation="delete";
		break;
	case OPER_REPLACE:
		operation="replace";
		break;
	default:
		operation="";
			break;
	}
	person["Operation"]    = operation;

	root.append(person);
	strJson = writer.write(root);
	root.clear();

	bRes = TRUE;
//END:
	if (bRes)
	{
		if (pStrErr)
		{
			*pStrErr = strFormat.str();
		}
	}
	return bRes;

}


BOOL CWLJsonParse::Trust_Path_GetJson(__in VEC_TRUST_PATH& vecTrustPath,
									  __in TRUST_PROCESS_PATH_OPERATION oper,
									  __out string &strJson,
									  __out tstring *pStrErr)
{
				/*
	json格式：

	//信任路径
	{
		"Operation":"delete",
		"CMDContent":
		[
			{ "Path": "xxxx"},
			{ "Path": "xxxx1111"},
		]
	}


*/
	BOOL bRes = FALSE;
	wostringstream  strFormat;

	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	Json::Value CMDContent;

	int nCount = (int)vecTrustPath.size();

	for (int i=0; i< nCount; i++)
	{
		CMDContent["Path"]			= UnicodeToUTF8(vecTrustPath[i].strPath);
		root1.append(CMDContent);
	}
	person["CMDContent"]    = (Json::Value)root1;

	// 操作类型判断
	std::string operation;
	switch (oper)
	{
	case OPER_ADD:
		operation="add";
		break;
	case OPER_DELETE:
		operation="delete";
		break;
	case OPER_REPLACE:
		operation="replace";
		break;
	default:
		operation="";
			break;
	}
	person["Operation"] = operation;

	root.append(person);

	strJson = writer.write(root);
	root.clear();

	bRes = TRUE;
//END:
	if (!bRes)
	{
		if (pStrErr)
		{
			*pStrErr = strFormat.str();
		}
	}
	return bRes;
}




std::string CWLJsonParse::Safety_APP_GetJson(__in tstring ComputerID,
                                               __in WORD cmdType , __in WORD cmdID,
                                               stSAFETY_APP_UKEY &stEntry)
{

    /*
        客户端心跳响应消息下发运维key开关

        //json格式


        [
            {
              "CMDTYPE": 15,
              "CMDID": 20,
              "CMDVER": 1,
              "ComputerID": "XXX-Pc",
              "CMDContent":
                {
                    "Switch": "On",
                }
            }
        ]


    */

    std::string sJsonPacket = "";
    Json::FastWriter writer;

	Json::Value CMDContent_Item;
	Json::Value CMDContent;
	Json::Value root;
	Json::Value person;
    std::wstring wstrSwitch;

    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;
    person["CMDVER"] = (int)1;
    person["ComputerID"] = UnicodeToUTF8(ComputerID);


    if (UKEY_DEVICE_PRIVILEGE_SWITCH_OPEN == stEntry.enSwitch)
    {
        CMDContent["Switch"] = (int)1;
    }
    else
    {
        CMDContent["Switch"] = (int)0;
    }

    person["CMDContent"] = CMDContent;

    root.append(person);

    writer.omitEndingLineFeed();
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;

}
BOOL CWLJsonParse::Safety_APP_ParseJson(__in std::string& sJson,
                                                    stSAFETY_APP_UKEY &stEntry)
{
    /*
        客户端心跳响应消息下发运维key开关

        //json格式

        [
            {
              "CMDTYPE": 15,
              "CMDID": 20,
              "CMDVER": 1,
              "ComputerID": "XXX-Pc",
              "CMDContent":
                {
                    "Switch": "On",
                }
            }
        ]

        */


        std::string     strValue = "";
        BOOL bRes = FALSE;
        wostringstream  strTemp;
        Json::Reader    reader;
        Json::Value root;

        Json::Value CMDContent;

        Json::Value Switch;
        int nObject = 0;

        if( sJson.length() == 0)
        {
            goto END;
        }

        //补全 按数组解析
        strValue = sJson;
        if( strValue.substr(0, 1).compare("{") == 0)
        {
            strValue =  "[" + strValue;
            strValue +=  "]";
        }

        if (!reader.parse(strValue, root))
        {
            goto END;
        }

        if (root.size() <= 0 || !root.isArray())
        {
            goto END;
        }

        if (!root[0].isMember("CMDContent"))
        {
            goto END;

        }

        CMDContent = (Json::Value)root[0]["CMDContent"];

        if (!CMDContent.isMember("Switch"))
        {
            goto END;
        }

        Switch = CMDContent["Switch"];

        if (Switch.asInt())
        {
            stEntry.enSwitch = UKEY_DEVICE_PRIVILEGE_SWITCH_OPEN;
        }
        else
        {
            stEntry.enSwitch = UKEY_DEVICE_PRIVILEGE_SWITCH_CLOSE;
        }

        bRes = TRUE;
    END:

        return bRes;

}


std::string CWLJsonParse::Safety_APP_Ukeylog_GetJson(__in tstring ComputerID,
                                                                    __in WORD cmdType , __in WORD cmdID,
                                                                    stSATETYPE_APP_UKEY_LOG &stLogInfo)
{
/*
    [{
      "CMDTYPE": 15,
      "CMDID": 21,
      "CMDVER": 1,
      "ComputerID": "XXX-Pc",
      "CMDContent":
        {
            "Action": "Insert", // Insert, Remove
            "timestamp": "",
            "User": "Admin",
            "KeyMatched": "Yes",  // Yes, No

        }，

    }]


*/

	std::string sJsonPacket = "";
	Json::Value person;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value CMDContent;

    person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]			= (int)cmdID;
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);


    CMDContent["Action"] = stLogInfo.bInsert ? "Insert" : "Remove";
    time_t tm = time(NULL);
    CMDContent["timestamp"] = convertTime2Str(tm).c_str();
    CMDContent["User"] = UnicodeToUTF8(stLogInfo.szUser);
    CMDContent["KeyMatched"] = stLogInfo.bKeyMatch ? "Yes" : "No";

    person["CMDContent"] = CMDContent;

	root.append(person);

    writer.omitEndingLineFeed();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}
std::string CWLJsonParse::Vulenrable_Protect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in DWORD dwControlMode)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value CMDContent;

	CMDContent["ComputerID"]	= UnicodeToUTF8(ComputerID);
	CMDContent["CMDTYPE"]		= (int)cmdType;
	CMDContent["CMDID"]			= (int)cmdID;
	CMDContent["alarmMode"]		= (int)dwControlMode;


	root.append(CMDContent);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

BOOL CWLJsonParse::Vulenrable_Protect_ParseJson(__in std::string& sJson, __out DWORD* pdwControlMode, __out DWORD* pComputerID)
{
	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		return FALSE;
	}

	if (pComputerID != NULL && root[0].isMember("ComputerID"))
	{
		std::string computerID = root[0]["ComputerID"].asString();
		if ("LOCAL" == computerID)
		{
			*pComputerID = 0; // from LOCAL
		}
		else
		{
			*pComputerID = 1; // from SERVER
		}
	}

	if (root[0].isMember("alarmMode"))
	{
		root_Content =  (Json::Value)root[0]["alarmMode"];
		*pdwControlMode = 	root_Content.asInt();
	}
	return TRUE;
}

std::string CWLJsonParse::Accessable_Protect_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in DWORD dwControlMode)
{
	std::string sJsonPacket = "";
	Json::Value root1;
	Json::Value root;
	Json::FastWriter writer;
	Json::Value CMDContent;

	CMDContent["ComputerID"]	= UnicodeToUTF8(ComputerID);
	CMDContent["CMDTYPE"]		= (int)cmdType;
	CMDContent["CMDID"]			= (int)cmdID;
	CMDContent["alarmMode"]		= (int)dwControlMode;

	root.append(CMDContent);
	const string& strlog=root.toStyledString();
	sJsonPacket = writer.write(root);
	root.clear();
	return sJsonPacket;
}

BOOL CWLJsonParse::Accessable_Protect_ParseJson(__in std::string& sJson, __out DWORD* pdwControlMode, __out DWORD* pComputerID)
{
	std::string		strValue = "";
	Json::Value root;
	Json::Value root_Content;
	Json::Reader	reader;

	//补全 按数组解析
	strValue = sJson;
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		return FALSE;
	}

	if (pComputerID != NULL && root[0].isMember("ComputerID"))
	{
		std::string computerID = root[0]["ComputerID"].asString();
		if ("LOCAL" == computerID)
		{
			*pComputerID = 0; // from LOCAL
		}
		else
		{
			*pComputerID = 1; // from SERVER
		}
	}

	if (root[0].isMember("alarmMode"))
	{
		root_Content =  (Json::Value)root[0]["alarmMode"];
		*pdwControlMode = 	root_Content.asInt();
	}
	return TRUE;
}

BOOL CWLJsonParse::Upload_Dir_ServerInfo_ParseJson(__in string strJson, __out string &strTargetIP, __out unsigned short &nTargetPort, __out unsigned int &nTimeout, __out tstring *pStrErr)
{

	/*
	服务器下发的连接指令
	json格式：


	[
		{
			"ComputerID":"FEFOEACD",
			"CMDTYPE": 150,
			"CMDID": CMD_UPLOAD_DIR_CONNECT,
			"CMDContent":
			{

				"IP": "127.0.0.1"  //可选字段，没有则使用USM的IP
				"Port": 8888
				"Timeout": 60 //单位秒
			}
		}
	]

*/

	BOOL bRes = FALSE;
	Json::Reader	reader;
	Json::Value		rootArray;
	Json::Value		root;
	Json::Value		jsonContent;
	wostringstream  strFormat;

	const string	strIP = "IP";
	const string	strPort = "Port";
	const string	strTimeout = "Timeout";
	const string	strContent = "CMDContent";


	if (!reader.parse(strJson, root))
	{
		strFormat<< _T("invalid json")<<_T(", ");
		goto END;
	}

	if (!root.isArray())
	{
		//strFormat<< _T("json is not array")<<_T(", ");
		//goto END;
		rootArray.append(root);
		root.clear();
		root = rootArray;
	}



	if (!root[0].isMember(strContent))
	{
		strFormat<< _T("no CMDContent")<<_T(", ");
		goto END;
	}
	jsonContent = (Json::Value)root[0][strContent.c_str()];

	if (jsonContent.isMember(strIP))
	{
		if (!root[strIP].isString())
		{
			strFormat<< _T("invalid json: IP")<<_T(", ");
			goto END;
		}

		strTargetIP = root[strIP].asString();

	}
	else
	{
		strTargetIP = "";
	}

	if (jsonContent.isMember(strPort))
	{
		if (!jsonContent[strPort].isUInt())
		{
			strFormat<< _T("invalid json: Port")<<_T(", ");
			goto END;
		}

		nTargetPort = jsonContent[strPort].asUInt();

	}
	else
	{
		strFormat<< _T("no port")<<_T(", ");
		goto END;
	}

	if (jsonContent.isMember(strTimeout))
	{
		if (!jsonContent[strTimeout].isUInt())
		{
			strFormat<< _T("invalid json: Timeout")<<_T(", ");
			goto END;
		}

		nTimeout = jsonContent[strTimeout].asUInt();

	}
	else
	{
		strFormat<< _T("no timeout")<<_T(", ");
		goto END;
	}



	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}

	return bRes;
}


BOOL CWLJsonParse::Upload_Dir_Login_GetJson(__in tstring strComputerID, /*__in CMDTYPE nCmdType, __in int nCmdID, */__out string &strJson, __out tstring *pStrErr)
{
/*
	登录协议
	json格式：
	[
		{
			"ComputerID":"FEFOEACD",
		}
	]

*/
	BOOL bRes = FALSE;
	wostringstream  strFormat;
	Json::Value		root;
	Json::Value		JsonObject;
	Json::FastWriter writer;

	if (strComputerID.size() == 0)
	{
		strFormat<< _T("ComputerID.size == 0")<<_T(", ");
		goto END;
	}

	JsonObject["ComputerID"] = UnicodeToUTF8(strComputerID.c_str());
	//JsonObject["CMDTYPE"] = nCmdType;
	//JsonObject["CMDID"] = nCmdID;

	root.append(JsonObject);

	strJson = writer.write(root);

	bRes = TRUE;
END:

	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}
	return bRes;
}

BOOL CWLJsonParse::Upload_Dir_Fail_GetJson(__in tstring ComputerID,  __in const STC_USM_REQUEST_DIR &stcReqDir, __in tstring strErr, __out string &strJson, __out tstring *pStrErr)
{
	/*

	//失败
	[
		{
			“ComputerID”:”FEFOEACD”,
			"CMDContent":
			{

				"ReqID": 1
				"Path": "/"   或者"c:\windows",
				"Result": false
				"ErrorInfo": "invalid path"
			}
		}
	]
	*/

	BOOL bRes = FALSE;
	//wostringstream  strFormat;
	Json::Value Root;
	Json::Value CMDContent;
	Json::FastWriter writer;

	//组织协议主体
	Root["ComputerID"]	= UnicodeToUTF8(ComputerID);

	//CMDContent
	CMDContent["ReqID"] = (unsigned int)stcReqDir.dwReqID;
	CMDContent["Path"] = UnicodeToUTF8(stcReqDir.strReqDir);
	CMDContent["Result"] = false;
	CMDContent["ErrorInfo"] = UnicodeToUTF8(strErr);

	Root["CMDContent"] = CMDContent;
	strJson = writer.write(Root);

	bRes = TRUE;
//END:
//	if (pStrErr)
//	{
//		*pStrErr = strFormat.str();
//	}

	return bRes;
}


BOOL CWLJsonParse::Upload_Dir_GetJson(__in tstring ComputerID, __in const STC_UPLOAD_DIR_INFO &stcDirInfo, __in const int nMaxLen, __out vector<string> &vecJson, __out tstring *pStrErr)
{
	/*
	返回请求目录
	成功
	[
		{
			“ComputerID”:”FEFOEACD”,
			"CMDContent":
			{


				"ReqID": 1
				"Path": "/"   或者"c:\windows",
				"Result": true
				"End":0 或 1    //0：未完成   1：完成
				"Items":
				[
					{
						"Path": "xxxx",
						"Type":  1
					},
					{
						"Path": "xxxx",
						"Type":  1
					}

				]
			}
		}
	]

	*/
	BOOL bRes = FALSE;
	//wostringstream  strFormat;

	Json::Value Root;
	Json::Value CMDContent;
	Json::Value CMDContentTemp;
	Json::FastWriter writer;

	//组织协议主体
	Root["ComputerID"]	= UnicodeToUTF8(ComputerID);

	//CMDContent
	CMDContent["ReqID"] = (unsigned int)stcDirInfo.dwReqID;
	CMDContent["Path"] = UnicodeToUTF8(stcDirInfo.strReqDir);
	CMDContent["Result"] = true;

	CMDContentTemp = CMDContent;
	for (unsigned int i = 0; i < stcDirInfo.vecItems.size(); i++)
	{
		Json::Value Item;

		Item["Path"] = UnicodeToUTF8(stcDirInfo.vecItems[i].strPath);
		Item["Type"] = stcDirInfo.vecItems[i].emType;

		const string strTempItem = Item.toStyledString();
		const string strCMDContent = CMDContentTemp.toStyledString();

		if (strTempItem.length() + strCMDContent.length() < nMaxLen)
		{
			CMDContentTemp["Items"].append(Item);
		}
		else
		{
			CMDContentTemp["Items"].append(Item);
			Json::Value RootTemp(Root);
			CMDContentTemp["End"] = 0;
			RootTemp["CMDContent"] = CMDContentTemp;
			//string strTest = writer.write(RootTemp);
			vecJson.push_back(writer.write(RootTemp));
			CMDContentTemp.clear();

			CMDContentTemp = CMDContent;
		}
	}

	CMDContentTemp["End"] = 1;
	Root["CMDContent"] = CMDContentTemp;
	vecJson.push_back(writer.write(Root));

	bRes = TRUE;
//END:
	//if (pStrErr)
	//{
	//	*pStrErr = strFormat.str();
	//}

	return bRes;
}



BOOL CWLJsonParse::Upload_Dir_Request_ParseJson(__in const std::string& sJson,  __out STC_USM_REQUEST_DIR &stcRequestDir, tstring *pStrErr)
{
	/*
	服务器请求目录
	CMD:

	json格式：


	[
		{
			"ComputerID":"FEFOEACD",
			"CMDContent":
			{
				"SessionFinsh": 1(关闭) 0(保持连接)
				"ReqID": 1 (累加),
				"Path": "/"   或者"c:\windows"
			}
		}
	]



*/

	BOOL bRes = FALSE;
	wostringstream  strFormat;
	Json::Reader	reader;
	Json::Value		root;
	Json::Value		jsonObject;
	const string	strContent = "CMDContent";
	const string	strSessionFinsh = "SessionFinsh";
	const string	strReqID = "ReqID";
	const string	strPath = "Path";
	const string	strReqType = "ReqType";

	if (!reader.parse(sJson, root))
	{
		strFormat<< _T("invalid json")<<_T(", ");
		goto END;
	}

	if (!root.isArray() || root.size() < 1)
	{
		strFormat<< _T("json is not array")<<_T(", ");
		goto END;
	}

	jsonObject = root[0][strContent.c_str()];

	//获取SessionFinsh字段
	if (jsonObject.isMember(strSessionFinsh.c_str()))
	{
		if (!jsonObject[strSessionFinsh.c_str()].isInt())
		{
			strFormat<< _T("invalid json: SessionFinsh")<<_T(", ");
			goto END;
		}

		stcRequestDir.nSessionFinish = jsonObject[strSessionFinsh.c_str()].asInt();

	}

	//获取ReqID
	if (!jsonObject.isMember(strReqID.c_str()))
	{
		strFormat<< _T("invalid json: ReqID")<<_T(", ");
		goto END;
	}
	stcRequestDir.dwReqID = jsonObject[strReqID.c_str()].asUInt();

	//获取Path
	if (!jsonObject.isMember(strPath.c_str()))
	{
		strFormat<< _T("invalid json: Path")<<_T(", ");
		goto END;
	}
	stcRequestDir.strReqDir = UTF8ToUnicode(jsonObject[strPath.c_str()].asString());

	//ReqType， USM暂时没有
	//if (!root.isMember(strReqType.c_str()))
	//{
	//	strFormat<< _T("invalid json: ReqType")<<_T(", ");
	//	goto END;
	//}
	//stcRequestDir. = root[strReqType.c_str()].asUInt();


	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}

	return bRes;
}

// ==================== 白名单查询相关JSON函数 ====================

BOOL CWLJsonParse::Upload_Whitelist_ServerInfo_ParseJson(__in string strJson, __out string &strTargetIP, __out unsigned short &nTargetPort, __out unsigned int &nTimeout, __out tstring *pStrErr)
{
	// 与Upload_Dir_ServerInfo_ParseJson相同的实现
	return Upload_Dir_ServerInfo_ParseJson(strJson, strTargetIP, nTargetPort, nTimeout, pStrErr);
}

BOOL CWLJsonParse::Upload_Whitelist_Login_GetJson(__in tstring strComputerID, __out string &strJson, __out tstring *pStrErr)
{
	BOOL bRes = FALSE;
	wostringstream  strFormat;
	Json::Value		root;
	Json::Value		JsonObject;
	Json::FastWriter writer;

	if (strComputerID.size() == 0)
	{
		strFormat<< _T("ComputerID.size == 0")<<_T(", ");
		goto END;
	}

	JsonObject["ComputerID"] = UnicodeToUTF8(strComputerID.c_str());

	root.append(JsonObject);

	strJson = writer.write(root);

	bRes = TRUE;
END:

	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}
	return bRes;
}

BOOL CWLJsonParse::Upload_Whitelist_Request_ParseJson(__in const std::string& sJson, __out STC_USM_REQUEST_WHITELIST &stcRequestWhitelist, tstring *pStrErr)
{
	/*
	服务器请求白名单查询
	json格式：
	{
		"ComputerID":"FEFOEACD",
		"CMDContent":
		{
			"SessionFinsh": 1(关闭) 0(保持连接)
			"ReqID": 1 (累加),
			"ReqType": "0",  //0(指定条件查询) 1(查询所有)
			"KeyWord": "test.exe",
			"HashValue": "",
			"SystemFile": 0,  //1(是) 0(否) -1(不过滤)
			"PageIndex": "1",  //页码，从1开始
			"PerPageCount": "20",
			"ReqForExport": 0  //是否是导出查询，1(导出) 0(普通查询)
		}
	}
	*/

	BOOL bRes = FALSE;
	wostringstream  strFormat;
	Json::Reader	reader;
	Json::Value		root;
	Json::Value		jsonObject;
	const string	strContent = "CMDContent";
	const string	strSessionFinsh = "SessionFinsh";
	const string	strReqID = "ReqID";
	const string	strReqType = "ReqType";
	const string	strKeyWord = "KeyWord";
	const string	strHashValue = "HashValue";
	const string	strSystemFile = "SystemFile";
	const string	strPageIndex = "PageIndex";
	const string	strPerPageCount = "PerPageCount";
	const string	strReqForExport = "ReqForExport";  // 是否是导出查询

	if (!reader.parse(sJson, root))
	{
		strFormat<< _T("invalid json")<<_T(", ");
		goto END;
	}

	if (!root.isArray() || root.size() < 1)
	{
		strFormat<< _T("json is not array")<<_T(", ");
		goto END;
	}

	jsonObject = root[0][strContent.c_str()];

	// 获取SessionFinsh字段
	if (jsonObject.isMember(strSessionFinsh.c_str()))
	{
		if (!jsonObject[strSessionFinsh.c_str()].isInt())
		{
			strFormat<< _T("invalid json: SessionFinsh")<<_T(", ");
			goto END;
		}

		stcRequestWhitelist.nSessionFinish = jsonObject[strSessionFinsh.c_str()].asInt();
	}
	else
	{
		stcRequestWhitelist.nSessionFinish = 0; // 默认保持连接
	}

	// 获取ReqID
	if (!jsonObject.isMember(strReqID.c_str()))
	{
		strFormat<< _T("invalid json: ReqID")<<_T(", ");
		goto END;
	}
	stcRequestWhitelist.dwReqID = jsonObject[strReqID.c_str()].asUInt();

	// 获取ReqType
	if (jsonObject.isMember(strReqType.c_str()))
	{
		if (jsonObject[strReqType.c_str()].isInt())
		{
			stcRequestWhitelist.nReqType = jsonObject[strReqType.c_str()].asInt();
		}
		else if (jsonObject[strReqType.c_str()].isString())
		{
			stcRequestWhitelist.nReqType = atoi(jsonObject[strReqType.c_str()].asString().c_str());
		}
		else
		{
			strFormat<< _T("invalid json: ReqType")<<_T(", ");
			goto END;
		}
	}
	else
	{
		stcRequestWhitelist.nReqType = 0; // 默认指定条件查询
	}

	// 获取KeyWord
	if (jsonObject.isMember(strKeyWord.c_str()))
	{
		stcRequestWhitelist.strKeyWord = UTF8ToUnicode(jsonObject[strKeyWord.c_str()].asString());
	}
	else
	{
		stcRequestWhitelist.strKeyWord = _T("");
	}

	// 获取HashValue
	if (jsonObject.isMember(strHashValue.c_str()))
	{
		stcRequestWhitelist.strHashValue = UTF8ToUnicode(jsonObject[strHashValue.c_str()].asString());
	}
	else
	{
		stcRequestWhitelist.strHashValue = _T("");
	}

	// 获取SystemFile
	if (jsonObject.isMember(strSystemFile.c_str()))
	{
		if (jsonObject[strSystemFile.c_str()].isInt())
		{
			stcRequestWhitelist.nSystemFile = jsonObject[strSystemFile.c_str()].asInt();
		}
		else
		{
			stcRequestWhitelist.nSystemFile = -1; // 不过滤
		}
	}
	else
	{
		stcRequestWhitelist.nSystemFile = -1; // 不过滤
	}

	// 获取PageIndex
	if (jsonObject.isMember(strPageIndex.c_str()))
	{
		if (jsonObject[strPageIndex.c_str()].isUInt())
		{
			stcRequestWhitelist.dwPageIndex = jsonObject[strPageIndex.c_str()].asUInt();
		}
		else if (jsonObject[strPageIndex.c_str()].isString())
		{
			stcRequestWhitelist.dwPageIndex = atoi(jsonObject[strPageIndex.c_str()].asString().c_str());
		}
		else
		{
			strFormat<< _T("invalid json: PageIndex")<<_T(", ");
			goto END;
		}
	}
	else
	{
		stcRequestWhitelist.dwPageIndex = 1; // 默认第1页
	}

	// 获取PerPageCount
	if (jsonObject.isMember(strPerPageCount.c_str()))
	{
		if (jsonObject[strPerPageCount.c_str()].isUInt())
		{
			stcRequestWhitelist.dwPerPageCount = jsonObject[strPerPageCount.c_str()].asUInt();
		}
		else if (jsonObject[strPerPageCount.c_str()].isString())
		{
			stcRequestWhitelist.dwPerPageCount = atoi(jsonObject[strPerPageCount.c_str()].asString().c_str());
		}
		else
		{
			strFormat<< _T("invalid json: PerPageCount")<<_T(", ");
			goto END;
		}
	}
	else
	{
		stcRequestWhitelist.dwPerPageCount = 20; // 默认每页20条
	}

	// 获取ReqForExport（导出查询标志）
	if (jsonObject.isMember(strReqForExport.c_str()))
	{
		if (jsonObject[strReqForExport.c_str()].isInt())
		{
			stcRequestWhitelist.nReqForExport = jsonObject[strReqForExport.c_str()].asInt();
		}
		else if (jsonObject[strReqForExport.c_str()].isString())
		{
			stcRequestWhitelist.nReqForExport = atoi(jsonObject[strReqForExport.c_str()].asString().c_str());
		}
		else
		{
			stcRequestWhitelist.nReqForExport = 0; // 默认普通查询
		}
	}
	else
	{
		stcRequestWhitelist.nReqForExport = 0; // 默认普通查询
	}

	bRes = TRUE;
END:
	if (pStrErr)
	{
		*pStrErr = strFormat.str();
	}

	return bRes;
}

BOOL CWLJsonParse::Upload_Whitelist_GetJson(__in tstring ComputerID, __in const STC_UPLOAD_WHITELIST_INFO &stcWhitelistInfo, __in const int nMaxLen, __out vector<string> &vecJson, __out tstring *pStrErr)
{
	/*
	返回白名单列表（协议格式为数组）
	[
		{
			"ComputerID":"FEFOEACD",
			"CMDContent":
			{
				"ReqID": 1
				"Result": true
				"End":0 或 1    //0：未完成   1：完成（导出查询时只有最后一包为1）
				"CurCount": 20
				"TotalCount": 19876
				"CurPageIndex": 1    //当前页码（从1开始，与请求的PageIndex一致）
				"CurPerPageCount": 20  //每页条数（导出查询时使用）
				"Items":
				[
					{
						"FilePath": "xxxx",
						"HashValue":"",
						"Time":1764125939,
						"Source":0,
						"SystemFile":0,
					},
					...
				]
			}
		}
	]
	*/
	BOOL bRes = FALSE;
	Json::Value Root;
	Json::Value JsonObject;
	Json::Value CMDContent;
	Json::Value CMDContentTemp;
	Json::FastWriter writer;

	// 组织协议主体（注意：返回数组格式）
	JsonObject["ComputerID"]	= UnicodeToUTF8(ComputerID);

	// CMDContent
	CMDContent["ReqID"] = (unsigned int)stcWhitelistInfo.dwReqID;
	CMDContent["Result"] = stcWhitelistInfo.bResult;
	CMDContent["CurCount"] = (unsigned int)stcWhitelistInfo.dwCurCount;
	CMDContent["TotalCount"] = (unsigned int)stcWhitelistInfo.dwTotalCount;
	CMDContent["CurPageIndex"] = (unsigned int)stcWhitelistInfo.dwCurPageIndex;  // 当前页索引（用于导出查询）
	CMDContent["CurPerPageCount"] = (unsigned int)stcWhitelistInfo.dwCurPerPageCount;  // 每页条数（用于导出查询）

	CMDContentTemp = CMDContent;
	for (unsigned int i = 0; i < stcWhitelistInfo.vecItems.size(); i++)
	{
		Json::Value Item;

		Item["FilePath"] = UnicodeToUTF8(stcWhitelistInfo.vecItems[i].strFilePath);
		Item["HashValue"] = UnicodeToUTF8(stcWhitelistInfo.vecItems[i].strHashValue);
		Item["Time"] = (Json::Int64)stcWhitelistInfo.vecItems[i].tTime;
		Item["Source"] = stcWhitelistInfo.vecItems[i].nSource;
		Item["SystemFile"] = stcWhitelistInfo.vecItems[i].nSystemFile;

		const string strTempItem = Item.toStyledString();
		const string strCMDContent = CMDContentTemp.toStyledString();

		if (strTempItem.length() + strCMDContent.length() < (size_t)nMaxLen)
		{
			CMDContentTemp["Items"].append(Item);
		}
		else
		{
			CMDContentTemp["Items"].append(Item);
			CMDContentTemp["End"] = 0;
			
			// 封装成数组格式
			Json::Value JsonObjectTemp = JsonObject;
			JsonObjectTemp["CMDContent"] = CMDContentTemp;
			Root.append(JsonObjectTemp);
			vecJson.push_back(writer.write(Root));
			
			Root.clear();
			CMDContentTemp.clear();
			CMDContentTemp = CMDContent;
		}
	}

	CMDContentTemp["End"] = stcWhitelistInfo.nEnd;
	JsonObject["CMDContent"] = CMDContentTemp;
	Root.append(JsonObject);
	vecJson.push_back(writer.write(Root));

	bRes = TRUE;

	return bRes;
}

BOOL CWLJsonParse::Upload_Whitelist_Fail_GetJson(__in tstring ComputerID, __in const STC_USM_REQUEST_WHITELIST &stcRequestWhitelist, __in tstring strErr, __out string &strJson, __out tstring *pStrErr)
{
	/*
	失败（协议格式为数组）
	[
		{
			"ComputerID":"FEFOEACD",
			"CMDContent":
			{
				"ReqID": 1
				"Result": false
				"ErrorInfo": "query failed"
			}
		}
	]
	*/

	BOOL bRes = FALSE;
	Json::Value Root;
	Json::Value JsonObject;
	Json::Value CMDContent;
	Json::FastWriter writer;

	// 组织协议主体
	JsonObject["ComputerID"]	= UnicodeToUTF8(ComputerID);

	// CMDContent
	CMDContent["ReqID"] = (unsigned int)stcRequestWhitelist.dwReqID;
	CMDContent["Result"] = false;
	CMDContent["ErrorInfo"] = UnicodeToUTF8(strErr);

	JsonObject["CMDContent"] = CMDContent;
	Root.append(JsonObject);
	strJson = writer.write(Root);

	bRes = TRUE;

	return bRes;
}


//appstore-liudan

BOOL CWLJsonParse::AppStorePackages_GetValue(__in const std::string& sJson,
                                             __out TRACE_INSTALL_CFG_ST* traceInstallData, 
                                             __out UPDATEWLFILEBYHASH_CFG_ST* GreenPackage,
                                             __out VECT_SETUP_CFG_ST* vectSelfUpdatePackage,
                                             int *pnOperation)
{
    BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

   strValue = sJson;

   //补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

    if( root[0].isMember("Operation"))
    {
        *pnOperation = root[0]["Operation"].asInt();
    }
    else
    {
        *pnOperation = 0;
    }

    if( root[0].isMember("CMDContentSetupPackage"))
    {
        root_1 =  (Json::Value)root[0]["CMDContentSetupPackage"]; //进行解析

        // 进行解析
        nObject = root_1.size();
        std::string     sHashCode;
        std::string     sWlCode;
        std::wstring    wsAppName;
        std::string     sStartTime;
        std::string     sEndTime;
        struct tm _tm;
        APPSTORE_SETUP_ST stItem;
        for (int i=0; i < nObject; i++)
        {
            sHashCode    = (root_1[i]["MD5"].asString());
            sWlCode      = (root_1[i]["WlCode"].asString());
            wsAppName    = UTF8ToUnicode(root_1[i]["FileName"].asString());
            sStartTime   = (root_1[i]["StartTime"].asString());
            sEndTime     = (root_1[i]["EndTime"].asString());

            memset(&stItem,0,sizeof(stItem));
            //strptime("2001-11-12 18:31:01", "%Y-%m-%d %H:%M:%S", &tm);
            wcscpy_s(stItem.wszSetupAppName,sizeof(stItem.wszSetupAppName)/sizeof(stItem.wszSetupAppName[0]) ,wsAppName.c_str());
            Hex2Bin(sHashCode.c_str(), sizeof(stItem.ucHashCode)*2,(char*)stItem.ucHashCode);

            Hex2Bin(sWlCode.c_str(), sizeof(stItem.ucsWlCode)*2,(char*)stItem.ucsWlCode);
            //strncpy(stItem.szHashCode, sHashCode.c_str(),sizeof(stItem.szHashCode)/sizeof(stItem.szHashCode[0]));
            ///strncpy(stItem.szWlCode, sWlCode.c_str(),sizeof(stItem.szWlCode)/sizeof(stItem.szWlCode[0]));
            if(convertStr2Tm(_tm,sStartTime))
            {
                stItem.stStartTime.Year = _tm.tm_year+1900;
                stItem.stStartTime.Month = _tm.tm_mon+1;
                stItem.stStartTime.Day = _tm.tm_mday;
                stItem.stStartTime.Hour = _tm.tm_hour;
                stItem.stStartTime.Minute = _tm.tm_min;
                stItem.stStartTime.Second = _tm.tm_sec;
            }
            if(convertStr2Tm(_tm,sEndTime))
            {
                stItem.stEndTime.Year = _tm.tm_year+1900;
                stItem.stEndTime.Month = _tm.tm_mon+1;
                stItem.stEndTime.Day = _tm.tm_mday;
                stItem.stEndTime.Hour = _tm.tm_hour;
                stItem.stEndTime.Minute = _tm.tm_min;
                stItem.stEndTime.Second = _tm.tm_sec;
            }
           // strptime (sStartTime.c_str(), "%Y-%m-%d %H:%M:%S", &);
           // strptime (sEndTime.c_str(), "%Y-%m-%d %H:%M:%S", &stItem.stEndTime);
            traceInstallData->vecSetup.push_back(stItem);
        }
    }
    if( root[0].isMember("CMDContentSelfUpdate"))
    {
        root_1 =  (Json::Value)root[0]["CMDContentSelfUpdate"]; //进行解析
		if(root_1.isArray())
		{
			// 进行解析
			nObject = root_1.size();
			std::string     sHashCode;
			std::string     sWlCode;
			std::wstring    wsAppName;
			std::string     sStartTime;
			std::string     sEndTime;
			//  struct tm _tm;
			for (int i=0; i < nObject; i++)
			{
				sHashCode    = (root_1[i]["MD5"].asString());
				sWlCode      = (root_1[i]["WlCode"].asString());
				wsAppName    = UTF8ToUnicode(root_1[i]["FileName"].asString());


				APPSTORE_SETUP_ST stItem;
				//strptime("2001-11-12 18:31:01", "%Y-%m-%d %H:%M:%S", &tm);
				wcscpy_s(stItem.wszSetupAppName,sizeof(stItem.wszSetupAppName)/sizeof(stItem.wszSetupAppName[0]) ,wsAppName.c_str());
				//strncpy(stItem.szHashCode, sHashCode.c_str(),sizeof(stItem.szHashCode)/sizeof(stItem.szHashCode[0]));
				Hex2Bin(sWlCode.c_str(),sizeof(stItem.ucsWlCode)*2, (char*)stItem.ucsWlCode);
				Hex2Bin(sHashCode.c_str(),sizeof(stItem.ucHashCode)*2, (char*)stItem.ucHashCode);
				vectSelfUpdatePackage->push_back(stItem);
			}
       
        }
    }

    if( root[0].isMember("CMDContentGreenPackage"))
    {
        root_1 =  (Json::Value)root[0]["CMDContentGreenPackage"]; //进行解析
		if(root_1.isArray())
		{
			// 进行解析
			nObject = root_1.size();
			std::string     sHashCode;
			std::string     sWlCode;
			std::wstring    wsAppName;
			std::string     sStartTime;
			std::string     sEndTime;
			//  struct tm _tm;
			for (int i=0; i < nObject; i++)
			{
				//sHashCode    = (root_1[i]["HashCode"].asString());
				sWlCode      = (root_1[i]["WlCode"].asString());
				wsAppName    = UTF8ToUnicode(root_1[i]["FileName"].asString());


				UPDATEWLFILEBYHASH_ENTRY_JSON_ST stItem;
				//strptime("2001-11-12 18:31:01", "%Y-%m-%d %H:%M:%S", &tm);
				wcscpy_s(stItem.wszFileName,sizeof(stItem.wszFileName)/sizeof(stItem.wszFileName[0]) ,wsAppName.c_str());
				//strncpy(stItem.szHashCode, sHashCode.c_str(),sizeof(stItem.szHashCode)/sizeof(stItem.szHashCode[0]));
				Hex2Bin(sWlCode.c_str(),sizeof(stItem.ucsWlCode)*2, (char*)stItem.ucsWlCode);
				GreenPackage->vecEntry.push_back(stItem);
			}
		}
    }


	bResult = TRUE;

_exist_:
	return bResult;
}


BOOL CWLJsonParse::TraceInstall_GetValue(__in const std::string& sJson,  __out TRACE_INSTALL_CFG_ST* traceInstallData)
{
    BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

   strValue = sJson;

   //补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

    if( root[0].isMember("Operation"))
    {
        traceInstallData->nOperation = root[0]["Operation"].asInt();
    }
    else
    {
        traceInstallData->nOperation = 0;
    }

    if( root[0].isMember("CMDContent"))
    {
        root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		if(root_1.isArray())
		{
			// 进行解析
			nObject = root_1.size();
			std::string     sHashCode;
			std::string     sWlCode;
			std::wstring    wsAppName;
			std::string     sStartTime;
			std::string     sEndTime;
			struct tm _tm;
			for (int i=0; i < nObject; i++)
			{
				sHashCode    = (root_1[i]["HashCode"].asString());
				sWlCode      = (root_1[i]["WlCode"].asString());
				wsAppName    = UTF8ToUnicode(root_1[i]["FileName"].asString());
				sStartTime   = (root_1[i]["StartTime"].asString());
				sEndTime     = (root_1[i]["EndTime"].asString());

				APPSTORE_SETUP_ST stItem;
				//strptime("2001-11-12 18:31:01", "%Y-%m-%d %H:%M:%S", &tm);
				wcscpy_s(stItem.wszSetupAppName,sizeof(stItem.wszSetupAppName)/sizeof(stItem.wszSetupAppName[0]) ,wsAppName.c_str());
				Hex2Bin(sHashCode.c_str(), sizeof(stItem.ucHashCode)*2,(char*)stItem.ucHashCode);

				Hex2Bin(sWlCode.c_str(), sizeof(stItem.ucsWlCode)*2,(char*)stItem.ucsWlCode);
				//strncpy(stItem.szHashCode, sHashCode.c_str(),sizeof(stItem.szHashCode)/sizeof(stItem.szHashCode[0]));
				///strncpy(stItem.szWlCode, sWlCode.c_str(),sizeof(stItem.szWlCode)/sizeof(stItem.szWlCode[0]));
				convertStr2Tm(_tm,sStartTime);
				stItem.stStartTime.Year = _tm.tm_year+1900;
				stItem.stStartTime.Month = _tm.tm_mon+1;
				stItem.stStartTime.Day = _tm.tm_mday;
				stItem.stStartTime.Hour = _tm.tm_hour;
				stItem.stStartTime.Minute = _tm.tm_min;
				stItem.stStartTime.Second = _tm.tm_sec;
				convertStr2Tm(_tm,sEndTime);
				stItem.stEndTime.Year = _tm.tm_year+1900;
				stItem.stEndTime.Month = _tm.tm_mon+1;
				stItem.stEndTime.Day = _tm.tm_mday;
				stItem.stEndTime.Hour = _tm.tm_hour;
				stItem.stEndTime.Minute = _tm.tm_min;
				stItem.stEndTime.Second = _tm.tm_sec;
				// strptime (sStartTime.c_str(), "%Y-%m-%d %H:%M:%S", &);
				// strptime (sEndTime.c_str(), "%Y-%m-%d %H:%M:%S", &stItem.stEndTime);
				traceInstallData->vecSetup.push_back(stItem);
			}
		}
       
    }



	bResult = TRUE;

_exist_:
	return bResult;
}

std::string CWLJsonParse::TraceInstall_Stat_GetJson(
    __in tstring ComputerID,
    __in UCHAR *uczwlCode, __in tstring fileName,__in int stat,__in tstring result,__in int resultID)
{
	std::string sJsonPacket = "";
	std::string sJsonBody = "";

	Json::Value CMDContent;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	wstring wsRes;

{
    char tmp[60] = {0};
    int l = 0;
    for(int i = 0;i<20;i++)
    {
        l +=_snprintf(tmp+l, 2, "%02X",uczwlCode[i]);
    }
    CMDContent["WlCode"]    = tmp;
}
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)15;
	person["CMDID"]			= (int)16;
	CMDContent["AppName"]	= UnicodeToUTF8(fileName);
	CMDContent["State"]		= stat;
	CMDContent["REASON"]		= UnicodeToUTF8(result);
	CMDContent["ERRCODE"]		= resultID;

    person["CMDContent"] = CMDContent;
	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}




//appstore-liudan

BOOL CWLJsonParse::UpdateWlFileByHash_GetValue(__in const std::string& sJson,  __out UPDATEWLFILEBYHASH_CFG_ST* RetData)
{
    BOOL bResult = FALSE;

	Json::Value root;
	Json::Value root_1;
	Json::Value root_2;
	Json::Reader	reader;
	std::string		strValue = "";

	if( sJson.length() == 0)
	{
		goto _exist_;
	}

   strValue = sJson;

   //补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if (!reader.parse(strValue, root))
	{
		goto _exist_;
	}

	int nObject = root.size();
	if( nObject < 1 || !root.isArray())
	{
		goto _exist_;
	}

    if( root[0].isMember("Operation"))
    {
        RetData->nOperation = root[0]["Operation"].asInt();
    }
    else
    {
        RetData->nOperation = 0;
    }

    if( root[0].isMember("CMDContent"))
    {
        root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

		if(root_1.isArray())
		{
			// 进行解析
			nObject = root_1.size();
			std::string     sHashCode;
			std::string     sWlCode;
			std::wstring    wsAppName;
			std::string     sStartTime;
			std::string     sEndTime;
			//  struct tm _tm;
			for (int i=0; i < nObject; i++)
			{
				//sHashCode    = (root_1[i]["HashCode"].asString());
				sWlCode      = (root_1[i]["WlCode"].asString());
				wsAppName    = UTF8ToUnicode(root_1[i]["FileName"].asString());


				UPDATEWLFILEBYHASH_ENTRY_JSON_ST stItem;
				//strptime("2001-11-12 18:31:01", "%Y-%m-%d %H:%M:%S", &tm);
				wcscpy_s(stItem.wszFileName,sizeof(stItem.wszFileName)/sizeof(stItem.wszFileName[0]) ,wsAppName.c_str());
				//strncpy(stItem.szHashCode, sHashCode.c_str(),sizeof(stItem.szHashCode)/sizeof(stItem.szHashCode[0]));
				Hex2Bin(sWlCode.c_str(),sizeof(stItem.ucsWlCode)*2, (char*)stItem.ucsWlCode);
				RetData->vecEntry.push_back(stItem);
			}
		}  
    }



	bResult = TRUE;

_exist_:
	return bResult;
}
BOOL CWLJsonParse::SelfupdatePragam_GetValue(__in const std::string& sJson,  __out UPDATEWLFILEBYHASH_CFG_ST* RetData)
{
    return UpdateWlFileByHash_GetValue(sJson,RetData);
}
void CWLJsonParse::Hex2Bin(const char * hex,  int length, char * out)
{
	int i = 0, j = 0;
	char bin_table[256];

	for(i='0';i<='9';i++,j++) bin_table[i] = j;
	for(i='A';i<='F';i++,j++) bin_table[i] = j;
	j=10;
	for(i='a';i<='f';i++,j++) bin_table[i] = j;


	char uchTemp;
	for( i = 0 ; i<length; i=i+2){
		uchTemp = bin_table[hex[i]];
		uchTemp = uchTemp<<4;
		uchTemp += bin_table[hex[i+1]];
		out[i/2] = uchTemp;
	}
	return;
}
// -- 编码
 void CWLJsonParse::Bin2Hex(const unsigned char * bin, unsigned int length, char * out)
{
	//unsigned int i=0;
	unsigned char uchTemp;

    const char * hex_table = "0123456789ABCDEF";

	int i=0;

	for(i=0;i<length;i++){
		uchTemp = bin[i];
		out[i*2]= hex_table[uchTemp>>4];
		out[i*2+1] = hex_table[uchTemp & 0x0f];
	}
	return;
}

 //【白名单】- 文件摆渡 - 解析源文件和目的路径
 BOOL CWLJsonParse::SafeFileCopy_GetValue(__in const std::string& strJson, __out std::wstring &strTargetPath, 
	 __out std::vector<std::wstring> &vsSourcePath, __out std::wstring &strError)
 {
	 strError = _T("");
	 Json::Reader parser;
	 Json::Value root;
	 BOOL bRet = FALSE;

	 if (strJson.empty()) 
	 {
		 strError = _T("strJson is empty");
		 return bRet;
	 }

	 if (!parser.parse(strJson, root)) 
	 {
		 strError = _T("Failed to parse strJson");
		 return bRet;
	 }

	 if(!root.isObject())
	 {
		return bRet;
	 }
	 // 解析目的路径
	 if (root.isMember("target_path") && 
		 root["target_path"].isString())
	 {
		 std::string path = root["target_path"].asString();
		 strTargetPath = CStrUtil::UTF8ToUnicode(path);
	 } 
	else
	{
		 strError = _T("Failed to parse target_path");
		 return bRet;
	}

	 // 解析源路径
	 if (root.isMember("source_path"))
	 {
		 if (root["source_path"].isString())
		 {
			 std::string path = root["source_path"].asString();
			 std::wstring wsPath = CStrUtil::UTF8ToUnicode(path);
			 vsSourcePath.push_back(wsPath);
		 }
		 else if (root["source_path"].isArray())
		 {
			 std::string path;
			 std::wstring wsPath;
			 const Json::Value &arrScanPath = root["source_path"];
			 unsigned int nSize = arrScanPath.size();
			 for (unsigned int i = 0; i < nSize; ++i)
			 {
				 path = arrScanPath[i].asString();
				 wsPath = CStrUtil::UTF8ToUnicode(path);
				 vsSourcePath.push_back(wsPath);
			 }
		 }
		 else
		 {
			strError = _T("source_path is not string or array");
			return bRet;
		 }
	 }
	else
	{
		 strError = _T("Failed to parse source_path");
		 return bRet;
	}

	 bRet = TRUE;

	 return bRet;
 }

//【白名单】- 软件安装与卸载 - 指定文件
BOOL CWLJsonParse::Software_InstallAndUnInstall_GetValue(__in std::string& sJson, __out std::vector<ST_SOFTWARE_ENTRY> &vcSoftwareEntry, __out tstring *pStrErr)
{
	/*
	由客户端返回的json获取信息
    单个Content信息
    "CMDContent":
    {
        "OperCmd":1, //enum WL_SOFTWARE_SUBCMDID
        "ParentPid":222,
        "FilePath":"C:\\xx.exe"
    }
	*/

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;
    Json::Value     root_1;
    Json::Reader	reader;
    std::string		strValue = "";

    if( sJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::Software_InstallAndUnInstall_GetValue, sJson.length() == 0")<<_T(",");
        goto _exist_;
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::Software_InstallAndUnInstall_GetValue, parse fail")<<_T(",");
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::Software_InstallAndUnInstall_GetValue, nObject < 1")<<_T(",");
        goto _exist_;
    }

    if( root[0].isMember("CMDContent"))
    {
        root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

        nObject = root_1.size();
        for (int i=0; i<nObject; i++)
        {
            std::wstring wsPath = UTF8ToUnicode(root_1[i]["FilePath"].asString());
            std::wstring wsCmdLine = UTF8ToUnicode(root_1[i]["CmdLine"].asString());
            ST_SOFTWARE_ENTRY stNewStruct;
            stNewStruct.sFilePath = wsPath;
            stNewStruct.sCmdLine = wsCmdLine;
            stNewStruct.dwOperCmd = root_1[i]["OperCmd"].asInt();
            stNewStruct.dwParentPID = root_1[i]["ParentPid"].asInt();
            vcSoftwareEntry.push_back(stNewStruct);
        }
    }

    bResult = TRUE;

_exist_:
    if (pStrErr)
    {
        *pStrErr = strTemp.str();
    }

    return bResult;
}

std::string CWLJsonParse::Software_InstallAndUnInstall_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in std::vector<ST_SOFTWARE_ENTRY> &vcSoftwareEntry)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    Json::Value root_1;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;
    Json::Value CMDContent;

    int iSize = (int)vcSoftwareEntry.size();
    for (int i=0; i<iSize; i++)
    {
        std::wstring wsFullPath = vcSoftwareEntry[i].sFilePath;
        std::wstring wsCmdLine = vcSoftwareEntry[i].sCmdLine;//此处用作命令行参数
        CMDContent["FilePath"]  = UnicodeToUTF8(wsFullPath);
        CMDContent["CmdLine"]  = UnicodeToUTF8(wsCmdLine);
        CMDContent["OperCmd"]   = (int)vcSoftwareEntry[i].dwOperCmd;
        CMDContent["ParentPid"] = (int)vcSoftwareEntry[i].dwParentPID;
        root_1.append(CMDContent);
    }

    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;

    person["CMDContent"] = Json::Value(root_1);

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

// 【白名单】- 文件摆渡 - 指定源文件/文件夹和目的路径
std::string CWLJsonParse::FileCopy_GetJson(const std::string & strTargetPath ,const std::list<std::string> &lstScanPath)
{
	Json::Value root;
	Json::FastWriter jsWriter;
	std::string strJson = "";

	root["target_path"] = strTargetPath;

	std::list<std::string>::const_iterator it = lstScanPath.begin();
	for (; it != lstScanPath.end(); ++it)
	{
		root["source_path"].append(*it);
	}

	strJson = jsWriter.write(root);

	return strJson;
}

// 得到上传速度配置的json格式串
BOOL CWLJsonParse::Upload_Speed_Config_GetJson(__in const tstring tstrComputerID,
										    __in const CMDTYPE nCmdType,
										    __in const WORD nCmdID, // = 45
										    __in const WORD nMode,
										    /*__in vecData,*/
										    __out string &strJson,
										    __out tstring *pStrError)
{
	BOOL bRes = FALSE;
	wostringstream  strTemp;

	Json::Value CMDContent;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	wstring wsRes;

	person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
	person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
	if (PLY_CLIENT_UPLOAD_SPEED != nCmdID) {
		strTemp << _T("CWLJsonParse::Upload_Speed_Config_GetJson CMDID is not matched, default CMDID:") << (int)PLY_CLIENT_UPLOAD_SPEED \
				<< _T(", Your entered CMDID:") << nCmdID;
		goto END;
	}
	person["CMDID"] = nCmdID;
	person["Enable"] = nMode;

#if 0 // 暂时没用到 - modified by jian.ding 2020.11.04
	for (int i = 0; i < vecData.size(); i++)
	{
		Json::Value	temp;
		temp["xxx"] = vecData[i].xxx;

		CMDContent.append(temp);
	}
    person["CMDContent"] = CMDContent;
#else
    person["CMDContent"] = Json::nullValue;
#endif
	root.append(person);

	strJson = writer.write(root);
	root.clear();
	bRes = TRUE;
END:
	if (pStrError)
	{
		*pStrError = strTemp.str();
	}
	return bRes;
}

// 解析配置上传速度的json字符串
BOOL CWLJsonParse::Upload_Speed_Config_ParseJson(__in const string &sJson,
											   __out WORD &nMode,
											   /*__out &vecData,*/
											   __out tstring *pStrError)
{
/*
{
	"CMDID":45,
	"CMDTYPE":150,
	"CMDVER":1,
	"Enable":1,
	"ComputerID":"EA14A453WIN-93J8ESTPGGR-威努特",
	"CMDContent":null}
*/
    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;

    Json::Reader	reader;
    std::string		strValue = "";

    if( sJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, sJson.length() == 0")<<_T(",");
        goto _exist_;
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, parse fail")<<_T(",");
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, nObject < 1")<<_T(",");
        goto _exist_;
    }

	if (root[0].isMember("Enable"))
	{
		nMode = root[0]["Enable"].asInt();
	}
#if 0 // 暂时没用到 - modified by jian.ding 2020.11.04
    if( root[0].isMember("CMDContent"))
    {
        Json::Value root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

        nObject = root_1.size();
        for (int i=0; i<nObject; i++)
        {
            std::wstring wsPath = UTF8ToUnicode(root_1[i]["FilePath"].asString());
            ST_SOFTWARE_ENTRY stNewStruct;
            stNewStruct.sFilePath = wsPath;
            stNewStruct.dwOperCmd = root_1[i]["OperCmd"].asInt();
            stNewStruct.dwParentPID = root_1[i]["ParentPid"].asInt();
            vevData.push_back(stNewStruct);
        }
    }
#endif
    bResult = TRUE;

_exist_:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}


// 得到白名单扫描速度配置的json格式串
BOOL CWLJsonParse::WhiteList_Scan_Speed_Config_GetJson(__in const tstring tstrComputerID,
										    __in const CMDTYPE nCmdType,
										    __in const WORD nCmdID, // = 45
										    __in const WORD nMode,
										    /*__in data,*/
										    __out string &strJson,
										    __out tstring *pStrError)
{
	BOOL bRes = FALSE;
	wostringstream  strTemp;

	Json::Value CMDContent;

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person;
	wstring wsRes;

	person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
	person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
	if (PLY_CLIENT_WHITELIST_SCAN_SPEED != nCmdID) {
		strTemp << _T("CWLJsonParse::WhiteList_Scan_Speed_Config_GetJson CMDID is not matched, default CMDID:") << (int)PLY_CLIENT_WHITELIST_SCAN_SPEED \
				<< _T(", Your entered CMDID:") << nCmdID;
		goto END;
	}
	person["CMDID"] = nCmdID;
	person["Enable"] = nMode;

#if 0 // 暂时没用到 - modified by jian.ding 2020.11.04
//	for (int i = 0; i < vecData.size(); i++)
//	{
//		Json::Value	temp;
//		temp["xxx"] = vecData[i].xxx;
//
//		CMDContent.append(temp);
//	}
#else
    person["CMDContent"] = Json::nullValue;
#endif

	root.append(person);

	strJson = writer.write(root);
	root.clear();
	bRes = TRUE;
END:
	if (pStrError)
	{
		*pStrError = strTemp.str();
	}
	return bRes;
}

// 解析白名单扫描速度配置上传速度的json字符串
BOOL CWLJsonParse::WhiteList_Scan_Speed_Config_ParseJson(__in const string &sJson,__out WORD &nMode, /*__out data,*/__out tstring *pStrError)
{
/*
{
	"CMDID":45,
	"CMDTYPE":150,
	"CMDVER":1,
	"Enable":1,
	"ComputerID":"EA14A453WIN-93J8ESTPGGR-威努特",
	"CMDContent":null}
*/
    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;

    Json::Reader	reader;
    std::string		strValue = "";

    if( sJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, sJson.length() == 0")<<_T(",");
        goto _exist_;
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, parse fail")<<_T(",");
        goto _exist_;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::Upload_Speed_Config_ParseJson, nObject < 1")<<_T(",");
        goto _exist_;
    }

	if (root[0].isMember("Enable"))
	{
		nMode = root[0]["Enable"].asInt();
	}
#if 0	// 接口中有CMDContent字段，但是暂时没有用，先注释	- modified by jian.ding 2020.11.04
    if( root[0].isMember("CMDContent"))
    {
        Json::Value root_1 =  (Json::Value)root[0]["CMDContent"]; //进行解析

        nObject = root_1.size();
        for (int i=0; i<nObject; i++)
        {
            std::wstring wsPath = UTF8ToUnicode(root_1[i]["FilePath"].asString());
            ST_SOFTWARE_ENTRY stNewStruct;
            stNewStruct.sFilePath = wsPath;
            stNewStruct.dwOperCmd = root_1[i]["OperCmd"].asInt();
            stNewStruct.dwParentPID = root_1[i]["ParentPid"].asInt();
            vevData.push_back(stNewStruct);
        }
    }
#endif
    bResult = TRUE;

_exist_:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}



//安全商店日志上传USM (1)：
std::string CWLJsonParse::SafetyStoreLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<TRACEINSTALL_LOG_STRUCT *>& vecTraceLog)
{
    std::string sJsonPacket = "";
    //std::string sJsonBody = "";
	Json::Value Content;

    int nCount = (int)vecTraceLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;


    for (int i=0; i< nCount; i++)
    {
        TRACEINSTALL_LOG_STRUCT *pTraceLog = vecTraceLog[i] ;

        std::wstring wsTemp;

        //userName
        wsTemp = pTraceLog->szUser;
        person["userName"] = UnicodeToUTF8(wsTemp);

        //StartTime
        wsTemp = convertTimeTToStr((DWORD)pTraceLog->llStartTime);
        person["StartTime"] = UnicodeToUTF8(wsTemp);

        //EndTime
        wsTemp = convertTimeTToStr((DWORD)pTraceLog->llEndTime);
        person["EndTime"] = UnicodeToUTF8(wsTemp);

        //TraceType
        person["TraceType"] = (int)pTraceLog->dwTraceType;

        //installPackageWLCode
        wsTemp = pTraceLog->szInstallPackageMD5;
        person["installPackageWLCode"] = UnicodeToUTF8(wsTemp);

        //installPackageNameConfiged
        wsTemp = pTraceLog->szInstallPackageNameConfiged;
        person["installPackageNameConfiged"] = UnicodeToUTF8(wsTemp);

        //installSourcePath（暂时不需要）
        //wsTemp = pTraceLog->szInstallSourcePath;
        //person["installSourcePath"] = UnicodeToUTF8(wsTemp);

        //InstallorUnisntallState
        person["InstallorUnisntallState"] = (int)pTraceLog->dwInstallorUnisntallState;

        //stat
        person["stat"] = (int)pTraceLog->dwStat;

        //Version
        wsTemp = pTraceLog->szVersion;
        person["Version"] = UnicodeToUTF8(wsTemp);

        //installDir
        wsTemp = pTraceLog->szInstallDir;
        person["installDir"] = UnicodeToUTF8(wsTemp);

        //DisplayIcon
        wsTemp = pTraceLog->szDisplayIcon;
        person["DisplayIcon"] = UnicodeToUTF8(wsTemp);

        //Publisher
        wsTemp = pTraceLog->szPublisher;
        person["Publisher"] = UnicodeToUTF8(wsTemp);

        Content.append(person);
        person.clear();
    }


    //sJsonBody = writer.write(root);
    //root.clear();

    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;
    person["CMDContent"] = Content;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();


    //wsJsonPacket = ConvertA2W(sJsonPacket);
    //sJsonPacket = UnicodeToUTF8(ws);
    //return sJsonPacket;

    //std::wstring wsJsonPacket = _T("");
    //wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

    return sJsonPacket;
}

//安全商店日志上传USM (2)：
std::string CWLJsonParse::SafetyStoreLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData *>& vecTraceLog)
{
    std::string sJsonPacket = "";
    //std::string sJsonBody = "";
	Json::Value Content;

    int nCount = (int)vecTraceLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;


    for (int i=0; i< nCount; i++)
    {
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecTraceLog[i]->GetData();
        TRACEINSTALL_LOG_STRUCT *pTraceLog = (TRACEINSTALL_LOG_STRUCT*)pipclogcomm->data;

        std::wstring wsTemp;

        //userName
        wsTemp = pTraceLog->szUser;
        person["userName"] = UnicodeToUTF8(wsTemp);

        //StartTime
        wsTemp = convertTimeTToStr((DWORD)pTraceLog->llStartTime);
        person["StartTime"] = UnicodeToUTF8(wsTemp);

        //EndTime
        wsTemp = convertTimeTToStr((DWORD)pTraceLog->llEndTime);
        person["EndTime"] = UnicodeToUTF8(wsTemp);

        //TraceType
        person["TraceType"] = (int)pTraceLog->dwTraceType;

        //installPackageWLCode
        wsTemp = pTraceLog->szInstallPackageMD5;
        person["installPackageWLCode"] = UnicodeToUTF8(wsTemp);

        //installPackageNameConfiged
        wsTemp = pTraceLog->szInstallPackageNameConfiged;
        person["installPackageNameConfiged"] = UnicodeToUTF8(wsTemp);

        //installSourcePath（暂时不需要）
        //wsTemp = pTraceLog->szInstallSourcePath;
        //person["installSourcePath"] = UnicodeToUTF8(wsTemp);

        //InstallorUnisntallState
        person["InstallorUnisntallState"] = (int)pTraceLog->dwInstallorUnisntallState;

        //stat
        person["stat"] = (int)pTraceLog->dwStat;

        //Version
        wsTemp = pTraceLog->szVersion;
        person["Version"] = UnicodeToUTF8(wsTemp);

        //installDir
        wsTemp = pTraceLog->szInstallDir;
        person["installDir"] = UnicodeToUTF8(wsTemp);

        //DisplayIcon
        wsTemp = pTraceLog->szDisplayIcon;
        person["DisplayIcon"] = UnicodeToUTF8(wsTemp);

        //Publisher
        wsTemp = pTraceLog->szPublisher;
        person["Publisher"] = UnicodeToUTF8(wsTemp);

        Content.append(person);
        person.clear();
    }


   // sJsonBody = writer.write(root);
  //  root.clear();


    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    person["CMDID"] = (int)cmdID;
    person["CMDContent"] = (Json::Value)Content;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();


    //wsJsonPacket = ConvertA2W(sJsonPacket);
    //sJsonPacket = UnicodeToUTF8(ws);
    //return sJsonPacket;

    //std::wstring wsJsonPacket = _T("");
    //wsJsonPacket =  UTF8ToUnicode(sJsonPacket);

    return sJsonPacket;
}
std::string CWLJsonParse::SafetyStoreSelfupdateConfig_GetJsonByVector(__in tstring ComputerID,  __in VECT_SETUP_CFG_ST& vecSelfUpdateConfig)
{
    std::string sJsonPacket = "";
    //std::string sJsonBody = "";
	Json::Value Content;

    int nCount = (int)vecSelfUpdateConfig.size();
    if( nCount == 0)
        return sJsonPacket;



    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    for (int i=0; i< nCount; i++)
    {
        APPSTORE_SETUP_ST &Item =   vecSelfUpdateConfig[i];
        std::wstring wsTemp;
        //fileName
        wsTemp = Item.wszSetupAppName;
        person["fileName"] = UnicodeToUTF8(wsTemp);

        //wlcode
        char HashCode[INTEGRITY_LENGTH*2+1] = {0};
        Bin2Hex(Item.ucsWlCode,INTEGRITY_LENGTH,HashCode);
        person["WlCode"] = HashCode;


        TI_TIME & tit=Item.SelfUpdateSofteInsertTime;
        char csStr[256]={0};
        sprintf(csStr,"%4d-%02d-%02d %02d:%02d:%02d",tit.Year,tit.Month,tit.Day,tit.Hour,tit.Minute,tit.Second);
        person["time"] = csStr;
        Content.append(person);
        person.clear();
    }
    //sJsonBody = writer.write(root);
    //root.clear();
    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)150;
    person["CMDID"] = (int)27;
    person["CMDContent"] = (Json::Value)Content;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();


    return sJsonPacket;
}

std::string CWLJsonParse::SafetyStoreTraceInstall_GetJson(__in tstring ComputerID,  __in VECT_SETUP_ST& vecSetupInfo)
{
    std::string sJsonPacket = "";
    //std::string sJsonBody = "";
	Json::Value Content;

    int nCount = (int)vecSetupInfo.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    for (int i=0; i< nCount; i++)
    {
        APPSTORE_SETUP_ST &Item =   vecSetupInfo[i];
        std::wstring wsTemp;
        //fileName
        wsTemp = Item.wszSetupAppName;
        person["fileName"] = UnicodeToUTF8(wsTemp);

        //wlcode
        char HashCode[INTEGRITY_LENGTH*2+1] = {0};
        Bin2Hex(Item.ucsWlCode,INTEGRITY_LENGTH,HashCode);
        person["WlCode"] = HashCode;
        //hash
        char md5Code[MD5_LENGTH*2+1] = {0};
        Bin2Hex(Item.ucHashCode,MD5_LENGTH,md5Code);
        person["MD5"] = md5Code;

{
        TI_TIME & tit=Item.stStartTime;
        char csStr[256]={0};
        sprintf(csStr,"%4d-%02d-%02d %02d:%02d:%02d",tit.Year,tit.Month,tit.Day,tit.Hour,tit.Minute,tit.Second);
        person["StartTime"] = csStr;

}

{
        TI_TIME & tit=Item.stEndTime;
        char csStr[256]={0};
        sprintf(csStr,"%4d-%02d-%02d %02d:%02d:%02d",tit.Year,tit.Month,tit.Day,tit.Hour,tit.Minute,tit.Second);
        person["EndTime"] = csStr;

}
        Content.append(person);
        person.clear();
    }
    //sJsonBody = writer.write(root);
    //root.clear();
#if 0
    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)150;
    person["CMDID"] = (int)27;

#endif
    person["CMDContent"] = (Json::Value)Content;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}


std::string CWLJsonParse::AppStore_Result_GetJson( __in ST_APPSTORE_RESULT &stRet)
{

    /*
        json格式：

        //软件安装与卸载：dlfender返回客户端信息
        {
            "AppStoreResult":
            {
                "Reason": "xxxx",
                "WLFileCount": 10
                "AppPackageName":"xx.exe"
            },

        }
    */

    std::string sJsonPacket = "";
    Json::FastWriter writer;

	Json::Value Condition;
	Json::Value root;

    Condition["WLFileCount"] = (int)stRet.ulWlFileCount;

    Condition["Reason"] = UnicodeToUTF8(stRet.sReason);
    Condition["AppPackageName"] = UnicodeToUTF8(stRet.AppPackageName);
    root["AppStoreResult"] = Condition;



    writer.omitEndingLineFeed();
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

BOOL CWLJsonParse::AppStore_Result_GetValue(__in std::string& sJson, __out ST_APPSTORE_RESULT &stRet)
{

   /*
        json格式：

        //软件安装与卸载：获取
        {
            "AppStoreResult":
            {
                "Result": 0,      // 0 成功   1失败 不同的失败结果定义不同的错误码
                "Reason": "xxxx",
                "DirWhiteListCount": 2
            }
        }


    */

    std::string     strValue = sJson;
    Json::Value Condition;
	Json::Value root;
    Json::Reader    reader;


    if (!reader.parse(strValue, root))
    {
        return FALSE;
    }

    if (root.isMember("AppStoreResult"))
    {
        Condition = root["AppStoreResult"];

        stRet.sReason = UTF8ToUnicode(Condition["Reason"].asString());
        stRet.ulWlFileCount = (ULONG)Condition["WLFileCount"].asInt();
        stRet.AppPackageName = UTF8ToUnicode(Condition["AppPackageName"].asString());
    }



    return TRUE;
}

BOOL CWLJsonParse::ConvertFwPly2DefenderJson(_In_ const IWLConfig_FWSec::FWSec &stcFWSec, _Out_ string &strJson, _Out_ tstring *pStrErr)
{
    return GetFwPlyJson4Defender(stcFWSec, strJson, pStrErr);
}

BOOL CWLJsonParse::GetFwPlyJson4Defender(_In_ const IWLConfig_FWSec::FWSec &stcFWSec, _Out_ string &strJson, _Out_ tstring *pStrErr)
{
	BOOL bRes = FALSE;
	string strJsonTemp;
	tstring strErr;
	wostringstream  strTemp;
	Json::Reader	reader;
	Json::Value jsonRoot;
	Json::FastWriter writer;


	if (!GetFwPlyJson4USM(stcFWSec, FALSE, _T("LOCAL"), strJsonTemp, &strErr))
	{
		strTemp << _T("GetFwPlyJson4USM fail, err=") << strErr.c_str() << _T(",");
		goto END;
	}

	//解析成JSON，增加命令号
	if (!reader.parse(strJsonTemp.c_str(), jsonRoot))
	{
		//WriteError(_T("reader.parse failed, sJson=%S"), pJson);
		strTemp << _T("reader.parse failed, sJson=")<<CStrUtil::UTF8ToUnicode(strJsonTemp).c_str() << _T(",");
		goto END;
	}
	

	jsonRoot[PLY_FW_STRKEY_CMDID] = PLY_CLIENT_BASELINE_IPSEC;
	jsonRoot[PLY_FW_STRKEY_CMDTYPE] = CMDTYPE_POLICY;
	jsonRoot[PLY_FW_STRKEY_COMPUTER_ID] = _T("LOCAL");

	strJson = writer.write(jsonRoot);

	bRes = TRUE;
END:

	if (pStrErr)
	{
		*pStrErr = strTemp.str();
	}
	return bRes;
}

BOOL CWLJsonParse::GetFwPlyJson4USM(_In_ const IWLConfig_FWSec::FWSec &stcFWSec,_In_ BOOL bIsXp, _In_ tstring strComputerID, _Out_ string &strJson, _Out_ tstring *pStrErr)
{
		/*
	示例
	{
	"FwItems": [
					{
					"RuleName": "WinMD5.exe",
					"FullPath": "C:\\WinMD5.exe"
					},
					{

					"RuleName": "WinMD5.exe",
					"FullPath": "C:\\WinMD5.exe"，
					"LocalIP": "192.168.1.235",
					"RemoteIP": "192.168.1.35",
					"LocalPort": "22",
					"RemotePort": "33",
					"ProtocalType": 256,
					"Operation": 1,
					"Direction": 1
					}
				],
	"CMDID": 162,
	"CMDTYPE": 150,
	"ComputerID": "dettfdswe",
	"FireWall": 1,
	"FireWallState": 1,
	"SynDefence": 1
	}
	*/
	BOOL bRes = FALSE;

	Json::FastWriter writer;
	Json::Value jsonRoot;
	Json::Value FwItemArray;

	
	jsonRoot[PLY_FW_STRKEY_FIREWALL] = (DWORD32)stcFWSec.emTakeOverState;
	jsonRoot[PLY_FW_STRKEY_FIREWALLSTATE] = (DWORD32)stcFWSec.emFwState;

#if NET_SYN_SWITCH
	jsonRoot[PLY_FW_STRKEY_SYNIPDEFENCE] = (DWORD32)stcFWSec.emSynState;  //从V300R003C01版本开始SYN移动到安全基线中第49项，保留该字段用于升级读取配置文件）
#endif
    jsonRoot[PLY_FW_STRKEY_SYNIPDEFENCE] = (DWORD32)IWLConfig_FWSec::em_FwSyn_Off;  //默认构造值为0

	jsonRoot[PLY_FW_STRKEY_DEFAULT_IN] = (DWORD32)stcFWSec.emDefauleIn;
	jsonRoot[PLY_FW_STRKEY_DEFAULT_OUT] = (DWORD32)stcFWSec.emDefauleOut;
	jsonRoot[PLY_FW_STRKEY_COMPUTER_ID] = CStrUtil::UnicodeToUTF8(strComputerID).c_str();

	//只获取用户定义的规则(vecFwItems)
	for(int i = 0; i < stcFWSec.vecFwItems.size(); i++)
	{
		IWLConfig_FWSec::FWItem stcTemp = stcFWSec.vecFwItems[i];
		Json::Value FwItem;

		FwItem[PLY_FW_STRKEY_ITEM_RULENAME] = CStrUtil::UnicodeToUTF8(stcTemp.szItemName).c_str();
		FwItem[PLY_FW_STRKEY_ITEM_OPERATION] = (DWORD32)stcTemp.emOperation;
		FwItem[PLY_FW_STRKEY_ITEM_DIRECTION] = (DWORD32)stcTemp.emDirection;

		if (stcTemp.IsAppValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_FULLPATH] = CStrUtil::UnicodeToUTF8(stcTemp.szItemPath).c_str();
		}

		if (stcTemp.IsLocalIPValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_LOCALIP] = CStrUtil::UnicodeToUTF8(stcTemp.szLocalIP).c_str();
		}

		if (stcTemp.IsRemoteIPValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_REMOTEIP] = CStrUtil::UnicodeToUTF8(stcTemp.szRemoteIP).c_str();
		}

		if (stcTemp.IsLocalPortValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_LOCALPORT] = CStrUtil::UnicodeToUTF8(stcTemp.szLocalPort).c_str();
		}

		if (stcTemp.IsRemotePortValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_REMOTEPORT] = CStrUtil::UnicodeToUTF8(stcTemp.szRemotePort).c_str();
		}

		if (stcTemp.IsProtocolValid())
		{
			FwItem[PLY_FW_STRKEY_ITEM_PROTOCALTYPE] = (DWORD32)stcTemp.dwProtacal;
		}

		if (bIsXp)//为了减少USM的判断逻辑增加了该字段
		{
			if (stcTemp.IsAppValid())
			{
				FwItem[PLY_FW_STRKEY_ITEM_TYPE_4_XP] = PLY_FW_STRKEY_ITEM_TYPE_4_XP_APP;
			}
			else if (stcTemp.IsLocalPortValid())
			{
				FwItem[PLY_FW_STRKEY_ITEM_TYPE_4_XP] = PLY_FW_STRKEY_ITEM_TYPE_4_XP_PORT;
			}
			
		}


		FwItemArray.append(FwItem);
	}

	jsonRoot[PLY_FW_STRKEY_ARRAY_FWITEMS] = (Json::Value)FwItemArray;
	
	strJson = writer.write(jsonRoot);

	bRes = TRUE;
	return bRes;
}


BOOL CWLJsonParse::ParseFwPlyJson(_In_ const  char *pJson, _Out_ IWLConfig_FWSec::FWSec &stcFWSec, _Out_ tstring *pStrErr)
{
	/*
	示例
	{
	"FwItems": [
					{
					"RuleName": "WinMD5.exe",
					"FullPath": "C:\\WinMD5.exe"
					},
					{

					"RuleName": "WinMD5.exe",
					"FullPath": "C:\\WinMD5.exe"，
					"LocalIP": "192.168.1.235",
					"RemoteIP": "192.168.1.35",
					"LocalPort": "22",
					"RemotePort": "33",
					"ProtocalType": 256,
					"Operation": 1,
					"Direction": 1
					}
				],
	"CMDID": 162,
	"CMDTYPE": 150,
	"ComputerID": "dettfdswe",
	"FireWall": 1,
	"FireWallState": 1,
	"SynDefence": 1
	}
	*/
	BOOL bRes = FALSE;
	Json::Reader	reader;
	Json::Value jsonRoot;
	wostringstream  strTemp;

	if (!pJson)
	{
		strTemp << _T("pJson == null") << _T(",") ;
		goto END;
	}

	//解析命令
	if (!reader.parse(pJson, jsonRoot))
	{
		//WriteError(_T("reader.parse failed, sJson=%S"), pJson);
		strTemp << _T("reader.parse failed, sJson=")<<CStrUtil::UTF8ToUnicode(pJson).c_str() << _T(",");
		goto END;
	}

	if(!jsonRoot.isMember(PLY_FW_STRKEY_FIREWALL))
	{
		strTemp << _T("invalid json, no member FireWall, sJson=")<<CStrUtil::UTF8ToUnicode(pJson).c_str() << _T(",");
		goto END;
	}
	stcFWSec.emTakeOverState = (IWLConfig_FWSec::EM_FW_TAKE_OVER_STATE)jsonRoot[PLY_FW_STRKEY_FIREWALL].asUInt();	

#if NET_SYN_SWITCH
	if (!jsonRoot.isMember(PLY_FW_STRKEY_SYNIPDEFENCE))
	{
		strTemp << _T("invalid json, no member SynIpDefence")<<CStrUtil::UTF8ToUnicode(pJson).c_str() << _T(",");
		goto END;
	}
	stcFWSec.emSynState = (IWLConfig_FWSec::EM_FW_SYN)jsonRoot[PLY_FW_STRKEY_SYNIPDEFENCE].asUInt();
#endif
    stcFWSec.emSynState = IWLConfig_FWSec::em_FwSyn_Off;  //默认赋值为0

	if (jsonRoot.isMember(PLY_FW_STRKEY_DEFAULT_IN))
	{
		stcFWSec.emDefauleIn = (IWLConfig_FWSec::EM_FW_OPERATION)jsonRoot[PLY_FW_STRKEY_DEFAULT_IN].asUInt();
	}


	if (jsonRoot.isMember(PLY_FW_STRKEY_DEFAULT_OUT))
	{
		stcFWSec.emDefauleOut = (IWLConfig_FWSec::EM_FW_OPERATION)jsonRoot[PLY_FW_STRKEY_DEFAULT_OUT].asUInt();
	}

	//只用于USM下发的JSON
	if (jsonRoot.isMember(PLY_FW_STRKEY_CONFIGTYPE))
	{
		stcFWSec.emFwConfigType = (IWLConfig_FWSec::EM_FW_CONFIG_TYPE)jsonRoot[PLY_FW_STRKEY_CONFIGTYPE].asUInt();
	}
		

	stcFWSec.vecFwItems.clear();
	stcFWSec.vecWntFwItems.clear();

	if (jsonRoot.isMember(PLY_FW_STRKEY_ARRAY_FWITEMS))
	{
		Json::Value jsonArray = jsonRoot[PLY_FW_STRKEY_ARRAY_FWITEMS];
		if(jsonArray.isArray())
		{
			for (int i = 0; i < jsonArray.size(); i++)
			{
				Json::Value jsonItem = jsonArray[i];
				IWLConfig_FWSec::FWItem stcItem;
				tstring strTemp;

				//RuleName
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_RULENAME))
				{
					stcItem.szItemName[_countof(stcItem.szItemName)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_RULENAME].asString());
					_tcsncpy(stcItem.szItemName, strTemp.c_str(), _countof(stcItem.szItemName)  - 1);
				}

				//FullPath
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_FULLPATH))
				{
					stcItem.szItemPath[_countof(stcItem.szItemPath)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_FULLPATH].asString());
					_tcsncpy(stcItem.szItemPath, strTemp.c_str(), _countof(stcItem.szItemPath)  - 1);
					stcItem.SetAppValid();
				}

				//LocalIP
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_LOCALIP))
				{
					stcItem.szLocalIP[_countof(stcItem.szLocalIP)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_LOCALIP].asString());
					_tcsncpy(stcItem.szLocalIP, strTemp.c_str(), _countof(stcItem.szLocalIP)  - 1);
					stcItem.SetLocalIPValid();
				}

				//RemoteIP
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_REMOTEIP))
				{
					stcItem.szRemoteIP[_countof(stcItem.szRemoteIP)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_REMOTEIP].asString());
					_tcsncpy(stcItem.szRemoteIP, strTemp.c_str(), _countof(stcItem.szRemoteIP)  - 1);
					stcItem.SetRemoteIPValid();
				}

				//LocalPort
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_LOCALPORT))
				{
					stcItem.szLocalPort[_countof(stcItem.szLocalPort)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_LOCALPORT].asString());
					_tcsncpy(stcItem.szLocalPort, strTemp.c_str(), _countof(stcItem.szLocalPort)  - 1);
					stcItem.SetLocalPortValid();
				}

				//RemotePort
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_REMOTEPORT))
				{
					stcItem.szRemotePort[_countof(stcItem.szRemotePort)  - 1] = 0;
					strTemp = CStrUtil::UTF8ToUnicode(jsonItem[PLY_FW_STRKEY_ITEM_REMOTEPORT].asString());
					_tcsncpy(stcItem.szRemotePort, strTemp.c_str(), _countof(stcItem.szRemotePort)  - 1);
					stcItem.SetRemotePortValid();
				}

				//ProtocalType
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_PROTOCALTYPE))
				{
					stcItem.dwProtacal = jsonItem[PLY_FW_STRKEY_ITEM_PROTOCALTYPE].asUInt();
					stcItem.SetProtocolValid();
				}

				//Operation
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_OPERATION))
				{
					stcItem.emOperation = (IWLConfig_FWSec::EM_FW_OPERATION)jsonItem[PLY_FW_STRKEY_ITEM_OPERATION].asUInt();
				}

				//Direction
				if (jsonItem.isMember(PLY_FW_STRKEY_ITEM_DIRECTION))
				{
					stcItem.emDirection = (IWLConfig_FWSec::EM_FW_DIRECTION)jsonItem[PLY_FW_STRKEY_ITEM_DIRECTION].asUInt();
				}

				stcFWSec.vecFwItems.push_back(stcItem);
			}
		}
		
	}

	bRes = TRUE;

END:

	if (pStrErr)
	{
		 *pStrErr = strTemp.str();
	}
	return bRes;
}

std::string CWLJsonParse::DeviceControl_NoReg_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const WORD nAuthority, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Authority":7,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bRes = FALSE;
    std::string strJson;

    Json::Value CMDContent;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
    person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
    person["CMDID"]         = nCmdID;
    person["Authority"]     = nAuthority;
    person["CMDContent"]    = Json::nullValue;

    root.append(person);

    strJson = writer.write(root);
    root.clear();
    bRes = TRUE;

    return strJson;
}

BOOL CWLJsonParse::DeviceControl_NoReg_GetValue(__in const string &strJson, __out WORD &nAuthority, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Authority":7,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;

    Json::Reader	reader;
    std::string		strValue = "";

    if( strJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::DeviceControl_NoReg_GetValue, sJson.length() == 0") << _T(",");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::DeviceControl_NoReg_GetValue, parse fail")<<_T(",");
        goto END;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::DeviceControl_NoReg_GetValue, nObject < 1")<<_T(",");
        goto END;
    }

    if (root[0].isMember("Authority"))
    {
        nAuthority = root[0]["Authority"].asInt();
    }

    bResult = TRUE;

END:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}

std::string CWLJsonParse::DeviceControl_UDisk_KillVirus_Sign_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const WORD nSign, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Sign":0,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bRes = FALSE;
    std::string strJson;

    Json::Value CMDContent;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
    person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
    person["CMDID"]         = nCmdID;
    person["Sign"]          = nSign;
    person["CMDContent"]    = Json::nullValue;

    root.append(person);

    strJson = writer.write(root);
    root.clear();
    bRes = TRUE;

    return strJson;
}

BOOL CWLJsonParse::DeviceControl_UDisk_KillVirus_Sign_GetValue(__in const string &strJson, __out WORD &nSign, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Sign":0,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;

    Json::Reader	reader;
    std::string		strValue = "";

    if( strJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::DeviceControl_SafeUDisk_KillVirus_Sign_GetValue, sJson.length() == 0") << _T(",");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::DeviceControl_SafeUDisk_KillVirus_Sign_GetValue, parse fail")<<_T(",");
        goto END;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::DeviceControl_SafeUDisk_KillVirus_Sign_GetValue, nObject < 1")<<_T(",");
        goto END;
    }

    if (root[0].isMember("Sign"))
    {
        nSign = root[0]["Sign"].asInt();
    }

    bResult = TRUE;

END:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}

//新V3.0 安全U盘重置写入软件操作（暂时只用于本地）
std::string CWLJsonParse::DeviceControl_ResetPwd_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const VEC_RESET_PWD vecResets, __in std::wstring wsPassword,  __in std::wstring wsUserName, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Password":"123456",
        "User":"admin:,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":
        [{
            "Drive":["E","F"],
            "Serial":"XXXXXXXXXXX"
        }]
        
    }
    */

    BOOL bRes = FALSE;
    std::string strJson = "";

    std::wstring wsDrive;

    Json::Value CMDContent;
    Json::Value root;
    Json::Value Item;
    Json::FastWriter writer;
    Json::Value person;

    person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
    person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
    person["CMDID"]         = nCmdID;
    person["Password"]      = UnicodeToUTF8(wsPassword);
    person["User"]          = UnicodeToUTF8(wsUserName);

    int iSize = (int)vecResets.size();
    if ( iSize == 0)
    {
        return strJson;
    }

    for (int i=0; i<iSize; i++)
    {
        Json::Value ResetPwdInfo;

        RESET_PASSWORD_ST stReset;
        stReset = vecResets[i];

        //序列号
        ResetPwdInfo["Serial"] = UnicodeToUTF8(stReset.szSerial);

        //U盘盘符
        if ( wcslen(stReset.szDrive) != 0)
        {
            for (int k=0; k < wcslen(stReset.szDrive); k++)
            {
                WCHAR szTemp[2] = {0};
                szTemp[0] = stReset.szDrive[k];
                ResetPwdInfo["Drive"].append( UnicodeToUTF8(szTemp));
            }
        }
        else
        {
            std::wstring wsDrive = _T("");
            ResetPwdInfo["Drive"].append( UnicodeToUTF8( wsDrive));
        }

        CMDContent.append(ResetPwdInfo);
    }

    person["CMDContent"] = (Json::Value)CMDContent;

    root.append(person);

    strJson = writer.write(root);
    root.clear();
    bRes = TRUE;

    return strJson;
}

BOOL CWLJsonParse::DeviceControl_ResetPwd_GetValue(__in const string &strJson, __out VEC_RESET_PWD &vecResets, __out std::wstring &wsPassword, __out std::wstring &wsUserName, __out tstring *pStrError /* = NULL */)
{
    /*
    {
	    "CMDID":102\103,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "Password":"123456",
        "User":"admin:,
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
        "CMDContent":
        [{
            "Drive":["E","F"],
            "Serial":"XXXXXXXXXXX"
        }]
    }
    */

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;
    Json::Value     Item;
    Json::Value     CMDContent;
    
    Json::Reader	reader;
    std::string		strValue = "";


    if( strJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::DeviceControl_ResetPwd_GetValue, sJson.length() == 0") << _T(",");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::DeviceControl_ResetPwd_GetValue, parse fail")<<_T(",");
        goto END;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::DeviceControl_ResetPwd_GetValue, nObject < 1")<<_T(",");
        goto END;
    }

    if (root[0].isMember("Password"))
    {
        wsPassword = UTF8ToUnicode(root[0]["Password"].asString());
    }

    if (root[0].isMember("User"))
    {
        wsUserName = UTF8ToUnicode(root[0]["User"].asString());
    }

    if ( !root[0].isMember("CMDContent"))
    {
        strTemp << _T("CWLJsonParse::DeviceControl_ResetPwd_GetValue,  no member CMDContent ")<<_T(",");
        goto END;
    }
    else
    {
        CMDContent = (Json::Value)root[0]["CMDContent"];
        int iNum = CMDContent.size();
        std::wstring wsDrive;
        for (int i=0; i < iNum; i++)
        {
            RESET_PASSWORD_ST   stReset;

            //序列号
            StrCpyW(stReset.szSerial, UTF8ToUnicode(CMDContent[i]["Serial"].asString()).c_str());

            //盘符
            Json::Value JsonDrive;
            JsonDrive = (Json::Value)CMDContent[i]["Drive"];
            int iCounts = JsonDrive.size();
            if ( iCounts > 0 && JsonDrive.isArray())
            {
                for (int k = 0; k < iCounts; k++)
                {
                    std::wstring wsDrive = UTF8ToUnicode(JsonDrive[k].asString());
                    //wsDrive.copy(&stReset.szDrive[k], 1, k);
                    stReset.szDrive[k] = wsDrive[0];
                }
            }
            else
            {
                memset(stReset.szDrive, 0, sizeof(stReset.szDrive));
            }

            vecResets.push_back(stReset);
        }
    }

    bResult = TRUE;

END:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}

//主机卫士相关注册U盘Json构建和解析
std::string CWLJsonParse::DeviceControl_RegUDiskInfo_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const DEVICE_CONTROL_REG_UDISK_STATUS_ST stStatus, __in const VEC_ALL_UDISK_ST vecRegUDiskInfo, __out tstring *pStrError /* = NULL */)
{
    
    /*
    [
        {
            "CMDContent":
            [
                {
                    "DiskDriverLetter":["E:\\", "F:\\"],
                    "ComKillVirusSign":1,
                    "PubliceAreaKillVirusSign":0,
                    "RegAuthority":1,
                    "UDiskType":1,
                    "USBClass":8,
                    "description":"威努特",
                    "plugState":1,
                    "registerStatus":1,
                    "secureAreaKillVirusSign":0,
                    "serialID":"90123123909011",
                    "time":"2021-03-01 12:22:31",
                    "type":1
                },
                {
                    "DiskDriverLetter":["G:\\", "H:\\"],
                    "ComKillVirusSign":0,
                    "PubliceAreaKillVirusSign":1,
                    "RegAuthority":5,
                    "UDiskType":3,
                    "USBClass":8,
                    "description":"威努特 V3.0",
                    "plugState":1,
                    "registerStatus":0,
                    "secureAreaKillVirusSign":0,
                    "serialID":"45ED3412312ED12C",
                    "time":"2021-03-02 14:21:12",
                    "type":1
                }
            ],
            "CMDTYPE":150,
            "ComputerID":"LOCAL",
            "KillVirusSign":0,
            "NoRegComUSB":7,
            "NoRegSafeUSB":7,
            "blueTooth":0,
            "infrared":0,
            "nCmdID":120,
            "usbSwitch":1,
            "cdrom":1,
            "wireless":1，
            "serialPort":1,
            "parallelPort":1,
            "UsbStorageCtrl":1
        }
    ]
    */

    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;
    

    wostringstream  strFormat;

    int iSize = (int)vecRegUDiskInfo.size();

    //参数检查
    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;    /*117、118、119、120*/

    if ( nCmdID == PLY_USM_CONFIG_DEVICE_CONTROL_INFO)
    {
        //开关状态
        Item["cdrom"] = (DWORD32)stStatus.dwCdromStatus;                         //CDROM
        Item["wireless"] = (DWORD32)stStatus.dwWlanStatus;                       //无线网卡
        Item["usbEthernet"] = (DWORD32)stStatus.dwUsbEthernetStatus;             //USB以太网卡
        Item["blueTooth"] = (DWORD32)stStatus.dwBlueToothStatus;                 //蓝牙状态
        Item["serialPort"] = (DWORD32)stStatus.dwSerialPortStatus;               //串口状态
        Item["parallelPort"] = (DWORD32)stStatus.dwParallelPortStatus;           //并口状态
        Item["floppyDisk"] = (DWORD32)stStatus.dwFloppyDiskStatus;               //软盘状态
        Item["usbDevice"] = (DWORD32)stStatus.dwUsbPortStatus;					 //USB设备
        Item["wpd"] = (DWORD32)stStatus.dwWpdStatus;							 //手机平板
        Item["WifiWhiteList"] = (DWORD32)stStatus.dwWifiDeviceStatus;               //WiFi白名单

        Item["UsbStorageCtrl"] = (DWORD32)stStatus.dwUsbStorageCtrlStatus;       //USB总开关状态

        Item["usbSwitch"] = (DWORD32)stStatus.dwRegisterUDiskStatus;             //注册U盘开关状态
        Item["NoRegComUSB"] = (DWORD32)stStatus.dwUnregisteredCommonUDiskStatus; //非注册普通U盘权限
        Item["NoRegSafeUSB"] = (DWORD32)stStatus.dwUnregisteredSafeUDiskStatus;  //非注册安全U盘状态
        //Item["KillVirusSign"] = (DWORD32)stStatus.dwUDiskKillVirusSign;        //杀毒标志
        
        //普通U盘杀毒标记
        if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_COMMON_UDISK) == UDISK_ANTI_AV_FLAG_COMMON_UDISK)
        {
            Item["KillVirusAreaofComUSB"] = UDISK_ANTI_AV_FLAG_COMMON_UDISK;
        }
        else
        {
            Item["KillVirusAreaofComUSB"] = UDISK_ANTI_AV_FLAG_NO;
        }

        //安全U盘杀毒标记
        if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == UDISK_ANTI_AV_FLAG_SAFE_PUBLIC 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == UDISK_ANTI_AV_FLAG_SAFE_SECURE)
        {
            Item["KillVirusAreaofSafeUSB"] = 3;
        }
        else if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == 0 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == UDISK_ANTI_AV_FLAG_SAFE_SECURE)
        {
            Item["KillVirusAreaofSafeUSB"] = 2;
        }
        else if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == UDISK_ANTI_AV_FLAG_SAFE_PUBLIC 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == 0)
        {
            Item["KillVirusAreaofSafeUSB"] = 1;
        }
        else
        {
            Item["KillVirusAreaofSafeUSB"] = 0;
        }

    }

    
    for (int i=0; i<iSize; i++)
    {
        Json::Value RegUDiskInfo;

        Json::Value JsonDriveLetter;
        Json::Value JsonSecureDisk;

        REG_UDISK_ST stInfo;
        stInfo = vecRegUDiskInfo[i];

        RegUDiskInfo["type"] = 1;       //固定不变（旧版Linux的Json需要该字段，保留，下同）
        RegUDiskInfo["USBClass"] = 8;   //固定不变

        //插拔状态
        RegUDiskInfo["plugState"] = (int)stInfo.bInsert;   //1 - 插入 0 - 拔出
        
        //序列号
        RegUDiskInfo["serialID"] = UnicodeToUTF8(stInfo.szSerial);

        //注册状态
        if ( stInfo.bRegister == TRUE)
        {
            RegUDiskInfo["registerStatus"] = 1;//已注册
        }
        else
        {
            RegUDiskInfo["registerStatus"] = 0;//未注册
        }
        
        RegUDiskInfo["time"] = UnicodeToUTF8(stInfo.szRegisterTime);
        
        RegUDiskInfo["description"] = UnicodeToUTF8(stInfo.szDescribe);

        //U盘类型
        if ( stInfo.dwUsbType == USB_TYPE_SAFE)
        {
            RegUDiskInfo["UDiskType"] = 2;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3)
        {
            RegUDiskInfo["UDiskType"] = 3;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3_1)
        {
            RegUDiskInfo["UDiskType"] = 4;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_COMMON)
        {
            RegUDiskInfo["UDiskType"] = 1;
        }
        else
        {
            RegUDiskInfo["UDiskType"] = 0; //未知U盘
        }
        
        //重置密码次数
        RegUDiskInfo["RegisterVersion"] = (DWORD32)stInfo.dwResetPwdCounts;
        
        //U盘盘符
        if (stInfo.vecDrive.size() != 0)
        {
            for (int k=0; k < stInfo.vecDrive.size(); k++)
            {
                RegUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( stInfo.vecDrive[k]));
            }
        }
        else
        {
            std::wstring wsDrive = _T("");
            RegUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( wsDrive));  
        }

        if (stInfo.vecVolumeName.size() != 0)
        {
            for (int k=0; k < stInfo.vecVolumeName.size(); k++)
            {
                RegUDiskInfo["VolumeName"].append( UnicodeToUTF8( stInfo.vecVolumeName[k]));
            }
        }
        else
        {
            std::wstring wsVolumeName = _T("");
            RegUDiskInfo["VolumeName"].append( UnicodeToUTF8( wsVolumeName));
        }



		//权限
		RegUDiskInfo["RegAuthority"] = (DWORD32)stInfo.dwAuthority;

        //普通U盘杀毒状态
        RegUDiskInfo["ComKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwCommonUDiskSign;
        
        //安全U盘公共区杀毒状态
        RegUDiskInfo["PubliceAreaKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwPublicSign;
        
        //安全U盘安全区杀毒状态
        RegUDiskInfo["secureAreaKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwSecureSign;


        //
        CMDContentItem.append(RegUDiskInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    if (pStrError && sJson.length() > 0)
    {
        *pStrError = strFormat.str();
    }

    return sJson;
}

BOOL CWLJsonParse::DeviceControl_RegUDiskInfo_GetValue(__in const string &strJson, __out DEVICE_CONTROL_REG_UDISK_STATUS_ST &stStatus, __out VEC_ALL_UDISK_ST &vecRegUDiskInfo, __out tstring *pStrError /* = NULL */)
{
    /*
    [
        {
            "CMDContent":
            [
                {
                    "DiskDriverLetter":["E:\\", "F:\\"],
                    "ComKillVirusSign":1,
                    "PubliceAreaKillVirusSign":0,
                    "RegAuthority":1,
                    "UDiskType":1,
                    "USBClass":8,
                    "description":"威努特",
                    "plugState":1,
                    "registerStatus":1,
                    "secureAreaKillVirusSign":0,
                    "serialID":"90123123909011",
                    "time":"2021-03-01 12:22:31",
                    "type":1
                },
                {
                    "DiskDriverLetter":["G:\\", "H:\\"],
                    "ComKillVirusSign":0,
                    "PubliceAreaKillVirusSign":1,
                    "RegAuthority":5,
                    "UDiskType":3,
                    "USBClass":8,
                    "description":"威努特 V3.0",
                    "plugState":1,
                    "registerStatus":0,
                    "secureAreaKillVirusSign":0,
                    "serialID":"45ED3412312ED12C",
                    "time":"2021-03-02 14:21:12",
                    "type":1
                }
            ],
            "CMDTYPE":150,
            "ComputerID":"LOCAL",
            "KillVirusSign":0,
            "NoRegComUSB":7,
            "NoRegSafeUSB":7,
            "blueTooth":0,
            "nCmdID":120,
            "usbSwitch":1,
            "UsbStorageCtrl":1,
            "cdrom":1,
            "wireless":1,
            "serialPort":1,
            "parallelPort":1
        }
    ]
    */

    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;
    int iCMDID = 0;


    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }

    if (!reader.parse(strJson, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strJson).c_str() << _T(",");
        goto END;
    }

    if(!root[0].isMember("CMDID"))
    {
        wosError << _T("invalid json, no member : CMDID") << _T(",");
        goto END;
    }
    iCMDID = root[0]["CMDID"].asInt();
    if ( iCMDID == PLY_USM_CONFIG_DEVICE_CONTROL_INFO/*120*/)
    {
        if(!root[0].isMember("usbSwitch"))
        {
            wosError << _T("invalid json, no member : usbSwitch") << _T(",");
            goto END;
        }
        stStatus.dwRegisterUDiskStatus = (DWORD)root[0]["usbSwitch"].asInt();

        if(!root[0].isMember("NoRegComUSB"))
        {
            wosError << _T("invalid json, no member : NoRegComUSB") << _T(",");
            goto END;
        }
        stStatus.dwUnregisteredCommonUDiskStatus = (DWORD)root[0]["NoRegComUSB"].asInt();

        if(!root[0].isMember("NoRegSafeUSB"))
        {
            wosError << _T("invalid json, no member : NoRegSafeUSB") << _T(",");
            goto END;
        }
        stStatus.dwUnregisteredSafeUDiskStatus = (DWORD)root[0]["NoRegSafeUSB"].asInt();

        /*if(!root[0].isMember("KillVirusSign")) 暂时保留
        {
            wosError << _T("invalid json, no member : KillVirusSign") << _T(",");
            goto END;
        }
        stStatus.dwUDiskKillVirusSign = (DWORD)root[0]["KillVirusSign"].asInt();*/
        if(!root[0].isMember("KillVirusAreaofSafeUSB"))
        {
            wosError << _T("invalid json, no member : KillVirusAreaofSafeUSB") << _T(",");
            goto END;
        }
        int iSafeKillVirusControl = root[0]["KillVirusAreaofSafeUSB"].asInt();
        stStatus.dwUDiskKillVirusSign = (DWORD)(iSafeKillVirusControl * 2);

        // V300R010重新加入，兼容以前USM版本的Json
        if(root[0].isMember("KillVirusAreaofComUSB"))
        {
            int iComKillVirusControl = root[0]["KillVirusAreaofComUSB"].asInt();
            stStatus.dwUDiskKillVirusSign = stStatus.dwUDiskKillVirusSign + (DWORD)(iComKillVirusControl);
        }

        if(!root[0].isMember("UsbStorageCtrl"))
        {
            wosError << _T("invalid json, no member : UsbStorageCtrl") << _T(",");
            goto END;
        }
        stStatus.dwUsbStorageCtrlStatus = (DWORD)root[0]["UsbStorageCtrl"].asInt();

        if(!root[0].isMember("blueTooth"))
        {
            wosError << _T("invalid json, no member : blueTooth") << _T(",");
            goto END;
        }
        stStatus.dwBlueToothStatus = (DWORD)root[0]["blueTooth"].asInt();

        if(!root[0].isMember("cdrom"))
        {
            wosError << _T("invalid json, no member : cdrom") << _T(",");
            goto END;
        }
        stStatus.dwCdromStatus = (DWORD)root[0]["cdrom"].asInt();

        if(!root[0].isMember("wireless"))
        {
            wosError << _T("invalid json, no member : wireless") << _T(",");
            goto END;
        }
        stStatus.dwWlanStatus = (DWORD)root[0]["wireless"].asInt();

        if(!root[0].isMember("usbEthernet"))
        {
            wosError << _T("invalid json, no member : usbEthernet") << _T(",");
            goto END;
        }
        stStatus.dwUsbEthernetStatus = (DWORD)root[0]["usbEthernet"].asInt();

        if(!root[0].isMember("serialPort"))
        {
            wosError << _T("invalid json, no member : serialPort") << _T(",");
            goto END;
        }
        stStatus.dwSerialPortStatus = (DWORD)root[0]["serialPort"].asInt();

        if(!root[0].isMember("parallelPort"))
        {
            wosError << _T("invalid json, no member : parallelPort") << _T(",");
            goto END;
        }
        stStatus.dwParallelPortStatus = (DWORD)root[0]["parallelPort"].asInt();

        if(!root[0].isMember("floppyDisk"))
        {
            wosError << _T("invalid json, no member : floppyDisk") << _T(",");
            goto END;
        }
        stStatus.dwFloppyDiskStatus = (DWORD)root[0]["floppyDisk"].asInt();

        if(!root[0].isMember("usbDevice"))
        {
            wosError << _T("invalid json, no member : usbDevice") << _T(",");
            goto END;
        }
        stStatus.dwUsbPortStatus = (DWORD)root[0]["usbDevice"].asInt();

        if(!root[0].isMember("wpd"))
        {
            wosError << _T("invalid json, no member : wpd") << _T(",");
            goto END;
        }
        stStatus.dwWpdStatus = (DWORD)root[0]["wpd"].asInt();

        if(!root[0].isMember("WifiWhiteList"))
        {
            wosError << _T("invalid json, no member : WifiWhiteList") << _T(",");
            goto END;
        }
        stStatus.dwWifiDeviceStatus = (DWORD)root[0]["WifiWhiteList"].asInt();
    }


    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent") << _T(",");
        goto END;
    }

    CMDContent =  (Json::Value)root[0]["CMDContent"];
	if(CMDContent.isArray())
	{
		int iObject = CMDContent.size();
		for (int i=0; i < iObject; i++)
		{
			REG_UDISK_ST stInfo;
			//序列号
			StrCpyW(stInfo.szSerial, UTF8ToUnicode(CMDContent[i]["serialID"].asString()).c_str());

			//U盘类型
			stInfo.dwUsbType = (DWORD)CMDContent[i]["UDiskType"].asInt();

			//U盘名称
			Json::Value JsonDiskName;
			JsonDiskName = (Json::Value)CMDContent[i]["VolumeName"];
			int iNum = JsonDiskName.size();
			if ( iNum != 0)
			{
				for (int k=0; k < iNum; k++)
				{
					std::wstring wsVolumeName = UTF8ToUnicode(JsonDiskName[k].asString());
					stInfo.vecVolumeName.push_back(wsVolumeName);
				}
			}
			else
			{
				stInfo.vecVolumeName.clear();
			}
			//驱动器号
			Json::Value JsonCommDisk;
			JsonCommDisk = (Json::Value)CMDContent[i]["DiskDriverLetter"];
			iNum = JsonCommDisk.size();
			if ( iNum != 0)
			{
				for (int k=0; k < iNum; k++)
				{
					std::wstring wsDrive = UTF8ToUnicode(JsonCommDisk[k].asString());
					stInfo.vecDrive.push_back(wsDrive);
				}
			}
			else
			{
				stInfo.vecDrive.clear();
			}


			//权限
			stInfo.dwAuthority = (DWORD)CMDContent[i]["RegAuthority"].asInt();

			//注册时间
			StrCpyW(stInfo.szRegisterTime, UTF8ToUnicode(CMDContent[i]["time"].asString()).c_str());

			//注册状态
			int iRegister = (int)CMDContent[i]["registerStatus"].asInt();
			if ( iRegister == 1)
			{
				stInfo.bRegister = TRUE; //已注册
			}
			else
			{
				stInfo.bRegister = FALSE; //未注册
			}

			//插拔状态
			int iInsert = (int)CMDContent[i]["plugState"].asInt();
			stInfo.bInsert = ( iInsert == 0) ? FALSE : TRUE;

			//杀毒标志
			stInfo.stSafeSign.dwCommonUDiskSign = (DWORD)CMDContent[i]["ComKillVirusSign"].asInt();
			stInfo.stSafeSign.dwPublicSign = (DWORD)CMDContent[i]["PubliceAreaKillVirusSign"].asInt();
			stInfo.stSafeSign.dwSecureSign = (DWORD)CMDContent[i]["secureAreaKillVirusSign"].asInt();


			//描述
			StrCpyW(stInfo.szDescribe, UTF8ToUnicode(CMDContent[i]["description"].asString()).c_str());

			vecRegUDiskInfo.push_back(stInfo);
		}        
    }

    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}

BOOL CWLJsonParse::DeviceControl_RegUDiskInfo_GetCount(__in const string &strJson, __in int &iNumber, __out tstring *pStrError /* = NULL */)
{
    BOOL bRes = FALSE;

    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;

    iNumber = 0;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }

    if (!reader.parse(strJson, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strJson).c_str() << _T(",");
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent") << _T(",");
        goto END;
    }

    CMDContent =  (Json::Value)root[0]["CMDContent"];
    iNumber = CMDContent.size();

    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}

// 该函数为了区分DeviceControl_RegUDiskInfo_GetJson函数，主要用于上传USM，其余位置请不要调用
std::string CWLJsonParse::DeviceControl_RegUDiskInfo_ToUSM_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const DEVICE_CONTROL_REG_UDISK_STATUS_ST stStatus, __in const VEC_ALL_UDISK_ST vecRegUDiskInfo, __out tstring *pStrError /* = NULL */)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;
    
    wostringstream  strFormat;

    int iSize = (int)vecRegUDiskInfo.size();

    //参数检查
    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;    /*117、118、119、120*/

    if ( nCmdID == PLY_USM_CONFIG_DEVICE_CONTROL_INFO)
    {
        //开关状态
        Item["cdrom"] = (DWORD32)stStatus.dwCdromStatus;                         //CDROM
        Item["wireless"] = (DWORD32)stStatus.dwWlanStatus;                       //无线网卡
        Item["usbEthernet"] = (DWORD32)stStatus.dwUsbEthernetStatus;             //USB以太网卡
        Item["blueTooth"] = (DWORD32)stStatus.dwBlueToothStatus;                 //蓝牙状态
        Item["serialPort"] = (DWORD32)stStatus.dwSerialPortStatus;               //串口状态
        Item["parallelPort"] = (DWORD32)stStatus.dwParallelPortStatus;           //并口状态
		Item["floppyDisk"] = (DWORD32)stStatus.dwFloppyDiskStatus;               //软盘状态
		Item["usbDevice"] = (DWORD32)stStatus.dwUsbPortStatus;					 //USB设备
		Item["wpd"] = (DWORD32)stStatus.dwWpdStatus;							 //手机平板
        
        //new 
        Item["WifiWhiteList"] = (DWORD32)stStatus.dwWifiDeviceStatus;         //WiFi白名单

        Item["UsbStorageCtrl"] = (DWORD32)stStatus.dwUsbStorageCtrlStatus;       //USB总开关状态

        Item["usbSwitch"] = (DWORD32)stStatus.dwRegisterUDiskStatus;             //注册U盘开关状态
        Item["NoRegComUSB"] = (DWORD32)stStatus.dwUnregisteredCommonUDiskStatus; //非注册普通U盘权限
        Item["NoRegSafeUSB"] = (DWORD32)stStatus.dwUnregisteredSafeUDiskStatus;  //非注册安全U盘状态
        //Item["KillVirusSign"] = (DWORD32)stStatus.dwUDiskKillVirusSign;        //杀毒标志
        
        //普通U盘杀毒标记
        if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_COMMON_UDISK) == UDISK_ANTI_AV_FLAG_COMMON_UDISK)
        {
            Item["KillVirusAreaofComUSB"] = UDISK_ANTI_AV_FLAG_COMMON_UDISK;
        }
        else
        {
            Item["KillVirusAreaofComUSB"] = UDISK_ANTI_AV_FLAG_NO;
        }

        //安全U盘杀毒标记
        if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == UDISK_ANTI_AV_FLAG_SAFE_PUBLIC 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == UDISK_ANTI_AV_FLAG_SAFE_SECURE)
        {
            Item["KillVirusAreaofSafeUSB"] = 3;
        }
        else if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == 0 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == UDISK_ANTI_AV_FLAG_SAFE_SECURE)
        {
            Item["KillVirusAreaofSafeUSB"] = 2;
        }
        else if ( (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_PUBLIC) == UDISK_ANTI_AV_FLAG_SAFE_PUBLIC 
            && (stStatus.dwUDiskKillVirusSign & UDISK_ANTI_AV_FLAG_SAFE_SECURE) == 0)
        {
            Item["KillVirusAreaofSafeUSB"] = 1;
        }
        else
        {
            Item["KillVirusAreaofSafeUSB"] = 0;
        }

    }

    for (int i=0; i<iSize; i++)
    {
        Json::Value RegUDiskInfo;

        Json::Value JsonDriveLetter;
        Json::Value JsonSecureDisk;

        REG_UDISK_ST stInfo;
        stInfo = vecRegUDiskInfo[i];

        RegUDiskInfo["type"] = 1;       //固定不变（旧版Linux的Json需要该字段，保留，下同）
        RegUDiskInfo["USBClass"] = 8;   //固定不变

        //插拔状态
        RegUDiskInfo["plugState"] = (int)stInfo.bInsert;   //1 - 插入 0 - 拔出
        
        //序列号
        RegUDiskInfo["serialID"] = UnicodeToUTF8(stInfo.szSerial);



        //注册状态
        if ( stInfo.bRegister == TRUE)
        {
            RegUDiskInfo["registerStatus"] = 1;//已注册
        }
        else
        {
            RegUDiskInfo["registerStatus"] = 0;//未注册
        }
        
        RegUDiskInfo["time"] = UnicodeToUTF8(stInfo.szRegisterTime);
        
        RegUDiskInfo["description"] = UnicodeToUTF8(stInfo.szDescribe);

        //U盘类型
        if ( stInfo.dwUsbType == USB_TYPE_SAFE)
        {
            RegUDiskInfo["UDiskType"] = 2;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3)
        {
            RegUDiskInfo["UDiskType"] = 3;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3_1)
        {
            RegUDiskInfo["UDiskType"] = 4;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_COMMON)
        {
            RegUDiskInfo["UDiskType"] = 1;
        }
        else
        {
            RegUDiskInfo["UDiskType"] = 0; //未知U盘
        }
        
        //重置密码次数
        RegUDiskInfo["RegisterVersion"] = (DWORD32)stInfo.dwResetPwdCounts;
        
        //U盘盘符
        if (stInfo.vecDrive.size() != 0)
        {
            for (int k=0; k < stInfo.vecDrive.size(); k++)
            {
                RegUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( stInfo.vecDrive[k]));
            }
        }
        else
        {
            std::wstring wsDrive = _T("");
            RegUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( wsDrive));
        }
    
        if (stInfo.vecVolumeName.size() != 0)
        {
            for (int k=0; k < stInfo.vecVolumeName.size(); k++)
            {
                //U盘名称
                RegUDiskInfo["VolumeName"].append(UnicodeToUTF8( stInfo.vecVolumeName[k]));
            }
        }
        else
        {
            std::wstring wsVolumeName = _T("");
            RegUDiskInfo["VolumeName"].append(UnicodeToUTF8( wsVolumeName));
        }


		//权限
        if (stInfo.bRegister == TRUE)
        {
            RegUDiskInfo["RegAuthority"] = (DWORD32)stInfo.dwAuthority;
        }
        else
        {
            // 这里把没有注册的U盘统一设置成为<7>权限，为了管理平台展示使用 BUG 53840
            RegUDiskInfo["RegAuthority"] = (DWORD32)(AUTHORIZED_READ + AUTHORIZED_WRITE + AUTHORIZED_EXEC);
        }

        //普通U盘杀毒状态
        RegUDiskInfo["ComKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwCommonUDiskSign;
        
        //安全U盘公共区杀毒状态
        RegUDiskInfo["PubliceAreaKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwPublicSign;
        
        //安全U盘安全区杀毒状态
        RegUDiskInfo["secureAreaKillVirusSign"] = (DWORD32)stInfo.stSafeSign.dwSecureSign;


        //加入数组
        CMDContentItem.append(RegUDiskInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    if (pStrError && sJson.length() > 0)
    {
        *pStrError = strFormat.str();
    }

    return sJson;
}

//#define USB_WL_REPLCACE   1		//USB白名单全替换
//#define USB_WL_ADD        2		//USB白名单增量

std::string CWLJsonParse::DeviceControl_UsbWhiteList_ToUSM_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in vector<USB_DEVICE_INFO> vecUsbWhite, __out tstring *pStrError,int nOperation, BOOL IsAddWhiteList)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    wostringstream  strFormat;
    //参数检查
    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;   
	
	// 只有是客户端发给服务端的时候，才给Operation赋值；代表是通过告警，还是通过列表  1代表全量替换  2代表 增量
	std::string strOperation;
	if(nOperation == USB_WL_REPLCACE)
	{
		strOperation = "replace";
		Item["Operation"] = strOperation;
	}
	else if(nOperation == USB_WL_ADD)
	{
		strOperation = "add";
		Item["Operation"] = strOperation;
	}
	   

	
	// 上报USB 白名单
	Json::Value CMDContentUsbItem;
	for(int i =0;i<vecUsbWhite.size();i++)
	{
		Json::Value jsonUsbInfo;
		USB_DEVICE_INFO stInfo;
		stInfo = vecUsbWhite[i];
		
		//USB没插入且不在白名单不需要上传到客户端、USM
		/*if(!stInfo.isInsert && !stInfo.bWhiteList)
		{
			continue;
		}*/
		
		// usb单个设备的信息上传赋值
		DWORD32 nWhite = 0;
		if(stInfo.bWhiteList)
		{
			nWhite = 1;
		}
		CString temp;
		temp.Format(_T("%04X"),stInfo.dwPid);
		jsonUsbInfo["PID"] = UnicodeToUTF8(temp.GetString());

		temp.Format(_T("%04X"),stInfo.dwVid);
		jsonUsbInfo["VID"] = UnicodeToUTF8(temp.GetString());
		//jsonUsbInfo["USBClass"] = UnicodeToUTF8(stInfo.wsUsbClass);
		jsonUsbInfo["ProductDesc"] = UnicodeToUTF8(stInfo.wsProductInfo);
		jsonUsbInfo["VendorDesc"] = UnicodeToUTF8(stInfo.wsVendorInfo);
		if(IsAddWhiteList)
		{
			jsonUsbInfo["IsWhiteList"] = 1;    //添加白名单情况下将IsWhiteList默认设置为1 ，添加到白名单
		}
		else
		{
			jsonUsbInfo["IsWhiteList"] = nWhite;
		}

		jsonUsbInfo["plugState"] = stInfo.isInsert;
		jsonUsbInfo["description"] = "";

		CMDContentUsbItem.append(jsonUsbInfo);
	}
	Item["CMDContent"] = (Json::Value)CMDContentUsbItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    if (pStrError && sJson.length() > 0)
    {
        *pStrError = strFormat.str();
    }

    return sJson;
}
#define REG_DEV_FLAG_REPLCACE   1		//注册U盘模板全替换
#define REG_DEV_FLAG_ADD        2		//注册U盘模板增量

BOOL CWLJsonParse::DeviceControl_UsbWhite_FromServer_GetValue(__in const string &strJson, 
														   __out std::vector<USB_DEVICE_INFO>& vecUsbWhite,
														   __out tstring *pStrError/* = NULL*/)
{
	//解析USM下发的Json串
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;

	wostringstream  wosError;

	std::string strValue = "";
	std::string strOperation = "";

	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv1
	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	// 解析USM下发的 USB设备 白名单
	if(root[0].isMember("CMDContent"))
	{
		Json::Value  &CMDContent = (Json::Value)root[0]["CMDContent"];

		int iObject = CMDContent.size();

		for (int i=0; i < iObject; i++)
		{
			USB_DEVICE_INFO usbInfo;
			// PID
			string strPID = CMDContent[i]["PID"].asString(); 
			usbInfo.dwPid = strtoul(strPID.c_str(),NULL,16);// UTF8ToUnicode(strPID);

			// VID
			string strVID = CMDContent[i]["VID"].asString(); 
			//usbInfo.wsVID = UTF8ToUnicode(strVID);
			usbInfo.dwVid = strtoul(strVID.c_str(),NULL,16);
			
			// ProductDesc
			string strProductDesc = CMDContent[i]["ProductDesc"].asString(); 
			usbInfo.wsProductInfo = UTF8ToUnicode(strProductDesc);
			
			// VendorDesc
			string strVendorDesc = CMDContent[i]["VendorDesc"].asString(); 
			usbInfo.wsVendorInfo = UTF8ToUnicode(strVendorDesc);

			// IsWhiteList
			usbInfo.bWhiteList = (DWORD)CMDContent[i]["IsWhiteList"].asInt();

			//wsUsbClass
			string strUsbClass = CMDContent[i]["USBClass"].asString(); 
			usbInfo.wsUsbClass = UTF8ToUnicode(strUsbClass);
			
			vecUsbWhite.push_back(usbInfo);
		}
	}
	bRes = TRUE;
END:
	return bRes;
}



BOOL CWLJsonParse::DeviceControl_UsbWhite_FromUSM_GetValue(__in const string &strJson, 
															   __out std::vector<USB_DEVICE_INFO>& vecUsbWhite,
															   __out int& nOperation,
															   __out tstring *pStrError/* = NULL*/)
{
    //解析USM下发的Json串
    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
   
    wostringstream  wosError;

    std::string strValue = "";
	std::string strOperation = "";
	BOOL isFromUSM = TRUE;

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
	//wwdv1
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }
	
	nOperation = 1;
	if(root[0].isMember("Operation"))
	{
		string strOperation = root[0]["Operation"].asString(); 
		if(strOperation == "replace")
		{
			nOperation = 1;
		}
		else if(strOperation == "add")
		{
			nOperation = 2;
		}
	}
	if(root[0].isMember("ComputerID"))
	{
		string ComputerId = root[0]["ComputerID"].asString(); 
		if(ComputerId == string("LOCAL"))
		{
			isFromUSM = FALSE;
		}
	}
	// 解析USM下发的 USB设备 白名单
	if(root[0].isMember("CMDContent"))
	{
		Json::Value  &CMDContent = (Json::Value)root[0]["CMDContent"];
		//wwdv1
	    if (CMDContent.isArray())
	    {
			int iObject = CMDContent.size();

			for (int i=0; i < iObject; i++)
			{
				USB_DEVICE_INFO usbInfo;
				// PID
				string strPID = CMDContent[i]["PID"].asString(); 
				usbInfo.dwPid = strtoul(strPID.c_str(),NULL,16);// UTF8ToUnicode(strPID);

				// VID
				string strVID = CMDContent[i]["VID"].asString(); 
				//usbInfo.wsVID = UTF8ToUnicode(strVID);
				usbInfo.dwVid = strtoul(strVID.c_str(),NULL,16);
				//IsWhiteList

				if (CMDContent[i].isMember("IsWhiteList"))
				{
					usbInfo.bWhiteList = (DWORD)CMDContent[i]["IsWhiteList"].asInt();
				}
				//else
				//{
				//	if(isFromUSM)
				//	{
				//		usbInfo.bWhiteList = 1;
				//	}
				//}
				// ProductDesc
				if (CMDContent[i].isMember("ProductDesc"))
				{
					string strProductDesc = CMDContent[i]["ProductDesc"].asString(); 
					usbInfo.wsProductInfo = UTF8ToUnicode(strProductDesc);
				}

				// VendorDesc
				if (CMDContent[i].isMember("VendorDesc"))
				{
					string strVendorDesc = CMDContent[i]["VendorDesc"].asString(); 
					usbInfo.wsVendorInfo = UTF8ToUnicode(strVendorDesc);
				}
				if (CMDContent[i].isMember("USBClass"))
				{
					//wsUsbClass
					string strUsbClass = CMDContent[i]["USBClass"].asString(); 
					usbInfo.wsUsbClass = UTF8ToUnicode(strUsbClass);
				}
				vecUsbWhite.push_back(usbInfo);
			}
	    }
		
	}
    bRes = TRUE;
END:
    return bRes;
}

BOOL CWLJsonParse::DeviceControl_RegUDiskInfo_FromUSM_GetValue(__in const string &strJson, 
															   __out DEVICE_CONTROL_REG_UDISK_STATUS_ST &stStatus, 
															   __out VEC_ALL_UDISK_ST &vecRegUDiskInfo, 
															   __out BOOL &bCMDContentFind, 
															   __out DWORD &dwUDiskRegFlag,
															   __out DWORD &dwUDiskKillVirusSignForSafeDisk,
                                                               __out DWORD &dwUDiskKillVirusSignForComDisk,
															   __out tstring *pStrError/* = NULL*/)
{
    //解析USM下发的Json串

    /*
    [
        {
            "CMDContent":
            [
                {
                    "RegAuthority":1,
                    "UDiskType":1,
                    "description":"威努特",
                    "registerStatus":1,
                    "serialID":"90123123909011",
                    ……
                }
            ],
            "CMDTYPE":150,
            "ComputerID":"LOCAL",
            "KillVirusAreaofComUSB":1,
            "KillVirusAreaofSafeUSB":3,
            "NoRegComUSB":7,
            "NoRegSafeUSB":7,
            "blueTooth":0,
            "CMDID":120,
            "usbSwitch":1,
            "cdrom":1,
            "wireless":1,
            "serialPort":1,
            "parallelPort":1,
            "UsbStorageCtrl":1
			"floppyDisk":1
			"wps":1
			"usbDevice":1
        }
    ]
    */

    

    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
   
    wostringstream  wosError;

    std::string strValue = "";
	std::string strOperation = "";

    strValue = strJson;

	bCMDContentFind = FALSE;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
	//wwdv1
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    if(!root[0].isMember("UsbStorageCtrl"))
    {
        wosError << _T("invalid json, no member : UsbStorageCtrl") << _T(",");
        stStatus.dwUsbStorageCtrlStatus = INVALID_JSON_INT;
        //goto END;
    }
    else
    {
        stStatus.dwUsbStorageCtrlStatus = (DWORD)root[0]["UsbStorageCtrl"].asInt();
    }

    if(!root[0].isMember("usbSwitch"))
    {
        wosError << _T("invalid json, no member : usbSwitch") << _T(",");
        stStatus.dwRegisterUDiskStatus = INVALID_JSON_INT;
        //goto END;
    }
    else
    {
        stStatus.dwRegisterUDiskStatus = (DWORD)root[0]["usbSwitch"].asInt();
    }

    if(!root[0].isMember("NoRegComUSB"))
    {
        wosError << _T("invalid json, no member : NoRegComUSB") << _T(",");
        stStatus.dwUnregisteredCommonUDiskStatus = INVALID_JSON_INT;
        //goto END;
    }
    else
    {
        stStatus.dwUnregisteredCommonUDiskStatus = (DWORD)root[0]["NoRegComUSB"].asInt();
    } 
    
    if(!root[0].isMember("NoRegSafeUSB"))
    {
        wosError << _T("invalid json, no member : NoRegSafeUSB") << _T(",");
        stStatus.dwUnregisteredSafeUDiskStatus = INVALID_JSON_INT;
        //goto END;
    }
    else
    {
        stStatus.dwUnregisteredSafeUDiskStatus = (DWORD)root[0]["NoRegSafeUSB"].asInt();
    }

    //安全U盘杀毒标记
    if(!root[0].isMember("KillVirusAreaofSafeUSB"))
    {
        wosError << _T("invalid json, no member : KillVirusAreaofSafeUSB") << _T(",");
        dwUDiskKillVirusSignForSafeDisk = INVALID_JSON_INT;
    }
    else
    {
        dwUDiskKillVirusSignForSafeDisk = root[0]["KillVirusAreaofSafeUSB"].asInt();
    }

    //普通U盘杀毒标记
    if(!root[0].isMember("KillVirusAreaofComUSB"))
    {
        wosError << _T("invalid json, no member : KillVirusAreaofComUSB") << _T(",");
        dwUDiskKillVirusSignForComDisk = INVALID_JSON_INT;
    }
    else
    {
        dwUDiskKillVirusSignForComDisk = root[0]["KillVirusAreaofComUSB"].asInt();
    }

    //int iSafeKillVirusControl = root[0]["KillVirusAreaofSafeUSB"].asInt();
    //int iCommonKillVirusControl = root[0]["KillVirusAreaofComUSB"].asInt();
    //stStatus.dwUDiskKillVirusSign = (DWORD)(iSafeKillVirusControl * 2 + iCommonKillVirusControl);

    if(!root[0].isMember("blueTooth"))
    {
        wosError << _T("invalid json, no member : blueTooth") << _T(",");
        stStatus.dwBlueToothStatus = INVALID_JSON_INT;
        //goto END;
    }
    else
    {
        stStatus.dwBlueToothStatus = (DWORD)root[0]["blueTooth"].asInt();
        
    }

    if(!root[0].isMember("cdrom"))
    {
        wosError << _T("invalid json, no member : cdrom") << _T(",");
        stStatus.dwCdromStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwCdromStatus = (DWORD)root[0]["cdrom"].asInt();
    }
    
    if(!root[0].isMember("wireless"))
    {
        wosError << _T("invalid json, no member : wireless") << _T(",");
        stStatus.dwWlanStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwWlanStatus = (DWORD)root[0]["wireless"].asInt();
    }

    if(!root[0].isMember("usbEthernet"))
    {
        wosError << _T("invalid json, no member : usbEthernet") << _T(",");
        stStatus.dwUsbEthernetStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwUsbEthernetStatus = (DWORD)root[0]["usbEthernet"].asInt();
    }

    if(!root[0].isMember("serialPort"))
    {
        wosError << _T("invalid json, no member : serialPort") << _T(",");
        stStatus.dwSerialPortStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwSerialPortStatus = (DWORD)root[0]["serialPort"].asInt();
    }
    
    if(!root[0].isMember("parallelPort"))
    {
        wosError << _T("invalid json, no member : parallelPort") << _T(",");
       stStatus.dwParallelPortStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwParallelPortStatus = (DWORD)root[0]["parallelPort"].asInt();
    }

	if (!root[0].isMember("floppyDisk"))
	{
		wosError << _T("invalid json, no member : floppyDisk") << _T(",");
		stStatus.dwFloppyDiskStatus = INVALID_JSON_INT;
	}
	else
	{
		stStatus.dwFloppyDiskStatus = (DWORD)root[0]["floppyDisk"].asInt();
	}

	if (!root[0].isMember("wpd"))
	{
		wosError << _T("invalid json, no member : wpd") << _T(",");
		stStatus.dwWpdStatus = INVALID_JSON_INT;
	}
	else
	{
		stStatus.dwWpdStatus = (DWORD)root[0]["wpd"].asInt();
	}

	if (!root[0].isMember("usbDevice"))
	{
		wosError << _T("invalid json, no member : usbDevice") << _T(",");
		stStatus.dwUsbPortStatus = INVALID_JSON_INT;
	}
	else
	{
		stStatus.dwUsbPortStatus = (DWORD)root[0]["usbDevice"].asInt();
	}

    if (!root[0].isMember("WifiWhiteList"))
    {
        wosError << _T("invalid json, no member : WifiWhiteList") << _T(",");
        stStatus.dwWifiDeviceStatus = INVALID_JSON_INT;
    }
    else
    {
        stStatus.dwWifiDeviceStatus = (DWORD)root[0]["WifiWhiteList"].asInt();
    }
    
    if(root[0].isMember("CMDContent"))
    {
		bCMDContentFind = TRUE;
        Json::Value  &CMDContent = (Json::Value)root[0]["CMDContent"];;
//        CMDContent =  
		//wwdv1   
        if (CMDContent.isArray())
        {
			int iObject = CMDContent.size();
			for (int i=0; i < iObject; i++)
			{
				REG_UDISK_ST stInfo;
				//序列号
				string strSerialNumber = CMDContent[i]["serialID"].asString(); 
				StrCpyW(stInfo.szSerial, UTF8ToUnicode(strSerialNumber).c_str());

				//U盘类型
				stInfo.dwUsbType = (DWORD)CMDContent[i]["UDiskType"].asInt();

				//权限
				stInfo.dwAuthority = (DWORD)CMDContent[i]["RegAuthority"].asInt();

				//注册状态
				int iRegister = (int)CMDContent[i]["registerStatus"].asInt();
				if ( iRegister == 1)
				{
					stInfo.bRegister = TRUE; //已注册
				}
				else
				{
					stInfo.bRegister = FALSE; //未注册
				}

				//描述
				if (CMDContent[i].isMember("description"))
				{
					StrCpyW(stInfo.szDescribe, UTF8ToUnicode(CMDContent[i]["description"].asString()).c_str());
				}

				//重置密码次数
				if (CMDContent[i].isMember("RegisterVersion"))
				{
					stInfo.dwResetPwdCounts = (DWORD)CMDContent[i]["RegisterVersion"].asInt();
				}

				vecRegUDiskInfo.push_back(stInfo);
			}

        }

    }

	//注册U盘模板标记全替换/增量
	if(!root[0].isMember("Operation"))
	{
		wosError << _T("invalid json, no member : Operation") << _T(",");
		dwUDiskRegFlag = REG_DEV_FLAG_REPLCACE;
	}
	else
	{
		strOperation = root[0]["Operation"].asString();
		if (strOperation.npos != strOperation.find("add"))
		{
			dwUDiskRegFlag = REG_DEV_FLAG_ADD;
		}
		else
		{
			dwUDiskRegFlag = REG_DEV_FLAG_REPLCACE;
		}
	}

    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}

std::string CWLJsonParse::DeviceControl_RegUDiskInfo_FromUSM_Result_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const int iResult, __in const std::vector<std::wstring> vecFailedInfo, __out tstring *pStrError /* = NULL */)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;

    wostringstream  strFormat;

    int iSize = (int)vecFailedInfo.size();  //注册失败U盘数量

    //参数检查

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;    /*120*/
    Item["CMDVER"] = 1;

    //成功或失败
    if ( iResult == REGISTER_UDISK_FAILED_REASON_NO)
    {
        CMDContentItem["RESULT"] = "SUC";
    }
    else
    {
        CMDContentItem["RESULT"] = "FAIL";
        CMDContentItem["FAILEDCODE"] = iResult; //1表示主机卫士离线，请稍后再试；2 注册U盘失败，读取U盘信息错误

        if ( iSize != 0)
        {
            for (int i=0; i<iSize; i++)
            {
                CMDContentItem["regFailedUSBID"].append( UnicodeToUTF8( vecFailedInfo[i]));
            }
        }
        else
        {
            std::wstring wsDrive = _T("");
            CMDContentItem["regFailedUSBID"].append( UnicodeToUTF8( wsDrive));
        }
    }

    Item["CMDContent"]  = (Json::Value)CMDContentItem; //去掉'\n'，直接写字符串会带上\n

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    if (pStrError && sJson.length() > 0)
    {
        *pStrError = strFormat.str();
    }

    return sJson;
}

//USB插拔上报USMJson
std::string CWLJsonParse::DeviceControl_PlugUDisk_GetJson(__in const tstring tstrComputerID,  __in const VEC_ALL_UDISK_ST vecRegUDiskInfo, __out tstring *pStrError /* = NULL */)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;

    wostringstream  strFormat;

    int iSize = (int)vecRegUDiskInfo.size();

    //参数检查

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDVER"] = 1;   

    for (int i=0; i<iSize; i++)
    {
        Json::Value PlugUDiskInfo;
    
        Json::Value JsonDriveLetter;
        Json::Value JsonSecureDisk;

        REG_UDISK_ST stInfo;
        stInfo = vecRegUDiskInfo[i];

        //插拔状态
        PlugUDiskInfo["plugEvent"] = (int)stInfo.bInsert;   //1 - 插入 0 - 拔出

        //序列号
        PlugUDiskInfo["serialID"] = UnicodeToUTF8(stInfo.szSerial);

        //注册状态
        if ( stInfo.bRegister == TRUE)
        {
            PlugUDiskInfo["registerStatus"] = 1;//已注册
        }
        else
        {
            PlugUDiskInfo["registerStatus"] = 0;//未注册
        }

        //插拔时间
        PlugUDiskInfo["time"] = UnicodeToUTF8(stInfo.szPlugTime);

        //U盘类型
        if ( stInfo.dwUsbType == USB_TYPE_SAFE)
        {
            PlugUDiskInfo["UDiskType"] = 2;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3)
        {
            PlugUDiskInfo["UDiskType"] = 3;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_SAFE_V3_1)
        {
            PlugUDiskInfo["UDiskType"] = 4;
        }
        else if ( stInfo.dwUsbType == USB_TYPE_COMMON)
        {
            PlugUDiskInfo["UDiskType"] = 1;
        }
        else
        {
            PlugUDiskInfo["UDiskType"] = 0; //未知U盘
        }

        //U盘盘符
        if (stInfo.vecDrive.size() != 0)
        {
            for (int k=0; k < stInfo.vecDrive.size(); k++)
            {
                PlugUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( stInfo.vecDrive[k]));
            }
        }
        else
        {
            std::wstring wsDrive = _T("");
            PlugUDiskInfo["DiskDriverLetter"].append( UnicodeToUTF8( wsDrive));
        }

        //
        CMDContentItem.append(PlugUDiskInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    if (pStrError && sJson.length() > 0)
    {
        *pStrError = strFormat.str();
    }

    return sJson;
}

std::string CWLJsonParse::DeviceControl_RegUDisk_Rule_GetJson(__in const tstring tstrComputerID, __in const VEC_REG_RULE vecRules)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;

    wostringstream  strFormat;

    int iSize = (int)vecRules.size();

    //参数检查

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDVER"] = 1;   

    for (int i=0; i<iSize; i++)
    {
        Json::Value RegUDiskInfo;

        Json::Value JsonDriveLetter;
        Json::Value JsonSecureDisk;

        REG_USB_RULE stRule;
        stRule = vecRules[i];

        //序列号
        RegUDiskInfo["Serial"] = UnicodeToUTF8(stRule.wcSerial);

        //描述
        RegUDiskInfo["Remark"] = UnicodeToUTF8(stRule.wcRemark);

        //权限
        RegUDiskInfo["Right"] = (int)stRule.dwRight; 

        //注册时间
        RegUDiskInfo["Time"] = UnicodeToUTF8(stRule.wcTime);

        //U盘类型
        RegUDiskInfo["UDiskType"] = (int)stRule.dwUsbType;

        //注册信息（重置密码次数）
        RegUDiskInfo["RegisterInfo"] = (int)stRule.dwResetPwdCounts;
        
        //U盘盘符
        if (stRule.vecVolumeName.size() != 0)
        {
            for (int k=0; k < stRule.vecVolumeName.size(); k++)
            {
                RegUDiskInfo["VolumeName"].append( UnicodeToUTF8( stRule.vecVolumeName[k]));
            }
        }
        else
        {
            std::wstring vecVolumeName = _T("");
            RegUDiskInfo["VolumeName"].append( UnicodeToUTF8( vecVolumeName));
        }

        CMDContentItem.append(RegUDiskInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    return sJson;
}

BOOL CWLJsonParse::DeviceControl_RegUDisk_Rule_GetValue(__in const string &strJson, __out VEC_REG_RULE &vecRules, __out tstring *pStrError/* = NULL */)
{
    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;

    std::string strValue = "";

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
	//wwdv1
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    
    CMDContent =  (Json::Value)root[0]["CMDContent"];
	if (CMDContent.isArray())
	{
		int iObject = CMDContent.size();
		for (int i=0; i < iObject; i++)
		{
			REG_USB_RULE stRule; 

			//序列号
			StrCpyW(stRule.wcSerial, UTF8ToUnicode(CMDContent[i]["Serial"].asString()).c_str());

			//描述
			StrCpyW(stRule.wcRemark, UTF8ToUnicode(CMDContent[i]["Remark"].asString()).c_str());

			//权限
			stRule.dwRight = (DWORD)CMDContent[i]["Right"].asInt();

			//注册时间
			StrCpyW(stRule.wcTime, UTF8ToUnicode(CMDContent[i]["Time"].asString()).c_str());

			//U盘类型
			stRule.dwUsbType = (DWORD)CMDContent[i]["UDiskType"].asInt();

			//注册信息（重置密码次数）
			stRule.dwResetPwdCounts = (DWORD)CMDContent[i]["RegisterInfo"].asInt();

			//U盘名称
			Json::Value JsonDiskName;
			JsonDiskName = (Json::Value)CMDContent[i]["VolumeName"];
			int iNum = JsonDiskName.size();
			if ( iNum != 0)
			{
				for (int k=0; k < iNum; k++)
				{
					std::wstring wsVolumeName = UTF8ToUnicode(JsonDiskName[k].asString());
					stRule.vecVolumeName.push_back(wsVolumeName);
				}
			}
			else
			{
				stRule.vecVolumeName.clear();
			}

			vecRules.push_back(stRule);
		}
	}


    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}

// WiFi白名单相关：WIFI列表扫描
std::string CWLJsonParse::DeviceControl_Scan_WiFi_LOCAL_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
    //此函数目的仅为了兼容客户端与USM的处理，返回WiFi扫描结果给客户端或USM。
    //针对本地客户端：CWLUDisk::ApplyPolicy函数，如果没有json串发送，此函数不会处理来自界面的消息。
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = nCmdType;
    Item["CMDID"] = nCmdID;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    return sJson;
}

std::string CWLJsonParse::DeviceControl_Scan_WiFi_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID, __in const std::vector<WIFI_INFO_ST> vecScanWiFi)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;

    int iSize = (int)vecScanWiFi.size();

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = nCmdType;
    Item["CMDID"] = nCmdID;

    for (int i=0; i<iSize; i++)
    {
        Json::Value jvWiFiInfo;

        WIFI_INFO_ST stWiFi = vecScanWiFi[i];

        //是否属于WiFi白名单
        jvWiFiInfo["IsWhiteList"] = (int)stWiFi.bIsWhiteList;

        //白名单入库时间
        jvWiFiInfo["WLTime"] = UnicodeToUTF8(stWiFi.wszStorageTime);

        //来源
        jvWiFiInfo["Source"] = (int)stWiFi.dwSource;

        //WiFi名称
        jvWiFiInfo["SSID"] = UnicodeToUTF8(stWiFi.wszSsid);

        //WiFi唯一标识符 MAC地址
        jvWiFiInfo["MAC"] = UnicodeToUTF8(stWiFi.wszMAC);

        //上传时间
        jvWiFiInfo["Time"] = UnicodeToUTF8(stWiFi.wszUploadTime);
        
        //WiFi接口GUID
        std::wstring wsGuid = CStrUtil::GuidToWString(stWiFi.stGuid);
        jvWiFiInfo["GUID"] = UnicodeToUTF8(wsGuid);

        ///WiFi网络的基本服务集类型
        jvWiFiInfo["BssType"] = (int)stWiFi.emDot11BssType;

        //默认的身份验证算法
        jvWiFiInfo["AuthAlgo"] = (int)stWiFi.emAuthAlgo;

        ///默认的加密算法/序列号
        jvWiFiInfo["CipherAlgo"] = (int)stWiFi.emCipherAlgo;

        //信号强度，0 ~ 100
        jvWiFiInfo["Intensity"] = (int)stWiFi.dwIntensity;

        //目前是否连接，TRUE连接
        jvWiFiInfo["Connect"] = (int)stWiFi.bConnected;

        //是否存在profile, TRUE存在/描述
        jvWiFiInfo["IsHaveProfile"] = (int)stWiFi.bHaveProfile;

        //汇总，写入Json
        CMDContentItem.append(jvWiFiInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    return sJson;
}

BOOL CWLJsonParse::DeviceControl_Scan_WiFi_GetValue(__in const string &strJson, __out std::vector<WIFI_INFO_ST> &vecScanWiFi, __out tstring *pStrError /* = NULL */ )
{
    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;

    std::string strValue = "";

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
	//wwdv1
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }


    CMDContent =  (Json::Value)root[0]["CMDContent"];
	if (CMDContent.isArray())
	{
		int iObject = CMDContent.size();
		for (int i = 0; i < iObject; i++)
		{
			WIFI_INFO_ST stWiFi;

			//是否属于WiFi白名单
			stWiFi.bIsWhiteList = (BOOL)CMDContent[i]["IsWhiteList"].asInt();

			//白名单入库时间
			StrCpyW(stWiFi.wszStorageTime, UTF8ToUnicode(CMDContent[i]["WLTime"].asString()).c_str());

			//来源
			stWiFi.dwSource = (DWORD)CMDContent[i]["Source"].asInt();

			//WiFi名称
			StrCpyW(stWiFi.wszSsid, UTF8ToUnicode(CMDContent[i]["SSID"].asString()).c_str());

			//WiFi唯一标识符 MAC地址
			StrCpyW(stWiFi.wszMAC, UTF8ToUnicode(CMDContent[i]["MAC"].asString()).c_str());

			//上传时间
			StrCpyW(stWiFi.wszUploadTime, UTF8ToUnicode(CMDContent[i]["Time"].asString()).c_str());

			//WiFi接口GUID
			std::wstring wsGuid = UTF8ToUnicode(CMDContent[i]["GUID"].asString());
			stWiFi.stGuid = CStrUtil::WStringToGuid(wsGuid);

			///WiFi网络的基本服务集类型
			stWiFi.emDot11BssType = (DOT11_BSS_TYPE)CMDContent[i]["BssType"].asInt();

			//默认的身份验证算法
			stWiFi.emAuthAlgo = (DOT11_AUTH_ALGORITHM)CMDContent[i]["AuthAlgo"].asInt();

			///默认的加密算法/序列号
			stWiFi.emCipherAlgo = (DOT11_CIPHER_ALGORITHM)CMDContent[i]["CipherAlgo"].asInt();

			//信号强度，0 ~ 100
			stWiFi.dwIntensity = (DWORD)CMDContent[i]["Intensity"].asInt();

			//目前是否连接，TRUE连接
			stWiFi.bConnected = (BOOL)CMDContent[i]["Connect"].asInt();

			//是否存在profile, TRUE存在/描述
			stWiFi.bHaveProfile = (BOOL)CMDContent[i]["IsHaveProfile"].asInt();

			//汇总
			vecScanWiFi.push_back(stWiFi);
		}
	}

    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}

// WiFi白名单相关：WIFI白名单列表（可用于USM通信）
std::string CWLJsonParse::DeviceControl_WiFi_WhiteList_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID, __in const std::vector<WIFI_INFO_ST> vecWiFiWhiteList)
{
    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;

    int iSize = (int)vecWiFiWhiteList.size();
    for (int i = 0; i < iSize; i++)
    {
        Json::Value jvWiFiInfo;

        WIFI_INFO_ST stWiFi = vecWiFiWhiteList[i];

        //是否属于WiFi白名单
        jvWiFiInfo["IsWhiteList"] = (int)stWiFi.bIsWhiteList;

        //白名单入库时间
        jvWiFiInfo["WLTime"] = UnicodeToUTF8(stWiFi.wszStorageTime);

        //来源
        jvWiFiInfo["Source"] = (int)stWiFi.dwSource;

        //WiFi名称
        jvWiFiInfo["SSID"] = UnicodeToUTF8(stWiFi.wszSsid);

        //WiFi唯一标识符 MAC地址
        jvWiFiInfo["MAC"] = UnicodeToUTF8(stWiFi.wszMAC);

        //WiFi接口GUID
        //std::wstring wsGuid = CStrUtil::GuidToWString(stWiFi.stGuid);
        //jvWiFiInfo["GUID"] = UnicodeToUTF8(wsGuid);

        //汇总，写入Json
        CMDContentItem.append(jvWiFiInfo);
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;
    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;    /*615*/
    Item["CMDVER"] = 1;

    root.append(Item);
    sJson = writer.write(root);
    root.clear();

    return sJson;
}

BOOL CWLJsonParse::DeviceControl_WiFi_WhiteList_GetValue(__in const string &strJson, __out std::vector<WIFI_INFO_ST> &vecWiFiWhiteList, __out tstring *pStrError /* = NULL */ )
{
    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;

    std::string strValue = "";

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
	//wwdv1
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    vecWiFiWhiteList.clear();

    CMDContent =  (Json::Value)root[0]["CMDContent"];
	if (CMDContent.isArray())
	{
		int iObject = CMDContent.size();
		for (int i = 0; i < iObject; i++)
		{
			 WIFI_INFO_ST stWiFi;

			//是否属于WiFi白名单
			 if (CMDContent[i].isMember("IsWhiteList") && !CMDContent[i]["IsWhiteList"].isNull())
			 {
				 stWiFi.bIsWhiteList = (BOOL)CMDContent[i]["IsWhiteList"].asInt();
			 }

			 //白名单入库时间
			 if (CMDContent[i].isMember("WLTime") && !CMDContent[i]["WLTime"].isNull())
			 {
				 StrCpyW(stWiFi.wszStorageTime, UTF8ToUnicode(CMDContent[i]["WLTime"].asString()).c_str());
			 }

			 //来源
			 if (CMDContent[i].isMember("Source") && !CMDContent[i]["Source"].isNull())
			 {
				stWiFi.dwSource = (DWORD)CMDContent[i]["Source"].asInt();
			 }

			 //WiFi名称
			 StrCpyW(stWiFi.wszSsid, UTF8ToUnicode(CMDContent[i]["SSID"].asString()).c_str());

			  //WiFi唯一标识符 MAC地址
			 StrCpyW(stWiFi.wszMAC, UTF8ToUnicode(CMDContent[i]["MAC"].asString()).c_str());

             //WiFi接口GUID
			 /*if (CMDContent[i].isMember("GUID"))
             {
				std::wstring wsGuid = UTF8ToUnicode(CMDContent[i]["GUID"].asString());
			    stWiFi.stGuid = CStrUtil::WStringToGuid(wsGuid);
			 }*/

			 //汇总
			vecWiFiWhiteList.push_back(stWiFi);
		}
    
	}
    

    bRes = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bRes;
}


/*
* @fn           SecDetect_UserList_GetJson
* @brief        从用户列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]    vecUserStu: 自定义用户列表结构体，调用位置 WLDeal
* @param[out]   
* @return       string: 返回根据用户列表结构体组装的Json字符串
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-21
*/
std::string CWLJsonParse::SecDetect_UserList_GetJson(__in vector<SECDETECT_USER_STRUCT> &vecUserStu)
{
	/*
	{
		"UserList": 
		[
			{
                "Name":"Admin",
                "FullName":"",
                "Status":1, （ 1 - 启用； 0 - 禁用）
                "UserAttr":2, （2 - 管理员，其他都为普通用户）
                "LastTime":"2012-12-02 14:39:14"
				"Group": [{"name":"group1"},{"name":"group2"}]
			}
		]
	}
	*/

	Json::Value root;
	Json::FastWriter writer;
    Json::Value Items = Json::nullValue;
	Json::Value Item;
	std:: string strJson;

	int nCount = (int)vecUserStu.size();
	if(0 == nCount)
	{
		goto END;
	}
	
	for (int i = 0; i < nCount; i++)
	{
		Item["Name"] = UnicodeToUTF8(vecUserStu[i].strUserName);
		Item["FullName"] = UnicodeToUTF8(vecUserStu[i].strFullName);
		Item["Status"] = (int)vecUserStu[i].dwStatus;
		Item["UserAttr"] = (int)vecUserStu[i].dwUserAttr;
		Item["LastTime"] = convertTime2Str(vecUserStu[i].dwLastLoginTime);
		int nGroup = (int)vecUserStu[i].wsGroupList.size();
		Json::Value groupObj;
		for (int j = 0; j < nGroup; j++)
		{
			groupObj["GroupName"] = UnicodeToUTF8( vecUserStu[i].wsGroupList[j] );
			Item["Group"].append(groupObj);
		}
		Items.append(Item);
		Item.clear();
	}

	root["UserList"] = (Json::Value)(Items);
	strJson = writer.write(root);

	root.clear();

END:
	return strJson;
}

/*
* @fn           SecDetect_UserList_ParseJson
* @brief        解析传入的Json串，如果存在User字段，则读取User字段数据存入User结构体
* @param[in]    strUserjson: 传入的Json字符串,见：SecDetect_UserList_GetJson 注释,从数组开始。
				[
					{
                       	"Name":"Admin",
                        "FullName":"",
                        "Status":1, （ 1 - 启用； 0 - 禁用）
                        "UserAttr":2, （2 - 管理员，其他都为普通用户）
                        "LastTime":"2012-12-02 14:39:14"
					}
				]
* @param[out]   vecUserStu : 自定义用户列表结构体。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       mingming.shi
* @date         2021-8-24
*/
BOOL CWLJsonParse::SecDetect_UserList_ParseJson(__in string &strUserjson, __out vector<SECDETECT_USER_STRUCT> &vecUserStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonUsers;
	Json::Reader reader;
	int nCount;

	if(strUserjson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strUserjson, jsonUsers))
	{
		goto _exit_;
	}

    if ( jsonUsers.isNull() || !jsonUsers.isArray())
    {
        goto _exit_;
    }

	nCount = jsonUsers.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_USER_STRUCT stuUser;
		string strTime;
		time_t t;

		_tcsncpy_s(stuUser.strUserName,ArraySize(stuUser.strUserName),UTF8ToUnicode(jsonUsers[i]["Name"].asString()).c_str(),ArraySize(stuUser.strUserName)-1);
		_tcsncpy_s(stuUser.strFullName,ArraySize(stuUser.strFullName),UTF8ToUnicode(jsonUsers[i]["FullName"].asString()).c_str(),ArraySize(stuUser.strFullName)-1);
		stuUser.dwStatus = jsonUsers[i]["Status"].asInt();
		stuUser.dwUserAttr = jsonUsers[i]["UserAttr"].asInt();
		strTime = jsonUsers[i]["LastTime"].asString();
		convertStr2Time(t,strTime);
		stuUser.dwLastLoginTime = (DWORD)t;

		Json::Value tempJsonObj = (Json::Value)jsonUsers[i]["Group"];
		int nGroup = tempJsonObj.size();
		for (int j = 0; j < nGroup; j++)
		{
			WCHAR wtGroupName[128];
			_tcsncpy_s(wtGroupName, ArraySize(stuUser.strUserName),UTF8ToUnicode(tempJsonObj[j]["GroupName"].asString()).c_str(),ArraySize(wtGroupName)-1);
			stuUser.wsGroupList.push_back(wtGroupName);
		}
		vecUserStu.push_back(stuUser);
		stuUser.wsGroupList.clear();
		tempJsonObj.clear();
	}

	bRet = TRUE;

_exit_:
	return bRet;
}


/*
* @fn           SecDetect_ServicesList_GetJson
* @brief        从服务列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]    vecServStu: 自定义用户列表结构体，调用位置 WLDeal
{
	"Services": 
	[
		{
      "Name":"AJRouter",
      "Version":"6.2.18362.1",
      "Company":"Microsoft Corporation",
      "Desp":"AllJoyn Router Service",
      "Running":0 （0 - 已停止； 1 - 正在运行）
		}
	]
}

* @param[out]   
* @return       string: 返回根据用户列表结构体组装的Json字符串
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-25
* @ modify      增加isconfig 1 表示是客户端发给用户的配置项，需要有Start字段，无Running字段，0 表示只是状态项，有Running， 无Start  liudan 2022.9.4
{
	"Services": 
	[
		{
      "Name":"AJRouter",
      "Start":1 // 2 表示要求关闭，1 表示要求开启。
		}
	]
}
*/
std::string CWLJsonParse::SecDetect_ServicesList_GetJson(__in vector<SECDETECT_SERVICES_STRUCT> &vecServStu, BOOL bIsConfig)
{
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strJson;

	int nCount = (int)vecServStu.size();
	if(0 == nCount)
	{
		goto END;
	}
	
	for (int i = 0; i < nCount; i++)
	{
		Item["Name"] = UnicodeToUTF8(vecServStu[i].strName);
		if(FALSE == bIsConfig)
		{
			Item["Running"] = (int)vecServStu[i].dwRunning;
			Item["Version"] = UnicodeToUTF8(vecServStu[i].strVersion);
			Item["Company"] = UnicodeToUTF8(vecServStu[i].strCompany);
			Item["Desp"] = UnicodeToUTF8(vecServStu[i].strDesp);
		}
		else
		{
			Item["Start"] = (int)vecServStu[i].dwStart;
			Item["Remarks"] = UnicodeToUTF8(vecServStu[i].strRemarks);
		}
		Items.append(Item);
	}

	root["Service"] = (Json::Value)(Items);
	strJson = writer.write(root);

	root.clear();

END:
	return strJson;
}

/*
* @fn           SecDetect_ServicesList_ParseJson
* @brief        解析传入的Json串，如果存在Services字段，则读取Services字段数据存入Services结构体
* @param[in]    strServJson: 传入的Json字符串,见：SecDetect_UserList_GetJson 注释,从数组开始。
[
	{
    	"Name":"AJRouter",
    	"Version":"6.2.18362.1",
    	"Company":"Microsoft Corporation",
    	"Desp":"AllJoyn Router Service",
    	"Running":0 （0 - 已停止； 1 - 正在运行）
	}
]
* @param[out]   vecUserStu : 自定义用户列表结构体。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       mingming.shi
* @date         2021-8-25
*/
BOOL CWLJsonParse::SecDetect_ServicesList_ParseJson(__in string &strServJson, __out vector<SECDETECT_SERVICES_STRUCT> &vecServStu)
{
	BOOL bRet = FALSE;

	Json::Value servs;
	Json::Reader reader;
	int nCount;

	if(strServJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strServJson, servs))
	{
		goto _exit_;
	}

    if ( servs.isNull() || !servs.isArray())
    {
        goto _exit_;
    }

	nCount = servs.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_SERVICES_STRUCT stSevMgr;

		_tcsncpy_s(stSevMgr.strName,ArraySize(stSevMgr.strName),
			UTF8ToUnicode(servs[i]["Name"].asString()).c_str(),ArraySize(stSevMgr.strName)-1);
		
		if(servs[i]["Version"].isString())
		{
			_tcsncpy_s(stSevMgr.strVersion,ArraySize(stSevMgr.strVersion),
				UTF8ToUnicode(servs[i]["Version"].asString()).c_str(),ArraySize(stSevMgr.strVersion)-1);
		}
		if(servs[i]["Desp"].isString())
		{
			_tcsncpy_s(stSevMgr.strDesp,ArraySize(stSevMgr.strDesp),
				UTF8ToUnicode(servs[i]["Desp"].asString()).c_str(),ArraySize(stSevMgr.strDesp)-1);
		}
		if(servs[i]["Company"].isString())
		{
			_tcsncpy_s(stSevMgr.strCompany,ArraySize(stSevMgr.strCompany),
				UTF8ToUnicode(servs[i]["Company"].asString()).c_str(),ArraySize(stSevMgr.strCompany)-1);

		}
		if(servs[i]["Running"].isInt())
		{
			stSevMgr.dwRunning = (DWORD)servs[i]["Running"].asInt();
		}

		if(servs[i]["Start"].isInt())
		{
			stSevMgr.dwStart = (DWORD)servs[i]["Start"].asInt();
		}

		if(servs[i]["Remarks"].isString())
		{
			_tcsncpy_s(stSevMgr.strRemarks,ArraySize(stSevMgr.strRemarks),
				UTF8ToUnicode(servs[i]["Remarks"].asString()).c_str(),ArraySize(stSevMgr.strRemarks)-1);
		}
		vecServStu.push_back(stSevMgr);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

//由于SecDetect_ServicesList_GetJson中输出的Json结构有些特别，所以单独解析时需要特殊处理
BOOL CWLJsonParse::SecBaseLine_SevMgr_ParseJson(__in std::string &strServJson, __out vector<SECDETECT_SERVICES_STRUCT> &vecServStu, __out tstring *pError/* = NULL*/)
{
    BOOL bRet = FALSE;
    wostringstream wosStream;

    Json::Value servs;
    Json::Reader reader;
    int nCount;

    if(strServJson.length() == 0)
    {
        wosStream << _T("strServJson.length() == 0");
        goto _exit_;
    }

    if (!reader.parse(strServJson, servs))
    {
        wosStream << _T("parse json failed");
        goto _exit_;
    }

    if ( servs.isNull())
    {
        wosStream << _T("servs is NULL");
        goto _exit_;
    }

    if ( !servs.isMember("Service"))
    {
        wosStream << _T("servs is no member <Service> ");
        goto _exit_;
    }

    servs = (Json::Value)servs["Service"];

    if ( !servs.isArray())
    {
        wosStream << _T("servs is not array");
        goto _exit_;
    }

    vecServStu.clear();
    nCount = servs.size();
    for (int i = 0; i < nCount; i++)
    {
        SECDETECT_SERVICES_STRUCT stSevMgr;
        
        // 服务名称
        _tcsncpy_s(stSevMgr.strName, ArraySize(stSevMgr.strName), UTF8ToUnicode(servs[i]["Name"].asString()).c_str(), ArraySize(stSevMgr.strName) - 1);
        // 服务版本
        _tcsncpy_s(stSevMgr.strVersion, ArraySize(stSevMgr.strVersion), UTF8ToUnicode(servs[i]["Version"].asString()).c_str(), ArraySize(stSevMgr.strVersion) - 1);
        // 服务描述
        _tcsncpy_s(stSevMgr.strDesp, ArraySize(stSevMgr.strDesp), UTF8ToUnicode(servs[i]["Desp"].asString()).c_str(), ArraySize(stSevMgr.strDesp) - 1);
        // 服务公司
        _tcsncpy_s(stSevMgr.strCompany, ArraySize(stSevMgr.strCompany), UTF8ToUnicode(servs[i]["Company"].asString()).c_str(), ArraySize(stSevMgr.strCompany) - 1);
        // 服务状态
        stSevMgr.dwRunning = (DWORD)servs[i]["Running"].asInt();
        // 配置状态
        stSevMgr.dwStart = (DWORD)servs[i]["Start"].asInt();
        // 备注
        _tcsncpy_s(stSevMgr.strRemarks, ArraySize(stSevMgr.strRemarks), UTF8ToUnicode(servs[i]["Remarks"].asString()).c_str(), ArraySize(stSevMgr.strRemarks) - 1);

        vecServStu.push_back(stSevMgr);
    }

    bRet = TRUE;

_exit_:

    if ( pError)
    {
        *pError = wosStream.str();
    }

    return bRet;
}

/*
* share目录的解析方式
* @fn			SecDetect_ShareList_GetJson
* @brief		从共享列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecDetectShare: 自定义用户列表结构体，调用位置 WLDeal
* @param[out]	
* @return		string: 返回根据共享列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-08-24
*/
std::string CWLJsonParse::SecDetect_ShareList_GetJson(__in std::vector<SECDETECT_SHARE_STRUCT> &vecDetectShare)
{
/*
	{
		"Share":
		[
			{
				"ShareName":"ADMIN$",
				"Type":"远程管理",
				"Path":"C:\Windows"
			}
		]
	}
*/
	std::string  strJsonShare = "";
    Json::Value  ShareContent = Json::nullValue;
	Json::Value  root;


	Json::FastWriter writer;
	
	int nCount = (int)vecDetectShare.size();
		
	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["ShareName"] = UnicodeToUTF8(vecDetectShare[i].szShareName);
		Temp["Type"]      = UnicodeToUTF8(vecDetectShare[i].szType);
		Temp["Path"]      = UnicodeToUTF8(vecDetectShare[i].szPath);
		ShareContent.append(Temp);
	}

	root["Share"] = (Json::Value)ShareContent;
	strJsonShare = writer.write(root);
	root.clear();

	return strJsonShare;
}


/*
* @fn           SecDetect_ShareList_ParseJson
* @brief        解析传入的Share Json串，将其字段数据存入User结构体
* @param[in]    strSharejson: 传入的ShareJson字符串
* @param[out]   vecShareStu : 共享信息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-8-24
*/
BOOL CWLJsonParse::SecDetect_ShareList_ParseJson(__in string strSharejson, __out vector<SECDETECT_SHARE_STRUCT> &vecShareStu)
{
	/*传入的Json：
	[
	   {
	     "Path":"",
	     "ShareName":"IPC$",
		 "Type":"远程IPC"
	    },
	   {
	      "Path":"C:\\WINDOWS",
		  "ShareName":"ADMIN$",
		  "Type":"远程管理"
		},
	   {
	     "Path":"C:\\",
		 "ShareName":"C$",
		 "Type":"默认共享"
	    }
	]

	*/
	BOOL               bRet = FALSE;
	Json::Value  ShareArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecShareStu.clear();

	if(strSharejson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strSharejson, ShareArray))
	{
		goto END;
	}

    if ( ShareArray.isNull() || !ShareArray.isArray())
    {
        goto END;
    }

	nCount = ShareArray.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_SHARE_STRUCT shareTemp;  //单独一项
		_tcsncpy_s(shareTemp.szShareName,ArraySize(shareTemp.szShareName),UTF8ToUnicode(ShareArray[i]["ShareName"].asString()).c_str(),ArraySize(shareTemp.szShareName)-1);
		_tcsncpy_s(shareTemp.szType,ArraySize(shareTemp.szType),UTF8ToUnicode(ShareArray[i]["Type"].asString()).c_str(),ArraySize(shareTemp.szType)-1);
		_tcsncpy_s(shareTemp.szPath,ArraySize(shareTemp.szPath),UTF8ToUnicode(ShareArray[i]["Path"].asString()).c_str(),ArraySize(shareTemp.szPath)-1);
		vecShareStu.push_back(shareTemp);	
	}

	bRet = TRUE;

END:
	return bRet;
}


/*
* @fn           SecDetect_ProgramList_GetJson
* @brief        从程序列表结构体获取数据，组装成Json字符串并返回
* @param[in]    vecProgramStu: 安全基线（程序）列表结构体，调用位置 WLDeal
	{
	"Program":[  （程序列表）
                {
                    "Name":"Tools",
                    "Version":"10.1.2.33.656.6",
                    "Company":"XML TOOLS",
                    "InstallDate":"2020/12/02",
                    "InstallPath":"C:\Windows\XML\ ",
                    "Size":"81564KB"
                }
            ]
	}
* @param[out]   
* @return       string: 返回根据已安装程序的列表结构体组装的Json字符串
*               
* @detail      
* @author       mingming.shi
* @date         2021-08-25
*/
std::string CWLJsonParse::SecDetect_ProgramList_GetJson(__in vector<SECDETECT_PROGRAM_STRUCT> &vecProgramStu)
{
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strjson;

	int nCount = (int)vecProgramStu.size();
	for (int i = 0; i < nCount; i++)
	{
		Item["Name"]        = UnicodeToUTF8(vecProgramStu[i].strName);
		Item["Version"]     = UnicodeToUTF8(vecProgramStu[i].strVersion);
		Item["Company"]     = UnicodeToUTF8(vecProgramStu[i].strCompany);
		Item["InstallDate"] = UnicodeToUTF8(vecProgramStu[i].strInstallDate);
		Item["InstallPath"] = UnicodeToUTF8(vecProgramStu[i].strInstallPath);
		Item["Size"]        = (int)vecProgramStu[i].dwSize;
		Items.append(Item);
	}

	root["Program"] = (Json::Value)(Items);
	strjson = writer.write(root);

	root.clear();

	return strjson;
}

/*
* @fn           SecDetect_ProgramList_ParseJson
* @brief        解析传入的Json串，如果存在Program字段，则读取Program字段数据存入Services结构体
* @param[in]    strProgramJson: 传入的Json字符串,见：SecDetect_ProgramList_GetJson 注释,从数组开始。
{
    "Program": [
        {
            "Company": "360安全中心",
            "InstallDate": "2021/10/14",
            "InstallPath": "C:\\Program Files\\360\\360sd",
            "Name": "360杀毒",
            "Size": 250443,
            "Version": "7.0.0.1001"
        },
        {
            "Company": "http://code.google.com/p/dnp3/",
            "InstallDate": "2016/10/27",
            "InstallPath": "",
            "Name": "DNP3 Test Set",
            "Size": 1196,
            "Version": "0.9.4"
        }
    ]
}
* @param[out]   vecProgramStu : 安全基线（程序）列表结构体。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       mingming.shi
* @date         2021-8-26
*/
BOOL CWLJsonParse::SecDetect_ProgramList_ParseJson(__in string &strProgramJson, __out vector<SECDETECT_PROGRAM_STRUCT> &vecProgramStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonPrograms;
	Json::Reader reader;

	int nCount;

	if(strProgramJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strProgramJson, jsonPrograms))
	{
		goto _exit_;
	}

    if ( jsonPrograms.isNull() || !jsonPrograms.isArray())
    {
        goto _exit_;
    }

    //CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_ProgramList_ParseJson Json %S"), strProgramJson.c_str());
	nCount = jsonPrograms.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PROGRAM_STRUCT stuTemp;

		_tcsncpy_s(stuTemp.strName,ArraySize(stuTemp.strName),
			UTF8ToUnicode(jsonPrograms[i]["Name"].asString()).c_str(),ArraySize(stuTemp.strName)-1);
		_tcsncpy_s(stuTemp.strVersion,ArraySize(stuTemp.strVersion),
			UTF8ToUnicode(jsonPrograms[i]["Version"].asString()).c_str(),ArraySize(stuTemp.strVersion)-1);
		_tcsncpy_s(stuTemp.strCompany,ArraySize(stuTemp.strCompany),
			UTF8ToUnicode(jsonPrograms[i]["Company"].asString()).c_str(),ArraySize(stuTemp.strCompany)-1);
		_tcsncpy_s(stuTemp.strInstallDate,ArraySize(stuTemp.strInstallDate),
			UTF8ToUnicode(jsonPrograms[i]["InstallDate"].asString()).c_str(),ArraySize(stuTemp.strInstallDate)-1);
		_tcsncpy_s(stuTemp.strInstallPath,ArraySize(stuTemp.strInstallPath),
			UTF8ToUnicode(jsonPrograms[i]["InstallPath"].asString()).c_str(),ArraySize(stuTemp.strInstallPath)-1);
		stuTemp.dwSize = (DWORD)jsonPrograms[i]["Size"].asInt();

		vecProgramStu.push_back(stuTemp);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}
/*
* @fn           SecDetect_HostInfoList_GetJson
* @brief        安全基线（主机信息）的数据获取
* @param[in]    vecHostInfoStu: 安全基线HostInfo结构体
	{
		"HostInfo":[ （主机信息）
        		{
            		"Host":"JIELUO-PC",
            		"Domain":"WORKGROUP",
            		"OS":"Windows 7 sp1 64bit",
            		"OsVersion":"6.1.7601",
            		"CPU":"Intel(R) Core(TM) i5-6500 CPU @ 3.20GHz",
            		"CPUNum":1, （CPU个数）
            		"CPUFreq":"1696 MHz",
            		"CPUType":"Intel64 Family 6 Model 94 Stepping 3",
            		"BiosProv":"LENOVO",
            		"BiosVer":"ACRSYS - 12f0",
            		"MemorySize":"15.91 GB",
            		"MemoryRev":"7.62 GB"
        		}
	}
* @param[out]   
* @return       string: 如果数据获取成功那个返回true；反之返回false
*               
* @detail		基于原该函数逻辑修改
* @author       mingming.shi
* @date         2021-08-26
*/
std::string CWLJsonParse::SecDetect_HostInfoList_GetJson(__in vector<SECDETECT_HOSTINFO_STRUCT> &vecHostInfoStu)
{
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strjson;

	int nCount = (int)vecHostInfoStu.size();
	for (int i = 0; i < nCount; i++)
	{
		Item["CPUNum"]     = (int)vecHostInfoStu[i].dwCPUNum;
		Item["Host"]       = UnicodeToUTF8(vecHostInfoStu[i].strHost);
		Item["Domain"]     = UnicodeToUTF8(vecHostInfoStu[i].strDomin);
		Item["OS"]         = UnicodeToUTF8(vecHostInfoStu[i].strOS);
		Item["OsVersion"]  = UnicodeToUTF8(vecHostInfoStu[i].strOsVersion);
		Item["CPU"]        = UnicodeToUTF8(vecHostInfoStu[i].strCPU);
		Item["CPUFreq"]    = UnicodeToUTF8(vecHostInfoStu[i].strCPUFreq);
		Item["CPUType"]    = UnicodeToUTF8(vecHostInfoStu[i].strCPUType);
		Item["BiosProv"]   = UnicodeToUTF8(vecHostInfoStu[i].strBiosProv);
		Item["BiosVer"]    = UnicodeToUTF8(vecHostInfoStu[i].strBiosVer);
		Item["MemorySize"] = UnicodeToUTF8(vecHostInfoStu[i].strMemorySize);
		Item["MemoryRev"]  = UnicodeToUTF8(vecHostInfoStu[i].strMemoryRev);

		Items.append(Item);
	}

	root["HostInfo"] = (Json::Value)(Items);
	strjson = writer.write(root);

	root.clear();

	return strjson;
}

/*
* @fn           SecDetect_HostInfoList_ParseJson
* @brief        解析传入的Json串，如果存在HostLine字段，则读取HostLine字段数据存入HostLine结构体

* @param[in]    strAutoRunJson: 传入的Json字符串,见：SecDetect_SecDetect_HostInfoList_ParseJsonList_GetJson 注释,从数组开始。
{
    "HostInfo": [
        {
            "BiosProv": "Phoenix Technologies LTD",
            "BiosVer": "Phoenix Technologies LTD 6.00, 2020/07/22",
            "CPU": "x64 Family 6 Model 165 Stepping 3",
            "CPUFreq": "3096 MHz",
            "CPUNum": 2,
            "CPUType": "Intel(R) Core(TM) i5-10500 CPU @ 3.10GHz",
            "Domain": "WORKGROUP",
            "Host": "WIN-yhl-win7x86",
            "MemoryRev": "2.00 GB",
            "MemorySize": "3.00 GB",
            "OS": "Microsoft Windows 7 旗舰版 ",
            "OsVersion": "6.1.7600  Build 7600"
        }
    ]
}
* @param[out]   vecAutoRunStu : 安全基线（主机信息）列表结构体。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       mingming.shi
* @date         2021-8-26
*/
BOOL CWLJsonParse::SecDetect_HostInfoList_ParseJson(__in string &strHostInfoJson, __out vector<SECDETECT_HOSTINFO_STRUCT> &vecHostInfoStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonHostInfo;
	Json::Reader reader;

	int nCount;

	if(strHostInfoJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strHostInfoJson, jsonHostInfo))
	{
		goto _exit_;
	}

    if ( jsonHostInfo.isNull())
    {
        goto _exit_;
    }

    if ( jsonHostInfo.isNull() || !jsonHostInfo.isArray())
    {
        goto _exit_;
    }

	nCount = jsonHostInfo.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_HOSTINFO_STRUCT stuTemp;

		stuTemp.dwCPUNum = (DWORD)jsonHostInfo[i]["CPUNum"].asInt();

		wcscpy_s(stuTemp.strHost, ArraySize(stuTemp.strHost),
								UTF8ToUnicode(jsonHostInfo[i]["Host"].asString()).c_str());

		wcscpy_s(stuTemp.strDomin, ArraySize(stuTemp.strDomin),
								UTF8ToUnicode(jsonHostInfo[i]["Domain"].asString()).c_str());

		wcscpy_s(stuTemp.strOS, ArraySize(stuTemp.strOS),
								UTF8ToUnicode(jsonHostInfo[i]["OS"].asString()).c_str());

		wcscpy_s(stuTemp.strOsVersion, ArraySize(stuTemp.strOsVersion),
								UTF8ToUnicode(jsonHostInfo[i]["OsVersion"].asString()).c_str());

		wcscpy_s(stuTemp.strCPU, ArraySize(stuTemp.strCPU),
								UTF8ToUnicode(jsonHostInfo[i]["CPU"].asString()).c_str());

		wcscpy_s(stuTemp.strCPUFreq, ArraySize(stuTemp.strCPUFreq),
								UTF8ToUnicode(jsonHostInfo[i]["CPUFreq"].asString()).c_str());

		wcscpy_s(stuTemp.strCPUType, ArraySize(stuTemp.strCPUType),
								UTF8ToUnicode(jsonHostInfo[i]["CPUType"].asString()).c_str());

		wcscpy_s(stuTemp.strBiosProv, ArraySize(stuTemp.strBiosProv),
								UTF8ToUnicode(jsonHostInfo[i]["BiosProv"].asString()).c_str());

		wcscpy_s(stuTemp.strBiosVer, ArraySize(stuTemp.strBiosVer),
								UTF8ToUnicode(jsonHostInfo[i]["BiosVer"].asString()).c_str());

		wcscpy_s(stuTemp.strMemorySize, ArraySize(stuTemp.strMemorySize),
								UTF8ToUnicode(jsonHostInfo[i]["MemorySize"].asString()).c_str());

		wcscpy_s(stuTemp.strMemoryRev, ArraySize(stuTemp.strMemoryRev),
								UTF8ToUnicode(jsonHostInfo[i]["MemoryRev"].asString()).c_str());

		vecHostInfoStu.push_back(stuTemp);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

/*
* @fn           SecDetect_AutoRunList_GetJson
* @brief        安全基线（程序）的数据获取
* @param[in]    vecAutoRunStu: 安全基线Program结构体
	{
	"Program":[  （程序列表）
                {
                    "Name":"Tools",
                    "Version":"10.1.2.33.656.6",
                    "Company":"XML TOOLS",
                    "InstallDate":"2020/12/02",
                    "InstallPath":"C:\Windows\XML\ ",
                    "Size":"81564KB"
                }
            ]
	}
* @param[out]   
* @return       string: 如果数据获取成功那个返回true；反之返回false
*               
* @detail		基于原该函数逻辑修改
* @author       mingming.shi
* @date         2021-08-26
*/
std::string CWLJsonParse::SecDetect_AutoRunList_GetJson(__in vector<SEC_DETECT_AUTO_RUNS_ST> &vecAutoRunStu)
{
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strjson;

	int nCount = (int)vecAutoRunStu.size();
	if(0 == nCount)
	{
		goto END;
	}

	for (int i = 0; i < nCount; i++)
	{
		Item["Type"]		= (int)vecAutoRunStu[i].dwType;
		Item["Description"] = UnicodeToUTF8(vecAutoRunStu[i].szDesp);
		Item["Path"]		= UnicodeToUTF8(vecAutoRunStu[i].szPath);
		Item["Version"]     = UnicodeToUTF8(vecAutoRunStu[i].szVersion);
		Item["Company"]		= UnicodeToUTF8(vecAutoRunStu[i].szCompany);
		Item["CmdLine"]     = UnicodeToUTF8(vecAutoRunStu[i].szCmdLine);
		Item["Name"]		= UnicodeToUTF8(vecAutoRunStu[i].szName);
		Items.append(Item);
	}

	root["AutoRuns"] = (Json::Value)(Items);
	strjson = writer.write(root);

	root.clear();

END:
	return strjson;
}

/*
* @fn           SecDetect_AutoRunList_ParseJson
* @brief        解析传入的Json串，如果存在Program字段，则读取Program字段数据存入Services结构体
* @param[in]    strAutoRunJson: 传入的Json字符串,见：SecDetect_AutoRunList_GetJson 注释,从数组开始。
{
    "AutoRuns": [
        {
            "CmdLine": "\"C:\\Program Files\\VMware\\VMware Tools\\vmtoolsd.exe\" -n vmusr",
            "Company": "VMware, Inc.",
            "Description": "VMware Tools Core Service",
            "Name": "VMware User Process",
            "Path": "c:\\program files\\vmware\\vmware tools\\vmtoolsd.exe",
            "Type": 1,
            "Version": "9.6.1.27366"
        },
        {
            "CmdLine": "\"C:\\Program Files\\IEG\\WorkstationDefender\\Launcher.exe\" -silence",
            "Company": "北京威努特技术有限公司",
            "Description": "主机卫士启动程序",
            "Name": "IEG",
            "Path": "c:\\program files\\ieg\\workstationdefender\\launcher.exe",
            "Type": 1,
            "Version": "3.2.1.20"
        }
    ]
}
* @param[out]   vecAutoRunStu : 安全基线（自启动）列表结构体。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       mingming.shi
* @date         2021-8-26
*/
BOOL CWLJsonParse::SecDetect_AutoRunList_ParseJson(__in string &strAutoRunJson, __out vector<SEC_DETECT_AUTO_RUNS_ST> &vecAutoRunStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonAutoRun;
	Json::Reader reader;

	int nCount;

	if(strAutoRunJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strAutoRunJson, jsonAutoRun))
	{
		goto _exit_;
	}

    if ( jsonAutoRun.isNull() || !jsonAutoRun.isArray())
    {
        goto _exit_;
    }

	nCount = jsonAutoRun.size();

	for (int i = 0; i < nCount; i++)
	{
		SEC_DETECT_AUTO_RUNS_ST stuTemp;

		stuTemp.dwType = (int)jsonAutoRun[i]["Type"].asInt();

		wcscpy_s(stuTemp.szName,ArraySize(stuTemp.szName),
			UTF8ToUnicode(jsonAutoRun[i]["Name"].asString()).c_str());
		wcscpy_s(stuTemp.szDesp,ArraySize(stuTemp.szDesp),
			UTF8ToUnicode(jsonAutoRun[i]["Description"].asString()).c_str());
		wcscpy_s(stuTemp.szPath,ArraySize(stuTemp.szPath),
			UTF8ToUnicode(jsonAutoRun[i]["Path"].asString()).c_str());
		wcscpy_s(stuTemp.szVersion,ArraySize(stuTemp.szVersion),
			UTF8ToUnicode(jsonAutoRun[i]["Version"].asString()).c_str());
		wcscpy_s(stuTemp.szCompany,ArraySize(stuTemp.szCompany),
			UTF8ToUnicode(jsonAutoRun[i]["Company"].asString()).c_str());
		wcscpy_s(stuTemp.szCmdLine,ArraySize(stuTemp.szCmdLine),
			UTF8ToUnicode(jsonAutoRun[i]["CmdLine"].asString()).c_str());

		vecAutoRunStu.push_back(stuTemp);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

/*
* @fn           SecDetect_VulConf_ParseJson
* @brief        解析漏洞检测规则文件Vul.conf
* @param[in]    strJson: 规则文件中保存的Json
* @param[out]   vecVulInfo  : 漏洞信息。
*               vecVulRules : 漏洞信息对应的规则。
*               pStrError   : 解析错误时返回的错误信息
* @return       
*               
* @detail       
* @author       
* @date         2021-09-01
*/
BOOL CWLJsonParse::SecDetect_VulConf_ParseJson(__in string strJson, __out vector<SEC_DETECT_VUL_INFO_ST> &vecVulInfo, __out vector<SEC_DETECT_VUL_RULE_ST> &vecVulRules, __out tstring *pStrError/* = NULL */)
{
    BOOL bRes = FALSE;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  VulInfo;
    wostringstream  wosError;

    std::string strValue = "";
    std::wstring wsContent = _T("");

    strValue = strJson;

    if ( strValue.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, json = ") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    VulInfo = (Json::Value)root[0]["vul"];

    if ( VulInfo.isNull() || !VulInfo.isArray())
    {
        wosError << _T("Outer layer: json is null, or json is not array, json = ") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    int iObject = VulInfo.size();
    for (int i = 0; i < iObject; i++)
    {
        SEC_DETECT_VUL_INFO_ST  stVulInfo;

        //漏洞名称
        if ( VulInfo[i].isMember("name") && VulInfo[i]["name"].isString())
        {
            StrCpyW(stVulInfo.szVulName, UTF8ToUnicode(VulInfo[i]["name"].asString()).c_str());
        }

        //漏洞CVEID
        if ( VulInfo[i].isMember("CVEID") && VulInfo[i]["CVEID"].isString())
        {
            StrCpyW(stVulInfo.szCVEID, UTF8ToUnicode(VulInfo[i]["CVEID"].asString()).c_str());
        }

        //漏洞描述
        if ( VulInfo[i].isMember("detail") && VulInfo[i]["detail"].isString())
        {
            StrCpyW(stVulInfo.szVulDesp, UTF8ToUnicode(VulInfo[i]["detail"].asString()).c_str());
        }

        //漏洞发现日期
        if ( VulInfo[i].isMember("date") && VulInfo[i]["date"].isString())
        {
            StrCpyW(stVulInfo.szVulDate, UTF8ToUnicode(VulInfo[i]["date"].asString()).c_str());
        }

        //漏洞对应端口
        if ( VulInfo[i].isMember("port") && VulInfo[i]["port"].isString())
        {
            StrCpyW(stVulInfo.szPorts, UTF8ToUnicode(VulInfo[i]["port"].asString()).c_str());
        }

        stVulInfo.dwVulInfoID = i + 1; //信息对应多条规则

        //存放结果
        vecVulInfo.push_back(stVulInfo);

        //漏洞规则
        Json::Value  VulRules;
        VulRules = (Json::Value)VulInfo[i]["rule"];

        if ( VulRules.isNull() || !VulRules.isArray())
        {
            wosError << _T("Inner layer: rules' json is null, or rules' json is not array, json = ") << UTF8ToUnicode(strValue).c_str() << _T(",");
            goto END;
        }

        int iCounts = VulRules.size();
        for (int k = 0; k < iCounts; k++)
        {
            SEC_DETECT_VUL_RULE_ST  stVulRule;

            stVulRule.dwVulInfoID = i + 1; //用于确定该条规则对应的漏洞信息。

            //操作系统大版本号
            if ( VulRules[k].isMember("major") && VulRules[k]["major"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["major"].asString());
                stVulRule.dwMajor = (DWORD)_wtoi(wsContent.c_str());
            }

            //操作系统小版本号
            if ( VulRules[k].isMember("minor") && VulRules[k]["minor"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["minor"].asString());
                stVulRule.dwMinor = (DWORD)_wtoi(wsContent.c_str());
            }


            //操作系统的补丁号
            if ( VulRules[k].isMember("sp") && VulRules[k]["sp"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["sp"].asString());
                stVulRule.dwWinSp = (DWORD)_wtoi(wsContent.c_str());
            }

            //操作系统类型
            if ( VulRules[k].isMember("product") && VulRules[k]["product"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["product"].asString());
                stVulRule.dwWinProduct = (DWORD)_wtoi(wsContent.c_str());
            }

            //操作系统内部版本号
            if ( VulRules[k].isMember("buildNumber") && VulRules[k]["buildNumber"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["buildNumber"].asString());
                stVulRule.dwWinBuildNumber = (DWORD)_wtoi(wsContent.c_str());
            }

            //补丁路径
            if ( VulRules[k].isMember("dirpath") && VulRules[k]["dirpath"].isString())
            {
                StrCpyW(stVulRule.szPatchPath, UTF8ToUnicode(VulRules[k]["dirpath"].asString()).c_str());
            }

            //文件路径
            if ( VulRules[k].isMember("filepath") && VulRules[k]["filepath"].isString())
            {
                StrCpyW(stVulRule.szFilePath, UTF8ToUnicode(VulRules[k]["filepath"].asString()).c_str());
            }

            //文件大版本号
            if ( VulRules[k].isMember("fileverMS") && VulRules[k]["fileverMS"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["fileverMS"].asString());
                stVulRule.dwFileVerMS = (DWORD)_wtoi(wsContent.c_str());
            }

            //文件小版本号
            if ( VulRules[k].isMember("fileverLS") && VulRules[k]["fileverLS"].isString())
            {
                wsContent = UTF8ToUnicode(VulRules[k]["fileverLS"].asString());
                stVulRule.dwFileVerLS = (DWORD)_wtoi(wsContent.c_str());
            }


            //补丁信息：PatchID、32位下的文件名、64位下的文件名
            if ( VulRules[k].isMember("kbid") && VulRules[k]["kbid"].isString())
            {
                StrCpyW(stVulRule.szPatchKBID, UTF8ToUnicode(VulRules[k]["kbid"].asString()).c_str());
            }

            if ( VulRules[k].isMember("patch32") && VulRules[k]["patch32"].isString())
            {
                StrCpyW(stVulRule.szPatch32bit, UTF8ToUnicode(VulRules[k]["patch32"].asString()).c_str());
            }

            if ( VulRules[k].isMember("patch64") && VulRules[k]["patch64"].isString())
            {
                StrCpyW(stVulRule.szPatch64bit, UTF8ToUnicode(VulRules[k]["patch64"].asString()).c_str());
            }

            //存放结果
            vecVulRules.push_back(stVulRule);
        }
    }

    bRes = TRUE;

END:
    if ( pStrError)
    {
        *pStrError = wosError.str();
    }

    if ( FALSE == bRes)
    {
        vecVulInfo.clear();
        vecVulRules.clear();
    }

    return bRes;
}


/*
* @fn           SecDetect_VulInfo_GetJson
* @brief        安全检测：漏洞检测构建Json
* @param[in]    vecDefense: 传入的主漏洞检测结果的结构体
* @param[out]   strJson : 漏洞检测的Json。
* @return       
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       
* @date         2021-09-01
*/
std::string CWLJsonParse::SecDetect_VulInfo_GetJson(__in std::vector<SEC_DETECT_VUL_INFO_ST> &vecVulInfo)
{
    /*
	{
		"VulInfo":
        [
             {
                  "VulName":"MS08-067 RPC远程执行漏洞",

             }
         ]
	}
	*/
	std::string strJson = "";
    Json::Value CMDContent = Json::nullValue;
	Json::Value Root;
	Json::Value Item;

	Json::FastWriter writer;
	
	int iCount = (int)vecVulInfo.size();
		
	for (int i = 0; i < iCount; i++)
	{
		Item["VulName"] = UnicodeToUTF8(vecVulInfo[i].szVulName);
		Item["VulDesp"] = UnicodeToUTF8(vecVulInfo[i].szVulDesp);
		Item["Status"]  = (int)vecVulInfo[i].dwStatus;
        Item["PatchID"] = UnicodeToUTF8(vecVulInfo[i].szPatchName);
        Item["CVEID"] = UnicodeToUTF8(vecVulInfo[i].szCVEID);
        Item["Ports"] = UnicodeToUTF8(vecVulInfo[i].szPorts);
		CMDContent.append(Item);
	}
	Root["VulInfo"] = (Json::Value)CMDContent;
	strJson = writer.write(Root);
	Root.clear();

	return strJson;
}

BOOL CWLJsonParse::SecDetect_VulInfo_ParseJson(__in string &strJson, __out vector<SEC_DETECT_VUL_INFO_ST> &vecVulInfo)
{
    /*传入的Json：
	[
	   {
            "VulName":"MS08-067 RPC远程执行漏洞",
            "VulDesp":"存在此漏洞的系统收到精心构造的RPC请求时，可能允许远程执行代码。在Windows 2000、Windows XP和Windows Server 2003系统中，利用这一漏洞，攻击者可以通过恶意构造的网络包直接发起攻击，无需通过认证地运行任意代码，并且获取完整的权限。因此该漏洞常被蠕虫用于大规模的传播和攻击。",
            "Status":1,
            "PatchID":"KB10789612"
	    },
	]

	*/
	BOOL bRet = FALSE;
    int  iCount;

	Json::Value   Item;//解析Json得到的数组
	Json::Reader  reader;
	Json::FastWriter writer;

	vecVulInfo.clear();

	if(strJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strJson, Item))
	{
		goto END;
	}

    if ( Item.isNull() || !Item.isArray())
    {
        goto END;
    }

	iCount = Item.size();

	for (int i = 0; i < iCount; i++)
	{
		SEC_DETECT_VUL_INFO_ST stVulInfo;  //单独一项（比较简单的漏洞信息项）

        //漏洞名称
		wcscpy_s(stVulInfo.szVulName, ARRAY_SIZE(stVulInfo.szVulName), UTF8ToUnicode(Item[i]["VulName"].asString()).c_str());
		//漏洞描述
        wcscpy_s(stVulInfo.szVulDesp, ARRAY_SIZE(stVulInfo.szVulDesp), UTF8ToUnicode(Item[i]["VulDesp"].asString()).c_str());
        //漏洞检测结果
        stVulInfo.dwStatus = Item[i]["Status"].asInt();
        //漏洞补丁
		wcscpy_s(stVulInfo.szPatchName, ARRAY_SIZE(stVulInfo.szPatchName), UTF8ToUnicode(Item[i]["PatchID"].asString()).c_str());
        //漏洞端口
        wcscpy_s(stVulInfo.szPorts, ARRAY_SIZE(stVulInfo.szPorts), UTF8ToUnicode(Item[i]["Ports"].asString()).c_str());

		vecVulInfo.push_back(stVulInfo);	
	}

	bRet = TRUE;

END:
	return bRet;
}


/*
* 分区状态目录的Json组建方式
* @fn			SecDetect_Parition_GetJson
* @brief		从分区状态列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecDetectPartition: 自定义分区状态列表结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonPartition: 返回根据分区状态列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-08-26
*/

std::string CWLJsonParse::SecDetect_Parition_GetJson(__in std::vector<SECDETECT_PARTITION_STRUCT> &vecPatitionStu)
{
	/*
	{
		"Parition":
		[
			{
				"Drive":"C:",
				"TotalSize":"59.67G",
				"FreeSize":"40.13G"
			}
		]
	}
	*/
	std::string strJsonPartition = "";
    Json::Value PartContent = Json::nullValue;
	Json::Value root;

	Json::FastWriter writer;
	
	int nCount = (int)vecPatitionStu.size();
		
	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["Drive"]     = UnicodeToUTF8(vecPatitionStu[i].szDrive);
		Temp["TotalSize"] = UnicodeToUTF8(vecPatitionStu[i].szTotalSize);
		Temp["FreeSize"]  = UnicodeToUTF8(vecPatitionStu[i].szFreeSize);
		PartContent.append(Temp);
	}
	root["Partition"] = (Json::Value)PartContent;
	strJsonPartition = writer.write(root);
	root.clear();

	return strJsonPartition;
}

/*
* @fn           SecDetect_Partition_ParseJson
* @brief        解析传入的Part Json串，将其字段数据存入Part结构体
* @param[in]    strPartJson: 传入的分区信息Json字符串
* @param[out]   vecPartStu : 共享信息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-8-24
*/
BOOL CWLJsonParse::SecDetect_Partition_ParseJson(__in string strPartJson, __out vector<SECDETECT_PARTITION_STRUCT> &vecPartStu)
{
	/*传入的Json：
	[
	   {
	   "Drive":"C:",
	   "TotalSize":"59.67G",
	   "FreeSize":"40.13G"
	    },
	]

	*/
	BOOL               bRet = FALSE;
	Json::Value   PartArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecPartStu.clear();

	if(strPartJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strPartJson, PartArray))
	{
		goto END;
	}

    if ( PartArray.isNull() || !PartArray.isArray())
    {
        goto END;
    }

	nCount = PartArray.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PARTITION_STRUCT PartTemp;  //单独一项
		_tcscpy_s(PartTemp.szDrive,sizeof(PartTemp.szDrive)/sizeof(TCHAR),UTF8ToUnicode(PartArray[i]["Drive"].asString()).c_str());
		_tcscpy_s(PartTemp.szTotalSize,sizeof(PartTemp.szTotalSize)/sizeof(TCHAR),UTF8ToUnicode(PartArray[i]["TotalSize"].asString()).c_str());
		_tcscpy_s(PartTemp.szFreeSize,sizeof(PartTemp.szFreeSize)/sizeof(TCHAR),UTF8ToUnicode(PartArray[i]["FreeSize"].asString()).c_str());
		vecPartStu.push_back(PartTemp);	
	}

	bRet = TRUE;

END:
	return bRet;
}

/*
* @fn           SecDetect_Defernse_GetJson
* @brief        构建主动防御：功能点的Json
* @param[in]    vecDefense: 传入的主动防御功能点的结构体
* @param[out]   strJson : 主动防御功能点的Json。
* @return       
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       
* @date         2021-09-01
*/
std::string CWLJsonParse::SecDetect_Defernse_GetJson(__in std::vector<SEC_DETECT_DEFENSE_ST> &vecDefense)
{
    /*
     {
       "Defense":[
                    {
                        "ProcessCtrl":1,
                        "ProcAudit":1,
                    }
                ]
     }

    */

    std::string strDefenseJson = "";
    Json::Value CMDContent = Json::nullValue;
    Json::Value root;
    Json::Value Item;

    Json::FastWriter writer;

    int nCount = (int)vecDefense.size();

    // 客户端获取的
    for (int i=0; i< nCount; i++)
    {
        //程序白名单  
        Item["ProcessCtrl"]     = (int)vecDefense[i].dwProcessCtrl;
        Item["ProcAudit"] = (int)vecDefense[i].dwProcAudit;
        Item["ProcSystem"]  = (int)vecDefense[i].dwProcSystem;
        Item["ProcOpt"]  = (int)vecDefense[i].dwProcOpt;
        Item["ProcAutoApprove"] = (int)vecDefense[i].dwProcAutoApprove;
        Item["ProcUploadSysWLFile"] = (int)vecDefense[i].dwProcUploadSysWLFile;
        
        //病毒查杀
        Item["VirusRealTime"] = (int)vecDefense[i].dwVirusRealTime;
        Item["VirusScanDeal"] = (int)vecDefense[i].dwVirusScanDeal;
        Item["VirusRealTimeMode"] = (int)vecDefense[i].dwVirusRealTimeMode;
        Item["VirusUsbDef"] = (int)vecDefense[i].dwVirusScanUDisk;

        //网络白名单
        Item["NetSynAttack"]  = (int)vecDefense[i].dwSynAttack;
        Item["NetCtrl"]  = (int)vecDefense[i].dwNetCtrl;

        //安全基线
        Item["SecBLLevel"]  = (int)vecDefense[i].dwSecBLLevel;
        Item["SecBLEnaNum"]  = (int)vecDefense[i].dwSecBLEnaNum;

        //外设管理
        Item["DevCtrlCdrom"]  = (int)vecDefense[i].dwDevCtrlCdrom;
        Item["DevCtrlWlan"]  = (int)vecDefense[i].dwDevCtrlWlan;
        Item["DevCtrlUsbEthernet"]  = (int)vecDefense[i].dwDevCtrlUsbEthernet;
        Item["DevCtrlWiFiWhiteList"]  = (int)vecDefense[i].dwDevCtrlWiFiWhiteList;
        Item["DevCtrlBluetooth"]  = (int)vecDefense[i].dwDevCtrlBluetooth;
        Item["DevCtrlSerPort"]  = (int)vecDefense[i].dwDevCtrlSerialPort;
        Item["DevCtrlParPort"]  = (int)vecDefense[i].dwDevCtrlParallelPort;
        Item["DevCtrlReg"]  = (int)vecDefense[i].dwDevCtrlReg;
        Item["DevCtrlNoRegCom"]  = (int)vecDefense[i].dwDevCtrlNoRegCom;
        Item["DevCtrlNoRegSafe"]  = (int)vecDefense[i].dwDevCtrlNoRegSafe;
        Item["DevCtrlAntiAv"]  = (int)vecDefense[i].dwDevCtrlAntiAv;
		Item["DevCtrlFloppyDisk"]  = (int)vecDefense[i].dwDevCtrlFloppyDisk;   // add V3R7C02
		Item["DevCtrlWpd"]  = (int)vecDefense[i].dwDevCtrlWpd;
		Item["DevCtrlUsbPort"]  = (int)vecDefense[i].dwDevCtrlUsbPort;
		
        //非法外联
        Item["IllegalCnt"]  = (int)vecDefense[i].dwIllegalCnt;

        //双因子认证
        Item["UsbKey"]  = (int)vecDefense[i].dwUsbKey;
        Item["UsbKeyPwd"]  = (int)vecDefense[i].dwSystemPassword;
        Item["UsbKeySafe"]  = (int)vecDefense[i].dwSystemSafeModel;

		//系统加固
		Item["SysProtectScore"] = (int)vecDefense[i].dwSysProtectScore;
		Item["SysProtect"] = (int)vecDefense[i].dwSysProtect;
		Item["MBRProtect"] = (int)vecDefense[i].dwMBRProtect;
		Item["ShadowSnapshotProtect"] = (int)vecDefense[i].dwShadowSnapshotProtect;
		Item["InjectProtect"] = (int)vecDefense[i].dwInjectProtect;
		Item["ProcessKillProtect"] = (int)vecDefense[i].dwProcessKillProtect;
		Item["SystemPasswordProtect"] = (int)vecDefense[i].dwSystemPasswordProtect;
		Item["KernelRunningLoad"] = (int)vecDefense[i].dwKernelRunningLoad;
		Item["DealMode"] = (int)vecDefense[i].dwDealMode;

        //访问控制
        Item["AceCtrlFile"]  = (int)vecDefense[i].dwAceCtrlFile;
        Item["AceCtrlReg"]  = (int)vecDefense[i].dwAceCtrlReg;
        Item["AceCtrlMac"]  = (int)vecDefense[i].dwAceCtrlMac;

        //漏洞防护
        Item["VulDef"]  = (int)vecDefense[i].dwVulDef;

        //勒索诱捕
        Item["Surveil"]  = (int)vecDefense[i].dwSurveil;
        Item["SurvStaticTrap"]  = (int)vecDefense[i].dwSurveilStatic;
        Item["SurvDynamicTrap"]  = (int)vecDefense[i].dwSurveilDynamic;

        //勒索行为分析
        Item["SurvBehavior"]  = (int)vecDefense[i].dwHavioural;
        Item["TrustMode"]  = (int)vecDefense[i].dwZeroTrust;

        //数据保护
        Item["DataGuard"]  = (int)vecDefense[i].dwDataGuard;

        //备份与恢复
        Item["BackUp"]  = (int)vecDefense[i].dwBakMgr;

        //威胁防护
        Item["ThreatData"]  = (int)vecDefense[i].dwThreatData;
        Item["ThreatSysLog"]  = (int)vecDefense[i].dwSystemData;
        Item["ThreatFake"]  = (int)vecDefense[i].dwThreatFake;

        CMDContent.append(Item);
    }

    root["Defense"] = (Json::Value)CMDContent;
    strDefenseJson = writer.write(root);
    root.clear();

    return strDefenseJson;
}

BOOL CWLJsonParse::SecDetect_Defense_ParseJson(__in string &strJson, __out vector<SEC_DETECT_DEFENSE_ST> &vecDefense)
{
    BOOL bRet = FALSE;
    int  iCount;

    Json::Value   Item;//解析Json得到的数组
    Json::Reader  reader;
    Json::FastWriter writer;

    vecDefense.clear();

    if(strJson.length() == 0)
    {
        goto END;
    }

    if (!reader.parse(strJson, Item))
    {
        goto END;
    }

    if ( Item.isNull() || !Item.isArray())
    {
        goto END;
    }

    iCount = Item.size();

    for (int i = 0; i < iCount; i++)
    {
        SEC_DETECT_DEFENSE_ST stDefense;  

        //程序白名单
        stDefense.dwProcessCtrl = Item[i]["ProcessCtrl"].asInt();
        stDefense.dwProcAudit = Item[i]["ProcAudit"].asInt();
        stDefense.dwProcSystem = Item[i]["ProcSystem"].asInt();
        stDefense.dwProcOpt = Item[i]["ProcOpt"].asInt();
        stDefense.dwProcAutoApprove = Item[i]["ProcAutoApprove"].asInt();
        stDefense.dwProcUploadSysWLFile = Item[i]["ProcUploadSysWLFile"].asInt();

        //病毒防护、病毒查杀
        stDefense.dwVirusRealTime  = Item[i]["VirusRealTime"].asInt();
        stDefense.dwVirusScanDeal  = Item[i]["VirusScanDeal"].asInt();
        stDefense.dwVirusRealTimeMode = Item[i]["VirusRealTimeMode"].asInt();
        stDefense.dwVirusScanUDisk  = Item[i]["VirusUsbDef"].asInt();

        //网络白名单
        stDefense.dwSynAttack = Item[i]["NetSynAttack"].asInt();
        stDefense.dwNetCtrl = Item[i]["NetCtrl"].asInt();

        //安全基线
        stDefense.dwSecBLLevel = Item[i]["SecBLLevel"].asInt();
        stDefense.dwSecBLEnaNum = Item[i]["SecBLEnaNum"].asInt();

        //外设管理
        stDefense.dwDevCtrlCdrom = Item[i]["DevCtrlCdrom"].asInt();
        stDefense.dwDevCtrlWlan = Item[i]["DevCtrlWlan"].asInt();
        stDefense.dwDevCtrlUsbEthernet = Item[i]["DevCtrlUsbEthernet"].asInt();
        stDefense.dwDevCtrlWiFiWhiteList = Item[i]["DevCtrlWiFiWhiteList"].asInt();
        stDefense.dwDevCtrlBluetooth = Item[i]["DevCtrlBluetooth"].asInt();
        stDefense.dwDevCtrlSerialPort = Item[i]["DevCtrlSerPort"].asInt();
        stDefense.dwDevCtrlParallelPort = Item[i]["DevCtrlParPort"].asInt();
		stDefense.dwDevCtrlFloppyDisk = Item[i]["DevCtrlFloppyDisk"].asInt();
		stDefense.dwDevCtrlWpd = Item[i]["DevCtrlWpd"].asInt();
		stDefense.dwDevCtrlUsbPort = Item[i]["DevCtrlUsbPort"].asInt();
        stDefense.dwDevCtrlReg = Item[i]["DevCtrlReg"].asInt();
        stDefense.dwDevCtrlNoRegCom = Item[i]["DevCtrlNoRegCom"].asInt();
        stDefense.dwDevCtrlNoRegSafe = Item[i]["DevCtrlNoRegSafe"].asInt();
        stDefense.dwDevCtrlAntiAv = Item[i]["DevCtrlAntiAv"].asInt();

        //非法外联
        stDefense.dwIllegalCnt = Item[i]["IllegalCnt"].asInt();

        //双因子认证
        stDefense.dwUsbKey = Item[i]["UsbKey"].asInt();
        stDefense.dwSystemPassword = Item[i]["UsbKeyPwd"].asInt();
        stDefense.dwSystemSafeModel = Item[i]["UsbKeySafe"].asInt();

		//系统加固
		stDefense.dwSysProtectScore = Item[i]["SysProtectScore"].asInt();
		stDefense.dwSysProtect = Item[i]["SysProtect"].asInt();
		stDefense.dwMBRProtect = Item[i]["MBRProtect"].asInt();
		stDefense.dwShadowSnapshotProtect = Item[i]["ShadowSnapshotProtect"].asInt();
		stDefense.dwInjectProtect = Item[i]["InjectProtect"].asInt();
		stDefense.dwProcessKillProtect = Item[i]["ProcessKillProtect"].asInt();
		stDefense.dwSystemPasswordProtect = Item[i]["SystemPasswordProtect"].asInt();
		stDefense.dwKernelRunningLoad = Item[i]["KernelRunningLoad"].asInt();
		stDefense.dwDealMode = Item[i]["DealMode"].asInt();

        //访问控制
        stDefense.dwAceCtrlFile = Item[i]["AceCtrlFile"].asInt();
        stDefense.dwAceCtrlReg = Item[i]["AceCtrlReg"].asInt();
        stDefense.dwAceCtrlMac = Item[i]["AceCtrlMac"].asInt();

        //漏洞防护
        stDefense.dwVulDef = Item[i]["VulDef"].asInt();
        
        //勒索诱捕
        stDefense.dwSurveil = Item[i]["Surveil"].asInt() ;
        stDefense.dwSurveilStatic = Item[i]["SurvStaticTrap"].asInt();
        stDefense.dwSurveilDynamic = Item[i]["SurvDynamicTrap"].asInt();

        //勒索行为分析
        stDefense.dwHavioural = Item[i]["SurvBehavior"].asInt();
        stDefense.dwZeroTrust = Item[i]["TrustMode"].asInt();

        //数据保护
        stDefense.dwDataGuard = Item[i]["DataGuard"].asInt();

        //备份与恢复
        stDefense.dwBakMgr = Item[i]["BackUp"].asInt();

        //威胁防护
        stDefense.dwThreatData = Item[i]["ThreatData"].asInt();
        stDefense.dwSystemData = Item[i]["ThreatSysLog"].asInt();
        stDefense.dwThreatFake = Item[i]["ThreatFake"].asInt();

        vecDefense.push_back(stDefense);	
    }

    bRet = TRUE;

END:
    return bRet;
}


/*
* @fn           SecDetect_Score_GetJson
* @brief        构建主动防御：评分分数的Json
* @param[in]    vecDefense: 传入的主动防御评分分数的结构体
* @param[out]   strJson : 主动防御评分分数的Json。
* @return       
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       
* @date         2021-09-01
*/
std::string CWLJsonParse::SecDetect_Score_GetJson(__in std::vector<SEC_DETECT_DEFENSE_ST> &vecDefense, __in std::wstring wsScoreLevel)
{
    /*
     {
         "Score":[
                    {
                        "ProcScore":20,
                        "NetScore":10,
                        "SecLevel":"中合规"
                    }
                 ]
    }
    */

    std::string strScoreJson = "";
    Json::Value CMDContent = Json::nullValue;
    Json::Value root;
    Json::Value Item;

    Json::FastWriter writer;

    int nCount = (int)vecDefense.size();

	DWORD dwDlgMainType = WLUtils::GetDlgMainType();

    // 客户端获取的
    for (int i=0; i< nCount; i++)
    {
        //程序白名单/病毒查杀
        Item["ProcScore"]     = (int)vecDefense[i].dwProcScore;

        //网络白名单
        Item["NetScore"]  = (int)vecDefense[i].dwNetScore;

        //安全基线
        Item["SecBLScore"]  = (int)vecDefense[i].dwSecBLScore;

        //外设管理
        Item["DevCtrlScore"]  = (int)vecDefense[i].dwDevCtrlScore;

        //非法外联
        Item["IllegalCntScore"]  = (int)vecDefense[i].dwIllegalCntScore;

        //双因子认证
        Item["UsbKeyScore"]  = (int)vecDefense[i].dwUsbKeyScore;

		//系统加固
		Item["SysProtectScore"] = (int)vecDefense[i].dwSysProtectScore;

        //访问控制
        Item["AceCtrlScore"]  = (int)vecDefense[i].dwAceCtrlScore;

        //漏洞防护
        Item["VulDefScore"]  = (int)vecDefense[i].dwVulDefScore;

        //勒索诱捕
        Item["SurvTrapScore"]  = (int)vecDefense[i].dwSurveilScore;

        //勒索行为检测
        Item["SurvBehaviorScore"]  = (int)vecDefense[i].dwHaviouralScore;

        //数据保护
        Item["DataGuardScore"]  = (int)vecDefense[i].dwDataGuardScore;

        //备份与恢复
        Item["BackUpScore"]  = (int)vecDefense[i].dwBakMgrScore;

        //威胁防护
        Item["ThreatScore"]  = (int)vecDefense[i].dwThreatexamScore;

        //总得分
		Item["SecScore"]  = (int)vecDefense[i].TotalScore();
		
		//总分
		if (LCS_PRODUCT_IEG == dwDlgMainType)
		{
			Item["FullScore"]  = (int)FULL_SCORE_IEG;
		}
		else if (LCS_PRODUCT_EDR == dwDlgMainType)
		{
			Item["FullScore"]  = (int)FULL_SCORE_EDR;
		}
        else if (LCS_PRODUCT_ARS == dwDlgMainType)
        {
            Item["FullScore"]  = (int)FULL_SCORE_ARS;
        }

        //合规等级
        Item["SecLevel"]  = UnicodeToUTF8(wsScoreLevel.c_str());

        CMDContent.append(Item);
    }

    root["Score"] = (Json::Value)CMDContent;
    strScoreJson = writer.write(root);
    root.clear();

    return strScoreJson;
}

BOOL CWLJsonParse::SecDetect_Score_ParseJson(__in string &strJson, __out vector<SEC_DETECT_DEFENSE_ST> &vecDefense)
{
    BOOL bRet = FALSE;
    int  iCount;

    Json::Value   Item;//解析Json得到的数组
    Json::Reader  reader;
    Json::FastWriter writer;

    vecDefense.clear();

    if(strJson.length() == 0)
    {
        goto END;
    }

    if (!reader.parse(strJson, Item))
    {
        goto END;
    }

    if ( Item.isNull() || !Item.isArray())
    {
        goto END;
    }

    iCount = Item.size();

	DWORD dwDlgMainType = WLUtils::GetDlgMainType();

    for (int i = 0; i < iCount; i++)
    {
        SEC_DETECT_DEFENSE_ST stDefense;  

        //程序白名单
        stDefense.dwProcScore = Item[i]["ProcScore"].asInt();

        //网络白名单
        stDefense.dwNetScore = Item[i]["NetScore"].asInt();

        //安全基线
        stDefense.dwSecBLScore = Item[i]["SecBLScore"].asInt();

        //外设管理
        stDefense.dwDevCtrlScore = Item[i]["DevCtrlScore"].asInt();

        //非法外联
        stDefense.dwIllegalCntScore = Item[i]["IllegalCntScore"].asInt();

        //双因子认证
        stDefense.dwUsbKeyScore = Item[i]["UsbKeyScore"].asInt();

		//系统加固
	    stDefense.dwSysProtectScore = Item[i]["SysProtectScore"].asInt();

        //访问控制
        stDefense.dwAceCtrlScore = Item[i]["AceCtrlScore"].asInt();

        //漏洞防护
        stDefense.dwVulDefScore = Item[i]["VulDefScore"].asInt();

        //勒索诱捕
        stDefense.dwSurveilScore = Item[i]["SurvTrapScore"].asInt();

        //勒索行为检测
        stDefense.dwHaviouralScore = Item[i]["SurvBehaviorScore"].asInt();

        //数据保护
        stDefense.dwDataGuardScore = Item[i]["DataGuardScore"].asInt();

        //备份与恢复
        stDefense.dwBakMgrScore = Item[i]["BackUpScore"].asInt();

        //威胁防护
        stDefense.dwThreatexamScore = Item[i]["ThreatScore"].asInt();

        vecDefense.push_back(stDefense);	
    }

    bRet = TRUE;

END:
    return bRet;
}

/*
* @fn           SecDetect_WeakPwd_GetJson
* @brief        构建弱口令：功能点的Json
* @param[in]    vecWeakPwd: 传入的弱口令功能点的结构体
* @param[out]   strJson : 弱口令功能点的Json。
* @return       
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       
* @date         
*/
std::string CWLJsonParse::SecDetect_WeakPwd_GetJson(__in std::vector<SEC_DETECT_WEAK_PWD_ST> &vecWeakPwd)
{
    /*
    {
        "Type": 1,          //检测类型：0未知，1操作系统，2远程登录，3数据库MySQL
        "UserName":"Admin", //账户名
        "State":1           //风险状态：0 存在“弱口令”风险，1 无风险
    },
    */

    std::string strJson = "";
    Json::Value CMDContent = Json::nullValue;
    Json::Value root;
    Json::Value Item;

    Json::FastWriter writer;

    int nCount = (int)vecWeakPwd.size();
    for (int i = 0; i < nCount; i++)
    {
        Item["Type"]     = (int)vecWeakPwd[i].dwType;
        Item["UserName"] = UnicodeToUTF8(vecWeakPwd[i].szUserName);
        Item["IsRisk"]   = (int)vecWeakPwd[i].dwIsRisk;

        CMDContent.append(Item);
    }

    root["WeakPwd"] = (Json::Value)CMDContent;
    strJson = writer.write(root);
    root.clear();

    return strJson;
}

BOOL CWLJsonParse::SecDetect_WeakPwd_ParseJson(__in string &strJson, __out vector<SEC_DETECT_WEAK_PWD_ST> &vecWeakPwd)
{
    BOOL bRet = FALSE;
    int  iCount;

    Json::Value   Item;//解析Json得到的数组
    Json::Reader  reader;
    Json::FastWriter writer;

    vecWeakPwd.clear();

    if(strJson.length() == 0)
    {
        goto END;
    }

    if (!reader.parse(strJson, Item))
    {
        goto END;
    }

    if ( Item.isNull() || !Item.isArray())
    {
        goto END;
    }

    iCount = Item.size();
    for (int i = 0; i < iCount; i++)
    {
        SEC_DETECT_WEAK_PWD_ST stWeakPwd;  

        stWeakPwd.dwType = Item[i]["Type"].asInt();
        _tcscpy_s(stWeakPwd.szUserName, _countof(stWeakPwd.szUserName), UTF8ToUnicode(Item[i]["UserName"].asString()).c_str());
        stWeakPwd.dwIsRisk = Item[i]["IsRisk"].asInt();

        vecWeakPwd.push_back(stWeakPwd);	
    }

    bRet = TRUE;

END:
    return bRet;
}

/*
* 进程目录的Json组建
* @fn			SecDetect_Process_GetJson
* @brief		从进程列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecDetectProc: 自定义用户列表结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonProc: 返回根据进程列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-08-27
*/
std::string CWLJsonParse::SecDetect_Process_GetJson(__in std::vector<SECDETECT_PROCESS_STRUCT> &vecDetectProc)
{
/*
	{
		"Process":
		[
			{
				"Name":"winlogon.exe",
				"PID":"588",
				"Company":"Microsoft Corporation",
				"Version":"10.0.18362.1",
				"Path":"C:\Windows\System32\winlogon.exe"
			}
		]
	}
*/
	std::string    strJsonProc = "";
    Json::Value    ProcContent = Json::nullValue;
	Json::Value    root;

	Json::FastWriter writer;
	
	int nCount = (int)vecDetectProc.size();
		
	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["PID"]       = (int)vecDetectProc[i].dwPID;
		Temp["Name"]      = UnicodeToUTF8(vecDetectProc[i].szName);
		Temp["Company"]   = UnicodeToUTF8(vecDetectProc[i].szCompany);
		Temp["Version"]   = UnicodeToUTF8(vecDetectProc[i].szVersion);
		Temp["Path"]      = UnicodeToUTF8(vecDetectProc[i].szPath);
		ProcContent.append(Temp);
	}

	root["Process"] = (Json::Value)ProcContent;
	strJsonProc = writer.write(root);
	root.clear();

	return strJsonProc;
}


/*
* @fn           SecDetect_Process_ParseJson
* @brief        解析传入的进程列表Json串，将其字段数据存入Process结构体
* @param[in]    strProcJson: 传入的ProcessJson字符串
* @param[out]   vecProcStu : 进程信息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-8-26
*/
BOOL CWLJsonParse::SecDetect_Process_ParseJson(__in string strProcJson, __out vector<SECDETECT_PROCESS_STRUCT> &vecProcStu)
{
	/*传入的Json：
	 [
	      {
	          "Name":"winlogon.exe",
              "PID":"588",
	          "Company":"Microsoft Corporation",
	          "Version":"10.0.18362.1",
	          "Path":"C:\Windows\System32\winlogon.exe"
		}
	]
	*/
	BOOL               bRet = FALSE;
	Json::Value   ProcArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecProcStu.clear();

	if(strProcJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strProcJson, ProcArray))
	{
		goto END;
	}

    if ( ProcArray.isNull() || !ProcArray.isArray())
    {
        goto END;
    }

	nCount = ProcArray.size();
	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PROCESS_STRUCT ProcTemp;  //单独一项
		_tcscpy_s(ProcTemp.szName,   sizeof(ProcTemp.szName)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Name"].asString()).c_str());
		_tcscpy_s(ProcTemp.szCompany,sizeof(ProcTemp.szCompany)/sizeof(TCHAR),UTF8ToUnicode(ProcArray[i]["Company"].asString()).c_str());
		_tcscpy_s(ProcTemp.szVersion,sizeof(ProcTemp.szVersion)/sizeof(TCHAR),UTF8ToUnicode(ProcArray[i]["Version"].asString()).c_str());
		_tcscpy_s(ProcTemp.szPath,   sizeof(ProcTemp.szPath)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Path"].asString()).c_str());
		ProcTemp.dwPID = (DWORD)ProcArray[i]["PID"].asInt();
		vecProcStu.push_back(ProcTemp);	
	}

	bRet = TRUE;
END:
	return bRet;
}

/*
* 安全基线列表的Json组建
* @fn			SecDetect_BaseLine_GetJson
* @brief		从安全基线列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecDetectBase: 自定义安全基线列表结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonBase: 返回根据安全基线列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-08-28
*/
std::string CWLJsonParse::SecDetect_BaseLine_GetJson(__in std::vector<SECDETECT_BASELINE_STRUCT> &vecDetectBase)
{
/*
	{
		"SecBaseLine":
		[
			{
				"Name":"审计操作系统登录事件",
				"ID":"1",
				"Desp":"(待补充)",
				"Status":"0",   //（0-不符合，1符合，2-不支持此系统）
				"Other":"(可选)",
                "Code":"dwAutoRun", // 兼容 USM 使用的转义字符，本地不使用
			}
		]
	}
*/
	std::string    strJsonBase = "";
    std::wstring   wsSecBLCode = _T("");
    Json::Value    BaseContent = Json::nullValue;
	Json::Value    root;

	Json::FastWriter writer;
	
	int nCount = (int)vecDetectBase.size();
		
	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["ID"]       = (int)vecDetectBase[i].dwID;
		Temp["Name"]     = UnicodeToUTF8(vecDetectBase[i].szKeyName);
		Temp["Desp"]     = UnicodeToUTF8(vecDetectBase[i].szDesp);
		Temp["Status"]   = (int)vecDetectBase[i].dwStatus;
		Temp["Other"]    = UnicodeToUTF8(vecDetectBase[i].szOther);

        wsSecBLCode = GetSecDetectKey( vecDetectBase[i].szKeyName );
        Temp["Code"]     = UnicodeToUTF8(wsSecBLCode);

		BaseContent.append(Temp);
	}

	root["SecBaseLine"] = (Json::Value)BaseContent;
	strJsonBase = writer.write(root);
	root.clear();

	return strJsonBase;
}


/*
* @fn           SecDetect_BaseLine_ParseJson
* @brief        解析传入的安全基线列表Json串，将其字段数据存入BaseLine结构体
* @param[in]    strBaseJson: 传入的安全基线Json字符串
* @param[out]   vecBaseStu : 安全基线信息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-8-28
*/
BOOL CWLJsonParse::SecDetect_BaseLine_ParseJson(__in string &strBaseJson, __out vector<SECDETECT_BASELINE_STRUCT> &vecBaseStu)
{
	/*传入的Json：
	 [
	      {
	          "Name":"审计操作系统登录事件",
              "ID":"1",
	          "Desp":"(待补充)",
	          "Status":"0",    //（0-不符合，1符合，2-不支持此系统）
	          "Other":"(可选)"
		}
	]
	*/
	BOOL               bRet = FALSE;
	Json::Value   BaseArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecBaseStu.clear();

	if(strBaseJson.length() == 0)
	{
		goto END;
	}

    if ( !reader.parse(strBaseJson, BaseArray))
    {
        goto END;
    }

    if ( BaseArray.isNull() || !BaseArray.isArray())
    {
        goto END;
    }

	nCount = BaseArray.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_BASELINE_STRUCT BaseTemp;  //单独一项
		BaseTemp.dwID     = (DWORD)BaseArray[i]["ID"].asInt();
		BaseTemp.dwStatus = (DWORD)BaseArray[i]["Status"].asInt();

		_tcscpy_s(BaseTemp.szKeyName, sizeof(BaseTemp.szKeyName)/sizeof(TCHAR), UTF8ToUnicode(BaseArray[i]["Name"].asString()).c_str());
		_tcscpy_s(BaseTemp.szDesp, sizeof(BaseTemp.szDesp)/sizeof(TCHAR), UTF8ToUnicode(BaseArray[i]["Desp"].asString()).c_str());
		_tcscpy_s(BaseTemp.szOther, sizeof(BaseTemp.szOther)/sizeof(TCHAR), UTF8ToUnicode(BaseArray[i]["Other"].asString()).c_str());
		
		vecBaseStu.push_back(BaseTemp);	
	}

	bRet = TRUE;

END:
	return bRet;
}


/*
* 端口信息目录的Json组建
* @fn			SecDetect_Port_GetJson
* @brief		从端口信息列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecDetectPort: 自定义端口信息列表结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonPort: 返回根据端口信息列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-08-31
*/
std::string CWLJsonParse::SecDetect_Port_GetJson(__in std::vector<SECDETECT_PORT_STRUCT> &vecDetectPort)
{
/*
	{
		[
			{
				"PortNumber":139,
				"PID":980,
				"Protocol":1,
				"State":"(待定),"
				"ProPath":"C:\Windows\System32\winlogon.exe"
			}
		]
	}
*/
	std::string    strJsonPort = "";
    Json::Value    PortContent = Json::nullValue;
	Json::Value    root;
	Json::FastWriter writer;
	
	int nCount = (int)vecDetectPort.size();

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["PID"]             = (int)vecDetectPort[i].dwPID;
		Temp["Company"]         = UnicodeToUTF8(vecDetectPort[i].szCompany);
		Temp["Version"]         = UnicodeToUTF8(vecDetectPort[i].szVersion);
		Temp["RunTime"]         = UnicodeToUTF8(vecDetectPort[i].szRunTime);
		Temp["ProcPath"]        = UnicodeToUTF8(vecDetectPort[i].szProPath);
		Temp["Protocol"]        = (int)vecDetectPort[i].dwProtocol;
		Temp["PortNumber"]      = (int)vecDetectPort[i].usPortNumber;
		Temp["FileDescription"] = UnicodeToUTF8(vecDetectPort[i].szDesp);
		PortContent.append(Temp);
	}

	root["PortList"] = (Json::Value)PortContent;
	strJsonPort = writer.write(root);

	root.clear();

	return strJsonPort;
}


/*
* @fn           SecDetect_Port_ParseJson
* @brief        解析传入的端口信息列表Json串，将其字段数据存入Port结构体
* @param[in]    strPortJson: 传入的端口信息Json字符串
* @param[out]   vecPortStu : 端口信息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-8-31
*/
BOOL CWLJsonParse::SecDetect_Port_ParseJson(__in string strPortJson, __out vector<SECDETECT_PORT_STRUCT> &vecPortStu)
{
	/*传入的Json：
	 [
	      {
		    "PortNumber":139,
		    "Protocol":1,
		    "PID":980,
		    "State":"(待定),"
		    "ProPath":"C:\Windows\System32\winlogon.exe"
		}
	]
	*/
	BOOL               bRet = FALSE;
	Json::Value   PortArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecPortStu.clear();


	if(strPortJson.length() == 0)
	{
		goto END;
	}

    //CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson Json %S"), strPortJson.c_str()); // {"PortList":null}

	if (!reader.parse(strPortJson, PortArray))
	{
		goto END;
	}

    if ( PortArray.isNull() || !PortArray.isArray())
    {
        goto END;
    }
    
	nCount = PortArray.size();
	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson nCount %d"),nCount);

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PORT_STRUCT stPort;  //单独一项

        stPort.dwPID        = (DWORD)PortArray[i]["PID"].asInt();

		stPort.dwProtocol   = (DWORD)PortArray[i]["Protocol"].asInt();

		stPort.usPortNumber = (WORD)PortArray[i]["PortNumber"].asInt();

		_tcscpy_s(stPort.szCompany, sizeof(stPort.szCompany)/sizeof(stPort.szCompany[0]), UTF8ToUnicode(PortArray[i]["Company"].asString()).c_str());

		_tcscpy_s(stPort.szVersion, sizeof(stPort.szVersion)/sizeof(stPort.szVersion[0]), UTF8ToUnicode(PortArray[i]["Version"].asString()).c_str());

		_tcscpy_s(stPort.szDesp,    sizeof(stPort.szDesp)/sizeof(stPort.szDesp[0]),    UTF8ToUnicode(PortArray[i]["FileDescription"].asString()).c_str());

		_tcscpy_s(stPort.szProPath, sizeof(stPort.szProPath)/sizeof(stPort.szProPath[0]), UTF8ToUnicode(PortArray[i]["ProcPath"].asString()).c_str());

		_tcscpy_s(stPort.szRunTime, sizeof(stPort.szRunTime)/sizeof(stPort.szRunTime[0]), UTF8ToUnicode(PortArray[i]["RunTime"].asString()).c_str());

		vecPortStu.push_back(stPort);	
	}

	bRet = TRUE;

END:
	return bRet;
}

/*
* 网络配置目录的Json组建
* @fn			SecDetect_NetConfig_GetJson
* @brief		从网络配置列表的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecNetConfig: 自定义网络配置信息列表结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonNetConfig: 返回根据网络配置信息列表结构体组装的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-09-02
*/
std::string CWLJsonParse::SecDetect_NetConfig_GetJson(__in std::vector<SECDETECT_NET_CONFIG_STRUT> &vecNetConfig)
{
/*
	{
		[
			{
				"NetName":本地连接,
				"PhyAddr":"8C:16:45:26:30:BF",
				"DHCP":0,  (0关闭，1开启)
				"IPAddr":[
				    "192.168.2.68",
				    "192.168.3.68",
				    "192.168.10.68"
				    ],
				"SubMask":[
				    "255.255.255.0",
					"255.255.255.0",
					"255.255.255.0"
					],
				"Gateway":[
					"192.168.3.1"
					],
				"DNS":[
					"192.168.3.1",
					"192.168.10.1"
					],
			}
		]
	}
*/
	std::string  strJsonNetConfig = "";
    Json::Value  ConfigContent = Json::nullValue;
	Json::Value  root;
	Json::FastWriter writer;
	
	int nCount = (int)vecNetConfig.size();

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value               Temp;
		Json::Value      IPAddrContent;
		Json::Value     SubMaskContent;
		Json::Value     GatewayContent;
		Json::Value         DNSContent;

		Temp["DHCP"]         = (int)vecNetConfig[i].dwDHCP;
		Temp["PhyAddr"]      = UnicodeToUTF8(vecNetConfig[i].szPhyAddr);
		Temp["NetName"]      = UnicodeToUTF8(vecNetConfig[i].szNet);

		//IP列表
		for (int j = 0; j < vecNetConfig[i].vecIPAddr.size(); j++)
		{
			IPAddrContent.append(UnicodeToUTF8(vecNetConfig[i].vecIPAddr[j]));
		}
		Temp["IPAddr"] = IPAddrContent;

		//子网掩码列表
		for (int j = 0; j < vecNetConfig[i].vecSubMask.size(); j++)
		{
			SubMaskContent.append(UnicodeToUTF8(vecNetConfig[i].vecSubMask[j]));
		}
		Temp["SubMask"] = SubMaskContent;

		//网关列表
		for (int j = 0; j < vecNetConfig[i].vecGateway.size(); j++)
		{
			GatewayContent.append(UnicodeToUTF8(vecNetConfig[i].vecGateway[j]));
		}
		Temp["Gateway"] = GatewayContent;

		//网关列表
		for (int j = 0; j < vecNetConfig[i].vecDNS.size(); j++)
		{
			DNSContent.append(UnicodeToUTF8(vecNetConfig[i].vecDNS[j]));
		}
		Temp["DNS"] = DNSContent;

		ConfigContent.append(Temp);
	}

	root["NetConfig"] = (Json::Value)ConfigContent;
	strJsonNetConfig = writer.write(root);
	root.clear();

	return strJsonNetConfig;
}


/*
* @fn           SecDetect_NetConfig_ParseJson
* @brief        解析传入的网络配置列表Json串，将其字段数据存入Port结构体
* @param[in]    strConfigJson: 传入的网络配置信息Json字符串
* @param[out]   vecConfigStu : 网络配置息结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True；反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-09-02
*/
BOOL CWLJsonParse::SecDetect_NetConfig_ParseJson(__in string strConfigJson, __out vector<SECDETECT_NET_CONFIG_STRUT> &vecConfigStu)
{
	/*传入的Json：
	[
	    {
		    "NetName":本地连接,
			"PhyAddr":"8C:16:45:26:30:BF",
			"DHCP":0,  (0关闭，1开启)
			"IPAddr":[
				    "192.168.2.68",
					"192.168.3.68",
					"192.168.10.68"
			],
			"SubMask":[
				    "255.255.255.0",
					"255.255.255.0",
					"255.255.255.0"
			],
			"Gateway":[
				"192.168.3.1"
			],
			"DNS":[
				"192.168.3.1",
					"192.168.10.1"
			],
	     }
	]
*/
	BOOL               bRet = FALSE;
	Json::Value   NetConfigArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecConfigStu.clear();

	if(strConfigJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strConfigJson, NetConfigArray))
	{
		goto END;
	}

    if ( NetConfigArray.isNull() || !NetConfigArray.isArray())
    {
        goto END;
    }

	nCount = NetConfigArray.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_NET_CONFIG_STRUT ConfigTemp;  //单独一项
		Json::Value      IPAddrContent;
		Json::Value     SubMaskContent;
		Json::Value     GatewayContent;
		Json::Value         DNSContent;

		ConfigTemp.dwDHCP       = (DWORD)NetConfigArray[i]["DHCP"].asInt();
		_tcscpy_s(ConfigTemp.szNet,    sizeof(ConfigTemp.szNet)/sizeof(TCHAR),    UTF8ToUnicode(NetConfigArray[i]["NetName"].asString()).c_str());
		_tcscpy_s(ConfigTemp.szPhyAddr,sizeof(ConfigTemp.szPhyAddr)/sizeof(TCHAR),UTF8ToUnicode(NetConfigArray[i]["PhyAddr"].asString()).c_str());

		IPAddrContent = NetConfigArray[i]["IPAddr"];
		if (IPAddrContent.isArray())
		{
			for (int j = 0; j < IPAddrContent.size(); j++)
			{
				wstring ipaddr = UTF8ToUnicode(IPAddrContent[j].asString().c_str());
				ConfigTemp.vecIPAddr.push_back(ipaddr);
			}
		}

		SubMaskContent = NetConfigArray[i]["SubMask"];
		if (SubMaskContent.isArray())
		{
			for (int j = 0; j < SubMaskContent.size(); j++)
			{
				wstring submask = UTF8ToUnicode(SubMaskContent[j].asString().c_str());
				ConfigTemp.vecSubMask.push_back(submask);
			}

		}
		GatewayContent = NetConfigArray[i]["Gateway"];
		if (GatewayContent.isArray())
		{
			for (int j = 0; j < GatewayContent.size(); j++)
			{
				wstring gateway = UTF8ToUnicode(GatewayContent[j].asString().c_str());
				ConfigTemp.vecGateway.push_back(gateway);
			}

		}
		DNSContent = NetConfigArray[i]["DNS"];
		if (DNSContent.isArray())
		{
			for (int j = 0; j < DNSContent.size(); j++)
			{
				wstring dns = UTF8ToUnicode(DNSContent[j].asString().c_str());
				ConfigTemp.vecDNS.push_back(dns);
			}
		}
		

		vecConfigStu.push_back(ConfigTemp);	
	}

	bRet = TRUE;

END:
	return bRet;
}

/*
* 防火墙状态的Json组建
* @fn			SecDetect_NetFireWall_GetJson
* @brief		从防火墙状态的自定义结构体获取数据组装成Json字符串并返回
* @param[in]	vecFireWall: 自定义防火墙状态结构体，调用位置 WLDeal
* @param[out]	
* @return		strJsonFireWall: 返回根据防火墙开启状态的Json字符串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-09-03
*/
std::string CWLJsonParse::SecDetect_NetFireWall_GetJson(__in std::vector<SECDETECT_NET_FIREWALL_STRUCT> &vecFireWall)
{
/*
	{
		[
			{
			"FireWallType":0,   //(防火墙类型:0 = 防火墙 、 1 = 域防火墙、2 = 公用防火墙、 3 = 专用防火墙)
			"Status": 1     //0关闭，1开启
			}
		]
	}
*/
	std::string    strJsonFireWall = "";
    Json::Value    FirewallContent = Json::nullValue;
	Json::Value               root;
	Json::FastWriter        writer;
	
	int nCount = (int)vecFireWall.size();

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value               Temp;

		Temp["FireWallType"] = (int)vecFireWall[i].dwFirewallType;
		Temp["Status"]       = (int)vecFireWall[i].dwStatus;

		FirewallContent.append(Temp);
	}

	root["NetFireWall"] = (Json::Value)FirewallContent;
	strJsonFireWall = writer.write(root);

	root.clear();

	return strJsonFireWall;
}


/*
* @fn           SecDetect_NetFireWall_ParseJson
* @brief        解析传入的防火墙列表Json串，将其字段数据存入FireWall结构体
* @param[in]    strFirewallJson: 传入的防火墙状态Json字符串
* @param[out]   vecFireWallStu : 防火墙状态结构体的数组。
* @return       BOOL: 如果Json字段解析成功则返回True;反之，但会false。
*               
* @detail       GUI页面读取Json后使用此函数解析，或生成各种PDF，或各种展示样式。 
* @author       zhiyong.liu
* @date         2021-09-03
*/
BOOL CWLJsonParse::SecDetect_NetFireWall_ParseJson(__in string strFirewallJson, __out vector<SECDETECT_NET_FIREWALL_STRUCT> &vecFireWallStu)
{
	/*传入的Json：
	[
	   {
	       "FireWallType":1,   //(防火墙类型:0 = 防火墙 、 1 = 域防火墙、2 = 公用防火墙、 3 = 专用防火墙)
	        "Status": 1     //0关闭，1开启
	   }
	   {
	   "FireWallType":2,   //(防火墙类型:0 = 防火墙 、 1 = 域防火墙、2 = 公用防火墙、 3 = 专用防火墙)
	   "Status": 1     //0关闭，1开启
	   }
	]
*/
	BOOL               bRet = FALSE;
	Json::Value   NetFireArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecFireWallStu.clear();

	if(strFirewallJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strFirewallJson, NetFireArray))
	{
		goto END;
	}

    if ( NetFireArray.isNull() || !NetFireArray.isArray())
    {
        goto END;
    }

	nCount = NetFireArray.size();
	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("nCount %d"),nCount);

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_NET_FIREWALL_STRUCT FirewallTemp;  //单独一项
		FirewallTemp.dwFirewallType  = (DWORD)NetFireArray[i]["FireWallType"].asInt();
		FirewallTemp.dwStatus        = (DWORD)NetFireArray[i]["Status"].asInt();
	
		vecFireWallStu.push_back(FirewallTemp);	
	}

	bRet = TRUE;

END:
	return bRet;
}

// 使用的Json对齐Linux
std::string CWLJsonParse::SecDetect_Cycle_ToUSM_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID, __in const DWORD dwEnable, __in const DWORD dwCycleItem, __in const DWORD dwDay, __in const DWORD dwHour, __in const DWORD dwMinute)
{
    /* 安全检测：上传周期配置
    {
        "ComputerID":"XXXX",
        "CMDTYPE":200,
        "CMDID":300,
        "CMDContent":
        {
            "checkStatus":1,
            "timeType":2,
            "weekDay":1,
            "hour":0,
            "minute":0
        }
    }
    */

    BOOL bRes = FALSE;
    std::string strJson;

    Json::Value CMDContent;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
    person["CMDTYPE"]		= (int)CMDTYPE_CMD;
    person["CMDID"]         = nCmdID;

    CMDContent["checkStatus"] = (int)dwEnable;
    CMDContent["timeType"]    = (int)dwCycleItem;
    CMDContent["weekDay"]     = (int)dwDay;
    CMDContent["hour"]        = (int)dwHour;
    CMDContent["minute"]      = (int)dwMinute;

    person["CMDContent"]    = (Json::Value)CMDContent;

    root.append(person);

    strJson = writer.write(root);
    root.clear();
    bRes = TRUE;

    return strJson;
}

std::string CWLJsonParse::SecDetect_Cycle_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID , __in const DWORD dwEnable, __in const DWORD dwCycleItem, __in const DWORD dwDays, __in const DWORD dwHours, __in const DWORD dwMinute)
{
    /* 安全检测：周期
    {
	    "CMDID":,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bRes = FALSE;
    std::string strJson;

    Json::Value CMDContent;
    Json::Value root;
    Json::FastWriter writer;
    Json::Value person;

    person["ComputerID"]	= UnicodeToUTF8(tstrComputerID);
    person["CMDTYPE"]		= (int)CMDTYPE_POLICY;
    person["CMDID"]         = nCmdID;
    person["Enable"]        = (int)dwEnable;
    person["Cycle"]         = (int)dwCycleItem;
    person["Day"]           = (int)dwDays;
    person["Hour"]          = (int)dwHours;
    person["Minute"]        = (int)dwMinute;
    person["CMDContent"]    = Json::nullValue;

    root.append(person);

    strJson = writer.write(root);
    root.clear();
    bRes = TRUE;

    return strJson;
}

BOOL CWLJsonParse::SecDetect_Cycle_GetValue(__in const string &strJson, __out DWORD &dwEnable, __out DWORD &dwCycleItem, __out DWORD &dwDays, __out DWORD &dwHours, __out DWORD &dwMinute, __out tstring *pStrError /* = NULL */)
{
    /* 安全检测：周期
    {
	    "CMDID":,
	    "CMDTYPE":150,
	    "CMDVER":1,
	    "
	    "ComputerID":"XXXXXXXXXXX-XXXXX",
	    "CMDContent":null
    }
    */

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;

    Json::Reader	reader;
    std::string		strValue = "";

    if( strJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::SecDetect_Cycle_GetValue, sJson.length() == 0") << _T(",");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::SecDetect_Cycle_GetValue, parse fail")<<_T(",");
        goto END;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::SecDetect_Cycle_GetValue, nObject < 1")<<_T(",");
        goto END;
    }

    if (root[0].isMember("Cycle"))
    {
        dwCycleItem = root[0]["Cycle"].asInt();
    }

    if (root[0].isMember("Day"))
    {
        dwDays = root[0]["Day"].asInt();
    }

    if (root[0].isMember("Hour"))
    {
        dwHours = root[0]["Hour"].asInt();
    }

    if (root[0].isMember("Minute"))
    {
        dwMinute = root[0]["Minute"].asInt();
    }

    if (root[0].isMember("Enable"))
    {
        dwEnable = root[0]["Enable"].asInt();
    }

    bResult = TRUE;

END:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
}

/*
* @fn           SecLine_IPRoute_Table_GetJson
* @brief        路由结构体转Json字符串
* @param[in]    pRow：Win32 API保存路由的结构体函数
* @param[out]   
* @return       string: 返回路由的Json字符串
*               
* @detail      
* @author      mingming.shi
* @date        2021-09-21
*/
std::string CWLJsonParse::SecLine_IPRoute_Table_GetJson(PMIB_IPFORWARDROW pRow)
{
	Json::Value			root;
	Json::FastWriter	writer;
	Json::Value			Items;
	Json::Value			Item;
	std:: string		strJson;

	if(pRow == NULL)
	{
        root["DefaultIpRoute"] = Json::nullValue;
		goto _END_;
	}
	Item["dwForwardDest"]      = (int)pRow->dwForwardDest;
	Item["dwForwardMask"]      = (int)pRow->dwForwardMask;
	Item["dwForwardPolicy"]    = (int)pRow->dwForwardPolicy;
	Item["dwForwardProto"]	   = (int)pRow->dwForwardProto;
	Item["dwForwardNextHop"]   = (int)pRow->dwForwardNextHop;
	Item["dwForwardIfIndex"]   = (int)pRow->dwForwardIfIndex;
	Item["dwForwardType"]      = (int)pRow->dwForwardType;
	Item["dwForwardAge"]       = (int)pRow->dwForwardAge;
	Item["dwForwardNextHopAS"] = (int)pRow->dwForwardNextHopAS;
	Item["dwForwardMetric1"]   = (int)pRow->dwForwardMetric1;
	Item["dwForwardMetric2"]   = (int)pRow->dwForwardMetric2;
	Item["dwForwardMetric3"]   = (int)pRow->dwForwardMetric3;
	Item["dwForwardMetric4"]   = (int)pRow->dwForwardMetric4;
	Item["dwForwardMetric5"]   = (int)pRow->dwForwardMetric5;
	Item["ForwardProto"]	   = (int)pRow->ForwardProto;
	Item["ForwardType"]	       = (int)pRow->ForwardType;

	root["DefaultIpRoute"] = (Json::Value)(Item);

_END_:
	strJson = writer.write(root);
	root.clear();
	return strJson;
}

/*
* @fn           SecLine_IPRoute_Table_GetValue
* @brief        （安全加固项）Json字符串转路由结构体
* @param[in]    strJson：存储路由结构的Json字符串
* @param[out]   pRow：Win32 API保存路由的结构体函数
* @return       BOOL : 返回操作结果，如果转Json成功则返回TEUE，反之，则返回FALSE。
*               
* @detail      
* @author      mingming.shi
* @date        2021-09-21
*/
BOOL CWLJsonParse::SecLine_IPRoute_Table_GetValue(__in string strJson,__out PMIB_IPFORWARDROW pRow)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
    Json::FastWriter writer;

    std::string str;

//	int nCount;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if( Root.isMember("DefaultIpRoute") && !Root["DefaultIpRoute"].isNull())
	{
		SubRoot = (Json::Value)Root["DefaultIpRoute"];

		pRow->dwForwardDest      = SubRoot["dwForwardDest"].asInt();
		pRow->dwForwardMask      = SubRoot["dwForwardMask"].asInt();
		pRow->dwForwardPolicy    = SubRoot["dwForwardPolicy"].asInt();
		pRow->dwForwardNextHop   = SubRoot["dwForwardNextHop"].asInt();
		pRow->dwForwardIfIndex   = SubRoot["dwForwardIfIndex"].asInt();
		pRow->dwForwardType      = SubRoot["dwForwardType"].asInt();
		// #BugID Win10 恢复默认路由 3表示静态路由
		if(!SubRoot.isMember("dwForwardProto"))
		{
			pRow->dwForwardProto = 3;
		}
		else
		{
			pRow->dwForwardProto = SubRoot["dwForwardProto"].asInt();
		}
		pRow->dwForwardAge       = SubRoot["dwForwardAge"].asInt();
		pRow->dwForwardNextHopAS = SubRoot["dwForwardNextHopAS"].asInt();
		pRow->dwForwardMetric1   = SubRoot["dwForwardMetric1"].asInt();
		pRow->dwForwardMetric2   = SubRoot["dwForwardMetric2"].asInt();
		pRow->dwForwardMetric3   = SubRoot["dwForwardMetric3"].asInt();
		pRow->dwForwardMetric4   = SubRoot["dwForwardMetric4"].asInt();
		pRow->dwForwardMetric5   = SubRoot["dwForwardMetric5"].asInt();
		pRow->ForwardProto		 = MIB_IPFORWARD_PROTO(SubRoot["ForwardProto"].asInt());
		pRow->ForwardType		 = MIB_IPFORWARD_TYPE(SubRoot["ForwardType"].asInt());
	}
	else
	{
		goto _exit_;
	}
    
	bRet = TRUE;

_exit_:

    SubRoot.clear();

    Root.clear();

	return bRet;
}

/*
* @fn			SecLine_ForBid_OPTUser_GetJson
* @brief		（安全加固项）返回默认操作用户禁用状态的Json字符串
* @param[in]    dwStatus: 表示默认操作用户的禁用状态
* @param[out]	
* @return		
*               
* @detail      	1 表示 Guest 用户状态为禁用
				2 表示 Administrator 用户状态为禁用
				3 表示 Guest 和 Administrator 用户都被禁用
* @author		mingming.shi
* @date			2021-09-23
*/
std::string CWLJsonParse::SecLine_ForBid_OPTUser_GetJson(__in DWORD dwStatus)
{
	std::string strJson = "";
	Json::FastWriter write;
	Json::Value root;
	Json::Value ForbidOPTUserRoot;

	/*if ( dwStatus == DEFAULT_OS_USER_DISABLE_STATUS_GUEST)
	{
		root["Administrator"] = 0;
		root["Guest"] = DEFAULT_OS_USER_DISABLE_STATUS_GUEST;
	}
	else */
    if( dwStatus == DEFAULT_OS_USER_DISABLE_STATUS_ADMIN )
	{
		root["Administrator"] = DEFAULT_OS_USER_DISABLE_STATUS_ADMIN;
		root["Guest"] = 0;
	}
	/*else if( dwStatus == DEFAULT_OS_USER_DISABLE_STATUS_ALL )
	{
		root["Administrator"] = DEFAULT_OS_USER_DISABLE_STATUS_ADMIN;
		root["Guest"] = DEFAULT_OS_USER_DISABLE_STATUS_GUEST;
	}*/
	else if( dwStatus == DEFAULT_OS_USER_DISABLE_STATUS_NO )
	{
		root["Administrator"] = 0;
		root["Guest"] = 0;
	}

	ForbidOPTUserRoot["ForbidDefaultOPTUser"] = (Json::Value)root;

	strJson = write.write(ForbidOPTUserRoot);
	ForbidOPTUserRoot.clear();

	return strJson;
}
/*
* @fn			SecLine_ForBid_OPTUser_GetValue
* @brief		（安全加固项）根据用户状态的Json字符串读取默认操作用户禁用状态
* @param[in]    strJson: 保存用户状态的Json字符串
* @param[out]	dwStatus: 表示默认操作用户的禁用状态
* @return		
*               
* @detail      	1 表示 Guest 用户状态为禁用
				2 表示 Administrator 用户状态为禁用
				3 表示 Guest 和 Administrator 用户都被禁用
* @author		mingming.shi
* @date			2021-09-23
*/
BOOL CWLJsonParse::SecLine_ForBid_OPTUser_GetValue(__in std::string strJson, __out DWORD& dwStatus)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	dwStatus = 0;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("ForbidDefaultOPTUser") && !Root["ForbidDefaultOPTUser"].isNull())
	{
		SubRoot = (Json::Value)Root["ForbidDefaultOPTUser"];
		if(SubRoot.isMember("Administrator"))
		{
			dwStatus += SubRoot["Administrator"].asInt();
		}

		if(SubRoot.isMember("Guest"))
		{
			dwStatus += SubRoot["Guest"].asInt();
		}
	}
	else
	{
		goto _exit_;
	}

	bRet = TRUE;
	Root.clear();
	SubRoot.clear();
_exit_:
	return bRet;
}
// 函数头见 SecLine_ForBid_OPTUser_GetValue
std::string CWLJsonParse::SecLine_RemoteHost_RDP_GetJson(__in DWORD dwfDeny, __in DWORD dwfSingle)
{
	std::string strJson;
	Json::FastWriter write;
	Json::Value root;
	Json::Value RemoteHostRDPRoot;


	root["fDenyTSConnections"]		= (unsigned int)dwfDeny;
	root["fSingleSessionPerUser"]	= (unsigned int)dwfSingle;
	
	RemoteHostRDPRoot["RemoteHostRDP"] = (Json::Value)root;

	strJson = write.write(RemoteHostRDPRoot);
	RemoteHostRDPRoot.clear();

	return strJson;
}

/*
* @fn			SecLine_RemoteHost_RDP_GetValue
* @brief		（安全加固项）远程主机RDP状态Json字符串转为具体值
* @param[in]    strJson：保存了RDP状态的Json字符串
* @param[out]	dwfDeny：注册表值
				dwfSingle：注册表值
* @return		BOOL：函数执行成功返回TRUE；反之，返回FALSE
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-23
*/
BOOL CWLJsonParse::SecLine_RemoteHost_RDP_GetValue(__in std::string strJson, __out DWORD& dwfDeny, __out DWORD& dwfSingle)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	dwfDeny		= 0;
	dwfSingle	= 0;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("RemoteHostRDP") && !Root["RemoteHostRDP"].isNull())
	{
		SubRoot = (Json::Value)Root["RemoteHostRDP"];
		if(SubRoot.isMember("fDenyTSConnections"))
		{
			dwfDeny = SubRoot["fDenyTSConnections"].asInt();
		}

		if(SubRoot.isMember("fSingleSessionPerUser"))
		{
			dwfSingle = SubRoot["fSingleSessionPerUser"].asInt();
		}
	}
	else
	{
		goto _exit_;
	}

	bRet = TRUE;
	Root.clear();
	SubRoot.clear();
_exit_:
	return bRet;
}

/*
* @fn			SecLine_RemoteHost_LoginTime_GetJson
* @brief		（安全加固项）获取远程主机登录时间相关注册表值到Json字符串中
* @param[in]    dwfReset：远程主机登录时间注册表相关值
				dwMaxIdle：远程主机登录时间注册表相关值
* @param[out]	
* @return		string：保存相关注册表值的Json字符串
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-23
*/
std::string CWLJsonParse::SecLine_RemoteHost_LoginTime_GetJson(__in DWORD dwfReset, __in DWORD dwMaxIdle)
{
	std::string strJson;
	Json::FastWriter write;
	Json::Value root;
	Json::Value RemoteHostRDPRoot;

	root["fResetBroken"] = (int)dwfReset;
	root["MaxIdleTime"]	 = (int)dwMaxIdle;

	RemoteHostRDPRoot["RemoteLoginTime"] = (Json::Value)root;

	strJson = write.write(RemoteHostRDPRoot);
	RemoteHostRDPRoot.clear();

	return strJson;
}

/*
* @fn			SecLine_RemoteHost_LoginTime_GetValue
* @brief		（安全加固项）远程主机登录时
* @param[in]    strJson: 保存远程主机登录恢复数据的Json字符串
* @param[out]	dwfReset：远程主机登录时间注册表相关值
				dwMaxIdle：远程主机登录时间注册表相关值
* @return		BOOL：函数执行成功返回TRUE；反之，返回FALSE
*               
* @detail      	参数相关见 SecLine_RemoteHost_LoginTime_GetJson 函数
* @author		mingming.shi
* @date			2021-09-24
*/
BOOL CWLJsonParse::SecLine_RemoteHost_LoginTime_GetValue(__in std::string strJson, __out DWORD& dwfReset, __out DWORD& dwMaxIdle)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	dwfReset	= 0;
	dwMaxIdle	= 0;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("RemoteLoginTime") && !Root["RemoteLoginTime"].isNull())
	{
		SubRoot = (Json::Value)Root["RemoteLoginTime"];
		if(SubRoot.isMember("fResetBroken"))
		{
			dwfReset = SubRoot["fResetBroken"].asInt();
		}

		if(SubRoot.isMember("MaxIdleTime"))
		{
			dwMaxIdle = SubRoot["MaxIdleTime"].asInt();
		}
	}
	else
	{
		goto _exit_;
	}
	bRet = TRUE;
_exit_:
	return bRet;
}

/*
* @fn			SecLine_ScrnSave_GetJson
* @brief		（安全加固项）屏保程序的数据保存
* @param[in]    dwAct: 确定是否选择屏幕保护程序
				dwSec：密码登录
				dwTime：无操作时长
				strEXE：保护程序位置
* @param[out]	
* @return		string: 根据屏保输入数据得到的Json字符串
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-24
*/
std::string CWLJsonParse::SecLine_ScrnSave_GetJson(__in DWORD dwAct, __in DWORD dwSec, __in DWORD dwTime, __in std::string strEXE)
{
	std::string strJson;
	Json::FastWriter write;
	Json::Value root;
	Json::Value ScrnSaveRoot;

	root["SetScreenSaveActive"]  = (int)dwAct;
	root["SetScreenSaveTimeOut"] = (int)dwTime;
	root["SetScreenSaveSecure"]  = (int)dwSec;
	root["ScrnSave.exe"]         = (string)strEXE;

	ScrnSaveRoot["ScrnSave"] = (Json::Value)root;

	strJson = write.write(ScrnSaveRoot);
	ScrnSaveRoot.clear();

	return strJson;
}

// 函数头见 SecLine_ScrnSave_GetJson
BOOL CWLJsonParse::SecLine_ScrnSave_GetValue(__in std::string strJson, __out DWORD &dwAct, __out DWORD &dwSec, __out DWORD &dwTime, __out std::string &strEXE)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	dwAct	= 0;
	dwSec	= 0;
	dwTime	= 0;
	strEXE	= "";

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("ScrnSave") && !Root["ScrnSave"].isNull())
	{
		SubRoot = (Json::Value)Root["ScrnSave"];
		if(SubRoot.isMember("SetScreenSaveActive"))
		{
			dwAct = SubRoot["SetScreenSaveActive"].asInt();
		}

		if(SubRoot.isMember("SetScreenSaveSecure"))
		{
			dwSec = SubRoot["SetScreenSaveSecure"].asInt();
		}

		if(SubRoot.isMember("SetScreenSaveTimeOut"))
		{
			dwTime = SubRoot["SetScreenSaveTimeOut"].asInt();
		}

		if(SubRoot.isMember("ScrnSave.exe"))
		{
			strEXE = SubRoot["ScrnSave.exe"].asString();
		}
	}
	else
	{
		goto _exit_;
	}

	bRet = TRUE;
	Root.clear();
	SubRoot.clear();
_exit_:
	return bRet;
}
/*
* @fn			SecLine_SynAtkDet_GetJson
* @brief		（安全加固项）Syn攻击检测
* @param[in]    dwTcpMaxExed: Syn攻击可以拒绝的连接请求数量
				dwTcpMaxHalf：Syn半打开状态下可以维持的连接数量
				dwRetried：在传输重新连接后，服务器连接处于把打开状态的数量
				dwSynAtk：Syn的攻击保护状态，为1表示启用，0则表示不启用
* @param[out]	
* @return		string : Json字符串
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-24
*/
std::string CWLJsonParse::SecLine_SynAtkDet_GetJson(__in DWORD dwTcpMaxExed, __in DWORD dwTcpMaxHalf, __in DWORD dwRetried, __in DWORD dwSynAtk)
{
	std::string strJson;
	Json::FastWriter write;
	Json::Value root;
	Json::Value ScrnSaveRoot;

	root["TcpMaxPortsExhausted"]  = (int)dwTcpMaxExed;
	root["TcpMaxHalfOpen"]        = (int)dwTcpMaxHalf;
	root["SynAttackProtect"]      = (int)dwSynAtk;
	root["TcpMaxHalfOpenRetried"] = (int)dwRetried;

	ScrnSaveRoot["SynProtection"] = (Json::Value)root;

	strJson = write.write(ScrnSaveRoot);

	ScrnSaveRoot.clear();
	root.clear();
	return strJson;
}
// 函数头见 SecLine_SynAtkDet_GetJson
BOOL CWLJsonParse::SecLine_SynAtkDet_GetValue(__in std::string strJson, __out DWORD &dwTcpMaxExed, __out DWORD &dwTcpMaxHalf, __out DWORD &dwRetried, __out DWORD &dwSynAtk)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	dwRetried    = 0;
	dwSynAtk     = 0;
	dwTcpMaxExed = 0;
	dwTcpMaxHalf = 0;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("SynProtection") && !Root["SynProtection"].isNull())
	{
		SubRoot = (Json::Value)Root["SynProtection"];
		if(SubRoot.isMember("TcpMaxPortsExhausted"))
		{
			dwTcpMaxExed = SubRoot["TcpMaxPortsExhausted"].asInt();
		}

		if(SubRoot.isMember("TcpMaxHalfOpen"))
		{
			dwTcpMaxHalf = SubRoot["TcpMaxHalfOpen"].asInt();
		}

		if(SubRoot.isMember("SynAttackProtect"))
		{
			dwSynAtk = SubRoot["SynAttackProtect"].asInt();
		}

		if(SubRoot.isMember("TcpMaxHalfOpenRetried"))
		{
			dwRetried = SubRoot["TcpMaxHalfOpenRetried"].asInt();
		}
	}
	else
	{
		goto _exit_;
	}
	
	bRet = TRUE;
	Root.clear();
	SubRoot.clear();
_exit_:
	return bRet;
}

/*
* @fn			SecLine_ForbidUserShutDown_GetJson
* @brief		（安全加固项）禁止非管理员用户关机
* @param[in]    mapUsers: 存储用户组名和用户组SID的map
				mapUser：存储用户名和用户SID的map
* @param[out]	
* @return		string : Json字符串
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-24
*/
std::string CWLJsonParse::SecLine_ForbidUserShutDown_GetJson(std::wstring wsShutDownPrivilege /*__in map<std::string, std::wstring> mapUsers, __in  map<std::string, std::wstring> mapUser*/)
{
    std::string strJson = "";
    
    Json::Value Item;
    Json::Value root;
    Json::FastWriter write;

    Item["Privilege"] = UnicodeToUTF8(wsShutDownPrivilege);

    root["ForbidUserShutDown"] = (Json::Value)Item;

    strJson = write.write(root);

    root.clear();

    return strJson;

	//std::string strJson;
	//std::string strTemp;
	//Json::FastWriter write;
	//Json::Value root;
	//Json::Value rootUser;
	//Json::Value rootUsers;
	//Json::Value ScrnSaveRoot;
	
	//map<std::string, std::wstring>::iterator it;
	//for (it = mapUser.begin(); it != mapUser.end(); it++)
	//{
	//	Json::Value temp;
	//	temp["Name"] = it->first;
	//	strTemp = ConvertW2A(it->second);
	//	temp["SID"]	 = strTemp;
	//	rootUser.append(temp);
	//}

	//for (it = mapUsers.begin(); it != mapUsers.end(); it++)
	//{
	//	Json::Value temp;
	//	temp["Name"] = it->first;
	//	strTemp = ConvertW2A(it->second);
	//	temp["SID"]	 = strTemp;
	//	rootUsers.append(temp);
	//}

	//root["User"] = (Json::Value)rootUser;
	//root["Group"] = (Json::Value)rootUsers;

	//ScrnSaveRoot["ForbidUserShutDown"] = (Json::Value)root;

	//strJson = write.write(ScrnSaveRoot);
	//ScrnSaveRoot.clear();

	//return strJson;
}

// 函数头见 SecLine_ForbidUserShutDown_GetJson
BOOL CWLJsonParse::SecLine_ForbidUserShutDown_GetValue(__in string strJson, __out std::wstring &wsShutDownPrivilege, __out tstring *pError /*= NULL*/ /*__out map<std::string, std::wstring> &mapUser, __out map<std::string, std::wstring> &mapUsers*/)
{
    BOOL bRet = FALSE;

    wostringstream wosError;

    Json::Reader reader;
    Json::Value root;
    Json::Value Item;

    if (strJson.length() == 0)
    {
        wosError << _T("strJson.length() == 0") << _T(",");
        goto END;
    }

    if ( !reader.parse(strJson, root))
    {
        wosError << _T("parse json failed, and json = ") << UTF8ToUnicode(strJson).c_str() << _T(",");
        goto END;
    }

    if(root.isMember("ForbidUserShutDown") && !root["ForbidUserShutDown"].isNull())
    {
        Item = (Json::Value)root["ForbidUserShutDown"];
        if(Item.isMember("Privilege"))
        {
            wsShutDownPrivilege = UTF8ToUnicode(Item["Privilege"].asString());
        }    		
    }

    bRet = TRUE;

END:

    if ( pError)
    {
        *pError = wosError.str();
    }

    return bRet;

//	BOOL bRet = FALSE;
//	Json::Value Root;
//	Json::Value SubRoot;
//	Json::Reader reader;
//	Json::FastWriter write;
//
//	int nArrSize;
//	int userSize = 0;
//	int usersSize = 0;
//
//	if(strJson.empty())
//	{
//		goto _exit_;
//	}
//
//	if (!reader.parse(strJson, Root))
//	{
//		goto _exit_;
//	}
//
//	if(Root.isMember("ForbidUserShutDown") && !Root["ForbidUserShutDown"].isNull())
//	{
//		SubRoot = (Json::Value)Root["ForbidUserShutDown"];
//		if(SubRoot.isMember("Group"))
//		{
//			Json::Value Group;
//
//			Group = SubRoot["Group"];
//			userSize = Group.size();
//			for (int i = 0; i < userSize; i++)
//			{
//				string strName = Group[i]["Name"].asString();
//				wstring wstrSid = UTF8ToUnicode(Group[i]["SID"].asString());
//				mapUser[strName] = wstrSid;
//			}
//		}
//
//		if(SubRoot.isMember("User"))
//		{
//			Json::Value Group;
//
//			Group = SubRoot["User"];
//			userSize = Group.size();
//			for (int i = 0; i < userSize; i++)
//			{
//				string strName = Group[i]["Name"].asString();
//				wstring wstrSid = UTF8ToUnicode(Group[i]["SID"].asString());
//				mapUser[strName] = wstrSid;
//			}
//		}
//	}
//	else
//	{
//		goto _exit_;
//	}
//	bRet = TRUE;
//
//_exit_:
//	Root.clear();
//	SubRoot.clear();
//	return bRet;
}


/*
* @fn			SecLine_EnableUAC_GetJson
* @brief		（安全加固项）用户账户控制UAC
* @param[in]    dwEnableUA：输入字段，UAC开启状态
				dwConPromtBhv：输入数组，UAC相关状态
				dwPromtDeskTop：输入数组，UAC相关状态
* @param[out]	
* @return		string : Json字符串
*               
* @detail      	
* @author		mingming.shi
* @date			2021-11-11
*/
std::string CWLJsonParse::SecLine_EnableUAC_GetJson(__in DWORD dwEnableUA, __in DWORD dwConPromtBhv, __in DWORD dwPromtDeskTop)
{
	std::string strJson;
	Json::FastWriter write;
	Json::Value root;
	Json::Value UACRoot;

	root["EnableLUA"]							= (int)dwEnableUA;
	root["ConsentPromptBehaviorAdmin"]			= (int)dwConPromtBhv;
	root["PromptOnSecureDesktop"]				= (int)dwPromtDeskTop;

	UACRoot["EnableUAC"] = (Json::Value)root;

	strJson = write.write(UACRoot);
	
	UACRoot.clear();
	root.clear();

	return strJson;
}

// 函数头见 SecLine_EnableUAC_GetJson
BOOL CWLJsonParse::SecLine_EnableUAC_GetValue(__in std::string strJson, __out DWORD &dwEnableUA, __out DWORD &dwConPromtBhv, __out DWORD &dwPromtDeskTop)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value SubRoot;
	Json::Reader reader;
	DWORD dwRetried    = 0;
	DWORD dwSynAtk     = 0;
	DWORD dwTcpMaxExed = 0;
	DWORD dwTcpMaxHalf = 0;

	if(strJson.empty())
	{
		goto _exit_;
	}

	if (!reader.parse(strJson, Root))
	{
		goto _exit_;
	}

	if(Root.isMember("EnableUAC") && !Root["EnableUAC"].isNull())
	{
		SubRoot = (Json::Value)Root["EnableUAC"];
		if(SubRoot.isMember("ConsentPromptBehaviorAdmin"))
		{
			dwConPromtBhv = SubRoot["ConsentPromptBehaviorAdmin"].asInt();
		}

		if(SubRoot.isMember("PromptOnSecureDesktop"))
		{
			dwPromtDeskTop = SubRoot["PromptOnSecureDesktop"].asInt();
		}

		if(SubRoot.isMember("EnableLUA"))
		{
			dwEnableUA = SubRoot["EnableLUA"].asInt();
		}

	}
	else
	{
		goto _exit_;
	}

	bRet = TRUE;
	Root.clear();
	SubRoot.clear();
_exit_:
	return bRet;
}

std::string CWLJsonParse::SecLine_the_OS_Log_GetJson(__in DWORD dwLogType, __in DWORD dwLogSize, __in DWORD dwLogTime)
{
    std::string strJson;

    std::string strTitle = "";

    Json::FastWriter write;
    Json::Value root;
    Json::Value OSLogRoot;

    switch (dwLogType)
    {
    case LOG_APP:
        {
            strTitle = "APPLICATIONLOG";
        }
        break;
    case LOG_SEC:
        {
            strTitle = "SECLOG";
        }
        break;
    case LOG_SYS:
        {
            strTitle = "SYSTEMLOG";
        }
        break;
    }

    root["LogSize"] = (int)dwLogSize;
    root["LogTime"] = (int)dwLogTime;

    OSLogRoot[strTitle.c_str()] = (Json::Value)root;

    strJson = write.write(OSLogRoot);

    OSLogRoot.clear();
    root.clear();

    return strJson;
}

BOOL CWLJsonParse::SecLine_the_OS_Log_GetValue(__in std::string strJson, __in DWORD dwLogType, __out DWORD &dwLogSize, __out DWORD &dwLogTime)
{
    BOOL bRet = FALSE;

    Json::Value Root;
    Json::Value SubRoot;
    Json::Reader reader;

    std::string strTitle = "";

    if(strJson.empty())
    {
        goto _exit_;
    }

    if (!reader.parse(strJson, Root))
    {
        goto _exit_;
    }


    switch (dwLogType)
    {
    case LOG_APP:
        {
            strTitle = "APPLICATIONLOG";
        }
        break;

    case LOG_SEC:
        {
            strTitle = "SECLOG";
        }
        break;

    case LOG_SYS:
        {
            strTitle = "SYSTEMLOG";
        }
        break;

    default:
        goto _exit_;
    }

    if( !Root.isMember(strTitle.c_str()) || Root[strTitle.c_str()].isNull())
    {
        goto _exit_;
    }

    SubRoot = (Json::Value)Root[strTitle.c_str()];

    if( SubRoot.isMember("LogSize"))
    {
        dwLogSize = SubRoot["LogSize"].asInt();
    }

    if( SubRoot.isMember("LogTime"))
    {
        dwLogTime = SubRoot["LogTime"].asInt();
    }

    bRet = TRUE;
    Root.clear();
    SubRoot.clear();

_exit_:

    return bRet;
}

BOOL CWLJsonParse::SecDetect_USM_GetValue(__in const string &strJson, /*__out DWORD &dwPdfType,*/ __out DWORD &dwTimeType, __out DWORD &dwDay, __out DWORD &dwHour, __out DWORD &dwMinute, __out DWORD &dwCheckType, __out DWORD &dwCheckStatus, __out tstring *pStrError/* = NULL */)
{
    /* 安全检测：通信
    [
        {
            “ComputerID”:”FEFOEACD”,
            “CMDTYPE”:200,
            “CMDVER”:1 
            “CMDID”:212,//212 合规报告
            “CMDContent”:
            {
                “client_type”:1 //（暂时没用）

                "checkType":1,      // 1：立即下发，2：周期配置
                "checkStatus":1,    // 周期上报开关 1：开启，2：关闭
                "hour":5,           // 小时 [0, 23]
                "timeType":2,       // 1：月，2：周，3：天
                "weekDay":1,            // timeType为1 表示[0, 28]号；timeType为2 表示周一至周天[1, 7]；timeType为3 ，表示一天 1
                "minute":1,         // [0, 55]分钟，间隔5分钟
            }
        }
    ]
    */

    BOOL bResult = FALSE;

    wostringstream  strTemp;
    Json::Value     root;
    Json::Value     CMDContent;

    Json::Reader	reader;
    std::string		strValue = "";

    if( strJson.length() == 0)
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, sJson.length() == 0") << _T(",");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, parse fail")<<_T(",");
        goto END;
    }

    int nObject = root.size();
    if( nObject < 1 || !root.isArray())
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, nObject < 1")<<_T(",");
        goto END;
    }

    if ( !root[0].isMember("CMDContent"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, root[0] member : CMDContent is not exist") << _T(",");
        goto END;
    }

    CMDContent = (Json::Value)root[0]["CMDContent"];

    /*if ( !CMDContent.isMember("reportType"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : reportType is not exist") << _T(",");
        goto END;
    }*/

    /*if ( !CMDContent.isMember("cycle"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : cycle is not exist") << _T(",");
        goto END;
    }*/

    if ( !CMDContent.isMember("checkType"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : checkType is not exist") << _T(",");
        goto END;
    }

    if ( !CMDContent.isMember("checkStatus"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : checkStatus is not exist") << _T(",");
        goto END;
    }

    if ( !CMDContent.isMember("hour"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : hour is not exist") << _T(",");
        goto END;
    }

    if ( !CMDContent.isMember("timeType"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : timeType is not exist") << _T(",");
        goto END;
    }

    if ( !CMDContent.isMember("weekDay"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : weekDay is not exist") << _T(",");
        goto END;
    }

    if ( !CMDContent.isMember("minute"))
    {
        strTemp << _T("CWLJsonParse::SecDetect_PDF_Type_GetValue, CMDContent member : minute is not exist") << _T(",");
        goto END;
    }

    //dwPdfType = CMDContent["reportType"].asInt();
    //dwDay = CMDContent["cycle"].asInt();

    dwCheckType = CMDContent["checkType"].asInt();
    dwCheckStatus = CMDContent["checkStatus"].asInt();
    dwHour = CMDContent["hour"].asInt();
    dwTimeType = CMDContent["timeType"].asInt();
    dwDay = CMDContent["weekDay"].asInt();
    dwMinute = CMDContent["minute"].asInt();

    bResult = TRUE;

END:
    if (pStrError)
    {
        *pStrError = strTemp.str();
    }

    return bResult;
} 





/*
* 向服务端发送基线项的选中状态
* @fn			baseLine_Detect_Selected_GetJson
* @brief		选中的框组建json
* @param[in]	pBLSelected: 基线项的勾选状态，会转化为0和1 ： 0--未勾选  1--勾选
* @param[out]	
* @return		strJsonBase：勾选项名+勾选状态的Json串
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-09-18
*/
std::string CWLJsonParse::baseLine_Selected_Item_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID,__in std::vector<std::string> vecSelected , __in tstring strOprate, DWORD dwLevel)
{
	std::string        sJson = "";
	Json::Value        root;
	Json::FastWriter   writer;
	Json::Value        CONTENT;
	Json::Value        person;

	if (vecSelected.size() == 0)
	{
		//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("Don't Slect any Item, the vector of Item is NULL"));
		goto END;
	}

	int nCount = (int)vecSelected.size();
	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("the Size of Selected Item is %d"), nCount);

	for (int i = 0; i < nCount; i++)
	{
		CONTENT.append(vecSelected[i]);
	}

	person[UnicodeToUTF8(strOprate)] = (Json::Value)CONTENT;
	person["ComputerID"]	= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"]		= (int)cmdType;
	person["CMDID"]         = (int)cmdID;

	root.append(person);

	sJson = writer.write(root);
	root.clear();

END:
	return sJson;
}



BOOL IsSelected(DWORD dwValue)
{
	return (dwValue == 0 ) ? FALSE : TRUE;
}

/*
* 向服务端发送基线项的选中状态
* @fn			baseLine_Selected_Item_GetValue
* @brief		选中的框组建json
* @param[in]	sJson:带关键字的Json
* @param[out]	pSelected:基线项的勾选状态 ： 0--未勾选  1--勾选
* @return		
*				
* @detail	   
* @author		zhiyong.liu
* @date 		2021-09-23
*/
BOOL CWLJsonParse::baseLine_Selected_Item_GetValue(__in std::string sJson, __out PBASELINE_PL_NEW_ST pSelected)
{

/*
	{
		"Detect":   (或者"Restore"  或者"Reinforce")
		[
		KeyName1,
		KeyName2
		]
	}
*/
	BOOL  bRet = FALSE;
	Json::Value  root;
	Json::Reader reader;
	Json::Value  person;
	Json::Value  Content;

	if (sJson.length() == 0)
	{
		//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("the Selected Json is NULL"));
		goto END;
	}

	if(!reader.parse(sJson, root))
	{
		//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("the Selected Json Parson Failed"));
		goto END;
	}

	if(root.isMember("Detect"))
	{
		Content = root["Detect"];
	}
	else if(root.isMember("Restore"))
	{
		Content = root["Restore"];
	}
	else if(root.isMember("Reinforce"))
	{
		Content = root["Reinforce"];
	}
	//wwdv2
	int nCount = Content.size();
	if (Content.isArray())
	{
		for(int i = 0; i < nCount; i ++)
		{
			//审核  UTF8ToUnicode(Content[i].asString()
			std::wstring wsTemp =  UTF8ToUnicode(Content[i].asString());

			if(_tcscmp(wsTemp.c_str(), _T("AUDITSYSTEM")) == 0)
			{
				pSelected->dwAuditCategorySystem = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITLOGON")) == 0)
			{
				pSelected->dwAuditCategoryLogon = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITOBJECTACCESS")) == 0)
			{
				pSelected->dwAuditCategoryObjectAccess = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITPRIVILEGEUSE")) == 0)
			{
				pSelected->dwAuditCategoryPrivilegeUse = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITDETAILEDTRACKING")) == 0)
			{
				pSelected->dwAuditCategoryDetailedTracking  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITPOLICYCHANGE")) == 0)
			{
				pSelected->dwAuditCategoryPolicyChange  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITACCOUNTMANAGEMENT")) == 0)
			{
				pSelected->dwAuditCategoryAccountManagement  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUDITDIRECTORYSERVICEACCESS")) == 0)
			{
				pSelected->dwAuditCategoryDirectoryServiceAccess  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "AUDITACCOUNTLOGON")) == 0)
			{
				pSelected->dwAuditCategoryAccountLogon  = TRUE;
			}

			//密码策略
			else if(_tcscmp(wsTemp.c_str(), _T("PASSWORDCOMPLEXITY")) == 0)
			{
				pSelected->dwPasswordComplexity  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("MINPASSWDLEN")) == 0)
			{
				pSelected->dwMin_passwd_len  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("PASSWORDHISTLEN")) == 0)
			{
				pSelected->dwPassword_hist_len  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("MAXPASSWDAGE")) == 0)
			{
				pSelected->dwMax_passwd_age  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "DISABLEGUEST")) == 0)
			{
				pSelected->dwDisable_guest  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "LOCKOUTTHRESHOLD")) == 0)
			{
				pSelected->dwLockout_threshold  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("USRMOD3LOCKOUTDURATION")) == 0)
			{
				pSelected->dwUsrmod3_lockout_duration  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("USRMOD3LOCKOUTOBSERVATION_WINDOW")) == 0)
			{
				pSelected->dwUsrmod3_lockout_observation_window  = TRUE;
			}

			//安全选项策略（1启用，0禁用）
			else if(_tcscmp(wsTemp.c_str(), _T("CLEARPAGESHUTDOWN")) == 0)
			{
				pSelected->dwClearPageShutDown  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DONTDISPLAYLASTUSERNAME")) == 0)
			{
				pSelected->dwDontDisplayLastUserName  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DISABLECAD")) == 0)
			{
				pSelected->dwDisableCAD  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("RESTRICTANONYMOUSSAM")) == 0)
			{
				pSelected->dwRestrictAnonymousSam  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("RESTRICTANONYMOUS")) == 0)
			{
				pSelected->dwRestrictAnonymous  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("AUTORUN")) == 0)
			{
				pSelected->dwAutoRun  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("SHARE")) == 0)
			{
				pSelected->dwShare  = TRUE;
			}

			// 新基线策略：1(启用)， 0（禁用）
			else if(_tcscmp(wsTemp.c_str(), _T("SYSTEMLOG")) == 0)
			{
				pSelected->dwSystemLog  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "SECLOG")) == 0)
			{
				pSelected->dwSeclog  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("APPLICATIONLOG")) == 0)
			{
				pSelected->dwApplicationLog  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORCEDLOGOFF")) == 0)
			{
				pSelected->dwForcedLogoff  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDFLOPPYCOPY")) == 0)
			{
				pSelected->dwForbidFloppyCopy  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDCONSOLEAUTOLOGIN")) == 0)
			{
				pSelected->dwForbidConsoleAutoLogin  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDAUTOSHUTDOWN") )== 0)
			{
				pSelected->dwForbidAutoShutdown  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("CACHEDLOGONSCOUNT")) == 0)
			{
				pSelected->dwCachedLogonsCount  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DISABLEDOMAINCREDS")) == 0)
			{
				pSelected->dwDisableDomainCreds  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDGETHELP")) == 0)
			{
				pSelected->dwForbidGethelp  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDAUTOREBOOT")) == 0)
			{
				pSelected->dwForbidAutoReboot  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDAUTOLOGIN")) == 0)
			{
				pSelected->dwForbidAutoLogin  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDCHANGEIP")) == 0)
			{
				pSelected->dwForbidChangeIp  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDCHANGENAME")) == 0)
			{
				pSelected->dwForbidChangeName  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("ENABLEUAC")) == 0)
			{
				pSelected->dwEnableUac  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "CHANGERDPPORTNUMBER")) == 0)
			{
				pSelected->dwRdpRortNum  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DEPIN")) == 0)
			{
				pSelected->dwDepIn  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DEPOUT")) == 0)
			{
				pSelected->dwDepOut  = TRUE;
			}


			// add by mingming.shi 2021-09-20
			else if(_tcscmp(wsTemp.c_str(),_T( "SCRNSAVE")) == 0)
			{
				pSelected->dwScrnSave  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DELETEIPFORWARDENTRY")) == 0)
			{
				pSelected->dwDeleteIpForwardEntry  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("REMOTEHOSTRDP")) == 0)
			{
				pSelected->dwRemoteHostRDP  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "REMOTELOGINTIME")) == 0)
			{
				pSelected->dwRemoteLoginTime  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("FORBIDADMINTURNOFF")) == 0)
			{
				pSelected->dwForbidAdminToTurnOff  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("SYN_ATTACK_PROTECT")) == 0)
			{
				pSelected->dwSynAttackDetectionDesign  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(),_T( "USER_PWD_EXPIRE_PROMPT")) == 0)
			{
				pSelected->dwPasswordExpiryWarning  = TRUE;
			}
			else if(_tcscmp(wsTemp.c_str(), _T("DISABLE_DEFAULT_OSUSER")) == 0)
			{
				pSelected->dwForbidDefaultOPTUser  = TRUE;
			}
		}
	}



	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("GetValue *****END*-**********************"));
	root.clear();

    bRet = TRUE;

END:

	return bRet;
}


//安全基线，保存原始配置

/*
* @fn			baseLine_PL_New_Origin_GetJson
* @brief		从Json获取安全基线V2的内容
* @param[in]    strJson: 安全基线V2的Json字符串
* @param[out]	stContentValue: 保存json解析后的安全基线v1结构体
				mapContentV2Value: 保存json解析后的安全基线v2Json字符串数组
				pStrError: 
* @return		
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-21
*/
BOOL CWLJsonParse::baseLine_PL_New_Origin_GetValue(__in std::string strJson, __out BASELINE_PL_NEW_ST &stContentValue, __out map<wstring,std::string> &mapContentV2Value, __out tstring *pStrError/* = NULL */)
{
	BOOL bRet = FALSE;
	Json::Value Root;
	Json::Value subRoot;
	Json::Value TempRoot;
	Json::Value retRoot;
	Json::Reader reader;
	Json::FastWriter write;

	std::wstring strTemp = L"";

    wostringstream  wosError;

	if( strJson.length() == 0)
	{
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto _exit_;
	}

	if ( !reader.parse(strJson, Root))
	{
        wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strJson).c_str() << _T(",");
		goto _exit_;
	}

    if ( Root.isMember("CMDContentV1"))
    {
        subRoot = Root["CMDContentV1"];
    }
    else if ( Root.isMember("CMDContent"))
    {
        subRoot = Root["CMDContent"];
    }
    else
    {
        wosError << _T("Root has not <CMDContentV1> or <CMDContent> member neither!") << _T(", json=") << UTF8ToUnicode(strJson).c_str() << _T(",");
        goto _exit_;
    }


	// 审核策略
    if(subRoot.isMember("dwAuditCategorySystem"))                 { stContentValue.dwAuditCategorySystem                 = subRoot["dwAuditCategorySystem"].asInt();                  }
	if(subRoot.isMember("dwAuditCategoryLogon"))                  { stContentValue.dwAuditCategoryLogon                  = subRoot["dwAuditCategoryLogon"].asInt();                  }
	if(subRoot.isMember("dwAuditCategoryObjectAccess"))           { stContentValue.dwAuditCategoryObjectAccess           = subRoot["dwAuditCategoryObjectAccess"].asInt();           }
	if(subRoot.isMember("dwAuditCategoryPrivilegeUse"))           { stContentValue.dwAuditCategoryPrivilegeUse           = subRoot["dwAuditCategoryPrivilegeUse"].asInt();           }
	if(subRoot.isMember("dwAuditCategoryDetailedTracking"))       { stContentValue.dwAuditCategoryDetailedTracking       = subRoot["dwAuditCategoryDetailedTracking"].asInt();       }
	if(subRoot.isMember("dwAuditCategoryPolicyChange"))           { stContentValue.dwAuditCategoryPolicyChange           = subRoot["dwAuditCategoryPolicyChange"].asInt();           }
	if(subRoot.isMember("dwAuditCategoryAccountManagement"))      { stContentValue.dwAuditCategoryAccountManagement      = subRoot["dwAuditCategoryAccountManagement"].asInt();      }
	if(subRoot.isMember("dwAuditCategoryDirectoryServiceAccess")) { stContentValue.dwAuditCategoryDirectoryServiceAccess = subRoot["dwAuditCategoryDirectoryServiceAccess"].asInt(); }
	if(subRoot.isMember("dwAuditCategoryAccountLogon"))           { stContentValue.dwAuditCategoryAccountLogon           = subRoot["dwAuditCategoryAccountLogon"].asInt();           }

	// 密码策略
	if(subRoot.isMember("dwPasswordComplexity"))                  { stContentValue.dwPasswordComplexity                  = subRoot["dwPasswordComplexity"].asInt();                  }
	if(subRoot.isMember("dwMin_passwd_len"))                      { stContentValue.dwMin_passwd_len                      = subRoot["dwMin_passwd_len"].asInt();                      }
	if(subRoot.isMember("dwPassword_hist_len"))                   { stContentValue.dwPassword_hist_len                   = subRoot["dwPassword_hist_len"].asInt();                   }
	if(subRoot.isMember("dwMax_passwd_age"))                      { stContentValue.dwMax_passwd_age                      = subRoot["dwMax_passwd_age"].asInt();                      }
	if(subRoot.isMember("dwDisable_guest"))                       { stContentValue.dwDisable_guest                       = subRoot["dwDisable_guest"].asInt();                       }

	// leve3
	if(subRoot.isMember("dwLockout_threshold"))                   { stContentValue.dwLockout_threshold                   = subRoot["dwLockout_threshold"].asInt();                   }
	if(subRoot.isMember("dwUsrmod3_lockout_duration"))            { stContentValue.dwUsrmod3_lockout_duration            = subRoot["dwUsrmod3_lockout_duration"].asInt();            }
	if(subRoot.isMember("dwUsrmod3_lockout_observation_window"))  { stContentValue.dwUsrmod3_lockout_observation_window  = subRoot["dwUsrmod3_lockout_observation_window"].asInt();  }

    // 安全选项策略：1(启用)， 0（禁用）
	if(subRoot.isMember("dwClearPageShutDown"))                   { stContentValue.dwClearPageShutDown                   = subRoot["dwClearPageShutDown"].asInt();                   }
	if(subRoot.isMember("dwDontDisplayLastUserName"))             { stContentValue.dwDontDisplayLastUserName             = subRoot["dwDontDisplayLastUserName"].asInt();             }
	if(subRoot.isMember("dwDisableCAD"))                          { stContentValue.dwDisableCAD                          = subRoot["dwDisableCAD"].asInt();                          }
	if(subRoot.isMember("dwRestrictAnonymousSam"))                { stContentValue.dwRestrictAnonymousSam                = subRoot["dwRestrictAnonymousSam"].asInt();                }
	if(subRoot.isMember("dwRestrictAnonymous"))                   { stContentValue.dwRestrictAnonymous                   = subRoot["dwRestrictAnonymous"].asInt();                   }
	if(subRoot.isMember("dwAutoRun"))                             { stContentValue.dwAutoRun                             = subRoot["dwAutoRun"].asInt();                             }
	if(subRoot.isMember("dwShare"))                               { stContentValue.dwShare                               = subRoot["dwShare"].asInt();                               }

	// 新基线策略：1(启用)， 0（禁用）
    if(subRoot.isMember("dwApplicationLog"))                      { stContentValue.dwApplicationLog                      = subRoot["dwApplicationLog"].asInt();                           }
	if(subRoot.isMember("dwSystemLog"))                           { stContentValue.dwSystemLog                           = subRoot["dwSystemLog"].asInt();                           }
	if(subRoot.isMember("dwSeclog"))                              { stContentValue.dwSeclog                              = subRoot["dwSeclog"].asInt();                              }
	if(subRoot.isMember("dwForbidFloppyCopy"))                    { stContentValue.dwForbidFloppyCopy                    = subRoot["dwForbidFloppyCopy"].asInt();                    }
	if(subRoot.isMember("dwForcedLogoff"))                        { stContentValue.dwForcedLogoff                        = subRoot["dwForcedLogoff"].asInt();                        }
	if(subRoot.isMember("dwForbidConsoleAutoLogin"))              { stContentValue.dwForbidConsoleAutoLogin              = subRoot["dwForbidConsoleAutoLogin"].asInt();              }
	if(subRoot.isMember("dwCachedLogonsCount"))                   { stContentValue.dwCachedLogonsCount                   = subRoot["dwCachedLogonsCount"].asInt();                   }
	if(subRoot.isMember("dwForbidAutoShutdown"))                  { stContentValue.dwForbidAutoShutdown                  = subRoot["dwForbidAutoShutdown"].asInt();                  }
	if(subRoot.isMember("dwDisableDomainCreds"))                  { stContentValue.dwDisableDomainCreds                  = subRoot["dwDisableDomainCreds"].asInt();                  }
	if(subRoot.isMember("dwForbidGethelp"))                       { stContentValue.dwForbidGethelp                       = subRoot["dwForbidGethelp"].asInt();                       }
	if(subRoot.isMember("dwForbidAutoReboot"))                    { stContentValue.dwForbidAutoReboot                    = subRoot["dwForbidAutoReboot"].asInt();                    }
	if(subRoot.isMember("dwForbidAutoLogin"))                     { stContentValue.dwForbidAutoLogin                     = subRoot["dwForbidAutoLogin"].asInt();                     }
	if(subRoot.isMember("dwForbidChangeIp"))                      { stContentValue.dwForbidChangeIp                      = subRoot["dwForbidChangeIp"].asInt();                      }
	if(subRoot.isMember("dwForbidChangeName"))                    { stContentValue.dwForbidChangeName                    = subRoot["dwForbidChangeName"].asInt();                    }
	if(subRoot.isMember("dwEnableUac"))                           { stContentValue.dwEnableUac                           = subRoot["dwEnableUac"].asInt();                           }
	if(subRoot.isMember("dwRdpRortNum"))                          { stContentValue.dwRdpRortNum                          = subRoot["dwRdpRortNum"].asInt();                          }
	if(subRoot.isMember("dwDepIn"))                               { stContentValue.dwDepIn                               = subRoot["dwDepIn"].asInt();                               }
	if(subRoot.isMember("dwDepOut"))                              { stContentValue.dwDepOut                              = subRoot["dwDepOut"].asInt();                              }


	subRoot.clear();	

	if(Root.isMember("CMDContentV2") && !Root["CMDContentV2"].isNull())
	{
		subRoot = Root["CMDContentV2"];

		// 设置屏保
		if(subRoot.isMember("ScrnSave"))
		{
			TempRoot = subRoot["ScrnSave"];
			retRoot["ScrnSave"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("SCRNSAVE")] = write.write(retRoot);
			retRoot.clear();
		}

		// SYN攻击保护状态检测
		if(subRoot.isMember("SynProtection"))
		{
			TempRoot = subRoot["SynProtection"];
			retRoot["SynProtection"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("SYN_ATTACK_PROTECT")] = write.write(retRoot);
			retRoot.clear();
		}

		// 用户口令过期提醒
		if(subRoot.isMember("PasswordExpiryWarning"))
		{
			TempRoot = subRoot["PasswordExpiryWarning"];
			retRoot["PasswordExpiryWarning"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("USER_PWD_EXPIRE_PROMPT")] = write.write(retRoot);
			retRoot.clear();
		}

		// 删除默认路由配置
		if(subRoot.isMember("DefaultIpRoute"))
		{
			TempRoot = subRoot["DefaultIpRoute"];
			retRoot["DefaultIpRoute"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("DELETEIPFORWARDENTRY")] = write.write(retRoot);
			retRoot.clear();
		}

		// 禁止非管理员关机
		if(subRoot.isMember("ForbidUserShutDown"))
		{
			TempRoot = subRoot["ForbidUserShutDown"];
			retRoot["ForbidUserShutDown"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("FORBIDADMINTURNOFF")] = write.write(retRoot);
			retRoot.clear();
		}

		// 远程主机RPD服务
		if(subRoot.isMember("RemoteHostRDP"))
		{
			TempRoot = subRoot["RemoteHostRDP"];
			retRoot["RemoteHostRDP"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("REMOTEHOSTRDP")] = write.write(retRoot);
			retRoot.clear();
		}

		// 默认操作用户禁用
		if(subRoot.isMember("ForbidDefaultOPTUser"))
		{
			TempRoot = subRoot["ForbidDefaultOPTUser"];
			retRoot["ForbidDefaultOPTUser"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("DISABLE_DEFAULT_OSUSER")] = write.write(retRoot);
			retRoot.clear();
		}

		// 用户账户控制
		if(subRoot.isMember("EnableUAC"))
		{
			TempRoot = subRoot["EnableUAC"];
			retRoot["EnableUAC"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("ENABLEUAC")] = write.write(retRoot);
			retRoot.clear();
		}

		// 限制远程登录时间 
		if(subRoot.isMember("RemoteLoginTime"))
		{
			TempRoot = subRoot["RemoteLoginTime"];
			retRoot["RemoteLoginTime"] = (Json::Value)TempRoot;
			mapContentV2Value[_T("REMOTELOGINTIME")] = write.write(retRoot);
			retRoot.clear();
		}

        // 操作系统日志 - 安全日志
        if(subRoot.isMember("SECLOG"))
        {
            TempRoot = subRoot["SECLOG"];
            retRoot["SECLOG"] = (Json::Value)TempRoot;
            mapContentV2Value[_T("SECLOG")] = write.write(retRoot);
            retRoot.clear();
        }

        // 操作系统日志 - 系统日志
        if(subRoot.isMember("SYSTEMLOG"))
        {
            TempRoot = subRoot["SYSTEMLOG"];
            retRoot["SYSTEMLOG"] = (Json::Value)TempRoot;
            mapContentV2Value[_T("SYSTEMLOG")] = write.write(retRoot);
            retRoot.clear();
        }

        // 操作系统日志 - 应用程序日志
        if(subRoot.isMember("APPLICATIONLOG"))
        {
            TempRoot = subRoot["APPLICATIONLOG"];
            retRoot["APPLICATIONLOG"] = (Json::Value)TempRoot;
            mapContentV2Value[_T("APPLICATIONLOG")] = write.write(retRoot);
            retRoot.clear();
        }

	}
	bRet = TRUE;

_exit_:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

	return bRet;
}

/*
* @fn			baseLine_PL_New_Origin_GetJson
* @brief		获取安全基线V2的内容
* @param[in]    
				mapContentV2Value: 保存安全基线v2的Json字符串map
* @param[out]	
* @return		
*               
* @detail      	
* @author		mingming.shi
* @date			2021-09-21
*/
std::string CWLJsonParse::baseLine_PL_New_Origin_GetJson( __in BASELINE_PL_NEW_ST &ContentValue, __in map<wstring, std::string> &mapContentV2Value)
{
	std::string strJson;
	Json::Value subRoot;
	Json::Value TempRoot;
	Json::Value  retRoot;
	Json::Value  Root;
	Json::Value  RootV1;
	Json::Value  RootV2;
	Json::Reader reader;
	Json::FastWriter writer;
	map<wstring, std::string>::iterator it;

	// V1的内容
	// 审核策略
	RootV1["dwAuditCategorySystem"]                 = (int)ContentValue.dwAuditCategorySystem;
	RootV1["dwAuditCategoryLogon"]                  = (int)ContentValue.dwAuditCategoryLogon;
	RootV1["dwAuditCategoryObjectAccess"]           = (int)ContentValue.dwAuditCategoryObjectAccess;
	RootV1["dwAuditCategoryPrivilegeUse"]           = (int)ContentValue.dwAuditCategoryPrivilegeUse;
	RootV1["dwAuditCategoryDetailedTracking"]       = (int)ContentValue.dwAuditCategoryDetailedTracking;
	RootV1["dwAuditCategoryPolicyChange"]           = (int)ContentValue.dwAuditCategoryPolicyChange;
	RootV1["dwAuditCategoryAccountManagement"]      = (int)ContentValue.dwAuditCategoryAccountManagement;
	RootV1["dwAuditCategoryDirectoryServiceAccess"] = (int)ContentValue.dwAuditCategoryDirectoryServiceAccess;
	RootV1["dwAuditCategoryAccountLogon"]           = (int)ContentValue.dwAuditCategoryAccountLogon;

	// 密码策略
	RootV1["dwPasswordComplexity"]                  = (int)ContentValue.dwPasswordComplexity;
	RootV1["dwMin_passwd_len"]                      = (int)ContentValue.dwMin_passwd_len;
	RootV1["dwPassword_hist_len"]                   = (int)ContentValue.dwPassword_hist_len;
	RootV1["dwMax_passwd_age"]                      = (int)ContentValue.dwMax_passwd_age;
	RootV1["dwDisable_guest"]                       = (int)ContentValue.dwDisable_guest;
	RootV1["dwLockout_threshold"]                   = (int)ContentValue.dwLockout_threshold;
	RootV1["dwUsrmod3_lockout_duration"]            = (int)ContentValue.dwUsrmod3_lockout_duration;
	RootV1["dwUsrmod3_lockout_observation_window"]  = (int)ContentValue.dwUsrmod3_lockout_observation_window;

    // 安全选项策略：1(启用)， 0（禁用）
    RootV1["dwClearPageShutDown"]                   = (int)ContentValue.dwClearPageShutDown;                   
    RootV1["dwDontDisplayLastUserName"]             = (int)ContentValue.dwDontDisplayLastUserName;             
    RootV1["dwDisableCAD"]                          = (int)ContentValue.dwDisableCAD;                          
    RootV1["dwRestrictAnonymousSam"]                = (int)ContentValue.dwRestrictAnonymousSam;                
    RootV1["dwRestrictAnonymous"]                   = (int)ContentValue.dwRestrictAnonymous;                   
    RootV1["dwAutoRun"]                             = (int)ContentValue.dwAutoRun;                             
    RootV1["dwShare"]                               = (int)ContentValue.dwShare;                               


	// 新基线策略：1(启用)， 0（禁用）
	RootV1["dwSystemLog"]                           = (int)ContentValue.dwSystemLog;
	RootV1["dwSeclog"]                              = (int)ContentValue.dwSeclog;
	RootV1["dwApplicationLog"]                      = (int)ContentValue.dwApplicationLog;
	RootV1["dwForcedLogoff"]                        = (int)ContentValue.dwForcedLogoff;
	RootV1["dwForbidFloppyCopy"]                    = (int)ContentValue.dwForbidFloppyCopy;
	RootV1["dwForbidConsoleAutoLogin"]              = (int)ContentValue.dwForbidConsoleAutoLogin;
	RootV1["dwForbidAutoShutdown"]                  = (int)ContentValue.dwForbidAutoShutdown;
	RootV1["dwCachedLogonsCount"]                   = (int)ContentValue.dwCachedLogonsCount;
	RootV1["dwDisableDomainCreds"]                  = (int)ContentValue.dwDisableDomainCreds;
	RootV1["dwForbidGethelp"]                       = (int)ContentValue.dwForbidGethelp;
	RootV1["dwForbidAutoReboot"]                    = (int)ContentValue.dwForbidAutoReboot;
	RootV1["dwForbidAutoLogin"]                     = (int)ContentValue.dwForbidAutoLogin;
	RootV1["dwForbidChangeIp"]                      = (int)ContentValue.dwForbidChangeIp;
	RootV1["dwForbidChangeName"]                    = (int)ContentValue.dwForbidChangeName;
	RootV1["dwEnableUac"]                           = (int)ContentValue.dwEnableUac;    //该处备份已经失效，实际备份在mapContentV2Value中
	RootV1["dwRdpRortNum"]                          = (int)ContentValue.dwRdpRortNum;
	RootV1["dwDepIn"]                               = (int)ContentValue.dwDepIn;
	RootV1["dwDepOut"]                              = (int)ContentValue.dwDepOut;

	// V2 的内容
	for (it = mapContentV2Value.begin(); it != mapContentV2Value.end(); it++)
	{
		if (!reader.parse(it->second, subRoot))
		{
			if(it != mapContentV2Value.end())
			{
				continue;
			}
			else
			{
				goto _exist_;
			}
		}
		if( it->first ==  _T("SCRNSAVE"))
		{
			RootV2["ScrnSave"] = subRoot["ScrnSave"];
		}
		else if( it->first ==  _T("SYN_ATTACK_PROTECT"))
		{
			RootV2["SynProtection"] = subRoot["SynProtection"];
		}
		else if( it->first ==  _T("USER_PWD_EXPIRE_PROMPT"))
		{
			RootV2["PasswordExpiryWarning"] = subRoot["PasswordExpiryWarning"];
		}
		else if( it->first ==  _T("DELETEIPFORWARDENTRY"))
		{
			RootV2["DefaultIpRoute"] = subRoot["DefaultIpRoute"];
		}
		else if( it->first ==  _T("FORBIDADMINTURNOFF"))
		{
			RootV2["ForbidUserShutDown"] = subRoot["ForbidUserShutDown"];
		}
		else if( it->first ==  _T("REMOTEHOSTRDP"))
		{
			RootV2["RemoteHostRDP"] = subRoot["RemoteHostRDP"];
		}
		else if( it->first ==  _T("DISABLE_DEFAULT_OSUSER"))
		{
			RootV2["ForbidDefaultOPTUser"] = subRoot["ForbidDefaultOPTUser"];
		}
		else if( it->first ==  _T("REMOTELOGINTIME"))
		{
			RootV2["RemoteLoginTime"] = subRoot["RemoteLoginTime"];
		}
		else if( it->first ==  _T("ENABLEUAC"))
		{
			RootV2["EnableUAC"] = subRoot["EnableUAC"];
		}
        else if( it->first ==  _T("SYSTEMLOG"))
        {
            RootV2["SYSTEMLOG"] = subRoot["SYSTEMLOG"];
        }
        else if( it->first ==  _T("SECLOG"))
        {
            RootV2["SECLOG"] = subRoot["SECLOG"];
        }
        else if( it->first ==  _T("APPLICATIONLOG"))
        {
            RootV2["APPLICATIONLOG"] = subRoot["APPLICATIONLOG"];
        }
		
		subRoot.clear();
	}

_exist_:


	Root["CMDContentV1"] = (Json::Value)RootV1;
	Root["CMDContentV2"] = (Json::Value)RootV2;

	strJson = writer.write(Root);

	retRoot.clear();
	Root.clear();
	RootV1.clear();
	RootV2.clear();

    return strJson;
}


//【资产信息：用于注册和保存本地】
std::string CWLJsonParse::AssetsInfo_GetJson(__in ASSETS_INFO_STRUCT stAssetsInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID )
{
    /*
    [{
        "ComputerID":"1EFADEF8WIN-PODD20V4JM1",
        "CMDID":270,（固定不变）
        "CMDTYPE":150,（固定不变）
        "CMDContent":
        {
            "AssetsPersonliable":"XXX",（资产管理信息：责任人）
            "AssetsName":"XXX",（资产管理信息：终端名称）
            "AssetsHost":"XXX",（资产管理信息：宿主机，可为空）
            "AssetsNumber":"XXX",（资产管理信息：资产编号，可为空）
            "AssetsPosition":"XXX",（资产管理信息：资产位置，可为空）
            "AssetsJobNo":"XXX",（资产管理信息：工号，可为空）
            "AssetsTel":"XXX",（资产管理信息：联系电话，可为空）
            "AssetsMail":"XXX",（资产管理信息：联系邮箱，可为空）
            "AssetsDepartment":"XXX",（资产管理信息：部门，可为空）       
        }
    }]
    */

    std::string sJson = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value Item;
    Json::Value CMDContentItem;
    
    wostringstream  strFormat;

    // 参数
    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; /*200*/
    Item["CMDID"] = (int)nCmdID;     /*270*/
    
    // 责任人
    CMDContentItem["AssetsPersonliable"]    = UnicodeToUTF8(stAssetsInfo.szAssetsPersonliable);
    // 终端名称
    CMDContentItem["AssetsName"]            = UnicodeToUTF8(stAssetsInfo.szAssetsName);
    // 宿主机
    CMDContentItem["AssetsHost"]            = UnicodeToUTF8(stAssetsInfo.szAssetsHost);
    // 资产编号
    CMDContentItem["AssetsNumber"]          = UnicodeToUTF8(stAssetsInfo.szAssetsNumber);
    // 资产位置
    CMDContentItem["AssetsPosition"]        = UnicodeToUTF8(stAssetsInfo.szAssetsPosition);
    // 工号
    CMDContentItem["AssetsJobNo"]           = UnicodeToUTF8(stAssetsInfo.szAssetsJobNo);
    // 联系电话
    CMDContentItem["AssetsTel"]             = UnicodeToUTF8(stAssetsInfo.szAssetsTel);
    // 联系邮箱
    CMDContentItem["AssetsMail"]            = UnicodeToUTF8(stAssetsInfo.szAssetsMail);
    // 部门
    CMDContentItem["AssetsDepartment"]      = UnicodeToUTF8(stAssetsInfo.szAssetsDepartment);
    
    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);

    sJson = writer.write(root);
    root.clear();

    return sJson;
}

BOOL CWLJsonParse::AssetsInfo_GetValue(__in const std::string strJson, __out ASSETS_INFO_STRUCT &stAssetsInfo, __out tstring *pStrError /* = NULL */ )
{
    /*
    [{
        "ComputerID":"1EFADEF8WIN-PODD20V4JM1",
        "CMDID":270,（固定不变）
        "CMDTYPE":150,（固定不变）
        "CMDContent":
        {
            "AssetsPersonliable":"XXX",（资产管理信息：责任人）
            "AssetsName":"XXX",（资产管理信息：终端名称）
            "AssetsHost":"XXX",（资产管理信息：宿主机，可为空）
            "AssetsNumber":"XXX",（资产管理信息：资产编号，可为空）
            "AssetsPosition":"XXX",（资产管理信息：资产位置，可为空）
            "AssetsJobNo":"XXX",（资产管理信息：工号，可为空）
            "AssetsTel":"XXX",（资产管理信息：联系电话，可为空）
            "AssetsMail":"XXX",（资产管理信息：联系邮箱，可为空）
            "AssetsDepartment":"XXX",（资产管理信息：部门，可为空）       
        }
    }]
    */

    BOOL bResult = FALSE;
    std::string strValue;
    Json::Reader reader;
    Json::Value  root;
    Json::Value  CMDContent;
    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
        goto END;
    }
    strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent") << _T(",");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    // 责任人
    if(CMDContent.isMember("AssetsPersonliable") && !CMDContent["AssetsPersonliable"].isNull())
    {
        std::wstring wsAssetsPeronliable = UTF8ToUnicode(CMDContent["AssetsPersonliable"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsPersonliable, _countof(stAssetsInfo.szAssetsPersonliable), wsAssetsPeronliable.c_str(), _countof(stAssetsInfo.szAssetsPersonliable) - 1);
    }
    else
    {
        wosError << _T("parse [AssetsPersonliable] fail, maybe it is not exist or is NULL, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }
    
    // 终端名称
    if(CMDContent.isMember("AssetsName") && !CMDContent["AssetsName"].isNull())
    {
        std::wstring wsAssetsName = UTF8ToUnicode(CMDContent["AssetsName"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsName, _countof(stAssetsInfo.szAssetsName), wsAssetsName.c_str(), _countof(stAssetsInfo.szAssetsName) - 1);
    }
    else
    {
        wosError << _T("parse [AssetsName] fail, maybe it is not exist or is NULL, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
        goto END;
    }

    // 宿主机
    if(CMDContent.isMember("AssetsHost"))
    {
        std::wstring wsAssetsHost = UTF8ToUnicode(CMDContent["AssetsHost"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsHost, _countof(stAssetsInfo.szAssetsHost), wsAssetsHost.c_str(), _countof(stAssetsInfo.szAssetsHost) - 1);
    }

    // 资产编号
    if(CMDContent.isMember("AssetsNumber"))
    {
        std::wstring wsAssetsNumber = UTF8ToUnicode(CMDContent["AssetsNumber"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsNumber, _countof(stAssetsInfo.szAssetsNumber), wsAssetsNumber.c_str(), _countof(stAssetsInfo.szAssetsNumber) - 1);
    }

    // 资产位置
    if(CMDContent.isMember("AssetsPosition"))
    {
        std::wstring wsAssetsPosition = UTF8ToUnicode(CMDContent["AssetsPosition"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsPosition, _countof(stAssetsInfo.szAssetsPosition), wsAssetsPosition.c_str(), _countof(stAssetsInfo.szAssetsPosition) - 1);
    }

    // 工号
    if(CMDContent.isMember("AssetsJobNo"))
    {
        std::wstring wsAssetsJobNo = UTF8ToUnicode(CMDContent["AssetsJobNo"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsJobNo, _countof(stAssetsInfo.szAssetsJobNo), wsAssetsJobNo.c_str(), _countof(stAssetsInfo.szAssetsJobNo) - 1);
    }

    // 联系电话
    if(CMDContent.isMember("AssetsTel"))
    {
        std::wstring wsAssetsTel = UTF8ToUnicode(CMDContent["AssetsTel"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsTel, _countof(stAssetsInfo.szAssetsTel), wsAssetsTel.c_str(), _countof(stAssetsInfo.szAssetsTel) - 1);
    }

    // 联系邮箱
    if(CMDContent.isMember("AssetsMail"))
    {
        std::wstring wsAssetsMail = UTF8ToUnicode(CMDContent["AssetsMail"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsMail, _countof(stAssetsInfo.szAssetsMail), wsAssetsMail.c_str(), _countof(stAssetsInfo.szAssetsMail) - 1);
    }

    // 部门
    if(CMDContent.isMember("AssetsDepartment"))
    {
        std::wstring wsAssetsDepartment = UTF8ToUnicode(CMDContent["AssetsDepartment"].asString());
        _tcsncpy_s(stAssetsInfo.szAssetsDepartment, _countof(stAssetsInfo.szAssetsDepartment), wsAssetsDepartment.c_str(), _countof(stAssetsInfo.szAssetsDepartment) - 1);
    }

    bResult = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bResult;
}

// 高危端口
BOOL CWLJsonParse::SecDetect_HighRiskPorts_GetValue(__in const std::string strJson, __out std::vector<SECDETECT_HIGHRISK_PORTS_STRUCT> &vecHighRiskPorts, __out tstring *pStrError /* = NULL */ )
{
    /*
    传入的Json：
	[
        {
            "Protocal":"TCP",
            "Port": "21"
        },
        {
            "Protocal":"TCP",
            "Port": "23"
        }
	]
    */

    BOOL bResult = FALSE;

    std::string strValue = strJson;
    unsigned int uiSize;

    Json::Reader reader;
    Json::Value  PortsArray;

    wostringstream  wosError;

	vecHighRiskPorts.clear();

	if( strJson.length() == 0)
	{
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}

	if (!reader.parse(strValue, PortsArray))
	{
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

    if ( PortsArray.isNull() || !PortsArray.isArray())
    {
        wosError << _T("invalid json, json is null or json is not array") << _T(",");
        goto END;
    }

	uiSize = PortsArray.size();
	for (int i = 0; i < uiSize; i++)
	{
		SECDETECT_HIGHRISK_PORTS_STRUCT stHighRiskPorts;

        // 协议（字符）
        std::wstring wsProtocal = UTF8ToUnicode(PortsArray[i]["Protocal"].asString());
        _tcsncpy_s(stHighRiskPorts.szProtocal, _countof(stHighRiskPorts.szProtocal), wsProtocal.c_str(), _countof(stHighRiskPorts.szProtocal) - 1);

        // 端口（字符）
        std::wstring wsPort = UTF8ToUnicode(PortsArray[i]["Port"].asString());
        _tcsncpy_s(stHighRiskPorts.szPort, _countof(stHighRiskPorts.szPort), wsPort.c_str(), _countof(stHighRiskPorts.szPort) - 1);

        // 协议（整型表示）
        if ( 0 == wsProtocal.compare(_T("TCP")))
        {
            stHighRiskPorts.dwProtocal = EM_PORT_PROTOCOL_TCP; //TCP
        }
        else if ( 0 == wsProtocal.compare(_T("UDP")))
        {
            stHighRiskPorts.dwProtocal = EM_PORT_PROTOCOL_UDP; //UDP
        }
        else
        {
            stHighRiskPorts.dwProtocal = EM_PORT_PROTOCOL_UNKNOWN;
        }
        
        // 端口（整型表示）
        stHighRiskPorts.usPort = (WORD)_ttol(wsPort.c_str());
	
        // 追加
		vecHighRiskPorts.push_back(stHighRiskPorts);	
	}

	bResult = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bResult;
}

// 高危服务
BOOL CWLJsonParse::SecDetect_HighRiskSecvices_GetValue(__in const std::string strJson, __out std::vector<SECDETECT_HIGHRISK_SERVICES_STRUCT> &vecHighRiskSecvices, __out tstring *pStrError /* = NULL */ )
{
    /*
    传入的Json：
	[
        "Server",
        "Alerter"
	]
    */

    BOOL bResult = FALSE;

    std::string strValue = strJson;
    unsigned int uiSize;

    Json::Reader reader;
    Json::Value  ServicesArray;

    wostringstream  wosError;

	vecHighRiskSecvices.clear();

	if( strJson.length() == 0)
	{
        wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}

	if (!reader.parse(strValue, ServicesArray))
	{
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

    if ( ServicesArray.isNull() || !ServicesArray.isArray())
    {
        wosError << _T("invalid json, json is null or json is not array") << _T(",");
        goto END;
    }

	uiSize = ServicesArray.size();
	for (int i = 0; i < uiSize; i++)
	{
		SECDETECT_HIGHRISK_SERVICES_STRUCT stHighRiskServices;

        // 服务名
        std::wstring wsServiceName = UTF8ToUnicode(ServicesArray[i].asString());
        _tcsncpy_s(stHighRiskServices.szServiceName, _countof(stHighRiskServices.szServiceName), wsServiceName.c_str(), _countof(stHighRiskServices.szServiceName) - 1);
	
        // 追加
		vecHighRiskSecvices.push_back(stHighRiskServices);	
	}

	bResult = TRUE;

END:

    if (pStrError)
    {
        *pStrError = wosError.str();
    }

    return bResult;
}

//资产扫描->基础信息
std::string CWLJsonParse::SecDetect_BaseInfo_GetJson(__in vector<SECDETECT_BASEINFO_STRUCT> &vecBaseInfoStu)
{
	/*
	{
		“HostInfo”:[（主机信息，修改名称为“基础信息”，新增内容）
					{
						"ComputerName":"WIN-7-Person",（计算机名称）
						"OS":"Windows7",（操作系统）
						"OSVersion":"XXXX",（版本号）
						"OSActivation":1,（激活状态，1激活，0未激活）
						"OSInstallTime":"20XX-XX-XX ……",（安装时间）
						"ComputerIPv4MAC":（IPv4/MAC地址）
										[
										{
											"IPv4":[
													"192.168.X.X",
													"192.168.X.X",
													"192.168.X.X",
													],
											"MAC":"XX.XX.XX.XX.XX.XX",
										},
										{
										},
										],
					"EDRVersion":"XXXX",（终端版本）
					"EDRVirusVersion":"XXXX",（病毒库版本）
					"EDRWhiteListVersion":"XXXX",（预置白名单版本）
					"AccessIP":"192.168.X.X",（最近接入IP。最近一次注册管理平台，使用的本地IP）
					"AccessTime":"20XX-XX-XX XX:XX:XX",（最近接入时间。最近一次注册管理平台的时间）
					"AssetsPersonliable":"XXX",（资产管理信息：责任人）
					"AssetsName":"XXX",（资产管理信息：终端名称）
					"AssetsHost":"XXX",（资产管理信息：宿主机，可为空）
					"AssetsNumber":"XXX",（资产管理信息：资产编号，可为空）
					"AssetsPosition":"XXX",（资产管理信息：资产位置，可为空）
					"AssetsJobNo":"XXX",（资产管理信息：工号，可为空）
					"AssetsTel":"XXX",（资产管理信息：联系电话，可为空）
					"AssetsMail":"XXX",（资产管理信息：联系邮箱，可为空）
					"AssetsDepartment":"XXX",(资产管理信息：部门，可为空)
					}
					],
	}
	*/
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strjson;

	int nCount = (int)vecBaseInfoStu.size();
	for (int i = 0; i < nCount; i++)
	{
		Item["ComputerName"]         = UnicodeToUTF8(vecBaseInfoStu[i].szHost);
		Item["OS"]                   = UnicodeToUTF8(vecBaseInfoStu[i].szOS);
		Item["OSVersion"]            = UnicodeToUTF8(vecBaseInfoStu[i].szOsVersion);
		Item["OSActivation"]         = (int)vecBaseInfoStu[i].dwActivation;
		Item["OSInstallTime"]        = UnicodeToUTF8(vecBaseInfoStu[i].szInstallTime);
		Item["EDRVersion"]           = UnicodeToUTF8(vecBaseInfoStu[i].szClientVersion);
		Item["EDRVirusVersion"]      = UnicodeToUTF8(vecBaseInfoStu[i].szVirusLibVersion);
		Item["EDRWhiteListVersion"]  = UnicodeToUTF8(vecBaseInfoStu[i].szWhiteListVersion);
		Item["AccessIP"]             = UnicodeToUTF8(vecBaseInfoStu[i].szAccessIP);
		Item["AccessTime"]           = UnicodeToUTF8(vecBaseInfoStu[i].szAccessTime);
		Item["AssetsPersonliable"]   = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsPersonliable);
		Item["AssetsName"]           = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsName);
		Item["AssetsHost"]           = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsHost);
		Item["AssetsNumber"]         = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsNumber);
		Item["AssetsPosition"]       = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsPosition);
		Item["AssetsJobNo"]          = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsJobNo);
		Item["AssetsTel"]            = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsTel);
		Item["AssetsMail"]           = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsMail);
		Item["AssetsDepartment"]     = UnicodeToUTF8(vecBaseInfoStu[i].stAssetsInfo.szAssetsDepartment);

		int nIPv4 = (int)vecBaseInfoStu[i].vecIPv4MAC.size();
		Json::Value IPv4Obj;
		for (int j = 0; j < nIPv4; j++)
		{
			IPv4Obj["MAC"] = UnicodeToUTF8( vecBaseInfoStu[i].vecIPv4MAC[j].szMAC );
			for(int k = 0; k < vecBaseInfoStu[i].vecIPv4MAC[j].vecIPAddr.size(); k++)
			{
				IPv4Obj["IPv4"].append( UnicodeToUTF8( vecBaseInfoStu[i].vecIPv4MAC[j].vecIPAddr[k] ) );
			}
			Item["ComputerIPv4MAC"].append(IPv4Obj);
            IPv4Obj.clear();
		}
		
		Items.append(Item);
	}

	root["HostInfo"] = (Json::Value)(Items);
	strjson = writer.write(root);

	root.clear();

	return strjson;
}

BOOL CWLJsonParse::SecDetect_BaseInfo_ParseJson(__in string &strBaseInfoJson, __out vector<SECDETECT_BASEINFO_STRUCT> &vecBaseInfoStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonBaseInfo;
	Json::Reader reader;

	int nCount;

	if(strBaseInfoJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strBaseInfoJson, jsonBaseInfo))
	{
		goto _exit_;
	}

    if ( jsonBaseInfo.isNull())
    {
        goto _exit_;
    }

    if ( jsonBaseInfo.isNull() || !jsonBaseInfo.isArray())
    {
        goto _exit_;
    }

	nCount = jsonBaseInfo.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_BASEINFO_STRUCT stuTemp;

		wcscpy_s(stuTemp.szHost, ArraySize(stuTemp.szHost),
								UTF8ToUnicode(jsonBaseInfo[i]["ComputerName"].asString()).c_str());

		wcscpy_s(stuTemp.szOS, ArraySize(stuTemp.szOS),
								UTF8ToUnicode(jsonBaseInfo[i]["OS"].asString()).c_str());

		wcscpy_s(stuTemp.szOsVersion, ArraySize(stuTemp.szOsVersion),
			                    UTF8ToUnicode(jsonBaseInfo[i]["OSVersion"].asString()).c_str());

		wcscpy_s(stuTemp.szInstallTime, ArraySize(stuTemp.szInstallTime),
								UTF8ToUnicode(jsonBaseInfo[i]["OSInstallTime"].asString()).c_str());

		wcscpy_s(stuTemp.szClientVersion, ArraySize(stuTemp.szClientVersion),
								UTF8ToUnicode(jsonBaseInfo[i]["EDRVersion"].asString()).c_str());

		wcscpy_s(stuTemp.szVirusLibVersion, ArraySize(stuTemp.szVirusLibVersion),
								UTF8ToUnicode(jsonBaseInfo[i]["EDRVirusVersion"].asString()).c_str());

		wcscpy_s(stuTemp.szWhiteListVersion, ArraySize(stuTemp.szWhiteListVersion),
								UTF8ToUnicode(jsonBaseInfo[i]["EDRWhiteListVersion"].asString()).c_str());

		wcscpy_s(stuTemp.szAccessIP, ArraySize(stuTemp.szAccessIP),
								UTF8ToUnicode(jsonBaseInfo[i]["AccessIP"].asString()).c_str());
		
		wcscpy_s(stuTemp.szAccessTime, ArraySize(stuTemp.szAccessTime),
								UTF8ToUnicode(jsonBaseInfo[i]["AccessTime"].asString()).c_str());

		stuTemp.dwActivation = jsonBaseInfo[i]["OSActivation"].asInt();

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsPersonliable, ArraySize(stuTemp.stAssetsInfo.szAssetsPersonliable),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsPersonliable"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsName, ArraySize(stuTemp.stAssetsInfo.szAssetsName),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsName"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsHost, ArraySize(stuTemp.stAssetsInfo.szAssetsHost),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsHost"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsNumber, ArraySize(stuTemp.stAssetsInfo.szAssetsNumber),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsNumber"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsPosition, ArraySize(stuTemp.stAssetsInfo.szAssetsPosition),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsPosition"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsJobNo, ArraySize(stuTemp.stAssetsInfo.szAssetsJobNo),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsJobNo"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsTel, ArraySize(stuTemp.stAssetsInfo.szAssetsTel),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsTel"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsMail, ArraySize(stuTemp.stAssetsInfo.szAssetsMail),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsMail"].asString()).c_str());

		wcscpy_s(stuTemp.stAssetsInfo.szAssetsDepartment, ArraySize(stuTemp.stAssetsInfo.szAssetsDepartment),
								UTF8ToUnicode(jsonBaseInfo[i]["AssetsDepartment"].asString()).c_str());

		Json::Value tempJsonObj = (Json::Value)jsonBaseInfo[i]["ComputerIPv4MAC"];
		int nGroup = tempJsonObj.size();
		for (int j = 0; j < nGroup; j++)
		{
			SECDETECT_BASEINFO_IPv4MAC_STRUCT stIpv4;
			
			_tcsncpy_s(stIpv4.szMAC, ArraySize(stIpv4.szMAC),UTF8ToUnicode(tempJsonObj[j]["MAC"].asString()).c_str(),ArraySize(stIpv4.szMAC)-1);
			for(int k = 0; k < tempJsonObj[j]["IPv4"].size(); k++)
			{
				stIpv4.vecIPAddr.push_back(UTF8ToUnicode(tempJsonObj[j]["IPv4"][k].asString()).c_str());
			}
			
			stuTemp.vecIPv4MAC.push_back(stIpv4);
		}

		vecBaseInfoStu.push_back(stuTemp);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

//资产扫描->硬件信息
std::string CWLJsonParse::SecDetect_Hardware_GetJson(__in vector<SECDETECT_HARDWAREINFO_STRUCT> &vecHardwareStu)
{
	/*
	{
	“Hardware”:[（硬件信息 - 新增一项，其中内容整合了原有的主机信息、分区信息和网卡列表）
				{
					"CPUUseValue":"XXX",（CPU占用率）
					"MemoryUseValue":"XXX",（内存占用率）
					"CPUName":"XXX",（CPU名称）
					"CPUNumber":"XXX",（CPU个数）
					"CPUFreq":"XXX",（CPU频率）
					"CPUType":"XXX",（CPU型号）
					"MemoryTotal":"XXX",（内存总大小）
					"MemoryFree":"XXX",（内存剩余大小）
					"Partition":[  （分区大小）
								{
									"Drive":"C:",      （分区名称）
									"TotalSize":"29.67G",（分区总大小）
									"FreeSize":"12.13G"（分区剩余大小）

								},
								{
									"Drive":"D:",
									"TotalSize":"99.67G",
									"FreeSize":"42.13G"

								},
								{
								}
								]
					"BaseBoardSerial":"XXX",（主板ID）
					"BaseBoardDesc":"XXX",（主板描述）
					"BIOSManu":"XXX",（BIOS制造商）
					"BIOSVersion":"XXX",（BIOS版本）
					"Screen:[
								{	
									"ScreenName":"XXX",（显示器名称）
									"ScreenDevID":"XXX",（显示器分辨率）
								}
								{

								}
							]
					""ScreenName":"XXX",（显示器名称）
					"ScreenDevID":"XXX",（显示器分辨率）
					"Net":[ （网卡信息）
							{
								"NetName":"本地连接",        （网卡名称）
								"NetDesc":"XXXXXXXXX"         （网卡描述）
							},
							{
							}
						]

				}
	]
	}
	*/

	Json::Value root;
	Json::FastWriter writer;
    Json::Value Items = Json::nullValue;
	Json::Value Item;
	std:: string strJson;

	int nCount = (int)vecHardwareStu.size();
	if(0 == nCount)
	{
		goto END;
	}
	
	for (int i = 0; i < nCount; i++)
	{

		for(int nCPU = 0; nCPU < vecHardwareStu[i].vecCPUInfo.size(); nCPU++)
		{
			Item["CPUUseValue"] = UnicodeToUTF8(vecHardwareStu[i].vecCPUInfo[nCPU].szCPURate);
			Item["CPUName"]     = UnicodeToUTF8(vecHardwareStu[i].vecCPUInfo[nCPU].szCPU);
			Item["CPUNumber"]   = UnicodeToUTF8(vecHardwareStu[i].vecCPUInfo[nCPU].szCPUNum);
			Item["CPUFreq"]     = UnicodeToUTF8(vecHardwareStu[i].vecCPUInfo[nCPU].szCPUFreq);
			Item["CPUType"]     = UnicodeToUTF8(vecHardwareStu[i].vecCPUInfo[nCPU].szCPUType);
		}

		for(int nMemory = 0; nMemory < vecHardwareStu[i].vecMemoryInfo.size(); nMemory++)
		{
			Item["MemoryUseValue"] = UnicodeToUTF8(vecHardwareStu[i].vecMemoryInfo[nMemory].szMemoryRate);
			Item["MemoryTotal"]    = UnicodeToUTF8(vecHardwareStu[i].vecMemoryInfo[nMemory].szTotalSize);
			Item["MemoryFree"]	   = UnicodeToUTF8(vecHardwareStu[i].vecMemoryInfo[nMemory].szFreeSize);
		}

		for(int nDisk = 0; nDisk < vecHardwareStu[i].vecDiskInfo.size(); nDisk++)
		{
			Json::Value DiskObj;

			DiskObj["Name"]    = UnicodeToUTF8(vecHardwareStu[i].vecDiskInfo[nDisk].szName);
			DiskObj["Serial"]  = UnicodeToUTF8(vecHardwareStu[i].vecDiskInfo[nDisk].szSerialNumber);
			DiskObj["Size"]    = UnicodeToUTF8(vecHardwareStu[i].vecDiskInfo[nDisk].szSize);

			Item["Disk"].append(DiskObj);
			DiskObj.clear();
		}

		for(int nBios = 0; nBios < vecHardwareStu[i].vecBIOSInfo.size(); nBios++)
		{
			Item["BaseBoardSerial"] = UnicodeToUTF8(vecHardwareStu[i].vecBIOSInfo[nBios].szMainBoardID);
			Item["BaseBoardDesc"]   = UnicodeToUTF8(vecHardwareStu[i].vecBIOSInfo[nBios].szMainBoardDesc);
			Item["BIOSManu"]        = UnicodeToUTF8(vecHardwareStu[i].vecBIOSInfo[nBios].szBiosProv);
			Item["BIOSVersion"]     = UnicodeToUTF8(vecHardwareStu[i].vecBIOSInfo[nBios].szBiosVersion);
		}

		for(int nScreen = 0; nScreen < vecHardwareStu[i].vecDisplayInfo.size(); nScreen++)
		{
			Json::Value ScreenObj;
            SECDETECT_DISPLAYINFO_STRUCT &stScreen = vecHardwareStu[i].vecDisplayInfo[nScreen];
            ScreenObj["ScreenName"]	= UnicodeToUTF8(vecHardwareStu[i].vecDisplayInfo[nScreen].szName);
            ScreenObj["ScreenDevID"]   = UnicodeToUTF8(vecHardwareStu[i].vecDisplayInfo[nScreen].szDeviceID);

            Item["Screen"].append(ScreenObj);
            ScreenObj.clear();
		}

		for(int nNet = 0; nNet < vecHardwareStu[i].vecNetInfo.size(); nNet++)
		{
			Json::Value NetObj;
			NetObj["NetName"] = UnicodeToUTF8(vecHardwareStu[i].vecNetInfo[nNet].szNet);
            NetObj["NetDesc"] = UnicodeToUTF8(vecHardwareStu[i].vecNetInfo[nNet].szNetDesc);
			/*
            NetObj["PhyAddr"]   = UnicodeToUTF8(vecHardwareStu[i].vecNetInfo[nNet].szPhyAddr);

			for(int nIPAddr = 0; nIPAddr < vecHardwareStu[i].vecNetInfo[nNet].vecIPAddr.size(); nIPAddr++)
			{
				NetObj["IPAddr"].append( UnicodeToUTF8(vecHardwareStu[i].vecNetInfo[nNet].vecIPAddr[nIPAddr]) );
			}
            */

			Item["Net"].append(NetObj);
			NetObj.clear();
		}

		Items.append(Item);

		Item.clear();
	}

	root["Hardware"] = (Json::Value)(Items);
	strJson = writer.write(root);

	root.clear();

END:
	return strJson;
}

BOOL CWLJsonParse::SecDetect_Hardware_ParseJson(__in string &strHardwareJson, __out vector<SECDETECT_HARDWAREINFO_STRUCT> &vecHardwareStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonHardware;
	Json::Reader reader;
	int nCount;

	if(strHardwareJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strHardwareJson, jsonHardware))
	{
		goto _exit_;
	}

    if ( jsonHardware.isNull() || !jsonHardware.isArray())
    {
        goto _exit_;
    }

	nCount = jsonHardware.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_HARDWAREINFO_STRUCT stHardware;
		string strTime;
		//time_t t;

		{
			SECDETECT_CPUINFO_STRUCT stCPU;
			_tcsncpy_s(stCPU.szCPU,ArraySize(stCPU.szCPU),UTF8ToUnicode(jsonHardware[i]["CPUName"].asString()).c_str(),ArraySize(stCPU.szCPU)-1);
			_tcsncpy_s(stCPU.szCPUNum,ArraySize(stCPU.szCPUNum),UTF8ToUnicode(jsonHardware[i]["CPUNumber"].asString()).c_str(),ArraySize(stCPU.szCPUNum)-1);
			_tcsncpy_s(stCPU.szCPUFreq,ArraySize(stCPU.szCPUFreq),UTF8ToUnicode(jsonHardware[i]["CPUFreq"].asString()).c_str(),ArraySize(stCPU.szCPUFreq)-1);
			_tcsncpy_s(stCPU.szCPUType,ArraySize(stCPU.szCPUType),UTF8ToUnicode(jsonHardware[i]["CPUType"].asString()).c_str(),ArraySize(stCPU.szCPUType)-1);
			_tcsncpy_s(stCPU.szCPURate,ArraySize(stCPU.szCPURate),UTF8ToUnicode(jsonHardware[i]["CPUUseValue"].asString()).c_str(),ArraySize(stCPU.szCPURate)-1);
			stHardware.vecCPUInfo.push_back(stCPU);
		}

		{
			SECDETECT_MEMORYINFO_STRUCT stMemory;
			_tcsncpy_s(stMemory.szMemoryRate,ArraySize(stMemory.szMemoryRate),UTF8ToUnicode(jsonHardware[i]["MemoryUseValue"].asString()).c_str(),ArraySize(stMemory.szMemoryRate)-1);
			_tcsncpy_s(stMemory.szTotalSize,ArraySize(stMemory.szTotalSize),UTF8ToUnicode(jsonHardware[i]["MemoryTotal"].asString()).c_str(),ArraySize(stMemory.szTotalSize)-1);
			_tcsncpy_s(stMemory.szFreeSize,ArraySize(stMemory.szFreeSize),UTF8ToUnicode(jsonHardware[i]["MemoryFree"].asString()).c_str(),ArraySize(stMemory.szFreeSize)-1);
			stHardware.vecMemoryInfo.push_back(stMemory);
		}

		{
			for(int nDisk = 0; nDisk < jsonHardware[i]["Disk"].size(); nDisk++)
			{
				SECDETECT_DISKINFO_STRUCT stDisk;

				_tcsncpy_s(stDisk.szName, ArraySize(stDisk.szName),
							UTF8ToUnicode(jsonHardware[i]["Disk"][nDisk]["Name"].asString()).c_str(), ArraySize(stDisk.szName) - 1);

				_tcsncpy_s(stDisk.szSerialNumber, ArraySize(stDisk.szSerialNumber),
							UTF8ToUnicode(jsonHardware[i]["Disk"][nDisk]["Serial"].asString()).c_str(), ArraySize(stDisk.szSerialNumber) - 1);

                _tcsncpy_s(stDisk.szSize, ArraySize(stDisk.szSize),
                    UTF8ToUnicode(jsonHardware[i]["Disk"][nDisk]["Size"].asString()).c_str(), ArraySize(stDisk.szSize) - 1);
				
				stHardware.vecDiskInfo.push_back(stDisk);
			}
		}

		{
			SECDETECT_BIOSINFO_STRUCT stBios;
			_tcsncpy_s(stBios.szMainBoardID,ArraySize(stBios.szMainBoardID),UTF8ToUnicode(jsonHardware[i]["BaseBoardSerial"].asString()).c_str(),ArraySize(stBios.szMainBoardID)-1);
			_tcsncpy_s(stBios.szMainBoardDesc,ArraySize(stBios.szMainBoardDesc),UTF8ToUnicode(jsonHardware[i]["BaseBoardDesc"].asString()).c_str(),ArraySize(stBios.szMainBoardDesc)-1);
			_tcsncpy_s(stBios.szBiosProv,ArraySize(stBios.szBiosProv),UTF8ToUnicode(jsonHardware[i]["BIOSManu"].asString()).c_str(),ArraySize(stBios.szBiosProv)-1);
			_tcsncpy_s(stBios.szBiosVersion,ArraySize(stBios.szBiosVersion),UTF8ToUnicode(jsonHardware[i]["BIOSVersion"].asString()).c_str(),ArraySize(stBios.szBiosVersion)-1);
			stHardware.vecBIOSInfo.push_back(stBios);

		}

		{
			for(int nScreen = 0; nScreen < jsonHardware[i]["Screen"].size(); nScreen++)
			{
                SECDETECT_DISPLAYINFO_STRUCT stScreen;

                // 显示器名称
                _tcsncpy_s(stScreen.szName, _countof(stScreen.szName), 
                        UTF8ToUnicode(jsonHardware[i]["Screen"][nScreen]["ScreenName"].asString()).c_str(), _countof(stScreen.szName) - 1);
                // 显示器硬件ID
                _tcsncpy_s(stScreen.szDeviceID, _countof(stScreen.szDeviceID),
                        UTF8ToUnicode(jsonHardware[i]["Screen"][nScreen]["ScreenDevID"].asString()).c_str(), _countof(stScreen.szDeviceID) - 1);

                stHardware.vecDisplayInfo.push_back(stScreen);
			}
		}

		{
			for(int nNet = 0; nNet < jsonHardware[i]["Net"].size(); nNet++)
			{
				SECDETECT_NETINFO_STRUCT stNet;

				_tcsncpy_s(stNet.szNet, _countof(stNet.szNet),
							UTF8ToUnicode(jsonHardware[i]["Net"][nNet]["NetName"].asString()).c_str(), _countof(stNet.szNet) - 1);

                _tcsncpy_s(stNet.szNetDesc, _countof(stNet.szNetDesc),
                            UTF8ToUnicode(jsonHardware[i]["Net"][nNet]["NetDesc"].asString()).c_str(), _countof(stNet.szNetDesc) - 1);
				
				/*
                _tcsncpy_s(stNet.szPhyAddr,ArraySize(stNet.szPhyAddr),
							UTF8ToUnicode(jsonHardware[i]["Net"][nNet]["PhyAddr"].asString()).c_str(),ArraySize(stNet.szPhyAddr)-1);
			
				for(int nIPAddr = 0; nIPAddr < jsonHardware[i]["Net"][nNet]["IPAddr"].size(); nIPAddr++)
				{
					stNet.vecIPAddr.push_back(UTF8ToUnicode(jsonHardware[i]["Net"][nNet]["IPAddr"][nIPAddr].asString()).c_str());
				}
                */

                stHardware.vecNetInfo.push_back(stNet);
			}
			
		}

		vecHardwareStu.push_back(stHardware);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

//资产扫描->终端账户
std::string CWLJsonParse::SecDetect_UserInfo_GetJson(__in vector<SECDETECT_USERINFO_STRUCT> &vecUserStu)
{
	/*
	{
		"UserList":[ （用户列表，修改名称为终端账户，内容新增）
                    {
                        "Name":"Admin",（账户名称）
                        "FullName":"",（账户全名）
                        "Status":1, （启用状态 1 - 启用； 0 - 禁用）
                        "UserAttr":2, （用户权限2 - 管理员，其他为普通用户）
                        "LastTime":"2012-12-02 14:39:14"（最后登录时间）
						"UserGroup":[{"GroupName":"group1"}]
						"UserType":1,（用户类型， 1 - 本地用户， 2 - 域用户，3 - 本地用户和域用户， 其他值为未知用户）
                        "PwdMaxAge":90,（密码最长期限，34表示密码修改密码后，最长使用时间为90天）
                        "LastModifyPwdTime":"2012-12-01 14:39:14",（最近修改密码时间）
                        "UserExpTime":"2012-12-12 14:39:14",（账户过期时间，永不过期时，显示字段为TIMEQ_FOREVER）
						"UserRisk":0,（暂时账号风险，0-未知，1-空密码，2-未设置密码，3-弱密码，4-强密码 ）
                    },
                    {

                    }
                ]
	}
	*/

	Json::Value root;
	Json::FastWriter writer;
    Json::Value Items = Json::nullValue;
	Json::Value Item;
	std:: string strJson;

	int nCount = (int)vecUserStu.size();
	if(0 == nCount)
	{
		goto END;
	}
	
	for (int i = 0; i < nCount; i++)
	{
		Item["Name"]			  = UnicodeToUTF8(vecUserStu[i].szUserName);
		Item["FullName"]		  = UnicodeToUTF8(vecUserStu[i].szFullName);
		Item["Status"]			  = (int)vecUserStu[i].dwStatus;
		Item["UserAttr"]		  = (int)vecUserStu[i].dwUserAttr;
		Item["LastTime"]		  = convertTime2Str(vecUserStu[i].dwLastLoginTime);
		Item["UserType"]		  = (int)vecUserStu[i].dwUserType;
		Item["PwdMaxAge"]		  = (int)vecUserStu[i].dwPasswordTime;
		Item["LastModifyPwdTime"] = convertTime2Str(vecUserStu[i].dwLastPasswordChange);
		Item["UserExpTime"]       = convertTime2Str(vecUserStu[i].dwAccountOverTime);
		Item["UserRisk"]          = (int)vecUserStu[i].dwUserRisk;
		
		int nGroup                = (int)vecUserStu[i].vecGroupList.size();
		Json::Value groupObj;
		for (int j = 0; j < nGroup; j++)
		{
			groupObj["GroupName"] = UnicodeToUTF8( vecUserStu[i].vecGroupList[j] );
			Item["UserGroup"].append(groupObj);
		}
		Items.append(Item);
		Item.clear();
	}

	root["UserList"] = (Json::Value)(Items);
	strJson = writer.write(root);

	root.clear();

END:
	return strJson;
}

BOOL CWLJsonParse::SecDetect_UserInfo_ParseJson(__in string &strUserjson, __out vector<SECDETECT_USERINFO_STRUCT> &vecUserStu)
{
	BOOL bRet = FALSE;

	Json::Value jsonUsers;
	Json::Reader reader;
	int nCount;

	if(strUserjson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strUserjson, jsonUsers))
	{
		goto _exit_;
	}

    if ( jsonUsers.isNull() || !jsonUsers.isArray())
    {
        goto _exit_;
    }

	nCount = jsonUsers.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_USERINFO_STRUCT stuUser;
		string strTime;
		time_t t;

		_tcsncpy_s(stuUser.szUserName,ArraySize(stuUser.szUserName),UTF8ToUnicode(jsonUsers[i]["Name"].asString()).c_str(),ArraySize(stuUser.szUserName)-1);
		_tcsncpy_s(stuUser.szFullName,ArraySize(stuUser.szFullName),UTF8ToUnicode(jsonUsers[i]["FullName"].asString()).c_str(),ArraySize(stuUser.szFullName)-1);
		stuUser.dwStatus		= jsonUsers[i]["Status"].asInt();
		stuUser.dwUserAttr		= jsonUsers[i]["UserAttr"].asInt();
		stuUser.dwUserType		= jsonUsers[i]["UserType"].asInt();
		stuUser.dwUserRisk	    = jsonUsers[i]["UserRisk"].asInt();
		stuUser.dwPasswordTime  = jsonUsers[i]["PwdMaxAge"].asInt();

		strTime = jsonUsers[i]["LastTime"].asString();
		convertStr2Time(t,strTime);
		stuUser.dwLastLoginTime = (DWORD)t;

		strTime = jsonUsers[i]["LastModifyPwdTime"].asString();
		convertStr2Time(t,strTime);
		stuUser.dwLastPasswordChange = (DWORD)t;

		strTime = jsonUsers[i]["UserExpTime"].asString();
		convertStr2Time(t,strTime);
		stuUser.dwAccountOverTime = (DWORD)t;

		Json::Value tempJsonObj = (Json::Value)jsonUsers[i]["UserGroup"];
		int nGroup = tempJsonObj.size();
		//wwdv2
		if (tempJsonObj.isArray())
		{
			for (int j = 0; j < nGroup; j++)
			{
				WCHAR wtGroupName[128];
				_tcsncpy_s(wtGroupName, ArraySize(wtGroupName),UTF8ToUnicode(tempJsonObj[j]["GroupName"].asString()).c_str(),ArraySize(wtGroupName)-1);
				stuUser.vecGroupList.push_back(wtGroupName);
			}

			vecUserStu.push_back(stuUser);
			stuUser.vecGroupList.clear();
			tempJsonObj.clear();
		}
		
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

//安全检测->运行进程
std::string CWLJsonParse::SecDetect_ProcessInfo_GetJson(__in std::vector<SECDETECT_PROCESSINFO_STRUCT> &vecProcessStu)
{
/*
	{
		"Process":
		[
			{
				"Name":"winlogon.exe",
				"PID":"588",
				"Company":"Microsoft Corporation",
				"Version":"10.0.18362.1",
				"Path":"C:\Windows\System32\winlogon.exe"
				"Description":"XXX", (进程描述)
				"ParentPID":"488",（父进程ID）
				"User":"XXX",（运行用户）
				"CPU":"XX%",（CPU占用率）
				"Memory":"XX%",（内存占用率）
				"Handle":100,（句柄数）
				"Thread":12,（线程数）
				"IOFile":34,（读写文件数）
				"CmdLine":"start XXX",（进程命令行）
				"DLLNumber":14,（进程关联DLL数量）
				"DLL":[
						{
							"Name":"DLL1",(进程关联DLL名称)
							"Manufacturer":"XXX",（DLL的制造商）
							"Version":"XXX",（DLL的版本号）
							"Size":"XXX",（DLL的文件大小）
							"Date":"XXX",（DLL的文件日期）
						}，
						{
						}
					],
			}
		]
	}
*/
	std::string    strJsonProc = "";
    Json::Value    ProcContent = Json::nullValue;
	Json::Value    root;

	Json::FastWriter writer;
	
	int nCount = (int)vecProcessStu.size();
		
	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["PID"]         = (int)vecProcessStu[i].dwPID;
		Temp["Name"]        = UnicodeToUTF8(vecProcessStu[i].szName);
		Temp["Company"]     = UnicodeToUTF8(vecProcessStu[i].szCompany);
		Temp["Version"]     = UnicodeToUTF8(vecProcessStu[i].szVersion);
		Temp["Path"]        = UnicodeToUTF8(vecProcessStu[i].szPath);
		Temp["Description"] = UnicodeToUTF8(vecProcessStu[i].szDescription);
		Temp["ParentPID"]   = (int)vecProcessStu[i].dwPPID;
		Temp["User"]        = UnicodeToUTF8(vecProcessStu[i].szRunUser);
		Temp["CPU"]         = UnicodeToUTF8(vecProcessStu[i].szCPURate);
		Temp["Memory"]      = UnicodeToUTF8(vecProcessStu[i].szMemoryWork);
		Temp["CmdLine"]     = UnicodeToUTF8(vecProcessStu[i].szCommandLine);
		Temp["Handle"]      = (int)vecProcessStu[i].dwHanldeCount;
		Temp["Thread"]      = (int)vecProcessStu[i].dwThreadCount;
		Temp["IOFile"]      = (int)vecProcessStu[i].llIOFileCount;
		Temp["DLLNumber"]   = (int)vecProcessStu[i].dwModuleCount;

		for(int j = 0; j < vecProcessStu[i].vecModuleInfo.size(); j++)
		{
			Json::Value DllObj;

			DllObj["Name"]            = UnicodeToUTF8(vecProcessStu[i].vecModuleInfo[j].szName);
			DllObj["Manufacturer"]    = UnicodeToUTF8(vecProcessStu[i].vecModuleInfo[j].szCompany);
			DllObj["Version"]         = UnicodeToUTF8(vecProcessStu[i].vecModuleInfo[j].szVersion);
			DllObj["Size"]            = UnicodeToUTF8(vecProcessStu[i].vecModuleInfo[j].szFileSize);
			DllObj["Date"]            = UnicodeToUTF8(vecProcessStu[i].vecModuleInfo[j].szUpdateTime);

			Temp["DLL"].append(DllObj);

            DllObj.clear();
		}

		ProcContent.append(Temp);
        Temp.clear();
	}

	root["Process"] = (Json::Value)ProcContent;
	strJsonProc = writer.write(root);
	root.clear();

	return strJsonProc;
}

BOOL CWLJsonParse::SecDetect_ProcessInfo_ParseJson(__in string strProcJson, __out vector<SECDETECT_PROCESSINFO_STRUCT> &vecProcessStu)
{
	BOOL               bRet = FALSE;
	Json::Value   ProcArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecProcessStu.clear();

	if(strProcJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strProcJson, ProcArray))
	{
		goto END;
	}

    if ( ProcArray.isNull() || !ProcArray.isArray())
    {
        goto END;
    }

	nCount = ProcArray.size();
	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PROCESSINFO_STRUCT ProcTemp;  //单独一项
		_tcscpy_s(ProcTemp.szName, sizeof(ProcTemp.szName)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Name"].asString()).c_str());
		_tcscpy_s(ProcTemp.szCompany,sizeof(ProcTemp.szCompany)/sizeof(TCHAR),UTF8ToUnicode(ProcArray[i]["Company"].asString()).c_str());
		_tcscpy_s(ProcTemp.szVersion,sizeof(ProcTemp.szVersion)/sizeof(TCHAR),UTF8ToUnicode(ProcArray[i]["Version"].asString()).c_str());
		_tcscpy_s(ProcTemp.szPath, sizeof(ProcTemp.szPath)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Path"].asString()).c_str());
		_tcscpy_s(ProcTemp.szRunUser, sizeof(ProcTemp.szRunUser)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["User"].asString()).c_str());
		_tcscpy_s(ProcTemp.szCPURate, sizeof(ProcTemp.szCPURate)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["CPU"].asString()).c_str());
		_tcscpy_s(ProcTemp.szMemoryWork, sizeof(ProcTemp.szMemoryWork)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Memory"].asString()).c_str());
		_tcscpy_s(ProcTemp.szCommandLine, sizeof(ProcTemp.szCommandLine)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["CmdLine"].asString()).c_str());
		_tcscpy_s(ProcTemp.szDescription, sizeof(ProcTemp.szDescription)/sizeof(TCHAR),   UTF8ToUnicode(ProcArray[i]["Description"].asString()).c_str());
		ProcTemp.dwPID         = (DWORD)ProcArray[i]["PID"].asInt();
		ProcTemp.dwPPID        = (DWORD)ProcArray[i]["ParentPID"].asInt();
		ProcTemp.dwHanldeCount = (DWORD)ProcArray[i]["Handle"].asInt();
		ProcTemp.dwThreadCount = (DWORD)ProcArray[i]["Thread"].asInt();
		ProcTemp.dwModuleCount = (DWORD)ProcArray[i]["DLLNumber"].asInt();
		ProcTemp.llIOFileCount = (ULONGLONG)ProcArray[i]["IOFile"].asInt();
		
		for(int j = 0; j < ProcArray[i]["DLL"].size(); j++)
		{
			SECDETECT_MODULEINFO_STRUCT stDll;
			_tcscpy_s(stDll.szName, sizeof(stDll.szName)/sizeof(TCHAR), UTF8ToUnicode(ProcArray[i]["DLL"][j]["Name"].asString()).c_str());
			_tcscpy_s(stDll.szCompany, sizeof(stDll.szCompany)/sizeof(TCHAR), UTF8ToUnicode(ProcArray[i]["DLL"][j]["Manufacturer"].asString()).c_str());
			_tcscpy_s(stDll.szVersion, sizeof(stDll.szVersion)/sizeof(TCHAR), UTF8ToUnicode(ProcArray[i]["DLL"][j]["Version"].asString()).c_str());
			_tcscpy_s(stDll.szFileSize, sizeof(stDll.szFileSize)/sizeof(TCHAR), UTF8ToUnicode(ProcArray[i]["DLL"][j]["Size"].asString()).c_str());
			_tcscpy_s(stDll.szUpdateTime, sizeof(stDll.szUpdateTime)/sizeof(TCHAR), UTF8ToUnicode(ProcArray[i]["DLL"][j]["Date"].asString()).c_str());
			ProcTemp.vecModuleInfo.push_back(stDll);
		}

		vecProcessStu.push_back(ProcTemp);	
	}

	bRet = TRUE;
END:
	return bRet;
}

//资产扫描->服务列表
std::string CWLJsonParse::SecDetect_ServicesInfo_GetJson(__in vector<SECDETECT_SERVICESINFO_STRUCT> &vecServStu)
{
	/*
	{
		"Service":[ （服务列表，修改名称为运行服务，内容新增）
					{
						"Name":"AJRouter",（服务名称）
						"Version":"6.2.18362.1",（软件版本）
						"Company":"Microsoft Corporation",（公司）
						"Desp":"AllJoyn Router Service",（描述）
						"Running":0 （服务状态，0 - 已停止； 1 - 正在运行）
						"Path":"XXXXXX",（运行服务执行路径）
						"User":"User",（运行服务启动用户）
						"StartType":0,（服务启动类型，0系统加载启动（驱动程序），1文件系统加载启动（驱动程序），2自动启动，3手动启动，4无法启动）
						"Port":[
								{
									"Number":"123",（端口号）
									"Protocol":1,（协议，0未知，1 TCP，2 UDP）
									"HighRiskPorts":1,（高危端口，0否1是）
								},
								{ 
									"Number":"139",
									"Protocol":1,
									"HighRiskPorts":1,
								}
								],
						"HighRiskServices":1,（高危服务，0不属于高危服务，1属于高危服务）
					},
					{

					}
				]
	}
*/
	Json::Value			root;
	Json::FastWriter	writer;
    Json::Value			Items = Json::nullValue;
	Json::Value			Item;
	std:: string		strJson;

	int nCount = (int)vecServStu.size();
	if(0 == nCount)
	{
		goto END;
	}
	
	for (int i = 0; i < nCount; i++)
	{
        SECDETECT_SERVICESINFO_STRUCT &stServicesInfo = vecServStu[i];

		Item["Name"] = UnicodeToUTF8(stServicesInfo.szName);	
		Item["Running"] = (int)stServicesInfo.dwRunning;
		Item["Version"] = UnicodeToUTF8(stServicesInfo.szVersion);
		Item["Company"] = UnicodeToUTF8(stServicesInfo.szCompany);
		Item["Desp"] = UnicodeToUTF8(stServicesInfo.szDescription);
		Item["Path"] = UnicodeToUTF8(stServicesInfo.szPath);
		Item["User"] = UnicodeToUTF8(stServicesInfo.szRunUser);
		Item["StartType"] = (int)stServicesInfo.dwStartType;
		Item["HighRiskServices"] = (int)stServicesInfo.dwIsRiskServices;
        Item["StartTime"] = UnicodeToUTF8(stServicesInfo.szStartTime);
        Item["Pid"] = (int)stServicesInfo.dwPid;

		for (int j = 0; j < stServicesInfo.vecPortInfo.size(); j++)
		{
			Json::Value			PortObj = Json::nullValue;
			PortObj["Number"]  = (int)stServicesInfo.vecPortInfo[j].usPortNumber;
			PortObj["Protocol"]  = (int)stServicesInfo.vecPortInfo[j].dwProtocol;
			PortObj["HighRiskPorts"]  = (int)stServicesInfo.vecPortInfo[j].dwIsRiskPort;
			Item["Port"].append(PortObj);
			PortObj.clear();
		}
		
		Items.append(Item);
        Item.clear();
	}

	root["Service"] = (Json::Value)(Items);
	strJson = writer.write(root);

	root.clear();

END:
	return strJson;
}


BOOL CWLJsonParse::SecDetect_ServicesInfo_ParseJson(__in string &strServJson, __out vector<SECDETECT_SERVICESINFO_STRUCT> &vecServStu)
{
	BOOL bRet = FALSE;

	Json::Value servs;
	Json::Reader reader;
	int nCount;

	if(strServJson.length() == 0)
	{
		goto _exit_;
	}

	if (!reader.parse(strServJson, servs))
	{
		goto _exit_;
	}

    if ( servs.isNull() || !servs.isArray())
    {
        goto _exit_;
    }

	nCount = servs.size();

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_SERVICESINFO_STRUCT stSevMgr;

		_tcsncpy_s(stSevMgr.szName, ArraySize(stSevMgr.szName), 
            UTF8ToUnicode(servs[i]["Name"].asString()).c_str(), ArraySize(stSevMgr.szName)-1);
		
		if(servs[i]["Version"].isString())
		{
			_tcsncpy_s(stSevMgr.szVersion, ArraySize(stSevMgr.szVersion),
				UTF8ToUnicode(servs[i]["Version"].asString()).c_str(), ArraySize(stSevMgr.szVersion)-1);
		}

		if(servs[i]["Desp"].isString())
		{
			_tcsncpy_s(stSevMgr.szDescription,ArraySize(stSevMgr.szDescription),
				UTF8ToUnicode(servs[i]["Desp"].asString()).c_str(), ArraySize(stSevMgr.szDescription)-1);
		}

        if(servs[i]["Path"].isString())
        {
            _tcsncpy_s(stSevMgr.szPath, ArraySize(stSevMgr.szPath),
                UTF8ToUnicode(servs[i]["Path"].asString()).c_str(), ArraySize(stSevMgr.szPath)-1);
        }

        if(servs[i]["User"].isString())
        {
            _tcsncpy_s(stSevMgr.szRunUser, ArraySize(stSevMgr.szRunUser),
                UTF8ToUnicode(servs[i]["User"].asString()).c_str(), ArraySize(stSevMgr.szRunUser)-1);
        }

		if(servs[i]["Company"].isString())
		{
			_tcsncpy_s(stSevMgr.szCompany, ArraySize(stSevMgr.szCompany),
				UTF8ToUnicode(servs[i]["Company"].asString()).c_str(), ArraySize(stSevMgr.szCompany)-1);
		}

		if(servs[i]["Running"].isInt())
		{
			stSevMgr.dwRunning = (DWORD)servs[i]["Running"].asInt();
		}

		if(servs[i]["StartType"].isInt())
		{
			stSevMgr.dwStartType = (DWORD)servs[i]["StartType"].asInt();
		}

		if(servs[i]["HighRiskServices"].isInt())
		{
			stSevMgr.dwIsRiskServices = (DWORD)servs[i]["HighRiskServices"].asInt();
		}

        if(servs[i]["StartTime"].isString())
        {
            _tcsncpy_s(stSevMgr.szStartTime, ArraySize(stSevMgr.szStartTime),
                UTF8ToUnicode(servs[i]["StartTime"].asString()).c_str(), ArraySize(stSevMgr.szStartTime) - 1);
        }

        if(servs[i]["Pid"].isInt())
        {
            stSevMgr.dwPid = (DWORD)servs[i]["Pid"].asInt();
        }

		for (int j = 0; j < servs[i]["Port"].size(); j++)
		{
			SECDETECT_SERVICESPORT_STRUCT stPort;
			if(servs[i]["Port"][j]["Number"].isInt())
			{
				stPort.usPortNumber = (WORD)servs[i]["Port"][j]["Number"].asInt();
			}

			if(servs[i]["Port"][j]["Protocol"].isInt())
			{
				stPort.dwProtocol = (WORD)servs[i]["Port"][j]["Protocol"].asInt();
			}

			if(servs[i]["Port"][j]["HighRiskPorts"].isInt())
			{
				stPort.dwIsRiskPort = (WORD)servs[i]["Port"][j]["HighRiskPorts"].asInt();
			}
			stSevMgr.vecPortInfo.push_back(stPort);
		}

		vecServStu.push_back(stSevMgr);
	}

	bRet = TRUE;

_exit_:
	return bRet;
}

//资产扫描->端口列表
std::string CWLJsonParse::SecDetect_PortInfo_GetJson(__in std::vector<SECDETECT_PORTINFO_STRUCT> &vecDetectPort)
{
/*
	{
		"PortList":[ （端口列表，修改名称为监听端口，内容新增）
						{
							"PortNumber":139,（端口号）
							"Protocol":1, （端口协议，0 未知；1 TCP；2 UDP）
							"PID":980,（监听进程ID）
							"RunTime":"2021-06-12 12:32:32" （进程启动时间，可能为空）
							"Version":"1.2.3.0.56"（进程对应启动软件版本）
							"Company":"XXXXXX公司"（进程对应启动软件公司）
							"Desp":"XXXXXXXXXXXXXXXXXXXXXXXXX" （进程对应启动软件描述）
							"State":""  （仅限TCP才有该状态描述，暂时定为String类型。未使用，后续如果有使用可规定为INT或String）
							"ProcPath":"C:\Windows\System32\XXX.exe"（进程对应软                            
							件启动路径）
							"IP":"192.168.X.X"（端口绑定IP）
							"External":1（是否对外，0不对外，1对外）
							"HighRiskPorts":1,（高危端口，0否 1是）
                            "Total":"XXXX"(超过限额数量，则需要填写具体值；否则，一直为0)
						},
						{

						}
		],
	}
*/
	std::string    strJsonPort = "";
    Json::Value    PortContent = Json::nullValue;
	Json::Value    root;
	Json::FastWriter writer;
	
	int nCount = (int)vecDetectPort.size();

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["PID"]             = (int)vecDetectPort[i].dwPID;
		Temp["Company"]         = UnicodeToUTF8(vecDetectPort[i].szCompany);
		Temp["Version"]         = UnicodeToUTF8(vecDetectPort[i].szVersion);
		Temp["RunTime"]         = UnicodeToUTF8(vecDetectPort[i].szRunTime);
		Temp["ProcPath"]        = UnicodeToUTF8(vecDetectPort[i].szProPath);
		Temp["Protocol"]        = (int)vecDetectPort[i].dwProtocol;
		Temp["PortNumber"]      = (int)vecDetectPort[i].usPortNumber;
		Temp["Desp"]            = UnicodeToUTF8(vecDetectPort[i].szDesp);
		Temp["IP"]              = UnicodeToUTF8(vecDetectPort[i].szBindIP);
		Temp["External"]        = (int)vecDetectPort[i].dwIsOutWard;
		Temp["HighRiskPorts"]   = (int)vecDetectPort[i].dwIsRiskPort;
        Temp["Total"]           = (int)vecDetectPort[i].dwTotalPorts;

		PortContent.append(Temp);
	}

	root["PortList"] = (Json::Value)PortContent;
	strJsonPort = writer.write(root);

	root.clear();

	return strJsonPort;
}

BOOL CWLJsonParse::SecDetect_PortInfo_ParseJson(__in string strPortJson, __out vector<SECDETECT_PORTINFO_STRUCT> &vecPortStu)
{
	BOOL               bRet = FALSE;
	Json::Value   PortArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecPortStu.clear();


	if(strPortJson.length() == 0)
	{
		goto END;
	}

    //CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson Json %S"), strPortJson.c_str()); // {"PortList":null}

	if (!reader.parse(strPortJson, PortArray))
	{
		goto END;
	}

    if ( PortArray.isNull() || !PortArray.isArray())
    {
        goto END;
    }
    
	nCount = PortArray.size();
	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson nCount %d"),nCount);

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_PORTINFO_STRUCT stPort;  //单独一项

        stPort.dwPID        = (DWORD)PortArray[i]["PID"].asInt();

		stPort.dwProtocol   = (DWORD)PortArray[i]["Protocol"].asInt();

		stPort.usPortNumber = (WORD)PortArray[i]["PortNumber"].asInt();

		stPort.dwIsOutWard  = (DWORD)PortArray[i]["External"].asInt();

		stPort.dwIsRiskPort = (DWORD)PortArray[i]["HighRiskPorts"].asInt();

        stPort.dwTotalPorts = (DWORD)PortArray[i]["Total"].asInt();

		_tcscpy_s(stPort.szCompany, sizeof(stPort.szCompany)/sizeof(stPort.szCompany[0]), UTF8ToUnicode(PortArray[i]["Company"].asString()).c_str());

		_tcscpy_s(stPort.szVersion, sizeof(stPort.szVersion)/sizeof(stPort.szVersion[0]), UTF8ToUnicode(PortArray[i]["Version"].asString()).c_str());

		_tcscpy_s(stPort.szDesp,    sizeof(stPort.szDesp)/sizeof(stPort.szDesp[0]),    UTF8ToUnicode(PortArray[i]["Desp"].asString()).c_str());

		_tcscpy_s(stPort.szProPath, sizeof(stPort.szProPath)/sizeof(stPort.szProPath[0]), UTF8ToUnicode(PortArray[i]["ProcPath"].asString()).c_str());

		_tcscpy_s(stPort.szRunTime, sizeof(stPort.szRunTime)/sizeof(stPort.szRunTime[0]), UTF8ToUnicode(PortArray[i]["RunTime"].asString()).c_str());

		_tcscpy_s(stPort.szBindIP, sizeof(stPort.szBindIP)/sizeof(stPort.szBindIP[0]), UTF8ToUnicode(PortArray[i]["IP"].asString()).c_str());


		vecPortStu.push_back(stPort);	
	}

	bRet = TRUE;

END:
	return bRet;
}

//资产扫描->网络链接
std::string CWLJsonParse::SecDetect_NetConnect_GetJson(__in std::vector<SECDETECT_NETCONNECT_STRUCT> &vecNetConnect)
{
	/*
	{
		"NetConnect":[ （网络连接，新增项）
						{
						"LocalIP":"192.168.X.X",（本地IP）
						"LocalPort":8442,（本地端口）
						"RemoteIP":"192.168.X.X",（远程IP）
						"RemotePort":54420,（远程端口）
						"Protocol":1,（协议，0未知，1 TCP，2 UDP）
						"PID":122,(进程ID)
						"ProName":"XXX",(进程名)
						},
						{

						}
		],
	}
*/
	std::string    strJsonPort = "";
    Json::Value    NetConnectContent = Json::nullValue;
	Json::Value    root;
	Json::FastWriter writer;
	
	int nCount = (int)vecNetConnect.size();

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            Temp;

		Temp["LocalIP"]         = UnicodeToUTF8(vecNetConnect[i].szLocalIP);
		Temp["LocalPort"]       = (int)vecNetConnect[i].usLocalPort;
		Temp["RemoteIP"]        = UnicodeToUTF8(vecNetConnect[i].szRemoteIP);
		Temp["RemotePort"]      = (int)vecNetConnect[i].usRemotePort;
		Temp["Protocol"]        = (int)vecNetConnect[i].dwProtocol;
		Temp["PID"]             = (int)vecNetConnect[i].dwPID;
		Temp["ProName"]         = UnicodeToUTF8(vecNetConnect[i].szProPath);
        Temp["CntDict"]         = (int)vecNetConnect[i].dwCntDict;
		NetConnectContent.append(Temp);
	}

	root["NetConnect"] = (Json::Value)NetConnectContent;
	strJsonPort = writer.write(root);

	root.clear();

	return strJsonPort;
}
BOOL CWLJsonParse::SecDetect_NetConnect_ParseJson(__in string strNetConnectJson, __out vector<SECDETECT_NETCONNECT_STRUCT> &vecNetConnect)
{
	BOOL               bRet = FALSE;
	Json::Value   NetConnectArray;//解析Json得到的数组
	Json::Reader     reader;
	int              nCount;
	Json::FastWriter writer;

	vecNetConnect.clear();


	if(strNetConnectJson.length() == 0)
	{
		goto END;
	}

	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson Json %S"), strPortJson.c_str()); // {"PortList":null}

	if (!reader.parse(strNetConnectJson, NetConnectArray))
	{
		goto END;
	}

	if ( NetConnectArray.isNull() || !NetConnectArray.isArray())
	{
		goto END;
	}

	nCount = NetConnectArray.size();
	//CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson nCount %d"),nCount);

	for (int i = 0; i < nCount; i++)
	{
		SECDETECT_NETCONNECT_STRUCT stNetConnect;  //单独一项

		stNetConnect.dwPID        = (DWORD)NetConnectArray[i]["PID"].asInt();

		stNetConnect.dwProtocol   = (DWORD)NetConnectArray[i]["Protocol"].asInt();

		stNetConnect.usLocalPort  = (WORD)NetConnectArray[i]["LocalPort"].asInt();
		
		stNetConnect.usRemotePort = (WORD)NetConnectArray[i]["RemotePort"].asInt();
		
		stNetConnect.dwCntDict    = (DWORD)NetConnectArray[i]["CntDict"].asInt();

		_tcscpy_s(stNetConnect.szLocalIP, sizeof(stNetConnect.szLocalIP)/sizeof(stNetConnect.szLocalIP[0]), UTF8ToUnicode(NetConnectArray[i]["LocalIP"].asString()).c_str());

		_tcscpy_s(stNetConnect.szRemoteIP, sizeof(stNetConnect.szRemoteIP)/sizeof(stNetConnect.szRemoteIP[0]), UTF8ToUnicode(NetConnectArray[i]["RemoteIP"].asString()).c_str());

		_tcscpy_s(stNetConnect.szProPath,    sizeof(stNetConnect.szProPath)/sizeof(stNetConnect.szProPath[0]),    UTF8ToUnicode(NetConnectArray[i]["ProName"].asString()).c_str());

		vecNetConnect.push_back(stNetConnect);	
	}

	bRet = TRUE;

END:
	return bRet;
}
//威胁诱捕
std::string CWLJsonParse::ThreatFake_GetJson(__in PDWORD pdwFakeALL, 
                                       __in PDWORD pdwFakeRDP, 
                                       __in PDWORD pdwFakeFTP, 
                                       __in PDWORD pdwFakeMYSQL, 
                                       __in PDWORD pdwFakeHTTP,
                                       __in THREATFAKE_SERVER_INFOR &stSerInfor,
									   __in PWCHAR szComputerID)
{
    /*
    DWORD dwFakeALL;                    //开启全部：0=关闭，1=全部开启，2=部分开启
    DWORD dwFakeRDP;                    //RDP：0=关闭，1=开启，2=占用
    DWORD dwFakeFTP;                    //FTP：0=关闭，1=开启，2=占用
    DWORD dwFakeMYSQL;                  //MYSQL：0=关闭，1=开启，2=占用
    DWORD dwFakeHTTP;                   //HTTP：0=关闭，1=开启，2=占用*/
    std::string    strJsonPort = "";
    Json::Value    ThreatFakeContent = Json::nullValue;
    Json::Value    root;
    Json::FastWriter writer;

	Json::Value            Content;

	if (pdwFakeALL)
	{
		Content["Enable"] = (int)*pdwFakeALL;
	}
	if (pdwFakeRDP)
	{
		Content["RDP"] = (int)*pdwFakeRDP;
        // 客户端获取的
        if (*pdwFakeRDP == 1)
        {
            Content["llRdpStarTime"]    = (long long)stSerInfor.llRdpStarTime;
            Content["llRdpRunTime"]     = (long long)stSerInfor.llRdpRunTime;
            Content["dwRdpNum"]         = (int)stSerInfor.dwRdpNum;
        }
	}
	if (pdwFakeFTP)
	{
		Content["FTP"] = (int)*pdwFakeFTP;

        if (*pdwFakeFTP == 1)
        {
            Content["llFtpStarTime"]    = (long long)stSerInfor.llFtpStarTime;
            Content["llFtpRunTime"]     = (long long)stSerInfor.llFtpRunTime;
            Content["dwFtpNum"]         = (int)stSerInfor.dwFtpNum;
        }
	}
	if (pdwFakeMYSQL)
	{
		Content["MYSQL"] = (int)*pdwFakeMYSQL;

        if (*pdwFakeMYSQL == 1)
        {
            Content["llMysqlStarTime"]  = (long long)stSerInfor.llMysqlStarTime;
            Content["llMysqlRunTime"]   = (long long)stSerInfor.llMysqlRunTime;
            Content["dwMysqlNum"]       = (int)stSerInfor.dwMysqlNum;
        }
	}
	if (pdwFakeHTTP)
	{
		Content["HTTP"] = (int)*pdwFakeHTTP;

        if (*pdwFakeHTTP == 1)
        {
            Content["llWebStarTime"]    = (long long)stSerInfor.llWebStarTime;
            Content["llWebRunTime"]     = (long long)stSerInfor.llWebRunTime;
            Content["dwWebNum"]         = (int)stSerInfor.dwWebNum;
        }
	}



    root["CMDContent"] = (Json::Value)Content;
	if (NULL != szComputerID)
	{
		root["ComputerID"] = UnicodeToUTF8(szComputerID);
	}
	root["CMDID"] = 322;
	root["CMDTYPE"] = 150;
    strJsonPort = writer.write(root);

    root.clear();

    return strJsonPort;

}
BOOL CWLJsonParse::ThreatFake_ParseJson(__in string strThreatFakeJson, 
                                        __out PDWORD pdwEnableAll, 
                                        __out PDWORD pdwEnableRDP, 
                                        __out PDWORD pdwEnableFTP, 
                                        __out PDWORD pdwEnableSQL, 
                                        __out PDWORD pdwEnableWEB,
                                        __out THREATFAKE_SERVER_INFOR &stSerInfor)
{
    BOOL             bRet = FALSE;
    Json::Value		 root;
	Json::Value		 Content;
    Json::Reader     reader;
    //int              nCount;
    Json::FastWriter writer;

    if(strThreatFakeJson.length() == 0)
    {
        goto END;
    }
    if (!reader.parse(strThreatFakeJson, root))
    {
        goto END;
    }

	Content = (Json::Value)root["CMDContent"];
	if (Content.isNull())
	{
		goto END;
	}

	if (Content.isMember("Enable") && NULL != pdwEnableAll)
	{
		*pdwEnableAll = Content["Enable"].asInt();
	}
	if (Content.isMember("RDP") && NULL != pdwEnableRDP)
	{
		*pdwEnableRDP = Content["RDP"].asInt();
	}
	if (Content.isMember("FTP") && NULL != pdwEnableFTP)
	{
		*pdwEnableFTP = Content["FTP"].asInt();
	}
	if (Content.isMember("MYSQL") && NULL != pdwEnableSQL)
	{
		*pdwEnableSQL = Content["MYSQL"].asInt();
	}
	if (Content.isMember("HTTP") && NULL != pdwEnableWEB)
	{
		*pdwEnableWEB = Content["HTTP"].asInt();
	}
    if (Content.isMember("llRdpStarTime"))
    {
        stSerInfor.llRdpStarTime = Content["llRdpStarTime"].asInt64();
    }
    if (Content.isMember("llRdpRunTime"))
    {
        stSerInfor.llRdpRunTime = Content["llRdpRunTime"].asInt64();
    }
    if (Content.isMember("dwRdpNum"))
    {
        stSerInfor.dwRdpNum = Content["dwRdpNum"].asInt();
    }
    if (Content.isMember("llFtpStarTime"))
    {
        stSerInfor.llFtpStarTime = Content["llFtpStarTime"].asInt64();
    }
    if (Content.isMember("llFtpRunTime"))
    {
        stSerInfor.llFtpRunTime = Content["llFtpRunTime"].asInt64();
    }
    if (Content.isMember("dwFtpNum"))
    {
        stSerInfor.dwFtpNum = Content["dwFtpNum"].asInt();
    }
    if (Content.isMember("llMysqlStarTime"))
    {
        stSerInfor.llMysqlStarTime = Content["llMysqlStarTime"].asInt64();
    }
    if (Content.isMember("llMysqlRunTime"))
    {
        stSerInfor.llMysqlRunTime = Content["llMysqlRunTime"].asInt64();
    }
    if (Content.isMember("dwMysqlNum"))
    {
        stSerInfor.dwMysqlNum = Content["dwMysqlNum"].asInt();
    }
    if (Content.isMember("llWebStarTime"))
    {
        stSerInfor.llWebStarTime = Content["llWebStarTime"].asInt64();
    }
    if (Content.isMember("llWebRunTime"))
    {
        stSerInfor.llWebRunTime = Content["llWebRunTime"].asInt64();
    }
    if (Content.isMember("dwWebNum"))
    {
        stSerInfor.dwWebNum = Content["dwWebNum"].asInt();
    }



    bRet = TRUE;

END:
    return bRet;
}

//威胁检测->威胁诱骗服务信息
std::string CWLJsonParse::ThreatFake_ServicesInfo_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in std::vector<THREATFAKE_IPSEC_LOG_STRUCT*> &vecThreatFakeInfe)
{
    	/*
        TCHAR  szFakeIP[MAX_SIZE_128];      //远程IP
        WORD   usFakePort;				    //远程端口号
        DWORD  dwProtocol;					//协议：0=未知, 1=FTP，2=RDP, 3=MySQL, 4=WEB
        TCHAR szStartTime[MAX_SIZE_128];    // 启动时间
        DWORD  dwNumber;	                //链接次数
*/

	std::string    strJsonPort = "";
    Json::Value    cmdContent;
	Json::Value    root;
	Json::FastWriter writer;
    Json::Value    vulLogJson;
    
	
	int nCount = (int)vecThreatFakeInfe.size();
    if( nCount == 0)
        return strJsonPort;

	// 客户端获取的
	for (int i=0; i< nCount; i++)
	{
		Json::Value            logItem;
        std::wstring wsTemp = convertTimeTToStr((DWORD)vecThreatFakeInfe[i]->llTime);
		logItem["FakeIP"]         = UnicodeToUTF8(vecThreatFakeInfe[i]->wszSrcIp);
		logItem["FakePort"]       = (int)vecThreatFakeInfe[i]->usSrcPort;
		logItem["Protocol"]       = UnicodeToUTF8(vecThreatFakeInfe[i]->dwPortType);
		logItem["StartTime"]      = UnicodeToUTF8(wsTemp);
		logItem["Type"]           = (int)vecThreatFakeInfe[i]->dwType;
		cmdContent.append(logItem);
	}


    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = cmdContent;

    root.append(vulLogJson);
    strJsonPort = writer.write(root);

    root.clear();

	return strJsonPort;

}
std::string CWLJsonParse::ThreatFake_ServicesInfo_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData*>& vecThreatFakeInfe)
{
    std::string strJsonPacket = "";

    int nCount = (int)vecThreatFakeInfe.size();
    if( nCount == 0)
        return strJsonPacket;

    Json::FastWriter writer;
    Json::Value root;
    Json::Value vulLogJson;
    Json::Value cmdContent;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value logItem;
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecThreatFakeInfe[i]->GetData();
        THREATFAKE_IPSEC_LOG_STRUCT *pThreatFakeLog = (THREATFAKE_IPSEC_LOG_STRUCT*)pipclogcomm->data;

        std::wstring wsTemp = convertTimeTToStr((DWORD)pThreatFakeLog->llTime);
        logItem["FakeIP"]         = UnicodeToUTF8(pThreatFakeLog->wszSrcIp);
        logItem["FakePort"]       = (int)pThreatFakeLog->usSrcPort;
        logItem["Protocol"]       = UnicodeToUTF8(pThreatFakeLog->dwPortType);
        logItem["StartTime"]      = UnicodeToUTF8(wsTemp);
        logItem["Type"]           = (int)pThreatFakeLog->dwType;

        cmdContent.append(logItem);
    }

    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = cmdContent;

    root.append(vulLogJson);
    strJsonPacket = writer.write(root);

    root.clear();

    return strJsonPacket;
}

/*
BOOL CWLJsonParse::ThreatFake_ServicesInfo_ParseJson(__in string strServicesInfeJson, __out vector<THREATFAKE_IPSEC_LOG_STRUCT> &vecFakeServices)
{

    BOOL               bRet = FALSE;
    Json::Value   ServicesInfeArray;//解析Json得到的数组
    Json::Reader     reader;
    int              nCount;
    Json::FastWriter writer;

    ServicesInfeArray.clear();


    if(strServicesInfeJson.length() == 0)
    {
        goto END;
    }

    //CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson Json %S"), strPortJson.c_str()); // {"PortList":null}

    if (!reader.parse(strServicesInfeJson, ServicesInfeArray))
    {
        goto END;
    }

    if ( ServicesInfeArray.isNull() || !ServicesInfeArray.isArray())
    {
        goto END;
    }

    nCount = ServicesInfeArray.size();
    //CWLLogger::getLogger(LOG_UIWLClient)->writeInfo(_T("SecDetect_Port_ParseJson nCount %d"),nCount);

    for (int i = 0; i < nCount; i++)
    {
        THREATFAKE_IPSEC_LOG_STRUCT stInfeConnect;  //单独一项

        stInfeConnect.dwNumber        = (DWORD)ServicesInfeArray[i]["Number"].asInt();

        stInfeConnect.dwProtocol   = (DWORD)ServicesInfeArray[i]["Protocol"].asInt();

        stInfeConnect.usFakePort = (WORD)ServicesInfeArray[i]["FakePort"].asInt();

        _tcscpy_s(stInfeConnect.szFakeIP, sizeof(stInfeConnect.szFakeIP)/sizeof(stInfeConnect.szFakeIP[0]), UTF8ToUnicode(ServicesInfeArray[i]["FakeIP"].asString()).c_str());

        _tcscpy_s(stInfeConnect.szStartTime,    sizeof(stInfeConnect.szStartTime)/sizeof(stInfeConnect.szStartTime[0]),    UTF8ToUnicode(ServicesInfeArray[i]["StartTime"].asString()).c_str());

        vecFakeServices.push_back(stInfeConnect);	
    }

    bRet = TRUE;

END:
    return bRet;
}*/


//威胁检测-数据日志
std::string CWLJsonParse::ThreatEventData_GetJson(__in BOOL bOpen, __in PWCHAR szComputerID)
{
	std::string strJson;
    Json::Value    root;
    Json::FastWriter writer;

	Json::Value            Content;

	Content["Enable"] = (int)bOpen;
	
    root["CMDContent"] = (Json::Value)Content;
	if (NULL != szComputerID)
	{
		root["ComputerID"] = UnicodeToUTF8(szComputerID);
	}
	root["CMDID"] = 320;
	root["CMDTYPE"] = 150;
    strJson = writer.write(root);

    root.clear();

    return strJson;

}
BOOL CWLJsonParse::ThreatEventData_ParseJson(__in string strEventDataJson, __out PBOOL pbOpen)
{
    BOOL               bRet = FALSE;
    Json::Value		 root;
	Json::Value		 Content;
    Json::Reader     reader;
    //int              nCount;
    Json::FastWriter writer;

    if(strEventDataJson.length() == 0 || NULL == pbOpen)
    {
        goto END;
    }

    if (!reader.parse(strEventDataJson, root))
    {
        goto END;
    }

	Content = (Json::Value)root["CMDContent"];
	if (Content.isNull())
	{
		goto END;
	}

	*pbOpen = Content["Enable"].asInt();
   
    bRet = TRUE;

END:
    return bRet;
}


//威胁检测-操作系统日志
std::string CWLJsonParse::ThreatSystemData_GetJson(__in PBOOL pbOpen, __in std::wstring strEventID, __in PBOOL pbResult, __in PWCHAR szComputerID)
{
	std::string strJson;
	Json::Value    root;
	Json::FastWriter writer;
    std::wstring wstrTab = EVENT_ID_TAB_BEYOND; //L"1000000" 添加标记总开关不修改ID号

	Json::Value            Content;

	if (NULL != pbOpen)
	{
		Content["Enable"] = (int)*pbOpen;
	}

	if (NULL != pbResult)
	{
		Content["Result"] = (int)*pbResult;
	}

    if (strEventID.compare(wstrTab))
    {
        Content["EventID"] = UnicodeToUTF8(strEventID);
    }

	root["CMDContent"] = (Json::Value)Content;
	if (NULL != szComputerID)
	{
		root["ComputerID"] = UnicodeToUTF8(szComputerID);
	}
	root["CMDID"] = 321;
	root["CMDTYPE"] = 150;
	strJson = writer.write(root);

	root.clear();

	return strJson;

}
BOOL CWLJsonParse::ThreatSystemData_ParseJson(__in string strEventDataJson, __out PBOOL pbOpen, __out std::wstring &strEventID, __out PBOOL pFromUSM)
{
	BOOL               bRet = FALSE;
	Json::Value		 root;
	Json::Value		 Content;
	Json::Reader     reader;
	//int              nCount;
	Json::FastWriter writer;
	std::string sEventID;

	if(strEventDataJson.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(strEventDataJson, root))
	{
		goto END;
	}

	Content = (Json::Value)root["CMDContent"];
	if (Content.isNull())
	{
		goto END;
	}

	if (NULL != pbOpen && Content.isMember("Enable"))
	{
		*pbOpen = Content["Enable"].asInt();
	}

	if (NULL != pFromUSM && Content.isMember("FromUSM"))
	{
		*pFromUSM = Content["FromUSM"].asInt();
	}

	if (Content.isMember("EventID"))
	{
		sEventID = Content["EventID"].asString();
		strEventID = UTF8ToUnicode(sEventID);
	}

	bRet = TRUE;

END:
	return bRet;
}

std::string CWLJsonParse::VirusLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<VP_ScanVirusLog*>& vecVirusScanLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecVirusScanLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::FastWriter writer;
    Json::Value root;
    Json::Value vulLogJson;
    Json::Value virusLog;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value logItem;
        logItem["Time"] = (Json::UInt64)vecVirusScanLog[i]->llTime;
        logItem["VirusPath"] = UnicodeToUTF8(vecVirusScanLog[i]->wszVirusPath);
        logItem["VirusName"] = UnicodeToUTF8(vecVirusScanLog[i]->wszVirusName);
        logItem["VirusType"] = (int)vecVirusScanLog[i]->dwVirusType;
        logItem["Score"] = (int)vecVirusScanLog[i]->dwScore;
        logItem["Source"] = (int)vecVirusScanLog[i]->dwFrom; // V3R8C01增加来源，取值参考include/VirusProtectCommonStruct:EM_VP_LOG_FROM
        logItem["RiskLevel"] = (int)vecVirusScanLog[i]->dwLevel;
        logItem["Result"] = (long)vecVirusScanLog[i]->dwResult;
        virusLog.append(logItem);
    }

    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = virusLog;

    root.append(vulLogJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::VirusLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData*>& vecVirusScanLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecVirusScanLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::FastWriter writer;
    Json::Value root;
    Json::Value vulLogJson;
    Json::Value virusLog;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value logItem;
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecVirusScanLog[i]->GetData();
        VP_ScanVirusLog *pVirusLog = (VP_ScanVirusLog*)pipclogcomm->data;

        logItem["Time"] = (Json::UInt64)pVirusLog->llTime;
        logItem["VirusPath"] = UnicodeToUTF8(pVirusLog->wszVirusPath);
        logItem["VirusName"] = UnicodeToUTF8(pVirusLog->wszVirusName);
        logItem["VirusType"] = (int)(pVirusLog->dwVirusType);
        logItem["Score"] = (int)(pVirusLog->dwScore);
        logItem["Source"] = (int)(pVirusLog->dwFrom); // V3R8C01增加来源，取值参考include/VirusProtectCommonStruct:EM_VP_LOG_FROM
        logItem["RiskLevel"] = (int)pVirusLog->dwLevel;
        logItem["Result"] = (long)pVirusLog->dwResult;
        virusLog.append(logItem);
    }

    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = virusLog;

    root.append(vulLogJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}


std::string CWLJsonParse::PresetWhiteList_Import_GetJson(const std::wstring &strPath)
{
    std::string strJson;
    Json::Value root;
    Json::FastWriter writer;

    root["ImportDir"] = CStrUtil::UnicodeToUTF8(strPath);
    strJson = writer.write(root);

    return strJson;
}

BOOL CWLJsonParse::PresetWhiteList_Import_GetValue(__in const std::string& strJson,
                                                                __out std::wstring &strPath)
{
    Json::Value root;
    Json::Reader parser;
    bool bRet = false;

    bRet = parser.parse(strJson, root);
    if(!bRet)
    {
        return FALSE;
    }

    if (root.isMember("ImportDir") &&
        root["ImportDir"].isString())
    {
        strPath = CStrUtil::UTF8ToUnicode(root["ImportDir"].asString());
    }

    return TRUE;
}

std::string CWLJsonParse::FetchPresetWhiteListFromUSM__GetJson(__in int nOsType, __in const std::wstring &strOsVersion, __in const std::wstring &strArch,
                                                                 __in const CMDTYPE nCmdType, __in const WORD nCmdID,
                                                                 __in const tstring &tstrComputerID)
{
    std::string strJson;
    Json::Value root;
    Json::Value realItem;
    Json::Value cmdContent;
    Json::FastWriter writer;
    
    realItem[PLY_FW_STRKEY_CMDTYPE] = nCmdType;
    realItem[PLY_FW_STRKEY_CMDID] = nCmdID;
    realItem[PLY_FW_STRKEY_COMPUTER_ID] = CStrUtil::UnicodeToUTF8(tstrComputerID);

    cmdContent[FETCH_PST_KEY_OSTYPE] = nOsType;
    cmdContent[FETCH_PST_KEY_OSVERSION] = CStrUtil::UnicodeToUTF8(strOsVersion);
    cmdContent[FETCH_PST_KEY_OSARCH] = CStrUtil::UnicodeToUTF8(strArch);
    realItem[PLY_FW_STRKEY_CMDCONTENT] = cmdContent;

    root.append(realItem);
    strJson = writer.write(root);

    return strJson;
}

BOOL CWLJsonParse::FetchPresetWhiteListFromUSM__GetValue(__in const std::string& strJson,
                                                           __out std::wstring &wstrDownloadUrl,
                                                           __out std::string &strMsg)
{
    if (!strJson.c_str())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Reader parser;

    if (!parser.parse(strJson, root))
    {
        return FALSE;
    }

    if (root.isArray() && root.size() > 0)
    {
        root = root[0];
    }

    if (!root.isMember(PLY_FW_STRKEY_CMDCONTENT))
    {
        return FALSE;
    } 

    if (!root[PLY_FW_STRKEY_CMDCONTENT].isObject())
    {
        return FALSE;
    }

    const Json::Value &cmdContent = root[PLY_FW_STRKEY_CMDCONTENT];
    if (cmdContent.isMember(FETCH_PST_KEY_URL) &&
        cmdContent[FETCH_PST_KEY_URL].isString())
    {
        wstrDownloadUrl = CStrUtil::UTF8ToUnicode(cmdContent[FETCH_PST_KEY_URL].asString());
    }
    else
    {
        wstrDownloadUrl.clear();
        wstrDownloadUrl.swap(std::wstring());
    }

    if (cmdContent.isMember(FETCH_PST_KEY_MSG) &&
        cmdContent[FETCH_PST_KEY_MSG].isString())
    {
        strMsg = cmdContent[FETCH_PST_KEY_MSG].asString();
    }

    return TRUE;
}

BOOL CWLJsonParse::Upgrade_NewVersion_GetJson(__in tstring ComputerID, __in tstring VirusVersion, __in tstring FeatureVersion, __out string &strJson)
{
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(ComputerID);
	Item["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	Item["CMDID"] = (int)PLY_GET_UPGRADE_VERSION;     /*350*/

//	CMDContentItem["ClientType"] = (DWORD)theApp.m_dwDlgMainType;
	CMDContentItem["ClientVersion"] = UnicodeToUTF8(WL_VSERSION);
	CMDContentItem["VirusLibVersion"] = UnicodeToUTF8(VirusVersion);
    CMDContentItem["FeatureLibVersion"] = UnicodeToUTF8(FeatureVersion);

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	root.append(Item);

	strJson = jsWriter.write(root);

	return TRUE;
}

BOOL CWLJsonParse::Upgrade_NewVersion_ParseJson(__in string strJson, __out New_Package_Version_ST &stVersionInfo)
{
	BOOL bResult = FALSE;
	std::string strValue;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	strValue = strJson;

	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray() ||  root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent") << _T(",");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	if (CMDContent.isMember("ClientName") && !CMDContent["ClientName"].isNull())
	{
		std::wstring wsClientName = UTF8ToUnicode(CMDContent["ClientName"].asString());
		_tcsncpy_s(stVersionInfo.wcClientName, _countof(stVersionInfo.wcClientName), wsClientName.c_str(), _countof(stVersionInfo.wcClientName) - 1);
	}

	if (CMDContent.isMember("ClientMd5") && !CMDContent["ClientMd5"].isNull())
	{
		std::wstring wsClientMd5 = UTF8ToUnicode(CMDContent["ClientMd5"].asString());
		_tcsncpy_s(stVersionInfo.wcClientMd5, _countof(stVersionInfo.wcClientMd5), wsClientMd5.c_str(), _countof(stVersionInfo.wcClientMd5) - 1);
	}

	if (CMDContent.isMember("ClientVersion") && !CMDContent["ClientVersion"].isNull())
	{
		std::wstring wsClientVersion = UTF8ToUnicode(CMDContent["ClientVersion"].asString());
		_tcsncpy_s(stVersionInfo.wcClientVersion, _countof(stVersionInfo.wcClientVersion), wsClientVersion.c_str(), _countof(stVersionInfo.wcClientVersion) - 1);
	}

	if (CMDContent.isMember("ClientUpgradeURL") && !CMDContent["ClientUpgradeURL"].isNull())
	{
		std::string strClientUpgradeURL = CMDContent["ClientUpgradeURL"].asString();
		strncpy_s(stVersionInfo.cClientUpgradeURL, _countof(stVersionInfo.cClientUpgradeURL), strClientUpgradeURL.c_str(), _countof(stVersionInfo.cClientUpgradeURL) - 1);
	}

	if (CMDContent.isMember("ClientSize") && !CMDContent["ClientSize"].isNull())
	{
		stVersionInfo.dwClientSize = CMDContent["ClientSize"].asInt();
	}

	if (CMDContent.isMember("ClientUploadTime") && !CMDContent["ClientUploadTime"].isNull())
	{
		std::wstring wsClientUploadTime = UTF8ToUnicode(CMDContent["ClientUploadTime"].asString());
		_tcsncpy_s(stVersionInfo.wcClientUploadTime, _countof(stVersionInfo.wcClientUploadTime), wsClientUploadTime.c_str(), _countof(stVersionInfo.wcClientUploadTime) - 1);
	}

	if (CMDContent.isMember("VirusLibMd5") && !CMDContent["VirusLibMd5"].isNull())
	{
		std::wstring wsVirusLibMd5 = UTF8ToUnicode(CMDContent["VirusLibMd5"].asString());
		_tcsncpy_s(stVersionInfo.wcVirusLibMd5, _countof(stVersionInfo.wcVirusLibMd5), wsVirusLibMd5.c_str(), _countof(stVersionInfo.wcVirusLibMd5) - 1);
	}

	if (CMDContent.isMember("VirusLibVersion") && !CMDContent["VirusLibVersion"].isNull())
	{
		std::wstring wsVirusLibVersion = UTF8ToUnicode(CMDContent["VirusLibVersion"].asString());
		_tcsncpy_s(stVersionInfo.wcVirusLibVersion, _countof(stVersionInfo.wcVirusLibVersion), wsVirusLibVersion.c_str(), _countof(stVersionInfo.wcVirusLibVersion) - 1);
	}

	if (CMDContent.isMember("VirusLibUpgradeURL") && !CMDContent["VirusLibUpgradeURL"].isNull())
	{
		std::string strVirusLibUpgradeURL = CMDContent["VirusLibUpgradeURL"].asString();
		strncpy_s(stVersionInfo.cVirusLibUpgradeURL, _countof(stVersionInfo.cVirusLibUpgradeURL), strVirusLibUpgradeURL.c_str(), _countof(stVersionInfo.cVirusLibUpgradeURL) - 1);
	}

	if (CMDContent.isMember("VirusLibSize") && !CMDContent["VirusLibSize"].isNull())
	{
		stVersionInfo.dwVirusLibSize = CMDContent["VirusLibSize"].asInt();
	}

	if (CMDContent.isMember("VirusLibName") && !CMDContent["VirusLibName"].isNull())
	{
		std::wstring wsVirusLibName = UTF8ToUnicode(CMDContent["VirusLibName"].asString());
		_tcsncpy_s(stVersionInfo.wcVirusLibName, _countof(stVersionInfo.wcVirusLibName), wsVirusLibName.c_str(), _countof(stVersionInfo.wcVirusLibName) - 1);
	}

	if (CMDContent.isMember("VirusLibUploadTime") && !CMDContent["VirusLibUploadTime"].isNull())
	{
		std::wstring wsVirusLibUploadTime = UTF8ToUnicode(CMDContent["VirusLibUploadTime"].asString());
		_tcsncpy_s(stVersionInfo.wcVirusLibUploadTime, _countof(stVersionInfo.wcVirusLibUploadTime), wsVirusLibUploadTime.c_str(), _countof(stVersionInfo.wcVirusLibUploadTime) - 1);
	}

	bResult = TRUE;

END:

	return bResult;
}

BOOL CWLJsonParse::UpdateClientInfo_GetJson(__in New_Package_Version_ST stVersionInfo, __in tstring ComputerID, __in DWORD dwPort, __in tstring SeverIp, __out string &strJson)
{
	/*新版：
	{
		"CMDContent":
		{
			"PackageMd5":"cc7a7d0c86ab2937b701f5e987d03c56",
				"PackageName":"XXX.exe",
				"Port":69,
				"Server":"192.168.4.207"
				"Size":158765*1024,
				"URL":"https://192.168.4.160:8440/USM/notLoginDownLoad/downloadByFileName.do?fileName=IEG_V300R003C11B190_ubuntu16-x64.bin"
		},
		"CMDID":255,
		"CMDTYPE":150,
		"ComputerID":"6CBE2F9EWIN-IP26O1UDS48",
		"DOMAIN":null,
		"Username":null
	}*/

	Json::Value root;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	root["ComputerID"] = UnicodeToUTF8(ComputerID);
	root["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	root["CMDID"] = (int)PLY_GET_UPGRADE_VERSION;     /*350*/

	//	CMDContentItem["ClientType"] = (DWORD)theApp.m_dwDlgMainType;
	CMDContentItem["PackageMd5"] = UnicodeToUTF8(stVersionInfo.wcClientMd5);
	CMDContentItem["PackageName"] = UnicodeToUTF8(stVersionInfo.wcClientName);
	CMDContentItem["Port"] = (int)dwPort;
	CMDContentItem["Server"] = UnicodeToUTF8(SeverIp);
	CMDContentItem["Size"] = (int)stVersionInfo.dwClientSize;
	CMDContentItem["URL"] = stVersionInfo.cClientUpgradeURL;

	root["CMDContent"] = (Json::Value)CMDContentItem;

	strJson = jsWriter.write(root);

	return TRUE;
}

BOOL CWLJsonParse::UpdateVirusLibInfo_GetJson(__in New_Package_Version_ST stVersionInfo, __in tstring ComputerID, __in DWORD dwPort, __in tstring SeverIp, __out string &strJson)
{
	/*新版：
	{
		"CMDContent":
		{
			"PackageMd5":"cc7a7d0c86ab2937b701f5e987d03c56",
				"PackageName":"XXX.exe",
				"Port":69,
				"Server":"192.168.4.207"
				"Size":158765*1024,
				"URL":"https://192.168.4.160:8440/USM/notLoginDownLoad/downloadByFileName.do?fileName=IEG_V300R003C11B190_ubuntu16-x64.bin"
		},
		"CMDID":255,
		"CMDTYPE":150,
		"ComputerID":"6CBE2F9EWIN-IP26O1UDS48",
		"DOMAIN":null,
		"Username":null
	}*/

	Json::Value root;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	root["ComputerID"] = UnicodeToUTF8(ComputerID);
	root["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	root["CMDID"] = (int)PLY_GET_UPGRADE_VERSION;     /*350*/

	//	CMDContentItem["ClientType"] = (DWORD)theApp.m_dwDlgMainType;
	CMDContentItem["PackageMd5"] = UnicodeToUTF8(stVersionInfo.wcVirusLibMd5);
	CMDContentItem["PackageName"] = UnicodeToUTF8(stVersionInfo.wcVirusLibName);
	CMDContentItem["Port"] = (int)dwPort;
	CMDContentItem["Server"] = UnicodeToUTF8(SeverIp);
	CMDContentItem["Size"] = (int)stVersionInfo.dwVirusLibSize;
	CMDContentItem["URL"] = stVersionInfo.cVirusLibUpgradeURL;

	root["CMDContent"] = (Json::Value)CMDContentItem;

	strJson = jsWriter.write(root);

	return TRUE;
}

std::string CWLJsonParse::SelfAdaptationStatus_GetJson(int nSwitch, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	Json::Value root;
	Json::FastWriter jsWriter;
	std::string strJson;
	Json::Value CMDContentItem;

	// 参数
	root["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	root["CMDTYPE"] = (int)nCmdType; /*200*/
	root["CMDID"] = (int)nCmdID;     /*270*/

	CMDContentItem["AutoApproval"]    = nSwitch;;

	root["CMDContent"] = (Json::Value)CMDContentItem;

	strJson = jsWriter.write(root);

	return strJson;
}

BOOL CWLJsonParse::SelfAdaptationStatus_GetValue(__in string strJson, int &nSwitch)
{
	Json::Value root;
	Json::Reader parser;
	bool bRet = FALSE;

	bRet = parser.parse(strJson, root);

    if (root.isArray() && root.size() > 0)
    {
        root = root[0];
    }

	if ( root.isMember("CMDContent") && 
		root["CMDContent"].isObject())
	{
		root = root["CMDContent"];
	}

	if (root.isMember("AutoApproval") &&
		root["AutoApproval"].isNumeric())
	{
		nSwitch = root["AutoApproval"].asInt();
		bRet = TRUE;
	}

	return bRet;
}


BOOL CWLJsonParse::Update_VirusLibVersionToUsm_GetJson(__in tstring ComputerID, __in tstring VirusVersion,  __in int iVirusLibType, __in int iResult, __out string &strJson)
{
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(ComputerID);
	Item["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	Item["CMDID"] = (int)PLY_UPDATE_VIRUS_LIB_VERSION;     /*351*/

	//	CMDContentItem["ClientType"] = (DWORD)theApp.m_dwDlgMainType;
	CMDContentItem["Result"] = (int)iResult;
	CMDContentItem["VirusLibVersion"] = UnicodeToUTF8(VirusVersion);
	CMDContentItem["VirusLibType"] = (int)iVirusLibType;

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	root.append(Item);

	strJson = jsWriter.write(root);

	return TRUE;
}

BOOL CWLJsonParse::Surveil_Config_GetValue(__in const std::string& strJson,
                                           __out WLSURVEIL_CONFIG &stConf)
{
    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root_1))
    {
        return FALSE;
    }

    int nObject = root_1.size();
    if( nObject < 1)
    {
        return FALSE;
    }

    root = root_1[0];

    if(root.isMember("enable") && root["enable"].isNumeric())
    {
        stConf.enable = root["enable"].asInt();
    }

    if(root.isMember("static_trap") && root["static_trap"].isNumeric())
    {
        stConf.static_trap = root["static_trap"].asInt();
    }

    if(root.isMember("dynamic_trap") && root["dynamic_trap"].isNumeric())
    {
        stConf.dynamic_trap = root["dynamic_trap"].asInt();
    }

    //if(root.isMember("virus_feature") && root["virus_feature"].isNumeric())
    //{
    //    stConf.virus_feature = root["virus_feature"].asInt();
    //}

    if(root.isMember("behavior_analysis") && root["behavior_analysis"].isNumeric())
    {
        stConf.behavior_analysis = root["behavior_analysis"].asInt();
    }

    if(root.isMember("ignore_trust_state") && root["ignore_trust_state"].isNumeric())
    {
        stConf.ignore_trust_state = root["ignore_trust_state"].asInt();
    }

    if(root.isMember("deal_mode") && root["deal_mode"].isNumeric())
    {
        stConf.deal_mode = root["deal_mode"].asInt();
    }

    return TRUE;
}

std::string CWLJsonParse::Surveil_Config_GetJson(__in const WLSURVEIL_CONFIG& stConf)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    Json::Value item;
    item["enable"] = stConf.enable;
    item["static_trap"] = stConf.static_trap;
    item["dynamic_trap"] = stConf.dynamic_trap;
    //item["virus_feature"] = stConf.virus_feature;
    item["behavior_analysis"] = stConf.behavior_analysis;
    item["ignore_trust_state"] = stConf.ignore_trust_state;
    item["deal_mode"] = stConf.deal_mode;
    root.append(item);

    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::Surveil_Bait_Config_GetValue(__in const std::string& strJson,
                                                __out std::vector<std::wstring> &vec_strDirs,
                                                __out std::vector<DEFAULT_BAITFILE_INFO> &vec_BaitFileInfo)
{
    if (strJson.empty())
    {
        return FALSE;
    }
    
    Json::Value root;
	Json::Value root_1;
    Json::Reader reader;
    std::string  strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root_1))
    {
        return FALSE;
    }
	//wwdv2
	if (root_1.isArray() && root_1.size() >= 1)
	{
		root = root_1[0];

	

		// 获取符合项
		if (root.isMember("bait_file") && root["bait_file"].isArray())
		{

			const Json::Value& items = root["bait_file"];
			for(unsigned int i = 0;i < items.size();i++)
			{
				const Json::Value& item = items[i];
				DEFAULT_BAITFILE_INFO  baitInfo;
				if(item.isMember("file") && item["file"].isString())
				{
					baitInfo.wstrFileName = CStrUtil::UTF8ToUnicode(item["file"].asString());
				}

				if(item.isMember("hash") && item["hash"].isString())
				{
					baitInfo.wstrHash = CStrUtil::UTF8ToUnicode(item["hash"].asString());
				}


				vec_BaitFileInfo.push_back(baitInfo);
			}
		}

		if (root.isMember("default_bait_path") && root["default_bait_path"].isArray())
		{

			const Json::Value& items = root["default_bait_path"];
			for(unsigned int i = 0;i < items.size();i++)
			{
				const Json::Value& item = items[i];
				std::wstring wstrPath;
				if(item.isMember("path") && item["path"].isString())
				{
					wstrPath = CStrUtil::UTF8ToUnicode(item["path"].asString());
				}


				vec_strDirs.push_back(wstrPath);
			}
		}

	}
	
	
    
    return TRUE;
}

BOOL CWLJsonParse::Surveil_Analyze_Config_GetValue(__in const std::string& strJson,
                                                   __out std::map<std::wstring, DOUBLE> &mapEntropy)
{
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }

    // 获取符合项
    if (root.isMember("ext_entropy") && root["ext_entropy"].isArray())
    {
        const Json::Value& items = root["ext_entropy"];
        for(unsigned int i = 0; i < items.size(); i++)
        {
            const Json::Value& item = items[i];
            std::wstring wstrExt;
            DOUBLE dEntropy;
            if(item.isMember("ext") && item["ext"].isString())
            {
                wstrExt = CStrUtil::UTF8ToUnicode(item["ext"].asString());
            }

            if(item.isMember("entropy") && item["entropy"].isDouble())
            {
                dEntropy = root["entropy"].asDouble();
            }

            mapEntropy.insert(std::make_pair(wstrExt, dEntropy));
        }
    }

    return TRUE;
}

std::string CWLJsonParse::Surveil_StaticBait_Record_GetJson(__in const std::vector<std::wstring>& vecBaitDir,
                                                            __in const std::vector<DEFAULT_BAITFILE_INFO>& vecBaitFileInfo)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::Value items_dir;
    Json::Value items_info;
    Json::FastWriter writer;


    if (vecBaitDir.empty() && vecBaitFileInfo.empty())
    {
        return sJsonPacket;
    }

    for(unsigned int i = 0; i < vecBaitDir.size(); i++)
    {
        Json::Value item;
        item["Dir"] = CStrUtil::UnicodeToUTF8(vecBaitDir[i]);

        items_dir.append(item);	
    }

    for(unsigned int i = 0; i < vecBaitFileInfo.size(); i++)
    {
        Json::Value item;
        item["FileName"] = CStrUtil::UnicodeToUTF8(vecBaitFileInfo[i].wstrFileName);
        item["Hash"] = CStrUtil::UnicodeToUTF8(vecBaitFileInfo[i].wstrHash);
        items_info.append(item);	
    }

    root["Dir"] = items_dir;
    root["FileName"] = items_info;
    sJsonPacket = writer.write(root);
    return sJsonPacket;
}

BOOL CWLJsonParse::Surveil_StaticBait_Record_GetValue(__in const std::string& strJson,
                                                      __out std::vector<std::wstring> &vecBaitDir,
                                                      __out std::vector<DEFAULT_BAITFILE_INFO> &vecBaitFileInfo)
{
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }

    // 获取符合项
    if (root.isMember("Dir") && root["Dir"].isArray())
    {

        const Json::Value& items = root["Dir"];
        for(unsigned int i = 0;i < items.size();i++)
        {
            const Json::Value& item = items[i];
            std::wstring  wstrDir;
            if(item.isMember("Dir") && item["Dir"].isString())
            {
                wstrDir = CStrUtil::UTF8ToUnicode(item["Dir"].asString());
            }

            vecBaitDir.push_back(wstrDir);
        }
    }

    if (root.isMember("FileName") && root["FileName"].isArray())
    {
        const Json::Value& items = root["FileName"];
        for(unsigned int i = 0; i < items.size(); i++)
        {
            const Json::Value& item = items[i];
            DEFAULT_BAITFILE_INFO baitinfo;
            if(item.isMember("FileName") && item["FileName"].isString())
            {
                baitinfo.wstrFileName = CStrUtil::UTF8ToUnicode(item["FileName"].asString());
            }

            if(item.isMember("Hash") && item["Hash"].isString())
            {
                baitinfo.wstrHash = CStrUtil::UTF8ToUnicode(item["Hash"].asString());
            }

            vecBaitFileInfo.push_back(baitinfo);
        }
    }

    return TRUE;
}

std::string CWLJsonParse::Surveil_DynamicBait_Record_GetJson(__in const std::vector<DYNAMIC_BAIT_INFO>& vecDynamicBait)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    if (vecDynamicBait.empty())
    {
        return sJsonPacket;
    }

    for(unsigned int i = 0; i < vecDynamicBait.size(); i++)
    {
        Json::Value item;
        item["FilePath"] = CStrUtil::UnicodeToUTF8(vecDynamicBait[i].pBaitPath);
        item["CreateTime"] = (unsigned int)vecDynamicBait[i].CreateTime;

        root.append(item);	
    }

    sJsonPacket = writer.write(root);
    return sJsonPacket;
}

BOOL CWLJsonParse::Surveil_DynamicBait_Record_GetValue(__in const std::string& strJson,
                                                       __out std::vector<DYNAMIC_BAIT_INFO> &vecDynamicBait)
{
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }

    // 获取符合项
    if (!root.isArray())
    {
        return TRUE;
    }

    std::wstring wstrPath;
    const Json::Value& items = root;
    for(unsigned int i = 0; i < items.size(); i++)
    {
        const Json::Value& item = items[i];
        DYNAMIC_BAIT_INFO baitinfo;

        if(item.isMember("FilePath") && item["FilePath"].isString())
        {
            wstrPath = CStrUtil::UTF8ToUnicode(item["FilePath"].asString());
            wmemset(baitinfo.pBaitPath, 0, MAX_PATH);
            _tcsncpy_s(baitinfo.pBaitPath, wstrPath.c_str(), wstrPath.length());
        }

        if (item.isMember("CreateTime") && item["CreateTime"].isUInt())
        {
            baitinfo.CreateTime =  item["CreateTime"].asUInt();
        }

        vecDynamicBait.push_back(baitinfo);
    }

    return TRUE;
}

BOOL CWLJsonParse::Surveil_HashStore_Path_GetValue(__in const std::string& strJson,
                                                   __out std::wstring &wstrPath,
                                                   __out std::wstring &wstrVersion)
{
    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root_1) || !root_1.isArray())
    {
        return FALSE;
    }

    int nObject = root_1.size();
    if( nObject < 1)
    {
        return FALSE;
    }

    root = root_1[0];

    if(root.isMember("filepath") && root["filepath"].isString())
    {
        wstrPath = CStrUtil::UTF8ToUnicode(root["filepath"].asString());
    }

    if(root.isMember("version") && root["version"].isString())
    {
        wstrVersion = CStrUtil::UTF8ToUnicode(root["version"].asString());
    }

    return TRUE;
}

std::string CWLJsonParse::Surveil_Config_GetJson_USM(__in const WLSURVEIL_CONFIG& stConf,
                                                     __in const std::wstring& ComputerID,
                                                     __in const int& CMDTYPE,
                                                     __in const UINT& CMDID)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    Json::Value person;
    Json::Value data;
    person["enable"] = stConf.enable;
    person["static_trap"] = stConf.static_trap;
    person["dynamic_trap"] = stConf.dynamic_trap;
    //person["virus_feature"] = stConf.virus_feature;
    person["behavior_analysis"] = stConf.behavior_analysis;
    person["ignore_trust_state"] = stConf.ignore_trust_state;
    person["deal_mode"] = stConf.deal_mode;
    data["CMDContent"] = person;

    data["ComputerID"] = CStrUtil::UnicodeToUTF8(ComputerID);
    data["CMDTYPE"] = CMDTYPE;
    data["CMDID"] = CMDID;

    root.append(data);

    const string& strlog = root.toStyledString();
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::Surveil_Config_GetValue_USM(__in const std::string& strJson,
                                 __out WLSURVEIL_CONFIG& stConf,
                                 __out std::wstring& ComputerID,
                                 __out int& CMDTYPE,
                                 __out UINT& CMDID)
{
    Json::Value root;
    Json::Value roots;
    Json::Value data;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    if (!reader.parse(strJson, roots))
    {
        return FALSE;
    }

    if(roots.isArray() && (roots.size() > 0))
    {
        root = roots[0];
    }
    else if(roots.isObject())
    {
       root = roots;
    }
    else
    {
        return FALSE;
    }

    if(root.isMember("CMDContent") && root["CMDContent"].isObject())
    {
        data = root["CMDContent"];
        if(data.isMember("behavior_analysis") && data["behavior_analysis"].isInt())
        {
            stConf.behavior_analysis = data["behavior_analysis"].asInt();
        }

        if(data.isMember("deal_mode") && data["deal_mode"].isInt())
        {
            stConf.deal_mode = data["deal_mode"].asInt();
        }

        if(data.isMember("dynamic_trap") && data["dynamic_trap"].isInt())
        {
            stConf.dynamic_trap = data["dynamic_trap"].asInt();
        }

        if(data.isMember("enable") && data["enable"].isInt())
        {
            stConf.enable = data["enable"].asInt();
        }

        if(data.isMember("static_trap") && data["static_trap"].isInt())
        {
            stConf.static_trap = data["static_trap"].asInt();
        }

        if(data.isMember("ignore_trust_state") && data["ignore_trust_state"].isInt())
        {
            stConf.ignore_trust_state = data["ignore_trust_state"].asInt();
        }
    }

    if(root.isMember("CMDID") && root["CMDID"].isUInt())
    {
        CMDID = data["static_trap"].asUInt();
    }

    if(root.isMember("CMDTYPE") && root["CMDTYPE"].isInt())
    {
        CMDTYPE = data["CMDTYPE"].asInt();
    }

    if(root.isMember("ComputerID") && root["ComputerID"].isString())
    {
        ComputerID = CStrUtil::UTF8ToUnicode(root["ComputerID"].asString());
    }

    return TRUE;
}

BOOL CWLJsonParse::Surveil_Config_Exception_GetValue(__in const std::string &strJson,
                                       __out std::vector<std::wstring> &vecExceptions)
{
    Json::Value root;
    Json::Value Data;
    Json::Reader parser;

    if (strJson.empty())
    {
        return TRUE;
    }

    if (!parser.parse(strJson, root))
    {
        return FALSE;
    }

    if (!(root.isMember("items") 
     && root["items"].isArray()))
    {
        return FALSE;
    }

    const Json::Value& items = root["items"];

    if (!items.isArray())
    {
        return FALSE;
    }

    if (items.size() == 0)
    {
        return FALSE;
    }
    
    if (!(items[0].isMember("paths") 
      && items[0]["paths"].isArray()))
    {
        return FALSE;
    }

    const Json::Value &paths = items[0]["paths"];
    for (std::size_t i = 0; i < paths.size(); ++i)
    {
        const Json::Value &item = paths[(int)i];
        if (item.isMember("name")
           && item["name"].isString())
        {
            std::wstring strExceptItem = CStrUtil::UTF8ToUnicode(item["name"].asString());
            vecExceptions.push_back(strExceptItem);
        }
    }

    return TRUE;
}

std::string CWLJsonParse::DPLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<EVENT_DATA_PROTECT*>& vecDPLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecDPLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::FastWriter writer;
    Json::Value root;
    Json::Value vulLogJson;
    Json::Value virusLog;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value logItem;
        logItem["OperationTime"] = (Json::UInt64)vecDPLog[i]->TimeStamp;
        logItem["Operator"] = UnicodeToUTF8(vecDPLog[i]->Subject);
        logItem["OperationObject"] = UnicodeToUTF8(vecDPLog[i]->Object);
        logItem["Action"] = (UINT)vecDPLog[i]->Action;
        logItem["Result"] = (UINT)vecDPLog[i]->Result;
        virusLog.append(logItem);
    }

    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = virusLog;

    root.append(vulLogJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::DPLog_GetJsonByVector(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, __in vector<CWLMetaData*>& vecDPLog)
{
    std::string sJsonPacket = "";
    std::string sJsonBody = "";

    int nCount = (int)vecDPLog.size();
    if( nCount == 0)
        return sJsonPacket;

    Json::FastWriter writer;
    Json::Value root;
    Json::Value vulLogJson;
    Json::Value virusLog;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value logItem;
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecDPLog[i]->GetData();
        EVENT_DATA_PROTECT *pDPLog = (EVENT_DATA_PROTECT*)pipclogcomm->data;

        logItem["Time"] = (Json::UInt64)pDPLog->TimeStamp;
        logItem["ProcessName"] = UnicodeToUTF8(pDPLog->Subject);
        logItem["FileName"] = UnicodeToUTF8(pDPLog->Object);
        logItem["Operation"] = (UINT)pDPLog->Action;
        logItem["Result"] = (UINT)pDPLog->Result;
        virusLog.append(logItem);
    }

    vulLogJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    vulLogJson["CMDTYPE"] = (int)cmdType;
    vulLogJson["CMDID"] = (int)cmdID;
    vulLogJson["CMDContent"] = virusLog;

    root.append(vulLogJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::DataGuard_Config_GetValue(__in const std::string& strJson,
                                           __out WLDP_CONFIG &stConf)
{
    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root_1) || !root_1.isArray())
    {
        return FALSE;
    }

    int nObject = root_1.size();
    if( nObject < 1)
    {
        return FALSE;
    }

    root = root_1[0];

    if(root.isMember("enable") && root["enable"].isNumeric())
    {
        stConf.enable = root["enable"].asInt();
    }

    if(root.isMember("deal_mode") && root["deal_mode"].isNumeric())
    {
        stConf.deal_mode = root["deal_mode"].asInt();
    }

    if (root.isMember("exts") && root["exts"].isArray())
    {

        const Json::Value& items = root["exts"];
        for(unsigned int i = 0;i < items.size();i++)
        {
            const Json::Value& item = items[i];
            WLDP_TYPES stType;
            if(item.isMember("name") && item["name"].isString())
            {
                stType.ext = CStrUtil::UTF8ToUnicode(item["name"].asString());
            }

            if(item.isMember("description") && item["description"].isString())
            {
                stType.description = CStrUtil::UTF8ToUnicode(item["description"].asString());
            }

            stConf.file_types.push_back(stType);
        }
    }

    return TRUE;
}

std::string CWLJsonParse::DataGuard_Config_GetJson(__in const WLDP_CONFIG& stConf)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::Value items;
    Json::FastWriter writer;
    Json::Value person;

    root["enable"] = stConf.enable;
    root["deal_mode"] = stConf.deal_mode;

    for(unsigned int i = 0; i < stConf.file_types.size(); i++)
    {
        person["name"] = CStrUtil::UnicodeToUTF8(stConf.file_types[i].ext);
        person["description"] = CStrUtil::UnicodeToUTF8(stConf.file_types[i].description);

        items.append(person);	
    }

    root["exts"] = items;
    
    sJsonPacket = writer.write(root);
    return sJsonPacket;
}

std::string CWLJsonParse::DataGuard_Config_GetJson_USM(__in const WLDP_CONFIG& stConf,
                                                     __in const std::wstring& ComputerID,
                                                     __in const int& CMDTYPE,
                                                     __in const UINT& CMDID)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    Json::Value person;
    Json::Value data;
    Json::Value items;
    Json::Value item;
    person["enable"] = stConf.enable;
    person["deal_mode"] = stConf.deal_mode;

    for(unsigned int i = 0; i < stConf.file_types.size(); i++)
    {
        item["name"] = CStrUtil::UnicodeToUTF8(stConf.file_types[i].ext);
        item["description"] = CStrUtil::UnicodeToUTF8(stConf.file_types[i].description);

        items.append(item);	
    }

    person["exts"] = items;	

    data["CMDContent"] = person;

    data["ComputerID"] = CStrUtil::UnicodeToUTF8(ComputerID);
    data["CMDTYPE"] = CMDTYPE;
    data["CMDID"] = CMDID;

    root.append(data);

    const string& strlog = root.toStyledString();
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::DataGuard_Config_GetValue_USM(__in const std::string& strJson,
                                   __out WLDP_CONFIG& stConf,
                                   __out std::wstring& ComputerID,
                                   __out int& CMDTYPE,
                                   __out UINT& CMDID)
{
    Json::Value root;
    Json::Value roots;
    Json::Value data;
    Json::Reader reader;

    if(strJson.empty())
    {
        return FALSE;
    }

    if (!reader.parse(strJson, roots))
    {
        return FALSE;
    }

    if(roots.isArray() && (roots.size() > 0))
    {
        root = roots[0];
    }
    else if(roots.isObject())
    {
        root = roots;
    }
    else
    {
        return FALSE;
    }

    if(root.isMember("CMDContent") && root["CMDContent"].isObject())
    {
        data = root["CMDContent"];
        if(data.isMember("deal_mode") && data["deal_mode"].isInt())
        {
            stConf.deal_mode = data["deal_mode"].asInt();
        }

        if(data.isMember("enable") && data["enable"].isInt())
        {
            stConf.enable = data["enable"].asInt();
        }

        if(data.isMember("exts") && data["exts"].isArray())
        {
            const Json::Value& items = data["exts"];
            for(unsigned int i = 0;i < items.size();i++)
            {
                const Json::Value& item = items[i];
                WLDP_TYPES type;

                if(item.isMember("description") && item["description"].isString())
                {
                    type.description = CStrUtil::UTF8ToUnicode(item["description"].asString());
                }

                if(item.isMember("name") && item["name"].isString())
                {
                    type.ext = CStrUtil::UTF8ToUnicode(item["name"].asString());
                }

                stConf.file_types.push_back(type);
            }
        }
    }

    if(root.isMember("CMDID") && root["CMDID"].isUInt())
    {
        CMDID = data["static_trap"].asUInt();
    }

    if(root.isMember("CMDTYPE") && root["CMDTYPE"].isInt())
    {
        CMDTYPE = data["CMDTYPE"].asInt();
    }

    if(root.isMember("ComputerID") && root["ComputerID"].isString())
    {
        ComputerID = CStrUtil::UTF8ToUnicode(root["ComputerID"].asString());
    }

    return TRUE;
}

BOOL CWLJsonParse::DataGuard_IE_Path_GetValue(__in const std::string& strJson,
                                              __out wstring& wstrPath)
{
    Json::Value root;
    Json::Reader reader;

    if(strJson.empty())
    {
        return FALSE;
    }


    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }


    if(root.isMember("path") && root["path"].isString())
    {
        wstrPath = CStrUtil::UTF8ToUnicode(root["path"].asString());
    }

    return TRUE;
}

std::string CWLJsonParse::DataGuard_IE_Path_GetJson(__in const wstring& wstrPath)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    root["path"] = UnicodeToUTF8(wstrPath);

    sJsonPacket = writer.write(root);
    return sJsonPacket;
}

std::string CWLJsonParse::SysGuardLog_GetJsonByVector(__in tstring ComputerID, 
                                                      __in WORD cmdType , __in WORD cmdID,
                                                      __in vector<SYSTEM_GUARD_LOG*>& vecLogs)
{
    std::string sJsonPacket;

    int nCount = (int)vecLogs.size();
    if( vecLogs.empty())
    {
        return sJsonPacket;
    }

    Json::Value root;
    Json::Value logJson;
    Json::Value logs;
    Json::FastWriter writer;

    for (int i = 0; i < nCount; i++)
    {
        Json::Value item;
        item["OperationTime"] = (Json::UInt64)vecLogs[i]->TimeStamp;
        item["Subject"] = UnicodeToUTF8(vecLogs[i]->Subject);
        item["Object"] = UnicodeToUTF8(vecLogs[i]->Object);
        item["Action"] = (UINT)vecLogs[i]->Action;
        item["Result"] = (UINT)vecLogs[i]->Result;
        logs.append(item);
    }

    logJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    logJson["CMDTYPE"] = (int)cmdType;
    logJson["CMDID"] = (int)cmdID;
    logJson["CMDContent"] = logs;

    root.append(logJson);
    sJsonPacket = writer.write(root);
    root.clear();
    return sJsonPacket;
}

std::string CWLJsonParse::SysGuardLog_GetJsonByVector(__in tstring ComputerID, 
                                                      __in WORD cmdType , __in WORD cmdID, 
                                                      __in vector<CWLMetaData*>& vecLogs)
{
    std::string sJsonPacket;

    if( vecLogs.empty())
    {
        return sJsonPacket;
    }

    Json::FastWriter writer;
    Json::Value root;
    Json::Value logJson;
    Json::Value logs;
    int nCount = (int)vecLogs.size();
    
    for (int i = 0; i < nCount; i++)
    {
        Json::Value item;
        IPC_LOG_COMMON* pipclogcomm = (IPC_LOG_COMMON*)vecLogs[i]->GetData();
        SYSTEM_GUARD_LOG *pDPLog = (SYSTEM_GUARD_LOG*)pipclogcomm->data;

        item["Time"] = (Json::UInt64)pDPLog->TimeStamp;
        item["Subject"] = UnicodeToUTF8(pDPLog->Subject);
        item["Object"] = UnicodeToUTF8(pDPLog->Object);
        item["Action"] = (UINT)pDPLog->Action;
        item["Result"] = (UINT)pDPLog->Result;
        logs.append(item);
    }

    logJson["ComputerID"]= UnicodeToUTF8(ComputerID);
    logJson["CMDTYPE"] = (int)cmdType;
    logJson["CMDID"] = (int)cmdID;
    logJson["CMDContent"] = logs;

    root.append(logJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::BackupConfig_GetJson(__in WLBACKUP_CONFIG& stBackConfig, __in PWCHAR szComputerID)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    Json::Value person;
    Json::Value data;
    Json::Value items;
    Json::Value item;

    root["enable"]                 = stBackConfig.enAble;
    root["path"]                   = CStrUtil::UnicodeToUTF8(stBackConfig.strPath);
    root["max_disk_space"]         = stBackConfig.max_disk_space;
    root["max_times"]              = stBackConfig.max_times;
    root["trusted_process_backup"] = stBackConfig.trusted_process_backup;
    root["expiration_period"]      = stBackConfig.expiration_period;
    root["max_file_size"]          = stBackConfig.max_file_size;

    for(unsigned int i = 0; i < stBackConfig.file_types.size(); i++)
    {
        item["ext"] = CStrUtil::UnicodeToUTF8(stBackConfig.file_types[i].ext);
        item["description"] = UnicodeToUTF8(stBackConfig.file_types[i].description);

        items.append(item);	
    }

    root["file_types"] = (Json::Value)items;	

    data["CMDContent"] = (Json::Value)root;

    if (NULL != szComputerID)
    {
        data["ComputerID"] = UnicodeToUTF8(szComputerID);
    }
    data["CMDTYPE"] = 150;
    data["CMDID"] = 503;

    person.append(data);

    sJsonPacket = writer.write(person);
    person.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Config_ParseJson(__in string strJson, __out WLBACKUP_CONFIG &stBackupConfig)
{
    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv1
    if (!reader.parse(strValue, root_1) || !root_1.isArray() || root_1.size() < 1)
    {
        return FALSE;
    }

    if (!root_1[0].isMember("CMDContent"))
    {
        return FALSE;
    }

    root = (Json::Value)root_1[0]["CMDContent"];

    if(root.isMember("enable") && root["enable"].isInt())
    {
        stBackupConfig.enAble = root["enable"].asInt();
    }

    if(root.isMember("path") && root["path"].isString())
    {
        stBackupConfig.strPath = CStrUtil::UTF8ToUnicode(root["path"].asString());
    }

    if(root.isMember("max_disk_space") && root["max_disk_space"].isInt())
    {
        stBackupConfig.max_disk_space = root["max_disk_space"].asInt();
    }

    if(root.isMember("max_times") && root["max_times"].isInt())
    {
        stBackupConfig.max_times = root["max_times"].asInt();
    }

    if(root.isMember("trusted_process_backup") && root["trusted_process_backup"].isBool())
    {
        stBackupConfig.trusted_process_backup = root["trusted_process_backup"].asBool();
    }

    if(root.isMember("expiration_period") && root["expiration_period"].isInt())
    {
        stBackupConfig.expiration_period = root["expiration_period"].asInt();
    }

    if(root.isMember("max_file_size") && root["max_file_size"].isInt())
    {
        stBackupConfig.max_file_size = root["max_file_size"].asInt();
    }

    if (root.isMember("file_types") && root["file_types"].isArray())
    {

        const Json::Value& items = root["file_types"];
        for(unsigned int i = 0;i < items.size();i++)
        {
            const Json::Value& item = items[i];
            WLBACKUP_TYPES stType;
            if(item.isMember("ext") && item["ext"].isString())
            {
                stType.ext = CStrUtil::UTF8ToUnicode(item["ext"].asString());
            }

            if(item.isMember("description") && item["description"].isString())
            {
                stType.description = CStrUtil::UTF8ToUnicode(item["description"].asString());
            }

            stBackupConfig.file_types.push_back(stType);
        }
    }

    return TRUE;
}

std::string CWLJsonParse::Backup_FileInfo_GetJson( __in vector<BACKUP_FILE_INFO_ST>& vecBackupLogs, 
                                                  __in const DWORD &dwCount,
                                                  __in const DWORD &dwAllCount,
                                                  __in PWCHAR szComputerID)
{
//     "file_name": "C:\\Users\\Public\\设计.docx",
//         "create_time": 1652861946,
//         "update_time": 1652861946,
//         "access_time": 1652861946,
//         "backup_time": 1652861946,
//         "hash_value": "c1c68d0db5253155942a751f26dffeb7af7ce756",
//         "process_name": "C:\\xxx.exe",
//         "file_size": "1024",
//         "standard_deviation": "1.244916919141",
//         "operate": "1"
    Json::FastWriter writer;
    Json::Value root;
    Json::Value logJson;
    Json::Value logs;
    std::string sJsonPacket;

    int nCount = (int)vecBackupLogs.size();
    if( vecBackupLogs.empty())
    {
        return sJsonPacket;
    }

    for (int i = 0; i < nCount; i++)
    {
        Json::Value item;

        item["id"] = UnicodeToUTF8(vecBackupLogs[i].ID);
        item["uuid"] = UnicodeToUTF8(vecBackupLogs[i].uuid);
        item["file_name"] = UnicodeToUTF8(vecBackupLogs[i].wstrFileName);
        item["create_time"] = (Json::UInt64)vecBackupLogs[i].llCreateTime;
        item["update_time"] = (Json::UInt64)vecBackupLogs[i].llUpdateTime;
        item["access_time"] = (Json::UInt64)vecBackupLogs[i].llAccessTime;
        item["backup_time"] = (Json::UInt64)vecBackupLogs[i].llBackupTime;
        item["hash_value"] = UnicodeToUTF8(vecBackupLogs[i].wstrHashValue);
        item["process_name"] = UnicodeToUTF8(vecBackupLogs[i].wstrProcessName);
        item["file_size"] = (Json::UInt64)vecBackupLogs[i].llFileSize;
        item["operate"] = (Json::UInt64)vecBackupLogs[i].nAction;
        logs.append(item);
    }

    if (dwCount)
    {
        logJson["RowCount"] = (UINT)dwCount;
    }
    if (dwAllCount)
    {
        logJson["AllCount"] = (UINT)dwAllCount;

    }

    if (NULL != szComputerID)
    {
        logJson["ComputerID"]= UnicodeToUTF8(szComputerID);
    }
    
    logJson["CMDTYPE"] = 200;
    logJson["CMDID"] = 603;
    logJson["CMDContent"] = logs;

    root.append(logJson);
    sJsonPacket = writer.write(root);

    root.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_FileInfo_GetValue(__in const std::string& strJson,
                                             __out std::vector<BACKUP_FILE_INFO_ST> &vecBackupLogs,
                                             __out int &nRowCount,
                                             __out int &nAllCount)
{
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string  strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2 
    if (!reader.parse(strValue, root_1) || !root.isArray() || root.size() < 1)
    {
        return FALSE;
    }

    root = root_1[0];

    // 获取总行数
    if (root.isMember("RowCount") && root["RowCount"].isNumeric())
    {
        nRowCount = root["RowCount"].asInt();
    }

    // 获取总条目数
    if (root.isMember("AllCount") && root["AllCount"].isNumeric())
    {
        nAllCount = root["AllCount"].asInt();
    }


    const Json::Value& items = root;
    for(unsigned int i = 0; i < items.size(); i++)
    {
        const Json::Value& item = items[i];
        BACKUP_FILE_INFO_ST Backupinfo;

        if(item.isMember("id") && item["id"].isString())
        {
            Backupinfo.ID = CStrUtil::UTF8ToUnicode(item["id"].asString());
        }
        if(item.isMember("uuid") && item["uuid"].isString())
        {
            Backupinfo.uuid = CStrUtil::UTF8ToUnicode(item["uuid"].asString());
        }
        if(item.isMember("file_name") && item["file_name"].isString())
        {
            Backupinfo.wstrFileName = CStrUtil::UTF8ToUnicode(item["file_name"].asString());

        }
        if (item.isMember("create_time") && item["create_time"].asInt64())
        {
            Backupinfo.llCreateTime = item["create_time"].asInt64();
        }
        if (item.isMember("update_time") && item["update_time"].asInt64())
        {
            Backupinfo.llUpdateTime = item["update_time"].asInt64();
        }
        if (item.isMember("access_time") && item["access_time"].asInt64())
        {
            Backupinfo.llAccessTime = item["access_time"].asInt64();
        }
        if (item.isMember("backup_time") && item["backup_time"].asInt64())
        {
            Backupinfo.llBackupTime = item["backup_time"].asInt64();
        }
        if (item.isMember("hash_value") && item["hash_value"].isString())
        {
            Backupinfo.wstrHashValue = CStrUtil::UTF8ToUnicode(item["hash_value"].asString());         
        }
        if (item.isMember("process_name") && item["process_name"].isString())
        {
           Backupinfo.wstrProcessName = CStrUtil::UTF8ToUnicode(item["process_name"].asString());
         
        }
        if (item.isMember("file_size") && item["file_size"].asInt64())
        {
            Backupinfo.llFileSize = item["file_size"].asInt64();
        }
        if (item.isMember("operate") && item["operate"].asInt64())
        {
            Backupinfo.nAction = static_cast<INT>(item["operate"].asInt64());
        }

        vecBackupLogs.push_back(Backupinfo);
    }

    return TRUE;
}

std::string CWLJsonParse::Backup_Modify_GetJson(__in vector<STRUCT_BACKUP_MODIFY_ITEM>& vecBackupLogs, 
                                                __in UINT nCmd,
                                                __in PWCHAR szComputerID)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::Value logJson;
    Json::Value logs;
    Json::Value person;
    Json::FastWriter writer;

    int nCount = (int)vecBackupLogs.size();
    if( vecBackupLogs.empty())
    {
        return sJsonPacket;
    }

    Json::Value item;
    for (int i = 0; i < nCount; i++)
    {      
        item[i] = UnicodeToUTF8(vecBackupLogs[i].uuid);      

    }

    if (NULL != szComputerID)
    {
        logJson["ComputerID"]= UnicodeToUTF8(szComputerID);
    }

    logJson["CMDTYPE"] = 150;
    logJson["CMDID"] = nCmd;
    logJson["CMDContent"] = item;

    root.append(logJson);
    sJsonPacket = writer.write(logJson);

    logJson.clear();

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Modify_GetValue(__in std::string strJson,
                                          __out std::vector<STRUCT_BACKUP_MODIFY_ITEM> &vecBackupLogs)
{
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;


    std::string  strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root_1) || !root_1.isArray() || root_1.size() < 1)
    {
        return FALSE;
    }

    if (!root_1[0].isMember("CMDContent"))
    {
        return FALSE;
    }

    root = (Json::Value)root_1[0]["CMDContent"];

    const Json::Value& items = root;
    for(unsigned int i = 0; i < items.size(); i++)
    {
        STRUCT_BACKUP_MODIFY_ITEM BackupModify;

        BackupModify.uuid = CStrUtil::UTF8ToUnicode(items[i].asString());

        vecBackupLogs.push_back(BackupModify);
    }

    return TRUE;
}

BOOL CWLJsonParse::Backup_Export_GetValue(__in std::string strJson,
                                               __out STRUCT_BACKUP_EXPORT& stExport)
{   
    if (strJson.empty())
    {
        return FALSE;
    }

    Json::Value root;
    Json::Reader reader;
    std::string  strValue = strJson;

    if (!reader.parse(strValue, root))
    {
        return FALSE;
    }

    // 获取导出地址
    if (root.isMember("export_path") && root["export_path"].isString())
    {
        stExport.strExportPath = CStrUtil::UTF8ToUnicode(root["export_path"].asString());
    }

    // 获取符合项
    if (root.isMember("Items") && root["Items"].isArray())
    {

        const Json::Value& items = root["Items"];
        for(unsigned int i = 0;i < items.size();i++)
        {
            const Json::Value& item = items[i];

            if(item.isMember("uuid") && item["uuid"].isString())
            {
                std::wstring strUuid = CStrUtil::UTF8ToUnicode(item["uuid"].asString());
                stExport.vecUuid.push_back(strUuid);
            }
        }
    }

    return TRUE;
}

std::string CWLJsonParse::Backup_Export_GetJson(const STRUCT_BACKUP_EXPORT& stExport)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::Value items;
    Json::FastWriter writer;

    root["export_path"] = CStrUtil::UnicodeToUTF8(stExport.strExportPath);

    for (unsigned int i = 0; i < stExport.vecUuid.size(); i++)
    {
        items[i]["uuid"] = CStrUtil::UnicodeToUTF8(stExport.vecUuid[i]);
    }
    root["Items"] = items;

    sJsonPacket = writer.write(root);

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Query_GetValue(__in std::string strJson,
                                         __out STRUCT_BACKUP_QUERY &query)
{
    Json::Value root;
    Json::Value root_1;
    Json::Reader reader;
    std::string strValue = "";

    if(strJson.empty())
    {
        return FALSE;
    }

    strValue = strJson;
	//wwdv2
    if (!reader.parse(strValue, root_1) || !root.isArray() )
    {
        return FALSE;
    }

    int nObject = root_1.size();
    if( nObject < 1)
    {
        return FALSE;
    }

    root = root_1[0];

    if(root.isMember("time_start") && root["time_start"].isNumeric())
    {
        query.TimeStart = root["time_start"].asInt64();
    }

    if(root.isMember("time_end") && root["time_end"].isNumeric())
    {
        query.TimeEnd = root["time_end"].asInt64();
    }

    if(root.isMember("file_name") && root["file_name"].isString())
    {
        query.wstrFilename = CStrUtil::UTF8ToUnicode(root["file_name"].asString());
    }

    if(root.isMember("process_name") && root["process_name"].isString())
    {
        query.wstrProcessname = CStrUtil::UTF8ToUnicode(root["process_name"].asString());
    }

    if(root.isMember("limit") && root["limit"].isNumeric())
    {
        query.Limit = static_cast<int>(root["limit"].asInt64());
    }

    if(root.isMember("offset") && root["offset"].isNumeric())
    {
        query.Offset = static_cast<int>(root["offset"].asInt64());
    }

    if (root.isMember("LastFile") && root["LastFile"].isBool())
    {
        query.nLastFile = root["LastFile"].asBool();	
    }

    return TRUE;
}

std::string CWLJsonParse::Backup_Query_GetJson(__in STRUCT_BACKUP_QUERY query)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::FastWriter writer;

    Json::Value person;
    person["time_start"] = query.TimeStart;
    person["time_end"] = query.TimeEnd;
    person["file_name"] = CStrUtil::UnicodeToUTF8(query.wstrFilename);
    person["process_name"] = CStrUtil::UnicodeToUTF8(query.wstrProcessname);
    person["limit"] = query.Limit;
    person["offset"] = query.Offset;
    person["LastFile"] = query.nLastFile;
    root.append(person);

    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::Bakcp_Data_GetJson(__in const std::vector<BACKUP_FILE_INFO_ST> &vecData,
                                                           __in const DWORD &dwCount,
                                                           __in const DWORD &dwAllCount)
{
    std::string sJsonPacket;
    Json::Value root;
    Json::Value items;
    Json::FastWriter writer;
    Json::Value person;

    for(unsigned int i = 0; i < vecData.size(); i++)
    {
        person["uuid"] = CStrUtil::UnicodeToUTF8(vecData[i].uuid);
        person["file_name"] = CStrUtil::UnicodeToUTF8(vecData[i].wstrFileName);
        person["process_name"] = CStrUtil::UnicodeToUTF8(vecData[i].wstrProcessName);
        person["file_size"] = vecData[i].llFileSize;
        person["update_time"] = vecData[i].llUpdateTime;
        person["backup_time"] = vecData[i].llBackupTime;
        person["action"] = vecData[i].nAction;

        items.append(person);	
    }

    root["Items"] = items;
    root["RowCount"] = (UINT)dwCount;
    root["AllCount"] = (UINT)dwAllCount;
    sJsonPacket = writer.write(root);
    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Data_GetValue(__in const std::string &strJson,
                                        __out std::vector<BACKUP_FILE_INFO_ST> &vecData,
                                        __out int &nRowCount,
                                        __out int &nAllCount)
{
    if (strJson.empty())
    {
        return FALSE;
    }
    
    Json::Value root;
	Json::Value root_1;
    Json::Reader reader;
    std::string  strValue = strJson;

    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root_1) || !root_1.isArray() || root_1.size() < 1)
    {
        return FALSE;
    }

	root = root_1[0];

    // 获取总行数
    if (root.isMember("RowCount") && root["RowCount"].isNumeric())
    {
        nRowCount = root["RowCount"].asInt();
    }

    // 获取隔离区总条目数
    if (root.isMember("AllCount") && root["AllCount"].isNumeric())
    {
        nAllCount = root["AllCount"].asInt();
    }

    // 获取符合项
    if (root.isMember("Items") && root["Items"].isArray())
    {

        const Json::Value& items = root["Items"];
		//wwdv2
		if (items.isArray())
		{
			for(unsigned int i = 0;i < items.size();i++)
			{
				const Json::Value& item = items[i];
				BACKUP_FILE_INFO_ST  data;
				if(item.isMember("uuid") && item["uuid"].isString())
				{
					data.uuid = CStrUtil::UTF8ToUnicode(item["uuid"].asString());
				}

				if(item.isMember("file_name") && item["file_name"].isString())
				{
					data.wstrFileName = CStrUtil::UTF8ToUnicode(item["file_name"].asString());
				}

				if(item.isMember("process_name") && item["process_name"].isString())
				{
					data.wstrProcessName = CStrUtil::UTF8ToUnicode(item["process_name"].asString());
				}

				if(item.isMember("file_size") && item["file_size"].isNumeric())
				{
					data.llFileSize = item["file_size"].asInt64();
				}

				if(item.isMember("update_time")&& item["update_time"].isNumeric())
				{
					data.llUpdateTime = item["update_time"].asInt64();
				}

				if(item.isMember("backup_time") && item["backup_time"].isNumeric())
				{
					data.llBackupTime = item["backup_time"].asInt64();

				}

				if(item.isMember("action") && item["action"].isNumeric())
				{
					data.nAction = item["action"].asInt();
				}

				vecData.push_back(data);
			}
		}
        
	}
    
    return TRUE;
}

std::string CWLJsonParse::Backup_Result_GetJson(__in int nType,
                                                             __in int nResult, 
                                                             __in int nSuccNum,
                                                             __in int nFailNum)
{
    Json::Value root;
    Json::FastWriter writer;
    std::string sJsonPacket;

    root["Type"] = nType;
    root["Result"] = nResult;
    root["SuccNum"] = nSuccNum;
    root["FailNum"] = nFailNum;
    sJsonPacket = writer.write(root);

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Result_GetValue(__in const std::string &strJson,
                                                       __out int &nType,
                                                       __out int &nResult,
                                                       __out int &nSuccNum,
                                                       __out int &nFailNum)
{
    Json::Value root;
    Json::Reader reader;

    if (strJson.empty())
    {
        return FALSE;
    }

    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }

    if (root.isMember("Result") && root["Result"].isNumeric())
    {
        nResult = root["Result"].asInt();	
    }

    if (root.isMember("Type") && root["Type"].isNumeric())
    {
        nType  = root["Type"].asInt();
    }

    if (root.isMember("SuccNum") && root["SuccNum"].isNumeric())
    {
        nSuccNum = root["SuccNum"].asInt();	
    }

    if (root.isMember("FailNum") && root["FailNum"].isNumeric())
    {
        nFailNum  = root["FailNum"].asInt();
    }

    return TRUE;
}

std::string CWLJsonParse::Result_BackupOperation(__in tstring ComputerID , __in WORD cmdType, __in WORD cmdID, __in BOOL Result)
{
    std::string sJsonPacket = "";

    Json::Value root;
    Json::FastWriter writer;
    Json::Value person, CMDContent;

    person["ComputerID"]= UnicodeToUTF8(ComputerID);
    person["CMDTYPE"] = (int)cmdType;
    if (cmdID != PLY_CLIENT_BACKUP_MANAGER_SAVE_CONFIG &&
        cmdID != PLY_USM_BACKUP_MANAGER_DELETE &&
        cmdID != PLY_USM_BACKUP_MANAGER_RESTORE)
    {
        sJsonPacket = "";
        return sJsonPacket;
    }
    person["CMDID"] = (int)cmdID;

    if (cmdID != PLY_CLIENT_BACKUP_MANAGER_SAVE_CONFIG)
    {
        if (0 == Result)
        {
            CMDContent["RESULT"] = "SUC";
        }
        else
        {
            CMDContent["RESULT"] = "FAIL";
        }
    }
    else
    {
        if (0 == Result)
        {
            CMDContent["RESULT"] = "SUC";
        }
        else
        {
            CMDContent["RESULT"] = "FAIL";
        }

        if (0 != Result)
        {
            CMDContent["description"] = UnicodeToUTF8((GetErrorInfoById(Result)));
        }     
    }
    person["CMDContent"] = (Json::Value)CMDContent;

    root.append(person);
    sJsonPacket = writer.write(root);
    root.clear();

    return sJsonPacket;
}

std::string CWLJsonParse::Backup_Result_GetJson(__in int nResult, __in int nSuccNum, __in int nFailNum)
{
    Json::Value root;
    Json::FastWriter writer;
    std::string sJsonPacket;

    root["Result"] = nResult;
    root["SuccNum"] = nSuccNum;
    root["FailNum"] = nFailNum;
    sJsonPacket = writer.write(root);

    return sJsonPacket;
}

BOOL CWLJsonParse::Backup_Result_GetValue(__in const std::string &strJson,
                                                       __out int &nResult,
                                                       __out int &nSuccNum,
                                                       __out int &nFailNum)
{
    Json::Value root;
    Json::Reader reader;

    if (strJson.empty())
    {
        return FALSE;
    }

    if (!reader.parse(strJson, root))
    {
        return FALSE;
    }

    if (root.isMember("Result") && root["Result"].isNumeric())
    {
        nResult = root["Result"].asInt();	
    }

    if (root.isMember("SuccNum") && root["SuccNum"].isNumeric())
    {
        nSuccNum = root["SuccNum"].asInt();	
    }

    if (root.isMember("FailNum") && root["FailNum"].isNumeric())
    {
        nFailNum  = root["FailNum"].asInt();
    }

    return TRUE;
}
/*
* @fn           UserMgmt_PasswordRule_GetValue
* @brief        解析json格式的密码复杂度设置，存储到结构体ST_BASIC_CONFIG中
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.25 create it.
*/
BOOL CWLJsonParse::UserMgmt_PasswordRule_ParseValue(__in const string &strJson, __out ST_BASIC_CONFIG &BaseConfigStruct, __out tstring *pStrError/* = NULL */)
{
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	BaseConfigStruct.iUserTimeout = (int)root[0]["CMDContent"]["UserTimeOut"]["Time"].asInt();
	BaseConfigStruct.iPwdMinLen = (int)root[0]["CMDContent"]["UserPasswdRules"]["MinLen"].asInt();
	BaseConfigStruct.iPwdMinD = (int)root[0]["CMDContent"]["UserPasswdRules"]["MinD"].asInt();
	BaseConfigStruct.iPwdMinU = (int)root[0]["CMDContent"]["UserPasswdRules"]["MinU"].asInt();
	BaseConfigStruct.iPwdMinL = (int)root[0]["CMDContent"]["UserPasswdRules"]["MinL"].asInt();
	BaseConfigStruct.iPwdMinO = (int)root[0]["CMDContent"]["UserPasswdRules"]["MinO"].asInt();

	bRes = TRUE;

END:
	if (pStrError)
	{
		*pStrError = wosError.str();
	}

	return bRes;
}

/*
* @fn           GetCMDContentJson：密码复杂度模块使用
* @brief        处理USM发过来的json，去除冗余，返回截取出的新json 
* @param[in]    inputJson: USM发送的json
* @param[out]	去除冗余后的json
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.28 create it.
*/
std::string CWLJsonParse::GetCMDContentJson(const string& inputJson)
{
	Json::Value root;
	Json::Reader reader;

	if (!reader.parse(inputJson, root))
	{
		std::cerr << "Failed to parse input JSON" << std::endl;
		return "";
	}

	// 判断是否存在 CMDContent 字段
	if (root.isMember("CMDContent"))
	{
		Json::Value subJson;
		subJson["CMDContent"] = root["CMDContent"];
		Json::FastWriter writer;
		return writer.write(subJson);
	}
	else
	{
		std::cerr << "CMDContent field not found" << std::endl;
		return "";
	}
}

/*
* @fn			USMIP_Config_GetJson
* @brief        ChangeUSMIP模块使用，将更换的USM和日志服务器的IP，Port和UserName组合成Json
* @param[in]    USMIP_CONFIG config  BOOL bProxy 是否启用代理模式
* @return       std::string json
*               
* @author       zhicheng.sun
* @modify：		2023.8.29 create it.
*/
std::string CWLJsonParse::USMIP_Config_GetJson(const USMIP_CONFIG config, EM_CHANGET_TYPE eChangeType, BOOL bProxy)
{
	/*
	"CMDContent":
	{
		"USMIP":"192.168.xxx.xxx",
		"USMPort":xxx,
		"LogServerIP":"192.168.xxx.xxx",
		"LogServerPort":xxx,
		"UserName":"xxx"
	}
	*/

	Json::Value root;
	Json::Value Content;
	Json::FastWriter writer;

	Content["usmIp"] = UnicodeToUTF8(config.wstrUSMIP);
	Content["usmPort"] = (int)config.dwUSMPort;
	Content["logIp"] = UnicodeToUTF8(config.wstrLogServerIP);
	Content["logPort"] = (int)config.dwLogServerPort;
	Content["Username"] = UnicodeToUTF8(config.wstrUserName);
	Content["ChangeType"] = (int)eChangeType;
	Content["Proxy"] = bProxy ? PROXY_STATUS_SERVICE : PROXY_STATUS_NONE;

	root["CMDContent"] = Content;

	writer.omitEndingLineFeed();
	std::string json = writer.write(root);
	root.clear();

    return json;
}

/*
* @fn           USMIP_Config_ParseJson
* @brief        ChangeUSMIP模块使用，解析Json，将更换的USM和日志服务器的IP，Port和UserName解析到结构体USMIP_CONFIG中
* @param[in]    std::string strJson
* @return       USMIP_CONFIG &config 
*               
* @author       zhicheng.sun
* @modify：		2023.8.29 create it.
*/
BOOL CWLJsonParse::USMIP_Config_ParseJson(std::string strJson, USMIP_CONFIG &config, BOOL &bProxy, __out tstring *pStrError/* = NULL */)
{
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = strJson;

	string strTmp = "";
	wstring wstrTmp = _T("");
	int iTmp = 0;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	strTmp = root[0]["CMDContent"]["usmIp"].asString();
	wstrTmp = UTF8ToUnicode(strTmp);
	_tcsncpy_s(config.wstrUSMIP, _countof(config.wstrUSMIP), wstrTmp.c_str(), wstrTmp.length());

	config.dwUSMPort = (int)root[0]["CMDContent"]["usmPort"].asInt();

	strTmp = root[0]["CMDContent"]["logIp"].asString();
	wstrTmp = UTF8ToUnicode(strTmp);
	_tcsncpy_s(config.wstrLogServerIP, _countof(config.wstrLogServerIP), wstrTmp.c_str(), wstrTmp.length());

	config.dwLogServerPort = (int)root[0]["CMDContent"]["logPort"].asInt();

	strTmp = root[0]["CMDContent"]["Username"].asString();
	wstrTmp = UTF8ToUnicode(strTmp);
	_tcsncpy_s(config.wstrUserName, _countof(config.wstrUserName), wstrTmp.c_str(), wstrTmp.length());

	if(!root[0]["CMDContent"].isMember("ChangeType"))
	{
		// 说明是USM传来的，后续通过传递的日志服务器信息是否为空，来判断eChangeType
	}
	else
	{
		config.eChangeType = (EM_CHANGET_TYPE)root[0]["CMDContent"]["ChangeType"].asInt();
	}
	
	iTmp = root[0]["CMDContent"]["Proxy"].asInt();
	bProxy = iTmp == PROXY_STATUS_SERVICE ? TRUE : FALSE;

	bRes = TRUE;

END:
	if (pStrError != NULL)
	{
		*pStrError = wosError.str();
	}

	return bRes;
}

/*
* @fn			USM_LogServer_ChangeRet_GetJson
* @brief        ChangeUSMIP模块服务端使用，返回客户端更改USM和LogServer的结果
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.30 create it.
*/
std::string CWLJsonParse::USM_LogServer_ChangeRet_GetJson(BOOL Ret, int Error, tstring wsError)
{
	/*
	"CMDContent":
	{
		"Ret":1,
		"Error":1
	}
	*/

	Json::Value root;
	Json::Value Content;
	Json::FastWriter writer;

	Content["Ret"] = (int)Ret;
	Content["Error"] = (int)Error;
	Content["ErrorStr"] = UnicodeToUTF8(wsError);

	root["CMDContent"] = Content;

	writer.omitEndingLineFeed();
	std::string json = writer.write(root);
	root.clear();

    return json;
}

/*
* @fn           USM_LogServer_ChangeRet_ParseJson
* @brief        ChangeUSMIP模块客户端使用，解析返回客户端更改USM和LogServer的结果
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.29 create it.
*/
BOOL CWLJsonParse::USM_LogServer_ChangeRet_ParseJson(std::string strJson, BOOL &Ret, int &Error, __out tstring &wsErrorStr, __out tstring *pStrError/* = NULL */)
{
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}
	
    if (!root[0].isMember("CMDContent"))
    {
        goto END;
    }
    
    if (root[0]["CMDContent"].isMember("Ret") && root[0]["CMDContent"]["Ret"].isInt())
    {
        Ret = (int)root[0]["CMDContent"]["Ret"].asInt();
    }
    
    if (root[0]["CMDContent"].isMember("Error") && root[0]["CMDContent"]["Error"].isInt())
    {
        Error = (int)root[0]["CMDContent"]["Error"].asInt();
    }
	
	if (root[0]["CMDContent"].isMember("ErrorStr") && root[0]["CMDContent"]["ErrorStr"].isString())
	{
	    std::string strTmp = root[0]["CMDContent"]["ErrorStr"].asString();
	    wsErrorStr = UTF8ToUnicode(strTmp);
	}

	bRes = TRUE;

END:
	if (pStrError)
	{
		*pStrError = wosError.str();
	}

	return bRes;
}

/*
* @fn           Ret_USM_USMChange_PasswordRule_GetJson
* @brief        组合Json，是要返回给USM，有关USM平台迁移的结果/或者密码复杂度配置的结果
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.31 create it.
*/
std::string CWLJsonParse::Ret_USM_USMChange_PasswordRule_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, wstring strDealResult)
{
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value person, CMDContent;

	person["ComputerID"]= UnicodeToUTF8(ComputerID);
	person["CMDTYPE"] = (int)cmdType;
	person["CMDID"] = (int)cmdID;

	if(strDealResult.length() == 0)
	{
		CMDContent["RESULT"] = "SUC";
		CMDContent["description"] = "";
	}
	else
	{
		CMDContent["RESULT"] = "FAIL";
		CMDContent["description"] = UnicodeToUTF8(strDealResult);
	}

	person["CMDContent"] = (Json::Value)CMDContent;

	root.append(person);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

//【主机卫士用户添加、修改（用户名、密码、权限）、重置（密码）和删除用户使用】
std::string CWLJsonParse::UserAdd_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{ 
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);
	CMDContentItem["Password"] = UnicodeToUTF8(stUserInfo.szUserPwd);
	CMDContentItem["RoleID"] = (int)stUserInfo.dwRoleID;
	CMDContentItem["IsDel"] = (int)stUserInfo.dwIsDel;
	CMDContentItem["MaxErrorNum"] = (int)stUserInfo.llMaxErrorNum;
	CMDContentItem["Authorize"] = (UINT64)stUserInfo.dwAuthorize; 

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UserAdd_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);
    
    //密码
	if (!CMDContent.isMember("Password") || CMDContent["Password"].isNull())
	{
		wosError << _T("invalid json, no member : Password");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Password"].asString());
	_tcsncpy_s(stUserInfo.szUserPwd, _countof(stUserInfo.szUserPwd), wsItem.c_str(), _countof(stUserInfo.szUserPwd) - 1);

	//角色
	if (!CMDContent.isMember("RoleID") || CMDContent["RoleID"].isNull())
	{
		wosError << _T("invalid json, no member : RoleID");
		goto END;
	}
	stUserInfo.dwRoleID = CMDContent["RoleID"].asInt();
	
	//保留用户
	if (!CMDContent.isMember("IsDel") || CMDContent["IsDel"].isNull())
	{
		wosError << _T("invalid json, no member : IsDel");
		goto END;
	}
	stUserInfo.dwIsDel = CMDContent["IsDel"].asInt();
	
	//最大错误次数
	if (!CMDContent.isMember("MaxErrorNum") || CMDContent["MaxErrorNum"].isNull())
	{
		wosError << _T("invalid json, no member : MaxErrorNum");
		goto END;
	}
	stUserInfo.llMaxErrorNum = CMDContent["MaxErrorNum"].asInt();

	//授权
	if (!CMDContent.isMember("Authorize") || CMDContent["Authorize"].isNull())
	{
		wosError << _T("invalid json, no member : MaxErrorNum");
		goto END;
	}
	stUserInfo.dwAuthorize = (DWORD)CMDContent["Authorize"].asUInt64();


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::UserDel_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UserDel_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);
    
	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::UserEditPwd_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);
	CMDContentItem["Password"] = UnicodeToUTF8(stUserInfo.szUserPwd);
	CMDContentItem["NewPassword"] = UnicodeToUTF8(stUserInfo.szUserNewPwd);

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UserEditPwd_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);
    
    //密码
	if (!CMDContent.isMember("Password") || CMDContent["Password"].isNull())
	{
		wosError << _T("invalid json, no member : Password");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Password"].asString());
	_tcsncpy_s(stUserInfo.szUserPwd, _countof(stUserInfo.szUserPwd), wsItem.c_str(), _countof(stUserInfo.szUserPwd) - 1);

	//新密码
	if (!CMDContent.isMember("NewPassword") || CMDContent["NewPassword"].isNull())
	{
		wosError << _T("invalid json, no member : NewPassword");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["NewPassword"].asString());
	_tcsncpy_s(stUserInfo.szUserNewPwd, _countof(stUserInfo.szUserNewPwd), wsItem.c_str(), _countof(stUserInfo.szUserNewPwd) - 1);

	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::UserEditAuthorize_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);
	CMDContentItem["Authorize"] = (UINT64)stUserInfo.dwAuthorize; 

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UserEditAuthorize_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);
    
	//授权
	if (!CMDContent.isMember("Authorize") || CMDContent["Authorize"].isNull())
	{
		wosError << _T("invalid json, no member : MaxErrorNum");
		goto END;
	}
	stUserInfo.dwAuthorize = (DWORD)CMDContent["Authorize"].asUInt64();


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::UserEditUserName_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);
	CMDContentItem["NewName"] = UnicodeToUTF8(stUserInfo.szUserNewName);

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UserEditUserName_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);

	//新用户名
	if (!CMDContent.isMember("NewName") || CMDContent["NewName"].isNull())
	{
		wosError << _T("invalid json, no member : NewName");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["NewName"].asString());
	_tcsncpy_s(stUserInfo.szUserNewName, _countof(stUserInfo.szUserNewName), wsItem.c_str(), _countof(stUserInfo.szUserNewName) - 1);

	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}


std::string CWLJsonParse::UpdateMaxErrorNum_GetJson(__in ST_WL_USER_INFO stUserInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["Name"] = UnicodeToUTF8(stUserInfo.szUserName);
	CMDContentItem["MaxErrorNum"] = (LONGLONG)stUserInfo.llMaxErrorNum;
	CMDContentItem["LockStartTime"] = (LONGLONG)stUserInfo.llLockStartTime;
	CMDContentItem["IsLockStartTime"] = (int)stUserInfo.bIsLockStartTime; 

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::UpdateMaxErrorNum_ParseJson(__in string strJson, __out ST_WL_USER_INFO &stUserInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//用户名
	if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
	{
		wosError << _T("invalid json, no member : Name");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
	_tcsncpy_s(stUserInfo.szUserName, _countof(stUserInfo.szUserName), wsItem.c_str(), _countof(stUserInfo.szUserName) - 1);
	
	//最大错误次数
	if (!CMDContent.isMember("LockStartTime") || CMDContent["LockStartTime"].isNull())
	{
		wosError << _T("invalid json, no member : LockStartTime");
		goto END;
	}
	stUserInfo.llLockStartTime = CMDContent["LockStartTime"].asInt();

	//锁定时间
	if (!CMDContent.isMember("MaxErrorNum") || CMDContent["MaxErrorNum"].isNull())
	{
		wosError << _T("invalid json, no member : MaxErrorNum");
		goto END;
	}
	stUserInfo.llMaxErrorNum = CMDContent["MaxErrorNum"].asInt();

	//是否锁定
	if (!CMDContent.isMember("IsLockStartTime") || CMDContent["IsLockStartTime"].isNull())
	{
		wosError << _T("invalid json, no member : IsLockStartTime");
		goto END;
	}
	stUserInfo.bIsLockStartTime = ( (int)CMDContent["IsLockStartTime"].asInt() == 0) ? FALSE : TRUE;


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}


std::string CWLJsonParse::DBClearConfig_GetJson(__in ST_WL_DB_CONFIG stDBClearConfig, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["DBIsAutoDelete"] = (int)stDBClearConfig.iDBIsAutoDelete;
	CMDContentItem["DBSize"] = (int)stDBClearConfig.iDBsize;
	CMDContentItem["DBDays"] = (int)stDBClearConfig.iDBDays;

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::DBClearConfig_ParseJson(__in string strJson, __out ST_WL_DB_CONFIG &stDBClearConfig, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//
	if (!CMDContent.isMember("DBIsAutoDelete") || CMDContent["DBIsAutoDelete"].isNull())
	{
		wosError << _T("invalid json, no member : DBIsAutoDelete");
		goto END;
	}
	stDBClearConfig.iDBIsAutoDelete = CMDContent["DBIsAutoDelete"].asInt();
	
	//
	if (!CMDContent.isMember("DBSize") || CMDContent["DBSize"].isNull())
	{
		wosError << _T("invalid json, no member : DBSize");
		goto END;
	}
	stDBClearConfig.iDBsize = CMDContent["DBSize"].asInt();

	//
	if (!CMDContent.isMember("DBDays") || CMDContent["DBDays"].isNull())
	{
		wosError << _T("invalid json, no member : DBDays");
		goto END;
	}
	stDBClearConfig.iDBDays = CMDContent["DBDays"].asInt();


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}


std::string CWLJsonParse::DBBackUp_GetJson(__in ST_WL_DB_BACKUP stDBBackUpInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["SrcBackDbUpPath"] = UnicodeToUTF8(stDBBackUpInfo.szSrc);
	CMDContentItem["DstBackDbUpPath"] = UnicodeToUTF8(stDBBackUpInfo.szDst);
	CMDContentItem["BackUpTimePoint"] = (LONGLONG)stDBBackUpInfo.llBackUpTime;
	CMDContentItem["DeleteBackUpData"] = (int)stDBBackUpInfo.bDeleteBackUpData;

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);
    root.clear();

	return strJson;
}

BOOL CWLJsonParse::DBBackUp_ParseJson(__in string strJson, __out ST_WL_DB_BACKUP &stDBBackUpInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//原备份路径
	if (!CMDContent.isMember("SrcBackDbUpPath") || CMDContent["SrcBackDbUpPath"].isNull())
	{
		wosError << _T("invalid json, no member : SrcBackDbUpPath");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["SrcBackDbUpPath"].asString());
	_tcsncpy_s(stDBBackUpInfo.szSrc, _countof(stDBBackUpInfo.szSrc), wsItem.c_str(), _countof(stDBBackUpInfo.szSrc) - 1);

	//目的路径
	if (!CMDContent.isMember("DstBackDbUpPath") || CMDContent["DstBackDbUpPath"].isNull())
	{
		wosError << _T("invalid json, no member : DstBackDbUpPath");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["DstBackDbUpPath"].asString());
	_tcsncpy_s(stDBBackUpInfo.szDst, _countof(stDBBackUpInfo.szDst), wsItem.c_str(), _countof(stDBBackUpInfo.szDst) - 1);

	//备份时间
	if (!CMDContent.isMember("BackUpTimePoint") || CMDContent["BackUpTimePoint"].isNull())
	{
		wosError << _T("invalid json, no member : BackUpTimePoint");
		goto END;
	}
	stDBBackUpInfo.llBackUpTime = CMDContent["BackUpTimePoint"].asInt();

	//删除备份数据
	if (!CMDContent.isMember("DeleteBackUpData") || CMDContent["DeleteBackUpData"].isNull())
	{
		wosError << _T("invalid json, no member : DeleteBackUpData");
		goto END;
	}
	stDBBackUpInfo.bDeleteBackUpData = ( CMDContent["DeleteBackUpData"].asInt() == 0) ? FALSE : TRUE;

	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::DBExportLog_GetJson(__in ST_COMPREHENSIVE_EXPORT_LOG stCophsExpLog, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;    
    Item["LogType"] = (int)stCophsExpLog.dwLogType;
    Item["ExpDir"] = UnicodeToUTF8(stCophsExpLog.stCommonInfo.szExpDir);
    Item["Rows"] = (int)stCophsExpLog.stCommonInfo.dwExpOnePageRows;

    switch (stCophsExpLog.dwLogType)
    {
    case WL_LOG_PAGE_WARNING:
        {
            const CWarningLog *pCWarningLog = stCophsExpLog.GetWarningLogStruct();

            CMDContentItem["HoldBackStyle"] = (int)pCWarningLog->m_nExportLogWarningHoldBackStyle;
            CMDContentItem["Total"]         = (int)pCWarningLog->m_nExportLogWarningItemTotal;
            CMDContentItem["Start"]         = (LONGLONG)pCWarningLog->m_llExportLogWarningStartTime;
            CMDContentItem["End"]           = (LONGLONG)pCWarningLog->m_llExportLogWarningEndTime;
            CMDContentItem["Include"]       = UnicodeToUTF8(pCWarningLog->m_wsExportLogWarningIncludeName);
        }
        break;

    case WL_LOG_PAGE_SAFETYSTORE:
        {
            const CSafetyStoreLog *pCSafetyStoreLog = stCophsExpLog.GetSafetyStoreLogStruct();

            CMDContentItem["Style"]   = (int)pCSafetyStoreLog->m_nExportLogSafetyStoreStyle;
            CMDContentItem["Total"]   = (int)pCSafetyStoreLog->m_nExportLogSafetyStoreItemTotal;
            CMDContentItem["Start"]   = (LONGLONG)pCSafetyStoreLog->m_llExportLogSafetyStoreStartTime;
            CMDContentItem["End"]     = (LONGLONG)pCSafetyStoreLog->m_llExportLogSafetyStoreEndTime;
            CMDContentItem["Include"] = UnicodeToUTF8(pCSafetyStoreLog->m_wsExportLogSafetyStoreIncludeName);
        }
        break;
  
    case WL_LOG_PAGE_VIRUSLOG:
        {
            const CVirusLog *pCVirusLog = stCophsExpLog.GetVirusLogStruct();

            CMDContentItem["Result"] = UnicodeToUTF8(pCVirusLog->m_wsExportLogVirusResult);
            CMDContentItem["Start"]  = (LONGLONG)pCVirusLog->m_llExportLogVirusStartTime;
            CMDContentItem["End"]    = (LONGLONG)pCVirusLog->m_llExportLogVirusEndTime;
            CMDContentItem["Total"]  = (int)pCVirusLog->m_nExportLogVirusItemTotal;
        }
        break;
    
    case WL_LOG_PAGE_USB:
        {
            const CUSBLog *pCUSBLog = stCophsExpLog.GetUSBLogStruct();

            CMDContentItem["Start"] = (LONGLONG)pCUSBLog->m_llExportLogUsbStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCUSBLog->m_llExportLogUsbEndTime;
            CMDContentItem["Style"] = (int)pCUSBLog->m_nExportLogUsbStyle;
            CMDContentItem["Total"] = (int)pCUSBLog->m_nExportLogUsbItemTotal;
        }
        break;

    case WL_LOG_PAGE_VUL:
        {
            const CVulLog *pCVulLog = stCophsExpLog.GetVulLogStruct();

            CMDContentItem["Start"]   = (LONGLONG)pCVulLog->m_llExportLogVulStartTime;
            CMDContentItem["End"]     = (LONGLONG)pCVulLog->m_llExportLogVulEndTime;
            CMDContentItem["Type"]    = (int)pCVulLog->m_nExportLogVulType;
            CMDContentItem["Total"]   = (int)pCVulLog->m_nExportLogVulItemTotal;
            CMDContentItem["Include"] = UnicodeToUTF8(pCVulLog->m_wsExportLogVulIncludeIP);
        }
        break;

    case WL_LOG_PAGE_ILLEGAL:
        {
            const CIllegalLog *pCIllegalLog = stCophsExpLog.GetIllegalLogStruct();

            CMDContentItem["Total"] = (int)pCIllegalLog->m_nExportLogIllegalItemTotal;
            CMDContentItem["Start"] = UnicodeToUTF8(pCIllegalLog->m_strExportLogIllegalStartTime.GetString());
            CMDContentItem["End"]   = UnicodeToUTF8(pCIllegalLog->m_strExportLogIllegalEndTime.GetString());
        }
        break;

    case WL_LOG_PAGE_FIREWALL:
        {
            const CFireWallLog *pCFireWallLog = stCophsExpLog.GetFirewallLogStruct();

            CMDContentItem["Total"] = (int)pCFireWallLog->m_nExportLogFireWallItemTotal;
            CMDContentItem["Start"] = (LONGLONG)pCFireWallLog->m_llExportLogFireWallStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCFireWallLog->m_llExportLogFireWallEndTime;
            CMDContentItem["Key"]   = UnicodeToUTF8(pCFireWallLog->m_wsExportLogFireWallKey);
        }
        break;

    case WL_LOG_PAGE_HOSTDEFENCE:
        {
            const CHostDefenceLog *pCHostDefenceLog = stCophsExpLog.GetHostDefenceLogStruct();

            CMDContentItem["Start"]   = (LONGLONG)pCHostDefenceLog->m_llExportLogHostDefenceStartTime;
            CMDContentItem["End"]     = (LONGLONG)pCHostDefenceLog->m_llExportLogHostDefenceEndTime;
            CMDContentItem["Style"]   = (int)pCHostDefenceLog->m_nExportLogHostDefenceStyle;
            CMDContentItem["Total"]   = (int)pCHostDefenceLog->m_nExportLogHostDefenceItemTotal;
            CMDContentItem["Include"] = UnicodeToUTF8(pCHostDefenceLog->m_wsExportLogHostDefenceIncludeName);
        }
        break;

    case WL_LOG_PAGE_THREATFAKE:
        {
            const CThreatFakeLog *pCThreatFakeLog = stCophsExpLog.GetThreatFakeLogStruct();

            CMDContentItem["Start"]   = (LONGLONG)pCThreatFakeLog->m_llExportLogThreatStartTime;
            CMDContentItem["End"]     = (LONGLONG)pCThreatFakeLog->m_llExportLogThreatEndTime;
            CMDContentItem["Total"]   = (int)pCThreatFakeLog->m_nExportLogThreatItemTotal;
        }
        break;          
            
    case WL_LOG_PAGE_USER:
        {
            const CUserLog *pCUserLog = stCophsExpLog.GetUserLogStruct();

            CMDContentItem["Start"] = (LONGLONG)pCUserLog->m_llExportLogUserActionStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCUserLog->m_llExportLogUserActionEndTime;
            CMDContentItem["Total"] = (int)pCUserLog->m_nExportLogUserActionItemTotal;
            CMDContentItem["Name"]  = UnicodeToUTF8(pCUserLog->m_wsExportLogUserActionUserName);
        }
        break;

    case WL_LOG_PAGE_OSRESOURCELOG:
        {
            const COSResourceLog *pCOSResourceLog = stCophsExpLog.GetOSResourceLogStruct();

            CMDContentItem["Start"] = (LONGLONG)pCOSResourceLog->m_llExportLogOSResourceStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCOSResourceLog->m_llExportLogOSResourceEndTime;
            CMDContentItem["Total"] = (int)pCOSResourceLog->m_nExportLogOSResourceItemTotal;
            CMDContentItem["Type"]  = (int)pCOSResourceLog->m_nExportLogOSResourceType;
        }
        break;

    case WL_LOG_PAGE_SYSTEMGUARD:
        {
            const CSystemGuardLog *pCSystemGuardLog = stCophsExpLog.GetSystemGuardLogStruct();

            CMDContentItem["Start"] = (LONGLONG)pCSystemGuardLog->m_llExportLogSysGuardStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCSystemGuardLog->m_llExportLogSysGuardEndTime;
            CMDContentItem["Total"] = (int)pCSystemGuardLog->m_nExportLogSysGuardItemTotal;
        }
        break;

    case WL_LOG_PAGE_DATAPROTECT:
        {
            const CDataProtectLog *pCDataProtectLog = stCophsExpLog.GetDataProtectLogStruct();

            CMDContentItem["Start"] = (LONGLONG)pCDataProtectLog->m_llExportLogDataProtectStartTime;
            CMDContentItem["End"]   = (LONGLONG)pCDataProtectLog->m_llExportLogDataProtectEndTime;
            CMDContentItem["Total"] = (int)pCDataProtectLog->m_nExportLogDataProtectItemTotal;
        }
        break;

    default:
        break;
    }

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);
    root.clear();

    return strJson;
}

BOOL CWLJsonParse::DBExportLog_ParseJson(__in string strJson, __out ST_COMPREHENSIVE_EXPORT_LOG &stCophsExpLog, __out tstring &wsError)
{
    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if (strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    // 导出日志类型
    if (!root[0].isMember("LogType"))
    {
        wosError << _T("invalid json, no member : LogType");
        goto END;
    }
    stCophsExpLog.dwLogType = (DWORD)root[0]["LogType"].asInt();

    // 每次执行导出的数量
    if (!root[0].isMember("Rows"))
    {
        wosError << _T("invalid json, no member : Rows");
        goto END;
    }
    stCophsExpLog.stCommonInfo.dwExpOnePageRows = (DWORD)root[0]["Rows"].asInt();

    // 执行导出的路径（目录）
    if (!root[0].isMember("ExpDir") || root[0]["ExpDir"].isNull())
    {
        wosError << _T("invalid json, no member : ExpDir");
        goto END;
    }
    wsItem = UTF8ToUnicode(root[0]["ExpDir"].asString());
    stCophsExpLog.stCommonInfo.Copy(stCophsExpLog.stCommonInfo.szExpDir, _countof(stCophsExpLog.stCommonInfo.szExpDir), wsItem.c_str());


    // 导出日志需要的具体配置
    if (!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    switch (stCophsExpLog.dwLogType)
    {
    case WL_LOG_PAGE_WARNING:
        {
            if (!CMDContent.isMember("Include") || CMDContent["Include"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_WARNING, no member : Include");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Include"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nHoldBackStyle = CMDContent["HoldBackStyle"].asInt();

            stCophsExpLog.SetWarningLogStruct(nHoldBackStyle, nTotal, wsItem, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_SAFETYSTORE:
        {
            if (!CMDContent.isMember("Include") || CMDContent["Include"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_SAFETYSTORE, no member : Include");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Include"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nStyle = CMDContent["Style"].asInt();

            stCophsExpLog.SetSafetyStoreLogStruct(llStartTime, llEndTime, nStyle, nTotal, wsItem);
        }
        break;

    case WL_LOG_PAGE_VIRUSLOG:
        {
            if (!CMDContent.isMember("Result") || CMDContent["Result"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_VIRUSLOG, no member : Result");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Result"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            
            stCophsExpLog.SetVirusLogStruct(llStartTime, llEndTime, nTotal, wsItem);
        }
        break;

    case WL_LOG_PAGE_USB:
        {
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nStyle = CMDContent["Style"].asInt();

            stCophsExpLog.SetUSBLogStruct(nStyle, nTotal, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_VUL:
        {
            if (!CMDContent.isMember("Include") || CMDContent["Include"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_VUL, no member : Include");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Include"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nType = CMDContent["Type"].asInt();

            stCophsExpLog.SetVulLogStruct(llStartTime, llEndTime, nTotal, nType, wsItem);
        }
        break;

    case WL_LOG_PAGE_ILLEGAL:
        {
            if (!CMDContent.isMember("Start") || CMDContent["Start"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_ILLEGAL, no member : Start");
                goto END;
            }
            std::wstring wsStart = UTF8ToUnicode(CMDContent["Start"].asString());

            if (!CMDContent.isMember("End") || CMDContent["End"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_ILLEGAL, no member : End");
                goto END;
            }
            std::wstring wsEnd = UTF8ToUnicode(CMDContent["End"].asString());

            int nTotal = (int)CMDContent["Total"].asInt();

            stCophsExpLog.SetIllegalLogStruct(wsStart.c_str(), wsEnd.c_str(), nTotal);
        }
        break;

    case WL_LOG_PAGE_FIREWALL:
        {
            if (!CMDContent.isMember("Key") || CMDContent["Key"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_FIREWALL, no member : Key");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Key"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();

            stCophsExpLog.SetFirewallLogStruct(nTotal, wsItem, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_HOSTDEFENCE:
        {
            if (!CMDContent.isMember("Include") || CMDContent["Include"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_HOSTDEFENCE, no member : Include");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Include"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nStyle = CMDContent["Style"].asInt();

            stCophsExpLog.SetHostDefenceLogStruct(nStyle, nTotal, wsItem, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_THREATFAKE:
        {
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();

            stCophsExpLog.SetThreatFakeLogStruct(llStartTime, llEndTime, nTotal);
        }
        break; 

    case WL_LOG_PAGE_USER:
        {
            if (!CMDContent.isMember("Name") || CMDContent["Name"].isNull())
            {
                wosError << _T("invalid json, log type = WL_LOG_PAGE_USER, no member : Name");
                goto END;
            }
            wsItem = UTF8ToUnicode(CMDContent["Name"].asString());
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();

            stCophsExpLog.SetUserLogStruct(wsItem, nTotal, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_OSRESOURCELOG:
        {
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();
            int nType = CMDContent["Type"].asInt();

            stCophsExpLog.SetOSResourceLogStruct(nType, nTotal, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_SYSTEMGUARD:
        {
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();

            stCophsExpLog.SetSystemGuardLogStruct(nTotal, llStartTime, llEndTime);
        }
        break;

    case WL_LOG_PAGE_DATAPROTECT:
        {
            LONGLONG llStartTime = CMDContent["Start"].asUInt64();
            LONGLONG llEndTime = CMDContent["End"].asUInt64();
            int nTotal = CMDContent["Total"].asInt();

            stCophsExpLog.SetDataProtectLogStruct(nTotal, llStartTime, llEndTime);
        }
        break;

    default:
        break;
    }

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

/*
* @fn           OSResource_GetJson
* @brief        将用户设置的资源阈值组成Json
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023. create it.
*/
std::string CWLJsonParse::OSResource_GetJson(__in tstring ComputerID, __in WORD cmdType , __in WORD cmdID, PIEG_SYS_RESOURCE_BASE_CFG_ST pstOSResourceCfg)
{
	std::string strJson = "";

	Json::Value root;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;
	Json::Value person;

	person["CMDID"] = (int)cmdID;
	person["CMDTYPE"] = (int)cmdType;
	person["ComputerID"]= UnicodeToUTF8(ComputerID);

	CMDContentItem.append(pstOSResourceCfg->iEnable);
	CMDContentItem.append(pstOSResourceCfg->iCpuThreshold);
	CMDContentItem.append(pstOSResourceCfg->iRamThreshold);
	CMDContentItem.append(pstOSResourceCfg->iDskThresHold);
	person["CMDContent"] = CMDContentItem;

	strJson = jsWriter.write(person);

	return strJson;
}

/*
* @fn           OSResource_ParseJson
* @brief        解析json格式的系统资源阈值设置，存储到结构体_IEG_SYS_RESOURCE_BASE_CFG_ST中
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.9.5 create it.
*/
BOOL CWLJsonParse::OSResource_ParseJson(__in const string &strJson, __out IEG_SYS_RESOURCE_BASE_CFG_ST &OSResourceConfigStruct, __out tstring *pStrError/* = NULL */)
{
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	OSResourceConfigStruct.iEnable = (int)root[0]["CMDContent"][0].asInt();
	OSResourceConfigStruct.iCpuThreshold = (int)root[0]["CMDContent"][1].asInt();
	OSResourceConfigStruct.iRamThreshold = (int)root[0]["CMDContent"][2].asInt();
	OSResourceConfigStruct.iDskThresHold = (int)root[0]["CMDContent"][3].asInt();

	bRes = TRUE;

END:
	if (pStrError != NULL)
	{
		*pStrError = wosError.str();
	}

	return bRes;
}

//【本地更新授权文件】
std::string CWLJsonParse::LocalLcsUpdate_GetJson(__in ST_LOCAL_LCS_UPDATE stLocalLcsUpdate, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["NewPath"] = UnicodeToUTF8(stLocalLcsUpdate.szNewPath);
    CMDContentItem["NewName"] = UnicodeToUTF8(stLocalLcsUpdate.szNewLcsName);
    CMDContentItem["OldPath"] = UnicodeToUTF8(stLocalLcsUpdate.szOldPath);
    CMDContentItem["OldName"] = UnicodeToUTF8(stLocalLcsUpdate.szOldLcsName);

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);
    root.clear();

    return strJson;
}

BOOL CWLJsonParse::LocalLcsUpdate_ParseJson(__in std::string strJson, __out ST_LOCAL_LCS_UPDATE &stLocalLcsUpdate, __out tstring &wsError)
{
    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    //新授权文件路径
    if (!CMDContent.isMember("NewPath") || CMDContent["NewPath"].isNull())
    {
        wosError << _T("invalid json, no member : NewPath");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["NewPath"].asString());
    _tcsncpy_s(stLocalLcsUpdate.szNewPath, _countof(stLocalLcsUpdate.szNewPath), wsItem.c_str(), _countof(stLocalLcsUpdate.szNewPath) - 1);

    //新授权文件名称
    if (!CMDContent.isMember("NewName") || CMDContent["NewName"].isNull())
    {
        wosError << _T("invalid json, no member : NewName");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["NewName"].asString());
    _tcsncpy_s(stLocalLcsUpdate.szNewLcsName, _countof(stLocalLcsUpdate.szNewLcsName), wsItem.c_str(), _countof(stLocalLcsUpdate.szNewLcsName) - 1);
    
    //目前主机卫士正在使用授权文件的路径
    if (!CMDContent.isMember("OldPath") || CMDContent["OldPath"].isNull())
    {
        wosError << _T("invalid json, no member : OldPath");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["OldPath"].asString());
    _tcsncpy_s(stLocalLcsUpdate.szOldPath, _countof(stLocalLcsUpdate.szOldPath), wsItem.c_str(), _countof(stLocalLcsUpdate.szOldPath) - 1);

    //目前主机卫士正在使用授权文件的名称
    if (!CMDContent.isMember("OldName") || CMDContent["OldName"].isNull())
    {
        wosError << _T("invalid json, no member : OldName");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["OldName"].asString());
    _tcsncpy_s(stLocalLcsUpdate.szOldLcsName, _countof(stLocalLcsUpdate.szOldLcsName), wsItem.c_str(), _countof(stLocalLcsUpdate.szOldLcsName) - 1);

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

/*
* @fn           PasswordExpiration_GetJson
* @brief        密码超期限制配置json拼接
* @param[in]    stPwdExpiration ：配置
* @param[out]   
* @return       json
*               
* @detail      
* @author       yxd
* @date         2023-09-04
*/
std::string CWLJsonParse::PasswordExpiration_GetJson(__in PWD_EXPIRATION_CONFIG stPwdExpiration, __in const tstring tstrComputerID)
{
	std::string strJson;
	Json::Value root;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	root["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	root["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	root["CMDID"] = (int)PLY_PASSWORD_EXPIRATION_UPDATE;     /*360*/

	CMDContentItem["PasswordExpiration"]["Enable"] = (int)stPwdExpiration.dwEnable;
	CMDContentItem["PasswordExpiration"]["TimeLimit"] = (int)stPwdExpiration.dwTimeLimit;

	root["CMDContent"] = (Json::Value)CMDContentItem;

	strJson = jsWriter.write(root);

	return strJson;
}

/*
* @fn           PasswordExpiration_GetJson
* @brief        密码超期限制配置json解析
* @param[in]    strJson ：配置json
* @param[out]   stPwdExpiration ：配置
				wsError ：错误原因
* @return       TRUE ：成功  
				FALSE ：失败
*               
* @detail      
* @author       yxd
* @date         2023-09-04
*/
BOOL CWLJsonParse::PasswordExpiration_GetValue(__in string strJson, __out PWD_EXPIRATION_CONFIG &stPwdExpiration, __out tstring &wsError)
{
	BOOL bResult = FALSE;

	std::string strValue = "";
	std::wstring wsItem = _T("");

	Json::Reader reader;

	Json::Value  root;
	Json::Value  CMDContent;

	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}

	strValue = strJson;
	//补全 按数组解析
	if ( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if (!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];
	if (!CMDContent.isMember("PasswordExpiration"))
	{
		wosError << _T("invalid json, no member : PasswordExpiration");
		goto END;
	}

	//开关状态
	if (!CMDContent["PasswordExpiration"].isMember("Enable") || CMDContent["PasswordExpiration"]["Enable"].isNull())
	{
		wosError << _T("invalid json, no member : Enable");
		goto END;
	}
	stPwdExpiration.dwEnable = (DWORD)CMDContent["PasswordExpiration"]["Enable"].asInt();

	//超期时间
	if (!CMDContent["PasswordExpiration"].isMember("TimeLimit") || CMDContent["PasswordExpiration"]["TimeLimit"].isNull())
	{
		wosError << _T("invalid json, no member : Enable");
		goto END;
	}
	stPwdExpiration.dwTimeLimit = (DWORD)CMDContent["PasswordExpiration"]["TimeLimit"].asInt();
	
	bResult = TRUE;

END:
	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

/*
* @fn           OSResource_ConverFromUsm
* @brief        转换USM下发json格式
* @param[in]    src   源Json
				strJson 转换之后 本地使用
				strPostJson 转换之后 上报使用
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2023-
*/
BOOL CWLJsonParse::OSResource_ConverFromUsm(__in const string &src, __out string &strPostJson, __out string &strLocalJson, __out wstring &strError)
{
	// 平台下发策略与本地上报是两套json格式
	/*
	src
	{
	"ComputerID": "00:0c:29:a5:cc:70-6bfd05e5-7d85-407d-86b2-88dd74eb2c29-redhat7-5",
	"CMDTYPE": 150,
	"CMDID": 227,
	"DOMAIN": "",
	"CMDContent": {
	"OSResAlarmType": 1,
	"cpu": 21,
	"ram": 81,
	"disk": 81
	},
	"Username": null
	}
	*/

	/*
	strJson
	{"CMDContent": [1, 21, 81, 81]}
	*/

	BOOL bResult = FALSE;

	Json::FastWriter jsWriter;
	std::string strValue = "";
	std::wstring wsItem = _T("");

	Json::Reader reader;

	Json::Value  root;
	Json::Value  CMDContent;
	Json::Value  TempJson;
	Json::Value  Temproot;
	Json::Value  TempPostroot;

	wostringstream  wosError;

	if ( src.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}

	strValue = src;
	//补全 按数组解析
	if ( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if (!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	// 开关
	if (!CMDContent.isMember("OSResAlarmType") || CMDContent["OSResAlarmType"].isNull())
	{
		TempJson.append(0);
	}
	else
	{
		TempJson.append(CMDContent["OSResAlarmType"].asInt());
	}

	// CPU
	if (!CMDContent.isMember("cpu") || CMDContent["cpu"].isNull())
	{
		wosError << _T("invalid json, no member : cpu");
		goto END;
	}
	else
	{
		TempJson.append(CMDContent["cpu"].asInt());
	}

	// disk
	if (!CMDContent.isMember("ram") || CMDContent["ram"].isNull())
	{
		wosError << _T("invalid json, no member : ram");
		goto END;
	}
	else
	{
		TempJson.append(CMDContent["ram"].asInt());
	}

	// ram
	if (!CMDContent.isMember("disk") || CMDContent["disk"].isNull())
	{
		wosError << _T("invalid json, no member : disk");
		goto END;
	}
	else
	{
		TempJson.append(CMDContent["disk"].asInt());
	}

	Temproot["CMDContent"] = (Json::Value)TempJson;

	strLocalJson = jsWriter.write(Temproot);
	
	TempPostroot["CMDID"] = (int)PLY_SET_OSRESOURCE;
	TempPostroot["CMDTYPE"] = (int)150;
	TempPostroot["ComputerID"]= root[0]["ComputerID"];
	TempPostroot["CMDContent"]= (Json::Value)TempJson;
	strPostJson = jsWriter.write(TempPostroot);

	bResult = TRUE;

END:
	strError = wosError.str();
	return bResult;
}

/*
* @fn           PasswordRules_GetJson
* @brief        密码复杂度上报json组建
* @param[in]    
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2023-
*/
std::string CWLJsonParse::PasswordRules_GetJson(__in ST_BASIC_CONFIG stPwdRules, __in const tstring tstrComputerID)
{
	std::string strJson;
	Json::Value root;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	root["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	root["CMDTYPE"] = (int)CMDTYPE_POLICY; /*150*/
	root["CMDID"] = (int)PLY_CLIENT_BASIC_CONFIG;     /*1000*/

	CMDContentItem["UserTimeOut"]["Time"] = stPwdRules.iUserTimeout;
	CMDContentItem["UserPasswdRules"]["MinLen"] = stPwdRules.iPwdMinLen;
	CMDContentItem["UserPasswdRules"]["MinD"] = stPwdRules.iPwdMinD;
	CMDContentItem["UserPasswdRules"]["MinU"] = stPwdRules.iPwdMinU;
	CMDContentItem["UserPasswdRules"]["MinL"] = stPwdRules.iPwdMinL;
	CMDContentItem["UserPasswdRules"]["MinO"] = stPwdRules.iPwdMinO;

	root["CMDContent"] = (Json::Value)CMDContentItem;

	strJson = jsWriter.write(root);

	return strJson;
}

std::string CWLJsonParse::SecDetectFunc_GetJson(__in ST_WL_SECDETECT_WRITEPROFILE stWriteProfileInfo, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["AppName"] = UnicodeToUTF8(stWriteProfileInfo.szAppName);
	CMDContentItem["KeyName"] = UnicodeToUTF8(stWriteProfileInfo.szKeyName);
	CMDContentItem["String"] = UnicodeToUTF8(stWriteProfileInfo.szString);
	CMDContentItem["FileName"] = UnicodeToUTF8(stWriteProfileInfo.szFileName);
	CMDContentItem["FuncItem"] = (int)stWriteProfileInfo.dwFuncItem;

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);

	return strJson;
}

BOOL CWLJsonParse::SecDetectFunc_ParseJson(__in string strJson, __out ST_WL_SECDETECT_WRITEPROFILE &stWriteProfileInfo, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//AppName
	if (!CMDContent.isMember("AppName") || CMDContent["AppName"].isNull())
	{
		wosError << _T("invalid json, no member : AppName");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["AppName"].asString());
	_tcsncpy_s(stWriteProfileInfo.szAppName, _countof(stWriteProfileInfo.szAppName), wsItem.c_str(), _countof(stWriteProfileInfo.szAppName) - 1);
    
    //KeyName
	if (!CMDContent.isMember("KeyName") || CMDContent["KeyName"].isNull())
	{
		wosError << _T("invalid json, no member : KeyName");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["KeyName"].asString());
	_tcsncpy_s(stWriteProfileInfo.szKeyName, _countof(stWriteProfileInfo.szKeyName), wsItem.c_str(), _countof(stWriteProfileInfo.szKeyName) - 1);
    
	//String
	if (!CMDContent.isMember("String") || CMDContent["String"].isNull())
	{
		wosError << _T("invalid json, no member : String");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["String"].asString());
	_tcsncpy_s(stWriteProfileInfo.szString, _countof(stWriteProfileInfo.szString), wsItem.c_str(), _countof(stWriteProfileInfo.szString) - 1);
    
	//FileName
	if (!CMDContent.isMember("FileName") || CMDContent["FileName"].isNull())
	{
		wosError << _T("invalid json, no member : FileName");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["FileName"].asString());
	_tcsncpy_s(stWriteProfileInfo.szFileName, _countof(stWriteProfileInfo.szFileName), wsItem.c_str(), _countof(stWriteProfileInfo.szFileName) - 1);
    
	//FuncItem
	if (!CMDContent.isMember("FuncItem") || CMDContent["FuncItem"].isNull())
	{
		wosError << _T("invalid json, no member : FuncItem");
		goto END;
	}
	stWriteProfileInfo.dwFuncItem = CMDContent["FuncItem"].asInt();


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}


std::string CWLJsonParse::BaseLineExport_GetJson(__in ST_BASELINE_EXPORT stBaseLineExport, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	/*
	[{
		"Key":"XXXX",
		"Dest":"XXXX",
		"FileName":"XXXX",
		"Level":XXXX,
		"CMDContent":
			[{
				"KeyName":"XXXX",
				"Dest":"XXXX",
				"Select":XXXX,
				"Param":XXXX
			},
			{
				……
			} 
			]
	
	}]
	*/
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	
	Json::FastWriter jsWriter;

	int iCount = (int)stBaseLineExport.vecExportItems.size();

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	Item["Key"] = UnicodeToUTF8(stBaseLineExport.szKey);
	Item["Dest"] = UnicodeToUTF8(stBaseLineExport.szDest);
	Item["FileName"] = UnicodeToUTF8(stBaseLineExport.szFileName);
	Item["Level"] = (int)stBaseLineExport.dwLevel;

	for(int i=0; i<iCount; i++)
	{
		Json::Value ItemsObj;
		ST_BASELINE_EXPORT_ITEM &stExportItem = stBaseLineExport.vecExportItems[i];

		ItemsObj["KeyName"] = UnicodeToUTF8(stBaseLineExport.vecExportItems[i].szKeyName);
		ItemsObj["Dest"] = UnicodeToUTF8(stBaseLineExport.vecExportItems[i].szDesp);
		ItemsObj["Select"] = (int)stBaseLineExport.vecExportItems[i].dwSelect;
		ItemsObj["Param"] = (int)stBaseLineExport.vecExportItems[i].dwParam;

		Item["CMDContent"].append(ItemsObj);
		ItemsObj.clear();
	}
	
	root.append(Item);
	strJson = jsWriter.write(root);
	root.clear();

	return strJson;
}

BOOL CWLJsonParse::BaseLineExport_ParseJson(__in string strJson, __out ST_BASELINE_EXPORT &stBaseLineExport, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//Key
	if (!root[0].isMember("Key") || root[0]["Key"].isNull())
	{
		wosError << _T("invalid json, no member : Key");
		goto END;
	}
	wsItem = UTF8ToUnicode(root[0]["Key"].asString());
	_tcsncpy_s(stBaseLineExport.szKey, _countof(stBaseLineExport.szKey), wsItem.c_str(), _countof(stBaseLineExport.szKey) - 1);
    
    //Dest
	if (!root[0].isMember("Dest") || root[0]["Dest"].isNull())
	{
		wosError << _T("invalid json, no member : Dest");
		goto END;
	}
	wsItem = UTF8ToUnicode(root[0]["Dest"].asString());
	_tcsncpy_s(stBaseLineExport.szDest, _countof(stBaseLineExport.szDest), wsItem.c_str(), _countof(stBaseLineExport.szDest) - 1);
    
	//FileName
	if (!root[0].isMember("FileName") || root[0]["FileName"].isNull())
	{
		wosError << _T("invalid json, no member : FileName");
		goto END;
	}
	wsItem = UTF8ToUnicode(root[0]["FileName"].asString());
	_tcsncpy_s(stBaseLineExport.szFileName, _countof(stBaseLineExport.szFileName), wsItem.c_str(), _countof(stBaseLineExport.szFileName) - 1);
    
	//Level
	if (!root[0].isMember("Level") || root[0]["Level"].isNull())
	{
		wosError << _T("invalid json, no member : Level");
		goto END;
	}
	stBaseLineExport.dwLevel = root[0]["Level"].asInt();

	for(int i=0; i<CMDContent.size(); i++)
	{
		ST_BASELINE_EXPORT_ITEM stExportItem;
		
		//KeyName
		wsItem = UTF8ToUnicode(CMDContent[i]["KeyName"].asString());
		_tcsncpy_s(stExportItem.szKeyName, _countof(stExportItem.szKeyName), wsItem.c_str(), _countof(stExportItem.szKeyName) - 1);
		
		//Dest
		wsItem = UTF8ToUnicode(CMDContent[i]["Dest"].asString());
		_tcsncpy_s(stExportItem.szDesp, _countof(stExportItem.szDesp), wsItem.c_str(), _countof(stExportItem.szDesp) - 1);

		//Select
		stExportItem.dwSelect = CMDContent[i]["Select"].asInt();
		
		//Param
		stExportItem.dwParam = CMDContent[i]["Param"].asInt();

		stBaseLineExport.vecExportItems.push_back(stExportItem);
	}

	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}


std::string CWLJsonParse::SetSecDetect_SystemCtrl_GetJson(__in IWLConfig_SystemCtrl::SystemCtrl_SecDetect stcSecDetect, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

	CMDContentItem["CurrentOsUserSID"] = UnicodeToUTF8(stcSecDetect.szCurrentOsUserSID);

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);

	return strJson;
}

BOOL CWLJsonParse::SetSecDetect_SystemCtrl_ParseJson(__in string strJson, __out IWLConfig_SystemCtrl::SystemCtrl_SecDetect &stcSecDetect, __out tstring &wsError)
{
	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

	//AppName
	if (!CMDContent.isMember("CurrentOsUserSID") || CMDContent["CurrentOsUserSID"].isNull())
	{
		wosError << _T("invalid json, no member : CurrentOsUserSID");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["CurrentOsUserSID"].asString());
	_tcsncpy_s(stcSecDetect.szCurrentOsUserSID, _countof(stcSecDetect.szCurrentOsUserSID), wsItem.c_str(), _countof(stcSecDetect.szCurrentOsUserSID) - 1);

	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

std::string CWLJsonParse::VirusUpdate_GetJson(__in ST_VIRUSUPDATE_CONFIG stVirusUpdateCfg, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
        "ComputerID":"XXXXX",
        "CMDTYPE":150,
        "CMDID":354,  //CMDID暂时定义为354，如果重复了可以修改
        "CMDContent":
            { 
                "Address":"192.168.7.80",  //IP地址或域名
                "Protocol":"https",        //协议
                "Port":8440,               //升级端口，默认8440
                "CycleItem":1,             //1：月，2：周；3：天
                "CycleDay":1,              //天数[1, 28] 或 周一到周天[1, 7]
                "CycleHour":14,            //小时[0, 23]
                "CycleMinute":0            //分钟[0, 59]
            }
    }]
    */

	std::string strJson = "";
	
	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType; 
	Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["Protocol"] = UnicodeToUTF8(stVirusUpdateCfg.szProtocol);
	CMDContentItem["Address"] = UnicodeToUTF8(stVirusUpdateCfg.szAddress);
	CMDContentItem["Port"] = (int)stVirusUpdateCfg.dwPort;
	CMDContentItem["CycleItem"] = (int)stVirusUpdateCfg.dwCycleItem;
	CMDContentItem["CycleDay"] = (int)stVirusUpdateCfg.dwCycleDay;
	CMDContentItem["CycleHour"] = (int)stVirusUpdateCfg.dwCycleHour;
	CMDContentItem["CycleMinute"] = (int)stVirusUpdateCfg.dwCycleMinute;

	Item["CMDContent"] = (Json::Value)CMDContentItem;
	
	root.append(Item);
	strJson = jsWriter.write(root);

	return strJson;
}

BOOL CWLJsonParse::VirusUpdate_ParseJson(__in string strJson, __out ST_VIRUSUPDATE_CONFIG &stVirusUpdateCfg, __out tstring &wsError)
{
    /*
    [{
        "ComputerID":"XXXXX",
        "CMDTYPE":150,
        "CMDID":354,  //CMDID暂时定义为354，如果重复了可以修改
        "CMDContent":
            { 
                "Address":"192.168.7.80",  //IP地址或域名
                "Port":8440,               //升级端口，默认8440
                "CycleItem":1,             //1：月，2：周；3：天
                "CycleDay":1,              //天数[1, 28] 或 周一到周天[1, 7]
                "CycleHour":14,            //小时[0, 23]
                "CycleMinute":0            //分钟[0, 59]
            }
    }]
    */

	BOOL bResult = FALSE;
	
	std::string strValue = "";
	std::wstring wsItem = _T("");
	
	Json::Reader reader;
	
	Json::Value  root;
	Json::Value  CMDContent;
	
	wostringstream  wosError;

	if ( strJson.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0");
		goto END;
	}
	
	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
		goto END;
	}

	if(!root[0].isMember("CMDContent"))
	{
		wosError << _T("invalid json, no member : CMDContent");
		goto END;
	}
	CMDContent =  (Json::Value)root[0]["CMDContent"];

    // Protocol
    if (CMDContent.isMember("Protocol") && !CMDContent["Protocol"].isNull())
    {
        wsItem = UTF8ToUnicode(CMDContent["Protocol"].asString());
        _tcsncpy_s(stVirusUpdateCfg.szProtocol, _countof(stVirusUpdateCfg.szProtocol), wsItem.c_str(), _countof(stVirusUpdateCfg.szProtocol) - 1);
    }

	//Address
	if (!CMDContent.isMember("Address") || CMDContent["Address"].isNull())
	{
		wosError << _T("invalid json, no member : Address");
		goto END;
	}
	wsItem = UTF8ToUnicode(CMDContent["Address"].asString());
	_tcsncpy_s(stVirusUpdateCfg.szAddress, _countof(stVirusUpdateCfg.szAddress), wsItem.c_str(), _countof(stVirusUpdateCfg.szAddress) - 1);

    //Port
    if (!CMDContent.isMember("Port") || CMDContent["Port"].isNull())
    {
        wosError << _T("invalid json, no member : Port");
        goto END;
    }
    stVirusUpdateCfg.dwPort = CMDContent["Port"].asInt();
    
    //CycleItem
	if (!CMDContent.isMember("CycleItem") || CMDContent["CycleItem"].isNull())
	{
		wosError << _T("invalid json, no member : CycleItem");
		goto END;
	}
	stVirusUpdateCfg.dwCycleItem = CMDContent["CycleItem"].asInt();

	//CycleDay
	if (!CMDContent.isMember("CycleDay") || CMDContent["CycleDay"].isNull())
	{
		wosError << _T("invalid json, no member : CycleDay");
		goto END;
	}
	stVirusUpdateCfg.dwCycleDay = CMDContent["CycleDay"].asInt();

	//CycleHour
	if (!CMDContent.isMember("CycleHour") || CMDContent["CycleHour"].isNull())
	{
		wosError << _T("invalid json, no member : CycleHour");
		goto END;
	}
	stVirusUpdateCfg.dwCycleHour = CMDContent["CycleHour"].asInt();

	//CycleMinute
	if (!CMDContent.isMember("CycleMinute") || CMDContent["CycleMinute"].isNull())
	{
		wosError << _T("invalid json, no member : CycleMinute");
		goto END;
	}
	stVirusUpdateCfg.dwCycleMinute = CMDContent["CycleMinute"].asInt();


	bResult = TRUE;

END:

	if (FALSE == bResult)
	{
		wsError = wosError.str();
	}

	return bResult;
}

//请求USM或云中心的病毒库版本
std::string CWLJsonParse::VirusUpgrqade_Version_GetJson(__in tstring wsArch, __in tstring wsOSType, __in tstring wsAnviProduct, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{
    std::string strJson = "";

    Json::Value root;
    Json::Value Item;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    Item["Arch"] = UnicodeToUTF8(wsArch);
    Item["AntVirusProduct"] = UnicodeToUTF8(wsAnviProduct);
    Item["OsType"] = UnicodeToUTF8(wsOSType);

    root = (Json::Value)Item;
    strJson = jsWriter.write(root);

    return strJson;
}

//解析来自云中心或USM的病毒库版本
// eVirusEngineType: 本地安装的病毒库类型
BOOL CWLJsonParse::VirusUpgrqade_Version_FromUSM_PardeJson(__in string strJson, __out ST_VIRUS_UPGRADE_VERSION &stVirusUpgradeVersion, __out tstring &wsError, SCAN_ENGINE_TYPE eVirusEngineType)
{
    /*
    {
        "code": 200,
        "message": "OK",
        "data": 
        {
            "VirusLibMd5": "54207fe2e5ea342f0d4838d95c272dae",
            "VirusLibName": "rising_25.00.45.74.zip",
            "VirusLibSize": 108612602,
            "VirusLibDownloadURL": "/USM/ieg/upgrade/download?antVirusProduct=rising&type=1&osType=windows&arch=x86_64&fileName=rising_25.00.45.74.zip",
            "VirusEngineMd5": "3beb1d4b2b22e73b0c0468d43c032b40",
            "VirusEngineName": "rising_engine_1.0.0.18.zip",
            "VirusEngineSize": 10861681,
            "VirusEngineDownloadURL": "/USM/ieg/upgrade/download?antVirusProduct=rising&type=2&osType=windows&arch=x86_64&fileName=win_x64_rising_engine_1.0.0.18.zip",
            "featrueLibMd5": "54207fe2e5ea342f0d4838d95c272dae",
            "featrueLibName": "rising_25.00.45.74.zip",
            "featrueLibSize": 108612602,
            "featrueLibDownloadURL": "/USM/ieg/upgrade/download?antVirusProduct=rising&type=1&osType=windows&arch=x86_64&fileName=rising_25.00.45.74.zip",          
            "VirusLibUploadTime": "2023-09-20 18:01:43",
            "VirusLibVersion": "25.00.45.74",
            "VirusEngineVersion": "1.0.0.18"
            "featureLibVersion": "25.00.45.74",
            "featureLibUploadTime": "2023-09-20 18:01:43",
        }
    }
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("data"))
    {
        wosError << _T("invalid json, no member : data");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["data"];

    //"VirusLibMd5": "e9f9ab4184507e84506e7c7e74159d1a",
    if (!CMDContent.isMember("VirusLibMd5") || CMDContent["VirusLibMd5"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibMd5");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusLibMd5"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusLibMD5, _countof(stVirusUpgradeVersion.szVirusLibMD5), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusLibMD5) - 1);

    //"VirusLibName": "rising_30.70.01.18.zip",
    if (!CMDContent.isMember("VirusLibName") || CMDContent["VirusLibName"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibName");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusLibName"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusLibName, _countof(stVirusUpgradeVersion.szVirusLibName), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusLibName) - 1);

    //"VirusLibSize": 209053624,
    if (!CMDContent.isMember("VirusLibSize") || CMDContent["VirusLibSize"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibSize");
        goto END;
    }
    stVirusUpgradeVersion.dwVirusLibSize = (DWORD)CMDContent["VirusLibSize"].asUInt64();

    //"VirusLibDownloadURL": "https://192.168.4.100:8440/USM/notLoginDownLoad/download?fileName=rising_30.70.01.18.zip",
    if (!CMDContent.isMember("VirusLibDownloadURL") || CMDContent["VirusLibDownloadURL"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibDownloadURL");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusLibDownloadURL"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusLibDwonloadURL, _countof(stVirusUpgradeVersion.szVirusLibDwonloadURL), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusLibDwonloadURL) - 1);

    //"VirusLibVersion": "30.70.01.18",
    if (!CMDContent.isMember("VirusLibVersion") || CMDContent["VirusLibVersion"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibVersion");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusLibVersion"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusLibVersion, _countof(stVirusUpgradeVersion.szVirusLibVersion), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusLibVersion) - 1);

    //"VirusEngineMd5":"dd",
    if (!CMDContent.isMember("VirusEngineMd5") || CMDContent["VirusEngineMd5"].isNull())
    {
		if(eVirusEngineType == RISING_ENGINE)	// 目前只有瑞星有病毒引擎升级
		{
			wosError << _T("invalid json, no member : VirusEngineMd5");
			goto END;
		}
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusEngineMd5"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusEngineMD5, _countof(stVirusUpgradeVersion.szVirusEngineMD5), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusEngineMD5) - 1);

    //"VirusEngineName":"rising_engine_3.3.3.3.zip",
    if (!CMDContent.isMember("VirusEngineName") || CMDContent["VirusEngineName"].isNull())
    {
		if(eVirusEngineType == RISING_ENGINE)	// 目前只有瑞星有病毒引擎升级
		{
			wosError << _T("invalid json, no member : VirusEngineName");
			goto END;
		}
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusEngineName"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusEngineName, _countof(stVirusUpgradeVersion.szVirusEngineName), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusEngineName) - 1);

    //"VirusEngineSize":333,//字节
    if (!CMDContent.isMember("VirusEngineSize") || CMDContent["VirusEngineSize"].isNull())
    {
		if(eVirusEngineType == RISING_ENGINE)	// 目前只有瑞星有病毒引擎升级
		{
			wosError << _T("invalid json, no member : VirusEngineSize");
			goto END;
		}
    }
    stVirusUpgradeVersion.dwVirusEngineSize = (DWORD)CMDContent["VirusEngineSize"].asUInt64();

    //"VirusEngineDownloadURL":"https://ipaddr:8440/USM/notLoginDownLoad/download?fileName=rising_engine_3.3.3.3.zip"
    if (!CMDContent.isMember("VirusEngineDownloadURL") || CMDContent["VirusEngineDownloadURL"].isNull())
    {
		if(eVirusEngineType == RISING_ENGINE)	// 目前只有瑞星有病毒引擎升级
		{
			wosError << _T("invalid json, no member : VirusEngineDownloadURL");
			goto END;
		}
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusEngineDownloadURL"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusEngineDownloadURL, _countof(stVirusUpgradeVersion.szVirusEngineDownloadURL), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusEngineDownloadURL) - 1);

    //"VirusEngineVersion":"3.3.3.3"
    if (!CMDContent.isMember("VirusEngineVersion") || CMDContent["VirusEngineVersion"].isNull())
    {
		if(eVirusEngineType == RISING_ENGINE)	// 目前只有瑞星有病毒引擎升级
		{
			wosError << _T("invalid json, no member : VirusEngineVersion");
			goto END;
		}
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusEngineVersion"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusEngineVersion, _countof(stVirusUpgradeVersion.szVirusEngineVersion), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusEngineVersion) - 1);

    //"VirusLibUploadTime": "2023-08-21 17:24:22",
    if (!CMDContent.isMember("VirusLibUploadTime") || CMDContent["VirusLibUploadTime"].isNull())
    {
        wosError << _T("invalid json, no member : VirusLibUploadTime");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["VirusLibUploadTime"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szVirusLibUploadTime, _countof(stVirusUpgradeVersion.szVirusLibUploadTime), wsItem.c_str(), _countof(stVirusUpgradeVersion.szVirusLibUploadTime) - 1);

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}


BOOL CWLJsonParse::FeatureUpgrqade_Version_FromUSM_PardeJson(__in string strJson, __out ST_VIRUS_UPGRADE_VERSION &stVirusUpgradeVersion, __out tstring &wsError)
{
    /*
    {
        "code": 200,
        "message": "OK",
        "data": 
        {
            "featrueLibMd5": "54207fe2e5ea342f0d4838d95c272dae",
            "featrueLibName": "rising_25.00.45.74.zip",
            "featrueLibSize": 108612602,
            "featrueLibDownloadURL": "/USM/ieg/upgrade/download?antVirusProduct=rising&type=1&osType=windows&arch=x86_64&fileName=rising_25.00.45.74.zip",          
            "featureLibVersion": "25.00.45.74",
            "featureLibUploadTime": "2023-09-20 18:01:43",
        }
    }
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("data"))
    {
        wosError << _T("invalid json, no member : data");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["data"];

    //"FeatureLibMd5": "e9f9ab4184507e84506e7c7e74159d1a",
    if (!CMDContent.isMember("featureLibMd5") || CMDContent["featureLibMd5"].isNull())
    {
        wosError << _T("invalid json, no member : featureLibMd5");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["featureLibMd5"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szFeatureLibMD5, _countof(stVirusUpgradeVersion.szFeatureLibMD5), wsItem.c_str(), _countof(stVirusUpgradeVersion.szFeatureLibMD5) - 1);

    //"FeatureLibName": "rising_30.70.01.18.zip",
    if (!CMDContent.isMember("featureLibName") || CMDContent["featureLibName"].isNull())
    {
        wosError << _T("invalid json, no member : featureLibName");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["featureLibName"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szFeatureLibName, _countof(stVirusUpgradeVersion.szFeatureLibName), wsItem.c_str(), _countof(stVirusUpgradeVersion.szFeatureLibName) - 1);

    //"FeatureLibSize": 209053624,
    if (!CMDContent.isMember("featureLibSize") || CMDContent["featureLibSize"].isNull())
    {
        wosError << _T("invalid json, no member : featureLibSize");
        goto END;
    }
    stVirusUpgradeVersion.dwFeatureLibSize = (DWORD)CMDContent["featureLibSize"].asUInt64();

    //"FeatureLibDownloadURL": "https://192.168.4.100:8440/USM/notLoginDownLoad/download?fileName=rising_30.70.01.18.zip",
    if (!CMDContent.isMember("featureLibDownloadUrl") || CMDContent["featureLibDownloadUrl"].isNull())
    {
        wosError << _T("invalid json, no member : featureLibDownloadUrl");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["featureLibDownloadUrl"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szFeatureLibDwonloadURL, _countof(stVirusUpgradeVersion.szFeatureLibDwonloadURL), wsItem.c_str(), _countof(stVirusUpgradeVersion.szFeatureLibDwonloadURL) - 1);

    //"FeatureLibVersion": "30.70.01.18",
    if (!CMDContent.isMember("featureLibVersion") || CMDContent["featureLibVersion"].isNull())
    {
        wosError << _T("invalid json, no member : FeatureLibVersion");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["featureLibVersion"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szFeatureLibVersion, _countof(stVirusUpgradeVersion.szFeatureLibVersion), wsItem.c_str(), _countof(stVirusUpgradeVersion.szFeatureLibVersion) - 1);

    //"FeatureLibUploadTime": "2023-08-21 17:24:22",
    if (!CMDContent.isMember("featureLibUploadTime") || CMDContent["featureLibUploadTime"].isNull())
    {
        wosError << _T("invalid json, no member : featureLibUploadTime");
        goto END;
    }
    wsItem = UTF8ToUnicode(CMDContent["featureLibUploadTime"].asString());
    _tcsncpy_s(stVirusUpgradeVersion.szFeatureLibUploadTime, _countof(stVirusUpgradeVersion.szFeatureLibUploadTime), wsItem.c_str(), _countof(stVirusUpgradeVersion.szFeatureLibUploadTime) - 1);
    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

std::string CWLJsonParse::GetFaileJsonToUsm( __in tstring ComputerID,__in CMDTYPE CmdType,__in WORD cmdId,__in wstring &FailedReason)
{
	Json::Value CMDContent;
	std::string sJsonPacket = "";

	Json::Value root;
	Json::FastWriter writer;
	Json::Value head;

	CMDContent["RESULT"] = "FAIL";
	CMDContent["REASON"] = UnicodeToUTF8(FailedReason);

	head["ComputerID"]  = UnicodeToUTF8(ComputerID);
	head["CMDTYPE"]     = CmdType;
	head["CMDID"]       = cmdId;

	head["CMDContent"]    = CMDContent;

	root.append(head);
	sJsonPacket = writer.write(root);
	root.clear();

	return sJsonPacket;
}

BOOL CWLJsonParse::ImportVBPackage_ParseJson(__in string wsStr, __out SCAN_ENGINE_TYPE &eVirusDBType)
{
	BOOL               bRet = FALSE;
	Json::Value		 root;
	Json::Value		 typeValue;
	Json::Reader     reader;
	//int              nCount;
	Json::FastWriter writer;
	std::string type;
	std::string sKav = "kav";
	std::string sRs = "rising";

	if(wsStr.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(wsStr, root))
	{
		goto END;
	}

	typeValue = root["type"];
	if (typeValue.isNull())
	{
		goto END;
	}

	type = typeValue.asString();
	if (type == sKav)
	{
		eVirusDBType = KAV_ENGINE;
	}
	else if (type == sRs)
	{
		eVirusDBType = RISING_ENGINE;
	}
	else
	{
		eVirusDBType = UNKNOW_ENGINE;
	}

	bRet = TRUE;

END:
	return bRet;
}

BOOL CWLJsonParse::DeskConnect_ParseJson(__in string strJson, __out WLDESK_CONNECT &stDeskConnect, __out tstring &wsError)
{
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  510,
    "cmdContent": {
    "enable": 1,（客户端上报）
    "readWritePermission": 1,（服务端下发）
    "bindingMinute": 60（服务端下发）
    "id": "XXX"（客户端上报)
    "verificationCode": "XXXXXXXXX"(客户端上报)
    "deskUser":"admin", （平台下发）
    "policyIp":192.168.4.22,(客户端上报)
    "policyPort":5500,(客户端上报)
    }
    }]
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : cmdContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    // enable
    if (CMDContent.isMember("enable") && CMDContent["enable"].asInt64())
    {
        stDeskConnect.dwEnable = CMDContent["enable"].asInt64();
    }

    //read_write_permission"        
    if(CMDContent.isMember("readWritePermission") && CMDContent["readWritePermission"].asInt64())
    {
        stDeskConnect.dwPermission = CMDContent["readWritePermission"].asInt64();
    }

    //binding_minute"                    
    if (CMDContent.isMember("bindingMinute") && CMDContent["bindingMinute"].asInt64())
    {
        stDeskConnect.dwBindMinute = CMDContent["bindingMinute"].asInt64();
    }

    //id"                               
    if (CMDContent.isMember("connId") && CMDContent["connId"].asInt64())
    {
        stDeskConnect.dwId = CMDContent["connId"].asInt64();
    }

    //verification_code"
    if(CMDContent.isMember("verificationCode") && CMDContent["verificationCode"].isString())
    {
        wsItem = CStrUtil::UTF8ToUnicode(CMDContent["verificationCode"].asString());
        _tcsncpy_s(stDeskConnect.szVerification, _countof(stDeskConnect.szVerification), wsItem.c_str(), _countof(stDeskConnect.szVerification) - 1);
    }

    //desk_user" 
    if(CMDContent.isMember("deskUser") && CMDContent["deskUser"].isString())
    {
        wsItem = CStrUtil::UTF8ToUnicode(CMDContent["deskUser"].asString());
        _tcsncpy_s(stDeskConnect.szDeskUser, _countof(stDeskConnect.szDeskUser), wsItem.c_str(), _countof(stDeskConnect.szDeskUser) - 1);
    }

    //policyIp
    if (CMDContent.isMember("policyIp") && CMDContent["policyIp"].isString())
    {
        wsItem = CStrUtil::UTF8ToUnicode(CMDContent["policyIp"].asString());
        _tcsncpy_s(stDeskConnect.szPolicyIp, _countof(stDeskConnect.szPolicyIp), wsItem.c_str(), _countof(stDeskConnect.szPolicyIp) - 1);
    }

    //policyPort
    if (CMDContent.isMember("policyPort") && CMDContent["policyPort"].isString())
    {
        stDeskConnect.dwPort = CMDContent["policyPort"].asInt64();
    }

    //remainMinute
    if (CMDContent.isMember("remainMinute") && CMDContent["remainMinute"].asInt64())
    {
        stDeskConnect.dwRemainMinute = CMDContent["remainMinute"].asInt64();
    }



    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}
 

std::string CWLJsonParse::DeskConnect_GetJson(__in WLDESK_CONNECT stDeskConnect, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  510,
    "cmdContent": {
    "enable": 1,（客户端上报）
    "readWritePermission": 1,（服务端下发）
    "bindingMinute": 60（服务端下发）
    "id": "XXX"（客户端上报)
    "verificationCode": "XXXXXXXXX"(客户端上报)
    "deskUser":"admin", （平台下发）
    "policyIp":192.168.4.22,(客户端上报)
    "policyPort":5500,(客户端上报)
        }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["enable"] = (int)stDeskConnect.dwEnable;
    CMDContentItem["readWritePermission"] = (int)stDeskConnect.dwPermission;
    CMDContentItem["bindingMinute"] = (int)stDeskConnect.dwBindMinute;
    CMDContentItem["connId"] = (int)stDeskConnect.dwId;
    CMDContentItem["verificationCode"] = UnicodeToUTF8(stDeskConnect.szVerification);
    CMDContentItem["deskUser"] = UnicodeToUTF8(stDeskConnect.szDeskUser);
    CMDContentItem["policyIp"] = UnicodeToUTF8(stDeskConnect.szPolicyIp);
    CMDContentItem["policyPort"] = (int)stDeskConnect.dwPort;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

std::string CWLJsonParse::DeskConnect_GetJson_Client(__in WLDESK_CONNECT stDeskConnect)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  510,
    "cmdContent": {
    "enable": 1,（客户端上报）
    "readWritePermission": 1,（服务端下发）
    "bindingMinute": 60（服务端下发）
    "id": "XXX"（客户端上报)
    "verificationCode": "XXXXXXXXX"(客户端上报)
    "deskUser":"admin", （平台下发）
    "policyIp":192.168.4.22,(客户端上报)
    "policyPort":5500,(客户端上报)
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;    

    CMDContentItem["enable"] = (int)stDeskConnect.dwEnable;
    CMDContentItem["readWritePermission"] = (int)stDeskConnect.dwPermission;
    CMDContentItem["bindingMinute"] = (int)stDeskConnect.dwBindMinute;
    CMDContentItem["connId"] = (int)stDeskConnect.dwId;
    CMDContentItem["verificationCode"] = UnicodeToUTF8(stDeskConnect.szVerification);
    CMDContentItem["deskUser"] = UnicodeToUTF8(stDeskConnect.szDeskUser);
    CMDContentItem["policyIp"] = UnicodeToUTF8(stDeskConnect.szPolicyIp);
    CMDContentItem["policyPort"] = (int)stDeskConnect.dwPort;
    CMDContentItem["remainMinute"] = (int)stDeskConnect.dwRemainMinute;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

std::string CWLJsonParse::DeskReturn_GetJson(__in int nDeskReturn, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    “ComputerID”:”FEFOEACD”,
    “CMDTYPE”:150,
    “CMDID”:511,
    “cmdContent”:{
    “RESULT”:”0”
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["RESULT"] = (int)nDeskReturn;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

// 解析导入包中卡巴斯基病毒库版本号版本号
BOOL CWLJsonParse::ImprotKavDB_ParseJson(__in string wsStr, __out wstring& wsVirusDBVersion)
{
	/*
	{
	"Version":"17.25.32.26",
	"type":"kav"
	}
	*/
	BOOL             bRet = FALSE;
	Json::Value		 root;
	Json::Value		 typeValue;
	Json::Value		 versionValue;
	Json::Reader     reader;
	Json::FastWriter writer;
	std::string type;
	std::wstring wsVirusDBType = _T("");

	if(wsStr.length() == 0)
	{
		goto END;
	}

	if (!reader.parse(wsStr, root))
	{
		goto END;
	}

	if(!root.isMember("type"))
	{
		goto END;
	}

	typeValue = root["type"];
	if (typeValue.isNull())
	{
		goto END;
	}

	wsVirusDBType = UTF8ToUnicode(typeValue.asString());

	if(wsVirusDBType != _T("kav"))	// 不是卡巴，则退出
	{
		goto END;
	}

	if(!root.isMember("Version"))
	{
		goto END;
	}

	versionValue = root["Version"];
	if (versionValue.isNull())
	{
		goto END;
	}

	wsVirusDBVersion = UTF8ToUnicode(versionValue.asString());	// 读取到卡巴斯基的版本号

	bRet = TRUE;

END:
	return bRet;
}

// 解析导入包中瑞星病毒库版本号和病毒引擎版本号
BOOL CWLJsonParse::ImprotRSDB_ParseJson(__in string wsStr, __out wstring& wsVirusDBVersion, __out wstring& wsVirusEngineVersion, __out wstring& wsError)
{
	/*
	{
	"VirusDB":
		{
		"RS_Version":"25.00.53.23",
		"Version":"25.00.53.23"
		},
	"Engine":
		{
		"RS_Version":"1.0.0.18",
		"Version":"1.0.0.18"
		}
	}
	*/
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  VirusDB;
	Json::Value  Engine;
	Json::Value  typeValue;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = wsStr;

	string strTmp = "";
	wstring wstrTmp = _T("");
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	if(!root[0].isMember("VirusDB"))
	{
		wosError << _T("invalid json, no member : VirusDB");
		goto END;
	}

	VirusDB = (Json::Value)root[0]["VirusDB"];
	if (VirusDB.isMember("RS_Version") && !VirusDB["RS_Version"].isNull())
	{
		wsVirusDBVersion = UTF8ToUnicode(VirusDB["RS_Version"].asString());	// 读取病毒库版本号
	}
	else
	{
		wosError << _T("Failed to Parse VirusDB RS_Version");
		bRes = FALSE;
		goto END;
	}

	if(!root[0].isMember("Engine"))
	{
		wosError << _T("invalid json, no member : VirusDB");
		goto END;
	}

	Engine = (Json::Value)root[0]["Engine"];
	if (Engine.isMember("RS_Version") && !Engine["RS_Version"].isNull())
	{
		wsVirusEngineVersion = UTF8ToUnicode(Engine["RS_Version"].asString());	// 读取病毒引擎版本号
	}
	else
	{
		wosError << _T("Failed to Parse Engine RS_Version");
		bRes = FALSE;
		goto END;
	}

	bRes = TRUE;

END:
	wsError = wosError.str();
	return bRes;
}

//暂时弃用
BOOL CWLJsonParse::DeskSwitch_ParseJson(__in string strJson, __out BOOL &bShurdown, __out tstring &wsError)
{
    /*
    [{
        "ComputerID": "FEFOEACD",
        "CMDTYPE": 150,
        "CMDID":  512,
        "CMDContent": {
        "active-shutdown": True,
         }
    }]
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    // enable
    if (CMDContent.isMember("active-shutdown"))
    {
        bShurdown = CMDContent["active-shutdown"].asInt64();
    }

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

std::string CWLJsonParse::DeskSwitch_GetJson(__in BOOL bShutdown, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  512,
    "CMDContent": {
    "active-shutdown": True,
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["active-shutdown"] = (BOOL)bShutdown;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

BOOL CWLJsonParse::DeskShutdown_ParseJson(__in string strJson, __out int &bShurdown, __out tstring &wsError)
{
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  512,
    "cmdContent": {
    "isShutdown": 1,
    }
    }
    }]
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    // enable
    if (CMDContent.isMember("isShutdown"))
    {
        bShurdown = CMDContent["isShutdown"].asInt64();
    }

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

std::string CWLJsonParse::DeskShutdown_GetJson(__in int bShutdown, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "cmdId":  512,
    "cmdContent": {
    "isShutdown": 1,
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["RESULT"] = bShutdown;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

std::string CWLJsonParse::DeskShutdown_Client(__in int bShutdown, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "cmdId":  512,
    "cmdContent": {
    "isShutdown": 1,
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["isShutdown"] = bShutdown;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}


std::string CWLJsonParse::FirewallBase_GetJson(__in std::list<NET_CONNECTION> infos)
{
    /*
    unsigned char b8Protocal;       // 协议 6:TCP , 17:UDP
    unsigned char b8State;	        // 连接状态 - 仅限TCP
    unsigned short usLocalPort;     // 本地端口
    unsigned short usRemotePort;    // 远程端口
    unsigned long dwLocalIP;        // 本地IP
    unsigned long dwRemoteIP;       // 远程IP
    unsigned long dwPid;            // 进程ID
    wchar_t wFileName[MAX_DEVICE_AND_DOS_NAME];     // 程序 - 绝对路径
    */

    std::string strJson;
    
    Json::Value Item;
    Json::FastWriter writer;
    int i = 0;
    std::string strFileName = "";

    std::list<NET_CONNECTION>::const_iterator it = infos.begin();
    for (;it != infos.end(); ++it)
    {
        Json::Value root;
        std::string strProtocal(1, static_cast<char>(it->b8Protocal));
        root["b8Protocal"] = strProtocal;

        std::string strState(1, static_cast<char>(it->b8State));
        root["b8State"] = strState;

        root["usLocalPort"] = (short)it->usLocalPort;
        root["usRemotePort"] = (short)it->usRemotePort;
        root["dwLocalIP"] = (long)it->dwLocalIP;
        root["dwRemoteIP"] = (long)it->dwRemoteIP;
        root["dwPid"] = (long)it->dwPid;

        strFileName = UnicodeToUTF8(it->wFileName);
        root["wFileName"] = strFileName;

        Item.append(root);
    }
   
    strJson = writer.write(Item);
    return strJson;
}


BOOL CWLJsonParse::FirewallBase_GetValue(const std::string &sJson, std::list<NET_CONNECTION> &infos) 
{
    BOOL bRet = FALSE;

    Json::Reader parser;
    Json::Value root;
    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    if ( sJson.length() == 0)
    {
        return bRet; 
    }

    strValue = sJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }

    if (!reader.parse(strValue, root) || !root.isArray())
    {
        return bRet; 
    }

    infos.clear();
    // 遍历JSON数组
    const Json::Value &items = root;
    for (Json::ArrayIndex i = 0; i < items.size(); ++i) 
    {
        const Json::Value &item = items[i];

        NET_CONNECTION conn;

        // 解析每个字段并填充到NET_CONNECTION结构体中

        if (item.isMember("b8Protocal"))
        {
            std::string protoStr = item["b8Protocal"].asString();
            int proto = static_cast<int>(protoStr[0]);  
            conn.b8Protocal = static_cast<unsigned char>(proto);
        }

        if (item.isMember("b8State"))
        {
            std::string protoStr = item["b8State"].asString();
            int proto = static_cast<int>(protoStr[0]);  
            conn.b8State = static_cast<unsigned char>(proto);
        }

        if (item.isMember("usLocalPort") && item["usLocalPort"].isNumeric())
        {
            conn.usLocalPort = item["usLocalPort"].asInt();
        }
        
        if (item.isMember("usRemotePort") && item["usRemotePort"].isNumeric())
        {
            conn.usRemotePort = item["usRemotePort"].asInt();
        }
        
        if (item.isMember("dwLocalIP") && item["dwLocalIP"].isNumeric())
        {
            conn.dwLocalIP = item["dwLocalIP"].asInt();
        }

        if (item.isMember("dwRemoteIP") && item["dwRemoteIP"].isNumeric())
        {
            conn.dwRemoteIP = item["dwRemoteIP"].asInt();
        }
        
        if (item.isMember("dwPid") && item["dwPid"].isNumeric())
        {
            conn.dwPid = item["dwPid"].asInt();
        }
        
        if (item.isMember("wFileName") && item["wFileName"].isString())
        {
            wsItem = CStrUtil::UTF8ToUnicode(item["wFileName"].asString());
            _tcsncpy_s(conn.wFileName, _countof(conn.wFileName), wsItem.c_str(), _countof(conn.wFileName) - 1);
        }

        // 将解析后的NET_CONNECTION添加到infos列表中
        infos.push_back(conn);
    }

    return TRUE;  // 解析成功
}

BOOL CWLJsonParse::Maintenance_ParseJson(__in string strJson, __out int &bMaintenance, __out tstring &wsError)
{
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "CMDID":  512,
    "cmdContent": {
    "enable": 1,
    }
    }
    }]
    */

    BOOL bResult = FALSE;

    std::string strValue = "";
    std::wstring wsItem = _T("");

    Json::Reader reader;

    Json::Value  root;
    Json::Value  CMDContent;

    wostringstream  wosError;

    if ( strJson.length() == 0)
    {
        wosError << _T("invalid param, strJson.length() == 0");
        goto END;
    }

    strValue = strJson;
    //补全 按数组解析
    if( strValue.substr(0, 1).compare("{") == 0)
    {
        strValue =  "[" + strValue;
        strValue +=  "]";
    }
	//wwdv2
    if (!reader.parse(strValue, root) || !root.isArray()|| root.size() < 1)
    {
        wosError << _T("parse fail, ") << _T("json=") << UTF8ToUnicode(strValue).c_str();
        goto END;
    }

    if(!root[0].isMember("CMDContent"))
    {
        wosError << _T("invalid json, no member : CMDContent");
        goto END;
    }
    CMDContent =  (Json::Value)root[0]["CMDContent"];

    // enable
    if (CMDContent.isMember("enable"))
    {
        bMaintenance = CMDContent["enable"].asInt64();
    }

    bResult = TRUE;

END:

    if (FALSE == bResult)
    {
        wsError = wosError.str();
    }

    return bResult;
}

std::string CWLJsonParse::Maintenance_Client(__in int bMaintenance, __in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID)
{    
    /*
    [{
    "ComputerID": "FEFOEACD",
    "CMDTYPE": 150,
    "cmdId":  512,
    "cmdContent": {
    "Maintenance": 1,
    }
    }]
    */

    std::string strJson = "";

    Json::Value root;
    Json::Value Item;
    Json::Value CMDContentItem;

    Json::FastWriter jsWriter;

    Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
    Item["CMDTYPE"] = (int)nCmdType; 
    Item["CMDID"] = (int)nCmdID;     

    CMDContentItem["enable"] = bMaintenance;

    Item["CMDContent"] = (Json::Value)CMDContentItem;

    root.append(Item);
    strJson = jsWriter.write(root);

    return strJson;
}

/*
* @fn           USM_LogServer_ChangeRet_ParseJson
* @brief        ChangeUSMIP模块客户端使用，解析返回客户端更改USM和LogServer的结果
* @param[in]    
* @return       
*               
* @author       zhicheng.sun
* @modify：		2023.8.29 create it.
*/
BOOL CWLJsonParse::DbrStartState_ParseJson(std::string strJson, DWORD &dwRet, __out tstring *pStrError/* = NULL */)
{
	BOOL bRes = FALSE;
	Json::Reader reader;
	Json::Value  root;
	Json::Value  CMDContent;
	wostringstream  wosError;

	std::string strValue = "";

	strValue = strJson;
	//补全 按数组解析
	if( strValue.substr(0, 1).compare("{") == 0)
	{
		strValue =  "[" + strValue;
		strValue +=  "]";
	}

	if ( strValue.length() == 0)
	{
		wosError << _T("invalid param, strJson.length() == 0") << _T(",");
		goto END;
	}
	//wwdv2
	if (!reader.parse(strValue, root) || !root.isArray() || root.size() < 1)
	{
		wosError << _T("parse fail, path=") << _T(", json=") << UTF8ToUnicode(strValue).c_str() << _T(",");
		goto END;
	}

	dwRet = (int)root[0]["CMDContent"]["CommandType"].asInt();

	bRes = TRUE;

END:
	if (pStrError)
	{
		*pStrError = wosError.str();
	}

	return bRes;
}

/*
* @fn           LogServerConfig_GetJson
* @brief        构建上报日志服务器配置的JSON数据
* @param[in]    tstrComputerID 计算机ID
* @param[in]    nCmdType 命令类型
* @param[in]    nCmdID 命令ID
* @param[in]    tstrLogServerIP 日志服务器IP
* @param[in]    dwLogServerPort 日志服务器端口
* @return       JSON字符串
*               
* @author       AI Assistant
* @modify：		2025.12.22 create it.
*/
std::string CWLJsonParse::LogServerConfig_GetJson(__in const tstring tstrComputerID, __in const CMDTYPE nCmdType, __in const WORD nCmdID, __in const tstring tstrLogServerIP, __in const DWORD dwLogServerPort)
{
	/*
	[{
		"ComputerID": "FEFOEACD",
		"CMDTYPE": 150,
		"CMDID":  168,
		"CMDContent": {
			"LogServerIP":"192.168.1.3",
			"LogServerPort":4565
		}
	}]
	*/

	std::string strJson = "";

	Json::Value root;
	Json::Value Item;
	Json::Value CMDContentItem;
	Json::FastWriter jsWriter;

	Item["ComputerID"] = UnicodeToUTF8(tstrComputerID);
	Item["CMDTYPE"] = (int)nCmdType;
	Item["CMDID"] = (int)nCmdID;

	CMDContentItem["LogServerIP"] = UnicodeToUTF8(tstrLogServerIP);
	CMDContentItem["LogServerPort"] = (int)dwLogServerPort;

	Item["CMDContent"] = (Json::Value)CMDContentItem;

	root.append(Item);
	strJson = jsWriter.write(root);

	return strJson;
}
