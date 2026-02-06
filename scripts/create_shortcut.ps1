param(
    [string]$TargetExe,
    [switch]$CreateInRepoTools
)

# 作用：在当前用户桌面创建指向 DevRunner 的快捷方式，可选在仓库 tools 目录也创建一份
try {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $repoRoot = Resolve-Path (Join-Path $scriptDir "..")
    if (-not $TargetExe) {
        $TargetExe = Join-Path $repoRoot "artifacts\DevRunner\DevRunner.exe"
    }

    if (-not (Test-Path $TargetExe)) {
        Write-Error "目标可执行不存在: $TargetExe"
        exit 2
    }

    $desktop = [Environment]::GetFolderPath('Desktop')
    $name = "DevRunner.lnk"
    $shortcutPath = Join-Path $desktop $name

    $wsh = New-Object -ComObject WScript.Shell
    $sc = $wsh.CreateShortcut($shortcutPath)
    $sc.TargetPath = $TargetExe
    $sc.WorkingDirectory = Split-Path $TargetExe
    $sc.WindowStyle = 1
    $sc.Description = "DevRunner - 自动化构建与运行工具"
    $sc.IconLocation = $TargetExe
    $sc.Save()
    Write-Host "已在桌面创建快捷方式: $shortcutPath"

    if ($CreateInRepoTools) {
        $toolsDir = Join-Path $repoRoot "tools"
        if (-not (Test-Path $toolsDir)) { New-Item -ItemType Directory -Path $toolsDir | Out-Null }
        $repoShortcut = Join-Path $toolsDir $name
        $sc2 = $wsh.CreateShortcut($repoShortcut)
        $sc2.TargetPath = $TargetExe
        $sc2.WorkingDirectory = Split-Path $TargetExe
        $sc2.WindowStyle = 1
        $sc2.Description = "DevRunner - 自动化构建与运行工具（仓库复制）"
        $sc2.IconLocation = $TargetExe
        $sc2.Save()
        Write-Host "已在仓库 tools 目录创建快捷方式: $repoShortcut"
    }
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
