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
		tm.tv_sec = 2;   //等待超时
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

	std::string sJson = json.ScanStatus_GetJson(lpGuid, 200, DATA_TO_SERVER_SCANSTATUS, dwScanStatus, 0, wsTime.c_str());
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

//1.HB:心跳发送SendData
BOOL CSendInfoToServer::SendData(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID)//1.hb  
{
	BOOL bRes = FALSE;
	CProtocal protocal;
	char *pProtocalData = NULL;
	unsigned int nProtocalLen = 0;
	tstring strErr;
	int nSendCount = 0;


	//协议封装
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

	//循环发送
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

//2.ThreatLog 5种:发送SendData
BOOL CSendInfoToServer::SendData_OnlyCompress(SOCKET sockSend, const char* pSendBuff, unsigned int nSendLen,int cmdID)
{
	BOOL bRes = FALSE;
	CProtocal protocal;
	char *pProtocalData = NULL;
	unsigned int nProtocalLen = 0;
	tstring strErr;
	int nSendCount = 0;


	//协议封装
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

	//循环发送
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

	//接收包头
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv  header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//校验包头
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//解析包头
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//获取命令
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, dwCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//接收包体
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

	//解析包体
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

		if(iRecvTryCount>100)//尝试接收100次
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

BOOL CSendInfoToServer::RegisterClientToServer(CString szComputerID, CString szClientID,CString szComputerIP)
{
	BOOL bResult = FALSE;

	CString str_URL;
	str_URL.Format(URL_CLIENT_INSTALL, m_strServerIP, _ttoi(m_strServerPort));
	//保存安装URL
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

	// FALSE - 不是64位系统； FALSE - 不回收授权节点； TRUE - 是IEG注册（不是SRS）
	std::string sData = m_json.SetUp_GetJson(wsComputerID, sUserName, 0x01, CMD_CLIENT_REGISTRY, sCupName, szComputerIP.GetBuffer(), _T("00-00-00-00-00-00"), _T("Windows 7"), FALSE, FALSE, TRUE, _T("V300R010C01B100"));

	CString stMsg;
	stMsg.Format(_T("Register . IP=%s, data= %S"), m_strServerIP.GetBuffer(), (sData.c_str()));
	WriteInfo(stMsg.GetBuffer());

	char *pResult = NULL;

	//AfxMessageBox(m_wsUrlClientSetup.c_str());
	CWLNetCommApi::instance()->pEnableTLSv1();//added by lzq:必须确保位于同目录WLNetComm.dll

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

	//解析JSON
	int nErrorCode = 0;
	if( !m_json.Setup_CheckResultByJson(sJson, nErrorCode))
	{
		CString strMsg;
		CWLJsonParse WLJsonParse;

		// 将错误信息写入日志
		wstring wJson = WLJsonParse.UTF8ToUnicode(sJson);
		stMsg.Format(_T("doPost Error, Json = %s"),wJson.c_str() );
		WriteInfo(stMsg.GetBuffer());

		// 将错误原因输出到MessageBox
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
// 通过长连接发送威胁检测日志；；完全按照心跳端口192.168.7.254 8441  JinGe
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
//USM:也给返回数据
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

*/ // TCP - 发送威胁日志 
BOOL CSendInfoToServer::SendThreatLog_ToserverTCP(client& pCurClient,SOCKET sockSend, BOOL bHit)//每个线程执行一次这个函数！  每条json其实需要避免粘包：Sleep(100);
{
	CString strMsg = _T("");
	BOOL bResult = FALSE;

	std::wstring wtrsComputerID = pCurClient.Client_GetComputerID();
	std::wstring wstrClientIP = pCurClient.GetClientIP();
	std::wstring wstrClientID = pCurClient.GetClientID();

	//发送json  构建的5种json

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


	//协议封装
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

	//循环发送
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

// TCP - 确认是否有数据可以允许接收，接收时确认返回的数据类型。是策略时，使用HTTPS执行数据的完整接收和处理
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

	//接收包头
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv  header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//校验包头
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//解析包头
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//获取命令
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, uiCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//检查有没有包体，有的话取出来但不处理
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

// HTTPS - 接收数据hm
BOOL CSendInfoToServer::SendThreatLog_(client& curClient)
{

	CWLJsonParse cJson;
	char *retData = NULL;

	WCHAR url[100] = {0};
	std::string strJson = "";

	CString strErrLog = _T("");
	BOOL bResult = FALSE;

	CString strIP = m_strServerIP;
	DWORD dwPort = _tcstoul(_T("8441"), NULL, 10); //10进制CString to DWORD

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_HEARTBEAT, strIP, dwPort);

	int iCpuUseValue = (int)rand() % 100;
	int iMemoryUseValue = (int)rand() % 100;

	std::wstring wtrsComputerID = curClient.Client_GetComputerID();
	std::wstring wstrClientIP = curClient.GetClientIP();
	std::wstring wstrClientID = curClient.GetClientID();
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), iCpuUseValue, iMemoryUseValue, _T("Windows 7"),wstrClientID, wstrClientIP);

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

			//解析随心跳从USM返回的Json或其他数据
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

// HTTPS - 本地解析HTTPS到来的数据
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

			// 返回结果
			char *pResultJson = NULL;
			int iResult = ERROR_SUCCESS;

			//todo 有一部分策略需要构建Json（pResultJson），返回具体信息给USM

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
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), rand()%100, rand()%100, _T("Windows 7"), wstrClientID, wstrClientIP);

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

	//接收包头
	if (!RecvData(sockRecv, saBufHeader, nHeaderLen))
	{
		WriteError(_T("Recv (WL_PORTOCAL_HEAD)header fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//校验包头
	if (!protocal.IsValidHeader(saBufHeader, nHeaderLen))
	{
		WriteError(_T("invalid protocal header, buf[0]=%C, buf[1]=%C"), saBufHeader, saBufHeader+1);
		goto END;
	}

	//解析包头
	if (!protocal.GetProtacalBodyLen(saBufHeader, nHeaderLen, nBodyLen))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//获取命令
	if (!protocal.GetProtacalCmd(saBufHeader, nHeaderLen, uiCmdID))
	{
		WriteError(_T("GetProtacalBodyLen fail,  nHeaderLen=%d"), nHeaderLen);
		goto END;
	}

	//检查有没有包体，有的话取出来但不处理
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

// HTTPS方式，该方式只是用于接收数据 :
BOOL CSendInfoToServer::SendHeartbeat(client& curClient)//lzq:https短链接   实际是去USM请求策略
{
	CWLJsonParse cJson;
	char *retData = NULL;

	WCHAR url[100] = {0};
	std::string strJson = "";

	CString strErrLog = _T("");
	BOOL bResult = FALSE;

	CString strIP = m_strServerIP;
	DWORD dwPort = _tcstoul(_T("8440"), NULL, 10); //10进制CString to DWORD   //modified by lzq:June19    8441-->8440 ;;注意

	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_HEARTBEAT, strIP, dwPort);

	int iCpuUseValue = (int)rand() % 100;
	int iMemoryUseValue = (int)rand() % 100;

	std::wstring wtrsComputerID = curClient.Client_GetComputerID();
	std::wstring wstrClientIP = curClient.GetClientIP();
	std::wstring wstrClientID = curClient.GetClientID();
	strJson = cJson.HeartBeat_GetJson(wtrsComputerID, CMDTYPE_CMD, DATA_TO_SERVER_HEARTBEAT, _T("test.com"), iCpuUseValue, iMemoryUseValue, _T("Windows 7"),wstrClientID, wstrClientIP);

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

			//解析随心跳从USM返回的Json或其他数据
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

			// 返回结果
			char* pResultJson = NULL;
			int  iResult = ERROR_SUCCESS;

			//todo 有一部分策略需要构建Json（pResultJson），返回具体信息给USM

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
				if(!SendExecResult(CMDID, iResult)) //本地应用策略成功，发送结果给USM
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

// HTTPS - 返回解析结果给USM
BOOL CSendInfoToServer::SendExecResult(WORD CMDID, int nDealResult, char *pResultJson)
{
	if ( 3 == CMDID )
	{
		return TRUE;
	}

	BOOL bRet = FALSE;
	WCHAR url[100] = {0};
	DWORD dwPort = _tcstoul(m_strServerPort, NULL, 10); //十进制

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

// HTTPS - 返回解析结果给USM
BOOL CSendInfoToServer::SendExecResult(WORD CMDID, int nDealResult)
{
	if ( 3 == CMDID )
	{
		return TRUE;
	}

	BOOL bRet = FALSE;
	WCHAR url[100] = {0}; 
	DWORD dwPort = _tcstoul(m_strServerPort, NULL, 10); //十进制

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

// HTTPS - 解析CMDID属于策略还是命令
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
	//程序白名单的四种类型
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
	sJson = json.WarningLog_GetJsonByVector(lpComputerID, 200, DATA_TO_SERVER_PROCESS_ALERT_LOG, vecLog);  //得到合法的json字符串


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
//程序白名单的四种类型
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

	// 拼接不同路径使用
	std::wstring wsThreadId = _T("");
	std::wstring wsTime     = _T("");
	if (!g_bSamePath)
	{
		DWORD dwThreadId = GetCurrentThreadId(); // 获取线程ID
		wchar_t buffer[20] = {0};
		// 使用 _itow_s 进行转换
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
	//5.OPTYPE_PWL_AUTO_APPROVE
	{
		pLogBuf_Auto = new BYTE[iLogHeadBodyLen];
		memset(pLogBuf_Auto, 0, iLogHeadBodyLen);

		IPC_LOG_COMMON* ipclogcomm = (IPC_LOG_COMMON*)pLogBuf_Auto;
		ipclogcomm->dwLogType = WL_IPC_LOG_TYPE_ALARM;
		ipclogcomm->dwDetailLogTypeLevel1 = WL_IPC_LOG_TYPE_LEVE_1_PROCESS_WHITELIST;
		//	ipclogcomm->dwDetailLogTypeLevel2 = WL_IPC_LOG_TYPE_LEVE_2_PROCESS_UNWHITELIST_ALLOW;
		ipclogcomm->dwSize = sizeof(WARNING_LOG_STRUCT);

		PWARNING_LOG_STRUCT pLog        = (PWARNING_LOG_STRUCT)ipclogcomm->data;
		pLog->bHoldback                 = 0;
		//pLog->nSubType                  = OPTYPE_PWL_SYSFILE_CHECK;
		WLUtils::WarningLog_Type_2_DB(OPTYPE_PWL_AUTO_APPROVE, 0, pLog->nSubType);
		pLog->bCertCheckFailed          = 1;
		pLog->bIntegrityCheckFailed     = 1;  

		pLog->llTime = _time32(NULL);  

		std::wstring wsPath = _T("c:\\Tmp\\OPTYPE_PWL_AUTO_APPROVE");
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

		pLog->nSubType = OPTYPE_PWL_AUTO_APPROVE;
		pLog->processId = 0x7893;



		pMData_Auto = new CWLMetaData(iLogHeadBodyLen,pLogBuf_Auto);


		vecLog.push_back(pMData_Auto);
	}


	CWLJsonParse json;
	std::string  sJson_FinalFive;
	sJson_FinalFive = json.WarningLog_GetJsonByVector(lpComputerID, 200, DATA_TO_SERVER_PROCESS_ALERT_LOG, vecLog);  //得到合法的json字符串

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

	//实际发送本条jsonw
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
	strOpt.Format(_T("测试字段，无需关注--WLServerTest--操作内容：%s"),chGuid);
	_tcscpy(OperationLogStruct.szLogContent, strOpt.GetBuffer());

	std::vector<ADMIN_OPERATION_LOG_STRUCT*> vec;
	vec.push_back(&OperationLogStruct);
	CWLJsonParse json;
	std::string sJson = json.UserActionLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, DATA_TO_SERVER_PROCESS_ALERT_LOG, vec);


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
//暂时不可调用
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

//added by lzq:MAY17 //需要实现主体
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

//模拟WinEventLog
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
//模拟FileAccess
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
//模拟进程访问ProcessAccess
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
//模拟进程启动Process
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
//模拟Registry
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
	// 创建Json串
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
		// 发送失败
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

	/* 组装json串 */
	sData = jsonParse.OSUser_GetJson(lpComputerID, stUsersHead, vecUSMUsersSend);

	WriteDebug(_T("SendUSM_AllUsers_DoPost Json =  %s"), jsonParse.UTF8ToUnicode(sData).c_str());

	/* 发送到USM */
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
	ST_USERS_INFO_HEAD  stUsersHead;        /* 上报报文的头部信息 */


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

	//先向USM发送“上传中”状态
	sendScanStatus(lpComputerID,WL_SOLIDIFY_UPLOAD);

	//为拼接URL，文件路径需要做修改
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
	//URL中白名单文件路径需加密Base64
	CString cstrUrlWlFilePath = szPath;
	std::string strTemp = CT2A(cstrUrlWlFilePath.GetString());
	char szTmp[FU_MAX_FILE_LEN] = {0};
	base64.Base64URLEncode(strTemp.c_str(), strTemp.length(), szTmp, FU_MAX_FILE_LEN);
	std::wstring wstrPath = CStrUtil::UTF8ToUnicode(szTmp);

	//URL中ComputerID需加密Base64
	strTemp = CStrUtil::UnicodeToUTF8(lpComputerID).c_str();
	base64.Base64URLEncode(strTemp.c_str(), strTemp.length(), szTmp, FU_MAX_FILE_LEN);
	std::wstring wsClientID = CStrUtil::UTF8ToUnicode(szTmp);

	//拼接URL
	_snwprintf_s(url, sizeof(url)/sizeof(url[0]), _TRUNCATE, URL_UPLOAD_WHITE_FILE_LIST,
		m_strServerIP, _ttoi(m_strServerPort), wsClientID.c_str(), wstrPath.c_str());

	//上传文件
	DWORD re = CWLNetCommApi::instance()->puploadFile(url, cstrWLFilePath);
	if( ERROR_SUCCESS == re)
	{
		// 上传成功像USM发送状态更新信息
		sendScanStatus(lpComputerID,WL_SOLIDIFY_UPDATE);
		return TRUE;
	}
	else
	{
		WriteError(_T("程序白名单上传失败：%lu"),re);
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
* @brief        数据保护日志
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

		_tcscpy(pLog->Subject, _T("C:\\数据保护测试主体.exe"));
		_tcscpy(pLog->Object, _T("C:\\数据保护测试客体.exe"));

		pMData_DataProtectLog = new CWLMetaData(iLogHeadBodyLen,pLogBuf_DataProtectLog);

		vecLog.push_back(pMData_DataProtectLog);
	}

	_snwprintf_s(URL_DataGuard_WarningLog, sizeof(URL_DataGuard_WarningLog)/sizeof(URL_DataGuard_WarningLog[0]), _TRUNCATE, URL_SYNC_DATAGUARD_LOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.DPLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, PLY_DATAPROTECT_POST_LOG, vecLog);


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
* @brief        系统防护日志
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

		_tcscpy(pLog->Subject, _T("C:\\系统防护测试主体.exe"));
		_tcscpy(pLog->Object, _T("C:\\系统防护测试客体.exe"));

		pMData_SysProtectLog = new CWLMetaData(iLogHeadBodyLen,pLogBuf_SysProtectLog);

		vecLog.push_back(pMData_SysProtectLog);
	}

	_snwprintf_s(URL_SysProtect_WarningLog, sizeof(URL_SysProtect_WarningLog)/sizeof(URL_SysProtect_WarningLog[0]), _TRUNCATE, URL_SYSTEM_GUARD_LOG, m_strServerIP, _ttoi(m_strServerPort));

	CWLJsonParse json;
	std::string sJson = json.SysGuardLog_GetJsonByVector(lpComputerID, DATA_TO_SERVER_HEARTBEAT, DATA_TO_SERVER_SYSGUARD_LOG, vecLog);

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
* @brief        备份与恢复日志
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
	stBackup.wstrFileName = _T("C:\\备份与恢复测试文件.txt");
	stBackup.wstrProcessName = _T("C:\\备份与恢复测试进程.exe");
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
* @brief        病毒日志
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
		_tcsncpy_s(pLog->wszVirusName, SAFE_PATH_LEN, _T("勒索病毒"), SAFE_PATH_LEN);
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