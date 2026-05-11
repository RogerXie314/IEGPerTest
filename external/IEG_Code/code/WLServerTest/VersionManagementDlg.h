// VersionManagementDlg.h : 客户端版本管理对话框
#pragma once

class CVersionManagementDlg : public CDialog
{
    DECLARE_DYNAMIC(CVersionManagementDlg)

public:
    // bLinux: TRUE=显示Linux版本, FALSE=显示Windows版本
    CVersionManagementDlg(BOOL bLinux, CWnd* pParent = NULL);
    virtual ~CVersionManagementDlg();

    enum { IDD = 203 };  // IDD_VER_MGMT

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

    afx_msg void OnBnClickedOsWin();
    afx_msg void OnBnClickedOsLinux();
    afx_msg void OnBnClickedAdd();
    afx_msg void OnBnClickedDelete();
    afx_msg void OnBnClickedReset();
    afx_msg void OnBnClickedSave();

private:
    BOOL        m_bLinux;

    CButton     m_radioWin;
    CButton     m_radioLinux;
    CListBox    m_listVersions;
    CEdit       m_editNew;

    static const LPCTSTR s_szDefaultWin;
    static const LPCTSTR s_szDefaultLinux;

    void        LoadVersionList();
    CString     GetCurrentListAsString();
};
