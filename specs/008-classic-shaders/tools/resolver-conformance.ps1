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

# Fixed id contract. A count of 14 is not enough: dropping R-014 (the only ABSENT vector) and adding a
# trivial case keeps the count at 14 while a whole resolver status loses its conformance case. The ids
# must match exactly, so replacing a vector is a deliberate, reviewable edit.
$expectedIds = @(1..14 | ForEach-Object { 'R-{0:000}' -f $_ })
$missingIds = @($expectedIds | Where-Object { $caseIds -notcontains $_ })
$extraIds = @($caseIds | Where-Object { $expectedIds -notcontains $_ })
if ($missingIds.Count -gt 0 -or $extraIds.Count -gt 0) {
    Write-Host "VECTOR CONTRACT: id set drift" -ForegroundColor Red
    if ($missingIds.Count -gt 0) { Write-Host " - missing: $($missingIds -join ', ')" }
    if ($extraIds.Count -gt 0) { Write-Host " - unexpected: $($extraIds -join ', ')" }
    exit 1
}

# The keyword decides the stage in the engine (vertexProgram/fragmentProgram/program). A vector that
# also states a target must state the one the keyword implies; otherwise it would claim to exercise a
# stage it never touches.
foreach ($case in $data.cases) {
    if (-not $case.PSObject.Properties.Name.Contains('request')) { continue }
    if (-not $case.request.PSObject.Properties.Name.Contains('target')) { continue }
    $reqTarget = $case.request.target
    if (-not $reqTarget) { continue }
    $implied = switch ($case.request.keyword) {
        'vertexProgram' { 'vs' }
        'fragmentProgram' { 'ps' }
        'program' { 'vs+ps' }
        default { 'unknown' }
    }
    if ($implied -ne 'vs+ps' -and $reqTarget -ne $implied) {
        Write-Host "VECTOR CONTRACT: $($case.id) keyword '$($case.request.keyword)' implies $implied but target is '$reqTarget'" -ForegroundColor Red
        exit 1
    }
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
