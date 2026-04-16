using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SimulatorApp.Utils
{
    /// <summary>
    /// 白名单文件(.wl)读取器
    /// 支持V2/V3/V4格式
    /// </summary>
    public class WhitelistReader
    {
        private const ushort WL_VERSION_1 = 0xFEFF;
        private const ushort WL_VERSION_2 = 0xFEFE;
        private const ushort WL_VERSION_3 = 0xFEFC;
        private const ushort WL_VERSION_4 = 0xFEFB;

        public ushort Version { get; private set; }
        public List<WhitelistEntry> Entries { get; private set; } = new();

        private readonly string _filePath;

        public WhitelistReader(string filePath)
        {
            _filePath = filePath;
        }

        public bool Read()
        {
            try
            {
                using var fs = new FileStream(_filePath, FileMode.Open, FileAccess.Read);
                using var reader = new BinaryReader(fs);

                // 读取版本头 (2字节)
                if (fs.Length < 2)
                {
                    return false;
                }

                Version = reader.ReadUInt16();

                // 根据版本解析
                return Version switch
                {
                    WL_VERSION_4 or WL_VERSION_3 => ReadV4(reader),
                    WL_VERSION_2 => ReadV2(reader),
                    WL_VERSION_1 => false, // V1暂不支持
                    _ => ReadV4(reader) // 未知版本尝试按V4解析
                };
            }
            catch
            {
                return false;
            }
        }

        private bool ReadV4(BinaryReader reader)
        {
            // V4格式：包含所有字段
            while (reader.BaseStream.Position < reader.BaseStream.Length)
            {
                try
                {
                    var entry = new WhitelistEntry();

                    // dwAddOrDel (4字节)
                    entry.AddOrDel = reader.ReadInt32();

                    // dwIsSyetemFile (4字节)
                    entry.IsSystemFile = reader.ReadInt32();

                    // dwItemFrom (4字节)
                    entry.ItemFrom = reader.ReadInt32();

                    // dwJudgeMethod (4字节)
                    entry.JudgeMethod = reader.ReadInt32();

                    // dwFullPathLength (4字节)
                    int pathLength = reader.ReadInt32();
                    if (pathLength > 0 && pathLength < 100000)
                    {
                        byte[] pathBytes = reader.ReadBytes(pathLength);
                        entry.FullPath = Encoding.Unicode.GetString(pathBytes).TrimEnd('\0');
                    }

                    // dwHashType (4字节)
                    entry.HashType = reader.ReadInt32();

                    // dwFileHashLength (4字节)
                    int hashLength = reader.ReadInt32();
                    if (hashLength > 0 && hashLength < 10000)
                    {
                        byte[] hashBytes = reader.ReadBytes(hashLength);
                        entry.FileHash = Encoding.Unicode.GetString(hashBytes).TrimEnd('\0');
                    }

                    Entries.Add(entry);
                }
                catch (EndOfStreamException)
                {
                    break;
                }
            }

            return true;
        }

        private bool ReadV2(BinaryReader reader)
        {
            // V2格式：最简化，只有 dwAddOrDel + 路径 + Hash
            while (reader.BaseStream.Position < reader.BaseStream.Length)
            {
                try
                {
                    var entry = new WhitelistEntry();

                    // dwAddOrDel (4字节)
                    entry.AddOrDel = reader.ReadInt32();

                    // V2格式直接是路径长度，没有其他字段

                    // dwFullPathLength (4字节)
                    int pathLength = reader.ReadInt32();
                    if (pathLength > 0 && pathLength < 100000)
                    {
                        byte[] pathBytes = reader.ReadBytes(pathLength);
                        entry.FullPath = Encoding.Unicode.GetString(pathBytes).TrimEnd('\0');
                    }

                    // dwHashType (4字节)
                    entry.HashType = reader.ReadInt32();

                    // dwFileHashLength (4字节)
                    int hashLength = reader.ReadInt32();
                    if (hashLength > 0 && hashLength < 10000)
                    {
                        byte[] hashBytes = reader.ReadBytes(hashLength);
                        entry.FileHash = Encoding.Unicode.GetString(hashBytes).TrimEnd('\0');
                    }

                    Entries.Add(entry);
                }
                catch (EndOfStreamException)
                {
                    break;
                }
            }

            return true;
        }
    }

    /// <summary>
    /// 白名单条目
    /// </summary>
    public class WhitelistEntry
    {
        public int AddOrDel { get; set; }           // 1=添加, 2=删除
        public int IsSystemFile { get; set; }       // 是否系统文件
        public int ItemFrom { get; set; }           // 来源
        public int JudgeMethod { get; set; }        // 判定方式
        public string FullPath { get; set; } = string.Empty;
        public int HashType { get; set; }           // 1=SHA1, 2=MD5
        public string FileHash { get; set; } = string.Empty;
    }
}
