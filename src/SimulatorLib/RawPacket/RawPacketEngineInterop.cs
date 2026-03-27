using System;
using System.Runtime.InteropServices;

namespace SimulatorLib.RawPacket
{
    /// <summary>
    /// RPE_Rule 纯值类型（fixed buffer，无托管引用），可安全 fixed 传指针。
    /// 布局与 C++ RPE_Rule (#pragma pack(1)) 完全对齐：34 字节。
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public unsafe struct RpeRule
    {
        public byte   Valid;
        public uint   Flags;
        public ushort Offset;
        public byte   Width;
        public sbyte  BitsFrom;
        public sbyte  BitsLen;
        public fixed byte Rsv[4];
        public fixed byte BaseValue[8];
        public fixed byte MaxValue[8];
        public uint   StepSize;
    }

    /// <summary>
    /// P/Invoke wrapper for RawPacketEngine.dll。
    /// </summary>
    public sealed unsafe class RawPacketEngineInterop : IDisposable
    {
        private const string DLL = "RawPacketEngine.dll";
        private bool _disposed;

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_Init();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_Cleanup();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_GetAdapterCount();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_GetAdapterInfo(
            int index, byte* name, int nameLen, byte* ipv4, int ipv4Len);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_SelectAdapter(int index);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_AddStream(
            byte* data, int len,
            [MarshalAs(UnmanagedType.LPStr)] string name,
            RpeRule* rules, int ruleCount, uint checksumFlags);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_ClearStreams();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_SetStreamEnabled(int streamId, int enabled);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_SetRateConfig(
            int speedType, long speedValue, int sndMode, long sndCount);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_Start();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_Stop();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_GetStats(
            ulong* sendTotal, ulong* sendBytes, ulong* sendFail);

        // ── 托管包装 ─────────────────────────────────────────────────────

        public bool Init() => RPE_Init() == 0;

        public void Cleanup()
        {
            if (!_disposed) { RPE_Cleanup(); _disposed = true; }
        }

        public int GetAdapterCount() => RPE_GetAdapterCount();

        public (string PcapName, string Ipv4) GetAdapterInfo(int index)
        {
            const int N = 512;
            byte* nb = stackalloc byte[N];
            byte* ib = stackalloc byte[N];
            if (RPE_GetAdapterInfo(index, nb, N, ib, N) != 0) return ("", "");
            return (Marshal.PtrToStringAnsi((IntPtr)nb) ?? "",
                    Marshal.PtrToStringAnsi((IntPtr)ib) ?? "");
        }

        public bool SelectAdapter(int index) => RPE_SelectAdapter(index) == 0;

        public int AddStream(StreamConfig config)
        {
            if (config.FrameData == null || config.FrameData.Length == 0) return -1;
            var rules = ConvertRules(config.Rules);
            fixed (byte* data = config.FrameData)
            fixed (RpeRule* rp = rules)
            {
                return RPE_AddStream(data, config.FrameData.Length,
                    config.Name ?? "",
                    rules.Length > 0 ? rp : null,
                    rules.Length, config.ChecksumFlags);
            }
        }

        public void ClearStreams() => RPE_ClearStreams();

        public void SetStreamEnabled(int streamId, bool enabled)
            => RPE_SetStreamEnabled(streamId, enabled ? 1 : 0);

        public void SetRateConfig(RateConfig config)
        {
            int st = config.SpeedType switch
            {
                SpeedType.Interval => 1, SpeedType.Fastest => 2, _ => 0
            };
            RPE_SetRateConfig(st, config.SpeedValue,
                config.SendMode == SendMode.Burst ? 1 : 0, config.BurstCount);
        }

        public bool Start() => RPE_Start() == 0;
        public void Stop()  => RPE_Stop();

        public (ulong SendTotal, ulong SendBytes, ulong SendFail) GetStats()
        {
            ulong t = 0, b = 0, f = 0;
            RPE_GetStats(&t, &b, &f);
            return (t, b, f);
        }

        private static RpeRule[] ConvertRules(
            System.Collections.Generic.List<FieldRuleConfig>? rules)
        {
            if (rules == null || rules.Count == 0) return Array.Empty<RpeRule>();
            var result = new RpeRule[rules.Count];
            for (int i = 0; i < rules.Count; i++)
            {
                var r = rules[i];
                result[i].Valid    = r.Valid ? (byte)1 : (byte)0;
                result[i].Flags    = r.Flags;
                result[i].Offset   = r.Offset;
                result[i].Width    = r.Width;
                result[i].BitsFrom = r.BitsFrom;
                result[i].BitsLen  = r.BitsLen;
                result[i].StepSize = r.StepSize;
                var bv = r.BaseValue ?? Array.Empty<byte>();
                var mv = r.MaxValue  ?? Array.Empty<byte>();
                for (int b = 0; b < 8; b++)
                {
                    result[i].BaseValue[b] = b < bv.Length ? bv[b] : (byte)0;
                    result[i].MaxValue[b]  = b < mv.Length ? mv[b] : (byte)0;
                }
            }
            return result;
        }

        public void Dispose() => Cleanup();
    }
}
