#!/usr/bin/env python3
"""搜索包含特定URL的表格及其附近表格"""
import json

def search_nearby_tables(json_path, urls):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    endpoint_tables = data.get('endpointTables', {})    
    all_tables = []
    for path, tables in endpoint_tables.items():
        for table in tables:
            table['endpoint'] = path
            all_tables.append(table)
    
    # 按表格索引排序
    all_tables.sort(key=lambda t: t.get('tableIndex', 0))
    
    for url in urls:
        print(f"\n{'='*80}")
        print(f"Searching for: {url}")
        print('='*80)
        
        # 查找包含此URL的表格
        matching_indices = []
        for table in all_tables:
            rows = table.get('rows', [])
            for row in rows:
                for cell in row:
                    if url in str(cell):
                        matching_indices.append(table.get('tableIndex'))
                        break
        
        if not matching_indices:
            print(f"No tables found for URL: {url}")
            continue
        
        print(f"Found in table indices: {matching_indices}")
        
        # 对每个匹配的索引，显示它及其前后各2个表格（共5个）
        for idx in matching_indices[:2]:  # 只处理前2个匹配
            context_start = max(0, idx - 1)
            context_end = idx + 2
            
            print(f"\n--- Context for Table {idx} (showing {context_start} to {context_end}) ---")
            
            context_tables = [t for t in all_tables 
                            if context_start <= t.get('tableIndex', 0) <= context_end]
            
            for table in context_tables:
                t_idx = table.get('tableIndex')
                endpoint = table.get('endpoint', 'Unknown')
                rows = table.get('rows', [])
                
                print(f"\nTable Index: {t_idx} | Endpoint: {endpoint}")
                print(f"Rows: {len(rows)}")
                
                for i, row in enumerate(rows):
                    if i > 10 and len(rows) > 15:  # 如果行数太多，只显示前10行
                        print(f"  ... ({len(rows) - 10} more rows)")
                        break
                    print(f"  Row {i}: {row}")

if __name__ == '__main__':
    json_path = 'artifacts/interface_doc_index.json'
    
    # 关键接口URL
    urls = [
        '/USM/clientAlertlog.do',
        '/USM/hostDefenceWarning.do',
        '/USM/clientAdminLog.do',
    ]
    
    try:
        search_nearby_tables(json_path, urls)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
