#Requires -Version 5.1
<#
.SYNOPSIS
  Deterministic-compile check plus golden hash comparison (spec 008, T-023).
.DESCRIPTION
  Compiles the shipping set twice into two separate output directories and requires every DXIL,
  SPIR-V and WGSL blob to be byte-identical between the runs. A non-deterministic shader build makes
  a golden hash meaningless, so this runs before the goldens are trusted.

  Then compares the per-stage hashes with shaders/manifest/compile-goldens.json, which records the
  pinned toolchain version alongside the hashes. A toolchain bump invalidates every golden on purpose;
  re-record with -Update and say so in the PR.

  Exit 0 = identical and matching, 1 = mismatch, 2 = environment error.
.EXAMPLE
  powershell -NoProfile -File shaders/tools/verify-golden.ps1
.EXAMPLE
  powershell -NoProfile -File shaders/tools/verify-golden.ps1 -Update
#>
[CmdletBinding()]
param([switch]$Update)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$compile = Join-Path $PSScriptRoot 'compile.ps1'
$goldensPath = Join-Path $repo 'shaders\manifest\compile-goldens.json'
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json

$runA = Join-Path $repo 'out\golden-a'
$runB = Join-Path $repo 'out\golden-b'
foreach ($d in @($runA, $runB)) { if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force } }

& powershell -NoProfile -ExecutionPolicy Bypass -File $compile -All -OutDir $runA | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: first compile failed' -ForegroundColor Red; exit 2 }
& powershell -NoProfile -ExecutionPolicy Bypass -File $compile -All -OutDir $runB | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: second compile failed' -ForegroundColor Red; exit 2 }

$a = Get-Content -LiteralPath (Join-Path $runA 'compile-summary.json') -Raw | ConvertFrom-Json
$b = Get-Content -LiteralPath (Join-Path $runB 'compile-summary.json') -Raw | ConvertFrom-Json

if ($a.results.Count -ne $b.results.Count) {
    Write-Host "ERROR: compile counts differ ($($a.results.Count) vs $($b.results.Count))" -ForegroundColor Red
    exit 1
}

$current = @{}
$nondeterministic = New-Object System.Collections.Generic.List[string]
foreach ($r in $a.results) {
    $match = @($b.results | Where-Object { $_.key -eq $r.key -and $_.stage -eq $r.stage })
    if ($match.Count -ne 1) { $nondeterministic.Add("$($r.key)/$($r.stage): not found in second run"); continue }
    $o = $match[0]
    if ($o.dxil_sha -ne $r.dxil_sha) { $nondeterministic.Add("$($r.key)/$($r.stage): DXIL differs between runs") }
    if ($o.spv_sha -ne $r.spv_sha) { $nondeterministic.Add("$($r.key)/$($r.stage): SPIR-V differs between runs") }
    if ($o.wgsl_sha -ne $r.wgsl_sha) { $nondeterministic.Add("$($r.key)/$($r.stage): WGSL differs between runs") }
    $current["$($r.key)/$($r.stage)"] = [pscustomobject]@{ dxil = $r.dxil_sha; spv = $r.spv_sha; wgsl = $r.wgsl_sha; source = $r.source_sha }
}

if ($nondeterministic.Count -ne 0) {
    Write-Host "GOLDEN FAILED: build is not deterministic ($($nondeterministic.Count) mismatch(es))" -ForegroundColor Red
    $nondeterministic | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

if ($Update) {
    $doc = [pscustomobject]@{
        schema = 'doom3-shader-goldens/1'
        dxc    = $pins.dxc.compilerVersion
        asset  = $pins.dxc.asset
        tint   = $pins.tint.dawnCommit
        tintAssetSha256 = $pins.tint.assetSha256
        stages = $current
    }
    $doc | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $goldensPath
    Write-Host "GOLDEN UPDATED: $($current.Count) stage hashes recorded for dxc $($pins.dxc.compilerVersion) and tint $($pins.tint.dawnCommit.Substring(0, 12))"
    exit 0
}

if (-not (Test-Path -LiteralPath $goldensPath)) {
    Write-Host "ERROR: $goldensPath missing; run with -Update once the toolchain pin is accepted" -ForegroundColor Red
    exit 2
}
$goldens = Get-Content -LiteralPath $goldensPath -Raw | ConvertFrom-Json
if ($goldens.dxc -ne $pins.dxc.compilerVersion) {
    Write-Host "ERROR: goldens recorded for dxc $($goldens.dxc), pin is $($pins.dxc.compilerVersion); re-record deliberately" -ForegroundColor Red
    exit 1
}
if ($goldens.tint -ne $pins.tint.dawnCommit) {
    Write-Host "ERROR: goldens recorded for tint $($goldens.tint), pin is $($pins.tint.dawnCommit); re-record deliberately" -ForegroundColor Red
    exit 1
}

$mismatches = New-Object System.Collections.Generic.List[string]
foreach ($k in $current.Keys) {
    $g = $goldens.stages.$k
    if ($null -eq $g) { $mismatches.Add("${k}: no golden (new stage?)"); continue }
    if ($g.dxil -ne $current[$k].dxil) { $mismatches.Add("${k}: DXIL hash drift") }
    if ($g.spv -ne $current[$k].spv) { $mismatches.Add("${k}: SPIR-V hash drift") }
    if ($g.wgsl -ne $current[$k].wgsl) { $mismatches.Add("${k}: WGSL hash drift") }
}
foreach ($k in @($goldens.stages.PSObject.Properties.Name)) {
    if (-not $current.ContainsKey($k)) { $mismatches.Add("${k}: golden exists but stage was not compiled") }
}

if ($mismatches.Count -ne 0) {
    Write-Host "GOLDEN FAILED: $($mismatches.Count) mismatch(es)" -ForegroundColor Red
    $mismatches | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "GOLDEN OK: $($current.Count) stage hashes (DXIL, SPIR-V, WGSL) deterministic across two runs and equal to the recorded goldens (dxc $($pins.dxc.compilerVersion), tint $($pins.tint.dawnCommit.Substring(0, 12)))"
exit 0
