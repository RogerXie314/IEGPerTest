# WLServerTest V6.1 发布说明

## 版本信息
- **版本号**: V6.1
- **发布日期**: 2026-05-25
- **构建配置**: Release/x64
- **Git标签**: v6.1
- **提交哈希**: `fb74a5d`

## 主要修复

### 🐛 攻击报文多stream加载问题修复
**问题描述**: MS17-010和MS20-796攻击类型只能加载部分报文，导致攻击检测不完整。

**根本原因**: 原始实现将多个streams合并成一个数据包，导致只有部分报文被发送。

**修复内容**:
1. **左侧列表**: 仍显示3个攻击类型（MS08-067、MS17-010、MS20-796）
2. **右侧发送**: 正确显示所有streams：
   - MS08-067：显示1个stream
   - MS17-010：显示2个streams（MS17-010 #1, MS17-010 #2）
   - MS20-796：显示2个streams（MS20-796 #1, MS20-796 #2）
3. **技术实现**:
   - 修改`BuiltinPacket`结构支持多个streams
   - 修改`LoadBuiltinPackets()`函数保存所有streams信息
   - 修改`OnStart()`函数为每个选中的攻击类型添加所有streams

### 🔧 编码问题修复
- 修复GBK编码导致的编译错误
- 将中文字符替换为英文：
  - 对话框标题：Attack Packet Sender
  - 列标题：Name, Src Addr, Dest Addr, Protocol, Size(B), Info
  - 速度模式：PPS, Interval(ms), Fastest
  - 发送模式：Continuous, Burst

## 包含文件
```
WLServerTest_v6.1.zip
├── WLServerTest.exe          # 主程序（已修复攻击报文问题）
├── WLNetComm.dll             # 网络通信库
├── RawPacketEngine.dll       # 攻击报文引擎
├── WLServerTest.ini          # 配置文件
└── README_v6.1.txt           # 本说明文件
```

## 系统要求
- **操作系统**: Windows 7/8/10/11 (64位)
- **运行库**: 
  - Microsoft Visual C++ Redistributable 2015-2022
  - Npcap (用于攻击报文功能，可从 https://npcap.com 下载)
- **网络**: 需要网络适配器支持原始报文发送

## 安装使用
1. 解压`WLServerTest_v6.1.zip`到任意目录
2. 如需攻击报文功能，请先安装Npcap
3. 双击`WLServerTest.exe`运行
4. 点击"攻击报文"按钮测试修复后的功能

## 测试验证
1. 运行WLServerTest，打开攻击报文对话框
2. 左侧列表显示3个攻击类型
3. 勾选MS17-010或MS20-796
4. 点击开始发送
5. 右侧窗口显示正确的stream数量
6. 所有streams都能正确发送，确保攻击检测完整性

## 版本历史
- **V6.1** (当前): 修复攻击报文多stream加载问题
- **V6.0**: 注册doPost弹窗移除 + RawPacketEngine.dll入库
- **V5.9**: OPT调度大修 + 日志路由对齐
- **V5.8**: 攻击报文UI复刻
- **V5.7**: 短连接漏发修复
- **V5.6**: IEG/EDR联动
- **V5.5**: 计数器原子化 + 卡片化

## 技术支持
- GitHub仓库: https://github.com/RogerXie314/IEGPerTest
- 问题反馈: 请提交GitHub Issue
- 版本标签: v6.1

---
**注意**: 攻击报文功能仅供合法安全测试使用，请遵守相关法律法规。