#Requires -Version 5.1
<#
.SYNOPSIS
  Apply targets/<Name>/patches/all-changes.patch to a pristine-derived target copy.
.DESCRIPTION
  Fresh-clone workflow: new-target.ps1 -Name <n>  ->  apply-target-patches.ps1 -Name <n>  ->  cmake build.
  Re-applies unless every 'M' row already matches (then it exits 0); -Force re-applies anyway.
  Unrelated local edits outside the manifest are not detected, and the patch may fail loudly.
  Verifies every 'M' row of the manifest by SHA-256 afterwards.
  The patch reproduces modified files only. If the manifest contains 'A' or 'D' rows, this script
  fails closed by default: such a copy is incomplete and must be delivered through the engine
  fork/submodule path. -AllowIncomplete still applies the modified rows for inspection, but the
  script reports failure because the copy does not equal the patched tree.
  Exit 0 = the copy equals the patched tree (no added/deleted rows, every 'M' verified).
  Exit 2 = target copy, patch or manifest missing (or an unexpected error before or during the apply);
  1 = apply failed, mismatches, or an incomplete reconstruction.
  Note: 'powershell -Command .\script.ps1' collapses a nonzero script exit to 1; use -File to see the real code.
.EXAMPLE
  .\scripts\apply-target-patches.ps1 -Name win11-amd64
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Name, [switch]$Force, [switch]$AllowIncomplete)
$ErrorActionPreference = 'Stop'
trap { Write-Host "ERROR $($_.Exception.Message)" -ForegroundColor Red; exit 2 }
$repo = Split-Path -Parent $PSScriptRoot
$dst = Join-Path $repo "targets\$Name\doom3"
$patch = Join-Path $repo "targets\$Name\patches\all-changes.patch"
$man = Join-Path $repo "targets\$Name\patches\all-changes.manifest.txt"
foreach ($p in $dst, $patch, $man) { if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR missing: $p" -ForegroundColor Red; exit 2 } }
$rows = @(Get-Content $man | Where-Object { $_ -and $_.Trim() })
$nonM = @($rows | Where-Object { ($_ -split "`t")[0] -ne 'M' })
$mRows = @($rows | Where-Object { ($_ -split "`t")[0] -eq 'M' })
$incomplete = $nonM.Count -gt 0
if ($incomplete -and -not $AllowIncomplete) {
    Write-Host "ERROR manifest contains $($nonM.Count) added/deleted row(s); the patch reproduces modified files only. Deliver those through the engine fork/submodule path, or pass -AllowIncomplete to apply the modified rows for inspection." -ForegroundColor Red
    exit 1
}
if ($mRows.Count -eq 0) {
    Write-Host "ERROR no modified rows; the copy would be incomplete" -ForegroundColor Red
    exit 1
}
$ok = 0; $already = 0; $bad = 0
foreach ($line in $mRows) {
    $c = $line -split "`t"
    $f = Join-Path $dst ($c[1] -replace '/', '\')
    if ((Test-Path $f) -and (Get-FileHash $f -Algorithm SHA256).Hash -eq $c[2]) { $already++ }
}
if ($already -eq $mRows.Count -and -not $Force) {
    if ($incomplete) { Write-Host "INCOMPLETE: all modified rows match, but $($nonM.Count) added/deleted row(s) were not reconstructed." -ForegroundColor Red; exit 1 }
    Write-Host "Target already patched ($already files match). Nothing to do (use -Force to re-apply)."; exit 0
}
if ($already -gt 0 -and $already -lt $mRows.Count -and -not $Force) {
    Write-Host "ERROR target is partially patched ($already of $($mRows.Count) modified rows match); use -Force to re-apply." -ForegroundColor Red
    exit 1
}
Push-Location $dst
try {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $env:GIT_CEILING_DIRECTORIES = (Split-Path -Parent $dst)   # the copy sits inside this repo: stop git discovering the enclosing repo, else apply skips hunks silently
    $out = & git -c core.autocrlf=false apply --whitespace=nowarn -p2 --verbose $patch 2>&1
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    Remove-Item Env:GIT_CEILING_DIRECTORIES -ErrorAction SilentlyContinue
} finally { Pop-Location }
if ($code -ne 0) { Write-Host "ERROR git apply failed ($code):`n$out" -ForegroundColor Red; exit 1 }
foreach ($line in $mRows) {
    $c = $line -split "`t"
    $f = Join-Path $dst ($c[1] -replace '/', '\')
    if ((Test-Path $f) -and (Get-FileHash $f -Algorithm SHA256).Hash -eq $c[2]) { $ok++ } else { $bad++; Write-Host "MISMATCH $($c[1])" }
}
Write-Host "Applied patch: $ok files verified, $bad mismatches"
if ($bad -gt 0) { exit 1 }
if ($incomplete) { Write-Host "INCOMPLETE: $($nonM.Count) added/deleted row(s) were not reconstructed; this copy does not equal the patched tree." -ForegroundColor Red; exit 1 }
exit 0
