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

        [Fact]
        public async Task ValidateInputs_ReturnsFalse_OnInvalidPort()
        {
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            await Task.Delay(50);
            vm.PlatformPort = 70000; // invalid

            var mi = typeof(SimulatorApp.ViewModels.MainViewModel).GetMethod("ValidateInputs", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            Assert.NotNull(mi);
            var res = mi.Invoke(vm, null);
            Assert.IsType<ValueTuple<bool, string>>(res);
            dynamic tuple = res;
            Assert.False((bool)tuple.ok);
            Assert.Contains("PlatformPort", (string)tuple.reason);
        }

        [Fact]
        public async Task ValidateInputs_ReturnsTrue_OnDefaults()
        {
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            await Task.Delay(50);
            var mi = typeof(SimulatorApp.ViewModels.MainViewModel).GetMethod("ValidateInputs", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            Assert.NotNull(mi);
            var res = mi.Invoke(vm, null);
            dynamic tuple = res;
            Assert.True((bool)tuple.ok);
        }
    }
}
