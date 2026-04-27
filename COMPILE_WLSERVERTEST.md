# 编译 WLServerTest.exe 指南

## 概述

WLServerTest.exe 是子进程架构的核心组件，负责心跳和日志发送。

## 前提条件

- Visual Studio 2019 或更高版本（需要 C++ 工作负载）

## 使用 Visual Studio 编译

### 步骤

1. **打开解决方案**
   - 用 Visual Studio 打开：`external/WLServerTest/WLServerTest.sln`

2. **添加 ConsoleMode 文件到项目**
   - 在解决方案资源管理器中，右键点击 WLServerTest 项目
   - 选择"添加" → "现有项"
   - 添加：`ConsoleMode.h` 和 `ConsoleMode.cpp`

3. **编译**
   - 选择 Release 配置
   - 点击"生成" → "生成解决方案"

4. **查找生成的文件**
   - `external/WLServerTest/Release/WLServerTest.exe`

## 测试 ConsoleMode

创建测试配置文件 `test_config.ini`：

```ini
[Server]
IP=192.168.7.254
Port=8440
HBPort=4575

[Client]
IDPrefix=TestClient-
StartIP=6.6.6.6
StartNum=1
Count=5
SkipRegistration=false

[Heartbeat]
Interval=30000
TotalMinutes=1

[Log]
ClientCount=0
```

运行测试：
```cmd
WLServerTest.exe --console --config test_config.ini
```

## 部署

复制到发布目录：
```cmd
copy external\WLServerTest\Release\WLServerTest.exe artifacts\SimulatorAppPublish\
```
