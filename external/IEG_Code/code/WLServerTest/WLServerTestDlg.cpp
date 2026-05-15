// WLServerTestDlg.cpp : ʵļ




//









#include "stdafx.h"




#include "WLServerTest.h"
#include "WLServerTestDlg.h"
#include "WhitelistPreviewDlg.h"
#include "VersionManagementDlg.h"
#include "LogCategoryHelpDlg.h"
#include "RawPacketDlg.h"

#include "ProfileConfig.h"




#include "SendInfoToServer.h"




#include "client.h"




#include <sstream>



#include <string>




#include <set>








// Forward declarations for new sub-dialog classes



class CWhitelistPreviewDlg;



class CVersionManagementDlg;



class CLogCategoryHelpDlg;



class CRawPacketDlg;

















#ifdef _DEBUG




#define new DEBUG_NEW




#endif









int g_nTotalRegistered_ClientCount = 0;		              //ǰעĿͻ?









int g_nHeartBeatSending_ClientCount = 0;









volatile long g_nHeartBeatNotSending_ClientCount = 0;     //ǰδĿͻ




volatile long g_nFileLogNotSending_ClientCount = 0;       //ǰδļ־Ŀͻ









volatile long g_nMsgLogNotSending_ClientCount = 0;        //ǰδϢ־Ŀͻ   volatile long









volatile int g_iThreadCount_RegSameTime = 0;        //









CString  g_strDefaultPrefix			= _T("WLClient_");




CString  g_strDefaultStartNum		= _T("1");




CString  g_strDefaultStartIP		= _T("6.6.6.6");














CString g_cstrIPAd;		              //ûõ׸ͻ˵IPַ









vector<client> g_vecAllClientObjects; //еǰעĿͻ˶?














CCriticalSection g_csApplogCount;	    // ͻ־ٽ




CCriticalSection g_csApplogListCtrl;	// ־бٽ




CCriticalSection g_csVecClients;        // g_*ȫֱٽ









CSingleLock g_singleLockGManage(&g_csVecClients);     // ȫֱ




CSingleLock g_singleLockApplogCount(&g_csApplogCount);// ͻ־




CSingleLock g_singleLockListCtrl(&g_csApplogListCtrl);// ?	









BOOL g_bStopTask = FALSE;
BOOL g_bStopLogTask = FALSE; // r3: log-only stop flag
volatile LONG g_nActiveHbThreads = 0; // v16: active HB thread counter for safe Reset









int g_nSocketCount = 0;




SOCKET g_sock[1000] = {INVALID_SOCKET};









BOOL g_bSamePath = FALSE;









// Ӧó򡰹ڡ˵? CAboutDlg Ի









class CAboutDlg : public CDialog




{




public:




	CAboutDlg();









// Ի




	enum { IDD = IDD_ABOUTBOX };









	protected:




	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧









// ʵ




protected:




	DECLARE_MESSAGE_MAP()




};









CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)




{




}









void CAboutDlg::DoDataExchange(CDataExchange* pDX)




{




	CDialog::DoDataExchange(pDX);




}









BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)




ON_BN_CLICKED(IDC_RADIO_OS_WINDOWS, &CWLServerTestDlg::OnBnClickedOsWindows)



ON_BN_CLICKED(IDC_RADIO_OS_LINUX, &CWLServerTestDlg::OnBnClickedOsLinux)



ON_BN_CLICKED(IDC_BUTTON_HB_START, &CWLServerTestDlg::OnBnClickedHbStart)



ON_BN_CLICKED(IDC_BUTTON_HB_STOP, &CWLServerTestDlg::OnBnClickedHbStop)



ON_BN_CLICKED(IDC_BUTTON_LOG_ADD, &CWLServerTestDlg::OnBnClickedLogAdd)



ON_BN_CLICKED(IDC_BUTTON_LOG_STOP, &CWLServerTestDlg::OnBnClickedLogStop)



ON_BN_CLICKED(IDC_BUTTON_WL_UPLOAD, &CWLServerTestDlg::OnBnClickedWlUpload)



ON_BN_CLICKED(IDC_BUTTON_WL_STOP2, &CWLServerTestDlg::OnBnClickedWlStop2)



ON_BN_CLICKED(IDC_BUTTON_WL_PREVIEW, &CWLServerTestDlg::OnBnClickedWlPreview)



ON_BN_CLICKED(IDC_BUTTON_VER_MGMT, &CWLServerTestDlg::OnBnClickedVerMgmt)



ON_BN_CLICKED(IDC_BUTTON_LOG_HELP, &CWLServerTestDlg::OnBnClickedLogHelp)






ON_CBN_SELCHANGE(IDC_COMBO_CLIENT_VERSION, &CWLServerTestDlg::OnClientVersionSelChange)



ON_WM_TIMER()






ON_WM_ERASEBKGND()



ON_WM_DESTROY()



ON_MESSAGE(WM_APP+100, &CWLServerTestDlg::OnAppendLogOutput)



END_MESSAGE_MAP()




  









// CWLServerTestDlg Ի









// WLHeartBeat.hļиƹ




#define HEARTBEAT_CMD_NOREGISTER	18		//TCPأͻδע




#define HEARTBEAT_CMD_POLCY			17		//Աָ?




#define HEARTBEAT_CMD_BACK			1		//ָ









CWLServerTestDlg* g_WLServerTestDlg = NULL;









CWLServerTestDlg::CWLServerTestDlg(CWnd* pParent): CDialog(CWLServerTestDlg::IDD, pParent), m_lMsgLogSuccessCount(0),m_lUploadState_SuccessCount(0), m_lFileLog_WL_SuccessCount(0),
  m_hBrushDlg(NULL), m_hBrushWhite(NULL), m_hBrushPlug(NULL), m_hBrushExt(NULL), m_hBrushThreat(NULL), m_nLinuxHbIntervalMs(30000) //ȴIniȡȡʧܣĬֵдIni




{  




	// ip




	m_strServerIP = CProfileConfig::GetProfileConfigInstance()->ReadServerIP_FromIni();




	if(m_strServerIP.GetLength() == 0)




	{




		m_strServerIP = _T("192.168.7.254");




		




		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);




	}




 




	// ˿




	m_strServerPort = CProfileConfig::GetProfileConfigInstance()->ReadServerPort_FromIni();




	if(m_strServerPort.GetLength() == 0)




	{




		m_strServerPort = _T("8441");









		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);




	}









	// ˿




	m_strServerPortHB = CProfileConfig::GetProfileConfigInstance()->ReadServerHBPort_FromIni();




	if(m_strServerPortHB.GetLength() == 0)




	{




		m_strServerPortHB = _T("4575");









		CProfileConfig::GetProfileConfigInstance()->WriteServerHBPort_ToIni(m_strServerPortHB);




	}









	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);




}









void CWLServerTestDlg::DoDataExchange(CDataExchange* pDX)




{




    CDialog::DoDataExchange(pDX);




    DDX_Text(pDX, IDC_EDIT_SERV_IP,		m_strServerIP);




    DDX_Text(pDX, IDC_EDIT_SERV_PORT,	m_strServerPort);




    DDX_Text(pDX, IDC_EDIT_SERV_PORT2,	m_strServerPortHB);














    DDX_Text(pDX, IDC_EDIT_IDPRE,			m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




    DDX_Text(pDX, IDC_EDIT_STARTNUM,		m_strEditCtrl_ToRegisterClient_StartNum);




    DDX_Control(pDX, IDC_COMBO_REG_COUNT,	m_comRegButton_ToRegisterClient_ClientCount);




    DDX_Control(pDX, IDC_IPADDRESS_CLIENT,  m_ctrlBox_ToRegisterClient_FirstClientIP);














    DDX_Control(pDX, IDC_COMBO_HEARTBEAT_ClientCount, m_comHB_ClientCount);




    DDX_Control(pDX, IDC_COMBO_HEARTBEAT_IntervalTime, m_comHB_Interval);




    DDX_Control(pDX, IDC_COMBO_HEARTBEAT_TotalMimutes, m_comHB_TotalMinutes);
    DDX_Control(pDX, IDC_EDIT_HB_DURATION, m_editHbDuration);




    DDX_Control(pDX, IDC_LIST_HEARTBEAT_ListShowWindow, m_listHeartBeat_MainWindow);














    DDX_Control(pDX, IDC_COMBO_APPLOG_COUNT, m_comAppLog_Task_ClientCount);




    DDX_Control(pDX, IDC_COMBO_APPLOG_HZ, m_comAppLog_Task_EachClientTotalItems);




    DDX_Control(pDX, IDC_COMBO_APPLOG_TIME, m_comAppLog_Task_EachClientPerSecondItems);









    DDX_Control(pDX, IDC_LIST_LOWPART_ListShowWindow, m_listLowPart_MainWindow);




    DDX_Control(pDX, IDC_STATIC_APPLOG, m_staticApplog);




    DDX_Control(pDX, IDC_STATIC_HEARTBEAT_OnlineClientCountRigthData, m_OnlineClientCountRigthData);




    DDX_Control(pDX, IDC_STATIC_RegisteredClientCount, m_cRegisteredClientCounts);




    



    



    








    DDX_Control(pDX, IDC_STATIC_FILELOG, m_staticFilelog);  // ļ־(ɹ/):




    DDX_Control(pDX, IDC_WL_FILE_PATH_EDIT, m_WLFilePathEdit);//ļѡ?




    DDX_Control(pDX, IDC_STATIC_WL_SUC_NUM, m_WL_SuccessNum_Left);




    DDX_Control(pDX, IDC_STATIC_STATE_ALL_NUM, m_StateUKey_BLine_AllNum_RightTotal);




    DDX_Control(pDX, IDC_STATIC_LOG_SUC_NUM, mMsgLog_ThreatOpt_SuccessCount_Left);




    DDX_Control(pDX, IDC_STATIC_STATE_SUC_NUM, m_StateUKey_BLine_SuccessNum_Left);




    DDX_Control(pDX, IDC_STATIC_WL_ALL_NUM, m_WL_TatalNum_RightTotal);




    DDX_Control(pDX, IDC_STATIC_LOG_ALL_NUM, mMsgLog_ThreatOpt_TotalCount_RightTotal);




    DDX_Control(pDX, IDC_EDIT_SERV_IP, m_ServerIPAddress);




    DDX_Control(pDX, IDC_EDIT_SERV_PORT, m_ServerRegPort);




    DDX_Control(pDX, IDC_EDIT_SERV_PORT2, m_ServerHBPort);




    DDX_Control(pDX, IDC_OPT_REGISTER_SAMETIME, m_bRegisterSameTime);




    DDX_Control(pDX, IDC_RegisteredThreadCount, m_RegisterThreadCount);




    DDX_Control(pDX, IDC_EDIT_REGFAIL_CD, m_EditRegFailCD);




    DDX_Control(pDX, IDC_UPLOADWLCOUNT, m_StaticUploadWLCount);



    // === Phase 4: New control bindings ===



    DDX_Control(pDX, IDC_CHECK_USE_LOG_SERVER,   m_chkUseLogServer);



    DDX_Control(pDX, IDC_EDIT_LOG_HOST,          m_editLogHost);



    DDX_Control(pDX, IDC_EDIT_LOG_PORT,          m_editLogPort);



    DDX_Control(pDX, IDC_RADIO_OS_WINDOWS,       m_radioOsWin);



    DDX_Control(pDX, IDC_RADIO_OS_LINUX,         m_radioOsLinux);



    DDX_Control(pDX, IDC_COMBO_CLIENT_VERSION,   m_comboClientVersion);



    DDX_Control(pDX, IDC_COMBO_PROJECT_TYPE,     m_comboProjectType);



    DDX_Control(pDX, IDC_CAT_CLIENT_OPS,         m_catClientOps);



    DDX_Control(pDX, IDC_CAT_OS,                 m_catOs);



    DDX_Control(pDX, IDC_CAT_OUTBOUND,           m_catOutbound);



    DDX_Control(pDX, IDC_CAT_FILE_PROTECT,       m_catFileProtect);



    DDX_Control(pDX, IDC_CAT_REG_PROTECT,        m_catRegProtect);



    DDX_Control(pDX, IDC_CAT_MANDATORY_ACCESS,   m_catMandatoryAccess);



    DDX_Control(pDX, IDC_CAT_VIRUS_ALERT,        m_catVirusAlert);



    DDX_Control(pDX, IDC_CAT_USB,                m_catUsb);



    DDX_Control(pDX, IDC_CAT_USB_WARNING,        m_catUsbWarning);



    DDX_Control(pDX, IDC_CAT_FIREWALL,           m_catFirewall);



    DDX_Control(pDX, IDC_CAT_VULN_PROTECT,       m_catVulnProtect);



    DDX_Control(pDX, IDC_CAT_PROC_AUDIT,         m_catProcAudit);



    DDX_Control(pDX, IDC_CAT_NON_WHITELIST,      m_catNonWhitelist);



    DDX_Control(pDX, IDC_CAT_WL_TAMPER,          m_catWlTamper);



    DDX_Control(pDX, IDC_CAT_SYS_GUARD,          m_catSysGuard);



    DDX_Control(pDX, IDC_CAT_UDISK_PLUG,         m_catUDiskPlug);



    DDX_Control(pDX, IDC_CAT_NET_ADAPTER,        m_catNetAdapter);



    DDX_Control(pDX, IDC_CAT_EXT_USB_PORT,       m_catExtUsbPort);



    DDX_Control(pDX, IDC_CAT_EXT_WPD,            m_catExtWpd);



    DDX_Control(pDX, IDC_CAT_EXT_CDROM,          m_catExtCdrom);



    DDX_Control(pDX, IDC_CAT_EXT_WLAN,           m_catExtWlan);



    DDX_Control(pDX, IDC_CAT_EXT_USB_ETH,        m_catExtUsbEth);



    DDX_Control(pDX, IDC_CAT_EXT_FLOPPY,         m_catExtFloppy);



    DDX_Control(pDX, IDC_CAT_EXT_BT,             m_catExtBt);



    DDX_Control(pDX, IDC_CAT_EXT_SERIAL,         m_catExtSerial);



    DDX_Control(pDX, IDC_CAT_EXT_PARALLEL,       m_catExtParallel);



    DDX_Control(pDX, IDC_CAT_THREAT_PROC,        m_catThreatProc);



    DDX_Control(pDX, IDC_CAT_THREAT_REG,         m_catThreatReg);



    DDX_Control(pDX, IDC_CAT_THREAT_FILE,        m_catThreatFile);



    DDX_Control(pDX, IDC_CAT_THREAT_DLL,         m_catThreatDll);



    DDX_Control(pDX, IDC_CAT_THREAT_OS,          m_catThreatOs);



    DDX_Control(pDX, IDC_EDIT_HB_INTERVAL,       m_editHbInterval);



    DDX_Control(pDX, IDC_CHECK_POLICY_RECV,      m_chkPolicyRecv);



    DDX_Control(pDX, IDC_STATIC_HB_BADGE,        m_staticHbBadge);



    DDX_Control(pDX, IDC_EDIT_WL_CONCURRENT,     m_editWlConcurrent);



    DDX_Control(pDX, IDC_CHECK_AUTO_WL,          m_chkAutoWl);



    DDX_Control(pDX, IDC_EDIT_LOG_TOTAL,         m_editLogTotal);



    DDX_Control(pDX, IDC_EDIT_HTTPS_COUNT,       m_editHttpsCount);



    DDX_Control(pDX, IDC_EDIT_HTTPS_EPS,         m_editHttpsEps);



    DDX_Control(pDX, IDC_EDIT_TCP_COUNT,         m_editTcpCount);



    DDX_Control(pDX, IDC_EDIT_TCP_EPS,           m_editTcpEps);



    DDX_Control(pDX, IDC_EDIT_TCP_HIT,           m_editTcpHit);



    DDX_Control(pDX, IDC_STATIC_REG_TOTAL,       m_stRegTotal);



    DDX_Control(pDX, IDC_STATIC_REG_SUCC,        m_stRegSucc);



    DDX_Control(pDX, IDC_STATIC_REG_FAIL,        m_stRegFail);



    DDX_Control(pDX, IDC_STATIC_HB_ONLINE,       m_stHbOnline);



    DDX_Control(pDX, IDC_STATIC_HB_TOTAL,        m_stHbTotal);



    DDX_Control(pDX, IDC_STATIC_HB_POLICY,       m_stHbPolicy);



    DDX_Control(pDX, IDC_STATIC_LOG_SUCC2,       m_stLogSucc2);



    DDX_Control(pDX, IDC_STATIC_LOG_FAIL,        m_stLogFail);



    DDX_Control(pDX, IDC_STATIC_WL_SUCC2,        m_stWlSucc2);



    DDX_Control(pDX, IDC_STATIC_WL_FAIL2,        m_stWlFail2);



    DDX_Control(pDX, IDC_STATIC_HB_RESP,         m_stHbResp);



    DDX_Control(pDX, IDC_EDIT_LOG_OUTPUT,        m_editLogOutput);



    DDX_Control(pDX, IDC_STATIC_OSINFO_TEXT,     m_staticOsInfo);








}









BEGIN_MESSAGE_MAP(CWLServerTestDlg, CDialog)




	ON_WM_SYSCOMMAND()




	ON_WM_PAINT()




	ON_WM_QUERYDRAGICON()




	//}}AFX_MSG_MAP




	ON_BN_CLICKED(IDC_BUTTON_REG_REG, &CWLServerTestDlg::OnBnClicked_RegisterClients)




	ON_BN_CLICKED(IDC_BUTTON_HEARTBEAT_AddTask, &CWLServerTestDlg::OnBnClickedButtonHeartbeat_AddTask)








	ON_BN_CLICKED(IDC_BUTTON_REG_RESET, &CWLServerTestDlg::OnBnClicked_ClientReg_Reset)




	ON_BN_CLICKED(IDC_Btn_TestConn, &CWLServerTestDlg::OnBnClicked_PortsTest)








//	ON_WM_DROPFILES()




//ON_WM_DROPFILES()




ON_BN_CLICKED(IDC_BUTTON_STOP_TASK, &CWLServerTestDlg::OnBnClicked_Lowest_StopTask)




ON_WM_VSCROLL()
















































ON_BN_CLICKED(IDC_OPT_REGISTER_SAMETIME, &CWLServerTestDlg::OnBnClickedOptRegisterSametime)




ON_BN_CLICKED(IDC_RADIO_OS_WINDOWS, &CWLServerTestDlg::OnBnClickedOsWindows)



ON_BN_CLICKED(IDC_RADIO_OS_LINUX, &CWLServerTestDlg::OnBnClickedOsLinux)



ON_BN_CLICKED(IDC_BUTTON_HB_START, &CWLServerTestDlg::OnBnClickedHbStart)



ON_BN_CLICKED(IDC_BUTTON_HB_STOP, &CWLServerTestDlg::OnBnClickedHbStop)



ON_BN_CLICKED(IDC_BUTTON_LOG_ADD, &CWLServerTestDlg::OnBnClickedLogAdd)



ON_BN_CLICKED(IDC_BUTTON_LOG_STOP, &CWLServerTestDlg::OnBnClickedLogStop)



ON_BN_CLICKED(IDC_BUTTON_WL_UPLOAD, &CWLServerTestDlg::OnBnClickedWlUpload)



ON_BN_CLICKED(IDC_BUTTON_WL_STOP2, &CWLServerTestDlg::OnBnClickedWlStop2)



ON_BN_CLICKED(IDC_BUTTON_WL_PREVIEW, &CWLServerTestDlg::OnBnClickedWlPreview)



ON_BN_CLICKED(IDC_BUTTON_VER_MGMT, &CWLServerTestDlg::OnBnClickedVerMgmt)



ON_BN_CLICKED(IDC_BUTTON_LOG_HELP, &CWLServerTestDlg::OnBnClickedLogHelp)






ON_WM_TIMER()



ON_MESSAGE(WM_APP+100, &CWLServerTestDlg::OnAppendLogOutput)




ON_CBN_SELCHANGE(IDC_COMBO_PROJECT_TYPE, &CWLServerTestDlg::OnProjectTypeSelChange)

ON_BN_CLICKED(IDC_BUTTON_RAWPACKET, &CWLServerTestDlg::OnBnClickedRawPacket)
END_MESSAGE_MAP()














 




int Check_FirstIP_TotalRegCount_Overflow(int IPAddrClips[4],int iRegCount)//Ϸ1Ƿ0ºͻIP?2




{




	//׸ͻIPΪX.0.0.0,Ϸ




	if (IPAddrClips[1] == 0 && IPAddrClips[2] == 0  && IPAddrClips[3] == 0)




	{




		return 0;




	}









	//IPַж?




	int maxIPAddrClips[4] = {0};




	maxIPAddrClips[3] = (IPAddrClips[3] + iRegCount) % 255;




	maxIPAddrClips[2] = (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) % 255;




	maxIPAddrClips[1] = (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) % 255;




	maxIPAddrClips[0] = IPAddrClips[0] 




	+ (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) / 255;









	if (maxIPAddrClips[0] >= 255)//עһͻIP254.254.254.254Ϸ




	{




		return 2;




	}









	return 1;




}









void CWLServerTestDlg::PrepareVecClients_UpdateControls()




{









	USES_CONVERSION;




	char* pstr_Tmp_PreviousRegistered_StartIP = T2A(m_strPreviousRegistered_StartIP); 




	int nFirstIP_FromIni[4] = {0};						//ǴIniõFirstClientIP--4int




	sscanf(pstr_Tmp_PreviousRegistered_StartIP, "%d.%d.%d.%d", &nFirstIP_FromIni[0],&nFirstIP_FromIni[1],&nFirstIP_FromIni[2],&nFirstIP_FromIni[3]);









	int iTmpRet = Check_FirstIP_TotalRegCount_Overflow(nFirstIP_FromIni,m_iPreviousRegistered_ClientCount);




	if (0 == iTmpRet)




	{




		AfxMessageBox(_T("起始IP地址格式不正确"));




		return;




	}




	else if(2 == iTmpRet)




	{




		AfxMessageBox(_T("注册结束，IP地址超出范围"));




		return;




	}









	int iClientStartNum=_ttoi(m_strPreviousRegistered_StartNum.GetBuffer());









	int i = 0;




	int tmpIPAddrClips[4] = {0};




	CString szThisClientIP=_T("");




	CString szClientID_PrefixNum = _T("");









	CString sThisClientIndex=_T("");









	for ( i = 0; i< m_iPreviousRegistered_ClientCount; i++)




	{




		sThisClientIndex=_T("");




		sThisClientIndex.Format(_T("%04d"),iClientStartNum + i);




		szClientID_PrefixNum = m_strEditCtrl_ToRegisterClient_ClientIDPrefix;




		szClientID_PrefixNum.Append(sThisClientIndex);//ͨǰ׺+ClientNum









		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;




		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;




		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;




		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;




		szThisClientIP=_T("");




		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);









		client objTmpClient(szClientID_PrefixNum,szThisClientIP);




		objTmpClient.Client_SetComputerID();









		g_nHeartBeatNotSending_ClientCount++; //ǰעClientCount. ҲΪûù




		g_nMsgLogNotSending_ClientCount++;




		g_nFileLogNotSending_ClientCount++;









		g_vecAllClientObjects.push_back(objTmpClient); 




	}




	




	CString strPreRegCount;




	strPreRegCount.Format(_T("已注册: %d"), m_iPreviousRegistered_ClientCount);




	m_cRegisteredClientCounts.SetWindowText(strPreRegCount);




	  




	{//Clientؿؼ?




		




		//´עClientʱ�?????�ʼ�?StartNum   IP  




		sThisClientIndex=_T("");




		sThisClientIndex.Format(_T("%04d"),iClientStartNum + i);









		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;




		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;




		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;




		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;




		szThisClientIP=_T("");




		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);




		




		//2.µStartIP (2.2.2.2) ʾ SetWindowsText




		//m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(szThisClientIP); // v5: ע��󲻸结束�ʼIP




			 




		//3.SetWindowsText IDPrefix




		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);









		//4.µStartNum (200+10) ʾ SetWindowsText 




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(sThisClientIndex);




	}














	//Ժ  Ҫ ؼµĿʼ?  ֵ




};









BOOL CWLServerTestDlg::OnInitDialog()




{




	CDialog::OnInitDialog();









	// ...˵ӵϵͳ˵С









	// IDM_ABOUTBOX ϵͳΧ�?




	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);




	ASSERT(IDM_ABOUTBOX < 0xF000);









	CMenu* pSysMenu = GetSystemMenu(FALSE);




	if (pSysMenu != NULL)




	{




		BOOL bNameValid;




		CString strAboutMenu;




		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);




		ASSERT(bNameValid);




		if (!strAboutMenu.IsEmpty())




		{




			pSysMenu->AppendMenu(MF_SEPARATOR);




			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);




		}




	}  









	// ô˶ԻͼꡣӦóڲǶԻʱܽԶִд�?




	SetIcon(m_hIcon, TRUE);			// ôͼ




	SetIcon(m_hIcon, FALSE);		// Сͼ









	// TODO: ڴӶĳʼ?




	DWORD dwStyle = m_listHeartBeat_MainWindow.GetExtendedStyle(); 




	dwStyle |= LVS_EX_FULLROWSELECT /*| LVS_EX_GRIDLINES*/ | LVS_EX_FLATSB | LVS_EX_SUBITEMIMAGES /*| LVS_EX_CHECKBOXES*/; 




	m_listHeartBeat_MainWindow.SetExtendedStyle(dwStyle);




	  m_listLowPart_MainWindow.SetExtendedStyle(dwStyle);









	WORD wVersionRequested;




	WSAData wsaData;




	wVersionRequested = MAKEWORD( 2, 2 );









	int err;




	err = WSAStartup(wVersionRequested, &wsaData);




	if (err != 0 )




	{




		AfxMessageBox(_T("WSAStartup() called failed!"));




		return -1;




	}




	else




	{




		//printf("WSAStartup() called successful!\n");









	}




	CString strHeartBeat_TitleLine;




	CString strLog_MSGFILE_TitleLine;




	    strHeartBeat_TitleLine.Format(_T("序号,50;开始时间,136;客户端数,80;心跳/发送间隔(ms),100;心跳时长(min),0;活跃客户端/消息日志,130;状态,90;更新时间/文件日志,136;失败客户端数,150"));




	  strLog_MSGFILE_TitleLine.Format(_T("序号,50;开始时间,150;客户端数,80; 发送间隔(ms),100;发送时长(min),0;消息日志,130;文件日志,100;状态,100"));









	m_listHeartBeat_MainWindow.SetHeadings(strHeartBeat_TitleLine);




	  m_listLowPart_MainWindow.SetHeadings(strLog_MSGFILE_TitleLine);









	m_listHeartBeat_MainWindow.SetEnableSort(FALSE);




	  m_listLowPart_MainWindow.SetEnableSort(FALSE);









    m_comRegButton_ToRegisterClient_ClientCount.SetWindowText(_T("1"));
        GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(_T("1"));









	




	//m_comHB_ClientCount.Create(CBS_DROPDOWNLIST | WS_VISIBLE | WS_CHILD, CRect(10, 10, 150, 300), pParentWnd, IDC_COMBO_BOX);




	m_comHB_ClientCount.AddString(_T("1"));




	m_comHB_ClientCount.AddString(_T("10"));




	m_comHB_ClientCount.AddString(_T("20"));




	m_comHB_ClientCount.AddString(_T("50"));




	m_comHB_ClientCount.AddString(_T("100"));




	m_comHB_ClientCount.AddString(_T("200"));




	m_comHB_ClientCount.AddString(_T("300"));




	m_comHB_ClientCount.AddString(_T("400"));




	m_comHB_ClientCount.AddString(_T("500"));




	m_comHB_ClientCount.AddString(_T("1000"));




	m_comHB_ClientCount.SetCurSel(0);









	m_comHB_Interval.AddString(_T("30000"));




	m_comHB_Interval.SetCurSel(0);




	









	m_comHB_TotalMinutes.AddString(_T("1"));




	m_comHB_TotalMinutes.AddString(_T("10"));




	m_comHB_TotalMinutes.AddString(_T("15"));




	m_comHB_TotalMinutes.AddString(_T("30"));




	m_comHB_TotalMinutes.AddString(_T("60"));




	m_comHB_TotalMinutes.AddString(_T("120"));




	m_comHB_TotalMinutes.AddString(_T("300"));




	m_comHB_TotalMinutes.AddString(_T("600"));




	m_comHB_TotalMinutes.AddString(_T("1200"));




	m_comHB_TotalMinutes.AddString(_T("2400"));




	m_comHB_TotalMinutes.AddString(_T("4800"));




	m_comHB_TotalMinutes.AddString(_T("7200"));




	m_comHB_TotalMinutes.AddString(_T("14400")); // r8: 10 天，作为新UI默认




	m_comHB_TotalMinutes.SetCurSel(m_comHB_TotalMinutes.GetCount() - 1); // r8: 默认选 14400 分钟 (10 天)

	// v5.2: 心跳时长输入框默认 7200 分钟
	if (m_editHbDuration.GetSafeHwnd()) m_editHbDuration.SetWindowText(_T("7200"));



















	m_comAppLog_Task_ClientCount.AddString(_T("1"));




	m_comAppLog_Task_ClientCount.AddString(_T("2"));




	m_comAppLog_Task_ClientCount.AddString(_T("5"));




	m_comAppLog_Task_ClientCount.AddString(_T("10"));




	m_comAppLog_Task_ClientCount.AddString(_T("20"));




	m_comAppLog_Task_ClientCount.AddString(_T("50"));




	m_comAppLog_Task_ClientCount.AddString(_T("100"));




	m_comAppLog_Task_ClientCount.AddString(_T("200"));




	m_comAppLog_Task_ClientCount.AddString(_T("300"));




	m_comAppLog_Task_ClientCount.AddString(_T("400"));




	m_comAppLog_Task_ClientCount.AddString(_T("500"));




	m_comAppLog_Task_ClientCount.AddString(_T("1000"));




	m_comAppLog_Task_ClientCount.SetCurSel(0);









	m_comAppLog_Task_EachClientTotalItems.AddString(_T("5"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("10"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("50"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("100"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("500"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("1000"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("5000"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("10000"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("50000"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("100000"));




	m_comAppLog_Task_EachClientTotalItems.AddString(_T("500000"));




	m_comAppLog_Task_EachClientTotalItems.SetCurSel(0);









	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("1"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("2"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("5"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("10"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("20"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("50"));




	m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("100"));




	//m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("200"));




	//m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("500"));




	//m_comAppLog_Task_EachClientPerSecondItems.AddString(_T("1000"));




	m_comAppLog_Task_EachClientPerSecondItems.SetCurSel(0);









	g_WLServerTestDlg = this;




	




	m_bRegisterSameTime_Now = FALSE;




	m_EditRegFailCD.SetWindowText(_T("30"));




	m_ulSametimeRegister_UploadWLCount = 0;




	









	/*ֱʼ



	SCROLLINFO ScrInfo;



	GetScrollInfo(SB_VERT, &ScrInfo, SIF_ALL);



	ScrInfo.nPage = 10; //û?



	ScrInfo.nMax = 100; //ùλ?0C100



	SetScrollInfo(SB_VERT, &ScrInfo, SIF_ALL);




	*/














	SCROLLINFO si;



	si.cbSize = sizeof(SCROLLINFO);



	si.fMask = SIF_ALL;




	GetScrollInfo(SB_VERT, &si);









	// Modify the scroll range and page size values to hide the scrollbar




	si.nPos = 0;




	si.nMin = 0;




	si.nMax = 0;




	si.nPage = 1;




	// Set the modified scroll info to hide the scrollbar




	SetScrollInfo(SB_VERT, &si, TRUE);



















	m_ServerIPAddress.SetWindowText(m_strServerIP); 




	  m_ServerRegPort.SetWindowText(m_strServerPort);




	   m_ServerHBPort.SetWindowText(m_strServerPortHB);









	//1.ȡǰעܸ?




	m_iPreviousRegistered_ClientCount = CProfileConfig::GetProfileConfigInstance()->ReadTotalClientCount_FromIni();//ûУȡõ?-1




	if (m_iPreviousRegistered_ClientCount > 0)




	{




		




		g_nTotalRegistered_ClientCount = m_iPreviousRegistered_ClientCount;//lzq:Dlgʼʱ�?????�Ini-->g_nTotalRegistered_ClientCount




	




		//2.ȡStartIP




		m_strPreviousRegistered_StartIP = CProfileConfig::GetProfileConfigInstance()->ReadClientStartIp_FromIni();




		if (0 != m_strPreviousRegistered_StartIP.GetLength())




		{




			




			//m_cFirstClientIP_CtrBox.SetWindowText(csFirstClientIp_FromIni);




		}




		else




		{




			m_strPreviousRegistered_StartIP = g_strDefaultStartIP;









			CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);




		}




		//3.ȡIDPrefix




		m_strEditCtrl_ToRegisterClient_ClientIDPrefix = CProfileConfig::GetProfileConfigInstance()->ReadEachClientIdPrefix_FromIni();




		if(0 != m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())




		{




			




		}




		else




		{




			m_strEditCtrl_ToRegisterClient_ClientIDPrefix = g_strDefaultPrefix;









			CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);	




		}




		//4.ȡStartNum




		m_strPreviousRegistered_StartNum = CProfileConfig::GetProfileConfigInstance()->ReadClientStartNum_FromIni();




		if( 0 != m_strPreviousRegistered_StartNum.GetLength())




		{




			




		}




		else




		{




			m_strPreviousRegistered_StartNum = g_strDefaultStartNum;









			CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);




		}









		PrepareVecClients_UpdateControls();//lzq:Dlgʼʱ�?????�ǰעĸClient�?????��?? FileLog MsgLog(ǰǰзע)




	}




	else




	{




		m_iPreviousRegistered_ClientCount = 0;




		   g_nTotalRegistered_ClientCount = 0;









		CString strTmp;




		strTmp.Format(_T("已注册: %d"),g_nTotalRegistered_ClientCount);




		m_cRegisteredClientCounts.SetWindowText(strTmp);














		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);









		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);




 




 




		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(m_iPreviousRegistered_ClientCount);   //ûǰ?�?????ȡõ-1д0PreviousRegisteredClientCount=0




	}













	// === Phase 4: New UI initialization ===



	// ProjectType Combo



	m_comboProjectType.AddString(_T("IEG"));



	m_comboProjectType.AddString(_T("EDR"));



	m_comboProjectType.SetCurSel(0);







	ApplyProjectTypeSelection(); // Ĭ�� IEG Ԥѡ

	// Disable Visual Styles on buttons for custom coloring via OnCtlColor
	HMODULE hUxTheme = ::LoadLibrary(_T("uxtheme.dll"));
	if (hUxTheme)
	{
		typedef HRESULT (WINAPI *pfnSetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
		pfnSetWindowTheme fnSetWindowTheme = (pfnSetWindowTheme)::GetProcAddress(hUxTheme, "SetWindowTheme");
		if (fnSetWindowTheme)
		{
			UINT themeBtns[] = { IDC_BUTTON_REG_REG, IDC_BUTTON_REG_RESET,
				IDC_BUTTON_HB_START, IDC_BUTTON_HB_STOP,
				IDC_BUTTON_LOG_ADD, IDC_BUTTON_LOG_STOP,
				IDC_BUTTON_STOP_TASK, IDC_BUTTON_WL_UPLOAD, IDC_BUTTON_WL_STOP2, IDC_BUTTON_WL_PREVIEW };
			for (int i = 0; i < _countof(themeBtns); ++i)
			{
				fnSetWindowTheme(::GetDlgItem(m_hWnd, themeBtns[i]), L"", L"");
				GetDlgItem(themeBtns[i])->InvalidateRect(NULL, TRUE);
			}
		}
		::FreeLibrary(hUxTheme);
	}









	// OS Type: default Windows



	m_radioOsWin.SetCheck(BST_CHECKED);



	m_radioOsLinux.SetCheck(BST_UNCHECKED);







	// Load client version combo (Windows list by default)



	LoadClientVersionCombo();







	// HB Interval default



	m_editHbInterval.SetWindowText(_T("30000"));







	// Log send defaults



	m_editLogTotal.SetWindowText(_T("1000"));



	m_editHttpsCount.SetWindowText(_T("0"));



	m_editHttpsEps.SetWindowText(_T("0"));



	m_editTcpCount.SetWindowText(_T("5"));



	m_editTcpEps.SetWindowText(_T("1"));



	m_editTcpHit.SetWindowText(_T("71"));







	// Whitelist concurrency default



	m_editWlConcurrent.SetWindowText(_T("4"));







	// HB badge default



	m_staticHbBadge.SetWindowText(_T("TCP(Windows)"));







	// OsInfo text



	UpdateOsInfoText();







	// Linux HB thread control



	m_bLinuxHbRunning = FALSE;



	m_hLinuxHbThread = NULL;







	// Start 1-second stats refresh timer



	SetTimer(1, 1000, NULL);

	// UI Theming: color brushes + fonts
	m_hBrushDlg   = ::CreateSolidBrush(RGB(242, 245, 250));
	m_hBrushWhite = ::CreateSolidBrush(RGB(255, 255, 255));
	LOGFONT lf = {};
	lf.lfHeight    = -10;
	lf.lfWeight    = FW_BOLD;
	lf.lfCharSet   = GB2312_CHARSET;
	_tcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Microsoft YaHei UI"));
	m_fontBold.CreateFontIndirect(&lf);
	lf.lfWeight    = FW_NORMAL;
	m_fontNormal.CreateFontIndirect(&lf);
	for (CWnd* pCh = GetWindow(GW_CHILD); pCh; pCh = pCh->GetWindow(GW_HWNDNEXT))
	{
		TCHAR szCls[64] = {};
		::GetClassName(pCh->GetSafeHwnd(), szCls, 64);
		if (_tcsicmp(szCls, _T("Button")) == 0)
		{
			LONG st = ::GetWindowLong(pCh->GetSafeHwnd(), GWL_STYLE);
			if ((st & BS_TYPEMASK) == BS_GROUPBOX)
				pCh->SetFont(&m_fontBold, FALSE);
			else
				pCh->SetFont(&m_fontNormal, FALSE);
		}
		else
			pCh->SetFont(&m_fontNormal, FALSE);
	}

	return TRUE;  // ǽõؼ򷵻 TRUE




}









void CWLServerTestDlg::OnSysCommand(UINT nID, LPARAM lParam)




{




	if ((nID & 0xFFF0) == IDM_ABOUTBOX)




	{




		CAboutDlg dlgAbout;




		dlgAbout.DoModal();




	}




	else




	{




		CDialog::OnSysCommand(nID, lParam);




	}




}









//  ԻСťҪĴ?




//  Ƹͼꡣʹ�?/ͼģ͵ MFC Ӧó




//  �?????ɿԶɡ









void CWLServerTestDlg::OnPaint()




{




	if (IsIconic())




	{




		CPaintDC dc(this); // ڻƵ�?????









		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);









		// ʹͼڹо




		int cxIcon = GetSystemMetrics(SM_CXICON);




		int cyIcon = GetSystemMetrics(SM_CYICON);




		CRect rect;




		GetClientRect(&rect);




		int x = (rect.Width() - cxIcon + 1) / 2;




		int y = (rect.Height() - cyIcon + 1) / 2;









		// ͼ




		dc.DrawIcon(x, y, m_hIcon);




	}




	else




	{




		CDialog::OnPaint();




	}




}









//û϶Сʱϵͳô˺ȡùʾ?




HCURSOR CWLServerTestDlg::OnQueryDragIcon()




{




	return static_cast<HCURSOR>(m_hIcon);




}









BOOL CWLServerTestDlg::CreateGuidString(LPTSTR lpGuid)




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









	_stprintf(lpGuid, _T("%04d"),rand()%10000);














	return TRUE;




}









BOOL CWLServerTestDlg::CheckServer_IP_Port_HBPort_NotEmpty()




{




	UpdateData(TRUE);




	if (0 == m_strServerIP.GetLength() || 0 == m_strServerPort.GetLength()|| 0 == m_strServerPortHB.GetLength())




	{




		return FALSE;




	}









	return TRUE;




}









BOOL CWLServerTestDlg::RegisterClientToServer(CString szComputerID, CString szClientID,CString szClientIP)




{
	// v6: ��ȡ��ǰ UI ѡ�еİ汾�� OS
	CString strVer = _T("V300R011C01B090");
	int nSel = m_comboClientVersion.GetCurSel();
	if (nSel >= 0) m_comboClientVersion.GetLBText(nSel, strVer);
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	CString strOS = bLinux ? _T("Linux centos7") : _T("Windows 10");





	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	return SendInfoToServer.RegisterClientToServer(szComputerID,szClientID,szClientIP,strVer,strOS);




}









BOOL CWLServerTestDlg::SendHeartbeatToserver_TCP(client& pCurClient,SOCKET sock)




{




	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPortHB,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	SendInfoToServer.m_WindowsOSVersion = (m_radioOsLinux.GetCheck() == BST_CHECKED) ? _T("Linux centos7") : _T("Windows 10");



	return SendInfoToServer.SendHeartbeatToserverTCP(pCurClient,sock);




}









DWORD CWLServerTestDlg::RecvHeartBeatBack_TCP(SOCKET sockRecv)




{




    CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPortHB,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




    return SendInfoToServer.RecvHeartbeat(sockRecv);




}









BOOL CWLServerTestDlg::SendHeartbeat(client& curClient)




{




    CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
    SendInfoToServer.m_WindowsOSVersion = (m_radioOsLinux.GetCheck() == BST_CHECKED) ? _T("Linux centos7") : _T("Windows 10");



    return SendInfoToServer.SendHeartbeat(curClient);




}









//added by lzq




//added by lzq:MAY19 \/




BOOL CWLServerTestDlg::SendUsingHBPort_ThreatLog(client& pCurClient,SOCKET sock)




{




	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPortHB,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	return SendInfoToServer.SendThreatLog_ToserverTCP(pCurClient,sock);




}









DWORD CWLServerTestDlg::RecvHeartBeatBack_TCP_ThreatLog(SOCKET sockRecv) //lzq:wy




{




	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPortHB,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	return SendInfoToServer.RecvThreatLog_(sockRecv);




}









BOOL CWLServerTestDlg::SendHeartbeat_ThreatLog(client& curClient)//lzq:wy




{




	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	SendInfoToServer.m_WindowsOSVersion = (m_radioOsLinux.GetCheck() == BST_CHECKED) ? _T("Linux centos7") : _T("Windows 10");



	return SendInfoToServer.SendHeartbeat(curClient);




}









unsigned int ThreadFunc_HeartbeatSend_JustSend(PVOID JustSend) //ֻά




{




    ULONGLONG ulHBTotal_MilSeconds = 600*60*1000;	




    ULONGLONG ulTime_First = GetTickCount();//ʱȽϣСulHBTotalMilSec




    ULONGLONG ulTime_Second = 0; //added by lzq;




    ULONGLONG ulTime_During = 0; //added by lzq;




    PJUST_SEND_HEART pJustSend = (PJUST_SEND_HEART)JustSend;




    client *cclient = (client *)pJustSend->pClient;









    SOCKET sock = INVALID_SOCKET;




    CSendInfoToServer* pSendInfo = new CSendInfoToServer;









    pSendInfo->CreateConnection(sock,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);









    do 




    {  




        if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(*cclient,sock))




        {




            pSendInfo->CreateConnection(sock,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




            if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(*cclient,sock))




            {




                WriteError(_T("ST:HB SendHeartbeatToserver_TCP second time fails!"));




            }




            else




            {




                WriteInfo(_T("ST:HB SendHeartbeatToserver_TCP successes but at second time!"));




            }




        }




        else




        {




            WriteInfo(_T("ST:HB SendHeartbeatToserver_TCP successes at first time!"));




        }









        DWORD dwRet = g_WLServerTestDlg->RecvHeartBeatBack_TCP(sock);




        if (HEARTBEAT_CMD_BACK == dwRet)




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_BACK!"));




        }




        else if (HEARTBEAT_CMD_POLCY == dwRet)




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_POLCY!"));




            dwRet = g_WLServerTestDlg->SendHeartbeat(*cclient);//ʱʹhttps,Ѿʹóӡȥԡ?




            if (ERROR_SUCCESS != dwRet)




            {




                WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!"));	             




            }




        }




        else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//δע




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_NOREGISTER!"));




            pSendInfo->CloseConnection(sock);




            if (g_WLServerTestDlg->RegisterClientToServer(cclient->Client_GetComputerID(),




                cclient->GetClientID(),




                cclient->GetClientIP()))




            {




                cclient->Client_SetRegistered(TRUE);




                pSendInfo->CreateConnection(sock,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




                g_WLServerTestDlg->SendHeartbeatToserver_TCP(*cclient,sock);




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Succ!"));




            }




            else




            {




                cclient->Client_SetRegistered(FALSE);




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Fail!"));




            }




        }




        else




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns neither!"));




        }









        if (g_bStopTask) break; // v9: ��Ӧֹͣ�ź�
        if (g_bStopTask) break; // v9: ��Ӧֹͣ�ź�
        { for (int _si=0;_si<300&&!g_bStopTask;_si++) Sleep(100); } //v13: cancellable 30s









        ulTime_Second = GetTickCount();




        ulTime_During = ulTime_Second - ulTime_First;









        if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:ʱѳ




        {




            WriteInfo(_T("ST:HB Actual during time:%lld;Interval time %lld"),ulTime_During,ulHBTotal_MilSeconds);




            break;




        }




        




        if (pJustSend->bExit)




        {




            break;




        }









    } while ( 1 );









    pSendInfo->CloseConnection(sock);




    delete pSendInfo;




    pSendInfo=NULL;









    _endthreadex( 0 );









    return 0;




}









unsigned int ThreadFunc_Register_SameTime(PVOID pIndex)




{




    if (!pIndex)




    {




        return 0;




    }




    




    client *pClient = (client *)pIndex;




    




    CString strThreadCount_RegSameTime;




    g_singleLockGManage.Lock();




    g_iThreadCount_RegSameTime ++;




    strThreadCount_RegSameTime.Format(_T("%d"),g_iThreadCount_RegSameTime);




    g_WLServerTestDlg->m_RegisterThreadCount.SetWindowText(strThreadCount_RegSameTime);




    g_singleLockGManage.Unlock();




    




    CString strRegFailCD;




    int iRegFailCD = 0;




    g_WLServerTestDlg->m_EditRegFailCD.GetWindowText(strRegFailCD);




    if (strRegFailCD.GetLength() > 0)




    {




        iRegFailCD = _ttoi(strRegFailCD.GetBuffer());




    }




    




    if (iRegFailCD <= 0)




    {




        iRegFailCD = 30;




    }




    




    while(TRUE)




    {




        if (!g_WLServerTestDlg->m_bRegisterSameTime_Now)




        {




            Sleep(200);




            continue;




        }




        




        WriteInfo(_T("RegisterClientToServer Begin, ClientID = %s"), pClient->GetClientID().GetBuffer());




        if (g_WLServerTestDlg->RegisterClientToServer(pClient->Client_GetComputerID(),pClient->GetClientID(),pClient->GetClientIP()))




        {




            WriteInfo(_T("RegisterClientToServer Success, ClientID = %s"), pClient->GetClientID().GetBuffer());




            




            ::InterlockedIncrement((LONG*)&g_nTotalRegistered_ClientCount); // v5.3: 并发原子递增，修复多线程注册时计数丢更新









            g_nHeartBeatNotSending_ClientCount++;









            g_nMsgLogNotSending_ClientCount++;




            g_nFileLogNotSending_ClientCount++;









            pClient->Client_SetRegistered(TRUE);




            




            CString strTmpRegisteredClientCount;




            




            g_singleLockGManage.Lock();




            




            strTmpRegisteredClientCount.Format(_T("已注册: %d"), g_nTotalRegistered_ClientCount);




            g_WLServerTestDlg->m_cRegisteredClientCounts.SetWindowText(strTmpRegisteredClientCount);




            g_iThreadCount_RegSameTime --;




            strThreadCount_RegSameTime.Format(_T("%d"),g_iThreadCount_RegSameTime);




            g_WLServerTestDlg->m_RegisterThreadCount.SetWindowText(strThreadCount_RegSameTime);




            




            g_singleLockGManage.Unlock();




            




            break;




        }




        




        WriteInfo(_T("RegisterClientToServer Failed, ClientID = %s"), pClient->GetClientID().GetBuffer());




        




        Sleep(iRegFailCD * 1000);




    }




    




    if (((CButton *)g_WLServerTestDlg->GetDlgItem(IDC_WHITE_LIST))->GetCheck())




    {




        //ȴ߳ά




        PJUST_SEND_HEART pJustSend = (PJUST_SEND_HEART)malloc(sizeof(JUST_SEND_HEART));




        pJustSend->pClient = pIndex;




        pJustSend->bExit = FALSE;




        AfxBeginThread((AFX_THREADPROC)ThreadFunc_HeartbeatSend_JustSend, pJustSend, THREAD_PRIORITY_NORMAL, 0, 0, NULL); 




        




        CString strWhiteListFilePath = _T("");




        g_WLServerTestDlg->m_WLFilePathEdit.GetWindowText(strWhiteListFilePath);









        CSendInfoToServer SendInfoToServer(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPort,g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);









        BOOL bRet = SendInfoToServer.Send_FileLog_WL_ToServer(pClient->Client_GetComputerID().GetBuffer(), strWhiteListFilePath);




        if (bRet)




        {




            CString strWL_Success_Count = _T("");




            




            g_singleLockApplogCount.Lock();




            




            g_WLServerTestDlg->m_ulSametimeRegister_UploadWLCount++;




            strWL_Success_Count.Format(_T("%d"), g_WLServerTestDlg->m_ulSametimeRegister_UploadWLCount);




            g_WLServerTestDlg->m_StaticUploadWLCount.SetWindowText(strWL_Success_Count);




            




            g_singleLockApplogCount.Unlock();




        }




        




        pJustSend->bExit = TRUE;




    }









    _endthreadex( 0 );









    return 0;




}














//added by lzq:/\




 




void CWLServerTestDlg::OnBnClicked_RegisterClients() //lzq:?ᰴ�?




{









	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("请输入客户端信息"));




		return;




	}




	if (0 == m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())




	{




		AfxMessageBox(_T("请输入客户端ID前缀"));




		return;




	}









	CString strRegClientCount;




	m_comRegButton_ToRegisterClient_ClientCount.GetWindowText(strRegClientCount);









	int iNeedToRegClientCount = _ttoi(strRegClientCount);




	if (iNeedToRegClientCount <= 0)




	{




		AfxMessageBox(_T("注册结束无效"));




		return;




	}









	CString strFirstClientIPToRegister;




	m_ctrlBox_ToRegisterClient_FirstClientIP.GetWindowText(strFirstClientIPToRegister);




	if (0 == strFirstClientIPToRegister.GetLength())




	{




		AfxMessageBox(_T("请输入起始IP地址"));




		return;




	}









	USES_CONVERSION;




	char* pcFirstClientIPToRegister = T2A(strFirstClientIPToRegister); 




	int IPAddrClips_FourInt[4] = {0};




	sscanf(pcFirstClientIPToRegister, "%d.%d.%d.%d", &IPAddrClips_FourInt[0],&IPAddrClips_FourInt[1],&IPAddrClips_FourInt[2],&IPAddrClips_FourInt[3]);









	if (0 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))




	{




		AfxMessageBox(_T("起始IP地址格式不正确"));




		return;




	}




	else if(2 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))




	{




		AfxMessageBox(_T("注册结束，IP地址超出范围"));




		return;




	}




	




	BOOL bRegisterSameTime = m_bRegisterSameTime.GetCheck();




	




	if (bRegisterSameTime)




	{




        if (((CButton *)GetDlgItem(IDC_WHITE_LIST))->GetCheck())




        {




            CString strWhiteListFilePath = _T("");




            m_WLFilePathEdit.GetWindowText(strWhiteListFilePath);









            if(!PathFileExists(strWhiteListFilePath))




            {




                AfxMessageBox(_T("请选择文件路径"));




                return;




            }




        }




        else




        {




        }




	}









					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);




			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);




			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(FALSE);




	




	//עᰴ�? --> ? client




	int iClientStartNum= _ttoi(m_strEditCtrl_ToRegisterClient_StartNum.GetBuffer());//DDE




	int i = 0;




	int EachClient_UniqueIP[4] = {0};









	CString szUniqueClientID = m_strEditCtrl_ToRegisterClient_ClientIDPrefix;




	CString sThisClientIndex=_T("");




	CString szUniqueClientIP=_T("");




    




    if (bRegisterSameTime)




    {




        ((CStatic *)GetDlgItem(IDC_STATIC_RegisteredThreadCount))->ShowWindow(SW_SHOW);




        m_RegisterThreadCount.ShowWindow(SW_SHOW);




        g_vecAllClientObjects.clear();




        g_vecAllClientObjects.reserve(iNeedToRegClientCount + 1);




        ((CStatic *)GetDlgItem(IDC_STATIC_UPLOADWLCount))->ShowWindow(SW_SHOW);




        ((CStatic *)GetDlgItem(IDC_UPLOADWLCOUNT))->ShowWindow(SW_SHOW);




    }




    else




    {  




        ((CStatic *)GetDlgItem(IDC_STATIC_RegisteredThreadCount))->ShowWindow(SW_HIDE);




        m_RegisterThreadCount.ShowWindow(SW_HIDE);




        ((CStatic *)GetDlgItem(IDC_STATIC_UPLOADWLCount))->ShowWindow(SW_HIDE);




        ((CStatic *)GetDlgItem(IDC_UPLOADWLCOUNT))->ShowWindow(SW_HIDE);




    }




    




	for (i=0; i<iNeedToRegClientCount; i++)




	{




		sThisClientIndex=_T("");




		sThisClientIndex.Format(_T("%04d"),iClientStartNum + i);




		szUniqueClientID = m_strEditCtrl_ToRegisterClient_ClientIDPrefix;




		szUniqueClientID.Append(sThisClientIndex);




		




		EachClient_UniqueIP[3] = (IPAddrClips_FourInt[3] + i) % 255;




		EachClient_UniqueIP[2] = (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) % 255;




		EachClient_UniqueIP[1] = (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) % 255;




		EachClient_UniqueIP[0] = IPAddrClips_FourInt[0]  + (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) / 255;




		szUniqueClientIP=_T("");




		szUniqueClientIP.Format(_T("%d.%d.%d.%d"),EachClient_UniqueIP[0],EachClient_UniqueIP[1],EachClient_UniqueIP[2],EachClient_UniqueIP[3]);




							




		client clientTemp(szUniqueClientID,szUniqueClientIP);




		clientTemp.Client_SetComputerID();




		




		if (bRegisterSameTime)




		{




		    g_vecAllClientObjects.push_back(clientTemp);




		    client& clt = g_vecAllClientObjects.back();









            AfxBeginThread((AFX_THREADPROC)ThreadFunc_Register_SameTime, (PVOID)&clt, THREAD_PRIORITY_NORMAL, 0, 0, NULL);




            Sleep(20);




		}




		else




		{




            if (RegisterClientToServer(clientTemp.Client_GetComputerID(),szUniqueClientID,szUniqueClientIP))




            {




                ::InterlockedIncrement((LONG*)&g_nTotalRegistered_ClientCount); // v5.3: 并发原子递增









                g_nHeartBeatNotSending_ClientCount++;









                g_nMsgLogNotSending_ClientCount++;




                g_nFileLogNotSending_ClientCount++;









                clientTemp.Client_SetRegistered(TRUE);




                g_vecAllClientObjects.push_back(clientTemp);




            }




            else




            {




                AfxMessageBox(_T("注册失败"));




                GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




                GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




                GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);




                GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);









                return;




            }




            




            CString strTmpRegisteredClientCount;




            strTmpRegisteredClientCount.Format(_T("已注册: %d"), g_nTotalRegistered_ClientCount);




            m_cRegisteredClientCounts.SetWindowText(strTmpRegisteredClientCount);




		}









		{




			// Էʱȴһ´ں?




			MSG msg;




			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))




			{




				TranslateMessage(&msg);




				DispatchMessage(&msg);  




			}




		}




	}




	




    if (bRegisterSameTime)




    {




        m_bRegisterSameTime_Now = TRUE;




    }









	{//¿ؼ  ?ᣨǣǰʵֻעһΣť�??









		//1.µʼ?




		CString cstrNextRegistration_StartNum;




		cstrNextRegistration_StartNum.Format(_T("%d"),iClientStartNum + iNeedToRegClientCount);//200+10




		//GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(cstrNextRegistration_StartNum); // v5: ע��󲻸结束�ʼ���









		//2.µIP




		EachClient_UniqueIP[3] = (IPAddrClips_FourInt[3] + i) % 255;




		EachClient_UniqueIP[2] = (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) % 255;




		EachClient_UniqueIP[1] = (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) % 255;




		EachClient_UniqueIP[0] = IPAddrClips_FourInt[0]  + (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) / 255;




		CString cstrNextRegistration_FirstClientIP = _T("");




		cstrNextRegistration_FirstClientIP.Format(_T("%d.%d.%d.%d"),EachClient_UniqueIP[0],EachClient_UniqueIP[1],EachClient_UniqueIP[2],EachClient_UniqueIP[3]);




		




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(cstrNextRegistration_FirstClientIP);









	}




	




	{//˴עϢ¼Ini









		// ÿ ?  ---"ָĿclient"ļм¼ֵעֻǡעInfo




		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(iNeedToRegClientCount);//10









		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);//WLClient_




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(strFirstClientIPToRegister);//6.6.6.6




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(m_strEditCtrl_ToRegisterClient_StartNum);//200  









		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);




		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);




		CProfileConfig::GetProfileConfigInstance()->WriteServerHBPort_ToIni(m_strServerPortHB);









	}




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);  //ţǷѭע




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);




			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);




}









void CWLServerTestDlg::OnBnClicked_ClientReg_Reset()




{




	if (IDOK == AfxMessageBox(_T("确定清空当前所有客户端的日志记录？\n文件日志/消息日志将停止"), MB_OKCANCEL))




	{




		g_nTotalRegistered_ClientCount = 0;




		g_nHeartBeatNotSending_ClientCount = 0;




		g_nMsgLogNotSending_ClientCount = 0;




		g_nFileLogNotSending_ClientCount = 0;     









		CString strTmp;




		strTmp.Format(_T("已注册: %d"),g_nTotalRegistered_ClientCount);




		m_cRegisteredClientCounts.SetWindowText(strTmp);




 









		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(g_nTotalRegistered_ClientCount);




		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);









		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);




		




		//ãȫգ?
		g_bStopTask = TRUE;
		{ // v16: wait for all HB threads to finish before clearing vector
			DWORD _t0 = GetTickCount();
			while (g_nActiveHbThreads > 0 && (GetTickCount() - _t0) < 10000) {
				MSG _msg;
				while (PeekMessage(&_msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&_msg); DispatchMessage(&_msg); }
				Sleep(100);
			}
		}
		g_vecAllClientObjects.clear();




	}




}  









//added by lzq:MAY19




unsigned int UseHBPort_ThreatLog_CheckBox(int iCurUsedClient)//lzq:һ߳ÿηһ,?




{




	client  sObj = g_vecAllClientObjects[iCurUsedClient];




	CString strCount;




 




	SOCKET sockTemp;




	CSendInfoToServer* sSendInfo = {0};









	sSendInfo = new CSendInfoToServer();




	sSendInfo->CreateConnection(sockTemp,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);









	if(!g_WLServerTestDlg->SendUsingHBPort_ThreatLog(sObj,sockTemp))




	{




		sSendInfo->CreateConnection(sockTemp,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




		g_WLServerTestDlg->SendUsingHBPort_ThreatLog(sObj, sockTemp);




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




}  









const DWORD HBClientCount_InEveryThread = 10;  // һִ߳10




unsigned int ThreadFunc_HeartbeatSend_Ori(void* pArgument) //lzq:ÿ̵߳ڣģ?10Client




{




	int iThisTaskLineIndex = *(int*)pArgument;//iIndex �?????-->Task




	delete pArgument;









	




	CString csHBIntervalMilSec = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 3); //lzq:ms




	 CString strHBTotalMinutes = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 4); //lzq:ܷ









	int iHBIntervalMilSec = _ttoi(csHBIntervalMilSec); //lzq:δã?




	int iHBTotalMinutes = _ttoi(strHBTotalMinutes);









	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(iHBTotalMinutes)*60*1000;	




	ULONGLONG ulTime_First = GetTickCount();//ʱȽϣСulHBTotalMilSec




	ULONGLONG ulTime_Second = 0; //added by lzq;









	//std::map; pair  map ;;//key=int, value=client




	




    client ThisHBTask_ToUseClientsArray[HBClientCount_InEveryThread];









	g_singleLockGManage.Lock(); //




	if((g_nTotalRegistered_ClientCount==-1)||(g_nHeartBeatNotSending_ClientCount==-1))




	{




		g_singleLockGManage.Unlock();









		return -1;




	}  









	DWORD dwThisTask_ClientIndex = 0;//(0 - 999)









	std::vector<int> vecIndex;




	for (int i=0; i<g_nTotalRegistered_ClientCount; i++)                             //עĿͻ?,ҳδĿͻ




	{




		if (!(g_vecAllClientObjects[i].Get_IsThisClientSendingHeartBeat()))  //ΨһõThisClient_HasSentHBжǷѷ




		{




			ThisHBTask_ToUseClientsArray[dwThisTask_ClientIndex] = g_vecAllClientObjects[i];




			vecIndex.push_back(i);









			g_vecAllClientObjects[i].Set_IsSendingHeartBeat(TRUE);//Client¼Ѿ




			




			g_nHeartBeatNotSending_ClientCount--;









			dwThisTask_ClientIndex++;









			if (dwThisTask_ClientIndex >= HBClientCount_InEveryThread)//Ϊ˴ ѡClientdwThisTask_ClientIndex == 10break




			{




				break;




			}




		}




	}//Ϊ⣺п�?10









	CString strOnlineClientCount;




	strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




	g_singleLockGManage.Unlock();//




	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//ʾѷ͹Client;;߿ͻ10



















					 SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};




	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};




	




	//Client




	for(int i=0; i<dwThisTask_ClientIndex; i++)//lzq:ǰ߳ ͻ




	{




		sSendInfo[i] = new CSendInfoToServer();




		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);//ʱsock <------>IP:HBPort




	}




	














	do 




	{




		for(int i=0; i<dwThisTask_ClientIndex; i++)//dwThisTask_ClientIndex==10 ʱ˳ˣ




		{




			if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(ThisHBTask_ToUseClientsArray[i],sock[i]))//˴ѡÿclient  --  TmpSock




			{// ʧܣһ




				




				sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




				g_WLServerTestDlg->SendHeartbeatToserver_TCP(ThisHBTask_ToUseClientsArray[i], sock[i]);




			}









	        DWORD dwRet = g_WLServerTestDlg->RecvHeartBeatBack_TCP(sock[i]);




	        if (HEARTBEAT_CMD_BACK == dwRet)




	        {




	            //todo




	        }




	        else if (HEARTBEAT_CMD_POLCY == dwRet)




	        {




	            dwRet = g_WLServerTestDlg->SendHeartbeat(ThisHBTask_ToUseClientsArray[i]);




	            if (ERROR_SUCCESS != dwRet)




	            {




	                WriteError(_T("CWLHeartBeat::instance()->sendHeartBeat() get policy change failed"));




	            }




	        }




	        else




	        {




	            //todo




	        }




		}




		




        Sleep(30 * 1000);//˯30









		    ulTime_Second = GetTickCount();




		if (ulTime_Second - ulTime_First > ulHBTotal_MilSeconds)//lzq:ʱѳ




		{




			break;




		}









	} while ( 1 );














	//Clientر




	for(int i=0; i<dwThisTask_ClientIndex; i++)




	{




		sSendInfo[i]->CloseConnection(sock[i]);




		delete sSendInfo[i];




		sSendInfo[i]=NULL;




	}









	//  ԭʼTotalĳԱ?




	




	int iNum = vecIndex.size();




	int k=0;




	g_singleLockGManage.Lock();




	




	for (k = 0;k < iNum; k++)




	{




		int iIdnex = vecIndex[k];









		g_vecAllClientObjects[iIdnex].Set_IsSendingHeartBeat(FALSE);









		g_nHeartBeatNotSending_ClientCount ++;// ԭ--




	}




	/*for(int i=0; i<dwThisTask_ClientIndex; i++)




	{




		g_vecAllClientObjects[i].Set_HasHeartBeatSent(FALSE);




	}




	




	g_nHeatBeatNotSent_ClientCount--;*/




	




	strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);




	g_singleLockGManage.Unlock();









	CSingleLock singleLockListCtrl(&g_WLServerTestDlg->m_csHeatbeatListCtrl);	









	//ȡý "Ծͻ˼"    -1   ʾȥ




	singleLockListCtrl.Lock();




	CString strActiveClientCountCol = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 5);




	int iActiveClient = _ttoi(strActiveClientCountCol);




	




	//iActiveClient--;//yb




	iActiveClient -= dwThisTask_ClientIndex;









	strActiveClientCountCol.Format(_T("%d"), iActiveClient);




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(iThisTaskLineIndex, 5, strActiveClientCountCol);




	singleLockListCtrl.Unlock();









	if (iActiveClient <= 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(iThisTaskLineIndex, 6, _T("已停止"));
	}









	_endthreadex( 0 );









	return 0;




}









std::set<int>  g_setOfflineClient;




unsigned int ThreadFunc_HeartbeatSend_New(PHB_SENDER_THREAD_ARG pHeapArgs) //lzq:ÿ̵߳ڣģ?10Client




{




	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(pHeapArgs->iHBTotalMinutes)*60*1000;	




	ULONGLONG ulTime_First = GetTickCount();//ʱȽϣСulHBTotalMilSec




	ULONGLONG ulTime_Second = 0; //added by lzq;




	ULONGLONG ulTime_During = 0; //added by lzq;









	SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};




	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};









	int iThisThreadClientCount = pHeapArgs->iClientCount;




	




	//Client




	for(int i=0; i < iThisThreadClientCount; i++)//lzq:ǰ߳ ͻ




	{




		sSendInfo[i] = new CSendInfoToServer();




		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




		g_sock[g_nSocketCount++] = sock[i];




		WriteInfo(_T("ST:HB CreateConnection success!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




	}




	




	do 




	{  
		if (g_bStopTask) break; // v16: exit main HB loop on stop signal




		for(int i=0; i < iThisThreadClientCount; i++)//dwThisTask_ClientIndex==10 ʱ˳ˣ




		{




		    if (FALSE == g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_IsRegistered())




		    {




                if (g_WLServerTestDlg->RegisterClientToServer(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_GetComputerID(),




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(),




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientIP()))




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(TRUE);




                    WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Succ!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                }




                else




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(FALSE);




                    WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Fail!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                    continue;




                }




		    }




		    




			if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]],sock[i]))




			{




				WriteWarn(_T("ST:HB SendHeartbeatToserver_TCP first time fails!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);









				sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




	 			if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]],sock[i]))




				{




					WriteError(_T("ST:HB SendHeartbeatToserver_TCP second time fails!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




				}




				else




				{




					WriteInfo(_T("ST:HB SendHeartbeatToserver_TCP successes but at second time!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




				}




			}




			else




			{




				 WriteInfo(_T("ST:HB SendHeartbeatToserver_TCP successes at first time!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




			}









	        DWORD dwRet = g_WLServerTestDlg->RecvHeartBeatBack_TCP(sock[i]);




	        if (HEARTBEAT_CMD_BACK == dwRet)




	        {




	             WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_BACK!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




	        }




	        else if (HEARTBEAT_CMD_POLCY == dwRet)




	        {




				WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_POLCY!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




	            dwRet = g_WLServerTestDlg->SendHeartbeat(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]]);//ʱʹhttps,Ѿʹóӡȥԡ?




	            if (ERROR_SUCCESS != dwRet)




	            {




					WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);	             




	            }




	        }




            else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//δע




            {




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_NOREGISTER!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                sSendInfo[i]->CloseConnection(sock[i]);




                if (g_WLServerTestDlg->RegisterClientToServer(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_GetComputerID(),




                                        g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(),




                                        g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientIP()))




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(TRUE);




                    sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




                    g_WLServerTestDlg->SendHeartbeatToserver_TCP(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]],sock[i]);




                    WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Succ!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                }




                else




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(FALSE);




                    WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Fail!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                }




            }




	        else




	        {




				WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns neither!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




	        }




		}




		




        { for (int _si=0;_si<300&&!g_bStopTask;_si++) Sleep(100); } //v13: cancellable 30s









		    ulTime_Second = GetTickCount();




			ulTime_During = ulTime_Second - ulTime_First;









		if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:ʱѳ




		{




			WriteInfo(_T("ST:HB Actual during time:%lld;Interval time %lld"),ulTime_During,ulHBTotal_MilSeconds);




			break;




		}









	} while ( 1 );









	//Clientر




	for(int i=0; i < iThisThreadClientCount; i++)




	{




		sSendInfo[i]->CloseConnection(sock[i]);




		delete sSendInfo[i];




		sSendInfo[i]=NULL;









		




		//  ԭʼTotalĳԱ?




		g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Set_IsSendingHeartBeat(FALSE);




		InterlockedIncrement(&g_nHeartBeatNotSending_ClientCount);




	}
	InterlockedDecrement(&g_nActiveHbThreads); // v16: all vector accesses done









	//¾̬ؼ




	CString strOnlineClientCount;




	g_singleLockGManage.Lock();




	//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




	g_nHeartBeatSending_ClientCount -= iThisThreadClientCount;




	strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);




	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);




	g_singleLockGManage.Unlock();














	//Listؼ




	CSingleLock singleLockListCtrl(&g_WLServerTestDlg->m_csHeatbeatListCtrl);	




	singleLockListCtrl.Lock();




	CString strHBActiveClientCount = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);




	int iHBActiveClientCount = _ttoi(strHBActiveClientCount);




	




	iHBActiveClientCount -= iThisThreadClientCount;









	strHBActiveClientCount.Format(_T("%d"), iHBActiveClientCount);




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strHBActiveClientCount);
	
	if (iHBActiveClientCount > 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("执行中"));
	}

	singleLockListCtrl.Unlock();




	if (iHBActiveClientCount <= 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("已停止"));
		SYSTEMTIME tm;
		GetLocalTime(&tm);
		TCHAR szTimeNow[MAX_PATH] = {0};
		_sntprintf_s(szTimeNow, MAX_PATH, _TRUNCATE,_T("%04d-%02d-%02d %02d:%02d:%02d"),tm.wYear, tm.wMonth, tm.wDay,tm.wHour, tm.wMinute, tm.wSecond);
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 7, szTimeNow);
	}














	if (pHeapArgs)




	{




		delete pHeapArgs;




		pHeapArgs = NULL;




	}









	_endthreadex( 0 );









	return 0;




}














// Ϳͻļ־߳




unsigned int ThreadFunc_FileLogSend(PLOG_SENDER_THREAD_ARG pHeapArgs)




{




	TCHAR szThisThreadSelected_ClientComputerID[MAX_PATH] = {0};









	int    iThisThreadSelected_ClientIndex = pHeapArgs->iThisClient_VectorIndex;









	_tcscpy(szThisThreadSelected_ClientComputerID, g_vecAllClientObjects[iThisThreadSelected_ClientIndex].Client_GetComputerID());



















	BOOL bRet = FALSE;




	CSendInfoToServer SendInfoToServer(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPort,g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);














	bRet = SendInfoToServer.Send_FileLog_WL_ToServer(szThisThreadSelected_ClientComputerID, pHeapArgs->csWhiteListFilePath);




	if (bRet)




	{




		InterlockedIncrement64(&g_WLServerTestDlg->m_lFileLog_WL_SuccessCount); 




	




	    CString strWL_Success_Count;




	    




	    strWL_Success_Count.Format(_T("%d"), g_WLServerTestDlg->m_lFileLog_WL_SuccessCount);




	   




		g_singleLockApplogCount.Lock();




	    g_WLServerTestDlg->m_WL_SuccessNum_Left.SetWindowText(strWL_Success_Count);




		g_singleLockApplogCount.Unlock();//ΧһС









        // USMϸΪ滻�?�?????�ֻϴɹһ�?




	}	




	// reset









	




	InterlockedIncrement(&g_nFileLogNotSending_ClientCount);









	// õǰͻ˻Ծ״?




	g_singleLockListCtrl.Lock();




	CString strActiveClient = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 6);




	int iActiveClient = _ttoi(strActiveClient);




	iActiveClient--;




	strActiveClient.Format(_T("%d"), iActiveClient);




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, strActiveClient);




	CString strMsgLogActNum = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);




	g_singleLockListCtrl.Unlock();














	int iMsgLogActNum = _ttoi(strMsgLogActNum);




	if ( iActiveClient==0 && iMsgLogActNum==0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, _T(""));
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("已停止"));
	}









	delete pHeapArgs;









	_endthreadex( 0 );









	return 0;




}









unsigned int ThreadFunc_MsgLogSend(PLOG_SENDER_THREAD_ARG pHeapArgs)   //һ߳? ҪͷŲ




{	




	TCHAR szThisThread_Selected_ComputerID[MAX_PATH] = {0};




	int nSendCount = 0;









	//ȷÿη͵Ŀͻ˶һ?




	//ULONGLONG seed = GetCurrentThreadId(); // ȡ߳ID




	//srand(seed);




	//pHeapArgs->iThisClient_VectorIndex = rand() % g_nTotalRegistered_ClientCount;




	int iThisThread_Selected_ClientIndex = pHeapArgs->iThisClient_VectorIndex;




	WriteInfo(_T("iThisClient_VectorIndex = %d"),pHeapArgs->iThisClient_VectorIndex);









	_tcscpy(szThisThread_Selected_ComputerID, g_vecAllClientObjects[iThisThread_Selected_ClientIndex].Client_GetComputerID());




	




	int iThisClient_CurrentSuccessCount = 0; //ɹ־Ĵ	









	




	CSendInfoToServer SendInfoToServer_LogPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPort,  g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	CString strThisClient_IP = g_vecAllClientObjects[iThisThread_Selected_ClientIndex].GetClientIP();
	SendInfoToServer_LogPort.SetClientIP(strThisClient_IP);




	









	client& objForHBThreatLog = g_vecAllClientObjects[iThisThread_Selected_ClientIndex];




	CSendInfoToServer  SendInfoToServer_HBPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB,g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	SendInfoToServer_HBPort.m_WindowsOSVersion = (g_WLServerTestDlg->m_radioOsLinux.GetCheck() == BST_CHECKED) ? _T("Linux centos7") : _T("Windows 10");
	SendInfoToServer_HBPort.SetClientIP(strThisClient_IP);



	//SOCKET stTmpSock;




	//SendInfoToServer_HBPort.CreateConnection(stTmpSock,g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB);














	do //̷߳һ Sleepһ




	{




		if (g_bStopTask || g_bStopLogTask)




		{




			g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(_T("0"));




			  g_WLServerTestDlg->m_StateUKey_BLine_SuccessNum_Left.SetWindowText(_T("0"));









			break;




		}




		//Ϳͻ˲־




		{




			BOOL bRet = FALSE; 









			//opt threat 




			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_OPT)




			{ 




				bRet = SendInfoToServer_LogPort.SendClientOptLogToServer(szThisThread_Selected_ComputerID);




			}




			




			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NWL)




			{ 




				 SendInfoToServer_LogPort.SendClientNwlLogToServer_FiveType(szThisThread_Selected_ComputerID);




			}




		




			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_THREAT)




			{//added by lzq




				//bRet = SendInfoToServer.SendClientThtLogToServer(szComputerID);




				//UseHBPort_ThreatLog_CheckBox(iThisThread_Selected_ClientIndex);//lzq:һ̵߳ÿ һν









				if (pHeapArgs->sock != INVALID_SOCKET)




				{




					// 70һ  




					BOOL bHit = FALSE;




					




					if (70 <= nSendCount++)




					{




						bHit = TRUE;




						nSendCount = 0;




					}









					SendInfoToServer_HBPort.SendThreatLog_ToserverTCP(objForHBThreatLog,pHeapArgs->sock, bHit);




				}	




			}









			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_DATAPROTECT)




			{




				SendInfoToServer_LogPort.SendClientDataProtectLogToServer(szThisThread_Selected_ComputerID);




			}









			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_SYSPROTECT)




			{




				SendInfoToServer_LogPort.SendClientSysProtectLogToServer(szThisThread_Selected_ComputerID);




			}









			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BACKUP)




			{




				SendInfoToServer_LogPort.SendClientBackupLogToServer(szThisThread_Selected_ComputerID);




			}









			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_Virus)
			{
				SendInfoToServer_LogPort.SendClientVirusLogToServer(szThisThread_Selected_ComputerID);
			}

			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NETADAPTER)
			{
				BOOL bNetRet = SendInfoToServer_LogPort.SendClientNetAdapterLogToServer(szThisThread_Selected_ComputerID);
				if (!bNetRet) g_WLServerTestDlg->AppendLogOutput(_T("[NET] Send FAILED"));
			}

			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_EXTDEV)
			{
				BOOL bExtRet = SendInfoToServer_LogPort.SendClientExtDevLogToServer(szThisThread_Selected_ComputerID, pHeapArgs->dwExtDevSubTypeMask);
				if (!bExtRet) g_WLServerTestDlg->AppendLogOutput(_T("[EXT] Send FAILED"));
			}

			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_UDISKPLUG)
			{
				BOOL bUdiskRet = SendInfoToServer_LogPort.SendClientUDiskPlugLogToServer(szThisThread_Selected_ComputerID);
				if (!bUdiskRet) g_WLServerTestDlg->AppendLogOutput(_T("[UDISK] Send FAILED"));
			}









			if ((pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_OPT)||




				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_THREAT) || 




				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NWL) ||




				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_DATAPROTECT) ||




				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_SYSPROTECT) ||




				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BACKUP) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_Virus) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NETADAPTER) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_EXTDEV))




			{




				InterlockedIncrement64(&(g_WLServerTestDlg->m_lMsgLogSuccessCount));//ͳɹŵһ









				CString strTmpMsgLogSuccessCount;




				strTmpMsgLogSuccessCount.Format(_T("%lld"), g_WLServerTestDlg->m_lMsgLogSuccessCount);




				g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(strTmpMsgLogSuccessCount);









			}









			//baseline  ukey 




			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BLINE)




			{




				bRet = SendInfoToServer_LogPort.SendBaseLineToServer(szThisThread_Selected_ComputerID);




			}




			if (g_bStopTask || g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_UKEY)




			{




				//UkeyϢ




				VEC_ST_USERS_USM    vecUSMUsersSend;    /* ϱȫûб */




				ST_USER_JSON oneUser;




				time_t t = time(NULL); //ȡʱ




				strcpy(oneUser.stUser.szUserName, "TestData");




				strcpy(oneUser.stUser.szGroup, "TestData");




				oneUser.stUser.iEnableStatus = 0;  




				oneUser.stUser.iUSMCreate = 0;  




				strcpy(oneUser.szDomain, "TestData");




				strcpy(oneUser.stBind.szHID, "TestData");




				strcpy(oneUser.stBind.szUkeyName, "TestData");   




				oneUser.stBind.tBindTime = t;




				vecUSMUsersSend.push_back(oneUser);









				oneUser.stUser.iUSMCreate = !(oneUser.stUser.iUSMCreate);




				bRet = SendInfoToServer_LogPort.SendUSBKeyManageToServer(szThisThread_Selected_ComputerID, vecUSMUsersSend);




			}




			if ((pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BLINE)||(pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_UKEY))




			{




				InterlockedIncrement64(&(g_WLServerTestDlg->m_lUploadState_SuccessCount));//ͳɹŵһ




				CString strUploadState_SuccessCount;




				strUploadState_SuccessCount.Format(_T("%lld"), g_WLServerTestDlg->m_lUploadState_SuccessCount);




				g_WLServerTestDlg->m_StateUKey_BLine_SuccessNum_Left.SetWindowText(strUploadState_SuccessCount);




			}









			iThisClient_CurrentSuccessCount++;









		}  




		




		if ((iThisClient_CurrentSuccessCount == pHeapArgs->iMsgLog_EachClientTotalCount) ) //modified by lzq:>=  == 




		{




			break;




		}









		{ // r6: chunked sleep for fast stop response
			int _slpR6 = pHeapArgs->iMsgLog_SleepInterval;
			for (int _eR6 = 0; _eR6 < _slpR6; _eR6 += 50) {
				if (g_bStopTask || g_bStopLogTask) break;
				Sleep(50);
			}
		}









	}while ( 1 );









	InterlockedIncrement(&g_nMsgLogNotSending_ClientCount); 









	//߳==ͻˣTotalCount񡣱߳и±ǰԾ




	g_singleLockListCtrl.Lock();




	CString strActiveClientCount_InMainWindowLine = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);




	int iThisTaskOtherThreads_FromCtr_ActiveClientCount = _ttoi(strActiveClientCount_InMainWindowLine);




	iThisTaskOtherThreads_FromCtr_ActiveClientCount--;




	strActiveClientCount_InMainWindowLine.Format(_T("%d"), iThisTaskOtherThreads_FromCtr_ActiveClientCount);  




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strActiveClientCount_InMainWindowLine);//̼߳һ,Ϣ־Ծ









	CString strFileLog_ActiveClientCount = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 7);




	g_singleLockListCtrl.Unlock();














	




	int iFileLog_ActiveClientCount = _ttoi(strFileLog_ActiveClientCount);









	if ((iThisTaskOtherThreads_FromCtr_ActiveClientCount <= 0) && (iFileLog_ActiveClientCount == 0))////ļ־Ծ Ҳ==0   //modified by lzq JUNE21:<=   == 




	{




		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 7, _T(""));

		// r8: 日志任务自然/被动结束时，把"状态"列置为"已停止"
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("已停止"));




		if (g_bStopTask || g_bStopLogTask)




		{




			g_WLServerTestDlg->m_lMsgLogSuccessCount = 0;  




			g_WLServerTestDlg->m_lUploadState_SuccessCount = 0;









			g_bStopTask = FALSE;




		} 









		  g_WLServerTestDlg->GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);




							g_WLServerTestDlg->GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




				  g_WLServerTestDlg->GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




						  g_WLServerTestDlg->GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(TRUE);




	}




	




	




	delete pHeapArgs;  









	_endthreadex( 0 );









	return 0;




}









void CWLServerTestDlg::OnBnClickedButtonHeartbeat_AddTask()




{




	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("请输入客户端信息"));




		return;




	}









	CString strNeedHB_ClientCount;




	CString strInterval;  




	CString strTotalMinutes;














	   m_comHB_Interval.GetWindowText(strInterval);




	m_comHB_ClientCount.GetWindowText(strNeedHB_ClientCount);     //ͻ




   strTotalMinutes = _T("9999");               // override to run indefinitely //ʱ()









	int iNeedHB_ClientCount = _ttoi(strNeedHB_ClientCount);//lzq:clientcount




	      int iTotalMinutes = _ttoi(strTotalMinutes);//ʱ









	if (iNeedHB_ClientCount > g_nHeartBeatNotSending_ClientCount)




	{




		AfxMessageBox(_T("已注册客户端不能重复注册"));




		return;




	}




	int iThisTask_ThreadCount = iNeedHB_ClientCount / HB_CLIENTCOUNT_PER_THREAD ;














	int iPreviousTaskItemCount = m_listHeartBeat_MainWindow.GetItemCount();//iTaskItemCount ʾЧ?









	CString strNewTaskOrdial;




	strNewTaskOrdial.Format(_T("%d"), iPreviousTaskItemCount+1);









	SYSTEMTIME tm;




	TCHAR szTimeNow[MAX_PATH] = {0};









	GetLocalTime(&tm);




	_sntprintf_s(szTimeNow, MAX_PATH, _TRUNCATE,_T("%04d-%02d-%02d %02d:%02d:%02d"),tm.wYear, tm.wMonth, tm.wDay,tm.wHour, tm.wMinute, tm.wSecond);









	//˴AddTaskĿ  µһ




	int iNewTaskInsertIndex = m_listHeartBeat_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);//iPreviousTaskItemCount Ϊ1 Ϊ1




	




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 1, szTimeNow);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 2, strNeedHB_ClientCount);//ͻ




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 3, strInterval);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 4, strTotalMinutes);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 5, strNeedHB_ClientCount);//Ծͻ˼




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("开始中"));  









	int nTmp = m_listHeartBeat_MainWindow.GetItemCount()-1;




	m_listHeartBeat_MainWindow.EnsureVisible(nTmp,FALSE);









			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);  
	g_bStopTask = FALSE; // v14: reset stop flag before launching HB threads




	









	int iStartIndex_FromVector = 0;




	int iThisThread_ClientIndex = 0;









	for (int i=0; i < iThisTask_ThreadCount; i++)//lzq:ͻ/10 == ٸ̣߳




	{




		PHB_SENDER_THREAD_ARG pThreadArgs = new HB_SENDER_THREAD_ARG;




		if (pThreadArgs == NULL)




		{




			AfxMessageBox(_T("Not enough heap memory!"));









			return;




		}




		else




		{




			iThisThread_ClientIndex = 0; 









			//�????? δmemset 0 




			pThreadArgs ->iClientCount = HB_CLIENTCOUNT_PER_THREAD;




			pThreadArgs ->iHBIntervalMilSec = 30*1000;




			pThreadArgs ->iHBTotalMinutes = iTotalMinutes;




			pThreadArgs ->iThisTask_LineIndex = iNewTaskInsertIndex;




		}









		for(iStartIndex_FromVector;iStartIndex_FromVector<g_nTotalRegistered_ClientCount;iStartIndex_FromVector++)




		{	




			if (!(g_vecAllClientObjects[iStartIndex_FromVector].Get_IsThisClientSendingHeartBeat()))  //ΨһõThisClient_HasSentHBжǷѷ




			{




				g_vecAllClientObjects[iStartIndex_FromVector].Set_IsSendingHeartBeat(TRUE);//Client¼Ѿ









				g_nHeartBeatNotSending_ClientCount--;









				pThreadArgs->iArrClientIndex[iThisThread_ClientIndex] = iStartIndex_FromVector;









				iThisThread_ClientIndex++;









				if (iThisThread_ClientIndex >= HB_CLIENTCOUNT_PER_THREAD)//Ϊ˴ ѡClientdwThisTask_ClientIndex == 10break




				{




					g_nHeartBeatSending_ClientCount += HB_CLIENTCOUNT_PER_THREAD;




					break;




				}




			}




		}




		CString strOnlineClientCount;




		//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




		strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);




		g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//ʾѷ͹Client;;߿ͻ10




		InterlockedIncrement(&g_nActiveHbThreads); // v16: track thread count
		AfxBeginThread((AFX_THREADPROC)ThreadFunc_HeartbeatSend_New, (PHB_SENDER_THREAD_ARG)pThreadArgs, THREAD_PRIORITY_NORMAL, 0, 0, NULL); 		




	




		Sleep(500);




		{




			MSG msg;




			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))




			{




				TranslateMessage(&msg);




				DispatchMessage(&msg);




			}




		}




	}









	GetLocalTime(&tm);




	_sntprintf_s(szTimeNow, MAX_PATH, _TRUNCATE,_T("%04d-%02d-%02d %02d:%02d:%02d"),tm.wYear, tm.wMonth, tm.wDay,tm.wHour, tm.wMinute, tm.wSecond);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 1, szTimeNow);









	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("执行中"));//每线程 执









			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);




}









void CWLServerTestDlg::OnBnClickedButton_Lowest_AddTask()




{




	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("请输入客户端信息"));




		return;




	}









	CString strThisTask_ClientCount = _T("");			




	CString strThisTask_EachClientTotalItems = _T("");			 




	CString strThisTask_EachClientPerSecondItems = _T("");









	CString strWhiteListFilePath = _T("");









	m_comAppLog_Task_ClientCount.GetWindowText(strThisTask_ClientCount);




	m_comAppLog_Task_EachClientTotalItems.GetWindowText(strThisTask_EachClientTotalItems); 




	m_comAppLog_Task_EachClientPerSecondItems.GetWindowText(strThisTask_EachClientPerSecondItems);









	int iThisTask_ClientCount = _ttoi(strThisTask_ClientCount);




	int iThisTask_EachClient_TotalItems = _ttoi(strThisTask_EachClientTotalItems);




	int iThisTask_EachClient_PerSecondItems = _ttoi(strThisTask_EachClientPerSecondItems);
	if (iThisTask_EachClient_PerSecondItems <= 0) iThisTask_EachClient_PerSecondItems = 1; // v6: 结束结束�?









	int iThisTask_TotalItems = iThisTask_ClientCount * iThisTask_EachClient_TotalItems;









	int iRemainRight = iThisTask_EachClient_PerSecondItems;









	if ((iThisTask_ClientCount > g_nMsgLogNotSending_ClientCount) || (iThisTask_ClientCount > g_nFileLogNotSending_ClientCount))




	{




		AfxMessageBox(_T("未注册客户端不能发送日志"));




		return;




	}









	m_iThisTask_SelectedOperationType &= (CLIENT_MSGLOG_NETADAPTER | CLIENT_MSGLOG_EXTDEV | CLIENT_MSGLOG_UDISKPLUG); // preserve extension bits set by caller









	if (((CButton *)GetDlgItem(IDC_OPT_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_OPT;




	}









	if (((CButton *)GetDlgItem(IDC_THT_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_THREAT;




	}









	if (((CButton *)GetDlgItem(IDC_NWL_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_NWL;




	}









	if (((CButton *)GetDlgItem(IDC_WHITE_LIST))->GetCheck())




	{




		m_WLFilePathEdit.GetWindowText(strWhiteListFilePath);




		m_iThisTask_SelectedOperationType |= CLIENT_FILELOG_WLFILE;









		if(!PathFileExists(strWhiteListFilePath))




		{




			AfxMessageBox(_T("请选择文件路径"));




			return;




		}




	}









	if (((CButton *)GetDlgItem(IDC_CHECK_BASE_LINE))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BLINE;




	}









	// Ukey־




	if (((CButton *)GetDlgItem(IDC_CHECK_UKEY))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_UKEY;




	}









	// ݱ־




	if (((CButton *)GetDlgItem(IDC_DATAPROTECT_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_DATAPROTECT;




	}









	// ϵͳ־




	if (((CButton *)GetDlgItem(IDC_SYSPROTECT_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_SYSPROTECT;




	}




	// ϵͳ־




	if (((CButton *)GetDlgItem(IDC_Backup_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BACKUP;




	}









	// ־




	if (((CButton *)GetDlgItem(IDC_Virus_LOG))->GetCheck())




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_Virus;




	}









	// ־hashǷͬ




	if (((CButton *)GetDlgItem(IDC_Whitelist_Path_Type))->GetCheck())




	{




		g_bSamePath = TRUE;




	}




	else




	{




		g_bSamePath = FALSE;




	}









	if( 0 == m_iThisTask_SelectedOperationType )




	{




		AfxMessageBox(_T("请选择日志类型"));




		return;




	}









	//ʽʼ�?????��?




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);




	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(FALSE);









	int iPreviousTaskItemCount = m_listHeartBeat_MainWindow.GetItemCount();//ʾЧ?




	CString strNewTaskOrdial;




	strNewTaskOrdial.Format(_T("%d"), iPreviousTaskItemCount + 1);









	SYSTEMTIME stTimeNow;




	GetLocalTime(&stTimeNow);




	TCHAR szThisTask_StartTime[MAX_PATH] = {0};




	_sntprintf_s(szThisTask_StartTime, MAX_PATH, _TRUNCATE,




		_T("%04d-%02d-%02d %02d:%02d:%02d"),




		stTimeNow.wYear, stTimeNow.wMonth, stTimeNow.wDay,




		stTimeNow.wHour, stTimeNow.wMinute, stTimeNow.wSecond);









	CString     csNew_Interval_MilSec;  









	float			fMisSec = 1000;




	float			fNew_Interval_MisSec = fMisSec / iThisTask_EachClient_PerSecondItems;









	int				iNew_Interval_MisSec   = 1000 / iThisTask_EachClient_PerSecondItems;//һ ?




	char		cNew_TmpBuf[0x20] = {0};




	sprintf(cNew_TmpBuf, "%.02f", fNew_Interval_MisSec); 




	//csNew_Interval_MilSec  = cNew_TmpBuf;		//ÿ֮˯ߵMilSecond




	csNew_Interval_MilSec.Format(_T("%S"),cNew_TmpBuf );














	CString     csThisTask_LastingSeconds = _T("");




	float		iNew_LastingSeconds    = (float)iThisTask_EachClient_TotalItems / iThisTask_EachClient_PerSecondItems;//ÿͻʱ




	char		cNew2_TmpBuf[0x20] = {0};




	sprintf(cNew2_TmpBuf, "%.02f", iNew_LastingSeconds); 




	//csThisTask_LastingSeconds  = cNew2_TmpBuf;  //˴�??????




	csThisTask_LastingSeconds.Format(_T("%S"),cNew2_TmpBuf );














	




	//˴AddTaskĿ




	int iThisTask_InsertLineIndex = m_listHeartBeat_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);////iPreviousTaskItemCount Ϊ2 Ϊ2




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 1, szThisTask_StartTime);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 2, strThisTask_ClientCount);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 3, csNew_Interval_MilSec);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 4, csThisTask_LastingSeconds);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, _T("开始中"));









	csNew_Interval_MilSec = _T("");









	int nTmp = m_listHeartBeat_MainWindow.GetItemCount()-1;




	m_listHeartBeat_MainWindow.EnsureVisible(nTmp,FALSE);




 




	{//Prepare args for each thread:Added by lzq:June07









		if ( (m_iThisTask_SelectedOperationType & MSG_LOG_TYPE) )




		{




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, strThisTask_ClientCount);//ͻ == Ϣ־Ծ




			




			if (m_iThisTask_SelectedOperationType & (CLIENT_MSGLOG_BLINE | CLIENT_MSGLOG_UKEY))




			{




				CString cstrTotalItems_BLine_UKey = _T("");




				m_StateUKey_BLine_AllNum_RightTotal.GetWindowText(cstrTotalItems_BLine_UKey);




				int PreviousTasks_Total_LogNumber = _ttoi(cstrTotalItems_BLine_UKey);




				PreviousTasks_Total_LogNumber += iThisTask_TotalItems;




				cstrTotalItems_BLine_UKey.Format(_T("%d"), PreviousTasks_Total_LogNumber);









				m_StateUKey_BLine_AllNum_RightTotal.SetWindowText(cstrTotalItems_BLine_UKey);




			}




			if (m_iThisTask_SelectedOperationType & (CLIENT_MSGLOG_OPT| CLIENT_MSGLOG_NWL  | CLIENT_MSGLOG_THREAT | CLIENT_MSGLOG_DATAPROTECT | CLIENT_MSGLOG_SYSPROTECT | CLIENT_MSGLOG_BACKUP | CLIENT_MSGLOG_Virus | CLIENT_MSGLOG_NETADAPTER | CLIENT_MSGLOG_EXTDEV | CLIENT_MSGLOG_UDISKPLUG))    //added by lzq: |CLIENT_THREAT_LOG




			{




				CString cstrTotalItems_Opt_Threat = _T("");




				mMsgLog_ThreatOpt_TotalCount_RightTotal.GetWindowText(cstrTotalItems_Opt_Threat);




				int PreviousTasks_Total_LogNumber = _ttoi(cstrTotalItems_Opt_Threat);




				PreviousTasks_Total_LogNumber += iThisTask_TotalItems;




				cstrTotalItems_Opt_Threat.Format(_T("%d"), PreviousTasks_Total_LogNumber);









				mMsgLog_ThreatOpt_TotalCount_RightTotal.SetWindowText(cstrTotalItems_Opt_Threat);




			}




		}




		else




		{//ûMsgLog




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, _T("0"));//0  ȡint ж




		}









		if ( m_iThisTask_SelectedOperationType & FILE_LOG_TYPE )




		{




			




			//½




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, strThisTask_ClientCount);//ͻ == ļ־Ծ









			CString csFileLog_TotalNum = _T("");




			m_WL_TatalNum_RightTotal.GetWindowText(csFileLog_TotalNum);




			int PreviousTasks_Total_FileLogNumber = _ttoi(csFileLog_TotalNum);




			PreviousTasks_Total_FileLogNumber += iThisTask_ClientCount;




			csFileLog_TotalNum.Format(_T("%d"), PreviousTasks_Total_FileLogNumber);









			m_WL_TatalNum_RightTotal.SetWindowText(csFileLog_TotalNum);




			




		}




		else




		{//ûFileLog




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, _T("0"));//0  ȡint ж




		}		




	}//Prepare End









		if ( (m_iThisTask_SelectedOperationType & MSG_LOG_TYPE) )




		{




			PLOG_SENDER_THREAD_ARG  pHeapArgsForAllThread_MsgLog = NULL;




			int iClientIndex = 0;









			for (int i=0; i< iThisTask_ClientCount; i++)




			{




				pHeapArgsForAllThread_MsgLog = new LOG_SENDER_THREAD_ARG;




				pHeapArgsForAllThread_MsgLog->csWhiteListFilePath = _T("");









				if (!pHeapArgsForAllThread_MsgLog)  




				{




					AfxMessageBox(_T("pHeapArgsForAllThread_MsgLog  null!"));




				}




				else




				{




					//memset(pHeapArgsForAllThread_MsgLog,0,sizeof(LOG_SENDER_THREAD_ARG));




					pHeapArgsForAllThread_MsgLog->iThisTask_LineIndex = iThisTask_InsertLineIndex;




					pHeapArgsForAllThread_MsgLog->iThisTask_SelectedLogType = m_iThisTask_SelectedOperationType;
					pHeapArgsForAllThread_MsgLog->dwExtDevSubTypeMask = m_dwExtDevSubTypeMask;









					pHeapArgsForAllThread_MsgLog->iMsgLog_ClientCount = iThisTask_ClientCount;




					pHeapArgsForAllThread_MsgLog->iMsgLog_EachClientTotalCount = iThisTask_EachClient_TotalItems;




					pHeapArgsForAllThread_MsgLog->iMsgLog_EachClientPerSecondCount = iRemainRight;




					pHeapArgsForAllThread_MsgLog->iMsgLog_SleepInterval = fNew_Interval_MisSec; 




					pHeapArgsForAllThread_MsgLog->iMsgLog_ClientCount_X_EachClientTotalCount = iThisTask_TotalItems;









					pHeapArgsForAllThread_MsgLog->csWhiteListFilePath = _T("");




					pHeapArgsForAllThread_MsgLog->sock = g_sock[i];




					pHeapArgsForAllThread_MsgLog->iThisClient_VectorIndex = i;




				}




				




				/*




				for ( iClientIndex = 0; iClientIndex < g_nTotalRegistered_ClientCount; iClientIndex ++)




				{




					if (!(g_vecAllClientObjects[iClientIndex].ThisClient_IsSendingMsgLog()))




					{




							pHeapArgsForAllThread_MsgLog->iThisClient_VectorIndex = iClientIndex;









							g_vecAllClientObjects[iClientIndex].Set_IsSendingMsgLog(TRUE);




							g_nMsgLogNotSending_ClientCount--;




							break;




					}




				}




				*/









				AfxBeginThread((AFX_THREADPROC)ThreadFunc_MsgLogSend,(PLOG_SENDER_THREAD_ARG)pHeapArgsForAllThread_MsgLog, THREAD_PRIORITY_TIME_CRITICAL, 0, 0, NULL);//ÿclient һ߳




			}




		}









		if ( m_iThisTask_SelectedOperationType & FILE_LOG_TYPE ) 




		{




			PLOG_SENDER_THREAD_ARG  pHeapArgsForAllThread_FileLog = NULL;




			int iClientIndex = 0;









			for (int i =0; i< iThisTask_ClientCount; i++)




			{




				pHeapArgsForAllThread_FileLog = new LOG_SENDER_THREAD_ARG;




				 pHeapArgsForAllThread_FileLog->csWhiteListFilePath = _T("");









				if (!pHeapArgsForAllThread_FileLog)




				{




					AfxMessageBox(_T("pHeapArgsForAllThread_FileLog  null!"));




				}




				else




				{




					//memset(pHeapArgsForAllThread_FileLog,0,sizeof(LOG_SENDER_THREAD_ARG));




				   




					pHeapArgsForAllThread_FileLog->iThisTask_LineIndex = iThisTask_InsertLineIndex;




					pHeapArgsForAllThread_FileLog->iThisTask_SelectedLogType = m_iThisTask_SelectedOperationType;









					pHeapArgsForAllThread_FileLog->iMsgLog_ClientCount = iThisTask_ClientCount;




					pHeapArgsForAllThread_FileLog->iMsgLog_EachClientTotalCount = iThisTask_EachClient_TotalItems;




					pHeapArgsForAllThread_FileLog->iMsgLog_EachClientPerSecondCount = iRemainRight;




					pHeapArgsForAllThread_FileLog->iMsgLog_SleepInterval = fNew_Interval_MisSec; 




					pHeapArgsForAllThread_FileLog->iMsgLog_ClientCount_X_EachClientTotalCount = iThisTask_TotalItems;









					pHeapArgsForAllThread_FileLog->csWhiteListFilePath = strWhiteListFilePath; //һ




				}









				for (iClientIndex; iClientIndex < g_nTotalRegistered_ClientCount; iClientIndex ++)




				{




					if (!(g_vecAllClientObjects[iClientIndex].ThisClient_IsSendingFileLog()))




					{




						pHeapArgsForAllThread_FileLog->iThisClient_VectorIndex = iClientIndex;









						g_vecAllClientObjects[iClientIndex].Set_IsSendingFileLog(TRUE);




						g_nFileLogNotSending_ClientCount--;




						break;




					}




				}  









				AfxBeginThread((AFX_THREADPROC)ThreadFunc_FileLogSend,(PLOG_SENDER_THREAD_ARG)pHeapArgsForAllThread_FileLog, THREAD_PRIORITY_NORMAL, 0, 0, NULL);




			}			




		}   




  




	    /*




		Sleep(20);




		{




			MSG msg;




			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))




			{




				TranslateMessage(&msg);




				DispatchMessage(&msg);




			}




		}*/




	  




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, _T("执行中"));//每客户端线程 执














	//GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE); //modified by lzq 0621:ֻ?�?????��?




	                  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




	                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




	                GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(TRUE);









}









void CWLServerTestDlg::OnBnClicked_PortsTest()




{




	// TODO: ڴӿؼ֪ͨ?




	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("请输入客户端信息"));




		return;




	}









	CString strMsg;




	CSendInfoToServer TestConn_Reg(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	SOCKET sockTmp_Reg_HB=INVALID_SOCKET;




	if(!TestConn_Reg.CreateConnection(sockTmp_Reg_HB,m_strServerIP,m_strServerPort))




	{




		strMsg.Format(_T("IP:%s,Port:%s 连接失败！"),m_strServerIP.GetBuffer(),m_strServerPort.GetBuffer());




		AfxMessageBox(strMsg);




		return;




	}   




	




	TestConn_Reg.CloseConnection(sockTmp_Reg_HB);




	if(!TestConn_Reg.CreateConnection(sockTmp_Reg_HB,m_strServerIP,m_strServerPortHB))




	{




		strMsg.Format(_T("IP:%s,Port:%s 连接失败！"),m_strServerIP.GetBuffer(),m_strServerPortHB.GetBuffer());




		AfxMessageBox(strMsg);




		return;




	}




	TestConn_Reg.CloseConnection(sockTmp_Reg_HB);









	AfxMessageBox(_T("连接可用！"));




	return;




}










void CWLServerTestDlg::OnBnClicked_Lowest_StopTask()




{




	if (!g_bStopTask)




	{




		g_bStopTask = TRUE;




		GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);




		GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




		GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);




		GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(FALSE);




		m_StateUKey_BLine_AllNum_RightTotal.SetWindowText(_T("0"));




		m_StateUKey_BLine_SuccessNum_Left.SetWindowText(_T("0"));









		m_WL_TatalNum_RightTotal.SetWindowText(_T("0"));




		m_WL_SuccessNum_Left.SetWindowText(_T("0"));









		mMsgLog_ThreatOpt_TotalCount_RightTotal.SetWindowText(_T("0"));




		mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(_T("0"));









		GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);




		GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(TRUE);




	}




}









void CWLServerTestDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)




{




	SCROLLINFO scrollinfo;




	GetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);




	int unit=3;        




	switch (nSBCode)  




	{      




	case SB_LINEUP:          //Scroll one line up




		scrollinfo.nPos -= 1;  




		if (scrollinfo.nPos<scrollinfo.nMin)




		{  




			scrollinfo.nPos = scrollinfo.nMin;  




			break;  




		}  




		SetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);  




		ScrollWindow(0,unit); 




		break;  




	case SB_LINEDOWN:           //Scroll one line down




		scrollinfo.nPos += 1;  




		if (scrollinfo.nPos+scrollinfo.nPage>scrollinfo.nMax)  //˴һҪעϻĳȣж




		{  




			scrollinfo.nPos = scrollinfo.nMax;  




			break;  




		}  




		SetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);  




		ScrollWindow(0,-unit);  




		break;  




	case SB_PAGEUP:            //Scroll one page up.




		scrollinfo.nPos -= 5;  




		if (scrollinfo.nPos<=scrollinfo.nMin)




		{  




			scrollinfo.nPos = scrollinfo.nMin;  




			break;  




		}  




		SetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);  




		ScrollWindow(0,unit*5);  




		break;  




	case SB_PAGEDOWN:        //Scroll one page down        




		scrollinfo.nPos += 5;  




		if (scrollinfo.nPos+scrollinfo.nPage>=scrollinfo.nMax)  //˴һҪעϻĳȣж




		{  




			scrollinfo.nPos = scrollinfo.nMax;  




			break;  




		}  




		SetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);  




		ScrollWindow(0,-unit*5);  




		break;  




	case SB_ENDSCROLL:      //End scroll     




		break;  




	case SB_THUMBPOSITION:  //Scroll to the absolute position. The current position is provided in nPos




		break;  




	case SB_THUMBTRACK:                  //Drag scroll box to specified position. The current position is provided in nPos




		ScrollWindow(0,(scrollinfo.nPos-nPos)*unit);  




		scrollinfo.nPos = nPos;  




		SetScrollInfo(SB_VERT,&scrollinfo,SIF_ALL);




		break;  




	}









	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);




}













void CWLServerTestDlg::OnBnClickedOptRegisterSametime()




{




    // TODO: ڴӿؼ֪ͨ?




    BOOL bRegisterSameTime = m_bRegisterSameTime.GetCheck();




    if (bRegisterSameTime)




    {




        ((CStatic *)GetDlgItem(IDC_STATIC_RegisteredThreadCount))->ShowWindow(SW_SHOW);




        m_RegisterThreadCount.ShowWindow(SW_SHOW);




        m_EditRegFailCD.ShowWindow(SW_SHOW);




        ((CStatic *)GetDlgItem(IDC_STATIC_REGFAIL_CD))->ShowWindow(SW_SHOW);




        ((CStatic *)GetDlgItem(IDC_STATIC_UPLOADWLCount))->ShowWindow(SW_SHOW);




        ((CStatic *)GetDlgItem(IDC_UPLOADWLCOUNT))->ShowWindow(SW_SHOW);




    }




    else




    {  




        ((CStatic *)GetDlgItem(IDC_STATIC_RegisteredThreadCount))->ShowWindow(SW_HIDE);




        m_RegisterThreadCount.ShowWindow(SW_HIDE);




        m_EditRegFailCD.ShowWindow(SW_HIDE);




        ((CStatic *)GetDlgItem(IDC_STATIC_REGFAIL_CD))->ShowWindow(SW_HIDE);




        ((CStatic *)GetDlgItem(IDC_STATIC_UPLOADWLCount))->ShowWindow(SW_HIDE);




        ((CStatic *)GetDlgItem(IDC_UPLOADWLCOUNT))->ShowWindow(SW_HIDE);




    }




}












// TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT



#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")


// =======================================================================
// Phase 4: New method implementations (WLServerTest v2)
// =======================================================================

void CWLServerTestDlg::ApplyProjectTypeSelection()
{
	// Auto-select log categories based on project type (IEG/EDR) - aligned with C# SimulatorApp
	int sel = m_comboProjectType.GetCurSel();
	CString strType;
	m_comboProjectType.GetLBText(sel, strType);

	bool bIEG = (strType == _T("IEG"));
	bool bEDR = (strType == _T("EDR"));

	if (bIEG)
	{
		// IEG exclusive
		m_catNonWhitelist.SetCheck(BST_CHECKED);
		m_catRegProtect.SetCheck(BST_CHECKED);
		m_catUsb.SetCheck(BST_CHECKED);
		m_catUsbWarning.SetCheck(BST_CHECKED);
		m_catWlTamper.SetCheck(BST_CHECKED);
		m_catVulnProtect.SetCheck(BST_CHECKED);
		m_catProcAudit.SetCheck(BST_CHECKED);
		m_catUDiskPlug.SetCheck(BST_CHECKED);
		m_catNetAdapter.SetCheck(BST_CHECKED);
		// External device control (9 types, IEG fine-grained alerts)
		m_catExtUsbPort.SetCheck(BST_CHECKED);
		m_catExtWpd.SetCheck(BST_CHECKED);
		m_catExtCdrom.SetCheck(BST_CHECKED);
		m_catExtWlan.SetCheck(BST_CHECKED);
		m_catExtUsbEth.SetCheck(BST_CHECKED);
		m_catExtFloppy.SetCheck(BST_CHECKED);
		m_catExtBt.SetCheck(BST_CHECKED);
		m_catExtSerial.SetCheck(BST_CHECKED);
		m_catExtParallel.SetCheck(BST_CHECKED);

		// Shared (both IEG & EDR)
		m_catClientOps.SetCheck(BST_CHECKED);
		m_catOs.SetCheck(BST_CHECKED);
		m_catOutbound.SetCheck(BST_CHECKED);
		m_catFileProtect.SetCheck(BST_CHECKED);
		m_catMandatoryAccess.SetCheck(BST_CHECKED);
		m_catVirusAlert.SetCheck(BST_CHECKED);

		// Threat detection: IEG has 3 types
		m_catThreatProc.SetCheck(BST_CHECKED);
		m_catThreatReg.SetCheck(BST_CHECKED);
		m_catThreatFile.SetCheck(BST_CHECKED);

		// EDR exclusive - unchecked
		m_catFirewall.SetCheck(BST_UNCHECKED);
		m_catSysGuard.SetCheck(BST_UNCHECKED);
		m_catThreatDll.SetCheck(BST_UNCHECKED);

		// Disabled for both
		m_catThreatOs.SetCheck(BST_UNCHECKED);
	}
	else if (bEDR)
	{
		// EDR exclusive
		m_catFirewall.SetCheck(BST_CHECKED);
		m_catSysGuard.SetCheck(BST_CHECKED);

		// Shared (both IEG & EDR)
		m_catClientOps.SetCheck(BST_CHECKED);
		m_catOs.SetCheck(BST_CHECKED);
		m_catOutbound.SetCheck(BST_CHECKED);
		m_catFileProtect.SetCheck(BST_CHECKED);
		m_catMandatoryAccess.SetCheck(BST_CHECKED);
		m_catVirusAlert.SetCheck(BST_CHECKED);

		// Threat detection: EDR has all 4 types
		m_catThreatProc.SetCheck(BST_CHECKED);
		m_catThreatReg.SetCheck(BST_CHECKED);
		m_catThreatFile.SetCheck(BST_CHECKED);
		m_catThreatDll.SetCheck(BST_CHECKED);

		// IEG exclusive - unchecked
		m_catNonWhitelist.SetCheck(BST_UNCHECKED);
		m_catRegProtect.SetCheck(BST_UNCHECKED);
		m_catUsb.SetCheck(BST_UNCHECKED);
		m_catUsbWarning.SetCheck(BST_UNCHECKED);
		m_catWlTamper.SetCheck(BST_UNCHECKED);
		m_catVulnProtect.SetCheck(BST_UNCHECKED);
		m_catProcAudit.SetCheck(BST_UNCHECKED);
		m_catUDiskPlug.SetCheck(BST_UNCHECKED);
		m_catNetAdapter.SetCheck(BST_UNCHECKED);
		m_catExtUsbPort.SetCheck(BST_UNCHECKED);
		m_catExtWpd.SetCheck(BST_UNCHECKED);
		m_catExtCdrom.SetCheck(BST_UNCHECKED);
		m_catExtWlan.SetCheck(BST_UNCHECKED);
		m_catExtUsbEth.SetCheck(BST_UNCHECKED);
		m_catExtFloppy.SetCheck(BST_UNCHECKED);
		m_catExtBt.SetCheck(BST_UNCHECKED);
		m_catExtSerial.SetCheck(BST_UNCHECKED);
		m_catExtParallel.SetCheck(BST_UNCHECKED);

		// Disabled for both
		m_catThreatOs.SetCheck(BST_UNCHECKED);
	}
}
void CWLServerTestDlg::OnProjectTypeSelChange()
{
    ApplyProjectTypeSelection();
}

void CWLServerTestDlg::OnBnClickedOsWindows()
{
	LoadClientVersionCombo();
	m_staticHbBadge.SetWindowText(_T("TCP(Windows)"));
	UpdateOsInfoText();
}

void CWLServerTestDlg::OnBnClickedOsLinux()
{
	LoadClientVersionCombo();
	m_staticHbBadge.SetWindowText(_T("HTTPS(Linux)"));
	UpdateOsInfoText();
}

void CWLServerTestDlg::UpdateOsInfoText()
{
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	CString strVer;
	int nSel = m_comboClientVersion.GetCurSel();
	if (nSel >= 0)
		m_comboClientVersion.GetLBText(nSel, strVer);
	CString strOS = bLinux ? _T("Linux centos7") : _T("Windows 10");
	CString strInfo;
	strInfo.Format(_T("OS: %s\r\nVer: %s"), (LPCTSTR)strOS, (LPCTSTR)strVer); // r8: 2-line in narrow box
	m_staticOsInfo.SetWindowText(strInfo);
}

void CWLServerTestDlg::OnClientVersionSelChange()
{
	UpdateOsInfoText();
}

void CWLServerTestDlg::LoadClientVersionCombo()
{
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	m_comboClientVersion.ResetContent();
	CString strList;
	if (!bLinux)
	{
		strList = CProfileConfig::GetProfileConfigInstance()->ReadWindowsVersionList_FromIni();
		if (strList.IsEmpty())
			strList = _T("V300R011C01B090|V300R006C05B270|V300R006C02B090"); // r8: remove B030
	}
	else
	{
		strList = CProfileConfig::GetProfileConfigInstance()->ReadLinuxVersionList_FromIni();
		if (strList.IsEmpty())
			strList = _T("V300R011C11B060-Redhat7.x-x64");
	}
	int nStart = 0;
	CString strToken = strList.Tokenize(_T("|"), nStart);
	while (!strToken.IsEmpty())
	{
		m_comboClientVersion.AddString(strToken);
		strToken = strList.Tokenize(_T("|"), nStart);
	}
	m_comboClientVersion.SetCurSel(0);
	UpdateOsInfoText();
}

void CWLServerTestDlg::OnBnClickedHbStart()
{
	CString strInterval;
	m_editHbInterval.GetWindowText(strInterval);
	if (strInterval.IsEmpty()) strInterval = _T("30000");
	m_comHB_Interval.ResetContent();
	m_comHB_Interval.AddString(strInterval);
	m_comHB_Interval.SetCurSel(0);

	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	if (!bLinux)
	{
		// 结束 HB �ͻ结束�=��ע结束结束结束ʱ��=9999���ӣ��ֶ�ֹͣ��
		{
			CString strCC;
			strCC.Format(_T("%d"), g_nTotalRegistered_ClientCount > 0
				? g_nTotalRegistered_ClientCount : 1);
			m_comHB_ClientCount.ResetContent();
			m_comHB_ClientCount.AddString(strCC);
			m_comHB_ClientCount.SetCurSel(0);
			m_comHB_TotalMinutes.ResetContent();
			CString strHbDur;
			m_editHbDuration.GetWindowText(strHbDur);
			int nHbDur = _ttoi(strHbDur);
			if (nHbDur <= 0) nHbDur = 7200;
			CString strHbDurOut;
			strHbDurOut.Format(_T("%d"), nHbDur);
			m_comHB_TotalMinutes.AddString(strHbDurOut);
			m_comHB_TotalMinutes.SetCurSel(0);
		}
		GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->SendMessage(BM_CLICK);
	}
	else
	{
		if (m_bLinuxHbRunning)
		{
			AfxMessageBox(_T("Linux HB already running"));
			return;
		}
		if (g_vecAllClientObjects.empty())
		{
			AfxMessageBox(_T("Please register clients first"));
			return;
		}
		// ���߳�Ԥ�� interval���̰߳�ȫ��
		m_nLinuxHbIntervalMs = _ttoi(strInterval);
		if (m_nLinuxHbIntervalMs <= 0) m_nLinuxHbIntervalMs = 30000;
		m_bLinuxHbRunning = TRUE;
		m_hLinuxHbThread = (HANDLE)_beginthreadex(NULL, 0,
			[](void* pArg) -> unsigned int {
				CWLServerTestDlg* pDlg = (CWLServerTestDlg*)pArg;
				int nInterval = pDlg->m_nLinuxHbIntervalMs;
				CString strHost = pDlg->m_strServerIP;
				int nPort = _ttoi(pDlg->m_strServerPort);
				int nHbPort = _ttoi(pDlg->m_strServerPortHB);
				while (pDlg->m_bLinuxHbRunning && !g_bStopTask)
				{
					CSingleLock lk(&g_csVecClients, TRUE);
					for (int i = 0; i < (int)g_vecAllClientObjects.size(); ++i)
					{
						if (!pDlg->m_bLinuxHbRunning) break;
						pDlg->SendLinuxHeartbeat_HTTPS(g_vecAllClientObjects[i], strHost, nPort, nHbPort);
					}
					lk.Unlock();
					Sleep(nInterval);
				}
				pDlg->m_bLinuxHbRunning = FALSE;
				return 0;
			}, this, 0, NULL);
		AppendLogOutput(_T("[Linux HB] Thread started"));
	}
}

void CWLServerTestDlg::OnBnClickedHbStop()
{
	m_bLinuxHbRunning = FALSE;
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	if (!bLinux) g_bStopTask = TRUE;
	AppendLogOutput(_T("[HB] Stop signal sent"));
}

void CWLServerTestDlg::OnBnClickedLogAdd()
{
	g_bStopLogTask = FALSE; // r3: reset log-stop flag on each task add
	DWORD dwTypes = 0;
	if (m_catClientOps.GetCheck())       dwTypes |= 0x00000001;
	if (m_catOs.GetCheck())              dwTypes |= 0x00000002;
	if (m_catOutbound.GetCheck())        dwTypes |= 0x00000004;
	if (m_catFileProtect.GetCheck())     dwTypes |= 0x00000008;
	if (m_catRegProtect.GetCheck())      dwTypes |= 0x00000010;
	if (m_catMandatoryAccess.GetCheck()) dwTypes |= 0x00000020;
	if (m_catVirusAlert.GetCheck())      dwTypes |= 0x00000040;
	if (m_catUsb.GetCheck())             dwTypes |= 0x00000080;
	if (m_catUsbWarning.GetCheck())      dwTypes |= 0x00000100;
	if (m_catFirewall.GetCheck())        dwTypes |= 0x00000200;
	if (m_catVulnProtect.GetCheck())     dwTypes |= 0x00000400;
	if (m_catProcAudit.GetCheck())       dwTypes |= 0x00000800;
	if (m_catNonWhitelist.GetCheck())    dwTypes |= 0x00001000;
	if (m_catWlTamper.GetCheck())        dwTypes |= 0x00002000;
	if (m_catSysGuard.GetCheck())        dwTypes |= 0x00004000;
	if (m_catUDiskPlug.GetCheck())       dwTypes |= 0x00008000;
	if (m_catNetAdapter.GetCheck())      dwTypes |= 0x00010000;
	if (m_catExtUsbPort.GetCheck())      dwTypes |= 0x00020000;
	if (m_catExtWpd.GetCheck())          dwTypes |= 0x00040000;
	if (m_catExtCdrom.GetCheck())        dwTypes |= 0x00080000;
	if (m_catExtWlan.GetCheck())         dwTypes |= 0x00100000;
	if (m_catExtUsbEth.GetCheck())       dwTypes |= 0x00200000;
	if (m_catExtFloppy.GetCheck())       dwTypes |= 0x00400000;
	if (m_catExtBt.GetCheck())           dwTypes |= 0x00800000;
	if (m_catExtSerial.GetCheck())       dwTypes |= 0x01000000;
	if (m_catExtParallel.GetCheck())     dwTypes |= 0x02000000;
	if (m_catThreatProc.GetCheck())      dwTypes |= 0x04000000;
	if (m_catThreatReg.GetCheck())       dwTypes |= 0x08000000;
	if (m_catThreatFile.GetCheck())      dwTypes |= 0x10000000;
	if (m_catThreatDll.GetCheck())       dwTypes |= 0x20000000;
	if (m_catThreatOs.GetCheck())        dwTypes |= 0x40000000;

	if (dwTypes == 0)
	{
		AfxMessageBox(_T("Please select at least one log category"));
		return;
	}

	CString strTotal, strHttpsC, strHttpsEps, strTcpC, strTcpEps, strTcpHit;
	m_editLogTotal.GetWindowText(strTotal);
	m_editHttpsCount.GetWindowText(strHttpsC);
	m_editHttpsEps.GetWindowText(strHttpsEps);
	m_editTcpCount.GetWindowText(strTcpC);
	m_editTcpEps.GetWindowText(strTcpEps);
	m_editTcpHit.GetWindowText(strTcpHit);

	int nTotal    = _ttoi(strTotal);
	int nHttpsC   = _ttoi(strHttpsC);
	int nHttpsEps = _ttoi(strHttpsEps);
	int nTcpC     = _ttoi(strTcpC);
	int nTcpEps   = _ttoi(strTcpEps);

	// r7: 分离双通道——HTTPS 短连接（低 26 位）与 TCP 长连接威胁检测（高 5 位 0x7C000000）
	BOOL bHttpsLog = (dwTypes & 0x03FFFFFF) != 0;
	BOOL bTcpLog   = (dwTypes & 0x7C000000) != 0;
	BOOL bHttpsRun = bHttpsLog && (nHttpsC > 0);
	BOOL bTcpRun   = bTcpLog   && (nTcpC   > 0);

	if (!bHttpsRun && !bTcpRun)
	{
		if (bHttpsLog && !bTcpLog)
			AfxMessageBox(_T("请填写 HTTPS 客户端数（短连接日志）"));
		else if (!bHttpsLog && bTcpLog)
			AfxMessageBox(_T("请填写 TCP 客户端数（威胁检测长连接日志）"));
		else
			AfxMessageBox(_T("请填写至少一个通道的客户端数"));
		return;
	}

	CString sTmp;
	sTmp.Format(_T("%d"), nTotal);
	m_comAppLog_Task_EachClientTotalItems.ResetContent();
	m_comAppLog_Task_EachClientTotalItems.AddString(sTmp);
	m_comAppLog_Task_EachClientTotalItems.SetCurSel(0);

	// ── Channel 1: HTTPS 短连接 ──
	if (bHttpsRun)
	{
		sTmp.Format(_T("%d"), nHttpsC);
		m_comAppLog_Task_ClientCount.ResetContent();
		m_comAppLog_Task_ClientCount.AddString(sTmp);
		m_comAppLog_Task_ClientCount.SetCurSel(0);

		sTmp.Format(_T("%d"), max(1, nHttpsEps));
		m_comAppLog_Task_EachClientPerSecondItems.ResetContent();
		m_comAppLog_Task_EachClientPerSecondItems.AddString(sTmp);
		m_comAppLog_Task_EachClientPerSecondItems.SetCurSel(0);

		// r8: 把新位图(dwTypes)映射到老 hidden checkbox，AddTask 据此再 OR 回 CLIENT_MSGLOG_*
		BOOL bFileProtect = (dwTypes & 0x00000008) ? TRUE : FALSE; // -> DATAPROTECT
    BOOL bRegProtect  = (dwTypes & 0x00000010) ? TRUE : FALSE;
    BOOL bVirus       = (dwTypes & 0x00000040) ? TRUE : FALSE; // -> CLIENT_MSGLOG_Virus
    BOOL bSysGuard    = (dwTypes & 0x00004000) ? TRUE : FALSE; // -> SYSPROTECT
    BOOL bVulnProtect = (dwTypes & 0x00000400) ? TRUE : FALSE; // -> THREAT
    BOOL bNonWl       = (dwTypes & 0x00001000) ? TRUE : FALSE; // -> NWL
    BOOL bOs          = (dwTypes & 0x00000002) ? TRUE : FALSE; // -> OPT (ClientOps)
    BOOL bOutbound    = (dwTypes & 0x00000004) ? TRUE : FALSE; // -> OPT
    BOOL bMandatory   = (dwTypes & 0x00000020) ? TRUE : FALSE; // -> OPT
    BOOL bUsb         = (dwTypes & 0x00000080) ? TRUE : FALSE; // -> OPT
    BOOL bUsbWarn     = (dwTypes & 0x00000100) ? TRUE : FALSE; // -> OPT
    BOOL bFirewall    = (dwTypes & 0x00000200) ? TRUE : FALSE; // -> OPT
    BOOL bProcAudit   = (dwTypes & 0x00000800) ? TRUE : FALSE; // -> OPT
    BOOL bWlTamper    = (dwTypes & 0x00002000) ? TRUE : FALSE; // -> OPT
    BOOL bUdiskPlug   = (dwTypes & 0x00008000) ? TRUE : FALSE; // -> UDISKPLUG

    // Set CLIENT_MSGLOG_* bits for HTTPS categories (old-style log sender compatibility)
    if (bFileProtect)           m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_DATAPROTECT;
    if (bSysGuard)              m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_SYSPROTECT;
    if (bVulnProtect)           m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_THREAT;
    if (bVirus)                 m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_Virus;
    if (bNonWl)                 m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_NWL;
    if (bOs || bOutbound || bMandatory || bUsb || bUsbWarn || bFirewall || bProcAudit || bWlTamper)
        m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_OPT;
    if (bUdiskPlug)             m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_UDISKPLUG;

    // Warn about unimplemented categories
    DWORD dwUnsupported = (dwTypes & 0x03FFFFFF) & ~(0x00000008 | 0x00000010 | 0x00000040 | 0x00004000 | 0x00000400 | 0x00001000 | 0x00000002 | 0x00000004 | 0x00000020 | 0x00000080 | 0x00000100 | 0x00000200 | 0x00000800 | 0x00002000 | 0x00008000 | 0x00000001); // ClientOps(0x01) not in enum
    if (dwUnsupported) {
        CString s; s.Format(_T("[LOG][WARN] Unsupported HTTPS categories (bits=0x%08X)"), dwUnsupported);
        AppendLogOutput(s);
    }

    // Extension bits (NETADAPTER + EXTDEV)
    if (dwTypes & 0x00010000) m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_NETADAPTER;
    m_dwExtDevSubTypeMask = dwTypes & 0x03FE0000;
    if (dwTypes & 0x03FE0000) m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_EXTDEV;
OnBnClickedButton_Lowest_AddTask();
		// 还原 hidden checkbox 防止下次复选
		((CButton*)GetDlgItem(IDC_OPT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_NWL_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_DATAPROTECT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_SYSPROTECT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_Virus_LOG))->SetCheck(0);
		AppendLogOutput(_T("[LOG] HTTPS 通道任务已添加"));
	}

	// ── Channel 2: TCP 威胁检测长连接 ──
	if (bTcpRun)
	{
		sTmp.Format(_T("%d"), nTcpC);
		m_comAppLog_Task_ClientCount.ResetContent();
		m_comAppLog_Task_ClientCount.AddString(sTmp);
		m_comAppLog_Task_ClientCount.SetCurSel(0);

		sTmp.Format(_T("%d"), max(1, nTcpEps));
		m_comAppLog_Task_EachClientPerSecondItems.ResetContent();
		m_comAppLog_Task_EachClientPerSecondItems.AddString(sTmp);
		m_comAppLog_Task_EachClientPerSecondItems.SetCurSel(0);

		// Clear all hidden checkboxes, then set threat type only
		((CButton*)GetDlgItem(IDC_OPT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_NWL_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_DATAPROTECT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_SYSPROTECT_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_Virus_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_THT_LOG))->SetCheck(1);
		((CButton*)GetDlgItem(IDC_Backup_LOG))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_CHECK_BASE_LINE))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_CHECK_UKEY))->SetCheck(0);
		((CButton*)GetDlgItem(IDC_WHITE_LIST))->SetCheck(0);
		OnBnClickedButton_Lowest_AddTask();
		((CButton*)GetDlgItem(IDC_THT_LOG))->SetCheck(0);
		AppendLogOutput(_T("[LOG] TCP 威胁通道任务已添加"));
	}
}

void CWLServerTestDlg::OnBnClickedLogStop()
{
	g_bStopLogTask = TRUE; // r3: only stop log, leave HB running
	AppendLogOutput(_T("[LOG] Stop task sent"));
}

void CWLServerTestDlg::OnBnClickedWlUpload()
{
	// v15: IDC_WHITE_LIST is hidden; check file path, force-check it, then delegate
	CString strWLPath;
	m_WLFilePathEdit.GetWindowText(strWLPath);
	if (strWLPath.IsEmpty())
	{
		AfxMessageBox(_T("请选择白名单文件"));
		return;
	}
	if (!PathFileExists(strWLPath))
	{
		AfxMessageBox(_T("请选择文件路径"));
		return;
	}
	// Force-check the hidden IDC_WHITE_LIST so AddTask picks up FILE_LOG_TYPE
	CButton* pWLChk = (CButton*)GetDlgItem(IDC_WHITE_LIST);
	if (pWLChk) pWLChk->SetCheck(BST_CHECKED);
	OnBnClickedButton_Lowest_AddTask();
	if (pWLChk) pWLChk->SetCheck(BST_UNCHECKED); // restore
	AppendLogOutput(_T("[WL] Whitelist upload task added"));
}

void CWLServerTestDlg::OnBnClickedWlStop2()
{
	GetDlgItem(IDC_BUTTON_STOP_TASK)->SendMessage(BM_CLICK);
	AppendLogOutput(_T("[WL] Stop whitelist upload"));
}

void CWLServerTestDlg::OnBnClickedWlPreview()
{
	CString strPath;
	m_WLFilePathEdit.GetWindowText(strPath);
	if (strPath.IsEmpty())
	{
		AfxMessageBox(_T("Please select a whitelist file first"));
		return;
	}
	CWhitelistPreviewDlg dlg(strPath, this);
	dlg.DoModal();
}

void CWLServerTestDlg::OnBnClickedVerMgmt()
{
	// r7: 根据当前主界面 OS 选择打开对应的版本管理弹窗
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	CVersionManagementDlg dlg(bLinux, this);
	dlg.DoModal();
	LoadClientVersionCombo();
}

void CWLServerTestDlg::OnBnClickedLogHelp()
{
	CLogCategoryHelpDlg dlg(this);
	dlg.DoModal();
}



void CWLServerTestDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		UpdateStatsDisplay();
	}
	CDialog::OnTimer(nIDEvent);
}

void CWLServerTestDlg::UpdateStatsDisplay()
{
	CString s;
	s.Format(_T("%d"), g_nTotalRegistered_ClientCount);
	m_stRegTotal.SetWindowText(s);
	m_stRegSucc.SetWindowText(s);
	s.Format(_T("%d"), g_nHeartBeatSending_ClientCount);
	m_stHbOnline.SetWindowText(s);
	s.Format(_T("%d"), g_nTotalRegistered_ClientCount);
	m_stHbTotal.SetWindowText(s);
	s.Format(_T("%lld"), m_lMsgLogSuccessCount);
	m_stLogSucc2.SetWindowText(s);
	s.Format(_T("%lld"), m_lFileLog_WL_SuccessCount);
	m_stWlSucc2.SetWindowText(s);
}

void CWLServerTestDlg::AppendLogOutput(LPCTSTR szMsg)
{
	CString* pStr = new CString(szMsg);
	PostMessage(WM_APP + 100, (WPARAM)pStr, 0);
}

LRESULT CWLServerTestDlg::OnAppendLogOutput(WPARAM wParam, LPARAM /*lParam*/)
{
	CString* pStr = (CString*)wParam;
	if (!pStr) return 0;
	int nLen = m_editLogOutput.GetWindowTextLength();
	m_editLogOutput.SetSel(nLen, nLen);
		CString strTime;
	SYSTEMTIME st;
	GetLocalTime(&st);
	strTime.Format(_T("[%04d-%02d-%02d %02d:%02d:%02d.%03d] "),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	m_editLogOutput.ReplaceSel(strTime + *pStr + _T("\r\n"));
	// v9: 从顶部显示，不自动滚到底�?
	m_editLogOutput.SetSel(0, 0);
	m_editLogOutput.PostMessage(EM_SCROLL, SB_TOP, 0);
	delete pStr;
	return 0;
}

BOOL CWLServerTestDlg::SendLinuxHeartbeat_HTTPS(client& curClient,
	CString strHost, int nPort, int /*nHbPort*/)
{
	HINTERNET hSession = NULL;
	HINTERNET hConn    = NULL;
	HINTERNET hReq     = NULL;
	BOOL bOK = FALSE;

	WCHAR wHost[256] = {0};
	MultiByteToWideChar(CP_ACP, 0, CT2A(strHost), -1, wHost, 256);

	hSession = WinHttpOpen(L"WLServerTest/2.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) goto cleanup;

	hConn = WinHttpConnect(hSession, wHost, (INTERNET_PORT)nPort, 0);
	if (!hConn) goto cleanup;

	hReq = WinHttpOpenRequest(hConn, L"POST", L"/USM/clientHeartbeat.do",
		NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		WINHTTP_FLAG_SECURE);
	if (!hReq) goto cleanup;

	{
		DWORD dwSecFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
			| SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
			| SECURITY_FLAG_IGNORE_CERT_CN_INVALID
			| SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
		WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &dwSecFlags, sizeof(DWORD));

		CString strIP = curClient.GetClientIP();
		int ip[4] = {0};
		_stscanf(strIP, _T("%d.%d.%d.%d"), &ip[0], &ip[1], &ip[2], &ip[3]);
		CString strMac;
		strMac.Format(_T("02-00-%02X-%02X-%02X-%02X"), ip[0], ip[1], ip[2], ip[3]);

		CString strJson;
		strJson.Format(
			_T("[{\"ComputerID\":\"%s\",\"CMDTYPE\":200,\"CMDID\":200,")
			_T("\"Domain\":\"\",\"CMDContent\":{\"dwCPU\":1,\"dwMem\":20,")
			_T("\"WindowsVersion\":\"Linux centos7\",\"ComputerName\":\"\",")
			_T("\"ComputerIP\":\"%s\",\"ComputerMac\":\"%s\",")
			_T("\"bootTime\":0,\"bootTimeStr\":\"\",")
			_T("\"Partiton\":[{\"Drive\":\"C:\",\"TotalSize\":\"100G\",\"UsageRate\":\"30%%\"}]},")
			_T("\"clientLanguage\":\"zh\"}]"),
			(LPCTSTR)curClient.Client_GetComputerID(),
			(LPCTSTR)strIP,
			(LPCTSTR)strMac
		);

		int nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, CT2W(strJson), -1, NULL, 0, NULL, NULL);
		std::string bodyUtf8(nUtf8Len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, CT2W(strJson), -1, &bodyUtf8[0], nUtf8Len, NULL, NULL);
		if (!bodyUtf8.empty() && bodyUtf8.back() == '\0')
			bodyUtf8.pop_back();

		BOOL bSent = WinHttpSendRequest(hReq,
			L"Content-Type: application/json\r\nCharset: UTF-8\r\n",
			(DWORD)-1L,
			(LPVOID)bodyUtf8.c_str(),
			(DWORD)bodyUtf8.size(),
			(DWORD)bodyUtf8.size(),
			0);
		if (bSent)
		{
			bOK = WinHttpReceiveResponse(hReq, NULL);
			if (bOK) InterlockedIncrement64(&m_lMsgLogSuccessCount);
		}
	}

cleanup:
	if (hReq)     WinHttpCloseHandle(hReq);
	if (hConn)    WinHttpCloseHandle(hConn);
	if (hSession) WinHttpCloseHandle(hSession);
	return bOK;
}


BOOL CWLServerTestDlg::OnEraseBkgnd(CDC* pDC)
{
        CRect rc;
        GetClientRect(&rc);
        pDC->FillSolidRect(&rc, RGB(242, 245, 250));

        // Draw colored zone backgrounds + 1px borders using DLU->pixel via MapDialogRect
        struct { int x1,y1,x2,y2; COLORREF bg; COLORREF border; } zones[] = {
            { 8, 285, 298, 310, RGB(235,248,253), RGB(123,176,208) },  // Plug zone
            { 8, 310, 298, 356, RGB(240,244,248), RGB(176,196,222) },  // Ext  zone
            { 8, 356, 298, 392, RGB(255,253,240), RGB(200,160, 32) },  // Threat zone
        };
        for (auto& z : zones) {
            RECT dlu = { z.x1, z.y1, z.x2, z.y2 };
            MapDialogRect(&dlu);
            pDC->FillSolidRect(&dlu, z.bg);
            pDC->Draw3dRect(&dlu, z.border, z.border);
        }
        return TRUE;
}

void CWLServerTestDlg::OnDestroy()
{
	CDialog::OnDestroy();
	if (m_hBrushDlg)    { ::DeleteObject(m_hBrushDlg);    m_hBrushDlg    = NULL; }
	if (m_hBrushWhite)  { ::DeleteObject(m_hBrushWhite);  m_hBrushWhite  = NULL; }
	if (m_hBrushPlug)   { ::DeleteObject(m_hBrushPlug);   m_hBrushPlug   = NULL; }
	if (m_hBrushExt)    { ::DeleteObject(m_hBrushExt);    m_hBrushExt    = NULL; }
	if (m_hBrushThreat) { ::DeleteObject(m_hBrushThreat); m_hBrushThreat = NULL; }
}


HBRUSH CWLServerTestDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_BTN && pWnd)
	{
		UINT nID = pWnd->GetDlgCtrlID();
		static CBrush brBlue(RGB(70,130,180)), brGreen(RGB(60,179,113)),
		             brOrange(RGB(255,140,0)), brRed(RGB(220,80,80)),
		             brPurple(RGB(147,112,219));
		switch (nID)
		{
		case IDC_BUTTON_REG_REG:
		case IDC_BUTTON_REG_RESET:
			pDC->SetTextColor(RGB(255,255,255));
			pDC->SetBkColor(RGB(70,130,180));
			return (HBRUSH)brBlue;
		case IDC_BUTTON_HB_START:
			pDC->SetTextColor(RGB(255,255,255));
			pDC->SetBkColor(RGB(60,179,113));
			return (HBRUSH)brGreen;
		case IDC_BUTTON_LOG_ADD:
			pDC->SetTextColor(RGB(255,255,255));
			pDC->SetBkColor(RGB(255,140,0));
			return (HBRUSH)brOrange;
		case IDC_BUTTON_STOP_TASK:
		case IDC_BUTTON_HB_STOP:
		case IDC_BUTTON_LOG_STOP:
		case IDC_BUTTON_WL_STOP2:
			pDC->SetTextColor(RGB(255,255,255));
			pDC->SetBkColor(RGB(220,80,80));
			return (HBRUSH)brRed;
		case IDC_BUTTON_WL_UPLOAD:
		case IDC_BUTTON_WL_PREVIEW:
			pDC->SetTextColor(RGB(255,255,255));
			pDC->SetBkColor(RGB(147,112,219));
			return (HBRUSH)brPurple;
		}
	}
	return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CWLServerTestDlg::OnBnClickedRawPacket()
{
	CRawPacketDlg dlg(this);
	dlg.DoModal();
}
