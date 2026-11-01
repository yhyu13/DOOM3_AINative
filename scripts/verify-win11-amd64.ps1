#Requires -Version 5.1
<#
.SYNOPSIS
  One-command verification for spec 001 (win11-amd64). Prints PASS/FAIL/SKIPPED per criterion.
.DESCRIPTION
  Does not build. Run the CMake build first (see targets/win11-amd64/README.md). Exit 0 only if no FAIL.
  -Sc006Only runs only the SC-006 export/reconstruction checks, and -TargetName/-PristineDir choose
  the trees they use. That combination is the negative-control seam used by test-target-patches.ps1;
  it is not a stage result. -PatchDir additionally supplies a ready-made patch set for the control
  (SC-006a then reports the supplied rows, not a fresh export) and is refused without -Sc006Only.
  SC-006b fails closed unless every 'M' row hash matches after reconstruction, git apply exited 0,
  and the manifest names exactly the files the patch touches. A truncated manifest that omits a
  patched file is a FAIL, not "M rows verified 1/1".
.EXAMPLE
  .\scripts\verify-win11-amd64.ps1 -DataDir <data-root>\run
.EXAMPLE
  .\scripts\verify-win11-amd64.ps1 -Sc006Only -TargetName zz-control -PristineDir <synthetic-pristine>
#>
[CmdletBinding()]
param([string]$DataDir = $env:D3_DATA_DIR, [string]$Exe, [switch]$SkipSmoke,
    [string]$TargetName = 'win11-amd64', [string]$PristineDir, [switch]$Sc006Only,
    [string]$PatchDir)
$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot '_common.ps1')
if ($PatchDir -and -not $Sc006Only) {
    Write-Host 'ERROR -PatchDir is a control-test seam and requires -Sc006Only; a stage run must export from the target copy.' -ForegroundColor Red
    exit 2
}
$pristine = if ($PristineDir) { $PristineDir } else { Join-Path $repo 'doom3_orig\doom3' }
$results = New-Object System.Collections.Generic.List[object]
function Add-Result($id, $status, $detail) { $results.Add([pscustomobject]@{ Id = $id; Status = $status; Detail = $detail }); Write-Host ("{0,-8} {1,-8} {2}" -f $id, $status, $detail) }

if (-not $Sc006Only) {
if (-not $Exe) { $Exe = Resolve-Doom3Exe -Repo $repo }
$exe = $Exe

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
}

# SC-006 patches reproduce target. Export to a temp directory: never overwrite the tracked patches.
$exp = Join-Path $repo 'scripts\export-target-patches.ps1'
$expDir = $null
$rows = @()
$expCode = -1
if ($PatchDir) {
    $man = Join-Path $PatchDir 'all-changes.manifest.txt'
    $patch = Join-Path $PatchDir 'all-changes.patch'
    if (Test-Path -LiteralPath $man) {
        $rows = @(Get-Content $man | Where-Object { $_ -and $_.Trim() })
        $expCode = 0
        Add-Result 'SC-006a' ($(if ($rows.Count -gt 0) { 'PASS' } else { 'FAIL' })) "control patch set: $($rows.Count) manifest rows from $PatchDir"
    } else {
        Add-Result 'SC-006a' 'FAIL' "control patch set has no manifest: $man"
    }
} else {
    $expDir = Join-Path $env:TEMP ("d3exp-" + [guid]::NewGuid().ToString('N'))
    $expArgs = @{ Name = $TargetName; OutDir = $expDir }
    if ($PristineDir) { $expArgs.SourceDir = $PristineDir }
    try { & $exp @expArgs | Out-Null; $expCode = $LASTEXITCODE }
    catch { $expCode = -1 }
    $man = Join-Path $expDir 'all-changes.manifest.txt'
    $patch = Join-Path $expDir 'all-changes.patch'
    if (Test-Path $man) { $rows = @(Get-Content $man | Where-Object { $_ -and $_.Trim() }) }
    Add-Result 'SC-006a' ($(if ($expCode -eq 0 -and $rows.Count -gt 0) { 'PASS' } else { 'FAIL' })) "export exit $expCode, patch manifest entries: $($rows.Count)"
}
$nonM = @($rows | Where-Object { ($_ -split "`t")[0] -ne 'M' })
$mRows = @($rows | Where-Object { ($_ -split "`t")[0] -eq 'M' })
if ($expCode -eq 1) {
    Add-Result 'SC-006b' 'FAIL' 'export exited 1: the manifest has added/deleted rows, so the patch is incomplete; no reconstruction is claimed'
} elseif ($expCode -ne 0) {
    Add-Result 'SC-006b' 'FAIL' "export exited $expCode (environment error); no reconstruction is claimed"
} elseif ($rows.Count -eq 0) {
    Add-Result 'SC-006b' 'FAIL' 'manifest is empty; there is nothing to reconstruct'
} elseif ($nonM.Count -gt 0) {
    Add-Result 'SC-006b' 'FAIL' "manifest contains $($nonM.Count) added/deleted row(s); the patch reproduces modified files only and cannot reconstruct them"
} elseif ($mRows.Count -eq 0) {
    Add-Result 'SC-006b' 'FAIL' 'manifest has no modified rows; there is nothing to reconstruct'
} else {
    $tmp = Join-Path $env:TEMP ("d3repro-" + [guid]::NewGuid().ToString('N'))
    $applyCode = -1; $matched = 0; $mismatch = 0; $reported = $false
    # Coverage first: the manifest must name every file the patch touches. Counting only the
    # listed rows lets a truncated manifest (1 of 2 patched files) report "verified 1/1".
    # git apply --numstat -p2 parses the same path form the reconstruction applies, so the
    # comparison cannot drift from the apply step.
    $patchFiles = @(); $patchParse = 0
    if ((Test-Path -LiteralPath $patch) -and (Get-Item $patch).Length -gt 0) {
        $ns = @(& git apply --numstat -p2 $patch 2>$null)
        $nsCode = $LASTEXITCODE
        if ($nsCode -eq 0) { $patchFiles = @($ns | Where-Object { $_ } | ForEach-Object { (($_ -split "`t")[-1]).Trim() }) }
        else { $patchParse = 1 }
    }
    $manifestPaths = @($mRows | ForEach-Object { ($_ -split "`t")[1] })
    $patchSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($pf in $patchFiles) { if ($pf) { [void]$patchSet.Add($pf) } }
    $manifestSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($mp in $manifestPaths) { if ($mp) { [void]$manifestSet.Add($mp) } }
    $unlisted = @($patchFiles | Where-Object { $_ -and -not $manifestSet.Contains($_) })
    $unpatched = @($manifestPaths | Where-Object { $_ -and -not $patchSet.Contains($_) })
    try {
        Copy-Item -LiteralPath $pristine -Destination $tmp -Recurse -Force
        Remove-Item (Join-Path $tmp '.git') -Force -ErrorAction SilentlyContinue
        if ((Test-Path -LiteralPath $patch) -and (Get-Item $patch).Length -gt 0) {
            Push-Location $tmp
            $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
            & git -c core.autocrlf=false apply --whitespace=nowarn -p2 $patch 2>$null
            $applyCode = $LASTEXITCODE
            $ErrorActionPreference = $prev
            Pop-Location
        }
        # Every 'M' row must match; a partial match is not a verification.
        foreach ($line in $mRows) {
            $p = $line -split "`t"
            $f = Join-Path $tmp ($p[1] -replace '/', '\')
            if ((Test-Path -LiteralPath $f) -and (Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash -eq $p[2]) { $matched++ } else { $mismatch++ }
        }
        $coverageOk = ($patchParse -eq 0) -and ($patchFiles.Count -gt 0) -and ($unlisted.Count -eq 0) -and ($unpatched.Count -eq 0)
        $ok = ($applyCode -eq 0) -and ($mismatch -eq 0) -and ($matched -eq $mRows.Count) -and $coverageOk
    } catch { Add-Result 'SC-006b' 'FAIL' $_.Exception.Message; $reported = $true }
    finally { Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue }
    if (-not $reported) {
        $detail = "git apply exit $applyCode, M rows verified $matched/$($mRows.Count), mismatches $mismatch, patch files $($patchFiles.Count), manifest misses $($unlisted.Count) patched file(s), patch misses $($unpatched.Count) manifest file(s)"
        if ($patchParse -ne 0) { $detail += ', patch could not be parsed' }
        Add-Result 'SC-006b' ($(if ($ok) { 'PASS' } else { 'FAIL' })) $detail
    }
}
if ($expDir) { Remove-Item $expDir -Recurse -Force -ErrorAction SilentlyContinue }

# SC-003 smoke
if (-not $Sc006Only) {
if ($SkipSmoke) { Add-Result 'SC-003' 'SKIPPED' 'smoke skipped by flag' }
elseif (-not (Test-Path -LiteralPath $exe)) { Add-Result 'SC-003' 'FAIL' 'no exe' }
elseif (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir)) { Add-Result 'SC-003' 'SKIPPED' 'no game data dir (set -DataDir or D3_DATA_DIR)' }
else {
    $out = Join-Path $repo ("targets\$TargetName\build\smoke-" + [guid]::NewGuid().ToString('N'))
    & (Join-Path $repo 'scripts\smoke-win11-amd64.ps1') -DataDir $DataDir -Exe $exe -OutDir $out
    $code = $LASTEXITCODE
    Add-Result 'SC-003' ($(if ($code -eq 0) { 'PASS' } else { 'FAIL' })) "smoke exit $code, output $out"
}
}
$fail = @($results | Where-Object Status -eq 'FAIL').Count
Write-Host ("SUMMARY pass={0} fail={1} skipped={2}" -f @($results | Where-Object Status -eq 'PASS').Count, $fail, @($results | Where-Object Status -eq 'SKIPPED').Count)
exit ([int]($fail -gt 0))
