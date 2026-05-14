#include "stdafx.h"
#include "WLServerTest.h"
#include "RawPacketDlg.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CRawPacketDlg, CDialog)

CRawPacketDlg::CRawPacketDlg(CWnd* pParent)
    : CDialog(CRawPacketDlg::IDD, pParent), m_hRpeDll(NULL), m_selectedAdapterIndex(-1), m_isRunning(false), m_nStatsTimer(0)
{
    RPE_Init = NULL; RPE_Cleanup = NULL; RPE_GetAdapterCount = NULL;
    RPE_GetAdapterInfo = NULL; RPE_SelectAdapter = NULL; RPE_AddStream = NULL;
    RPE_ClearStreams = NULL; RPE_SetRateConfig = NULL; RPE_Start = NULL;
    RPE_Stop = NULL; RPE_GetStats = NULL;
}

CRawPacketDlg::~CRawPacketDlg() { UnloadRawPacketEngine(); }

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
    DDX_Control(pDX, IDC_ST_RP_AVG_BPS, m_stAvgBps);
    DDX_Control(pDX, IDC_ST_RP_AVG_PPS, m_stAvgPps);
    DDX_Control(pDX, IDC_ST_RP_STATUS, m_stStatus);
    DDX_Control(pDX, IDC_ST_NPCAP, m_stNpcap);
}

BEGIN_MESSAGE_MAP(CRawPacketDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_RP_START, &CRawPacketDlg::OnBnClickedStart)
    ON_BN_CLICKED(IDC_BTN_RP_STOP, &CRawPacketDlg::OnBnClickedStop)
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_CB_ADAPTER, &CRawPacketDlg::OnCbnSelchangeAdapter)
END_MESSAGE_MAP()

static CString GetFriendlyName(const char* pcapName)
{
    CString s(pcapName);
    int gs = s.Find(_T('{'));
    int ge = s.Find(_T('}'));
    if (gs >= 0 && ge > gs)
    {
        CString guid = s.Mid(gs + 1, ge - gs - 1);
        guid.MakeUpper();
        HKEY hKey;
        CString regPath;
        regPath.Format(_T("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection"), guid);
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            TCHAR name[256] = {0};
            DWORD size = sizeof(name);
            if (RegQueryValueEx(hKey, _T("Name"), NULL, NULL, (LPBYTE)name, &size) == ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                CString friendly;
                friendly.Format(_T("[%d] %s"), 0, name);
                return friendly;
            }
            RegCloseKey(hKey);
        }
    }
    CString friendly;
    friendly.Format(_T("[%d] %S"), 0, pcapName);
    return friendly;
}

BOOL CRawPacketDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetWindowText(_T("RawPacket Attack Sender"));

    // Packet list
    m_listPackets.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listPackets.InsertColumn(0, _T("Packet"), LVCFMT_LEFT, 320);
    LoadBuiltinPackets();

    // Speed mode
    m_cbSpeedMode.AddString(_T("PPS"));
    m_cbSpeedMode.AddString(_T("Interval (ms)"));
    m_cbSpeedMode.AddString(_T("Max bps"));
    m_cbSpeedMode.SetCurSel(0);

    // Send mode
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
        m_stNpcap.SetWindowText(_T("RawPacketEngine.dll not found"));
        m_stStatus.SetWindowText(_T("DLL missing"));
        m_btnStart.EnableWindow(FALSE);
        return TRUE;
    }

    int initRet = RPE_Init ? RPE_Init() : -1;
    if (initRet == 0)
    {
        m_stNpcap.SetWindowText(_T("Npcap: ready"));
        m_stStatus.SetWindowText(_T("Ready"));
        RefreshAdapterList();
    }
    else if (initRet == -2)
    {
        m_stNpcap.SetWindowText(_T("Npcap: NOT INSTALLED"));
        m_stStatus.SetWindowText(_T("Npcap missing - install from https://npcap.com/"));
        m_btnStart.EnableWindow(FALSE);
        AfxMessageBox(_T("Npcap not detected!\n\nPlease install Npcap from https://npcap.com/\nthen restart this tool."), MB_ICONWARNING);
    }
    else
    {
        CString s; s.Format(_T("Npcap: init error %d"), initRet);
        m_stNpcap.SetWindowText(s);
        m_stStatus.SetWindowText(_T("Init failed"));
        m_btnStart.EnableWindow(FALSE);
    }

    return TRUE;
}

void CRawPacketDlg::OnDestroy()
{
    if (m_isRunning) OnStop();
    if (m_nStatsTimer) { KillTimer(m_nStatsTimer); m_nStatsTimer = 0; }
    UnloadRawPacketEngine();
    CDialog::OnDestroy();
}

void CRawPacketDlg::LoadBuiltinPackets()
{
    struct { int id; LPCTSTR name; LPCTSTR os; LPCTSTR res; } pkts[] = {
        { 1, _T("MS08-067 NetAPI"),         _T("Windows XP/2003"),      _T("ms08_067") },
        { 2, _T("MS17-010 EternalBlue"),   _T("Windows 7/2008"),       _T("eternal_blue") },
        { 3, _T("CVE-2019-0708 BlueKeep"), _T("Windows 7/2008 R2"),    _T("bluekeep") },
        { 4, _T("SMB Ghost (CVE-2020-0796)"), _T("Windows 10 v1903"), _T("smb_ghost") },
        { 5, _T("PrintNightmare"),          _T("Windows All"),          _T("print_nightmare") },
        { 6, _T("WinRM Lateral Movement"),  _T("Windows 2012+"),       _T("winrm_lat") },
        { 7, _T("ARP Spoof"),               _T("All"),                  _T("arp_spoof") },
        { 8, _T("DNS Poison"),              _T("All"),                  _T("dns_poison") },
        { 9, _T("LLMNR Poison"),            _T("Windows All"),          _T("llmnr_poison") },
        { 10, _T("SMB Relay"),              _T("Windows"),              _T("smb_relay") },
    };
    for (int i = 0; i < _countof(pkts); ++i)
    {
        CString text; text.Format(_T("%s  [%s]"), pkts[i].name, pkts[i].os);
        int idx = m_listPackets.InsertItem(i, text);
        m_listPackets.SetCheck(idx, FALSE);
        BuiltinPacket bp; bp.id = pkts[i].id; bp.name = pkts[i].name;
        bp.targetOs = pkts[i].os; bp.resourceName = pkts[i].res; bp.selected = false;
        m_builtinPackets.push_back(bp);
    }
}

bool CRawPacketDlg::LoadRawPacketEngine()
{
    m_hRpeDll = LoadLibrary(_T("RawPacketEngine.dll"));
    if (!m_hRpeDll) return false;
    #define LOAD(n) n = (pfn##n)GetProcAddress(m_hRpeDll, #n); if (!n) return false
    LOAD(RPE_Init); LOAD(RPE_Cleanup); LOAD(RPE_GetAdapterCount);
    LOAD(RPE_GetAdapterInfo); LOAD(RPE_SelectAdapter); LOAD(RPE_AddStream);
    LOAD(RPE_ClearStreams); LOAD(RPE_SetRateConfig); LOAD(RPE_Start);
    LOAD(RPE_Stop); LOAD(RPE_GetStats);
    return true;
}

void CRawPacketDlg::UnloadRawPacketEngine()
{
    if (m_hRpeDll) { FreeLibrary(m_hRpeDll); m_hRpeDll = NULL; }
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
        char name[512] = {0}, ip[64] = {0};
        RPE_GetAdapterInfo(i, name, sizeof(name), ip, sizeof(ip));
        CString friendly = GetFriendlyName(name);
        friendly.Replace(_T("[0]"), _T(""));
        CString display; display.Format(_T("#%d %s  (%S)"), i, friendly, ip);
        m_cbAdapter.AddString(display);
        m_adapterNames.Add(CString(name));
        m_adapterIps.Add(CString(ip));
    }
    if (count > 0)
    {
        m_cbAdapter.SetCurSel(0);
        m_selectedAdapterIndex = 0;
        if (RPE_SelectAdapter) RPE_SelectAdapter(0);
    }
}

void CRawPacketDlg::OnCbnSelchangeAdapter()
{
    int sel = m_cbAdapter.GetCurSel();
    if (sel >= 0 && RPE_SelectAdapter) { m_selectedAdapterIndex = sel; RPE_SelectAdapter(sel); }
}

void CRawPacketDlg::OnBnClickedStart() { if (!m_isRunning) OnStart(); }
void CRawPacketDlg::OnBnClickedStop()  { if (m_isRunning) OnStop(); }

void CRawPacketDlg::OnStart()
{
    if (!RPE_ClearStreams) return;
    for (int i = 0; i < (int)m_builtinPackets.size(); ++i)
        m_builtinPackets[i].selected = m_listPackets.GetCheck(i) != FALSE;
    RPE_ClearStreams();
    int streamCount = 0;
    for (size_t i = 0; i < m_builtinPackets.size(); ++i)
    {
        if (!m_builtinPackets[i].selected) continue;
        BYTE arpFrame[] = {
            0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0x00,0x11,0x22,0x33,0x44,0x55, 0x08,0x06,
            0x00,0x01, 0x08,0x00, 0x06,0x04, 0x00,0x01,
            0x00,0x11,0x22,0x33,0x44,0x55, 0xC0,0xA8,0x01,0x64,
            0x00,0x00,0x00,0x00,0x00,0x00, 0xC0,0xA8,0x01,0x01,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        };
        CStringA name(m_builtinPackets[i].name);
        RPE_AddStream(arpFrame, sizeof(arpFrame), name, NULL, 0, 0);
        streamCount++;
    }
    if (streamCount == 0) { AfxMessageBox(_T("Select at least one packet")); return; }

    CString strSpeed; m_editSpeedValue.GetWindowText(strSpeed);
    long long speedVal = _ttoi64(strSpeed);
    int speedMode = m_cbSpeedMode.GetCurSel();
    int sendMode = m_cbSendMode.GetCurSel();
    CString strBurst; m_editBurstCount.GetWindowText(strBurst);
    long long burstCnt = _ttoi64(strBurst);
    if (RPE_SetRateConfig) RPE_SetRateConfig(speedMode, speedVal, sendMode, burstCnt);

    if (RPE_Start && RPE_Start() == 0)
    {
        m_isRunning = true;
        m_btnStart.EnableWindow(FALSE);
        m_btnStop.EnableWindow(TRUE);
        m_stStatus.SetWindowText(_T("Running..."));
        m_nStatsTimer = SetTimer(1, 1000, NULL);
    }
    else { m_stStatus.SetWindowText(_T("Start failed")); }
}

void CRawPacketDlg::OnStop()
{
    if (m_nStatsTimer) { KillTimer(m_nStatsTimer); m_nStatsTimer = 0; }
    if (RPE_Stop) RPE_Stop();
    m_isRunning = false;
    m_btnStart.EnableWindow(TRUE);
    m_btnStop.EnableWindow(FALSE);
    m_stStatus.SetWindowText(_T("Stopped"));
    UpdateStats();
}

void CRawPacketDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1 && m_isRunning) UpdateStats();
    CDialog::OnTimer(nIDEvent);
}

void CRawPacketDlg::UpdateStats()
{
    if (!RPE_GetStats) return;
    unsigned long long total = 0, bytes = 0, fail = 0;
    RPE_GetStats(&total, &bytes, &fail);
    CString s;
    s.Format(_T("%llu"), bytes * 8); m_stBps.SetWindowText(s);
    s.Format(_T("%llu"), total); m_stPps.SetWindowText(s);
    s.Format(_T("%llu"), total); m_stTotal.SetWindowText(s);
    s.Format(_T("%llu"), fail); m_stFail.SetWindowText(s);
    s.Format(_T("%llu"), total > 0 ? bytes * 8 / total : 0); m_stAvgBps.SetWindowText(s);
    s.Format(_T("%llu"), total); m_stAvgPps.SetWindowText(s);
}
