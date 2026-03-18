# Publish SimulatorApp as a single self-contained exe (Brotli compressed, ~73 MB)
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
$csproj.Project.PropertyGroup.Version        = $newVer
$csproj.Project.PropertyGroup.AssemblyVersion = "$newVer.0"
$csproj.Project.PropertyGroup.FileVersion    = "$newVer.0"
$csproj.Save((Resolve-Path $projectPath))
Write-Host "Version: $oldVer -> $newVer" -ForegroundColor Cyan

# -- 2. Publish ---------------------------------------------------------------
Write-Host "Publishing SimulatorApp v$newVer (single-file compressed)..." -ForegroundColor Cyan

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

$exeSize = (Get-Item "$outputPath\SimulatorApp.exe").Length / 1MB
Write-Host "Published: $outputPath\SimulatorApp.exe  $([math]::Round($exeSize, 1)) MB" -ForegroundColor Green

# -- 3. Update docs version number -------------------------------------------
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

# -- 4. Git commit + push -----------------------------------------------------
Push-Location "$PSScriptRoot\.."
git add src/SimulatorApp/SimulatorApp.csproj
git add docs/
git commit -m "chore: bump SimulatorApp to v$newVer"
git push
Pop-Location
Write-Host "Committed and pushed: v$newVer" -ForegroundColor Green
