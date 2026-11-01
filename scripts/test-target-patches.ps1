#Requires -Version 5.1
<#
.SYNOPSIS
  Negative controls for apply-target-patches.ps1 on a synthetic, self-cleaning target.
.DESCRIPTION
  Creates targets/zz-patchtest-<guid> with two files and a manifest, then asserts:
    * a missing target copy exits 2;
    * an added ('A') row exits 1, and still exits 1 with -AllowIncomplete;
    * a deleted ('D') row exits 1, and still exits 1 with -AllowIncomplete;
    * a manifest whose modified rows all match exits 0;
    * a partially patched copy exits 1;
    * a real patch applies, produces the expected bytes, and exits 0;
    * a manifest with a wrong hash after a successful apply exits 1.
  Then creates a synthetic pristine tree and a synthetic modified target and asserts:
    * an export that contains only modified files exits 0 with an M-only manifest;
    * an export containing an added file exits 1 (not a PASS) and records the 'A' row;
    * an export containing a deleted file exits 1 (not a PASS) and records the 'D' row;
    * the verifier refuses a manifest with an added or deleted row;
    * the verifier refuses a partial M-row match (1 of 2 rows verified), including when both rows are
      files the patch touches;
    * the verifier refuses a truncated manifest that omits a patched file (1 of 2 patch files listed);
    * the verifier refuses a manifest row the patch never touches (the file exists and hashes match);
    * the verifier fails closed when the patch disagrees with the manifest, and when it is absent.
  Every control asserts the exit code and, where relevant, the reported reason, so a check that
  always passes cannot satisfy it.
  It never reads or writes a real target copy, game data or the pristine submodule.
  Exit 0 = every control met its expected exit code; 1 = at least one control failed.
.EXAMPLE
  .\scripts\test-target-patches.ps1
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$apply = Join-Path $PSScriptRoot 'apply-target-patches.ps1'
$export = Join-Path $PSScriptRoot 'export-target-patches.ps1'
$verify = Join-Path $PSScriptRoot 'verify-win11-amd64.ps1'
$powershell = (Get-Command powershell -ErrorAction Stop).Source
$suffix = [guid]::NewGuid().ToString('N').Substring(0, 8)
$name = 'zz-patchtest-' + $suffix
$vname = $name + '-v'
$base = Join-Path $repo "targets\$name"
$dst = Join-Path $base 'doom3'
$patch = Join-Path $base 'patches\all-changes.patch'
$manifest = Join-Path $base 'patches\all-changes.manifest.txt'
$srcBase = Join-Path $repo "targets\zz-patchsrc-$suffix"
$vbase = Join-Path $repo "targets\$vname"
$vdst = Join-Path $vbase 'doom3'
$vctl = Join-Path $vbase 'ctl'
$failures = 0
$passes = 0

function Assert-Exit([string]$Label, [int]$Expected, [int]$Actual) {
    if ($Actual -eq $Expected) {
        Write-Host ("PASS {0}: exit {1}" -f $Label, $Actual)
        $script:passes++
    } else {
        Write-Host ("FAIL {0}: expected exit {1}, got {2}" -f $Label, $Expected, $Actual) -ForegroundColor Red
        $script:failures++
    }
}

function Assert-True([string]$Label, [bool]$Condition) {
    if ($Condition) {
        Write-Host ("PASS {0}" -f $Label)
        $script:passes++
    } else {
        Write-Host ("FAIL {0}" -f $Label) -ForegroundColor Red
        $script:failures++
    }
}

function Assert-Contains([string]$Label, [string]$Text, [string]$Needle) {
    Assert-True $Label ($Text -like "*$Needle*")
}


function Invoke-Apply([string[]]$Extra) {
    $out = & $powershell -NoProfile -ExecutionPolicy Bypass -File $apply -Name $name @Extra 2>&1
    $code = $LASTEXITCODE
    if ($code -ne 0) { $out | ForEach-Object { Write-Host ("    {0}" -f $_) } }
    return $code
}

function Write-Manifest([string[]]$Rows) {
    Set-Content -LiteralPath $manifest -Value $Rows -Encoding ASCII
}

function Write-Ascii([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.ASCIIEncoding))
}

function Get-AsciiHash([string]$Text) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::ASCII.GetBytes($Text))) -replace '-', '') }
    finally { $sha.Dispose() }
}

function Invoke-Export([string]$OutDir, [string[]]$Extra = @()) {
    $out = & $powershell -NoProfile -ExecutionPolicy Bypass -File $export -Name $vname -SourceDir $srcBase -OutDir $OutDir @Extra 2>&1
    $code = $LASTEXITCODE
    if ($code -ne 0) { $out | ForEach-Object { Write-Host ("    {0}" -f $_) } }
    return $code
}

function Invoke-Verify([string[]]$Extra = @()) {
    $out = & $powershell -NoProfile -ExecutionPolicy Bypass -File $verify -Sc006Only -TargetName $vname -PristineDir $srcBase @Extra 2>&1
    $code = $LASTEXITCODE
    $script:lastVerifyOutput = ($out | Out-String)
    if ($code -ne 0) { $out | ForEach-Object { Write-Host ("    {0}" -f $_) } }
    return $code
}

function Get-ManifestRows([string]$Path) {
    @(Get-Content -LiteralPath $Path | Where-Object { $_ -and $_.Trim() })
}

foreach ($required in $apply, $export, $verify) {
    if (-not (Test-Path -LiteralPath $required)) { Write-Host "ERROR missing $required" -ForegroundColor Red; exit 1 }
}

try {
    New-Item -ItemType Directory -Force -Path (Join-Path $base 'patches'), (Join-Path $dst 'neo') | Out-Null
    Set-Content -LiteralPath (Join-Path $dst 'neo\file_a.cpp') -Value 'alpha' -NoNewline -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $dst 'neo\file_b.cpp') -Value 'bravo' -NoNewline -Encoding ASCII
    Set-Content -LiteralPath $patch -Value '' -Encoding ASCII
    $hashA = (Get-FileHash -LiteralPath (Join-Path $dst 'neo\file_a.cpp') -Algorithm SHA256).Hash
    $hashB = (Get-FileHash -LiteralPath (Join-Path $dst 'neo\file_b.cpp') -Algorithm SHA256).Hash
    $rowA = "M`tneo/file_a.cpp`t$hashA"
    $rowB = "M`tneo/file_b.cpp`t$hashB"

    # 1. Missing target copy is an environment error, not a pass.
    Remove-Item -LiteralPath $dst -Recurse -Force
    Write-Manifest @($rowA)
    Assert-Exit 'missing target' 2 (Invoke-Apply @())
    New-Item -ItemType Directory -Force -Path (Join-Path $dst 'neo') | Out-Null
    Set-Content -LiteralPath (Join-Path $dst 'neo\file_a.cpp') -Value 'alpha' -NoNewline -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $dst 'neo\file_b.cpp') -Value 'bravo' -NoNewline -Encoding ASCII

    # 2. Added row fails closed.
    Write-Manifest @(("A`tneo/new.cpp`t" + ('0' * 64)))
    Assert-Exit 'added row' 1 (Invoke-Apply @())

    # 3. -AllowIncomplete still reports the copy as incomplete.
    Write-Manifest @($rowA, ("A`tneo/new.cpp`t" + ('0' * 64)))
    Assert-Exit 'added row with -AllowIncomplete' 1 (Invoke-Apply @('-AllowIncomplete'))

    # 3b. A deleted row also fails closed, with and without -AllowIncomplete.
    Write-Manifest @($rowA, "D`tneo/file_b.cpp`t")
    Assert-Exit 'deleted row' 1 (Invoke-Apply @())
    Write-Manifest @($rowA, "D`tneo/file_b.cpp`t")
    Assert-Exit 'deleted row with -AllowIncomplete' 1 (Invoke-Apply @('-AllowIncomplete'))

    # 4. Every modified row matches: already patched.
    Write-Manifest @($rowA)
    Assert-Exit 'all modified rows match' 0 (Invoke-Apply @())

    # 5. Partial match is refused before applying.
    Write-Manifest @($rowA, ("M`tneo/file_b.cpp`t" + ('0' * 64)))
    Assert-Exit 'partial modified rows' 1 (Invoke-Apply @())

    # 6. A real non-empty patch applies and every manifest hash verifies.
    $fileA = Join-Path $dst 'neo\file_a.cpp'
    Write-Ascii $fileA "alpha`n"
    $patchText = "diff --git a/a/neo/file_a.cpp b/b/neo/file_a.cpp`n--- a/a/neo/file_a.cpp`n+++ b/b/neo/file_a.cpp`n@@ -1 +1 @@`n-alpha`n+alpha-patched`n"
    Write-Ascii $patch $patchText
    Write-Manifest @(("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")))
    Assert-Exit 'real apply verifies hashes' 0 (Invoke-Apply @())
    $content = [IO.File]::ReadAllText($fileA)
    if ($content -eq "alpha-patched`n") {
        Write-Host 'PASS applied content: alpha-patched'
        $script:passes++
    } else {
        Write-Host 'FAIL applied content: patch did not produce the expected bytes' -ForegroundColor Red
        $script:failures++
    }

    # 7. A successful apply with a wrong manifest hash is still a failure.
    Write-Ascii $fileA "alpha`n"
    Write-Ascii $patch $patchText
    Write-Manifest @(("M`tneo/file_a.cpp`t" + (Get-AsciiHash "not-the-applied-content`n")))
    Assert-Exit 'hash mismatch after apply' 1 (Invoke-Apply @())

    # --- Exporter and verifier controls (T084) on a synthetic pristine + target pair ---
    New-Item -ItemType Directory -Force -Path (Join-Path $srcBase 'neo'), (Join-Path $vdst 'neo'), $vctl | Out-Null
    Write-Ascii (Join-Path $srcBase 'neo\file_a.cpp') "alpha`n"
    Write-Ascii (Join-Path $srcBase 'neo\file_b.cpp') "bravo`n"
    Write-Ascii (Join-Path $srcBase 'neo\file_c.cpp') "charlie`n"
    Write-Ascii (Join-Path $vdst 'neo\file_a.cpp') "alpha-patched`n"
    Write-Ascii (Join-Path $vdst 'neo\file_b.cpp') "bravo`n"
    Write-Ascii (Join-Path $vdst 'neo\file_c.cpp') "charlie`n"
    $ctlPatch = "diff --git a/a/neo/file_a.cpp b/b/neo/file_a.cpp`n--- a/a/neo/file_a.cpp`n+++ b/b/neo/file_a.cpp`n@@ -1 +1 @@`n-alpha`n+alpha-patched`n"

    # 8. Modified-only export is complete and exits 0.
    $outOk = Join-Path $vbase 'export-ok'
    Assert-Exit 'export modified-only exits 0' 0 (Invoke-Export $outOk)
    $okRows = Get-ManifestRows (Join-Path $outOk 'all-changes.manifest.txt')
    Assert-True 'export modified-only manifest is M-only' (($okRows.Count -eq 1) -and ((($okRows[0] -split "`t")[0]) -eq 'M'))
    Assert-True 'export modified-only writes a non-empty patch' ((Get-Item (Join-Path $outOk 'all-changes.patch')).Length -gt 0)

    # 9. An added file makes the export incomplete: exit 1, A row recorded.
    $added = Join-Path $vdst 'neo\added.cpp'
    Write-Ascii $added "added`n"
    $outA = Join-Path $vbase 'export-added'
    Assert-Exit 'export with added file refuses PASS' 1 (Invoke-Export $outA)
    $aRows = Get-ManifestRows (Join-Path $outA 'all-changes.manifest.txt')
    Assert-True 'export with added file records an A row' (@($aRows | Where-Object { ($_ -split "`t")[0] -eq 'A' }).Count -eq 1)

    # 10. A deleted file makes the export incomplete: exit 1, D row recorded.
    Remove-Item -LiteralPath $added -Force
    Remove-Item -LiteralPath (Join-Path $vdst 'neo\file_b.cpp') -Force
    $outD = Join-Path $vbase 'export-deleted'
    Assert-Exit 'export with deleted file refuses PASS' 1 (Invoke-Export $outD)
    $dRows = Get-ManifestRows (Join-Path $outD 'all-changes.manifest.txt')
    Assert-True 'export with deleted file records a D row' (@($dRows | Where-Object { ($_ -split "`t")[0] -eq 'D' }).Count -eq 1)
    Write-Ascii (Join-Path $vdst 'neo\file_b.cpp') "bravo`n"

    # 11. Positive verifier control: a modified-only reconstruction is fully verified.
    Assert-Exit 'verify modified-only reconstruction PASSes' 0 (Invoke-Verify @())
    Assert-Contains 'verify reports full M coverage' $script:lastVerifyOutput 'M rows verified 1/1'

    # 12. Verifier refuses an added file (the exporter also exits 1).
    Write-Ascii $added "added`n"
    Assert-Exit 'verify refuses added file' 1 (Invoke-Verify @())
    Assert-Contains 'verify reports added/deleted incompleteness' $script:lastVerifyOutput 'manifest has added/deleted rows'
    Remove-Item -LiteralPath $added -Force

    # 13. Verifier refuses a deleted file.
    Remove-Item -LiteralPath (Join-Path $vdst 'neo\file_b.cpp') -Force
    Assert-Exit 'verify refuses deleted file' 1 (Invoke-Verify @())
    Assert-Contains 'verify reports deleted incompleteness' $script:lastVerifyOutput 'manifest has added/deleted rows'
    Write-Ascii (Join-Path $vdst 'neo\file_b.cpp') "bravo`n"

    # 13b. The verifier's own completeness guard, independent of the exporter exit code.
    $pA = Join-Path $vctl 'addedrow'
    New-Item -ItemType Directory -Force -Path $pA | Out-Null
    Set-Content -LiteralPath (Join-Path $pA 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")),
        ("A`tneo/added.cpp`t" + (Get-AsciiHash "added`n")))
    Write-Ascii (Join-Path $pA 'all-changes.patch') $ctlPatch
    Assert-Exit 'verify refuses an A row in a supplied manifest' 1 (Invoke-Verify @('-PatchDir', $pA))
    Assert-Contains 'verify reports the supplied A row' $script:lastVerifyOutput 'added/deleted row'
    $pD = Join-Path $vctl 'deletedrow'
    New-Item -ItemType Directory -Force -Path $pD | Out-Null
    Set-Content -LiteralPath (Join-Path $pD 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")),
        "D`tneo/file_b.cpp`t")
    Write-Ascii (Join-Path $pD 'all-changes.patch') $ctlPatch
    Assert-Exit 'verify refuses a D row in a supplied manifest' 1 (Invoke-Verify @('-PatchDir', $pD))
    Assert-Contains 'verify reports the supplied D row' $script:lastVerifyOutput 'added/deleted row'

    # 14a. A truncated manifest that omits a patched file is refused (reviewer repro on PR #100:
    # a 2-file patch with 1 manifest row reported "M rows verified 1/1" and exited 0).
    $patch2 = $ctlPatch +
        "diff --git a/a/neo/file_b.cpp b/b/neo/file_b.cpp`n--- a/a/neo/file_b.cpp`n+++ b/b/neo/file_b.cpp`n@@ -1 +1 @@`n-bravo`n+bravo-patched`n"
    $p3a = Join-Path $vctl 'truncated'
    New-Item -ItemType Directory -Force -Path $p3a | Out-Null
    Set-Content -LiteralPath (Join-Path $p3a 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")))
    Write-Ascii (Join-Path $p3a 'all-changes.patch') $patch2
    Assert-Exit 'verify refuses a manifest that omits a patched file' 1 (Invoke-Verify @('-PatchDir', $p3a))
    Assert-Contains 'verify reports the omitted patch file' $script:lastVerifyOutput 'manifest misses 1 patched file'

    # 14b. The other direction: a manifest row the patch never touches must not count as verified
    # (the file exists and its hash matches, so only coverage can catch it).
    $p3c = Join-Path $vctl 'stale'
    New-Item -ItemType Directory -Force -Path $p3c | Out-Null
    Set-Content -LiteralPath (Join-Path $p3c 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")),
        ("M`tneo/file_c.cpp`t" + (Get-AsciiHash "charlie`n")))
    Write-Ascii (Join-Path $p3c 'all-changes.patch') $ctlPatch
    Assert-Exit 'verify refuses a manifest row the patch never touches' 1 (Invoke-Verify @('-PatchDir', $p3c))
    Assert-Contains 'verify reports the manifest row outside the patch' $script:lastVerifyOutput 'patch misses 1 manifest file'

    # 14c. A partial hash match over the patch's own files: both rows are patched, one hash is wrong.
    $p3d = Join-Path $vctl 'partial2'
    New-Item -ItemType Directory -Force -Path $p3d | Out-Null
    Set-Content -LiteralPath (Join-Path $p3d 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")),
        ("M`tneo/file_b.cpp`t" + (Get-AsciiHash "wrong-bravo`n")))
    Write-Ascii (Join-Path $p3d 'all-changes.patch') $patch2
    Assert-Exit 'verify refuses a partial hash match over patched files' 1 (Invoke-Verify @('-PatchDir', $p3d))
    Assert-Contains 'verify reports 1/2 verified over patched files' $script:lastVerifyOutput 'M rows verified 1/2'

    # 14. A partial M-row match (1 of 2) is refused, not counted as verified.
    $p3 = Join-Path $vctl 'partial'
    New-Item -ItemType Directory -Force -Path $p3 | Out-Null
    Set-Content -LiteralPath (Join-Path $p3 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")),
        ("M`tneo/file_b.cpp`t" + (Get-AsciiHash "wrong-bravo`n")))
    Write-Ascii (Join-Path $p3 'all-changes.patch') $ctlPatch
    Assert-Exit 'verify refuses partial M-row match' 1 (Invoke-Verify @('-PatchDir', $p3))
    Assert-Contains 'verify reports partial coverage 1/2' $script:lastVerifyOutput 'M rows verified 1/2'

    # 15. Patch disagrees with the manifest: apply succeeds, the hash does not.
    $p4 = Join-Path $vctl 'disagree'
    New-Item -ItemType Directory -Force -Path $p4 | Out-Null
    Set-Content -LiteralPath (Join-Path $p4 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "different-content`n")))
    Write-Ascii (Join-Path $p4 'all-changes.patch') $ctlPatch
    Assert-Exit 'verify fails patch/manifest disagreement' 1 (Invoke-Verify @('-PatchDir', $p4))
    Assert-Contains 'verify reports zero verified rows' $script:lastVerifyOutput 'M rows verified 0/1'

    # 16. A manifest without a patch fails closed instead of verifying nothing.
    $p5 = Join-Path $vctl 'nopatch'
    New-Item -ItemType Directory -Force -Path $p5 | Out-Null
    Set-Content -LiteralPath (Join-Path $p5 'all-changes.manifest.txt') -Encoding ASCII -Value @(
        ("M`tneo/file_a.cpp`t" + (Get-AsciiHash "alpha-patched`n")))
    Assert-Exit 'verify fails a missing patch' 1 (Invoke-Verify @('-PatchDir', $p5))
    Assert-Contains 'verify reports apply not run' $script:lastVerifyOutput 'git apply exit -1'
} finally {
    if ($name -like 'zz-patchtest-*') {
        foreach ($p in $base, $vbase, $srcBase) {
            if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }
        }
    }
}

Write-Host ("TEST target-patches: {0} passed, {1} failed" -f $passes, $failures)
if ($failures -gt 0) { exit 1 }
exit 0
