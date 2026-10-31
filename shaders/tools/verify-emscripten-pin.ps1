#Requires -Version 5.1
<#
.SYNOPSIS
  Verify the pinned Emscripten identity when an emsdk is present (spec 008, T-023).

.DESCRIPTION
  The classic shader library compiles to DXIL, SPIR-V and WGSL and never invokes emcc; the wasm leg
  belongs to the web target. The pin still matters, because "it built with the emcc I happened to have"
  is the same non-reproducibility this pin file exists to stop. This script reads the `emscripten`
  block from shaders/tools/pins.json and compares it with the local `emcc --version` output.

  Discovery order: -EmccPath, then emcc/emcc.bat on PATH, then $env:EMSDK, then %USERPROFILE%\emsdk.
  The emsdk path is per host and is never recorded in the repository.

  Exit 0 = the local emcc reports the pinned version and commit.
  Exit 1 = emcc is present but reports something else, or the pin is incomplete. Fail-closed: a
           different compiler must stop a build that claims to be pinned.
  Exit 2 = SKIP: no emcc found. A skip is not a pass and must never be reported as one.

.EXAMPLE
  powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1

.EXAMPLE
  # explicit tool, used by the mutation control in evidence/implementation.md 13.2
  powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-wrong.cmd
#>
[CmdletBinding()]
param(
    [string]$EmccPath,
    [string]$PinsPath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $PinsPath) { $PinsPath = Join-Path $scriptDir 'pins.json' }

if (-not (Test-Path -LiteralPath $PinsPath)) {
    Write-Host 'EMSCRIPTEN PIN MISMATCH: pins.json missing' -ForegroundColor Red
    exit 1
}
$pins = Get-Content -LiteralPath $PinsPath -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $pins.PSObject.Properties['emscripten']) {
    Write-Host 'EMSCRIPTEN PIN MISMATCH: pins.json has no emscripten block' -ForegroundColor Red
    exit 1
}
$pin = $pins.emscripten
$version = [string]$pin.version
$commit = [string]$pin.commit
if (-not $version -or -not $commit) {
    Write-Host 'EMSCRIPTEN PIN MISMATCH: emscripten.version or emscripten.commit is empty' -ForegroundColor Red
    exit 1
}

function Find-Emcc {
    if ($EmccPath) {
        if (Test-Path -LiteralPath $EmccPath) { return (Resolve-Path -LiteralPath $EmccPath).Path }
        return $null
    }
    foreach ($name in @('emcc', 'emcc.bat')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    $roots = @()
    if ($env:EMSDK) { $roots += $env:EMSDK }
    if ($env:USERPROFILE) { $roots += (Join-Path $env:USERPROFILE 'emsdk') }
    foreach ($root in $roots) {
        $candidate = Join-Path $root 'upstream\emscripten\emcc.bat'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

$emcc = Find-Emcc
if (-not $emcc) {
    Write-Host "EMSCRIPTEN PIN SKIP: no emcc found (checked -EmccPath, PATH, EMSDK, %USERPROFILE%\emsdk)"
    Write-Host "  pinned identity: emcc $version ($commit)"
    Write-Host '  SKIP is not a pass: the web target owns the emsdk install; nothing was verified here.'
    exit 2
}

try {
    $raw = (& $emcc --version 2>&1 | Out-String)
} catch {
    Write-Host "EMSCRIPTEN PIN MISMATCH: $emcc --version raised: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
$text = $raw.Trim()

# A version must match as a whole token: 6.0.13 must not satisfy 6.0.130, and the commit is matched by
# its 12-character prefix so a build that prints either the short or the full hash still matches.
$versionOk = $text -match ('(?<![\d.])' + [regex]::Escape($version) + '(?![\d.])')
$prefix = $commit.Substring(0, [Math]::Min(12, $commit.Length))
$commitOk = $text -match [regex]::Escape($prefix)

$firstLine = ''
foreach ($line in ($text -split "`r?`n")) {
    if ($line.Trim()) { $firstLine = $line.Trim(); break }
}

if ($versionOk -and $commitOk) {
    Write-Host "EMSCRIPTEN PIN OK: emcc $version ($prefix) at $emcc"
    Write-Host "  reported: $firstLine"
    exit 0
}

Write-Host 'EMSCRIPTEN PIN MISMATCH' -ForegroundColor Red
Write-Host "  expected version $version ($commit)"
Write-Host "  version matched: $versionOk; commit matched: $commitOk"
Write-Host "  reported: $firstLine"
Write-Host "  tool: $emcc"
exit 1
