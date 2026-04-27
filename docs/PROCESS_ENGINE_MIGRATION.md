# ProcessEngine 迁移指南

## 概述

将 MainViewModel.cs 从使用 NativeEngine（DLL方式）迁移到 ProcessEngine（进程方式）。

## 改动步骤

### 1. 修改字段声明

改动前：
```csharp
private NativeEngineInterop? _nativeEngine;
private CancellationTokenSource? _neStatsCts;
```

改动后：
```csharp
private ProcessEngine? _processEngine;
```

### 2. 修改初始化逻辑

改动前：
```csharp
_nativeEngine = new NativeEngineInterop();
var config = new NE_Config { ... };
_nativeEngine.Init(ref config, clients, ...);
```

改动后：
```csharp
_processEngine = new ProcessEngine();
_processEngine.OnStatsUpdated += () => {
    RunOnUi(() => {
        HbConnected = _processEngine.HBSending;
    });
};

_processEngine.Start(
    PlatformHost, PlatformPort, PlatformPort,
    RegPrefix, RegStartIp, RegStart, RegCount,
    HbInterval, 60,
    LogThreatClientCount, (int)LogTotalMessages, LogThreatEps, 2
);
```

### 3. 移除统计轮询，改用事件

改动前：轮询GetStats()
改动后：订阅OnStatsUpdated事件

### 4. 修改停止逻辑

改动前：
```csharp
_nativeEngine?.StopAll();
```

改动后：
```csharp
_processEngine?.Stop();
```

## 完整示例见文档
