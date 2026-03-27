# Publish SimulatorApp as a single self-contained exe (Brotli compressed)
# NativeEngine.dll + NativeSender.dll 全部打包进同一个 EXE，发布物只有一个文件。
# Automatically bumps patch version, updates docs, and git commits on each run.
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath  = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

# -- 0. Pre-flight: working tree must be clean --------------------------------
# 正确发布流程：功能代码改完 → 写变更记录 → git commit → 再跑此脚本。
# 此检查确保"写记录+commit"步骤不被跳过。
Push-Location "$PSScriptRoot\.."
$gitStatus = git status --porcelain 2>&1
Pop-Location
if ($gitStatus) {
    Write-Error @"
发布中止：工作区有未提交的改动：

$gitStatus

正确流程：
  1. 功能代码改完
  2. 在 docs/项目实施文档.md 写好变更记录
  3. git add . && git commit
  4. 再运行此脚本
"@
    exit 1
}

# -- 1. Auto-bump patch version -----------------------------------------------
[xml]$csproj = Get-Content $projectPath
$oldVer = $csproj.Project.PropertyGroup.Version
if ($oldVer -match '^(\d+)\.(\d+)\.(\d+)$') {
    $newVer = "$($matches[1]).$($matches[2]).$([int]$matches[3] + 1)"
} else {
    Write-Warning "Cannot parse version '$oldVer', skipping bump"
    $newVer = $oldVer
}

# -- 1b. Pre-flight: changelog entry must exist for the new version ----------
# 发布前必须在 docs/项目实施文档.md 里写好 "### v{新版本}" 条目，否则中止。
$docFileName2 = [char]0x9879 + [char]0x76EE + [char]0x5B9E + [char]0x65BD + [char]0x6587 + [char]0x6863 + ".md"
$docPath2 = [System.IO.Path]::Combine((Resolve-Path "$PSScriptRoot\..").Path, "docs", $docFileName2)
if ([System.IO.File]::Exists($docPath2)) {
    $docLines = [System.IO.File]::ReadAllLines($docPath2, [System.Text.Encoding]::UTF8)
    $hasEntry = $docLines | Where-Object { $_ -match "^###\s+v$([regex]::Escape($newVer))\b" }
    if (-not $hasEntry) {
        Write-Error @"
发布中止：docs/项目实施文档.md 中缺少 v$newVer 的变更记录。

请先在变更记录区域添加：

  ### v$newVer — $(Get-Date -Format 'yyyy-MM-dd')：<本次改动标题>

  <改动说明>

然后 git add docs/ && git commit，再运行此脚本。
"@
        exit 1
    }
} else {
    Write-Warning "Doc not found, skipping changelog check"
}
$csproj.Project.PropertyGroup.Version        = $newVer
$csproj.Project.PropertyGroup.AssemblyVersion = "$newVer.0"
$csproj.Project.PropertyGroup.FileVersion    = "$newVer.0"
$csproj.Save((Resolve-Path $projectPath))
Write-Host "Version: $oldVer -> $newVer" -ForegroundColor Cyan

# -- 2. 先编译 C++ DLL（dotnet publish 打包时需要它们已存在）-----------------
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# -- 2a. NativeEngine.dll -------------------------------------------------------
$neDir = "$PSScriptRoot\..\src\NativeEngine"
if (Test-Path $cmake) {
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

# -- 2b. NativeSender.dll -------------------------------------------------------
# CMakeLists.txt 输出到 build/Release；CMakeCache.txt 存在时强制重新配置以确保路径正确。
$nsDir = "$PSScriptRoot\..\src\NativeSender"
if (Test-Path $cmake) {
    Write-Host "Building NativeSender.dll..." -ForegroundColor Cyan
    if (-not (Test-Path "$nsDir\build")) { New-Item -ItemType Directory "$nsDir\build" | Out-Null }
    # 每次重新配置确保 CMakeCache 中的输出路径与 CMakeLists.txt 一致
    Push-Location "$nsDir\build"
    & $cmake -G "Visual Studio 17 2022" -A x64 .. | Out-Null
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

# -- 2c. RawPacketEngine.dll -------------------------------------------------------
$rpeDir = "$PSScriptRoot\..\src\RawPacketEngine"
if (Test-Path $cmake) {
    Write-Host "Building RawPacketEngine.dll..." -ForegroundColor Cyan
    if (-not (Test-Path "$rpeDir\build")) { New-Item -ItemType Directory "$rpeDir\build" | Out-Null }
    Push-Location "$rpeDir\build"
    & $cmake -G "Visual Studio 17 2022" -A x64 .. | Out-Null
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

# -- 3. Publish（dotnet publish 会将两个 DLL 打包进单文件 EXE）----------------
Write-Host "Publishing SimulatorApp v$newVer (single-file, DLLs embedded)..." -ForegroundColor Cyan

dotnet publish $projectPath `
    -c Release `
    -r win-x64 `
    --self-contained true `
    /p:PublishSingleFile=true `
    /p:IncludeNativeLibrariesForSelfExtract=true `
    /p:EnableCompressionInSingleFile=true `
    -o $outputPath

if ($LASTEXITCODE -ne 0) {
    Write-Error "Publish failed"
    exit $LASTEXITCODE
}

# 验证：输出目录只应有 SimulatorApp.exe 一个文件
$publishedFiles = Get-ChildItem $outputPath | Select-Object -ExpandProperty Name
Write-Host "Published files: $($publishedFiles -join ', ')" -ForegroundColor Cyan
if ($publishedFiles -contains "NativeEngine.dll" -or $publishedFiles -contains "NativeSender.dll" -or $publishedFiles -contains "RawPacketEngine.dll") {
    Write-Warning "DLL still in output dir -- csproj None item may be misconfigured"
}

$exeSize = (Get-Item "$outputPath\SimulatorApp.exe").Length / 1MB
Write-Host "Published: $outputPath\SimulatorApp.exe  $([math]::Round($exeSize, 1)) MB" -ForegroundColor Green

# -- 4. Update docs version number -------------------------------------------
# Construct Chinese filename via [char] codes to avoid any console encoding issues:
# docs/项目实施文档.md
# 项=9879 目=76EE 实=5B9E 施=65BD 文=6587 档=6863
$docFileName = [char]0x9879 + [char]0x76EE + [char]0x5B9E + [char]0x65BD + [char]0x6587 + [char]0x6863 + ".md"
$docPath = [System.IO.Path]::Combine((Resolve-Path "$PSScriptRoot\..").Path, "docs", $docFileName)
$today = (Get-Date).ToString("yyyy-MM-dd")
if ([System.IO.File]::Exists($docPath)) {
    $docContent = [System.IO.File]::ReadAllText($docPath, [System.Text.Encoding]::UTF8)
    $pat1 = "(?<=\| " + [char]0x5F53 + [char]0x524D + [char]0x7248 + [char]0x672C + " \| \*\*v)[\d.]+(?=\*\*)"
    $pat2 = "(?<=\| " + [char]0x4E0A + [char]0x6B21 + [char]0x66F4 + [char]0x65B0 + " \| )[\d-]+"
    $docContent = $docContent -replace $pat1, $newVer
    $docContent = $docContent -replace $pat2, $today
    [System.IO.File]::WriteAllText($docPath, $docContent, [System.Text.Encoding]::UTF8)
    Write-Host "Docs updated: v$newVer  $today" -ForegroundColor Cyan
} else {
    Write-Warning "Doc not found at $docPath, skipping"
}

# -- 5. Git commit + push -----------------------------------------------------
Push-Location "$PSScriptRoot\.."
git add src/SimulatorApp/SimulatorApp.csproj
git add docs/
git commit -m "chore: bump SimulatorApp to v$newVer"
git push
Pop-Location
Write-Host "Committed and pushed: v$newVer" -ForegroundColor Green
