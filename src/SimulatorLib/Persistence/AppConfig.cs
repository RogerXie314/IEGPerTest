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

        public bool UseLogServer { get; set; } = false;

        public string RegClientPrefix { get; set; } = "Client-";
        public int RegStartIndex { get; set; } = 1;
        public string RegStartIp { get; set; } = "192.168.0.1";
        public int RegCount { get; set; } = 5;

        public int HeartbeatIntervalMs { get; set; } = 30000;

        public int LogMessagesPerClient { get; set; } = 50;
        /// <summary>HTTPS 短连接通道：客户端个数</summary>
        public int LogHttpsClientCount { get; set; } = 5;
        /// <summary>HTTPS 短连接日志：每客户端每秒条数（平台规格 ≤100 EPS）</summary>
        public int LogHttpsEps { get; set; } = 10;
        /// <summary>威胁检测 TCP 长连接通道：客户端个数</summary>
        public int LogThreatClientCount { get; set; } = 5;
        /// <summary>威胁检测 TCP 长连接日志：每客户端每秒条数（平台规格 6000 EPS）</summary>
        public int LogThreatEps { get; set; } = 100;

        public int RegConcurrency { get; set; } = 20;
        public int RegRetryIntervalSec { get; set; } = 30;
        public int RegTimeoutMs { get; set; } = 60000;

        public string WhitelistFilePath { get; set; } = string.Empty;
        public int WhitelistClientCount { get; set; } = 5;
        public int WhitelistConcurrency { get; set; } = 4;

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
