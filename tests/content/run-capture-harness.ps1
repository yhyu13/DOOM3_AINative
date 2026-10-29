#Requires -Version 5.1
<#
.SYNOPSIS
  Test the Q4 capture harness (spec 009 stage 6) without a Q4 renderer.
.DESCRIPTION
  capture-q4.ps1 must accept exactly one situation -- a build that reports the claimed backend and adapter
  and writes a real frame -- and refuse every other one. This suite drives it with a stub renderer that
  breaks one thing at a time, so the refusal paths are tested rather than assumed.

  Nothing here is Q4 evidence: the stub is not a renderer, and its output is written under out/ only. The
  suite asserts the real capture index still says 0 captures.

  Exit 0 when every case behaves; 1 otherwise.
.EXAMPLE
  powershell -NoProfile -File tests/content/run-capture-harness.ps1
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$capture = Join-Path $repo 'content\tools\capture-q4.ps1'
$build = Join-Path $repo 'content\tools\build-material.ps1'
$matrix = Join-Path $repo 'content\tools\matrix-report.ps1'
$stub = Join-Path $PSScriptRoot 'fixtures\fake-rhi.cmd'
$work = Join-Path $repo 'out\capture-test'
$contentDir = Join-Path $work 'content'
$pkgDir = Join-Path $contentDir 'hero_dielectric'

foreach ($p in @($capture, $build, $matrix, $stub)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ENVIRONMENT ERROR: missing $p" -ForegroundColor Red; exit 2 }
}

if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Item -ItemType Directory -Force -Path $pkgDir | Out-Null

& powershell -NoProfile -ExecutionPolicy Bypass -File $build -Material hero_dielectric -OutDir $pkgDir | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'ENVIRONMENT ERROR: could not build the test package' -ForegroundColor Red; exit 2 }

$script:failures = 0
function Invoke-Case {
    param(
        [string]$Name,
        [hashtable]$Params,
        [int]$ExpectExit,
        [string]$ExpectText
    )
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $capture @Params 2>&1 | Out-String
    $code = $LASTEXITCODE
    $ok = ($code -eq $ExpectExit)
    if ($ok -and $ExpectText) { $ok = ($output -match [regex]::Escape($ExpectText)) }
    if ($ok) {
        Write-Host ("{0}: ok (exit {1})" -f $Name, $code)
    } else {
        Write-Host ("{0}: FAILED (exit {1}, expected {2})" -f $Name, $code, $ExpectExit) -ForegroundColor Red
        ($output.Trim() -split "`n" | Select-Object -First 4) | ForEach-Object { Write-Host "    $_" }
        $script:failures++
    }
}

$frame = Join-Path $work 'frame.png'

# 1. No build at all: the only state that exists today.
Invoke-Case -Name 'blocked-without-build' -ExpectExit 3 -ExpectText 'BLOCKED: spec 004 has not reached Q4' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir
}

# 2. A build that reports no RHI identity is not a Q4 build, even if it exits 0 and writes a frame.
Invoke-Case -Name 'identity-missing' -ExpectExit 4 -ExpectText 'IDENTITY FAILED' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;no-identity;-Capture;$frame"
}

# 3. A different adapter than claimed is a different machine: refuse rather than mislabel the evidence.
Invoke-Case -Name 'identity-mismatch' -ExpectExit 4 -ExpectText 'reported a different backend or adapter' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;wrong-adapter;-Capture;$frame"
}

# 4. A blank frame is not a capture.
Invoke-Case -Name 'blank-frame' -ExpectExit 5 -ExpectText 'distinct colours' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;blank;-Capture;$frame"
}

# 5. No frame at all.
Invoke-Case -Name 'no-frame' -ExpectExit 5 -ExpectText 'wrote no screenshot' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;no-image"
}

# 6. A build that fails is not a capture, even if it printed an identity line.
Invoke-Case -Name 'engine-failed' -ExpectExit 6 -ExpectText 'the build exited 1' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;exit-1;-Capture;$frame"
}

# 7. The good case: identity matches, the frame is real, so a record is written.
$captureJson = Join-Path $pkgDir 'capture.json'
if (Test-Path -LiteralPath $captureJson) { Remove-Item -LiteralPath $captureJson -Force }
Invoke-Case -Name 'accepted' -ExpectExit 0 -ExpectText 'CAPTURE OK: hero_dielectric d3d12 Fake RHI Adapter' -Params @{
    Material = 'hero_dielectric'; OutDir = $pkgDir; Exe = $stub; Backend = 'd3d12'; Adapter = 'Fake RHI Adapter'
    Screenshot = $frame; EngineArgs = "-Mode;ok;-Capture;$frame"
}
if (Test-Path -LiteralPath $captureJson) {
    $record = Get-Content -LiteralPath $captureJson -Raw | ConvertFrom-Json
    $problems = @()
    if ($record.backend -ne 'd3d12') { $problems += 'backend' }
    if ($record.adapter -ne 'Fake RHI Adapter') { $problems += 'adapter' }
    if ($record.image_sha256 -notmatch '^[0-9a-f]{64}$') { $problems += 'image_sha256' }
    if ($record.package_sha256 -notmatch '^[0-9a-f]{64}$') { $problems += 'package_sha256' }
    if ($record.image_distinct -lt 16) { $problems += 'image_distinct' }
    if (-not (Test-Path -LiteralPath (Join-Path $pkgDir 'capture.png'))) { $problems += 'capture.png' }
    if ($problems.Count -eq 0) {
        Write-Host 'record-fields: ok (backend, adapter, hashes, distinct colours, image)'
    } else {
        Write-Host ("record-fields: FAILED ({0})" -f ($problems -join ', ')) -ForegroundColor Red
        $script:failures++
    }
} else {
    Write-Host 'record-fields: FAILED (capture.json missing)' -ForegroundColor Red
    $script:failures++
}

# 8. The matrix reads the record rather than a hard-coded string.
$matrixOut = & powershell -NoProfile -ExecutionPolicy Bypass -File $matrix -OutDir $contentDir 2>&1 | Out-String
if ($matrixOut -match 'captured \(d3d12 / Fake RHI Adapter\)') {
    Write-Host 'matrix-reads-record: ok'
} else {
    Write-Host 'matrix-reads-record: FAILED (matrix did not pick up capture.json)' -ForegroundColor Red
    ($matrixOut.Trim() -split "`n" | Select-Object -First 3) | ForEach-Object { Write-Host "    $_" }
    $script:failures++
}

# 9. The real capture index must still say zero: the stub is not evidence.
$index = Join-Path $repo 'specs\009-rhi-hd-content\evidence\captures.md'
if (Test-Path -LiteralPath $index) {
    $text = [System.IO.File]::ReadAllText($index)
    if ($text -match 'Captures on file: 0') {
        Write-Host 'index-still-zero: ok'
    } else {
        Write-Host 'index-still-zero: FAILED (the capture index no longer says 0)' -ForegroundColor Red
        $script:failures++
    }
}

Write-Host ''
if ($script:failures -ne 0) {
    Write-Host ("CAPTURE HARNESS FAILED: {0} case(s)" -f $script:failures) -ForegroundColor Red
    exit 1
}
Write-Host 'CAPTURE HARNESS OK: 1 blocked + 5 refusals + 1 accepted + record + matrix + index'
exit 0
