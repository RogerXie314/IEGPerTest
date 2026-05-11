# V5.5 更新记录 (WLServerTest)

发布日期：2026-05-11

## 🐞 缺陷修复

### 1. "已注册"计数器多线程丢更新（概率 bug）
- **现象**：注册 500 客户端，"注册统计-成功"显示 500，但"已注册: N"显示 14、200 等不到 500 的数字。
- **根因**：`g_nTotalRegistered_ClientCount++` 在多个注册线程中并发执行，非原子递增导致丢失更新。
- **修复**：[WLServerTestDlg.cpp:4715](external/IEG_Code/code/WLServerTest/WLServerTestDlg.cpp#L4715)、[WLServerTestDlg.cpp:5695](external/IEG_Code/code/WLServerTest/WLServerTestDlg.cpp#L5695) 改用 `::InterlockedIncrement((LONG*)&g_nTotalRegistered_ClientCount)`。

## 🎨 UI / 布局

### 2. "任务与状态"卡片化（2×2 网格）
- 原"任务与状态"单一大分组替换为 4 张子卡片：
  - **注册统计**（465,20,172,58）：目标 / 成功 / 失败 / 已注册
  - **心跳统计**（643,20,172,58）：在线/总数 / 策略收到 / 心跳回包
  - **日志统计**（465,82,172,40）：成功 / 失败
  - **白名单统计**（643,82,172,40）：成功 / 失败
- 所有 IDC 控件 ID 保持不变，DDX 绑定不受影响。

### 3. 三列布局重排
- 左列分组框宽度 295 → 270；`IDC_STATIC_RegisteredClientCount` 宽度 188 → 158
- 中列整体左移 25 DLU（87 个控件 x -= 25）
- 右列左移 25 DLU + 列宽 +25（4 个右列大控件 `IDC_LIST_HEARTBEAT`、`IDC_EDIT_LOG_OUTPUT`、状态卡组、心跳面板组）
- 净效果：左列收窄、中列紧凑、右列任务面板/日志输出可视区域 +50 DLU。

## 版本号

- `FILEVERSION`、`PRODUCTVERSION`：`5,5,0,0`
- 主窗口标题：`WLServerTest V5.5 - 注册/心跳/日志/白名单 压测工具`

## 构建产物

- `artifacts/WLServerTestPublish/WLServerTest.exe`（x64 Release，3,584,000 字节）
- `artifacts/WLServerTestPublish/WLNetComm.dll`（x64 必备）
- `artifacts/WLServerTestPublish/RawPacketEngine.dll`
- `artifacts/WLServerTestPublish/WLServerTest.ini`
