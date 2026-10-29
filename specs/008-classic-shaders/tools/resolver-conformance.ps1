<#
.SYNOPSIS
  Conformance harness for the fail-loud shader resolver, checked against executable vectors.

.DESCRIPTION
  Uses tools/resolver-model.ps1 (phase 1). Validates the manifest first (MANIFEST_INVALID), then
  runs every vector and compares code, decision and draw gating.

  Exit 1 when any vector fails or the manifest is invalid.
#>
[CmdletBinding()]
param(
    [string]$Vectors
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir 'resolver-model.ps1')

if (-not $Vectors) { $Vectors = Join-Path $scriptDir 'resolver-vectors.json' }
if (-not (Test-Path -LiteralPath $Vectors)) {
    Write-Error "Vectors not found: $Vectors"
    exit 1
}

$data = Get-Content -LiteralPath $Vectors -Raw -Encoding UTF8 | ConvertFrom-Json

$caseCount = @($data.cases).Count
if ($caseCount -ne 14) {
    Write-Host "VECTOR CONTRACT: expected 14 cases, found $caseCount" -ForegroundColor Red
    exit 1
}
$caseIds = @($data.cases | ForEach-Object { $_.id })
if (@($caseIds | Sort-Object -Unique).Count -ne $caseCount) {
    Write-Host "VECTOR CONTRACT: duplicate case ids" -ForegroundColor Red
    exit 1
}

$manifestProblems = @(Test-Manifest -Manifest $data.manifest)
if ($manifestProblems.Count -gt 0) {
    Write-Host "MANIFEST_INVALID ($($manifestProblems.Count))" -ForegroundColor Red
    $manifestProblems | ForEach-Object { Write-Host " - $_" }
    exit 1
}

$failures = New-Object System.Collections.ArrayList
$passed = 0

foreach ($case in $data.cases) {
    $result = Invoke-ResolverRequest -Manifest $data.manifest -Keyword $case.request.keyword -Name $case.request.name
    $decision = Get-ResolverDecision -Code $result.code -Mode $case.mode
    $draw = [bool]$result.draw

    $ok = ($result.code -eq $case.expect.code) -and ($decision -eq $case.expect.decision) -and ($draw -eq [bool]$case.expect.draw)
    if ($ok) {
        $passed++
        Write-Host ("PASS {0}  {1,-14} {2}" -f $case.id, $result.code, $decision)
    } else {
        $detail = ($result.stages.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value.code)" }) -join ' '
        [void]$failures.Add(("{0}: expected {1}/{2}/draw={3}, got {4}/{5}/draw={6} [{7} {8}]" -f `
            $case.id, $case.expect.code, $case.expect.decision, $case.expect.draw, $result.code, $decision, $draw, $detail, $result.detail))
        Write-Host ("FAIL {0}  expected {1}/{2}, got {3}/{4}" -f $case.id, $case.expect.code, $case.expect.decision, $result.code, $decision) -ForegroundColor Red
    }
}

$total = @($data.cases).Count
if ($failures.Count -gt 0) {
    Write-Host ""
    $failures | ForEach-Object { Write-Host " - $_" }
    Write-Host "RESOLVER VECTORS: $passed/$total passed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "RESOLVER VECTORS: $passed/$total passed (manifest validated: $(@($data.manifest).Count) entries)"
exit 0
