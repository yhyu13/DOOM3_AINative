#Requires -Version 5.1
<#
.SYNOPSIS
  Test: DOOM3XP.exe (Resurrection of Evil game code, statically linked) boots and fails cleanly (task T066).

.DESCRIPTION
  Launches ONLY a uniquely renamed copy of the exe (DOOM3XP_test_<pid>.exe in -OutDir), by PID, and
  kills it by PID. No retail d3xp data is needed or used (none is available to the test).
    1  PE check: size > 1 MB, machine 0x8664 (x64)
    2  menu boot, probe d3xp-only cvars (g_grabberHoldSeconds, g_enableSlowmo): proves the d3xp game
       module is the one running
    3  +set fs_game d3xp + devmap d3xp/erebus1 without d3xp data: clean "Can't find map", back to menu
    4  demo map under the d3xp game code: entities spawn, then a clean Error for the RoE-only weapon
       def, engine returns to the main menu

.PARAMETER Exe      DOOM3XP.exe. Default: DOOM3XP.exe in the directory of the exe chosen by Resolve-Doom3Exe.
.PARAMETER DataDir  Directory containing base\ . Default: $env:D3_DATA_DIR.
.PARAMETER OutDir   Scratch dir. Default: <repo-root>\build\test-d3xp.
.PARAMETER Sec      Seconds each run is left alive (default 35).

.EXAMPLE
  $env:D3_DATA_DIR = '<dir containing base>'
  .\scripts\test-d3xp.ps1
.EXAMPLE
  .\scripts\test-d3xp.ps1 -Exe .\build\bin\Release\DOOM3XP.exe -DataDir <dir> -OutDir .\build\t

.NOTES
  Exit codes: 0 all checks passed, 1 at least one check failed, 3 setup problem.
  Weakness: the d3xp-only map content (erebus levels, RoE weapons) is not exercised, no data exists here.
#>
param(
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [int]$Sec = 35
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_common.ps1')
$Repo = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path (Split-Path -Parent (Resolve-Doom3Exe -Repo $Repo)) 'DOOM3XP.exe' }
if (-not $OutDir) { $OutDir = Join-Path $Repo 'build\test-d3xp' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "FAIL: exe missing: $Exe"; exit 3 }
if (-not $DataDir -or -not (Test-Path -LiteralPath (Join-Path $DataDir 'base'))) { Write-Host 'FAIL: -DataDir (or D3_DATA_DIR) must contain base\'; exit 3 }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

$script:fail = 0
function Check([bool]$ok, [string]$msg) { if ($ok) { Write-Host "PASS: $msg" } else { Write-Host "FAIL: $msg"; $script:fail++ } }

# 1. PE check
$b = [IO.File]::ReadAllBytes($Exe); $o = [BitConverter]::ToInt32($b, 0x3C); $m = [BitConverter]::ToUInt16($b, $o + 4)
Check ($b.Length -gt 1MB) ("size {0} bytes > 1 MB" -f $b.Length)
Check ($m -eq 0x8664) ("PE machine 0x{0:X} == 0x8664 (x64)" -f $m)

$copy = Join-Path $OutDir ("DOOM3XP_test_{0}.exe" -f $PID)
Copy-Item -LiteralPath $Exe -Destination $copy -Force

function Read-Shared([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    try {
        $fs = New-Object IO.FileStream($path, 'Open', 'Read', 'ReadWrite,Delete')
        $sr = New-Object IO.StreamReader($fs, [Text.Encoding]::GetEncoding(28591))
        try { return $sr.ReadToEnd() } finally { $sr.Dispose() }
    } catch { return '' }
}
function Run([string]$Name, [string[]]$Extra, [string]$Cfg) {
    $save = Join-Path $OutDir $Name
    if (Test-Path -LiteralPath $save) { Remove-Item -LiteralPath $save -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Join-Path $save 'base') | Out-Null
    if ($Cfg) { Set-Content -LiteralPath (Join-Path $save 'base\probe.cfg') -Value $Cfg -Encoding ASCII }
    $a = @('+set','fs_basepath',$DataDir,'+set','fs_savepath',$save,'+set','logFile','2','+set','r_fullscreen','0','+set','win_allowMultipleInstances','1') + $Extra
    $p = Start-Process -FilePath $copy -ArgumentList $a -PassThru
    $t = 0
    try {
        while ($t -lt $Sec -and -not $p.HasExited) { Start-Sleep -Seconds 1; $t++ }
        $alive = -not $p.HasExited; $code = $null
        if ($alive) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } else { $code = $p.ExitCode }
    } finally {
        if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    }
    Start-Sleep -Seconds 1
    [pscustomobject]@{ Alive = $alive; Code = $code; Text = (Read-Shared (Join-Path $save 'base\qconsole.log')) }
}

# 2. menu boot, probe d3xp-only cvars (absent in the base game module)
$r = Run 'menu' @('+exec','probe') "g_grabberHoldSeconds`r`ng_enableSlowmo"
Check $r.Alive "menu boot: process alive after ${Sec}s (no crash)"
Check ($r.Text -match 'game initialized') 'log: game initialized'
Check ($r.Text -match 'session initialized') 'log: session initialized'
Check ($r.Text -match '"g_grabberHoldSeconds" is:"3"') 'd3xp game module active: d3xp-only cvar g_grabberHoldSeconds exists'
Check ($r.Text -notmatch "Unknown command 'g_grabberHoldSeconds'") 'no Unknown command for d3xp cvar'

# 3. fs_game d3xp without d3xp data, then a d3xp map that is not there
$r = Run 'missing' @('+set','fs_game','d3xp','+devmap','d3xp/erebus1') $null
Check $r.Alive 'missing d3xp data: process alive (no crash)'
Check ($r.Text -match "Can't find map maps/d3xp/erebus1\.map") "log: clean ""Can't find map maps/d3xp/erebus1.map"""
Check ($r.Text -match 'triggering mainmenu watchdog') 'log: session fell back to main menu'
Check ($r.Text -match 'd3xp') 'log: search path contains d3xp'

# 4. demo map under the d3xp game code: expected clean Error (RoE weapon def missing), back at menu
$r = Run 'demomap' @('+devmap','game/demo_mars_city1') $null
Check $r.Alive 'demo map under d3xp code: process alive (no crash)'
Check ($r.Text -match 'entities spawned') 'log: map entities spawned by d3xp game code'
Check ($r.Text -match "ERROR: Unknown weapon in decl 'weapon_bloodstone_passive'") 'log: expected clean Error for RoE-only weapon def'
Check ($r.Text -match 'triggering mainmenu watchdog') 'log: engine returned to main menu after the Error'

Remove-Item -LiteralPath $copy -Force -ErrorAction SilentlyContinue
if ($script:fail) { Write-Host "RESULT FAIL ($script:fail)"; exit 1 } else { Write-Host 'RESULT PASS'; exit 0 }
