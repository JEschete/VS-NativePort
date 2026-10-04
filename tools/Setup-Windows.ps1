param([switch]$SkipToolDownloads)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$python = Join-Path $root 'build/python/Scripts/python.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Install Visual Studio 2022 with Desktop development with C++ and the Clang compiler component.'
}
if (-not (Test-Path -LiteralPath $python)) {
    $systemPython = Get-Command python.exe -ErrorAction Stop
    & $systemPython.Source -m venv (Join-Path $root 'build/python')
    if ($LASTEXITCODE -ne 0) { throw 'Windows build environment creation failed.' }
}
& $python -m pip install -r (Join-Path $root 'requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Windows build dependency installation failed.' }
if (-not $SkipToolDownloads) {
    & $python (Join-Path $PSScriptRoot 'windows.py') bootstrap
    if ($LASTEXITCODE -ne 0) { throw 'Windows tool bootstrap failed.' }
}
$cmake = & $vswhere -latest -products '*' -version '[17,18)' `
    -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (-not $cmake -or -not (Test-Path -LiteralPath $cmake)) {
    $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
}
Push-Location $root
try {
    & $cmake --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio solution generation failed.' }
}
finally { Pop-Location }
Write-Output "Visual Studio solution: $root/build/windows/VagrantStory.sln"
