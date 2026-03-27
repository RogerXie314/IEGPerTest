// Feature: raw-packet-sender, Property 4: .etc 文件格式往返
using System.Collections.Generic;
using System.IO;
using System.Linq;
using FsCheck;
using FsCheck.Xunit;
using SimulatorLib.RawPacket;
using Xunit;

namespace SimulatorLib.Tests
{
    public class EtcFileParserTests
    {
        [Fact]
        public void WriteAndParse_EmptyStreams_RoundTrips()
        {
            var rate = new RateConfig { SpeedType = SpeedType.Pps, SpeedValue = 1000 };
            var streams = new List<StreamConfig>();

            using var ms = new MemoryStream();
            EtcFileParser.Write(ms, rate, streams);
            ms.Position = 0;
            var (rate2, streams2) = EtcFileParser.Parse(ms);

            Assert.Equal(rate.SpeedType, rate2.SpeedType);
            Assert.Equal(rate.SpeedValue, rate2.SpeedValue);
            Assert.Empty(streams2);
        }

        [Fact]
        public void WriteAndParse_SingleStream_RoundTrips()
        {
            var rate = new RateConfig { SpeedType = SpeedType.Interval, SpeedValue = 500 };
            var streams = new List<StreamConfig>
            {
                new() { Name = "test", Enabled = true, FrameData = new byte[] { 0x01, 0x02, 0x03 }, ChecksumFlags = 0x1F }
            };

            using var ms = new MemoryStream();
            EtcFileParser.Write(ms, rate, streams);
            ms.Position = 0;
            var (rate2, streams2) = EtcFileParser.Parse(ms);

            Assert.Equal(rate.SpeedType, rate2.SpeedType);
            Assert.Single(streams2);
            Assert.Equal("test", streams2[0].Name);
            Assert.Equal(new byte[] { 0x01, 0x02, 0x03 }, streams2[0].FrameData);
        }

        // Feature: raw-packet-sender, Property 4: .etc 文件格式往返
        // 使用简化的手动生成而非 FsCheck Arbitrary，避免复杂类型生成问题
        [Fact]
        public void WriteAndParse_MultipleStreams_RoundTrips()
        {
            var rate = new RateConfig { SpeedType = SpeedType.Fastest };
            var streams = Enumerable.Range(0, 5).Select(i => new StreamConfig
            {
                Name = $"stream_{i}",
                Enabled = i % 2 == 0,
                FrameData = new byte[] { (byte)i, (byte)(i + 1), (byte)(i + 2) },
                ChecksumFlags = (uint)i,
            }).ToList();

            using var ms = new MemoryStream();
            EtcFileParser.Write(ms, rate, streams);
            ms.Position = 0;
            var (rate2, streams2) = EtcFileParser.Parse(ms);

            Assert.Equal(SpeedType.Fastest, rate2.SpeedType);
            Assert.Equal(5, streams2.Count);
            for (int i = 0; i < 5; i++)
            {
                Assert.Equal(streams[i].Name, streams2[i].Name);
                Assert.Equal(streams[i].FrameData, streams2[i].FrameData);
            }
        }
    }
}
