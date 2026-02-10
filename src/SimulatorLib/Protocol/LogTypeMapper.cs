namespace SimulatorLib.Protocol;

/// <summary>
/// 提供日志 Type/SubType 的映射表，便于将友好类别映射为原项目期望的数值。
/// 优先覆盖白名单 / 文件保护 / 强制访问控制 等关键项。
/// </summary>
public static class LogTypeMapper
{
    /// <summary>
    /// 获取进程类告警（白名单相关）的 Type/SubType。
    /// </summary>
    public static (int Type, int SubType) GetProcessTypeForWhitelist(bool isModify)
    {
        // 对齐现有模拟器默认值：非白名单告警 -> Type=2, SubType=1；白名单修改/防篡改 -> Type=2, SubType=2
        return isModify ? (2, 2) : (2, 1);
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
