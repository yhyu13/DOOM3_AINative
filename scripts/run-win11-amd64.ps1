#Requires -Version 5.1
<#
.SYNOPSIS
  Build (optional) and run the win11-amd64 engine with a map, capture log + screenshot, report PASS/FAIL.
.DESCRIPTION
  Exit codes: 0 = map loaded, player spawned, process alive at the end, screenshot saved;
  1 = build failed or a stale build cache was refused; 3 = exe missing; 4 = process died (crash code printed); 6 = map did not load;
  7 = screenshot flat/blank; 8 = an instance of the resolved executable is already running and
  -StopExisting was not supplied; 9 = -OutDir is non-empty and -Clean was not supplied.
  Only processes whose executable path resolves to the same file are stopped, and the output
  directory is never deleted without -Clean. Paths derive from the script location; no
  machine-specific paths.
.EXAMPLE
  .\scripts\run-win11-amd64.ps1 -DataDir <data-root>\run -Map game/demo_mars_city1 -Build
.EXAMPLE
  .\scripts\run-win11-amd64.ps1 -DataDir <data-root>\run -Clean -StopExisting
#>
[CmdletBinding()]
param(
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$Map = 'game/demo_mars_city1',
    [int]$WaitSec = 80,
    [string]$OutDir,
    [string]$Exe,
    [string]$Game = '',   # optional +set fs_game <Game>; DOOM3XP.exe defaults to d3xp
    [switch]$Build,
    [switch]$Clean,        # allow replacing a non-empty -OutDir
    [switch]$StopExisting  # allow stopping instances of the same resolved executable
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$tgt = Join-Path $repo 'targets\win11-amd64'
. (Join-Path $PSScriptRoot '_common.ps1')
$exe = $Exe
if (-not $OutDir) { $OutDir = Join-Path $tgt 'build\run' }
$cmake = $null
$buildDir = $null
if ($Build) {
    $cmake = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path $cmake)) { $cmake = 'cmake' }
    $buildDir = if (Test-Path (Join-Path $repo 'build\CMakeCache.txt')) { Join-Path $repo 'build' } else { Join-Path $tgt 'build' }
    if (-not $exe) { $exe = if ($buildDir -eq (Join-Path $repo 'build')) { Join-Path $buildDir 'bin\Release\DOOM3.exe' } else { Join-Path $buildDir 'Release\DOOM3.exe' } }
}
if (-not $exe) { $exe = Resolve-Doom3Exe -Repo $repo }
$exe = [IO.Path]::GetFullPath($exe)

# A running instance locks its image, so the build cannot replace it. Check and stop before building.
if (Test-Path -LiteralPath $exe) {
    $running = @(Get-ProcessesUsingExe -Path $exe)
    if ($running.Count -gt 0) {
        if (-not $StopExisting) {
            Write-Host ("RUN {0} instance(s) of {1} already running; pass -StopExisting to stop them" -f $running.Count, $exe)
            exit 8
        }
        Stop-ProcessesUsingExe -Path $exe
    }
}

if ($Build) {
    # A cache configured for another checkout builds the wrong tree; refuse instead of rebuilding it.
    $cache = Join-Path $buildDir 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $cacheEntry = Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$' | Select-Object -First 1
        if ($cacheEntry) {
            $cacheHome = $cacheEntry.Matches[0].Groups[1].Value.Trim()
            if ([IO.Path]::GetFullPath($cacheHome).TrimEnd('\') -ine [IO.Path]::GetFullPath($repo).TrimEnd('\')) {
                Write-Host "RUN build cache $buildDir was configured for $cacheHome, not this checkout; reconfigure with 'cmake --preset win11-amd64' or remove the stale build directory"
                exit 1
            }
        }
    }
    & $cmake --build $buildDir --config Release -- -m -v:q -clp:ErrorsOnly
    if ($LASTEXITCODE -ne 0) { Write-Host "RUN build failed ($LASTEXITCODE)"; exit 1 }
}
if (-not (Test-Path -LiteralPath $exe)) { Write-Host "RUN exe missing: $exe"; exit 3 }

if (Test-Path -LiteralPath $OutDir) {
    $existing = @(Get-ChildItem -LiteralPath $OutDir -Force -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) {
        if (-not $Clean) { Write-Host "RUN refusing to overwrite non-empty OutDir: $OutDir (pass -Clean)"; exit 9 }
        Remove-Item -LiteralPath $OutDir -Recurse -Force
    }
}
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
& (Join-Path $PSScriptRoot 'capture-window.ps1') -ProcessId $p.Id -Out $shot | Tee-Object -Variable cap | Out-Host
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
$colours = 0
if ("$cap" -match 'distinctColours=(\d+)') { $colours = [int]$Matches[1] }
Write-Host ("RUN map='{0}' spawned={1} alive=True colours={2} shot={3}" -f $Map, $spawned, $colours, $shot)
if ($Map -and -not $spawned) { exit 6 }
if ($colours -lt 8) { exit 7 }
exit 0
