# 修改任务清单

### 任务 1：清理界面日志显示 ✅
- [x] 移除频繁的断线重连日志（NativeEngine 心跳统计）
- [x] 简化心跳状态输出（移除详细断线原因）
- [x] 启动时只显示 TCP/HTTPS 连接状态
- [x] 详细统计移至 Debug.WriteLine

**修改文件**：
- `src/SimulatorApp/ViewModels/MainViewModel.cs`

---

### 任务 2：完全移除 C# 心跳和长连接日志逻辑 ✅
- [x] 移除 `StartHeartbeatAsync` 中的 C# HeartbeatWorker 分支
- [x] 移除 `StartLogAsync` 中的 C# LogWorker TCP 长连接分支
- [x] NativeEngine.dll 变为强制依赖（启动时检测）
- [x] HTTPS 短连接日志传递 `null` 给 `_hbStreamRegistry`

**修改文件**：
- `src/SimulatorApp/ViewModels/MainViewModel.cs`

---

### 任务 3：去掉 DLL 内嵌打包，恢复同目录部署 ✅
- [x] 移除 `ExcludeFromSingleFile=false`
- [x] 移除 `IncludeNativeLibrariesForSelfExtract`
- [x] DLL 输出到 EXE 同目录
- [x] 启动时检测 DLL，缺失则报错退出

**修改文件**：
- `src/SimulatorApp/SimulatorApp.csproj`
- `src/SimulatorApp/App.xaml.cs`

---

### 任务 4：移除长连接日志的断线重连逻辑 ✅
- [x] 确认老工具行为：`ThreadFunc_MsgLogSend` 无重连逻辑
- [x] 修改 `LogThreadProc`：send 失败不关闭 socket，不重连
- [x] Socket 生命周期完全由心跳线程管理
- [x] 日志线程只负责发送，失败则跳过

**修改文件**：
- `src/NativeEngine/native_engine.cpp`

---

### 任务 5：修复攻击报文发送模块 Npcap 检测问题 ✅
- [x] `RPE_Init()` 添加 3 秒超时保护（使用 `std::async`）
- [x] 返回 -2 表示超时，-1 表示一般错误
- [x] `RawPacketEngineInterop.Init()` 返回 int 错误码
- [x] `RawPacketSenderService.Initialize()` 根据错误码设置友好提示
- [x] `RawPacketViewModel.InitializeAsync()` 显示 MessageBox 错误对话框
- [x] 错误提示包含 Npcap 下载地址：https://npcap.com/

**修改文件**：
- `src/RawPacketEngine/raw_packet_engine.cpp`
- `src/SimulatorLib/RawPacket/RawPacketEngineInterop.cs`
- `src/SimulatorLib/RawPacket/RawPacketSenderService.cs`
- `src/SimulatorApp/ViewModels/RawPacketViewModel.cs`

---

### 任务 6：调整日志发送默认值 ✅
- [x] 短连接客户端数：0
- [x] 短连接 EPS：0
- [x] 长连接客户端数：50（从 0 改为 50）
- [x] 长连接轮次：1

**修改文件**：
- `src/SimulatorApp/ViewModels/MainViewModel.cs`

---

## 🎉 所有任务已完成！

所有 6 项修改任务均已完成：
1. ✅ 清理界面日志显示
2. ✅ 完全移除 C# 心跳和长连接日志逻辑
3. ✅ 去掉 DLL 内嵌打包，恢复同目录部署
4. ✅ 移除长连接日志的断线重连逻辑
5. ✅ 修复攻击报文发送模块 Npcap 检测问题
6. ✅ 调整日志发送默认值

### 关键改进总结：
- 心跳和长连接日志完全使用 C++ NativeEngine，移除 C# 回退逻辑
- DLL 恢复同目录部署，启动时强制检测
- 日志线程对齐老工具行为：send 失败不重连，socket 由心跳线程管理
- Npcap 检测添加超时保护，避免 UI 冻结，提供友好错误提示
- 界面日志简化，移除频繁的断线重连信息
- 日志发送默认值调整：短连接默认 0，长连接客户端 50，轮次 1
