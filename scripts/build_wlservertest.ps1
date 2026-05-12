# build_wlservertest.ps1 — 一键编译 WLServerTest 并发布到 artifacts/
#
# 设计目标：跨开发环境运行
#   - 自动探测 VS 安装（vswhere）：支持 2017/2019/2022，任意盘符、Community/Pro/Enterprise/BuildTools
#   - 仓库路径相对脚本自身解析，与 cwd 无关
#   - 失败时退出非零码并打印关键错误
#
# 使用：
#   pwsh -File scripts/build_wlservertest.ps1
#   pwsh -File scripts/build_wlservertest.ps1 -Configuration Debug
#   pwsh -File scripts/build_wlservertest.ps1 -SkipPublish
#
# 环境变量覆盖（适配特殊环境）：
#   $env:MSBUILD_EXE      明确指定 MSBuild.exe 路径，跳过自动探测
#   $env:WLNETCOMM_DLL    明确指定 WLNetComm.dll 来源（默认仓库 external/.../bin/Release/x64）

[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [switch]$SkipPublish,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

# ---------- 路径锚定（脚本相对，环境无关） ----------
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
Set-Location $RepoRoot

$Project   = Join-Path $RepoRoot 'external\IEG_Code\code\WLServerTest\WLServerTest.vcxproj'
$BuildOut  = Join-Path $RepoRoot ("external\IEG_Code\code\WLServerTest\$Platform\$Configuration")
$PublishDir = Join-Path $RepoRoot 'artifacts\WLServerTestPublish'
$DllSrc    = if ($env:WLNETCOMM_DLL) { $env:WLNETCOMM_DLL } else { Join-Path $RepoRoot 'external\IEG_Code\code\bin\Release\x64\WLNetComm.dll' }
$IniSrc    = Join-Path $RepoRoot 'external\IEG_Code\code\WLServerTest\WLServerTest.ini'

if (-not (Test-Path $Project)) {
    Write-Host "[FATAL] 找不到工程文件: $Project" -ForegroundColor Red
    Write-Host "        请检查仓库是否完整，或脚本是否被移动到非 scripts/ 目录。"
    exit 2
}

# ---------- 探测 MSBuild ----------
function Find-MSBuild {
    if ($env:MSBUILD_EXE -and (Test-Path $env:MSBUILD_EXE)) {
        return $env:MSBUILD_EXE
    }
    # 1) PATH 上现成的
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # 2) vswhere（VS Installer 标准位置）
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if (Test-Path $vswhere) {
        $msb = & $vswhere -latest -prerelease `
            -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
        if ($msb -and (Test-Path $msb)) { return $msb }
        # 兜底：仅要求 MSBuild 组件（可能没装 VC 工具，但能让用户看清楚 fail 在哪）
        $msb = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
        if ($msb -and (Test-Path $msb)) { return $msb }
    }

    # 3) 常见绝对路径兜底
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
    )
    foreach ($p in $candidates) { if (Test-Path $p) { return $p } }

    return $null
}

$msbuild = Find-MSBuild
if (-not $msbuild) {
    Write-Host "[FATAL] 找不到 MSBuild。请安装 VS2022 (含 MFC + C++ 桌面开发) 或设置 `$env:MSBUILD_EXE。" -ForegroundColor Red
    exit 3
}
Write-Host "[INFO] MSBuild     : $msbuild"
Write-Host "[INFO] Project     : $Project"
Write-Host "[INFO] Config/Plat : $Configuration / $Platform"
Write-Host ''

# ---------- 编译 ----------
$logFile = Join-Path $RepoRoot 'build_wlservertest.log'
$msbArgs = @(
    "`"$Project`"",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    '/m',
    '/nologo',
    '/v:minimal',
    "/fl",
    "/flp:logfile=`"$logFile`";verbosity=normal"
)
if ($Clean) { $msbArgs += '/t:Rebuild' }

Write-Host "[BUILD] 开始编译..." -ForegroundColor Cyan
$proc = Start-Process -FilePath $msbuild -ArgumentList $msbArgs -NoNewWindow -PassThru -Wait
if ($proc.ExitCode -ne 0) {
    Write-Host ''
    Write-Host "[FAIL] 编译失败 (ExitCode=$($proc.ExitCode))" -ForegroundColor Red
    Write-Host "       完整日志: $logFile" -ForegroundColor Yellow
    if (Test-Path $logFile) {
        Write-Host "----- 关键错误（前 30 行）-----" -ForegroundColor Yellow
        Get-Content $logFile | Select-String -Pattern 'error|fatal' -SimpleMatch -CaseSensitive:$false |
            Select-Object -First 30 | ForEach-Object { Write-Host $_.Line }
    }
    exit $proc.ExitCode
}
Write-Host "[BUILD] OK" -ForegroundColor Green
Write-Host ''

# ---------- 发布 ----------
if ($SkipPublish) { Write-Host "[SKIP] -SkipPublish 已跳过拷贝"; exit 0 }

$exe = Join-Path $BuildOut 'WLServerTest.exe'
if (-not (Test-Path $exe)) {
    Write-Host "[FATAL] 编译声称成功但产物不存在: $exe" -ForegroundColor Red
    exit 4
}

New-Item -ItemType Directory -Path $PublishDir -Force | Out-Null
Copy-Item -Force $exe $PublishDir

if (Test-Path $DllSrc) {
    Copy-Item -Force $DllSrc $PublishDir
} else {
    Write-Host "[WARN] 未找到 WLNetComm.dll: $DllSrc" -ForegroundColor Yellow
    Write-Host "       请手动拷入 artifacts\WLServerTestPublish\ 或设置 `$env:WLNETCOMM_DLL"
}

# RawPacketEngine.dll：与 WLServerTest 同目录的 bin\Release\x64
$rawDll = Join-Path $RepoRoot 'external\IEG_Code\code\bin\Release\x64\RawPacketEngine.dll'
if (Test-Path $rawDll) { Copy-Item -Force $rawDll $PublishDir }

if (Test-Path $IniSrc)  { Copy-Item -Force $IniSrc $PublishDir }

$published = Get-ChildItem $PublishDir | Select-Object Name, @{n='Size';e={'{0:N0}' -f $_.Length}}
Write-Host "[PUBLISH] $PublishDir" -ForegroundColor Green
$published | Format-Table -AutoSize | Out-String | Write-Host

Write-Host "[DONE] 一切就绪。把 artifacts\WLServerTestPublish\ 整目录拷到目标机即可双击运行。" -ForegroundColor Green
