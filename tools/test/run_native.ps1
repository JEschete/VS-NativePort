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
    [switch]$Devices   # use the keyboard/gamepad too (default: scripted input only)
)
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repo
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
if (-not $NoCopy) {
    python tools\port\copy_to_native.py 2>&1 | Select-String 'files \(|rror|not found|copy_fixups' | ForEach-Object { $_.Line }
    python tools\scaffold\gen_solution.py | Out-Null
}
& $msbuild VagrantStory.Native.sln /m /nologo /v:minimal "/p:Configuration=$Config" /p:Platform=x64 '/t:VS_Desktop' 2>&1 |
    Out-File -Encoding utf8 build\run_native_build.log
$errs = Select-String -Path build\run_native_build.log -Pattern ' error |error [A-Z]+\d+' | Select-Object -First 25
if ($errs) { $errs | ForEach-Object { $_.Line.Trim() -replace '\[D:.*$', '' }; return }
if (Test-Path $Cap) { Remove-Item $Cap -Recurse -Force }
$presses = @()
if ($SkipIntro) { $presses += '1000:Start' }
if ($Press) { $presses += $Press }
$argv = @('--frames', "$Frames", '--capture', $Cap, '--at', $At)
if ($presses) { $argv += @('--press', ($presses -join ',')) }
if (-not $Devices) { $argv += '--scripted' }
$p = Start-Process -FilePath "build\$Config\VS.Desktop.exe" -ArgumentList $argv -PassThru
if (-not $p.WaitForExit(300000)) { Stop-Process -Id $p.Id; 'TIMEOUT' }
"exit: $($p.ExitCode)"
Get-Content "build\$Config\vs-native.log" |
    Where-Object { $_ -notmatch 'SDL_|exe_common|WinMain|invoke_main|BaseThread|RtlUser|scrt|TryCreatePackage|pointer table' } |
    Select-Object -Last 45
