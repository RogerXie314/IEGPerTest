using System;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using SimulatorLib.Persistence;
using Xunit;

namespace SimulatorLib.Tests
{
    public class ClientsPersistenceTests
    {
        [Fact]
        public async Task AppendAndRead_AllowsWritingAndReading()
        {
            // Ensure clean
            ClientsPersistence.Delete();

            var rec = new ClientRecord("UT-1", "127.0.0.1", DateTime.UtcNow, "OK")
            {
                DeviceId = 123,
                TcpPort = 4567,
            };
            await ClientsPersistence.AppendAsync(rec);

            var list = await ClientsPersistence.ReadAllAsync();
            Assert.NotEmpty(list);
            var found = list.FirstOrDefault(r => r.ClientId == "UT-1");
            Assert.NotNull(found);
            Assert.Equal("127.0.0.1", found!.IP);
            Assert.Equal((uint)123, found.DeviceId);
            Assert.Equal(4567, found.TcpPort);
        }
    }
}
