# NOTE: For normal releases, use publish_simulatorapp.ps1 instead.
# This script rebuilds ALL 4 packages and should only be run when
# SimulatorRunner / TestReceiver / DevRunner source code has changed.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "Publishing all apps..."

# -- DevRunner ----------------------------------------------------------------
Write-Host "Publishing DevRunner..." -ForegroundColor Cyan
dotnet publish (Join-Path $root "..\src\DevTools") -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true -o (Join-Path $root "..\artifacts\DevRunner")
if ($LASTEXITCODE -ne 0) { Write-Error "DevRunner publish failed"; exit $LASTEXITCODE }

# -- SimulatorRunner ----------------------------------------------------------
Write-Host "Publishing SimulatorRunner..." -ForegroundColor Cyan
dotnet publish (Join-Path $root "..\src\SimulatorRunner") -c Release -r win-x64 --self-contained false -o (Join-Path $root "..\artifacts\SimulatorRunnerPublish")
if ($LASTEXITCODE -ne 0) { Write-Error "SimulatorRunner publish failed"; exit $LASTEXITCODE }

# -- TestReceiver -------------------------------------------------------------
Write-Host "Publishing TestReceiver..." -ForegroundColor Cyan
dotnet publish (Join-Path $root "..\src\TestReceiver") -c Release -r win-x64 --self-contained false -o (Join-Path $root "..\artifacts\TestReceiverPublish")
if ($LASTEXITCODE -ne 0) { Write-Error "TestReceiver publish failed"; exit $LASTEXITCODE }

# -- SimulatorApp （含版本 bump + git commit/push）----------------------------
& (Join-Path $root "publish_simulatorapp.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "All done." -ForegroundColor Green

Write-Host "All publish complete." -ForegroundColor Green
