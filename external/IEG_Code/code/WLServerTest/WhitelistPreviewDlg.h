// WhitelistPreviewDlg.h : 白名单文件预览对话框
#pragma once

class CWhitelistPreviewDlg : public CDialog
{
    DECLARE_DYNAMIC(CWhitelistPreviewDlg)

public:
    CWhitelistPreviewDlg(const CString& strFilePath, CWnd* pParent = NULL);
    virtual ~CWhitelistPreviewDlg();

    enum { IDD = 201 };  // IDD_WL_PREVIEW

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

private:
    CString     m_strFilePath;
    CListCtrl   m_listPreview;

    void        LoadWlFile();
};
