using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;

namespace SimulatorLib.Protocol
{
    public static class HeartbeatJsonBuilder
    {
        // Matches external/IEG_Code/code/include/CmdWord/WLCmdWordDef.h
        public const int CmdTypeCmd = 200; // CMDTYPE_CMD
        public const int DataToServerHeartbeat = 200; // DATA_TO_SERVER_HEARTBEAT

        private static readonly object BootLock = new();
        private static long? _bootTimeSeconds;

        private static readonly object CpuLock = new();
        private static ulong? _lastIdle;
        private static ulong? _lastKernel;
        private static ulong? _lastUser;

        public static string BuildV3R7C02(string computerId, string domainName, string computerIp, string computerMac)
        {
            int cpu = GetCpuUsagePercent();
            int mem = GetMemoryUsagePercent();

            var partitions = GetPartitions();
            long bootTimeSeconds = GetBootTimeSeconds();

            var rootArray = new JsonArray();
            var person = new JsonObject
            {
                ["ComputerID"] = computerId,
                ["CMDTYPE"] = CmdTypeCmd,
                ["CMDID"] = DataToServerHeartbeat,
                ["Domain"] = domainName,
            };

            var cmdContent = new JsonObject
            {
                ["dwCPU"] = cpu,
                ["dwMem"] = mem,
                ["WindowsVersion"] = GetWindowsVersionName(),
                // 原项目在调用 HeartBeat_GetJson_FromV3R7C02 时传了空字符串
                ["ComputerName"] = "",
                ["ComputerIP"] = computerIp,
                ["ComputerMac"] = computerMac,
                ["bootTime"] = bootTimeSeconds,
                ["bootTimeStr"] = ToAsctimeLocalString(bootTimeSeconds),
                ["Partiton"] = partitions,
            };

            person["CMDContent"] = cmdContent;
            person["clientLanguage"] = GetClientLanguage();

            rootArray.Add(person);

            // Json::FastWriter 默认带尾部换行；这里也加一个，尽量贴近。
            return rootArray.ToJsonString() + "\n";
        }

        public static string GetDeterministicMacFromIpv4(string ipv4)
        {
            if (IPAddress.TryParse(ipv4, out var ip) && ip.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
            {
                byte[] b = ip.GetAddressBytes();
                // 02: locally administered unicast
                byte[] mac = new byte[] { 0x02, 0x00, b[0], b[1], b[2], b[3] };
                return string.Join("-", mac.Select(x => x.ToString("X2", CultureInfo.InvariantCulture)));
            }
            return "02-00-00-00-00-00";
        }

        private static JsonArray GetPartitions()
        {
            var arr = new JsonArray();

            foreach (var drive in System.IO.DriveInfo.GetDrives())
            {
                if (!drive.IsReady) continue;
                if (drive.DriveType != System.IO.DriveType.Fixed) continue;

                long total = drive.TotalSize;
                long free = drive.TotalFreeSpace;
                if (total <= 0) continue;

                int usageRate = (int)((total - free) * 100 / total);

                string driveName = drive.Name.TrimEnd('\\'); // "C:\\" -> "C:"
                string totalSize = ToHumanSize(total);
                string usageRateStr = usageRate.ToString(CultureInfo.InvariantCulture) + "%";

                var part = new JsonObject
                {
                    ["Drive"] = driveName,
                    ["TotalSize"] = totalSize,
                    ["UsageRate"] = usageRateStr,
                };
                arr.Add(part);
            }

            return arr;
        }

        private static string ToHumanSize(long bytes)
        {
            const long KB = 1024;
            const long MB = 1024 * 1024;
            const long GB = 1024 * 1024 * 1024;

            if (bytes > GB)
            {
                return (bytes / GB).ToString(CultureInfo.InvariantCulture) + "G";
            }
            if (bytes > MB)
            {
                return (bytes / MB).ToString(CultureInfo.InvariantCulture) + "M";
            }
            if (bytes > KB)
            {
                return (bytes / KB).ToString(CultureInfo.InvariantCulture) + "K";
            }
            return bytes.ToString(CultureInfo.InvariantCulture) + "B";
        }

        private static string GetClientLanguage()
        {
            try
            {
                var ui = CultureInfo.CurrentUICulture;
                return ui.TwoLetterISOLanguageName.Equals("zh", StringComparison.OrdinalIgnoreCase) ? "zh" : "en";
            }
            catch
            {
                return "zh";
            }
        }

        private static string GetWindowsVersionName()
        {
            return OsInfo.GetWindowsVersionName();
        }

        private static long GetBootTimeSeconds()
        {
            lock (BootLock)
            {
                if (_bootTimeSeconds.HasValue) return _bootTimeSeconds.Value;

                long now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                long upSeconds = Environment.TickCount64 / 1000;
                _bootTimeSeconds = now - upSeconds;
                return _bootTimeSeconds.Value;
            }
        }

        private static string ToAsctimeLocalString(long unixSeconds)
        {
            // C asctime(localtime()) format: "Www Mmm dd hh:mm:ss yyyy\n"
            var dt = DateTimeOffset.FromUnixTimeSeconds(unixSeconds).ToLocalTime().DateTime;
            string day = dt.Day.ToString(CultureInfo.InvariantCulture).PadLeft(2, ' ');
            string s = string.Format(CultureInfo.InvariantCulture,
                "{0} {1} {2} {3} {4}\n",
                dt.ToString("ddd", CultureInfo.InvariantCulture),
                dt.ToString("MMM", CultureInfo.InvariantCulture),
                day,
                dt.ToString("HH:mm:ss", CultureInfo.InvariantCulture),
                dt.ToString("yyyy", CultureInfo.InvariantCulture));
            return s;
        }

        private static int GetMemoryUsagePercent()
        {
            if (!TryGetMemoryStatus(out var mem)) return 0;

            if (mem.ullTotalPhys == 0) return 0;
            ulong used = mem.ullTotalPhys - mem.ullAvailPhys;
            return (int)(used * 100 / mem.ullTotalPhys);
        }

        private static int GetCpuUsagePercent()
        {
            if (!GetSystemTimes(out var idle, out var kernel, out var user)) return 0;

            ulong idleTicks = ToUInt64(idle);
            ulong kernelTicks = ToUInt64(kernel);
            ulong userTicks = ToUInt64(user);

            lock (CpuLock)
            {
                if (!_lastIdle.HasValue)
                {
                    _lastIdle = idleTicks;
                    _lastKernel = kernelTicks;
                    _lastUser = userTicks;
                    return 0;
                }

                ulong idleDelta = idleTicks - _lastIdle.Value;
                ulong kernelDelta = kernelTicks - _lastKernel!.Value;
                ulong userDelta = userTicks - _lastUser!.Value;

                _lastIdle = idleTicks;
                _lastKernel = kernelTicks;
                _lastUser = userTicks;

                ulong total = kernelDelta + userDelta;
                if (total == 0) return 0;

                ulong busy = total - idleDelta;
                int percent = (int)(busy * 100 / total);
                if (percent < 0) return 0;
                if (percent > 100) return 100;
                return percent;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct FILETIME
        {
            public uint dwLowDateTime;
            public uint dwHighDateTime;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetSystemTimes(out FILETIME lpIdleTime, out FILETIME lpKernelTime, out FILETIME lpUserTime);

        private static ulong ToUInt64(FILETIME ft) => ((ulong)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        private struct MEMORYSTATUSEX
        {
            public uint dwLength;
            public uint dwMemoryLoad;
            public ulong ullTotalPhys;
            public ulong ullAvailPhys;
            public ulong ullTotalPageFile;
            public ulong ullAvailPageFile;
            public ulong ullTotalVirtual;
            public ulong ullAvailVirtual;
            public ulong ullAvailExtendedVirtual;
        }

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

        private static bool TryGetMemoryStatus(out MEMORYSTATUSEX mem)
        {
            mem = new MEMORYSTATUSEX { dwLength = (uint)Marshal.SizeOf<MEMORYSTATUSEX>() };
            return GlobalMemoryStatusEx(ref mem);
        }
    }
}
