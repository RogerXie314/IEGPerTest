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
            LogCategory.TFWarning => "威胁数据采集",
            LogCategory.DP => "文件保护",
            LogCategory.SysGuard => "系统防护",
            LogCategory.OSResource => "操作系统",
            LogCategory.RegProtect => "注册表保护",
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
            "威胁数据采集" => LogCategory.TFWarning,
            "文件保护" => LogCategory.HostDefence, // IEG: HostDefence with DetailLogTypeLevel2=1
            "系统防护" => LogCategory.SysGuard, // EDR专属
            "系统加固" => LogCategory.SysGuard, // 别名，EDR专属
            "操作系统" => LogCategory.OSResource,
            "注册表保护" => LogCategory.RegProtect, // IEG: HostDefence with DetailLogTypeLevel2=2
            _ => null,
        };
    }

    /// <summary>
    /// 判断是否为威胁数据采集类别（需要特殊的PT协议封装）
    /// </summary>
    public static bool IsThreatDataCategory(LogCategory category)
    {
        return category == LogCategory.TFWarning;
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
