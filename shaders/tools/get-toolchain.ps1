#Requires -Version 5.1
<#
.SYNOPSIS
  Materialise and verify the pinned DXC toolchain (spec 008, T-023).
.DESCRIPTION
  Reads shaders/tools/pins.json, downloads the pinned release assets into the git-ignored tool cache
  when they are missing, verifies each asset hash and (for DXC) the compiler version, and prints the
  tool paths.

  DXC is materialised from a zip. Tint comes from a Dawn release tarball (~118 MB); only
  <root>/bin/tint.exe is extracted, and the binary is identified by the asset hash plus the Dawn
  commit, because this tint build answers no --version.

  Fail-closed: a hash mismatch, a version mismatch, a missing tar/gh client, or an extracted binary
  that does not run exits non-zero. A shader build must never proceed on an unverified compiler.

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

# --- Tint: SPIR-V -> WGSL translation and WGSL validation -----------------------------------------
$tintPin = $pins.tint
$tintCache = Join-Path $repo ($tintPin.cacheRelativePath -replace '/', '\')
$tintExe = Join-Path $tintCache 'bin\tint.exe'
$tintArchive = Join-Path $tintCache (Split-Path -Leaf $tintPin.asset)

function Test-PinnedTint {
    if (-not (Test-Path -LiteralPath $tintExe)) { return $false }
    try {
        $help = (& $tintExe --help 2>&1 | Out-String)
    } catch {
        return $false
    }
    return ($help -match 'Usage: tint')
}

if ($Force -and (Test-Path -LiteralPath $tintArchive)) { Remove-Item -LiteralPath $tintArchive -Force }
if ($Force -and (Test-Path -LiteralPath $tintCache)) { Remove-Item -LiteralPath $tintCache -Recurse -Force }

if (-not (Test-PinnedTint)) {
    New-Item -ItemType Directory -Force -Path $tintCache | Out-Null
    if (-not (Test-Path -LiteralPath $tintArchive)) {
        $gh = Get-Command gh -ErrorAction SilentlyContinue
        if (-not $gh) {
            Write-Host "ERROR: tint missing and gh CLI unavailable; fetch $($tintPin.url) into $tintCache manually." -ForegroundColor Red
            exit 2
        }
        Write-Host "fetching $($tintPin.release) / $($tintPin.asset) (~118 MB)"
        & gh release download $tintPin.release --repo google/dawn --pattern $tintPin.asset --dir $tintCache --clobber
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $tintArchive)) {
            Write-Host "ERROR: download failed (exit $LASTEXITCODE)" -ForegroundColor Red
            exit 2
        }
    }

    $tintHash = (Get-FileHash -LiteralPath $tintArchive -Algorithm SHA256).Hash
    if ($tintHash.ToUpperInvariant() -ne $tintPin.assetSha256.ToUpperInvariant()) {
        Write-Host "ERROR: tint asset hash mismatch" -ForegroundColor Red
        Write-Host "  expected $($tintPin.assetSha256)"
        Write-Host "  actual   $tintHash"
        exit 3
    }

    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if (-not $tar) {
        Write-Host "ERROR: tar.exe unavailable; cannot extract $($tintPin.asset)" -ForegroundColor Red
        exit 2
    }
    $member = (& tar -tf $tintArchive | Where-Object { $_ -match $tintPin.extractMemberPattern } | Select-Object -First 1)
    if (-not $member) {
        Write-Host "ERROR: $($tintPin.asset) contains no bin/tint.exe" -ForegroundColor Red
        exit 3
    }
    $staging = Join-Path $tintCache '_extract'
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $staging | Out-Null
    & tar -xzf $tintArchive -C $staging $member
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: extracting $member failed (exit $LASTEXITCODE)" -ForegroundColor Red
        exit 3
    }
    $extracted = Join-Path $staging ($member -replace '/', '\')
    if (-not (Test-Path -LiteralPath $extracted)) {
        Write-Host "ERROR: expected $extracted after extraction" -ForegroundColor Red
        exit 3
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $tintExe) | Out-Null
    Move-Item -LiteralPath $extracted -Destination $tintExe -Force
    Remove-Item -LiteralPath $staging -Recurse -Force

    if (-not (Test-PinnedTint)) {
        Write-Host "ERROR: extracted tint does not answer --help; the asset is not a usable build" -ForegroundColor Red
        exit 3
    }
}

Write-Host "TOOLCHAIN OK: tint $($tintPin.dawnCommit.Substring(0, 12)) at $tintExe"
Write-Host "  asset sha256 verified: $($tintPin.assetSha256)"
Write-Host "  version: no --version in this build; the pin is the asset hash plus the Dawn commit"
if ($PassThru) { Write-Output $exe }
exit 0
