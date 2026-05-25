f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
with open(f, 'r', encoding='utf-8') as fp:
    lines = fp.readlines()

# Add mouse message map entries before END_MESSAGE_MAP
for i in range(len(lines)):
    if 'END_MESSAGE_MAP()' in lines[i] and i > 1300:
        mouse_entries = '\tON_WM_MOUSEMOVE()\n\tON_WM_MOUSELEAVE()\n'
        lines.insert(i, mouse_entries)
        print(f'Added mouse message map at L{i+1}')
        break

# Add hover implementation functions before ON_WM_TIMER
timer_line = None
for i, line in enumerate(lines):
    if 'ON_WM_TIMER()' in line and i > 1200:
        timer_line = i
        print(f'ON_WM_TIMER at L{i+1}')
        break

if timer_line:
    hover_impl = '''
// =========================================================
// Level 2: Hover effect & round button implementation
// =========================================================
void CWLServerTestDlg::OnMouseMove(UINT nFlags, CPoint point)
{
\tCDialog::OnMouseMove(nFlags, point);

\tCWnd* pBtn = WindowFromPoint(point);
\tif (pBtn && ::IsWindow(pBtn->GetSafeHwnd())) {
\t\tUINT nID = pBtn->GetDlgCtrlID();
\t\t// Only buttons with OnCtlColor color mapping
\t\tswitch (nID) {
\t\tcase IDC_BUTTON_REG_REG: case IDC_BUTTON_REG_RESET:
\t\tcase IDC_BUTTON_HB_START: case IDC_BUTTON_HB_STOP:
\t\tcase IDC_BUTTON_LOG_ADD: case IDC_BUTTON_LOG_STOP:
\t\tcase IDC_BUTTON_WL_UPLOAD: case IDC_BUTTON_WL_PREVIEW:
\t\tcase IDC_BUTTON_STOP_TASK: case IDC_BTN_UNSELECT_ALL:
\t\tcase IDC_BUTTON_VER_MGMT: case IDC_BUTTON_RAWPACKET:
\t\tcase IDC_Btn_TestConn:
\t\t\tif (nID != m_nHoverBtnID) {
\t\t\t\tm_nHoverBtnID = nID;
\t\t\t\tInvalidate();
\t\t\t}
\t\t\t::TrackMouseEvent(nFlags);
\t\t\treturn;
\t\t}
\t}
\tif (m_nHoverBtnID) {
\t\tm_nHoverBtnID = 0;
\t\tInvalidate();
\t}
}

void CWLServerTestDlg::OnMouseLeave()
{
\tif (m_nHoverBtnID) {
\t\tInvalidate();
\t\tm_nHoverBtnID = 0;
\t}
\tCDialog::OnMouseLeave();
}

void CWLServerTestDlg::DrawHoverRect(CDC* pDC, CRect rc, COLORREF clr, bool bHover)
{
\trc.DeflateRect(1, 1);
\tif (bHover) {
\t\tpDC->FillSolidRect(&rc, clr);
\t\tpDC->Draw3dRect(&rc, RGB(255,255,255), RGB(180,180,180));
\t} else {
\t\tpDC->FillSolidRect(&rc, clr);
\t\tpDC->Draw3dRect(&rc, clr, clr);
\t}
}

void CWLServerTestDlg::DrawRoundButton(CDC* pDC, CRect rc, COLORREF clr, CString text, bool bHover)
{
\tint r = GetRValue(clr), g = GetGValue(clr), b = GetBValue(clr);
\tCOLORREF lightClr = RGB(min(255,r+50), min(255,g+50), min(255,b+50));

\tpDC->SetBkMode(TRANSPARENT);

\tif (bHover) {
\t\tCBrush br(lightClr);
\t\tpDC->FillRect(&rc, &br);
\t\tpDC->Draw3dRect(&rc, RGB(255,255,255), RGB(200,200,200));
\t\tpDC->SetTextColor(RGB(255,255,255));
\t\tCSize sz = pDC->GetTextExtent(text);
\t\tpDC->TextOut((rc.Width()-sz.cx)/2, (rc.Height()-sz.cy)/2, text);
\t} else {
\t\tCBrush br(clr);
\t\tpDC->FillRect(&rc, &br);
\t\tpDC->Draw3dRect(&rc, clr, clr);
\t\tpDC->SetTextColor(RGB(255,255,255));
\t\tCSize sz = pDC->GetTextExtent(text);
\t\tpDC->TextOut((rc.Width()-sz.cx)/2, (rc.Height()-sz.cy)/2, text);
\t}
}

void CWLServerTestDlg::DrawStatsCard(CDC* pDC, CRect rc, CString title, COLORREF bgColor)
{
\tCRect rcTitle(rc);
\trcTitle.bottom = rcTitle.top + 16;
\tpDC->FillSolidRect(&rcTitle, RGB(70,130,180));
\tpDC->SetTextColor(RGB(255,255,255));
\tpDC->SetBkMode(TRANSPARENT);
\tpDC->DrawText(title, &rcTitle, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
\trc.top += 18;
\tpDC->FillSolidRect(&rc, bgColor);
\tpDC->Draw3dRect(&rc, RGB(123,176,208), RGB(123,176,208));
}
'''
    lines.insert(timer_line, hover_impl)
    print(f'Inserted hover impl at L{timer_line+1}')

with open(f, 'w', encoding='utf-8') as fp:
    fp.writelines(lines)
print('SAVED')
