#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试白名单解析工具
"""

import sys
from pathlib import Path

# 测试文件路径
TEST_FILE = r"G:\Pictures\stress\wl\IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl"

def test_reader():
    """测试命令行版本"""
    print("=" * 60)
    print("测试白名单文件解析工具")
    print("=" * 60)
    
    # 检查测试文件是否存在
    test_path = Path(TEST_FILE)
    if not test_path.exists():
        print(f"\n错误: 测试文件不存在: {TEST_FILE}")
        print("请修改 test_reader.py 中的 TEST_FILE 路径")
        return False
    
    print(f"\n测试文件: {test_path.name}")
    print(f"文件大小: {test_path.stat().st_size:,} 字节")
    
    # 导入解析器
    try:
        from wl_reader import WhitelistReader
    except ImportError as e:
        print(f"\n错误: 无法导入 wl_reader 模块: {e}")
        return False
    
    # 读取文件
    print("\n正在读取文件...")
    reader = WhitelistReader(str(test_path))
    
    if not reader.read():
        print("读取失败！")
        return False
    
    # 显示摘要
    reader.print_summary()
    
    # 显示前5条记录
    print("前5条白名单记录:")
    print("-" * 60)
    reader.print_entries(max_count=5)
    
    # 统计信息
    print(f"\n{'='*60}")
    print("统计信息:")
    print(f"{'='*60}")
    
    add_count = sum(1 for e in reader.entries if e.add_or_del == 1)
    del_count = sum(1 for e in reader.entries if e.add_or_del == 2)
    sys_count = sum(1 for e in reader.entries if e.is_system_file)
    sha1_count = sum(1 for e in reader.entries if e.hash_type == 1)
    md5_count = sum(1 for e in reader.entries if e.hash_type == 2)
    
    print(f"添加操作: {add_count} 条")
    print(f"删除操作: {del_count} 条")
    print(f"系统文件: {sys_count} 条")
    print(f"SHA1 Hash: {sha1_count} 条")
    print(f"MD5 Hash: {md5_count} 条")
    
    print(f"\n{'='*60}")
    print("测试完成！")
    print(f"{'='*60}")
    
    return True


if __name__ == '__main__':
    try:
        success = test_reader()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n发生错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
