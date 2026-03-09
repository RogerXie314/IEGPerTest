# 发布 SimulatorApp 为单个 exe 文件（自包含 + Brotli 压缩，约 73 MB）
# 每次发布自动递增 patch 版本号并 git commit，无需手动维护版本
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath  = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

# ── 1. 自动 bump patch 版本号 ──────────────────────────────────────────────
[xml]$csproj = Get-Content $projectPath
$oldVer = $csproj.Project.PropertyGroup.Version
if ($oldVer -match '^(\d+)\.(\d+)\.(\d+)$') {
    $newVer = "$($matches[1]).$($matches[2]).$([int]$matches[3] + 1)"
} else {
    Write-Warning "无法解析版本号 '$oldVer'，跳过自动 bump"
    $newVer = $oldVer
}
$csproj.Project.PropertyGroup.Version        = $newVer
$csproj.Project.PropertyGroup.AssemblyVersion = "$newVer.0"
$csproj.Project.PropertyGroup.FileVersion    = "$newVer.0"
$csproj.Save((Resolve-Path $projectPath))
Write-Host "版本号：$oldVer → $newVer" -ForegroundColor Cyan

# ── 2. 发布 ──────────────────────────────────────────────────────────────
Write-Host "正在发布 SimulatorApp v$newVer (单文件压缩模式)..." -ForegroundColor Cyan

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
Write-Host "发布成功！$outputPath\SimulatorApp.exe  $([math]::Round($exeSize, 1)) MB" -ForegroundColor Green

# ── 3. 自动更新项目实施文档版本号 ─────────────────────────────────────────
$docPath = Get-ChildItem "$PSScriptRoot\..\docs\" -Filter "*.md" |
           Where-Object { $_.Name -match "\u5b9e\u65bd" } |
           Select-Object -ExpandProperty FullName
$today   = (Get-Date).ToString("yyyy-MM-dd")
if ($docPath) {
    $docContent = [System.IO.File]::ReadAllText($docPath, [System.Text.Encoding]::UTF8)
    $docContent = $docContent -replace '(?<=\| 当前版本 \| \*\*v)[\d.]+(?=\*\*)', $newVer
    $docContent = $docContent -replace '(?<=\| 上次更新 \| )[\d-]+', $today
    [System.IO.File]::WriteAllText($docPath, $docContent, [System.Text.Encoding]::UTF8)
    Write-Host "文档版本号已更新：v$newVer  $today" -ForegroundColor Cyan
} else {
    Write-Warning "未找到项目实施文档，跳过文档更新"
}

# ── 4. 自动 git commit + push csproj + 文档版本号变更 ────────────────────
Push-Location "$PSScriptRoot\.."
$docRelPath = git ls-files --others --cached --modified docs/ | Where-Object { $_ -match "\u5b9e\u65bd" }
git add src/SimulatorApp/SimulatorApp.csproj
if ($docRelPath) { git add $docRelPath }
git commit -m "chore: bump SimulatorApp to v$newVer"
git push
Pop-Location
Write-Host "版本号已提交并推送：v$newVer" -ForegroundColor Green
