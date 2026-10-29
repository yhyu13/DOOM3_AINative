<#
.SYNOPSIS
  Run every gate and write a browser-verifiable review report.

.DESCRIPTION
  Executes the six checks, captures their exit codes and output, verifies that each produced its
  success marker, reads census.json and the task summary from tasks.md, and writes
  evidence/report.html. The HTML is static and self-contained so a browser can assert on it.

  Exit 1 when any gate fails or its success marker is missing (the report is still written).
#>
[CmdletBinding()]
param(
    [string]$Out
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$specDir   = Split-Path -Parent $scriptDir
$repoRoot  = Split-Path -Parent (Split-Path -Parent $specDir)
if (-not $Out) { $Out = Join-Path $specDir 'evidence\report.html' }

function HtmlEncode([string]$s) {
    if ($null -eq $s) { return '' }
    return [System.Net.WebUtility]::HtmlEncode($s)
}

function Get-LastLine([string]$text) {
    $lines = @($text -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
    if ($lines.Count -eq 0) { return '' }
    return $lines[$lines.Count - 1]
}

$gates = @(
    [pscustomobject]@{ id = 'census';      name = 'Census drift';            file = 'census.ps1';               extra = @('-Verify'); expect = 'CENSUS OK' },
    [pscustomobject]@{ id = 'resolver';    name = 'Resolver vectors';        file = 'resolver-conformance.ps1'; extra = @();          expect = 'RESOLVER VECTORS: 14/14' },
    [pscustomobject]@{ id = 'integration'; name = 'Census-plan integration'; file = 'integration-check.ps1';    extra = @();          expect = 'INTEGRATION OK' },
    [pscustomobject]@{ id = 'fuzz';        name = 'Resolver property test';  file = 'fuzz-resolver.ps1';        extra = @();          expect = 'FUZZ OK' },
    [pscustomobject]@{ id = 'docs';        name = 'Anchors and links';       file = 'verify-docs.ps1';          extra = @();          expect = 'DOCS OK' },
    [pscustomobject]@{ id = 'provenance';  name = 'Provenance gate';         file = 'provenance-check.ps1';     extra = @();          expect = 'PROVENANCE OK' }
)

$results = New-Object System.Collections.ArrayList
foreach ($g in $gates) {
    $path = Join-Path $scriptDir $g.file
    $psArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $path) + $g.extra
    $output = & powershell @psArgs 2>&1 | Out-String
    $code = $LASTEXITCODE
    $marker = ($output -cmatch [regex]::Escape($g.expect))
    $note = ''
    if ($code -eq 0 -and -not $marker) { $note = "expected marker '$($g.expect)' missing" }
    $summary = Get-LastLine $output
    if ($note) { $summary = "$summary [$note]" }
    [void]$results.Add([pscustomobject]@{
        id = $g.id
        name = $g.name
        command = ("powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/$($g.file) " + ($g.extra -join ' ')).Trim()
        code = $code
        pass = ($code -eq 0 -and $marker)
        summary = $summary
        output = $output.Trim()
    })
}

$census = Get-Content -LiteralPath (Join-Path $specDir 'census.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$counts = $census.counts

$tasksText = Get-Content -LiteralPath (Join-Path $specDir 'tasks.md') -Raw -Encoding UTF8
$specDone = 0; $specTotal = 0; $implDone = 0; $implTotal = 0; $gateDone = 0; $gateTotal = 0; $allDone = 0; $allTotal = 0
if ($tasksText -match '\| Specification \(S\) \| (\d+) \| (\d+) \|') { $specDone = [int]$Matches[1]; $specTotal = [int]$Matches[2] }
if ($tasksText -match '\| Implementation \(I\) \| (\d+) \| (\d+) \|') { $implDone = [int]$Matches[1]; $implTotal = [int]$Matches[2] }
if ($tasksText -match '\| Gates \(G\) \| (\d+) \| (\d+) \|') { $gateDone = [int]$Matches[1]; $gateTotal = [int]$Matches[2] }
if ($tasksText -match '\| \*\*All\*\* \| \*\*(\d+)\*\* \| \*\*(\d+)\*\*') { $allDone = [int]$Matches[1]; $allTotal = [int]$Matches[2] }

$contentAssertions = '?'
$docsOut = ($results | Where-Object { $_.id -eq 'docs' }).output
if ($docsOut -match '(\d+) content assertions') { $contentAssertions = $Matches[1] }

$head = (git -C $repoRoot rev-parse --short HEAD).Trim()
$sub = ((git -C $repoRoot submodule status doom3_orig/doom3) -split '\s+')[1]
$null = git -C $repoRoot ls-files --error-unmatch specs/008-classic-shaders/spec.md 2>$null
$pkgState = if ($LASTEXITCODE -eq 0) { 'tracked' } else { 'untracked' }
$stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'
$passCount = @($results | Where-Object { $_.pass }).Count
$gateCount = $results.Count

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('<!doctype html><html lang="en"><meta charset="utf-8">')
[void]$sb.AppendLine('<title>Classic shaders - spec verification</title>')
[void]$sb.AppendLine('<style>')
[void]$sb.AppendLine(':root{--ink:#12141a;--muted:#5b6270;--line:#e4e6eb;--bg:#f6f7f9;--pass:#0a6b2d;--fail:#a11;}')
[void]$sb.AppendLine('*{box-sizing:border-box}body{margin:0;padding:40px;background:var(--bg);color:var(--ink);font:14px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}')
[void]$sb.AppendLine('h1{font-size:26px;margin:0 0 6px}h2{font-size:17px;margin:36px 0 10px;border-bottom:1px solid var(--line);padding-bottom:6px}')
[void]$sb.AppendLine('p.meta{color:var(--muted);margin:0 0 22px}code{font-family:ui-monospace,Consolas,monospace;background:#fff;padding:1px 5px;border-radius:3px;border:1px solid var(--line)}')
[void]$sb.AppendLine('.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}.card{background:#fff;border:1px solid var(--line);border-radius:8px;padding:14px 16px}')
[void]$sb.AppendLine('.card .name{font-weight:600}.card .sum{margin-top:6px;font-family:ui-monospace,Consolas,monospace;font-size:12px;color:var(--muted);word-break:break-word}')
[void]$sb.AppendLine('.badge{display:inline-block;font-size:11px;font-weight:700;letter-spacing:.04em;padding:2px 7px;border-radius:20px;margin-right:8px}')
[void]$sb.AppendLine('.pass{color:var(--pass);background:#e8f5ec}.fail{color:var(--fail);background:#fbeaea}')
[void]$sb.AppendLine('.kpis{display:flex;flex-wrap:wrap;gap:10px;margin:18px 0}.kpi{background:#fff;border:1px solid var(--line);border-radius:6px;padding:8px 12px}')
[void]$sb.AppendLine('.kpi b{font-size:18px;margin-right:6px}table{border-collapse:collapse;width:100%;background:#fff;border:1px solid var(--line)}th,td{padding:7px 10px;border-bottom:1px solid var(--line);text-align:left}')
[void]$sb.AppendLine('th{background:#eef0f4;font-size:12px;text-transform:uppercase;letter-spacing:.04em}tr.live td:last-child{color:var(--pass);font-weight:600}tr.debug td:last-child{color:#8a6d00}tr.dead td:last-child{color:var(--fail)}')
[void]$sb.AppendLine('.gaps li{margin:4px 0}.art{font-family:ui-monospace,Consolas,monospace;font-size:12px;color:var(--muted)}footer{margin-top:36px;color:var(--muted);font-size:12px}')
[void]$sb.AppendLine('</style>')

[void]$sb.AppendLine('<h1>Classic shaders - spec verification</h1>')
[void]$sb.AppendLine("<p class=""meta"">superproject <code>$head</code> &middot; submodule <code>$sub</code> &middot; generated $stamp &middot; package <code>specs/008-classic-shaders</code> ($pkgState)</p>")

[void]$sb.AppendLine('<div class="kpis">')
[void]$sb.AppendLine("<span class=""kpi""><b>$passCount/$gateCount</b>gates passing</span>")
[void]$sb.AppendLine("<span class=""kpi""><b>$allDone/$allTotal</b>tasks (spec $specDone/$specTotal)</span>")
[void]$sb.AppendLine("<span class=""kpi""><b>$($counts.static_entries)</b>program entries</span>")
[void]$sb.AppendLine("<span class=""kpi""><b>$($counts.live_default_distinct_files)</b>files to author</span>")
[void]$sb.AppendLine("<span class=""kpi""><b>$contentAssertions</b>content assertions</span>")
[void]$sb.AppendLine('</div>')

[void]$sb.AppendLine('<h2>Gates</h2><div class="grid">')
foreach ($r in $results) {
    $cls = if ($r.pass) { 'pass' } else { 'fail' }
    $label = if ($r.pass) { 'PASS' } else { 'FAIL' }
    [void]$sb.AppendLine("<div class=""card""><div><span class=""badge $cls"">$label</span><span class=""name"">$(HtmlEncode $r.name)</span></div><div class=""sum"">$(HtmlEncode $r.summary)</div><div class=""art"">exit=$($r.code)</div><details><summary class=""art"">command and output</summary><pre class=""art"">$(HtmlEncode $r.command)`n$(HtmlEncode $r.output)</pre></details></div>")
}
[void]$sb.AppendLine('</div>')

[void]$sb.AppendLine('<h2>Program census</h2>')
[void]$sb.AppendLine("<p class=""meta"">$($counts.static_entries) entries over $($counts.static_distinct_files) files: $($counts.live_default_entries) live-default entries ($($counts.live_default_distinct_files) files), $($counts.debug_only_entries) debug-only, $($counts.non_arb2_entries) non-ARB2, $($counts.dead_entries) dead.</p>")
[void]$sb.AppendLine('<table><thead><tr><th>#</th><th>line</th><th>target</th><th>enum</th><th>file</th><th>classification</th></tr></thead><tbody>')
foreach ($row in $census.table) {
    [void]$sb.AppendLine("<tr class=""$($row.classification)""><td>$($row.index)</td><td>$($row.line)</td><td>$($row.target)</td><td><code>$(HtmlEncode $row.enum)</code></td><td><code>$(HtmlEncode $row.file)</code></td><td>$($row.classification)</td></tr>")
}
[void]$sb.AppendLine('</tbody></table>')

[void]$sb.AppendLine('<h2>Known gaps</h2><ul class="gaps">')
[void]$sb.AppendLine('<li>Runtime census on retail data: <b>SKIPPED</b>; no authorized pk4 in the worktree.</li>')
[void]$sb.AppendLine('<li>Legal review of the derivative-work position: <b>BLOCKED</b>; conservative rules only.</li>')
[void]$sb.AppendLine('<li>Engine resolver, ABI header, shaders: <b>not started</b> (T-020..T-030).</li>')
[void]$sb.AppendLine('<li>The five-file authoring set is a lower bound; material-declared programs are unknown until the runtime census.</li>')
[void]$sb.AppendLine('</ul>')

[void]$sb.AppendLine('<h2>Artifacts</h2><table><thead><tr><th>path</th></tr></thead><tbody>')
$files = @(Get-ChildItem -LiteralPath $specDir -Recurse -File | Sort-Object FullName)
foreach ($f in $files) {
    $rel = $f.FullName.Substring($specDir.Length).TrimStart('\', '/') -replace '\\', '/'
    [void]$sb.AppendLine("<tr><td><code>$(HtmlEncode $rel)</code></td></tr>")
}
[void]$sb.AppendLine('</tbody></table>')

[void]$sb.AppendLine('<footer>Generated by <code>tools/report.ps1</code>. The report is a rendering of the gates above; it is not independent evidence of shader behaviour.</footer>')
[void]$sb.AppendLine('</html>')

[System.IO.File]::WriteAllText($Out, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Host "wrote $Out"
Write-Host ("gates: {0}/{1} passing" -f $passCount, $gateCount)
foreach ($r in $results) { Write-Host ("  {0,-12} exit={1}  {2}" -f $r.id, $r.code, $r.summary) }

if ($passCount -ne $gateCount) { exit 1 }
exit 0
