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

# There is no Q4 renderer to drive. The script refuses rather than pretending, and it also refuses to
# invent a pass path: a "CAPTURE OK" line without a written image would be a lie the matrix would then
# display as a capture. When Q4 lands, T-050 replaces this block with a real capture and writes
# out/content/<material>/capture.json (backend, adapter, image, hash), which is what the matrix reads.
if (-not $Backend -or -not $Adapter) {
    Write-Host "BLOCKED: spec 004 has not reached Q4"
    Write-Host "  material: $Material ($pkgState)"
    Write-Host '  reason:   there is no RHI path that renders a PBR material; Q2/Q3/Q4 are not delivered'
    Write-Host '  next:     T-050 owns the exit-0 path; it requires a Q4 build, a backend name and an adapter name'
    exit 3
}

Write-Host 'CAPTURE NOT IMPLEMENTED: a Q4 build was named, but no capture path exists yet'
Write-Host "  backend: $Backend"
Write-Host "  adapter: $Adapter"
Write-Host '  refusing to print CAPTURE OK without writing an image: T-050 implements this, not this script'
exit 2
