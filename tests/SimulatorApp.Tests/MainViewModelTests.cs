using System.Threading.Tasks;
using Xunit;

namespace SimulatorApp.Tests
{
    public class MainViewModelTests
    {
        [Fact]
        public async Task Constructor_LoadsDefaults_And_CommandsExist()
        {
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            // Allow background LoadConfigAsync to complete
            await Task.Delay(200);

            Assert.Equal("localhost", vm.PlatformHost);
            Assert.NotNull(vm.RegisterCommand);
            Assert.NotNull(vm.StartHeartbeatCommand);
            Assert.NotNull(vm.StopHeartbeatCommand);
            Assert.NotNull(vm.PortTestCommand);
        }
    }
}
