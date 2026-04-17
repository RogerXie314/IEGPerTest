using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace SimulatorLib.Config
{
    /// <summary>
    /// 客户端版本配置
    /// </summary>
    public class ClientVersionConfig
    {
        public List<string> WindowsVersions { get; set; } = new();
        public List<string> LinuxVersions { get; set; } = new();

        private static readonly string ConfigFilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "SimulatorApp",
            "client_versions.json"
        );

        /// <summary>
        /// 加载配置，如果文件不存在则返回默认配置
        /// </summary>
        public static ClientVersionConfig Load()
        {
            try
            {
                if (File.Exists(ConfigFilePath))
                {
                    var json = File.ReadAllText(ConfigFilePath);
                    var config = JsonSerializer.Deserialize<ClientVersionConfig>(json);
                    if (config != null && (config.WindowsVersions.Any() || config.LinuxVersions.Any()))
                    {
                        return config;
                    }
                }
            }
            catch { }

            // 返回默认配置
            return GetDefaultConfig();
        }

        /// <summary>
        /// 保存配置到文件
        /// </summary>
        public void Save()
        {
            try
            {
                var dir = Path.GetDirectoryName(ConfigFilePath);
                if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                {
                    Directory.CreateDirectory(dir);
                }

                var options = new JsonSerializerOptions { WriteIndented = true };
                var json = JsonSerializer.Serialize(this, options);
                File.WriteAllText(ConfigFilePath, json);
            }
            catch (Exception ex)
            {
                throw new Exception($"保存版本配置失败: {ex.Message}", ex);
            }
        }

        /// <summary>
        /// 获取默认配置
        /// </summary>
        private static ClientVersionConfig GetDefaultConfig()
        {
            return new ClientVersionConfig
            {
                WindowsVersions = new List<string>
                {
                    "V300R011C01B090",   // Windows 当前版本
                    "V300R011C01B030",   // Windows 旧版
                    "V300R006C05B270",   // 老版 V6（支持白名单上传）
                    "V300R006C02B090",   // 老版 V6（支持白名单上传）
                },
                LinuxVersions = new List<string>
                {
                    "V300R011C11B060-Redhat7.x-x64"
                }
            };
        }

        /// <summary>
        /// 重置为默认配置
        /// </summary>
        public static ClientVersionConfig ResetToDefault()
        {
            var config = GetDefaultConfig();
            config.Save();
            return config;
        }
    }
}
