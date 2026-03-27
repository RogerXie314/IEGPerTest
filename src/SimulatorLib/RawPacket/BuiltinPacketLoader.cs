using System.Collections.Generic;
using System.IO;
using System.Reflection;

namespace SimulatorLib.RawPacket
{
    public record BuiltinPacketDef(string Name, string TargetOs, string ResourceName);

    public static class BuiltinPacketLoader
    {
        public static readonly BuiltinPacketDef[] Definitions =
        {
            new("MS08-067", "Windows XP",  "ms08067-winXP.etc"),
            new("MS17-010", "Windows 7",   "ms17010-win7.etc"),
            new("MS20-796", "Windows 10",  "ms20796-win10.etc"),
        };

        public static List<StreamConfig> Load(string resourceName)
        {
            var asm = Assembly.GetExecutingAssembly();
            using var stream = asm.GetManifestResourceStream(resourceName)
                ?? throw new FileNotFoundException(
                    $"内置报文资源未找到: {resourceName}");

            var (_, streams) = EtcFileParser.Parse(stream);
            return streams;
        }
    }
}
