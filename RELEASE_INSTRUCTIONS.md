# 创建 v3.9.5 Release 的详细步骤

## 方法一：通过 GitHub Web 界面（推荐）

### 步骤：

1. **访问 Releases 页面**
   - 打开浏览器访问：https://github.com/RogerXie314/IEGPerTest/releases
   - 点击右上角的 "Draft a new release" 按钮

2. **选择标签**
   - 在 "Choose a tag" 下拉框中选择：`v3.9.5`
   - 标签已经创建并推送到远程仓库

3. **填写 Release 信息**
   - **Release title**：`v3.9.5 - 客户端版本管理 + Windows Server 识别修复`
   - **Target**：`main` 分支
   - **Description**：复制下面的 Release Notes 内容

4. **上传文件**
   - 点击 "Attach binaries by dropping them here or selecting them"
   - 上传文件：`SimulatorAppPublish(V3.9.5).zip`

5. **发布设置**
   - ✅ 勾选 "Set as the latest release"
   - 不要勾选 "Set as a pre-release"

6. **发布**
   - 点击 "Publish release" 按钮

---

## 方法二：安装 GitHub CLI 后使用命令行

### 安装 GitHub CLI：
```powershell
# 使用 winget 安装
winget install --id GitHub.cli

# 或者从 https://cli.github.com/ 下载安装
```

### 安装后执行：
```powershell
# 登录 GitHub
gh auth login

# 创建 Release
gh release create v3.9.5 `
  --title "v3.9.5 - 客户端版本管理 + Windows Server 识别修复" `
  --notes-file RELEASE_NOTES_v3.9.5.md `
  --latest `
  "SimulatorAppPublish(V3.9.5).zip"
```

---

## Release Notes（复制到 Description 中）

```markdown
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
1. 下载 `SimulatorAppPublish(V3.9.5).zip`
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

## 📝 版本历史

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
```

---

## 检查清单

- [x] Git 标签 v3.9.5 已创建并推送
- [x] README.md 已更新至 v3.9.5
- [x] 项目实施文档已更新
- [x] Release Notes 已准备
- [ ] 在 GitHub 上创建 Release（需要手动完成）
- [ ] 上传 SimulatorAppPublish(V3.9.5).zip
- [ ] 设置为 latest release
