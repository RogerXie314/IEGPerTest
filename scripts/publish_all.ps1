# NOTE: For normal releases, use publish_simulatorapp.ps1 instead.
# This script rebuilds ALL 4 packages and should only be run when
# SimulatorRunner / TestReceiver / DevRunner source code has changed.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "Publishing all apps..."

& (Join-Path $root "publish_devrunner.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_simulatorapp.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_simulatorrunner.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_testreceiver.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "All publish complete." -ForegroundColor Green
