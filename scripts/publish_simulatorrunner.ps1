param(
    [string]$Runtime = "win-x64",
    [switch]$SelfContained,
    [switch]$SingleFile
)

$proj = "src/SimulatorRunner"
$out = "artifacts\SimulatorRunnerPublish"

Write-Host "Publishing SimulatorRunner to $out"

$args = @($proj, "-c", "Release", "-r", $Runtime, "-o", $out)

if ($SelfContained) {
    $args += "--self-contained"
    $args += "true"
} else {
    $args += "--self-contained"
    $args += "false"
}

if ($SingleFile) {
    $args += "/p:PublishSingleFile=true"
}

dotnet publish @args

if ($LASTEXITCODE -ne 0) {
    Write-Error "publish failed"
    exit $LASTEXITCODE
}

Write-Host "Publish complete: $out"
