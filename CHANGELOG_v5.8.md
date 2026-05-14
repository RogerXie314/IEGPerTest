# V5.8 更新记录 (WLServerTest)

发布日期：2026-05-14

## 🎨 攻击报文发送 窗口 UI 复刻

- 重写 `IDD_RAWPACKET` 对话框模板（600 x 360 DLU，Microsoft YaHei UI 9pt），1:1 还原 C# SimulatorApp 攻击报文发送窗口的布局/控件大小/位置：
  - 左侧 5 个编号分组：`1. 选择攻击报文` / `2. 设置目标` / `3. 源IP自动递增（可选）` / `4. 发包配置` / `5. 执行`
  - 右侧报文列表 `IDC_LIST_RP_RIGHT`（#/名称/源地址/目的地址/协议/长度(B)/信息 7 列）+ 导入/编辑/删除/清空 4 个工具按钮
  - 右下 2x3 实时统计网格（BPS / PPS / 平均BPS / 平均PPS / 总数 / 失败）+ 状态行

## 🐛 缺陷修复

### 网卡组合框显示原始 pcap 名称（关键）
- **现象**：攻击报文发送窗口的网卡下拉框显示 `#0 rpcap://\Device\NPF_{A0777708-3C3E-4D13-963D-3E52A7109D44} ()`，不是友好名（C# 版早前已修复过同名 bug）
- **根因**：[RawPacketDlg.cpp:53](external/IEG_Code/code/WLServerTest/RawPacketDlg.cpp#L53) `GetFriendlyName` 抽取 GUID 时用 `Mid(gs+1, ge-gs-1)` 把花括号剥掉，但注册表键 `HKLM\SYSTEM\CurrentControlSet\Control\Network\{4D36E972-...}\{NIC_GUID}\Connection` 第二段 GUID 必须含 `{}`，导致 `RegOpenKeyEx` 永远失败，回退到原始 pcap 名分支
- **修复**：保留花括号 `s.Mid(gs, ge-gs+1)`；同时简化 `RefreshAdapterList` 显示为 `#N <友好名> (IP)`（IP 为空时不显示括号），删除遗留的 `[0]` 占位 hack
