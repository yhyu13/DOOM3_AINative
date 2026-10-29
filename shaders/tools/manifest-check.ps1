#Requires -Version 5.1
<#
.SYNOPSIS
  Shipping-manifest conformance check (spec 008, FR-009, FR-013, SC-009).
.DESCRIPTION
  Proves that shaders/manifest/classic-programs.json is the single source of truth and cannot drift from
  the census or from the authored files:

    1. it parses, has the expected schema, and every required field is present;
    2. its key set equals the census key set (13 static files after extension stripping) plus the
       data-forced megaTexture.vfp;
    3. `absent` keys are exactly the census dead + non-ARB2 keys, so nothing dead is authored and
       nothing authored is silently dead;
    4. every `stages` list is a non-empty subset of vs/ps;
    5. every declared `source` exists, and every declared entry point is present in that source;
    6. every `provenance` id appears in the register in specs/008-classic-shaders/provenance.md and no
       synthetic PV-Txx id leaks into the shipping manifest;
    7. `owned` entries satisfy the definition in ownership.md section 5, so an entry cannot claim
       ownership without a source, an entry point per stage and a resolvable provenance id.

  Fail-closed: any violation exits 1. Counts are printed as one line for `shader_status` parity.
.EXAMPLE
  powershell -NoProfile -File shaders/tools/manifest-check.ps1
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manifestPath = Join-Path $repo 'shaders\manifest\classic-programs.json'
$censusPath = Join-Path $repo 'specs\008-classic-shaders\census.json'
$provenancePath = Join-Path $repo 'specs\008-classic-shaders\provenance.md'

foreach ($p in @($manifestPath, $censusPath, $provenancePath)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR: missing $p" -ForegroundColor Red; exit 2 }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$census = Get-Content -LiteralPath $censusPath -Raw | ConvertFrom-Json
$provenanceText = [System.IO.File]::ReadAllText($provenancePath)
$violations = New-Object System.Collections.Generic.List[string]

if ($manifest.schema -ne 'doom3-classic-programs/1') { $violations.Add("schema is '$($manifest.schema)', expected doom3-classic-programs/1") }

$allowedStatus = @('owned', 'stub', 'unsupported', 'absent')
$entries = @{}
foreach ($p in @($manifest.programs)) {
    if (-not $p.key) { $violations.Add('an entry has no key'); continue }
    if ($entries.ContainsKey($p.key)) { $violations.Add("duplicate key '$($p.key)' in the shipping manifest") }
    $entries[$p.key] = $p

    if (-not $p.status) { $violations.Add("$($p.key): missing status") }
    elseif ($allowedStatus -notcontains $p.status) { $violations.Add("$($p.key): status '$($p.status)' is not one of $($allowedStatus -join ', ')") }
    if (-not $p.provenance) { $violations.Add("$($p.key): missing provenance id") }

    $stages = @($p.stages)
    if ($stages.Count -eq 0) { $violations.Add("$($p.key): empty stages") }
    foreach ($s in $stages) { if (@('vs', 'ps') -notcontains $s) { $violations.Add("$($p.key): stage '$s' is not vs/ps") } }

    if ($p.provenance -and $provenanceText -notmatch [regex]::Escape($p.provenance)) {
        $violations.Add("$($p.key): provenance '$($p.provenance)' is not in the register")
    }
    if ($p.provenance -and $p.provenance -like 'PV-T*') {
        $violations.Add("$($p.key): synthetic provenance id '$($p.provenance)' must not ship")
    }

    if ($p.source) {
        $src = Join-Path $repo ($p.source -replace '/', '\')
        if (-not (Test-Path -LiteralPath $src)) {
            $violations.Add("$($p.key): source '$($p.source)' does not exist")
        } else {
            $text = [System.IO.File]::ReadAllText($src)
            foreach ($s in $stages) {
                $entryName = "${s}_main"
                if (-not $p.entry -or -not ($p.entry.PSObject.Properties.Name -contains $s)) {
                    $violations.Add("$($p.key): declares stage '$s' but has no entry point for it")
                } elseif ($text -notmatch ("\b" + [regex]::Escape($entryName) + "\b")) {
                    $violations.Add("$($p.key): entry '$entryName' not found in $($p.source)")
                }
            }
        }
    }

    if ($p.status -eq 'owned') {
        if (-not $p.source) { $violations.Add("$($p.key): owned without a source (ownership.md section 5)") }
        if (-not $p.entry) { $violations.Add("$($p.key): owned without entry points (ownership.md section 5)") }
        foreach ($s in $stages) {
            if (-not $p.entry -or -not ($p.entry.PSObject.Properties.Name -contains $s)) {
                $violations.Add("$($p.key): owned but stage '$s' has no entry point")
            }
        }
    }
    if (($p.status -eq 'absent' -or $p.status -eq 'unsupported') -and -not $p.notes) {
        $violations.Add("$($p.key): $($p.status) entries need a one-sentence reason in notes")
    }
}

# Census key set: the distinct glprogs files, extension stripped and lowercased, plus megaTexture.vfp.
$censusKeys = New-Object System.Collections.Generic.HashSet[string]
$censusClasses = @{}
foreach ($row in @($census.table)) {
    $file = $row.file
    if (-not $file) { continue }
    $key = ($file -replace '\.[^.]*$', '').ToLowerInvariant()
    [void]$censusKeys.Add($key)
    if (-not $censusClasses.ContainsKey($key)) { $censusClasses[$key] = New-Object System.Collections.Generic.List[string] }
    $censusClasses[$key].Add($row.classification)
}
[void]$censusKeys.Add('megatexture')

foreach ($k in $censusKeys) {
    if (-not $entries.ContainsKey($k)) { $violations.Add("census key '$k' is missing from the shipping manifest (FR-009)") }
}
foreach ($k in $entries.Keys) {
    if (-not $censusKeys.Contains($k)) { $violations.Add("manifest key '$k' is not in the census and is not the data-forced megaTexture") }
}

foreach ($k in $censusClasses.Keys) {
    if (-not $entries.ContainsKey($k)) { continue }
    $classes = $censusClasses[$k]
    $expected = 'stub'
    if ($classes -contains 'dead' -or $classes -contains 'non-ARB2') { $expected = 'absent' }
    $actual = $entries[$k].status
    if ($expected -eq 'absent' -and $actual -ne 'absent') {
        $violations.Add("$k is census $($classes -join '/') but the manifest says '$actual' (dead and removed-backend programs must be absent)")
    }
    if ($expected -eq 'stub' -and $actual -eq 'absent') {
        $violations.Add("$k is census $($classes -join '/') but the manifest marks it absent")
    }
}

if ($entries.ContainsKey('megatexture') -and $entries['megatexture'].status -ne 'unsupported') {
    $violations.Add("megatexture must be unsupported in milestone 1 (decision D-003), found '$($entries['megatexture'].status)'")
}

$counts = @{}
foreach ($s in $allowedStatus) { $counts[$s] = 0 }
foreach ($p in $entries.Values) { if ($counts.ContainsKey($p.status)) { $counts[$p.status]++ } }

if ($violations.Count -ne 0) {
    Write-Host "MANIFEST FAILED: $($violations.Count) violation(s)" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host ("MANIFEST OK: keys={0} owned={1} stub={2} unsupported={3} absent={4}" -f `
        $entries.Count, $counts['owned'], $counts['stub'], $counts['unsupported'], $counts['absent'])
exit 0
