#Requires -Version 5.1
<#
.SYNOPSIS
  Run the in-engine resolver contract test (spec 008 T-024) against a built DOOM3.exe.

.DESCRIPTION
  Boots the engine with the demo data, runs `shader_selftest`, prints `shader_status`, and asserts the
  five contract behaviours plus the manifest/table counts in the engine's own qconsole.log. This is not
  a ctest: it needs a real GL context and game data, so it is the reviewable command that proves the
  resolver is wired into the executable, not just into the standalone core.

  Exit 0 = every marker found; 1 = a marker is missing; 2 = inputs missing or the process failed;
  3 = the engine did not write a log.

.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File tests/shader/run-engine-selftest.ps1 `
      -Exe build/bin/Release/DOOM3.exe -DataDir <demo-run-dir> -OutDir build/run-shader-selftest
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$DataDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "ERROR exe not found: $Exe"
    exit 2
}
if (-not (Test-Path -LiteralPath (Join-Path $DataDir 'base'))) {
    Write-Host "ERROR data dir has no base/: $DataDir"
    exit 2
}

$exe = (Resolve-Path -LiteralPath $Exe).Path
$save = Join-Path $OutDir 'save'
New-Item -ItemType Directory -Force -Path (Join-Path $save 'base') | Out-Null

$arguments = @(
    '+set', 'logFile', '2',
    '+set', 'r_fullscreen', '0',
    '+set', 'fs_basepath', $DataDir,
    '+set', 'fs_savepath', $save,
    '+set', 'r_shaderMode', 'off',
    '+shader_selftest',
    '+shader_status',
    '+quit'
)
$quoted = ($arguments | ForEach-Object {
    if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
}) -join ' '

Write-Host "RUN $exe $quoted"
$p = Start-Process -FilePath $exe -ArgumentList $quoted -PassThru
$exited = $p.WaitForExit($TimeoutSec * 1000)
if (-not $exited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Write-Host "ERROR engine did not exit within $TimeoutSec s"
    exit 2
}
if ($p.ExitCode -ne 0) {
    Write-Host ("ERROR engine exit 0x{0:X}" -f $p.ExitCode)
    exit 2
}

$log = Join-Path $save 'base\qconsole.log'
if (-not (Test-Path -LiteralPath $log)) {
    Write-Host "ERROR no qconsole.log at $log"
    exit 3
}

$text = Get-Content -LiteralPath $log -Raw
$markers = @(
    'SHADER MANIFEST COVERAGE: 18/18 engine table keys covered',
    'SHADER SELFTEST: PASS missing -> UNRESOLVED, material rejected, no draw',
    'SHADER SELFTEST: PASS one-sided -> both stages blocked',
    'SHADER SELFTEST: PASS compile-failed -> phase 2 blocks the draw',
    'SHADER SELFTEST: PASS tolerant -> error material, no draw',
    'SHADER SELFTEST: PASS reload -> before/after report',
    'SHADER SELFTEST: PASS 5/5',
    'shader_status: manifest=14 owned=0 stub=6 unsupported=1 absent=7'
)
$missing = @()
foreach ($marker in $markers) {
    if ($text -notlike "*$marker*") { $missing += $marker }
}

Copy-Item -LiteralPath $log -Destination (Join-Path $OutDir 'qconsole.log') -Force
if ($missing.Count -gt 0) {
    Write-Host "ENGINE SELFTEST FAIL: $($missing.Count) marker(s) missing"
    $missing | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

$status = ($text -split "`r?`n" | Where-Object { $_ -like 'shader_status:*' } | Select-Object -Last 1)
Write-Host "ENGINE SELFTEST OK: 8/8 markers"
Write-Host "  $status"
exit 0
