param(
    [switch]$RunBackground
)

Write-Host "开始构建..."
$build = dotnet build
if ($LASTEXITCODE -ne 0) { Write-Error "构建失败"; exit $LASTEXITCODE }

if ($RunBackground) {
    Write-Host "后台运行 SimulatorRunner..."
    Start-Process -FilePath "dotnet" -ArgumentList "run --project src/SimulatorRunner" -NoNewWindow
} else {
    Write-Host "运行 SimulatorRunner..."
    dotnet run --project src/SimulatorRunner
}
