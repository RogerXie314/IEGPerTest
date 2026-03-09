#pragma once
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



	char* wchar2char(const wchar_t* wchar);
	wchar_t* char2wchar(const char* cchar);

	// TCP-ThreatLog 发送接口  added by lzq:MAY19
	// TCP - 发送威胁日志
	BOOL SendThreatLog_ToserverTCP(client& pCurClient,SOCKET sockSend, BOOL bHit = TRUE);
	BOOL SendThreatLog_Data(SOCKET sockSend, const char *pSendBuff, unsigned int nSendLen,int cmdID);

	// TCP - 确认是否有数据可以允许接收，接收时确认返回的数据类型。是策略时，使用HTTPS执行数据的完整接收和处理
	DWORD RecvThreatLog_(SOCKET sockRecv);
	UINT RecvData_ThreatLog_(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen);

	// HTTPS - 接收数据hm
	BOOL SendThreatLog_(client& curClient);

	// HTTPS - 本地解析HTTPS到来的数据
	DWORD ParseRevData_ThreatLog(std::string strJson);


	
    // TCP - 发送心跳
	BOOL SendHeartbeatToserverTCP(client& pCurClient,SOCKET sockSend);
    BOOL SendData(SOCKET sockSend, const char *pSendBuff, unsigned int nSendLen,int cmdID);

	BOOL SendData_OnlyCompress(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID);//2.threatlog

    // TCP - 确认是否有数据可以允许接收，接收时确认返回的数据类型。是策略时，使用HTTPS执行数据的完整接收和处理
    DWORD RecvHeartbeat(SOCKET sockRecv);
    UINT RecvData(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen);

    // HTTPS - 接收数据hm
	BOOL SendHeartbeat(client& curClient);

    // HTTPS - 本地解析HTTPS到来的数据
    DWORD ParseRevData(std::string strJson);

    // HTTPS - 返回解析结果给USM
    BOOL SendExecResult(WORD CMDID, int nDealResult, char *pResultJson);
    BOOL SendExecResult(WORD CMDID, int nDealResult);

    // HTTPS - 解析CMDID属于策略还是命令
    WORD GetCMDTYPE(WORD CMDID);

    //
	BOOL RegisterClientToServer(CString szComputerID, CString szClientID,CString szComputerIP);

	// 发送客户端操作日志
	BOOL SendClientOptLogToServer(LPTSTR lpComputerID);

	BOOL SendClientNwlLogToServer_SingleRule(LPTSTR lpComputerID);
	BOOL CSendInfoToServer::SendClientNwlLogToServer_FiveType(LPTSTR lpComputerID);

	// 发送客户端威胁日志
	//BOOL SendClientThtLogToServer(LPTSTR lpComputerID,int iCurUsedClient);
  
	//发送客户端安全基线
	BOOL SendBaseLineToServer(LPTSTR lpComputerID);
	BOOL UkeyToNotifyUSM(LPTSTR lpComputerID, WORD cmdType, WORD cmdID, BASELINE_PL_NEW_ST *pSecbStatus, BASELINE_PL_NEW_ST *pSecbParam, DWORD dwLevel);
	//发送UKey信息
	BOOL SendUSBKeyManageToServer(LPTSTR lpComputerID,VEC_ST_USERS_USM &vecUSMUsersSend);
	BOOL BOSendUSM_AllUsers_DoPost(LPTSTR lpComputerID, __in ST_USERS_INFO_HEAD & stUsersHead, VEC_ST_USERS_USM & vecUSMUsersSend);
	// 发送程序白名单日志
	BOOL Send_FileLog_WL_ToServer(LPTSTR lpComputerID, CString cstrWLFilePath);
	BOOL CreateConnection(SOCKET &sockClient,CString strServerIP,const CString strServerPort);
	char* RecvSockData(SOCKET sockRecv, unsigned int &nSrcLen, unsigned int &dwCmdID);
	BOOL RecvData(SOCKET sockRecv, char *pRecvBuff, int nRecvLen);
	BOOL CloseConnection(SOCKET sockClose);
	void InitParm(CString sComputerID,CString sDomain,CString sClientLanguage,CString sWindowsVersion);
	BOOL CreateGuidString(LPTSTR lpGuid);
	void sendScanStatus(LPTSTR lpGuid, DWORD dwScanStatus);

	BOOL SendClientDataProtectLogToServer(LPTSTR lpComputerID);
	BOOL SendClientSysProtectLogToServer(LPTSTR lpComputerID);
	BOOL SendClientBackupLogToServer(LPTSTR lpComputerID);
	BOOL SendClientVirusLogToServer(LPTSTR lpComputerID);
};

