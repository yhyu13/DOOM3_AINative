<#
.SYNOPSIS
  Fail when a tracked file looks like shader source or a retail ARB program body.

.DESCRIPTION
  Blunt by design: a false positive costs one review reply, a leaked retail shader costs the
  project. Scans superproject files only; the pristine submodule is a gitlink and is excluded.

  Checks:
    1. no tracked *.vfp / *.vp / *.fp
    2. no tracked path containing a glprogs/ directory
    3. no tracked text file containing an ARB program header (ARBvp1.0 / ARBfp1.0)

  The three exact paths that discuss the header as policy are allowlisted.

  Exit 1 on any violation. Read-only.
#>
[CmdletBinding()]
param(
    # Test seam: check this list instead of the git index. Paths are repo-relative.
    [string[]]$TrackedFiles
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$specDir   = Split-Path -Parent $scriptDir
$repoRoot  = Split-Path -Parent (Split-Path -Parent $specDir)

$allowHeaderIn = @(
    'specs/008-classic-shaders/provenance.md',
    'specs/008-classic-shaders/tools/provenance-check.ps1',
    # Existing engine-analysis doc: quotes the directive name in a table, contains no program body.
    'doom3_orig/doc/02_renderer.md'
)

$textExtensions = @('.md', '.cpp', '.h', '.hpp', '.c', '.cc', '.txt', '.ps1', '.json', '.yml', '.yaml', '.mtr', '.cmake', '.hlsl', '.hlsli', '.glsl', '.frag', '.vert', '.comp', '.inc', '.fx', '.shader', '.wgsl')

Push-Location $repoRoot
try {
    if ($TrackedFiles) { $tracked = @($TrackedFiles) }
    else {
        $tracked = @(git ls-files)
        if ($LASTEXITCODE -ne 0) { throw "git ls-files failed" }
    }
} finally {
    Pop-Location
}

$failures = New-Object System.Collections.ArrayList
$headerPattern = 'ARBvp1\.0|ARBfp1\.0'

foreach ($file in $tracked) {
    $fwd = $file -replace '\\', '/'

    if ($fwd -match '\.(vfp|vp|fp)$') {
        [void]$failures.Add("tracked shader source: $fwd")
        continue
    }
    if ($fwd -match '(^|/)glprogs/') {
        [void]$failures.Add("tracked glprogs path: $fwd")
        continue
    }

    $ext = [System.IO.Path]::GetExtension($fwd).ToLowerInvariant()
    if ($textExtensions -notcontains $ext) { continue }
    if ($allowHeaderIn -contains $fwd) { continue }

    $full = Join-Path $repoRoot ($fwd -replace '/', '\')
    if (-not (Test-Path -LiteralPath $full)) { continue }
    $content = [System.IO.File]::ReadAllText($full)
    if ($content -match $headerPattern) {
        [void]$failures.Add("ARB program header in tracked file: $fwd")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "PROVENANCE CHECK FAILED ($($failures.Count))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "PROVENANCE OK: $($tracked.Count) tracked files, no shader source, no ARB program bodies"
exit 0
