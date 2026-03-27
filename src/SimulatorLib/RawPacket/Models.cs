using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace SimulatorLib.RawPacket
{
    public enum PacketType
    {
        IcmpEcho, TcpSyn, Udp, Arp, IcmpV6Echo,
        BuiltinMs08067, BuiltinMs17010, BuiltinMs20796,
        Custom
    }

    public enum SpeedType { Pps, Interval, Fastest }
    public enum SendMode  { Continuous, Burst }
    public enum SendTaskStatus { Idle, Running, Stopped, Completed, Error }

    public class FieldRuleConfig
    {
        public bool   Valid     { get; set; }
        public uint   Flags     { get; set; }
        public ushort Offset    { get; set; }
        public byte   Width     { get; set; }
        public sbyte  BitsFrom  { get; set; } = -1;
        public sbyte  BitsLen   { get; set; }
        public byte[] BaseValue { get; set; } = new byte[8];
        public byte[] MaxValue  { get; set; } = new byte[8];
        public uint   StepSize  { get; set; }
    }

    public class StreamConfig
    {
        public int    Id            { get; set; }
        public string Name          { get; set; } = "";
        public PacketType Type      { get; set; }
        public bool   Enabled       { get; set; } = true;
        public byte[] FrameData     { get; set; } = System.Array.Empty<byte>();
        public List<FieldRuleConfig> Rules { get; set; } = new();
        public uint   ChecksumFlags { get; set; }

        [JsonIgnore] public string SrcIp  { get; set; } = "";
        [JsonIgnore] public string DstIp  { get; set; } = "";
        [JsonIgnore] public string DstMac { get; set; } = "";
        /// <summary>帧长度（显示用）</summary>
        [JsonIgnore] public int FrameLen => FrameData?.Length ?? 0;
        /// <summary>协议信息摘要（显示用）</summary>
        [JsonIgnore] public string Info   { get; set; } = "";
    }

    public class RateConfig
    {
        public SpeedType SpeedType  { get; set; } = SpeedType.Pps;
        public long      SpeedValue { get; set; } = 1000;
        public SendMode  SendMode   { get; set; } = SendMode.Continuous;
        public long      BurstCount { get; set; }
    }

    public class RawPacketSenderConfig
    {
        public string     LastAdapterName { get; set; } = "";
        public RateConfig Rate            { get; set; } = new();
        public List<StreamConfig> Streams { get; set; } = new();
    }

    public class NicAdapterInfo
    {
        public int    Index        { get; set; }
        public string PcapName     { get; set; } = "";
        public string FriendlyName { get; set; } = "";
        public string Ipv4         { get; set; } = "";
        public string DisplayText  => $"{Ipv4,-15} {FriendlyName}";
    }

    public class SendStats
    {
        public ulong  SendTotal  { get; set; }
        public ulong  SendBytes  { get; set; }
        public ulong  SendFail   { get; set; }
        public double CurrentPps { get; set; }
        public double CurrentBps { get; set; }
        public double AvgPps     { get; set; }
        public double AvgBps     { get; set; }
    }
}
