<#
.SYNOPSIS
  Verify source anchors, critical anchor content, absence claims and relative links.

.DESCRIPTION
  Three layers:
    1. every  file:line(-line)  anchor in the .md files resolves to an existing file and is inside
       the file's line count;
    2. every critical anchor in tools/anchors.json matches the expected token on that exact line,
       so a correct-looking line number pointing at the wrong statement fails;
    3. every 'absence' entry in anchors.json is actually absent from the named tracked file.

  Remote links are not fetched. Read-only. Exit 1 on any failure.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$SpecDir
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $SpecDir) { $SpecDir = Split-Path -Parent $scriptDir }
$repoRoot = Split-Path -Parent (Split-Path -Parent $SpecDir)
if (-not $EngineRoot) { $EngineRoot = Join-Path $repoRoot 'doom3_orig\doom3' }

$anchorRegex = '(?<![\w/])((?:neo|targets|docs|scripts|specs)/[A-Za-z0-9_./-]+\.(?:cpp|h|hpp|mm|txt|cmake|md|json|ps1)):(\d+)(?:-(\d+))?'
$linkRegex   = '\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)'

$anchorChecks = 0
$linkChecks   = 0
$failures     = New-Object System.Collections.ArrayList
$citations    = New-Object System.Collections.ArrayList

$mdFiles = @(Get-ChildItem -LiteralPath $SpecDir -Filter '*.md' -File -Recurse | Sort-Object FullName)

foreach ($md in $mdFiles) {
    $text  = [System.IO.File]::ReadAllText($md.FullName) -replace "`r`n", "`n"
    $lines = $text -split "`n"
    $rel   = $md.FullName.Substring($SpecDir.Length).TrimStart('\', '/')

    # ---- source anchors (range check + citation collection)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($m in [regex]::Matches($lines[$i], $anchorRegex)) {
            $anchorChecks++
            $relative = $m.Groups[1].Value
            $start = [int]$m.Groups[2].Value
            $end = if ($m.Groups[3].Success) { [int]$m.Groups[3].Value } else { $start }
            $base = if ($relative.StartsWith('neo/')) { $EngineRoot } else { $repoRoot }
            $full = Join-Path $base ($relative -replace '/', '\')

            [void]$citations.Add([pscustomobject]@{ file = $relative; start = $start; end = $end })

            if (-not (Test-Path -LiteralPath $full)) {
                [void]$failures.Add("${rel}:$($i+1) anchor file missing: $relative")
                continue
            }
            $count = @(Get-Content -LiteralPath $full -Encoding UTF8).Count
            if ($end -lt $start) {
                [void]$failures.Add("${rel}:$($i+1) anchor range reversed: $($m.Value)")
            } elseif ($end -gt $count) {
                [void]$failures.Add("${rel}:$($i+1) anchor out of range ($count lines): $($m.Value)")
            }
        }
    }

    # ---- relative links
    foreach ($m in [regex]::Matches($text, $linkRegex)) {
        $target = $m.Groups[1].Value
        if ($target -match '^(https?:|mailto:|#)') { continue }
        $target = ($target -split '#')[0]
        if (-not $target) { continue }
        $linkChecks++
        $resolved = Join-Path $md.DirectoryName ($target -replace '/', '\')
        if (-not (Test-Path -LiteralPath $resolved)) {
            [void]$failures.Add("${rel}: broken link: $target")
        }
    }
}

# ---------------------------------------------------------------- critical content assertions
$contentChecks = 0
$contentCited = 0
$uncited = New-Object System.Collections.ArrayList
$anchorsPath = Join-Path $scriptDir 'anchors.json'

if (Test-Path -LiteralPath $anchorsPath) {
    $anchors = Get-Content -LiteralPath $anchorsPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $registered = @($anchors.anchors).Count
    $declared = [int]$anchors.count
    if ($registered -ne $declared) {
        [void]$failures.Add("anchors.json declares $declared anchors but contains $registered")
    }

    foreach ($a in $anchors.anchors) {
        $file = $a.file
        $line = [int]$a.line
        $pattern = $a.match
        $base = if ($file.StartsWith('neo/')) { $EngineRoot } else { $repoRoot }
        $full = Join-Path $base ($file -replace '/', '\')

        if (-not (Test-Path -LiteralPath $full)) {
            [void]$failures.Add("anchors.json: file missing: $file")
            continue
        }
        $lines = @(Get-Content -LiteralPath $full -Encoding UTF8)
        if ($line -lt 1 -or $line -gt $lines.Count) {
            [void]$failures.Add("anchors.json: ${file}:$line out of range")
            continue
        }

        $contentChecks++
        $actual = $lines[$line - 1]
        if (-not ($actual -cmatch $pattern)) {
            [void]$failures.Add("anchors.json: $file`:$line content mismatch; expected /$pattern/, actual: $($actual.Trim())")
        }

        $isCited = @($citations | Where-Object { $_.file -eq $file -and $line -ge $_.start -and $line -le $_.end }).Count -gt 0
        if ($isCited) { $contentCited++ } else { [void]$uncited.Add("$file`:$line") }
    }

    foreach ($a in $anchors.absences) {
        $file = $a.file
        $full = Join-Path $repoRoot ($file -replace '/', '\')
        if (-not (Test-Path -LiteralPath $full)) {
            [void]$failures.Add("anchors.json absence: file missing: $file")
            continue
        }
        $actual = [System.IO.File]::ReadAllText($full)
        if ($actual -cmatch $a.match) {
            [void]$failures.Add("anchors.json absence failed: /$($a.match)/ found in $file ($($a.why))")
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host "DOC CHECK FAILED ($($failures.Count))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "ANCHORS: $anchorChecks checked (engine + repo), 0 failed"
if ($contentChecks -gt 0) {
    Write-Host "CONTENT: $contentChecks asserted, 0 failed; cited in prose: $contentCited/$contentChecks"
    if ($uncited.Count -gt 0) {
        [void]$failures.Add("registered anchors not cited in prose: $($uncited -join ', ')")
    }
}
Write-Host "LINKS:   $linkChecks checked, 0 failed"
Write-Host "DOCS OK: $anchorChecks anchors, $contentChecks content assertions, $linkChecks links"
exit 0
