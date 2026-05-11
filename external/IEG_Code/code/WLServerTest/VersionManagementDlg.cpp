// VersionManagementDlg.cpp
#include "stdafx.h"
#include "WLServerTest.h"
#include "VersionManagementDlg.h"
#include "ProfileConfig.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CVersionManagementDlg, CDialog)

const LPCTSTR CVersionManagementDlg::s_szDefaultWin =
    _T("V300R011C01B090|V300R006C05B270|V300R006C02B090");
const LPCTSTR CVersionManagementDlg::s_szDefaultLinux =
    _T("V300R011C11B060-Redhat7.x-x64");

CVersionManagementDlg::CVersionManagementDlg(BOOL bLinux, CWnd* pParent)
    : CDialog(CVersionManagementDlg::IDD, pParent)
    , m_bLinux(bLinux)
{
}

CVersionManagementDlg::~CVersionManagementDlg()
{
}

void CVersionManagementDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_VER_RADIO_WIN,   m_radioWin);
    DDX_Control(pDX, IDC_VER_RADIO_LINUX, m_radioLinux);
    DDX_Control(pDX, IDC_VER_LIST,        m_listVersions);
    DDX_Control(pDX, IDC_VER_EDIT_NEW,    m_editNew);
}

BEGIN_MESSAGE_MAP(CVersionManagementDlg, CDialog)
    ON_BN_CLICKED(IDC_VER_RADIO_WIN,   &CVersionManagementDlg::OnBnClickedOsWin)
    ON_BN_CLICKED(IDC_VER_RADIO_LINUX, &CVersionManagementDlg::OnBnClickedOsLinux)
    ON_BN_CLICKED(IDC_VER_BTN_ADD,     &CVersionManagementDlg::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_VER_BTN_DELETE,  &CVersionManagementDlg::OnBnClickedDelete)
    ON_BN_CLICKED(IDC_VER_BTN_RESET,   &CVersionManagementDlg::OnBnClickedReset)
    ON_BN_CLICKED(IDC_VER_BTN_SAVE,    &CVersionManagementDlg::OnBnClickedSave)
END_MESSAGE_MAP()

BOOL CVersionManagementDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    if (m_bLinux)
    {
        m_radioLinux.SetCheck(BST_CHECKED);
        m_radioWin.SetCheck(BST_UNCHECKED);
    }
    else
    {
        m_radioWin.SetCheck(BST_CHECKED);
        m_radioLinux.SetCheck(BST_UNCHECKED);
    }

    LoadVersionList();
    return TRUE;
}

void CVersionManagementDlg::LoadVersionList()
{
    m_listVersions.ResetContent();

    CString strList;
    if (!m_bLinux)
    {
        strList = CProfileConfig::GetProfileConfigInstance()->ReadWindowsVersionList_FromIni();
        if (strList.IsEmpty()) strList = s_szDefaultWin;
    }
    else
    {
        strList = CProfileConfig::GetProfileConfigInstance()->ReadLinuxVersionList_FromIni();
        if (strList.IsEmpty()) strList = s_szDefaultLinux;
    }

    int nStart = 0;
    CString strToken = strList.Tokenize(_T("|"), nStart);
    while (!strToken.IsEmpty())
    {
        strToken.Trim();
        if (!strToken.IsEmpty())
            m_listVersions.AddString(strToken);
        strToken = strList.Tokenize(_T("|"), nStart);
    }
}

CString CVersionManagementDlg::GetCurrentListAsString()
{
    CString strResult;
    int nCount = m_listVersions.GetCount();
    for (int i = 0; i < nCount; ++i)
    {
        CString strItem;
        m_listVersions.GetText(i, strItem);
        if (!strResult.IsEmpty()) strResult += _T("|");
        strResult += strItem;
    }
    return strResult;
}

void CVersionManagementDlg::OnBnClickedOsWin()
{
    // v19: 自动保存已在增删时完成，无需切换时提示
    m_bLinux = FALSE;
    LoadVersionList();
}

void CVersionManagementDlg::OnBnClickedOsLinux()
{
    // v19: 自动保存已在增删时完成，无需切换时提示
    m_bLinux = TRUE;
    LoadVersionList();
}

void CVersionManagementDlg::OnBnClickedAdd()
{
    CString strNew;
    m_editNew.GetWindowText(strNew);
    strNew.Trim();
    if (strNew.IsEmpty())
    {
        AfxMessageBox(_T("请输入版本号"));
        return;
    }
    // 防止重复
    if (m_listVersions.FindStringExact(-1, strNew) != LB_ERR)
    {
        AfxMessageBox(_T("该版本已存在"));
        return;
    }
    m_listVersions.AddString(strNew);
    m_editNew.SetWindowText(_T(""));
    // v19: 自动保存，不要求用户手动点保存
    {
        CString strList = GetCurrentListAsString();
        if (!m_bLinux)
            CProfileConfig::GetProfileConfigInstance()->WriteWindowsVersionList_ToIni(strList);
        else
            CProfileConfig::GetProfileConfigInstance()->WriteLinuxVersionList_ToIni(strList);
    }
}

void CVersionManagementDlg::OnBnClickedDelete()
{
    int nSel = m_listVersions.GetCurSel();
    if (nSel == LB_ERR)
    {
        AfxMessageBox(_T("请先选择要删除的版本"));
        return;
    }
    // v17: confirm before delete (matches C# VersionManagementViewModel)
    CString strItem;
    m_listVersions.GetText(nSel, strItem);
    CString strMsg;
    strMsg.Format(_T("确定要删除版本 \"%s\" 吗？"), (LPCTSTR)strItem);
    if (AfxMessageBox(strMsg, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    m_listVersions.DeleteString(nSel);
    int nCount = m_listVersions.GetCount();
    if (nCount > 0)
        m_listVersions.SetCurSel(min(nSel, nCount - 1));
    // v19: 自动保存
    {
        CString strList = GetCurrentListAsString();
        if (!m_bLinux)
            CProfileConfig::GetProfileConfigInstance()->WriteWindowsVersionList_ToIni(strList);
        else
            CProfileConfig::GetProfileConfigInstance()->WriteLinuxVersionList_ToIni(strList);
    }
}

void CVersionManagementDlg::OnBnClickedReset()
{
    if (AfxMessageBox(_T("确定要恢复默认版本列表吗？"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    m_listVersions.ResetContent();
    CString strDefault = m_bLinux ? s_szDefaultLinux : s_szDefaultWin;
    int nStart = 0;
    CString strToken = strDefault.Tokenize(_T("|"), nStart);
    while (!strToken.IsEmpty())
    {
        m_listVersions.AddString(strToken);
        strToken = strDefault.Tokenize(_T("|"), nStart);
    }
    // v17: auto-save after reset (matches C# ClientVersionConfig.ResetToDefault())
    CString strList = GetCurrentListAsString();
    if (!m_bLinux)
        CProfileConfig::GetProfileConfigInstance()->WriteWindowsVersionList_ToIni(strList);
    else
        CProfileConfig::GetProfileConfigInstance()->WriteLinuxVersionList_ToIni(strList);
    AfxMessageBox(_T("已恢复默认版本列表"));
}

void CVersionManagementDlg::OnBnClickedSave()
{
    CString strList = GetCurrentListAsString();
    if (!m_bLinux)
        CProfileConfig::GetProfileConfigInstance()->WriteWindowsVersionList_ToIni(strList);
    else
        CProfileConfig::GetProfileConfigInstance()->WriteLinuxVersionList_ToIni(strList);
    AfxMessageBox(_T("版本列表已保存"));
}
