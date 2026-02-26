param(
    [string]$Runtime = "win-x64",
    [switch]$FrameworkDependent,
    [switch]$NoSingleFile
)

$proj = "src/SimulatorApp"
$out = "artifacts\SimulatorAppPublish"

Write-Host "Publishing SimulatorApp to $out"

$args = @($proj, "-c", "Release", "-r", $Runtime, "-o", $out)

if ($FrameworkDependent) {
    $args += "--self-contained"
    $args += "false"
} else {
    $args += "--self-contained"
    $args += "true"
}

if (-not $NoSingleFile) {
    $args += "/p:PublishSingleFile=true"
}

dotnet publish @args

if ($LASTEXITCODE -ne 0) {
    Write-Error "publish failed"
    exit $LASTEXITCODE
}

Write-Host "Publish complete: $out"
