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

            if (desc.Contains("Windows 11", StringComparison.OrdinalIgnoreCase)) return "Windows 11";
            if (desc.Contains("Windows 10", StringComparison.OrdinalIgnoreCase)) return "Windows 10";
            if (desc.Contains("Windows 8.1", StringComparison.OrdinalIgnoreCase)) return "Windows 8.1";
            if (desc.Contains("Windows 8", StringComparison.OrdinalIgnoreCase)) return "Windows 8";
            if (desc.Contains("Windows 7", StringComparison.OrdinalIgnoreCase)) return "Windows 7";

            // Fallback: keep it readable.
            return string.IsNullOrWhiteSpace(desc) ? "Windows" : desc;
        }
    }
}
