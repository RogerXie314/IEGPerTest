using System;
using System.Net;
using System.Text.RegularExpressions;

namespace SimulatorLib.RawPacket
{
    public static class InputValidator
    {
        private static readonly Regex MacRegex = new(
            @"^([0-9A-Fa-f]{2}[:\-]){5}[0-9A-Fa-f]{2}$",
            RegexOptions.Compiled);

        public static bool IsValidIpv4(string? s)
        {
            if (string.IsNullOrWhiteSpace(s)) return false;
            return IPAddress.TryParse(s, out var addr)
                && addr.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork;
        }

        public static bool IsValidMac(string? s)
        {
            if (string.IsNullOrWhiteSpace(s)) return false;
            return MacRegex.IsMatch(s);
        }

        public static bool IsValidPps(long pps) => pps >= 1 && pps <= 1_000_000;

        public static bool IsValidIpRange(string? startIp, string? endIp)
        {
            if (!IsValidIpv4(startIp) || !IsValidIpv4(endIp)) return false;
            var start = IpToUint(startIp!);
            var end   = IpToUint(endIp!);
            return start <= end;
        }

        public static uint IpToUint(string ip)
        {
            var bytes = IPAddress.Parse(ip).GetAddressBytes();
            return ((uint)bytes[0] << 24) | ((uint)bytes[1] << 16)
                 | ((uint)bytes[2] << 8)  |  bytes[3];
        }

        public static string UintToIp(uint ip)
        {
            return $"{ip >> 24}.{(ip >> 16) & 0xff}.{(ip >> 8) & 0xff}.{ip & 0xff}";
        }
    }
}
