// Feature: raw-packet-sender, Property 3: 输入验证拒绝无效输入
using FsCheck;
using FsCheck.Xunit;
using SimulatorLib.RawPacket;
using Xunit;

namespace SimulatorLib.Tests
{
    public class InputValidatorTests
    {
        [Theory]
        [InlineData("192.168.1.1", true)]
        [InlineData("0.0.0.0", true)]
        [InlineData("255.255.255.255", true)]
        [InlineData("256.0.0.1", false)]
        [InlineData("abc", false)]
        [InlineData("", false)]
        [InlineData(null, false)]
        public void IsValidIpv4_KnownCases(string? input, bool expected)
        {
            Assert.Equal(expected, InputValidator.IsValidIpv4(input));
        }

        [Theory]
        [InlineData("AA:BB:CC:DD:EE:FF", true)]
        [InlineData("aa-bb-cc-dd-ee-ff", true)]
        [InlineData("GG:BB:CC:DD:EE:FF", false)]
        [InlineData("AA:BB:CC:DD:EE", false)]
        [InlineData("", false)]
        public void IsValidMac_KnownCases(string input, bool expected)
        {
            Assert.Equal(expected, InputValidator.IsValidMac(input));
        }

        [Theory]
        [InlineData(1, true)]
        [InlineData(1_000_000, true)]
        [InlineData(0, false)]
        [InlineData(1_000_001, false)]
        [InlineData(-1, false)]
        public void IsValidPps_KnownCases(long pps, bool expected)
        {
            Assert.Equal(expected, InputValidator.IsValidPps(pps));
        }

        // Feature: raw-packet-sender, Property 3: 输入验证拒绝无效输入
        [Property(MaxTest = 500)]
        public Property IsValidPps_OutOfRange_ReturnsFalse(long pps)
        {
            // 仅测试超出范围的值
            var outOfRange = pps < 1 || pps > 1_000_000;
            if (!outOfRange) return true.ToProperty(); // 跳过合法值
            return (!InputValidator.IsValidPps(pps)).ToProperty();
        }
    }
}
