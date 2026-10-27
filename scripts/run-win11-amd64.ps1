#Requires -Version 5.1
<#
.SYNOPSIS
  Build (optional) and run the win11-amd64 engine with a map, capture log + screenshot, report PASS/FAIL.
.DESCRIPTION
  Exit codes: 0 = map loaded, player spawned, process alive at the end, screenshot saved;
  1 = build failed; 3 = exe missing; 4 = process died (crash code printed); 6 = map did not load;
  7 = screenshot flat/blank. Paths derive from the script location; no machine-specific paths.
.EXAMPLE
  .\scripts\run-win11-amd64.ps1 -DataDir <data-root>\run -Map game/demo_mars_city1 -Build
#>
[CmdletBinding()]
param(
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$Map = 'game/demo_mars_city1',
    [int]$WaitSec = 80,
    [string]$OutDir,
    [string]$Exe,
    [string]$Game = '',   # optional +set fs_game <Game>; DOOM3XP.exe defaults to d3xp
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$tgt = Join-Path $repo 'targets\win11-amd64'
. (Join-Path $PSScriptRoot '_common.ps1')
$exe = $Exe
if (-not $OutDir) { $OutDir = Join-Path $tgt 'build\run' }
Get-Process DOOM3 -ErrorAction SilentlyContinue | Stop-Process -Force
if ($Build) {
    $cmake = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path $cmake)) { $cmake = 'cmake' }
    $buildDir = if (Test-Path (Join-Path $repo 'build\CMakeCache.txt')) { Join-Path $repo 'build' } else { Join-Path $tgt 'build' }
    if (-not $Exe) { $exe = if ($buildDir -eq (Join-Path $repo 'build')) { Join-Path $buildDir 'bin\Release\DOOM3.exe' } else { Join-Path $buildDir 'Release\DOOM3.exe' } }
    & $cmake --build $buildDir --config Release -- -m -v:q -clp:ErrorsOnly
    if ($LASTEXITCODE -ne 0) { Write-Host "RUN build failed ($LASTEXITCODE)"; exit 1 }
}
if (-not $exe) { $exe = Resolve-Doom3Exe -Repo $repo }
if (-not (Test-Path -LiteralPath $exe)) { Write-Host "RUN exe missing: $exe"; exit 3 }
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$a = @('+set', 'logFile', '2', '+set', 'r_fullscreen', '0', '+set', 'fs_savepath', (Join-Path $OutDir 'save'))
if (-not $Game -and $exe -and (Split-Path -Leaf $exe) -ieq 'DOOM3XP.exe') { $Game = 'd3xp' }
if ($Game) { $a += @('+set', 'fs_game', $Game) }
if ($DataDir) { $a += @('+set', 'fs_basepath', $DataDir) }
if ($Map) { $a += @('+devmap', $Map) }
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $WaitSec -and -not $p.HasExited) { Start-Sleep -Seconds 2 }
$log = Join-Path $OutDir 'save\base\qconsole.log'
$logText = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
if ($logText) { Copy-Item $log (Join-Path $OutDir 'qconsole.log') -Force }
if ($p.HasExited) { Write-Host ("RUN process died, exit code 0x{0:X}" -f $p.ExitCode); exit 4 }
$spawned = $logText -match 'entities spawned' -and $logText -match 'SpawnPlayer'
$shot = Join-Path $OutDir 'screenshot.png'
& (Join-Path $PSScriptRoot 'capture-window.ps1') -ProcessName DOOM3 -Out $shot | Tee-Object -Variable cap | Out-Host
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
$colours = 0
if ("$cap" -match 'distinctColours=(\d+)') { $colours = [int]$Matches[1] }
Write-Host ("RUN map='{0}' spawned={1} alive=True colours={2} shot={3}" -f $Map, $spawned, $colours, $shot)
if ($Map -and -not $spawned) { exit 6 }
if ($colours -lt 8) { exit 7 }
exit 0
