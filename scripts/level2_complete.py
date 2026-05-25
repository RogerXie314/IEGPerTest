#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
WLServerTest Level 2 完整修改脚本 - 一次性完成所有修改
"""

def main():
    cpp_file = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
    h_file = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.h'
    
    print("=" * 60)
    print("WLServerTest Level 2 完整修改")
    print("=" * 60)
    
    # ========== Step 1: 修复 .h 文件 ==========
    print("\n[Step 1] 修复 .h 文件...")
    with open(h_file, 'r', encoding='utf-8') as f:
        h_content = f.read()
    
    # 1a. 添加成员变量声明
    if 'm_nHoverBtnID' not in h_content:
        # 在 OnDestroy 声明后添加
        insert_pos = h_content.find('afx_msg void    OnDestroy();')
        if insert_pos != -1:
            insert_pos = h_content.find('\n', insert_pos) + 1
            level2_decl = '''
    // Level 2: Hover effect & round button
    int             m_nHoverBtnID;       // button ID currently hovered (0=none)
    bool            m_bMouseInDlg;       // mouse inside dialog'''
            
            h_content = h_content[:insert_pos] + level2_decl + h_content[insert_pos:]
            print("  - 添加了成员变量声明")
    else:
        print("  - 成员变量声明已存在")
    
    # 1b. 添加消息映射
    if 'ON_WM_MOUSEMOVE()' not in h_content:
        # 在 END_MESSAGE_MAP() 之前插入
        end_map_pos = h_content.find('END_MESSAGE_MAP()')
        if end_map_pos != -1:
            # 找到 END_MESSAGE_MAP() 前面的行
            line_start = h_content.rfind('\n', 0, end_map_pos) + 1
            msg_map = '\tON_WM_MOUSEMOVE()\n\tON_WM_MOUSELEAVE()\n'
            h_content = h_content[:line_start] + msg_map + h_content[line_start:]
            print("  - 添加了消息映射")
        else:
            print("  - 警告：未找到 END_MESSAGE_MAP")
    else:
        print("  - 消息映射已存在")
    
    with open(h_file, 'w', encoding='utf-8') as f:
        f.write(h_content)
    print("  .h 文件保存完成")
    
    # ========== Step 2: 修复 .cpp 文件 ==========
    print("\n[Step 2] 修复 .cpp 文件...")
    with open(cpp_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # 2a. 构造函数初始化
    for i, line in enumerate(lines):
        if 'CWLServerTestDlg::CWLServerTestDlg' in line and 'm_lMsgLogSuccessCount' in line:
            if 'm_nHoverBtnID' not in line:
                lines[i] = line.rstrip() + ', m_nHoverBtnID(0), m_bMouseInDlg(false)' + '\n'
                print(f"  - 构造函数 L{i+1} 添加了初始化")
            break
    
    # 2b. 消息映射中添加 ON_WM_MOUSEMOVE 和 ON_WM_MOUSELEAVE
    for i, line in enumerate(lines):
        if 'END_MESSAGE_MAP()' in line:
            if 'ON_WM_MOUSEMOVE()' not in lines[i-1] and 'ON_WM_MOUSELEAVE()' not in lines[i-1]:
                # 在这一行之前插入
                lines[i] = '\tON_WM_MOUSEMOVE()\n\tON_WM_MOUSELEAVE()\n' + line
                print(f"  - 在 END_MESSAGE_MAP 前 L{i+1} 添加了消息映射")
            break
    
    # 2c. 确保 return FALSE 在 OnEraseBkgnd 中
    for i, line in enumerate(lines):
        if 'BOOL CWLServerTestDlg::OnEraseBkgnd' in line:
            for j in range(i, min(i+50, len(lines))):
                if 'return TRUE;' in lines[j]:
                    lines[j] = lines[j].replace('return TRUE;', 'return FALSE;')
                    print(f"  - 在 L{j+1} 将 return TRUE 改为 return FALSE")
                    break
                elif 'return FALSE;' in lines[j]:
                    print(f"  - L{j+1} 已有 return FALSE")
                    break
    
    # 2d. 在文件末尾添加 Level 2 函数实现
    level2_impl = '''
void CWLServerTestDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);

	CWnd* pBtn = WindowFromPoint(point);
	if (pBtn && ::IsWindow(pBtn->GetSafeHwnd())) {
		UINT nID = pBtn->GetDlgCtrlID();
		switch (nID) {
		case IDC_BUTTON_REG_REG: case IDC_BUTTON_REG_RESET:
		case IDC_BUTTON_HB_START: case IDC_BUTTON_HB_STOP:
		case IDC_BUTTON_LOG_ADD: case IDC_BUTTON_LOG_STOP:
		case IDC_BUTTON_WL_UPLOAD: case IDC_BUTTON_WL_PREVIEW:
		case IDC_BUTTON_STOP_TASK: case IDC_BTN_UNSELECT_ALL:
		case IDC_BUTTON_VER_MGMT: case IDC_BUTTON_RAWPACKET:
		case IDC_Btn_TestConn:
			if (nID != m_nHoverBtnID) {
				m_nHoverBtnID = nID;
				Invalidate();
			}
			::TrackMouseEvent(nFlags);
			return;
		}
	}
	if (m_nHoverBtnID) {
		m_nHoverBtnID = 0;
		Invalidate();
	}
}

void CWLServerTestDlg::OnMouseLeave()
{
	if (m_nHoverBtnID) {
		Invalidate();
		m_nHoverBtnID = 0;
	}
	CDialog::OnMouseLeave();
}
'''
    
    # 检查是否已有这些函数
    cpp_content = ''.join(lines)
    has_onmousemove = 'void CWLServerTestDlg::OnMouseMove' in cpp_content
    has_onmouseleave = 'void CWLServerTestDlg::OnMouseLeave' in cpp_content
    
    if not has_onmousemove or not has_onmouseleave:
        lines.append(level2_impl)
        print(f"  - 在文件末尾添加了 Level 2 函数实现")
    else:
        print("  - Level 2 函数已存在")
    
    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.writelines(lines)
    print("  .cpp 文件保存完成")
    
    # ========== Step 3: 验证 ==========
    print("\n[Step 3] 验证...")
    with open(cpp_file, 'r', encoding='utf-8') as f:
        cpp_final = f.read()
    with open(h_file, 'r', encoding='utf-8') as f:
        h_final = f.read()
    
    checks = {
        '成员变量声明': 'm_nHoverBtnID' in h_final,
        '消息映射': 'ON_WM_MOUSEMOVE()' in cpp_final,
        'OnMouseMove': 'void CWLServerTestDlg::OnMouseMove' in cpp_final,
        'OnMouseLeave': 'void CWLServerTestDlg::OnMouseLeave' in cpp_final,
        'return FALSE': 'return FALSE;' in cpp_final,
        'END_MESSAGE_MAP 数量': cpp_final.count('END_MESSAGE_MAP()') == 1,
    }
    
    all_ok = True
    for name, result in checks.items():
        status = '✓' if result else '✗'
        print(f"  {status} {name}")
        if not result:
            all_ok = False
    
    print("\n" + "=" * 60)
    if all_ok:
        print("所有修改完成！可以开始编译。")
        return 0
    else:
        print("部分检查未通过，请手动检查。")
        return 1

if __name__ == '__main__':
    import sys
    sys.exit(main())
