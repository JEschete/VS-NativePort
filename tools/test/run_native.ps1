<#
.SYNOPSIS
    Copy (optional), build and run the native port for a fixed number of ticks, capturing frames.

.DESCRIPTION
    Test loop for the port: regenerates the copied code (unless -NoCopy), builds VS.Desktop (Debug), runs it with
    --frames/--capture/--at and prints the end of build\Debug\vs-native.log (crash stacks included).
    Ticks are 1/60 s. The intro movie starts about tick 780 and is skipped by pressing Start (-SkipIntro, or
    "--press 1000:Start"); the title menu is then up much earlier than the tick 3300 of an unskipped run.

.EXAMPLE
    tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1800 -At '1500,1700'
    tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1800 -At '1700' -Press '1400:Down,1500:Circle'
#>
param(
    [int]$Frames = 600,
    [string]$At = '30,120,240,400,599',
    [switch]$NoCopy,
    [switch]$SkipIntro,
    [string]$Press = '',
    [string]$Cap = 'build\cap1',
    [string]$Config = 'Debug',
    [switch]$Devices,  # use the keyboard/gamepad too (default: scripted input only)
    [int]$Speed = 1,   # logical ticks per real tick (attract/long runs: 8)
    [ValidateSet('trace', 'debug', 'info', 'warn', 'error', 'off')]
    [string]$LogLevel = 'info',
    [string]$LogChannels = '*'
)
$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repo
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
if (-not $NoCopy) {
    python tools\port\copy_to_native.py 2>&1 | Select-String 'files \(|rror|not found|copy_fixups' | ForEach-Object { $_.Line }
    if ($LASTEXITCODE -ne 0) { throw 'Native copy failed' }
    python tools\scaffold\gen_solution.py | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Project generation failed' }
}
& $msbuild VagrantStory.Native.sln /m /nologo /v:minimal "/p:Configuration=$Config" /p:Platform=x64 '/t:VS_Desktop' 2>&1 |
    Out-File -Encoding utf8 build\run_native_build.log
$buildExit = $LASTEXITCODE
$errs = Select-String -Path build\run_native_build.log -Pattern ' error |error [A-Z]+\d+' | Select-Object -First 25
if ($buildExit -ne 0 -or $errs) {
    $errs | ForEach-Object { $_.Line.Trim() -replace '\[D:.*$', '' }
    throw "Native build failed ($buildExit); see build\run_native_build.log"
}
if (Test-Path $Cap) { Remove-Item $Cap -Recurse -Force }
$presses = @()
if ($SkipIntro) { $presses += '1000:Start' }
if ($Press) { $presses += $Press }
$argv = @('--frames', "$Frames", '--capture', "`"$Cap`"", '--at', $At,
          '--log-level', $LogLevel, '--log-channels', $LogChannels)
if ($presses) { $argv += @('--press', ($presses -join ',')) }
if (-not $Devices) { $argv += '--scripted' }
if ($Speed -gt 1) { $argv += @('--speed', "$Speed") }
$logPath = Join-Path $repo "build\$Config\vs-native.log"
if (Test-Path $logPath) { Remove-Item $logPath -Force }
# Own the handle from launch; Start-Process can lose the exit code when startup fails immediately.
$p = New-Object System.Diagnostics.Process
$p.StartInfo.FileName = Join-Path $repo "build\$Config\VS.Desktop.exe"
$p.StartInfo.WorkingDirectory = $repo
$p.StartInfo.UseShellExecute = $false
$p.StartInfo.Arguments = $argv -join ' '
if (-not $p.Start()) { throw 'Native process did not start' }
if (-not $p.WaitForExit(300000)) {
    $p.Kill()
    $p.WaitForExit()
    $p.Dispose()
    throw 'Native run timed out'
}
$exitCode = $p.ExitCode
$p.Dispose()
"exit: $exitCode"
$log = if (Test-Path $logPath) { Get-Content $logPath } else { @() }
$log |
    Where-Object { $_ -notmatch 'SDL_|exe_common|WinMain|invoke_main|BaseThread|RtlUser|scrt|TryCreatePackage|pointer table' } |
    Select-Object -Last 45
if ($exitCode -ne 0) { throw "Native run failed ($exitCode)" }
if ($log -match '\[error\].*crash:') { throw 'Native run logged a crash' }
foreach ($tick in ($At -split ',')) {
    if ($tick -match '^\d+$' -and [long]$tick -gt 0 -and [long]$tick -le $Frames) {
        if (-not (Test-Path (Join-Path $Cap "frame_$([long]$tick).bmp"))) {
            throw "Requested capture at tick $tick was not produced"
        }
    }
}
