// -*- coding: gbk -*-
// WhitelistPreviewDlg.cpp
#include "stdafx.h"
#include "WLServerTest.h"
#include "WhitelistPreviewDlg.h"
#include <fstream>
#include <string>
#include <vector>

IMPLEMENT_DYNAMIC(CWhitelistPreviewDlg, CDialog)

CWhitelistPreviewDlg::CWhitelistPreviewDlg(const CString& strFilePath, CWnd* pParent)
    : CDialog(CWhitelistPreviewDlg::IDD, pParent)
    , m_strFilePath(strFilePath)
{
}

CWhitelistPreviewDlg::~CWhitelistPreviewDlg()
{
}

void CWhitelistPreviewDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_WP_LIST, m_listPreview);
}

BEGIN_MESSAGE_MAP(CWhitelistPreviewDlg, CDialog)
END_MESSAGE_MAP()

BOOL CWhitelistPreviewDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 显示文件路径
    GetDlgItem(IDC_WP_FILE_PATH)->SetWindowText(m_strFilePath);

    // 扩展风格（整行选中）
    m_listPreview.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 设置列（与C# 版本对齐）
    m_listPreview.InsertColumn(0, _T("序号"),     LVCFMT_LEFT,  50);
    m_listPreview.InsertColumn(1, _T("文件名"),   LVCFMT_LEFT, 160);
    m_listPreview.InsertColumn(2, _T("文件路径"), LVCFMT_LEFT, 240);
    m_listPreview.InsertColumn(3, _T("操作"),     LVCFMT_LEFT,  50);
    m_listPreview.InsertColumn(4, _T("Hash类型"), LVCFMT_LEFT,  60);
    m_listPreview.InsertColumn(5, _T("Hash值"),   LVCFMT_LEFT, 270);

    LoadWlFile();
    return TRUE;
}

// 从 Unicode 字节数组读取 wstring（按字节对） 
static std::wstring ReadWStr(const std::vector<BYTE>& buf, size_t& pos, int byteLen)
{
    if (byteLen <= 0 || pos + byteLen > buf.size()) { pos += (byteLen > 0 ? byteLen : 0); return L""; }
    std::wstring ws;
    ws.reserve(byteLen / 2);
    for (int i = 0; i < byteLen - 1; i += 2)
    {
        WCHAR wc = (WCHAR)(buf[pos + i] | (buf[pos + i + 1] << 8));
        if (wc == L'\0') break;
        ws += wc;
    }
    pos += byteLen;
    return ws;
}

static int ReadInt32(const std::vector<BYTE>& buf, size_t& pos)
{
    if (pos + 4 > buf.size()) { pos += 4; return 0; }
    int v = (int)(buf[pos] | (buf[pos+1]<<8) | (buf[pos+2]<<16) | (buf[pos+3]<<24));
    pos += 4;
    return v;
}

static UINT16 ReadUInt16(const std::vector<BYTE>& buf, size_t& pos)
{
    if (pos + 2 > buf.size()) { pos += 2; return 0; }
    UINT16 v = (UINT16)(buf[pos] | (buf[pos+1]<<8));
    pos += 2;
    return v;
}

void CWhitelistPreviewDlg::LoadWlFile()
{
    // 以二进制方式读取整个文件
    std::ifstream ifs(CT2A(m_strFilePath, CP_ACP), std::ios::binary);
    if (!ifs.is_open())
    {
        AfxMessageBox(_T("无法打开文件"));
        return;
    }
    std::vector<BYTE> buf((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
    ifs.close();

    if (buf.size() < 2)
    {
        AfxMessageBox(_T("文件过小，无法解析"));
        return;
    }

    size_t pos = 0;
    UINT16 version = ReadUInt16(buf, pos);

    // 支持版本 V2(0xFEFE) V3(0xFEFC) V4(0xFEFB)
    // V4/V3: AddOrDel(4) + IsSystemFile(4) + ItemFrom(4) + JudgeMethod(4) + PathLen(4) + Path + HashType(4) + HashLen(4) + Hash
    // V2:    AddOrDel(4) + PathLen(4) + Path + HashType(4) + HashLen(4) + Hash
    bool bV4 = (version == 0xFEFB || version == 0xFEFC || version == 0xFEFF);
    bool bV2 = (version == 0xFEFE);

    if (!bV4 && !bV2)
    {
        // 未知版本，尝试按 V4 解析
        bV4 = true;
    }

    m_listPreview.SetRedraw(FALSE);
    int nRow = 0;
    while (pos + 8 <= buf.size())
    {
        int addOrDel = ReadInt32(buf, pos);
        if (pos > buf.size()) break;

        int isSystemFile = 0, itemFrom = 0, judgeMethod = 0;
        if (bV4)
        {
            isSystemFile = ReadInt32(buf, pos);
            itemFrom     = ReadInt32(buf, pos);
            judgeMethod  = ReadInt32(buf, pos);
        }
        if (pos > buf.size()) break;

        int pathLen = ReadInt32(buf, pos);
        if (pos > buf.size()) break;
        std::wstring fullPath = ReadWStr(buf, pos, pathLen);
        if (pos > buf.size()) break;

        int hashType = ReadInt32(buf, pos);
        if (pos > buf.size()) break;

        int hashLen = ReadInt32(buf, pos);
        if (pos > buf.size()) break;
        std::wstring fileHash = ReadWStr(buf, pos, hashLen);

        // 派生字段
        CString strAction = (addOrDel == 1) ? _T("添加") : (addOrDel == 2) ? _T("删除") : _T("未知");
        CString strHashType = (hashType == 1) ? _T("SHA1") : (hashType == 2) ? _T("MD5") : _T("未知");

        // 从路径提取文件名
        CString strFullPath(fullPath.c_str());
        CString strFileName = strFullPath;
        int nSlash = strFullPath.ReverseFind(L'\\');
        if (nSlash >= 0) strFileName = strFullPath.Mid(nSlash + 1);
        int nSlash2 = strFileName.ReverseFind(L'/');
        if (nSlash2 >= 0) strFileName = strFileName.Mid(nSlash2 + 1);

        CString strHash(fileHash.c_str());
        CString strIdx;
        strIdx.Format(_T("%d"), nRow + 1);

        int idx = m_listPreview.InsertItem(nRow, strIdx);
        m_listPreview.SetItemText(idx, 1, strFileName);
        m_listPreview.SetItemText(idx, 2, strFullPath);
        m_listPreview.SetItemText(idx, 3, strAction);
        m_listPreview.SetItemText(idx, 4, strHashType);
        m_listPreview.SetItemText(idx, 5, strHash);
        ++nRow;
    }

    m_listPreview.SetRedraw(TRUE);

    CString strCount;
    strCount.Format(_T("共 %d 条"), nRow);
    GetDlgItem(IDC_WP_COUNT)->SetWindowText(strCount);
}
