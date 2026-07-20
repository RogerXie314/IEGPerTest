#pragma once

class client
{
private:
	CString m_clientID;     // �Ľ�Ǯ����ComputerID���������ڱ����û�����Ŀͻ���ǰ׺
	CString m_csComputerID_PreClientIDSuf;
	CString m_clientIP;

	BOOL m_bHasHeartBeatSent;   // ��ǰ�ͻ����Ƿ����ڷ�������

	BOOL m_bHasFileLogSent; // ��ǰ�ͻ����ļ�������־�ϴ����

	BOOL m_bHasMsgLogSent;  // ��ǰ�ͻ�����Ϣ������־�ϴ����
	
	BOOL m_bRegistered; //��ǰ�ͻ����Ƿ�����ע��״̬
	DWORD m_dwDevID;      // devid returned from server registration

public:
	client();
	client(CString inClientID, __in CString inClientIP);
	~client(void);

	CString GetClientID();
	CString GetClientIP();

	DWORD GetDevID() const { return m_dwDevID; }
	void  SetDevID(DWORD dwDevID) { m_dwDevID = dwDevID; }

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