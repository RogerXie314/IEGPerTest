using System;
using System.Runtime.InteropServices;
using System.Text;

#pragma warning disable CS8500 // unsafe pointer to managed type

namespace SimulatorLib.RawPacket
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct RpeRule
    {
        public byte   Valid;
        public uint   Flags;
        public ushort Offset;
        public byte   Width;
        public sbyte  BitsFrom;
        public sbyte  BitsLen;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] Rsv;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public byte[] BaseValue;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public byte[] MaxValue;
        public uint   StepSize;
    }

    /// <summary>
    /// P/Invoke wrapper for RawPacketEngine.dll — 基于 Npcap 的原始报文发送引擎。
    /// </summary>
    public sealed unsafe class RawPacketEngineInterop : IDisposable
    {
        private const string DLL = "RawPacketEngine.dll";
        private bool _disposed;

        // ── P/Invoke 声明 ────────────────────────────────────────────────

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_Init();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_Cleanup();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_GetAdapterCount();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_GetAdapterInfo(
            int    index,
            byte*  name,    int nameLen,
            byte*  ipv4,    int ipv4Len);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_SelectAdapter(int index);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_AddStream(
            byte*    data,
            int      len,
            [MarshalAs(UnmanagedType.LPStr)] string name,
            RpeRule* rules,
            int      ruleCount,
            uint     checksumFlags);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_ClearStreams();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_SetStreamEnabled(int streamId, int enabled);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_SetRateConfig(
            int   speedType,
            long  speedValue,
            int   sndMode,
            long  sndCount);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RPE_Start();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_Stop();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RPE_GetStats(
            ulong* sendTotal,
            ulong* sendBytes,
            ulong* sendFail);

        // ── 托管包装方法 ─────────────────────────────────────────────────

        public bool Init() => RPE_Init() == 0;

        public void Cleanup()
        {
            if (!_disposed)
            {
                RPE_Cleanup();
                _disposed = true;
            }
        }

        public int GetAdapterCount() => RPE_GetAdapterCount();

        public (string Name, string Ipv4) GetAdapterInfo(int index)
        {
            const int BufSize = 256;
            byte* nameBuf = stackalloc byte[BufSize];
            byte* ipv4Buf = stackalloc byte[BufSize];

            int ret = RPE_GetAdapterInfo(index, nameBuf, BufSize, ipv4Buf, BufSize);
            if (ret != 0) return ("", "");

            string name = Marshal.PtrToStringAnsi((IntPtr)nameBuf) ?? "";
            string ipv4 = Marshal.PtrToStringAnsi((IntPtr)ipv4Buf) ?? "";
            return (name, ipv4);
        }

        public bool SelectAdapter(int index) => RPE_SelectAdapter(index) == 0;

        public int AddStream(StreamConfig config)
        {
            var rules = ConvertRules(config.Rules);
            if (rules.Length == 0)
            {
                fixed (byte* data = config.FrameData)
                    return RPE_AddStream(data, config.FrameData.Length,
                        config.Name, null, 0, config.ChecksumFlags);
            }

            var handle = GCHandle.Alloc(rules, GCHandleType.Pinned);
            try
            {
                fixed (byte* data = config.FrameData)
                    return RPE_AddStream(data, config.FrameData.Length,
                        config.Name,
                        (RpeRule*)handle.AddrOfPinnedObject(),
                        rules.Length, config.ChecksumFlags);
            }
            finally
            {
                handle.Free();
            }
        }

        public void ClearStreams() => RPE_ClearStreams();

        public void SetStreamEnabled(int streamId, bool enabled)
            => RPE_SetStreamEnabled(streamId, enabled ? 1 : 0);

        public void SetRateConfig(RateConfig config)
        {
            int speedType = config.SpeedType switch
            {
                SpeedType.Pps      => 0,
                SpeedType.Interval => 1,
                SpeedType.Fastest  => 2,
                _                  => 0
            };
            int sndMode = config.SendMode == SendMode.Burst ? 1 : 0;
            RPE_SetRateConfig(speedType, config.SpeedValue, sndMode, config.BurstCount);
        }

        public bool Start() => RPE_Start() == 0;

        public void Stop() => RPE_Stop();

        public (ulong SendTotal, ulong SendBytes, ulong SendFail) GetStats()
        {
            ulong total = 0, bytes = 0, fail = 0;
            RPE_GetStats(&total, &bytes, &fail);
            return (total, bytes, fail);
        }

        // ── 规则转换 ─────────────────────────────────────────────────────

        private static RpeRule[] ConvertRules(System.Collections.Generic.List<FieldRuleConfig> rules)
        {
            if (rules == null || rules.Count == 0) return Array.Empty<RpeRule>();

            var result = new RpeRule[rules.Count];
            for (int i = 0; i < rules.Count; i++)
            {
                var r = rules[i];
                result[i] = new RpeRule
                {
                    Valid     = r.Valid ? (byte)1 : (byte)0,
                    Flags     = r.Flags,
                    Offset    = r.Offset,
                    Width     = r.Width,
                    BitsFrom  = r.BitsFrom,
                    BitsLen   = r.BitsLen,
                    Rsv       = new byte[4],
                    BaseValue = PadTo8(r.BaseValue),
                    MaxValue  = PadTo8(r.MaxValue),
                    StepSize  = r.StepSize,
                };
            }
            return result;
        }

        private static byte[] PadTo8(byte[] src)
        {
            var dst = new byte[8];
            if (src != null)
                Buffer.BlockCopy(src, 0, dst, 0, Math.Min(src.Length, 8));
            return dst;
        }

        // ── IDisposable ──────────────────────────────────────────────────

        public void Dispose() => Cleanup();
    }
}
