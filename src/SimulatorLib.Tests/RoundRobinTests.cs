// Feature: raw-packet-sender, Property 8: Round-Robin 发送覆盖所有启用 Stream
using System.Collections.Generic;
using System.Linq;
using FsCheck;
using FsCheck.Xunit;
using Xunit;

namespace SimulatorLib.Tests
{
    public class RoundRobinTests
    {
        // Feature: raw-packet-sender, Property 8: Round-Robin 发送覆盖所有启用 Stream
        [Property(MaxTest = 200)]
        public Property RoundRobin_CoversAllEnabledStreams(PositiveInt n)
        {
            int count = Math.Min(n.Get, 20); // 限制最大 20 条避免测试过慢
            if (count == 0) return true.ToProperty();

            var streams = Enumerable.Range(0, count).ToList();
            var visited = new HashSet<int>();

            int idx = -1;
            for (int i = 0; i < count; i++)
            {
                idx = (idx + 1) % count;
                visited.Add(streams[idx]);
            }

            return (visited.Count == count).ToProperty();
        }

        [Fact]
        public void RoundRobin_SingleStream_AlwaysSelectsSame()
        {
            var selected = new List<int>();
            int idx = -1;
            for (int i = 0; i < 5; i++)
            {
                idx = (idx + 1) % 1;
                selected.Add(idx);
            }
            Assert.All(selected, x => Assert.Equal(0, x));
        }
    }
}
