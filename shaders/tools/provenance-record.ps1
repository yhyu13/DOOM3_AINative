#Requires -Version 5.1
<#
.SYNOPSIS
  Provenance record check (spec 008, FR-013, FR-014, P-6).
.DESCRIPTION
  shaders/manifest/provenance.json carries one record per authored program with the SHA-256 of the file
  that was reviewed. This script makes the hash load-bearing:

    - every record's files[] entry must exist and hash to the recorded value;
    - every record with a file must answer retail_text_consulted;
    - every shipping-manifest key with a `source` must have a record;
    - the shared headers are hashed too, because editing d3_abi.hlsli changes every program.

  -Update rewrites the hashes after a deliberate edit. Updating without saying why in the PR is the
  failure this file exists to make visible.

  Exit 0 = consistent, 1 = mismatch, 2 = environment error.
.EXAMPLE
  powershell -NoProfile -File shaders/tools/provenance-record.ps1
.EXAMPLE
  powershell -NoProfile -File shaders/tools/provenance-record.ps1 -Update
#>
[CmdletBinding()]
param([switch]$Update)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$recordPath = Join-Path $repo 'shaders\manifest\provenance.json'
$manifestPath = Join-Path $repo 'shaders\manifest\classic-programs.json'
foreach ($p in @($recordPath, $manifestPath)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR: missing $p" -ForegroundColor Red; exit 2 }
}

$doc = Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$problems = New-Object System.Collections.Generic.List[string]
$checked = 0
$changed = 0

function Resolve-RepoPath([string]$relative) { Join-Path $repo ($relative -replace '/', '\') }

foreach ($record in @($doc.records)) {
    if ($null -eq $record.retail_text_consulted) { $problems.Add("$($record.id): retail_text_consulted is unanswered") }
    foreach ($f in @($record.files)) {
        if ($null -eq $f) { continue }
        $full = Resolve-RepoPath $f.path
        if (-not (Test-Path -LiteralPath $full)) { $problems.Add("$($record.id): file missing: $($f.path)"); continue }
        $hash = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
        $checked++
        if ($Update) {
            if ($f.sha256 -ne $hash) { $f.sha256 = $hash; $changed++ }
        } elseif ($f.sha256 -ne $hash) {
            $problems.Add("$($record.id): hash drift for $($f.path) (recorded $($f.sha256), actual $hash)")
        }
    }
}

foreach ($h in @($doc.shared_headers)) {
    if ($null -eq $h) { continue }
    $full = Resolve-RepoPath $h.path
    if (-not (Test-Path -LiteralPath $full)) { $problems.Add("shared header missing: $($h.path)"); continue }
    $hash = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    $checked++
    if ($Update) {
        if ($h.sha256 -ne $hash) { $h.sha256 = $hash; $changed++ }
    } elseif ($h.sha256 -ne $hash) {
        $problems.Add("hash drift for shared header $($h.path)")
    }
}

$recordIds = @{}
foreach ($record in @($doc.records)) {
    if ($record.key) { $recordIds["$($record.id)|$($record.key)"] = $true }
    foreach ($k in @($record.keys)) { if ($k) { $recordIds["$($record.id)|$k"] = $true } }
}
foreach ($entry in @($manifest.programs | Where-Object { $_.source })) {
    $hit = @($recordIds.Keys | Where-Object { $_ -like "*|$($entry.key)" })
    if ($hit.Count -ne 1) { $problems.Add("$($entry.key): expected exactly one provenance record for a program with a source, found $($hit.Count)") }
    elseif ($hit[0].Split('|')[0] -ne $entry.provenance) {
        $problems.Add("$($entry.key): manifest provenance '$($entry.provenance)' does not match record '$($hit[0].Split('|')[0])'")
    }
}

if ($Update) {
    $doc | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath
    Write-Host "PROVENANCE UPDATED: $checked hashes verified, $changed rewritten"
    exit 0
}

if ($problems.Count -ne 0) {
    Write-Host "PROVENANCE RECORD FAILED: $($problems.Count) problem(s)" -ForegroundColor Red
    $problems | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "PROVENANCE RECORD OK: $($doc.records.Count) records, $checked hashes match, every sourced program has exactly one record"
exit 0
