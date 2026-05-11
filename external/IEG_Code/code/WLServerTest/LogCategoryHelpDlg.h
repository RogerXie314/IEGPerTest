// LogCategoryHelpDlg.h : 日志分类说明对话框
#pragma once

class CLogCategoryHelpDlg : public CDialog
{
    DECLARE_DYNAMIC(CLogCategoryHelpDlg)

public:
    CLogCategoryHelpDlg(CWnd* pParent = NULL);
    virtual ~CLogCategoryHelpDlg();

    enum { IDD = 200 };  // IDD_LOG_HELP

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

private:
    CListCtrl   m_listHelp;
    void        PopulateList();
};
