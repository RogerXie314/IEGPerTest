# SimulatorApp v3.9.6 编译打包总结

## 编译时间
2026-04-26

## 版本信息
- **版本号**: v3.9.6
- **架构**: 子进程方式（选项B：注册在C#，心跳和日志在C++子进程）
- **编译方式**: 使用绝对路径，避免路径问题
- **状态**: C# 部分已完成，C++ WLServerTest.exe 需要编译

## 编译结果

### ✅ C++ DLL 编译成功
- **NativeEngine.dll**: 81.5 KB
- **NativeSender.dll**: 12.5 KB  
- **RawPacketEngine.dll**: 67 KB

### ✅ C# 应用编译成功
- **SimulatorApp.exe**: 154.6 MB (单文件发布)
- **总大小**: 162.7 MB (包含所有依赖)

### ✅ 发布包内容
```
artifacts/SimulatorAppPublish/
├── SimulatorApp.exe          (154.6 MB) - 主程序
├── NativeEngine.dll          (81.5 KB)  - C++ 引擎
├── NativeSender.dll          (12.5 KB)  - 原生发送器
├── RawPacketEngine.dll       (67 KB)    - 原始包引擎
├── D3DCompiler_47_cor3.dll   (4.7 MB)   - WPF 依赖
├── PenImc_cor3.dll           (154 KB)   - WPF 依赖
├── PresentationNative_cor3.dll (1.2 MB) - WPF 依赖
├── vcruntime140_cor3.dll     (122 KB)   - VC++ 运行时
└── wpfgfx_cor3.dll           (1.9 MB)   - WPF 图形
```

### ✅ ZIP 包
- **文件名**: `SimulatorApp-v3.9.6-process-architecture.zip`
- **大小**: 49 MB (压缩后)
- **位置**: `artifacts/SimulatorApp-v3.9.6-process-architecture.zip`

## 编译命令（使用绝对路径）

```powershell
# 1. 发布编译
powershell -ExecutionPolicy Bypass -File "D:\Github\IEGPerTest\scripts\publish_simulatorapp.ps1"

# 2. 打包 ZIP
Compress-Archive -Path "D:\Github\IEGPerTest\artifacts\SimulatorAppPublish\*" `
                 -DestinationPath "D:\Github\IEGPerTest\artifacts\SimulatorApp-v3.9.6-process-architecture.zip" `
                 -Force
```

## 架构说明

### 当前版本（v3.9.6）- 子进程架构（选项B）

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

**优势**：
- ✅ 完全进程隔离，无GC干扰
- ✅ 保留现有功能（白名单上传、HTTPS日志等）
- ✅ 最小改动，风险最低
- ✅ 数据持久化（Clients.log）

**当前状态**：
- ✅ C# MainViewModel 已修改（使用 ProcessEngine）
- ✅ C# ProcessEngine 已实现
- ✅ C++ ConsoleMode 已修改（支持 SkipRegistration）
- ⏳ C++ WLServerTest.exe 需要编译（包含 ConsoleMode.cpp）

**待完成**：
1. 将 ConsoleMode.cpp 和 ConsoleMode.h 添加到 WLServerTest.vcproj
2. 编译 WLServerTest.exe
3. 将 WLServerTest.exe 复制到发布目录
4. 测试完整流程

## 部署说明

### 方式1：解压 ZIP 包
```bash
# 解压到任意目录
unzip SimulatorApp-v3.9.6-process-architecture.zip -d C:\SimulatorApp

# 运行
cd C:\SimulatorApp
SimulatorApp.exe
```

### 方式2：直接使用发布目录
```bash
cd D:\Github\IEGPerTest\artifacts\SimulatorAppPublish
SimulatorApp.exe
```

## 系统要求
- **操作系统**: Windows 10/11 (x64)
- **.NET 运行时**: 已内嵌，无需安装
- **VC++ 运行时**: 已内嵌，无需安装

## 已知问题
1. **300客户端不稳定**: 建议升级到 v4.0（进程分离架构）
2. **文档未更新**: `docs/项目实施文档.md` 中未添加 v3.9.6 变更记录

## 下一步
1. ✅ v3.9.6 C# 部分编译完成（子进程架构）
2. ⏳ 编译 C++ WLServerTest.exe（包含 ConsoleMode）
   - 方法1：用 Visual Studio 打开 WLServerTest.sln，添加 ConsoleMode.cpp/h，编译
   - 方法2：用 MSBuild 命令行编译（需要先修改 .vcproj 文件）
3. ⏳ 将 WLServerTest.exe 复制到发布目录
4. ⏳ 功能测试（注册→心跳→日志）
5. ⏳ 压力测试（5-50-300客户端）
6. ⏳ 更新项目文档

## 参考文档
- `PROCESS_MIGRATION_SUMMARY.md` - 进程分离架构迁移总结
- `CHECKLIST.md` - 实施检查清单
- `docs/PROCESS_ARCHITECTURE.md` - 架构说明
- `docs/PROCESS_ENGINE_MIGRATION.md` - MainViewModel 迁移指南
