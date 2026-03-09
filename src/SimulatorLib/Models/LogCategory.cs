namespace SimulatorLib.Models;

/// <summary>
/// 日志分类枚举，对齐原项目 external/IEG_Code/code/WLMainDataHandle/AccessServer.h 的 em_LogType。
/// </summary>
public enum LogCategory
{
    /// <summary>
    /// 进程控制日志
    /// </summary>
    Process = 0,

    /// <summary>
    /// USB日志（CDROM、WIFi、蓝牙、串口等使用禁止控制日志）
    /// </summary>
    Usb = 1,

    /// <summary>
    /// 用户行为（客户端操作）
    /// </summary>
    Admin = 2,

    /// <summary>
    /// 访问控制（强制访问控制）
    /// </summary>
    HostDefence = 3,

    /// <summary>
    /// 防火墙日志
    /// </summary>
    FireWall = 4,

    /// <summary>
    /// 非法外联日志
    /// </summary>
    Illegal = 5,

    /// <summary>
    /// 漏洞防护
    /// </summary>
    VulDefense = 6,

    /// <summary>
    /// 安全商店（软件安装追踪）
    /// </summary>
    SafetyStore = 7,

    /// <summary>
    /// USB访问告警日志（U盘的文件操作违权告警）
    /// </summary>
    UsbWarning = 8,

    /// <summary>
    /// Udisk 插拔或插入告警（V3R2）
    /// </summary>
    UDiskPlug = 9,

    /// <summary>
    /// 病毒扫描日志（病毒告警）
    /// </summary>
    VPScan = 10,

    /// <summary>
    /// 威胁伪装告警（威胁数据采集）
    /// </summary>
    TFWarning = 11,

    /// <summary>
    /// 数据保护（文件保护、白名单防篡改）【EDR项目专属】
    /// </summary>
    DP = 12,

    /// <summary>
    /// 系统加固（系统防护）【EDR项目专属】
    /// </summary>
    SysGuard = 13,

    /// <summary>
    /// 系统资源告警日志（操作系统）
    /// </summary>
    OSResource = 14,

    /// <summary>
    /// 注册表保护
    /// </summary>
    RegProtect = 15,

    // ---- 外设控制子类（共享 /USM/clientULog.do + CMDID=204，区别仅在 UsbType 字段值）----

    /// <summary>禁用USB接口（UDISK_LOG_TYPE_USBPORT=15）</summary>
    ExtDevUsbPort = 16,

    /// <summary>禁用手机/平板（UDISK_LOG_TYPE_WPD=14）</summary>
    ExtDevWpd = 17,

    /// <summary>禁用CDROM（UDISK_LOG_TYPE_CDROM=4）</summary>
    ExtDevCdrom = 18,

    /// <summary>禁用无线网卡（UDISK_LOG_TYPE_WIRELESS=5）</summary>
    ExtDevWlan = 19,

    /// <summary>禁用USB网卡（UDISK_LOG_TYPE_USB_ETHERNET_ADAPTER=20）</summary>
    ExtDevUsbEthernet = 20,

    /// <summary>禁用软盘（UDISK_LOG_TYPE_FD=13）</summary>
    ExtDevFloppy = 21,

    /// <summary>禁用蓝牙（UDISK_LOG_TYPE_BLUETOOTH=6）</summary>
    ExtDevBluetooth = 22,

    /// <summary>禁用串口（UDISK_LOG_TYPE_SERIALPORT=7）</summary>
    ExtDevSerial = 23,

    /// <summary>禁用并口（UDISK_LOG_TYPE_PARALLELPORT=8）</summary>
    ExtDevParallel = 24,

    // ---- 威胁检测子类（共享 TCP 长连接 + PtProtocol + CMDID=21，区别仅在 EventType 字段） ----

    /// <summary>威胁检测-注册表访问事件（EDR）THREAT_LOG_TYPE_REG, EventType=63</summary>
    ThreatRegAccess = 25,

    /// <summary>威胁检测-文件访问事件（EDR）THREAT_LOG_TYPE_FILE, EventType=62</summary>
    ThreatFileAccess = 26,

    /// <summary>威胁检测-操作系统日志（IEG）WLOSEventLog, EventType=65</summary>
    ThreatOsEvent = 27,

    /// <summary>威胁检测-DLL加载/跨进程（EDR）THREAT_LOG_TYPE_PROC, EventType=61</summary>
    ThreatDllLoad = 28,

    // ---- 插拔 & 网口事件子类（共享 /USM/hotplugDevLog.do + CMDID=204，区别仅在 CMDVER 和 CMDContentOtherDev.OtherDevType） ----

    /// <summary>
    /// 网口 Up/Down 事件（CMDVER=4, OtherDevType=7）。
    /// 与 UDiskPlug 共用同一 CMDID（204），通过 CMDVER=4 + CMDContentOtherDev 区分。
    /// PlugEvent: 1=UP状态轮询, 2=UP变化触发, 3=DOWN状态轮询, 4=DOWN变化触发。
    /// 对应 IEG 源码: WLCUDisk/WLNetAdapterEvent.cpp → NetAdapterLog_GetJsonByVector
    /// </summary>
    NetAdapterEvent = 29,
}

/// <summary>
/// 日志分类辅助类，提供友好的中文名称映射
/// </summary>
public static class LogCategoryHelper
{
    /// <summary>
    /// 获取日志分类的中文显示名称
    /// </summary>
    public static string GetDisplayName(LogCategory category)
    {
        return category switch
        {
            LogCategory.Process => "进程控制",
            LogCategory.Usb => "USB设备",
            LogCategory.Admin => "客户端操作",
            LogCategory.HostDefence => "强制访问控制",
            LogCategory.FireWall => "防火墙",
            LogCategory.Illegal => "非法外联",
            LogCategory.VulDefense => "漏洞防护",
            LogCategory.SafetyStore => "软件安装",
            LogCategory.UsbWarning => "USB访问告警",
            LogCategory.UDiskPlug => "U盘插拔",
            LogCategory.VPScan => "病毒告警",
            LogCategory.TFWarning => "威胁检测-进程启动",
            LogCategory.ThreatRegAccess => "威胁检测-注册表访问",
            LogCategory.ThreatFileAccess => "威胁检测-文件访问",
            LogCategory.ThreatOsEvent => "威胁检测-系统日志",
            LogCategory.ThreatDllLoad => "威胁检测-DLL加载",
            LogCategory.DP => "文件保护",
            LogCategory.SysGuard => "系统防护",
            LogCategory.OSResource => "操作系统",
            LogCategory.RegProtect => "注册表保护",
            LogCategory.ExtDevUsbPort => "禁USB接口",
            LogCategory.ExtDevWpd => "禁手机平板",
            LogCategory.ExtDevCdrom => "禁CDROM",
            LogCategory.ExtDevWlan => "禁无线网卡",
            LogCategory.ExtDevUsbEthernet => "禁USB网卡",
            LogCategory.ExtDevFloppy => "禁软盘",
            LogCategory.ExtDevBluetooth => "禁蓝牙",
            LogCategory.ExtDevSerial => "禁串口",
            LogCategory.ExtDevParallel => "禁并口",
            LogCategory.NetAdapterEvent => "网口Up/Down",
            _ => "未知",
        };
    }

    /// <summary>
    /// 从UI显示名称解析日志分类
    /// </summary>
    public static LogCategory? ParseDisplayName(string displayName)
    {
        return displayName switch
        {
            "进程控制" => LogCategory.Process,
            "进程审计" => LogCategory.Process, // 别名
            "非白名单" => LogCategory.Process, // 对应进程白名单告警 (WARNING_LOG_STRUCT, OPTYPE_PWL_CONTROL)
            "非白名单告警" => LogCategory.Process, // 别名
            "白名单防篡改" => LogCategory.Process, // 对应白名单文件防篡改 (WARNING_LOG_STRUCT, OPTYPE_PWL_MODIFY_FILE)
            "USB设备" => LogCategory.Usb,
            "客户端操作" => LogCategory.Admin,
            "强制访问控制" => LogCategory.HostDefence,
            "防火墙" => LogCategory.FireWall,
            "非法外联" => LogCategory.Illegal,
            "漏洞防护" => LogCategory.VulDefense,
            "软件安装" => LogCategory.SafetyStore,
            "USB访问告警" => LogCategory.UsbWarning,
            "U盘插拔" => LogCategory.UDiskPlug,
            "病毒告警" => LogCategory.VPScan,
            "威胁数据采集" => LogCategory.TFWarning, // 向后兼容旧名称
            "威胁检测-进程启动" => LogCategory.TFWarning,
            "威胁检测-注册表访问" => LogCategory.ThreatRegAccess,
            "威胁检测-文件访问" => LogCategory.ThreatFileAccess,
            "威胁检测-系统日志" => LogCategory.ThreatOsEvent,
            "威胁检测-DLL加载" => LogCategory.ThreatDllLoad,
            "文件保护" => LogCategory.HostDefence, // IEG: HostDefence with DetailLogTypeLevel2=1
            "系统防护" => LogCategory.SysGuard, // EDR专属
            "系统加固" => LogCategory.SysGuard, // 别名，EDR专属
            "操作系统" => LogCategory.OSResource,
            "注册表保护" => LogCategory.RegProtect, // IEG: HostDefence with DetailLogTypeLevel2=2
            // 外设控制子类
            "禁USB接口" => LogCategory.ExtDevUsbPort,
            "禁手机平板" => LogCategory.ExtDevWpd,
            "禁CDROM" => LogCategory.ExtDevCdrom,
            "禁无线网卡" => LogCategory.ExtDevWlan,
            "禁USB网卡" => LogCategory.ExtDevUsbEthernet,
            "禁软盘" => LogCategory.ExtDevFloppy,
            "禁蓝牙" => LogCategory.ExtDevBluetooth,
            "禁串口" => LogCategory.ExtDevSerial,
            "禁并口" => LogCategory.ExtDevParallel,
            // 插拔 & 网口事件子类
            "网口Up/Down" => LogCategory.NetAdapterEvent,
            _ => null,
        };
    }

    /// <summary>
    /// 获取外设控制子类对应的 UDISK_LOG_TYPE 整数值（ExtDevLog_GetJsonByVector 的 UsbType 字段）。
    /// 非外设控制类型返回 null。
    /// </summary>
    public static int? GetExtDevUsbType(LogCategory category) => category switch
    {
        LogCategory.ExtDevUsbPort      => 15,  // UDISK_LOG_TYPE_USBPORT
        LogCategory.ExtDevWpd          => 14,  // UDISK_LOG_TYPE_WPD
        LogCategory.ExtDevCdrom        => 4,   // UDISK_LOG_TYPE_CDROM
        LogCategory.ExtDevWlan         => 5,   // UDISK_LOG_TYPE_WIRELESS
        LogCategory.ExtDevUsbEthernet  => 20,  // UDISK_LOG_TYPE_USB_ETHERNET_ADAPTER
        LogCategory.ExtDevFloppy       => 13,  // UDISK_LOG_TYPE_FD
        LogCategory.ExtDevBluetooth    => 6,   // UDISK_LOG_TYPE_BLUETOOTH
        LogCategory.ExtDevSerial       => 7,   // UDISK_LOG_TYPE_SERIALPORT
        LogCategory.ExtDevParallel     => 8,   // UDISK_LOG_TYPE_PARALLELPORT
        _ => null,
    };

    /// <summary>判断是否为外设控制子类（UsbType 告警）</summary>
    public static bool IsExtDevCategory(LogCategory category) => GetExtDevUsbType(category).HasValue;

    /// <summary>
    /// 获取外设控制子类对应的标准日志描述（LogContent 字段）。
    /// 对齐 IEG 源码注释: "U盘使用被禁止,cdrom使用被禁止,wifi使用被禁止"
    /// </summary>
    public static string GetExtDevLogContent(LogCategory category) => category switch
    {
        LogCategory.ExtDevUsbPort      => "USB接口使用被禁止",
        LogCategory.ExtDevWpd          => "移动设备使用被禁止",
        LogCategory.ExtDevCdrom        => "CDROM使用被禁止",
        LogCategory.ExtDevWlan         => "wifi使用被禁止",
        LogCategory.ExtDevUsbEthernet  => "USB网卡使用被禁止",
        LogCategory.ExtDevFloppy       => "软盘使用被禁止",
        LogCategory.ExtDevBluetooth    => "蓝牙使用被禁止",
        LogCategory.ExtDevSerial       => "串口使用被禁止",
        LogCategory.ExtDevParallel     => "并口使用被禁止",
        _ => "设备使用被禁止",
    };

    /// <summary>
    /// 判断是否为威胁检测类别（5种事件均使用 TCP 长连接 + PtProtocol 封装，CMDID=21）
    /// </summary>
    public static bool IsThreatDataCategory(LogCategory category)
    {
        return category == LogCategory.TFWarning
            || category == LogCategory.ThreatRegAccess
            || category == LogCategory.ThreatFileAccess
            || category == LogCategory.ThreatOsEvent
            || category == LogCategory.ThreatDllLoad;
    }

    /// <summary>
    /// 判断是否为仅支持HTTPS的类别（不支持UDP日志服务器）
    /// </summary>
    public static bool IsHttpsOnlyCategory(LogCategory category)
    {
        // 对齐原项目 AccessServer.cpp 的 SendLog2Server 逻辑：
        // DP, SysGuard, RegProtect 仅通过 HTTPS 发送，不支持 UDP 日志服务器
        return category == LogCategory.DP 
            || category == LogCategory.SysGuard 
            || category == LogCategory.RegProtect;
    }
}
