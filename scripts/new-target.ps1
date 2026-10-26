#Requires -Version 5.1
<#
.SYNOPSIS
  Create a per-target working copy of the pristine DOOM 3 source.

.DESCRIPTION
  Copies doom3_orig/doom3 -> targets/<Name>/doom3 without any git metadata.
  The pristine submodule is never modified; each target evolves on its own copy.
  Refuses to copy from a dirty submodule and refuses to overwrite an existing
  target unless -Force is given (which deletes the old copy first).

.EXAMPLE
  .\scripts\new-target.ps1 -Name web

.EXAMPLE
  .\scripts\new-target.ps1 -Name web -Force
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot 'doom3_orig\doom3'
$dest = Join-Path $repoRoot "targets\$Name\doom3"

if ($Name -notmatch '^[a-z0-9]+(-[a-z0-9]+)*$') {
    Fail "Target name '$Name' is not kebab-case (e.g. web, wasm-headless, vr)."
}
if (-not (Test-Path -LiteralPath (Join-Path $source 'neo\doom.sln'))) {
    Fail "Pristine source not found at $source. Run: git submodule update --init --recursive"
}

$pristineStatus = & git -C $source status --porcelain
if ($LASTEXITCODE -ne 0) {
    Fail "$source is not a git checkout. Re-init the submodule: git submodule update --init --recursive"
}
if (@($pristineStatus | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }).Count -gt 0) {
    Fail "Pristine submodule is dirty. Restore it first (git -C `"$source`" status), then retry."
}

if (Test-Path -LiteralPath $dest) {
    if (-not $Force) {
        Fail "Target already exists: $dest. Use -Force to replace it (this deletes that copy)."
    }
    Write-Host "Removing existing target $dest (-Force)..." -ForegroundColor Yellow
    Remove-Item -LiteralPath $dest -Recurse -Force
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null

Write-Host "Copying pristine source to $dest ..."
# /XD excludes the .git directory; /XF excludes the root .git file of a submodule.
robocopy $source $dest /E /XD ".git" /XF ".git" /NFL /NDL /NJH /NJS /NP /R:2 /W:1 | Out-Null
$copyExit = $LASTEXITCODE
if ($copyExit -ge 8) {
    Fail "robocopy failed with exit $copyExit. Remove $dest and retry."
}

$sourceFiles = @(Get-ChildItem -LiteralPath $source -Recurse -File -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne '.git' -and $_.FullName -notmatch '\\\.git\\' })
$targetFiles = @(Get-ChildItem -LiteralPath $dest -Recurse -File -Force -ErrorAction SilentlyContinue)
if ($sourceFiles.Count -ne $targetFiles.Count) {
    Fail "Copy incomplete: source has $($sourceFiles.Count) files, target has $($targetFiles.Count)."
}
if (Test-Path -LiteralPath (Join-Path $dest '.git')) {
    Fail "Copy contains git metadata at $dest\.git; it would share the pristine gitdir. Remove it."
}

$bytes = ($targetFiles | Measure-Object Length -Sum).Sum
Write-Host ""
Write-Host "Target  : $dest" -ForegroundColor Green
Write-Host ("Files   : {0}  ({1:N1} MB)" -f $targetFiles.Count, ($bytes / 1MB))
Write-Host ""
Write-Host "Next:"
Write-Host "  cd `"$dest`""
Write-Host "  # edit this copy only; the pristine submodule stays untouched"
Write-Host "  .\scripts\diff-target.ps1 -Name $Name"
