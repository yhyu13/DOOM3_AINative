<#
.SYNOPSIS
    Validates the tracked skill tree and its local Kilo mirror.

.DESCRIPTION
    For every directory under `.claude/skills` (the tree tracked in git), checks that
    SKILL.md exists, is non-empty, and has minimal YAML frontmatter with:
      - a non-empty "name" equal to the directory name, lowercase kebab-case, at most 64 chars
      - a non-empty "description" of at most 1024 characters

    That is the format both Claude Code and Kilo require; a skill that fails it is not
    picked up by either harness.

    Unless -NoMirror is given, the tree is then compared with the local, git-ignored mirror
    at `.kilo/skills`: both must hold the same skill names and byte-identical files. A
    stale or missing mirror is a failure - run `scripts/sync-skills.ps1` to rebuild it.

    Prints one row per skill (skill, files, size KB, status), then a mirror summary, and
    exits 1 if any check fails, 0 otherwise. PowerShell 5.1 compatible.

.PARAMETER SkillsPath
    Directory holding the tracked skill folders. Defaults to `.claude/skills` relative to
    this script, so it works from any working directory.

.PARAMETER MirrorPath
    Directory holding the local mirror. Defaults to `.kilo/skills`.

.PARAMETER NoMirror
    Skip the mirror comparison (format checks only).

.EXAMPLE
    powershell -NoProfile -File scripts/verify-skills.ps1
    powershell -NoProfile -File scripts/verify-skills.ps1 -NoMirror
#>
[CmdletBinding()]
param(
    [string]$SkillsPath,
    [string]$MirrorPath,
    [switch]$NoMirror
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

# --- Locate the skills directory ---------------------------------------------
if (-not $SkillsPath) {
    $SkillsPath = Join-Path $PSScriptRoot '..\.claude\skills'
}
if (-not (Test-Path -LiteralPath $SkillsPath -PathType Container)) {
    Write-Host "Skills directory not found: $SkillsPath" -ForegroundColor Red
    exit 1
}
$SkillsPath = (Resolve-Path -LiteralPath $SkillsPath).ProviderPath

if (-not $MirrorPath) {
    $MirrorPath = Join-Path $PSScriptRoot '..\.kilo\skills'
}
$MirrorPath = [System.IO.Path]::GetFullPath($MirrorPath)

function Get-Frontmatter {
    param([string[]]$Lines)

    if ($Lines.Count -lt 3 -or $Lines[0].Trim() -ne '---') { return $null }

    $end = -1
    for ($i = 1; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i].Trim() -eq '---') { $end = $i; break }
    }
    if ($end -lt 0) { return $null }

    $map = @{}
    $i = 1
    while ($i -lt $end) {
        $line = $Lines[$i]
        if ($line -match '^([A-Za-z0-9_.-]+):[ \t]*(.*)$') {
            $key = $Matches[1]
            $value = $Matches[2]

            if ($value -match '^[|>][+-]?$') {
                # Block scalar: consume the indented continuation lines.
                $parts = New-Object System.Collections.Generic.List[string]
                $i++
                while ($i -lt $end -and ($Lines[$i] -match '^\s+' -or $Lines[$i].Trim() -eq '')) {
                    if ($Lines[$i].Trim() -ne '') { $parts.Add($Lines[$i].Trim()) }
                    $i++
                }
                $value = ($parts -join ' ')
            }
            else {
                $i++
            }

            $value = $value.Trim()
            if ($value.Length -ge 2 -and
                (($value.StartsWith('"') -and $value.EndsWith('"')) -or
                 ($value.StartsWith("'") -and $value.EndsWith("'")))) {
                $value = $value.Substring(1, $value.Length - 2).Trim()
            }

            if (-not $map.ContainsKey($key)) { $map[$key] = $value }
        }
        else {
            $i++
        }
    }
    return $map
}

# --- Validate each skill ------------------------------------------------------
$rows = @()
$failureCount = 0

$skillDirs = Get-ChildItem -LiteralPath $SkillsPath -Directory | Sort-Object Name
if ($skillDirs.Count -eq 0) {
    Write-Host "No skill directories under: $SkillsPath" -ForegroundColor Red
    exit 1
}

foreach ($dir in $skillDirs) {
    $problems = New-Object System.Collections.Generic.List[string]
    $files = @(Get-ChildItem -LiteralPath $dir.FullName -Recurse -File -ErrorAction SilentlyContinue)
    $bytes = ($files | Measure-Object -Property Length -Sum).Sum
    if (-not $bytes) { $bytes = 0 }

    $skillFile = Join-Path $dir.FullName 'SKILL.md'
    if (-not (Test-Path -LiteralPath $skillFile -PathType Leaf)) {
        $problems.Add('SKILL.md missing')
    }
    else {
        $lines = [System.IO.File]::ReadAllLines($skillFile, [System.Text.Encoding]::UTF8)
        if ($lines.Length -eq 0) {
            $problems.Add('SKILL.md empty')
        }
        else {
            $frontmatter = Get-Frontmatter -Lines $lines
            if ($null -eq $frontmatter) {
                $problems.Add('frontmatter missing or malformed')
            }
            else {
                if (-not $frontmatter.ContainsKey('name') -or [string]::IsNullOrWhiteSpace($frontmatter['name'])) {
                    $problems.Add('name missing')
                }
                elseif ($frontmatter['name'] -cne $dir.Name) {
                    $problems.Add("name '$($frontmatter['name'])' != directory '$($dir.Name)'")
                }
                elseif ($frontmatter['name'] -cnotmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
                    $problems.Add("name '$($frontmatter['name'])' is not lowercase kebab-case")
                }
                elseif ($frontmatter['name'].Length -gt 64) {
                    $problems.Add("name $($frontmatter['name'].Length) chars > 64")
                }

                if (-not $frontmatter.ContainsKey('description') -or [string]::IsNullOrWhiteSpace($frontmatter['description'])) {
                    $problems.Add('description missing')
                }
                elseif ($frontmatter['description'].Length -gt 1024) {
                    $problems.Add("description $($frontmatter['description'].Length) chars > 1024")
                }
            }
        }
    }

    if ($problems.Count -eq 0) {
        $status = 'OK'
    }
    else {
        $status = 'FAIL: ' + ($problems -join '; ')
        $failureCount++
    }

    $rows += [pscustomobject]@{
        Skill  = $dir.Name
        Files  = $files.Count
        SizeKB = [math]::Round($bytes / 1KB, 1)
        Status = $status
    }
}

# --- Report -------------------------------------------------------------------
Write-Host "Skills root: $SkillsPath"
Write-Host ''
Write-Host ('{0,-30} {1,5} {2,9}  {3}' -f 'Skill', 'Files', 'Size KB', 'Status')
Write-Host ('{0,-30} {1,5} {2,9}  {3}' -f ('-' * 30), ('-' * 5), ('-' * 9), ('-' * 6))
foreach ($row in $rows) {
    Write-Host ('{0,-30} {1,5} {2,9:0.0}  {3}' -f $row.Skill, $row.Files, $row.SizeKB, $row.Status)
}

Write-Host ''
Write-Host ("{0} skills checked, {1} OK, {2} failed." -f $rows.Count, ($rows.Count - $failureCount), $failureCount)

# --- Compare with the local Kilo mirror ---------------------------------------
$mirrorFailures = 0
if (-not $NoMirror) {
    Write-Host ''
    Write-Host "Mirror root: $MirrorPath"

    if (-not (Test-Path -LiteralPath $MirrorPath -PathType Container)) {
        Write-Host 'Mirror missing - run: powershell -NoProfile -File scripts/sync-skills.ps1' -ForegroundColor Red
        $mirrorFailures++
    }
    else {
        $srcNames = @($skillDirs | ForEach-Object { $_.Name })
        $dstNames = @(Get-ChildItem -LiteralPath $MirrorPath -Directory | ForEach-Object { $_.Name })
        $missingSkills = @($srcNames | Where-Object { $dstNames -notcontains $_ })
        $extraSkills = @($dstNames | Where-Object { $srcNames -notcontains $_ })

        if ($missingSkills.Count -gt 0) {
            Write-Host ("missing from mirror: {0}" -f ($missingSkills -join ', ')) -ForegroundColor Red
            $mirrorFailures++
        }
        if ($extraSkills.Count -gt 0) {
            Write-Host ("stale in mirror: {0}" -f ($extraSkills -join ', ')) -ForegroundColor Red
            $mirrorFailures++
        }

        $differingSkills = New-Object System.Collections.Generic.List[string]
        foreach ($name in @($srcNames | Where-Object { $dstNames -contains $_ })) {
            $srcDir = Join-Path $SkillsPath $name
            $dstDir = Join-Path $MirrorPath $name
            $srcHashes = @{}
            foreach ($f in Get-ChildItem -LiteralPath $srcDir -Recurse -File) {
                $rel = $f.FullName.Substring($srcDir.Length).TrimStart('\', '/')
                $srcHashes[$rel] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
            }
            $dstHashes = @{}
            foreach ($f in Get-ChildItem -LiteralPath $dstDir -Recurse -File) {
                $rel = $f.FullName.Substring($dstDir.Length).TrimStart('\', '/')
                $dstHashes[$rel] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
            }

            $diff = 0
            foreach ($rel in $srcHashes.Keys) {
                if (-not $dstHashes.ContainsKey($rel)) { $diff++ }
                elseif ($dstHashes[$rel] -ne $srcHashes[$rel]) { $diff++ }
            }
            foreach ($rel in $dstHashes.Keys) {
                if (-not $srcHashes.ContainsKey($rel)) { $diff++ }
            }
            if ($diff -gt 0) { $differingSkills.Add(("{0} ({1} file(s))" -f $name, $diff)) }
        }

        if ($differingSkills.Count -gt 0) {
            Write-Host ("content differs: {0}" -f ($differingSkills -join ', ')) -ForegroundColor Red
            $mirrorFailures++
        }
        if ($mirrorFailures -eq 0) {
            Write-Host ("{0} skills in both trees, byte-identical." -f $srcNames.Count) -ForegroundColor Green
        }
    }
}

$totalFailures = $failureCount + $mirrorFailures
if ($totalFailures -gt 0) { exit 1 }
exit 0
