#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
调试白名单文件格式
"""

import struct
import sys

def debug_wl_file(file_path):
    """调试白名单文件"""
    print(f"正在分析文件: {file_path}")
    print("=" * 60)
    
    try:
        with open(file_path, 'rb') as f:
            # 读取前100字节
            data = f.read(100)
            
            print(f"\n文件大小: {f.seek(0, 2):,} 字节")
            f.seek(0)
            
            print(f"\n前100字节 (十六进制):")
            for i in range(0, min(len(data), 100), 16):
                hex_str = ' '.join(f'{b:02X}' for b in data[i:i+16])
                ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
                print(f"{i:04X}: {hex_str:<48} {ascii_str}")
            
            # 尝试读取版本头
            f.seek(0)
            version_bytes = f.read(2)
            if len(version_bytes) == 2:
                version = struct.unpack('<H', version_bytes)[0]
                print(f"\n版本标识: 0x{version:04X}")
                
                if version == 0xFEFF:
                    print("  -> V1 格式")
                elif version == 0xFEFE:
                    print("  -> V2 格式")
                elif version == 0xFEFC:
                    print("  -> V3 格式")
                elif version == 0xFEFB:
                    print("  -> V4 格式")
                else:
                    print(f"  -> 未知格式")
            
            # 尝试读取第一条记录
            print("\n尝试解析第一条记录:")
            print("-" * 60)
            
            f.seek(2)  # 跳过版本头
            
            # 读取 dwAddOrDel
            data = f.read(4)
            if len(data) == 4:
                add_or_del = struct.unpack('<I', data)[0]
                print(f"dwAddOrDel: {add_or_del} ({'添加' if add_or_del == 1 else '删除' if add_or_del == 2 else '未知'})")
            
            # 读取 dwIsSyetemFile
            data = f.read(4)
            if len(data) == 4:
                is_sys = struct.unpack('<I', data)[0]
                print(f"dwIsSyetemFile: {is_sys}")
            
            # 读取 dwItemFrom (V3+)
            data = f.read(4)
            if len(data) == 4:
                item_from = struct.unpack('<I', data)[0]
                print(f"dwItemFrom: {item_from}")
            
            # 读取 dwJudgeMethod (V3+)
            data = f.read(4)
            if len(data) == 4:
                judge_method = struct.unpack('<I', data)[0]
                print(f"dwJudgeMethod: {judge_method}")
            
            # 读取 dwFullPathLength
            data = f.read(4)
            if len(data) == 4:
                path_length = struct.unpack('<I', data)[0]
                print(f"dwFullPathLength: {path_length} 字节")
                
                # 读取路径
                if path_length > 0 and path_length < 10000:  # 合理范围
                    path_data = f.read(path_length)
                    if len(path_data) == path_length:
                        try:
                            path = path_data.decode('utf-16le').rstrip('\x00')
                            print(f"路径: {path}")
                        except Exception as e:
                            print(f"路径解码失败: {e}")
                            print(f"原始数据: {path_data[:40].hex()}")
            
            print("\n" + "=" * 60)
            print("分析完成")
            
    except Exception as e:
        print(f"\n错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python debug_wl.py <文件路径>")
        sys.exit(1)
    
    debug_wl_file(sys.argv[1])
