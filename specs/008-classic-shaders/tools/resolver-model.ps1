<#
.SYNOPSIS
  Reusable reference model for the fail-loud shader resolver.

.DESCRIPTION
  Dot-source this file; it defines no side effects and writes nothing to the host. It is the
  single implementation used by the conformance harness, the census integration check and the
  property test, so the three cannot drift.

  The model covers phase 1 only (name -> manifest entry -> stage/status). Phase 2 (blob binding)
  is a runtime concern and is not modelled here.
#>

Set-StrictMode -Version 2.0

function Get-Prop {
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    $p = $Object.PSObject.Properties[$Name]
    if ($null -eq $p) { return $null }
    return $p.Value
}

function Get-ProgramKey {
    param([string]$Name)
    # Mirrors idStr::StripFileExtension (last '.' anywhere) plus the case-insensitive comparison
    # in R_FindARBProgram. The path prefix is intentionally preserved.
    $n = $Name -replace '\\', '/'
    $dot = $n.LastIndexOf('.')
    if ($dot -ge 0) { $n = $n.Substring(0, $dot) }
    return $n.ToLowerInvariant()
}

function Get-ManifestHits {
    param($Manifest, [string]$Key)
    return @($Manifest | Where-Object { (Get-Prop $_ 'key') -eq $Key })
}

function Resolve-ProgramStage {
    param($Manifest, [string]$Name, [string]$Target)

    if ($null -eq $Name) { $Name = '' }
    if ($Name.Length -gt 63) {
        return [ordered]@{ code = 'NAME_TOO_LONG'; key = (Get-ProgramKey $Name); target = $Target; detail = 'name exceeds 63 bytes' }
    }

    $key = Get-ProgramKey $Name
    $hits = @(Get-ManifestHits -Manifest $Manifest -Key $key)

    if ($hits.Count -eq 0) {
        return [ordered]@{ code = 'UNRESOLVED'; key = $key; target = $Target; detail = "no manifest entry for '$key'" }
    }
    if ($hits.Count -gt 1) {
        return [ordered]@{ code = 'AMBIGUOUS'; key = $key; target = $Target; detail = "$($hits.Count) manifest entries for '$key'" }
    }

    $entry = $hits[0]
    if (-not ((Get-Prop $entry 'stages') -contains $Target)) {
        return [ordered]@{ code = 'STAGE_MISSING'; key = $key; target = $Target; detail = "entry '$key' does not provide stage '$Target'" }
    }

    switch (Get-Prop $entry 'status') {
        'owned' {
            return [ordered]@{ code = 'OK'; key = $key; target = $Target; detail = "entry '$key' stage '$Target' provenance '$(Get-Prop $entry 'provenance')'" }
        }
        'stub' {
            return [ordered]@{ code = 'STUB'; key = $key; target = $Target; detail = "entry '$key' is a deliberate stub" }
        }
        'unsupported' {
            $notes = Get-Prop $entry 'notes'
            if (-not $notes) { $notes = 'declared unsupported' }
            return [ordered]@{ code = 'UNSUPPORTED'; key = $key; target = $Target; detail = "entry '$key' is unsupported: $notes" }
        }
        'absent' {
            return [ordered]@{ code = 'ABSENT'; key = $key; target = $Target; detail = "entry '$key' has no implementation" }
        }
        default {
            return [ordered]@{ code = 'MANIFEST_INVALID'; key = $key; target = $Target; detail = "entry '$key' has unknown status '$(Get-Prop $entry 'status')'" }
        }
    }
}

function Get-StagesForKeyword {
    param([string]$Keyword)
    switch ($Keyword) {
        'program'          { return @('vs', 'ps') }
        'megaTexture'      { return @('vs', 'ps') }
        'vertexProgram'    { return @('vs') }
        'fragmentProgram'  { return @('ps') }
        default            { return @() }
    }
}

function Invoke-ResolverRequest {
    param($Manifest, [string]$Keyword, [string]$Name)

    $stages = @(Get-StagesForKeyword -Keyword $Keyword)
    if ($stages.Count -eq 0) {
        return [ordered]@{
            keyword = $Keyword
            stages  = [ordered]@{}
            code    = 'MANIFEST_INVALID'
            draw    = $false
            detail  = "unknown keyword '$Keyword'"
        }
    }

    $results = [ordered]@{}
    $firstBad = $null
    foreach ($stage in $stages) {
        $r = Resolve-ProgramStage -Manifest $Manifest -Name $Name -Target $stage
        $results[$stage] = $r
        if ($null -eq $firstBad -and $r.code -ne 'OK') { $firstBad = $r }
    }

    $code = if ($null -eq $firstBad) { 'OK' } else { $firstBad.code }
    return [ordered]@{
        keyword = $Keyword
        stages  = $results
        code    = $code
        draw    = ($code -eq 'OK')
        detail  = if ($null -eq $firstBad) { 'all stages resolved' } else { $firstBad.detail }
    }
}

function Get-ResolverDecision {
    param([string]$Code, [string]$Mode)
    if ($Code -eq 'OK') { return 'draw' }
    if ($Code -eq 'MANIFEST_INVALID') { return 'refuse-start' }
    if ($Mode -eq 'strict') { return 'reject-material' }
    return 'error-material'
}

function Test-Manifest {
    param($Manifest)

    $validStatuses = @('owned', 'stub', 'unsupported', 'absent')
    $validStages   = @('vs', 'ps')
    $problems = New-Object System.Collections.ArrayList

    foreach ($entry in $Manifest) {
        $key = Get-Prop $entry 'key'
        $stages = Get-Prop $entry 'stages'
        $status = Get-Prop $entry 'status'
        $prov = Get-Prop $entry 'provenance'

        if (-not $key) { [void]$problems.Add('entry without key'); continue }
        if (-not $stages -or @($stages | Where-Object { $validStages -notcontains $_ }).Count -gt 0) {
            [void]$problems.Add("entry '$key' has invalid stages")
        }
        if ($validStatuses -notcontains $status) {
            [void]$problems.Add("entry '$key' has unknown status '$status'")
        }
        if (-not $prov) { [void]$problems.Add("entry '$key' has no provenance") }
        if ($status -eq 'owned') {
            if (-not (Get-Prop $entry 'source')) { [void]$problems.Add("owned entry '$key' has no source") }
            if (-not (Get-Prop $entry 'entry'))  { [void]$problems.Add("owned entry '$key' has no entry points") }
        }
    }

    return @($problems)
}
