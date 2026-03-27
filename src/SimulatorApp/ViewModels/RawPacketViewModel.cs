using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using Microsoft.Win32;
using SimulatorLib.RawPacket;

namespace SimulatorApp.ViewModels
{
    public class RawPacketViewModel : INotifyPropertyChanged
    {
        // ── Service ──────────────────────────────────────────────────────
        private readonly RawPacketSenderService _service;
        private readonly DispatcherTimer _statsTimer;

        // ── Inner types ───────────────────────────────────────────────────
        public class BuiltinPacketItem : INotifyPropertyChanged
        {
            private bool _isSelected;
            public string Name       { get; init; } = "";
            public string TargetOs   { get; init; } = "";
            public string ResourceName { get; init; } = "";
            public string DisplayText => $"{Name} ({TargetOs})";

            public bool IsSelected
            {
                get => _isSelected;
                set { _isSelected = value; OnPropertyChanged(); SelectionChanged?.Invoke(this, EventArgs.Empty); }
            }

            public event EventHandler? SelectionChanged;
            public event PropertyChangedEventHandler? PropertyChanged;
            private void OnPropertyChanged([CallerMemberName] string? n = null)
                => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
        }

        // ── Backing fields ────────────────────────────────────────────────
        private NicAdapterInfo? _selectedAdapter;
        private StreamConfig?   _selectedStream;
        private string _destIp       = "";
        private string _destMac      = "";
        private string _srcIpStart   = "";
        private string _srcIpEnd     = "";
        private string _destIpError  = "";
        private string _destMacError = "";
        private string _srcIpRangeError = "";
        private string _rateError    = "";
        private string _statusText   = "";
        private string _npcapStatusText = "未知";
        private bool   _isPpsMode      = true;
        private bool   _isIntervalMode = false;
        private bool   _isFastestMode  = false;
        private bool   _isContinuousMode = true;
        private bool   _isBurstMode    = false;
        private string _ppsValue       = "1000";
        private string _intervalUs     = "1000";
        private string _burstCount     = "100";
        private SendStats _stats       = new();

        // ── Observable collections ────────────────────────────────────────
        public ObservableCollection<NicAdapterInfo>   Adapters      { get; } = new();
        public ObservableCollection<StreamConfig>     Streams       { get; } = new();
        public ObservableCollection<BuiltinPacketItem> BuiltinPackets { get; } = new();

        // ── Properties ────────────────────────────────────────────────────
        public NicAdapterInfo? SelectedAdapter
        {
            get => _selectedAdapter;
            set
            {
                _selectedAdapter = value;
                OnPropertyChanged();
                if (value != null) _service.SelectAdapter(value.Index);
            }
        }

        public StreamConfig? SelectedStream
        {
            get => _selectedStream;
            set { _selectedStream = value; OnPropertyChanged(); }
        }

        public string DestIp
        {
            get => _destIp;
            set
            {
                _destIp = value; OnPropertyChanged();
                DestIpError = string.IsNullOrWhiteSpace(value) ? "" :
                    InputValidator.IsValidIpv4(value) ? "" : "无效 IPv4 地址";
            }
        }

        public string DestMac
        {
            get => _destMac;
            set
            {
                _destMac = value; OnPropertyChanged();
                DestMacError = string.IsNullOrWhiteSpace(value) ? "" :
                    InputValidator.IsValidMac(value) ? "" : "无效 MAC 地址（格式：AA:BB:CC:DD:EE:FF）";
            }
        }

        public string SrcIpStart
        {
            get => _srcIpStart;
            set { _srcIpStart = value; OnPropertyChanged(); ValidateSrcIpRange(); }
        }

        public string SrcIpEnd
        {
            get => _srcIpEnd;
            set { _srcIpEnd = value; OnPropertyChanged(); ValidateSrcIpRange(); }
        }

        public string DestIpError
        {
            get => _destIpError;
            private set { _destIpError = value; OnPropertyChanged(); }
        }

        public string DestMacError
        {
            get => _destMacError;
            private set { _destMacError = value; OnPropertyChanged(); }
        }

        public string SrcIpRangeError
        {
            get => _srcIpRangeError;
            private set { _srcIpRangeError = value; OnPropertyChanged(); }
        }

        public string RateError
        {
            get => _rateError;
            private set { _rateError = value; OnPropertyChanged(); }
        }

        public string StatusText
        {
            get => _statusText;
            private set { _statusText = value; OnPropertyChanged(); }
        }

        public string NpcapStatusText
        {
            get => _npcapStatusText;
            private set { _npcapStatusText = value; OnPropertyChanged(); }
        }

        public bool IsPpsMode
        {
            get => _isPpsMode;
            set { _isPpsMode = value; OnPropertyChanged(); if (value) { _isIntervalMode = false; _isFastestMode = false; OnPropertyChanged(nameof(IsIntervalMode)); OnPropertyChanged(nameof(IsFastestMode)); } }
        }

        public bool IsIntervalMode
        {
            get => _isIntervalMode;
            set { _isIntervalMode = value; OnPropertyChanged(); if (value) { _isPpsMode = false; _isFastestMode = false; OnPropertyChanged(nameof(IsPpsMode)); OnPropertyChanged(nameof(IsFastestMode)); } }
        }

        public bool IsFastestMode
        {
            get => _isFastestMode;
            set { _isFastestMode = value; OnPropertyChanged(); if (value) { _isPpsMode = false; _isIntervalMode = false; OnPropertyChanged(nameof(IsPpsMode)); OnPropertyChanged(nameof(IsIntervalMode)); } }
        }

        public bool IsContinuousMode
        {
            get => _isContinuousMode;
            set { _isContinuousMode = value; OnPropertyChanged(); if (value) { _isBurstMode = false; OnPropertyChanged(nameof(IsBurstMode)); } }
        }

        public bool IsBurstMode
        {
            get => _isBurstMode;
            set { _isBurstMode = value; OnPropertyChanged(); if (value) { _isContinuousMode = false; OnPropertyChanged(nameof(IsContinuousMode)); } }
        }

        public string PpsValue
        {
            get => _ppsValue;
            set { _ppsValue = value; OnPropertyChanged(); }
        }

        public string IntervalUs
        {
            get => _intervalUs;
            set { _intervalUs = value; OnPropertyChanged(); }
        }

        public string BurstCount
        {
            get => _burstCount;
            set { _burstCount = value; OnPropertyChanged(); }
        }

        public SendStats Stats
        {
            get => _stats;
            private set { _stats = value; OnPropertyChanged(); }
        }

        // ── Commands ──────────────────────────────────────────────────────
        public ICommand StartCommand        { get; }
        public ICommand StopCommand         { get; }
        public ICommand ImportCommand       { get; }
        public ICommand EditStreamCommand   { get; }
        public ICommand DeleteStreamCommand { get; }
        public ICommand ClearStreamsCommand { get; }

        // ── Constructor ───────────────────────────────────────────────────
        public RawPacketViewModel()
        {
            var interop = new RawPacketEngineInterop();
            _service = new RawPacketSenderService(interop);

            // Init builtin packets
            foreach (var def in BuiltinPacketLoader.Definitions)
            {
                var item = new BuiltinPacketItem
                {
                    Name         = def.Name,
                    TargetOs     = def.TargetOs,
                    ResourceName = def.ResourceName,
                };
                item.SelectionChanged += OnBuiltinSelectionChanged;
                BuiltinPackets.Add(item);
            }

            StartCommand        = new RelayCommand(_ => ExecuteStart());
            StopCommand         = new RelayCommand(_ => ExecuteStop());
            ImportCommand       = new RelayCommand(_ => ExecuteImport());
            EditStreamCommand   = new RelayCommand(_ => ExecuteEditStream(), _ => SelectedStream != null);
            DeleteStreamCommand = new RelayCommand(_ => ExecuteDeleteStream(), _ => SelectedStream != null);
            ClearStreamsCommand = new RelayCommand(_ => ExecuteClearStreams());

            _statsTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            _statsTimer.Tick += (_, _) => RefreshStats();
            _statsTimer.Start();
        }

        // ── Initialization ────────────────────────────────────────────────
        public async Task InitializeAsync()
        {
            await Task.Run(() =>
            {
                bool ok = _service.Initialize();
                Application.Current.Dispatcher.Invoke(() =>
                {
                    NpcapStatusText = ok ? "已安装 ✓" : $"未就绪 — {_service.LastError}";
                    Adapters.Clear();
                    foreach (var a in _service.Adapters)
                        Adapters.Add(a);
                    if (Adapters.Count > 0)
                        SelectedAdapter = Adapters[0];
                });
            });
        }

        public void OnWindowClosing()
        {
            _statsTimer.Stop();
            if (_service.Status == SendTaskStatus.Running)
                _service.Stop();
        }

        // ── Private helpers ───────────────────────────────────────────────
        private void ValidateSrcIpRange()
        {
            if (string.IsNullOrWhiteSpace(_srcIpStart) && string.IsNullOrWhiteSpace(_srcIpEnd))
            {
                SrcIpRangeError = "";
                return;
            }
            if (!InputValidator.IsValidIpv4(_srcIpStart) || !InputValidator.IsValidIpv4(_srcIpEnd))
            {
                SrcIpRangeError = "起始/结束 IP 格式无效";
                return;
            }
            SrcIpRangeError = InputValidator.IsValidIpRange(_srcIpStart, _srcIpEnd)
                ? "" : "起始 IP 必须 ≤ 结束 IP";
        }

        private RateConfig BuildRateConfig()
        {
            var cfg = new RateConfig();
            if (IsFastestMode)
            {
                cfg.SpeedType = SpeedType.Fastest;
            }
            else if (IsIntervalMode)
            {
                cfg.SpeedType = SpeedType.Interval;
                cfg.SpeedValue = long.TryParse(_intervalUs, out var iv) ? iv : 1000;
            }
            else
            {
                cfg.SpeedType = SpeedType.Pps;
                cfg.SpeedValue = long.TryParse(_ppsValue, out var pps) ? pps : 1000;
            }
            cfg.SendMode   = IsBurstMode ? SendMode.Burst : SendMode.Continuous;
            cfg.BurstCount = long.TryParse(_burstCount, out var bc) ? bc : 100;
            return cfg;
        }

        private bool ValidateForStart()
        {
            bool ok = true;
            if (string.IsNullOrWhiteSpace(_destIp) || !InputValidator.IsValidIpv4(_destIp))
            {
                DestIpError = "目的 IP 必填且格式正确";
                ok = false;
            }
            if (string.IsNullOrWhiteSpace(_destMac) || !InputValidator.IsValidMac(_destMac))
            {
                DestMacError = "目的 MAC 必填且格式正确";
                ok = false;
            }
            if (!string.IsNullOrWhiteSpace(_srcIpRangeError))
                ok = false;

            if (!IsFastestMode)
            {
                if (IsPpsMode && (!long.TryParse(_ppsValue, out var pps) || !InputValidator.IsValidPps(pps)))
                {
                    RateError = "PPS 须为 1~1000000";
                    ok = false;
                }
                else if (IsIntervalMode && (!long.TryParse(_intervalUs, out var iv) || iv < 1))
                {
                    RateError = "间隔须 ≥ 1 μs";
                    ok = false;
                }
                else
                {
                    RateError = "";
                }
            }
            else
            {
                RateError = "";
            }

            return ok;
        }

        private void ExecuteStart()
        {
            if (!ValidateForStart()) return;

            // Clear existing streams in service
            foreach (var s in Streams.ToList())
                _service.RemoveStream(s.Id);
            Streams.Clear();

            // Load selected builtin packets
            var dstMacBytes = ParseMac(_destMac);
            var dstIpUint   = InputValidator.IpToUint(_destIp);
            var srcMacBytes = new byte[] { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

            foreach (var item in BuiltinPackets.Where(b => b.IsSelected))
            {
                try
                {
                    var loaded = BuiltinPacketLoader.Load(item.ResourceName);
                    foreach (var sc in loaded)
                    {
                        // Apply dest IP/MAC
                        if (sc.FrameData.Length >= 34)
                        {
                            PacketTemplateBuilder.SetDestinationMac(sc.FrameData, dstMacBytes);
                            PacketTemplateBuilder.SetDestinationIp(sc.FrameData, dstIpUint);
                        }
                        sc.DstIp  = _destIp;
                        sc.DstMac = _destMac;
                        sc.Type   = PacketType.Custom;
                        AddStreamToService(sc);
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"加载内置报文 {item.Name} 失败：{ex.Message}", "错误",
                        MessageBoxButton.OK, MessageBoxImage.Warning);
                }
            }

            // Expand IP range if provided
            if (!string.IsNullOrWhiteSpace(_srcIpStart) && !string.IsNullOrWhiteSpace(_srcIpEnd)
                && string.IsNullOrEmpty(_srcIpRangeError))
            {
                var startUint = InputValidator.IpToUint(_srcIpStart);
                var endUint   = InputValidator.IpToUint(_srcIpEnd);
                var toExpand  = Streams.ToList();
                foreach (var s in toExpand)
                {
                    _service.RemoveStream(s.Id);
                    Streams.Remove(s);
                    var expanded = _service.ExpandIpRange(s, startUint, endUint);
                    foreach (var es in expanded)
                        AddStreamToService(es);
                }
            }

            if (Streams.Count == 0)
            {
                StatusText = "没有可发送的 Stream，请勾选内置报文或导入报文";
                return;
            }

            _service.SetRateConfig(BuildRateConfig());
            bool ok = _service.Start();
            StatusText = ok ? "发送中…" : $"启动失败：{_service.LastError}";
        }

        private void ExecuteStop()
        {
            _service.Stop();
            StatusText = "已停止";
        }

        private void ExecuteImport()
        {
            var dlg = new OpenFileDialog
            {
                Filter = "报文文件 (*.etc;*.pcap)|*.etc;*.pcap|所有文件 (*.*)|*.*",
                Title  = "导入报文"
            };
            if (dlg.ShowDialog() != true) return;

            try
            {
                var ext = Path.GetExtension(dlg.FileName).ToLowerInvariant();
                if (ext == ".etc")
                {
                    using var fs = File.OpenRead(dlg.FileName);
                    var (_, streams) = EtcFileParser.Parse(fs);
                    foreach (var s in streams)
                    {
                        s.Type = PacketType.Custom;
                        AddStreamToService(s);
                    }
                }
                else if (ext == ".pcap")
                {
                    var streams = ParsePcap(dlg.FileName);
                    foreach (var s in streams)
                        AddStreamToService(s);
                }
                else
                {
                    MessageBox.Show("不支持的文件格式", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"导入失败：{ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ExecuteEditStream()
        {
            if (SelectedStream == null) return;
            var s = SelectedStream;

            var dlg = new Views.StreamEditDialog(
                dstMac: s.DstMac,
                srcMac: "",
                dstIp:  s.DstIp,
                srcIp:  s.SrcIp,
                dstPort: "",
                srcPort: "")
            {
                Owner = Application.Current.MainWindow
            };

            if (dlg.ShowDialog() != true) return;

            if (!string.IsNullOrWhiteSpace(dlg.DstMac) && InputValidator.IsValidMac(dlg.DstMac))
            {
                PacketTemplateBuilder.SetDestinationMac(s.FrameData, ParseMac(dlg.DstMac));
                s.DstMac = dlg.DstMac;
            }
            if (!string.IsNullOrWhiteSpace(dlg.SrcMac) && InputValidator.IsValidMac(dlg.SrcMac))
                PacketTemplateBuilder.SetSourceMac(s.FrameData, ParseMac(dlg.SrcMac));

            if (!string.IsNullOrWhiteSpace(dlg.DstIp) && InputValidator.IsValidIpv4(dlg.DstIp))
            {
                PacketTemplateBuilder.SetDestinationIp(s.FrameData, InputValidator.IpToUint(dlg.DstIp));
                s.DstIp = dlg.DstIp;
            }
            if (!string.IsNullOrWhiteSpace(dlg.SrcIp) && InputValidator.IsValidIpv4(dlg.SrcIp))
            {
                PacketTemplateBuilder.SetSourceIp(s.FrameData, InputValidator.IpToUint(dlg.SrcIp));
                s.SrcIp = dlg.SrcIp;
            }
            if (!string.IsNullOrWhiteSpace(dlg.DstPort) && ushort.TryParse(dlg.DstPort, out var dp))
                PacketTemplateBuilder.SetDestinationPort(s.FrameData, dp);
            if (!string.IsNullOrWhiteSpace(dlg.SrcPort) && ushort.TryParse(dlg.SrcPort, out var sp))
                PacketTemplateBuilder.SetSourcePort(s.FrameData, sp);

            // Refresh the list view
            var idx = Streams.IndexOf(s);
            if (idx >= 0)
            {
                Streams.RemoveAt(idx);
                Streams.Insert(idx, s);
                SelectedStream = s;
            }
        }

        private void ExecuteDeleteStream()
        {
            if (SelectedStream == null) return;
            _service.RemoveStream(SelectedStream.Id);
            Streams.Remove(SelectedStream);
            SelectedStream = null;
        }

        private void ExecuteClearStreams()
        {
            foreach (var s in Streams.ToList())
                _service.RemoveStream(s.Id);
            Streams.Clear();
        }

        private void OnBuiltinSelectionChanged(object? sender, EventArgs e)
        {
            // Builtin selection changes are applied at Start time
        }

        private void RefreshStats()
        {
            if (_service.Status == SendTaskStatus.Running)
            {
                _service.RefreshStats();
                Stats = _service.Stats;
            }
        }

        private void AddStreamToService(StreamConfig sc)
        {
            int id = _service.AddStream(sc);
            if (id >= 0)
            {
                sc.Id = id;
                Streams.Add(sc);
            }
        }

        private static byte[] ParseMac(string mac)
        {
            var parts = mac.Split(':', '-');
            var result = new byte[6];
            for (int i = 0; i < 6; i++)
                result[i] = Convert.ToByte(parts[i], 16);
            return result;
        }

        private static List<StreamConfig> ParsePcap(string filePath)
        {
            var result = new List<StreamConfig>();
            using var fs = File.OpenRead(filePath);
            using var reader = new BinaryReader(fs);

            // Global header (24 bytes)
            uint magic = reader.ReadUInt32();
            bool bigEndian = magic == 0xD4C3B2A1;
            if (magic != 0xA1B2C3D4 && magic != 0xD4C3B2A1)
                throw new InvalidDataException("不是有效的 pcap 文件（magic number 不匹配）");

            reader.ReadUInt16(); // version_major
            reader.ReadUInt16(); // version_minor
            reader.ReadInt32();  // thiszone
            reader.ReadUInt32(); // sigfigs
            reader.ReadUInt32(); // snaplen
            reader.ReadUInt32(); // network

            int idx = 0;
            while (fs.Position < fs.Length - 16)
            {
                uint tsSec  = reader.ReadUInt32();
                uint tsUsec = reader.ReadUInt32();
                uint inclLen = reader.ReadUInt32();
                uint origLen = reader.ReadUInt32();

                if (bigEndian)
                {
                    inclLen = ReverseBytes(inclLen);
                    origLen = ReverseBytes(origLen);
                }

                if (inclLen > 65535) break; // sanity check
                byte[] data = reader.ReadBytes((int)inclLen);

                result.Add(new StreamConfig
                {
                    Id        = idx,
                    Name      = $"pcap_{idx}",
                    Type      = PacketType.Custom,
                    Enabled   = true,
                    FrameData = data,
                });
                idx++;
            }
            return result;
        }

        private static uint ReverseBytes(uint value)
            => ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16)
             | (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);

        // ── INotifyPropertyChanged ────────────────────────────────────────
        public event PropertyChangedEventHandler? PropertyChanged;
        private void OnPropertyChanged([CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}
