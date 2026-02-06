using System.Threading.Tasks;
using SimulatorLib.Persistence;
using Xunit;

namespace SimulatorLib.Tests
{
    public class AppConfigTests
    {
        [Fact]
        public async Task SaveAndLoad_PersistsValues()
        {
            var cfg = new AppConfig
            {
                PlatformHost = "unit-test-host",
                PlatformPort = 12345,
                LogHost = "log-host",
                LogPort = 54321,
                RegClientPrefix = "T-",
                RegStartIndex = 10,
                RegCount = 2,
                HeartbeatIntervalMs = 500
            };

            await AppConfig.SaveAsync(cfg);
            var loaded = await AppConfig.LoadAsync();

            Assert.Equal(cfg.PlatformHost, loaded.PlatformHost);
            Assert.Equal(cfg.PlatformPort, loaded.PlatformPort);
            Assert.Equal(cfg.LogHost, loaded.LogHost);
            Assert.Equal(cfg.LogPort, loaded.LogPort);
            Assert.Equal(cfg.RegClientPrefix, loaded.RegClientPrefix);
        }
    }
}
