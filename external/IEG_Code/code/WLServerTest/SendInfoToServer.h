#pragma once

// Global debug toggle — controlled by IDC_CHECK_DEBUG in main dialog
extern BOOL g_bEnableDebugOutput;
#include "client.h"
#include "..\BaseLineStruct.h"
#include "..\WLCredentialMgr\CredentialStruct.h"
#include "..\WLCConfig\WLOSUserManage.h"


#define FU_MAX_FILE_LEN         2048
class CSendInfoToServer
{
public:
	CSendInfoToServer(void);

	CSendInfoToServer(CString strServerIP, CString strServerPort,CString strIdPre);

	~CSendInfoToServer(void);

	CString m_strIdPre;
	CString m_strServerIP;
	CString m_strServerPort;
	CString m_ComputerID;
	CString m_Domain;
	CString m_ClientLanguage;
	CString m_WindowsOSVersion;
        CString m_strClientIP;       // ???????????????IP



	char* wchar2char(const wchar_t* wchar);
	wchar_t* char2wchar(const char* cchar);

	// TCP-ThreatLog ??????  added by lzq:MAY19
	// TCP - ??????в???
	BOOL SendThreatLog_ToserverTCP(client& pCurClient,SOCKET sockSend, DWORD dwSubTypes = 0xFFFFFFFF, BOOL bHit = TRUE);
	BOOL SendThreatLog_Data(SOCKET sockSend, const char *pSendBuff, unsigned int nSendLen,int cmdID);

	// TCP - ????????????????????????????????????????????????????????HTTPS????????????????????
	DWORD RecvThreatLog_(SOCKET sockRecv);
	UINT RecvData_ThreatLog_(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen);

	// HTTPS - ????????hm
	BOOL SendThreatLog_(client& curClient);

	// HTTPS - ???????HTTPS??????????
	DWORD ParseRevData_ThreatLog(std::string strJson);


	
    // TCP - ????????
	BOOL SendHeartbeatToserverTCP(client& pCurClient,SOCKET sockSend);
    BOOL SendData(SOCKET sockSend, const char *pSendBuff, unsigned int nSendLen,int cmdID);

	BOOL SendData_OnlyCompress(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID, DWORD dwDeviceID = 0);//2.threatlog

    // TCP - ????????????????????????????????????????????????????????HTTPS????????????????????
    DWORD RecvHeartbeat(SOCKET sockRecv);
    UINT RecvData(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen);

    // HTTPS - ????????hm
	BOOL SendHeartbeat(client& curClient);

    // HTTPS - ???????HTTPS??????????
    DWORD ParseRevData(std::string strJson);

    // HTTPS - ????????????USM
    BOOL SendExecResult(WORD CMDID, int nDealResult, char *pResultJson);
    BOOL SendExecResult(WORD CMDID, int nDealResult);

    // HTTPS - ????CMDID??????????????
    WORD GetCMDTYPE(WORD CMDID);

    //
	BOOL RegisterClientToServer(CString szComputerID, CString szClientID,CString szComputerIP, CString szVersion = _T("V300R011C01B090"), CString szOS = _T("Windows 10"), DWORD* pdwOutDevID = NULL); // v6.5: out devid

	// ??????????????
	BOOL SendClientOptLogToServer(LPTSTR lpComputerID);

	BOOL SendClientNwlLogToServer_SingleRule(LPTSTR lpComputerID);
	BOOL CSendInfoToServer::SendClientNwlLogToServer_FiveType(LPTSTR lpComputerID);

	// ??????????в???
	//BOOL SendClientThtLogToServer(LPTSTR lpComputerID,int iCurUsedClient);
  
	//??????????????
	BOOL SendBaseLineToServer(LPTSTR lpComputerID);
	BOOL UkeyToNotifyUSM(LPTSTR lpComputerID, WORD cmdType, WORD cmdID, BASELINE_PL_NEW_ST *pSecbStatus, BASELINE_PL_NEW_ST *pSecbParam, DWORD dwLevel);
	//????UKey???
	BOOL SendUSBKeyManageToServer(LPTSTR lpComputerID,VEC_ST_USERS_USM &vecUSMUsersSend);
	BOOL BOSendUSM_AllUsers_DoPost(LPTSTR lpComputerID, __in ST_USERS_INFO_HEAD & stUsersHead, VEC_ST_USERS_USM & vecUSMUsersSend);
	// ???????????????
	BOOL Send_FileLog_WL_ToServer(LPTSTR lpComputerID, CString cstrWLFilePath);
	BOOL CreateConnection(SOCKET &sockClient,CString strServerIP,const CString strServerPort);
	char* RecvSockData(SOCKET sockRecv, unsigned int &nSrcLen, unsigned int &dwCmdID);
	BOOL RecvData(SOCKET sockRecv, char *pRecvBuff, int nRecvLen);
	BOOL CloseConnection(SOCKET sockClose);
	void SetClientIP(const CString& ip) { m_strClientIP = ip; }
        void InitParm(CString sComputerID,CString sDomain,CString sClientLanguage,CString sWindowsVersion);
	BOOL CreateGuidString(LPTSTR lpGuid);
	void sendScanStatus(LPTSTR lpGuid, DWORD dwScanStatus);

	BOOL SendClientDataProtectLogToServer(LPTSTR lpComputerID);
	BOOL SendClientSysProtectLogToServer(LPTSTR lpComputerID);
	BOOL SendClientBackupLogToServer(LPTSTR lpComputerID);
	BOOL SendClientVirusLogToServer(LPTSTR lpComputerID);
	BOOL SendClientNetAdapterLogToServer(LPTSTR lpComputerID);
	BOOL SendClientUDiskPlugLogToServer(LPTSTR lpComputerID);
	BOOL SendClientExtDevLogToServer(LPTSTR lpComputerID, DWORD dwSubTypeMask);
    // New per-category HTTPS senders (aligned with C# SimulatorApp)
    BOOL SendClientProcessAlertLogToServer(LPTSTR lpComputerID, int type = 2, int subType = 6);
    BOOL SendClientSysFileCheckLogToServer(LPTSTR lpComputerID);
    BOOL SendClientAdminLogToServer(LPTSTR lpComputerID);
    BOOL SendClientUsbLogToServer(LPTSTR lpComputerID);
    BOOL SendClientUsbWarningLogToServer(LPTSTR lpComputerID);
    BOOL SendClientFirewallLogToServer(LPTSTR lpComputerID);
    BOOL SendClientOsResourceLogToServer(LPTSTR lpComputerID);
    BOOL SendClientRegProtectLogToServer(LPTSTR lpComputerID);
    BOOL SendClientMacProtectLogToServer(LPTSTR lpComputerID);
    BOOL SendClientSafetyStoreLogToServer(LPTSTR lpComputerID);
    BOOL SendClientThreatFakeLogToServer(LPTSTR lpComputerID);
    BOOL SendClientIllegalConnectLogToServer(LPTSTR lpComputerID);
BOOL SendClientHostDefenceLogToServer(LPTSTR lpComputerID, int logType);
    BOOL SendClientVulDefenseLogToServer(LPTSTR lpComputerID);

    // HTTPS - 客户端上报最新配置（CMDTYPE=200, CMDID=201, /USM/clientRefresh.do）
    // lpConfigJson: CMDContent 级 UTF-8 JSON（由 CClientConfigUploader 加载）
    // 返回值三态：CFG_UPLOAD_RET_OK / CFG_UPLOAD_RET_FAIL_REPLY / CFG_UPLOAD_RET_NET_ERROR
    
    // Full-spec policy rule upload APIs (aligned with IEG client interfaces)
    BOOL SendClientFileProtectToServer(LPTSTR lpComputerID);
    BOOL SendClientRegProtectToServer(LPTSTR lpComputerID);
    BOOL SendClientProcessAuditToServer(LPTSTR lpComputerID);
    BOOL SendClientMacSubObjToServer(LPTSTR lpComputerID);
    BOOL SendClientTrustProcessToServer(LPTSTR lpComputerID);
    BOOL SendClientTrustPathToServer(LPTSTR lpComputerID);
    BOOL SendClientFileProtectExceptionToServer(LPTSTR lpComputerID);
    BOOL SendClientRegProtectExceptionToServer(LPTSTR lpComputerID);
    BOOL SendClientOSUserToServer(LPTSTR lpComputerID);
int SendClientConfigToServer(LPTSTR lpComputerID, LPTSTR lpComputerIP, LPCSTR lpConfigJsonUtf8);
    // Fix11: missing policy rule upload APIs
    BOOL SendClientOSResourceToServer(LPTSTR lpComputerID);
    BOOL SendClientScanExcludeToServer(LPTSTR lpComputerID);
    BOOL SendClientUsbWhitelistToServer(LPTSTR lpComputerID);
    BOOL SendClientUDiskToServer(LPTSTR lpComputerID);
    BOOL SendClientWiFiToServer(LPTSTR lpComputerID);
    BOOL SendClientIllegalConnectToServer(LPTSTR lpComputerID);
    BOOL SendClientFirewallToServer(LPTSTR lpComputerID);
    BOOL SendClientBaseLineToServer(LPTSTR lpComputerID);
    BOOL SendClientUSBKeyToServer(LPTSTR lpComputerID);
    BOOL SendClientMacDomainToServer(LPTSTR lpComputerID);
    BOOL SendClientThreatDetectToServer(LPTSTR lpComputerID);
    BOOL SendClientWorkSheetToServer(LPTSTR lpComputerID);
    BOOL SendClientWorkSheetResultToServer(LPTSTR lpComputerID);
    BOOL SendClientMacExceptionToServer(LPTSTR lpComputerID);
    BOOL SendClientTrustSignerToServer(LPTSTR lpComputerID);
    BOOL SendClientVirusUpgradeToServer(LPTSTR lpComputerID);
    BOOL SendClientSurveilToServer(LPTSTR lpComputerID);
    BOOL SendClientDataGuardToServer(LPTSTR lpComputerID);
    BOOL SendClientBackupToServer(LPTSTR lpComputerID);


};

// 客户端配置上报返回值（SendClientConfigToServer）
#define CFG_UPLOAD_RET_OK           0   // pdoPost 成功且回包 RESULT=SUC
#define CFG_UPLOAD_RET_FAIL_REPLY   1   // pdoPost 成功但回包 RESULT=FAIL 或回包解析失败
#define CFG_UPLOAD_RET_NET_ERROR    2   // pdoPost 失败（网络不通/超时/HTTP 错误）


