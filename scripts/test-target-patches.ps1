#Requires -Version 5.1
<#
.SYNOPSIS
  Negative controls for apply-target-patches.ps1 on a synthetic, self-cleaning target.
.DESCRIPTION
  Creates targets/zz-patchtest-<guid> with two files and a manifest, then asserts:
    * a missing target copy exits 2;
    * an added ('A') row exits 1, and still exits 1 with -AllowIncomplete;
    * a manifest whose modified rows all match exits 0;
    * a partially patched copy exits 1;
    * a real patch applies, produces the expected bytes, and exits 0;
    * a manifest with a wrong hash after a successful apply exits 1.
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
$powershell = (Get-Command powershell -ErrorAction Stop).Source
$name = 'zz-patchtest-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$base = Join-Path $repo "targets\$name"
$dst = Join-Path $base 'doom3'
$patch = Join-Path $base 'patches\all-changes.patch'
$manifest = Join-Path $base 'patches\all-changes.manifest.txt'
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

if (-not (Test-Path -LiteralPath $apply)) { Write-Host "ERROR missing $apply" -ForegroundColor Red; exit 1 }

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
} finally {
    if ($name -like 'zz-patchtest-*' -and (Test-Path -LiteralPath $base)) {
        Remove-Item -LiteralPath $base -Recurse -Force
    }
}

Write-Host ("TEST target-patches: {0} passed, {1} failed" -f $passes, $failures)
if ($failures -gt 0) { exit 1 }
exit 0
