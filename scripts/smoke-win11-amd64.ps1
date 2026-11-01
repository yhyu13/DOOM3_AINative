#Requires -Version 5.1
<#
.SYNOPSIS
  Start the win11-amd64 engine twice (main menu, then the demo map), assert log lines, capture screenshots of a -Width x -Height client (logical, default 1280x720) and compare the physical capture with that size times the display scale.
.DESCRIPTION
  Phase MENU: engine boots with game data, no startup map; log must show the ARB2 renderer, game init, session init and
              no "Couldn't load gui"; the client area must match the requested size after DPI scaling and must not be flat.
  Phase MAP : engine boots with +devmap <Map>; log must show "entities spawned" and "SpawnPlayer"; process must still be
              alive after SettleSec; screenshot must not be flat.
  Each phase uses its own save/log directory, so logs and screenshots always belong to the same run.
  Exit codes: 0 pass; 3 exe missing; 4 process died; 5 no window; 6 flat screenshot; 7 log assertion failed;
  8 the capture was unreadable or the physical client size does not match -Width x -Height multiplied by
  the display scale (see -Scale; the requested, expected and observed sizes are printed); 10 the
  map-phase audio check failed (audio-peak.ps1's printed code is the detail; a silent session is one
  cause). Without -DataDir the script exits 9 (SKIPPED by the caller; never a PASS).
  -SizeSelfTest checks the size comparison itself (scaled match, unscaled window, wrong width, rounding
  tolerance, 100% host) and exits 0/1 without starting the engine.
  11 = -OutDir is non-empty and -Clean was not supplied; 12 = an instance of the resolved executable is
  already running and -StopExisting was not supplied. Only processes whose executable path matches the
  resolved executable are stopped.
.EXAMPLE
  .\scripts\smoke-win11-amd64.ps1 -DataDir <data-root>\run -OutDir targets\win11-amd64\build\smoke
.EXAMPLE
  .\scripts\smoke-win11-amd64.ps1 -DataDir <data-root>\run -Clean -StopExisting
#>
[CmdletBinding()]
param(
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [string]$Map = 'game/demo_mars_city1',
    [int]$TimeoutSec = 90,
    [int]$SettleSec = 40,
    [int]$Width = 1280,
    [int]$Height = 720,
    [double]$Scale = 0,      # physical pixels per requested pixel; 0 = detect from the system DPI
    [int]$SizeTolerance = 2, # physical-pixel tolerance for DPI rounding
    [switch]$Clean,          # allow replacing a non-empty -OutDir
    [switch]$StopExisting,   # allow stopping instances of the same resolved executable
    [switch]$SizeSelfTest    # run comparison controls and exit without starting the engine
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot '_common.ps1')

function Get-DisplayScale {
    <#
      Physical pixels per requested pixel. The engine window is DPI-unaware, so Windows multiplies its
      logical client size by the system DPI (120 at 125%). Read LOGPIXELSX with this process made
      DPI-aware; GetDpiForSystem returns 96 to an unaware process and would hide the scaling.
    #>
    try {
        if (-not ('SmokeDpi' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SmokeDpi {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hDC, int index);
}
'@
        }
        [void][SmokeDpi]::SetProcessDPIAware()
        $dc = [SmokeDpi]::GetDC([IntPtr]::Zero)
        try { $dpi = [SmokeDpi]::GetDeviceCaps($dc, 88) } finally { [void][SmokeDpi]::ReleaseDC([IntPtr]::Zero, $dc) }
        if ($dpi -ge 96) { return ([double]$dpi / 96.0) }
    } catch { }
    return 1.0
}

function Test-ClientSize([int]$ObservedWidth, [int]$ObservedHeight, [int]$RequestedWidth, [int]$RequestedHeight, [double]$DisplayScale, [int]$Tolerance) {
    # One coordinate system: the capture reports physical pixels, so scale the requested logical size.
    $ew = [int][math]::Round($RequestedWidth * $DisplayScale)
    $eh = [int][math]::Round($RequestedHeight * $DisplayScale)
    [pscustomobject]@{
        Ok = (([math]::Abs($ObservedWidth - $ew) -le $Tolerance) -and ([math]::Abs($ObservedHeight - $eh) -le $Tolerance))
        ExpectedWidth = $ew; ExpectedHeight = $eh
    }
}

if ($SizeSelfTest) {
    $cases = @(
        [pscustomobject]@{ Label = '125% scaled window matches';                    W = 2500; H = 1406; RW = 2000; RH = 1125; S = 1.25; Want = $true },
        [pscustomobject]@{ Label = 'unscaled 2000x1125 window at 125% scaling';     W = 2000; H = 1125; RW = 2000; RH = 1125; S = 1.25; Want = $false },
        [pscustomobject]@{ Label = 'genuinely wrong width after scaling';           W = 2400; H = 1406; RW = 2000; RH = 1125; S = 1.25; Want = $false },
        [pscustomobject]@{ Label = 'genuinely wrong height after scaling';          W = 2500; H = 1300; RW = 2000; RH = 1125; S = 1.25; Want = $false },
        [pscustomobject]@{ Label = 'DPI rounding inside tolerance';                 W = 2500; H = 1407; RW = 2000; RH = 1125; S = 1.25; Want = $true },
        [pscustomobject]@{ Label = '100% host exact match';                         W = 1280; H = 720;  RW = 1280; RH = 720;  S = 1.0;  Want = $true }
    )
    $bad = 0
    foreach ($c in $cases) {
        $r = Test-ClientSize $c.W $c.H $c.RW $c.RH $c.S $SizeTolerance
        $got = if ($r.Ok) { 'PASS' } else { 'FAIL' }
        $want = if ($c.Want) { 'PASS' } else { 'FAIL' }
        Write-Host ("SIZETEST {0}: expected {1}, got {2} (observed {3}x{4}; expected {5}x{6} at scale {7})" -f $c.Label, $want, $got, $c.W, $c.H, $r.ExpectedWidth, $r.ExpectedHeight, $c.S)
        if ($got -ne $want) { $bad++ }
    }
    Write-Host ("SIZETEST summary: {0} passed, {1} failed" -f ($cases.Count - $bad), $bad)
    if ($bad -gt 0) { exit 1 }
    exit 0
}

$displayScale = if ($Scale -gt 0) { $Scale } else { Get-DisplayScale }
if (-not $Exe) { $Exe = Resolve-Doom3Exe -Repo $repo }
if (-not $OutDir) { $OutDir = Join-Path $repo 'targets\win11-amd64\build\smoke' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "SMOKE exe missing: $Exe"; exit 3 }
$Exe = [IO.Path]::GetFullPath($Exe)
if (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { Write-Host 'SMOKE no game data dir'; exit 9 }
$capture = Join-Path $PSScriptRoot 'capture-window.ps1'

$running = @(Get-ProcessesUsingExe -Path $Exe)
if ($running.Count -gt 0) {
    if (-not $StopExisting) {
        Write-Host ("SMOKE {0} instance(s) of {1} already running; pass -StopExisting to stop them" -f $running.Count, $Exe)
        exit 12
    }
    Stop-ProcessesUsingExe -Path $Exe
}

if (Test-Path -LiteralPath $OutDir) {
    $existing = @(Get-ChildItem -LiteralPath $OutDir -Force -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) {
        if (-not $Clean) { Write-Host "SMOKE refusing to overwrite non-empty OutDir: $OutDir (pass -Clean)"; exit 11 }
        Remove-Item -LiteralPath $OutDir -Recurse -Force
    }
}

function Invoke-Phase([string]$Name, [string[]]$Extra, [string[]]$MustMatch, [string[]]$MustNotMatch, [bool]$CheckAudio = $false) {
    $dir = Join-Path $OutDir $Name
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $a = @('+set', 'logFile', '2', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '-1',
        '+set', 'r_customWidth', "$Width", '+set', 'r_customHeight', "$Height",
        '+set', 'fs_basepath', $DataDir, '+set', 'fs_savepath', (Join-Path $dir 'save')) + $Extra
    $p = Start-Process -FilePath $Exe -ArgumentList $a -PassThru
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec -and -not $p.HasExited) {
        $p.Refresh(); if ($p.MainWindowHandle -ne [IntPtr]::Zero) { break }; Start-Sleep -Milliseconds 500
    }
    if ($p.HasExited) { Write-Host ("SMOKE[{0}] process died early, exit 0x{1:X}" -f $Name, $p.ExitCode); return 4 }
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { Stop-Process -Id $p.Id -Force; Write-Host "SMOKE[$Name] no window"; return 5 }
    Start-Sleep -Seconds $SettleSec
    if ($p.HasExited) { Write-Host ("SMOKE[{0}] process died during settle, exit 0x{1:X}" -f $Name, $p.ExitCode); return 4 }
    $shot = Join-Path $dir 'screenshot.png'
    $cap = & $capture -ProcessId $p.Id -Out $shot
    $audioCode = 0
    if ($CheckAudio) {
        $au = & (Join-Path $PSScriptRoot 'audio-peak.ps1') -ProcessId $p.Id -Ms 6000
        $audioCode = $LASTEXITCODE
        Write-Host ("SMOKE[{0}] {1}" -f $Name, $au)
    }
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    $log = Join-Path $dir 'save\base\qconsole.log'
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
    if ($text) { Copy-Item $log (Join-Path $dir 'qconsole.log') -Force }
    $colours = 0; $cw = 0; $ch = 0
    if ("$cap" -match '(\d+)x(\d+) distinctColours=(\d+)') { $cw = [int]$Matches[1]; $ch = [int]$Matches[2]; $colours = [int]$Matches[3] }
    $bad = @()
    foreach ($m in $MustMatch) { if ($text -notmatch $m) { $bad += "missing /$m/" } }
    foreach ($m in $MustNotMatch) { if ($text -match $m) { $bad += "forbidden /$m/" } }
    $size = Test-ClientSize $cw $ch $Width $Height $displayScale $SizeTolerance
    Write-Host ("SMOKE[{0}] client={1}x{2} requested={3}x{4} scale={5} expected={6}x{7} colours={8} logChecks={9} shot={10}" -f $Name, $cw, $ch, $Width, $Height, $displayScale, $size.ExpectedWidth, $size.ExpectedHeight, $colours, $(if ($bad.Count) { 'FAIL: ' + ($bad -join '; ') } else { 'ok' }), $shot)
    if ($bad.Count) { return 7 }
    if ($CheckAudio -and $audioCode -ne 0) { Write-Host "SMOKE[$Name] audio silent or no session (code $audioCode)"; return 10 }
    if (-not $size.Ok) { Write-Host ("SMOKE[{0}] client size {1}x{2} != expected {3}x{4} (requested {5}x{6}, scale {7})" -f $Name, $cw, $ch, $size.ExpectedWidth, $size.ExpectedHeight, $Width, $Height, $displayScale); return 8 }
    if ($colours -lt 50) { return 6 }
    return 0
}

$menuOk = @('using ARB2 renderSystem', 'game initialized', 'session initialized', 'Common Initialization Complete')
$menuNo = @("Couldn't load gui: 'guis/mainmenu.gui'")
$r1 = Invoke-Phase 'menu' @() $menuOk $menuNo
if ($r1 -ne 0) { exit $r1 }
$r2 = Invoke-Phase 'map' @('+devmap', $Map) ($menuOk + @('entities spawned', 'SpawnPlayer')) @('ERROR:') $true
if ($r2 -ne 0) { exit $r2 }
Write-Host 'SMOKE menu+map OK'
exit 0
