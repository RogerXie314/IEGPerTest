// LogCategoryHelpDlg.cpp
#include "stdafx.h"
#include "WLServerTest.h"
#include "LogCategoryHelpDlg.h"

IMPLEMENT_DYNAMIC(CLogCategoryHelpDlg, CDialog)

CLogCategoryHelpDlg::CLogCategoryHelpDlg(CWnd* pParent)
    : CDialog(CLogCategoryHelpDlg::IDD, pParent)
{
}

CLogCategoryHelpDlg::~CLogCategoryHelpDlg()
{
}

void CLogCategoryHelpDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC, m_listHelp);
}

BEGIN_MESSAGE_MAP(CLogCategoryHelpDlg, CDialog)
END_MESSAGE_MAP()

BOOL CLogCategoryHelpDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 设置列表列
    m_listHelp.InsertColumn(0, _T("分类名称"),    LVCFMT_LEFT, 140);
    m_listHelp.InsertColumn(1, _T("类型"),         LVCFMT_LEFT, 80);
    m_listHelp.InsertColumn(2, _T("典型EPS"),      LVCFMT_LEFT, 80);
    m_listHelp.InsertColumn(3, _T("说明"),         LVCFMT_LEFT, 300);

    PopulateList();
    return TRUE;
}

void CLogCategoryHelpDlg::PopulateList()
{
    struct LogCatInfo {
        LPCTSTR name;
        LPCTSTR type;
        LPCTSTR eps;
        LPCTSTR desc;
    };
    static const LogCatInfo cats[] = {
        { _T("客户端操作"),      _T("HTTPS"), _T("低"), _T("用户登录/注销/锁屏等客户端操作") },
        { _T("操作系统"),        _T("HTTPS"), _T("低"), _T("系统关机/重启/蓝屏等OS事件") },
        { _T("非法外联"),        _T("HTTPS"), _T("低"), _T("违规访问外网或拨号事件") },
        { _T("文件保护"),        _T("HTTPS"), _T("低"), _T("重要文件被篡改/删除告警") },
        { _T("注册表保护"),      _T("HTTPS"), _T("低"), _T("关键注册表项被篡改告警") },
        { _T("强制访问控制"),    _T("HTTPS"), _T("低"), _T("MAC策略违规行为") },
        { _T("病毒告警"),        _T("HTTPS"), _T("低"), _T("杀毒引擎检测到恶意文件") },
        { _T("U盘告警(老版本)"), _T("HTTPS"), _T("低"), _T("旧版U盘接入告警") },
        { _T("USB访问告警"),     _T("HTTPS"), _T("低"), _T("USB设备访问违规告警") },
        { _T("防火墙"),          _T("HTTPS"), _T("低"), _T("主机防火墙拦截事件") },
        { _T("漏洞防护(IEG)"),   _T("HTTPS"), _T("低"), _T("IEG漏洞防护拦截") },
        { _T("进程审计(IEG)"),   _T("HTTPS"), _T("低"), _T("IEG进程启动/停止审计") },
        { _T("非白名单(IEG)"),   _T("HTTPS"), _T("低"), _T("IEG非白名单程序运行告警") },
        { _T("白名单防篡改(IEG)"),_T("HTTPS"),_T("低"), _T("白名单文件被篡改告警") },
        { _T("系统防护(EDR)"),   _T("HTTPS"), _T("低"), _T("EDR系统防护拦截事件") },
        { _T("U盘插拔"),         _T("HTTPS"), _T("中"), _T("U盘插入/拔出事件") },
        { _T("网口Up/Down"),     _T("HTTPS"), _T("中"), _T("网络适配器状态变更") },
        { _T("禁USB接口"),       _T("HTTPS"), _T("低"), _T("外设控制：禁用USB接口") },
        { _T("禁手机平板"),      _T("HTTPS"), _T("低"), _T("外设控制：禁MTP/PTP设备") },
        { _T("禁CDROM"),         _T("HTTPS"), _T("低"), _T("外设控制：禁光驱") },
        { _T("禁无线网卡"),      _T("HTTPS"), _T("低"), _T("外设控制：禁WLAN适配器") },
        { _T("禁USB网卡"),       _T("HTTPS"), _T("低"), _T("外设控制：禁USB以太网卡") },
        { _T("禁软盘"),          _T("HTTPS"), _T("低"), _T("外设控制：禁软盘驱动器") },
        { _T("禁蓝牙"),          _T("HTTPS"), _T("低"), _T("外设控制：禁蓝牙设备") },
        { _T("禁串口"),          _T("HTTPS"), _T("低"), _T("外设控制：禁串口") },
        { _T("禁并口"),          _T("HTTPS"), _T("低"), _T("外设控制：禁并口") },
        { _T("进程启动(EDR)"),   _T("TCP"),   _T("~6000"), _T("EDR进程创建事件（长连接高速）") },
        { _T("注册表访问(EDR)"), _T("TCP"),   _T("~6000"), _T("EDR注册表访问事件（长连接高速）") },
        { _T("文件访问(EDR)"),   _T("TCP"),   _T("~6000"), _T("EDR文件访问事件（长连接高速）") },
        { _T("DLL加载(EDR)"),    _T("TCP"),   _T("~6000"), _T("EDR DLL加载事件（长连接高速）") },
        { _T("操作系统日志(IEG)"),_T("TCP"),  _T("~6000"), _T("IEG OS级别日志（TCP高速通道）") },
    };

    for (int i = 0; i < _countof(cats); ++i)
    {
        int idx = m_listHelp.InsertItem(i, cats[i].name);
        m_listHelp.SetItemText(idx, 1, cats[i].type);
        m_listHelp.SetItemText(idx, 2, cats[i].eps);
        m_listHelp.SetItemText(idx, 3, cats[i].desc);
    }
}
