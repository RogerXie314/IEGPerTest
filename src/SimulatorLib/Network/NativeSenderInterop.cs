using System.Runtime.InteropServices;

namespace SimulatorLib.Network
{
    /// <summary>
    /// P/Invoke 声明：NativeSender.dll — Winsock 同步发送层。
    /// 100% 对齐 C++ 老工具 SendInfoToServer 的网络行为：
    ///   socket() → non-blocking connect(2s) → blocking mode → send(1KB chunks) → recv。
    /// C# 端负责 PT 协议打包，DLL 只做字节级 TCP 收发。
    /// </summary>
    public static class NativeSenderInterop
    {
        private const string DLL = "NativeSender";

        /// <summary>Winsock 无效句柄（对应 INVALID_SOCKET = ~0ULL）。</summary>
        public const ulong INVALID_SOCKET = unchecked((ulong)~0L);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int NS_Init();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void NS_Cleanup();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern ulong NS_CreateConnection(string host, int port);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void NS_CloseConnection(ulong sock);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int NS_SendData(ulong sock, byte[] data, int dataLen);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int NS_RecvData(ulong sock, byte[] buf, int expectLen, int timeoutMs);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint NS_SendHeartbeatAndRecv(
            ulong sock, byte[] hbPacket, int hbPacketLen, int recvTimeoutMs);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int NS_SendThreatBatch(
            ulong sock,
            byte[] pkt1, int pkt1Len,
            byte[] pkt2, int pkt2Len,
            byte[] pkt3, int pkt3Len);
    }
}
