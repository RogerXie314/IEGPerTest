#include "StdAfx.h"
#include "SendInfoToServer.h"


#include "wlServertest.h"


#include "../WLCData/WLSecModLogProcess.h"
#include "../include/WLUtilities/WLJsonParse.h"
#include "../include/WLUtilities/base64.h"
#include "../include/WLUtilities/StrUtil.h"
#include "../include/CmdWord/WLCmdWordDef.h"
//#include "../include/WLNetComm/HttpClient.h"
#include "../include/format/installcommon.h"
#include "../include/format/commurl.h"
#include "../include/format/license.h"
#include "../include/WLProtocal/Protocal.h"
#include "../common/WLNetCommApi.h"
#include "../WLNetComm/HttpClient.h"
#include "SimulateJson.h"
#include "../include/WLUtilities/WinUtils.h"

extern BOOL g_bSamePath;

// Helper: �� ComputerIP �ֶ�ע����־ JSON������ÿ�� "CMDTYPE": ֮ǰ��
static std::string InjectComputerIP(const std::string& sJson, const CString& csClientIP)
{
    (void)csClientIP;
    return sJson;
}

CSendInfoToServer::CSendInfoToServer(void)
{
}
CSendInfoToServer::CSendInfoToServer(CString strServerIP, CString strServerPort,CString strIdPre) : m_strServerIP(strServerIP),m_strServerPort(strServerPort),m_strIdPre(strIdPre)
{
	m_ComputerID=_T("UnKnown");
	m_Domain=_T("UnKnown");
	m_ClientLanguage=_T("UnKnown");
	m_WindowsOSVersion=_T("UnKnown");
}
CSendInfoToServer::~CSendInfoToServer(void)
{
} 

void CSendInfoToServer::InitParm(CString sComputerID,CString sDomain,CString sClientLanguage,CString sWindowsVersion)
{
	m_ComputerID=sComputerID;
	m_Domain=sDomain;
	m_ClientLanguage=sClientLanguage;
	m_WindowsOSVersion=sWindowsVersion;
}

wchar_t*  CSendInfoToServer::char2wchar(const char* cchar) 
{     
	wchar_t* m_wchar;

	int len = MultiByteToWideChar( CP_ACP ,0,cchar ,strlen( cchar), NULL,0);     

	m_wchar = new wchar_t[len+1];


	MultiByteToWideChar( CP_ACP ,0,cchar,strlen( cchar),m_wchar,len);     


	m_wchar[len]= '\0' ;

	return m_wchar; 
}
char* CSendInfoToServer::wchar2char(const wchar_t* wchar )
{
	char* m_char;    
	int len= WideCharToMultiByte( CP_ACP ,0,wchar ,wcslen( wchar ), NULL,0, NULL ,NULL );    
	m_char= new char[len+1];     
	WideCharToMultiByte( CP_ACP ,0,wchar ,wcslen( wchar ),m_char,len, NULL ,NULL );     
	m_char[len]= '\0';     

	return m_char; 
}

BOOL CSendInfoToServer::CreateConnection(SOCKET& sockClient,CString strServerIP,const CString strServerPort)
{
	if(sockClient!=INVALID_SOCKET)
	{
		CloseConnection(sockClient);
	}
	sockClient = socket(AF_INET, SOCK_STREAM, 0);
	if(sockClient == INVALID_SOCKET)
	{
		WriteError(_T("socket() called failed!"));
		return -1;
	}
	unsigned long ul = 1;


	SOCKADDR_IN addrServer;

	addrServer.sin_addr.S_un.S_addr = inet_addr(wchar2char(strServerIP.GetBuffer()));
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(_ttoi(strServerPort));
	int nRes=-1;

	ioctlsocket(sockClient, FIONBIO, &ul);

	nRes = connect(sockClient, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (nRes == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)//WSAETIMEDOUT
	{

		timeval tm;
		fd_set WriteSet;
		fd_set ExceptSet;
		tm.tv_sec = 2;   //�ȴ���ʱ
		tm.tv_usec = 0;
		int error = -1;
		int len = sizeof(error);

		FD_ZERO(&WriteSet);
		FD_SET(sockClient, &WriteSet);

		FD_ZERO(&ExceptSet);
		FD_SET(sockClient, &ExceptSet);

		nRes = select(0, NULL, &WriteSet,  &ExceptSet, &tm);
		if(  nRes == SOCKET_ERROR)
		{
			//fail
			WriteError(_T("select fail, ip=%S, port=%d, errno=%d"),
				strServerIP.GetBuffer(), _ttoi(strServerPort), WSAGetLastError());
			return nRes;
		}

		if (nRes == 0)
		{
			//time out
			WriteError(_T("connect timeout, ip=%S, port=%d"),
				strServerIP.GetBuffer(), _ttoi(strServerPort));
			return nRes;
		}

		if (FD_ISSET(sockClient, &ExceptSet))
		{
			WriteError(_T("socket  in ExceptSet"));

			nRes = getsockopt(sockClient, SOL_SOCKET, SO_ERROR, (char *)&error, &len);

			if (nRes == SOCKET_ERROR)
			{
				WriteError(_T("getsockopt fail, err=%d"), WSAGetLastError());

			}
			else
			{
				WriteError(_T("  sock err=%d"), error);
			}
			return nRes;

		}

		if (FD_ISSET(sockClient, &WriteSet))
		{
			//WriteInfo(_T("sock in WriteSet"));
		}
		else
		{
			WriteError(_T("sock not in WriteSet"));
			return nRes;
		}

	}

	int Res = ioctlsocket(sockClient, FIONBIO, (unsigned long*)&ul);

	if(Res == SOCKET_ERROR)
	{
		WriteError(_T("ioctlsocket  FIONBIO 2 fail, ip=%S, port=%d, errno=%d"),strServerIP.GetBuffer(0), _ttoi(strServerPort), WSAGetLastError());
		return Res;
	}

	nRes=true;

	return nRes;
}

void CSendInfoToServer::sendScanStatus(LPTSTR lpGuid, DWORD dwScanStatus)
{
	WCHAR 		TimeBuf[20] = {0};
	WCHAR 		url[100]    = {0};
	wstring 	wsTime;
	SYSTEMTIME 	time;

	GetLocalTime(&time);
	_snwprintf_s(TimeBuf, sizeof(TimeBuf),_TRUNCATE, _T("%04d-%02d-%02d %02d:%02d:%02d"),time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond	);
	wsTime = TimeBuf;

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_SCANSTATUS, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	char *retData = NULL;

	std::string sJson = json.ScanStatus_GetJson(lpGuid, 200, DATA_TO_SERVER_SCANSTATUS, dwScanStatus, 0, 0, wsTime.c_str());
	WriteInfo(_T("Send WL_SOLIDIFY_STATUS:%d, sJson:%s"), dwScanStatus, json.UTF8ToUnicode(sJson).c_str());
	if (CWLNetCommApi::instance()->pdoPost(url, sJson.c_str(), &retData))
	{
		if (NULL == retData)
		{
			WriteError(_T("send succ and retdata is null"));
		}
		else
		{
			Json::Reader	reader;
			Json::Value 	root;

			WriteInfo(_T("send succ and retdata is not null, retDat=%S"), retData);

			CWLNetCommApi::instance()->pdoDelete((void**)&retData);
			retData = NULL;
		}
	}
}

//1.HB:��������SendData
BOOL CSendInfoToServer::SendData(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID)//1.hb  
{
	BOOL bRes = FALSE;
	CProtocal protocal;
	char *pProtocalData = NULL;
	unsigned int nProtocalLen = 0;
	tstring strErr;
	int nSendCount = 0;


	//Э���װ
	if (!protocal.GetPortocal(pSendBuff, nSendLen, cmdID, pProtocalData, nProtocalLen, &strErr))
	{
		WriteError(_T("GetPortocal fail, errinfo=%s"), strErr.c_str());
		bRes = TRUE;
		goto END;
	}

	char* sSendBuf = pProtocalData;


	if (!sSendBuf || nProtocalLen <= 0)
	{
		WriteError(_T("invalid param, pSendBuff=%x, nSendLen=%d"), sSendBuf, nSendLen);
		goto END;
	}

	//ѭ������
	while(nProtocalLen - nSendCount > 0)
	{
		int nMax = 1024;
		int nSendMax = (nProtocalLen - nSendCount) > nMax ? nMax : (nProtocalLen - nSendCount);

		int nRet = send(sockSend, sSendBuf + nSendCount, nSendMax, 0);
		if (SOCKET_ERROR == nRet)
		{
			WriteError(_T("send fail, errno=%d"), WSAGetLastError());
			goto END;
		}

		nSendCount += nRet;
	}
	//WriteInfo(_T("nProtocalLen=%d  nSendCount=%d"),nProtocalLen, nSendCount);
	/*FILE *fp=fopen("c:\\new1.bin","rb");
	if (fp)
	{
	int iwrite = fwrite(sbuf,nSendCount,1024,fp);
	fclose(fp);
	}*/
	bRes = TRUE;
END:

	if (sSendBuf != NULL)
	{
		delete [] sSendBuf;
	}
	return bRes;
}

//2.ThreatLog 5��:����SendData
BOOL CSendInfoToServer::SendData_OnlyCompress(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID)
{
	BOOL bRes = FALSE;
	CProtocal protocal;
	char *pProtocalData = NULL;
	unsigned int nProtocalLen = 0;
	tstring strErr;
	int nSendCount = 0;


	//Э���װ
	if (!protocal.GetPortocal(pSendBuff, nSendLen, cmdID, em_portocal_compress_zlib, em_portocal_encrypt_none, pProtocalData, nProtocalLen, &strErr))
	{
		WriteError(_T("GetPortocal fail, errinfo=%s"), strErr.c_str());
		bRes = TRUE;
		goto END;
	}

	char* sSendBuf = pProtocalData;


	if (!sSendBuf || nProtocalLen <= 0)
	{
		WriteError(_T("invalid param, pSendBuff=%x, nSendLen=%d"), sSendBuf, nSendLen);
		goto END;
	}

	//ѭ������
	while(nProtocalLen - nSendCount > 0)
	{
		int nMax = 1024;
		int nSendMax = (nProtocalLen - nSendCount) > nMax ? nMax : (nProtocalLen - nSendCount);

		int nRet = send(sockSend, sSendBuf + nSendCount, nSendMax, 0);
		if (SOCKET_ERROR == nRet)
		{
			WriteError(_T("send fail, errno=%d"), WSAGetLastError());
			goto END;
		}

		nSendCount += nRet;
	}
	//WriteInfo(_T("nProtocalLen=%d  nSendCount=%d"),nProtocalLen, nSendCount);
	/*FILE *fp=fopen("c:\\new1.bin","rb");
	if (fp)
	{
	int iwrite = fwrite(sbuf,nSendCount,1024,fp);
	fclose(fp);
	}*/
	bRes = TRUE;
END:

	if (sSendBuf != NULL)
	{
		delete [] sSendBuf;
	}
	return bRes;
}



char* CSendInfoToServer::RecvSockData(SOCKET sockRecv, unsigned int &nSrcLen, unsigned int &dwCmdID)//CNC
{
	BOOL bRes = TRUE;
	CProtocal protocal;
	int nHeaderLen = sizeof(WL_PORTOCAL_HEAD);
	int nBodyLen = 0;
	char *saBufHeader=new char[nHeaderLen];
	char *saBufBody=NULL;
	char *saProtocalBuf=NULL;
	char *pSrcData = NULL;
	//unsigned int nSrcLen = 0;
	tstring StrErr;
	BOOL bExit = FALSE;

	if (sockRecv == INVALID_SOCKET)
	{
		WriteError(_T(" invalid sock"));
		goto END;
	}

	//���հ�ͷ
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv  header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//У���ͷ
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//������ͷ
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//��ȡ����
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, dwCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//���հ���
	if (nBodyLen <= 0)
	{
		WriteError(_T("invalid nBodyLen=%d"), nBodyLen);
		goto END;
	}

	saBufBody=new char[nBodyLen];
	if (!RecvData(sockRecv, saBufBody, nBodyLen))
	{
		WriteError(_T("Recv  body fail, nBodyLen=%d"), nBodyLen);
		goto END;
	}

	saProtocalBuf=new char[nHeaderLen + nBodyLen];
	memcpy(saProtocalBuf, saBufHeader, nHeaderLen);
	memcpy(saProtocalBuf + nHeaderLen, saBufBody, nBodyLen);

	//��������
	if (!protocal.ParsePortocal(saProtocalBuf, nHeaderLen + nBodyLen, pSrcData, nSrcLen,  &StrErr))
	{
		WriteError(_T("ParsePortocal fail, errinfo=%s"), StrErr.c_str());
		goto END;
	}


	//saProtocalBuf.reset(pSrcData);

	bRes = FALSE;

END:
	if (saBufHeader!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saBufBody!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saProtocalBuf!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	return pSrcData;
}

BOOL CSendInfoToServer::RecvData(SOCKET sockRecv, char *pRecvBuff, int nRecvLen)//WL_PORTOCAL_HEAD 
{
	BOOL bRes = FALSE;
	int nRecvCount = 0;
	int nRet = 0;
	int nRecvMax = 0;
	char RecvBufTemp[1024] = {0};

	if (!pRecvBuff || nRecvLen <= 0)
	{
		WriteError(_T("invalid param, pRecvBuff=%x, nRecvLen=%d"), pRecvBuff, nRecvLen);
		goto END;
	}

	int iRecvTryCount=0;

	while(nRecvLen - nRecvCount > 0)
	{
		nRecvMax = ((nRecvLen - nRecvCount) > sizeof(RecvBufTemp)) ?  sizeof(RecvBufTemp) : (nRecvLen - nRecvCount);

		nRet = recv(sockRecv, RecvBufTemp, nRecvMax, 0);
		if (nRet == SOCKET_ERROR)
		{
			int iErr = WSAGetLastError();

			iRecvTryCount++;
			Sleep(20);
		}
		else if (nRet == 0)
		{
			int iErr = WSAGetLastError();

			iRecvTryCount++;
			Sleep(20);
		}
		else
		{
			memcpy(pRecvBuff + nRecvCount, RecvBufTemp, nRet);
			nRecvCount += nRet;
			iRecvTryCount=0;
		}

		if(iRecvTryCount>100)//���Խ���100��
		{
			goto END;
		}
	}


	bRes = TRUE;
END:
	return bRes;
}

UINT CSendInfoToServer::RecvData(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen)
{
	wstring strFormat = _T("");
	UINT uiRet = NO_ERROR;

	DWORD dwRecvLen = 0;
	DWORD dwResult = 0;
	DWORD dwRecvMaxLen = 0;
	DWORD dwRecvTimes = 0;
	char RecvBufTemp[1024] = {0};

	if (!pData || (nDataLen <= 0))
	{
		WriteError(_T("invalid param!"));
		uiRet = WSAGetLastError();
		goto END;
	}

	while (nDataLen - dwRecvLen > 0)
	{
		dwRecvMaxLen = ((nDataLen - dwRecvLen) > sizeof(RecvBufTemp)) ?  sizeof(RecvBufTemp) : (nDataLen - dwRecvLen);

		dwResult = recv(sockRecv, RecvBufTemp, dwRecvMaxLen, 0);
		if ((dwResult == SOCKET_ERROR) || (dwResult == 0)) 
		{
			dwRecvTimes++;
			Sleep(20);
		}
		else
		{
			memcpy(pData + dwRecvLen, RecvBufTemp, dwResult);
			dwRecvLen += dwResult;
			dwRecvTimes=0;
		}
		if (dwRecvTimes > 10)
		{
			WriteError(_T("Recv TimeOut!"));
			uiRet = -1;
			goto END;
		}
	}

END:

	return uiRet;
}

BOOL CSendInfoToServer::CloseConnection(SOCKET sockClose)
{

	if (sockClose != INVALID_SOCKET)
	{
		/*if (SOCKET_ERROR == shutdown(sockClose, SD_BOTH))
		{
		WriteError(_T("shutdown fail, errno=%d"), WSAGetLastError());
		}*/

		closesocket(sockClose);
		sockClose = INVALID_SOCKET;
	}
	return TRUE;
}

BOOL CSendInfoToServer::RegisterClientToServer(CString szComputerID, CString szClientID,CString szComputerIP, CString szVersion, CString szOS)
{
	// v5.2: WLNetComm.dll ����ʧ�ܷ���
	CWLNetCommApi* pNetApi = CWLNetCommApi::instance();
	if (!pNetApi || !pNetApi->pEnableTLSv1 || !pNetApi->pdoPost)
	{
		WriteError(_T("WLNetComm.dll δ���أ�ȱʧ��λ����ƥ�䣩����Ѷ�Ӧ WLNetComm.dll ���� exe ͬĿ¼"));
		return FALSE;
	}
	BOOL bResult = FALSE;

	CString str_URL;
	str_URL.Format(URL_CLIENT_INSTALL, m_strServerIP, _ttoi(m_strServerPort));
	//���氲װURL
	tstring m_wsURL_CLIENT_INSTALL = str_URL;

	str_URL.Format(URL_RESULT, m_strServerIP, _ttoi(m_strServerPort));
	tstring m_wsURL_CLIENT_INSTALL_END = str_URL;

	CWLJsonParse m_json;

	std::wstring sUserName=_T("User_Test_");
	CString sCount=szClientID.Right(4);
	sUserName+=sCount.GetBuffer();

	std::wstring sCupID=szClientID.GetBuffer();
	std::wstring wsComputerID = szComputerID.GetBuffer();

	std::wstring sCupName= m_strIdPre.GetBuffer();  
	sCupName+=sCount.GetBuffer();

	// FALSE - ����64λϵͳ�� FALSE - ��������Ȩ�ڵ㣻 TRUE - ��IEGע�ᣨ����SRS��
	std::string sData = m_json.SetUp_GetJson(wsComputerID, sUserName, 0x01, CMD_CLIENT_REGISTRY, sCupName, szComputerIP.GetBuffer(), _T("00-00-00-00-00-00"), (LPCTSTR)szOS, FALSE, FALSE, TRUE, TRUE, (LPCTSTR)szVersion);

	CString stMsg;
	stMsg.Format(_T("Register . IP=%s, data= %S"), m_strServerIP.GetBuffer(), (sData.c_str()));
	WriteInfo(stMsg.GetBuffer());

	char *pResult = NULL;

	//AfxMessageBox(m_wsUrlClientSetup.c_str());
	CWLNetCommApi::instance()->pEnableTLSv1();//added by lzq:����ȷ��λ��ͬĿ¼WLNetComm.dll

	if ( !CWLNetCommApi::instance()->pdoPost(m_wsURL_CLIENT_INSTALL.c_str(), (LPSTR)sData.c_str(), &pResult))
	{
		CString strMsg;
		strMsg.Format(_T("doPost ERROR. URL=%s, sData= %S"), m_wsURL_CLIENT_INSTALL.c_str(), sData.c_str());
		//AfxMessageBox(strMsg);
		return FALSE;
	}	

	std::string sJson = "";
	if(  !pResult)
	{  
		CString strMsg;
		strMsg.Format(_T("doPost OK, pResult = null;"));
		//AfxMessageBox(strMsg);

		return FALSE;
	}
	else
	{
		sJson = pResult;
	}

	if( sJson.length() == 0)
	{
		CString strMsg;
		strMsg.Format(_T("doPost OK, sJson.length() = 0"));
		//AfxMessageBox(strMsg);

		return FALSE;
	}

	//����JSON
	int nErrorCode = 0;
	std::wstring wsMsg;
	if( !m_json.Setup_CheckResultByJson(sJson, nErrorCode, wsMsg))
	{
		CString strMsg;
		CWLJsonParse WLJsonParse;

		// ��������Ϣд����־
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		stMsg.Format(_T("doPost Error, Json = %s"),wJson.c_str() );
		WriteInfo(stMsg.GetBuffer());

		// ������ԭ�������MessageBox
		int MsgPos = wJson.find(_T("MESSAGE"));
		int RstPos = wJson.find(_T("RESULT"));
		wstring WMsg = wJson.substr(MsgPos+10, RstPos-MsgPos-12);
		strMsg.Format(_T("doPost OK, Setup_CheckResultByJson =FALSE, nErrorCode = %d, reseaon:%s"), nErrorCode,WMsg.c_str());
		//AfxMessageBox(strMsg);
		return FALSE;
	}
	stMsg.Format(_T("doPost OK, Json = %S"), (sJson.c_str()));
	WriteInfo(stMsg.GetBuffer());

	CWLNetCommApi::instance()->pdoDelete((void**)&pResult);

	std::string sData1 = m_json.Setup_GetJsonInstallEnd(szClientID.GetBuffer(),szClientID.GetBuffer(), 1, 1, 0);
	//MessageBoxA(NULL, sData.c_str(), "sData=", MB_OK);

	//AfxMessageBox(m_wsUrlClientSetup.c_str());
	if ( ! CWLNetCommApi::instance()->pdoPost(m_wsURL_CLIENT_INSTALL_END.c_str(), (LPSTR)sData1.c_str(), &pResult))
	{
		CString strMsg;
		strMsg.Format(_T("Setup_InstallEnd URL=%s, sData= %S"), m_wsURL_CLIENT_INSTALL_END.c_str(), sData1.c_str());
		//AfxMessageBox(strMsg);
		return FALSE;
	}	


	bResult =  TRUE;

	return bResult;

}

//
/*
// ͨ�������ӷ�����в�����־������ȫ���������˿�192.168.7.254 8441  JinGe
BOOL CSendInfoToServer::SendDetectLogTCP(client& pCurClient, SOCKET sockSend, std::wstring& strJson)
{
if(!SendData(sockSend, strJson.c_str(), strJson.length()-1, 1))
{
strMsg.Format(_T("SendData ERROR. IP=%s, data= %S"), m_strServerIP.GetBuffer(),  (strJson.c_str()));
WriteError(strMsg.GetBuffer());

goto END;
}
}

BOOL CSendInfoToServer::RecvDetectLogTCP(client& pCurClient, SOCKET sockSend )
{
//USM:Ҳ����������
//RecvData()
}
*/
//added by lzq:MAY19 Xia
/*
#define THREAT_EVENT_TYPE_SYSTEM			(10)
#define THREAT_EVENT_TYPE_NETWORK			(20)
#define THREAT_EVENT_TYPE_FILE				(30)
#define THREAT_EVENT_TYPE_REG				(40)
#define THREAT_EVENT_TYPE_PROC				(50)
#define THREAT_EVENT_TYPE_PROCSTART			(60)

#define THREATLOG_TYPE_FILE_OPEN		(1)
#define THREATLOG_TYPE_FILE_CREATE		(2)
#define THREATLOG_TYPE_FILE_READ		(5)
#define THREATLOG_TYPE_FILE_WRITE		(6)
#define THREATLOG_TYPE_FILE_ENDOF		(7)
#define THREATLOG_TYPE_FILE_DELETE		(8)
#define THREATLOG_TYPE_FILE_RENAME		(9)
#define THREATLOG_TYPE_FILE_SETSECURITY	(10)
#define THREATLOG_TYPE_FILE_CLOSE		(11)

*/ // TCP - ������в��־ 
BOOL CSendInfoToServer::SendThreatLog_ToserverTCP(client& pCurClient,SOCKET sockSend, BOOL bHit)//ÿ���߳�ִ��һ�����������  ÿ��json��ʵ��Ҫ����ճ����Sleep(100);
{
	CString strMsg = _T("");
	BOOL bResult = FALSE;

	std::wstring wtrsComputerID = pCurClient.Client_GetComputerID();
	std::wstring wstrClientIP = pCurClient.GetClientIP();
	std::wstring wstrClientID = pCurClient.GetClientID();
	CString csClientIP_TCP(wstrClientIP.c_str());

	//����json  ������5��json

	WLSimulateJson Obj;
	std::string TmpJson;

	char* pSendBuf=NULL;
	UINT BufLen=0;
	
	

	//File 30
	TmpJson = Obj.ThreatLog_SimulateJson_File(wtrsComputerID, bHit);

	//BufLen = Obj.ThreatLog_SimulateJson_File_ReturnBuf(wtrsComputerID,&pSendBuf);

	if(!SendData_OnlyCompress(sockSend, TmpJson.c_str(), TmpJson.length()-1, THREAT_EVENT_UPLOAD_CMDID))
	{
		strMsg.Format(_T("SendData ERROR. IP=%s, data= %S"), m_strServerIP.GetBuffer(), (TmpJson.c_str()));
		WriteError(strMsg.GetBuffer());

		goto END; 
	}
	Sleep(50);



	//ProcStart 60
	TmpJson = Obj.ThreatLog_SimulateJson_ProcStart(wtrsComputerID,wstrClientID,wstrClientIP, bHit);

	if(!SendData_OnlyCompress(sockSend, TmpJson.c_str(), TmpJson.length()-1, THREAT_EVENT_UPLOAD_CMDID))
	{
		strMsg.Format(_T("SendData ERROR. IP=%s, data= %S"), m_strServerIP.GetBuffer(), (TmpJson.c_str()));
		WriteError(strMsg.GetBuffer());

		goto END;
	}
	Sleep(50);


	//Reg 40
	TmpJson = Obj.ThreatLog_SimulateJson_Reg(wtrsComputerID,wstrClientID,wstrClientIP, bHit);

	if(!SendData_OnlyCompress(sockSend, TmpJson.c_str(), TmpJson.length()-1, THREAT_EVENT_UPLOAD_CMDID))
	{
		strMsg.Format(_T("SendData ERROR. IP=%s, data= %S"), m_strServerIP.GetBuffer(), (TmpJson.c_str()));
		WriteError(strMsg.GetBuffer());

		goto END;
	}  



	bResult = TRUE;

END:

	return bResult;
}
BOOL CSendInfoToServer::SendThreatLog_Data(SOCKET sockSend, const char *pSendBuff, unsigned int nSendLen,int cmdID)
{
	BOOL bRes = FALSE;
	CProtocal protocal;
	char *pProtocalData = NULL;
	unsigned int nProtocalLen = 0;
	tstring strErr;
	int nSendCount = 0;


	//Э���װ
	if (!protocal.GetPortocal(pSendBuff, nSendLen, cmdID, pProtocalData, nProtocalLen, &strErr))
	{
		WriteError(_T("GetPortocal fail, errinfo=%s"), strErr.c_str());
		bRes = TRUE;
		goto END;
	}

	char *sbuf=pProtocalData;


	if (!sbuf || nProtocalLen <= 0)
	{
		WriteError(_T("invalid param, pSendBuff=%x, nSendLen=%d"), sbuf, nSendLen);
		goto END;
	}

	//ѭ������
	while(nProtocalLen - nSendCount > 0)
	{
		int nMax = 1024;
		int nSendMax = (nProtocalLen - nSendCount) > nMax ? nMax : (nProtocalLen - nSendCount);

		int nRet = send(sockSend, sbuf + nSendCount, nSendMax, 0);
		if (SOCKET_ERROR == nRet)
		{
			WriteError(_T("send fail, errno=%d"), WSAGetLastError());
			goto END;
		}

		nSendCount += nRet;
	}
	//WriteInfo(_T("nProtocalLen=%d  nSendCount=%d"),nProtocalLen, nSendCount);
	/*FILE *fp=fopen("c:\\new1.bin","rb");
	if (fp)
	{
	int iwrite = fwrite(sbuf,nSendCount,1024,fp);
	fclose(fp);
	}*/
	bRes = TRUE;
END:
	if(pProtocalData !=NULL)
	{
		delete [] pProtocalData;
	}


	return bRes;

}

// TCP - ȷ���Ƿ������ݿ����������գ�����ʱȷ�Ϸ��ص��������͡��ǲ���ʱ��ʹ��HTTPSִ�����ݵ��������պʹ���
DWORD CSendInfoToServer::RecvThreatLog_(SOCKET sockRecv)
{
	BOOL bRes = TRUE;
	CProtocal protocal;
	int nHeaderLen = sizeof(WL_PORTOCAL_HEAD);
	int nBodyLen = 0;
	char *saBufHeader=new char[nHeaderLen];
	char *saBufBody=NULL;
	char *saProtocalBuf=NULL;

	DWORD dwRet = 0;
	UINT uiCmdID = 0;

	tstring StrErr;
	BOOL bExit = FALSE;

	if (sockRecv == INVALID_SOCKET)
	{
		WriteError(_T(" invalid sock"));
		goto END;
	}

	//���հ�ͷ
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv  header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//У���ͷ
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//������ͷ
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//��ȡ����
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, uiCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//�����û�а��壬�еĻ�ȡ������������
	if (nBodyLen > 0)
	{
		saBufBody = new char[nBodyLen];
		RecvData(sockRecv, saBufBody, nBodyLen);
	}

	dwRet = uiCmdID;

END:
	if (saBufHeader!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saBufBody!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saProtocalBuf!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	return dwRet;


}
UINT CSendInfoToServer::RecvData_ThreatLog_(_Out_ char *pData, _In_ SOCKET sockRecv, _In_ UINT nDataLen)
{
	wstring strFormat = _T("");
	UINT uiRet = NO_ERROR;

	DWORD dwRecvLen = 0;
	DWORD dwResult = 0;
	DWORD dwRecvMaxLen = 0;
	DWORD dwRecvTimes = 0;
	char RecvBufTemp[1024] = {0};

	if (!pData || (nDataLen <= 0))
	{
		WriteError(_T("invalid param!"));
		uiRet = WSAGetLastError();
		goto END;
	}

	while (nDataLen - dwRecvLen > 0)
	{
		dwRecvMaxLen = ((nDataLen - dwRecvLen) > sizeof(RecvBufTemp)) ?  sizeof(RecvBufTemp) : (nDataLen - dwRecvLen);

		dwResult = recv(sockRecv, RecvBufTemp, dwRecvMaxLen, 0);
		if ((dwResult == SOCKET_ERROR) || (dwResult == 0)) 
		{
			dwRecvTimes++;
			Sleep(20);
		}
		else
		{
			memcpy(pData + dwRecvLen, RecvBufTemp, dwResult);
			dwRecvLen += dwResult;
			dwRecvTimes=0;
		}
		if (dwRecvTimes > 10)
		{
			WriteError(_T("Recv TimeOut!"));
			uiRet = -1;
			goto END;
		}
	}

END:

	return uiRet;

}

// HTTPS - ��������hm
BOOL CSendInfoToServer::SendThreatLog_(client& curClient)
{

	CWLJsonParse cJson;
	char *retData = NULL;

	WCHAR url[100] = {0};
	std::string strJson = "";

	CString strErrLog = _T("");
	BOOL bResult = FALSE;

	CString strIP = m_strServerIP;
	DWORD dwPort = _tcstoul(_T("8441"), NULL, 10); //10����CString to DWORD

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_HEARTBEAT, strIP, dwPort);

	int iCpuUseValue = (int)rand() % 100;
	int iMemoryUseValue = (int)rand() % 100;

	std::wstring wtrsComputerID = curClient.Client_GetComputerID();
	std::wstring wstrClientIP = curClient.GetClientIP();
	std::wstring wstrClientID = curClient.GetClientID();
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), iCpuUseValue, iMemoryUseValue, (LPCTSTR)m_WindowsOSVersion,wstrClientID, wstrClientIP);

	if (CWLNetCommApi::instance()->pdoHeartBeat(url, strJson.c_str(), &retData))
	{
		bResult = TRUE;

		if (NULL == retData)
		{
			WriteDebug(_T("recv heart beat succ, retData=NULL, %S"), strJson);
		}
		else
		{
			WriteDebug(_T("recv heart beat succ, new data = %S"), retData);

			//������������USM���ص�Json����������
			m_ComputerID = curClient.Client_GetComputerID();
			DWORD dwParse = ParseRevData(retData); 

			CWLNetCommApi::instance()->pdoDelete((void**)&retData);
		}
	}
	else
	{
		WriteError(_T("recv heart beat failed, url = %s, json = %s"), url, cJson.UTF8ToUnicode(strJson).c_str());
		bResult = FALSE;
	}

	return bResult;

}

// HTTPS - ���ؽ���HTTPS����������
DWORD CSendInfoToServer::ParseRevData_ThreatLog(std::string strJson)
{
	Json::Value root;
	Json::Value CMDContent;
	Json::FastWriter writer;
	Json::Reader	reader;
	Json::Value cmd;

	try
	{
		WriteInfo(_T("---start-- json = %S\n"), strJson.c_str());
		if (!reader.parse(strJson, root))
		{
			WriteError(_T("parse error:%S"), strJson.c_str());
			return 3;
		}

		UINT CMDTYPE = 0, CMDID = 0;
		UINT uiTcpPort = 0;

		for (unsigned int i=0; i<root.size(); i++)
		{
			if (!root[i].isMember("ComputerID")
				|| !root[i].isMember("CMDTYPE")
				|| !root[i].isMember("CMDID"))
			{
				WriteError(_T("error cmd:%s"), writer.write(root[i]).c_str());
				continue;
			}

			CMDTYPE = root[i]["CMDTYPE"].asUInt();
			CMDID = root[i]["CMDID"].asUInt();

			if (CMD_CLIENT_NOREGINFO == CMDID)
			{
				WriteFatal(_T("no reg info"));
				return 4;
			}

			// ���ؽ��
			char *pResultJson = NULL;
			int iResult = ERROR_SUCCESS;

			//todo ��һ���ֲ�����Ҫ����Json��pResultJson�������ؾ�����Ϣ��USM

			if(NULL != pResultJson && strlen(pResultJson) > 0)
			{
				if(!SendExecResult(CMDID, iResult, pResultJson))
				{
					WriteError(_T("CWLPolicyThread::MainThread: sendExecResult Failed, nPly=%d, iResult = %d, strReslutJson=%S"), CMDID, iResult, pResultJson);
					continue;
				}
			}
			else
			{
				if(!SendExecResult(CMDID, iResult))
				{
					WriteError(_T("CWLPolicyThread::MainThread: sendExecResult Failed, nPly=%d, iResult=%d"), CMDID, iResult);
					continue;
				}
			}

		}
	}
	catch (...)
	{
		WriteError(_T("catch exception"));
		return 4;
	}

	return ERROR_SUCCESS;


}


//added by lzq:MAY19 Shang

BOOL CSendInfoToServer::SendHeartbeatToserverTCP(client& pCurClient, SOCKET sockSend)//CC
{
	CWLJsonParse cJson;

	CString strMsg = _T("");
	BOOL bResult = FALSE;

	std::string strJson = "";

	std::wstring wtrsComputerID = pCurClient.Client_GetComputerID();
	std::wstring wstrClientIP = pCurClient.GetClientIP();
	std::wstring wstrClientID = pCurClient.GetClientID();
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), rand()%100, rand()%100, (LPCTSTR)m_WindowsOSVersion, wstrClientID, wstrClientIP);

	//strMsg.Format(_T("SendData . IP=%s, data= %S"), m_strServerIP.GetBuffer(),  (sJson.c_str()));
	if(!SendData(sockSend, strJson.c_str(), strJson.length()-1, 1))
	{
		strMsg.Format(_T("SendData ERROR. IP=%s, data= %S"), m_strServerIP.GetBuffer(), (strJson.c_str()));
		WriteError(strMsg.GetBuffer());

		goto END;
	}

	bResult = TRUE;

END:

	return bResult;
}

DWORD CSendInfoToServer::RecvHeartbeat(SOCKET sockRecv)
{
	BOOL bRes = TRUE;
	CProtocal protocal;
	int nHeaderLen = sizeof(WL_PORTOCAL_HEAD);
	int nBodyLen = 0;
	char *saBufHeader=new char[nHeaderLen];
	char *saBufBody=NULL;
	char *saProtocalBuf=NULL;

	DWORD dwRet = 0;
	UINT uiCmdID = 0;

	tstring StrErr;
	BOOL bExit = FALSE;

	if (sockRecv == INVALID_SOCKET)
	{
		WriteError(_T(" invalid sock"));
		goto END;
	}

	//���հ�ͷ
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv (WL_PORTOCAL_HEAD)header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//У���ͷ
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//������ͷ
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//��ȡ����
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, uiCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//�����û�а��壬�еĻ�ȡ������������
	if (nBodyLen > 0)
	{
		saBufBody = new char[nBodyLen];
		RecvData(sockRecv, saBufBody, nBodyLen);
	}

	dwRet = uiCmdID;

END:
	if (saBufHeader!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saBufBody!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	if (saProtocalBuf!=NULL)
	{
		delete saBufHeader;
		saBufHeader = NULL;
	}

	return dwRet;
}

// HTTPS��ʽ���÷�ʽֻ�����ڽ������� :
BOOL CSendInfoToServer::SendHeartbeat(client& curClient)//lzq:https������   ʵ����ȥUSM�������
{
	CWLJsonParse cJson;
	char *retData = NULL;

	WCHAR url[100] = {0};
	std::string strJson = "";

	CString strErrLog = _T("");
	BOOL bResult = FALSE;

	CString strIP = m_strServerIP;
	DWORD dwPort = _tcstoul(_T("8440"), NULL, 10); //10����CString to DWORD   //modified by lzq:June19    8441-->8440 ;;ע��

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_HEARTBEAT, strIP, dwPort);

	int iCpuUseValue = (int)rand() % 100;
	int iMemoryUseValue = (int)rand() % 100;

	std::wstring wtrsComputerID = curClient.Client_GetComputerID();
	std::wstring wstrClientIP = curClient.GetClientIP();
	std::wstring wstrClientID = curClient.GetClientID();
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), iCpuUseValue, iMemoryUseValue, (LPCTSTR)m_WindowsOSVersion,wstrClientID, wstrClientIP);

	if (CWLNetCommApi::instance()->pdoHeartBeat(url, strJson.c_str(), &retData))
	{
		bResult = TRUE;

		if (NULL == retData)
		{
			WriteDebug(_T("recv heart beat succ, retData=NULL, %S"), strJson);
		}
		else
		{
			WriteDebug(_T("recv heart beat succ, new data = %S"), retData);

			//������������USM���ص�Json����������
			m_ComputerID = curClient.Client_GetComputerID();
			DWORD dwParse = ParseRevData(retData);   

			CWLNetCommApi::instance()->pdoDelete((void**)&retData);
		}
	}
	else
	{
		WriteError(_T("recv heart beat failed, url = %s, json = %s"), url, cJson.UTF8ToUnicode(strJson).c_str());
		bResult = FALSE;
	}

	return bResult;
}

DWORD CSendInfoToServer::ParseRevData(std::string strJson)
{
	Json::Value root;
	Json::Value CMDContent;
	Json::FastWriter writer;
	Json::Reader	reader;
	Json::Value cmd;

	try
	{
		WriteInfo(_T("---start-- json = %S\n"), strJson.c_str());
		if (!reader.parse(strJson, root))
		{
			WriteError(_T("parse error:%S"), strJson.c_str());
			return 3;
		}

		UINT CMDTYPE = 0, CMDID = 0;
		UINT uiTcpPort = 0;

		for (unsigned int i=0; i<root.size(); i++)
		{
			if (!root[i].isMember("ComputerID")
				|| !root[i].isMember("CMDTYPE")
				|| !root[i].isMember("CMDID"))
			{
				WriteError(_T("error cmd:%s"), writer.write(root[i]).c_str());
				continue;
			}

			CMDTYPE = root[i]["CMDTYPE"].asUInt();
			CMDID = root[i]["CMDID"].asUInt();

			if (CMD_CLIENT_NOREGINFO == CMDID)
			{
				WriteFatal(_T("no reg info"));
				return 4;
			}

			// ���ؽ��
			char* pResultJson = NULL;
			int  iResult = ERROR_SUCCESS;

			//todo ��һ���ֲ�����Ҫ����Json��pResultJson�������ؾ�����Ϣ��USM

			if(NULL != pResultJson && strlen(pResultJson) > 0)//NeverEnter
			{
				if(!SendExecResult(CMDID, iResult, pResultJson))//NEVER
				{
					WriteError(_T("CWLPolicyThread::MainThread: sendExecResult Failed, nPly=%d, iResult = %d, strReslutJson=%S"), CMDID, iResult, pResultJson);
					continue;
				}
			}
			else
			{
				if(!SendExecResult(CMDID, iResult)) //����Ӧ�ò��Գɹ������ͽ����USM
				{
					WriteError(_T("CWLPolicyThread::MainThread: sendExecResult Failed, nPly=%d, iResult=%d"), CMDID, iResult);
					continue;
				}
			}

		}
	}
	catch (...)
	{
		WriteError(_T("catch exception"));
		return 4;
	}

	return ERROR_SUCCESS;
}

// HTTPS - ���ؽ��������USM
BOOL CSendInfoToServer::SendExecResult(WORD CMDID, int nDealResult, char *pResultJson)
{
	if ( 3 == CMDID )
	{
		return TRUE;
	}

	BOOL bRet = FALSE;
	WCHAR url[100] = {0};
	DWORD dwPort = _tcstoul(m_strServerPort, NULL, 10); //ʮ����

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_RESULT, m_strServerIP, dwPort);

	CWLJsonParse json;
	char *retData = NULL;
	if (CWLNetCommApi::instance()->pdoPost(url, pResultJson, &retData))
	{
		WriteInfo(_T("send result succ = %d-%d"), CMDID, nDealResult);
		bRet = TRUE;

		if (NULL != retData)
		{
			CWLNetCommApi::instance()->pdoDelete((void**)&retData);
		}
	}
	else
	{
		WriteError(_T("send result failed = %d - %d"), CMDID, nDealResult);
	}
	return bRet;
}

// HTTPS - ���ؽ��������USM
BOOL CSendInfoToServer::SendExecResult(WORD CMDID, int nDealResult)
{
	if ( 3 == CMDID )
	{
		return TRUE;
	}

	BOOL bRet = FALSE;
	WCHAR url[100] = {0}; 
	DWORD dwPort = _tcstoul(m_strServerPort, NULL, 10); //ʮ����

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_RESULT, m_strServerIP, dwPort);

	CWLJsonParse json;
	char *retData = NULL;

	std::string strResult = json.Result_GetJsonByDealResult(m_ComputerID.GetString(), GetCMDTYPE(CMDID), CMDID, nDealResult);
	if (CWLNetCommApi::instance()->pdoPost(url, strResult.c_str(), &retData))
	{
		WriteInfo(_T("send result succ = %d - %d, json = %S"), CMDID, nDealResult, strResult.c_str());
		bRet = TRUE;

		if (NULL != retData)
		{
			CWLNetCommApi::instance()->pdoDelete((void**)&retData);
		}
	}
	else
	{
		WriteError(_T("send result failed = %d - %d"), CMDID, nDealResult);
	}
	return bRet;
}

// HTTPS - ����CMDID���ڲ��Ի�������
WORD CSendInfoToServer::GetCMDTYPE(WORD CMDID)
{
	WORD CMDTYPE = -1;

	if (CMDID == PLY_CLIENT_SAFETYAPP_UKEY_SET)
	{
		CMDTYPE = CMDTYPE_SAFETYAPP_POLICY;
	}
	else if ( PLY_CLIENT_UPLOAD_SPEED == CMDID ||  PLY_CLIENT_WHITELIST_SCAN_SPEED == CMDID)
	{ 
		CMDTYPE = CMDTYPE_POLICY;
	}
	else if ((CMDID>0) && (CMDID<50))
	{
		CMDTYPE = 1;
	}
	else if ((CMDID>=50) && (CMDID<100))
	{
		CMDTYPE = 50;
	}
	else if ((CMDID>=100) && (CMDID<150))
	{
		CMDTYPE = 100;
	}
	else if ((CMDID>=150) && (CMDID<200))
	{
		CMDTYPE = 150;
	}
	else if ((CMDID>=200) && (CMDID<250))
	{
		CMDTYPE = 200;
	}

	return CMDTYPE;
}


BOOL CSendInfoToServer::SendClientNwlLogToServer_SingleRule(LPTSTR lpComputerID)
{
	const DWORD iLen = 100;
	WCHAR		URL_PROCESS_LOG[iLen] = {0};

	int			nSize;
	UCHAR*		pTmpData=NULL;

	int  iLogHeadBodyLen  =   (sizeof(IPC_LOG_COMMON)+sizeof(WARNING_LOG_STRUCT));

	BYTE*  pLogBuf;
	pLogBuf = new BYTE[iLogHeadBodyLen];
	memset(pLogBuf, 0, iLogHeadBodyLen);

	IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf;
	ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
	ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
	//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
	ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

	PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
	pLog->bHoldback                 = 0;
	//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
	WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_SYSFILE_CHECK, 0, pLog->nSubType);
	pLog->bCertCheckFailed          = 1;
	pLog->bIntegrityCheckFailed     = 1;  

	pLog->llTime = _time32(NULL);  

	_tcscpy(pLog->szFullPath,_T("c:\\Tmp\\June10.exe"));
	_tcslwr(pLog->szFullPath);
	_tcscpy(pLog->szVersion,_T("7893"));
	_tcscpy(pLog->szCompany,_T("Some Company"));
	_tcscpy(pLog->szProduct,_T("SomeProduct"));
	_tcscpy(pLog->szDefIntegrity,_T("Some defintegrity"));
	/*
	//�������������������
	typedef enum TYPE_OPTYPE_PWL
	{
	OPTYPE_PWL_CONTROL = 1,
	OPTYPE_PWL_AUDIT,
	OPTYPE_PWL_MODIFY_FILE,
	OPTYPE_PWL_SYSFILE_CHECK, 
	OPTYPE_PWL_AUTO_APPROVE,
	OPTYPE_PWL_COUNT,
	OPTYPE_PBL_CONTROL,
	OPTYPE_VIRUS_CONTROL  
	}TYPE_OPTYPE_PWL;
	*/
	pLog->nSubType = OPTYPE_PWL_SYSFILE_CHECK;
	pLog->processId = 0x7893;


	//GetProductInfo(pLog);
	//DWORD dwRet = m_WLWarnLogSender.SendLog(ipclogcomm);

	CWLMetaData*  pMData = new CWLMetaData(iLogHeadBodyLen,pLogBuf);

	std::vector<CWLMetaData*>  vecLog;

	vecLog.push_back(pMData);



	CWLJsonParse json;
	std::string  sJson;

	_snwprintf_s(URL_PROCESS_LOG, sizeof(URL_PROCESS_LOG)/sizeof(URL_PROCESS_LOG[0]), _TRUNCATE, URL_LOG_PROCESS, m_strServerIP, _ttoi(m_strServerPort));
	sJson = json.WarningLog_GetJsonByVector(lpComputerID, 200, DATA_TO_SERVER_PROCESS_ALERT_LOG, vecLog);  //�õ��Ϸ���json�ַ���


	delete pMData;
	delete pLogBuf;  

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	char*		pResult = NULL;
	BOOL		bRet = FALSE;
	bRet = objTmp->pdoPost(URL_PROCESS_LOG, (LPSTR)sJson.c_str(), &pResult);
	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;

		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Operation log Error:%s"), wJson.c_str());
	}

	return bRet;
}

/*
OPTYPE_PWL_CONTROL = 1,
OPTYPE_PWL_AUDIT,
OPTYPE_PWL_MODIFY_FILE,
OPTYPE_PWL_SYSFILE_CHECK,
OPTYPE_PWL_AUTO_APPROVE,
*/
/*
//�������������������
typedef enum TYPE_OPTYPE_PWL
{
OPTYPE_PWL_CONTROL = 1,
OPTYPE_PWL_AUDIT,
OPTYPE_PWL_MODIFY_FILE,
OPTYPE_PWL_SYSFILE_CHECK, 
OPTYPE_PWL_AUTO_APPROVE,
OPTYPE_PWL_COUNT,
OPTYPE_PBL_CONTROL,
OPTYPE_VIRUS_CONTROL  
}TYPE_OPTYPE_PWL;
*/

BOOL CSendInfoToServer::SendClientNwlLogToServer_FiveType(LPTSTR lpComputerID)
{
	WCHAR		URL_PROCESS_LOG[100] = {0};

	_snwprintf_s(URL_PROCESS_LOG, sizeof(URL_PROCESS_LOG)/sizeof(URL_PROCESS_LOG[0]), _TRUNCATE, URL_LOG_PROCESS, m_strServerIP, _ttoi(m_strServerPort));

	int  iLogHeadBodyLen  = (sizeof(IPC_LOG_COMMON)+sizeof(WARNING_LOG_STRUCT));

	std::vector<CWLMetaData*>  vecLog;

	CWLMetaData*  pMData_SysFileCheck = NULL;
	BYTE*         pLogBuf_SysFileCheck = NULL;

	CWLMetaData*  pMData_Control = NULL;
	BYTE*         pLogBuf_Control = NULL;

	CWLMetaData*  pMData_Audit = NULL;
	BYTE*         pLogBuf_Audit = NULL;

	CWLMetaData*  pMData_Modify = NULL;
	BYTE*         pLogBuf_Modify = NULL;

	CWLMetaData*  pMData_Auto = NULL;
	BYTE*         pLogBuf_Auto = NULL;

	// ƴ�Ӳ�ͬ·��ʹ��
	std::wstring wsThreadId = _T("");
	std::wstring wsTime     = _T("");
	if (!g_bSamePath)
	{
		DWORD dwThreadId = GetCurrentThreadId(); // ��ȡ�߳�ID
		wchar_t buffer[20] = {0};
		// ʹ�� _itow_s ����ת��
		_itow_s(dwThreadId, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
		wsThreadId = buffer;

		DWORD dwTime = time(NULL);
		memset(buffer, 0, 20 * 2);
		_itow_s(dwTime, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
		wsTime = buffer;
	}

	//1.OPTYPE_PWL_SYSFILE_CHECK
	{
		pLogBuf_SysFileCheck = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_SysFileCheck, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_SysFileCheck;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
		//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
		ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

		PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
		pLog->bHoldback                 = 0;
		//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
		WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_SYSFILE_CHECK, 0, pLog->nSubType);
		pLog->bCertCheckFailed          = 1;
		pLog->bIntegrityCheckFailed     = 1;  

		pLog->llTime = _time32(NULL);  

		std::wstring wsPath = _T("c:\\Tmp\\OPTYPE_PWL_SYSFILE_CHECK");
		if (!g_bSamePath)
		{
			wsPath += _T("_");
			wsPath += wsThreadId;
			wsPath += _T("_");
			wsPath += wsTime;
		}
		wsPath += _T(".exe");

		_tcscpy(pLog->szFullPath, wsPath.c_str());
		_tcslwr(pLog->szFullPath);
		_tcscpy(pLog->szVersion,_T("7893"));
		_tcscpy(pLog->szCompany,_T("Some Company"));
		_tcscpy(pLog->szProduct,_T("SomeProduct"));
		_tcscpy(pLog->szDefIntegrity,_T("Some defintegrity"));

		pLog->nSubType = OPTYPE_PWL_SYSFILE_CHECK;
		pLog->processId = 0x7893;



		pMData_SysFileCheck = new CWLMetaData(iLogHeadBodyLen,pLogBuf_SysFileCheck);


		vecLog.push_back(pMData_SysFileCheck);
	}
	//2.OPTYPE_PWL_CONTROL
	{
		pLogBuf_Control = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_Control, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_Control;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
		//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
		ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

		PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
		pLog->bHoldback                 = 0;
		//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
		WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_CONTROL, 0, pLog->nSubType);
		pLog->bCertCheckFailed          = 1;
		pLog->bIntegrityCheckFailed     = 1;  

		pLog->llTime = _time32(NULL);  

		std::wstring wsPath = _T("c:\\Tmp\\OPTYPE_PWL_CONTROL");
		if (!g_bSamePath)
		{
			wsPath += _T("_");
			wsPath += wsThreadId;
			wsPath += _T("_");
			wsPath += wsTime;
		}
		wsPath += _T(".exe");

		_tcscpy(pLog->szFullPath, wsPath.c_str());
		_tcslwr(pLog->szFullPath);
		_tcscpy(pLog->szVersion,_T("7893"));
		_tcscpy(pLog->szCompany,_T("Some Company"));
		_tcscpy(pLog->szProduct,_T("SomeProduct"));
		_tcscpy(pLog->szDefIntegrity,_T("Some defintegrity"));

		pLog->nSubType = OPTYPE_PWL_CONTROL;
		pLog->processId = 0x7893;



		pMData_Control = new CWLMetaData(iLogHeadBodyLen,pLogBuf_Control);


		vecLog.push_back(pMData_Control);
	}
	//3.OPTYPE_PWL_AUDIT
	{
		pLogBuf_Audit = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_Audit, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_Audit;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
		//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
		ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

		PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
		pLog->bHoldback                 = 0;
		//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
		WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_AUDIT, 0, pLog->nSubType);
		pLog->bCertCheckFailed          = 1;
		pLog->bIntegrityCheckFailed     = 1;  

		pLog->llTime = _time32(NULL);  

		std::wstring wsPath = _T("c:\\Tmp\\OPTYPE_PWL_AUDIT");
		if (!g_bSamePath)
		{
			wsPath += _T("_");
			wsPath += wsThreadId;
			wsPath += _T("_");
			wsPath += wsTime;
		}
		wsPath += _T(".exe");

		_tcscpy(pLog->szFullPath, wsPath.c_str());
		_tcslwr(pLog->szFullPath);
		_tcscpy(pLog->szVersion,_T("7893"));
		_tcscpy(pLog->szCompany,_T("Some Company"));
		_tcscpy(pLog->szProduct,_T("SomeProduct"));
		_tcscpy(pLog->szDefIntegrity,_T("Some defintegrity"));

		pLog->nSubType = OPTYPE_PWL_AUDIT;
		pLog->processId = 0x7893;

		pMData_Audit = new CWLMetaData(iLogHeadBodyLen,pLogBuf_Audit);


		vecLog.push_back(pMData_Audit);
	}
	//4.OPTYPE_PWL_MODIFY_FILE
	{
		pLogBuf_Modify = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_Modify, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_Modify;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
		//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
		ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

		PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
		pLog->bHoldback                 = 0;
		//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
		WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_MODIFY_FILE, 0, pLog->nSubType);
		pLog->bCertCheckFailed          = 1;
		pLog->bIntegrityCheckFailed     = 1;  

		pLog->llTime = _time32(NULL);  

		std::wstring wsPath = _T("c:\\Tmp\\OPTYPE_PWL_MODIFY_FILE");
		if (!g_bSamePath)
		{
			wsPath += _T("_");
			wsPath += wsThreadId;
			wsPath += _T("_");
			wsPath += wsTime;
		}
		wsPath += _T(".exe");

		_tcscpy(pLog->szFullPath, wsPath.c_str());
		_tcslwr(pLog->szFullPath);
		_tcscpy(pLog->szVersion,_T("7893"));
		_tcscpy(pLog->szCompany,_T("Some Company"));
		_tcscpy(pLog->szProduct,_T("SomeProduct"));
		_tcscpy(pLog->szDefIntegrity,_T("Some defintegrity"));

		pLog->nSubType = OPTYPE_PWL_MODIFY_FILE;
		pLog->processId = 0x7893;



		pMData_Modify = new CWLMetaData(iLogHeadBodyLen,pLogBuf_Modify);


		vecLog.push_back(pMData_Modify);
	}


	CWLJsonParse json;
	std::string  sJson_FinalFive;
	sJson_FinalFive = json.WarningLog_GetJsonByVector(lpComputerID, 200, DATA_TO_SERVER_PROCESS_ALERT_LOG, vecLog);  //�õ��Ϸ���json�ַ���
sJson_FinalFive = InjectComputerIP(sJson_FinalFive, m_strClientIP);

	if (NULL != pMData_Control)
	{
		delete pMData_Control;
	}
	if (NULL != pLogBuf_Control)
	{
		delete [] pLogBuf_Control;
	}
	if (NULL != pMData_Audit)
	{
		delete pMData_Audit;
	}
	if (NULL != pLogBuf_Audit)
	{
		delete [] pLogBuf_Audit;
	}
	if (NULL != pMData_Modify)
	{
		delete pMData_Modify;
	}
	if (NULL != pLogBuf_Modify)
	{
		delete [] pLogBuf_Modify;
	}
	if (NULL != pMData_SysFileCheck)
	{
		delete pMData_SysFileCheck;
	}
	if (NULL != pLogBuf_SysFileCheck)
	{
		delete [] pLogBuf_SysFileCheck;
	}
	if (NULL != pMData_Auto)
	{
		delete pMData_Auto;
	}
	if (NULL != pLogBuf_Auto)
	{
		delete [] pLogBuf_Auto;
	}

	//ʵ�ʷ��ͱ���jsonw
	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	char*		pResult = NULL;
	BOOL		bRet = FALSE;
	bRet = objTmp->pdoPost(URL_PROCESS_LOG, (LPSTR)sJson_FinalFive.c_str(), &pResult);
	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;

		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson_FinalFive);
		WriteError(_T("NWL:send Admin Operation log Error:%s"), wJson.c_str());
	}

	return bRet;
}

BOOL CSendInfoToServer::SendClientOptLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	CString strOpt=_T("");
	WCHAR URL_OPERATORLOG[100] = {0};

	_snwprintf_s(URL_OPERATORLOG, sizeof(URL_OPERATORLOG)/sizeof(URL_OPERATORLOG[0]), _TRUNCATE, URL_LOG_OPERATOR, m_strServerIP, _ttoi(m_strServerPort));

	ADMIN_OPERATION_LOG_STRUCT OperationLogStruct = {0};
	OperationLogStruct.llTime = _time32(NULL);
	OperationLogStruct.dwIsSuccess=1;
	_tcscpy(OperationLogStruct.szUserName, _T("Admin"));

	char chGuid[MAX_PATH]= {0};
	CreateGuidString((LPTSTR)chGuid);
	strOpt.Format(_T("�����ֶΣ������ע--WLServerTest--�������ݣ�%s"),chGuid);
	_tcscpy(OperationLogStruct.szLogContent, strOpt.GetBuffer());

	std::vector<ADMIN_OPERATION_LOG_STRUCT*> vec;
	vec.push_back(&OperationLogStruct);
	CWLJsonParse json;
	std::string sJson = json.UserActionLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, DATA_TO_SERVER_PROCESS_ALERT_LOG, vec);
sJson = InjectComputerIP(sJson, m_strClientIP);


	char *pResult = NULL;

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	if ( objTmp->pdoPost == NULL )
	{
		AfxMessageBox(_T("Function pointer null!"));
	} 

	bRet = objTmp->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);

	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Operation log Error:%s"), wJson.c_str());
	}
	return bRet; 
}

//added by lzq:JUNE06
//��ʱ���ɵ���
/*
BOOL CSendInfoToServer::SendClientThtLogToServer(LPTSTR lpComputerID,int iCurUsedClient)
{
client  sObj;// g_vecAllClientObjects[iCurUsedClient];
CString strCount;

SOCKET sockTemp;
CSendInfoToServer* sSendInfo = {0};

sSendInfo = new CSendInfoToServer();
sSendInfo->CreateConnection(sockTemp,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);

if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP_ThreatLog(sObj,sockTemp))
{
sSendInfo->CreateConnection(sockTemp,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);
g_WLServerTestDlg->SendHeartbeatToserver_TCP_ThreatLog(sObj, sockTemp);
}

DWORD dwRet = g_WLServerTestDlg->RecvHeartBeatBack_TCP_ThreatLog(sockTemp);
if (HEARTBEAT_CMD_BACK == dwRet)
{
// 
}
else if (HEARTBEAT_CMD_POLCY == dwRet)
{
dwRet = g_WLServerTestDlg->SendHeartbeat_ThreatLog(sObj);
if (ERROR_SUCCESS != dwRet)
{
WriteError(_T("CWLHeartBeat::instance()->sendHeartBeat() get policy change failed"));
}
}
else
{

}
sSendInfo->CloseConnection(sockTemp);
delete sSendInfo;
sSendInfo=NULL;

return 0;
}*/

//added by lzq:MAY17 //��Ҫʵ������
/*
BOOL CSendInfoToServer::SendClientThtLogToServer(LPTSTR lpComputerID)
{
BOOL bRet = FALSE;
CString strOpt=_T("");
WCHAR URL_OPERATORLOG[100] = {0};

WLSimulateJson  objSJ;
_snwprintf_s(URL_OPERATORLOG, sizeof(URL_OPERATORLOG)/sizeof(URL_OPERATORLOG[0]), _TRUNCATE, URL_LOG_OPERATOR, m_strServerIP, _ttoi(m_strServerPort));

char* pResult = NULL;

std::string sJson = "";

//ģ��WinEventLog
{
sJson = objSJ.ThreatLog_SimulateJson_WinEventLog(lpComputerID);
bRet = CWLNetCommApi::instance()->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);
if (bRet)
{
if( pResult)
CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
} 
else
{
CWLJsonParse WLJsonParse;
wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
WriteError(_T("send threat log Error:%s"), wJson.c_str());
}
}
//ģ��FileAccess
{
sJson =  objSJ.ThreatLog_SimulateJson_File(lpComputerID);
bRet = CWLNetCommApi::instance()->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);
if (bRet)
{
if( pResult)
CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
}
else
{
CWLJsonParse WLJsonParse;
wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
WriteError(_T("send threat log Error:%s"), wJson.c_str());
}
}
//ģ����̷���ProcessAccess
{
sJson =  objSJ.ThreatLog_SimulateJson_Proc(lpComputerID);
bRet = CWLNetCommApi::instance()->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);
if (bRet)
{
if( pResult)
CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
}
else
{
CWLJsonParse WLJsonParse;
wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
WriteError(_T("send threat log Error:%s"), wJson.c_str());
}
}
//ģ���������Process
{
sJson =  objSJ.ThreatLog_SimulateJson_ProcStart(lpComputerID);
bRet = CWLNetCommApi::instance()->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);
if (bRet)
{
if( pResult)
CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
}
else
{
CWLJsonParse WLJsonParse;
wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
WriteError(_T("send threat log Error:%s"), wJson.c_str());
}
}
//ģ��Registry
{
sJson = objSJ.ThreatLog_SimulateJson_Reg(lpComputerID);
bRet = CWLNetCommApi::instance()->pdoPost(URL_OPERATORLOG, (LPSTR)sJson.c_str(), &pResult);
if (bRet)
{
if( pResult)
CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
}
else
{
CWLJsonParse WLJsonParse;
wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
WriteError(_T("send threat log Error:%s"), wJson.c_str());
}
}

return bRet;
}
*/

BOOL CSendInfoToServer::UkeyToNotifyUSM(LPTSTR lpComputerID, WORD cmdType, WORD cmdID, BASELINE_PL_NEW_ST *pSecbStatus, BASELINE_PL_NEW_ST *pSecbParam, DWORD dwLevel)
{	
	// ����Json��
	std::string sJson;
	CWLJsonParse jsonParser;

	BOOL bRet = FALSE;
	char *pResult = NULL;
	WCHAR url[100] = {0};
	CString cstrComputerID = lpComputerID;
	//WCHAR computerID[MAX_PATH] = {0};

	//sJson = jsonParser.baseLine_PL_New_Status_GetJson(strComputerID, cmdType, cmdID, pSecbStatus, pSecbParam, dwLevel);
	sJson = jsonParser.baseLine_PL_New_Status_GetJson(lpComputerID, cmdType, cmdID, pSecbStatus, pSecbParam, dwLevel);

	WriteDebug(_T("Notify USM, cmdType = %d, cmdID = %d, dwLevel = %d, Json = %S"), cmdType, cmdID, dwLevel, sJson.c_str());

	//_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_SECB_REPORT, strip, iPort);
	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_SECB_REPORT, m_strServerIP, _ttoi(m_strServerPort));

	WriteDebug(_T("pdoPost url is %s"), url);

	WriteInfo(_T("Ready to post base line, computerID=%s"), cstrComputerID);
	bRet = CWLNetCommApi::instance()->pdoPost(url, (LPSTR)sJson.c_str(), &pResult);
	if(!bRet)
	{
		// ����ʧ��
		WriteError(_T("BaseLIne pdoPost Error, computerID=%s"), cstrComputerID);
		if (NULL == pResult)
		{
			WriteInfo(_T("BaseLine send error and pResult is null, computerID=%s"),cstrComputerID );
		}
	}
	else if (NULL != pResult)
	{
		WriteInfo(_T("BaseLine send succ and pResult is not null, pResult=%S"), pResult);
		CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		WriteInfo(_T("BaseLine send succ and pResult is null, computerID=%s"),cstrComputerID);
	}
	return TRUE;
}

BOOL CSendInfoToServer::SendBaseLineToServer(LPTSTR lpComputerID)
{
	CWLJsonParse json;
	std::wstring strFilePath = _T("");
	std::string sJson = "";
	int nErr = 0;

	strFilePath = POLICY_DATA_W_HOSTBASELINE_NEW;
	if( !PathFileExists(strFilePath.c_str())) 
	{
		WriteWarn(_T("path=%s is not exist"), strFilePath.c_str());
		return TRUE;
	}
	sJson = json.ReadJsonFile(strFilePath);

	BASELINE_PL_NEW_ST stSecbParam = {0};
	BASELINE_PL_NEW_ST stSecbStatus = {0};
	DWORD dwLevel = 0;
	//json.baseLine_PL_New_GetValue(sJson, &stSecbParam, dwLevel);

	return UkeyToNotifyUSM(lpComputerID, 150, PLY_CLIENT_BASELINE_NEW, &stSecbStatus, &stSecbParam, dwLevel);
}

BOOL CSendInfoToServer::BOSendUSM_AllUsers_DoPost(LPTSTR lpComputerID, __in ST_USERS_INFO_HEAD & stUsersHead, VEC_ST_USERS_USM & vecUSMUsersSend)
{
	BOOL bRet = TRUE;
	char* pResult = NULL;
	std::string sData; 
	WCHAR URL_SERVER_USBKEY[100] = {0};
	CWLJsonParse jsonParse;

	/* ��װjson�� */
	sData = jsonParse.OSUser_GetJson(lpComputerID, stUsersHead, vecUSMUsersSend);

	WriteDebug(_T("SendUSM_AllUsers_DoPost Json =  %s"), jsonParse.UTF8ToUnicode(sData).c_str());

	/* ���͵�USM */
	_snwprintf_s(URL_SERVER_USBKEY, sizeof(URL_SERVER_USBKEY)/sizeof(URL_SERVER_USBKEY[0]), _TRUNCATE, URL_USBKEY_INFO, m_strServerIP, _ttoi(m_strServerPort));
	//if (! doPost(m_wsURL_USBKEY.c_str(), (LPSTR)sData.c_str(), &pResult))
	if (! CWLNetCommApi::instance()->pdoPost(URL_SERVER_USBKEY, (LPSTR)sData.c_str(), &pResult))
	{
		WriteError(_T("UKey SendUSM_AllUsers_DoPost ERROR. URL=%s, sData= %s"), URL_SERVER_USBKEY, jsonParse.UTF8ToUnicode(sData).c_str());
		bRet = FALSE;
		goto END;
	}
	else
	{
		if( pResult)
		{
			WriteInfo(_T("Ukey send UKey succ and pResult is not null, pResult=%S"), pResult);
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
		}
		else
		{
			WriteInfo(_T("Ukey send UKey succ and pResult is null, computerIS=%s"), lpComputerID);
		}
	}

	WriteDebug(_T("SendUSM_AllUsers_DoPost end"));
	bRet = TRUE;

END:
	return bRet;
}

BOOL CSendInfoToServer::SendUSBKeyManageToServer(LPTSTR lpComputerID,VEC_ST_USERS_USM &vecUSMUsersSend)
{
	BOOL				bRet = FALSE;
	UK_MANAGER_ST		usbKeyManagerInfo;
	std::string			strJsonA;
	ST_USERS_INFO_HEAD  stUsersHead;        /* �ϱ����ĵ�ͷ����Ϣ */


	//bRet = USBKeyManagerInfo_DoPost(lpComputerID, usbKeyManagerInfo, strJsonA, err);
	bRet = BOSendUSM_AllUsers_DoPost(lpComputerID, stUsersHead, vecUSMUsersSend);
	if (!bRet)
	{
		//bRet = USBKeyManagerInfo_DoPost(lpComputerID, usbKeyManagerInfo, strJsonA, err);
		bRet = BOSendUSM_AllUsers_DoPost(lpComputerID, stUsersHead, vecUSMUsersSend);
		if (!bRet)
		{
			WriteError(_T("Ukey DoPost fail, computerID = %s"), lpComputerID);
		}
	}

	return bRet;
}

BOOL CSendInfoToServer::Send_FileLog_WL_ToServer(LPTSTR lpComputerID, CString cstrWLFilePath)
{
	BOOL bRet = FALSE;
	WCHAR url[1024] = {0};
	WLUtils::EM_LOGSERVER_TYPE nType;
	tstring StrIp;
	WORD nPort;
	tstring StrErr;

	//����USM���͡��ϴ��С�״̬
	sendScanStatus(lpComputerID,WL_SOLIDIFY_UPLOAD);

	//Ϊƴ��URL���ļ�·����Ҫ���޸�
	int nLen = wcslen(cstrWLFilePath);
	WCHAR szPath[MAX_PATH] = {0};
	for (int i = 0, j = 0; i < nLen; i++)
	{
		if (L':' == cstrWLFilePath[i])
		{
			continue;
		}
		else if (L'\\' == cstrWLFilePath[i])
		{
			szPath[j++] = L'-';
		}
		else
		{
			szPath[j++] = cstrWLFilePath[i];
		}
	}

	CBase64 base64;
	//URL�а������ļ�·�������Base64
	CString cstrUrlWlFilePath = szPath;
	std::string strTemp = CT2A(cstrUrlWlFilePath.GetString());
	char szTmp[FU_MAX_FILE_LEN] = {0};
	base64.Base64URLEncode(strTemp.c_str(), strTemp.length(), szTmp, FU_MAX_FILE_LEN);
	std::wstring wstrPath = CStrUtil::UTF8ToUnicode(szTmp);

	//URL��ComputerID�����Base64
	strTemp = CStrUtil::UnicodeToUTF8(lpComputerID).c_str();
	base64.Base64URLEncode(strTemp.c_str(), strTemp.length(), szTmp, FU_MAX_FILE_LEN);
	std::wstring wsClientID = CStrUtil::UTF8ToUnicode(szTmp);

	//ƴ��URL
	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_UPLOAD_WHITE_FILE_LIST,
		m_strServerIP, _ttoi(m_strServerPort), wsClientID.c_str(), wstrPath.c_str());

	//�ϴ��ļ�
	DWORD re = CWLNetCommApi::instance()->puploadFile(url, cstrWLFilePath);
	if( ERROR_SUCCESS == re)
	{
		// �ϴ��ɹ���USM����״̬������Ϣ
		sendScanStatus(lpComputerID,WL_SOLIDIFY_UPDATE);
		return TRUE;
	}
	else
	{
		WriteError(_T("����������ϴ�ʧ�ܣ�%lu"),re);
	}
	return bRet;
}

BOOL CSendInfoToServer::CreateGuidString(LPTSTR lpGuid)
{
	if (NULL == lpGuid)
	{
		return FALSE;
	}

	GUID guid;

	if (S_OK != ::CoCreateGuid(&guid))
	{
		return FALSE;
	}

	_stprintf(lpGuid, _T("%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X"),\
		guid.Data1, guid.Data2, guid.Data3, \
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], \
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] );


	return TRUE;
}

/*
* @fn           SendClientDataProtectLogToServer
* @brief        ���ݱ�����־
* @param[in]    lpComputerID
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2024-3-1
*/
BOOL CSendInfoToServer::SendClientDataProtectLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	CString strOpt=_T("");
	WCHAR URL_DataGuard_WarningLog[100] = {0};

	CWLMetaData*  pMData_DataProtectLog = NULL;
	BYTE*         pLogBuf_DataProtectLog = NULL;
	std::vector<CWLMetaData*>  vecLog;

	{
		int  iLogHeadBodyLen  = (sizeof(IPC_LOG_COMMON)+sizeof(EVENT_DATA_PROTECT));
		pLogBuf_DataProtectLog = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_DataProtectLog, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_DataProtectLog;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_DATA_PROTECT;
		ipclogcomm->dwSize = sizeof(EVENT_DATA_PROTECT);

		PEVENT_DATA_PROTECT pLog        = (PEVENT_DATA_PROTECT)ipclogcomm->data;

		pLog->TimeStamp = _time32(NULL); 
		pLog->Result = 1;
		pLog->Action = 1;

		_tcscpy(pLog->Subject, _T("C:\\���ݱ�����������.exe"));
		_tcscpy(pLog->Object, _T("C:\\���ݱ������Կ���.exe"));

		pMData_DataProtectLog = new CWLMetaData(iLogHeadBodyLen,pLogBuf_DataProtectLog);

		vecLog.push_back(pMData_DataProtectLog);
	}

	_snwprintf_s(URL_DataGuard_WarningLog, sizeof(URL_DataGuard_WarningLog)/sizeof(URL_DataGuard_WarningLog[0]), _TRUNCATE, URL_SYNC_DATAGUARD_LOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.DPLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, PLY_DATAPROTECT_POST_LOG, vecLog);
sJson = InjectComputerIP(sJson, m_strClientIP);


	char *pResult = NULL;

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	if ( objTmp->pdoPost == NULL )
	{
		AfxMessageBox(_T("Function pointer null!"));
	} 

	bRet = objTmp->pdoPost(URL_DataGuard_WarningLog, (LPSTR)sJson.c_str(), &pResult);

	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Operation log Error:%s"), wJson.c_str());
	}

	if (NULL != pMData_DataProtectLog)
	{
		delete pMData_DataProtectLog;
	}
	if (NULL != pLogBuf_DataProtectLog)
	{
		delete [] pLogBuf_DataProtectLog;
	}

	return bRet; 
}

/*
* @fn           SendClientSysProtectLogToServer
* @brief        ϵͳ������־
* @param[in]    lpComputerID
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2024-3-1
*/
BOOL CSendInfoToServer::SendClientSysProtectLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	CString strOpt=_T("");
	WCHAR URL_SysProtect_WarningLog[100] = {0};

	CWLMetaData*  pMData_SysProtectLog = NULL;
	BYTE*         pLogBuf_SysProtectLog = NULL;
	std::vector<CWLMetaData*>  vecLog;

	{
		int  iLogHeadBodyLen  = (sizeof(IPC_LOG_COMMON)+sizeof(SYSTEM_GUARD_LOG));
		pLogBuf_SysProtectLog = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_SysProtectLog, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_SysProtectLog;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_SYSTEM_GUARD_LOG;
		ipclogcomm->dwSize = sizeof(SYSTEM_GUARD_LOG);

		PSYSTEM_GUARD_LOG pLog        = (PSYSTEM_GUARD_LOG)ipclogcomm->data;

		pLog->TimeStamp = _time32(NULL); 
		pLog->Result = 1;
		pLog->Action = 1;

		_tcscpy(pLog->Subject, _T("C:\\ϵͳ������������.exe"));
		_tcscpy(pLog->Object, _T("C:\\ϵͳ�������Կ���.exe"));

		pMData_SysProtectLog = new CWLMetaData(iLogHeadBodyLen,pLogBuf_SysProtectLog);

		vecLog.push_back(pMData_SysProtectLog);
	}

	_snwprintf_s(URL_SysProtect_WarningLog, sizeof(URL_SysProtect_WarningLog)/sizeof(URL_SysProtect_WarningLog[0]), _TRUNCATE, URL_SYSTEM_GUARD_LOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.SysGuardLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, DATA_TO_SERVER_SYSGUARD_LOG, vecLog);
sJson = InjectComputerIP(sJson, m_strClientIP);

	char *pResult = NULL;

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	if ( objTmp->pdoPost == NULL )
	{
		AfxMessageBox(_T("Function pointer null!"));
	} 

	bRet = objTmp->pdoPost(URL_SysProtect_WarningLog, (LPSTR)sJson.c_str(), &pResult);

	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Operation log Error:%s, errorcode = 0x%08x"), wJson.c_str(), GetLastError());
	}

	if (NULL != pMData_SysProtectLog)
	{
		delete pMData_SysProtectLog;
	}
	if (NULL != pLogBuf_SysProtectLog)
	{
		delete [] pLogBuf_SysProtectLog;
	}

	return bRet; 
}

/*
* @fn           SendClientBackupLogToServer
* @brief        ������ָ���־
* @param[in]    lpComputerID
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2024-3-8
*/
BOOL CSendInfoToServer::SendClientBackupLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	WCHAR URL_Backup_WarningLog[100] = {0};

	std::vector<BACKUP_FILE_INFO_ST> vecLog;
	BACKUP_FILE_INFO_ST stBackup;

	stBackup.ID = _T("4DBE9C130678C1393162507A6E902ADF04413DE9E9273B4ABF5C3093F6058DDF");
	stBackup.uuid = _T("37D2F730603548CC9EA50297B3A89F12");
	stBackup.wstrFileName = _T("C:\\������ָ������ļ�.txt");
	stBackup.wstrProcessName = _T("C:\\������ָ����Խ���.exe");
	stBackup.wstrHashValue = _T("81659AE1A757A783D8847896027723E8DDC7AB2EA2CB698C3E943A6C0BCC276E");
	stBackup.wstrFileType = _T(".txt");
	stBackup.llFileSize = 1;
	stBackup.llBackupTime = ::time (NULL);
	stBackup.llUpdateTime = ::time (NULL);
	stBackup.llCreateTime = ::time (NULL);
	stBackup.llAccessTime = ::time (NULL);
	stBackup.nAction = BFA_FILE_OPEN;
	stBackup.nReport = eADD_UNREPORTED;

	vecLog.push_back(stBackup);

	_snwprintf_s(URL_Backup_WarningLog, sizeof(URL_Backup_WarningLog)/sizeof(URL_Backup_WarningLog[0]), _TRUNCATE, URL_SYNC_BACKUPLOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.Backup_FileInfo_GetJson(vecLog, NULL, NULL, lpComputerID);

	char *pResult = NULL;

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	if ( objTmp->pdoPost == NULL )
	{
		AfxMessageBox(_T("Function pointer null!"));
	} 

	bRet = objTmp->pdoPost(URL_Backup_WarningLog, (LPSTR)sJson.c_str(), &pResult);

	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Operation log Error:%s, errorcode = 0x%08x"), wJson.c_str(), GetLastError());
	}

	return bRet; 
}

/*
* @fn           SendClientBackupLogToServer
* @brief        ������־
* @param[in]    lpComputerID
* @param[out]   
* @return       
*               
* @detail      
* @author       yxd
* @date         2025-3-5
*/
BOOL CSendInfoToServer::SendClientVirusLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	CString strOpt=_T("");
	WCHAR URL_Virus_WarningLog[100] = {0};

	CWLMetaData*  pMData_VirusLog = NULL;
	BYTE*         pLogBuf_VirusLog = NULL;
	std::vector<CWLMetaData*>  vecLog;

	{
		int  iLogHeadBodyLen  = (sizeof(IPC_LOG_COMMON)+sizeof(VP_ScanVirusLog));
		pLogBuf_VirusLog = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_VirusLog, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_VirusLog;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_SYSTEM_GUARD_LOG;
		ipclogcomm->dwSize = sizeof(VP_ScanVirusLog);

		PVP_ScanVirusLog pLog        = (PVP_ScanVirusLog)ipclogcomm->data;

		pLog->llTime = time(NULL);
		pLog->dwVirusType = 33;  //vtRansom
		pLog->dwResult = eVKR_UNDISPOSED;
		pLog->dwScore = 0;
		pLog->dwLevel = emLV_IGNORE;
		pLog->dwFrom = emFromRTProtect;
		_tcsncpy_s(pLog->wszVirusName, SAFE_PATH_LEN, _T("��������"), SAFE_PATH_LEN);
		_tcsncpy_s(pLog->wszVirusPath,_countof(pLog->wszVirusPath), _T("C:\\VirusPath.exe"),_TRUNCATE);
		_tcsncpy_s(pLog->wszVirusSubPath, SAFE_PATH_LEN, _T("C:\\SubPath.exe"), SAFE_PATH_LEN);

		pMData_VirusLog = new CWLMetaData(iLogHeadBodyLen,pLogBuf_VirusLog);

		vecLog.push_back(pMData_VirusLog);
	}

	_snwprintf_s(URL_Virus_WarningLog, sizeof(URL_Virus_WarningLog)/sizeof(URL_Virus_WarningLog[0]), _TRUNCATE, URL_VIRUS_PROTECT_LOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.VirusLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, DATA_TO_SERVER_VIRUS_LOG, vecLog);

	char *pResult = NULL;

	CWLNetCommApi* objTmp = CWLNetCommApi::instance();

	if ( objTmp->pdoPost == NULL )
	{
		AfxMessageBox(_T("Function pointer null!"));
	} 

	bRet = objTmp->pdoPost(URL_Virus_WarningLog, (LPSTR)sJson.c_str(), &pResult);

	if (bRet)
	{
		if( pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	}
	else
	{
		CWLJsonParse WLJsonParse;
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		WriteError(_T("send Admin Virus log Error:%s, errorcode = 0x%08x"), wJson.c_str(), GetLastError());
	}

	if (NULL != pMData_VirusLog)
	{
		delete pMData_VirusLog;
	}
	if (NULL != pLogBuf_VirusLog)
	{
		delete [] pLogBuf_VirusLog;
	}

	return bRet; 
}
// Helper: build current local time string "YYYY-MM-DD HH:MM:SS"
static void BuildNowTimeStr(char* buf, size_t cch)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf_s(buf, cch, "%04d-%02d-%02d %02d:%02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// NetAdapter: network interface Up/Down event (hotplugDevLog.do, CMDID=204, CMDVER=4, OtherDevType=7)
// Reference: WLUtilities/WLJsonParse.cpp::UsbDiskPlugLog_GetJsonByVector (CMDVER=4 branch, DEV_TYPE_NET)
BOOL CSendInfoToServer::SendClientNetAdapterLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	WCHAR URL_NetAdapterLog[100] = {0};
	_snwprintf_s(URL_NetAdapterLog, sizeof(URL_NetAdapterLog)/sizeof(URL_NetAdapterLog[0]), _TRUNCATE, URL_PLUG_UDISK_INFO, m_strServerIP, _ttoi(m_strServerPort));

	char szTime[32] = {0};
	BuildNowTimeStr(szTime, _countof(szTime));

	CStringA sComputerID(lpComputerID);
	CStringA sClientIP(m_strClientIP);
	// CMDVER appears once. Fields match IEG client: Time, Name, IP, Mac, PlugEvent, OtherDevType=7.
	std::string sJson = "[{\"ComputerID\":\"" + std::string(sComputerID) + "\","
		"\"CMDTYPE\":200,\"CMDID\":204,\"CMDVER\":4,"
		"\"CMDContent\":[],\"CMDUsbContent\":[],"
		"\"CMDContentOtherDev\":[{"
			"\"Time\":\"" + std::string(szTime) + "\","
			"\"Name\":\"Ethernet\","
			"\"IP\":\"" + std::string(sClientIP) + "\","
			"\"Mac\":\"00:11:22:33:44:55\","
			"\"PlugEvent\":2,"
			"\"OtherDevType\":7"
		"}]}]";

	char *pResult = NULL;
	CWLNetCommApi* objTmp = CWLNetCommApi::instance();
	if (objTmp->pdoPost == NULL)
	{
		return FALSE;
	}
	bRet = objTmp->pdoPost(URL_NetAdapterLog, (LPSTR)sJson.c_str(), &pResult);
	WriteInfo(_T("[NETADAPTER] pdoPost ret=%d, json=%S, resp=%S"),
		bRet, sJson.c_str(), pResult ? pResult : "(null)");
	if (pResult)
		CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	return bRet;
}

// ExtDev: external device control (clientULog.do, CMDID=204, various UsbType values)
// dwSubTypeMask: bitmask of selected ExtDev sub-types (same encoding as dwTypes)
// Reference: WLUtilities/WLJsonParse.cpp::ExtDevLog_GetJsonByVector (else branch)
//   Fields per entry: Time, UsbType, LogContent, UserName, FullPath
// IMPORTANT: source file is stored as GBK without BOM; never use wide-char Chinese
//   literals here. Use explicit UTF-8 byte escapes so the compiled bytes are
//   deterministic regardless of compiler /source-charset.
BOOL CSendInfoToServer::SendClientExtDevLogToServer(LPTSTR lpComputerID, DWORD dwSubTypeMask)
{
	BOOL bRet = FALSE;
	WCHAR URL_ExtDevLog[100] = {0};
	_snwprintf_s(URL_ExtDevLog, sizeof(URL_ExtDevLog)/sizeof(URL_ExtDevLog[0]), _TRUNCATE, URL_LOG_USB, m_strServerIP, _ttoi(m_strServerPort));

	// UsbType values aligned with IEG kernel constants UDISK_LOG_TYPE_* and the
	// known-good C# SimulatorApp implementation (commit fbb6f91, LogCategory.cs::GetExtDevUsbType):
	//   USBPORT=15, WPD=14, CDROM=4, WIRELESS=5, USB_ETHERNET_ADAPTER=20,
	//   FD=13, BLUETOOTH=6, SERIALPORT=7, PARALLELPORT=8
	struct ExtDevEntry { DWORD mask; int usbType; const char* nameUtf8; };
	static const ExtDevEntry entries[] = {
		// USB接口使用被禁止
		{ 0x00020000, 15, "USB\xE6\x8E\xA5\xE5\x8F\xA3\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// 移动设备使用被禁止
		{ 0x00040000, 14, "\xE7\xA7\xBB\xE5\x8A\xA8\xE8\xAE\xBE\xE5\xA4\x87\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// CDROM使用被禁止
		{ 0x00080000,  4, "CDROM\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// wifi使用被禁止
		{ 0x00100000,  5, "wifi\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// USB网卡使用被禁止
		{ 0x00200000, 20, "USB\xE7\xBD\x91\xE5\x8D\xA1\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// 软盘使用被禁止
		{ 0x00400000, 13, "\xE8\xBD\xAF\xE7\x9B\x98\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// 蓝牙使用被禁止
		{ 0x00800000,  6, "\xE8\x93\x9D\xE7\x89\x99\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// 串口使用被禁止
		{ 0x01000000,  7, "\xE4\xB8\xB2\xE5\x8F\xA3\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
		// 并口使用被禁止
		{ 0x02000000,  8, "\xE5\xB9\xB6\xE5\x8F\xA3\xE4\xBD\xBF\xE7\x94\xA8\xE8\xA2\xAB\xE7\xA6\x81\xE6\xAD\xA2" },
	};

	char *pResult = NULL;
	CWLNetCommApi* objTmp = CWLNetCommApi::instance();
	if (objTmp->pdoPost == NULL) return FALSE;

	CStringA sCID(lpComputerID);
	for (int i = 0; i < _countof(entries); ++i)
	{
		if (!(dwSubTypeMask & entries[i].mask)) continue;

		char szTime[32] = {0};
		BuildNowTimeStr(szTime, _countof(szTime));

		char szUsbType[16];
		_itoa_s(entries[i].usbType, szUsbType, 10);

		// Fields aligned with C# SimulatorApp BuildUsbDeviceLog (working version,
		// commit fbb6f91): Time, UsbType, LogContent, UserName only (no FullPath).
		std::string sJson = "[{\"ComputerID\":\"" + std::string(sCID) + "\","
			"\"CMDTYPE\":200,\"CMDID\":204,"
			"\"CMDContent\":[{"
				"\"Time\":\"" + std::string(szTime) + "\","
				"\"UsbType\":" + std::string(szUsbType) + ","
				"\"LogContent\":\"" + std::string(entries[i].nameUtf8) + "\","
				"\"UserName\":\"-\""
			"}]}]";

		pResult = NULL;
		BOOL bThis = objTmp->pdoPost(URL_ExtDevLog, (LPSTR)sJson.c_str(), &pResult);
		WriteInfo(_T("[EXTDEV] UsbType=%d, pdoPost ret=%d, json=%S, resp=%S"),
			entries[i].usbType, bThis, sJson.c_str(), pResult ? pResult : "(null)");
		if (pResult)
			CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
		if (bThis) bRet = TRUE;
	}
	return bRet;
}

// UDiskPlug: USB device plug/unplug event (hotplugDevLog.do, CMDID=204, CMDVER=1)
// Reference: WLUtilities/WLJsonParse.cpp::UsbDiskPlugLog_GetJsonByVector (else branch)
//   Fields: Time, UDiskType, serialID, registerStatus, DiskDriverLetter (array), plugEvent
BOOL CSendInfoToServer::SendClientUDiskPlugLogToServer(LPTSTR lpComputerID)
{
	BOOL bRet = FALSE;
	WCHAR URL_UDiskLog[100] = {0};
	_snwprintf_s(URL_UDiskLog, sizeof(URL_UDiskLog)/sizeof(URL_UDiskLog[0]), _TRUNCATE, URL_PLUG_UDISK_INFO, m_strServerIP, _ttoi(m_strServerPort));

	char szTime[32] = {0};
	BuildNowTimeStr(szTime, _countof(szTime));

	CStringA sComputerID(lpComputerID);
	std::string sJson = "[{\"ComputerID\":\"" + std::string(sComputerID) + "\","
		"\"CMDTYPE\":200,\"CMDID\":204,\"CMDVER\":1,"
		"\"CMDContent\":[{"
			"\"Time\":\"" + std::string(szTime) + "\","
			"\"UDiskType\":1,"
			"\"serialID\":\"SIM-001\","
			"\"registerStatus\":0,"
			"\"DiskDriverLetter\":[\"E:\\\\\"],"
			"\"plugEvent\":1"
		"}]}]";

	char *pResult = NULL;
	CWLNetCommApi* objTmp = CWLNetCommApi::instance();
	if (objTmp->pdoPost == NULL) return FALSE;
	bRet = objTmp->pdoPost(URL_UDiskLog, (LPSTR)sJson.c_str(), &pResult);
	WriteInfo(_T("[UDISKPLUG] pdoPost ret=%d, json=%S, resp=%S"),
		bRet, sJson.c_str(), pResult ? pResult : "(null)");
	if (pResult)
		CWLNetCommApi::instance()->pdoDelete((void**)&pResult);
	return bRet;
}
