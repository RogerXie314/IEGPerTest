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
        private PolicyNotifyDelegate? _pinnedPolicyNotify;

        // ── 外部可注册的回调 ──
        public Action<string>? OnNeedReregister { get; set; }
        public Func<string, uint, byte[]?>? OnBuildHBPayload { get; set; }
        public Action<string>? OnPolicyNotify { get; set; }

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
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
            public string computerIdTemplate;
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

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate void PolicyNotifyDelegate(IntPtr clientIdPtr);

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
            int hitEvery,
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
        private static extern void NE_SetPolicyCallback(PolicyNotifyDelegate? cb);

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
                    tcpPort = clients[i].TcpPort,
                    computerIdTemplate = clients[i].ClientId ?? ""
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
        /// 启动日志发送（纯 C++ 热路径：JSON 构建+zlib+PT 打包全在 DLL 内完成）。
        /// </summary>
        public void StartLogSend(
            int typeCount,
            int logClientCount,
            int intervalMs,
            int totalMessages,
            int hitEvery = 71,
            int sleepBetweenTypesMs = 50)
        {
            NE_StartLogSend(hitEvery, typeCount, logClientCount,
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

        /// <summary>
        /// 注册 cmdId=17 策略通知回调（调用前请先设置 <see cref="OnPolicyNotify"/>）。
        /// </summary>
        public void SetPolicyCallback()
        {
            _pinnedPolicyNotify = new PolicyNotifyDelegate(OnPolicyNotifyNative);
            NE_SetPolicyCallback(_pinnedPolicyNotify);
        }

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

        private void OnPolicyNotifyNative(IntPtr clientIdPtr)
        {
            string? clientId = Marshal.PtrToStringAnsi(clientIdPtr);
            if (clientId != null)
                OnPolicyNotify?.Invoke(clientId);
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

        // ==================== IDisposable ====================

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            try { NE_Shutdown(); } catch { }
            _pinnedReregister = null;
            _pinnedBuildHB = null;
            _pinnedPolicyNotify = null;
        }
    }
}
