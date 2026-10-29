#Requires -Version 5.1
<#
.SYNOPSIS
  Materialise and verify the pinned DXC toolchain (spec 008, T-023).
.DESCRIPTION
  Reads shaders/tools/pins.json, downloads the pinned release asset into the git-ignored tool cache
  when it is missing, verifies the asset hash and the compiler version, and prints the dxc path.

  Fail-closed: a hash mismatch, a version mismatch, or a missing network/gh client exits non-zero.
  A shader build must never proceed on an unverified compiler.

  Exit 0 = toolchain ready, 2 = environment error, 3 = hash/version mismatch.
.EXAMPLE
  powershell -NoProfile -File shaders/tools/get-toolchain.ps1
.EXAMPLE
  $dxc = powershell -NoProfile -File shaders/tools/get-toolchain.ps1 -PassThru
#>
[CmdletBinding()]
param(
    [switch]$PassThru,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$pinsPath = Join-Path $PSScriptRoot 'pins.json'
if (-not (Test-Path -LiteralPath $pinsPath)) { Write-Host 'ERROR: pins.json missing' ; exit 2 }
$pins = Get-Content -LiteralPath $pinsPath -Raw | ConvertFrom-Json
$dxcPin = $pins.dxc

$cache = Join-Path $repo ($dxcPin.cacheRelativePath -replace '/', '\')
$exe = Join-Path $cache 'bin\x64\dxc.exe'
$zip = Join-Path $cache $dxcPin.asset

function Test-PinnedCompiler {
    if (-not (Test-Path -LiteralPath $exe)) { return $false }
    try {
        $version = (& $exe --version 2>&1 | Out-String)
    } catch {
        return $false
    }
    return ($version -like "*$($dxcPin.compilerVersion)*")
}

if ($Force -and (Test-Path -LiteralPath $zip)) { Remove-Item -LiteralPath $zip -Force }
if ($Force -and (Test-Path -LiteralPath $cache)) { Remove-Item -LiteralPath $cache -Recurse -Force }

if (-not (Test-PinnedCompiler)) {
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    if (-not (Test-Path -LiteralPath $zip)) {
        $gh = Get-Command gh -ErrorAction SilentlyContinue
        if (-not $gh) {
            Write-Host "ERROR: dxc missing and gh CLI unavailable; fetch $($dxcPin.url) into $cache manually." -ForegroundColor Red
            exit 2
        }
        Write-Host "fetching $($dxcPin.release) / $($dxcPin.asset)"
        & gh release download $dxcPin.release --repo microsoft/DirectXShaderCompiler --pattern $dxcPin.asset --dir $cache --clobber
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $zip)) {
            Write-Host "ERROR: download failed (exit $LASTEXITCODE)" -ForegroundColor Red
            exit 2
        }
    }

    $hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
    if ($hash.ToUpperInvariant() -ne $dxcPin.assetSha256.ToUpperInvariant()) {
        Write-Host "ERROR: asset hash mismatch" -ForegroundColor Red
        Write-Host "  expected $($dxcPin.assetSha256)"
        Write-Host "  actual   $hash"
        exit 3
    }

    Expand-Archive -LiteralPath $zip -DestinationPath $cache -Force
    if (-not (Test-PinnedCompiler)) {
        Write-Host "ERROR: extracted compiler does not report $($dxcPin.compilerVersion)" -ForegroundColor Red
        if (Test-Path -LiteralPath $exe) { & $exe --version }
        exit 3
    }
}

$reported = (& $exe --version 2>&1 | Out-String).Trim()
Write-Host "TOOLCHAIN OK: dxc $($dxcPin.compilerVersion) at $exe"
Write-Host "  asset sha256 verified: $($dxcPin.assetSha256)"
Write-Host "  reported: $reported"
if ($PassThru) { Write-Output $exe }
exit 0
