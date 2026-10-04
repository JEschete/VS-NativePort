param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../../..").Path
$python = Join-Path $root 'build/python/Scripts/python.exe'
$tools = Get-Content -LiteralPath (Join-Path $root 'build/tools/toolchain.json') -Raw | ConvertFrom-Json
$build = Join-Path $root 'native/build/psx'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$name = [System.IO.Path]::GetFileNameWithoutExtension($Source)
$elf = Join-Path $build "$name.elf"
$binary = Join-Path $build "$name.bin"
$compiler = Join-Path $tools.mips_bin 'mipsel-none-elf-gcc.exe'
$objcopy = Join-Path $tools.mips_bin 'mipsel-none-elf-objcopy.exe'
& $compiler -march=mips1 -mabi=32 -msoft-float -nostdlib -fno-pic -mno-abicalls -G0 `
    '-Wl,--build-id=none' "-Wl,-T,$PSScriptRoot/link.ld" -o $elf $Source
if ($LASTEXITCODE -ne 0) { throw 'Windows MIPS hardware-test assembly failed.' }
& $objcopy -O binary $elf $binary
if ($LASTEXITCODE -ne 0) { throw 'Windows MIPS hardware-test extraction failed.' }
& $python (Join-Path $root 'native/tools/mkpsexe.py') $binary $Output 0x80010000
if ($LASTEXITCODE -ne 0) { throw 'PS-EXE generation failed.' }
