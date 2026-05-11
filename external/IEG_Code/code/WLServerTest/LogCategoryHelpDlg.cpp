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

    m_listHelp.InsertColumn(0, _T("分类名称"),    LVCFMT_LEFT,  130);
    m_listHelp.InsertColumn(1, _T("项目/通道"),   LVCFMT_LEFT,   80);
    m_listHelp.InsertColumn(2, _T("EPS规格"),     LVCFMT_LEFT,   60);
    m_listHelp.InsertColumn(3, _T("说明"),         LVCFMT_LEFT,  600);

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
        // 项目类型说明（标题行）
        { _T("【项目类型说明】"),  _T(""), _T(""), _T("IEG=传统安全管控（HTTPS短连接, ≤100EPS）; EDR=端点检测响应（TCP长连接, 最高6000EPS）。切换项目类型不会自动取消勾选，请手动确认勾选与项目匹配。") },
        { _T("【颜色含义】"),      _T(""), _T(""), _T("黑色=IEG/EDR通用; 蓝色=IEG专属; 绿色=EDR专属。") },

        // === 通用类（IEG/EDR） ===
        { _T("客户端操作"),         _T("IEG/EDR HTTPS"), _T("≤100"), _T("用户登录/注销/重启等客户端操作记录。接口 /USM/clientLog.do。") },
        { _T("操作系统"),           _T("IEG/EDR HTTPS"), _T("≤100"), _T("系统关机/重启/启动等OS事件。接口 /USM/clientOSLog.do。") },
        { _T("非法外联"),           _T("IEG/EDR HTTPS"), _T("≤100"), _T("违规外联或拨号事件。接口 /USM/illegalConnLog.do。") },
        { _T("文件保护"),           _T("IEG/EDR HTTPS"), _T("≤100"), _T("重要文件被篡改/删除告警。接口 /USM/dataProtectLog.do。") },
        { _T("强制访问控制"),       _T("IEG/EDR HTTPS"), _T("≤100"), _T("MAC策略违规行为。") },
        { _T("病毒告警"),           _T("IEG/EDR HTTPS"), _T("≤100"), _T("杀毒引擎检测到病毒文件。接口 /USM/virusLog.do。") },

        // === USB/网口（IEG重点区） ===
        { _T("U盘告警(老版本)"),   _T("IEG HTTPS"), _T("≤100"), _T("旧版通用U盘禁用告警（UsbType=2, LogContent=\"U盘使用被禁止\"），模拟旧版IEG客户端上报行为。当前版本IEG源码从UsbType=4开始（外设控制9种子类），不再产生UsbType=2，仅用于验证服务端向后兼容性。接口 /USM/clientULog.do。") },
        { _T("外设控制(9种)"),     _T("IEG HTTPS"), _T("≤100"), _T("IEG新版精细化告警，每种外设有专属UsbType值（CDROM=4, 蓝牙=6, 串口=7…），服务端可精确区分被禁设备种类。与\"U盘告警(老版本)\"共用 /USM/clientULog.do，但UsbType字段不同。推荐：测试IEG外设策略时优先勾选具体子类，不必再勾\"U盘告警(老版本)\"。") },
        { _T("USB访问告警"),       _T("IEG HTTPS"), _T("≤100"), _T("U盘插入后对文件进行违规操作（非法读写）时产生。接口 /USM/clientUSBLog.do。") },
        { _T("U盘插拔"),           _T("IEG HTTPS"), _T("≤100"), _T("U盘物理插入/拔出动作本身的上报，V3R2新增。接口 /USM/hotplugDevLog.do。共用 CMDID=204, CMDVER=1，数据在CMDContent数组中。") },
        { _T("网口Up/Down"),       _T("IEG HTTPS"), _T("≤100"), _T("网卡接口Up/Down状态变化事件。接口 /USM/hotplugDevLog.do, CMDID=204, CMDVER=4, OtherDevType=7。PlugEvent: 2=UP触发, 4=DOWN触发, 1/3=状态轮询。对应IEG源码 WLNetAdapterEvent.cpp。") },

        // === IEG专属其它 ===
        { _T("漏洞防护(IEG)"),     _T("IEG HTTPS"), _T("≤100"), _T("IEG漏洞防护拦截事件上报。") },
        { _T("进程审计(IEG)"),     _T("IEG HTTPS"), _T("≤100"), _T("IEG进程启动/停止审计记录。") },
        { _T("非白名单(IEG)"),     _T("IEG HTTPS"), _T("≤100"), _T("IEG非白名单进程运行告警。接口 /USM/clientNWLLog.do。") },
        { _T("白名单防篡改(IEG)"), _T("IEG HTTPS"), _T("≤100"), _T("白名单文件被篡改告警。") },
        { _T("注册表保护"),         _T("IEG HTTPS"), _T("≤100"), _T("关键注册表被篡改告警。接口 /USM/sysProtectLog.do。") },

        // === EDR专属 ===
        { _T("防火墙"),             _T("EDR HTTPS"), _T("≤100"), _T("操作系统防火墙拦截事件。") },
        { _T("系统防护(EDR)"),     _T("EDR HTTPS"), _T("≤100"), _T("EDR系统防护拦截事件。") },
        { _T("进程启动(EDR)"),     _T("EDR TCP长连接"), _T("≤6000"), _T("EDR威胁检测：进程创建事件（长连接高频）。") },
        { _T("注册表访问(EDR)"),   _T("EDR TCP长连接"), _T("≤6000"), _T("EDR威胁检测：注册表访问事件（长连接高频）。") },
        { _T("文件访问(EDR)"),     _T("EDR TCP长连接"), _T("≤6000"), _T("EDR威胁检测：文件访问事件（长连接高频）。") },
        { _T("DLL加载(EDR)"),      _T("EDR TCP长连接"), _T("≤6000"), _T("EDR威胁检测：DLL加载事件（长连接高频）。") },
        { _T("操作系统日志(IEG)"), _T("IEG TCP长连接"), _T("≤6000"), _T("IEG OS长连接日志（TCP威胁通道）。") },

        // === 典型场景建议 ===
        { _T("【场景A】"),  _T("IEG综合压测"), _T(""), _T("项目类型选IEG，勾选黑色+蓝色分类。外设控制选具体子类（如禁USB接口+禁蓝牙），无需再勾\"U盘告警(老版本)\"。") },
        { _T("【场景B】"),  _T("EDR威胁压测"), _T(""), _T("项目类型选EDR，勾选绿色威胁检测分类（进程启动/文件访问/DLL加载等）。TCP长连接模式，平台规格最高6000 EPS。") },
        { _T("【场景C】"),  _T("U盘全链路"),   _T(""), _T("同时勾\"U盘插拔\"+\"USB访问告警\"，验证服务端两条接口处理能力。") },
        { _T("【场景D】"),  _T("老版IEG兼容"), _T(""), _T("勾选\"U盘告警(老版本)\"（UsbType=2），模拟旧版客户端验证服务端向后兼容性。") },
        { _T("【场景E】"),  _T("网口管控"),    _T(""), _T("勾选\"网口Up/Down\"，模拟终端网卡接口上下线事件（CMDVER=4, OtherDevType=7）。可与\"U盘插拔\"同时勾选共同压测 /USM/hotplugDevLog.do。") },
    };

    for (int i = 0; i < _countof(cats); ++i)
    {
        int idx = m_listHelp.InsertItem(i, cats[i].name);
        m_listHelp.SetItemText(idx, 1, cats[i].type);
        m_listHelp.SetItemText(idx, 2, cats[i].eps);
        m_listHelp.SetItemText(idx, 3, cats[i].desc);
    }
}
