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

# The directive name may be *discussed* in prose, tables, comments and inline code. It may never appear
# as a program line. Line-scoped on purpose: a file-level allowlist let a whole retail program be
# appended to an allowlisted document and still pass.
$selfPath = 'specs/008-classic-shaders/tools/provenance-check.ps1'
$docLikePattern = '^\s*(#|\||>|\*|-|//|/\*|\*>)'
$inlineCodePattern = '`ARB(?:vp|fp)1\.0`'

# A real ARB program body has several opcode lines. Counting them catches a body pasted without its
# header, which a header-string check alone cannot see.
$opcodePattern = '(?m)^[ \t]{0,8}(?:TEMP|PARAM|ATTRIB|OUTPUT|MOV|ADD|MUL|DP3|DP4|SUB|RCP|RSQ|MIN|MAX|ABS|FLR|FRC|KIL|TEX|TXB|TXP|LIT|DST|SCS|CMP|LRP|MAD|POW|EX2|LG2|SGE|SGT|SLT|SLE|END)[ \t]'
$opcodeThreshold = 4

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

    $full = Join-Path $repoRoot ($fwd -replace '/', '\')
    if (-not (Test-Path -LiteralPath $full)) { continue }
    $content = [System.IO.File]::ReadAllText($full)

    $text = $content -replace "`r`n", "`n"
    if ($fwd -ne $selfPath) {
        $lineNo = 0
        foreach ($line in ($text -split "`n")) {
            $lineNo++
            if ($line -notmatch $headerPattern) { continue }
            if (($line -match $docLikePattern) -or ($line -match $inlineCodePattern)) { continue }
            [void]$failures.Add("ARB program header as a program line: ${fwd}:$lineNo")
        }
    }

    $opcodeLines = [regex]::Matches($text, $opcodePattern).Count
    if ($opcodeLines -ge $opcodeThreshold) {
        [void]$failures.Add("ARB opcode body ($opcodeLines opcode lines) in tracked file: $fwd")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "PROVENANCE CHECK FAILED ($($failures.Count))" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "PROVENANCE OK: $($tracked.Count) tracked files, no shader source, no ARB program bodies"
exit 0
