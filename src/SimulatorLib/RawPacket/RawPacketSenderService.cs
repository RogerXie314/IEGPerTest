using System;
using System.Collections.Generic;
using System.Net.NetworkInformation;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace SimulatorLib.RawPacket
{
    /// <summary>
    /// 原始报文发送服务：封装 RawPacketEngineInterop，管理适配器、Stream 和统计。
    /// </summary>
    public sealed class RawPacketSenderService : IDisposable
    {
        private readonly RawPacketEngineInterop _interop;
        private readonly List<NicAdapterInfo>   _adapters = new();
        private readonly List<StreamConfig>     _streams  = new();
        private SendTaskStatus _status = SendTaskStatus.Idle;
        private ulong    _lastSendTotal;
        private ulong    _lastSendBytes;
        private DateTime _lastStatsTime;
        private DateTime _startTime;
        private int      _selectedAdapterIndex = -1;

        public IReadOnlyList<NicAdapterInfo> Adapters => _adapters;
        public IReadOnlyList<StreamConfig>   Streams  => _streams;
        public SendTaskStatus                Status   => _status;
        public SendStats                     Stats    { get; private set; } = new();
        public string?                       LastError { get; private set; }

        public RawPacketSenderService(RawPacketEngineInterop interop)
        {
            _interop = interop;
        }

        /// <summary>初始化引擎并枚举网络适配器。</summary>
        public bool Initialize()
        {
            int initResult = _interop.Init();
            if (initResult != 0)
            {
                LastError = initResult switch
                {
                    -2 => "Npcap 检测超时（未安装或无响应）\n\n请访问 https://npcap.com/ 下载并安装 Npcap",
                    -1 => "RawPacketEngine 初始化失败\n\n请确认 Npcap 已正确安装",
                    _  => $"RawPacketEngine 初始化失败（错误码：{initResult}）"
                };
                return false;
            }

            _adapters.Clear();
            int count = _interop.GetAdapterCount();
            for (int i = 0; i < count; i++)
            {
                var (pcapName, ipv4) = _interop.GetAdapterInfo(i);
                // 从 pcap 名称中提取 GUID，再查友好名称；无法解析（Npcap 虚拟适配器）则跳过
                string? friendly = GetFriendlyName(pcapName, ipv4);
                if (friendly == null) continue;
                _adapters.Add(new NicAdapterInfo
                {
                    Index        = i,
                    PcapName     = pcapName,
                    FriendlyName = friendly,
                    Ipv4         = ipv4,
                });
            }

            _lastStatsTime = DateTime.UtcNow;
            _startTime     = DateTime.UtcNow;
            return true;
        }

        /// <summary>
        /// 从 pcap 设备名（含 GUID）解析 Windows 友好名称。
        /// 解析顺序：① NetworkInterface GUID 匹配 → ② NetworkInterface IP 匹配 → ③ 注册表 GUID 查询。
        /// 全部失败且无 IP 时返回 null（Npcap 专属虚拟适配器，调用方跳过不显示）。
        /// </summary>
        private static string? GetFriendlyName(string pcapName, string ipv4)
        {
            try
            {
                // pcapName 格式：rpcap://\Device\NPF_{GUID} 或 \Device\NPF_{GUID}
                var m = Regex.Match(pcapName, @"\{([0-9A-Fa-f\-]{36})\}");
                string guid = m.Success ? m.Groups[1].Value.ToUpperInvariant() : "";

                foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
                {
                    // ① GUID 匹配（nic.Id 在 .NET/Windows 上可能带花括号，统一去掉后比较）
                    if (!string.IsNullOrEmpty(guid) &&
                        nic.Id.Trim('{', '}').ToUpperInvariant() == guid)
                        return nic.Name;

                    // ② IPv4 地址匹配
                    if (!string.IsNullOrEmpty(ipv4))
                    {
                        foreach (var ua in nic.GetIPProperties().UnicastAddresses)
                        {
                            if (ua.Address.ToString() == ipv4)
                                return nic.Name;
                        }
                    }
                }

                // ③ 注册表查询：适用于 Npcap 枚举到但 NetworkInterface 未包含的真实适配器
                if (!string.IsNullOrEmpty(guid))
                {
                    using var key = Registry.LocalMachine.OpenSubKey(
                        $@"SYSTEM\CurrentControlSet\Control\Network\{{4D36E972-E325-11CE-BFC1-08002BE10318}}\{{{guid}}}\Connection");
                    var regName = key?.GetValue("Name") as string;
                    if (!string.IsNullOrEmpty(regName)) return regName;
                }
            }
            catch { }

            // 无法解析：有 IP 时退化显示 IP，否则返回 null（调用方将跳过此适配器）
            return string.IsNullOrEmpty(ipv4) ? null : ipv4;
        }

        /// <summary>选择发包适配器。</summary>
        public void SelectAdapter(int index)
        {
            _interop.SelectAdapter(index);
            _selectedAdapterIndex = index;
        }

        /// <summary>添加一条 Stream，成功时返回 stream id（>=0），失败返回 -1。</summary>
        public int AddStream(StreamConfig config)
        {
            int id = _interop.AddStream(config);
            if (id >= 0)
            {
                config.Id = id;
                _streams.Add(config);
            }
            return id;
        }

        /// <summary>移除指定 Stream。</summary>
        public void RemoveStream(int streamId)
        {
            _interop.SetStreamEnabled(streamId, false);
            _streams.RemoveAll(s => s.Id == streamId);
        }

        /// <summary>启用或禁用指定 Stream（运行中立即生效）。</summary>
        public void SetStreamEnabled(int streamId, bool enabled)
        {
            _interop.SetStreamEnabled(streamId, enabled);
            var s = _streams.Find(x => x.Id == streamId);
            if (s != null) s.Enabled = enabled;
        }

        /// <summary>设置速率配置。</summary>
        public void SetRateConfig(RateConfig config)
        {
            _interop.SetRateConfig(config);
        }

        /// <summary>启动发包。</summary>
        public bool Start()
        {
            if (_selectedAdapterIndex < 0)
            {
                LastError = "请先选择网络适配器";
                return false;
            }
            if (_streams.Count == 0)
            {
                LastError = "请先添加报文 Stream";
                return false;
            }

            bool ok = _interop.Start();
            _status = ok ? SendTaskStatus.Running : SendTaskStatus.Error;
            if (ok) { _startTime = DateTime.UtcNow; _lastSendTotal = 0; _lastSendBytes = 0; }
            if (!ok) LastError = "启动发包失败，请检查 Npcap 权限";
            return ok;
        }

        /// <summary>停止发包。</summary>
        public void Stop()
        {
            _interop.Stop();
            _status = SendTaskStatus.Stopped;
        }

        /// <summary>刷新统计数据（含 PPS/BPS 计算）。</summary>
        public void RefreshStats()
        {
            var raw = _interop.GetStats();
            var now = DateTime.UtcNow;
            double elapsed = (now - _lastStatsTime).TotalSeconds;
            double totalElapsed = (now - _startTime).TotalSeconds;

            double curPps = elapsed > 0 ? (raw.SendTotal - _lastSendTotal) / elapsed : 0;
            double curBps = elapsed > 0 ? (raw.SendBytes - _lastSendBytes) * 8.0 / elapsed : 0;
            double avgPps = totalElapsed > 0 ? raw.SendTotal / totalElapsed : 0;
            double avgBps = totalElapsed > 0 ? raw.SendBytes * 8.0 / totalElapsed : 0;

            _lastSendTotal = raw.SendTotal;
            _lastSendBytes = raw.SendBytes;
            _lastStatsTime = now;

            Stats = new SendStats
            {
                SendTotal  = raw.SendTotal,
                SendBytes  = raw.SendBytes,
                SendFail   = raw.SendFail,
                CurrentPps = curPps,
                CurrentBps = curBps,
                AvgPps     = avgPps,
                AvgBps     = avgBps,
            };
        }

        /// <summary>
        /// 为 [startIp, endIp] 范围内每个 IP 克隆模板帧，设置【源 IP】，返回 StreamConfig 列表。
        /// </summary>
        public List<StreamConfig> ExpandSrcIpRange(
            StreamConfig template, uint startIp, uint endIp)
        {
            var result = new List<StreamConfig>();
            for (uint ip = startIp; ip <= endIp; ip++)
            {
                var clone = CloneStream(template);
                clone.Name = $"{template.Name}_{InputValidator.UintToIp(ip)}";
                PacketTemplateBuilder.SetSourceIp(clone.FrameData, ip);
                result.Add(clone);
            }
            return result;
        }

        private static StreamConfig CloneStream(StreamConfig s) => new()
        {
            Name          = s.Name,
            Type          = s.Type,
            Enabled       = s.Enabled,
            FrameData     = (byte[])s.FrameData.Clone(),
            Rules         = new List<FieldRuleConfig>(s.Rules),
            ChecksumFlags = s.ChecksumFlags,
        };

        public void Dispose() => _interop.Dispose();
    }
}
