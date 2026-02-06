using System;
using System.Threading.Tasks;
using SimulatorLib.Persistence;

namespace SimulatorLib.Workers
{
    public class RegistrationWorker
    {
        public async Task RegisterAsync(string clientIdPrefix, int startIndex, int count)
        {
            for (int i = 0; i < count; i++)
            {
                var id = clientIdPrefix + (startIndex + i).ToString();
                var ip = $"192.168.0.{(startIndex + i) % 254 + 1}";
                var rec = new ClientRecord(id, ip, DateTime.UtcNow, "Registered");
                await ClientsPersistence.AppendAsync(rec).ConfigureAwait(false);
                await Task.Delay(20).ConfigureAwait(false); // 模拟网络延迟
            }
        }
    }
}
