using System;
using System.Text;
using SimulatorLib.Protocol;
using Xunit;

namespace SimulatorLib.Tests
{
    public class PtProtocolTests
    {
        [Fact]
        public void PackAndUnpack_RoundTrips()
        {
            var src = Encoding.UTF8.GetBytes("[{\"k\":\"v\",\"n\":123}]");
            var packet = PtProtocol.Pack(src, cmdId: 1, compressType: PtCompressType.Zlib, encryptType: PtEncryptType.Aes, deviceId: 0);

            Assert.True(packet.Length > PtProtocol.HeaderLength);
            Assert.Equal((byte)'P', packet[0]);
            Assert.Equal((byte)'T', packet[1]);

            var unpacked = PtProtocol.Unpack(packet);
            Assert.Equal(src, unpacked);
        }

        [Fact]
        public void Pack_HeaderLengthIs48()
        {
            Assert.Equal(48, PtProtocol.HeaderLength);
        }

        [Fact]
        public void HeartbeatJsonBuilder_BuildsArrayWithNewline()
        {
            var json = HeartbeatJsonBuilder.BuildV3R7C02("CID", "DOMAIN", "192.168.0.2", "02-00-C0-A8-00-02");
            Assert.StartsWith("[", json);
            Assert.EndsWith("\n", json);
            Assert.Contains("\"ComputerID\"", json, StringComparison.Ordinal);
            Assert.Contains("\"CMDContent\"", json, StringComparison.Ordinal);
            Assert.Contains("\"Partiton\"", json, StringComparison.Ordinal);
        }
    }
}
