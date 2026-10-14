?// WLServerTestDlg.cpp : ??




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
#include "ClientConfigUploader.h"




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









int g_nTotalRegistered_ClientCount = 0;		              //?????









int g_nHeartBeatSending_ClientCount = 0;









volatile long g_nHeartBeatNotSending_ClientCount = 0;     //?ï¿½ï¿½??




volatile long g_nFileLogNotSending_ClientCount = 0;       //?ï¿½ï¿½????









volatile long g_nMsgLogNotSending_ClientCount = 0;        //?ï¿½ï¿½????   volatile long









volatile int g_iThreadCount_RegSameTime = 0;        //









CString  g_strDefaultPrefix			= _T("WLClient_");




CString  g_strDefaultStartNum		= _T("1");




CString  g_strDefaultStartIP		= _T("6.6.6.6");














CString g_cstrIPAd;		              //?????IP?









vector<client> g_vecAllClientObjects; //ï¿½ï¿½??????














CCriticalSection g_csApplogCount;	    // ???




CCriticalSection g_csApplogListCtrl;	// ?ï¿½ï¿½?




CCriticalSection g_csVecClients;        // g_*???









CSingleLock g_singleLockGManage(&g_csVecClients);     // ??




CSingleLock g_singleLockApplogCount(&g_csApplogCount);// ??




CSingleLock g_singleLockListCtrl(&g_csApplogListCtrl);// ?	









BOOL g_bStopTask = FALSE;
BOOL g_bStopLogTask = FALSE; // r3: log-only stop flag
volatile LONG g_nActiveHbThreads = 0; // v16: active HB thread counter for safe Reset

// ---- cfg upload retry queue (t6) ----
struct CfgUploadRetryItem
{
    CString    strComputerID;
    CString    strClientIP;
    int        nRetryCount;          // attempted retry count
    ULONGLONG  ullNextRetryTick;     // next retry time (GetTickCount64)
};

struct CfgManualUploadThreadArg
{
    std::vector<std::pair<CString, CString>> vecTargets;
    std::string strCfgJson;
    CString strServerIP;
    CString strServerPort;
    CString strIdPre;
    HWND hWnd;
    CWLServerTestDlg* pDlg;
};
std::vector<CfgUploadRetryItem> g_vecCfgRetryQueue;
CCriticalSection g_csCfgRetryQueue;   // guards g_vecCfgRetryQueue
int g_nCfgRetryMaxAttempts = 4;           // max retry attempts (INI overridable)
const int g_nCfgRetryQueueLimit  = 1024;  // queue capacity
int g_nCfgRetryBaseDelayMs = 1000;        // backoff base ms (INI overridable)
volatile LONG64 g_nCfgUploadOk64    = 0;  // final upload ok count
volatile LONG64 g_nCfgUploadFail64  = 0;  // final upload fail count
volatile LONG64 g_nCfgUploadRetry64 = 0;  // total retry attempts count









int g_nSocketCount = 0;




SOCKET g_sock[1000] = {INVALID_SOCKET};









BOOL g_bSamePath = FALSE;









// ?ï¿½ï¿½???? CAboutDlg ?









class CAboutDlg : public CDialog




{




public:




	CAboutDlg();









// ?




	enum { IDD = IDD_ABOUTBOX };









	protected:




	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ?









// ?




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
namespace {
	void DebugTrace(const char* szMsg) {
		HANDLE hFile = CreateFileA("WLTest_Debug.txt", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			DWORD dwWritten = 0;
			SetFilePointer(hFile, 0, NULL, FILE_END);
			WriteFile(hFile, szMsg, (DWORD)strlen(szMsg), &dwWritten, NULL);
			const char szNewLine[2] = { 13, 10 };
			WriteFile(hFile, szNewLine, 2, &dwWritten, NULL);
			CloseHandle(hFile);
		}
	}
}





  









// CWLServerTestDlg ?









// WLHeartBeat.h?ï¿½ï¿½?




#define HEARTBEAT_CMD_NOREGISTER	18		//TCP??ï¿½ï¿½?




#define HEARTBEAT_CMD_POLCY			17		//???




#define HEARTBEAT_CMD_BACK			1		//?









CWLServerTestDlg* g_WLServerTestDlg = NULL;









CWLServerTestDlg::CWLServerTestDlg(CWnd* pParent): CDialog(CWLServerTestDlg::IDD, pParent), m_lMsgLogSuccessCount(0),m_lUploadState_SuccessCount(0), m_lFileLog_WL_SuccessCount(0),
  m_hBrushDlg(NULL), m_hBrushWhite(NULL), m_hBrushPlug(NULL), m_hBrushExt(NULL), m_hBrushThreat(NULL), m_nHoverBtnID(0), m_bMouseInDlg(false), m_nLinuxHbIntervalMs(30000) //?Ini??????ï¿½ï¿½Ini




{  




	// ip




	m_strServerIP = CProfileConfig::GetProfileConfigInstance()->ReadServerIP_FromIni();




	if(m_strServerIP.GetLength() == 0)




	{




		m_strServerIP = _T("192.168.7.254");




		




		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);




	}




 




	// ?




	m_strServerPort = CProfileConfig::GetProfileConfigInstance()->ReadServerPort_FromIni();




	if(m_strServerPort.GetLength() == 0)




	{




		m_strServerPort = _T("8441");









		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);




	}









	// ?




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




    



    



    








    DDX_Control(pDX, IDC_STATIC_FILELOG, m_staticFilelog);  // ??(?/):




    DDX_Control(pDX, IDC_WL_FILE_PATH_EDIT, m_WLFilePathEdit);//???




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
    DDX_Control(pDX, IDC_CHECK_DEBUG,          m_chkDebug);



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
	DDX_Control(pDX, IDC_CAT_USB,               m_catUsb);



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
    DDX_Control(pDX, IDC_CHECK_AUTO_UPLOAD_CFG,   m_chkAutoUploadCfg);
    DDX_Control(pDX, IDC_STATIC_CFG_UPLOAD_STAT,  m_stCfgUploadStat);



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
	ON_BN_CLICKED(IDC_WL_FILE_CHOOSE_BUTTON, &CWLServerTestDlg::OnBnClickedWlFileChooseButton)




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




ON_BN_CLICKED(IDC_BUTTON_UPLOAD_CFG, &CWLServerTestDlg::OnBnClickedUploadCfg)




ON_BN_CLICKED(IDC_CHECK_AUTO_UPLOAD_CFG, &CWLServerTestDlg::OnBnClickedChkAutoUploadCfg)



ON_BN_CLICKED(IDC_BUTTON_WL_STOP2, &CWLServerTestDlg::OnBnClickedWlStop2)



ON_BN_CLICKED(IDC_BUTTON_WL_PREVIEW, &CWLServerTestDlg::OnBnClickedWlPreview)



ON_BN_CLICKED(IDC_BUTTON_VER_MGMT, &CWLServerTestDlg::OnBnClickedVerMgmt)



ON_BN_CLICKED(IDC_BUTTON_LOG_HELP, &CWLServerTestDlg::OnBnClickedLogHelp)







ON_WM_TIMER()



ON_MESSAGE(WM_APP+100, &CWLServerTestDlg::OnAppendLogOutput)




ON_CBN_SELCHANGE(IDC_COMBO_PROJECT_TYPE, &CWLServerTestDlg::OnProjectTypeSelChange)

ON_BN_CLICKED(IDC_BUTTON_RAWPACKET, &CWLServerTestDlg::OnBnClickedRawPacket)
	ON_BN_CLICKED(IDC_BTN_UNSELECT_ALL, &CWLServerTestDlg::OnBnClickedUnselectAll)
	ON_BN_CLICKED(IDC_CHECK_DEBUG, &CWLServerTestDlg::OnBnClickedDebug)
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_MESSAGE(WM_APP+101, &CWLServerTestDlg::OnCfgUploadStatUpdate)
END_MESSAGE_MAP()














 




int Check_FirstIP_TotalRegCount_Overflow(int IPAddrClips[4],int iRegCount)//?1?0??IP?2




{




	//??IP?X.0.0.0,?




	if (IPAddrClips[1] == 0 && IPAddrClips[2] == 0  && IPAddrClips[3] == 0)




	{




		return 0;




	}









	//IP?ï¿½ï¿½?




	int maxIPAddrClips[4] = {0};




	maxIPAddrClips[3] = (IPAddrClips[3] + iRegCount) % 255;




	maxIPAddrClips[2] = (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) % 255;




	maxIPAddrClips[1] = (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) % 255;




	maxIPAddrClips[0] = IPAddrClips[0] 




	+ (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) / 255;









	if (maxIPAddrClips[0] >= 255)//???IP254.254.254.254?




	{




		return 2;




	}









	return 1;




}









void CWLServerTestDlg::PrepareVecClients_UpdateControls()




{









	USES_CONVERSION;




	char* pstr_Tmp_PreviousRegistered_StartIP = T2A(m_strPreviousRegistered_StartIP); 




	int nFirstIP_FromIni[4] = {0};						//?Ini?FirstClientIP--4int




	sscanf(pstr_Tmp_PreviousRegistered_StartIP, "%d.%d.%d.%d", &nFirstIP_FromIni[0],&nFirstIP_FromIni[1],&nFirstIP_FromIni[2],&nFirstIP_FromIni[3]);









	int iTmpRet = Check_FirstIP_TotalRegCount_Overflow(nFirstIP_FromIni,m_iPreviousRegistered_ClientCount);




	if (0 == iTmpRet)




	{




		AfxMessageBox(_T("ï¿½ï¿½Ê¼IPï¿½ï¿½Ö·ï¿½ï¿½Ê½ï¿½ï¿½ï¿½ï¿½È·"));




		return;




	}




	else if(2 == iTmpRet)




	{




		AfxMessageBox(_T("×¢ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½IPï¿½ï¿½Ö·ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Î§"));




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




		szClientID_PrefixNum.Append(sThisClientIndex);//???+ClientNum









		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;




		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;




		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;




		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;




		szThisClientIP=_T("");




		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);









		client objTmpClient(szClientID_PrefixNum,szThisClientIP);




		objTmpClient.Client_SetComputerID();
		// v6.5: restore server-assigned devid from INI
		DWORD dwSavedDevID = CProfileConfig::GetProfileConfigInstance()->ReadDevID_FromIni(objTmpClient.GetClientID());
		if (dwSavedDevID != 0) objTmpClient.SetDevID(dwSavedDevID);









		g_nHeartBeatNotSending_ClientCount++; //??ClientCount. ???ï¿½ï¿½




		g_nMsgLogNotSending_ClientCount++;




		g_nFileLogNotSending_ClientCount++;









		g_vecAllClientObjects.push_back(objTmpClient); 




	}




	




	CString strPreRegCount;




	strPreRegCount.Format(_T("ï¿½ï¿½×¢ï¿½ï¿½: %d"), m_iPreviousRegistered_ClientCount);




	m_cRegisteredClientCounts.SetWindowText(strPreRegCount);




	  




	{//Client???




		




		//??Client???????????StartNum   IP  




		sThisClientIndex=_T("");




		sThisClientIndex.Format(_T("%04d"),iClientStartNum + i);









		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;




		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;




		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;




		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;




		szThisClientIP=_T("");




		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);




		




		//2.?StartIP (2.2.2.2) ? SetWindowsText




		//m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(szThisClientIP); // v5: ????ï¿½ï¿½ï¿½ï¿½??IP




			 




		//3.SetWindowsText IDPrefix




		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);









		//4.?StartNum (200+10) ? SetWindowsText 




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(sThisClientIndex);




	}














	//?  ? ?????  ?




};









BOOL CWLServerTestDlg::OnInitDialog()




{




	CDialog::OnInitDialog();









	// ...?????ï¿½ï¿½









	// IDM_ABOUTBOX ???ï¿½ï¿½??




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









	// ??????ï¿½ï¿½???????ï¿½ï¿½??




	SetIcon(m_hIcon, TRUE);			// ??




	SetIcon(m_hIcon, FALSE);		// ï¿½ï¿½?









	// TODO: ?????




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




	    strHeartBeat_TitleLine.Format(_T("ÐòºÅ,50;¿ªÊ¼Ê±¼ä,136;¿Í»§¶ËÊý,80;ÐÄÌø/·¢ËÍ¼ä¸ô(ms),100;ÐÄÌøÊ±³¤(min),0;»îÔ¾¿Í»§¶Ë/ÏûÏ¢ÈÕÖ¾,130;×´Ì¬,90;¸üÐÂÊ±¼ä/ÎÄ¼þÈÕÖ¾,136;Ê§°Ü¿Í»§¶ËÊý,150"));




	  strLog_MSGFILE_TitleLine.Format(_T("ÐòºÅ,50;¿ªÊ¼Ê±¼ä,150;¿Í»§¶ËÊý,80; ·¢ËÍ¼ä¸ô(ms),100;·¢ËÍÊ±³¤(min),0;ÏûÏ¢ÈÕÖ¾,130;ÎÄ¼þÈÕÖ¾,100;×´Ì¬,100"));









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




	m_comHB_TotalMinutes.AddString(_T("14400")); // r8: 10 ï¿½ì£¬ï¿½ï¿½Îªï¿½ï¿½UIÄ¬ï¿½ï¿½




	m_comHB_TotalMinutes.SetCurSel(m_comHB_TotalMinutes.GetCount() - 1); // r8: Ä¬ï¿½ï¿½Ñ¡ 14400 ï¿½ï¿½ï¿½ï¿½ (10 ï¿½ï¿½)

	// v5.2: ï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ä?ï¿½ï¿½ 7200 ï¿½ï¿½ï¿½ï¿½
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




	









	/*??



	SCROLLINFO ScrInfo;



	GetScrollInfo(SB_VERT, &ScrInfo, SIF_ALL);



	ScrInfo.nPage = 10; //??



	ScrInfo.nMax = 100; //ï¿½ï¿½ï¿½ï¿½?0C100



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









	//1.?????




	m_iPreviousRegistered_ClientCount = CProfileConfig::GetProfileConfigInstance()->ReadTotalClientCount_FromIni();//?ï¿½ï¿½???-1




	if (m_iPreviousRegistered_ClientCount > 0)




	{




		




		g_nTotalRegistered_ClientCount = m_iPreviousRegistered_ClientCount;//lzq:Dlg?????????Ini-->g_nTotalRegistered_ClientCount




	




		//2.?StartIP




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




		//3.?IDPrefix




		m_strEditCtrl_ToRegisterClient_ClientIDPrefix = CProfileConfig::GetProfileConfigInstance()->ReadEachClientIdPrefix_FromIni();




		if(0 != m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())




		{




			




		}




		else




		{




			m_strEditCtrl_ToRegisterClient_ClientIDPrefix = g_strDefaultPrefix;









			CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);	




		}




		//4.?StartNum




		m_strPreviousRegistered_StartNum = CProfileConfig::GetProfileConfigInstance()->ReadClientStartNum_FromIni();




		if( 0 != m_strPreviousRegistered_StartNum.GetLength())




		{




			




		}




		else




		{




			m_strPreviousRegistered_StartNum = g_strDefaultStartNum;









			CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);




		}









		PrepareVecClients_UpdateControls();//lzq:Dlg????????????Client?????????? FileLog MsgLog(??ï¿½ï¿½?)




	}




	else




	{




		m_iPreviousRegistered_ClientCount = 0;




		   g_nTotalRegistered_ClientCount = 0;









		CString strTmp;




		strTmp.Format(_T("ï¿½ï¿½×¢ï¿½ï¿½: %d"),g_nTotalRegistered_ClientCount);




		m_cRegisteredClientCounts.SetWindowText(strTmp);














		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);









		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);




 




 




		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(m_iPreviousRegistered_ClientCount);   //???????????-1ï¿½ï¿½0PreviousRegisteredClientCount=0
		CProfileConfig::GetProfileConfigInstance()->ClearAllDevIDs_FromIni();  // v6.5




	}













	// === Phase 4: New UI initialization ===



	// ProjectType Combo



	m_comboProjectType.AddString(_T("IEG"));



	m_comboProjectType.AddString(_T("EDR"));



	m_comboProjectType.SetCurSel(0);







	ApplyProjectTypeSelection(); // ??? IEG ??

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









	// ???????????????????????????????ï¿½ï¿½RC ???ï¿½ï¿½?????
// t7: load cfg-upload settings (auto-upload switch + retry params) from INI
LoadCfgUploadSettingsFromIni();

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

	return TRUE;  // ???? TRUE




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









//  ?ï¿½ï¿½????




//  ??????/??? MFC ?ï¿½ï¿½




//  ????????ï¿½ï¿½









void CWLServerTestDlg::OnPaint()




{




	if (IsIconic())




	{




		CPaintDC dc(this); // ????????









		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);









		// ???ï¿½ï¿½




		int cxIcon = GetSystemMetrics(SM_CXICON);




		int cyIcon = GetSystemMetrics(SM_CYICON);




		CRect rect;




		GetClientRect(&rect);




		int x = (rect.Width() - cxIcon + 1) / 2;




		int y = (rect.Height() - cyIcon + 1) / 2;









		// ?




		dc.DrawIcon(x, y, m_hIcon);




	}




	else




	{




		CDialog::OnPaint();




	}




}









//??ï¿½ï¿½??????ï¿½ï¿½??




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









BOOL CWLServerTestDlg::RegisterClientToServer(CString szComputerID, CString szClientID,CString szClientIP, DWORD* pdwOutDevID)




{
	// v6: ?????? UI ??ï¿½ï¿½?ï¿½ï¿½?? OS
	CString strVer = _T("V300R011C01B090");
	int nSel = m_comboClientVersion.GetCurSel();
	if (nSel >= 0) m_comboClientVersion.GetLBText(nSel, strVer);
	BOOL bLinux = (m_radioOsLinux.GetCheck() == BST_CHECKED);
	CString strOS = bLinux ? _T("Linux centos7") : _T("Windows 10");





	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	return SendInfoToServer.RegisterClientToServer(szComputerID,szClientID,szClientIP,strVer,strOS, pdwOutDevID);




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









unsigned int ThreadFunc_HeartbeatSend_JustSend(PVOID JustSend) //??




{




    ULONGLONG ulHBTotal_MilSeconds = 600*60*1000;	




    ULONGLONG ulTime_First = GetTickCount();//???ï¿½ï¿½ulHBTotalMilSec




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
                cclient->Client_SetOnline(FALSE);  // t4: send fail -> offline




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
            g_WLServerTestDlg->OnClientOnlineEdgeDetected(*cclient);  // t4: heartbeat ack -> online edge




        }




        else if (HEARTBEAT_CMD_POLCY == dwRet)




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_POLCY!"));
            g_WLServerTestDlg->OnClientOnlineEdgeDetected(*cclient);  // t4: policy reply -> online edge




            dwRet = g_WLServerTestDlg->SendHeartbeat(*cclient);//??https,??ï¿½ï¿½????




            if (ERROR_SUCCESS != dwRet)




            {




                WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!"));	             




            }




        }




        else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//ï¿½ï¿½?




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_NOREGISTER!"));




            pSendInfo->CloseConnection(sock);





            DWORD dwDevID = 0;            if (g_WLServerTestDlg->RegisterClientToServer(cclient->Client_GetComputerID(),




                cclient->GetClientID(),




                cclient->GetClientIP(), &dwDevID))




            {




                cclient->Client_SetRegistered(TRUE);
                if (dwDevID != 0) { cclient->SetDevID(dwDevID); CProfileConfig::GetProfileConfigInstance()->WriteDevID_ToIni(cclient->GetClientID(), dwDevID); }




                pSendInfo->CreateConnection(sock,g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




                g_WLServerTestDlg->SendHeartbeatToserver_TCP(*cclient,sock);




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Succ!"));
                g_WLServerTestDlg->OnClientOnlineEdgeDetected(*cclient, TRUE);  // t4: register ok -> online edge (force trigger for optional callback)




            }




            else




            {




                cclient->Client_SetRegistered(FALSE);




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Fail!"));
                cclient->Client_SetOnline(FALSE);  // t4: re-register fail -> offline




            }




        }




        else




        {




            WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns neither!"));
            cclient->Client_SetOnline(FALSE);  // t4: unexpected reply -> offline




        }









        if (g_bStopTask) break; // v9: ????????
        if (g_bStopTask) break; // v9: ????????
        { for (int _si=0;_si<300&&!g_bStopTask;_si++) Sleep(100); } //v13: cancellable 30s









        ulTime_Second = GetTickCount();




        ulTime_During = ulTime_Second - ulTime_First;









        if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:??




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

        DWORD dwDevID = 0;




        if (g_WLServerTestDlg->RegisterClientToServer(pClient->Client_GetComputerID(),pClient->GetClientID(),pClient->GetClientIP(), &dwDevID))




        {




            WriteInfo(_T("RegisterClientToServer Success, ClientID = %s"), pClient->GetClientID().GetBuffer());




            




            ::InterlockedIncrement((LONG*)&g_nTotalRegistered_ClientCount); // v5.3: ï¿½ï¿½ï¿½ï¿½Ô­ï¿½Óµï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ß³ï¿½×¢ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½









            g_nHeartBeatNotSending_ClientCount++;









            g_nMsgLogNotSending_ClientCount++;




            g_nFileLogNotSending_ClientCount++;









            pClient->Client_SetRegistered(TRUE);
            if (dwDevID != 0) { pClient->SetDevID(dwDevID); CProfileConfig::GetProfileConfigInstance()->WriteDevID_ToIni(pClient->GetClientID(), dwDevID); }




            




            CString strTmpRegisteredClientCount;




            




            g_singleLockGManage.Lock();




            




            strTmpRegisteredClientCount.Format(_T("ï¿½ï¿½×¢ï¿½ï¿½: %d"), g_nTotalRegistered_ClientCount);




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




    




    if (FALSE)




    {




        //???




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




 




void CWLServerTestDlg::OnBnClicked_RegisterClients() //lzq:????




{









	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½Ï?"));




		return;




	}




	if (0 == m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())




	{




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Í»ï¿½ï¿½ï¿½IDÇ°×º"));




		return;




	}









	CString strRegClientCount;




	m_comRegButton_ToRegisterClient_ClientCount.GetWindowText(strRegClientCount);









	int iNeedToRegClientCount = _ttoi(strRegClientCount);




	if (iNeedToRegClientCount <= 0)




	{




		AfxMessageBox(_T("×¢ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ð?"));




		return;




	}









	CString strFirstClientIPToRegister;




	m_ctrlBox_ToRegisterClient_FirstClientIP.GetWindowText(strFirstClientIPToRegister);




	if (0 == strFirstClientIPToRegister.GetLength())




	{




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê¼IPï¿½ï¿½Ö·"));




		return;




	}









	USES_CONVERSION;




	char* pcFirstClientIPToRegister = T2A(strFirstClientIPToRegister); 




	int IPAddrClips_FourInt[4] = {0};




	sscanf(pcFirstClientIPToRegister, "%d.%d.%d.%d", &IPAddrClips_FourInt[0],&IPAddrClips_FourInt[1],&IPAddrClips_FourInt[2],&IPAddrClips_FourInt[3]);









	if (0 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))




	{




		AfxMessageBox(_T("ï¿½ï¿½Ê¼IPï¿½ï¿½Ö·ï¿½ï¿½Ê½ï¿½ï¿½ï¿½ï¿½È·"));




		return;




	}




	else if(2 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))




	{




		AfxMessageBox(_T("×¢ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½IPï¿½ï¿½Ö·ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Î§"));




		return;




	}




	




	BOOL bRegisterSameTime = m_bRegisterSameTime.GetCheck();




	




	if (bRegisterSameTime)




	{




        if (FALSE /*removed*/)




        {




            CString strWhiteListFilePath = _T("");




            m_WLFilePathEdit.GetWindowText(strWhiteListFilePath);









            if(!PathFileExists(strWhiteListFilePath))




            {




                AfxMessageBox(_T("ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½Ä¼ï¿½Â·ï¿½ï¿½"));




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




	



			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(FALSE);




	




	//???? --> ? client




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




                ::InterlockedIncrement((LONG*)&g_nTotalRegistered_ClientCount); // v5.3: ï¿½ï¿½ï¿½ï¿½Ô­ï¿½Óµï¿½ï¿½ï¿½









                g_nHeartBeatNotSending_ClientCount++;









                g_nMsgLogNotSending_ClientCount++;




                g_nFileLogNotSending_ClientCount++;









                clientTemp.Client_SetRegistered(TRUE);




                g_vecAllClientObjects.push_back(clientTemp);




            }




            else




            {




                AfxMessageBox(_T("×¢ï¿½ï¿½Ê§ï¿½ï¿½"));




                GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




                GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




                



                GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);









                return;




            }




            




            CString strTmpRegisteredClientCount;




            strTmpRegisteredClientCount.Format(_T("ï¿½ï¿½×¢ï¿½ï¿½: %d"), g_nTotalRegistered_ClientCount);




            m_cRegisteredClientCounts.SetWindowText(strTmpRegisteredClientCount);




		}









		{




			// ???????




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









	{//??  ????????ï¿½ï¿½????









		//1.???




		CString cstrNextRegistration_StartNum;




		cstrNextRegistration_StartNum.Format(_T("%d"),iClientStartNum + iNeedToRegClientCount);//200+10




		//GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(cstrNextRegistration_StartNum); // v5: ????ï¿½ï¿½ï¿½ï¿½?????









		//2.?IP




		EachClient_UniqueIP[3] = (IPAddrClips_FourInt[3] + i) % 255;




		EachClient_UniqueIP[2] = (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) % 255;




		EachClient_UniqueIP[1] = (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) % 255;




		EachClient_UniqueIP[0] = IPAddrClips_FourInt[0]  + (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) / 255;




		CString cstrNextRegistration_FirstClientIP = _T("");




		cstrNextRegistration_FirstClientIP.Format(_T("%d.%d.%d.%d"),EachClient_UniqueIP[0],EachClient_UniqueIP[1],EachClient_UniqueIP[2],EachClient_UniqueIP[3]);




		




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(cstrNextRegistration_FirstClientIP);









	}




	




	{//????Ini









		// ? ?  ---"??client"?ï¿½ï¿½??????Info




		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(iNeedToRegClientCount);//10









		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);//WLClient_




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(strFirstClientIPToRegister);//6.6.6.6




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(m_strEditCtrl_ToRegisterClient_StartNum);//200  









		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);




		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);




		CProfileConfig::GetProfileConfigInstance()->WriteServerHBPort_ToIni(m_strServerPortHB);









	}




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);  //????




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




	



			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);




}









void CWLServerTestDlg::OnBnClicked_ClientReg_Reset()




{




	if (IDOK == AfxMessageBox(_T("È·ï¿½ï¿½ï¿½ï¿½Õµï¿½Ç°ï¿½ï¿½ï¿½Ð¿Í»ï¿½ï¿½Ëµï¿½ï¿½ï¿½Ö¾ï¿½ï¿½Â¼ï¿½ï¿½\nï¿½Ä¼ï¿½ï¿½ï¿½Ö¾/ï¿½ï¿½Ï¢ï¿½ï¿½Ö¾ï¿½ï¿½Í£Ö¹"), MB_OKCANCEL))




	{




		g_nTotalRegistered_ClientCount = 0;




		g_nHeartBeatNotSending_ClientCount = 0;




		g_nMsgLogNotSending_ClientCount = 0;




		g_nFileLogNotSending_ClientCount = 0;     









		CString strTmp;




		strTmp.Format(_T("ï¿½ï¿½×¢ï¿½ï¿½: %d"),g_nTotalRegistered_ClientCount);




		m_cRegisteredClientCounts.SetWindowText(strTmp);




 









		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(g_nTotalRegistered_ClientCount);




		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);




		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);









		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);




		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);




		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);




		




		//????
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




unsigned int UseHBPort_ThreatLog_CheckBox(int iCurUsedClient)//lzq:???ï¿½ï¿½?,?




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









const DWORD HBClientCount_InEveryThread = 10;  // ???10




unsigned int ThreadFunc_HeartbeatSend_Ori(void* pArgument) //lzq:??????10Client




{




	int iThisTaskLineIndex = *(int*)pArgument;//iIndex ??????-->Task




	delete pArgument;









	




	CString csHBIntervalMilSec = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 3); //lzq:ms




	 CString strHBTotalMinutes = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 4); //lzq:?









	int iHBIntervalMilSec = _ttoi(csHBIntervalMilSec); //lzq:ï¿½ï¿½??




	int iHBTotalMinutes = _ttoi(strHBTotalMinutes);









	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(iHBTotalMinutes)*60*1000;	




	ULONGLONG ulTime_First = GetTickCount();//???ï¿½ï¿½ulHBTotalMilSec




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




	for (int i=0; i<g_nTotalRegistered_ClientCount; i++)                             //????,?ï¿½ï¿½??




	{




		if (!(g_vecAllClientObjects[i].Get_IsThisClientSendingHeartBeat()))  //ï¿½ï¿½??ThisClient_HasSentHBï¿½ï¿½??




		{




			ThisHBTask_ToUseClientsArray[dwThisTask_ClientIndex] = g_vecAllClientObjects[i];




			vecIndex.push_back(i);









			g_vecAllClientObjects[i].Set_IsSendingHeartBeat(TRUE);//Client??




			




			g_nHeartBeatNotSending_ClientCount--;









			dwThisTask_ClientIndex++;









			if (dwThisTask_ClientIndex >= HBClientCount_InEveryThread)//?? ?ClientdwThisTask_ClientIndex == 10break




			{




				break;




			}




		}




	}//??ï¿½ï¿½??10









	CString strOnlineClientCount;




	strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




	g_singleLockGManage.Unlock();//




	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//???Client;;??10



















					 SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};




	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};




	




	//Client




	for(int i=0; i<dwThisTask_ClientIndex; i++)//lzq:?? ?




	{




		sSendInfo[i] = new CSendInfoToServer();




		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);//?sock <------>IP:HBPort




	}




	














	do 




	{




		for(int i=0; i<dwThisTask_ClientIndex; i++)//dwThisTask_ClientIndex==10 ???




		{




			if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(ThisHBTask_ToUseClientsArray[i],sock[i]))//???client  --  TmpSock




			{// ???




				




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




		




        Sleep(30 * 1000);//?30









		    ulTime_Second = GetTickCount();




		if (ulTime_Second - ulTime_First > ulHBTotal_MilSeconds)//lzq:??




		{




			break;




		}









	} while ( 1 );














	//Client?




	for(int i=0; i<dwThisTask_ClientIndex; i++)




	{




		sSendInfo[i]->CloseConnection(sock[i]);




		delete sSendInfo[i];




		sSendInfo[i]=NULL;




	}









	//  ??Total???




	




	int iNum = vecIndex.size();




	int k=0;




	g_singleLockGManage.Lock();




	




	for (k = 0;k < iNum; k++)




	{




		int iIdnex = vecIndex[k];









		g_vecAllClientObjects[iIdnex].Set_IsSendingHeartBeat(FALSE);









		g_nHeartBeatNotSending_ClientCount ++;// ?--




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









	//?? "???"    -1   ??




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
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(iThisTaskLineIndex, 6, _T("ï¿½ï¿½Í£Ö¹"));
	}









	_endthreadex( 0 );









	return 0;




}









std::set<int>  g_setOfflineClient;




unsigned int ThreadFunc_HeartbeatSend_New(PHB_SENDER_THREAD_ARG pHeapArgs) //lzq:??????10Client




{




	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(pHeapArgs->iHBTotalMinutes)*60*1000;	




	ULONGLONG ulTime_First = GetTickCount();//???ï¿½ï¿½ulHBTotalMilSec




	ULONGLONG ulTime_Second = 0; //added by lzq;




	ULONGLONG ulTime_During = 0; //added by lzq;









	SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};




	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};









	int iThisThreadClientCount = pHeapArgs->iClientCount;




	




	//Client




	for(int i=0; i < iThisThreadClientCount; i++)//lzq:?? ?




	{




		sSendInfo[i] = new CSendInfoToServer();




		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




		g_sock[g_nSocketCount++] = sock[i];




		WriteInfo(_T("ST:HB CreateConnection success!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




	}




	




	do 




	{  
		if (g_bStopTask) break; // v16: exit main HB loop on stop signal




		for(int i=0; i < iThisThreadClientCount; i++)//dwThisTask_ClientIndex==10 ???




		{




		    if (FALSE == g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_IsRegistered())




		    {





                DWORD dwDevID = 0;                if (g_WLServerTestDlg->RegisterClientToServer(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_GetComputerID(),




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(),




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientIP(), &dwDevID))




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(TRUE);
                    if (dwDevID != 0) { g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].SetDevID(dwDevID); CProfileConfig::GetProfileConfigInstance()->WriteDevID_ToIni(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(), dwDevID); }




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
	            g_WLServerTestDlg->OnClientOnlineEdgeDetected(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]]);




	        }




	        else if (HEARTBEAT_CMD_POLCY == dwRet)




	        {




				WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_POLCY!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);
	            g_WLServerTestDlg->OnClientOnlineEdgeDetected(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]]);




	            dwRet = g_WLServerTestDlg->SendHeartbeat(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]]);//??https,??ï¿½ï¿½????




	            if (ERROR_SUCCESS != dwRet)




	            {




					WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);	             




	            }




	        }




            else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//ï¿½ï¿½?




            {




                WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP returns HEARTBEAT_CMD_NOREGISTER!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);




                sSendInfo[i]->CloseConnection(sock[i]);





                DWORD dwDevID = 0;                if (g_WLServerTestDlg->RegisterClientToServer(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_GetComputerID(),




                                        g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(),




                                        g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientIP(), &dwDevID))




                {




                    g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Client_SetRegistered(TRUE);
                    if (dwDevID != 0) { g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].SetDevID(dwDevID); CProfileConfig::GetProfileConfigInstance()->WriteDevID_ToIni(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].GetClientID(), dwDevID); }




                    sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);




                    g_WLServerTestDlg->SendHeartbeatToserver_TCP(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]],sock[i]);




                    WriteInfo(_T("ST:HB RecvHeartBeatBack_TCP Re RegisterClientToServer Succ!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);
	                g_WLServerTestDlg->OnClientOnlineEdgeDetected(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]], TRUE);




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









		if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:??




		{




			WriteInfo(_T("ST:HB Actual during time:%lld;Interval time %lld"),ulTime_During,ulHBTotal_MilSeconds);




			break;




		}









	} while ( 1 );









	//Client?




	for(int i=0; i < iThisThreadClientCount; i++)




	{




		sSendInfo[i]->CloseConnection(sock[i]);




		delete sSendInfo[i];




		sSendInfo[i]=NULL;









		




		//  ??Total???




		g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Set_IsSendingHeartBeat(FALSE);




		InterlockedIncrement(&g_nHeartBeatNotSending_ClientCount);




	}
	InterlockedDecrement(&g_nActiveHbThreads); // v16: all vector accesses done









	//???




	CString strOnlineClientCount;




	g_singleLockGManage.Lock();




	//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




	g_nHeartBeatSending_ClientCount -= iThisThreadClientCount;




	strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);




	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);




	g_singleLockGManage.Unlock();














	//List?




	CSingleLock singleLockListCtrl(&g_WLServerTestDlg->m_csHeatbeatListCtrl);	




	singleLockListCtrl.Lock();




	CString strHBActiveClientCount = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);




	int iHBActiveClientCount = _ttoi(strHBActiveClientCount);




	




	iHBActiveClientCount -= iThisThreadClientCount;









	strHBActiveClientCount.Format(_T("%d"), iHBActiveClientCount);




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strHBActiveClientCount);
	
	if (iHBActiveClientCount > 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("Ö´ï¿½ï¿½ï¿½ï¿½"));
	}

	singleLockListCtrl.Unlock();




	if (iHBActiveClientCount <= 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("ï¿½ï¿½Í£Ö¹"));
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














// ?????




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




		g_singleLockApplogCount.Unlock();//ï¿½ï¿½?ï¿½ï¿½









        // USM??ï¿½I???????????????




	}	




	// reset









	




	InterlockedIncrement(&g_nFileLogNotSending_ClientCount);









	// ???????




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
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("ï¿½ï¿½Í£Ö¹"));
	}









	delete pHeapArgs;









	_endthreadex( 0 );









	return 0;




}









unsigned int ThreadFunc_MsgLogSend(PLOG_SENDER_THREAD_ARG pHeapArgs)   //??? ???




{	




	TCHAR szThisThread_Selected_ComputerID[MAX_PATH] = {0};




	int nSendCount = 0;









	//??ï¿½ï¿½??????




	//ULONGLONG seed = GetCurrentThreadId(); // ??ID




	//srand(seed);




	//pHeapArgs->iThisClient_VectorIndex = rand() % g_nTotalRegistered_ClientCount;




	int iThisThread_Selected_ClientIndex = pHeapArgs->iThisClient_VectorIndex;




	WriteInfo(_T("iThisClient_VectorIndex = %d"),pHeapArgs->iThisClient_VectorIndex);









	_tcscpy(szThisThread_Selected_ComputerID, g_vecAllClientObjects[iThisThread_Selected_ClientIndex].Client_GetComputerID());




	




	int iThisClient_CurrentSuccessCount = 0; //???	









	




	CSendInfoToServer SendInfoToServer_LogPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPort,  g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	CString strThisClient_IP = g_vecAllClientObjects[iThisThread_Selected_ClientIndex].GetClientIP();
	SendInfoToServer_LogPort.SetClientIP(strThisClient_IP);




	









	client& objForHBThreatLog = g_vecAllClientObjects[iThisThread_Selected_ClientIndex];




	CSendInfoToServer  SendInfoToServer_HBPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB,g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	SendInfoToServer_HBPort.m_WindowsOSVersion = (g_WLServerTestDlg->m_radioOsLinux.GetCheck() == BST_CHECKED) ? _T("Linux centos7") : _T("Windows 10");
	SendInfoToServer_HBPort.SetClientIP(strThisClient_IP);



	//SOCKET stTmpSock;




	//SendInfoToServer_HBPort.CreateConnection(stTmpSock,g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB);














	do //??? Sleep?




	{




		if (g_bStopLogTask)




		{




			g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(_T("0"));




			  g_WLServerTestDlg->m_StateUKey_BLine_SuccessNum_Left.SetWindowText(_T("0"));









			break;




		}




		//????




		{




			BOOL bRet = FALSE; 









			//opt threat 




			if (g_bStopLogTask) break; // r6
					// Per-category HTTPS dispatch (aligned with C# SimulatorApp)
		DWORD dwSub = pHeapArgs->dwHttpsSubTypes;
		if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_OPT)
		{
			if (dwSub & 0x00000001) SendInfoToServer_LogPort.SendClientAdminLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000080) SendInfoToServer_LogPort.SendClientSysFileCheckLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000100) SendInfoToServer_LogPort.SendClientUsbWarningLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000200) SendInfoToServer_LogPort.SendClientFirewallLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000400) SendInfoToServer_LogPort.SendClientVulDefenseLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000002) SendInfoToServer_LogPort.SendClientOsResourceLogToServer(szThisThread_Selected_ComputerID);
			if (dwSub & 0x00000010) SendInfoToServer_LogPort.SendClientHostDefenceLogToServer(szThisThread_Selected_ComputerID, 2);
			if (dwSub & 0x00000020) SendInfoToServer_LogPort.SendClientHostDefenceLogToServer(szThisThread_Selected_ComputerID, 4);
			if (dwSub & 0x00000800) SendInfoToServer_LogPort.SendClientProcessAlertLogToServer(szThisThread_Selected_ComputerID, 2, 6);
			if (dwSub & 0x00000004) SendInfoToServer_LogPort.SendClientProcessAlertLogToServer(szThisThread_Selected_ComputerID, 1, 6);
			if ((dwSub & 0x00002000)) SendInfoToServer_LogPort.SendClientProcessAlertLogToServer(szThisThread_Selected_ComputerID, 3, 3);
			// Fallback for any remaining OPT bits (safety store, etc.)
			DWORD dwMapped = dwSub & 0x00000001 | dwSub & 0x00000080 | dwSub & 0x00000100 | dwSub & 0x00000200 | dwSub & 0x00000002 | dwSub & 0x00000010 | dwSub & 0x00000020 | dwSub & 0x00000800 | dwSub & 0x00000004 | (dwSub & 0x00002000);
			if (0 == dwMapped) SendInfoToServer_LogPort.SendClientAdminLogToServer(szThisThread_Selected_ComputerID);
		}





			




			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NWL)




			{ 




				 SendInfoToServer_LogPort.SendClientProcessAlertLogToServer(szThisThread_Selected_ComputerID, 1, 6);




			}




		




			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_THREAT)




			{//added by lzq




				//bRet = SendInfoToServer.SendClientThtLogToServer(szComputerID);




				//UseHBPort_ThreatLog_CheckBox(iThisThread_Selected_ClientIndex);//lzq:???? ?ï¿½ï¿½









				if (pHeapArgs->sock != INVALID_SOCKET)




				{




					// 70?  




					BOOL bHit = FALSE;




					




					if (70 <= nSendCount++)




					{




						bHit = TRUE;




						nSendCount = 0;




					}









					SendInfoToServer_HBPort.SendThreatLog_ToserverTCP(objForHBThreatLog,pHeapArgs->sock, pHeapArgs->dwHttpsSubTypes, bHit);




				}	




			}









			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_DATAPROTECT)




			{




			SendInfoToServer_LogPort.SendClientHostDefenceLogToServer(szThisThread_Selected_ComputerID, 1);




			}









			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_SYSPROTECT)




			{




				SendInfoToServer_LogPort.SendClientSysProtectLogToServer(szThisThread_Selected_ComputerID);




			}









			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BACKUP)




			{




				SendInfoToServer_LogPort.SendClientBackupLogToServer(szThisThread_Selected_ComputerID);




			}









			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_Virus)
			{
				SendInfoToServer_LogPort.SendClientVirusLogToServer(szThisThread_Selected_ComputerID);
			}

			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NETADAPTER)
			{
				BOOL bNetRet = SendInfoToServer_LogPort.SendClientNetAdapterLogToServer(szThisThread_Selected_ComputerID);
				if (!bNetRet) g_WLServerTestDlg->AppendLogOutput(_T("[NET] Send FAILED"));
			}

			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_EXTDEV)
			{
				BOOL bExtRet = SendInfoToServer_LogPort.SendClientExtDevLogToServer(szThisThread_Selected_ComputerID, pHeapArgs->dwExtDevSubTypeMask);
				if (!bExtRet) g_WLServerTestDlg->AppendLogOutput(_T("[EXT] Send FAILED"));
			}

			if (g_bStopLogTask) break; // r6
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




				InterlockedIncrement64(&(g_WLServerTestDlg->m_lMsgLogSuccessCount));//????









				CString strTmpMsgLogSuccessCount;




				strTmpMsgLogSuccessCount.Format(_T("%lld"), g_WLServerTestDlg->m_lMsgLogSuccessCount);




				g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(strTmpMsgLogSuccessCount);









			}









			//baseline  ukey 




			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BLINE)




			{




				bRet = SendInfoToServer_LogPort.SendBaseLineToServer(szThisThread_Selected_ComputerID);




			}




			if (g_bStopLogTask) break; // r6
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_UKEY)




			{




				//Ukey?




				VEC_ST_USERS_USM    vecUSMUsersSend;    /* ???ï¿½ï¿½ */




				ST_USER_JSON oneUser;




				time_t t = time(NULL); //??




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




				InterlockedIncrement64(&(g_WLServerTestDlg->m_lUploadState_SuccessCount));//????




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
				if (g_bStopLogTask) break;
				Sleep(50);
			}
		}









	}while ( 1 );









	InterlockedIncrement(&g_nMsgLogNotSending_ClientCount); 









	//?==??TotalCount??ï¿½Ú¡ï¿½??




	g_singleLockListCtrl.Lock();




	CString strActiveClientCount_InMainWindowLine = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);




	int iThisTaskOtherThreads_FromCtr_ActiveClientCount = _ttoi(strActiveClientCount_InMainWindowLine);




	iThisTaskOtherThreads_FromCtr_ActiveClientCount--;




	strActiveClientCount_InMainWindowLine.Format(_T("%d"), iThisTaskOtherThreads_FromCtr_ActiveClientCount);  




	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strActiveClientCount_InMainWindowLine);//???,???









	CString strFileLog_ActiveClientCount = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 7);




	g_singleLockListCtrl.Unlock();














	




	int iFileLog_ActiveClientCount = _ttoi(strFileLog_ActiveClientCount);









	if ((iThisTaskOtherThreads_FromCtr_ActiveClientCount <= 0) && (iFileLog_ActiveClientCount == 0))////??? ?==0   //modified by lzq JUNE21:<=   == 




	{




		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 7, _T(""));

		// r8: ï¿½ï¿½Ö¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½È»/ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½"×´Ì¬"ï¿½ï¿½ï¿½ï¿½Îª"ï¿½ï¿½Í£Ö¹"
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("ï¿½ï¿½Í£Ö¹"));




		if (g_bStopLogTask)




		{




			g_WLServerTestDlg->m_lMsgLogSuccessCount = 0;  




			g_WLServerTestDlg->m_lUploadState_SuccessCount = 0;









			g_bStopTask = FALSE;




		} 









		  



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




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½Ï?"));




		return;




	}









	CString strNeedHB_ClientCount;




	CString strInterval;  




	CString strTotalMinutes;














	   m_comHB_Interval.GetWindowText(strInterval);




	m_comHB_ClientCount.GetWindowText(strNeedHB_ClientCount);     //?




   strTotalMinutes = _T("9999");               // override to run indefinitely //?()









	int iNeedHB_ClientCount = _ttoi(strNeedHB_ClientCount);//lzq:clientcount




	      int iTotalMinutes = _ttoi(strTotalMinutes);//?









	if (iNeedHB_ClientCount > g_nHeartBeatNotSending_ClientCount)




	{




		AfxMessageBox(_T("ï¿½ï¿½×¢ï¿½ï¿½Í»ï¿½ï¿½Ë²ï¿½ï¿½ï¿½ï¿½Ø¸ï¿½×?ï¿½ï¿½"));




		return;




	}




	int iThisTask_ThreadCount = iNeedHB_ClientCount / HB_CLIENTCOUNT_PER_THREAD ;














	int iPreviousTaskItemCount = m_listHeartBeat_MainWindow.GetItemCount();//iTaskItemCount ?ï¿½ï¿½?









	CString strNewTaskOrdial;




	strNewTaskOrdial.Format(_T("%d"), iPreviousTaskItemCount+1);









	SYSTEMTIME tm;




	TCHAR szTimeNow[MAX_PATH] = {0};









	GetLocalTime(&tm);




	_sntprintf_s(szTimeNow, MAX_PATH, _TRUNCATE,_T("%04d-%02d-%02d %02d:%02d:%02d"),tm.wYear, tm.wMonth, tm.wDay,tm.wHour, tm.wMinute, tm.wSecond);









	//?AddTask?  ??




	int iNewTaskInsertIndex = m_listHeartBeat_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);//iPreviousTaskItemCount ?1 ?1




	




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 1, szTimeNow);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 2, strNeedHB_ClientCount);//?




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 3, strInterval);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 4, strTotalMinutes);




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 5, strNeedHB_ClientCount);//???




	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("ï¿½ï¿½Ê¼ï¿½ï¿½"));  









	int nTmp = m_listHeartBeat_MainWindow.GetItemCount()-1;




	m_listHeartBeat_MainWindow.EnsureVisible(nTmp,FALSE);









			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);




	/* removed */  
	g_bStopTask = FALSE; // v14: reset stop flag before launching HB threads




	









	int iStartIndex_FromVector = 0;




	int iThisThread_ClientIndex = 0;









	for (int i=0; i < iThisTask_ThreadCount; i++)//lzq:?/10 == ???




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









			//?????? ï¿½ï¿½memset 0 




			pThreadArgs ->iClientCount = HB_CLIENTCOUNT_PER_THREAD;




			pThreadArgs ->iHBIntervalMilSec = 30*1000;




			pThreadArgs ->iHBTotalMinutes = iTotalMinutes;




			pThreadArgs ->iThisTask_LineIndex = iNewTaskInsertIndex;




		}









		for(iStartIndex_FromVector;iStartIndex_FromVector<g_nTotalRegistered_ClientCount;iStartIndex_FromVector++)




		{	




			if (!(g_vecAllClientObjects[iStartIndex_FromVector].Get_IsThisClientSendingHeartBeat()))  //ï¿½ï¿½??ThisClient_HasSentHBï¿½ï¿½??




			{




				g_vecAllClientObjects[iStartIndex_FromVector].Set_IsSendingHeartBeat(TRUE);//Client??









				g_nHeartBeatNotSending_ClientCount--;









				pThreadArgs->iArrClientIndex[iThisThread_ClientIndex] = iStartIndex_FromVector;









				iThisThread_ClientIndex++;









				if (iThisThread_ClientIndex >= HB_CLIENTCOUNT_PER_THREAD)//?? ?ClientdwThisTask_ClientIndex == 10break




				{




					g_nHeartBeatSending_ClientCount += HB_CLIENTCOUNT_PER_THREAD;




					break;




				}




			}




		}




		CString strOnlineClientCount;




		//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);




		strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);




		g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//???Client;;??10




		// Fix10b: reset task clients to offline before HB thread starts,
		// so the first HB ack (online edge) triggers ONE auto config upload.
		for (int k = 0; k < iThisThread_ClientIndex; k++)
		{
			g_vecAllClientObjects[pThreadArgs->iArrClientIndex[k]].Client_SetOnline(FALSE);
		}

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









	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("Ö´ï¿½ï¿½ï¿½ï¿½"));//Ã¿ï¿½ß³ï¿½ Ö´









			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);




					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




	



}









void CWLServerTestDlg::OnBnClickedButton_Lowest_AddTask()




{




	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½Ï?"));




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
	int iOrigPerSecond = iThisTask_EachClient_PerSecondItems; // save before forced min
	if (iThisTask_EachClient_PerSecondItems <= 0) iThisTask_EachClient_PerSecondItems = 1; // v6: ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½??









	int iThisTask_TotalItems = iThisTask_ClientCount * iThisTask_EachClient_TotalItems;









	int iRemainRight = iThisTask_EachClient_PerSecondItems;









	if ((iThisTask_ClientCount > g_nMsgLogNotSending_ClientCount) || (iThisTask_ClientCount > g_nFileLogNotSending_ClientCount))




	{




		AfxMessageBox(_T("Î´×¢ï¿½ï¿½Í»ï¿½ï¿½Ë²ï¿½ï¿½Ü·ï¿½ï¿½ï¿½ï¿½ï¿½Ö?"));




		return;




	}









	// m_iThisTask_SelectedOperationType set by caller (OnBnClickedLogAdd) via CAT_* checkboxes









	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_OPT;




	}









	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_THREAT;




	}









	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_NWL;




	}









	if (FALSE /*removed*/)




	{




		m_WLFilePathEdit.GetWindowText(strWhiteListFilePath);




		m_iThisTask_SelectedOperationType |= CLIENT_FILELOG_WLFILE;









		if(!PathFileExists(strWhiteListFilePath))




		{




			AfxMessageBox(_T("ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½Ä¼ï¿½Â·ï¿½ï¿½"));




			return;




		}




	}









	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BLINE;




	}









	// Ukey?




	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_UKEY;




	}









	// ??




	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_DATAPROTECT;




	}









	// ???




	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_SYSPROTECT;




	}




	// ???




	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BACKUP;




	}









	// ?




	if (FALSE /*removed*/)




	{




		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_Virus;




	}









	// ?hash??




	if (FALSE)




	{




		g_bSamePath = TRUE;




	}




	else




	{




		g_bSamePath = FALSE;




	}









	if( 0 == m_iThisTask_SelectedOperationType )




	{




		AfxMessageBox(_T("ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½ï¿½Ö¾ï¿½ï¿½ï¿½ï¿½"));




		return;




	}









	//???????????




	



					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);




	



					GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(FALSE);









	int iPreviousTaskItemCount = m_listHeartBeat_MainWindow.GetItemCount();//?ï¿½ï¿½?




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









	int				iNew_Interval_MisSec   = 1000 / iThisTask_EachClient_PerSecondItems;//? ?




	char		cNew_TmpBuf[0x20] = {0};




	sprintf(cNew_TmpBuf, "%.02f", fNew_Interval_MisSec); 




	//csNew_Interval_MilSec  = cNew_TmpBuf;		//????MilSecond




	csNew_Interval_MilSec.Format(_T("%S"),cNew_TmpBuf );














	CString     csThisTask_LastingSeconds = _T("");




	float		iNew_LastingSeconds    = (float)iThisTask_EachClient_TotalItems / iThisTask_EachClient_PerSecondItems;//???




	char		cNew2_TmpBuf[0x20] = {0};




	sprintf(cNew2_TmpBuf, "%.02f", iNew_LastingSeconds); 




	//csThisTask_LastingSeconds  = cNew2_TmpBuf;  //????????




	csThisTask_LastingSeconds.Format(_T("%S"),cNew2_TmpBuf );














	




	//?AddTask?




	int iThisTask_InsertLineIndex = m_listHeartBeat_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);////iPreviousTaskItemCount ?2 ?2




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 1, szThisTask_StartTime);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 2, strThisTask_ClientCount);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 3, csNew_Interval_MilSec);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 4, csThisTask_LastingSeconds);




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, _T("ï¿½ï¿½Ê¼ï¿½ï¿½"));

	// Track log task rows for OnBnClickedLogStop
	if (m_iThisTask_SelectedOperationType & MSG_LOG_TYPE) m_setLogTaskRows.insert(iThisTask_InsertLineIndex);








	csNew_Interval_MilSec = _T("");









	int nTmp = m_listHeartBeat_MainWindow.GetItemCount()-1;




	m_listHeartBeat_MainWindow.EnsureVisible(nTmp,FALSE);




 




	{//Prepare args for each thread:Added by lzq:June07









		if ( (m_iThisTask_SelectedOperationType & MSG_LOG_TYPE) )




		{




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, strThisTask_ClientCount);//? == ???




			




			
    if (0 == m_iThisTask_SelectedOperationType && (iThisTask_ClientCount > 0 || iOrigPerSecond > 0))
    {
        AfxMessageBox(_T("No log type selected"));
        return;
    }

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




		{//?MsgLog




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, _T("0"));//0  ?int ï¿½ï¿½




		}









		if ( m_iThisTask_SelectedOperationType & FILE_LOG_TYPE )




		{




			




			//?




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, strThisTask_ClientCount);//? == ???









			CString csFileLog_TotalNum = _T("");




			m_WL_TatalNum_RightTotal.GetWindowText(csFileLog_TotalNum);




			int PreviousTasks_Total_FileLogNumber = _ttoi(csFileLog_TotalNum);




			PreviousTasks_Total_FileLogNumber += iThisTask_ClientCount;




			csFileLog_TotalNum.Format(_T("%d"), PreviousTasks_Total_FileLogNumber);









			m_WL_TatalNum_RightTotal.SetWindowText(csFileLog_TotalNum);




			




		}




		else




		{//?FileLog




			m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, _T("0"));//0  ?int ï¿½ï¿½




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
		pHeapArgsForAllThread_MsgLog->dwHttpsSubTypes = m_dwHttpsSubTypes;
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









				AfxBeginThread((AFX_THREADPROC)ThreadFunc_MsgLogSend,(PLOG_SENDER_THREAD_ARG)pHeapArgsForAllThread_MsgLog, THREAD_PRIORITY_TIME_CRITICAL, 0, 0, NULL);//?client ??




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
		pHeapArgsForAllThread_FileLog->dwHttpsSubTypes = m_dwHttpsSubTypes;









					pHeapArgsForAllThread_FileLog->iMsgLog_ClientCount = iThisTask_ClientCount;




					pHeapArgsForAllThread_FileLog->iMsgLog_EachClientTotalCount = iThisTask_EachClient_TotalItems;




					pHeapArgsForAllThread_FileLog->iMsgLog_EachClientPerSecondCount = iRemainRight;




					pHeapArgsForAllThread_FileLog->iMsgLog_SleepInterval = fNew_Interval_MisSec; 




					pHeapArgsForAllThread_FileLog->iMsgLog_ClientCount_X_EachClientTotalCount = iThisTask_TotalItems;









					pHeapArgsForAllThread_FileLog->csWhiteListFilePath = strWhiteListFilePath; //?




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




	  




	m_listHeartBeat_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, _T("Ö´ï¿½ï¿½ï¿½ï¿½"));//Ã¿ï¿½Í»ï¿½ï¿½ï¿½ï¿½ß³ï¿½ Ö´














	///* removed */ //modified by lzq 0621:???????????




	                  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);




	                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);




	                GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(TRUE);









}









void CWLServerTestDlg::OnBnClicked_PortsTest()




{




	// TODO: ??????




	if (!CheckServer_IP_Port_HBPort_NotEmpty())




	{




		AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½Ï?"));




		return;




	}









	CString strMsg;




	CSendInfoToServer TestConn_Reg(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);




	SOCKET sockTmp_Reg_HB=INVALID_SOCKET;




	if(!TestConn_Reg.CreateConnection(sockTmp_Reg_HB,m_strServerIP,m_strServerPort))




	{




		strMsg.Format(_T("IP:%s,Port:%s ï¿½ï¿½ï¿½ï¿½Ê§ï¿½Ü£ï¿½"),m_strServerIP.GetBuffer(),m_strServerPort.GetBuffer());




		AfxMessageBox(strMsg);




		return;




	}   




	




	TestConn_Reg.CloseConnection(sockTmp_Reg_HB);




	if(!TestConn_Reg.CreateConnection(sockTmp_Reg_HB,m_strServerIP,m_strServerPortHB))




	{




		strMsg.Format(_T("IP:%s,Port:%s ï¿½ï¿½ï¿½ï¿½Ê§ï¿½Ü£ï¿½"),m_strServerIP.GetBuffer(),m_strServerPortHB.GetBuffer());




		AfxMessageBox(strMsg);




		return;




	}




	TestConn_Reg.CloseConnection(sockTmp_Reg_HB);









	AfxMessageBox(_T("ï¿½ï¿½ï¿½Ó¿ï¿½ï¿½Ã£ï¿½"));




	return;




}










void CWLServerTestDlg::OnBnClicked_Lowest_StopTask()




{




	if (!g_bStopTask)




	{




		g_bStopTask = TRUE;




		



		GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);




		GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);




		GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(FALSE);




		m_StateUKey_BLine_AllNum_RightTotal.SetWindowText(_T("0"));




		m_StateUKey_BLine_SuccessNum_Left.SetWindowText(_T("0"));









		m_WL_TatalNum_RightTotal.SetWindowText(_T("0"));




		m_WL_SuccessNum_Left.SetWindowText(_T("0"));









		mMsgLog_ThreatOpt_TotalCount_RightTotal.SetWindowText(_T("0"));




		mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(_T("0"));









		



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




		if (scrollinfo.nPos+scrollinfo.nPage>scrollinfo.nMax)  //???????ï¿½ï¿½




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




		if (scrollinfo.nPos+scrollinfo.nPage>=scrollinfo.nMax)  //???????ï¿½ï¿½




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




    // TODO: ??????




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
		// ï¿½ï¿½ï¿½ï¿½ HB ??ï¿½ï¿½ï¿½ï¿½?=???ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½???=9999????????????
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
		// ???????? interval?????????
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
						if (pDlg->SendLinuxHeartbeat_HTTPS(g_vecAllClientObjects[i], strHost, nPort, nHbPort))
					{
						pDlg->OnClientOnlineEdgeDetected(g_vecAllClientObjects[i]);  // t4: linux hb ok -> online edge (auto upload if checked)
					}
					else
					{
						g_vecAllClientObjects[i].Client_SetOnline(FALSE);  // t4: linux hb fail -> offline
					}
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
	g_bStopLogTask = TRUE;  Sleep(300);  g_bStopLogTask = FALSE;
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
	if (m_catUsb.GetCheck())             dwTypes |= 0x00000080;
	if (m_catUsb.GetCheck())             dwTypes |= 0x00000080;
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

		m_dwHttpsSubTypes = dwTypes;
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

	// r7: ï¿½ï¿½ï¿½ï¿½Ë«Í¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½HTTPS ï¿½ï¿½ï¿½ï¿½ï¿½Ó£ï¿½ï¿½ï¿½ 26 Î»ï¿½ï¿½ï¿½ï¿½ TCP ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ð²ï¿½ï¿½â£?ï¿½ï¿½ 5 Î» 0x7C000000ï¿½ï¿½
	BOOL bHttpsLog = (dwTypes & 0x03FFFFFF) != 0;
	BOOL bTcpLog   = (dwTypes & 0x7C000000) != 0;
	BOOL bHttpsRun = bHttpsLog && (nHttpsC > 0);
	BOOL bTcpRun   = bTcpLog   && (nTcpC   > 0);

	if (!bHttpsRun && !bTcpRun)
	{
		if (bHttpsLog && !bTcpLog)
			AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½Ð´ HTTPS ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö¾ï¿½ï¿½"));
		else if (!bHttpsLog && bTcpLog)
			AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½Ð´ TCP ï¿½Í»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ð²ï¿½ï¿½â³¤ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö¾ï¿½ï¿?"));
		else
			AfxMessageBox(_T("ï¿½ï¿½ï¿½ï¿½Ð´ï¿½ï¿½ï¿½ï¿½Ò»ï¿½ï¿½Í¨ï¿½ï¿½ï¿½Ä¿Í»ï¿½ï¿½ï¿½ï¿½ï¿½"));
		return;
	}

	CString sTmp;
	sTmp.Format(_T("%d"), nTotal);
	m_comAppLog_Task_EachClientTotalItems.ResetContent();
	m_comAppLog_Task_EachClientTotalItems.AddString(sTmp);
	m_comAppLog_Task_EachClientTotalItems.SetCurSel(0);

	// ï¿½ï¿½ï¿½ï¿½ Channel 1: HTTPS ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½
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

		// r8: ï¿½ï¿½ï¿½ï¿½Î»Í¼(dwTypes)Ó³ï¿½äµ½ï¿½ï¿½ hidden checkboxï¿½ï¿½AddTask ï¿½Ý´ï¿½ï¿½ï¿½ OR ï¿½ï¿½ CLIENT_MSGLOG_*
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
    // bVulnProtect goes to OPT (not THREAT) - aligned with C# VulDefense HTTPS category
    if (bVirus)                 m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_Virus;
    if (bNonWl)                 m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_NWL;
    if (bOs || bOutbound || bMandatory || bUsb || bUsbWarn || bFirewall || bProcAudit || bWlTamper || bRegProtect || bVulnProtect)
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
if (bHttpsRun) OnBnClickedButton_Lowest_AddTask();
		// ï¿½ï¿½Ô­ hidden checkbox ï¿½ï¿½Ö¹ï¿½Â´Î¸ï¿½Ñ¡
		/*removed*/;
		/*removed*/;
		/*removed*/;
		/*removed*/;
		/*removed*/;
		AppendLogOutput(_T("[LOG] HTTPS Í¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½"));
	}

	// ï¿½ï¿½ï¿½ï¿½ Channel 2: TCP ï¿½ï¿½Ð²ï¿½ï¿½â³¤ï¿½ï¿½ï¿½ï¿? ï¿½ï¿½ï¿½ï¿½
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

		// Set threat detection type for TCP channel
		m_iThisTask_SelectedOperationType = (int)((dwTypes & 0x7C000000) | CLIENT_MSGLOG_THREAT);
		if (nTcpC > 0) OnBnClickedButton_Lowest_AddTask();

		AppendLogOutput(_T("[LOG] TCP ï¿½ï¿½Ð²Í¨ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½"));
	}
}

void CWLServerTestDlg::OnBnClickedLogStop()
{
	g_bStopLogTask = TRUE; // r3: only stop log, leave HB running
	// Only update threat log task rows, not heartbeat rows sharing the same list
	for (int iRow : m_setLogTaskRows)
	{
		CString strStatus = m_listHeartBeat_MainWindow.GetItemText(iRow, 6);
		if (strStatus == _T("Ö´ï¿½ï¿½ï¿½ï¿½"))
			m_listHeartBeat_MainWindow.SetItemText(iRow, 6, _T("ï¿½ï¿½Í£Ö¹"));
	}
	AppendLogOutput(_T("[LOG] Stop task sent"));
}


void CWLServerTestDlg::OnBnClickedWlFileChooseButton()
{
	CString cstrFile;
	CFileDialog dlgFile(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("Describe Files (*.wl)|*.wl|"), NULL);
	if (dlgFile.DoModal()) { cstrFile = dlgFile.GetPathName(); m_WLFilePathEdit.SetWindowText(cstrFile); }
}

void CWLServerTestDlg::OnBnClickedDebug()
{
	g_bEnableDebugOutput = m_chkDebug.GetCheck();
}

void CWLServerTestDlg::OnBnClickedUnselectAll()
{
	m_catClientOps.SetCheck(BST_UNCHECKED);
	m_catOs.SetCheck(BST_UNCHECKED);
	m_catOutbound.SetCheck(BST_UNCHECKED);
	m_catFileProtect.SetCheck(BST_UNCHECKED);
	m_catRegProtect.SetCheck(BST_UNCHECKED);
	m_catMandatoryAccess.SetCheck(BST_UNCHECKED);
	m_catVirusAlert.SetCheck(BST_UNCHECKED);
	m_catUsb.SetCheck(BST_UNCHECKED);
	m_catUsbWarning.SetCheck(BST_UNCHECKED);
	m_catFirewall.SetCheck(BST_UNCHECKED);
	m_catVulnProtect.SetCheck(BST_UNCHECKED);
	m_catProcAudit.SetCheck(BST_UNCHECKED);
	m_catNonWhitelist.SetCheck(BST_UNCHECKED);
	m_catWlTamper.SetCheck(BST_UNCHECKED);
	m_catSysGuard.SetCheck(BST_UNCHECKED);
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
	m_catThreatProc.SetCheck(BST_UNCHECKED);
	m_catThreatReg.SetCheck(BST_UNCHECKED);
	m_catThreatFile.SetCheck(BST_UNCHECKED);
	m_catThreatDll.SetCheck(BST_UNCHECKED);
	m_catThreatOs.SetCheck(BST_UNCHECKED);
}
void CWLServerTestDlg::OnBnClickedWlUpload()
{
	// v15: IDC_WHITE_LIST is hidden; check file path, force-check it, then delegate
	CString strWLPath;
	m_WLFilePathEdit.GetWindowText(strWLPath);
	if (strWLPath.IsEmpty())
	{
		AfxMessageBox(_T("ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ä¼ï¿?"));
		return;
	}
	if (!PathFileExists(strWLPath))
	{
		AfxMessageBox(_T("ï¿½ï¿½Ñ¡ï¿½ï¿½ï¿½Ä¼ï¿½Â·ï¿½ï¿½"));
		return;
	}
	// Force-check the hidden IDC_WHITE_LIST so AddTask picks up FILE_LOG_TYPE
	CButton* pWLChk = (CButton*)NULL /*removed*/;
	if (pWLChk) pWLChk->SetCheck(BST_CHECKED);
	OnBnClickedButton_Lowest_AddTask();
	if (pWLChk) pWLChk->SetCheck(BST_UNCHECKED); // restore
	AppendLogOutput(_T("[WL] Whitelist upload task added"));
}

void CWLServerTestDlg::OnBnClickedUploadCfg()
{
	DebugTrace("OnBnClickedUploadCfg entered");
	// t5: ?????????????????????????????????ï¿½ï¿½????????????????
	// ???????????????????? -> ???????? (CSingleLock) ????????????????×® -> ????????
	// ?? ?????????ï¿½ï¿½??????? -> GetTickCount64 ????????? -> ?ï¿½ï¿½????? -> ???????????
	DebugTrace("checking empty clients");
	if (g_vecAllClientObjects.empty())
	{
		AfxMessageBox(_T("Please register clients first"));
		return;
	}

	std::string strCfgJson;
	CString strErrMsg;
	if (!CClientConfigUploader::LoadClientConfigTemplate(strCfgJson, strErrMsg))
	{
		WriteError(_T("[CFG-UPLOAD] load template failed: %s"), strErrMsg.GetString());
		DebugTrace("load template failed");
		AfxMessageBox(strErrMsg.IsEmpty() ? _T("Load config template failed") : strErrMsg);
		return;
	}

	// ??????????????????????????? (ComputerID, IP) ?????? -------------------------------
	struct CfgUploadTarget
	{
		CString strComputerID;
		CString strClientIP;
	};
	std::vector<CfgUploadTarget> vecTargets;
	{
		CSingleLock lk(&g_csVecClients, TRUE);
		for (int i = 0; i < (int)g_vecAllClientObjects.size(); ++i)
		{
			if (g_vecAllClientObjects[i].Client_IsRegistered())
			{
				CfgUploadTarget tgt;
				tgt.strComputerID = g_vecAllClientObjects[i].Client_GetComputerID();
				tgt.strClientIP   = g_vecAllClientObjects[i].GetClientIP();
				vecTargets.push_back(tgt);
			}
		}
	}

	DebugTrace("no registered targets");
	if (vecTargets.empty())
	{
		AfxMessageBox(_T("No registered clients to upload config"));
		return;
	}

	ULONGLONG ullBatchStart = GetTickCount64();   // t5: ????????????
	int nBatchCount = 0;
	DebugTrace("batch loop starting");
	WriteInfo(_T("[CFG-UPLOAD] manual batch start, target=%d"), (int)vecTargets.size());


	// ?????????????ï¿½ï¿½?????????????????????? -------------------------------
	CString strIdPre = m_strEditCtrl_ToRegisterClient_ClientIDPrefix;
	// Fix9: launch background thread for batch upload to avoid UI freeze
	CfgManualUploadThreadArg* pArg = new CfgManualUploadThreadArg();
	pArg->vecTargets.reserve(vecTargets.size());
	for (int i = 0; i < (int)vecTargets.size(); ++i)
		pArg->vecTargets.push_back(std::make_pair(vecTargets[i].strComputerID, vecTargets[i].strClientIP));
	pArg->strCfgJson = strCfgJson;
	pArg->strServerIP = m_strServerIP;
	pArg->strServerPort = m_strServerPort;
	pArg->strIdPre = strIdPre;
	pArg->hWnd = GetSafeHwnd();
	pArg->pDlg = this;
	AppendLogOutput(_T("[CFG-UPLOAD] manual upload background thread starting..."));
	_beginthreadex(NULL, 0,
		[](void* pParam) -> unsigned int {
			CfgManualUploadThreadArg* pArg = (CfgManualUploadThreadArg*)pParam;
			CWLServerTestDlg* pDlg = pArg->pDlg;
			int nBatchCount = 0;
			for (int i = 0; i < (int)pArg->vecTargets.size(); ++i)
			{
				if (g_bStopTask) break;
				CSendInfoToServer snd(pArg->strServerIP, pArg->strServerPort, pArg->strIdPre);
				int nRet = snd.SendClientConfigToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first,
				                                        (LPTSTR)(LPCTSTR)pArg->vecTargets[i].second,
				                                        pArg->strCfgJson.c_str());
				BOOL bFP    = snd.SendClientFileProtectToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bReg   = snd.SendClientRegProtectToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bProc  = snd.SendClientProcessAuditToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bMac   = snd.SendClientMacSubObjToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bTP    = snd.SendClientTrustProcessToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bTPath = snd.SendClientTrustPathToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bFPE   = snd.SendClientFileProtectExceptionToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bRPE   = snd.SendClientRegProtectExceptionToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				BOOL bUser  = snd.SendClientOSUserToServer((LPTSTR)(LPCTSTR)pArg->vecTargets[i].first);
				WriteInfo(_T("[CFG-UPLOAD] manual computerID=%s ret=%d"),
				          pArg->vecTargets[i].first.GetString(), nRet);
				if (nRet == CFG_UPLOAD_RET_OK && bFP && bReg && bProc && bMac && bTP && bTPath && bFPE && bRPE && bUser)
					InterlockedIncrement64(&g_nCfgUploadOk64);
				else
					if (!pDlg->AddCfgUploadRetry(pArg->vecTargets[i].first, pArg->vecTargets[i].second))
						InterlockedIncrement64(&g_nCfgUploadFail64);
				++nBatchCount;
			}
			::PostMessage(pArg->hWnd, WM_APP + 101, 0, 0);
			delete pArg;
			return 0;
		}, pArg, 0, NULL);

	ULONGLONG ullBatchCost = GetTickCount64() - ullBatchStart;   // t5: ???????????????
	WriteInfo(_T("[CFG-UPLOAD] manual batch done, count=%d, cost=%llu ms"), nBatchCount, ullBatchCost);
	CString strDone;
	strDone.Format(_T("[CFG-UPLOAD] manual upload done, count=%d, cost=%llu ms"), nBatchCount, ullBatchCost);
	AppendLogOutput(strDone);
	PostMessage(WM_APP + 101, 0, 0);   // t7: refresh stats text immediately
}

void CWLServerTestDlg::OnBnClickedChkAutoUploadCfg()
{
	// UI ????????????????ï¿½ï¿½?????????????ï¿½ï¿½??????????? HWND??
	m_bAutoUploadCfgChecked = (m_chkAutoUploadCfg.GetCheck() == BST_CHECKED);
	// t7: persist switch to INI for next launch
	CProfileConfig* pCfg = CProfileConfig::GetProfileConfigInstance();
	if (pCfg) pCfg->WriteAutoUploadCfg_ToIni(m_bAutoUploadCfgChecked);
	AppendLogOutput(m_bAutoUploadCfgChecked ? _T("[CFG-UPLOAD] auto upload on online enabled")
	                                        : _T("[CFG-UPLOAD] auto upload on online disabled"));
}

void CWLServerTestDlg::LoadCfgUploadSettingsFromIni()
{
	CProfileConfig* pCfg = CProfileConfig::GetProfileConfigInstance();
	if (!pCfg) return;

	// auto upload switch: restore from INI, then sync checkbox
	m_bAutoUploadCfgChecked = pCfg->ReadAutoUploadCfg_FromIni();
	m_chkAutoUploadCfg.SetCheck(m_bAutoUploadCfgChecked ? BST_CHECKED : BST_UNCHECKED);

	// retry params: use INI if present, else persist defaults for exposure
	int nMax = pCfg->ReadCfgRetryMaxAttempts_FromIni();
	if (nMax <= 0) { nMax = 4; pCfg->WriteCfgRetryMaxAttempts_ToIni(nMax); }
	g_nCfgRetryMaxAttempts = nMax;

	int nDelay = pCfg->ReadCfgRetryBaseDelayMs_FromIni();
	if (nDelay <= 0) { nDelay = 1000; pCfg->WriteCfgRetryBaseDelayMs_ToIni(nDelay); }
	g_nCfgRetryBaseDelayMs = nDelay;

	WriteInfo(_T("[CFG-UPLOAD] ini loaded: auto=%d maxAttempts=%d baseDelayMs=%d"),
		(m_bAutoUploadCfgChecked ? 1 : 0), g_nCfgRetryMaxAttempts, g_nCfgRetryBaseDelayMs);
}

void CWLServerTestDlg::UploadClientConfigForClient(client& cclient)
{
	// ??????????4???????????????3?????????????????
	std::string strCfgJson;
	CString strErrMsg;
	if (!CClientConfigUploader::LoadClientConfigTemplate(strCfgJson, strErrMsg))
	{
		WriteError(_T("[CFG-UPLOAD] load template failed: %s"), strErrMsg.GetString());
		return;
	}

	CString strComputerID = cclient.Client_GetComputerID();
	CString strClientIP   = cclient.GetClientIP();
	CString strIdPre      = m_strEditCtrl_ToRegisterClient_ClientIDPrefix;

	CSendInfoToServer snd(m_strServerIP, m_strServerPort, strIdPre);
	int nRet = snd.SendClientConfigToServer((LPTSTR)(LPCTSTR)strComputerID,
	                                        (LPTSTR)(LPCTSTR)strClientIP,
	                                        strCfgJson.c_str());
	// t8: full-spec policy rule upload (aligned with IEG client)
	BOOL bFP    = snd.SendClientFileProtectToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bReg   = snd.SendClientRegProtectToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bProc  = snd.SendClientProcessAuditToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bMac   = snd.SendClientMacSubObjToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bTP    = snd.SendClientTrustProcessToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bTPath = snd.SendClientTrustPathToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bFPE   = snd.SendClientFileProtectExceptionToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bRPE   = snd.SendClientRegProtectExceptionToServer((LPTSTR)(LPCTSTR)strComputerID);
	BOOL bUser  = snd.SendClientOSUserToServer((LPTSTR)(LPCTSTR)strComputerID);

	WriteInfo(_T("[CFG-UPLOAD] computerID=%s ip=%s ret=%d"), strComputerID.GetString(), strClientIP.GetString(), nRet);
	if (nRet == CFG_UPLOAD_RET_OK && bFP && bReg && bProc && bMac && bTP && bTPath && bFPE && bRPE && bUser)
	{
		InterlockedIncrement64(&g_nCfgUploadOk64);
	}
	else
	{
		// t6: failed -> enqueue retry, final ok/fail counted in retry thread
		if (!AddCfgUploadRetry(strComputerID, strClientIP))
		{
			InterlockedIncrement64(&g_nCfgUploadFail64);
			WriteInfo(_T("[CFG-UPLOAD] enqueue retry failed, drop computerID=%s"), strComputerID.GetString());
		}
	}
	PostMessage(WM_APP + 101, 0, 0);   // t7: refresh stats text immediately
}
BOOL CWLServerTestDlg::AddCfgUploadRetry(const CString& strComputerID, const CString& strClientIP)
{
	if (strComputerID.IsEmpty())
		return FALSE;

	// t6: queue limit + dedup by ComputerID (one pending item per terminal)
	{
		CSingleLock lk(&g_csCfgRetryQueue, TRUE);
		if ((int)g_vecCfgRetryQueue.size() >= g_nCfgRetryQueueLimit)
		{
			WriteInfo(_T("[CFG-UPLOAD] retry queue full, drop computerID=%s"), strComputerID.GetString());
			return FALSE;
		}
		for (int i = 0; i < (int)g_vecCfgRetryQueue.size(); ++i)
		{
			if (g_vecCfgRetryQueue[i].strComputerID == strComputerID)
			{
				WriteInfo(_T("[CFG-UPLOAD] retry dup skip computerID=%s"), strComputerID.GetString());
				return TRUE; // already queued, treat as ok
			}
		}
		CfgUploadRetryItem item;
		item.strComputerID    = strComputerID;
		item.strClientIP      = strClientIP;
		item.nRetryCount      = 0;
		item.ullNextRetryTick = GetTickCount64() + g_nCfgRetryBaseDelayMs; // 1st retry after 1s
		g_vecCfgRetryQueue.push_back(item);
		InterlockedIncrement64(&g_nCfgUploadRetry64);
	}
	WriteInfo(_T("[CFG-UPLOAD] enqueued retry computerID=%s"), strComputerID.GetString());

	// t6: lazy start retry thread if not running
	if (!m_bCfgRetryThreadRunning)
	{
		m_bCfgRetryThreadRunning = TRUE;
		m_hCfgRetryThread = (HANDLE)_beginthreadex(NULL, 0,
			[](void* pArg) -> unsigned int {
				CWLServerTestDlg* pDlg = (CWLServerTestDlg*)pArg;
				while (pDlg->m_bCfgRetryThreadRunning && !g_bStopTask)
				{
					CfgUploadRetryItem item;
					BOOL bGot = FALSE;
					{
						CSingleLock lk(&g_csCfgRetryQueue, TRUE);
						ULONGLONG ullNow = GetTickCount64();
						for (int i = 0; i < (int)g_vecCfgRetryQueue.size(); ++i)
						{
							if (g_vecCfgRetryQueue[i].ullNextRetryTick <= ullNow)
							{
								item = g_vecCfgRetryQueue[i];
								g_vecCfgRetryQueue.erase(g_vecCfgRetryQueue.begin() + i);
								bGot = TRUE;
								break;
							}
						}
					}
					if (!bGot) { Sleep(100); continue; }

					std::string strCfgJson;
					CString strErrMsg;
					if (!CClientConfigUploader::LoadClientConfigTemplate(strCfgJson, strErrMsg))
					{
						InterlockedIncrement64(&g_nCfgUploadFail64);
						pDlg->PostMessage(WM_APP + 101, 0, 0);   // t7: refresh stats text
						WriteError(_T("[CFG-UPLOAD] retry load template failed, drop computerID=%s: %s"),
							item.strComputerID.GetString(), strErrMsg.GetString());
						continue;
					}

					CSendInfoToServer snd(pDlg->m_strServerIP, pDlg->m_strServerPort,
						pDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
					int nRet = snd.SendClientConfigToServer((LPTSTR)(LPCTSTR)item.strComputerID,
						(LPTSTR)(LPCTSTR)item.strClientIP,
						strCfgJson.c_str());
					WriteInfo(_T("[CFG-UPLOAD] retry computerID=%s ip=%s attempt=%d ret=%d"),
						item.strComputerID.GetString(), item.strClientIP.GetString(), item.nRetryCount + 1, nRet);

					if (nRet == CFG_UPLOAD_RET_OK)
					{
						InterlockedIncrement64(&g_nCfgUploadOk64);
						pDlg->PostMessage(WM_APP + 101, 0, 0);   // t7: refresh stats text
						continue;
					}

					++item.nRetryCount;
					if (item.nRetryCount >= g_nCfgRetryMaxAttempts)
					{
						InterlockedIncrement64(&g_nCfgUploadFail64);
						pDlg->PostMessage(WM_APP + 101, 0, 0);   // t7: refresh stats text
						WriteInfo(_T("[CFG-UPLOAD] retry exhausted, drop computerID=%s"), item.strComputerID.GetString());
						continue;
					}
					// backoff: 1s->2s->4s->8s (attempt k => 2^(k-1) s)
					ULONGLONG ullDelay = (ULONGLONG)g_nCfgRetryBaseDelayMs * (1ULL << (item.nRetryCount - 1));
					item.ullNextRetryTick = GetTickCount64() + ullDelay;
					{
						CSingleLock lk(&g_csCfgRetryQueue, TRUE);
						g_vecCfgRetryQueue.push_back(item);
					}
				}
				pDlg->m_bCfgRetryThreadRunning = FALSE;
				return 0;
			}, this, 0, NULL);
		if (m_hCfgRetryThread) CloseHandle(m_hCfgRetryThread); // handle not awaited; flag-based stop
		AppendLogOutput(_T("[CFG-UPLOAD] retry thread started"));
	}
	return TRUE;
}

void CWLServerTestDlg::OnClientOnlineEdgeDetected(client& cclient, BOOL bForce)
{
	// ????ï¿½ï¿½??????????->?????????????????????
	// bForce=TRUE ???????????????????????????????????????????ï¿½ï¿½???????
	BOOL bWasOnline = cclient.Client_IsOnline();
	if (!bForce && bWasOnline)
	{
		WriteInfo(_T("[CFG-UPLOAD] still online, skip edge trigger, computerID=%s"), cclient.Client_GetComputerID().GetString());
		return;
	}
	cclient.Client_SetOnline(TRUE);
	if (!m_bAutoUploadCfgChecked)
	{
		WriteInfo(_T("[CFG-UPLOAD] online edge, auto upload disabled, computerID=%s"), cclient.Client_GetComputerID().GetString());
		return;
	}
	WriteInfo(_T("[CFG-UPLOAD] online edge detected, trigger auto upload, computerID=%s"), cclient.Client_GetComputerID().GetString());
	// Fix9: launch auto-upload in background thread to avoid blocking heartbeat thread
	client* pClientCopy = new client(cclient);
	_beginthreadex(NULL, 0,
		[](void* pParam) -> unsigned int {
			client* pClient = (client*)pParam;
			if (g_WLServerTestDlg) g_WLServerTestDlg->UploadClientConfigForClient(*pClient);
			delete pClient;
			return 0;
		}, pClientCopy, 0, NULL);
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
	// r7: ï¿½ï¿½ï¿½Ýµï¿½Ç°ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ OS Ñ¡ï¿½ï¿½ò¿?¶ï¿½Ó¦ï¿½Ä°æ±¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
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
	// t7: cfg upload stats (ok / fail / retry count)
	s.Format(_T("%lld / %lld / %lld"), (LONG64)g_nCfgUploadOk64,
		(LONG64)g_nCfgUploadFail64, (LONG64)g_nCfgUploadRetry64);
	m_stCfgUploadStat.SetWindowText(s);
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
	// v9: ï¿½Ó¶ï¿½ï¿½ï¿½ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½Ô¶ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½??
	m_editLogOutput.SetSel(0, 0);
	m_editLogOutput.PostMessage(EM_SCROLL, SB_TOP, 0);
	delete pStr;
	return 0;
}

LRESULT CWLServerTestDlg::OnCfgUploadStatUpdate(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	UpdateStatsDisplay();
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
        return FALSE;  // Level 2: MUST return FALSE so WM_PAINT processes button drawing
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

void CWLServerTestDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);

	CWnd* pBtn = WindowFromPoint(point);
	if (pBtn && ::IsWindow(pBtn->GetSafeHwnd())) {
		UINT nID = pBtn->GetDlgCtrlID();
		switch (nID) {
		case IDC_BUTTON_REG_REG: case IDC_BUTTON_REG_RESET:
		case IDC_BUTTON_HB_START: case IDC_BUTTON_HB_STOP:
		case IDC_BUTTON_LOG_ADD: case IDC_BUTTON_LOG_STOP:
		case IDC_BUTTON_WL_UPLOAD: case IDC_BUTTON_WL_PREVIEW:
		case IDC_BUTTON_STOP_TASK: case IDC_BTN_UNSELECT_ALL:
		case IDC_BUTTON_VER_MGMT: case IDC_BUTTON_RAWPACKET:
		case IDC_Btn_TestConn:
			if (nID != m_nHoverBtnID) {
				m_nHoverBtnID = nID;
				Invalidate();
			}
			TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), nFlags, pBtn->GetSafeHwnd(), TME_LEAVE };
			::TrackMouseEvent(&tme);
			return;
		}
	}
	if (m_nHoverBtnID) {
		m_nHoverBtnID = 0;
		Invalidate();
	}
}

void CWLServerTestDlg::OnMouseLeave()
{
	if (m_nHoverBtnID) {
		Invalidate();
		m_nHoverBtnID = 0;
	}
	CDialog::OnMouseLeave();
}
