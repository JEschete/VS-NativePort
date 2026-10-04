param(
    [string]$Executable = "$PSScriptRoot/../build/windows/Debug/vagrant-native.exe",
    [int]$Seconds = 8,
    [switch]$Windowed
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$stdout = Join-Path $root 'build/native-boot.stdout.txt'
$stderr = Join-Path $root 'build/native-boot.stderr.txt'
$previousHeadless = $env:VS_HEADLESS
$previousAudio = $env:VS_NO_AUDIO
$previousWatchdog = $env:VS_WATCHDOG
try {
    $env:VS_HEADLESS = if ($Windowed) { $null } else { '1' }
    $env:VS_NO_AUDIO = '1'
    $env:VS_WATCHDOG = [string]$Seconds
    $process = Start-Process -FilePath $Executable -WorkingDirectory $root -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $process.Handle
    if (-not $process.WaitForExit(($Seconds + 5) * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        Write-Error 'Native probe exceeded its hard bound before reaching the Windows watchdog.'
    }
    Get-Content -LiteralPath $stdout
    Get-Content -LiteralPath $stderr
    if ($process.ExitCode -notin @(0, 3)) {
        throw "Native probe failed with exit code $($process.ExitCode)."
    }
}
finally {
    $env:VS_HEADLESS = $previousHeadless
    $env:VS_NO_AUDIO = $previousAudio
    $env:VS_WATCHDOG = $previousWatchdog
}
