#Requires -Version 5.1
<#
.SYNOPSIS
  Compare a per-target copy against the pristine DOOM 3 source.

.DESCRIPTION
  Builds SHA-256 maps of both trees (excluding git metadata) and reports added,
  removed, and modified files. Uses hashes rather than `git diff --no-index` so
  the repository's .gitattributes (CRLF rules) cannot produce false differences,
  and the pristine submodule's `.git` file is never counted.

  Exit 0 = identical, 1 = differences, 2 = usage/environment error.

.EXAMPLE
  .\scripts\diff-target.ps1 -Name web

.EXAMPLE
  .\scripts\diff-target.ps1 -Name web -Detailed
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [switch]$Detailed
)

$ErrorActionPreference = 'Stop'

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 2
}

function Get-TreeMap {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $map = @{}
    $files = Get-ChildItem -LiteralPath $Root -Recurse -File -Force -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        $rel = $file.FullName.Substring($Root.Length).TrimStart('\', '/')
        if ($rel -eq '.git' -or $rel -like '.git\*' -or $rel -like '.git/*') { continue }
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $map[$rel.ToLowerInvariant()] = [pscustomobject]@{
            Rel    = $rel
            Hash   = $hash
            Length = $file.Length
            Label  = $Label
        }
    }
    return $map
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot 'doom3_orig\doom3'
$target = Join-Path $repoRoot "targets\$Name\doom3"

if ($Name -notmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
    Fail "Target name '$Name' is not kebab-case."
}
if (-not (Test-Path -LiteralPath $source)) {
    Fail "Pristine source not found at $source."
}
if (-not (Test-Path -LiteralPath $target)) {
    Fail "Target not found at $target. Create it: .\scripts\new-target.ps1 -Name $Name"
}

Write-Host "Pristine: $source"
Write-Host "Target  : $target"
Write-Host "Hashing both trees..." -ForegroundColor DarkGray

$srcMap = Get-TreeMap -Root $source -Label 'pristine'
$dstMap = Get-TreeMap -Root $target -Label 'target'

$added = New-Object System.Collections.Generic.List[object]
$removed = New-Object System.Collections.Generic.List[object]
$changed = New-Object System.Collections.Generic.List[object]

foreach ($key in $srcMap.Keys) {
    if (-not $dstMap.ContainsKey($key)) {
        $removed.Add($srcMap[$key])
    }
    elseif ($srcMap[$key].Hash -ne $dstMap[$key].Hash) {
        $changed.Add($dstMap[$key])
    }
}
foreach ($key in $dstMap.Keys) {
    if (-not $srcMap.ContainsKey($key)) {
        $added.Add($dstMap[$key])
    }
}

$deltaBytes = 0
foreach ($item in $added) { $deltaBytes += $item.Length }
foreach ($item in $removed) { $deltaBytes += $item.Length }
foreach ($item in $changed) { $deltaBytes += $item.Length }

Write-Host ""
Write-Host ("Pristine files : {0}" -f $srcMap.Count)
Write-Host ("Target files   : {0}" -f $dstMap.Count)
Write-Host ("Modified       : {0}" -f $changed.Count)
Write-Host ("Added          : {0}" -f $added.Count)
Write-Host ("Removed        : {0}" -f $removed.Count)
Write-Host ("Touched bytes  : {0:N0}" -f $deltaBytes)

if ($added.Count + $removed.Count + $changed.Count -eq 0) {
    Write-Host ""
    Write-Host "Identical: the target has no changes against the pristine source." -ForegroundColor Green
    exit 0
}

function Show-List {
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[object]]$Items,
        [Parameter(Mandatory = $true)][string]$Prefix,
        [int]$Limit = 20
    )
    if ($Items.Count -eq 0) { return }
    Write-Host ""
    Write-Host "$Title ($($Items.Count)):" -ForegroundColor Yellow
    $shown = 0
    foreach ($item in ($Items | Sort-Object Rel)) {
        if ($shown -ge $Limit) {
            Write-Host "  ... ($($Items.Count - $Limit) more; use -Detailed)"
            break
        }
        Write-Host ("  {0} {1}  ({2:N0} B)" -f $Prefix, $item.Rel, $item.Length)
        $shown++
    }
}

$limit = if ($Detailed) { [int]::MaxValue } else { 20 }
Show-List -Title 'Modified' -Items $changed -Prefix 'M' -Limit $limit
Show-List -Title 'Added in target' -Items $added -Prefix 'A' -Limit $limit
Show-List -Title 'Removed in target' -Items $removed -Prefix 'D' -Limit $limit

Write-Host ""
Write-Host "Differences found (exit 1)." -ForegroundColor Yellow
exit 1
