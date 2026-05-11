# V5.2 更新记录 (WLServerTest)

发布日期：2026-05-11

## 关键修复

### 1. 修复异机 500 客户端启动后约 1 分钟崩溃（c0000005）
- **根因**：开发机 `C:\Windows\System32\WLNetComm.dll` 存在，但发布目录 `artifacts/WLServerTestPublish/` 未携带该 x64 DLL；异机加载失败后，`CWLNetCommApi::instance()` 单例仍非空但函数指针全部为 NULL，注册线程批量调用 `pEnableTLSv1()` 即跳转至地址 0 崩溃。
- **修复 A（发布侧）**：把 x64 `WLNetComm.dll`（来自 `C:\Windows\System32\WLNetComm.dll`，3.23 MB）归档至仓库 `external/IEG_Code/code/bin/Release/x64/WLNetComm.dll`，并随 `artifacts/WLServerTestPublish/` 一同发布。
- **修复 B（代码侧）**：`SendInfoToServer.cpp::RegisterClientToServer` 入口加防御性判空：当 `CWLNetCommApi::instance()` 返回 NULL 或核心函数指针未解析时，写错误日志后立即返回 FALSE，避免崩溃。

## 新增功能

### 2. 心跳设置区域新增"心跳时长(分钟)"输入框
- 位置：心跳设置组内（IDC_STATIC_HB_BADGE 下方，313,142 处）
- 默认值：7200 分钟
- 控件 ID：`IDC_EDIT_HB_DURATION = 1208`
- 行为：点击「开始心跳」时取该输入框的值传入心跳总时长，<=0 时回退到 7200。
- 涉及文件：`resource.h`、`WLServerTest.rc`、`WLServerTestDlg.h`、`WLServerTestDlg.cpp`。

## 版本号

- `FILEVERSION`、`PRODUCTVERSION`：`5,2,0,0`
- 主窗口标题：`WLServerTest V5.2 - 注册/心跳/日志/白名单 压测工具`

## 构建产物

- `artifacts/WLServerTestPublish/WLServerTest.exe`（x64 Release）
- `artifacts/WLServerTestPublish/WLNetComm.dll`（x64，必备）
- `artifacts/WLServerTestPublish/RawPacketEngine.dll`
- `artifacts/WLServerTestPublish/WLServerTest.ini`
