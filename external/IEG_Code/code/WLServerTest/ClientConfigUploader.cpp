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
  "UDiskControlMode": 1,
  "SafeUDiskControlMode": 1,
  "CDROMControlMode": 1,
  "WlanControlMode": 0,
  "UsbEthernetControlMode": 0,
  "NoRegCommonCtrlMode": 1,
  "NoRegSafeCtrlMode": 1,
  "RegUDiskCtrlMode": 1,
  "SafeKillVirusCtrlMode": 0,
  "CommonKillVirusCtrlMode": 0,
  "BlueToothControlMode": 1,
  "SerialPortControlMode": 1,
  "ParallelPortControlMode": 1,
  "FloppyDiskControlMode": 1,
  "UsbDeviceControlMode": 1,
  "WpdControlMode": 1,
  "UsbStorageCtrl": 1,
  "WifiWhiteList": 0,
  "dwPrinterControlMode": 1,
  "dwCameraControlMode": 1,
  "dwMicControlMode": 1,
  "dwPeripheralPolicy": 1,
  "peripheralWhiteList": [
    {
      "devId": "USB\VID_1234&PID_5678",
      "devName": "SafeUKey",
      "devType": 1
    }
  ],
  "peripheralBlackList": [
    {
      "devId": "USB\VID_9999&PID_0001",
      "devName": "UnknownDevice",
      "devType": 9
    }
  ],
  "dwSystemCheckStatus": 1,
  "dwBaselinePolicyId": 1001,
  "dwBaselineScore": 95,
  "dwBaselineCheckResult": 1,
  "baselineItems": [
    {
      "itemId": 1001,
      "itemName": "PasswordPolicy",
      "checkResult": 1,
      "riskLevel": 1
    },
    {
      "itemId": 1002,
      "itemName": "AuditPolicy",
      "checkResult": 1,
      "riskLevel": 1
    },
    {
      "itemId": 1003,
      "itemName": "FirewallStatus",
      "checkResult": 1,
      "riskLevel": 1
    },
    {
      "itemId": 1004,
      "itemName": "RegistryProtection",
      "checkResult": 1,
      "riskLevel": 2
    },
    {
      "itemId": 1005,
      "itemName": "FileProtection",
      "checkResult": 1,
      "riskLevel": 2
    },
    {
      "itemId": 1006,
      "itemName": "DataProtection",
      "checkResult": 1,
      "riskLevel": 2
    },
    {
      "itemId": 1007,
      "itemName": "VirusDefinition",
      "checkResult": 1,
      "riskLevel": 1
    },
    {
      "itemId": 1008,
      "itemName": "BehaviorMonitor",
      "checkResult": 1,
      "riskLevel": 2
    },
    {
      "itemId": 1009,
      "itemName": "BackupStatus",
      "checkResult": 1,
      "riskLevel": 3
    },
    {
      "itemId": 1010,
      "itemName": "MACPolicy",
      "checkResult": 1,
      "riskLevel": 3
    }
  ],
  "dwRegProtectStatus": 1,
  "dwRegProtectAlarmMode": 1,
  "dwRegProtectMode": 1,
  "dwRegAuditMode": 1,
  "regProtectItems": [
    {
      "key": "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run",
      "mode": 1
    },
    {
      "key": "HKLM\SYSTEM\CurrentControlSet\Services",
      "mode": 2
    }
  ],
  "dwFilePotectStatus": 1,
  "dwFileProtectAlarmMode": 1,
  "dwFileProtectMode": 1,
  "fileProtectPaths": [
    "C:\Windows\System32",
    "C:\Program Files\IEG"
  ],
  "fileProtectExts": [
    ".exe",
    ".dll",
    ".sys",
    ".drv"
  ],
  "dwFileAutoBackup": 1,
  "dwFileTrustMode": 0,
  "dwMACStatus": 0,
  "dwMACPolicy": 1,
  "dwMACLevel": 3,
  "macSubjectList": [
    {
      "name": "System",
      "sid": "S-1-5-18",
      "level": 3
    },
    {
      "name": "Administrators",
      "sid": "S-1-5-32-544",
      "level": 2
    }
  ],
  "macObjectList": [
    {
      "path": "C:\Secret",
      "level": 3,
      "type": 1
    },
    {
      "path": "C:\Classified",
      "level": 2,
      "type": 2
    }
  ],
  "dwDataPotectStatus": 1,
  "dwDataProtectionPolicy": 1,
  "dwDataEncryptStatus": 1,
  "dwDataBackupStatus": 0,
  "dataProtectScopes": [
    {
      "scopeName": "Confidential",
      "scopeType": 1,
      "encryptAlg": "AES-256"
    },
    {
      "scopeName": "Internal",
      "scopeType": 2,
      "encryptAlg": "SM4"
    }
  ],
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
  "dwIsUpLoadSysLog": 1,
  "dwUpLoadSysLogCycleDay": 1,
  "uploadSpeed": 0,
  "scanControl": 0,
  "UploadSysWLFile": 1,
  "LogServerIP": "",
  "LogServerPort": 0,
  "dwIsProcessAudit": 1,
  "dwProcessAuditPolicy": 1,
  "processAuditItems": [
    {
      "procName": "cmd.exe",
      "auditType": 1,
      "action": 1
    },
    {
      "procName": "powershell.exe",
      "auditType": 2,
      "action": 1
    },
    {
      "procName": "wscript.exe",
      "auditType": 1,
      "action": 1
    }
  ],
  "loopholeProtect": 1,
  "dwVulScanResult": 0,
  "dwVulFixStatus": 1,
  "vulDbVersion": "20261013.001",
  "dwVulAutoFix": 1,
  "dwVulScanScope": 1,
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
  "dwTrustZoneEnabled": 1,
  "trustZoneList": [
    {
      "path": "C:\IEG\Trust",
      "type": 1,
      "desc": "internal tools"
    },
    {
      "path": "D:\DevTools",
      "type": 2,
      "desc": "development zone"
    }
  ],
  "dwIsolationZoneEnabled": 1,
  "isolationZonePath": "C:\IEG\Isolation",
  "isolationZoneCapacity": 10240,
  "isolationZoneUsage": 512,
  "isolationZoneItems": [
    {
      "fileName": "suspicious.exe",
      "originPath": "C:\Users\Test\Downloads",
      "isolateTime": "2026-10-13T14:00:00Z",
      "threatLevel": 3
    },
    {
      "fileName": "trojan.dll",
      "originPath": "C:\Users\Test\AppData\Local\Temp",
      "isolateTime": "2026-10-12T10:30:00Z",
      "threatLevel": 5
    }
  ],
  "dwVirusRealTimeScan": 1,
  "dwVirusScheduledScan": 1,
  "virusEngineVersion": "V2026.10.01.001",
  "virusDbVersion": "20261013.001",
  "dwVirusScanScope": 1,
  "dwVirusHealMode": 1,
  "dwVirusQuarantineMode": 1,
  "dwVirusScanResult": 0,
  "dwBehaviorMonitorPolicy": 1,
  "dwBehaviorBlockMode": 1,
  "behaviorMonitorItems": [
    {
      "ruleId": 2001,
      "ruleName": "RansomwareBehavior",
      "ruleType": 1,
      "severity": 5,
      "status": 1
    },
    {
      "ruleId": 2002,
      "ruleName": "DataExfiltration",
      "ruleType": 2,
      "severity": 4,
      "status": 1
    },
    {
      "ruleId": 2003,
      "ruleName": "PrivilegeEscalation",
      "ruleType": 3,
      "severity": 5,
      "status": 1
    },
    {
      "ruleId": 2004,
      "ruleName": "LateralMovement",
      "ruleType": 4,
      "severity": 4,
      "status": 1
    }
  ],
  "dwBackupPolicy": 1,
  "dwBackupScheduleType": 1,
  "backupStoragePath": "C:\IEG\Backup",
  "dwBackupLastTime": 0,
  "dwBackupStatus": 1,
  "dwRecoveryStatus": 0,
  "dwThreatDetectStatus": 1,
  "dwThreatDetectLevel": 2,
  "dwHostDefenceStatus": 1,
  "dwSysProtectStatus": 1,
  "networkAdapterInfo": [
    {
      "name": "Ethernet0",
      "mac": "00-50-56-C0-00-08",
      "ip": "192.168.1.100",
      "type": 1
    },
    {
      "name": "Wi-Fi",
      "mac": "00-50-56-C0-00-09",
      "ip": "192.168.1.101",
      "type": 2
    }
  ],
  "extDevPolicy": 1,
  "extDevSubTypeMask": 0,
  "dwSafetyStoreEnabled": 1,
  "safetyStoreList": [
    {
      "storeName": "VaultA",
      "storePath": "C:\IEG\VaultA",
      "capacity": 5120,
      "usage": 1024
    }
  ],
  "dwNwlPolicy": 1,
  "nwlWhiteList": [
    {
      "ip": "10.0.0.0",
      "mask": "255.0.0.0",
      "desc": "intranet"
    }
  ],
  "dwOptLogEnabled": 1,
  "dwOptLogLevel": 2,
  "dwThtLogEnabled": 1,
  "dwThtLogLevel": 3,
  "dwAdminAuditEnabled": 1,
  "dwAdminPrivilegeLimit": 1
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
			baseRoot[members[i]] = root[members[i]];
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
