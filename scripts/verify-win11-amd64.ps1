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

# SC-004 pristine untouched. An uninitialised submodule, a failed git command, or a git repository
# discovered above the submodule is FAIL, never a clean tree.
$pristine = Join-Path $repo 'doom3_orig\doom3'
$pristineFull = [IO.Path]::GetFullPath($pristine).TrimEnd('\')
if (-not (Test-Path -LiteralPath $pristine -PathType Container)) {
    Add-Result 'SC-004' 'FAIL' "pristine submodule missing: $pristine"
} elseif (-not (Test-Path -LiteralPath (Join-Path $pristine '.git'))) {
    Add-Result 'SC-004' 'FAIL' "pristine submodule not initialised (no .git): run git submodule update --init"
} elseif (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Add-Result 'SC-004' 'FAIL' 'git not found; pristine cleanliness was not checked'
} else {
    $top = @(& git -C $pristine rev-parse --show-toplevel 2>$null | Where-Object { $_ -and $_.Trim() })
    $topCode = $LASTEXITCODE
    if ($topCode -ne 0) {
        Add-Result 'SC-004' 'FAIL' "git could not resolve the pristine repository (exit $topCode)"
    } elseif ($top.Count -eq 0 -or [IO.Path]::GetFullPath($top[0].Trim()).TrimEnd('\') -ine $pristineFull) {
        $found = if ($top.Count -gt 0) { $top[0].Trim() } else { '<none>' }
        Add-Result 'SC-004' 'FAIL' "git discovered $found instead of $pristineFull"
    } else {
        $dirt = @(& git -C $pristine status --porcelain 2>$null | Where-Object { $_ -and $_.Trim() })
        $gitCode = $LASTEXITCODE
        Add-Result 'SC-004' ($(if ($gitCode -eq 0 -and $dirt.Count -eq 0) { 'PASS' } else { 'FAIL' })) "pristine git status exit $gitCode, porcelain lines: $($dirt.Count)"
    }
}

# SC-006 patches reproduce target. Export to a temp directory: never overwrite the tracked patches.
$exp = Join-Path $repo 'scripts\export-target-patches.ps1'
$expDir = Join-Path $env:TEMP ("d3exp-" + [guid]::NewGuid().ToString('N'))
$expCode = -1
try { & $exp -Name win11-amd64 -OutDir $expDir | Out-Null; $expCode = $LASTEXITCODE }
catch { $expCode = -1 }
$man = Join-Path $expDir 'all-changes.manifest.txt'
$patch = Join-Path $expDir 'all-changes.patch'
$rows = @()
if (Test-Path $man) { $rows = @(Get-Content $man | Where-Object { $_ -and $_.Trim() }) }
$nonM = @($rows | Where-Object { ($_ -split "`t")[0] -ne 'M' })
Add-Result 'SC-006a' ($(if ($expCode -eq 0 -and $rows.Count -gt 0) { 'PASS' } else { 'FAIL' })) "export exit $expCode, patch manifest entries: $($rows.Count)"
if ($expCode -ne 0) {
    Add-Result 'SC-006b' 'FAIL' "export failed ($expCode); reconstruction not attempted"
} elseif ($nonM.Count -gt 0) {
    Add-Result 'SC-006b' 'FAIL' "manifest contains $($nonM.Count) added/deleted row(s); the patch reproduces modified files only and cannot reconstruct them"
} else {
    $tmp = Join-Path $env:TEMP ("d3repro-" + [guid]::NewGuid().ToString('N'))
    $ok = $false; $applyCode = -1; $bad = -1; $reported = $false
    try {
        Copy-Item (Join-Path $repo 'doom3_orig\doom3') $tmp -Recurse -Force
        Remove-Item (Join-Path $tmp '.git') -Force -ErrorAction SilentlyContinue
        if ((Test-Path $patch) -and (Get-Item $patch).Length -gt 0) {
            Push-Location $tmp
            $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
            & git -c core.autocrlf=false apply --whitespace=nowarn -p2 $patch 2>$null
            $applyCode = $LASTEXITCODE
            $ErrorActionPreference = $prev
            Pop-Location
        }
        # compare modified-file hashes against manifest
        $bad = 0
        foreach ($line in $rows) {
            $p = $line -split "`t"
            if ($p[0] -eq 'M') {
                $f = Join-Path $tmp ($p[1] -replace '/', '\')
                if (-not (Test-Path $f) -or (Get-FileHash $f -Algorithm SHA256).Hash -ne $p[2]) { $bad++ }
            }
        }
        $ok = ($bad -eq 0) -and ($applyCode -eq 0)
    } catch { Add-Result 'SC-006b' 'FAIL' $_.Exception.Message; $reported = $true }
    finally { Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue }
    if (-not $reported) {
        Add-Result 'SC-006b' ($(if ($ok) { 'PASS' } else { 'FAIL' })) "git apply exit $applyCode, patched copy hash mismatches on modified files: $bad"
    }
}
Remove-Item $expDir -Recurse -Force -ErrorAction SilentlyContinue

# SC-003 smoke
if ($SkipSmoke) { Add-Result 'SC-003' 'SKIPPED' 'smoke skipped by flag' }
elseif (-not (Test-Path -LiteralPath $exe)) { Add-Result 'SC-003' 'FAIL' 'no exe' }
elseif (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { Add-Result 'SC-003' 'SKIPPED' 'no game data dir (set -DataDir or D3_DATA_DIR)' }
else {
    $out = Join-Path $repo ("targets\win11-amd64\build\smoke-" + [guid]::NewGuid().ToString('N'))
    & (Join-Path $repo 'scripts\smoke-win11-amd64.ps1') -DataDir $DataDir -Exe $exe -OutDir $out
    $code = $LASTEXITCODE
    Add-Result 'SC-003' ($(if ($code -eq 0) { 'PASS' } else { 'FAIL' })) "smoke exit $code, output $out"
}
$fail = @($results | Where-Object Status -eq 'FAIL').Count
Write-Host ("SUMMARY pass={0} fail={1} skipped={2}" -f @($results | Where-Object Status -eq 'PASS').Count, $fail, @($results | Where-Object Status -eq 'SKIPPED').Count)
exit ([int]($fail -gt 0))
