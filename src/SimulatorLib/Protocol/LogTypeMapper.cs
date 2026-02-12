namespace SimulatorLib.Protocol;

/// <summary>
/// 提供日志 Type/SubType 的映射表，便于将友好类别映射为原项目期望的数值。
/// 基于 external/IEG_Code/code/include/wlCommonSecMod.h:
/// TYPE_OPTYPE_PWL enum:
///   OPTYPE_PWL_CONTROL = 1,      // 非白名单控制报警
///   OPTYPE_PWL_AUDIT = 2,        // 审计（进程审计）
///   OPTYPE_PWL_MODIFY_FILE = 3,  // 白名单防篡改
/// SUBTYPE_OPTYPE_FILE enum:  
///   SUBTYPE_WRITEFILE = 3,       // 写文件
///   SUBTYPE_EXECUTEFILE = 6,     // 执行文件
/// </summary>
public static class LogTypeMapper
{
    /// <summary>
    /// 获取进程类告警（白名单相关）的 Type/SubType。
    /// </summary>
    public static (int Type, int SubType) GetProcessTypeForWhitelist(bool isModify)
    {
        // 非白名单告警: Type=1 (OPTYPE_PWL_CONTROL), SubType=6 (SUBTYPE_EXECUTEFILE)
        // 白名单防篡改: Type=3 (OPTYPE_PWL_MODIFY_FILE), SubType=3 (SUBTYPE_WRITEFILE)
        return isModify ? (3, 3) : (1, 6);
    }

    /// <summary>
    /// 获取进程审计的 Type/SubType。
    /// </summary>
    public static (int Type, int SubType) GetProcessAuditType()
    {
        // 进程审计: Type=2 (OPTYPE_PWL_AUDIT), SubType=6 (SUBTYPE_EXECUTEFILE)
        return (2, 6);
    }

    /// <summary>
    /// 默认进程告警 Type/SubType（当未指定时使用）。
    /// </summary>
    public static (int Type, int SubType) GetDefaultProcessType()
    {
        return (0, 0);
    }

    // 以后可扩展更多映射，例如基于 audit 的 cType/cSubType 映射到 DB 的 Type/SubType
}
