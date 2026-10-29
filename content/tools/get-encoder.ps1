#Requires -Version 5.1
<#
.SYNOPSIS
  Materialise and verify the pinned texture encoder (spec 009, T-002).
.DESCRIPTION
  Reads content/tools/pins.json, downloads the pinned texconv.exe into the git-ignored tool cache when it
  is missing, and verifies both the SHA-256 and the version line. Fail-closed: a hash mismatch, a version
  mismatch or no download client exits non-zero, so no gate ever runs on an unverified encoder.

  Exit 0 = ready, 2 = environment error, 3 = hash or version mismatch.
.EXAMPLE
  powershell -NoProfile -File content/tools/get-encoder.ps1
#>
[CmdletBinding()]
param(
    [switch]$PassThru,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$repo = Get-RepoRoot -Start $PSScriptRoot
$pins = Read-Json (Join-Path $PSScriptRoot 'pins.json')
$pin = $pins.texconv
$exe = Join-Path $repo ($pin.cacheRelativePath -replace '/', '\')
$cache = Split-Path -Parent $exe

function Test-PinnedEncoder {
    if (-not (Test-Path -LiteralPath $exe)) { return $false }
    try { $line = (& $exe -h 2>&1 | Out-String) } catch { return $false }
    return ($line -like "*$($pin.version)*")
}

if ($Force -and (Test-Path -LiteralPath $exe)) { Remove-Item -LiteralPath $exe -Force }

if (-not (Test-PinnedEncoder)) {
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        Write-Host "ERROR: texconv missing and gh is unavailable; fetch $($pin.url) to $exe manually." -ForegroundColor Red
        exit 2
    }
    Write-Host "fetching $($pin.release) / $($pin.asset)"
    & gh release download $pin.release --repo microsoft/DirectXTex --pattern $pin.asset --dir $cache --clobber 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $exe)) {
        Write-Host 'ERROR: download failed' -ForegroundColor Red
        exit 2
    }
    $hash = Get-Sha256 $exe
    if ($hash -ne $pin.sha256) {
        Write-Host "ERROR: encoder hash mismatch`n  expected $($pin.sha256)`n  actual   $hash" -ForegroundColor Red
        exit 3
    }
}

$size = (Get-Item -LiteralPath $exe).Length
if ($size -ne $pin.bytes) {
    Write-Host "ERROR: encoder size $size does not match the pin $($pin.bytes)" -ForegroundColor Red
    exit 3
}
$hash = Get-Sha256 $exe
if ($hash -ne $pin.sha256) {
    Write-Host "ERROR: encoder hash mismatch`n  expected $($pin.sha256)`n  actual   $hash" -ForegroundColor Red
    exit 3
}
$line = (& $exe -h 2>&1 | Out-String)
if ($line -notlike "*$($pin.version)*") {
    Write-Host "ERROR: encoder version does not match the pin $($pin.version)" -ForegroundColor Red
    exit 3
}

Write-Host "ENCODER OK: texconv $($pin.version), $size bytes, sha256 $hash"
if ($PassThru) { Write-Output $exe }
exit 0
