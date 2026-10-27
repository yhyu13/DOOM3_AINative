#Requires -Version 5.1
<#
.SYNOPSIS
  One-command verification for spec 001 (win11-amd64). Prints PASS/FAIL/SKIPPED per criterion.
.DESCRIPTION
  Does not build. Run the CMake build first (see targets/win11-amd64/README.md). Exit 0 only if no FAIL.
.EXAMPLE
  .\scripts\verify-win11-amd64.ps1 -DataDir <data-root>\run
#>
[CmdletBinding()]
param([string]$DataDir = $env:D3_DATA_DIR, [string]$Exe, [switch]$SkipSmoke)
$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot '_common.ps1')
if (-not $Exe) { $Exe = Resolve-Doom3Exe -Repo $repo }
$exe = $Exe
$results = New-Object System.Collections.Generic.List[object]
function Add-Result($id, $status, $detail) { $results.Add([pscustomobject]@{ Id = $id; Status = $status; Detail = $detail }); Write-Host ("{0,-8} {1,-8} {2}" -f $id, $status, $detail) }

# SC-001/002 exe, PE machine, size
if (Test-Path -LiteralPath $exe) {
    $len = (Get-Item $exe).Length
    $fs = [IO.File]::OpenRead($exe); $br = New-Object IO.BinaryReader $fs
    $fs.Position = 0x3C; $pe = $br.ReadInt32(); $fs.Position = $pe + 4; $machine = $br.ReadUInt16(); $br.Close(); $fs.Close()
    Add-Result 'SC-001' ($(if ($machine -eq 0x8664) { 'PASS' } else { 'FAIL' })) ("PE machine 0x{0:X4}" -f $machine)
    Add-Result 'SC-002' ($(if ($len -gt 1048576) { 'PASS' } else { 'FAIL' })) "size $len bytes"
} else { Add-Result 'SC-001' 'FAIL' "exe missing: $exe"; Add-Result 'SC-002' 'FAIL' 'exe missing' }

# SC-004 pristine untouched
$dirt = @(& git -C (Join-Path $repo 'doom3_orig\doom3') status --porcelain 2>$null | Where-Object { $_ -and $_.Trim() })
Add-Result 'SC-004' ($(if ($dirt.Count -eq 0) { 'PASS' } else { 'FAIL' })) "pristine porcelain lines: $($dirt.Count)"

# SC-006 patches reproduce target
$exp = Join-Path $repo 'scripts\export-target-patches.ps1'
& $exp -Name win11-amd64 | Out-Null
$man = Join-Path $repo 'targets\win11-amd64\patches\all-changes.manifest.txt'
$cnt = if (Test-Path $man) { @(Get-Content $man).Count } else { 0 }
Add-Result 'SC-006a' ($(if ($cnt -gt 0) { 'PASS' } else { 'FAIL' })) "patch manifest entries: $cnt"
$tmp = Join-Path $env:TEMP ("d3repro-" + [guid]::NewGuid().ToString('N'))
$ok = $false
try {
    Copy-Item (Join-Path $repo 'doom3_orig\doom3') $tmp -Recurse -Force
    Remove-Item (Join-Path $tmp '.git') -Force -ErrorAction SilentlyContinue
    $patch = Join-Path $repo 'targets\win11-amd64\patches\all-changes.patch'
    if ((Test-Path $patch) -and (Get-Item $patch).Length -gt 0) {
        Push-Location $tmp
        $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        & git -c core.autocrlf=false apply --whitespace=nowarn -p2 $patch 2>$null
        $applyCode = $LASTEXITCODE
        $ErrorActionPreference = $prev
        # added files come from manifest A rows
        Pop-Location
    }
    # compare modified-file hashes against manifest
    $bad = 0
    foreach ($line in (Get-Content $man)) {
        $p = $line -split "`t"
        if ($p[0] -eq 'M') {
            $f = Join-Path $tmp ($p[1] -replace '/', '\')
            if (-not (Test-Path $f) -or (Get-FileHash $f -Algorithm SHA256).Hash -ne $p[2]) { $bad++ }
        }
    }
    $ok = ($bad -eq 0) -and ($applyCode -eq 0)
    Add-Result 'SC-006b' ($(if ($ok) { 'PASS' } else { 'FAIL' })) "git apply exit $applyCode, patched copy hash mismatches on modified files: $bad"
} catch { Add-Result 'SC-006b' 'FAIL' $_.Exception.Message }
finally { Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue }

# SC-003 smoke
if ($SkipSmoke) { Add-Result 'SC-003' 'SKIPPED' 'smoke skipped by flag' }
elseif (-not (Test-Path -LiteralPath $exe)) { Add-Result 'SC-003' 'FAIL' 'no exe' }
elseif (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { Add-Result 'SC-003' 'SKIPPED' 'no game data dir (set -DataDir or D3_DATA_DIR)' }
else {
    $out = Join-Path $repo 'targets\win11-amd64\build\smoke'
    & (Join-Path $repo 'scripts\smoke-win11-amd64.ps1') -DataDir $DataDir -Exe $exe -OutDir $out
    $code = $LASTEXITCODE
    Add-Result 'SC-003' ($(if ($code -eq 0) { 'PASS' } else { 'FAIL' })) "smoke exit $code, shot $out\screenshot.png"
}
$fail = @($results | Where-Object Status -eq 'FAIL').Count
Write-Host ("SUMMARY pass={0} fail={1} skipped={2}" -f @($results | Where-Object Status -eq 'PASS').Count, $fail, @($results | Where-Object Status -eq 'SKIPPED').Count)
exit ([int]($fail -gt 0))
