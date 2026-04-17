#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
白名单文件(.wl)解析工具
用于读取和显示IEG白名单文件的内容
"""

import struct
import sys
from pathlib import Path
from typing import List, Dict, Any
import argparse


class WhitelistEntry:
    """白名单条目"""
    def __init__(self):
        self.add_or_del = 0  # 1=添加, 2=删除
        self.is_system_file = 0
        self.item_from = 0
        self.judge_method = 0
        self.full_path = ""
        self.hash_type = 0  # 1=SHA1, 2=MD5
        self.file_hash = ""
    
    def __str__(self):
        action = "添加" if self.add_or_del == 1 else "删除" if self.add_or_del == 2 else f"未知({self.add_or_del})"
        sys_file = "是" if self.is_system_file else "否"
        hash_type_str = "SHA1" if self.hash_type == 1 else "MD5" if self.hash_type == 2 else f"未知({self.hash_type})"
        
        return (f"路径: {self.full_path}\n"
                f"  操作: {action}\n"
                f"  系统文件: {sys_file}\n"
                f"  来源: {self.item_from}\n"
                f"  判定方式: {self.judge_method}\n"
                f"  Hash类型: {hash_type_str}\n"
                f"  Hash值: {self.file_hash}")


class WhitelistReader:
    """白名单文件读取器"""
    
    # 版本标识
    WL_VERSION_1 = 0xFEFF
    WL_VERSION_2 = 0xFEFE
    WL_VERSION_3 = 0xFEFC
    WL_VERSION_4 = 0xFEFB
    
    # Hash类型
    WL_HASH_TYPE_SHA1 = 1
    WL_HASH_TYPE_MD5 = 2
    
    def __init__(self, file_path: str):
        self.file_path = Path(file_path)
        self.version = 0
        self.entries: List[WhitelistEntry] = []
    
    def read(self) -> bool:
        """读取白名单文件"""
        if not self.file_path.exists():
            print(f"错误: 文件不存在: {self.file_path}")
            return False
        
        try:
            # 使用二进制模式打开，避免编码问题
            with open(self.file_path, 'rb') as f:
                # 读取版本头 (2字节)
                version_bytes = f.read(2)
                if len(version_bytes) < 2:
                    print("错误: 文件太小，无法读取版本头")
                    return False
                
                self.version = struct.unpack('<H', version_bytes)[0]
                print(f"文件版本: 0x{self.version:04X}")
                
                # 根据版本解析
                if self.version == self.WL_VERSION_4:
                    return self._read_v4(f)
                elif self.version == self.WL_VERSION_3:
                    return self._read_v3(f)
                elif self.version == self.WL_VERSION_2:
                    return self._read_v2(f)
                elif self.version == self.WL_VERSION_1:
                    return self._read_v1(f)
                else:
                    print(f"警告: 未知版本 0x{self.version:04X}，尝试按V4格式解析")
                    return self._read_v4(f)
        
        except Exception as e:
            print(f"读取文件时出错: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def _read_v4(self, f) -> bool:
        """读取V4版本（包含来源和判定方式字段）"""
        entry_count = 0
        
        while True:
            # 读取 dwAddOrDel (4字节)
            data = f.read(4)
            if len(data) < 4:
                break  # 文件结束
            
            entry = WhitelistEntry()
            entry.add_or_del = struct.unpack('<I', data)[0]
            
            # 读取 dwIsSyetemFile (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            entry.is_system_file = struct.unpack('<I', data)[0]
            
            # 读取 dwItemFrom (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            entry.item_from = struct.unpack('<I', data)[0]
            
            # 读取 dwJudgeMethod (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            entry.judge_method = struct.unpack('<I', data)[0]
            
            # 读取 dwFullPathLength (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            path_length = struct.unpack('<I', data)[0]
            
            # 读取路径 (Unicode UTF-16LE)
            if path_length > 0:
                path_data = f.read(path_length)
                if len(path_data) < path_length:
                    print(f"警告: 条目 {entry_count + 1} 路径数据不完整")
                    break
                try:
                    entry.full_path = path_data.decode('utf-16le').rstrip('\x00')
                except:
                    entry.full_path = f"<解码失败: {path_data.hex()}>"
            
            # 读取 dwHashType (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            entry.hash_type = struct.unpack('<I', data)[0]
            
            # 读取 dwFileHashLength (4字节)
            data = f.read(4)
            if len(data) < 4:
                print(f"警告: 条目 {entry_count + 1} 数据不完整")
                break
            hash_length = struct.unpack('<I', data)[0]
            
            # 读取Hash值 (Unicode UTF-16LE)
            if hash_length > 0:
                hash_data = f.read(hash_length)
                if len(hash_data) < hash_length:
                    print(f"警告: 条目 {entry_count + 1} Hash数据不完整")
                    break
                try:
                    entry.file_hash = hash_data.decode('utf-16le').rstrip('\x00')
                except:
                    entry.file_hash = f"<解码失败: {hash_data.hex()}>"
            
            self.entries.append(entry)
            entry_count += 1
        
        print(f"成功读取 {entry_count} 条白名单记录")
        return True
    
    def _read_v3(self, f) -> bool:
        """读取V3版本（包含来源和判定方式字段）"""
        return self._read_v4(f)  # V3和V4格式相同
    
    def _read_v2(self, f) -> bool:
        """读取V2版本（最简格式：只有 dwAddOrDel + 路径 + Hash）"""
        entry_count = 0
        
        while True:
            # 读取 dwAddOrDel (4字节)
            data = f.read(4)
            if len(data) < 4:
                break
            
            entry = WhitelistEntry()
            entry.add_or_del = struct.unpack('<I', data)[0]
            
            # V2版本直接是路径长度，没有 dwIsSyetemFile, dwItemFrom, dwJudgeMethod
            
            # 读取 dwFullPathLength (4字节)
            data = f.read(4)
            if len(data) < 4:
                break
            path_length = struct.unpack('<I', data)[0]
            
            # 读取路径
            if path_length > 0 and path_length < 100000:  # 合理范围检查
                path_data = f.read(path_length)
                if len(path_data) < path_length:
                    break
                try:
                    entry.full_path = path_data.decode('utf-16le').rstrip('\x00')
                except:
                    entry.full_path = f"<解码失败>"
            
            # 读取 dwHashType (4字节)
            data = f.read(4)
            if len(data) < 4:
                break
            entry.hash_type = struct.unpack('<I', data)[0]
            
            # 读取 dwFileHashLength (4字节)
            data = f.read(4)
            if len(data) < 4:
                break
            hash_length = struct.unpack('<I', data)[0]
            
            # 读取Hash值
            if hash_length > 0 and hash_length < 10000:  # 合理范围检查
                hash_data = f.read(hash_length)
                if len(hash_data) < hash_length:
                    break
                try:
                    entry.file_hash = hash_data.decode('utf-16le').rstrip('\x00')
                except:
                    entry.file_hash = f"<解码失败>"
            
            self.entries.append(entry)
            entry_count += 1
        
        print(f"成功读取 {entry_count} 条白名单记录")
        return True
    
    def _read_v1(self, f) -> bool:
        """读取V1版本"""
        print("V1版本格式暂不支持，请联系开发者")
        return False
    
    def print_summary(self):
        """打印摘要信息"""
        print(f"\n{'='*60}")
        print(f"白名单文件: {self.file_path.name}")
        print(f"文件路径: {self.file_path}")
        print(f"文件版本: 0x{self.version:04X}")
        print(f"白名单数量: {len(self.entries)}")
        print(f"{'='*60}\n")
    
    def print_entries(self, max_count: int = None):
        """打印白名单条目"""
        count = len(self.entries) if max_count is None else min(max_count, len(self.entries))
        
        for i, entry in enumerate(self.entries[:count], 1):
            print(f"\n[{i}] {entry}")
        
        if max_count and len(self.entries) > max_count:
            print(f"\n... 还有 {len(self.entries) - max_count} 条记录未显示")
    
    def export_to_txt(self, output_path: str = None):
        """导出到文本文件"""
        if output_path is None:
            output_path = str(self.file_path.with_suffix('.txt'))
        
        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(f"白名单文件: {self.file_path.name}\n")
                f.write(f"文件路径: {self.file_path}\n")
                f.write(f"文件版本: 0x{self.version:04X}\n")
                f.write(f"白名单数量: {len(self.entries)}\n")
                f.write(f"{'='*60}\n\n")
                
                for i, entry in enumerate(self.entries, 1):
                    f.write(f"[{i}] {entry}\n\n")
            
            print(f"\n已导出到: {output_path}")
            return True
        except Exception as e:
            print(f"导出失败: {e}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description='IEG白名单文件(.wl)解析工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s file.wl                    # 显示摘要信息
  %(prog)s file.wl -l                 # 显示所有条目
  %(prog)s file.wl -l -n 10           # 显示前10条
  %(prog)s file.wl -e output.txt      # 导出到文本文件
        """
    )
    
    parser.add_argument('file', help='白名单文件路径(.wl)')
    parser.add_argument('-l', '--list', action='store_true', help='显示白名单条目')
    parser.add_argument('-n', '--number', type=int, help='显示的最大条目数')
    parser.add_argument('-e', '--export', metavar='FILE', help='导出到文本文件')
    
    args = parser.parse_args()
    
    # 读取文件
    reader = WhitelistReader(args.file)
    if not reader.read():
        return 1
    
    # 显示摘要
    reader.print_summary()
    
    # 显示条目
    if args.list:
        reader.print_entries(args.number)
    
    # 导出
    if args.export:
        reader.export_to_txt(args.export)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
