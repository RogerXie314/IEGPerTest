#pragma once

class client
{
private:
	CString m_clientID;     // 改进钱用作ComputerID，现暂用于保存用户定义的客户端前缀
	CString m_csComputerID_PreClientIDSuf;
	CString m_clientIP;

	BOOL m_bHasHeartBeatSent;   // 当前客户端是否已在发送心跳

	BOOL m_bHasFileLogSent; // 当前客户端文件类型日志上传标记

	BOOL m_bHasMsgLogSent;  // 当前客户端消息类型日志上传标记
	
	BOOL m_bRegistered; //当前客户端是否是已注册状态

public:
	client();
	client(CString inClientID, __in CString inClientIP);
	~client(void);

	CString GetClientID();
	CString GetClientIP();

	BOOL Get_IsThisClientSendingHeartBeat();
	BOOL ThisClient_IsSendingFileLog();
	BOOL ThisClient_IsSendingMsgLog();

	BOOL Set_IsSendingHeartBeat(BOOL _in_isHeartbeated);
	BOOL Set_IsSendingFileLog(BOOL _in_isSendFileLoged);
	BOOL Set_IsSendingMsgLog(BOOL _in_isSendOptLoged);

	BOOL    Client_SetComputerID();
	CString Client_GetComputerID();
	
	BOOL    Client_IsRegistered();
	void    Client_SetRegistered(BOOL bRegistered);
};