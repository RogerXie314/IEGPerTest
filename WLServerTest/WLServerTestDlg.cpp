// WLServerTestDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "WLServerTest.h"
#include "WLServerTestDlg.h"
#include "ProfileConfig.h"
#include "SendInfoToServer.h"
#include "client.h"
#include <sstream>
#include <set>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

int g_nTotalRegistered_ClientCount = 0;		              //当前已注册的客户端总数

int g_nHeartBeatSending_ClientCount = 0;

volatile long g_nHeartBeatNotSending_ClientCount = 0;     //当前未发送心跳的客户端总数
volatile long g_nFileLogNotSending_ClientCount = 0;       //当前未发送文件日志的客户端总数

volatile long g_nMsgLogNotSending_ClientCount = 0;        //当前未发送消息日志的客户端总数   volatile long

volatile int g_iThreadCount_RegSameTime = 0;        //

CString  g_strDefaultPrefix			= _T("WLClient_");
CString  g_strDefaultStartNum		= _T("200");
CString  g_strDefaultStartIP		= _T("6.6.6.6");


CString g_cstrIPAd;		              //用户设置的首个客户端的IP地址

vector<client> g_vecAllClientObjects; //所有当前已注册的客户端对象


CCriticalSection g_csApplogCount;	    // 客户端日志数临界区
CCriticalSection g_csApplogListCtrl;	// 日志任务列表临界区
CCriticalSection g_csVecClients;        // g_*全局变量临界区

CSingleLock g_singleLockGManage(&g_csVecClients);     // 以上全局变量锁
CSingleLock g_singleLockApplogCount(&g_csApplogCount);// 客户端日志数锁
CSingleLock g_singleLockListCtrl(&g_csApplogListCtrl);// 任务变量锁	

BOOL g_bStopTask = FALSE;

int g_nSocketCount = 0;
SOCKET g_sock[1000] = {INVALID_SOCKET};

BOOL g_bSamePath = FALSE;

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
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
END_MESSAGE_MAP()
  

// CWLServerTestDlg 对话框

// 定义从WLHeartBeat.h文件中复制过来
#define HEARTBEAT_CMD_NOREGISTER	18		//TCP返回：客户端未注册
#define HEARTBEAT_CMD_POLCY			17		//策略变更指令
#define HEARTBEAT_CMD_BACK			1		//心跳返回指令

CWLServerTestDlg* g_WLServerTestDlg = NULL;

CWLServerTestDlg::CWLServerTestDlg(CWnd* pParent): CDialog(CWLServerTestDlg::IDD, pParent), m_lMsgLogSuccessCount(0),m_lUploadState_SuccessCount(0), m_lFileLog_WL_SuccessCount(0) //首先从Ini获取，如果读取失败，填充默认值，并写入Ini
{  
	// 服务器ip
	m_strServerIP = CProfileConfig::GetProfileConfigInstance()->ReadServerIP_FromIni();
	if(m_strServerIP.GetLength() == 0)
	{
		m_strServerIP = _T("192.168.7.254");
		
		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);
	}
 
	// 服务器端口
	m_strServerPort = CProfileConfig::GetProfileConfigInstance()->ReadServerPort_FromIni();
	if(m_strServerPort.GetLength() == 0)
	{
		m_strServerPort = _T("8440");

		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);
	}

	// 心跳端口
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
    DDX_Control(pDX, IDC_LIST_HEARTBEAT_ListShowWindow, m_listHeartBeat_MainWindow);


    DDX_Control(pDX, IDC_COMBO_APPLOG_COUNT, m_comAppLog_Task_ClientCount);
    DDX_Control(pDX, IDC_COMBO_APPLOG_HZ, m_comAppLog_Task_EachClientTotalItems);
    DDX_Control(pDX, IDC_COMBO_APPLOG_TIME, m_comAppLog_Task_EachClientPerSecondItems);

    DDX_Control(pDX, IDC_LIST_LOWPART_ListShowWindow, m_listLowPart_MainWindow);
    DDX_Control(pDX, IDC_STATIC_APPLOG, m_staticApplog);
    DDX_Control(pDX, IDC_STATIC_HEARTBEAT_OnlineClientCountRigthData, m_OnlineClientCountRigthData);
    DDX_Control(pDX, IDC_STATIC_RegisteredClientCount, m_cRegisteredClientCounts);
    DDX_Control(pDX, IDC_OPT_LOG, m_Check_OPTLog);
    DDX_Control(pDX, IDC_THT_LOG, m_Check_THTLog);
    DDX_Control(pDX, IDC_NWL_LOG, m_Check_NWLLog);

    DDX_Control(pDX, IDC_STATIC_FILELOG, m_staticFilelog);  // 发送文件日志计数(成功/总数):
    DDX_Control(pDX, IDC_WL_FILE_PATH_EDIT, m_WLFilePathEdit);//白名单文件选择框
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
}

BEGIN_MESSAGE_MAP(CWLServerTestDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON_REG_REG, &CWLServerTestDlg::OnBnClicked_RegisterClients)
	ON_BN_CLICKED(IDC_BUTTON_HEARTBEAT_AddTask, &CWLServerTestDlg::OnBnClickedButtonHeartbeat_AddTask)
	ON_BN_CLICKED(IDC_BUTTON_APPLOG_SEND_LowestAddTask, &CWLServerTestDlg::OnBnClickedButton_Lowest_AddTask)
	ON_BN_CLICKED(IDC_BUTTON_REG_RESET, &CWLServerTestDlg::OnBnClicked_ClientReg_Reset)
	ON_BN_CLICKED(IDC_Btn_TestConn, &CWLServerTestDlg::OnBnClicked_PortsTest)
	ON_BN_CLICKED(IDC_WL_FILE_CHOOSE_BUTTON, &CWLServerTestDlg::OnBnClickedWlFileChooseButton)// 白名单文件的选择按钮
//	ON_WM_DROPFILES()
//ON_WM_DROPFILES()
ON_BN_CLICKED(IDC_BUTTON_STOP_TASK, &CWLServerTestDlg::OnBnClicked_Lowest_StopTask)
ON_WM_VSCROLL()
ON_BN_CLICKED(IDC_OPT_LOG, &CWLServerTestDlg::OnBnClickedOptLog)
ON_BN_CLICKED(IDC_THT_LOG, &CWLServerTestDlg::OnBnClickedThtLog)
ON_BN_CLICKED(IDC_NWL_LOG, &CWLServerTestDlg::OnBnClickedNwlLog)
ON_BN_CLICKED(IDC_WHITE_LIST, &CWLServerTestDlg::OnBnClicked_PwlChooseFile_CheckBox)
ON_BN_CLICKED(IDC_CHECK_BASE_LINE, &CWLServerTestDlg::OnBnClickedCheckBaseLine)
ON_BN_CLICKED(IDC_CHECK_UKEY, &CWLServerTestDlg::OnBnClickedCheckUkey)
ON_BN_CLICKED(IDC_DATAPROTECT_LOG, &CWLServerTestDlg::OnBnClickedDataprotectLog)
ON_BN_CLICKED(IDC_SYSPROTECT_LOG, &CWLServerTestDlg::OnBnClickedSysprotectLog)
ON_BN_CLICKED(IDC_Backup_LOG, &CWLServerTestDlg::OnBnClickedBackupLog)
ON_BN_CLICKED(IDC_Virus_LOG, &CWLServerTestDlg::OnBnClickedVirusLog)
ON_BN_CLICKED(IDC_Whitelist_Hash_Type, &CWLServerTestDlg::OnBnClickedWhitelistPathType)
ON_BN_CLICKED(IDC_OPT_REGISTER_SAMETIME, &CWLServerTestDlg::OnBnClickedOptRegisterSametime)
END_MESSAGE_MAP()


 
int Check_FirstIP_TotalRegCount_Overflow(int IPAddrClips[4],int iRegCount)//合法返回1，非法返回0，导致后续客户端IP溢出返回2
{
	//首个客户端IP为X.0.0.0,不合法
	if (IPAddrClips[1] == 0 && IPAddrClips[2] == 0  && IPAddrClips[3] == 0)
	{
		return 0;
	}

	//计算最大的IP地址有多大
	int maxIPAddrClips[4] = {0};
	maxIPAddrClips[3] = (IPAddrClips[3] + iRegCount) % 255;
	maxIPAddrClips[2] = (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) % 255;
	maxIPAddrClips[1] = (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) % 255;
	maxIPAddrClips[0] = IPAddrClips[0] 
	+ (IPAddrClips[1] + (IPAddrClips[2] + ((IPAddrClips[3] + iRegCount) / 255)) / 255) / 255;

	if (maxIPAddrClips[0] >= 255)//注册后最后一个客户端IP超过254.254.254.254，不合法
	{
		return 2;
	}

	return 1;
}

void CWLServerTestDlg::PrepareVecClients_UpdateControls()
{

	USES_CONVERSION;
	char* pstr_Tmp_PreviousRegistered_StartIP = T2A(m_strPreviousRegistered_StartIP); 
	int nFirstIP_FromIni[4] = {0};						//是从Ini得到：FirstClientIP--4个int
	sscanf(pstr_Tmp_PreviousRegistered_StartIP, "%d.%d.%d.%d", &nFirstIP_FromIni[0],&nFirstIP_FromIni[1],&nFirstIP_FromIni[2],&nFirstIP_FromIni[3]);

	int iTmpRet = Check_FirstIP_TotalRegCount_Overflow(nFirstIP_FromIni,m_iPreviousRegistered_ClientCount);
	if (0 == iTmpRet)
	{
		AfxMessageBox(_T("首个客户端的IP地址不合法！"));
		return;
	}
	else if(2 == iTmpRet)
	{
		AfxMessageBox(_T("首个客户端的IP地址导致了后续客户端的IP地址溢出！"));
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
		szClientID_PrefixNum.Append(sThisClientIndex);//通用前缀+本ClientNum

		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;
		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;
		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;
		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;
		szThisClientIP=_T("");
		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);

		client objTmpClient(szClientID_PrefixNum,szThisClientIP);
		objTmpClient.Client_SetComputerID();

		g_nHeartBeatNotSending_ClientCount++; //先前注册的ClientCount. 也认为都没有用过！
		g_nMsgLogNotSending_ClientCount++;
		g_nFileLogNotSending_ClientCount++;

		g_vecAllClientObjects.push_back(objTmpClient); 
	}
	
	CString strPreRegCount;
	strPreRegCount.Format(_T("(上次)注册客户端总数: %d"), m_iPreviousRegistered_ClientCount);
	m_cRegisteredClientCounts.SetWindowText(strPreRegCount);
	  
	{//更新Client相关控件
		
		//下次再注册的Client时候，开始的：StartNum   IP  
		sThisClientIndex=_T("");
		sThisClientIndex.Format(_T("%04d"),iClientStartNum + i);

		tmpIPAddrClips[3] = (nFirstIP_FromIni[3] + i) % 255;
		tmpIPAddrClips[2] = (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) % 255;
		tmpIPAddrClips[1] = (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) % 255;
		tmpIPAddrClips[0] =  nFirstIP_FromIni[0] + (nFirstIP_FromIni[1] + (nFirstIP_FromIni[2] + ((nFirstIP_FromIni[3] + i) / 255)) / 255) / 255;
		szThisClientIP=_T("");
		szThisClientIP.Format(_T("%d.%d.%d.%d"),tmpIPAddrClips[0],tmpIPAddrClips[1],tmpIPAddrClips[2],tmpIPAddrClips[3]);
		
		//2.把新的StartIP (2.2.2.2) 显示出来 SetWindowsText
		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(szThisClientIP);
			 
		//3.SetWindowsText IDPrefix
		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);

		//4.把新的StartNum (200+10) 显示出来 SetWindowsText 
		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(sThisClientIndex);
	}


	//构建以后  需要更新 主界面各个控件的新的开始后数值  建议值
};

BOOL CWLServerTestDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
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

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
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
	    strHeartBeat_TitleLine.Format(_T("序号,50;开始时间,136;客户端数量,80;时间间隔(毫秒),100;持续时间(分钟),100;心跳活跃客户端计数,130;任务状态,64;结束时间,136;发送心跳失败客户端计数,150"));
	  strLog_MSGFILE_TitleLine.Format(_T("序号,50;开始时间,150;客户端数量,80;时间间隔(毫秒),100;持续时间(秒),100;消息日志活跃客户端计数,150;文件日志活跃客户端计数,150;任务状态,64"));//文件日志活跃数,120;任务状态,50;CTRL,0"));

	m_listHeartBeat_MainWindow.SetHeadings(strHeartBeat_TitleLine);
	  m_listLowPart_MainWindow.SetHeadings(strLog_MSGFILE_TitleLine);

	m_listHeartBeat_MainWindow.SetEnableSort(FALSE);
	  m_listLowPart_MainWindow.SetEnableSort(FALSE);

    m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("1"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("10"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("50"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("100"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("200"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("300"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("400"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("500"));
	m_comRegButton_ToRegisterClient_ClientCount.AddString(_T("1000"));
	m_comRegButton_ToRegisterClient_ClientCount.SetCurSel(0);

	
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
	m_comHB_TotalMinutes.SetCurSel(0);



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
	

	/*垂直滚动条初始化
	SCROLLINFO ScrInfo;
	GetScrollInfo(SB_VERT, &ScrInfo, SIF_ALL);
	ScrInfo.nPage = 10; //设置滑块大小
	ScrInfo.nMax = 100; //设置滚动条的最大位置0–100
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

	//1.读取先前注册的总个数
	m_iPreviousRegistered_ClientCount = CProfileConfig::GetProfileConfigInstance()->ReadTotalClientCount_FromIni();//如果没有，读取得到-1
	if (m_iPreviousRegistered_ClientCount > 0)
	{
		
		g_nTotalRegistered_ClientCount = m_iPreviousRegistered_ClientCount;//lzq:Dlg初始化时候，就用Ini-->g_nTotalRegistered_ClientCount
	
		//2.读取StartIP
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
		//3.读取IDPrefix
		m_strEditCtrl_ToRegisterClient_ClientIDPrefix = CProfileConfig::GetProfileConfigInstance()->ReadEachClientIdPrefix_FromIni();
		if(0 != m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())
		{
			
		}
		else
		{
			m_strEditCtrl_ToRegisterClient_ClientIDPrefix = g_strDefaultPrefix;

			CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);	
		}
		//4.读取StartNum
		m_strPreviousRegistered_StartNum = CProfileConfig::GetProfileConfigInstance()->ReadClientStartNum_FromIni();
		if( 0 != m_strPreviousRegistered_StartNum.GetLength())
		{
			
		}
		else
		{
			m_strPreviousRegistered_StartNum = g_strDefaultStartNum;

			CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);
		}

		PrepareVecClients_UpdateControls();//lzq:Dlg初始化时候，构建先前注册的各个Client对象，并计入可用（发心跳 发FileLog 发MsgLog）！(前提是先前有非零注册)
	}
	else
	{
		m_iPreviousRegistered_ClientCount = 0;
		   g_nTotalRegistered_ClientCount = 0;

		CString strTmp;
		strTmp.Format(_T("(上次)注册客户端总数: %d"),g_nTotalRegistered_ClientCount);
		m_cRegisteredClientCounts.SetWindowText(strTmp);


		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);

		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);
		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);
		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);
 
 
		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(m_iPreviousRegistered_ClientCount);   //如果没有先前注册，读取得到-1，这里写入0（PreviousRegisteredClientCount=0）
	}

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
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

//  如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CWLServerTestDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标显示。
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
	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPort,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	return SendInfoToServer.RegisterClientToServer(szComputerID,szClientID,szClientIP);
}

BOOL CWLServerTestDlg::SendHeartbeatToserver_TCP(client& pCurClient,SOCKET sock)
{
	CSendInfoToServer SendInfoToServer(m_strServerIP, m_strServerPortHB,m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
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
	return SendInfoToServer.SendHeartbeat(curClient);
}

unsigned int ThreadFunc_HeartbeatSend_JustSend(PVOID JustSend) //只维持心跳
{
    ULONGLONG ulHBTotal_MilSeconds = 600*60*1000;	
    ULONGLONG ulTime_First = GetTickCount();//时间比较，小于ulHBTotalMilSec
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
            dwRet = g_WLServerTestDlg->SendHeartbeat(*cclient);//此时使用https,已经不再使用长连接。主动去请求策略。
            if (ERROR_SUCCESS != dwRet)
            {
                WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!"));	             
            }
        }
        else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//处理未注册
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

        Sleep(30000);//心跳睡30秒

        ulTime_Second = GetTickCount();
        ulTime_During = ulTime_Second - ulTime_First;

        if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:总时长已超
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
            
            g_nTotalRegistered_ClientCount++;

            g_nHeartBeatNotSending_ClientCount++;

            g_nMsgLogNotSending_ClientCount++;
            g_nFileLogNotSending_ClientCount++;

            pClient->Client_SetRegistered(TRUE);
            
            CString strTmpRegisteredClientCount;
            
            g_singleLockGManage.Lock();
            
            strTmpRegisteredClientCount.Format(_T("已注册客户端总数: %d"), g_nTotalRegistered_ClientCount);
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
        //先创建心跳线程维持心跳
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
 
void CWLServerTestDlg::OnBnClicked_RegisterClients() //lzq:点击注册按钮
{

	if (!CheckServer_IP_Port_HBPort_NotEmpty())
	{
		AfxMessageBox(_T("请首先设置服务器信息！"));
		return;
	}
	if (0 == m_strEditCtrl_ToRegisterClient_ClientIDPrefix.GetLength())
	{
		AfxMessageBox(_T("请先设置客户端ID！"));
		return;
	}

	CString strRegClientCount;
	m_comRegButton_ToRegisterClient_ClientCount.GetWindowText(strRegClientCount);

	int iNeedToRegClientCount = _ttoi(strRegClientCount);
	if (iNeedToRegClientCount <= 0)
	{
		AfxMessageBox(_T("注册客户端个数必须是正数！"));
		return;
	}

	CString strFirstClientIPToRegister;
	m_ctrlBox_ToRegisterClient_FirstClientIP.GetWindowText(strFirstClientIPToRegister);
	if (0 == strFirstClientIPToRegister.GetLength())
	{
		AfxMessageBox(_T("请先设置首个客户端的IP地址！"));
		return;
	}

	USES_CONVERSION;
	char* pcFirstClientIPToRegister = T2A(strFirstClientIPToRegister); 
	int IPAddrClips_FourInt[4] = {0};
	sscanf(pcFirstClientIPToRegister, "%d.%d.%d.%d", &IPAddrClips_FourInt[0],&IPAddrClips_FourInt[1],&IPAddrClips_FourInt[2],&IPAddrClips_FourInt[3]);

	if (0 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))
	{
		AfxMessageBox(_T("首个客户端的IP地址不合法！"));
		return;
	}
	else if(2 == Check_FirstIP_TotalRegCount_Overflow(IPAddrClips_FourInt,iNeedToRegClientCount))
	{
		AfxMessageBox(_T("首个客户端的IP地址导致了后续客户端的IP地址溢出！"));
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
                AfxMessageBox(_T("未找到需要上传的程序白名单文件"));
                return;
            }
        }
        else
        {
            if (IDYES != MessageBoxEx(NULL, _T("未勾选上传程序白名单，注册完成后不会上传，是否继续"), _T("提示"), 
                MB_YESNO | MB_TASKMODAL, 
                MAKELANGID(LANG_CHINESE_SIMPLIFIED, SUBLANG_CHINESE_SIMPLIFIED)))
            {
                return;
            }
        }
	}

					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);
					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);
			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);
			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(FALSE);
	
	//注册按钮 --> 逐个构造 client对象
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
                g_nTotalRegistered_ClientCount++; //点击注册按钮，但是可能有先前计数10 100 ...

                g_nHeartBeatNotSending_ClientCount++;

                g_nMsgLogNotSending_ClientCount++;
                g_nFileLogNotSending_ClientCount++;

                clientTemp.Client_SetRegistered(TRUE);
                g_vecAllClientObjects.push_back(clientTemp);
            }
            else
            {
                AfxMessageBox(_T("注册客户端失败！"));
                GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);
                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);
                GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);
                GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);
                GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);

                return;
            }
            
            CString strTmpRegisteredClientCount;
            strTmpRegisteredClientCount.Format(_T("已注册客户端总数: %d"), g_nTotalRegistered_ClientCount);
            m_cRegisteredClientCounts.SetWindowText(strTmpRegisteredClientCount);
		}

		{
			// 用于以防函数处理时间过长，先处理一下窗口函数
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

	{//更新控件  方便继续注册（但是，当前实际只能注册一次，按钮变灰）

		//1.设置新的起始编号
		CString cstrNextRegistration_StartNum;
		cstrNextRegistration_StartNum.Format(_T("%d"),iClientStartNum + iNeedToRegClientCount);//200+10
		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(cstrNextRegistration_StartNum);

		//2.设置新的IP
		EachClient_UniqueIP[3] = (IPAddrClips_FourInt[3] + i) % 255;
		EachClient_UniqueIP[2] = (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) % 255;
		EachClient_UniqueIP[1] = (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) % 255;
		EachClient_UniqueIP[0] = IPAddrClips_FourInt[0]  + (IPAddrClips_FourInt[1] + (IPAddrClips_FourInt[2] + ((IPAddrClips_FourInt[3] + i) / 255)) / 255) / 255;
		CString cstrNextRegistration_FirstClientIP = _T("");
		cstrNextRegistration_FirstClientIP.Format(_T("%d.%d.%d.%d"),EachClient_UniqueIP[0],EachClient_UniqueIP[1],EachClient_UniqueIP[2],EachClient_UniqueIP[3]);
		
		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(cstrNextRegistration_FirstClientIP);

	}
	
	{//此次注册信息，记录到Ini

		// 每次 点击注册  ---》"指定数目的client"结束，往配置文件中记录配置值，注意只是“本次新注册的Info”：
		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(iNeedToRegClientCount);//10

		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(m_strEditCtrl_ToRegisterClient_ClientIDPrefix);//WLClient_
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(strFirstClientIPToRegister);//6.6.6.6
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(m_strEditCtrl_ToRegisterClient_StartNum);//200  

		CProfileConfig::GetProfileConfigInstance()->WriteServerIP_ToIni(m_strServerIP);
		CProfileConfig::GetProfileConfigInstance()->WriteServerPort_ToIni(m_strServerPort);
		CProfileConfig::GetProfileConfigInstance()->WriteServerHBPort_ToIni(m_strServerPortHB);

	}
					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);  //控制着：是否允许循环注册
					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);
			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);
			   GetDlgItem(IDC_WL_FILE_CHOOSE_BUTTON)->EnableWindow(TRUE);
}

void CWLServerTestDlg::OnBnClicked_ClientReg_Reset()
{
	if (IDOK == AfxMessageBox(_T("重置操作将清除上次及本次注册的所有客户端对象，您要继续吗？\n(但不能清除服务器上的注册信息)"), MB_OKCANCEL))
	{
		g_nTotalRegistered_ClientCount = 0;
		g_nHeartBeatNotSending_ClientCount = 0;
		g_nMsgLogNotSending_ClientCount = 0;
		g_nFileLogNotSending_ClientCount = 0;     

		CString strTmp;
		strTmp.Format(_T("已注册客户端总数: %d"),g_nTotalRegistered_ClientCount);
		m_cRegisteredClientCounts.SetWindowText(strTmp);
 

		CProfileConfig::GetProfileConfigInstance()->WriteTotalClientCount_ToIni(g_nTotalRegistered_ClientCount);
		CProfileConfig::GetProfileConfigInstance()->WriteClientIdPrefix_ToIni(g_strDefaultPrefix);
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartIp_ToIni(g_strDefaultStartIP);
		CProfileConfig::GetProfileConfigInstance()->WriteClientStartNum_ToIni(g_strDefaultStartNum);

		GetDlgItem(IDC_EDIT_IDPRE)->SetWindowText(g_strDefaultPrefix);
		GetDlgItem(IDC_EDIT_STARTNUM)->SetWindowText(g_strDefaultStartNum);
		m_ctrlBox_ToRegisterClient_FirstClientIP.SetWindowText(g_strDefaultStartIP);
		
		//重置：对象全部清空！
		g_vecAllClientObjects.clear();
	}
}  

//added by lzq:MAY19
unsigned int UseHBPort_ThreatLog_CheckBox(int iCurUsedClient)//lzq:一个线程每次发送一条任务,进入这个函数
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

const DWORD HBClientCount_InEveryThread = 10;  // 一个线程执行10个心跳
unsigned int ThreadFunc_HeartbeatSend_Ori(void* pArgument) //lzq:每个心跳线程的入口，其中模拟10个Client
{
	int iThisTaskLineIndex = *(int*)pArgument;//iIndex 恒定-->本条Task
	delete pArgument;

	
	CString csHBIntervalMilSec = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 3); //lzq:间隔ms
	 CString strHBTotalMinutes = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(iThisTaskLineIndex, 4); //lzq:总分钟

	int iHBIntervalMilSec = _ttoi(csHBIntervalMilSec); //lzq:间隔未用！
	int iHBTotalMinutes = _ttoi(strHBTotalMinutes);

	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(iHBTotalMinutes)*60*1000;	
	ULONGLONG ulTime_First = GetTickCount();//时间比较，小于ulHBTotalMilSec
	ULONGLONG ulTime_Second = 0; //added by lzq;

	//std::map; pair  map ;;//key=int, value=client
	
    client ThisHBTask_ToUseClientsArray[HBClientCount_InEveryThread];

	g_singleLockGManage.Lock(); //加锁
	if((g_nTotalRegistered_ClientCount==-1)||(g_nHeartBeatNotSending_ClientCount==-1))
	{
		g_singleLockGManage.Unlock();

		return -1;
	}  

	DWORD dwThisTask_ClientIndex = 0;//(0 - 999)

	std::vector<int> vecIndex;
	for (int i=0; i<g_nTotalRegistered_ClientCount; i++)                             //遍历所有已注册的客户端,找出还未发送心跳的客户端
	{
		if (!(g_vecAllClientObjects[i].Get_IsThisClientSendingHeartBeat()))  //唯一用到ThisClient_HasSentHB，判断是否已发送心跳
		{
			ThisHBTask_ToUseClientsArray[dwThisTask_ClientIndex] = g_vecAllClientObjects[i];
			vecIndex.push_back(i);

			g_vecAllClientObjects[i].Set_IsSendingHeartBeat(TRUE);//这个Client记录已经发送心跳。
			
			g_nHeartBeatNotSending_ClientCount--;

			dwThisTask_ClientIndex++;

			if (dwThisTask_ClientIndex >= HBClientCount_InEveryThread)//为此次任务 选择的Client计数；；dwThisTask_ClientIndex == 10则break
			{
				break;
			}
		}
	}//极为特殊：有可能不足10个

	CString strOnlineClientCount;
	strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);
	g_singleLockGManage.Unlock();//解锁
	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//显示已发送过心跳的Client数量;;在线客户端数量增加了10个



					 SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};
	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};
	
	//逐个Client：连接
	for(int i=0; i<dwThisTask_ClientIndex; i++)//lzq:当前线程 客户端数量
	{
		sSendInfo[i] = new CSendInfoToServer();
		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);//临时sock <------>IP:HBPort
	}
	


	do 
	{
		for(int i=0; i<dwThisTask_ClientIndex; i++)//dwThisTask_ClientIndex==10 时退出了！
		{
			if(!g_WLServerTestDlg->SendHeartbeatToserver_TCP(ThisHBTask_ToUseClientsArray[i],sock[i]))//此次任务选择的每个client  --  TmpSock
			{// 若发送失败，则再连接一次
				
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
		
        Sleep(30 * 1000);//心跳睡30秒

		    ulTime_Second = GetTickCount();
		if (ulTime_Second - ulTime_First > ulHBTotal_MilSeconds)//lzq:总时长已超
		{
			break;
		}

	} while ( 1 );


	//逐个Client：关闭
	for(int i=0; i<dwThisTask_ClientIndex; i++)
	{
		sSendInfo[i]->CloseConnection(sock[i]);
		delete sSendInfo[i];
		sSendInfo[i]=NULL;
	}

	// 重置 原始Total数组的成员！
	
	int iNum = vecIndex.size();
	int k=0;
	g_singleLockGManage.Lock();
	
	for (k = 0;k < iNum; k++)
	{
		int iIdnex = vecIndex[k];

		g_vecAllClientObjects[iIdnex].Set_IsSendingHeartBeat(FALSE);

		g_nHeartBeatNotSending_ClientCount ++;// 原本--
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

	//取得界面上 "活跃客户端计数"    -1   再显示上去！
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
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(iThisTaskLineIndex, 6, _T("结束"));
	}

	_endthreadex( 0 );

	return 0;
}

std::set<int>  g_setOfflineClient;
unsigned int ThreadFunc_HeartbeatSend_New(PHB_SENDER_THREAD_ARG pHeapArgs) //lzq:每个心跳线程的入口，其中模拟10个Client
{
	ULONGLONG ulHBTotal_MilSeconds = ULONGLONG(pHeapArgs->iHBTotalMinutes)*60*1000;	
	ULONGLONG ulTime_First = GetTickCount();//时间比较，小于ulHBTotalMilSec
	ULONGLONG ulTime_Second = 0; //added by lzq;
	ULONGLONG ulTime_During = 0; //added by lzq;

	SOCKET sock[HBClientCount_InEveryThread] = {INVALID_SOCKET};
	CSendInfoToServer* sSendInfo[HBClientCount_InEveryThread] = {0};

	int iThisThreadClientCount = pHeapArgs->iClientCount;
	
	//逐个Client：连接
	for(int i=0; i < iThisThreadClientCount; i++)//lzq:当前线程 客户端数量
	{
		sSendInfo[i] = new CSendInfoToServer();
		sSendInfo[i]->CreateConnection(sock[i],g_WLServerTestDlg->m_strServerIP,g_WLServerTestDlg->m_strServerPortHB);
		g_sock[g_nSocketCount++] = sock[i];
		WriteInfo(_T("ST:HB CreateConnection success!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);
	}
	
	do 
	{  
		for(int i=0; i < iThisThreadClientCount; i++)//dwThisTask_ClientIndex==10 时退出了！
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
	            dwRet = g_WLServerTestDlg->SendHeartbeat(g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]]);//此时使用https,已经不再使用长连接。主动去请求策略。
	            if (ERROR_SUCCESS != dwRet)
	            {
					WriteError(_T("ST:HB CWLHeartBeat::instance()->sendHeartBeat() get policy change failed!Client index:%d;Socket:%d"),pHeapArgs->iArrClientIndex[i],sock[i]);	             
	            }
	        }
            else if (HEARTBEAT_CMD_NOREGISTER == dwRet)//处理未注册
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
		
        Sleep(30000);//心跳睡30秒

		    ulTime_Second = GetTickCount();
			ulTime_During = ulTime_Second - ulTime_First;

		if ( ulTime_During > ulHBTotal_MilSeconds)//lzq:总时长已超
		{
			WriteInfo(_T("ST:HB Actual during time:%lld;Interval time %lld"),ulTime_During,ulHBTotal_MilSeconds);
			break;
		}

	} while ( 1 );

	//逐个Client：关闭
	for(int i=0; i < iThisThreadClientCount; i++)
	{
		sSendInfo[i]->CloseConnection(sock[i]);
		delete sSendInfo[i];
		sSendInfo[i]=NULL;

		
		// 重置 原始Total数组的成员！
		g_vecAllClientObjects[pHeapArgs->iArrClientIndex[i]].Set_IsSendingHeartBeat(FALSE);
		InterlockedIncrement(&g_nHeartBeatNotSending_ClientCount);
	}

	//更新静态控件计数：
	CString strOnlineClientCount;
	g_singleLockGManage.Lock();
	//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);
	g_nHeartBeatSending_ClientCount -= iThisThreadClientCount;
	strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);
	g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);
	g_singleLockGManage.Unlock();


	//更新List控件计数：
	CSingleLock singleLockListCtrl(&g_WLServerTestDlg->m_csHeatbeatListCtrl);	
	singleLockListCtrl.Lock();
	CString strHBActiveClientCount = g_WLServerTestDlg->m_listHeartBeat_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);
	int iHBActiveClientCount = _ttoi(strHBActiveClientCount);
	
	iHBActiveClientCount -= iThisThreadClientCount;

	strHBActiveClientCount.Format(_T("%d"), iHBActiveClientCount);
	g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strHBActiveClientCount);
	singleLockListCtrl.Unlock();

	if (iHBActiveClientCount <= 0)
	{
		g_WLServerTestDlg->m_listHeartBeat_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, _T("结束"));
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


// 发送客户端文件日志线程
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
		g_singleLockApplogCount.Unlock();//锁定范围，下一行。

        // 因程序白名单在USM上更新为替换制原则，故只需上传成功一次即可
	}	
	// reset

	
	InterlockedIncrement(&g_nFileLogNotSending_ClientCount);

	// 设置当前任务客户端活跃状态数
	g_singleLockListCtrl.Lock();
	CString strActiveClient = g_WLServerTestDlg->m_listLowPart_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 6);
	int iActiveClient = _ttoi(strActiveClient);
	iActiveClient--;
	strActiveClient.Format(_T("%d"), iActiveClient);
	g_WLServerTestDlg->m_listLowPart_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 6, strActiveClient);
	CString strMsgLogActNum = g_WLServerTestDlg->m_listLowPart_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);
	g_singleLockListCtrl.Unlock();


	int iMsgLogActNum = _ttoi(strMsgLogActNum);
	if ( iActiveClient==0 && iMsgLogActNum==0)
	{
		g_WLServerTestDlg->m_listLowPart_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 7, _T("结束"));
	}

	delete pHeapArgs;

	_endthreadex( 0 );

	return 0;
}

unsigned int ThreadFunc_MsgLogSend(PLOG_SENDER_THREAD_ARG pHeapArgs)   //最后一个线程 需要释放参数！
{	
	TCHAR szThisThread_Selected_ComputerID[MAX_PATH] = {0};
	int nSendCount = 0;

	//确保每次发送的客户端都不一样，随机数
	//ULONGLONG seed = GetCurrentThreadId(); // 获取线程ID
	//srand(seed);
	//pHeapArgs->iThisClient_VectorIndex = rand() % g_nTotalRegistered_ClientCount;
	int iThisThread_Selected_ClientIndex = pHeapArgs->iThisClient_VectorIndex;
	WriteInfo(_T("iThisClient_VectorIndex = %d"),pHeapArgs->iThisClient_VectorIndex);

	_tcscpy(szThisThread_Selected_ComputerID, g_vecAllClientObjects[iThisThread_Selected_ClientIndex].Client_GetComputerID());
	
	int iThisClient_CurrentSuccessCount = 0; //成功发送日志的次数	

	
	CSendInfoToServer SendInfoToServer_LogPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPort,  g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	

	client& objForHBThreatLog = g_vecAllClientObjects[iThisThread_Selected_ClientIndex];
	CSendInfoToServer  SendInfoToServer_HBPort(g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB,g_WLServerTestDlg->m_strEditCtrl_ToRegisterClient_ClientIDPrefix);
	//SOCKET stTmpSock;
	//SendInfoToServer_HBPort.CreateConnection(stTmpSock,g_WLServerTestDlg->m_strServerIP, g_WLServerTestDlg->m_strServerPortHB);


	do //本线程发完一条 Sleep一次
	{
		if (g_bStopTask)
		{
			g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(_T("0"));
			  g_WLServerTestDlg->m_StateUKey_BLine_SuccessNum_Left.SetWindowText(_T("0"));

			break;
		}
		//发送客户端操作日志
		{
			BOOL bRet = FALSE; 

			//opt threat 
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_OPT)
			{ 
				bRet = SendInfoToServer_LogPort.SendClientOptLogToServer(szThisThread_Selected_ComputerID);
			}
			
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NWL)
			{ 
				 SendInfoToServer_LogPort.SendClientNwlLogToServer_FiveType(szThisThread_Selected_ComputerID);
			}
		
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_THREAT)
			{//added by lzq
				//bRet = SendInfoToServer.SendClientThtLogToServer(szComputerID);
				//UseHBPort_ThreatLog_CheckBox(iThisThread_Selected_ClientIndex);//lzq:一个线程的每条任务 一次进入

				if (pHeapArgs->sock != INVALID_SOCKET)
				{
					// 70次命中一次  
					BOOL bHit = FALSE;
					
					if (70 <= nSendCount++)
					{
						bHit = TRUE;
						nSendCount = 0;
					}

					SendInfoToServer_HBPort.SendThreatLog_ToserverTCP(objForHBThreatLog,pHeapArgs->sock, bHit);
				}	
			}

			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_DATAPROTECT)
			{
				SendInfoToServer_LogPort.SendClientDataProtectLogToServer(szThisThread_Selected_ComputerID);
			}

			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_SYSPROTECT)
			{
				SendInfoToServer_LogPort.SendClientSysProtectLogToServer(szThisThread_Selected_ComputerID);
			}

			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BACKUP)
			{
				SendInfoToServer_LogPort.SendClientBackupLogToServer(szThisThread_Selected_ComputerID);
			}

			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_Virus)
			{
				SendInfoToServer_LogPort.SendClientVirusLogToServer(szThisThread_Selected_ComputerID);
			}

			if ((pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_OPT)||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_THREAT) || 
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_NWL) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_DATAPROTECT) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_SYSPROTECT) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BACKUP) ||
				( pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_Virus))
			{
				InterlockedIncrement64(&(g_WLServerTestDlg->m_lMsgLogSuccessCount));//发送成功，才递增一个！

				CString strTmpMsgLogSuccessCount;
				strTmpMsgLogSuccessCount.Format(_T("%lld"), g_WLServerTestDlg->m_lMsgLogSuccessCount);
				g_WLServerTestDlg->mMsgLog_ThreatOpt_SuccessCount_Left.SetWindowText(strTmpMsgLogSuccessCount);

			}

			//baseline  ukey 
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_BLINE)
			{
				bRet = SendInfoToServer_LogPort.SendBaseLineToServer(szThisThread_Selected_ComputerID);
			}
			if (pHeapArgs->iThisTask_SelectedLogType & CLIENT_MSGLOG_UKEY)
			{
				//制作Ukey消息
				VEC_ST_USERS_USM    vecUSMUsersSend;    /* 上报的全量用户列表 */
				ST_USER_JSON oneUser;
				time_t t = time(NULL); //获取日历时间
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
				InterlockedIncrement64(&(g_WLServerTestDlg->m_lUploadState_SuccessCount));//发送成功，才递增一个！
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

		Sleep(pHeapArgs->iMsgLog_SleepInterval);

	}while ( 1 );

	InterlockedIncrement(&g_nMsgLogNotSending_ClientCount); 

	//本线程==本客户端：彻底完成TotalCount发送任务。本线程中更新表格当前活跃数
	g_singleLockListCtrl.Lock();
	CString strActiveClientCount_InMainWindowLine = g_WLServerTestDlg->m_listLowPart_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 5);
	int iThisTaskOtherThreads_FromCtr_ActiveClientCount = _ttoi(strActiveClientCount_InMainWindowLine);
	iThisTaskOtherThreads_FromCtr_ActiveClientCount--;
	strActiveClientCount_InMainWindowLine.Format(_T("%d"), iThisTaskOtherThreads_FromCtr_ActiveClientCount);  
	g_WLServerTestDlg->m_listLowPart_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 5, strActiveClientCount_InMainWindowLine);//本线程减一,消息日志活跃数

	CString strFileLog_ActiveClientCount = g_WLServerTestDlg->m_listLowPart_MainWindow.GetItemText(pHeapArgs->iThisTask_LineIndex, 6);
	g_singleLockListCtrl.Unlock();


	
	int iFileLog_ActiveClientCount = _ttoi(strFileLog_ActiveClientCount);

	if ((iThisTaskOtherThreads_FromCtr_ActiveClientCount <= 0) && (iFileLog_ActiveClientCount == 0))////文件日志活跃数 也==0   //modified by lzq JUNE21:<=   == 
	{
		g_WLServerTestDlg->m_listLowPart_MainWindow.SetItemText(pHeapArgs->iThisTask_LineIndex, 7, _T("结束"));

		if (g_bStopTask)
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
		AfxMessageBox(_T("请首先设置服务器信息！"));
		return;
	}

	CString strNeedHB_ClientCount;
	CString strInterval;  
	CString strTotalMinutes;


	   m_comHB_Interval.GetWindowText(strInterval);
	m_comHB_ClientCount.GetWindowText(strNeedHB_ClientCount);     //客户端数量
   m_comHB_TotalMinutes.GetWindowText(strTotalMinutes);   //时长(分钟)

	int iNeedHB_ClientCount = _ttoi(strNeedHB_ClientCount);//lzq:clientcount
	      int iTotalMinutes = _ttoi(strTotalMinutes);//总时长

	if (iNeedHB_ClientCount > g_nHeartBeatNotSending_ClientCount)
	{
		AfxMessageBox(_T("没有足够的客户端可用于此次任务，请增加注册客户端数量！"));
		return;
	}
	int iThisTask_ThreadCount = iNeedHB_ClientCount / HB_CLIENTCOUNT_PER_THREAD ;


	int iPreviousTaskItemCount = m_listHeartBeat_MainWindow.GetItemCount();//iTaskItemCount 表示有效行数（不计入标题栏）

	CString strNewTaskOrdial;
	strNewTaskOrdial.Format(_T("%d"), iPreviousTaskItemCount+1);

	SYSTEMTIME tm;
	TCHAR szTimeNow[MAX_PATH] = {0};

	GetLocalTime(&tm);
	_sntprintf_s(szTimeNow, MAX_PATH, _TRUNCATE,_T("%04d-%02d-%02d %02d:%02d:%02d"),tm.wYear, tm.wMonth, tm.wDay,tm.wHour, tm.wMinute, tm.wSecond);

	//此次AddTask添加新任务条目  即新的一行
	int iNewTaskInsertIndex = m_listHeartBeat_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);//iPreviousTaskItemCount 若为1 ，返回为1
	
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 1, szTimeNow);
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 2, strNeedHB_ClientCount);//客户端数量
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 3, strInterval);
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 4, strTotalMinutes);
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 5, strNeedHB_ClientCount);//活跃客户端计数
	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("初始化中..."));  

	int nTmp = m_listHeartBeat_MainWindow.GetItemCount()-1;
	m_listHeartBeat_MainWindow.EnsureVisible(nTmp,FALSE);

			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(FALSE);
					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);
					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);  
	

	int iStartIndex_FromVector = 0;
	int iThisThread_ClientIndex = 0;

	for (int i=0; i < iThisTask_ThreadCount; i++)//lzq:客户端/10 == 多少个线程！
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

			//结构体 未memset 0 
			pThreadArgs ->iClientCount = HB_CLIENTCOUNT_PER_THREAD;
			pThreadArgs ->iHBIntervalMilSec = 30*1000;
			pThreadArgs ->iHBTotalMinutes = iTotalMinutes;
			pThreadArgs ->iThisTask_LineIndex = iNewTaskInsertIndex;
		}

		for(iStartIndex_FromVector;iStartIndex_FromVector<g_nTotalRegistered_ClientCount;iStartIndex_FromVector++)
		{	
			if (!(g_vecAllClientObjects[iStartIndex_FromVector].Get_IsThisClientSendingHeartBeat()))  //唯一用到ThisClient_HasSentHB，判断是否已发送心跳
			{
				g_vecAllClientObjects[iStartIndex_FromVector].Set_IsSendingHeartBeat(TRUE);//这个Client记录已经发送心跳。

				g_nHeartBeatNotSending_ClientCount--;

				pThreadArgs->iArrClientIndex[iThisThread_ClientIndex] = iStartIndex_FromVector;

				iThisThread_ClientIndex++;

				if (iThisThread_ClientIndex >= HB_CLIENTCOUNT_PER_THREAD)//为此次任务 选择的Client计数；；dwThisTask_ClientIndex == 10则break
				{
					g_nHeartBeatSending_ClientCount += HB_CLIENTCOUNT_PER_THREAD;
					break;
				}
			}
		}
		CString strOnlineClientCount;
		//strOnlineClientCount.Format(_T("%d"), g_nTotalRegistered_ClientCount - g_nHeartBeatNotSending_ClientCount);
		strOnlineClientCount.Format(_T("%d"),g_nHeartBeatSending_ClientCount);
		g_WLServerTestDlg->m_OnlineClientCountRigthData.SetWindowText(strOnlineClientCount);//显示已发送过心跳的Client数量;;在线客户端数量增加了10个
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

	m_listHeartBeat_MainWindow.SetItemText(iNewTaskInsertIndex, 6, _T("执行中..."));//已经启动n个线程  模拟n*10个客户端发送心跳。

			GetDlgItem(IDC_BUTTON_HEARTBEAT_AddTask)->EnableWindow(TRUE);
					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);
					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE);
}

void CWLServerTestDlg::OnBnClickedButton_Lowest_AddTask()
{
	if (!CheckServer_IP_Port_HBPort_NotEmpty())
	{
		AfxMessageBox(_T("请首先设置服务器信息！"));
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

	int iThisTask_TotalItems = iThisTask_ClientCount * iThisTask_EachClient_TotalItems;

	int iRemainRight = iThisTask_EachClient_PerSecondItems;

	if ((iThisTask_ClientCount > g_nMsgLogNotSending_ClientCount) || (iThisTask_ClientCount > g_nFileLogNotSending_ClientCount))
	{
		AfxMessageBox(_T("没有足够的客户端可用于此任务，请增加注册客户端数量！"));
		return;
	}

	m_iThisTask_SelectedOperationType = 0;

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
			AfxMessageBox(_T("未找到需要上传的程序白名单文件"));
			return;
		}
	}

	if (((CButton *)GetDlgItem(IDC_CHECK_BASE_LINE))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BLINE;
	}

	// Ukey日志
	if (((CButton *)GetDlgItem(IDC_CHECK_UKEY))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_UKEY;
	}

	// 数据保护日志
	if (((CButton *)GetDlgItem(IDC_DATAPROTECT_LOG))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_DATAPROTECT;
	}

	// 系统防护日志
	if (((CButton *)GetDlgItem(IDC_SYSPROTECT_LOG))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_SYSPROTECT;
	}
	// 系统防护日志
	if (((CButton *)GetDlgItem(IDC_Backup_LOG))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_BACKUP;
	}

	// 病毒日志
	if (((CButton *)GetDlgItem(IDC_Virus_LOG))->GetCheck())
	{
		m_iThisTask_SelectedOperationType |= CLIENT_MSGLOG_Virus;
	}

	// 白名单日志hash是否相同
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
		AfxMessageBox(_T("请选择至少一种日志！"));
		return;
	}

	//正式开始本次任务，防止重入
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);
					  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(FALSE);
					GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(FALSE);
					GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(FALSE);

	int iPreviousTaskItemCount = m_listLowPart_MainWindow.GetItemCount();//表示有效行数（不计入标题栏）
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

	int				iNew_Interval_MisSec   = 1000 / iThisTask_EachClient_PerSecondItems;//发送一条的 间隔毫秒
	char		cNew_TmpBuf[0x20] = {0};
	sprintf(cNew_TmpBuf, "%.02f", fNew_Interval_MisSec); 
	//csNew_Interval_MilSec  = cNew_TmpBuf;		//每条之后，睡眠的MilSecond
	csNew_Interval_MilSec.Format(_T("%S"),cNew_TmpBuf );


	CString     csThisTask_LastingSeconds = _T("");
	float		iNew_LastingSeconds    = (float)iThisTask_EachClient_TotalItems / iThisTask_EachClient_PerSecondItems;//每个客户都持续时长；
	char		cNew2_TmpBuf[0x20] = {0};
	sprintf(cNew2_TmpBuf, "%.02f", iNew_LastingSeconds); 
	//csThisTask_LastingSeconds  = cNew2_TmpBuf;  //此次添加任务，持续的秒数
	csThisTask_LastingSeconds.Format(_T("%S"),cNew2_TmpBuf );


	
	//此次AddTask添加新任务条目，即添加新行
	int iThisTask_InsertLineIndex = m_listLowPart_MainWindow.InsertItem(iPreviousTaskItemCount, strNewTaskOrdial, 0);////iPreviousTaskItemCount 若为2 ，返回为2
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 1, szThisTask_StartTime);
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 2, strThisTask_ClientCount);
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 3, csNew_Interval_MilSec);
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 4, csThisTask_LastingSeconds);
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, _T("初始化中..."));

	csNew_Interval_MilSec = _T("");

	int nTmp = m_listLowPart_MainWindow.GetItemCount()-1;
	m_listLowPart_MainWindow.EnsureVisible(nTmp,FALSE);
 
	{//Prepare args for each thread:Added by lzq:June07

		if ( (m_iThisTask_SelectedOperationType & MSG_LOG_TYPE) )
		{
			m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, strThisTask_ClientCount);//客户端数量 == 消息日志活跃数
			
			if (m_iThisTask_SelectedOperationType & (CLIENT_MSGLOG_BLINE | CLIENT_MSGLOG_UKEY))
			{
				CString cstrTotalItems_BLine_UKey = _T("");
				m_StateUKey_BLine_AllNum_RightTotal.GetWindowText(cstrTotalItems_BLine_UKey);
				int PreviousTasks_Total_LogNumber = _ttoi(cstrTotalItems_BLine_UKey);
				PreviousTasks_Total_LogNumber += iThisTask_TotalItems;
				cstrTotalItems_BLine_UKey.Format(_T("%d"), PreviousTasks_Total_LogNumber);

				m_StateUKey_BLine_AllNum_RightTotal.SetWindowText(cstrTotalItems_BLine_UKey);
			}
			if (m_iThisTask_SelectedOperationType & (CLIENT_MSGLOG_OPT| CLIENT_MSGLOG_NWL  | CLIENT_MSGLOG_THREAT | CLIENT_MSGLOG_DATAPROTECT | CLIENT_MSGLOG_SYSPROTECT | CLIENT_MSGLOG_BACKUP))    //added by lzq: |CLIENT_THREAT_LOG
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
		{//没有MsgLog
			m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 5, _T("0"));//必须置0  后续用来取得int 判断
		}

		if ( m_iThisTask_SelectedOperationType & FILE_LOG_TYPE )
		{
			
			//更新界面任务总数
			m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, strThisTask_ClientCount);//客户端数量 == 文件日志活跃数

			CString csFileLog_TotalNum = _T("");
			m_WL_TatalNum_RightTotal.GetWindowText(csFileLog_TotalNum);
			int PreviousTasks_Total_FileLogNumber = _ttoi(csFileLog_TotalNum);
			PreviousTasks_Total_FileLogNumber += iThisTask_ClientCount;
			csFileLog_TotalNum.Format(_T("%d"), PreviousTasks_Total_FileLogNumber);

			m_WL_TatalNum_RightTotal.SetWindowText(csFileLog_TotalNum);
			
		}
		else
		{//没有FileLog
			m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 6, _T("0"));//必须置0  后续用来取得int 判断
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

				AfxBeginThread((AFX_THREADPROC)ThreadFunc_MsgLogSend,(PLOG_SENDER_THREAD_ARG)pHeapArgsForAllThread_MsgLog, THREAD_PRIORITY_TIME_CRITICAL, 0, 0, NULL);//每个client 独立一个线程
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

					pHeapArgsForAllThread_FileLog->csWhiteListFilePath = strWhiteListFilePath; //参数多一条
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
	  
	m_listLowPart_MainWindow.SetItemText(iThisTask_InsertLineIndex, 7, _T("执行中..."));//每个客户端线程 都已启动执行


	//GetDlgItem(IDC_BUTTON_APPLOG_SEND_LowestAddTask)->EnableWindow(TRUE); //modified by lzq 0621:只有真正完成任务之后，才可以重新添加任务！
	                  GetDlgItem(IDC_BUTTON_REG_REG)->EnableWindow(TRUE);
	                GetDlgItem(IDC_BUTTON_REG_RESET)->EnableWindow(TRUE);
	                GetDlgItem(IDC_BUTTON_STOP_TASK)->EnableWindow(TRUE);

}

void CWLServerTestDlg::OnBnClicked_PortsTest()
{
	// TODO: 在此添加控件通知处理程序代码
	if (!CheckServer_IP_Port_HBPort_NotEmpty())
	{
		AfxMessageBox(_T("请首先设置服务器信息！"));
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

void CWLServerTestDlg::OnBnClickedWlFileChooseButton()
{
	CString cstrFile = _T("");

	CFileDialog dlgFile(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("Describe Files (*.wl)|*.wl|"), NULL);

	if (dlgFile.DoModal())
	{
		cstrFile = dlgFile.GetPathName();
		m_WLFilePathEdit.SetWindowText(cstrFile);
	}
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
		if (scrollinfo.nPos+scrollinfo.nPage>scrollinfo.nMax)  //此处一定要注意加上滑块的长度，再作判断
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
		if (scrollinfo.nPos+scrollinfo.nPage>=scrollinfo.nMax)  //此处一定要注意加上滑块的长度，再作判断
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

void CWLServerTestDlg::OnBnClickedOptLog()
{
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*    pCheckboxLog = (CButton*)GetDlgItem(IDC_OPT_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


	if (!pCheckBox_WLFileChoose || !pCheckboxLog ||!pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckboxLog->GetCheck())
	{
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
		pCheckbox_NoneWL->EnableWindow(FALSE);

	}
	else
	{
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
		pCheckbox_NoneWL->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedThtLog()
{
	// TODO: 在此添加控件通知处理程序代码
		CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);

		CButton*	pCheckbox_Threat = (CButton*)GetDlgItem(IDC_THT_LOG);
		CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


		if (!pCheckBox_WLFileChoose || !pCheckbox_Threat ||!pCheckbox_NoneWL)
		{
			return;
		}

		if (pCheckbox_Threat->GetCheck())
		{
      pCheckBox_WLFileChoose->EnableWindow(FALSE);
			pCheckbox_NoneWL->EnableWindow(FALSE);

		}
		else
		{
	  pCheckBox_WLFileChoose->EnableWindow(TRUE);
			pCheckbox_NoneWL->EnableWindow(TRUE);
		}
}
  
void CWLServerTestDlg::OnBnClickedNwlLog()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton* pCheckboxLog = (CButton*)GetDlgItem(IDC_OPT_LOG);
	CButton* pCheckbox_Threat = (CButton*)GetDlgItem(IDC_THT_LOG);
	CButton* pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);

	if (!pCheckboxLog || !pCheckbox_Threat || !pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckbox_NoneWL->GetCheck())
	{
			pCheckboxLog->EnableWindow(FALSE);
		pCheckbox_Threat->EnableWindow(FALSE);

		GetDlgItem(IDC_CHECK_BASE_LINE)->EnableWindow(FALSE);
		     GetDlgItem(IDC_CHECK_UKEY)->EnableWindow(FALSE);

	}
	else
	{
		pCheckboxLog->EnableWindow(TRUE);
		pCheckbox_Threat->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_BASE_LINE)->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_UKEY)->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClicked_PwlChooseFile_CheckBox()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);

	CButton*	pCheckbox_Log = (CButton*)GetDlgItem(IDC_OPT_LOG);
	CButton*	pCheckbox_Threat = (CButton*)GetDlgItem(IDC_THT_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);

	CButton*	pCheckBox_BLine =  (CButton*)GetDlgItem(IDC_CHECK_BASE_LINE);

	CButton*	pCheckBox_UKey = (CButton*)GetDlgItem(IDC_CHECK_UKEY);

	if (!pCheckBox_WLFileChoose || !pCheckbox_Log || !pCheckbox_Threat ||!pCheckbox_NoneWL ||!pCheckBox_BLine ||!pCheckBox_UKey)
	{
		return;
	}

	if (pCheckBox_WLFileChoose->GetCheck())
	{
		pCheckbox_Log->EnableWindow(FALSE);
		pCheckbox_Threat->EnableWindow(FALSE);

		pCheckBox_BLine->EnableWindow(FALSE);
		pCheckBox_UKey->EnableWindow(FALSE);
	}
	else
	{
		pCheckbox_Log->EnableWindow(TRUE);
		pCheckbox_Threat->EnableWindow(TRUE);

		pCheckBox_BLine->EnableWindow(TRUE);
		pCheckBox_UKey->EnableWindow(TRUE);

	}
}

void CWLServerTestDlg::OnBnClickedCheckBaseLine()
{
	// TODO: 在此添加控件通知处理程序代码

	CButton*	pCheckBox_BLine =  (CButton*)GetDlgItem(IDC_CHECK_BASE_LINE);

	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);
	
	if ( !pCheckBox_BLine || !pCheckBox_WLFileChoose || !pCheckbox_NoneWL)
	{
	    return;
	}

	if (pCheckBox_BLine->GetCheck())
	{
		pCheckbox_NoneWL->EnableWindow(FALSE);
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
	}
	else 
	{
		pCheckbox_NoneWL->EnableWindow(TRUE);
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedCheckUkey()
{
	// TODO: 在此添加控件通知处理程序代码

	CButton*	pCheckBox_UKey = (CButton*)GetDlgItem(IDC_CHECK_UKEY);
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);

	if ( !pCheckBox_UKey || !pCheckBox_WLFileChoose || !pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckBox_UKey->GetCheck())
	{
			  pCheckbox_NoneWL->EnableWindow(FALSE);
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
	}
	else 
	{
		pCheckbox_NoneWL->EnableWindow(TRUE);
		pCheckBox_WLFileChoose->EnableWindow(TRUE);

	}
}

void CWLServerTestDlg::OnBnClickedDataprotectLog()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*    pCheckboxLog = (CButton*)GetDlgItem(IDC_DATAPROTECT_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


	if (!pCheckBox_WLFileChoose || !pCheckboxLog ||!pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckboxLog->GetCheck())
	{
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
		pCheckbox_NoneWL->EnableWindow(FALSE);

	}
	else
	{
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
		pCheckbox_NoneWL->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedSysprotectLog()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*    pCheckboxLog = (CButton*)GetDlgItem(IDC_SYSPROTECT_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


	if (!pCheckBox_WLFileChoose || !pCheckboxLog ||!pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckboxLog->GetCheck())
	{
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
		pCheckbox_NoneWL->EnableWindow(FALSE);

	}
	else
	{
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
		pCheckbox_NoneWL->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedBackupLog()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*    pCheckboxLog = (CButton*)GetDlgItem(IDC_Backup_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


	if (!pCheckBox_WLFileChoose || !pCheckboxLog ||!pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckboxLog->GetCheck())
	{
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
		pCheckbox_NoneWL->EnableWindow(FALSE);

	}
	else
	{
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
		pCheckbox_NoneWL->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedVirusLog()
{
	// TODO: 在此添加控件通知处理程序代码
	CButton*	pCheckBox_WLFileChoose =  (CButton*)GetDlgItem(IDC_WHITE_LIST);
	CButton*    pCheckboxLog = (CButton*)GetDlgItem(IDC_Virus_LOG);
	CButton*	pCheckbox_NoneWL = (CButton*)GetDlgItem(IDC_NWL_LOG);


	if (!pCheckBox_WLFileChoose || !pCheckboxLog ||!pCheckbox_NoneWL)
	{
		return;
	}

	if (pCheckboxLog->GetCheck())
	{
		pCheckBox_WLFileChoose->EnableWindow(FALSE);
		pCheckbox_NoneWL->EnableWindow(FALSE);

	}
	else
	{
		pCheckBox_WLFileChoose->EnableWindow(TRUE);
		pCheckbox_NoneWL->EnableWindow(TRUE);
	}
}

void CWLServerTestDlg::OnBnClickedWhitelistPathType()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CWLServerTestDlg::OnBnClickedOptRegisterSametime()
{
    // TODO: 在此添加控件通知处理程序代码
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
