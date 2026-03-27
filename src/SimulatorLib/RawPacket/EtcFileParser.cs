using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SimulatorLib.RawPacket
{
    /// <summary>
    /// 解析和写入 xb-ether-tester .etc 二进制格式。
    /// 对齐 save_stream / load_stream 的 C 结构体布局（小端序）。
    /// </summary>
    public static class EtcFileParser
    {
        // t_rule 大小（packed）：
        //   valid(1) + flags(4) + offset(2) + width(1) + bits_from(1) + bits_len(1)
        //   + rsv(4) + base_value(8) + max_value(8) + step_size(4) = 34 bytes
        private const int RuleSize = 34;
        private const int MaxRuleNum = 10;

        // t_fc_cfg 大小：speed_type(4) + speed_value(4) + snd_mode(4) + snd_times_cnt(4) + rsv(32) = 48
        private const int FcCfgSize = 48;

        // t_pkt_cap_cfg 固定部分大小（到 filter_str_usr 之前）：
        //   9 × int32 + rsv(32) = 36 + 32 = 68
        private const int PktCapCfgFixedSize = 68;

        // STREAM_HDR_LEN（到 data 字段之前）：
        //   selected(4) + snd_cnt(1) + rsv(7) + name(64) + flags(4)
        //   + rule_num(1) + rule_idx(10) + at_rules(10×34=340) + len(4) = 435
        private const int StreamHdrSize = 435;

        /// <summary>
        /// 解析 .etc 文件，返回 (RateConfig, List&lt;StreamConfig&gt;)。
        /// </summary>
        public static (RateConfig rate, List<StreamConfig> streams) Parse(Stream input)
        {
            using var reader = new BinaryReader(input, Encoding.Latin1, leaveOpen: true);

            // ── 版本头（4 字节）────────────────────────────────────────────
            var version = reader.ReadBytes(4);
            if (version[0] != (byte)'3')
                throw new InvalidDataException(
                    $"不支持的 .etc 版本：version[0]=0x{version[0]:X2}，期望 0x33 ('3')");

            // ── t_fc_cfg（48 字节）─────────────────────────────────────────
            int speedType    = reader.ReadInt32();
            int speedValue   = reader.ReadInt32();
            int sndMode      = reader.ReadInt32();
            int sndTimesCnt  = reader.ReadInt32();
            reader.ReadBytes(32); // rsv

            var rate = new RateConfig
            {
                SpeedType  = speedType switch { 1 => SpeedType.Interval, 2 => SpeedType.Fastest, _ => SpeedType.Pps },
                SpeedValue = speedValue,
                SendMode   = sndMode == 1 ? SendMode.Burst : SendMode.Continuous,
                BurstCount = sndTimesCnt,
            };

            // ── t_pkt_cap_cfg（跳过）──────────────────────────────────────
            // 固定部分：9 × int32 + rsv(32) = 68 字节
            // 其中第 9 个 int32 是 filter_str_len
            reader.ReadBytes(8 * 4); // need_save_capture … pkt_cap_dport（8 个 int32）
            int filterStrLen = reader.ReadInt32();
            reader.ReadBytes(32);    // rsv
            if (filterStrLen > 0)
                reader.ReadBytes(filterStrLen); // filter_str_usr（变长）

            // ── Stream 数量（4 字节）──────────────────────────────────────
            int nrCurStream = reader.ReadInt32();

            var streams = new List<StreamConfig>(nrCurStream);

            for (int i = 0; i < nrCurStream; i++)
            {
                // ── Stream 头（435 字节）──────────────────────────────────
                int  selected  = reader.ReadInt32();          // 4
                byte sndCnt    = reader.ReadByte();           // 1
                reader.ReadBytes(7);                          // rsv[7]
                var  nameBytes = reader.ReadBytes(64);        // name[64]
                uint flags     = reader.ReadUInt32();         // flags（checksumFlags）
                byte ruleNum   = reader.ReadByte();           // rule_num
                reader.ReadBytes(MaxRuleNum);                 // rule_idx[10]

                // at_rules[10]（每条 34 字节）
                var rules = new List<FieldRuleConfig>(ruleNum);
                for (int r = 0; r < MaxRuleNum; r++)
                {
                    var rule = ReadRule(reader);
                    if (r < ruleNum)
                        rules.Add(rule);
                }

                int len = reader.ReadInt32();                 // len（报文字节数）

                // ── 报文数据（len 字节）───────────────────────────────────
                byte[] data = len > 0 ? reader.ReadBytes(len) : Array.Empty<byte>();

                string name = Encoding.Latin1.GetString(nameBytes).TrimEnd('\0');

                streams.Add(new StreamConfig
                {
                    Id            = i,
                    Name          = name,
                    Enabled       = selected != 0,
                    FrameData     = data,
                    Rules         = rules,
                    ChecksumFlags = flags,
                });
            }

            return (rate, streams);
        }

        /// <summary>
        /// 将 RateConfig 和 StreamConfig 列表序列化为 .etc 格式。
        /// </summary>
        public static void Write(Stream output, RateConfig rate, IEnumerable<StreamConfig> streams)
        {
            using var writer = new BinaryWriter(output, Encoding.Latin1, leaveOpen: true);

            // ── 版本头（4 字节）────────────────────────────────────────────
            writer.Write((byte)'3');
            writer.Write((byte)0);
            writer.Write((byte)0);
            writer.Write((byte)0);

            // ── t_fc_cfg（48 字节）─────────────────────────────────────────
            int speedType = rate.SpeedType switch
            {
                SpeedType.Interval => 1,
                SpeedType.Fastest  => 2,
                _                  => 0,
            };
            writer.Write(speedType);
            writer.Write((int)rate.SpeedValue);
            writer.Write(rate.SendMode == SendMode.Burst ? 1 : 0);
            writer.Write((int)rate.BurstCount);
            writer.Write(new byte[32]); // rsv

            // ── t_pkt_cap_cfg（68 字节固定 + 0 字节变长）─────────────────
            // 写 8 个 int32（全零）+ filter_str_len=0 + rsv(32)
            for (int i = 0; i < 8; i++) writer.Write(0);
            writer.Write(0); // filter_str_len = 0
            writer.Write(new byte[32]); // rsv

            // ── Stream 数量（4 字节）──────────────────────────────────────
            var streamList = new List<StreamConfig>(streams);
            writer.Write(streamList.Count);

            for (int i = 0; i < streamList.Count; i++)
            {
                var s = streamList[i];

                // selected
                writer.Write(s.Enabled ? 1 : 0);
                // snd_cnt
                writer.Write((byte)0);
                // rsv[7]
                writer.Write(new byte[7]);
                // name[64]
                var nameBytes = new byte[64];
                var encoded   = Encoding.Latin1.GetBytes(s.Name ?? "");
                int copyLen   = Math.Min(encoded.Length, 63);
                Array.Copy(encoded, nameBytes, copyLen);
                writer.Write(nameBytes);
                // flags（checksumFlags）
                writer.Write(s.ChecksumFlags);
                // rule_num
                int ruleNum = Math.Min(s.Rules?.Count ?? 0, MaxRuleNum);
                writer.Write((byte)ruleNum);
                // rule_idx[10]（全零）
                writer.Write(new byte[MaxRuleNum]);
                // at_rules[10]
                for (int r = 0; r < MaxRuleNum; r++)
                {
                    if (r < ruleNum)
                        WriteRule(writer, s.Rules![r]);
                    else
                        WriteRule(writer, new FieldRuleConfig());
                }
                // len
                int len = s.FrameData?.Length ?? 0;
                writer.Write(len);
                // data
                if (len > 0)
                    writer.Write(s.FrameData!);
            }
        }

        // ── 私有辅助方法 ──────────────────────────────────────────────────

        private static FieldRuleConfig ReadRule(BinaryReader reader)
        {
            // t_rule（packed，34 字节）：
            //   valid(1) + flags(4) + offset(2) + width(1) + bits_from(1) + bits_len(1)
            //   + rsv(4) + base_value(8) + max_value(8) + step_size(4)
            bool   valid     = reader.ReadByte() != 0;
            uint   flags     = reader.ReadUInt32();
            ushort offset    = reader.ReadUInt16();
            byte   width     = reader.ReadByte();
            sbyte  bitsFrom  = reader.ReadSByte();
            sbyte  bitsLen   = reader.ReadSByte();
            reader.ReadBytes(4); // rsv
            byte[] baseValue = reader.ReadBytes(8);
            byte[] maxValue  = reader.ReadBytes(8);
            uint   stepSize  = reader.ReadUInt32();

            return new FieldRuleConfig
            {
                Valid     = valid,
                Flags     = flags,
                Offset    = offset,
                Width     = width,
                BitsFrom  = bitsFrom,
                BitsLen   = bitsLen,
                BaseValue = baseValue,
                MaxValue  = maxValue,
                StepSize  = stepSize,
            };
        }

        private static void WriteRule(BinaryWriter writer, FieldRuleConfig rule)
        {
            writer.Write(rule.Valid ? (byte)1 : (byte)0);
            writer.Write(rule.Flags);
            writer.Write(rule.Offset);
            writer.Write(rule.Width);
            writer.Write(rule.BitsFrom);
            writer.Write(rule.BitsLen);
            writer.Write(new byte[4]); // rsv
            var baseVal = EnsureLength(rule.BaseValue, 8);
            var maxVal  = EnsureLength(rule.MaxValue,  8);
            writer.Write(baseVal);
            writer.Write(maxVal);
            writer.Write(rule.StepSize);
        }

        private static byte[] EnsureLength(byte[]? arr, int len)
        {
            if (arr != null && arr.Length == len) return arr;
            var result = new byte[len];
            if (arr != null)
                Array.Copy(arr, result, Math.Min(arr.Length, len));
            return result;
        }
    }
}
