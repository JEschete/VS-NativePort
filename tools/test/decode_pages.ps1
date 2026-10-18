param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [Parameter(Mandatory = $true)][int]$X,
    [Parameter(Mandatory = $true)][int]$Y,
    [Parameter(Mandatory = $true)][int]$Width,
    [Parameter(Mandatory = $true)][int]$Height,
    [ValidateSet(4, 8, 16)][int]$Depth = 16,
    [ValidateRange(0, 32767)][int]$Clut = 0
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$data = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Source).Path)
if ($data.Length -ne 1048576) { throw 'Expected a 1024-by-512 capture of little-endian 16-bit words.' }
$pixelsPerWord = 16 / $Depth
$wordWidth = [int][Math]::Ceiling($Width / $pixelsPerWord)
if ($X -lt 0 -or $Y -lt 0 -or $Width -lt 1 -or $Height -lt 1 -or $X + $wordWidth -gt 1024 -or $Y + $Height -gt 512) {
    throw 'The requested rectangle is outside the captured page space.'
}
$paletteX = ($Clut -band 63) * 16
$paletteY = $Clut -shr 6
$mask = (1 -shl $Depth) - 1
$image = [System.Drawing.Bitmap]::new($Width, $Height)
try {
    for ($row = 0; $row -lt $Height; ++$row) {
        for ($column = 0; $column -lt $Width; ++$column) {
            $wordX = $X + [int][Math]::Floor($column / $pixelsPerWord)
            $word = [BitConverter]::ToUInt16($data, 2 * (($Y + $row) * 1024 + $wordX))
            if ($Depth -lt 16) {
                $index = ($word -shr (($column % $pixelsPerWord) * $Depth)) -band $mask
                $word = [BitConverter]::ToUInt16($data, 2 * ($paletteY * 1024 + (($paletteX + $index) -band 1023)))
            }
            $red = [int][Math]::Floor(($word -band 31) * 255 / 31)
            $green = [int][Math]::Floor((($word -shr 5) -band 31) * 255 / 31)
            $blue = [int][Math]::Floor((($word -shr 10) -band 31) * 255 / 31)
            $image.SetPixel($column, $row, [System.Drawing.Color]::FromArgb(255, $red, $green, $blue))
        }
    }
    $image.Save([System.IO.Path]::GetFullPath($Output), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $image.Dispose()
}