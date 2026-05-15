
// WLServerTestDlg.h : ͷ�ļ�
//

#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "../common/UI/CGridListCtrlEx/CGridListCtrlEx.h"
#include "client.h"

// CWLServerTestDlg �Ի���
class  CWLServerTestDlg;
extern CWLServerTestDlg* g_WLServerTestDlg;

#define CLIENT_MSGLOG_OPT			0x00000001
#define CLIENT_MSGLOG_BLINE			0x00000004
#define CLIENT_MSGLOG_UKEY			0x00000008
#define CLIENT_MSGLOG_THREAT		0x00000010
#define CLIENT_MSGLOG_NWL    		0x00000020
#define CLIENT_MSGLOG_DATAPROTECT   0x00000040
#define CLIENT_MSGLOG_SYSPROTECT    0x00000080
#define CLIENT_MSGLOG_BACKUP        0x00000100
#define CLIENT_MSGLOG_Virus         0x00000200
#define CLIENT_MSGLOG_NETADAPTER   0x00000400
#define CLIENT_MSGLOG_EXTDEV       0x00000800
#define CLIENT_MSGLOG_UDISKPLUG     0x00001000

#define MSG_LOG_TYPE		(CLIENT_MSGLOG_OPT | CLIENT_MSGLOG_BLINE | CLIENT_MSGLOG_UKEY|CLIENT_MSGLOG_THREAT|CLIENT_MSGLOG_NWL | CLIENT_MSGLOG_DATAPROTECT | CLIENT_MSGLOG_SYSPROTECT | CLIENT_MSGLOG_BACKUP | CLIENT_MSGLOG_Virus | CLIENT_MSGLOG_NETADAPTER | CLIENT_MSGLOG_EXTDEV | CLIENT_MSGLOG_UDISKPLUG)

#define CLIENT_FILELOG_WLFILE		0x02

#define FILE_LOG_TYPE       (CLIENT_FILELOG_WLFILE)


// ������־�̲߳����ṹ��
typedef struct _LOG_SENDER_THREAD_ARG
{
	int				iThisTask_LineIndex;           //�ͻ�����vector�е����?
	int				iThisClient_VectorIndex;       //ȫ��g_vecClinet�е�index 
	int				iThisTask_SelectedLogType;
	int				iMsgLog_ClientCount_X_EachClientTotalCount; // Ҫ���͵���־����
	int             iMsgLog_ClientCount;
	int             iMsgLog_EachClientTotalCount;
	int             iMsgLog_EachClientPerSecondCount;
	int             iMsgLog_SleepInterval;
	CString			csWhiteListFilePath;
	SOCKET          sock;

	DWORD           dwExtDevSubTypeMask; // bitmask for ExtDev sub-types (same encoding as dwTypes)
    DWORD           dwHttpsSubTypes;       // full dwTypes mask for per-category routing
}LOG_SENDER_THREAD_ARG,*PLOG_SENDER_THREAD_ARG;  


// ���������̲߳����ṹ��
#define HB_CLIENTCOUNT_PER_THREAD 1
typedef struct _HB_SENDER_THREAD_ARG
{
	int				iThisTask_LineIndex;           //�ͻ�����vector�е����?
	int				iHBIntervalMilSec;       //ȫ��g_vecClinet�е�index 

	int				iHBTotalMinutes;

	int				iArrClientIndex[HB_CLIENTCOUNT_PER_THREAD]; // Ҫ���͵���־����
	int             iClientCount;

}HB_SENDER_THREAD_ARG,*PHB_SENDER_THREAD_ARG;

typedef struct _JUST_SEND_HEART
{
    PVOID pClient;
    
    BOOL bExit;

}JUST_SEND_HEART,*PJUST_SEND_HEART;

class CWLServerTestDlg : public CDialog  
{
public:
	CWLServerTestDlg(CWnd* pParent = NULL);	// ��׼���캯��

	enum { IDD = IDD_WLSERVERTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnBnClickedRawPacket();
DECLARE_MESSAGE_MAP()

private:
	CEdit     m_comRegButton_ToRegisterClient_ClientCount;
	

	CComboBox m_comHB_ClientCount;
	CComboBox m_comHB_Interval;
	CComboBox m_comHB_TotalMinutes;
	CEdit     m_editHbDuration; // v5.2: 心跳时长(分钟) 输入框
	CComboBox m_comAppLog_Task_ClientCount;
	CComboBox m_comAppLog_Task_EachClientTotalItems;
	CComboBox m_comAppLog_Task_EachClientPerSecondItems;

  
	

	BOOL CheckServer_IP_Port_HBPort_NotEmpty();
	BOOL CreateGuidString(LPTSTR lpGuid);


	// �ͻ����ļ���־�ϴ� 
	//Ϊ�˱���һ���ԣ������������� �������ڳ�Ա������
	//unsigned int static ThreadFunc_FileLogSend(void* pArgument);

	// �ͻ�����Ϣ��־�ϴ�
	//unsigned int static ThreadFunc_MsgLogSend(void* pArgument);

	afx_msg void OnBnClicked_RegisterClients();
	afx_msg void OnBnClickedButtonHeartbeat_AddTask();
	afx_msg void OnBnClickedButton_Lowest_AddTask();
	afx_msg void OnBnClicked_ClientReg_Reset();
	afx_msg void OnBnClicked_PortsTest();

public:
	CGridListCtrlEx			m_listHeartBeat_MainWindow;
	CGridListCtrlEx			m_listLowPart_MainWindow;
	CCriticalSection		m_csHeatbeatListCtrl;	
	

	int m_iThisTask_SelectedOperationType;
	DWORD       m_dwExtDevSubTypeMask;  // ExtDev sub-type bitmask for log sender thread
    DWORD           m_dwHttpsSubTypes;      // full dwTypes mask for per-category routing

	LONGLONG m_lMsgLogSuccessCount;
	LONGLONG m_lFileLog_WL_SuccessCount;

	LONGLONG m_lUploadState_SuccessCount;

	CStatic m_staticApplog;
	CStatic m_OnlineClientCountRigthData;
	CStatic m_cRegisteredClientCounts;


	CString m_strServerIP;    // ������ip
	CString m_strServerPort;	
	CString m_strServerPortHB;
	
	CEdit	m_ServerIPAddress;//��ע��ServerIP
	CEdit	m_ServerRegPort;
	CEdit	m_ServerHBPort;


	int         m_iPreviousRegistered_ClientCount;
	CString		m_strEditCtrl_ToRegisterClient_ClientIDPrefix;
	CString     m_strPreviousRegistered_StartNum;
	CString     m_strPreviousRegistered_StartIP;

	CString		m_strEditCtrl_ToRegisterClient_StartNum;

	CIPAddressCtrl			m_ctrlBox_ToRegisterClient_FirstClientIP;


	BOOL SendHeartbeatToserver_TCP(client& pCurClient, SOCKET sock);
    DWORD RecvHeartBeatBack_TCP(SOCKET sockRecv);
	BOOL SendHeartbeat(client& curClient);

	
	BOOL SendUsingHBPort_ThreatLog(client& pCurClient, SOCKET sock);
	DWORD RecvHeartBeatBack_TCP_ThreatLog(SOCKET sockRecv);
	BOOL SendHeartbeat_ThreatLog(client& curClient);
	
	BOOL RegisterClientToServer(CString computerID, CString lpGuid,CString szIP);

	void PrepareVecClients_UpdateControls();
	
	afx_msg void OnIpnFieldchangedIpaddressClient(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClicked_Lowest_StopTask();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);


	CStatic m_staticFilelog;
	
	CEdit	m_WLFilePathEdit;
	CStatic m_StateUKey_BLine_AllNum_RightTotal;
	CStatic m_StateUKey_BLine_SuccessNum_Left;
	CStatic m_WL_TatalNum_RightTotal;
	CStatic m_WL_SuccessNum_Left;
	
	CStatic mMsgLog_ThreatOpt_SuccessCount_Left;
	CStatic mMsgLog_ThreatOpt_TotalCount_RightTotal;
	

    CButton m_bRegisterSameTime;
    CStatic m_RegisterThreadCount;
    
    volatile BOOL m_bRegisterSameTime_Now;
    CEdit m_EditRegFailCD;
    afx_msg void OnBnClickedOptRegisterSametime();
    CStatic m_StaticUploadWLCount;
    
    ULONG m_ulSametimeRegister_UploadWLCount;

    // ===================================================
    // Phase 3: New UI members (WLServerTest v2 redesign)
    // ===================================================

    // --- ������/��־������ ---
    CEdit       m_editLogHost;
    CEdit       m_editLogPort;
    CButton     m_chkUseLogServer;

    // --- OS ���� ---
    CButton     m_radioOsWin;
    CButton     m_radioOsLinux;

    // --- ע�᣺�汾/��Ŀ���� ---
    CComboBox   m_comboClientVersion;
    CComboBox   m_comboProjectType;

    // --- 31 ����־���� CheckBox ---
    // HTTPS ������ (15��)
    CButton     m_catClientOps;
    CButton     m_catOs;
    CButton     m_catOutbound;
    CButton     m_catFileProtect;
    CButton     m_catRegProtect;
    CButton     m_catMandatoryAccess;
    CButton     m_catVirusAlert;
    CButton     m_catUsb;
    CButton     m_catUsbWarning;
    CButton     m_catFirewall;
    CButton     m_catVulnProtect;
    CButton     m_catProcAudit;
    CButton     m_catNonWhitelist;
    CButton     m_catWlTamper;
    CButton     m_catSysGuard;
    // ���? & ���� (2��)
    CButton     m_catUDiskPlug;
    CButton     m_catNetAdapter;
    // �������? (9��)
    CButton     m_catExtUsbPort;
    CButton     m_catExtWpd;
    CButton     m_catExtCdrom;
    CButton     m_catExtWlan;
    CButton     m_catExtUsbEth;
    CButton     m_catExtFloppy;
    CButton     m_catExtBt;
    CButton     m_catExtSerial;
    CButton     m_catExtParallel;
    // ��в���? TCP (5��)
    CButton     m_catThreatProc;
    CButton     m_catThreatReg;
    CButton     m_catThreatFile;
    CButton     m_catThreatDll;
    CButton     m_catThreatOs;

    // --- �������� ---
    CEdit       m_editHbInterval;
    CButton     m_chkPolicyRecv;
    CStatic     m_staticHbBadge;

    // --- �������ϴ� ---
    CEdit       m_editWlConcurrent;
    CButton     m_chkAutoWl;

    // --- ��־�������� ---
    CEdit       m_editLogTotal;
    CEdit       m_editHttpsCount;
    CEdit       m_editHttpsEps;
    CEdit       m_editTcpCount;
    CEdit       m_editTcpEps;
    CEdit       m_editTcpHit;

    // --- ͳ�� Static (14��) ---
    CStatic     m_stRegTotal;
    CStatic     m_stRegSucc;
    CStatic     m_stRegFail;
    CStatic     m_stHbOnline;
    CStatic     m_stHbTotal;
    CStatic     m_stHbPolicy;
    CStatic     m_stLogTotal2;
    CStatic     m_stLogSucc2;
    CStatic     m_stLogFail;
    CStatic     m_stWlTotal2;
    CStatic     m_stWlSucc2;
    CStatic     m_stWlFail2;
    CStatic     m_stRegRound;
    CStatic     m_stHbResp;

    // --- ��־�����? ---
    CEdit       m_editLogOutput;

    // --- OS��Ϣ/�汾 ��̬�ı� ---
    CStatic     m_staticOsInfo;

    // --- Linux HTTPS �����߳̿��� ---
    volatile BOOL   m_bLinuxHbRunning;
    HANDLE          m_hLinuxHbThread;
    int             m_nLinuxHbIntervalMs;

    // --- UI theming ---
    HBRUSH          m_hBrushDlg;
    HBRUSH          m_hBrushWhite;
    HBRUSH          m_hBrushPlug;
    HBRUSH          m_hBrushExt;
    HBRUSH          m_hBrushThreat;
    CFont           m_fontBold;
    CFont           m_fontNormal;
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg void    OnDestroy();

    // ===================================================
    // Phase 3: New methods
    // ===================================================

    // ����־�����ĩβ׷���ı����̰߳��?��ͨ�� PostMessage��
    void AppendLogOutput(LPCTSTR szMsg);

    // ����ͳ�� Static ��ʾ
    void UpdateStatsDisplay();

    // ���� OS �������汾 ComboBox���� ProfileConfig ���汾�б���
    void LoadClientVersionCombo();

    // ���� OsInfo ���ֺ� HB Badge
    void UpdateOsInfoText();

    // Linux HTTPS ���������η��ͣ����߳�ѭ�����ã�
    BOOL SendLinuxHeartbeat_HTTPS(client& curClient,
                                   CString strHost, int nPort, int nHbPort);

    // --- �°�ť�¼����� ---
    afx_msg void OnBnClickedOsWindows();
    afx_msg void OnProjectTypeSelChange();
    void ApplyProjectTypeSelection();
    afx_msg void OnBnClickedOsLinux();
    afx_msg void OnBnClickedHbStart();
    afx_msg void OnBnClickedHbStop();
    afx_msg void OnBnClickedLogAdd();
    afx_msg void OnBnClickedLogStop();
    afx_msg void OnBnClickedWlUpload();
    afx_msg void OnBnClickedWlStop2();
    afx_msg void OnBnClickedWlPreview();
    afx_msg void OnBnClickedVerMgmt();
    afx_msg void OnClientVersionSelChange();
    afx_msg void OnBnClickedLogHelp();
        afx_msg void OnTimer(UINT_PTR nIDEvent);

    // --- �Զ�����Ϣ�����ڿ��߳�׷����־��---
    afx_msg LRESULT OnAppendLogOutput(WPARAM wParam, LPARAM lParam);
    afx_msg void OnBnClickedWlFileChooseButton();

};
