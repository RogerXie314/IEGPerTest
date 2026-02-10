param(
    [string]$Runtime = "win-x64",
    [switch]$SelfContained,
    [switch]$SingleFile
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$common = @()
$common += "-Runtime"; $common += $Runtime
if ($SelfContained) { $common += "-SelfContained" }
if ($SingleFile) { $common += "-SingleFile" }

Write-Host "Publishing all apps (Runtime=$Runtime SelfContained=$SelfContained SingleFile=$SingleFile)"

& (Join-Path $root "publish_devrunner.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_simulatorapp.ps1") @common
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_simulatorrunner.ps1") @common
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $root "publish_testreceiver.ps1") @common
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "All publish complete."
