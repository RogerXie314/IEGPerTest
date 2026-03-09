# Publish SimulatorApp as a single self-contained exe (Brotli compressed, ~73 MB)
# Automatically bumps patch version, updates docs, and git commits on each run.
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath  = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

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
# Use git ls-files to get doc path from index, avoiding Chinese literals in script
Push-Location "$PSScriptRoot\.."
$docRelPath = git ls-files docs/*.md | Where-Object { $_ -match 'docs/' } | Select-Object -First 1
Pop-Location
$docPath = $null
if ($docRelPath) {
    $docPath = Join-Path (Resolve-Path "$PSScriptRoot\..") $docRelPath.Replace('/', '\')
}
$today = (Get-Date).ToString("yyyy-MM-dd")
if ($docPath -and (Test-Path $docPath)) {
    $docContent = [System.IO.File]::ReadAllText($docPath, [System.Text.Encoding]::UTF8)
    $pat1 = "(?<=\| " + [char]0x5F53 + [char]0x524D + [char]0x7248 + [char]0x672C + " \| \*\*v)[\d.]+(?=\*\*)"
    $pat2 = "(?<=\| " + [char]0x4E0A + [char]0x6B21 + [char]0x66F4 + [char]0x65B0 + " \| )[\d-]+"
    $docContent = $docContent -replace $pat1, $newVer
    $docContent = $docContent -replace $pat2, $today
    [System.IO.File]::WriteAllText($docPath, $docContent, [System.Text.Encoding]::UTF8)
    Write-Host "Docs updated: v$newVer  $today" -ForegroundColor Cyan
} else {
    Write-Warning "No docs/*.md found, skipping doc update"
}

# -- 4. Git commit + push -----------------------------------------------------
Push-Location "$PSScriptRoot\.."
git add src/SimulatorApp/SimulatorApp.csproj
if ($docRelPath) { git add $docRelPath }
git commit -m "chore: bump SimulatorApp to v$newVer"
git push
Pop-Location
Write-Host "Committed and pushed: v$newVer" -ForegroundColor Green
