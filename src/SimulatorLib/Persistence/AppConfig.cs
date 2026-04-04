using System;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;
using SimulatorLib.RawPacket;

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
        public int LogHttpsClientCount { get; set; } = 0;
        /// <summary>HTTPS 短连接日志：每客户端每秒条数（平台规格 ≤100 EPS）</summary>
        public int LogHttpsEps { get; set; } = 0;
        /// <summary>威胁检测 TCP 长连接通道：客户端个数</summary>
        public int LogThreatClientCount { get; set; } = 50;
        /// <summary>威胁检测 TCP 长连接日志：每客户端每秒条数（平台规格 6000 EPS）</summary>
        public int LogThreatEps { get; set; } = 1;
        /// <summary>
        /// 威胁检测命中轮比：每 N 轮中仅第 1 轮发含命中特征的事件包，其余发无害（miss）包。
        /// 对齐老工具默认值 71（约 1.4% 命中率），避免平台告警风暴触发熔断 FIN 踢人。
        /// 0 或 1 = 每轮均命中（等效旧行为，会导致告警风暴）。
        /// </summary>
        public int LogThreatHitEvery { get; set; } = 71;

        public int RegConcurrency { get; set; } = 20;
        public int RegRetryIntervalSec { get; set; } = 30;
        public int RegTimeoutMs { get; set; } = 60000;

        public string WhitelistFilePath { get; set; } = string.Empty;
        public int WhitelistConcurrency { get; set; } = 4;
        /// <summary>对齐老工具：注册完成后自动对全部已注册客户端上传一次白名单</summary>
        public bool EnableWhitelistOnReg { get; set; } = false;

        // ── SSH 日志收集 ────────────────────────────────────────────────────
        /// <summary>模拟客户端的操作系统类型："Windows" 或 "Linux"</summary>
        public string ClientOsType { get; set; } = "Windows";
        /// <summary>SSH 登录用户名（Ubuntu 通常为 sysadmin，再 sudo 提权）</summary>
        public string SshUser { get; set; } = "sysadmin";
        /// <summary>SSH 密码（明文保存，仅供开发调试使用）</summary>
        public string SshPassword { get; set; } = "";
        /// <summary>SSH 端口（默认 22）</summary>
        public int SshPort { get; set; } = 22;
        /// <summary>平台日志目录（默认 /root/logs）</summary>
        public string SshLogPath { get; set; } = "/root/logs";
        /// <summary>直接下载的文件大小上限(MB)，超过则在服务端 awk 过滤后再下载</summary>
        public int SshSizeThresholdMb { get; set; } = 50;
        /// <summary>日志采集开始时间（yyyy-MM-dd HH:mm），空 = 使用默认90分钟前</summary>
        public string SshCollectFrom { get; set; } = "";
        /// <summary>日志采集结束时间（yyyy-MM-dd HH:mm），空 = 使用当前时间</summary>
        public string SshCollectTo { get; set; } = "";

        private static string ConfigPath => Path.Combine(AppContext.BaseDirectory, "config.json");

        public RawPacketSenderConfig RawPacketSender { get; set; } = new();

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
