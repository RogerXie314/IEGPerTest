# 发布 SimulatorApp 为单个 exe 文件（自包含 + Brotli 压缩，约 73 MB）
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath  = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

Write-Host "正在发布 SimulatorApp (单文件压缩模式)..." -ForegroundColor Cyan

dotnet publish $projectPath `
    -c Release `
    -r win-x64 `
    --self-contained true `
    /p:PublishSingleFile=true `
    /p:IncludeNativeLibrariesForSelfExtract=true `
    /p:EnableCompressionInSingleFile=true `
    -o $outputPath

if ($LASTEXITCODE -eq 0) {
    $exeSize = (Get-Item "$outputPath\SimulatorApp.exe").Length / 1MB
    Write-Host "发布成功！$outputPath\SimulatorApp.exe  $([math]::Round($exeSize, 1)) MB" -ForegroundColor Green
} else {
    Write-Error "Publish failed"
    exit $LASTEXITCODE
}
