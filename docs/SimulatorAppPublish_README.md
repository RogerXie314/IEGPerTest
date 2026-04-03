# SimulatorApp 发布说明

## 版本信息

当前版本：v3.9.2

## 文件清单

本目录包含 SimulatorApp 的完整发布包，采用同目录部署方式。

### 主程序

- **SimulatorApp.exe** (约 70 MB)
  - 主应用程序，.NET 8.0 自包含发布
  - 包含所有 .NET 运行时和依赖项
  - 无需安装 .NET 即可在 Windows x64 系统运行

### C++ 原生 DLL（3个）

#### 1. NativeEngine.dll (约 73 KB)
**作用**：心跳和威胁检测日志的 TCP 长连接引擎（100% 对齐老工具 WLServerTest）

**功能**：
- 非阻塞 socket 管理（对齐老工具 `ThreadFunc_HeartbeatSend_New` 行为）
- 心跳线程：每客户端 1 个 OS 线程，30 秒周期发送心跳包
- 日志线程：威胁检测日志（TCP 长连接），每客户端 1 个 OS 线程
- PT 协议打包：48 字节 Big-Endian 协议头
- JSON 构建 + zlib 压缩（内嵌 zlibstat.lib）
- 心跳线程负责重连和断线检测

**技术细节**：
- 使用 Winsock 非阻塞 socket（`ioctlsocket(FIONBIO, 1)`）
- `send()` 返回 `WSAEWOULDBLOCK` 时立即失败，不阻塞（对齐老工具）
- 心跳和日志线程完全独立，socket 生命周期由心跳线程管理
- 日志线程 send 失败后直接跳过，不关闭 socket（对齐老工具 `ThreadFunc_MsgLogSend`）
- 每客户端 2 个 OS 线程（心跳 + 日志），对齐老工具线程模型

**依赖**：
- zlib 静态库（`external/IEG_Code/code/lib/x64/zlibstat.lib`）
- Winsock2 (`ws2_32.lib`)

#### 2. NativeSender.dll (约 12.5 KB)
**作用**：Winsock 同步发送层（100% 对齐老工具 SendInfoToServer）

**功能**：
- TCP 连接管理：非阻塞 connect + select(2s) 超时 + 切回阻塞模式
- 1KB 分块发送：对齐老工具 `while(nProtocalLen - nSendCount > 0) { send(1024) }`
- 精确接收：`recv_exact()` 循环接收指定字节数
- 心跳收发：`SendHeartbeatAndRecv()` 发送心跳包 + 解析 PT 协议头 + 提取 cmdId
- 威胁日志批发：`SendThreatBatch()` 依次发送 File/ProcStart/Reg 三种类型，间隔 50ms

**技术细节**：
- 100% 对齐老工具 `CSendInfoToServer::CreateConnection` 行为
- 不设置 `TCP_NODELAY` 和 `SO_KEEPALIVE`（与老工具一致）
- 使用阻塞 socket（老工具行为）
- 1KB 分块发送（对齐老工具，不是性能优化，而是行为一致性）

#### 3. RawPacketEngine.dll (约 67 KB)
**作用**：攻击报文发送引擎（基于 Npcap）

**功能**：
- 原始以太网帧发送（绕过 TCP/IP 协议栈）
- 支持内置漏洞利用报文（MS08-067, MS17-010, MS20-796）
- 支持外部 `.etc` 和 `.pcap` 文件导入
- 字段变化规则引擎（源 IP/MAC 自动递增）
- IP/TCP/UDP/ICMP 校验和自动更新
- 多 Stream 并发发送，Round-Robin 调度
- 微秒级速率控制（`QueryPerformanceCounter`）

**技术细节**：
- 依赖 Npcap 驱动（需单独安装：https://npcap.com/）
- 初始化超时保护（3 秒），避免 UI 冻结
- 连续 100 次失败自动停止
- 对齐 xb-ether-tester 的 `t_rule` 字段变化引擎

**依赖**：
- Npcap SDK（编译时）：`C:\npcap-sdk`
- Npcap 驱动（运行时）：用户需安装 Npcap

### WPF 运行时 DLL（7个）

以下为 .NET WPF 应用所需的原生依赖：

- **D3DCompiler_47_cor3.dll** - DirectX 着色器编译器
- **PenImc_cor3.dll** - 触控和手写笔输入
- **PresentationNative_cor3.dll** - WPF 渲染核心
- **vcruntime140_cor3.dll** - Visual C++ 运行时
- **wpfgfx_cor3.dll** - WPF 图形引擎

### 调试符号

- **SimulatorApp.pdb** - 主程序调试符号
- **SimulatorLib.pdb** - 核心库调试符号

## 运行要求

### 系统要求
- Windows 10/11 或 Windows Server 2016+ (x64)
- 无需安装 .NET（自包含发布）

### 可选依赖
- **Npcap**（仅攻击报文发送功能需要）
  - 下载地址：https://npcap.com/
  - 安装时勾选 "WinPcap API-compatible Mode"

## 部署说明

### 单机部署
1. 将整个目录拷贝到目标机器
2. 双击 `SimulatorApp.exe` 启动
3. 如需使用攻击报文发送功能，先安装 Npcap

### 多机部署
- 可直接拷贝整个目录到多台机器
- 所有文件必须保持在同一目录
- 不支持分离部署（DLL 必须与 EXE 同目录）

## 编译信息

### 编译环境
- Visual Studio 2019/2022 Professional
- CMake 3.20+
- .NET 8.0 SDK

### C++ DLL 编译
```powershell
# NativeEngine.dll
cd src/NativeEngine/build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# NativeSender.dll
cd src/NativeSender/build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# RawPacketEngine.dll
cd src/RawPacketEngine/build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### .NET 应用发布
```powershell
.\scripts\publish_simulatorapp.ps1
```

## 架构说明

### v3.9.2 架构特点

1. **完全移除 C# 心跳和长连接日志逻辑**
   - 心跳和威胁检测日志 100% 使用 NativeEngine.dll
   - 无 C# 回退分支，确保行为一致性

2. **DLL 同目录部署**
   - 去掉单文件内嵌逻辑
   - 启动时强制检测 3 个 DLL，缺失则报错退出

3. **对齐老工具行为**
   - 日志线程 send 失败不重连（对齐老工具 `ThreadFunc_MsgLogSend`）
   - Socket 生命周期完全由心跳线程管理（对齐老工具 `ThreadFunc_HeartbeatSend_New`）
   - 非阻塞 socket，`send()` 返回 `WSAEWOULDBLOCK` 立即失败不阻塞

## 故障排查

### 启动失败

**现象**：双击 EXE 无反应或立即退出

**可能原因**：
1. 缺少 DLL 文件 → 检查 3 个 C++ DLL 是否存在
2. 权限不足 → 右键"以管理员身份运行"
3. 被杀毒软件拦截 → 添加信任

### 攻击报文发送失败

**现象**：点击"开始发送"后提示初始化失败

**可能原因**：
1. 未安装 Npcap → 安装 Npcap 驱动
2. Npcap 服务未启动 → 重启 Npcap 服务
3. 初始化超时（3秒）→ 检查 Npcap 安装是否正常

### 心跳连接失败

**现象**：客户端无法上线

**可能原因**：
1. 平台地址/端口配置错误
2. 网络不通 → 使用 `Test-NetConnection` 测试
3. 平台连接数已满 → 检查平台配置

## 更新日志

详见项目根目录 `docs/项目实施文档.md`

## 技术支持

如遇问题，请查看：
- 项目文档：`docs/项目实施文档.md`
- 依赖清单：`MINIMAL_DEPENDENCIES.md`
- 修改记录：`MODIFICATIONS_TODO.md`
