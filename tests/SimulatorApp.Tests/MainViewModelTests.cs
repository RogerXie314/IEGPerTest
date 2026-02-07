using System.Threading.Tasks;
using System;
using System.IO;
using Xunit;

namespace SimulatorApp.Tests
{
    public class MainViewModelTests
    {
        private static void CleanupConfig()
        {
            try
            {
                var path = Path.Combine(AppContext.BaseDirectory, "config.json");
                if (File.Exists(path)) File.Delete(path);
            }
            catch { }
        }

        [Fact]
        public async Task Constructor_LoadsDefaults_And_CommandsExist()
        {
            CleanupConfig();
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            // Allow background LoadConfigAsync to complete
            await Task.Delay(200);

            Assert.False(string.IsNullOrWhiteSpace(vm.PlatformHost));
            Assert.NotNull(vm.RegisterCommand);
            Assert.NotNull(vm.StartHeartbeatCommand);
            Assert.NotNull(vm.StopHeartbeatCommand);
            Assert.NotNull(vm.PortTestCommand);
            Assert.NotNull(vm.StartLogSendCommand);
            Assert.NotNull(vm.StopLogSendCommand);
            Assert.NotNull(vm.BrowseWhitelistFileCommand);
            Assert.NotNull(vm.StartWhitelistUploadCommand);
            Assert.NotNull(vm.StopWhitelistUploadCommand);
        }

        [Fact]
        public async Task ValidateInputs_ReturnsFalse_OnInvalidPort()
        {
            CleanupConfig();
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            await Task.Delay(50);
            vm.PlatformPort = 70000; // invalid

            var mi = typeof(SimulatorApp.ViewModels.MainViewModel).GetMethod("ValidateInputs", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            Assert.NotNull(mi);
            var res = mi.Invoke(vm, null);
            Assert.IsType<ValueTuple<bool, string>>(res);
            var tuple = (ValueTuple<bool, string>)res!;
            Assert.False(tuple.Item1);
            Assert.Contains("PlatformPort", tuple.Item2);
        }

        [Fact]
        public async Task ValidateInputs_ReturnsTrue_OnDefaults()
        {
            CleanupConfig();
            var vm = new SimulatorApp.ViewModels.MainViewModel();
            await Task.Delay(50);
            var mi = typeof(SimulatorApp.ViewModels.MainViewModel).GetMethod("ValidateInputs", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            Assert.NotNull(mi);
            var res = mi.Invoke(vm, null);
            Assert.IsType<ValueTuple<bool, string>>(res);
            var tuple = (ValueTuple<bool, string>)res!;
            Assert.True(tuple.Item1);
        }
    }
}
