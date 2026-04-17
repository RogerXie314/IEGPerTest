using System;
using System.Runtime.InteropServices;

namespace SimulatorLib.Protocol
{
    public static class OsInfo
    {
        public static string GetWindowsVersionName()
        {
            // External project uses a friendly name like "Windows 10".
            // RuntimeInformation.OSDescription tends to be like "Microsoft Windows 10.0.19045".
            var desc = string.Empty;
            try { desc = RuntimeInformation.OSDescription ?? string.Empty; } catch { }

            // 先尝试从描述字符串中识别友好名称
            if (desc.Contains("Windows Server 2022", StringComparison.OrdinalIgnoreCase)) return "Windows Server 2022";
            if (desc.Contains("Windows Server 2019", StringComparison.OrdinalIgnoreCase)) return "Windows Server 2019";
            if (desc.Contains("Windows Server 2016", StringComparison.OrdinalIgnoreCase)) return "Windows Server 2016";
            if (desc.Contains("Windows Server 2012 R2", StringComparison.OrdinalIgnoreCase)) return "Windows Server 2012 R2";
            if (desc.Contains("Windows Server 2012", StringComparison.OrdinalIgnoreCase)) return "Windows Server 2012";
            if (desc.Contains("Windows 11", StringComparison.OrdinalIgnoreCase)) return "Windows 11";
            if (desc.Contains("Windows 10", StringComparison.OrdinalIgnoreCase)) return "Windows 10";
            if (desc.Contains("Windows 8.1", StringComparison.OrdinalIgnoreCase)) return "Windows 8.1";
            if (desc.Contains("Windows 8", StringComparison.OrdinalIgnoreCase)) return "Windows 8";
            if (desc.Contains("Windows 7", StringComparison.OrdinalIgnoreCase)) return "Windows 7";

            // 通过版本号识别（适用于 RuntimeInformation.OSDescription 不包含友好名称的情况）
            // 例如 Windows Server 2012 R2 可能显示为 "Microsoft Windows 6.3.9600"
            try
            {
                var version = Environment.OSVersion.Version;
                
                // Windows Server 版本号映射
                if (version.Major == 10 && version.Build >= 20348) return "Windows Server 2022";
                if (version.Major == 10 && version.Build >= 17763) return "Windows Server 2019";
                if (version.Major == 10 && version.Build >= 14393) return "Windows Server 2016";
                if (version.Major == 6 && version.Minor == 3) return "Windows Server 2012 R2"; // 6.3
                if (version.Major == 6 && version.Minor == 2) return "Windows Server 2012";    // 6.2
                if (version.Major == 6 && version.Minor == 1) return "Windows 7 / Server 2008 R2"; // 6.1
                
                // Windows 桌面版本号映射（如果不是 Server）
                if (version.Major == 10 && version.Build >= 22000) return "Windows 11";
                if (version.Major == 10) return "Windows 10";
                if (version.Major == 6 && version.Minor == 3) return "Windows 8.1";
                if (version.Major == 6 && version.Minor == 2) return "Windows 8";
            }
            catch { }

            // Fallback: keep it readable.
            return string.IsNullOrWhiteSpace(desc) ? "Windows" : desc;
        }
    }
}
