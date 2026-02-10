# 发布 SimulatorApp 为单个 exe 文件
$projectPath = "$PSScriptRoot\..\src\SimulatorApp\SimulatorApp.csproj"
$outputPath = "$PSScriptRoot\..\artifacts\SimulatorAppPublish"

Write-Host "正在发布 SimulatorApp (单文件模式)..." -ForegroundColor Cyan

dotnet publish $projectPath `
    -c Release `
    -r win-x64 `
    --self-contained true `
    /p:PublishSingleFile=true `
    /p:IncludeNativeLibrariesForSelfExtract=true `
    /p:EnableCompressionInSingleFile=true `
    -o $outputPath

if ($LASTEXITCODE -eq 0) {
    Write-Host "发布成功！输出目录: $outputPath" -ForegroundColor Green
    $exeSize = (Get-Item "$outputPath\SimulatorApp.exe").Length / 1MB
    Write-Host "SimulatorApp.exe 大小: $([math]::Round($exeSize, 2)) MB" -ForegroundColor Yellow
} else {
    Write-Host "发布失败！" -ForegroundColor Red
}
