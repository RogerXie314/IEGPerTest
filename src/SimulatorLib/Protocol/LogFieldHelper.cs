using System;
using System.Collections.Generic;

namespace SimulatorLib.Protocol;

/// <summary>
/// 辅助类：生成日志 JSON 中平台需要的扩展字段（基于 docs/JSON/log.log 真实日志格式）
/// </summary>
public static class LogFieldHelper
{
    // 日志分类枚举（对应UI的17种日志类型）
    public enum LogCategory
    {
        ClientOps,          // 客户端操作
        VulnProtect,        // 漏洞防护
        ProcessControl,     // 进程控制
        Os,                 // 操作系统
        Outbound,           // 非法外联
        Threat,             // 威胁检测
        NonWhitelist,       // 非白名单
        WhitelistTamper,    // 白名单防篡改
        FileProtect,        // 文件保护
        MandatoryAccess,    // 强制访问控制
        ProcessAudit,       // 进程审计
        VirusAlert,         // 病毒告警
        RegProtect,         // 注册表保护（IEG特有）
        Usb,                // USB外设（IEG特有）
        UsbWarning,         // USB告警（IEG特有）
        UDiskPlug,          // U盘插拔（IEG特有）
        Firewall,           // 防火墙（EDR特有）
        SysGuard,           // 系统防护（EDR特有）
    }

    /// <summary>
    /// 根据日志分类获取完整的字段集合
    /// </summary>
    public static Dictionary<string, object?> GetCategoryFields(LogCategory category, bool isIeg = true)
    {
        return category switch
        {
            LogCategory.FileProtect => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "访问控制事件",
                ["usmeventtype_en"] = "Access Control Events",
                ["usmalarmtype"] = "文件保护事件",
                ["usmalarmtype_en"] = "File protection events",
                ["eventname"] = "访问控制事件",
                ["eventname_en"] = "Access Control Events",
                ["eventcategory"] = "/攻击入侵/文件保护异常",
                ["eventcategory_en"] = "/Attack intrusion/Document protection exception",
                ["eventlevel"] = "一般",
                ["eventlevel_en"] = "General",
                ["cregex"] = "访问控制事件",
                ["cregex_en"] = "Access Control Events",
                ["operatoraction"] = "3",  // 3=修改
                ["threat"] = true,
            },
            
            LogCategory.MandatoryAccess => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "访问控制事件",
                ["usmeventtype_en"] = "Access Control Events",
                ["usmalarmtype"] = "强制访问控制事件",
                ["usmalarmtype_en"] = "Forced access control events",
                ["eventname"] = "访问控制事件",
                ["eventname_en"] = "Access Control Events",
                ["eventcategory"] = "/安全预警/尝试系统调用",
                ["eventcategory_en"] = "/Security alert/Attempted system calls",
                ["eventlevel"] = "一般",
                ["eventlevel_en"] = "General",
                ["cregex"] = "访问控制事件",
                ["cregex_en"] = "Access Control Events",
                ["operatoraction"] = "3",
                ["threat"] = true,
            },
            
            LogCategory.RegProtect => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "访问控制事件",
                ["usmeventtype_en"] = "Access Control Events",
                ["usmalarmtype"] = "注册表保护事件",
                ["usmalarmtype_en"] = "Registry protection events",
                ["eventname"] = "访问控制事件",
                ["eventname_en"] = "Access Control Events",
                ["eventcategory"] = "/攻击入侵/注册表保护异常",
                ["eventcategory_en"] = "/Attack intrusion/Registry protection exception",
                ["eventlevel"] = "一般",
                ["eventlevel_en"] = "General",
                ["cregex"] = "访问控制事件",
                ["cregex_en"] = "Access Control Events",
                ["operatoraction"] = "3",
                ["threat"] = true,
            },
            
            LogCategory.NonWhitelist => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "程序报警事件",
                ["usmeventtype_en"] = "Program alarm events",
                ["usmalarmtype"] = "非法程序启动事件",
                ["usmalarmtype_en"] = "Illegal program initiation events",
                ["eventname"] = "程序报警事件",
                ["eventname_en"] = "Program alarm events",
                ["eventcategory"] = "/业务异常/非法程序启动",
                ["eventcategory_en"] = "/Business abnormality/Illegal program initiation",
                ["eventlevel"] = "严重",
                ["eventlevel_en"] = "Serious",
                ["cregex"] = "程序报警事件",
                ["cregex_en"] = "Program alarm events",
                ["threat"] = true,
            },
            
            LogCategory.WhitelistTamper => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "程序报警事件",
                ["usmeventtype_en"] = "Program alarm events",
                ["usmalarmtype"] = "白名单篡改事件",
                ["usmalarmtype_en"] = "Whitelist tampering events",
                ["eventname"] = "程序报警事件",
                ["eventname_en"] = "Program alarm events",
                ["eventcategory"] = "/业务异常/白名单篡改事件",
                ["eventcategory_en"] = "/Business abnormality/Whitelist tampering events",
                ["eventlevel"] = "严重",
                ["eventlevel_en"] = "Serious",
                ["cregex"] = "程序报警事件",
                ["cregex_en"] = "Program alarm events",
                ["threat"] = true,
            },
            
            LogCategory.VirusAlert => new Dictionary<string, object?>
            {
                // 病毒告警使用不同的JSON结构，字段较少
                ["threat"] = true,
            },
            
            LogCategory.Usb => new Dictionary<string, object?>
            {
                ["usmeventtype"] = "非法外设接入",
                ["usmeventtype_en"] = "Peripheral operation log",
                ["usmalarmtype"] = "设备插拔事件",
                ["usmalarmtype_en"] = "device plugging and unplugging incident",
                ["eventname"] = "非法外设接入事件",
                ["eventname_en"] = "Peripheral operation log",
                ["eventcategory"] = "/用户行为/非法外设接入",
                ["eventcategory_en"] = "/User behavior/Peripheral operation log",
                ["eventlevel"] = "重要",
                ["eventlevel_en"] = "Important",
                ["cregex"] = "非法外设接入",
                ["plugEvent"] = new[] { "插入", "stick" },
                ["threat"] = true,
            },
            
            _ => new Dictionary<string, object?>
            {
                ["eventlevel"] = "一般",
                ["eventlevel_en"] = "General",
                ["threat"] = false,
            }
        };
    }

    /// <summary>
    /// 生成通用字段（所有日志类型都需要）
    /// </summary>
    public static Dictionary<string, object?> GetCommonFields(
        string clientIp,
        string clientName,
        string machineCode,
        string os,
        int factoryId = 3,
        int safeDeviceId = 12,
        bool isIeg = true)
    {
        var clientType = isIeg ? "终端防护" : "终端防护";  // 可根据项目类型区分
        return new Dictionary<string, object?>
        {
            ["assetIp"] = clientIp,
            ["clientip"] = clientIp,
            ["caffectedip"] = clientIp,
            ["clientname"] = clientName,
            ["machinecode"] = machineCode,
            ["os"] = os,
            ["client_type"] = clientType,
            ["client_type_en"] = "Endpoint protection",
            ["device_type"] = clientType,
            ["device_type_en"] = "Endpoint protection",
            ["factoryid"] = factoryId,
            ["safedeviceid"] = safeDeviceId,
            ["is_support_to_strategy"] = "true",
            ["receiptdate"] = DateTimeOffset.Now.ToString("o"),  // ISO8601 格式
            ["uuid"] = Guid.NewGuid().ToString(),
        };
    }

    /// <summary>
    /// 生成描述文本（ceventdesc）
    /// </summary>
    public static (string zh, string en) GenerateDescription(
        LogCategory category,
        string action,
        string targetPath)
    {
        return category switch
        {
            LogCategory.FileProtect => (
                $"文件保护事件异常，{action}告警，路径：{targetPath}",
                $"File protection eventsexception，{action} Alerted，path：{targetPath}"
            ),
            
            LogCategory.MandatoryAccess => (
                $"强制访问控制事件异常，{action}告警，路径：{targetPath}",
                $"Forced access control eventsexception，{action} Alerted，path：{targetPath}"
            ),
            
            LogCategory.RegProtect => (
                $"注册表保护事件异常，{action}告警，路径：{targetPath}",
                $"Registry protection eventsexception，{action} Alerted，path：{targetPath}"
            ),
            
            LogCategory.NonWhitelist => (
                $"程序:{Path.GetFileName(targetPath)},控制模式执行：阻止，白名单校验：未通过",
                $"Program:{Path.GetFileName(targetPath)},Control mode execution: Blocked, Whitelist verification: Failed"
            ),
            
            LogCategory.WhitelistTamper => (
                $"程序:{Path.GetFileName(targetPath)},删除被阻止",
                $"Program:{Path.GetFileName(targetPath)},Deletion blocked"
            ),
            
            _ => ($"日志事件：{targetPath}", $"Log event: {targetPath}")
        };
    }

    /// <summary>
    /// 生成程序相关字段（用于进程告警日志）
    /// </summary>
    public static Dictionary<string, object?> GetProgramFields(
        string fullPath,
        string companyName = "-",
        string productName = "-",
        string version = "-")
    {
        return new Dictionary<string, object?>
        {
            ["curl"] = fullPath,
            ["cprogram"] = System.IO.Path.GetFileName(fullPath),
            ["companyname"] = companyName,
            ["cproduct"] = productName,
            ["cproductver"] = version,
        };
    }
}
