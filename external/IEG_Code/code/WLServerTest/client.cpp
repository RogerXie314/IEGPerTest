#include "StdAfx.h"
#include "client.h"


client::client(void){}

client::client(__in CString inClientID, __in CString inClientIP)
{
	m_clientIP = inClientIP;
	m_clientID = inClientID;

	m_bHasHeartBeatSent = FALSE;
	m_bHasFileLogSent = FALSE;
	m_bHasMsgLogSent = FALSE;
	m_bRegistered = FALSE;
}

client::~client(void) 
{
}
CString client::GetClientIP()
{
	return m_clientIP;
}

CString client::GetClientID()
{
	return m_clientID;
}

BOOL client::Get_IsThisClientSendingHeartBeat()
{
	return m_bHasHeartBeatSent;
}
BOOL client::ThisClient_IsSendingMsgLog()
{
	return m_bHasMsgLogSent;
}

BOOL client::Client_IsRegistered()
{
    return m_bRegistered;
}

void client::Client_SetRegistered(BOOL bRegistered)
{
    m_bRegistered = bRegistered;
    return;
}

BOOL client::Client_SetComputerID()
{
	CString csPrefix_ClientID_Suffix = _T("FFFFFFFF");//8F  12-0
	if (0 == m_clientID.GetLength())
	{
		return FALSE;
	} 
	csPrefix_ClientID_Suffix += m_clientID;
	csPrefix_ClientID_Suffix += _T("000000000000");

	m_csComputerID_PreClientIDSuf = csPrefix_ClientID_Suffix;

	return TRUE;
}
CString client::Client_GetComputerID()
{
	return m_csComputerID_PreClientIDSuf;
}
BOOL client::ThisClient_IsSendingFileLog()
{
	return m_bHasFileLogSent;
}



BOOL client::Set_IsSendingFileLog(BOOL _in_isSendFileLoged)
{
	m_bHasFileLogSent = _in_isSendFileLoged;
	return TRUE;
}

BOOL client::Set_IsSendingHeartBeat(BOOL _in_isHeartbeated)
{
	m_bHasHeartBeatSent = _in_isHeartbeated;
	return TRUE;
}

BOOL client::Set_IsSendingMsgLog(BOOL _in_isSendMsgLoged)
{
	m_bHasMsgLogSent = _in_isSendMsgLoged;
	return TRUE;
}