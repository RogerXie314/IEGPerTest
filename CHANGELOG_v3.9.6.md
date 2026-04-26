# v3.9.6 变更日志

## 发布日期
2026-04-26

## 架构变更：从 DLL 方式迁移到子进程方式

### 核心变更
v3.9.6 采用**子进程架构（方案B）**，解决 v3.9.5 中 300 客户端心跳不稳定的问题。

**架构设计**：
```
C# WPF (SimulatorApp.exe)
  ├─ 注册客户端 → Clients.log
  ├─ 白名单上传、HTTPS日志等功能
  └─ 启动 C++ 子进程 (WLServerTest.exe --console)
       ├─ 读取配置文件（客户端信息）
       ├─ 跳过注册（SkipRegistration=true）
       ├─ 心跳发送（TCP长连接）
       └─ 日志发送（威胁检测）
```

### 优势
- ✅ 完全进程隔离，C++ 子进程不受 C# GC 影响
- ✅ 保留现有功能（白名单上传、HTTPS日志等）
- ✅ 最小改动，风险最低
- ✅ 数据持久化（Clients.log）

---

## 修改文件清单

### C# 代码修改

#### 1. `src/SimulatorApp/ViewModels/MainViewModel.cs`
**变更**：从 NativeEngine（DLL方式）迁移到 ProcessEngine（子进程方式）

**主要修改**：
- ❌ 删除：`using SimulatorApp.Interop;`
- ✅ 添加：`using SimulatorApp.Workers;`
- ❌ 删除：`private NativeEngineInterop? _nativeEngine;`
- ✅ 添加：`private ProcessEngine? _processEngine;`
- ❌ 删除：`_policyWorker` 字段（暂不支持策略接收）
- ✅ 修改：启动/停止逻辑使用 `ProcessEngine`

**影响**：
- 心跳和日志发送改为 C++ 子进程处理
- 注册仍在 C# 中完成，保存到 `Clients.log`

#### 2. `src/SimulatorApp/Workers/ProcessEngine.cs` ✨ 新增
**功能**：进程引擎，负责启动和管理 C++ 子进程

**核心功能**：
- 启动 `WLServerTest.exe --console` 子进程
- 生成配置文件（`config_process.ini`）
- 读取 `Clients.log` 获取已注册客户端信息
- 监控子进程输出（STATS 统计信息）
- 进程生命周期管理

**配置参数**：
```ini
[Client]
SkipRegistration=true  # 跳过注册，使用已注册客户端
IDPrefix=WLClient_
StartNum=200
Count=100
```

#### 3. `src/SimulatorApp/SimulatorApp.csproj`
**变更**：版本号更新

**修改**：
```xml
<Version>3.9.6</Version>
<AssemblyVersion>3.9.6</AssemblyVersion>
<FileVersion>3.9.6</FileVersion>
```

---

### C++ 代码修改

#### 4. `external/WLServerTest/ConsoleMode.h` ✨ 新增
**功能**：控制台模式头文件

**核心结构**：
```cpp
struct ConsoleConfig {
    std::string serverIP;
    int serverPort;
    int serverHBPort;
    std::string clientIDPrefix;
    std::string clientStartIP;
    int clientStartNum;
    int clientCount;
    int hbInterval;
    int hbTotalMinutes;
    int logClientCount;
    int logEachClientTotalItems;
    int logEachClientPerSecondItems;
    int logSelectedTypes;
    bool skipRegistration;  // ✨ 关键：跳过注册
};
```

**核心函数**：
- `LoadConfigFromFile()` - 从 INI 加载配置
- `RunConsoleMode()` - 运行控制台模式（阻塞）
- `OutputStats()` - 输出统计信息到 stdout

#### 5. `external/WLServerTest/ConsoleMode.cpp` ✨ 新增
**功能**：控制台模式实现（约 350 行）

**核心逻辑**：
1. 加载配置文件
2. 初始化 WSA 和全局变量
3. 创建客户端对象（根据配置）
4. **跳过注册**（如果 `SkipRegistration=true`）
5. 启动心跳线程（每个客户端一个线程）
6. 启动日志线程（部分客户端）
7. 定期输出统计信息（STATS 格式）

**关键特性**：
- ✅ 支持跳过注册（`skipRegistration` 参数）
- ✅ 输出格式：`STATS|RegisteredTotal=100|HBSending=98|...`
- ✅ 独立运行，不依赖 UI

#### 6. `external/WLServerTest/WLServerTest.vcxproj`
**变更**：项目配置修改，支持 VS2019 编译

**主要修改**：
```xml
<PropertyGroup Label="Globals">
  <VCToolsVersion>14.29.30037</VCToolsVersion>  <!-- 强制使用完整 MFC 版本 -->
  <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
</PropertyGroup>

<!-- 所有配置统一使用 v142 + Dynamic MFC -->
<PlatformToolset>v142</PlatformToolset>
<UseOfMfc>Dynamic</UseOfMfc>
```

**ItemGroup 添加**：
```xml
<ClCompile Include="ConsoleMode.cpp" />
<ClInclude Include="ConsoleMode.h" />
```

---

### 文档修改

#### 7. `README.md`
**变更**：更新版本号和架构说明

**修改**：
- 版本号：v3.9.5 → v3.9.6
- 添加子进程架构说明
- 更新编译和部署说明

#### 8. `docs/PROCESS_ARCHITECTURE.md` ✨ 新增
**内容**：子进程架构详细设计文档

**章节**：
- 架构对比（方案A vs 方案B）
- 数据流图
- 进程通信机制
- 配置文件格式
- 故障处理

#### 9. `docs/PROCESS_ENGINE_MIGRATION.md` ✨ 新增
**内容**：MainViewModel 迁移指南

**章节**：
- 迁移步骤
- 代码对比（Before/After）
- 测试验证
- 常见问题

#### 10. `docs/BUILD_INSTRUCTIONS.md` ✨ 新增
**内容**：完整编译指南

**章节**：
- 环境要求
- C++ DLL 编译
- C++ WLServerTest.exe 编译
- C# 应用编译
- 打包发布

#### 11. `docs/QUICK_START.md` ✨ 新增
**内容**：快速开始指南

**章节**：
- 5 客户端测试
- 50 客户端测试
- 300 客户端测试
- 故障排查

#### 12. `BUILD_v3.9.6_SUMMARY.md` ✨ 新增
**内容**：v3.9.6 编译打包总结

**章节**：
- 编译结果
- 架构说明
- 部署说明
- 已知问题
- 下一步计划

#### 13. `CHECKLIST.md` ✨ 新增
**内容**：实施检查清单

**章节**：
- 编译检查
- 功能测试
- 压力测试
- 部署验证

#### 14. `PROCESS_MIGRATION_SUMMARY.md` ✨ 新增
**内容**：进程分离架构迁移总结

**章节**：
- 方案对比
- 实施步骤
- 测试结果
- 经验教训

#### 15. `COMPILE_WLSERVERTEST.md` ✨ 新增
**内容**：WLServerTest.exe 编译说明

**章节**：
- MFC 依赖问题
- 工具链版本选择
- 编译命令
- 故障排查

---

### 辅助文件

#### 16. `build_all.bat` ✨ 新增
**功能**：一键编译所有组件

**内容**：
```batch
@echo off
echo Building all components...
call src\NativeEngine\build.bat
call src\NativeSender\build.bat
call src\RawPacketEngine\build.bat
echo Building C# application...
dotnet publish src\SimulatorApp\SimulatorApp.csproj -c Release
echo Done!
```

#### 17. `config.ini.example` ✨ 新增
**功能**：配置文件示例

**内容**：
```ini
[Server]
IP=192.168.7.254
Port=8440
HBPort=4575

[Client]
IDPrefix=WLClient_
StartIP=6.6.6.6
StartNum=200
Count=100
SkipRegistration=true

[Heartbeat]
Interval=30000
TotalMinutes=60

[Log]
ClientCount=50
EachClientTotalItems=100
EachClientPerSecondItems=1
SelectedTypes=2
```

---

## 删除的过时文件

以下文件已删除（v3.9.5.1 相关的临时分析文档）：

1. ❌ `MINIMAL_DEPENDENCIES.md` - 最小依赖分析
2. ❌ `memory_access_comparison.md` - 内存访问对比
3. ❌ `new_vs_old_tool_analysis.md` - 新旧工具分析
4. ❌ `v3.9.5.1_optimization_verification.md` - 优化验证
5. ❌ `C#_DLL交互问题分析.md` - DLL 交互问题（已改用子进程）
6. ❌ `新工具为什么不用全局socket数组.md` - 旧工具分析
7. ❌ `STANDALONE_TEST_GUIDE.md` - 独立测试指南
8. ❌ `test_console.bat` - 测试脚本
9. ❌ `使用步骤-StandaloneTest.txt` - 独立测试步骤
10. ❌ `src/StandaloneTest/` - 独立测试项目目录

---

## 编译状态

### ✅ 已完成
- C++ DLL 编译（NativeEngine.dll, NativeSender.dll, RawPacketEngine.dll）
- C# 应用编译（SimulatorApp.exe）
- ZIP 包打包（SimulatorApp-v3.9.6-process-architecture.zip, 49 MB）

### ⏳ 待完成
- C++ WLServerTest.exe 编译（需要在有 MFC 库的环境中编译）
  - 项目文件已修改（WLServerTest.vcxproj）
  - ConsoleMode.cpp/h 已添加
  - 需要 VS2019 + MFC 14.29.30037 工具链

---

## 测试计划

### 功能测试
1. ✅ 注册功能（C# 部分）
2. ⏳ 心跳功能（C++ 子进程）
3. ⏳ 日志发送（C++ 子进程）
4. ⏳ 进程通信（配置文件 + stdout）

### 压力测试
1. ⏳ 5 客户端（基础验证）
2. ⏳ 50 客户端（中等压力）
3. ⏳ 300 客户端（高压力，验证稳定性）

---

## 已知问题

1. **WLServerTest.exe 未编译**
   - 原因：当前环境缺少完整的 MFC 库（14.29.30037）
   - 解决：在有完整 VS2019 + MFC 的环境中编译

2. **300 客户端稳定性未验证**
   - 需要完成 WLServerTest.exe 编译后进行测试

---

## 下一步

1. ⏳ 在有 MFC 库的环境中编译 WLServerTest.exe
2. ⏳ 将 WLServerTest.exe 复制到发布目录
3. ⏳ 重新打包 ZIP
4. ⏳ 功能测试（注册→心跳→日志）
5. ⏳ 压力测试（5-50-300 客户端）
6. ⏳ 更新项目文档

---

## 参考文档

- `PROCESS_MIGRATION_SUMMARY.md` - 进程分离架构迁移总结
- `CHECKLIST.md` - 实施检查清单
- `docs/PROCESS_ARCHITECTURE.md` - 架构说明
- `docs/PROCESS_ENGINE_MIGRATION.md` - MainViewModel 迁移指南
- `docs/BUILD_INSTRUCTIONS.md` - 编译指南
- `BUILD_v3.9.6_SUMMARY.md` - 编译打包总结
- `COMPILE_WLSERVERTEST.md` - WLServerTest.exe 编译说明
