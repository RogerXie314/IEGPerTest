# 编译说明

## 编译C++老工具（控制台模式）

### 方式1：使用Visual Studio

1. 打开 `external/WLServerTest/WLServerTest.sln`
2. 选择 Release 配置
3. 右键项目 → 属性 → C/C++ → 预编译头 → 确保启用
4. 添加新文件到项目：
   - `ConsoleMode.h`
   - `ConsoleMode.cpp`
5. 编译（Ctrl+Shift+B）
6. 输出：`external/WLServerTest/Release/WLServerTest.exe`

### 方式2：使用MSBuild命令行

```bash
cd external/WLServerTest
msbuild WLServerTest.vcproj /p:Configuration=Release /p:Platform=Win32
```

### 测试控制台模式

```bash
cd external/WLServerTest/Release
WLServerTest.exe --console --config ../../../config.ini
```

应该看到：
```
INFO|Config loaded successfully
INFO|Server=192.168.7.254:8440
INFO|ClientCount=300
INFO|Starting console mode...
INFO|Creating 300 clients...
INFO|Clients created: 300
INFO|Registering clients...
...
```

## 编译C# SimulatorApp

### 方式1：使用Visual Studio

1. 打开 `IEGPerTest.sln`
2. 右键 SimulatorApp 项目 → 生成
3. 输出：`src/SimulatorApp/bin/Debug/net8.0-windows/SimulatorApp.exe`

### 方式2：使用dotnet命令行

```bash
cd src/SimulatorApp
dotnet build -c Release
```

## 部署

将以下文件复制到同一目录：

```
部署目录/
├── SimulatorApp.exe          (C# WPF界面)
├── SimulatorApp.dll
├── SimulatorLib.dll
├── WLServerTest.exe          (C++压测内核)
├── WLNetComm.dll             (如果需要)
└── config.ini                (自动生成)
```

## 运行

双击 `SimulatorApp.exe`，界面启动后：
1. 配置服务器地址、客户端数量等参数
2. 点击"启动压测"
3. 后台自动启动 `WLServerTest.exe --console --config config.ini`
4. 界面实时显示状态

## 故障排查

### 问题1：找不到WLServerTest.exe

**解决**：
- 确保 `WLServerTest.exe` 在 `SimulatorApp.exe` 同目录
- 或者修改 `ProcessEngine.cs` 中的路径

### 问题2：C++进程启动失败

**检查**：
```bash
# 手动测试C++程序
WLServerTest.exe --console --config config.ini
```

查看错误信息

### 问题3：看不到状态更新

**检查**：
- C++程序是否正常输出到stdout
- C#是否正确捕获stdout
- 查看 `ProcessEngine.OnErrorMessage` 事件

### 问题4：编译错误

**常见问题**：
- 缺少 `SendInfoToServer.h`：确保包含路径正确
- 缺少 `client.h`：确保包含路径正确
- 链接错误：确保所有依赖库都已添加

## 调试

### 调试C++

1. 修改 `ProcessEngine.cs`：
```csharp
CreateNoWindow = false,  // 改为false，显示控制台窗口
```

2. 或者单独运行C++：
```bash
WLServerTest.exe --console --config config.ini
```

### 调试C#

1. Visual Studio中按F5启动调试
2. 在 `ProcessEngine.cs` 中设置断点
3. 查看 `OnOutputDataReceived` 事件接收到的数据
