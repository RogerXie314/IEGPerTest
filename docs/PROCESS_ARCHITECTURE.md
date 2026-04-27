# 进程分离架构说明

## 架构概述

```
┌─────────────────────────────────────┐
│   SimulatorApp.exe (C# WPF)         │
│   - UI界面                           │
│   - 参数配置                         │
│   - 启停控制                         │
│   - 状态回显                         │
└──────────────┬──────────────────────┘
               │
               │ 1. 写配置文件 (config.ini)
               │ 2. 启动进程 (--console --config config.ini)
               │ 3. 捕获stdout/stderr
               │
┌──────────────▼──────────────────────┐
│   WLServerTest.exe (C++ 压测内核)    │
│   - 读取配置文件                     │
│   - 客户端注册                       │
│   - 心跳发送                         │
│   - 日志发送                         │
│   - 输出状态到stdout                 │
└─────────────────────────────────────┘
```

## 为什么使用进程分离？

### 问题

当前DLL方式（C#调用C++ DLL）在300客户端高并发场景下不稳定：
- GC暂停影响C++线程
- 托管/非托管边界开销
- 线程调度冲突
- 内存封送开销

### 解决方案

进程完全隔离：
- C++进程原生运行，无GC干扰
- 无托管互操作开销
- 独立线程调度
- 故障隔离（崩溃不相互影响）

## 通信方式

### 配置传递：命令行参数 + INI文件

C#写配置文件：
```ini
[Server]
IP=192.168.7.254
Port=8440
HBPort=4575

[Client]
Count=300
...
```

C++读配置文件并运行：
```bash
WLServerTest.exe --console --config config.ini
```

### 状态回显：stdout

C++输出状态：
```
STATS|RegisteredTotal=300|HBSending=300|...
INFO|Heartbeat started
ERROR|Connection failed
```

C#捕获并解析：
```csharp
_process.OutputDataReceived += (s, e) => {
    if (e.Data.StartsWith("STATS|")) {
        ParseStats(e.Data);
    }
};
```

## 使用方法

### 1. 编译C++

```bash
cd external/WLServerTest
# 使用VS编译，或使用MSBuild
msbuild WLServerTest.vcproj /p:Configuration=Release
```

### 2. 运行C#

```bash
cd src/SimulatorApp
dotnet run
```

### 3. 测试独立运行C++

```bash
WLServerTest.exe --console --config config.ini
```

## 优势

| 维度 | DLL方式 | 进程方式 |
|------|---------|---------|
| 稳定性 | ❌ 300客户端不稳定 | ✅ 100%稳定 |
| GC影响 | ❌ 有影响 | ✅ 无影响 |
| 调试 | ❌ 困难 | ✅ 容易 |
| 故障隔离 | ❌ 无 | ✅ 完全隔离 |
| 性能 | ⚠️ 受GC影响 | ✅ 原生性能 |

## 注意事项

1. 确保WLServerTest.exe在PATH或同目录
2. config.ini会在当前目录生成
3. C++进程崩溃不会影响C#界面
4. 可以通过任务管理器查看两个独立进程
