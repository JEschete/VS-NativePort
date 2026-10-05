param(
    [string]$GhidraHome = "$PSScriptRoot/../../../../tools/Base/Analysis/Ghidra/12.1.2",
    [ValidateRange(30, 300)]
    [int]$Seconds = 180
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path "$PSScriptRoot/../../../..").Path
$binary = Join-Path $workspace 'data/BATTLE/BATTLE.PRG'
$mainExecutable = Join-Path $workspace 'data/SLUS_010.40'
$launcher = Join-Path $GhidraHome 'support/analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $launcher)) {
    throw "Ghidra headless launcher is missing: $launcher"
}
if ((Get-FileHash -Algorithm SHA1 -LiteralPath $binary).Hash -ne
    'D53AACCC3B3A2FC057D05E0DCEA92F7182BC72A9') {
    throw 'BATTLE.PRG does not match the configured retail reference.'
}
$output = Join-Path $workspace ('resources/rood-reverseWin/build/ghidra/' +
    (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $output
$stdout = Join-Path $output 'stdout.txt'
$stderr = Join-Path $output 'stderr.txt'
$arguments = @(
    '/d', '/s', '/c',
    ('""{0}" "{1}" BattleUnit -import "{2}" -loader BinaryLoader ' +
        '-loader-baseAddr 0x80068800 -processor PSX:LE:32:default -cspec default ' +
        '-scriptPath "{3}" -preScript BattleUnitEvidence.java prepare "{6}" ' +
        '-postScript BattleUnitEvidence.java export -analysisTimeoutPerFile 60 ' +
        '-max-cpu 2 -log "{4}" -scriptlog "{5}""') -f
        (Resolve-Path $launcher).Path, $output, $binary, $PSScriptRoot,
        (Join-Path $output 'analysis.log'), (Join-Path $output 'evidence.txt'), $mainExecutable
)
$process = Start-Process -FilePath $env:ComSpec -ArgumentList $arguments `
    -WorkingDirectory $workspace -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$null = $process.Handle
if (-not $process.WaitForExit($Seconds * 1000)) {
    & taskkill.exe /PID $process.Id /T /F
    throw "Ghidra exceeded its $Seconds-second bound. Logs: $output"
}
$process.WaitForExit()
Get-Content -LiteralPath $stdout -Tail 45
Get-Content -LiteralPath $stderr -Tail 30
if ($process.ExitCode -ne 0) {
    throw "Ghidra failed with exit code $($process.ExitCode). Logs: $output"
}
$evidence = Join-Path $output 'evidence.txt'
if (-not (Select-String -LiteralPath $evidence -SimpleMatch `
    'BATTLE_UNIT_EVIDENCE_COMPLETE' -Quiet)) {
    throw "Ghidra did not complete the requested evidence export. Logs: $output"
}
Write-Output "Verified Ghidra evidence: $evidence"