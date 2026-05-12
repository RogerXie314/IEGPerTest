# Export Minimal Build Dependencies
# Run this script in D:\Development\IEGPerTest to export only required files

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Export Minimal Build Dependencies" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$sourceBase = "D:\Development\IEGPerTest\external\IEG_Code\code"
$exportDir = "D:\Development\IEGPerTest\dependencies_export"

# Clean export directory if exists
if (Test-Path $exportDir) {
    Write-Host "Cleaning existing export directory..." -ForegroundColor Yellow
    Remove-Item -Path $exportDir -Recurse -Force
}

# Create export directory structure
Write-Host "Creating export directory structure..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path "$exportDir\include\zlib" -Force | Out-Null
New-Item -ItemType Directory -Path "$exportDir\lib\x64" -Force | Out-Null
Write-Host ""

# Copy zlib headers
Write-Host "[1/2] Copying zlib headers..." -ForegroundColor Cyan
$zlibInclude = "$sourceBase\include\zlib"
if (Test-Path $zlibInclude) {
    Copy-Item -Path "$zlibInclude\*" -Destination "$exportDir\include\zlib\" -Recurse -Force
    $headerCount = (Get-ChildItem "$exportDir\include\zlib" -File).Count
    $headerSize = [math]::Round((Get-ChildItem "$exportDir\include\zlib" -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1KB, 1)
    Write-Host "  [OK] Copied $headerCount header files ($headerSize KB)" -ForegroundColor Green
} else {
    Write-Host "  [ERROR] Source not found: $zlibInclude" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Copy zlib static library
Write-Host "[2/2] Copying zlib static library..." -ForegroundColor Cyan
$zlibLib = "$sourceBase\lib\x64\zlibstat.lib"
if (Test-Path $zlibLib) {
    Copy-Item -Path $zlibLib -Destination "$exportDir\lib\x64\" -Force
    $libSize = [math]::Round((Get-Item "$exportDir\lib\x64\zlibstat.lib").Length / 1KB, 1)
    Write-Host "  [OK] Copied zlibstat.lib ($libSize KB)" -ForegroundColor Green
} else {
    Write-Host "  [ERROR] Source not found: $zlibLib" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Summary
$totalSize = [math]::Round((Get-ChildItem $exportDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1KB, 1)
$totalFiles = (Get-ChildItem $exportDir -Recurse -File).Count

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Export Complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Exported to: $exportDir" -ForegroundColor White
Write-Host "Total files: $totalFiles" -ForegroundColor White
Write-Host "Total size:  $totalSize KB" -ForegroundColor White
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Compress the 'dependencies_export' folder" -ForegroundColor White
Write-Host "  2. Transfer to Github environment (d:\Github\IEGPerTest)" -ForegroundColor White
Write-Host "  3. Extract to: d:\Github\IEGPerTest\external\IEG_Code\code\" -ForegroundColor White
Write-Host ""
