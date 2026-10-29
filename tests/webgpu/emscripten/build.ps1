# Builds the Q7a Emscripten encoder leg.
#
# Requires an emsdk installation with a WebGPU-capable Emscripten (6.0.13 was used
# for the recorded run). The path is discovered, never hard-coded:
#   -EmsdkPath <dir>  or  $env:EMSDK  or  %USERPROFILE%\emsdk
#
# The emdawnwebgpu port is fetched by Emscripten on first use from
# https://github.com/google/dawn/releases and cached under the emsdk tree.

param(
    [string]$EmsdkPath = $env:EMSDK,
    [string]$OutDir = (Join-Path $PSScriptRoot '')
)

$ErrorActionPreference = 'Stop'

if (-not $EmsdkPath) {
    foreach ($candidate in @((Join-Path $env:USERPROFILE 'emsdk'))) {
        if (Test-Path (Join-Path $candidate 'upstream\emscripten\em++.exe')) {
            $EmsdkPath = $candidate
            break
        }
    }
}
if (-not $EmsdkPath) {
    throw 'emsdk not found. Install it (see tests/webgpu/emscripten/README.md) and set EMSDK.'
}

$emxx = Join-Path $EmsdkPath 'upstream\emscripten\em++.exe'
if (-not (Test-Path $emxx)) {
    throw "em++ not found under $EmsdkPath"
}

# emsdk's launcher resolves `python` from PATH; the bundled Python 3 must win over
# any system Python 2.
$bundledPython = Get-ChildItem (Join-Path $EmsdkPath 'python') -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'python.exe') } |
    Select-Object -First 1
$env:PATH = @(
    ($(if ($bundledPython) { $bundledPython.FullName } else { $null })),
    (Join-Path $EmsdkPath 'upstream\emscripten'),
    (Join-Path $EmsdkPath 'upstream\bin'),
    $env:PATH
) -join ';'
$env:EMSDK = $EmsdkPath
$env:EM_CONFIG = Join-Path $EmsdkPath '.emscripten'

$source = Join-Path $PSScriptRoot 'conformance.cpp'
$shell = Join-Path $PSScriptRoot 'shell.html'
$output = Join-Path $OutDir 'index.html'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Write-Host ((& $emxx --version 2>&1 | Select-Object -First 1))

# Native stderr is not an error: em++ prints warnings there.
$ErrorActionPreference = 'Continue'
& $emxx $source `
    -I (Join-Path $repoRoot 'src\renderer') `
    -o $output `
    --shell-file $shell `
    --use-port=emdawnwebgpu `
    -sASYNCIFY -sALLOW_MEMORY_GROWTH -O2 -std=c++20
if ($LASTEXITCODE -ne 0) {
    throw "em++ failed with exit $LASTEXITCODE"
}
$ErrorActionPreference = 'Stop'

$port = Get-ChildItem (Join-Path $EmsdkPath 'upstream\emscripten\cache\ports') -Directory -Filter 'emdawnwebgpu*' -ErrorAction SilentlyContinue |
    Select-Object -First 1
Write-Host ("port       : " + ($(if ($port) { $port.Name } else { 'unknown' })))
Write-Host ("output     : " + $output)
Get-ChildItem $PSScriptRoot -Filter 'index.*' | ForEach-Object {
    Write-Host ("  " + $_.Name + " " + $_.Length + " bytes")
}
