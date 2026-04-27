# 进程分离架构实施检查清单

## ✅ 已完成

### 代码实现

- [x] C++ ConsoleMode.h - 控制台模式头文件
- [x] C++ ConsoleMode.cpp - 控制台模式实现（约300行）
- [x] C++ WLServerTest.cpp - 添加命令行参数解析
- [x] C# ProcessEngine.cs - 进程管理类（约250行）

### 文档

- [x] README.md - 更新主文档，说明新架构
- [x] PROCESS_MIGRATION_SUMMARY.md - 迁移总结
- [x] docs/PROCESS_ARCHITECTURE.md - 架构说明
- [x] docs/PROCESS_ENGINE_MIGRATION.md - MainViewModel迁移指南
- [x] docs/BUILD_INSTRUCTIONS.md - 编译说明
- [x] docs/QUICK_START.md - 快速开始指南
- [x] config.ini.example - 配置文件示例

### 脚本

- [x] build_all.bat - 一键编译脚本
- [x] test_console.bat - 测试脚本

## 📋 待完成（你需要做的）

### 1. 编译C++（5分钟）

```bash
# 方式1：使用脚本
build_all.bat

# 方式2：手动编译
cd external/WLServerTest
# 使用VS打开WLServerTest.sln
# 添加ConsoleMode.h和ConsoleMode.cpp到项目
# 编译Release版本
```

**检查点**：
- [ ] 编译成功，无错误
- [ ] 生成 external/WLServerTest/Release/WLServerTest.exe

### 2. 测试C++控制台模式（10分钟）

```bash
test_console.bat
```

**检查点**：
- [ ] 程序启动成功
- [ ] 看到 "INFO|Config loaded successfully"
- [ ] 看到 "INFO|Clients created: 5"
- [ ] 看到 "INFO|Registration completed: 5/5"
- [ ] 看到 "STATS|RegisteredTotal=5|..."
- [ ] 可以用Ctrl+C停止

### 3. 修改MainViewModel.cs（30分钟）

参考：`docs/PROCESS_ENGINE_MIGRATION.md`

**需要修改的地方**：

#### 3.1 字段声明
```csharp
// 找到这行
private NativeEngineInterop? _nativeEngine;

// 改为
private ProcessEngine? _processEngine;

// 删除这行（不再需要）
private CancellationTokenSource? _neStatsCts;
```

#### 3.2 添加using
```csharp
using SimulatorApp.Workers;  // 添加这行
```

#### 3.3 修改初始化逻辑

找到创建NativeEngine的地方（可能在StartHeartbeat或类似方法中），改为：

```csharp
_processEngine = new ProcessEngine();

// 订阅事件
_processEngine.OnStatsUpdated += () => {
    RunOnUi(() => {
        HbConnected = _processEngine.HBSending;
        HbTotal = _processEngine.RegisteredTotal;
        // 更新其他统计数据...
    });
};

_processEngine.OnInfoMessage += (msg) => {
    RunOnUi(() => {
        StatusLog += $"[{DateTime.Now:HH:mm:ss}] {msg}\n";
    });
};

_processEngine.OnErrorMessage += (msg) => {
    RunOnUi(() => {
        StatusLog += $"[{DateTime.Now:HH:mm:ss}] ERROR: {msg}\n";
    });
};

// 启动进程
bool success = _processEngine.Start(
    PlatformHost,
    PlatformPort,
    PlatformPort,  // HBPort，根据实际情况调整
    RegPrefix,
    RegStartIp,
    RegStart,
    RegCount,
    HbInterval,
    60,  // HB总分钟数
    LogThreatClientCount,
    (int)LogTotalMessages,
    LogThreatEps,
    CalculateLogSelectedTypes()  // 需要实现这个方法
);

if (!success) {
    MessageBox.Show("启动压测进程失败", "错误", 
        MessageBoxButton.OK, MessageBoxImage.Error);
}
```

#### 3.4 实现CalculateLogSelectedTypes方法

```csharp
private int CalculateLogSelectedTypes()
{
    int types = 0;
    if (CatClientOps) types |= 1;      // OPT
    if (CatThreatProcStart || CatThreatRegAccess || CatThreatFileAccess) 
        types |= 2;  // THREAT
    if (CatNonWhitelist) types |= 4;   // NWL
    // 根据实际需要添加其他类型
    return types;
}
```

#### 3.5 修改停止逻辑

找到停止的地方，改为：

```csharp
// 改动前
_nativeEngine?.StopAll();
_nativeEngine?.Dispose();
_neStatsCts?.Cancel();

// 改动后
_processEngine?.Stop();
_processEngine?.Dispose();
```

#### 3.6 移除统计轮询

找到并删除类似这样的代码：

```csharp
// 删除这段
_neStatsCts = new CancellationTokenSource();
Task.Run(async () => {
    while (!_neStatsCts.Token.IsCancellationRequested) {
        var stats = _nativeEngine.GetStats();
        RunOnUi(() => {
            HbConnected = stats.hbConnected;
            // ...
        });
        await Task.Delay(2000);
    }
});
```

**检查点**：
- [ ] 编译成功，无错误
- [ ] 没有NativeEngine相关的引用

### 4. 编译C#（5分钟）

```bash
cd src/SimulatorApp
dotnet build -c Release
```

**检查点**：
- [ ] 编译成功，无错误
- [ ] 生成 src/SimulatorApp/bin/Release/net8.0-windows/SimulatorApp.exe

### 5. 部署测试（5分钟）

```bash
# 创建部署目录
mkdir deploy
cd deploy

# 复制文件
copy ..\src\SimulatorApp\bin\Release\net8.0-windows\*.exe .
copy ..\src\SimulatorApp\bin\Release\net8.0-windows\*.dll .
copy ..\external\WLServerTest\Release\WLServerTest.exe .
```

**检查点**：
- [ ] SimulatorApp.exe 存在
- [ ] WLServerTest.exe 存在
- [ ] 所有DLL都存在

### 6. 功能测试（30分钟）

#### 6.1 启动测试
```bash
cd deploy
SimulatorApp.exe
```

**检查点**：
- [ ] 界面正常显示
- [ ] 配置项正常

#### 6.2 小规模测试（5个客户端）
- [ ] 配置5个客户端
- [ ] 点击"启动压测"
- [ ] 看到状态更新
- [ ] 注册成功：5/5
- [ ] 在线客户端：5/5
- [ ] 心跳正常
- [ ] 日志发送正常
- [ ] 点击"停止"，程序正常停止

#### 6.3 中等规模测试（50个客户端）
- [ ] 配置50个客户端
- [ ] 启动压测
- [ ] 注册成功：50/50
- [ ] 在线客户端：50/50
- [ ] 运行5分钟，稳定

#### 6.4 大规模测试（300个客户端）
- [ ] 配置300个客户端
- [ ] 启动压测
- [ ] 注册成功：300/300
- [ ] 在线客户端：300/300
- [ ] 运行1小时，稳定
- [ ] 无掉线
- [ ] 日志发送成功率100%

### 7. 对比测试（可选，2小时）

#### 7.1 DLL方式（v3.x）
- [ ] 使用原有NativeEngine.dll
- [ ] 300客户端测试
- [ ] 记录：掉线次数、日志失败率、EPS抖动

#### 7.2 进程方式（v4.0）
- [ ] 使用新的ProcessEngine
- [ ] 300客户端测试
- [ ] 记录：掉线次数、日志失败率、EPS抖动

#### 7.3 对比结果
| 指标 | DLL方式 | 进程方式 | 改善 |
|------|---------|---------|------|
| 掉线次数 | ___ | ___ | ___ |
| 日志失败率 | ___% | ___% | ___ |
| EPS抖动 | ___ | ___ | ___ |

## 🐛 常见问题

### 问题1：C++编译失败

**症状**：找不到SendInfoToServer.h或client.h

**解决**：
1. 确保所有头文件都在项目中
2. 检查包含路径设置
3. 确保使用VS2019或更高版本

### 问题2：C++程序启动失败

**症状**：双击WLServerTest.exe无反应

**解决**：
1. 检查是否缺少DLL依赖
2. 使用 `dumpbin /dependents WLServerTest.exe` 查看依赖
3. 安装VC++ Redistributable

### 问题3：C#找不到WLServerTest.exe

**症状**：启动压测时提示找不到文件

**解决**：
1. 确保WLServerTest.exe在SimulatorApp.exe同目录
2. 或修改ProcessEngine.cs中的路径

### 问题4：看不到状态更新

**症状**：界面无反应

**解决**：
1. 修改ProcessEngine.cs：`CreateNoWindow = false`
2. 查看C++控制台窗口是否有输出
3. 检查OnStatsUpdated事件是否正确订阅

## 📊 成功标准

- [ ] 编译无错误
- [ ] 5客户端测试通过
- [ ] 50客户端测试通过
- [ ] 300客户端测试通过，运行1小时稳定
- [ ] 掉线次数：0
- [ ] 日志失败率：0%
- [ ] EPS平稳，无抖动

## 🎉 完成

当所有检查点都打勾后，进程分离架构迁移完成！

**预期收益**：
- ✅ 300客户端100%稳定
- ✅ 无GC干扰
- ✅ 故障隔离
- ✅ 易于调试

**下一步**：
- 正式发布v4.0
- 更新用户文档
- 培训使用人员
