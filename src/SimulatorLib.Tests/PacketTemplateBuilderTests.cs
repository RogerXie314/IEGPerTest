// Feature: raw-packet-sender, Property 1: 构造帧校验和正确性
using FsCheck;
using FsCheck.Xunit;
using SimulatorLib.RawPacket;
using Xunit;

namespace SimulatorLib.Tests
{
    public class PacketTemplateBuilderTests
    {
        // ── 具体例子测试 ──────────────────────────────────────────────────

        [Fact]
        public void BuildIcmpEcho_HasCorrectEtherType()
        {
            var frame = PacketTemplateBuilder.BuildIcmpEcho(
                new byte[6], new byte[6], 0xC0A80101, 0xC0A80102);
            Assert.Equal(0x08, frame[12]);
            Assert.Equal(0x00, frame[13]);
        }

        [Fact]
        public void BuildTcpSyn_HasSynFlag()
        {
            var frame = PacketTemplateBuilder.BuildTcpSyn(
                new byte[6], new byte[6], 0xC0A80101, 0xC0A80102, 12345, 80);
            Assert.Equal(0x02, frame[47]); // SYN flag
        }

        [Fact]
        public void BuildArpRequest_HasCorrectOper()
        {
            var frame = PacketTemplateBuilder.BuildArpRequest(
                new byte[6], new byte[6], 0xC0A80101, 0xC0A80102);
            Assert.Equal(0x00, frame[20]);
            Assert.Equal(0x01, frame[21]); // oper = request
        }

        // ── 属性测试 ─────────────────────────────────────────────────────

        // Feature: raw-packet-sender, Property 1: 构造帧校验和正确性
        [Property(MaxTest = 200)]
        public Property BuildIcmpEcho_IpChecksumAlwaysValid(
            uint srcIp, uint dstIp)
        {
            var srcMac = new byte[6];
            var dstMac = new byte[6];
            var frame = PacketTemplateBuilder.BuildIcmpEcho(srcMac, dstMac, srcIp, dstIp);
            return ChecksumVerifier.IsIpChecksumValid(frame).ToProperty();
        }

        [Property(MaxTest = 200)]
        public Property BuildTcpSyn_IpChecksumAlwaysValid(
            uint srcIp, uint dstIp, ushort srcPort, ushort dstPort)
        {
            var frame = PacketTemplateBuilder.BuildTcpSyn(
                new byte[6], new byte[6], srcIp, dstIp, srcPort, dstPort);
            return ChecksumVerifier.IsIpChecksumValid(frame).ToProperty();
        }

        [Property(MaxTest = 200)]
        public Property SetDestinationIp_ChecksumRemainsValid(uint srcIp, uint dstIp, uint newDstIp)
        {
            var frame = PacketTemplateBuilder.BuildIcmpEcho(new byte[6], new byte[6], srcIp, dstIp);
            PacketTemplateBuilder.SetDestinationIp(frame, newDstIp);
            return ChecksumVerifier.IsIpChecksumValid(frame).ToProperty();
        }
    }

    /// <summary>独立校验和验证器（不依赖 PacketTemplateBuilder 内部实现）。</summary>
    public static class ChecksumVerifier
    {
        public static bool IsIpChecksumValid(byte[] frame)
        {
            if (frame.Length < 34) return false;
            // 验证 IP 校验和：对 IP 头（20字节）做 Internet checksum，结果应为 0
            uint sum = 0;
            for (int i = 14; i < 34; i += 2)
                sum += (uint)((frame[i] << 8) | frame[i + 1]);
            while (sum >> 16 != 0)
                sum = (sum & 0xFFFF) + (sum >> 16);
            return (ushort)sum == 0xFFFF;
        }
    }
}
