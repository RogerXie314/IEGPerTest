using System;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;

namespace SimulatorLib.Persistence
{
    public class AppConfig
    {
        public string PlatformHost { get; set; } = "localhost";
        public int PlatformPort { get; set; } = 8441;
        public string LogHost { get; set; } = "localhost";
        public int LogPort { get; set; } = 4565;

        public string RegClientPrefix { get; set; } = "Client-";
        public int RegStartIndex { get; set; } = 1;
        public int RegCount { get; set; } = 5;

        public int HeartbeatIntervalMs { get; set; } = 1000;

        private static string ConfigPath => Path.Combine(AppContext.BaseDirectory, "config.json");

        public static async Task<AppConfig> LoadAsync()
        {
            try
            {
                if (!File.Exists(ConfigPath))
                {
                    var def = new AppConfig();
                    await SaveAsync(def).ConfigureAwait(false);
                    return def;
                }

                var json = await File.ReadAllTextAsync(ConfigPath).ConfigureAwait(false);
                var cfg = JsonSerializer.Deserialize<AppConfig>(json);
                return cfg ?? new AppConfig();
            }
            catch
            {
                return new AppConfig();
            }
        }

        public static async Task SaveAsync(AppConfig cfg)
        {
            var json = JsonSerializer.Serialize(cfg, new JsonSerializerOptions { WriteIndented = true });
            await File.WriteAllTextAsync(ConfigPath, json).ConfigureAwait(false);
        }
    }
}
