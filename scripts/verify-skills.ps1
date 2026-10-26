<#
.SYNOPSIS
    Validates the project-local agent skills in .kilo/skills.

.DESCRIPTION
    For every directory under .kilo/skills, checks that SKILL.md exists, is
    non-empty, and has minimal YAML frontmatter with:
      - a non-empty "name" equal to the directory name
      - a non-empty "description" of at most 1024 characters

    Prints one row per skill (skill, files, size KB, status) and exits 1 if any
    check fails, 0 otherwise. PowerShell 5.1 compatible.

.PARAMETER SkillsPath
    Directory holding the skill folders. Defaults to .kilo/skills relative to
    this script, so it works from any working directory.

.EXAMPLE
    powershell -NoProfile -File scripts/verify-skills.ps1
#>
[CmdletBinding()]
param(
    [string]$SkillsPath
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
    $SkillsPath = Join-Path $PSScriptRoot '..\.kilo\skills'
}
if (-not (Test-Path -LiteralPath $SkillsPath -PathType Container)) {
    Write-Host "Skills directory not found: $SkillsPath" -ForegroundColor Red
    exit 1
}
$SkillsPath = (Resolve-Path -LiteralPath $SkillsPath).ProviderPath

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

if ($failureCount -gt 0) { exit 1 }
exit 0
