
// WLServerTestDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "../common/UI/CGridListCtrlEx/CGridListCtrlEx.h"
#include "client.h"

// CWLServerTestDlg 对话框
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

#define MSG_LOG_TYPE		(CLIENT_MSGLOG_OPT | CLIENT_MSGLOG_BLINE | CLIENT_MSGLOG_UKEY|CLIENT_MSGLOG_THREAT|CLIENT_MSGLOG_NWL | CLIENT_MSGLOG_DATAPROTECT | CLIENT_MSGLOG_SYSPROTECT | CLIENT_MSGLOG_BACKUP | CLIENT_MSGLOG_Virus)

#define CLIENT_FILELOG_WLFILE		0x02

#define FILE_LOG_TYPE       (CLIENT_FILELOG_WLFILE)


// 发送日志线程参数结构体
typedef struct _LOG_SENDER_THREAD_ARG
{
	int				iThisTask_LineIndex;           //客户端在vector中的序号
	int				iThisClient_VectorIndex;       //全局g_vecClinet中的index 
	int				iThisTask_SelectedLogType;
	int				iMsgLog_ClientCount_X_EachClientTotalCount; // 要发送的日志总数
	int             iMsgLog_ClientCount;
	int             iMsgLog_EachClientTotalCount;
	int             iMsgLog_EachClientPerSecondCount;
	int             iMsgLog_SleepInterval;
	CString			csWhiteListFilePath;
	SOCKET          sock;

}LOG_SENDER_THREAD_ARG,*PLOG_SENDER_THREAD_ARG;  


// 发送心跳线程参数结构体
#define HB_CLIENTCOUNT_PER_THREAD 1
typedef struct _HB_SENDER_THREAD_ARG
{
	int				iThisTask_LineIndex;           //客户端在vector中的序号
	int				iHBIntervalMilSec;       //全局g_vecClinet中的index 

	int				iHBTotalMinutes;

	int				iArrClientIndex[HB_CLIENTCOUNT_PER_THREAD]; // 要发送的日志总数
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
	CWLServerTestDlg(CWnd* pParent = NULL);	// 标准构造函数

	enum { IDD = IDD_WLSERVERTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
	CComboBox m_comRegButton_ToRegisterClient_ClientCount;
	

	CComboBox m_comHB_ClientCount;
	CComboBox m_comHB_Interval;
	CComboBox m_comHB_TotalMinutes;
	CComboBox m_comAppLog_Task_ClientCount;
	CComboBox m_comAppLog_Task_EachClientTotalItems;
	CComboBox m_comAppLog_Task_EachClientPerSecondItems;

	CButton m_Check_OPTLog;
	CButton m_Check_THTLog;
	CButton m_Check_NWLLog;
  
	

	BOOL CheckServer_IP_Port_HBPort_NotEmpty();
	BOOL CreateGuidString(LPTSTR lpGuid);


	// 客户端文件日志上传 
	//为了保持一致性，以下两个函数 不再属于成员函数！
	//unsigned int static ThreadFunc_FileLogSend(void* pArgument);

	// 客户端消息日志上传
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

	LONGLONG m_lMsgLogSuccessCount;
	LONGLONG m_lFileLog_WL_SuccessCount;

	LONGLONG m_lUploadState_SuccessCount;

	CStatic m_staticApplog;
	CStatic m_OnlineClientCountRigthData;
	CStatic m_cRegisteredClientCounts;


	CString m_strServerIP;    // 服务器ip
	CString m_strServerPort;	
	CString m_strServerPortHB;
	
	CEdit	m_ServerIPAddress;//绑定注册ServerIP
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
	
	afx_msg void OnBnClickedWlFileChooseButton();
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
	

	afx_msg void OnBnClickedOptLog();
	afx_msg void OnBnClickedThtLog();
	afx_msg void OnBnClickedNwlLog();
	afx_msg void OnBnClicked_PwlChooseFile_CheckBox();
	afx_msg void OnBnClickedCheckBaseLine();
	afx_msg void OnBnClickedCheckUkey();
	afx_msg void OnBnClickedDataprotectLog();
	afx_msg void OnBnClickedSysprotectLog();
	afx_msg void OnBnClickedBackupLog();
	afx_msg void OnBnClickedVirusLog();
	afx_msg void OnBnClickedWhitelistPathType();
    CButton m_bRegisterSameTime;
    CStatic m_RegisterThreadCount;
    
    volatile BOOL m_bRegisterSameTime_Now;
    CEdit m_EditRegFailCD;
    afx_msg void OnBnClickedOptRegisterSametime();
    CStatic m_StaticUploadWLCount;
    
    ULONG m_ulSametimeRegister_UploadWLCount;
};
