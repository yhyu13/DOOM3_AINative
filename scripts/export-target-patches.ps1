#Requires -Version 5.1
<#
.SYNOPSIS
  Export every difference between a target copy and the pristine source as one patch + a manifest.
.DESCRIPTION
  Writes targets/<Name>/patches/all-changes.patch (git diff --no-index per changed file, a/ b/ paths
  relative to neo/..) and all-changes.manifest.txt (status, path, SHA-256). Reviewable form of FR-007.
  -OutDir writes both files elsewhere, so a verifier can avoid overwriting the tracked copies.
  -SourceDir overrides the pristine tree; it exists so the negative controls can run against a
  synthetic tree without touching doom3_orig/doom3.
  The patch reproduces modified files only. Added ('A') and deleted ('D') files are manifest rows
  without content, so such an export is NOT a reconstruction and must not report success: this
  script writes the files for inspection, warns, and exits 1. Deliver added/deleted engine files
  through the reviewed fork/submodule path (specs/004-quality-renderer/engine-integration.md §3).
  Exit 0 = complete modified-only export; 1 = printed but incomplete (A/D rows present);
  2 = environment error.
.EXAMPLE
  .\scripts\export-target-patches.ps1 -Name win11-amd64
.EXAMPLE
  .\scripts\export-target-patches.ps1 -Name win11-amd64 -OutDir $env:TEMP\d3patches
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Name, [string]$OutDir, [string]$SourceDir)
$ErrorActionPreference = 'Stop'
trap { Write-Host "ERROR $($_.Exception.Message)" -ForegroundColor Red; exit 2 }
$repo = Split-Path -Parent $PSScriptRoot
$src = if ($SourceDir) { $SourceDir } else { Join-Path $repo 'doom3_orig\doom3' }
$dst = Join-Path $repo "targets\$Name\doom3"
$out = if ($OutDir) { $OutDir } else { Join-Path $repo "targets\$Name\patches" }
if (-not (Test-Path -LiteralPath (Join-Path $src 'neo'))) { Write-Host 'ERROR: pristine missing' -ForegroundColor Red; exit 2 }
if (-not (Test-Path -LiteralPath (Join-Path $dst 'neo'))) { Write-Host 'ERROR: target missing' -ForegroundColor Red; exit 2 }
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Get-Map([string]$Root) {
    $m = @{}
    Get-ChildItem -LiteralPath $Root -Recurse -File -Force | ForEach-Object {
        $rel = $_.FullName.Substring($Root.Length).TrimStart('\','/') -replace '\\','/'
        if ($rel -eq '.git' -or $rel.StartsWith('.git/')) { return }
        $m[$rel] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    $m
}
$a = Get-Map $src; $b = Get-Map $dst
$manifest = New-Object System.Collections.Generic.List[string]
$changed = $b.Keys | Where-Object { $a.ContainsKey($_) -and $a[$_] -ne $b[$_] } | Sort-Object
$added = $b.Keys | Where-Object { -not $a.ContainsKey($_) } | Sort-Object
$removed = $a.Keys | Where-Object { -not $b.ContainsKey($_) } | Sort-Object

# Source files are CRLF. PowerShell text pipelines strip CR from native output, which corrupts patches,
# so git's stdout is captured as raw bytes via System.Diagnostics.Process.
function Invoke-GitBytes([string]$WorkDir, [string[]]$GitArgs) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'git'
    $psi.Arguments = ($GitArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $psi.WorkingDirectory = $WorkDir
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $ms = New-Object System.IO.MemoryStream
    $proc.StandardOutput.BaseStream.CopyTo($ms)
    $err = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    # git diff --no-index exits 1 when the files differ (expected) and >1 on error.
    if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 1) {
        throw ("git diff failed (exit {0}): {1}" -f $proc.ExitCode, $err.Trim())
    }
    , $ms.ToArray()
}
$patchStream = New-Object System.IO.MemoryStream
foreach ($f in $changed) {
    $manifest.Add("M`t$f`t$($b[$f])")
    $scratch = Join-Path ([IO.Path]::GetTempPath()) ("d3exp-" + [guid]::NewGuid().ToString('N'))
    $win = $f -replace '/', '\'
    $relDir = Split-Path -Parent $win
    foreach ($side in 'a', 'b') { New-Item -ItemType Directory -Force -Path (Join-Path $scratch (Join-Path $side $relDir)) | Out-Null }
    Copy-Item -LiteralPath (Join-Path $src $win) (Join-Path $scratch "a\$win")
    Copy-Item -LiteralPath (Join-Path $dst $win) (Join-Path $scratch "b\$win")
    $bytes = Invoke-GitBytes $scratch @('-c', 'core.autocrlf=false', '-c', 'core.safecrlf=false', '-c', 'core.quotepath=false', 'diff', '--no-index', '--no-color', '--text', '--', "a/$f", "b/$f")
    $patchStream.Write($bytes, 0, $bytes.Length)
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
}
foreach ($f in $added) { $manifest.Add("A`t$f`t$($b[$f])") }
foreach ($f in $removed) { $manifest.Add("D`t$f`t") }
[System.IO.File]::WriteAllBytes((Join-Path $out 'all-changes.patch'), $patchStream.ToArray())
[System.IO.File]::WriteAllLines((Join-Path $out 'all-changes.manifest.txt'), $manifest, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("Modified {0}, Added {1}, Removed {2}" -f @($changed).Count, @($added).Count, @($removed).Count)
Write-Host "Wrote $out\all-changes.patch ($((Get-Item (Join-Path $out 'all-changes.patch')).Length) bytes) and manifest"
if (@($added).Count -gt 0 -or @($removed).Count -gt 0) {
    Write-Warning ("INCOMPLETE: the patch reproduces modified files only; {0} added and {1} removed file(s) are manifest rows. Deliver those through the engine fork/submodule path. This export is not a reconstruction." -f @($added).Count, @($removed).Count)
    exit 1
}
exit 0
