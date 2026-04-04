# Publish SimulatorApp as a self-contained exe with DLLs in same directory
# NativeEngine.dll + NativeSender.dll + RawPacketEngine.dll 与 EXE 同目录部署。
# 版本号需手动修改 SimulatorApp.csproj，脚本不再自动递增。
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath  = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

# -- 0. 读取当前版本号（不再检查工作区状态，允许在任意环境打包）-----------
[xml]$csproj = Get-Content $projectPath
$currentVer = $csproj.Project.PropertyGroup.Version
Write-Host "当前版本: v$currentVer" -ForegroundColor Cyan

# 不再自动递增版本号，版本号由开发者手动修改 SimulatorApp.csproj$'
# 不再检查文档中是否有版本记录（改为可选警告）
$docFileName2 = [char]0x9879 + [char]0x76EE + [char]0x5B9E + [char]0x65BD + [char]0x6587 + [char]0x6863 + ".md"
$docPath2 = [System.IO.Path]::Combine((Resolve-Path "$PSScriptRoot\..").Path, "docs", $docFileName2)
if ([System.IO.File]::Exists($docPath2)) {
    $docLines = [System.IO.File]::ReadAllLines($docPath2, [System.Text.Encoding]::UTF8)
    $hasEntry = $docLines | Where-Object { $_ -match "^###\s+v$([regex]::Escape($currentVer))\b" }
    if (-not $hasEntry) {
        Write-Warning @"
提示：docs/项目实施文档.md 中未找到 v$currentVer 的变更记录。
建议在发布后补充变更说明。
"@
    }
} else {
    Write-Warning "文档文件未找到，跳过版本记录检查"
}

# -- 1. 先编译 C++ DLL（dotnet publish 打包时需要它们已存在）-----------------
# 支持多个 Visual Studio 版本和路径
$cmakePaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

$cmake = $null
foreach ($path in $cmakePaths) {
    if (Test-Path $path) {
        $cmake = $path
        Write-Host "找到 CMake: $cmake" -ForegroundColor Green
        break
    }
}

if (-not $cmake) {
    Write-Warning "未找到 CMake，将跳过 C++ DLL 编译（使用已存在的 DLL）"
}

# -- 1a. NativeEngine.dll -------------------------------------------------------
$neDir = "$PSScriptRoot\..\src\NativeEngine"
if ($cmake -and (Test-Path $cmake)) {
    Write-Host "Building NativeEngine.dll..." -ForegroundColor Cyan
    Push-Location "$neDir\build"
    & $cmake --build . --config Release | Out-Null
    Pop-Location
} else {
    Write-Warning "CMake not found; skipping NativeEngine rebuild (using existing dll if present)"
}
$neDll = "$neDir\build\Release\NativeEngine.dll"
if (-not (Test-Path $neDll)) {
    Write-Error "NativeEngine.dll not found — publish aborted"
    exit 1
}
Write-Host "NativeEngine.dll ready: $([math]::Round((Get-Item $neDll).Length/1KB, 1)) KB" -ForegroundColor Cyan

# -- 1b. NativeSender.dll -------------------------------------------------------
# CMakeLists.txt 输出到 build/Release；CMakeCache.txt 存在时强制重新配置以确保路径正确。
$nsDir = "$PSScriptRoot\..\src\NativeSender"
if ($cmake -and (Test-Path $cmake)) {
    Write-Host "Building NativeSender.dll..." -ForegroundColor Cyan
    if (-not (Test-Path "$nsDir\build")) { New-Item -ItemType Directory "$nsDir\build" | Out-Null }
    # 每次重新配置确保 CMakeCache 中的输出路径与 CMakeLists.txt 一致
    Push-Location "$nsDir\build"
    & $cmake -G "Visual Studio 16 2019" -A x64 .. | Out-Null
    & $cmake --build . --config Release | Out-Null
    Pop-Location
} else {
    Write-Warning "CMake not found; skipping NativeSender rebuild (using existing dll if present)"
}
$nsDll = "$nsDir\build\Release\NativeSender.dll"
if (-not (Test-Path $nsDll)) {
    Write-Error "NativeSender.dll not found — publish aborted"
    exit 1
}
Write-Host "NativeSender.dll ready: $([math]::Round((Get-Item $nsDll).Length/1KB, 1)) KB" -ForegroundColor Cyan

# -- 1c. RawPacketEngine.dll -------------------------------------------------------
$rpeDir = "$PSScriptRoot\..\src\RawPacketEngine"
if ($cmake -and (Test-Path $cmake)) {
    Write-Host "Building RawPacketEngine.dll..." -ForegroundColor Cyan
    if (-not (Test-Path "$rpeDir\build")) { New-Item -ItemType Directory "$rpeDir\build" | Out-Null }
    Push-Location "$rpeDir\build"
    & $cmake -G "Visual Studio 16 2019" -A x64 .. | Out-Null
    & $cmake --build . --config Release | Out-Null
    Pop-Location
} else {
    Write-Warning "CMake not found; skipping RawPacketEngine rebuild (using existing dll if present)"
}
$rpeDll = "$rpeDir\build\Release\RawPacketEngine.dll"
if (-not (Test-Path $rpeDll)) {
    Write-Error "RawPacketEngine.dll not found — publish aborted"
    exit 1
}
Write-Host "RawPacketEngine.dll ready: $([math]::Round((Get-Item $rpeDll).Length/1KB, 1)) KB" -ForegroundColor Cyan

# -- 2. Publish（DLL 与 EXE 同目录部署）----------------------------------------
Write-Host "Publishing SimulatorApp v$currentVer (self-contained, DLLs in same directory)..." -ForegroundColor Cyan

dotnet publish $projectPath `
    -c Release `
    -r win-x64 `
    --self-contained true `
    /p:PublishSingleFile=true `
    /p:EnableCompressionInSingleFile=true `
    -o $outputPath
# ^^^ EnableCompressionInSingleFile=true: Brotli压缩托管程序集，EXE体积约70MB。
# ^^^ 安全前提：C++ DLL(NativeEngine/NativeSender/RawPacketEngine)与EXE同目录，
# ^^^           不使用IncludeNativeLibrariesForSelfExtract，无temp目录提取，不会导致WPF崩溃。
# ^^^ 禁止删除此参数。如需移除，必须在提交信息中说明原因。

if ($LASTEXITCODE -ne 0) {
    Write-Error "Publish failed"
    exit $LASTEXITCODE
}

# -- 3. 手动复制 C++ DLL（PublishSingleFile 不会自动打包 None 项）--------------
Copy-Item $neDll  "$outputPath\NativeEngine.dll"  -Force
Copy-Item $nsDll  "$outputPath\NativeSender.dll"  -Force
Copy-Item $rpeDll "$outputPath\RawPacketEngine.dll" -Force
Write-Host "Copied C++ DLLs to $outputPath" -ForegroundColor Cyan

# 验证：输出目录应包含 SimulatorApp.exe 和 3 个 DLL
$publishedFiles = Get-ChildItem $outputPath | Select-Object -ExpandProperty Name
Write-Host "Published files: $($publishedFiles -join ', ')" -ForegroundColor Cyan

$requiredDlls = @("NativeEngine.dll", "NativeSender.dll", "RawPacketEngine.dll")
$missingDlls = $requiredDlls | Where-Object { $publishedFiles -notcontains $_ }
if ($missingDlls) {
    Write-Error "Missing required DLLs: $($missingDlls -join ', ')"
    exit 1
}

$exeSize = (Get-Item "$outputPath\SimulatorApp.exe").Length / 1MB
$totalSize = (Get-ChildItem $outputPath -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host "Published: $outputPath\SimulatorApp.exe  $([math]::Round($exeSize, 1)) MB (total: $([math]::Round($totalSize, 1)) MB)" -ForegroundColor Green

Write-Host "`n打包完成！版本: v$currentVer" -ForegroundColor Green
Write-Host "输出目录: $outputPath" -ForegroundColor Cyan
Write-Host "`n提示：版本号和文档更新需手动管理，不再自动提交。" -ForegroundColor Yellow
