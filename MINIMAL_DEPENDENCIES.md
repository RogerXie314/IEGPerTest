# 编译 DLL 所需的最小依赖文件清单

## 📦 需要从 D:\Development\IEGPerTest 拷贝的文件

### 1. zlib 头文件（约 100 KB）
```
源路径：D:\Development\IEGPerTest\external\IEG_Code\code\include\zlib\
目标路径：d:\Github\IEGPerTest\external\IEG_Code\code\include\zlib\

包含文件：
- zlib.h
- zconf.h
- 以及该目录下所有 .h 文件
```

### 2. zlib 静态库（约 222 KB）
```
源文件：D:\Development\IEGPerTest\external\IEG_Code\code\lib\x64\zlibstat.lib
目标文件：d:\Github\IEGPerTest\external\IEG_Code\code\lib\x64\zlibstat.lib
```

---

## 📊 文件大小估算

| 项目 | 大小 | 用途 |
|------|------|------|
| zlib 头文件目录 | ~100 KB | NativeEngine.dll 编译 |
| zlibstat.lib | ~222 KB | NativeEngine.dll 链接 |
| **总计** | **~322 KB** | |

---

## 🚀 拷贝步骤

### 方式 1：手动拷贝

1. 在 `d:\Github\IEGPerTest` 创建目录结构：
   ```powershell
   mkdir external\IEG_Code\code\include\zlib -Force
   mkdir external\IEG_Code\code\lib\x64 -Force
   ```

2. 从 `D:\Development\IEGPerTest` 拷贝文件：
   - 拷贝整个 `external\IEG_Code\code\include\zlib\` 目录
   - 拷贝 `external\IEG_Code\code\lib\x64\zlibstat.lib` 文件

### 方式 2：使用脚本（在 D:\Development 环境运行）

创建 `export_dependencies.ps1`：
```powershell
# 在 D:\Development\IEGPerTest 运行此脚本
$sourceBase = "D:\Development\IEGPerTest\external\IEG_Code\code"
$exportDir = "D:\Development\IEGPerTest\dependencies_export"

# 创建导出目录
New-Item -ItemType Directory -Path "$exportDir\include\zlib" -Force
New-Item -ItemType Directory -Path "$exportDir\lib\x64" -Force

# 拷贝 zlib 头文件
Copy-Item -Path "$sourceBase\include\zlib\*" -Destination "$exportDir\include\zlib\" -Recurse -Force

# 拷贝 zlib 静态库
Copy-Item -Path "$sourceBase\lib\x64\zlibstat.lib" -Destination "$exportDir\lib\x64\" -Force

Write-Host "Dependencies exported to: $exportDir"
Write-Host "Total size: $([math]::Round((Get-ChildItem $exportDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1KB, 1)) KB"
```

然后压缩 `dependencies_export` 目录（约 300 KB），传输到 Github 环境。

### 方式 3：通过网络共享/U盘

如果两个环境在同一网络或可以用 U盘：
- 只需拷贝约 322 KB 的文件
- 比 2.05 GB 小 6000 多倍

---

## ✅ 验证拷贝结果

在 `d:\Github\IEGPerTest` 运行：
```powershell
# 检查 zlib 头文件
Test-Path external\IEG_Code\code\include\zlib\zlib.h

# 检查 zlib 静态库
Test-Path external\IEG_Code\code\lib\x64\zlibstat.lib

# 查看文件大小
Get-Item external\IEG_Code\code\lib\x64\zlibstat.lib | Select-Object Length
```

都应该返回 True 和正确的文件大小。

---

## 📝 其他依赖说明

### NativeSender.dll
- **无需额外依赖**，只需要 Windows SDK（ws2_32.lib）

### RawPacketEngine.dll
- **需要 Npcap SDK**（约 5 MB）
- 下载地址：https://npcap.com/#download
- 解压到：`C:\npcap-sdk`
- 这个需要在 Github 环境单独下载安装

---

## 🔨 拷贝完成后的编译步骤

```powershell
# 1. 构建 NativeEngine.dll
cd src\NativeEngine
build.bat

# 2. 构建 NativeSender.dll
cd ..\NativeSender
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release

# 3. 构建 RawPacketEngine.dll（需要先安装 Npcap SDK）
cd ..\..\RawPacketEngine
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release

# 4. 打包发布
cd ..\..\..\
powershell -ExecutionPolicy Bypass -File .\scripts\publish_simulatorapp.ps1
```
