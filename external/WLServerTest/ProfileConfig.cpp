#include "StdAfx.h"
#include "ProfileConfig.h"
#include <new>


CProfileConfig* CProfileConfig::m_pProfileConfig = NULL;

CProfileConfig::CProfileConfig(void)
{
	m_hMutex = ::CreateMutex(NULL, FALSE, NULL);
	if (!m_hMutex) 		
		exit(0);

	TCHAR szPath[MAX_PATH] = {0};
	GetModuleFileName(NULL, szPath, MAX_PATH-1);

	TCHAR* pFinder = _tcsrchr(szPath, _T('\\'));
	if (pFinder)
	{
		*++pFinder = _T('\0');   
	}
	
	_tcscat(szPath, _T("WLServerTest.ini"));
	_tcscpy(m_szConfPath, szPath);
}

CProfileConfig::~CProfileConfig(void)
{

	if (m_hMutex)
	{
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
	} 
}

CProfileConfig* CProfileConfig::GetProfileConfigInstance()
{
	if (NULL == m_pProfileConfig)
	{
		try
		{
			m_pProfileConfig = new CProfileConfig();
		}
		catch (std::bad_alloc)
		{
			m_pProfileConfig = NULL;
			OutputDebugString(_T("new failed"));
		}

		return m_pProfileConfig;
	} 
	else
	{
		return m_pProfileConfig;
	}
}


BOOL CProfileConfig::Lock(DWORD dwWaitTime/* = INFINITE*/)
{
	if (!m_hMutex) 
	{
		return FALSE;
	}

	switch(::WaitForSingleObject(m_hMutex, dwWaitTime))
	{
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED:
			return TRUE;	// success

		case WAIT_TIMEOUT:
			return FALSE;

		case WAIT_FAILED:
			return FALSE;

		default:
			return FALSE;
	}	
}

BOOL CProfileConfig::Unlock()
{
	if (!m_hMutex) 
	{
		return FALSE;
	}

	if (!::ReleaseMutex(m_hMutex))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CProfileConfig::WriteProfileString_ToIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpString)
{
	BOOL bRet = FALSE;
	//Lock();
	bRet = WritePrivateProfileString(lpAppName, lpKeyName, lpString, m_szConfPath);
	//Unlock();
	return bRet;
}

BOOL CProfileConfig::ReadProfileString_FromIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPTSTR lpReturnedString)
{
	BOOL bRet = FALSE;
	//Lock();
	bRet = GetPrivateProfileString(lpAppName, lpKeyName, NULL, lpReturnedString, 100, m_szConfPath);
	//Unlock();
	return bRet;
}

BOOL CProfileConfig::WriteProfileInt_ToIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, int nValue)
{
	TCHAR szValue[MAX_PATH] = {0};
	_stprintf(szValue, _T("%d"), nValue);
	return WriteProfileString_ToIni(lpAppName, lpKeyName, szValue);
}

int CProfileConfig::ReadProfileInt_FromIni(LPCTSTR lpAppName, LPCTSTR lpKeyName)
{
	TCHAR szValue[MAX_PATH] = {0};
	int iValue = -1;
	if (ReadProfileString_FromIni(lpAppName, lpKeyName, szValue))
	{
		iValue = _ttoi(szValue);
	}

	return iValue;	
}



BOOL CProfileConfig::WriteServerIP_ToIni(CString cstrIP)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ServerIPAddress"), cstrIP);
}

CString CProfileConfig::ReadServerIP_FromIni()
{
	CString cstrIP;
	TCHAR szIP[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ServerIPAddress"), szIP);
	cstrIP.Format(_T("%s"), szIP);

	return cstrIP;
}

CString CProfileConfig::ReadServerPort_FromIni()
{
	CString cstrPort;
	TCHAR szPort[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ServerPort"), szPort);
	cstrPort.Format(_T("%s"), szPort);

	return cstrPort;
}
CString CProfileConfig::ReadServerHBPort_FromIni()
{
	CString cstrPortHB;
	TCHAR szPortHB[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ServerPortHB"), szPortHB);
	cstrPortHB.Format(_T("%s"), szPortHB);

	return cstrPortHB;
}



BOOL CProfileConfig::WriteServerPort_ToIni(CString cstrServerPort)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ServerPort"), cstrServerPort);
}

BOOL CProfileConfig::WriteServerHBPort_ToIni(CString cstrServerPortHB)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ServerPortHB"), cstrServerPortHB);
}

BOOL CProfileConfig::WriteClientIdPrefix_ToIni(CString cstrIdPre)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ClientID_Prefix"),cstrIdPre);
}

CString CProfileConfig::ReadEachClientIdPrefix_FromIni()
{

	CString csIdPre;
	TCHAR szIdPre[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ClientID_Prefix"), szIdPre);
	csIdPre.Format(_T("%s"), szIdPre);

	return csIdPre;
}



BOOL CProfileConfig::WriteClientStartNum_ToIni(CString cstrStartNum)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ClientID_StartNumber"),cstrStartNum);
}

CString CProfileConfig::ReadClientStartNum_FromIni()
{

	CString cstrStartNum;
	TCHAR szStartNum[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ClientID_StartNumber"), szStartNum);
	cstrStartNum.Format(_T("%s"), szStartNum);

	return cstrStartNum;
}


BOOL CProfileConfig::WriteClientStartIp_ToIni(CString csClientStartIp)
{
	return WriteProfileString_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ClientStartIp"),csClientStartIp);
}
BOOL CProfileConfig::WriteTotalClientCount_ToIni(int iTotalCilentCount)
{
	return WriteProfileInt_ToIni(_T("Global"), _T("PreviousRegistrationInfo_ClientCount"), iTotalCilentCount);
}

int CProfileConfig::ReadTotalClientCount_FromIni()
{
	return ReadProfileInt_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ClientCount"));;
}
CString CProfileConfig::ReadClientStartIp_FromIni()
{
	CString cstrClientStartIp;
	TCHAR szClientStartIp[100] = {0};

	ReadProfileString_FromIni(_T("Global"), _T("PreviousRegistrationInfo_ClientStartIp"), szClientStartIp);
	cstrClientStartIp.Format(_T("%s"), szClientStartIp);

	return cstrClientStartIp;
}