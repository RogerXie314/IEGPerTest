#!/usr/bin/env python3
"""提取特定表格索引的完整内容"""
import json

def extract_table(json_path, table_indices):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    endpoint_tables = data.get('endpointTables', {})
    
    for path, tables in endpoint_tables.items():
        for table in tables:
            idx = table.get('tableIndex')
            if idx in table_indices:
                print(f"\n{'='*80}")
                print(f"Endpoint: {path}")
                print(f"Table Index: {idx}")
                print('='*80)
                
                rows = table.get('rows', [])
                for i, row in enumerate(rows):
                    print(f"Row {i}:")
                    for j, cell in enumerate(row):
                        # 截断过长的单元格内容
                        cell_str = str(cell)
                        if len(cell_str) > 200:
                            print(f"  Col {j}: {cell_str[:200]}...")
                            print(f"         (truncated, total length: {len(cell_str)})")
                        else:
                            print(f"  Col {j}: {cell_str}")
                    print()

if __name__ == '__main__':
    json_path = 'artifacts/interface_doc_index.json'
    
    # 我们关心的表格索引
    target_indices = [
        17,   # clientAlertlog - 程序报警/非白名单
        15,   # clientAdminLog - 客户端操作
        63,   # hostDefenceWarning - 文件保护/注册表保护1
        86,   # hostDefenceWarning - 文件保护/注册表保护2
        20,   # clientULog - USB设备
        379,  # clientULog - USB设备（V2格式）
        225,  # clientUSBLog - USB访问告警
        321,  # virusProtectLog - 病毒告警
    ]
    
    try:
        extract_table(json_path, target_indices)
    except FileNotFoundError:
        print(f"Error: File not found: {json_path}")
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}")
