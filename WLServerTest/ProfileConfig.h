#pragma once

class CProfileConfig
{
private:
	CProfileConfig(void);
	~CProfileConfig(void);

	static CProfileConfig* m_pProfileConfig;

	HANDLE m_hMutex;
	TCHAR m_szConfPath[MAX_PATH];

	BOOL Lock(DWORD dwWaitTime = INFINITE);
	BOOL Unlock();

	BOOL WriteProfileString_ToIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpString);
	BOOL ReadProfileString_FromIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPTSTR lpReturnedString);

	BOOL WriteProfileInt_ToIni(LPCTSTR lpAppName, LPCTSTR lpKeyName, int nValue);
	 int ReadProfileInt_FromIni(LPCTSTR lpAppName, LPCTSTR lpKeyName);

public:

	static CProfileConfig* GetProfileConfigInstance();

	BOOL WriteServerIP_ToIni(CString cstrIP);
	CString ReadServerIP_FromIni();
	
	CString ReadServerPort_FromIni();
	BOOL WriteServerPort_ToIni( CString cstrServerPort );

	BOOL WriteServerHBPort_ToIni(CString cstrServerPortHB);
	CString ReadServerHBPort_FromIni();

	BOOL WriteClientIdPrefix_ToIni(CString cstrIdPre);
	CString ReadEachClientIdPrefix_FromIni();

	BOOL WriteClientStartNum_ToIni( CString cstrStartNum);
	CString ReadClientStartNum_FromIni();

	CString ReadClientStartIp_FromIni();
	BOOL WriteClientStartIp_ToIni(CString cstrClientStartIp);

	int ReadTotalClientCount_FromIni();
	BOOL WriteTotalClientCount_ToIni(int iTotalCilentCount);
};
