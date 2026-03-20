using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using SimulatorLib.Persistence;
using SimulatorLib.Protocol;

namespace SimulatorLib.Network
{
    /// <summary>
    /// P/Invoke wrapper for NativeEngine.dll — C++ 非阻塞 socket + OS 线程心跳/日志发送引擎。
    /// </summary>
    public sealed class NativeEngineInterop : IDisposable
    {
        private const string DLL = "NativeEngine.dll";
        private bool _disposed;

        // ── 保持委托引用，防止 GC 回收 ──
        private NeedReregisterDelegate? _pinnedReregister;
        private BuildHBPayloadDelegate? _pinnedBuildHB;
        private BuildLogPayloadDelegate? _pinnedBuildLog;

        // ── 外部可注册的回调 ──
        public Action<string>? OnNeedReregister { get; set; }
        public Func<string, uint, byte[]?>? OnBuildHBPayload { get; set; }
        /// <summary>动态构建日志 payload：(clientIdx, typeIdx, msgCount) → 打包好的字节，null 表示跳过</summary>
        public Func<int, int, int, byte[]?>? OnBuildLogPayload { get; set; }

        // ==================== Native Structs ====================

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct NE_ClientInfo
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
            public string clientId;
            public int deviceId;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
            public string ip;
            public int tcpPort;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct NE_Config
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
            public string platformHost;
            public int platformPort;
            public int hbIntervalMs;
            public int connectGateMs;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NE_Stats
        {
            public int hbConnected;
            public int hbTotal;
            public int hbSendOk;
            public int hbSendFail;
            public int hbRecvOk;
            public int hbRecvNoReg;
            public int hbReplied;
            public int disconnects;
            public int reconnects;
            public long logSendOk;
            public long logSendFail;
        }

        // ==================== Callbacks ====================

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate void NeedReregisterDelegate(IntPtr clientIdPtr);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate int BuildHBPayloadDelegate(IntPtr clientIdPtr, int deviceId, IntPtr outBuf, int outBufSize);

        /// <summary>对齐老工具：DLL 线程每次发送前回调，动态构建 payload（新时间戳、rand 字段）</summary>
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate int BuildLogPayloadDelegate(int clientIdx, int typeIdx, int msgCount, IntPtr outBuf, int outBufSize);

        // ==================== P/Invoke ====================

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int NE_Init(
            ref NE_Config config,
            [In] NE_ClientInfo[] clients,
            int clientCount,
            NeedReregisterDelegate? onNeedReregister,
            BuildHBPayloadDelegate? onBuildHBPayload);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int NE_StartHeartbeat();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int NE_StartLogSend(
            BuildLogPayloadDelegate buildPayload,
            int typeCount,
            int logClientCount,
            int intervalMs,
            int totalMessages,
            int sleepBetweenTypesMs);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void NE_StopAll();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void NE_StopLogSend();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void NE_GetStats(out NE_Stats stats);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int NE_IsLogSendRunning();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern void NE_Shutdown();

        // ==================== Managed Wrapper ====================

        /// <summary>
        /// 初始化引擎。
        /// </summary>
        public bool Init(
            string platformHost, int platformPort, int hbIntervalMs, int connectGateMs,
            IReadOnlyList<ClientRecord> clients)
        {
            var config = new NE_Config
            {
                platformHost = platformHost ?? "",
                platformPort = platformPort,
                hbIntervalMs = hbIntervalMs,
                connectGateMs = connectGateMs
            };

            var arr = new NE_ClientInfo[clients.Count];
            for (int i = 0; i < clients.Count; i++)
            {
                arr[i] = new NE_ClientInfo
                {
                    clientId = clients[i].ClientId ?? "",
                    deviceId = (int)clients[i].DeviceId,
                    ip = clients[i].IP ?? "",
                    tcpPort = clients[i].TcpPort
                };
            }

            // 创建回调委托并固定引用
            _pinnedReregister = new NeedReregisterDelegate(OnNeedReregisterNative);
            _pinnedBuildHB = new BuildHBPayloadDelegate(OnBuildHBPayloadNative);

            int result = NE_Init(ref config, arr, clients.Count, _pinnedReregister, _pinnedBuildHB);
            return result == 0;
        }

        public void StartHeartbeat() => NE_StartHeartbeat();

        /// <summary>
        /// 启动日志发送。对齐老工具：DLL 线程每次循环回调 C# 动态构建 payload（实时时间戳+rand 字段）。
        /// 调用前必须先设置 OnBuildLogPayload。
        /// </summary>
        public void StartLogSend(
            int typeCount,
            int logClientCount,
            int intervalMs,
            int totalMessages,
            int sleepBetweenTypesMs = 50)
        {
            _pinnedBuildLog = new BuildLogPayloadDelegate(OnBuildLogPayloadNative);
            NE_StartLogSend(_pinnedBuildLog, typeCount, logClientCount,
                intervalMs, totalMessages, sleepBetweenTypesMs);
        }

        public void StopAll() => NE_StopAll();
        public void StopLogSendOnly() => NE_StopLogSend();

        public NE_Stats GetStats()
        {
            NE_GetStats(out var stats);
            return stats;
        }

        public bool IsLogSendRunning() => NE_IsLogSendRunning() != 0;

        public void Shutdown() => NE_Shutdown();

        // ==================== Native Callback Implementations ====================

        private void OnNeedReregisterNative(IntPtr clientIdPtr)
        {
            string? clientId = Marshal.PtrToStringAnsi(clientIdPtr);
            if (clientId != null)
            {
                OnNeedReregister?.Invoke(clientId);
            }
        }

        private int OnBuildHBPayloadNative(IntPtr clientIdPtr, int deviceId, IntPtr outBuf, int outBufSize)
        {
            try
            {
                string? clientId = Marshal.PtrToStringAnsi(clientIdPtr);
                if (clientId == null) return 0;

                byte[]? payload = OnBuildHBPayload?.Invoke(clientId, (uint)deviceId);
                if (payload == null || payload.Length == 0) return 0;
                if (payload.Length > outBufSize) return 0;

                Marshal.Copy(payload, 0, outBuf, payload.Length);
                return payload.Length;
            }
            catch
            {
                return 0;
            }
        }

        // 对齐老工具：每次发送前动态构建 payload（新时间戳、rand 字段）
        private int OnBuildLogPayloadNative(int clientIdx, int typeIdx, int msgCount, IntPtr outBuf, int outBufSize)
        {
            try
            {
                byte[]? payload = OnBuildLogPayload?.Invoke(clientIdx, typeIdx, msgCount);
                if (payload == null || payload.Length == 0) return 0;
                if (payload.Length > outBufSize) return 0;

                Marshal.Copy(payload, 0, outBuf, payload.Length);
                return payload.Length;
            }
            catch
            {
                return 0;
            }
        }

        // ==================== IDisposable ====================

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            try { NE_Shutdown(); } catch { }
            _pinnedReregister = null;
            _pinnedBuildHB = null;
            _pinnedBuildLog = null;
        }
    }
}
