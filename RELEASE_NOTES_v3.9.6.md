# IEGPerTest Simulator v3.9.6

发布日期：2026-04-26（补丁更新：2026-04-27）

## 🔧 核心架构重构：子进程引擎

v3.9.6 将心跳/日志发送从 DLL 方式迁移到独立 C++ 子进程（NativeRunner.exe），解决 v3.9.5 中 500 客户端心跳不稳定、掉线等问题。

### 架构变化
```
C# WPF (SimulatorApp.exe)
  ├─ 注册客户端
  ├─ 白名单上传、HTTPS 日志等
  └─ 启动 C++ 子进程 (NativeRunner.exe)
       ├─ 读取 config.ini（客户端信息）
       ├─ 心跳发送（TCP 长连接，多线程）
       └─ 日志发送（威胁检测，复用心跳连接）
```

### 进程间通信
- **stdout**：子进程向 C# 汇报状态（`INFO|`、`STATS|`、`REREGISTER|`、`POLICY|`、`ERROR|`）
- **stdin**：C# 控制子进程（`quit`、`STARTLOG`、`STOPLOG`、`DEVICEID`）

---

## 🐛 Bug 修复

### [关键] stdout 竞争条件导致在线数波动（约 490/500）
- **问题**：500 个心跳线程同时向 stdout 写 `REREGISTER` 消息，字节级交叉导致 C# 收到格式错误的行（如 `460REREGISTER|5|xrg2-5`），`int.Parse` 抛异常，重注册事件丢失，客户端永久卡在无注册状态
- **修复**：所有 stdout 输出统一通过全局 `CRITICAL_SECTION g_csStdout` + `PrintLine()` 串行化

### [关键] NativeRunner 运行 60 分钟后自动停止
- **问题**：`MainViewModel.cs` 中 `hbTotalMinutes` 硬编码为 `60`，写入 `config.ini` 后 NativeRunner 在 `Sleep(3600×1000)` 超时后自动退出，UI 任务面板仍显示"运行中"，实际心跳已中断
- **修复**：`hbTotalMinutes` 改为 `0`（无限运行），停止逻辑由用户手动操作触发，ProcessEngine 通过 stdin 发送 `quit` 命令正常关闭子进程

### [关键] 日志线程 stale socket（已回滚错误修复）
- **问题**：日志线程原本应复用心跳 socket（同一 TCP 会话），错误修复版本改为自建连接，导致 PT 协议会话不一致
- **修复**：回滚至原始 `NativeEngine.cpp` 模式，日志线程读取 `g_sock[idx]` 复用心跳连接

### [修复] 发布包缺少 NativeRunner.exe
- **问题**：`publish_simulatorapp.ps1` 仅打包三个 DLL，未包含 C++ 子进程 `NativeRunner.exe`，导致解压后运行 SimulatorApp 时子进程无法启动，心跳/日志发送功能完全失效
- **修复**：发布脚本补充编译 `src/NativeRunner` 并将产物复制到输出目录，同时在必需文件校验列表中加入 `NativeRunner.exe`，缺失时终止打包

---

## ✨ 新增功能

### 心跳/日志独立控制
- 心跳和日志可分别启动/停止，互不影响
- 支持先启动心跳，稳定后再启动日志发送

### 注册重置按钮
- 注册设置区域新增"重置"按钮（红色）
- 一键清空已注册客户端列表，恢复默认注册参数（前缀 `Client-`、起始编号 `1`、IP `192.168.0.1`、数量 `5`）

### 统计数据优化
- `LogSendOk` / `LogSendFail` 计数器从 `int` 改为 `long`，防止长时间运行溢出
- `STARTLOG` 命令时自动重置日志统计计数器

---

## 📦 部署说明

### 系统要求
- Windows x64 操作系统
- 攻击报文发送功能需要安装 [Npcap](https://npcap.com/) 驱动（可选）

### 安装步骤
1. 下载 `SimulatorApp-v3.9.6-win-x64.zip`
2. 解压到任意目录
3. 双击 `SimulatorApp.exe` 运行

**注意**：应用是自包含 EXE，无需安装 .NET 运行时。

---

## 📋 文件说明

| 文件 | 说明 |
|------|------|
| `SimulatorApp.exe` | 主程序（WPF，自包含） |
| `NativeRunner.exe` | C++ 子进程，负责心跳/日志发送 |
| `NativeEngine.dll` | 原生心跳引擎（备用） |
| `NativeSender.dll` | 原生发包库 |
| `RawPacketEngine.dll` | 攻击报文引擎 |
