using System;
using System.Buffers.Binary;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;

namespace SimulatorLib.Protocol
{
    public enum PtEncryptType : byte
    {
        None = 0,
        Aes = 1,
    }

    public enum PtCompressType : byte
    {
        None = 0,
        Zlib = 1,
    }

    public static class PtProtocol
    {
        // Matches external/IEG_Code/code/WLProtocal/Protocal.cpp
        private const string Flag = "PT";
        private const byte ProtoVer = 1;
        private const byte SourceWindowsIeg = 3; // em_portocal_Windows_IEG

        private const int Sha1Len = 20;
        private const int KeyLen = 32;
        private const string EncryptKeyPrefix = "4&k7w4&P588%e5r684k$bj@JB$Jf8bik";

        public const int HeaderLength = 48;

        private static readonly object RandomLock = new();
        private static readonly Random Random = new();

        public static byte[] Pack(
            ReadOnlySpan<byte> src,
            uint cmdId,
            PtCompressType compressType = PtCompressType.Zlib,
            PtEncryptType encryptType = PtEncryptType.Aes,
            uint deviceId = 0,
            uint serialNumber = 0)
        {
            if (src.Length <= 0) throw new ArgumentException("src is empty", nameof(src));
            if (src.Length > ushort.MaxValue) throw new ArgumentOutOfRangeException(nameof(src), "src too large for protocol nSrcLen");

            uint crc32 = ZlibCrc32.Compute(src);

            byte[] maybeCompressed = compressType switch
            {
                PtCompressType.Zlib => ZlibCompress(src),
                PtCompressType.None => src.ToArray(),
                _ => throw new ArgumentOutOfRangeException(nameof(compressType)),
            };

            ushort randomKey = 0;
            byte fillLen = 0;

            byte[] body = encryptType switch
            {
                PtEncryptType.Aes => AesEcbEncryptWithDerivedKey(maybeCompressed, out randomKey, out fillLen),
                PtEncryptType.None => maybeCompressed,
                _ => throw new ArgumentOutOfRangeException(nameof(encryptType)),
            };

            if (body.Length > ushort.MaxValue) throw new ArgumentOutOfRangeException(nameof(src), "body too large for protocol nBodyLen");

            long unixTimeSeconds = DateTimeOffset.UtcNow.ToUnixTimeSeconds();

            var packet = new byte[HeaderLength + body.Length];
            var header = packet.AsSpan(0, HeaderLength);

            // szFlag[2]
            header[0] = (byte)Flag[0];
            header[1] = (byte)Flag[1];
            // cProtoVer
            header[2] = ProtoVer;
            // cSource
            header[3] = SourceWindowsIeg;
            // nBodyLen (uint16, network order)
            BinaryPrimitives.WriteUInt16BigEndian(header.Slice(4, 2), (ushort)body.Length);
            // nEncryptType
            header[6] = (byte)encryptType;
            // nCompressType
            header[7] = (byte)compressType;
            // nFillLen
            header[8] = fillLen;
            // nReserve
            header[9] = 0;
            // nRandomKey (uint16, network order)
            BinaryPrimitives.WriteUInt16BigEndian(header.Slice(10, 2), randomKey);
            // nSerialNumber (uint32, network order)
            BinaryPrimitives.WriteUInt32BigEndian(header.Slice(12, 4), serialNumber);
            // CheckSum (uint32, network order)
            BinaryPrimitives.WriteUInt32BigEndian(header.Slice(16, 4), crc32);
            // nSessionID (uint32, network order) - unused
            BinaryPrimitives.WriteUInt32BigEndian(header.Slice(20, 4), 0);
            // nTime (int64, network order)
            BinaryPrimitives.WriteInt64BigEndian(header.Slice(24, 8), unixTimeSeconds);
            // nCmdID (uint32, network order)
            BinaryPrimitives.WriteUInt32BigEndian(header.Slice(32, 4), cmdId);
            // nDeviceID (uint32, network order)
            BinaryPrimitives.WriteUInt32BigEndian(header.Slice(36, 4), deviceId);
            // nSrcLen (uint16, network order)
            BinaryPrimitives.WriteUInt16BigEndian(header.Slice(40, 2), (ushort)src.Length);
            // sznReserve[6]
            header.Slice(42, 6).Clear();

            body.CopyTo(packet.AsSpan(HeaderLength));
            return packet;
        }

        public static byte[] Unpack(ReadOnlySpan<byte> packet)
        {
            if (packet.Length <= HeaderLength) throw new ArgumentException("packet too short", nameof(packet));
            if (packet[0] != (byte)'P' || packet[1] != (byte)'T') throw new InvalidDataException("Invalid PT flag");

            ushort bodyLen = BinaryPrimitives.ReadUInt16BigEndian(packet.Slice(4, 2));
            if (HeaderLength + bodyLen != packet.Length) throw new InvalidDataException("Body length mismatch");

            var encryptType = (PtEncryptType)packet[6];
            var compressType = (PtCompressType)packet[7];
            byte fillLen = packet[8];
            ushort randomKey = BinaryPrimitives.ReadUInt16BigEndian(packet.Slice(10, 2));
            ushort srcLen = BinaryPrimitives.ReadUInt16BigEndian(packet.Slice(40, 2));

            ReadOnlySpan<byte> body = packet.Slice(HeaderLength, bodyLen);

            byte[] maybeCompressed = encryptType switch
            {
                PtEncryptType.Aes => AesEcbDecryptWithDerivedKey(body, randomKey, fillLen),
                PtEncryptType.None => body.ToArray(),
                _ => throw new InvalidDataException("Unknown encryptType"),
            };

            byte[] src = compressType switch
            {
                PtCompressType.Zlib => ZlibUncompressExact(maybeCompressed, srcLen),
                PtCompressType.None => maybeCompressed,
                _ => throw new InvalidDataException("Unknown compressType"),
            };

            if (src.Length != srcLen) throw new InvalidDataException("Source length mismatch");
            return src;
        }

        private static ushort GetRandomKey()
        {
            lock (RandomLock)
            {
                return (ushort)Random.Next(0, ushort.MaxValue); // 0..65534
            }
        }

        private static byte[] GetDerivedAesKey(ushort randomKey)
        {
            // C++: sha1(prefix + nRandomKey_as_decimal), then copy SHA1 (20 bytes) into a 32-byte buffer and zero-fill the rest.
            byte[] input = Encoding.ASCII.GetBytes(EncryptKeyPrefix + randomKey.ToString(CultureInfo.InvariantCulture));

            byte[] sha1;
            using (var hasher = SHA1.Create())
            {
                sha1 = hasher.ComputeHash(input);
            }

            if (sha1.Length != Sha1Len) throw new InvalidOperationException("Unexpected SHA1 length");

            var key = new byte[KeyLen];
            Buffer.BlockCopy(sha1, 0, key, 0, sha1.Length);
            return key;
        }

        private static byte[] AesEcbEncryptWithDerivedKey(byte[] src, out ushort randomKey, out byte fillLen)
        {
            randomKey = GetRandomKey();
            byte[] key = GetDerivedAesKey(randomKey);

            // C++: nFillLen = 16 - (nSrcLen % 16); always add padding even if already aligned.
            fillLen = (byte)(16 - (src.Length % 16));
            int totalLen = src.Length + fillLen;

            var padded = new byte[totalLen];
            Buffer.BlockCopy(src, 0, padded, 0, src.Length);

            using var aes = Aes.Create();
            aes.Mode = CipherMode.ECB;
            aes.Padding = PaddingMode.None;
            aes.KeySize = 256;
            aes.Key = key;

            using var encryptor = aes.CreateEncryptor();
            return encryptor.TransformFinalBlock(padded, 0, padded.Length);
        }

        private static byte[] AesEcbDecryptWithDerivedKey(ReadOnlySpan<byte> encrypted, ushort randomKey, byte fillLen)
        {
            byte[] key = GetDerivedAesKey(randomKey);

            using var aes = Aes.Create();
            aes.Mode = CipherMode.ECB;
            aes.Padding = PaddingMode.None;
            aes.KeySize = 256;
            aes.Key = key;

            using var decryptor = aes.CreateDecryptor();
            byte[] padded = decryptor.TransformFinalBlock(encrypted.ToArray(), 0, encrypted.Length);

            int srcLen = padded.Length - fillLen;
            if (srcLen < 0) throw new InvalidDataException("Invalid fillLen");

            var src = new byte[srcLen];
            Buffer.BlockCopy(padded, 0, src, 0, srcLen);
            return src;
        }

        private static byte[] ZlibCompress(ReadOnlySpan<byte> src)
        {
            using var ms = new MemoryStream();
            using (var z = new ZLibStream(ms, CompressionLevel.Optimal, leaveOpen: true))
            {
                z.Write(src);
            }
            return ms.ToArray();
        }

        private static byte[] ZlibUncompressExact(ReadOnlySpan<byte> compressed, int expectedLen)
        {
            using var input = new MemoryStream(compressed.ToArray());
            using var z = new ZLibStream(input, CompressionMode.Decompress);
            var output = new byte[expectedLen];
            int readTotal = 0;
            while (readTotal < expectedLen)
            {
                int read = z.Read(output, readTotal, expectedLen - readTotal);
                if (read == 0) break;
                readTotal += read;
            }
            if (readTotal != expectedLen) throw new InvalidDataException("Zlib uncompress length mismatch");
            return output;
        }

        private static class ZlibCrc32
        {
            // Standard CRC-32 (IEEE) with init 0, matches zlib crc32(0, Z_NULL, 0) then update.
            private static readonly uint[] Table = CreateTable();

            public static uint Compute(ReadOnlySpan<byte> data)
            {
                // zlib crc32(): crc ^= 0xFFFFFFFF; update; return crc ^ 0xFFFFFFFF.
                uint crc = 0;
                crc ^= 0xFFFFFFFFu;
                foreach (byte b in data)
                {
                    crc = Table[(crc ^ b) & 0xFF] ^ (crc >> 8);
                }
                return crc ^ 0xFFFFFFFFu;
            }

            private static uint[] CreateTable()
            {
                const uint poly = 0xEDB88320u;
                var table = new uint[256];
                for (uint i = 0; i < table.Length; i++)
                {
                    uint c = i;
                    for (int k = 0; k < 8; k++)
                    {
                        c = (c & 1) != 0 ? poly ^ (c >> 1) : (c >> 1);
                    }
                    table[i] = c;
                }
                return table;
            }
        }
    }
}
