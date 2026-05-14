#include "stdafx.h"
#include "WLServerTest.h"
#include "RawPacketDlg.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CRawPacketDlg, CDialog)

CRawPacketDlg::CRawPacketDlg(CWnd* pParent)
    : CDialog(CRawPacketDlg::IDD, pParent)
    , m_hRpeDll(NULL)
    , m_selectedAdapterIndex(-1)
    , m_isRunning(false)
    , m_nStatsTimer(0)
{
    RPE_Init = NULL;
    RPE_Cleanup = NULL;
    RPE_GetAdapterCount = NULL;
    RPE_GetAdapterInfo = NULL;
    RPE_SelectAdapter = NULL;
    RPE_AddStream = NULL;
    RPE_ClearStreams = NULL;
    RPE_SetRateConfig = NULL;
    RPE_Start = NULL;
    RPE_Stop = NULL;
    RPE_GetStats = NULL;
}

CRawPacketDlg::~CRawPacketDlg()
{
    UnloadRawPacketEngine();
}

void CRawPacketDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CB_ADAPTER, m_cbAdapter);
    DDX_Control(pDX, IDC_LIST_PACKETS, m_listPackets);
    DDX_Control(pDX, IDC_EDIT_DEST_IP, m_editDestIp);
    DDX_Control(pDX, IDC_EDIT_DEST_MAC, m_editDestMac);
    DDX_Control(pDX, IDC_EDIT_SRCIP_START, m_editSrcIpStart);
    DDX_Control(pDX, IDC_EDIT_SRCIP_MAX, m_editSrcIpMax);
    DDX_Control(pDX, IDC_EDIT_SRCIP_STEP, m_editSrcIpStep);
    DDX_Control(pDX, IDC_CHK_SRCIP_RULE, m_chkSrcIpRule);
    DDX_Control(pDX, IDC_CB_SPEED_MODE, m_cbSpeedMode);
    DDX_Control(pDX, IDC_EDIT_SPEED_VALUE, m_editSpeedValue);
    DDX_Control(pDX, IDC_CB_SEND_MODE, m_cbSendMode);
    DDX_Control(pDX, IDC_EDIT_BURST_COUNT, m_editBurstCount);
    DDX_Control(pDX, IDC_BTN_RP_START, m_btnStart);
    DDX_Control(pDX, IDC_BTN_RP_STOP, m_btnStop);
    DDX_Control(pDX, IDC_ST_RP_BPS, m_stBps);
    DDX_Control(pDX, IDC_ST_RP_PPS, m_stPps);
    DDX_Control(pDX, IDC_ST_RP_TOTAL, m_stTotal);
    DDX_Control(pDX, IDC_ST_RP_FAIL, m_stFail);
    DDX_Control(pDX, IDC_ST_RP_STATUS, m_stStatus);
}

BEGIN_MESSAGE_MAP(CRawPacketDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_RP_START, &CRawPacketDlg::OnBnClickedStart)
    ON_BN_CLICKED(IDC_BTN_RP_STOP, &CRawPacketDlg::OnBnClickedStop)
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_CB_ADAPTER, &CRawPacketDlg::OnCbnSelchangeAdapter)
END_MESSAGE_MAP()

BOOL CRawPacketDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetWindowText(_T("RawPacket Attack Sender"));

    // Init packet list
    m_listPackets.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listPackets.InsertColumn(0, _T("Built-in Packets"), LVCFMT_LEFT, 400);

    LoadBuiltinPackets();

    // Init combo boxes
    m_cbSpeedMode.AddString(_T("PPS"));
    m_cbSpeedMode.AddString(_T("Interval(ms)"));
    m_cbSpeedMode.AddString(_T("Max bps"));
    m_cbSpeedMode.SetCurSel(0);

    m_cbSendMode.AddString(_T("Continuous"));
    m_cbSendMode.AddString(_T("Fixed Count"));
    m_cbSendMode.SetCurSel(0);

    m_editSpeedValue.SetWindowText(_T("1000"));
    m_editBurstCount.SetWindowText(_T("100"));
    m_editDestIp.SetWindowText(_T("192.168.1.1"));
    m_editDestMac.SetWindowText(_T("FF:FF:FF:FF:FF:FF"));
    m_editSrcIpStart.SetWindowText(_T("192.168.0.1"));
    m_editSrcIpMax.SetWindowText(_T("192.168.255.254"));
    m_editSrcIpStep.SetWindowText(_T("1"));

    m_btnStop.EnableWindow(FALSE);

    // Load RawPacketEngine
    if (!LoadRawPacketEngine())
    {
        m_stStatus.SetWindowText(_T("RawPacketEngine.dll not loaded"));
        m_btnStart.EnableWindow(FALSE);
    }
    else
    {
        if (RPE_Init && RPE_Init() == 0)
        {
            m_stStatus.SetWindowText(_T("Ready"));
            RefreshAdapterList();
        }
        else
        {
            m_stStatus.SetWindowText(_T("Npcap not ready"));
            m_btnStart.EnableWindow(FALSE);
        }
    }

    return TRUE;
}

void CRawPacketDlg::OnDestroy()
{
    if (m_isRunning)
        OnStop();
    if (m_nStatsTimer)
    {
        KillTimer(m_nStatsTimer);
        m_nStatsTimer = 0;
    }
    UnloadRawPacketEngine();
    CDialog::OnDestroy();
}

void CRawPacketDlg::LoadBuiltinPackets()
{
    struct { int id; LPCTSTR name; LPCTSTR os; LPCTSTR res; } pkts[] = {
        { 1, _T("MS08-067 NetAPI"), _T("Windows XP/2003"), _T("ms08_067") },
        { 2, _T("MS17-010 EternalBlue"), _T("Windows 7/2008"), _T("eternal_blue") },
        { 3, _T("CVE-2019-0708 BlueKeep"), _T("Windows 7/2008 R2"), _T("bluekeep") },
        { 4, _T("SMB Ghost (CVE-2020-0796)"), _T("Windows 10 v1903"), _T("smb_ghost") },
        { 5, _T("PrintNightmare"), _T("Windows All"), _T("print_nightmare") },
        { 6, _T("WinRM Lateral Movement"), _T("Windows 2012+"), _T("winrm_lat") },
        { 7, _T("ARP Spoof"), _T("All"), _T("arp_spoof") },
        { 8, _T("DNS Poison"), _T("All"), _T("dns_poison") },
        { 9, _T("LLMNR Poison"), _T("Windows All"), _T("llmnr_poison") },
        { 10, _T("SMB Relay"), _T("Windows"), _T("smb_relay") },
    };

    for (int i = 0; i < _countof(pkts); ++i)
    {
        int idx = m_listPackets.InsertItem(i, pkts[i].name);
        m_listPackets.SetCheck(idx, FALSE);
        BuiltinPacket bp;
        bp.id = pkts[i].id;
        bp.name = pkts[i].name;
        bp.targetOs = pkts[i].os;
        bp.resourceName = pkts[i].res;
        bp.selected = false;
        m_builtinPackets.push_back(bp);
    }
}

bool CRawPacketDlg::LoadRawPacketEngine()
{
    m_hRpeDll = LoadLibrary(_T("RawPacketEngine.dll"));
    if (!m_hRpeDll) return false;

    #define LOAD_FN(name) name = (pfn##name)GetProcAddress(m_hRpeDll, #name); if (!name) { UnloadRawPacketEngine(); return false; }

    LOAD_FN(RPE_Init);
    LOAD_FN(RPE_Cleanup);
    LOAD_FN(RPE_GetAdapterCount);
    LOAD_FN(RPE_GetAdapterInfo);
    LOAD_FN(RPE_SelectAdapter);
    LOAD_FN(RPE_AddStream);
    LOAD_FN(RPE_ClearStreams);
    LOAD_FN(RPE_SetRateConfig);
    LOAD_FN(RPE_Start);
    LOAD_FN(RPE_Stop);
    LOAD_FN(RPE_GetStats);
    return true;
}

void CRawPacketDlg::UnloadRawPacketEngine()
{
    if (m_hRpeDll)
    {
        FreeLibrary(m_hRpeDll);
        m_hRpeDll = NULL;
    }
}

void CRawPacketDlg::RefreshAdapterList()
{
    m_cbAdapter.ResetContent();
    m_adapterNames.RemoveAll();
    m_adapterIps.RemoveAll();

    if (!RPE_GetAdapterCount) return;
    int count = RPE_GetAdapterCount();
    for (int i = 0; i < count; ++i)
    {
        char name[512] = {0};
        char ip[64] = {0};
        RPE_GetAdapterInfo(i, name, sizeof(name), ip, sizeof(ip));
        CString display;
        display.Format(_T("[%d] %S (%S)"), i, name, ip);
        m_cbAdapter.AddString(display);
        m_adapterNames.Add(CString(name));
        m_adapterIps.Add(CString(ip));
    }
    if (count > 0)
    {
        m_cbAdapter.SetCurSel(0);
        m_selectedAdapterIndex = 0;
        RPE_SelectAdapter(0);
    }
}

void CRawPacketDlg::OnCbnSelchangeAdapter()
{
    int sel = m_cbAdapter.GetCurSel();
    if (sel >= 0 && RPE_SelectAdapter)
    {
        m_selectedAdapterIndex = sel;
        RPE_SelectAdapter(sel);
    }
}

void CRawPacketDlg::OnBnClickedStart()
{
    if (m_isRunning) return;
    OnStart();
}

void CRawPacketDlg::OnStart()
{
    if (!RPE_ClearStreams) return;

    // Update packet selections
    for (int i = 0; i < (int)m_builtinPackets.size(); ++i)
        m_builtinPackets[i].selected = m_listPackets.GetCheck(i) != FALSE;

    RPE_ClearStreams();

    // For now, add placeholder streams. Real implementation would load .bin files.
    // We'll add a simple ARP frame as placeholder for each selected packet.
    int streamCount = 0;
    for (size_t i = 0; i < m_builtinPackets.size(); ++i)
    {
        if (!m_builtinPackets[i].selected) continue;
        // Placeholder: 64-byte broadcast ARP frame
        BYTE arpFrame[] = {
            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0x00,0x11,0x22,0x33,0x44,0x55, 0x08,0x06,
            0x00,0x01, 0x08,0x00, 0x06,0x04, 0x00,0x01,
            0x00,0x11,0x22,0x33,0x44,0x55, 0xC0,0xA8,0x01,0x64,
            0x00,0x00,0x00,0x00,0x00,0x00, 0xC0,0xA8,0x01,0x01,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00
        };
        CStringA name(m_builtinPackets[i].name);
        RPE_AddStream(arpFrame, sizeof(arpFrame), name, NULL, 0, 0);
        streamCount++;
    }

    if (streamCount == 0)
    {
        AfxMessageBox(_T("Please select at least one attack packet"));
        return;
    }

    // Read speed config
    CString strSpeed;
    m_editSpeedValue.GetWindowText(strSpeed);
    long long speedValue = _ttoi64(strSpeed);
    int speedMode = m_cbSpeedMode.GetCurSel(); // 0=PPS, 1=Interval, 2=MaxBps
    int sendMode = m_cbSendMode.GetCurSel();   // 0=Continuous, 1=Fixed
    CString strBurst;
    m_editBurstCount.GetWindowText(strBurst);
    long long burstCount = _ttoi64(strBurst);

    if (RPE_SetRateConfig)
        RPE_SetRateConfig(speedMode, speedValue, sendMode, burstCount);

    if (RPE_Start && RPE_Start() == 0)
    {
        m_isRunning = true;
        m_btnStart.EnableWindow(FALSE);
        m_btnStop.EnableWindow(TRUE);
        m_stStatus.SetWindowText(_T("Running..."));
        m_nStatsTimer = SetTimer(1, 1000, NULL);
    }
    else
    {
        m_stStatus.SetWindowText(_T("Start failed"));
    }
}

void CRawPacketDlg::OnBnClickedStop()
{
    if (!m_isRunning) return;
    OnStop();
}

void CRawPacketDlg::OnStop()
{
    if (m_nStatsTimer)
    {
        KillTimer(m_nStatsTimer);
        m_nStatsTimer = 0;
    }
    if (RPE_Stop)
        RPE_Stop();
    m_isRunning = false;
    m_btnStart.EnableWindow(TRUE);
    m_btnStop.EnableWindow(FALSE);
    m_stStatus.SetWindowText(_T("Stopped"));
    UpdateStats();
}

void CRawPacketDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1 && m_isRunning)
        UpdateStats();
    CDialog::OnTimer(nIDEvent);
}

void CRawPacketDlg::UpdateStats()
{
    if (!RPE_GetStats) return;
    unsigned long long total = 0, bytes = 0, fail = 0;
    RPE_GetStats(&total, &bytes, &fail);

    CString s;
    s.Format(_T("%llu"), bytes * 8); // bps
    m_stBps.SetWindowText(s);

    s.Format(_T("%llu"), total); // pps approx
    m_stPps.SetWindowText(s);

    s.Format(_T("%llu"), total);
    m_stTotal.SetWindowText(s);

    s.Format(_T("%llu"), fail);
    m_stFail.SetWindowText(s);
}
