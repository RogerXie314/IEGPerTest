namespace SimulatorLib.Protocol;

/// <summary>
/// 提供告警 Type/SubType 的映射表（最小实现，优先对齐白名单/文件保护/强制访问控制）
/// 值基于原项目观察与模拟器历史硬编码，便于集中维护。
/// </summary>
public static class WarningTypeMapper
{
    public static (int Type, int SubType) GetTypeSubTypeForWhitelistAlert()
    {
        // 2/1 为模拟器既有默认（与之前实现一致）
        return (2, 1);
    }

    public static (int Type, int SubType) GetTypeSubTypeForWhitelistModify()
    {
        // 2/2 表示白名单防篡改/修改类
        return (2, 2);
    }

    public static (int Type, int SubType) GetTypeSubTypeForProcessDefault()
    {
        // 默认进程告警类型（保守值，平台 UI 常见）
        return (0, 0);
    }

    public static (int Type, int SubType) GetTypeSubTypeForHostDefenceDetail(int detailLevel2)
    {
        // HostDefence 的 detailLevel2 映射到 SubType（保持 detailLevel2 语义）
        // Type 取 3 作为 HostDefence/系统完整性相关（可根据后续发现调整）
        return (3, detailLevel2);
    }
}
