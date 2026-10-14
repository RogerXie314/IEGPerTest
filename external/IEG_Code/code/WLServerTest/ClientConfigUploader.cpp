#include "StdAfx.h"
#include "ClientConfigUploader.h"

#include "../include/WLUtilities/json/json.h"

// Get exe directory (e.g. D:\...\Release)
static BOOL GetExeDirectory(std::wstring& strDir)
{
	TCHAR szPath[MAX_PATH] = {0};
	DWORD dwLen = ::GetModuleFileName(NULL, szPath, MAX_PATH - 1);
	if (dwLen == 0)
		return FALSE;
	std::wstring strFull(szPath);
	size_t nPos = strFull.find_last_of(L"\\/");
	if (nPos == std::wstring::npos)
		return FALSE;
	strDir = strFull.substr(0, nPos);
	return TRUE;
}

// Read text file into std::string (raw bytes; caller handles encoding)
static BOOL ReadFileToUtf8(const std::wstring& strPath, std::string& strOut)
{
	HANDLE hFile = ::CreateFileW(strPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	DWORD dwSize = ::GetFileSize(hFile, NULL);
	if (dwSize == 0 || dwSize == INVALID_FILE_SIZE)
	{
		::CloseHandle(hFile);
		return FALSE;
	}

	strOut.resize(dwSize);
	DWORD dwRead = 0;
	BOOL bOk = ::ReadFile(hFile, &strOut[0], dwSize, &dwRead, NULL);
	::CloseHandle(hFile);
	if (!bOk || dwRead != dwSize)
	{
		strOut.clear();
		return FALSE;
	}

	// strip UTF-8 BOM if present
	if (strOut.size() >= 3
		&& (unsigned char)strOut[0] == 0xEF
		&& (unsigned char)strOut[1] == 0xBB
		&& (unsigned char)strOut[2] == 0xBF)
	{
		strOut = strOut.substr(3);
	}
	return TRUE;
}

const char* CClientConfigUploader::GetDefaultTemplate()
{
	static const char* s_pDefault =
		R"CFGJSON({
{
  "ComputerName": "IEG-SIM-PC",
  "ComputerIP": "",
  "ComputerMAC": "00-50-56-C0-00-08",
  "WindowsVersion": "Windows 10",
  "szCurVersion": "V300R011C01B100",
  "srsVersion": "",
  "WindowsX64": 1,
  "SelfProtectStatus": 1,
  "RealtimeAlertStatus": 1,
  "SolidifyStatus": 4,
  "ProcessProtectMode": 1,
  "ProcessControlMode": 1,
  "AutoApproval": 1,
  "UDiskControlMode": 0,
  "SafeUDiskControlMode": 0,
  "CDROMControlMode": 1,
  "WlanControlMode": 0,
  "UsbEthernetControlMode": 0,
  "NoRegCommonCtrlMode": 1,
  "NoRegSafeCtrlMode": 1,
  "RegUDiskCtrlMode": 0,
  "SafeKillVirusCtrlMode": 0,
  "CommonKillVirusCtrlMode": 0,
  "BlueToothControlMode": 1,
  "SerialPortControlMode": 1,
  "ParallelPortControlMode": 1,
  "FloppyDiskControlMode": 1,
  "UsbDeviceControlMode": 0,
  "WpdControlMode": 1,
  "UsbStorageCtrl": 0,
  "WifiWhiteList": 0,
  "dwSystemCheckStatus": 1,
  "dwRegProtectStatus": 1,
  "dwRegProtectAlarmMode": 1,
  "dwFilePotectStatus": 1,
  "dwFileProtectAlarmMode": 1,
  "dwMACStatus": 1,
  "dwDataPotectStatus": 1,
  "dwPasswordComplexity": 1,
  "dwMin_passwd_len": 8,
  "dwPassword_hist_len": 5,
  "dwMax_passwd_age": 90,
  "dwLockout_threshold": 5,
  "dwDisable_guest": 1,
  "dwUsrmod3_lockout_observation_window": 0,
  "dwUsrmod3_lockout_duration": 0,
  "dwAuditCategoryLogon": 1,
  "dwAuditCategoryAccountManagement": 1,
  "dwAuditCategoryAccountLogon": 1,
  "dwAuditCategoryObjectAccess": 1,
  "dwAuditCategoryPolicyChange": 1,
  "dwAuditCategoryPrivilegeUse": 1,
  "dwAuditCategorySystem": 1,
  "dwAuditCategoryDetailedTracking": 0,
  "dwAuditCategoryDirectoryServiceAccess": 0,
  "dwClearPageShutDown": 1,
  "dwDontDisplayLastUserName": 1,
  "dwDisableCAD": 1,
  "dwRestrictAnonymousSam": 1,
  "dwRestrictAnonymous": 1,
  "dwAutoRun": 0,
  "dwShare": 0,
  "dwApplicationLog": 1,
  "dwSeclog": 1,
  "dwSystemLog": 1,
  "dwCachedLogonsCount": 10,
  "dwDisableDomainCreds": 1,
  "dwEnableUac": 1,
  "dwForbidAutoLogin": 0,
  "dwForbidAutoReboot": 0,
  "dwForbidAutoShutdown": 0,
  "dwForbidChangeIp": 0,
  "dwForbidChangeName": 0,
  "dwForbidConsoleAutoLogin": 0,
  "dwForbidFloppyCopy": 0,
  "dwForbidGethelp": 0,
  "dwForcedLogoff": 0,
  "dwRdpRortNum": 0,
  "dwDepIn": 1,
  "dwDepOut": 1,
  "dwScrnSave": 0,
  "dwPasswordExpiryWarning": 1,
  "dwDeleteIpForwardEntry": 0,
  "dwForbidAdminToTurnOff": 1,
  "dwRemoteHostRDP": 0,
  "dwRemoteLoginTime": 0,
  "dwForbidDefaultOPTUser": 1,
  "dwSynAttackDetectionDesign": 0,
  "dwSynIpDefence": 1,
  "dwFireWall": 1,
  "dwFireWallState": 1,
  "dwIsUpLoadSysLog": 0,
  "dwUpLoadSysLogCycleDay": 0,
  "dwIsProcessAudit": 1,
  "uploadSpeed": 0,
  "scanControl": 0,
  "loopholeProtect": 1,
  "SecDetectCheckStatus": 1,
  "SecDetectTimeType": 1,
  "SecDectDay": 1,
  "SecDectHour": 0,
  "SecDectMinute": 0,
  "IllegalConnect": 0,
  "hardDiskInfo": [
    {
      "letter": "C:",
      "capacity": 204800,
      "remainder": 102400
    },
    {
      "letter": "D:",
      "capacity": 512000,
      "remainder": 256000
    }
  ],
  "dwMaintainUkeyStatus": 0,
  "UploadSysWLFile": 1,
  "LogServerIP": "",
  "LogServerPort": 0
}
})CFGJSON";
	return s_pDefault;
}

BOOL CClientConfigUploader::LoadClientConfigTemplate(std::string& strCmdContentJson, CString& strErrMsg)
{
	strErrMsg.Empty();

	std::wstring strExeDir;
	if (!GetExeDirectory(strExeDir))
	{
		strCmdContentJson = GetDefaultTemplate();
		strErrMsg = _T("cannot get exe directory, use built-in default template");
		return FALSE;
	}

	std::wstring strCfgPath = strExeDir + L"\\clientConfig.json";
	std::string strFile;
	if (!ReadFileToUtf8(strCfgPath, strFile))
	{
		strCmdContentJson = GetDefaultTemplate();
		strErrMsg.Format(_T("config file not found (%s), use built-in default template"), strCfgPath.c_str());
		return FALSE;
	}

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(strFile, root) || !root.isObject())
	{
		strCmdContentJson = GetDefaultTemplate();
		strErrMsg.Format(_T("config file parse failed (%s), use built-in default template"), strCfgPath.c_str());
		return FALSE;
	}

	// ???????????????????????¦È???????????????????????????????????????
	Json::Value baseRoot;
	if (reader.parse(GetDefaultTemplate(), baseRoot) && baseRoot.isObject())
	{
		Json::Value::Members members = root.getMemberNames();
		for (size_t i = 0; i < members.size(); ++i)
		{
			// Only override fields that the IEG standard template already has,
			// ignore unknown/legacy custom fields to keep the payload standard.
			if (baseRoot.isMember(members[i]))
			{
				baseRoot[members[i]] = root[members[i]];
			}
		}
		Json::FastWriter writer;
		strCmdContentJson = writer.write(baseRoot);
	}
	else
	{
		// ?????????????????????????????????????
		strCmdContentJson = strFile;
	}
	return TRUE;
}
