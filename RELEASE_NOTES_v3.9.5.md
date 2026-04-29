# IEGPerTest Simulator v3.9.5

发布日期：2026-04-17

## 🎉 新增功能

### 客户端版本管理
- ✅ 新增版本管理窗口，支持增删改客户端版本列表
- ✅ 版本配置保存到 JSON 文件（%AppData%/SimulatorApp/client_versions.json）
- ✅ Windows 和 Linux 版本分别管理
- ✅ 在注册设置区域添加"管理版本"按钮
- ✅ 支持恢复默认版本列表

### Windows Server 识别修复
- ✅ 修复 Windows Server 2012 R2 显示为 "Microsoft Windows 6.3.9600" 的问题
- ✅ 增强 OsInfo.GetWindowsVersionName() 方法，通过版本号识别 Windows Server
- ✅ 支持识别：Server 2022/2019/2016/2012 R2/2012/2008 R2

## 📦 部署说明

### 系统要求
- Windows x64 操作系统
- 攻击报文发送功能需要安装 [Npcap](https://npcap.com/) 驱动（可选）

### 安装步骤
1. 下载 `SimulatorAppPublish.zip`
2. 解压到任意目录
3. 双击 `SimulatorApp.exe` 运行

**注意**：应用是自包含 EXE，无需安装 .NET 运行时。

### 文件清单
```
SimulatorApp.exe              # 主程序（约 155 MB，含 .NET 运行时）
NativeEngine.dll              # C++ 心跳/威胁日志引擎
NativeSender.dll              # C++ PT 协议打包
RawPacketEngine.dll           # C++ 攻击报文发送引擎（需 Npcap 驱动）
wpfgfx_cor3.dll               # WPF 原生渲染库（及同目录其他 _cor3.dll）
config.json                   # 配置文件（首次运行自动生成）
```

## 🔄 从 v3.9.3 升级

本版本新增功能：
1. **客户端版本管理**：可在界面中管理客户端版本列表，不再需要修改代码
2. **Windows Server 识别**：在 Windows Server 系统上运行时，系统信息显示更准确

升级步骤：
1. 备份现有的 `Clients.log` 和 `config.json`（如需保留）
2. 用新版本文件替换旧版本
3. 启动程序，配置文件会自动迁移

## 📝 完整变更记录

### v3.9.5（本版本）
- 新增客户端版本管理功能
- 修复 Windows Server 识别问题

### v3.9.3
- 新增白名单预览功能
- 白名单上传功能增强
- UI 布局修复

### v3.9.2
- 架构简化和稳定性优化
- Npcap 检测优化
- 编译环境完善

## 🛠️ 技术栈

- .NET 8.0 (WPF)
- C++ DLL（NativeEngine、NativeSender、RawPacketEngine）
- CMake（C++ 构建，MSVC x64）
- 自包含发布（win-x64）

## 📖 文档

- [README.md](https://github.com/RogerXie314/IEGPerTest/blob/main/README.md) - 项目概览和快速开始
- [项目实施文档](https://github.com/RogerXie314/IEGPerTest/blob/main/docs/项目实施文档.md) - 完整变更记录和开发指南
- [白名单预览功能说明](https://github.com/RogerXie314/IEGPerTest/blob/main/docs/白名单预览功能说明.md)

## 🐛 已知问题

无

## 💬 反馈

如有问题或建议，请在 [Issues](https://github.com/RogerXie314/IEGPerTest/issues) 中提交。
