<#
.SYNOPSIS
  Extract, verify or render the classic ARB2 shader-program census.

.DESCRIPTION
  Source of truth is the pristine submodule doom3_orig/doom3 (read-only).
  The script reads C++ source only: no game data, no retail glprogs, no network.

  Modes:
    (default)      regenerate census.json and the generated blocks in census.md
    -Verify        regenerate in memory; exit 1 if census.json or census.md drifted
    -Html <path>   additionally write a standalone HTML preview

  Exit codes: 0 = verified / written, 1 = drift or unreadable source.

.NOTES
  Every documented count in specs/008-classic-shaders must be reproducible by this script.
  Run from anywhere; paths are derived from the script location.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [switch]$Verify,
    [string]$Html,
    [string]$OutJson
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$specDir   = Split-Path -Parent $scriptDir
$repoRoot  = Split-Path -Parent (Split-Path -Parent $specDir)

if (-not $EngineRoot) { $EngineRoot = Join-Path $repoRoot 'doom3_orig\doom3' }
if (-not (Test-Path -LiteralPath $EngineRoot)) {
    Write-Error "Engine source not found: $EngineRoot (run: git submodule update --init --recursive)"
    exit 1
}
if (-not $OutJson) { $OutJson = Join-Path $specDir 'census.json' }

$rendererDir = Join-Path $EngineRoot 'neo\renderer'
$drawArb2    = Join-Path $rendererDir 'draw_arb2.cpp'
$material    = Join-Path $rendererDir 'Material.cpp'
$sourcesCmake = Join-Path $repoRoot 'targets\win11-amd64\sources.cmake'
foreach ($required in @($drawArb2, $material)) {
    if (-not (Test-Path -LiteralPath $required)) {
        Write-Error "Required source missing: $required"
        exit 1
    }
}

function Read-Lines([string]$Path) {
    return @(Get-Content -LiteralPath $Path -Encoding UTF8)
}

# ---------------------------------------------------------------- static table
$arb2Lines = Read-Lines $drawArb2
$tableStart = -1
$tableEnd   = -1
for ($i = 0; $i -lt $arb2Lines.Count; $i++) {
    if ($arb2Lines[$i] -match 'static\s+progDef_t\s+progs\[\s*MAX_GLPROGS\s*\]\s*=\s*\{') {
        $tableStart = $i
        continue
    }
    if ($tableStart -ge 0 -and $arb2Lines[$i] -match '^\s*\}\s*;\s*$') {
        $tableEnd = $i
        break
    }
}
if ($tableStart -lt 0 -or $tableEnd -lt 0) {
    Write-Error "Could not locate the progDef_t table in $drawArb2"
    exit 1
}

$entryRegex = '\{\s*(GL_[A-Z_]+)\s*,\s*([A-Z0-9_]+)\s*,\s*"([^"]+)"\s*\}'
$table = New-Object System.Collections.ArrayList
for ($i = $tableStart; $i -le $tableEnd; $i++) {
    $m = [regex]::Match($arb2Lines[$i], $entryRegex)
    if ($m.Success) {
        [void]$table.Add([ordered]@{
            index  = $table.Count + 1
            line   = $i + 1
            target = if ($m.Groups[1].Value -eq 'GL_VERTEX_PROGRAM_ARB') { 'VP' } else { 'FP' }
            enum   = $m.Groups[2].Value
            file   = $m.Groups[3].Value
        })
    }
}
if ($table.Count -eq 0) {
    Write-Error "No table entries parsed between lines $($tableStart+1)-$($tableEnd+1)"
    exit 1
}

$maxGlProgs = $null
for ($i = 0; $i -lt $arb2Lines.Count; $i++) {
    if ($arb2Lines[$i] -match 'MAX_GLPROGS\s*=\s*(\d+)') { $maxGlProgs = [int]$Matches[1]; break }
}

# ---------------------------------------------------------------- bind sites
$sourceExtensions = @('.cpp', '.h', '.hpp', '.inl')
$cppFiles = @(Get-ChildItem -LiteralPath $rendererDir -File | Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() } | Sort-Object Name)
$bindRegex = 'qglBindProgramARB\(\s*(GL_[A-Z_]+)\s*,\s*([A-Z0-9_]+)\s*\)'
$binds = New-Object System.Collections.ArrayList
foreach ($f in $cppFiles) {
    $lines = @(Read-Lines $f.FullName)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $m = [regex]::Match($lines[$i], $bindRegex)
        if ($m.Success) {
            [void]$binds.Add([ordered]@{
                file   = 'neo/renderer/' + $f.Name
                line   = $i + 1
                target = if ($m.Groups[1].Value -eq 'GL_VERTEX_PROGRAM_ARB') { 'VP' } else { 'FP' }
                enum   = $m.Groups[2].Value
            })
        }
    }
}

# Which translation units are part of the shipped win11-amd64 build?
$compiled = @{}
if (Test-Path -LiteralPath $sourcesCmake) {
    $sc = (Read-Lines $sourcesCmake) -join "`n"
    foreach ($unit in @('draw_arb2', 'draw_common', 'draw_nv20', 'draw_r200', 'draw_nv10', 'draw_exp')) {
        $compiled[$unit] = [bool]($sc -match ("renderer/" + [regex]::Escape($unit) + '\.cpp'))
    }
} else {
    $compiled['draw_exp'] = $false
}

# ---------------------------------------------------------------- classification
$liveFiles    = @('draw_arb2.cpp', 'draw_common.cpp')
$nonArb2Files = @('draw_nv20.cpp', 'draw_r200.cpp', 'draw_nv10.cpp')

foreach ($row in $table) {
    $enum = $row.enum
    $sites = @($binds | Where-Object { $_.enum -eq $enum })
    $files = @($sites | ForEach-Object { ($_.file -split '/')[-1] } | Sort-Object -Unique)

    if ($enum -in @('VPROG_TEST', 'FPROG_TEST')) {
        $row.classification = 'debug-only'
    } elseif (@($files | Where-Object { $liveFiles -contains $_ }).Count -gt 0) {
        $row.classification = 'live-default'
    } elseif (@($files | Where-Object { $nonArb2Files -contains $_ -and $compiled[($_ -replace '\.cpp$', '')] }).Count -gt 0) {
        $row.classification = 'non-ARB2'
    } else {
        $row.classification = 'dead'
    }

    $row.boundIn = @($sites | ForEach-Object { "$($_.file):$($_.line)" })
}

# ---------------------------------------------------------------- preflight
# The classification is only valid for the backend map this script encodes. Fail closed when
# that map changes, instead of silently reporting a stale table.
$knownBindFiles = @('draw_arb2.cpp', 'draw_common.cpp', 'draw_nv20.cpp', 'draw_r200.cpp', 'draw_nv10.cpp', 'draw_exp.cpp')
$bindFiles = @($binds | ForEach-Object { ($_.file -split '/')[-1] } | Sort-Object -Unique)
$unknownBindFiles = @($bindFiles | Where-Object { $knownBindFiles -notcontains $_ })

$preflight = New-Object System.Collections.ArrayList
if ($compiled['draw_exp']) {
    [void]$preflight.Add('draw_exp.cpp is now compiled; the backend classification must be revisited')
}
if ($unknownBindFiles.Count -gt 0) {
    [void]$preflight.Add("unclassified bind source(s): $($unknownBindFiles -join ', ')")
}
if ($preflight.Count -gt 0) {
    Write-Host "CENSUS PREFLIGHT FAILED ($($preflight.Count))" -ForegroundColor Red
    $preflight | ForEach-Object { Write-Host " - $_" }
    exit 1
}

# ---------------------------------------------------------------- material requests
$matLines = Read-Lines $material
$matCalls = New-Object System.Collections.ArrayList
for ($i = 0; $i -lt $matLines.Count; $i++) {
    if ($matLines[$i] -notmatch 'R_FindARBProgram\(') { continue }
    $keyword = '(not found)'
    for ($j = $i; $j -ge [Math]::Max(0, $i - 15); $j--) {
        $km = [regex]::Match($matLines[$j], '!token\.Icmp\(\s*"([A-Za-z]+)"\s*\)')
        if ($km.Success) { $keyword = $km.Groups[1].Value; break }
    }
    [void]$matCalls.Add([ordered]@{
        line    = $i + 1
        keyword = $keyword
        target  = if ($matLines[$i] -match 'GL_VERTEX_PROGRAM_ARB') { 'VP' } else { 'FP' }
    })
}

# ---------------------------------------------------------------- counts
$distinctFiles = @($table | ForEach-Object { $_.file } | Sort-Object -Unique)
$counts = [ordered]@{
    static_entries              = $table.Count
    static_distinct_files       = $distinctFiles.Count
    live_default_entries        = @($table | Where-Object { $_.classification -eq 'live-default' }).Count
    live_default_distinct_files = @($table | Where-Object { $_.classification -eq 'live-default' } | ForEach-Object { $_.file } | Sort-Object -Unique).Count
    debug_only_entries          = @($table | Where-Object { $_.classification -eq 'debug-only' }).Count
    debug_only_distinct_files   = @($table | Where-Object { $_.classification -eq 'debug-only' } | ForEach-Object { $_.file } | Sort-Object -Unique).Count
    non_arb2_entries            = @($table | Where-Object { $_.classification -eq 'non-ARB2' }).Count
    non_arb2_distinct_files     = @($table | Where-Object { $_.classification -eq 'non-ARB2' } | ForEach-Object { $_.file } | Sort-Object -Unique).Count
    dead_entries                = @($table | Where-Object { $_.classification -eq 'dead' }).Count
    dead_distinct_files         = @($table | Where-Object { $_.classification -eq 'dead' } | ForEach-Object { $_.file } | Sort-Object -Unique).Count
    max_glprogs                 = $maxGlProgs
    free_slots                  = if ($maxGlProgs) { $maxGlProgs - $table.Count } else { $null }
    material_resolver_calls     = $matCalls.Count
    material_keywords           = @($matCalls | ForEach-Object { $_.keyword } | Sort-Object -Unique).Count
    draw_exp_compiled           = [bool]$compiled['draw_exp']
}

# ---------------------------------------------------------------- JSON
$json = [ordered]@{
    schema                     = 'doom3-classic-shader-census/1'
    engine_source              = 'doom3_orig/doom3'
    table_source               = 'doom3_orig/doom3/neo/renderer/draw_arb2.cpp'
    table_span                 = "$($tableStart + 1)-$($tableEnd + 1)"
    draw_exp_compiled          = [bool]$compiled['draw_exp']
    counts                     = $counts
    table                      = @($table)
    binds                      = @($binds)
    material_resolver_calls    = @($matCalls)
}
$jsonText = ($json | ConvertTo-Json -Depth 8) -replace "`r`n", "`n"

# ---------------------------------------------------------------- markdown blocks
$sbTable = New-Object System.Text.StringBuilder
[void]$sbTable.AppendLine('<!-- census:table:begin -->')
[void]$sbTable.AppendLine('| # | line | target | enum | `glprogs/` file | classification |')
[void]$sbTable.AppendLine('|---:|---:|---|---|---|---|')
foreach ($row in $table) {
    [void]$sbTable.AppendLine("| $($row.index) | $($row.line) | $($row.target) | ``$($row.enum)`` | ``$($row.file)`` | $($row.classification) |")
}
[void]$sbTable.AppendLine('<!-- census:table:end -->')
$mdTable = $sbTable.ToString() -replace "`r`n", "`n"

$sbCounts = New-Object System.Text.StringBuilder
[void]$sbCounts.AppendLine('<!-- census:counts:begin -->')
foreach ($k in $counts.Keys) {
    [void]$sbCounts.AppendLine("- ``$k`` = $($counts[$k])")
}
[void]$sbCounts.AppendLine('<!-- census:counts:end -->')
$mdCounts = $sbCounts.ToString() -replace "`r`n", "`n"

# ---------------------------------------------------------------- html
function ConvertTo-HtmlPreview {
    param($Rows, $Counts, $Source)
    $body = New-Object System.Text.StringBuilder
    [void]$body.AppendLine('<!doctype html><html lang="en"><meta charset="utf-8">')
    [void]$body.AppendLine('<title>Classic ARB2 shader census</title>')
    [void]$body.AppendLine('<style>body{font:14px/1.45 ui-monospace,Consolas,monospace;margin:32px;color:#111;background:#fafafa}h1{font:600 22px system-ui,sans-serif;margin:0 0 4px}p.meta{color:#555;margin:0 0 20px}table{border-collapse:collapse;background:#fff;box-shadow:0 1px 4px #0002}th,td{padding:6px 10px;border-bottom:1px solid #eee;text-align:left}th{background:#f0f0f0;font:600 12px system-ui,sans-serif;text-transform:uppercase;letter-spacing:.04em}tr.live td:last-child{color:#0a6b2d;font-weight:600}tr.debug td:last-child{color:#8a6d00}tr.non-ARB2 td:last-child{color:#444}tr.dead td:last-child{color:#a11}code{background:#f3f3f3;padding:1px 4px;border-radius:3px}.counts{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 20px}.counts span{background:#fff;border:1px solid #e5e5e5;padding:4px 8px;border-radius:4px}</style>')
    [void]$body.AppendLine("<h1>Classic ARB2 shader-program census</h1>")
    [void]$body.AppendLine("<p class=""meta"">source: <code>$Source</code> &middot; generated by <code>tools/census.ps1</code> &middot; verified against the pristine submodule</p>")
    [void]$body.AppendLine('<div class="counts">')
    foreach ($k in @('static_entries', 'static_distinct_files', 'live_default_entries', 'debug_only_entries', 'non_arb2_entries', 'dead_entries', 'max_glprogs')) {
        [void]$body.AppendLine("<span>$k <b>$($Counts[$k])</b></span>")
    }
    [void]$body.AppendLine('</div>')
    [void]$body.AppendLine('<table><thead><tr><th>#</th><th>line</th><th>target</th><th>enum</th><th>file</th><th>classification</th></tr></thead><tbody>')
    foreach ($r in $Rows) {
        $cls = $r.classification
        [void]$body.AppendLine("<tr class=""$cls""><td>$($r.index)</td><td>$($r.line)</td><td>$($r.target)</td><td><code>$($r.enum)</code></td><td><code>$($r.file)</code></td><td>$cls</td></tr>")
    }
    [void]$body.AppendLine('</tbody></table></html>')
    return $body.ToString()
}

# ---------------------------------------------------------------- write / verify
$problems = New-Object System.Collections.ArrayList

if ($Html) {
    $htmlText = ConvertTo-HtmlPreview -Rows $table -Counts $counts -Source 'doom3_orig/doom3/neo/renderer/draw_arb2.cpp'
    [System.IO.File]::WriteAllText($Html, $htmlText, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "wrote $Html"
}

function Replace-Block([string]$text, [string]$begin, [string]$end, [string]$block) {
    $pattern = "(?s)" + [regex]::Escape($begin) + ".*?" + [regex]::Escape($end)
    if ($text -notmatch $pattern) { return $null }
    return [regex]::Replace($text, $pattern, $block.TrimEnd("`n"))
}

$censusMd = Join-Path $specDir 'census.md'
$current  = if (Test-Path -LiteralPath $censusMd) { [System.IO.File]::ReadAllText($censusMd) -replace "`r`n", "`n" } else { '' }

if ($Verify) {
    if (-not (Test-Path -LiteralPath $OutJson)) {
        [void]$problems.Add("missing $OutJson")
    } else {
        $onDisk = ([System.IO.File]::ReadAllText($OutJson) -replace "`r`n", "`n").Trim()
        if ($onDisk -ne $jsonText.Trim()) { [void]$problems.Add("census.json differs from source extraction") }
    }
    $htmlPath = Join-Path $specDir 'evidence\census.html'
    if (Test-Path -LiteralPath $htmlPath) {
        $expectedHtml = (ConvertTo-HtmlPreview -Rows $table -Counts $counts -Source 'doom3_orig/doom3/neo/renderer/draw_arb2.cpp') -replace "`r`n", "`n"
        $actualHtml = ([System.IO.File]::ReadAllText($htmlPath)) -replace "`r`n", "`n"
        if ($expectedHtml.TrimEnd() -ne $actualHtml.TrimEnd()) {
            [void]$problems.Add("evidence/census.html is stale; regenerate with -Html")
        }
    }

    if (-not $current) {
        [void]$problems.Add("missing census.md")
    } else {
        $patched = Replace-Block $current '<!-- census:table:begin -->' '<!-- census:table:end -->' $mdTable
        if ($null -eq $patched) { [void]$problems.Add("census.md has no census:table block") }
        elseif (($patched -replace "`r`n", "`n") -ne $current) { [void]$problems.Add("census.md table block differs from source extraction") }

        $patched2 = Replace-Block $current '<!-- census:counts:begin -->' '<!-- census:counts:end -->' $mdCounts
        if ($null -eq $patched2) { [void]$problems.Add("census.md has no census:counts block") }
        elseif (($patched2 -replace "`r`n", "`n") -ne $current) { [void]$problems.Add("census.md counts block differs from source extraction") }
    }

    if ($problems.Count -gt 0) {
        Write-Host "CENSUS DRIFT ($($problems.Count))" -ForegroundColor Red
        $problems | ForEach-Object { Write-Host " - $_" }
        exit 1
    }
    Write-Host ("CENSUS OK: {0} entries, {1} files, {2} live-default entries, draw_exp compiled={3}" -f `
        $counts['static_entries'], $counts['static_distinct_files'], $counts['live_default_entries'], $counts['draw_exp_compiled'])
    exit 0
}

[System.IO.File]::WriteAllText($OutJson, $jsonText, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "wrote $OutJson"

if ($current) {
    $next = Replace-Block $current '<!-- census:table:begin -->' '<!-- census:table:end -->' $mdTable
    if ($null -eq $next) { Write-Host 'CENSUS FAILED: census.md has no census:table block' -ForegroundColor Red; exit 1 }
    else {
        $next2 = Replace-Block $next '<!-- census:counts:begin -->' '<!-- census:counts:end -->' $mdCounts
        if ($null -eq $next2) { Write-Host 'CENSUS FAILED: census.md has no census:counts block' -ForegroundColor Red; exit 1 }
        else { [System.IO.File]::WriteAllText($censusMd, $next2, (New-Object System.Text.UTF8Encoding($false))) ; Write-Host "updated $censusMd" }
    }
}

Write-Host ("entries={0} files={1} live={2} debug={3} non-arb2={4} dead={5} material_calls={6}" -f `
    $counts['static_entries'], $counts['static_distinct_files'], $counts['live_default_entries'], `
    $counts['debug_only_entries'], $counts['non_arb2_entries'], $counts['dead_entries'], $counts['material_resolver_calls'])
exit 0
