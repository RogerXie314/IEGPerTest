f_h = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.h'
f_cpp = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'

# ============== .h changes ==============
with open(f_h, 'r', encoding='utf-8') as fp:
    content_h = fp.read()

if 'Phase 3: New methods' in content_h:
    level2_decl = '''
    // Level 2: Hover effect for colored buttons
    int             m_nHoverBtnID;
    bool            m_bMouseInDlg;
    afx_msg void    OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void    OnMouseLeave();
'''
    content_h = content_h.replace(
        '    // Phase 3: New methods',
        level2_decl + '    // Phase 3: New methods'
    )
    print('OK: .h - Level 2 declarations')

with open(f_h, 'w', encoding='utf-8') as fp:
    fp.write(content_h)

# ============== .cpp changes ==============
with open(f_cpp, 'r', encoding='utf-8') as fp:
    lines_cpp = fp.readlines()

# Change 1: Constructor init
for i, line in enumerate(lines_cpp):
    if 'm_hBrushThreat(NULL)' in line:
        lines_cpp[i] = line.replace(
            'm_hBrushThreat(NULL), m_nLinuxHbIntervalMs(30000)',
            'm_hBrushThreat(NULL), m_nHoverBtnID(0), m_bMouseInDlg(false), m_nLinuxHbIntervalMs(30000)'
        )
        print(f'OK: .cpp L{i+1} - Constructor init')
        break

# Change 2: OnEraseBkgnd return FALSE
for i, line in enumerate(lines_cpp):
    if 'BOOL CWLServerTestDlg::OnEraseBkgnd' in line:
        for j in range(i, min(len(lines_cpp), i+60)):
            if 'return TRUE;' in lines_cpp[j]:
                lines_cpp[j] = lines_cpp[j].replace('return TRUE;', 'return FALSE;')
                print(f'OK: .cpp L{j+1} - OnEraseBkgnd return FALSE')
                break
        break

# Change 3: Message map
for i, line in enumerate(lines_cpp):
    if 'ON_WM_TIMER()' in line and i > 1400:
        mouse_entry = '\tON_WM_MOUSEMOVE()\n\tON_WM_MOUSELEAVE()\n\n'
        lines_cpp.insert(i, mouse_entry)
        print(f'OK: .cpp L{i+1} - Mouse message map')
        break

# Change 4: Hover functions at end
hover_funcs = '''
// Level 2: Hover effect for colored buttons
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
lines_cpp.append(hover_funcs)
print('OK: Hover functions appended')

with open(f_cpp, 'w', encoding='utf-8') as fp:
    fp.writelines(lines_cpp)

print('\\n=== Verification ===')
with open(f_cpp, 'r', encoding='utf-8') as fp:
    cpp_content = fp.read()
print(f'OnMouseMove count: {cpp_content.count("void CWLServerTestDlg::OnMouseMove")}')
print(f'OnMouseLeave count: {cpp_content.count("void CWLServerTestDlg::OnMouseLeave")}')
print('ALL DONE')
