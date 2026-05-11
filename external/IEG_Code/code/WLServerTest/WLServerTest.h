
// WLServerTest.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once
#define WL_SOLIDIFY_UPDATE			10		//更新完成
#define WL_SOLIDIFY_UPLOAD			11		//上传中
#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号


// CWLServerTestApp:
// 有关此类的实现，请参阅 WLServerTest.cpp
//

class CWLServerTestApp : public CWinAppEx
{
public:
	CWLServerTestApp();

// 重写
	public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CWLServerTestApp theApp;