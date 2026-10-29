<#
.SYNOPSIS
  Cross-artifact integration check: census -> authoring plan -> resolver model.

.DESCRIPTION
  Proves three things that the individual gates cannot:
    1. the source-derived census still has the documented shape;
    2. every census key is covered exactly once by the authoring plan, with matching stages,
       and every provenance id used by the plan exists in provenance.md;
    3. a manifest built from the plan resolves every static engine request to the expected code
       (live/debug -> OK, removed backends and dead entries -> ABSENT, megaTexture -> UNSUPPORTED).

  This is a plan check, not a claim that the shaders exist: the plan's 'authored' entries map to
  'owned' only inside this test.

  Exit 1 on any mismatch.
#>
[CmdletBinding()]
param(
    [string]$CensusPath,
    [string]$PlanPath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$specDir = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent (Split-Path -Parent $specDir)
. (Join-Path $scriptDir 'resolver-model.ps1')

if (-not $CensusPath) { $CensusPath = Join-Path $specDir 'census.json' }
if (-not $PlanPath) { $PlanPath = Join-Path $scriptDir 'authoring-plan.json' }
foreach ($p in @($CensusPath, $PlanPath)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Error "Missing input: $p"; exit 1 }
}

$census = Get-Content -LiteralPath $CensusPath -Raw -Encoding UTF8 | ConvertFrom-Json
$plan = Get-Content -LiteralPath $PlanPath -Raw -Encoding UTF8 | ConvertFrom-Json
$provenanceText = Get-Content -LiteralPath (Join-Path $specDir 'provenance.md') -Raw -Encoding UTF8
$provenanceIds = @([regex]::Matches($provenanceText, 'PV-\d{3}') | ForEach-Object { $_.Value } | Sort-Object -Unique)

$problems = New-Object System.Collections.ArrayList

# Map key -> provenance id from the register table in provenance.md. Group records (PV-008,
# PV-009) list several keys in one row, so membership alone is not enough: the id must belong to
# the key.
$provByKey = @{}
foreach ($line in ($provenanceText -split "`n")) {
    if ($line -match '^\|\s*(PV-\d{3})\s*\|\s*([^|]+)\|') {
        $id = $Matches[1]
        $cell = $Matches[2]
        foreach ($m in [regex]::Matches($cell, '`([a-z0-9_]+)`')) {
            $k = $m.Groups[1].Value
            if ($provByKey.ContainsKey($k) -and $provByKey[$k] -ne $id) {
                [void]$problems.Add("provenance.md maps '$k' to both $($provByKey[$k]) and $id")
            }
            $provByKey[$k] = $id
        }
    }
}

# ---------------------------------------------------------------- 1. documented census shape
$expected = [ordered]@{
    static_entries        = 18
    static_distinct_files = 13
    live_default_entries  = 8
    debug_only_entries    = 2
    non_arb2_entries      = 5
    dead_entries          = 3
}
foreach ($k in $expected.Keys) {
    $actual = $census.counts.$k
    if ($actual -ne $expected[$k]) {
        [void]$problems.Add("census.$k is $actual, expected $($expected[$k])")
    }
}
if ($census.draw_exp_compiled) { [void]$problems.Add('draw_exp.cpp is compiled; classification assumptions are stale') }

$expectedLiveFiles = @('interaction.vfp', 'bumpyEnvironment.vfp', 'environment.vfp', 'shadow.vp', 'arbFP_glasswarp.txt')
$actualLiveFiles = @($census.table | Where-Object { $_.classification -eq 'live-default' } | ForEach-Object { $_.file } | Sort-Object -Unique)
$diffLive = Compare-Object -ReferenceObject ($expectedLiveFiles | Sort-Object) -DifferenceObject $actualLiveFiles
if ($diffLive) { [void]$problems.Add("live-default file set changed: $(($diffLive | ForEach-Object { $_.InputObject }) -join ', ')") }

# ---------------------------------------------------------------- 2. plan coverage
$censusKeys = [ordered]@{}
foreach ($row in $census.table) {
    $key = Get-ProgramKey $row.file
    $stage = if ($row.target -eq 'VP') { 'vs' } else { 'ps' }
    if (-not $censusKeys.Contains($key)) { $censusKeys[$key] = @{ stages = @(); classes = @() } }
    $v = $censusKeys[$key]
    if ($v.stages -notcontains $stage) { $v.stages = @($v.stages + $stage) }
    if ($v.classes -notcontains $row.classification) { $v.classes = @($v.classes + $row.classification) }
    $censusKeys[$key] = $v
}

$planByKey = @{}
foreach ($e in $plan.entries) {
    $key = Get-Prop $e 'key'
    if (-not $key) { [void]$problems.Add('plan entry without key'); continue }
    if ($planByKey.ContainsKey($key)) { [void]$problems.Add("plan has duplicate key '$key'"); continue }
    $planByKey[$key] = $e
}

foreach ($key in $censusKeys.Keys) {
    if (-not $planByKey.ContainsKey($key)) {
        [void]$problems.Add("plan misses census key '$key'")
        continue
    }
    $planStages = @($planByKey[$key].stages | Sort-Object)
    $censusStages = @($censusKeys[$key].stages | Sort-Object)
    if (($planStages -join ',') -ne ($censusStages -join ',')) {
        [void]$problems.Add("stage mismatch for '$key': plan=$($planStages -join '+') census=$($censusStages -join '+')")
    }
}

$allowedExtra = @('megatexture')
foreach ($key in $planByKey.Keys) {
    if (-not $censusKeys.Contains($key) -and $allowedExtra -notcontains $key) {
        [void]$problems.Add("plan contains key '$key' that is neither in the census nor an allowed data-forced key")
    }
}

foreach ($e in $plan.entries) {
    $key = Get-Prop $e 'key'
    $prov = Get-Prop $e 'provenance'
    if (-not $prov) { [void]$problems.Add("plan entry '$key' has no provenance"); continue }
    $expectedProv = $provByKey[$key]
    if ($provenanceIds -notcontains $prov) {
        [void]$problems.Add("plan entry '$key' uses '$prov', absent from provenance.md")
    } elseif (-not $expectedProv) {
        [void]$problems.Add("plan entry '$key' has no row in the provenance register")
    } elseif ($prov -ne $expectedProv) {
        [void]$problems.Add("plan entry '$key' uses '$prov' but the register assigns '$expectedProv'")
    }
    $planSource = Get-Prop $e 'source'
    if ($planSource) {
        $fullSource = Join-Path $repoRoot ($planSource -replace '/', '\')
        if (-not (Test-Path -LiteralPath $fullSource)) {
            [void]$problems.Add("plan entry '$key' names a source that does not exist: $planSource")
        }
    }
}

# ---------------------------------------------------------------- 3. build a test manifest
$manifest = New-Object System.Collections.ArrayList
foreach ($e in $plan.entries) {
    $target = Get-Prop $e 'target_status'
    $status = switch ($target) {
        'authored'    { 'owned' }
        'unsupported' { 'unsupported' }
        'absent'      { 'absent' }
        default       { $target }
    }
    $m = [ordered]@{
        key        = $e.key
        stages     = $e.stages
        status     = $status
        provenance = $e.provenance
    }
    if ($status -eq 'owned') {
        $m.source = $e.source
        $m.entry = $e.entry
    } else {
        $m.notes = $e.notes
    }
    [void]$manifest.Add([pscustomobject]$m)
}

$manifestProblems = @(Test-Manifest -Manifest $manifest)
foreach ($p in $manifestProblems) { [void]$problems.Add("test manifest: $p") }

# ---------------------------------------------------------------- 4. resolve every static request
$rows = New-Object System.Collections.ArrayList
$okCount = 0
$absentCount = 0
foreach ($row in $census.table) {
    $keyword = if ($row.target -eq 'VP') { 'vertexProgram' } else { 'fragmentProgram' }
    $r = Invoke-ResolverRequest -Manifest $manifest -Keyword $keyword -Name $row.file
    $expectedCode = switch ($row.classification) {
        'live-default' { 'OK' }
        'debug-only'   { 'OK' }
        'non-ARB2'     { 'ABSENT' }
        'dead'         { 'ABSENT' }
        default        { 'UNKNOWN' }
    }
    if ($r.code -ne $expectedCode) {
        [void]$problems.Add("$($row.file) [$($row.classification)] resolved $($r.code), expected $expectedCode")
    }
    if ($r.code -eq 'OK') { $okCount++ }
    if ($r.code -eq 'ABSENT') { $absentCount++ }
    [void]$rows.Add([pscustomobject]@{
        file = $row.file
        target = $row.target
        class = $row.classification
        code = $r.code
        expected = $expectedCode
        ok = ($r.code -eq $expectedCode)
    })
}

# The six authored keys must be OK when requested on every declared stage.
$authoredKeys = @($plan.entries | Where-Object { $_.target_status -eq 'authored' })
foreach ($e in $authoredKeys) {
    foreach ($stage in $e.stages) {
        $kw = if ($stage -eq 'vs') { 'vertexProgram' } else { 'fragmentProgram' }
        $r = Invoke-ResolverRequest -Manifest $manifest -Keyword $kw -Name "$($e.key).vfp"
        if ($r.code -ne 'OK') { [void]$problems.Add("authored key '$($e.key)' stage $stage resolved $($r.code)") }
    }
}

# megaTexture is data-forced and must be reported, never drawn.
$mega = Invoke-ResolverRequest -Manifest $manifest -Keyword 'megaTexture' -Name 'megaTexture.vfp'
if ($mega.code -ne 'UNSUPPORTED' -or $mega.draw) {
    [void]$problems.Add("megaTexture resolved $($mega.code) draw=$($mega.draw), expected UNSUPPORTED/false")
}

# ---------------------------------------------------------------- report
$rows | Format-Table -AutoSize | Out-String -Width 120 | Write-Host

if ($problems.Count -gt 0) {
    Write-Host "INTEGRATION FAILED ($($problems.Count))" -ForegroundColor Red
    $problems | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host ("INTEGRATION OK: {0} requests ({1} OK, {2} ABSENT), {3} plan keys, " -f `
    $census.table.Count, $okCount, $absentCount, $plan.entries.Count) -NoNewline
Write-Host ("{0} authored, {1} provenance ids; megaTexture={2}" -f `
    $authoredKeys.Count, $provenanceIds.Count, $mega.code)
exit 0
