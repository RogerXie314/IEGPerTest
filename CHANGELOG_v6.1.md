# CHANGELOG V6.1 (2026-05-25)

## 修复

- **攻击报文多stream加载问题**：修复MS17-010和MS20-796攻击类型只能加载部分报文的问题。原始实现将多个streams合并成一个数据包，导致只有部分报文被发送。现在：
  - 左侧列表仍显示3个攻击类型（MS08-067、MS17-010、MS20-796）
  - 勾选并开始发送后，右侧窗口正确显示所有streams：
    - MS08-067：显示1个stream
    - MS17-010：显示2个streams（MS17-010 #1, MS17-010 #2）
    - MS20-796：显示2个streams（MS20-796 #1, MS20-796 #2）
  - 确保攻击报文能完整发送，触发正确的告警检测

- **RawPacketDlg编码问题**：修复GBK编码导致的编译错误，将中文字符替换为英文：
  - 对话框标题：攻击报文发送 → Attack Packet Sender
  - 列标题：名称、源地址、目的地址、协议、大小、信息
  - 速度模式：PPS、间隔(ms)、最快 → PPS、Interval(ms)、Fastest
  - 发送模式：连续、突发 → Continuous、Burst

## 技术实现

- **修改`BuiltinPacket`结构**：添加`streamsData`、`streamsRules`、`streamsFlags`支持多个streams
- **修改`LoadBuiltinPackets()`函数**：保存所有streams信息，保持向后兼容
- **修改`OnStart()`函数**：为每个选中的攻击类型添加所有streams到发送列表
- **修复`GetItemText`参数错误**：正确使用MFC API

## 版本

- 主版本号 6.0 → 6.1
- 构建时间：2026-05-25
- 构建配置：Release/x64
- 包含文件：WLServerTest.exe, WLNetComm.dll, RawPacketEngine.dll, WLServerTest.ini

## 测试验证

1. 运行WLServerTest，打开攻击报文对话框
2. 左侧列表显示3个攻击类型
3. 勾选MS17-010或MS20-796
4. 点击开始发送
5. 右侧窗口显示正确的stream数量（MS17-010显示2个，MS20-796显示2个）
6. 所有streams都能正确发送，确保攻击检测完整性