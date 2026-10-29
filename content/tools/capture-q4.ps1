#Requires -Version 5.1
<#
.SYNOPSIS
  Q4 capture stage (spec 009, stage 6 / T-015 / T-050).
.DESCRIPTION
  This stage exists so the gap is visible instead of silent. A capture requires the Q4 renderer, which
  spec 004 has not delivered; until it does, this script must refuse to run and must never exit 0.

  Exit codes:
    3  BLOCKED: spec 004 has not reached Q4 (the only outcome today)
    0  a capture was written, naming backend and adapter (impossible until Q4 exists)
    2  environment error

  When Q4 lands, the pass path is: run <material> on the named backend, write the capture next to the
  package, and print `CAPTURE OK: <material> <backend> <adapter>` — the exact string SC-008 looks for.
  Do not delete this refusal to make a pipeline green.
.EXAMPLE
  .\content\tools\capture-q4.ps1 -Material hero_dielectric
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Material,
    [string]$Backend,
    [string]$Adapter
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$repo = Get-RepoRoot -Start $PSScriptRoot
$pkg = Join-Path $repo "out\content\$Material\package.json"
$pkgState = if (Test-Path -LiteralPath $pkg) { 'package present' } else { 'package missing: build it with build-material.ps1' }

# The refusal is unconditional today. The check is written as "is there a Q4 renderer" rather than
# "does out/content exist", because a built package is not a renderer.
$q4Renderer = $false
if ($Backend -and $Adapter -and $q4Renderer) {
    Write-Host "CAPTURE OK: $Material $Backend $Adapter"
    exit 0
}

Write-Host "BLOCKED: spec 004 has not reached Q4"
Write-Host "  material: $Material ($pkgState)"
Write-Host '  reason:   there is no RHI path that renders a PBR material; Q2/Q3/Q4 are not delivered'
Write-Host '  next:     T-050 owns the exit-0 path; it requires a Q4 build, a backend name and an adapter name'
exit 3
