#Requires -Version 5.1
<#
.SYNOPSIS
  Apply targets/<Name>/patches/all-changes.patch to a pristine-derived target copy.
.DESCRIPTION
  Fresh-clone workflow: new-target.ps1 -Name <n>  ->  apply-target-patches.ps1 -Name <n>  ->  cmake build.
  Refuses to run when the copy already differs from pristine unless -Force (then patch may fail loudly).
  Verifies every 'M' row of the manifest by SHA-256 afterwards. Exit 0 = copy now equals the patched tree.
  Exit 2 = target copy, patch or manifest missing (or an unexpected error before the apply); 1 = apply failed or mismatches.
  Note: 'powershell -Command .\script.ps1' collapses a nonzero script exit to 1; use -File to see the real code.
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Name, [switch]$Force)
$ErrorActionPreference = 'Stop'
trap { Write-Host "ERROR $($_.Exception.Message)" -ForegroundColor Red; exit 2 }
$repo = Split-Path -Parent $PSScriptRoot
$dst = Join-Path $repo "targets\$Name\doom3"
$patch = Join-Path $repo "targets\$Name\patches\all-changes.patch"
$man = Join-Path $repo "targets\$Name\patches\all-changes.manifest.txt"
foreach ($p in $dst, $patch, $man) { if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR missing: $p" -ForegroundColor Red; exit 2 } }
$ok = 0; $already = 0; $bad = 0
foreach ($line in Get-Content $man) {
    $c = $line -split "`t"; if ($c[0] -ne 'M') { continue }
    $f = Join-Path $dst ($c[1] -replace '/', '\')
    if ((Test-Path $f) -and (Get-FileHash $f -Algorithm SHA256).Hash -eq $c[2]) { $already++ }
}
if ($already -gt 0 -and -not $Force) { Write-Host "Target already patched ($already files match). Nothing to do (use -Force to re-apply)."; exit 0 }
Push-Location $dst
try {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $env:GIT_CEILING_DIRECTORIES = (Split-Path -Parent $dst)   # the copy sits inside this repo: stop git discovering the enclosing repo, else apply skips hunks silently
    $out = & git -c core.autocrlf=false apply --whitespace=nowarn -p2 --verbose $patch 2>&1
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    Remove-Item Env:GIT_CEILING_DIRECTORIES -ErrorAction SilentlyContinue
} finally { Pop-Location }
if ($code -ne 0) { Write-Host "ERROR git apply failed ($code):`n$out" -ForegroundColor Red; exit 1 }
foreach ($line in Get-Content $man) {
    $c = $line -split "`t"; if ($c[0] -ne 'M') { continue }
    $f = Join-Path $dst ($c[1] -replace '/', '\')
    if ((Test-Path $f) -and (Get-FileHash $f -Algorithm SHA256).Hash -eq $c[2]) { $ok++ } else { $bad++; Write-Host "MISMATCH $($c[1])" }
}
Write-Host "Applied patch: $ok files verified, $bad mismatches"
exit ([int]($bad -gt 0))
