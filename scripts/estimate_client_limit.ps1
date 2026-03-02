# estimate_client_limit.ps1
# Estimate max stable client count for short-connection mode
# Usage: .\estimate_client_limit.ps1 [-RatePerClient 2]

param(
    [int]$RatePerClient = 2
)

# 1. Dynamic port range
$dynInfo = netsh int ipv4 show dynamicport tcp
$portCount = 16384  # default
foreach ($line in $dynInfo) {
    if ($line -match '(\d{4,})') {
        $n = [int]$matches[1]
        if ($n -gt 1000 -and $n -le 65536) { $portCount = $n }
    }
}

# 2. TcpTimedWaitDelay from registry
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters"
$regDelay = (Get-ItemProperty $regPath -Name TcpTimedWaitDelay -ErrorAction SilentlyContinue).TcpTimedWaitDelay
if ($regDelay) {
    $twDelayReg = [int]$regDelay
    $twSource = "registry"
} else {
    $twDelayReg = 240
    $twSource = "default"
}

# 3. Current TIME_WAIT count
$twCount = (netstat -n 2>$null | Select-String "TIME_WAIT").Count

Write-Host ""
Write-Host "========================================"
Write-Host "  TCP Short-Connection Capacity Report"
Write-Host "========================================"
Write-Host ""
Write-Host "[System Parameters]"
Write-Host "  Dynamic port count  : $portCount"
Write-Host "  TIME_WAIT timeout   : $twDelayReg s ($twSource)"
Write-Host "  Current TIME_WAIT   : $twCount"
Write-Host "  Rate per client     : $RatePerClient conn/s"
Write-Host ""

# 4. Theory limit based on registry
$maxTheory = [math]::Floor($portCount * 0.9 / ($twDelayReg * $RatePerClient))
Write-Host "[Theory Limit  (registry value)]"
Write-Host "  floor($portCount x 0.9 / ($twDelayReg x $RatePerClient)) = $maxTheory clients"
Write-Host ""

# 5. Measured estimate (if TIME_WAIT exists)
if ($twCount -gt 100) {
    Write-Host "[Measured Estimate  (current TIME_WAIT=$twCount)]"
    foreach ($c in @(10, 20, 30, 40, 50, 60)) {
        $twEst = $twCount / ($c * $RatePerClient)
        $maxEst = [math]::Floor($portCount * 0.9 / ($twEst * $RatePerClient))
        Write-Host ("  If {0,3} clients running -> actual TW ~{1,4:0}s -> ceiling = {2} clients" -f $c, $twEst, $maxEst)
    }
    Write-Host ""
}

# 6. Tuning options
$twOpt = 30
$maxA = [math]::Floor($portCount * 0.9 / ($twOpt * $RatePerClient))
$portOpt = 64510
$maxB = [math]::Floor($portOpt * 0.9 / ($twDelayReg * $RatePerClient))
$maxAB = [math]::Floor($portOpt * 0.9 / ($twOpt * $RatePerClient))

Write-Host "[Tuning Potential  (requires admin + reboot)]"
Write-Host "  A: TcpTimedWaitDelay=30s only          -> $maxA clients"
Write-Host "  B: Expand ports to 64510 only          -> $maxB clients"
Write-Host "  A+B: Both changes                      -> $maxAB clients"
Write-Host ""
Write-Host "  Commands:"
Write-Host '  reg add "HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" /v TcpTimedWaitDelay /t REG_DWORD /d 30 /f'
Write-Host "  netsh int ipv4 set dynamicport tcp start=1025 num=64510"
Write-Host ""
Write-Host "========================================"
