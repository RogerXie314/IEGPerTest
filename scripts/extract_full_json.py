#!/usr/bin/env python3
"""直接提取特定表格的完整JSON内容（不截断）"""
import json

def extract_full_tables(json_path, table_indices):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    endpoint_tables = data.get('endpointTables', {})
    
    output_file = 'artifacts/full_table_content.txt'
    with open(output_file, 'w', encoding='utf-8') as out:
        for path, tables in endpoint_tables.items():
            for table in tables:
                idx = table.get('tableIndex')
                if idx not in table_indices:
                    continue
                
                out.write(f"\n{'='*80}\n")
                out.write(f"Endpoint: {path}\n")
                out.write(f"Table Index: {idx}\n")
                out.write('='*80 + '\n\n')
                
                rows = table.get('rows', [])
                for i, row in enumerate(rows):
                    out.write(f"Row {i}:\n")
                    for j, cell in enumerate(row):
                        out.write(f"  Column {j}:\n")
                        # 写入完整内容，不截断
                        cell_lines = str(cell).split('\n')
                        for line in cell_lines:
                            out.write('    ' + line + '\n')
                    out.write('\n')
    
    print(f"Extracted tables to: {output_file}")
    
    # 同时直接打印到屏幕（仅前几个表格）
    for path, tables in endpoint_tables.items():
        for table in tables:
            idx = table.get('tableIndex')
            if idx not in table_indices[:3]:  # 只打印前3个表格
                continue
            
            print(f"\n{'='*80}")
            print(f"Endpoint: {path}")
            print(f"Table Index: {idx}")
            print('='*80 + '\n')
            
            rows = table.get('rows', [])
            for i, row in enumerate(rows):
                print(f"Row {i}:")
                for j, cell in enumerate(row):
                    print(f"  Col {j}:")
                    cell_lines = str(cell).split('\n')
                    for line in cell_lines:
                        print('    ' + line)
                print()

if __name__ == '__main__':
    json_path = 'artifacts/interface_doc_index.json'
    
    # 关键表格索引（从搜索结果中确定）
    target_indices = [
        379,  # clientULog - USB设备（包含完整JSON示例）
        225,  # clientUSBLog - USB访问告警
        # 17,   # clientAlertlog - 程序报警（只有URL，可能需要搜索其他表格）
        # 63, 86,  # hostDefenceWarning（只有URL）
    ]
    
    try:
        extract_full_tables(json_path, target_indices)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
