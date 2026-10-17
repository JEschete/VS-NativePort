<#
.SYNOPSIS
    Create the portable DuckStation copy used for reference captures (build\duckstation, git-ignored).

.DESCRIPTION
    Copies DuckStation from the machine's tool root and writes a settings.ini that points at the PS1 BIOS, disables
    memory cards (the port has none yet) and binds the pad to the keyboard (duckstation_capture.ps1 presses these).
    The installed DuckStation and its own settings are never touched.

    Locations on the development machine (see .github\copilot-instructions.md):
      DuckStation  C:\Tools\Systems\PSX\Emulators\DuckStation\duckstation-qt-x64-ReleaseLTCG.exe
      BIOS         F:\Emulators\PlayStation\bios\SCPH1001.BIN (NTSC-U; scph5501.bin also there)
#>
param(
    [string]$DuckStation = 'C:\Tools\Systems\PSX\Emulators\DuckStation',
    [string]$BiosDir = 'F:\Emulators\PlayStation\bios',
    [string]$Bios = 'SCPH1001.BIN',
    [switch]$Force
)
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$dst = Join-Path $repo 'build\duckstation'
if (-not (Test-Path (Join-Path $DuckStation 'duckstation-qt-x64-ReleaseLTCG.exe'))) { throw "DuckStation not found in $DuckStation" }
if (-not (Test-Path (Join-Path $BiosDir $Bios))) { throw "BIOS $Bios not found in $BiosDir" }
if ($Force -and (Test-Path $dst)) { Remove-Item $dst -Recurse -Force }
if (-not (Test-Path $dst)) { Copy-Item $DuckStation $dst -Recurse }
Set-Content -Path (Join-Path $dst 'portable.txt') -Value '' -Encoding ascii
@"
[Main]
SetupWizardIncomplete = false
ConfirmPowerOff = false
StartFullscreen = false
PauseOnFocusLoss = false
[AutoUpdater]
CheckAtStartup = false
[BIOS]
SearchDirectory = $BiosDir
PathNTSCU = $Bios
PatchFastBoot = true
[GPU]
ResolutionScale = 1
[Display]
Scaling = Nearest
[MemoryCards]
Card1Type = None
Card2Type = None
[Pad1]
Type = DigitalController
Up = Keyboard/Up
Down = Keyboard/Down
Left = Keyboard/Left
Right = Keyboard/Right
Cross = Keyboard/Z
Circle = Keyboard/X
Square = Keyboard/A
Triangle = Keyboard/S
Start = Keyboard/Return
Select = Keyboard/Backspace
"@ | Set-Content -Path (Join-Path $dst 'settings.ini') -Encoding ascii
"DuckStation ready in $dst (BIOS $BiosDir\$Bios)"
