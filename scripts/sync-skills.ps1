<#
.SYNOPSIS
    Mirrors the tracked skill tree (.claude/skills) into the local Kilo tree (.kilo/skills).

.DESCRIPTION
    `.claude/skills` is the only skill tree tracked in git. It is what Claude Code reads
    natively and what `kilo.json` lists under `skills.paths`.

    `.kilo/skills` is a generated, git-ignored mirror of that tree. It exists because the
    project tree is discovered by Kilo as a project config directory, independently of
    `kilo.json`, so an agent session sees the skills even when the config file is not
    applied (that has been observed in this workspace: a session loaded `.claude/skills`
    neither through `kilo.json` nor through auto-discovery).

    The copy is byte-for-byte: files are compared by SHA-256 and only differing files are
    written. Stale files and stale skill directories are deleted from the destination, so
    the two trees cannot drift. Never edit `.kilo/skills` by hand - edit `.claude/skills`
    and re-run this script.

    PowerShell 5.1 compatible (no `&&`, no ternary operator).

.PARAMETER Source
    Tracked skill tree. Defaults to `.claude/skills` beside this script.

.PARAMETER Destination
    Local mirror. Defaults to `.kilo/skills` beside this script.

.PARAMETER Check
    Compare only; write nothing. Exits 1 when the mirror is missing, stale, or holds extra
    content - the mode CI and `scripts/verify-skills.ps1` use.

.EXAMPLE
    powershell -NoProfile -File scripts/sync-skills.ps1
    powershell -NoProfile -File scripts/sync-skills.ps1 -Check
#>
[CmdletBinding()]
param(
    [string]$Source,
    [string]$Destination,
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

# --- Parse-check this file before doing anything else -------------------------
$selfTokens = $null
$selfErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($PSCommandPath, [ref]$selfTokens, [ref]$selfErrors)
if ($selfErrors -and $selfErrors.Count -gt 0) {
    Write-Host "SELF-PARSE FAILED: $PSCommandPath" -ForegroundColor Red
    foreach ($e in $selfErrors) { Write-Host ("  line {0}: {1}" -f $e.Extent.StartLineNumber, $e.Message) }
    exit 1
}

# --- Locate both trees --------------------------------------------------------
if (-not $Source) { $Source = Join-Path $PSScriptRoot '..\.claude\skills' }
if (-not $Destination) { $Destination = Join-Path $PSScriptRoot '..\.kilo\skills' }

if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
    Write-Host "Source skill tree not found: $Source" -ForegroundColor Red
    exit 1
}
$Source = (Resolve-Path -LiteralPath $Source).ProviderPath
$Destination = [System.IO.Path]::GetFullPath($Destination)

function Get-SkillDirectories {
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return @() }
    return @(Get-ChildItem -LiteralPath $Root -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'SKILL.md') -PathType Leaf } |
        Sort-Object Name)
}

function Get-RelativeFiles {
    param([string]$Root)
    $map = @{}
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return $map }
    foreach ($f in Get-ChildItem -LiteralPath $Root -Recurse -File) {
        $rel = $f.FullName.Substring($Root.Length).TrimStart('\', '/')
        $map[$rel.Replace('\', '/')] = $f.FullName
    }
    return $map
}

function Get-Sha {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

# --- Compare (and, unless -Check, materialise) --------------------------------
$sourceSkills = Get-SkillDirectories -Root $Source
if ($sourceSkills.Count -eq 0) {
    Write-Host "No skill directories (with SKILL.md) under: $Source" -ForegroundColor Red
    exit 1
}
$destSkills = Get-SkillDirectories -Root $Destination
$sourceNames = @($sourceSkills | ForEach-Object { $_.Name })
$destNames = @($destSkills | ForEach-Object { $_.Name })

$rows = @()
$driftCount = 0
$expectedFiles = New-Object System.Collections.Generic.HashSet[string]

foreach ($skill in $sourceSkills) {
    $destSkillDir = Join-Path $Destination $skill.Name
    $skillExisted = Test-Path -LiteralPath $destSkillDir -PathType Container
    $changed = New-Object System.Collections.Generic.List[string]
    $missing = New-Object System.Collections.Generic.List[string]

    foreach ($f in Get-ChildItem -LiteralPath $skill.FullName -Recurse -File) {
        $rel = $f.FullName.Substring($Source.Length).TrimStart('\', '/')
        $rel = $rel.Replace('\', '/')
        [void]$expectedFiles.Add($rel)

        $destFile = Join-Path $Destination $rel.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $destFile -PathType Leaf)) {
            $missing.Add($rel)
            if (-not $Check) {
                $parent = Split-Path -Parent $destFile
                if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
                    New-Item -ItemType Directory -Path $parent -Force | Out-Null
                }
                Copy-Item -LiteralPath $f.FullName -Destination $destFile -Force
            }
            continue
        }
        if ((Get-Sha $f.FullName) -ne (Get-Sha $destFile)) {
            $changed.Add($rel)
            if (-not $Check) { Copy-Item -LiteralPath $f.FullName -Destination $destFile -Force }
        }
    }

    $files = @(Get-ChildItem -LiteralPath $skill.FullName -Recurse -File).Count
    if ($missing.Count -eq 0 -and $changed.Count -eq 0) {
        $status = 'unchanged'
    }
    elseif ($Check) {
        $status = ('DRIFT: {0} missing, {1} differ' -f $missing.Count, $changed.Count)
        $driftCount++
    }
    elseif ($skillExisted) {
        $status = ('updated: {0} added, {1} rewritten' -f $missing.Count, $changed.Count)
    }
    else {
        $status = ('created: {0} files' -f $files)
    }

    $rows += [pscustomobject]@{ Skill = $skill.Name; Files = $files; Status = $status }
}

# --- Remove content the source no longer has ----------------------------------
$staleDirs = @($destNames | Where-Object { $sourceNames -notcontains $_ })
$staleFiles = New-Object System.Collections.Generic.List[string]
$destFiles = Get-RelativeFiles -Root $Destination
foreach ($rel in $destFiles.Keys) {
    $skillName = ($rel -split '/')[0]
    if ($sourceNames -notcontains $skillName) { continue }   # whole dir is stale, handled below
    if (-not $expectedFiles.Contains($rel)) { $staleFiles.Add($rel) }
}

if ($staleDirs.Count -gt 0 -or $staleFiles.Count -gt 0) {
    foreach ($name in $staleDirs) {
        $rows += [pscustomobject]@{ Skill = $name; Files = 0; Status = 'stale: not in source' }
        $driftCount++
    }
    foreach ($rel in $staleFiles) {
        $rows += [pscustomobject]@{ Skill = ($rel -split '/')[0]; Files = 0; Status = ("stale file: $rel") }
        $driftCount++
    }
    if (-not $Check) {
        foreach ($name in $staleDirs) { Remove-Item -LiteralPath (Join-Path $Destination $name) -Recurse -Force }
        foreach ($rel in $staleFiles) { Remove-Item -LiteralPath (Join-Path $Destination $rel.Replace('/', '\')) -Force -ErrorAction SilentlyContinue }
        # Drop directories left empty by the removals.
        if (Test-Path -LiteralPath $Destination -PathType Container) {
            Get-ChildItem -LiteralPath $Destination -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
                Where-Object { @(Get-ChildItem -LiteralPath $_.FullName -Recurse -File).Count -eq 0 } |
                Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    if ($staleDirs.Count -gt 0 -or $staleFiles.Count -gt 0) { $driftCount = [Math]::Max($driftCount, 1) }
}

# --- Report -------------------------------------------------------------------
$mode = 'sync'
if ($Check) { $mode = 'check' }
Write-Host "Source     : $Source"
Write-Host "Mirror     : $Destination"
Write-Host "Mode       : $mode"
Write-Host ''
Write-Host ('{0,-30} {1,5}  {2}' -f 'Skill', 'Files', 'Status')
Write-Host ('{0,-30} {1,5}  {2}' -f ('-' * 30), ('-' * 5), ('-' * 34))
foreach ($row in $rows) {
    Write-Host ('{0,-30} {1,5}  {2}' -f $row.Skill, $row.Files, $row.Status)
}

$rowsChanged = @($rows | Where-Object { $_.Status -ne 'unchanged' })
Write-Host ''
Write-Host ("{0} skills in source, {1} in mirror, {2} row(s) needing action." -f $sourceNames.Count, $destNames.Count, $rowsChanged.Count)

if ($Check) {
    if ($driftCount -gt 0) {
        Write-Host 'Mirror is out of date. Run: powershell -NoProfile -File scripts/sync-skills.ps1' -ForegroundColor Red
        exit 1
    }
    Write-Host 'Mirror matches the source.' -ForegroundColor Green
    exit 0
}

if ($rowsChanged.Count -gt 0) { Write-Host 'Mirror updated.' -ForegroundColor Green }
else { Write-Host 'Mirror already up to date.' -ForegroundColor Green }
exit 0
