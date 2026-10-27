#Requires -Version 5.1
<#
.SYNOPSIS
  Start the win11-amd64 engine twice (main menu, then the demo map), assert log lines, capture 1280x720 screenshots.
.DESCRIPTION
  Phase MENU: engine boots with game data, no startup map; log must show the ARB2 renderer, game init, session init and
              no "Couldn't load gui"; screenshot of the client area (1280x720) must not be flat.
  Phase MAP : engine boots with +devmap <Map>; log must show "entities spawned" and "SpawnPlayer"; process must still be
              alive after SettleSec; screenshot must not be flat.
  Each phase uses its own save/log directory, so logs and screenshots always belong to the same run.
  Exit codes: 0 pass; 3 exe missing; 4 process died; 5 no window; 6 flat screenshot; 7 log assertion failed;
  8 client size is not 1280x720; 10 map phase produced no audible output (peak 0). Without -DataDir the script exits 9 (SKIPPED by the caller; never a PASS).
.EXAMPLE
  .\scripts\smoke-win11-amd64.ps1 -DataDir <data-root>\run -OutDir targets\win11-amd64\build\smoke
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
    [int]$Height = 720
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot '_common.ps1')
if (-not $Exe) { $Exe = Resolve-Doom3Exe -Repo $repo }
if (-not $OutDir) { $OutDir = Join-Path $repo 'targets\win11-amd64\build\smoke' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "SMOKE exe missing: $Exe"; exit 3 }
if (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { Write-Host 'SMOKE no game data dir'; exit 9 }
$capture = Join-Path $PSScriptRoot 'capture-window.ps1'

function Invoke-Phase([string]$Name, [string[]]$Extra, [string[]]$MustMatch, [string[]]$MustNotMatch, [bool]$CheckAudio = $false) {
    Get-Process DOOM3 -ErrorAction SilentlyContinue | Stop-Process -Force
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
    $cap = & $capture -ProcessName DOOM3 -Out $shot
    $audioCode = 0
    if ($CheckAudio) {
        $au = & (Join-Path $PSScriptRoot 'audio-peak.ps1') -ProcessName DOOM3 -Ms 6000
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
    Write-Host ("SMOKE[{0}] client={1}x{2} colours={3} logChecks={4} shot={5}" -f $Name, $cw, $ch, $colours, $(if ($bad.Count) { 'FAIL: ' + ($bad -join '; ') } else { 'ok' }), $shot)
    if ($bad.Count) { return 7 }
    if ($CheckAudio -and $audioCode -ne 0) { Write-Host "SMOKE[$Name] audio silent or no session (code $audioCode)"; return 10 }
    if ($cw -ne $Width -or $ch -ne $Height) { return 8 }
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
