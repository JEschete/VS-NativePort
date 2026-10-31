param(
    [string]$Version = 'local-test',
    [string]$OutputDirectory = '',
    [string]$VcRedistDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
    throw 'Version may contain only letters, digits, dots, underscores and hyphens.'
}
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$binaries = Join-Path $repo 'build\Release'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repo 'build\releases'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$name = "VS-NativePort-$Version-win-x64"
$stage = Join-Path $OutputDirectory $name
$archive = "$stage.zip"
if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $archive)) {
    throw "Output already exists: $stage. Choose a new version or output directory."
}
$files = @('VS.Launcher.exe', 'VS.Desktop.exe', 'VS.Extract.exe',
    'SDL3.dll', 'dav1d.dll', 'sfizz.dll', 'webm.dll')
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $binaries $file) -PathType Leaf)) {
        throw "Missing Release build file: $file. Build the Release solution first."
    }
}
if (-not $VcRedistDirectory) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere was not found. Supply -VcRedistDirectory pointing to x64\Microsoft.VC*.CRT.'
    }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'A C++ Visual Studio installation was not found.' }
    $redist = Join-Path $vs 'VC\Redist\MSVC'
    $candidates = @(Get-ChildItem -LiteralPath $redist -Directory |
        Where-Object { $_.Name -match '^\d+\.' } |
        Sort-Object { [version]$_.Name } -Descending)
    foreach ($candidate in $candidates) {
        $crt = @(Get-ChildItem -LiteralPath (Join-Path $candidate.FullName 'x64') -Directory |
            Where-Object { $_.Name -like 'Microsoft.VC*.CRT' })
        if ($crt.Count -gt 0) {
            $VcRedistDirectory = $crt[0].FullName
            break
        }
    }
}
if (-not $VcRedistDirectory -or -not (Test-Path -LiteralPath (Join-Path $VcRedistDirectory 'vcruntime140.dll'))) {
    throw 'The redistributable x64 MSVC CRT directory is missing.'
}
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $binaries $file) -Destination $stage
}
Get-ChildItem -LiteralPath $VcRedistDirectory -Filter '*.dll' -File |
    Copy-Item -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Check-Dependencies.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Start-Game.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $repo 'Docs\TESTER_RELEASE.md') -Destination (Join-Path $stage 'README.md')
$notices = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Path $notices | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $notices 'VS-NativePort.txt')
'Microsoft Visual C++ app-local runtime redistribution terms: https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/' |
    Set-Content -LiteralPath (Join-Path $notices 'Microsoft-VC-Runtime.txt') -Encoding UTF8
foreach ($dependency in @('sdl3', 'dav1d', 'libwebm', 'sfizz', 'drlibs', 'nlohmann-json')) {
    $notice = Join-Path $repo "vcpkg_installed\x64-windows\share\$dependency\copyright"
    if (-not (Test-Path -LiteralPath $notice -PathType Leaf)) {
        throw "Missing redistribution notice for $dependency."
    }
    Copy-Item -LiteralPath $notice -Destination (Join-Path $notices "$dependency.txt")
}
$developerProfile = [Environment]::GetFolderPath('UserProfile')
$personalPaths = @($developerProfile, $developerProfile.Replace('\', '/'))
foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
    if ($file.Name -in @('launcher.json', 'setup.log', 'vs-native.log')) {
        throw "Personal configuration or logs must not be packaged: $($file.Name)"
    }
    $bytes = [IO.File]::ReadAllBytes($file.FullName)
    foreach ($encoding in @([Text.Encoding]::UTF8, [Text.Encoding]::Unicode)) {
        $text = $encoding.GetString($bytes)
        foreach ($personalPath in $personalPaths) {
            if ($personalPath -and $text.IndexOf($personalPath, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                throw "Developer profile path found in package file: $($file.Name)"
            }
        }
    }
}
$revision = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source revision.' }
$status = @(& git -C $repo status --porcelain)
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine worktree status.' }
$manifest = [ordered]@{
    version = $Version
    configuration = 'Release'
    source_revision = $revision
    source_dirty = ($status.Count -ne 0)
    source_status = $status
    created_utc = [DateTime]::UtcNow.ToString('o')
    files = @(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($stage.Length + 1)
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stage 'release.json') -Encoding UTF8
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
Write-Output $archive
Get-FileHash -LiteralPath $archive -Algorithm SHA256
