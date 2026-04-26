# 快速开始指南

## 5分钟快速测试

### 步骤1：编译C++（2分钟）

```bash
# 打开VS开发者命令提示符
cd external/WLServerTest
msbuild WLServerTest.vcproj /p:Configuration=Release
```

### 步骤2：测试C++控制台模式（1分钟）

```bash
cd Release

# 创建测试配置
echo [Server] > test.ini
echo IP=192.168.7.254 >> test.ini
echo Port=8440 >> test.ini
echo HBPort=4575 >> test.ini
echo. >> test.ini
echo [Client] >> test.ini
echo IDPrefix=TestClient_ >> test.ini
echo StartIP=6.6.6.6 >> test.ini
echo StartNum=1 >> test.ini
echo Count=5 >> test.ini
echo. >> test.ini
echo [Heartbeat] >> test.ini
echo Interval=30000 >> test.ini
echo TotalMinutes=1 >> test.ini
echo. >> test.ini
echo [Log] >> test.ini
echo ClientCount=5 >> test.ini
echo EachClientTotalItems=10 >> test.ini
echo EachClientPerSecondItems=1 >> test.ini
echo SelectedTypes=2 >> test.ini

# 运行测试（5个客户端，1分钟）
WLServerTest.exe --console --config test.ini
```

应该看到：
```
INFO|Config loaded successfully
INFO|Server=192.168.7.254:8440
INFO|ClientCount=5
INFO|Starting console mode...
INFO|Creating 5 clients...
INFO|Clients created: 5
INFO|Registering clients...
INFO|Registered 5/5
INFO|Registration completed: 5/5
INFO|Starting heartbeat threads...
INFO|Heartbeat threads started
INFO|Waiting for heartbeat to stabilize...
INFO|Starting log threads...
INFO|Log threads started
INFO|Console mode is running... (Press Ctrl+C to stop)
STATS|RegisteredTotal=5|HBSending=5|HBNotSending=0|LogNotSending=0
STATS|RegisteredTotal=5|HBSending=5|HBNotSending=0|LogNotSending=0
...
```

### 步骤3：编译C#（1分钟）

```bash
cd ../../../src/SimulatorApp
dotnet build -c Release
```

### 步骤4：运行完整系统（1分钟）

```bash
cd bin/Release/net8.0-windows

# 复制C++程序
copy ..\..\..\..\..\..\external\WLServerTest\Release\WLServerTest.exe .

# 运行
SimulatorApp.exe
```

## 完整测试（300客户端）

### 修改配置

在C# WPF界面中：
1. 服务器IP：`192.168.7.254`
2. 端口：`8440`
3. 客户端数量：`300`
4. 客户端前缀：`WLClient_`
5. 起始IP：`6.6.6.6`
6. 起始编号：`200`
7. 心跳间隔：`30000` (30秒)
8. 日志客户端数：`300`
9. 日志EPS：`1`

### 启动测试

1. 点击"启动压测"
2. 观察状态：
   - 注册进度
   - 在线客户端数
   - 心跳成功/失败
   - 日志发送成功/失败

### 预期结果

- 注册：300/300 成功
- 在线：300/300
- 心跳：稳定，无掉线
- 日志：持续发送，无丢失

## 对比测试（验证稳定性）

### 测试1：DLL方式（当前不稳定）

使用原有的 `NativeEngine.dll` 方式运行300客户端，观察：
- 是否有掉线
- 日志发送是否失败
- EPS是否抖动

### 测试2：进程方式（新方案）

使用 `ProcessEngine` + `WLServerTest.exe` 方式运行300客户端，观察：
- 是否稳定
- 日志发送是否成功
- EPS是否平稳

### 对比指标

| 指标 | DLL方式 | 进程方式 | 改善 |
|------|---------|---------|------|
| 掉线次数 | ? | 0 | ✅ |
| 日志失败率 | ?% | 0% | ✅ |
| EPS抖动 | 有 | 无 | ✅ |
| CPU使用率 | ?% | ?% | - |
| 内存使用 | ?MB | ?MB | - |

## 故障排查

### C++程序无法启动

```bash
# 检查依赖
dumpbin /dependents WLServerTest.exe

# 检查是否缺少DLL
# 常见缺失：mfc140.dll, vcruntime140.dll
```

### 看不到输出

```bash
# 修改ProcessEngine.cs，显示控制台窗口
CreateNoWindow = false
```

### 连接服务器失败

```bash
# 检查服务器是否可达
ping 192.168.7.254
telnet 192.168.7.254 8440
```

## 下一步

- 阅读 [架构说明](PROCESS_ARCHITECTURE.md)
- 阅读 [编译说明](BUILD_INSTRUCTIONS.md)
- 阅读 [迁移指南](PROCESS_ENGINE_MIGRATION.md)
