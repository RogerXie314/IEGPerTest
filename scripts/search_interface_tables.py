#!/usr/bin/env python3
"""搜索接口文档JSON中的特定表格数据"""
import json
import sys

def search_tables(json_path, endpoint_keywords):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    print(f"=== Interface Doc: {data.get('source', 'Unknown')} ===\n")
    
    endpoint_tables = data.get('endpointTables', {})
    
    for keyword in endpoint_keywords:
        print(f"\n{'='*80}")
        print(f"Searching for endpoints containing: {keyword}")
        print('='*80)
        
        matching_endpoints = [(path, tables) for path, tables in endpoint_tables.items() 
                            if keyword.lower() in path.lower()]
        
        if not matching_endpoints:
            print(f"No endpoints found for keyword: {keyword}")
            continue
        
        for path, tables in matching_endpoints:
            print(f"\n--- Endpoint: {path} ---")
            print(f"Found {len(tables)} table(s)")
            
            for table in tables[:2]:  # 只显示前2个表格
                table_idx = table.get('tableIndex', 'Unknown')
                rows = table.get('rows', [])
                print(f"\nTable Index: {table_idx}")
                print(f"Rows: {len(rows)}")
                
                # 打印表格内容（前5行）
                for i, row in enumerate(rows[:10]):
                    print(f"  Row {i}: {row}")
                
                if len(rows) > 10:
                    print(f"  ... ({len(rows) - 10} more rows)")

if __name__ == '__main__':
    json_path = 'artifacts/interface_doc_index.json'
    
    # 搜索关键接口
    keywords = [
        'clientAlertlog',       # 程序报警/非白名单
        'hostDefenceWarning',   # 文件保护/注册表保护/强制访问控制
        'clientULog',           # USB设备/U盘插拔
        'clientUSBLog',         # USB访问告警
        'clientAdminLog',       # 客户端操作
        'loopholeProtectLog',   # 漏洞防护
        'virusProtectLog',      # 病毒告警
    ]
    
    try:
        search_tables(json_path, keywords)
    except FileNotFoundError:
        print(f"Error: File not found: {json_path}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}")
        sys.exit(1)
