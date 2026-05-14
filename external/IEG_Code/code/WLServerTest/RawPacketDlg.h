#pragma once
#include "afxwin.h"
#include "afxcmn.h"

// RawPacketEngine.dll API declarations
typedef int (__cdecl *pfnRPE_Init)();
typedef void (__cdecl *pfnRPE_Cleanup)();
typedef int (__cdecl *pfnRPE_GetAdapterCount)();
typedef int (__cdecl *pfnRPE_GetAdapterInfo)(int index, char* name, int nameLen, char* ipv4, int ipv4Len);
typedef int (__cdecl *pfnRPE_SelectAdapter)(int index);
typedef int (__cdecl *pfnRPE_AddStream)(const BYTE* data, int len, const char* name, void* rules, int ruleCount, unsigned int checksumFlags);
typedef void (__cdecl *pfnRPE_ClearStreams)();
typedef void (__cdecl *pfnRPE_SetRateConfig)(int speedType, long long speedValue, int sndMode, long long sndCount);
typedef int (__cdecl *pfnRPE_Start)();
typedef void (__cdecl *pfnRPE_Stop)();
typedef void (__cdecl *pfnRPE_GetStats)(unsigned long long* sendTotal, unsigned long long* sendBytes, unsigned long long* sendFail);

struct BuiltinPacket {
    int id;
    CString name;
    CString targetOs;
    CString resourceName;
    bool selected;
};

class CRawPacketDlg : public CDialog
{
    DECLARE_DYNAMIC(CRawPacketDlg)

public:
    CRawPacketDlg(CWnd* pParent = NULL);
    virtual ~CRawPacketDlg();

    enum { IDD = IDD_RAWPACKET };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    // RawPacketEngine function pointers
    HMODULE m_hRpeDll;
    pfnRPE_Init            RPE_Init;
    pfnRPE_Cleanup         RPE_Cleanup;
    pfnRPE_GetAdapterCount RPE_GetAdapterCount;
    pfnRPE_GetAdapterInfo  RPE_GetAdapterInfo;
    pfnRPE_SelectAdapter   RPE_SelectAdapter;
    pfnRPE_AddStream       RPE_AddStream;
    pfnRPE_ClearStreams    RPE_ClearStreams;
    pfnRPE_SetRateConfig   RPE_SetRateConfig;
    pfnRPE_Start           RPE_Start;
    pfnRPE_Stop            RPE_Stop;
    pfnRPE_GetStats        RPE_GetStats;

    // Controls
    CComboBox m_cbAdapter;
    CListCtrl m_listPackets;
    CEdit m_editDestIp;
    CEdit m_editDestMac;
    CEdit m_editSrcIpStart;
    CEdit m_editSrcIpMax;
    CEdit m_editSrcIpStep;
    CButton m_chkSrcIpRule;
    CComboBox m_cbSpeedMode;
    CEdit m_editSpeedValue;
    CComboBox m_cbSendMode;
    CEdit m_editBurstCount;
    CButton m_btnStart;
    CButton m_btnStop;
    CStatic m_stBps;
    CStatic m_stPps;
    CStatic m_stTotal;
    CStatic m_stFail;
    CStatic m_stStatus;
    CStatic m_stNpcap;

    // Data
    std::vector<BuiltinPacket> m_builtinPackets;
    CStringArray m_adapterNames;
    CStringArray m_adapterIps;
    int m_selectedAdapterIndex;
    bool m_isRunning;

    // Timer for stats
    UINT_PTR m_nStatsTimer;

    bool LoadRawPacketEngine();
    void UnloadRawPacketEngine();
    void RefreshAdapterList();
    void OnStart();
    void OnStop();
    void UpdateStats();
    void LoadBuiltinPackets();

    afx_msg void OnBnClickedStart();
    afx_msg void OnBnClickedStop();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnCbnSelchangeAdapter();
};
