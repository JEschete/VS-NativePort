param(
    [switch]$InstallFfmpeg,
    [switch]$Pause
)

$ErrorActionPreference = 'Stop'
$result = 0
try {
    if (-not [Environment]::Is64BitOperatingSystem -or
            [Environment]::OSVersion.Version.Build -lt 18362) {
        throw 'This tester build requires 64-bit Windows 10 version 1903 or later, or Windows 11.'
    }
    foreach ($name in @('VS.Launcher.exe', 'VS.Desktop.exe', 'VS.Extract.exe',
            'SDL3.dll', 'dav1d.dll', 'sfizz.dll', 'webm.dll',
            'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) {
            throw "Missing $name. Extract the entire ZIP into one folder; do not run inside the ZIP."
        }
    }
    Write-Host 'Game files and bundled runtime dependencies are present.'
    $existingFfmpeg = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($InstallFfmpeg -and -not $existingFfmpeg) {
        $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
        if (-not $winget) {
            throw 'winget is unavailable. Install App Installer from Microsoft Store, or download FFmpeg from https://ffmpeg.org/download.html#build-windows'
        }
        if ($InstallFfmpeg -and $existingFfmpeg) {
            Write-Host 'FFmpeg is already on PATH; no installation was performed.'
        }
        & $winget.Source install --id Gyan.FFmpeg --exact --accept-package-agreements --accept-source-agreements
        if ($LASTEXITCODE -ne 0) {
            throw "winget returned $LASTEXITCODE. Download FFmpeg manually if installation is unavailable."
        }
        $env:PATH = [Environment]::GetEnvironmentVariable('PATH', 'Machine') + ';' +
            [Environment]::GetEnvironmentVariable('PATH', 'User')
    }
    $ffmpeg = Get-Command ffmpeg.exe -ErrorAction SilentlyContinue
    if ($ffmpeg) {
        & $ffmpeg.Source -version | Select-Object -First 1
        if ($LASTEXITCODE -ne 0) {
            throw "FFmpeg could not run: $($ffmpeg.Source)"
        }
        Write-Host 'FFmpeg is available. The launcher can also browse to a different copy.'
    } else {
        Write-Host 'FFmpeg was not found on PATH. It is required only for first-time asset preparation.' -ForegroundColor Yellow
        Write-Host 'In the launcher: Browse to ffmpeg.exe, Download FFmpeg, or Install with winget.'
        Write-Host 'Download: https://ffmpeg.org/download.html#build-windows'
    }
} catch {
    Write-Host "Dependency check failed: $($_.Exception.Message)" -ForegroundColor Red
    $result = 1
} finally {
    if ($Pause) {
        Read-Host 'Press Enter to close'
    }
}
exit $result
