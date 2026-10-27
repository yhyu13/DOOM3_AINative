#Requires -Version 5.1
<#
.SYNOPSIS
  Screenshot the main window of a running process (topmost, DPI-aware).
.EXAMPLE
  .\scripts\capture-window.ps1 -ProcessName DOOM3 -Out targets\win11-amd64\build\shot.png
.EXAMPLE
  .\scripts\capture-window.ps1 -ProcessId 1234 -Out targets\win11-amd64\build\shot.png
#>
[CmdletBinding()]
param(
    [string]$ProcessName = 'DOOM3',
    [int]$ProcessId = 0,
    [Parameter(Mandatory = $true)][string]$Out
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class CapW {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint fl, UIntPtr ex);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  public static bool Focus(IntPtr h) {
    uint pid; uint ft = GetWindowThreadProcessId(GetForegroundWindow(), out pid); uint me = GetCurrentThreadId();
    keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);   // Alt tap unlocks SetForegroundWindow
    AttachThreadInput(me, ft, true); ShowWindow(h, 9); BringWindowToTop(h); bool ok = SetForegroundWindow(h); AttachThreadInput(me, ft, false); return ok;
  }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
}
"@
[void][CapW]::SetProcessDPIAware()
$label = if ($ProcessId -gt 0) { "pid $ProcessId" } else { "process '$ProcessName'" }
$proc = $null
$deadline = [DateTime]::UtcNow.AddSeconds(15)
while ([DateTime]::UtcNow -lt $deadline) {
    if ($ProcessId -gt 0) {
        $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    } else {
        $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    }
    if ($proc) { break }
    if ($ProcessId -gt 0 -and -not (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 500
}
if (-not $proc) { Write-Host "CAPTURE no window for $label"; exit 5 }
$h = $proc.MainWindowHandle
[void][CapW]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0043)   # HWND_TOPMOST | NOSIZE | NOMOVE | SHOWWINDOW
[void][CapW]::Focus($h)   # DirectSound mutes unfocused windows, so audio checks need real focus
Start-Sleep -Milliseconds 1500
$cr = New-Object CapW+RECT
[void][CapW]::GetClientRect($h, [ref]$cr)
$pt = New-Object CapW+POINT
[void][CapW]::ClientToScreen($h, [ref]$pt)
$r = New-Object CapW+RECT
$r.L = $pt.X; $r.T = $pt.Y
$w = $cr.R - $cr.L; $ht = $cr.B - $cr.T   # client area = the exact GL render size
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $ht))
$g.Dispose()
$dir = Split-Path -Parent $Out
if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$set = New-Object 'System.Collections.Generic.HashSet[int]'
for ($yy = 0; $yy -lt 40; $yy++) { for ($xx = 0; $xx -lt 40; $xx++) { [void]$set.Add($bmp.GetPixel([int]($xx * ($w - 1) / 39), [int]($yy * ($ht - 1) / 39)).ToArgb()) } }
$bmp.Dispose()
Write-Output ("CAPTURE title='{0}' {1}x{2} distinctColours={3} file={4}" -f $proc.MainWindowTitle, $w, $ht, $set.Count, $Out)
exit 0
