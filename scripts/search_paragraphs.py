#!/usr/bin/env python3
"""搜索接口文档中的段落，查找JSON示例"""
import json
import re

def search_paragraphs(json_path, keywords):
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    paragraphs = data.get('paragraphs', [])
    
    print(f"Total paragraphs: {len(paragraphs)}\n")
    
    for keyword in keywords:
        print(f"\n{'='*80}")
        print(f"Searching for: {keyword}")
        print('='*80)
        
        matches = []
        for i, para in enumerate(paragraphs):
            if keyword.lower() in para.lower():
                # 找到匹配，获取上下文（前后各2个段落）
                start = max(0, i - 2)
                end = min(len(paragraphs), i + 3)
                context = paragraphs[start:end]
                matches.append((i, context))
        
        if not matches:
            print(f"No paragraphs found for: {keyword}")
            continue
        
        print(f"Found {len(matches)} matches")
        
        # 只显示前3个匹配
        for match_idx, (para_idx, context) in enumerate(matches[:3]):
            print(f"\n--- Match {match_idx + 1} (paragraph {para_idx}) ---")
            for j, para in enumerate(context):
                pos = para_idx - 2 + j
                marker = ">>>" if pos == para_idx else "   "
                # 截断过长的段落
                display = para[:500] if len(para) > 500 else para
                print(f"{marker} [{pos}]: {display}")
                if len(para) > 500:
                    print(f"         ... (truncated, total length: {len(para)})")

if __name__ == '__main__':
    json_path = 'artifacts/interface_doc_index.json'
    
    # 搜索关键词
    keywords = [
        'clientAlertlog',
        'CMDContent',
        'hostDefenceWarning',
        'DetailLogTypeLevel2',
    ]
    
    try:
        search_paragraphs(json_path, keywords)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
