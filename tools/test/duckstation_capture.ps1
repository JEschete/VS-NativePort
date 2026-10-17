<#
.SYNOPSIS
    Run the portable DuckStation (tools\test\duckstation_setup.ps1) on the disc and capture reference screens.

.DESCRIPTION
    Starts DuckStation, presses keys at given seconds and saves the window's picture at given seconds to
    <Out>\ds_<sec>.png (PrintWindow, so the window may be covered). Times are wall-clock seconds from launch; the
    title menu is up at about 75 s, about 30 s when Start is pressed during the movie (-SkipIntro: Return at 32 s).
    Keys (bindings from the setup script): Up Down Left Right, Z = Cross, X = Circle, A = Square, S = Triangle,
    Return = Start, Backspace = Select. Each key briefly brings DuckStation to the front (Qt reads only focused input)
    and gives focus back.

.EXAMPLE
    tools\test\duckstation_capture.ps1 -SkipIntro -Keys '40:Down,42:X' -Seconds 38,46,50 -Out build\ds_cont
#>
param(
    [double[]]$Seconds = @(5, 9, 16, 22, 30, 38, 46, 56, 62),
    [string]$Out = 'build\ds_cap',
    [string]$Keys = '',
    [switch]$SkipIntro
)
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repo
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DsWin {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    public static void Focus(IntPtr h) {
        uint fg = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero), me = GetCurrentThreadId();
        AttachThreadInput(me, fg, true);
        keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);  // Alt: allows the switch
        SetForegroundWindow(h);
        AttachThreadInput(me, fg, false);
    }
    public static void Key(byte vk, bool down) {
        bool ext = vk >= 0x25 && vk <= 0x28;   // arrows are extended keys
        uint flags = (down ? 0u : 2u) | (ext ? 1u : 0u);
        keybd_event(vk, (byte)MapVirtualKey(vk, 0), flags, UIntPtr.Zero);
    }
}
'@
if (-not (Test-Path 'build\duckstation\portable.txt')) { & (Join-Path $PSScriptRoot 'duckstation_setup.ps1') | Out-Null }
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
[DsWin]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory $Out | Out-Null
$exe = (Resolve-Path 'build\duckstation\duckstation-qt-x64-ReleaseLTCG.exe').Path
$p = Start-Process -FilePath $exe -ArgumentList '-batch', '--', (Resolve-Path 'disks\SLUS-01040.cue').Path -PassThru
$start = Get-Date
$vk = @{ Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27; Z = 0x5A; X = 0x58; A = 0x41; S = 0x53; Return = 0x0D; Backspace = 0x08 }
$list = @()
if ($SkipIntro) { $list += '32:Return' }
if ($Keys) { $list += $Keys.Split(',') }
$presses = $list | ForEach-Object { $a = $_.Split(':'); [pscustomobject]@{ T = [double]$a[0]; K = $a[1]; Done = $false } }
function Wait-Until([double]$s) {
    while (((Get-Date) - $start).TotalSeconds -lt $s) {
        foreach ($k in $presses) {
            if (-not $k.Done -and ((Get-Date) - $start).TotalSeconds -ge $k.T) {
                $p.Refresh()
                $prev = [DsWin]::GetForegroundWindow()
                [DsWin]::Focus($p.MainWindowHandle); Start-Sleep -Milliseconds 80
                [DsWin]::Key($vk[$k.K], $true); Start-Sleep -Milliseconds 100; [DsWin]::Key($vk[$k.K], $false)
                Start-Sleep -Milliseconds 50; [DsWin]::Focus($prev)
                $k.Done = $true
                "key $($k.K) at $($k.T)"
            }
        }
        Start-Sleep -Milliseconds 30
    }
}
foreach ($s in $Seconds) {
    Wait-Until $s
    $p.Refresh()
    $h = $p.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { "t=$s no window"; continue }
    $r = New-Object DsWin+RECT; [DsWin]::GetClientRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [DsWin]::PrintWindow($h, $hdc, 3) | Out-Null   # client area, GPU-rendered content
    $g.ReleaseHdc($hdc)
    $bmp.Save((Join-Path $Out ("ds_{0:000}.png" -f $s)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    "t=$s captured ${w}x${hh}"
}
Stop-Process -Id $p.Id
