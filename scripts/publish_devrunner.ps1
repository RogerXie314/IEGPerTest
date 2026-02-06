param()

$proj = "src/DevTools"
$out = "artifacts\DevRunner"

Write-Host "Publishing DevRunner self-contained exe to $out"

dotnet publish $proj -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true -o $out

if ($LASTEXITCODE -ne 0) {
    Write-Error "publish failed"
    exit $LASTEXITCODE
}

Write-Host "Publish complete: $out"
