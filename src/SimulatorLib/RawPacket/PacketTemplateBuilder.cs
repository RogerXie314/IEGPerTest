using System;
using System.Net;

namespace SimulatorLib.RawPacket
{
    /// <summary>
    /// 构建各类以太网帧模板，并提供字段编辑 + 校验和重算方法。
    /// </summary>
    public static class PacketTemplateBuilder
    {
        // ── 校验和 flags（对齐 RPE_CKSUM_* 宏）──────────────────────────
        public const uint CKSUM_IP   = 0x01;
        public const uint CKSUM_ICMP = 0x02;
        public const uint CKSUM_IGMP = 0x04;
        public const uint CKSUM_UDP  = 0x08;
        public const uint CKSUM_TCP  = 0x10;
        public const uint CKSUM_ALL  = 0x1F;

        // ── 帧偏移常量 ───────────────────────────────────────────────────
        private const int ETH_DST_OFF  = 0;
        private const int ETH_SRC_OFF  = 6;
        private const int ETH_TYPE_OFF = 12;
        private const int IP_OFF       = 14;   // IPv4 头起始
        private const int IP_PROTO_OFF = 23;
        private const int IP_CKSUM_OFF = 24;
        private const int IP_SRC_OFF   = 26;
        private const int IP_DST_OFF   = 30;
        private const int L4_OFF       = 34;   // TCP/UDP/ICMP 起始

        // ── EtherType ────────────────────────────────────────────────────
        private const ushort ETHERTYPE_IPV4 = 0x0800;
        private const ushort ETHERTYPE_ARP  = 0x0806;
        private const ushort ETHERTYPE_IPV6 = 0x86DD;

        // ════════════════════════════════════════════════════════════════
        //  Build 方法
        // ════════════════════════════════════════════════════════════════

        /// <summary>构建 ICMP Echo Request 帧（IPv4）。</summary>
        public static byte[] BuildIcmpEcho(
            byte[] srcMac, byte[] dstMac,
            uint srcIp, uint dstIp)
        {
            // 以太网(14) + IP(20) + ICMP头(8) + payload(32) = 74
            const int totalLen = 14 + 20 + 8 + 32;
            var frame = new byte[totalLen];

            WriteEthHeader(frame, dstMac, srcMac, ETHERTYPE_IPV4);
            WriteIpHeader(frame, srcIp, dstIp, 1 /*ICMP*/, 20 + 8 + 32);

            // ICMP Echo Request
            frame[34] = 8;   // type
            frame[35] = 0;   // code
            frame[36] = 0;   // checksum hi
            frame[37] = 0;   // checksum lo
            frame[38] = 0;   // id hi
            frame[39] = 1;   // id lo
            frame[40] = 0;   // seq hi
            frame[41] = 1;   // seq lo
            for (int i = 42; i < 74; i++) frame[i] = 0x61; // 'a'

            RecalculateChecksums(frame, CKSUM_IP | CKSUM_ICMP);
            return frame;
        }

        /// <summary>构建 TCP SYN 帧（IPv4）。</summary>
        public static byte[] BuildTcpSyn(
            byte[] srcMac, byte[] dstMac,
            uint srcIp, uint dstIp,
            ushort srcPort, ushort dstPort)
        {
            // 以太网(14) + IP(20) + TCP(20) = 54
            const int totalLen = 14 + 20 + 20;
            var frame = new byte[totalLen];

            WriteEthHeader(frame, dstMac, srcMac, ETHERTYPE_IPV4);
            WriteIpHeader(frame, srcIp, dstIp, 6 /*TCP*/, 20 + 20);

            // TCP SYN
            WriteUInt16BE(frame, 34, srcPort);
            WriteUInt16BE(frame, 36, dstPort);
            WriteUInt32BE(frame, 38, 0);   // seq
            WriteUInt32BE(frame, 42, 0);   // ack
            frame[46] = 0x50;              // data offset = 5*4 = 20
            frame[47] = 0x02;              // flags = SYN
            WriteUInt16BE(frame, 48, 65535); // window
            frame[50] = 0; frame[51] = 0;  // checksum
            frame[52] = 0; frame[53] = 0;  // urgent

            RecalculateChecksums(frame, CKSUM_IP | CKSUM_TCP);
            return frame;
        }

        /// <summary>构建 UDP 帧（IPv4）。</summary>
        public static byte[] BuildUdp(
            byte[] srcMac, byte[] dstMac,
            uint srcIp, uint dstIp,
            ushort srcPort, ushort dstPort,
            byte[] payload)
        {
            int udpLen = 8 + payload.Length;
            int totalLen = 14 + 20 + udpLen;
            var frame = new byte[totalLen];

            WriteEthHeader(frame, dstMac, srcMac, ETHERTYPE_IPV4);
            WriteIpHeader(frame, srcIp, dstIp, 17 /*UDP*/, 20 + udpLen);

            WriteUInt16BE(frame, 34, srcPort);
            WriteUInt16BE(frame, 36, dstPort);
            WriteUInt16BE(frame, 38, (ushort)udpLen);
            frame[40] = 0; frame[41] = 0; // checksum
            Buffer.BlockCopy(payload, 0, frame, 42, payload.Length);

            RecalculateChecksums(frame, CKSUM_IP | CKSUM_UDP);
            return frame;
        }

        /// <summary>构建 ARP Request 帧（最小 60 字节）。</summary>
        public static byte[] BuildArpRequest(
            byte[] srcMac, byte[] dstMac,
            uint senderIp, uint targetIp)
        {
            // ARP 帧 42 字节，填充到 60
            var frame = new byte[60];

            WriteEthHeader(frame, dstMac, srcMac, ETHERTYPE_ARP);

            WriteUInt16BE(frame, 14, 1);          // htype = Ethernet
            WriteUInt16BE(frame, 16, 0x0800);     // ptype = IPv4
            frame[18] = 6;                         // hlen
            frame[19] = 4;                         // plen
            WriteUInt16BE(frame, 20, 1);           // oper = request
            Buffer.BlockCopy(srcMac, 0, frame, 22, 6); // sender MAC
            WriteUInt32BE(frame, 28, senderIp);    // sender IP
            // target MAC = 00:00:00:00:00:00 (already zero)
            WriteUInt32BE(frame, 38, targetIp);    // target IP
            // 剩余字节已为 0（填充）

            return frame;
        }

        /// <summary>构建 ICMPv6 Echo Request 帧（IPv6）。</summary>
        public static byte[] BuildIcmpV6Echo(
            byte[] srcMac, byte[] dstMac,
            byte[] srcIpv6, byte[] dstIpv6)
        {
            // 以太网(14) + IPv6(40) + ICMPv6头(8) + payload(32) = 94
            const int totalLen = 14 + 40 + 8 + 32;
            var frame = new byte[totalLen];

            WriteEthHeader(frame, dstMac, srcMac, ETHERTYPE_IPV6);
            WriteIpv6Header(frame, srcIpv6, dstIpv6, 58 /*ICMPv6*/, 8 + 32);

            // ICMPv6 Echo Request
            frame[54] = 128; // type
            frame[55] = 0;   // code
            frame[56] = 0;   // checksum hi
            frame[57] = 0;   // checksum lo
            frame[58] = 0;   // id hi
            frame[59] = 1;   // id lo
            frame[60] = 0;   // seq hi
            frame[61] = 1;   // seq lo
            for (int i = 62; i < 94; i++) frame[i] = 0x61; // 'a'

            RecalculateChecksums(frame, CKSUM_ICMP);
            return frame;
        }

        // ════════════════════════════════════════════════════════════════
        //  字段编辑方法
        // ════════════════════════════════════════════════════════════════

        public static void SetDestinationIp(byte[] frame, uint dstIp)
        {
            WriteUInt32BE(frame, IP_DST_OFF, dstIp);
            RecalculateChecksums(frame, DetectChecksumFlags(frame));
        }

        public static void SetSourceIp(byte[] frame, uint srcIp)
        {
            WriteUInt32BE(frame, IP_SRC_OFF, srcIp);
            RecalculateChecksums(frame, DetectChecksumFlags(frame));
        }

        public static void SetDestinationMac(byte[] frame, byte[] dstMac)
        {
            Buffer.BlockCopy(dstMac, 0, frame, ETH_DST_OFF, 6);
        }

        public static void SetSourceMac(byte[] frame, byte[] srcMac)
        {
            Buffer.BlockCopy(srcMac, 0, frame, ETH_SRC_OFF, 6);
        }

        public static void SetDestinationPort(byte[] frame, ushort port)
        {
            WriteUInt16BE(frame, 36, port);
            RecalculateChecksums(frame, DetectChecksumFlags(frame));
        }

        public static void SetSourcePort(byte[] frame, ushort port)
        {
            WriteUInt16BE(frame, 34, port);
            RecalculateChecksums(frame, DetectChecksumFlags(frame));
        }

        // ════════════════════════════════════════════════════════════════
        //  校验和重算
        // ════════════════════════════════════════════════════════════════

        public static void RecalculateChecksums(byte[] frame, uint checksumFlags)
        {
            if (frame.Length < 14) return;

            ushort etherType = ReadUInt16BE(frame, ETH_TYPE_OFF);

            if (etherType == ETHERTYPE_IPV4 && frame.Length >= IP_OFF + 20)
            {
                if ((checksumFlags & CKSUM_IP) != 0)
                    RecalcIpChecksum(frame);

                byte proto = frame[IP_PROTO_OFF];
                if (proto == 1 && (checksumFlags & CKSUM_ICMP) != 0)
                    RecalcIcmpChecksum(frame);
                else if (proto == 6 && (checksumFlags & CKSUM_TCP) != 0)
                    RecalcTcpChecksum(frame);
                else if (proto == 17 && (checksumFlags & CKSUM_UDP) != 0)
                    RecalcUdpChecksum(frame);
            }
            else if (etherType == ETHERTYPE_IPV6 && frame.Length >= 14 + 40)
            {
                byte nextHeader = frame[14 + 6];
                if (nextHeader == 58 && (checksumFlags & CKSUM_ICMP) != 0)
                    RecalcIcmpV6Checksum(frame);
            }
        }

        // ════════════════════════════════════════════════════════════════
        //  私有辅助：帧构建
        // ════════════════════════════════════════════════════════════════

        private static void WriteEthHeader(byte[] frame, byte[] dstMac, byte[] srcMac, ushort etherType)
        {
            Buffer.BlockCopy(dstMac, 0, frame, 0, 6);
            Buffer.BlockCopy(srcMac, 0, frame, 6, 6);
            WriteUInt16BE(frame, 12, etherType);
        }

        private static void WriteIpHeader(byte[] frame, uint srcIp, uint dstIp, byte proto, int ipPayloadLen)
        {
            int totalLen = 20 + ipPayloadLen - 20; // IP total length = 20 + L4 length
            // ipPayloadLen here is the full IP payload (L4 header + data)
            int ipTotalLen = 20 + (ipPayloadLen - 20);
            // Recalculate: ipPayloadLen = L4 size, so IP total = 20 + ipPayloadLen
            // The caller passes (20 + L4size) as ipPayloadLen, so IP total = ipPayloadLen
            frame[14] = 0x45;                          // version=4, IHL=5
            frame[15] = 0;                             // TOS
            WriteUInt16BE(frame, 16, (ushort)ipPayloadLen); // total length
            frame[18] = 0; frame[19] = 0;              // ID
            frame[20] = 0x40; frame[21] = 0x00;        // DF bit
            frame[22] = 64;                            // TTL
            frame[23] = proto;
            frame[24] = 0; frame[25] = 0;              // checksum (filled later)
            WriteUInt32BE(frame, 26, srcIp);
            WriteUInt32BE(frame, 30, dstIp);
        }

        private static void WriteIpv6Header(byte[] frame, byte[] srcIpv6, byte[] dstIpv6, byte nextHeader, int payloadLen)
        {
            // version=6, traffic class=0, flow label=0
            frame[14] = 0x60;
            frame[15] = 0; frame[16] = 0; frame[17] = 0;
            WriteUInt16BE(frame, 18, (ushort)payloadLen); // payload length
            frame[20] = nextHeader;
            frame[21] = 64; // hop limit
            Buffer.BlockCopy(srcIpv6, 0, frame, 22, 16);
            Buffer.BlockCopy(dstIpv6, 0, frame, 38, 16);
        }

        // ════════════════════════════════════════════════════════════════
        //  私有辅助：校验和计算
        // ════════════════════════════════════════════════════════════════

        private static void RecalcIpChecksum(byte[] frame)
        {
            frame[24] = 0; frame[25] = 0;
            ushort cksum = InternetChecksum(frame, 14, 20);
            frame[24] = (byte)(cksum >> 8);
            frame[25] = (byte)(cksum & 0xFF);
        }

        private static void RecalcIcmpChecksum(byte[] frame)
        {
            int icmpStart = L4_OFF;
            int icmpLen = frame.Length - icmpStart;
            frame[icmpStart + 2] = 0; frame[icmpStart + 3] = 0;
            ushort cksum = InternetChecksum(frame, icmpStart, icmpLen);
            frame[icmpStart + 2] = (byte)(cksum >> 8);
            frame[icmpStart + 3] = (byte)(cksum & 0xFF);
        }

        private static void RecalcTcpChecksum(byte[] frame)
        {
            int tcpStart = L4_OFF;
            int tcpLen = frame.Length - tcpStart;
            frame[tcpStart + 16] = 0; frame[tcpStart + 17] = 0;

            // 伪头部: src IP(4) + dst IP(4) + 0(1) + proto=6(1) + TCP len(2)
            var pseudo = new byte[12 + tcpLen];
            Buffer.BlockCopy(frame, IP_SRC_OFF, pseudo, 0, 4);
            Buffer.BlockCopy(frame, IP_DST_OFF, pseudo, 4, 4);
            pseudo[8] = 0;
            pseudo[9] = 6;
            pseudo[10] = (byte)(tcpLen >> 8);
            pseudo[11] = (byte)(tcpLen & 0xFF);
            Buffer.BlockCopy(frame, tcpStart, pseudo, 12, tcpLen);

            ushort cksum = InternetChecksum(pseudo, 0, pseudo.Length);
            frame[tcpStart + 16] = (byte)(cksum >> 8);
            frame[tcpStart + 17] = (byte)(cksum & 0xFF);
        }

        private static void RecalcUdpChecksum(byte[] frame)
        {
            int udpStart = L4_OFF;
            int udpLen = frame.Length - udpStart;
            frame[udpStart + 6] = 0; frame[udpStart + 7] = 0;

            // 伪头部: src IP(4) + dst IP(4) + 0(1) + proto=17(1) + UDP len(2)
            var pseudo = new byte[12 + udpLen];
            Buffer.BlockCopy(frame, IP_SRC_OFF, pseudo, 0, 4);
            Buffer.BlockCopy(frame, IP_DST_OFF, pseudo, 4, 4);
            pseudo[8] = 0;
            pseudo[9] = 17;
            pseudo[10] = (byte)(udpLen >> 8);
            pseudo[11] = (byte)(udpLen & 0xFF);
            Buffer.BlockCopy(frame, udpStart, pseudo, 12, udpLen);

            ushort cksum = InternetChecksum(pseudo, 0, pseudo.Length);
            frame[udpStart + 6] = (byte)(cksum >> 8);
            frame[udpStart + 7] = (byte)(cksum & 0xFF);
        }

        private static void RecalcIcmpV6Checksum(byte[] frame)
        {
            // IPv6 头从 frame[14]，ICMPv6 从 frame[54]
            int icmpv6Start = 54;
            int icmpv6Len = frame.Length - icmpv6Start;
            frame[icmpv6Start + 2] = 0; frame[icmpv6Start + 3] = 0;

            // 伪头部: src IPv6(16) + dst IPv6(16) + payload_len(4) + 0(3) + next_header=58(1)
            var pseudo = new byte[40 + icmpv6Len];
            Buffer.BlockCopy(frame, 22, pseudo, 0, 16);  // src IPv6
            Buffer.BlockCopy(frame, 38, pseudo, 16, 16); // dst IPv6
            pseudo[32] = 0; pseudo[33] = 0;
            pseudo[34] = (byte)(icmpv6Len >> 8);
            pseudo[35] = (byte)(icmpv6Len & 0xFF);
            pseudo[36] = 0; pseudo[37] = 0; pseudo[38] = 0;
            pseudo[39] = 58; // next header = ICMPv6
            Buffer.BlockCopy(frame, icmpv6Start, pseudo, 40, icmpv6Len);

            ushort cksum = InternetChecksum(pseudo, 0, pseudo.Length);
            frame[icmpv6Start + 2] = (byte)(cksum >> 8);
            frame[icmpv6Start + 3] = (byte)(cksum & 0xFF);
        }

        private static ushort InternetChecksum(byte[] data, int offset, int length)
        {
            uint sum = 0;
            int end = offset + length;
            int i = offset;
            while (i + 1 < end)
            {
                sum += (uint)((data[i] << 8) | data[i + 1]);
                i += 2;
            }
            if (i < end)
                sum += (uint)(data[i] << 8);

            while (sum >> 16 != 0)
                sum = (sum & 0xFFFF) + (sum >> 16);

            return (ushort)~sum;
        }

        /// <summary>根据帧内容自动检测需要重算的校验和 flags。</summary>
        private static uint DetectChecksumFlags(byte[] frame)
        {
            if (frame.Length < 14) return 0;
            ushort etherType = ReadUInt16BE(frame, ETH_TYPE_OFF);
            if (etherType == ETHERTYPE_IPV4 && frame.Length >= IP_OFF + 20)
            {
                byte proto = frame[IP_PROTO_OFF];
                uint flags = CKSUM_IP;
                if (proto == 1)  flags |= CKSUM_ICMP;
                if (proto == 6)  flags |= CKSUM_TCP;
                if (proto == 17) flags |= CKSUM_UDP;
                return flags;
            }
            if (etherType == ETHERTYPE_IPV6 && frame.Length >= 14 + 40)
                return CKSUM_ICMP;
            return 0;
        }

        // ════════════════════════════════════════════════════════════════
        //  字节序工具
        // ════════════════════════════════════════════════════════════════

        private static void WriteUInt16BE(byte[] buf, int offset, ushort value)
        {
            buf[offset]     = (byte)(value >> 8);
            buf[offset + 1] = (byte)(value & 0xFF);
        }

        private static void WriteUInt32BE(byte[] buf, int offset, uint value)
        {
            buf[offset]     = (byte)(value >> 24);
            buf[offset + 1] = (byte)(value >> 16);
            buf[offset + 2] = (byte)(value >> 8);
            buf[offset + 3] = (byte)(value & 0xFF);
        }

        private static ushort ReadUInt16BE(byte[] buf, int offset)
            => (ushort)((buf[offset] << 8) | buf[offset + 1]);
    }
}
