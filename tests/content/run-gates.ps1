#Requires -Version 5.1
<#
.SYNOPSIS
    Positive run plus the six negative controls for the content contract gate (spec 009, T-006).

.DESCRIPTION
    Builds a clean fixture tree under out/content-gates/ from tests/content/fixtures/, generates the
    fixture PNG with py -3 (fixtures carry no committed binaries), then runs the gate with
    tests/content/fixtures into a fresh case directory and mutates that copy one way:

      1. a .tga copied into hero/                      -> extension rule
      2. the clean hero file's SHA-256 on a denylist   -> denylist rule
      3. its provenance record deleted                 -> provenance rule
      4. LICENSE replaced with a non-CC0 text          -> licence rule
      5. a channel state removed from a sidecar         -> channel state rule
      6. a sidecar mtr pointed at a missing file        -> mtr pairing rule

    Every case starts from an untouched copy of the fixture template, so no case can leak into the
    next. The clean case must exit 0 with VALIDATE OK and exercise the absent-pack SKIPPED path. Each
    mutation must exit 1 with the expected diagnostic phrase. Exits 0 only when all seven behave, and
    prints GATES OK: 1 clean + 6 negative controls.

.PARAMETER RepoRoot
    Repository root. Defaults to the nearest ancestor of this script that holds .git.

.EXAMPLE
    powershell -NoProfile -File tests/content/run-gates.ps1
#>
[CmdletBinding()]
param(
    [string]$RepoRoot
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$libPath = Join-Path $PSScriptRoot '..\..\content\tools\content-lib.ps1'
if (-not (Test-Path -LiteralPath $libPath -PathType Leaf)) {
    Write-Host "GATES FAILED: shared library not found: $libPath"
    exit 1
}
. $libPath

if (-not $RepoRoot) {
    try {
        $RepoRoot = Get-RepoRoot -Start $PSScriptRoot
    } catch {
        Write-Host "GATES FAILED: cannot locate the repository root: $($_.Exception.Message)"
        exit 1
    }
}
$script:RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)

$script:Validator = Join-Path $script:RepoRoot 'content\tools\validate.ps1'
$script:FixtureSource = Join-Path $PSScriptRoot 'fixtures'
$script:WorkDir = Join-Path $script:RepoRoot 'out\content-gates'
$script:LogDir = Join-Path $script:WorkDir '_logs'

function Fail {
    param([string]$Message)
    Write-Host "GATES FAILED: $Message"
    exit 1
}

if (-not (Test-Path -LiteralPath $script:Validator -PathType Leaf)) { Fail "validator not found: $($script:Validator)" }
if (-not (Test-Path -LiteralPath $script:FixtureSource -PathType Container)) { Fail "fixtures not found: $($script:FixtureSource)" }
if (-not (Get-Command py -ErrorAction SilentlyContinue)) { Fail 'py -3 with Pillow is required to generate the fixture PNG' }

if (Test-Path -LiteralPath $script:WorkDir) {
    Remove-Item -LiteralPath $script:WorkDir -Recurse -Force
}
New-Item -ItemType Directory -Path $script:LogDir -Force | Out-Null

$template = Join-Path $script:WorkDir '_template'
New-Item -ItemType Directory -Path $template -Force | Out-Null
# Only this gate's own fixture content is copied; tests/content/fixtures also holds fixtures owned by
# other tasks (for example the RXGB golden image) that this gate must neither depend on nor modify.
Copy-Item -LiteralPath (Join-Path $script:FixtureSource 'content') -Destination (Join-Path $template 'content') -Recurse -Force

function Invoke-Child {
    param(
        [string]$FilePath,
        [string]$Arguments,
        [string]$OutFile,
        [string]$ErrFile
    )
    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $OutFile -RedirectStandardError $ErrFile
    return $process.ExitCode
}

# The PNG is generated here, never committed: fixtures hold text only.
$pythonFile = Join-Path $script:WorkDir 'make-fixture-png.py'
$pythonSource = @'
import sys
from PIL import Image

path = sys.argv[1]
size = int(sys.argv[2])
image = Image.new("RGBA", (size, size), (32, 96, 160, 255))
image.putpixel((0, 0), (255, 255, 255, 255))
image.save(path, format="PNG")
'@
[System.IO.File]::WriteAllText($pythonFile, $pythonSource + "`n", (New-Object System.Text.UTF8Encoding($false)))

$seedDir = Join-Path $script:WorkDir '_seed'
New-Item -ItemType Directory -Path $seedDir -Force | Out-Null
$seedPng = Join-Path $seedDir 'hero_dielectric.png'
$pythonExit = Invoke-Child -FilePath 'py' -Arguments ('-3 "{0}" "{1}" 8' -f $pythonFile, $seedPng) `
    -OutFile (Join-Path $script:LogDir 'py.out.txt') -ErrFile (Join-Path $script:LogDir 'py.err.txt')
if ($pythonExit -ne 0 -or -not (Test-Path -LiteralPath $seedPng -PathType Leaf)) {
    Fail "py -3 could not generate the fixture PNG (exit $pythonExit); see $($script:LogDir)"
}
$seedHash = (Get-FileHash -LiteralPath $seedPng -Algorithm SHA256).Hash.ToLowerInvariant()

function New-Case {
    param([string]$Name)
    $caseRoot = Join-Path $script:WorkDir $Name
    if (Test-Path -LiteralPath $caseRoot) {
        Remove-Item -LiteralPath $caseRoot -Recurse -Force
    }
    Copy-Item -LiteralPath $template -Destination $caseRoot -Recurse -Force
    $heroDir = Join-Path $caseRoot 'content\hero\dielectric'
    Copy-Item -LiteralPath $seedPng -Destination (Join-Path $heroDir 'hero_dielectric.png') -Force
    $provPath = Join-Path $heroDir 'provenance.json'
    $provText = [System.IO.File]::ReadAllText($provPath).Replace('__PNG_SHA256__', $seedHash)
    [System.IO.File]::WriteAllText($provPath, $provText, (New-Object System.Text.UTF8Encoding($false)))
    return $caseRoot
}

function Invoke-Gate {
    param(
        [string]$CaseRoot,
        [string]$DenylistPath
    )
    $caseName = Split-Path -Leaf $CaseRoot
    $outFile = Join-Path $script:LogDir "$caseName.out.txt"
    $errFile = Join-Path $script:LogDir "$caseName.err.txt"
    $arguments = ('-NoProfile -File "{0}" -RepoRoot "{1}" -ContentRoot "{2}" -Denylist "{3}" -SkipTracked' -f `
            $script:Validator, $CaseRoot, (Join-Path $CaseRoot 'content'), $DenylistPath)
    $exitCode = Invoke-Child -FilePath 'powershell' -Arguments $arguments -OutFile $outFile -ErrFile $errFile
    $text = ''
    if (Test-Path -LiteralPath $outFile) { $text += [System.IO.File]::ReadAllText($outFile) }
    if (Test-Path -LiteralPath $errFile) { $text += [System.IO.File]::ReadAllText($errFile) }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = $text }
}

$script:Failures = New-Object 'System.Collections.Generic.List[string]'

function Report {
    param(
        [string]$Name,
        $Result,
        [int]$ExpectedExit,
        [string]$Phrase,
        [string]$Kind = 'diagnostic'
    )
    $phraseOk = ($Result.Output.IndexOf($Phrase, [System.StringComparison]::Ordinal) -ge 0)
    $exitOk = ($Result.ExitCode -eq $ExpectedExit)
    $diagnostic = ''
    foreach ($line in ($Result.Output -split "`r?`n")) {
        if ($line.StartsWith(' - ')) { $diagnostic = $line; break }
    }
    if (-not $diagnostic) {
        foreach ($line in ($Result.Output -split "`r?`n")) {
            if ($line.Trim().Length -gt 0) { $diagnostic = $line.Trim(); break }
        }
    }
    $status = if ($exitOk -and $phraseOk) { 'ok' } else { 'FAILED' }
    Write-Host ("{0}: {1}: exit {2} (expected {3}), {4} '{5}' -> {6}" -f `
            $Name, $status, $Result.ExitCode, $ExpectedExit, $Kind, $Phrase, $diagnostic)
    if (-not $exitOk) {
        $script:Failures.Add("$Name`: exit $($Result.ExitCode), expected $ExpectedExit") | Out-Null
    }
    if (-not $phraseOk) {
        $script:Failures.Add("$Name`: output does not contain '$Phrase'") | Out-Null
    }
    return $exitOk -and $phraseOk
}

$emptyDenylist = Join-Path $script:WorkDir 'denylist-empty.json'

# 0. Clean tree: exits 0, prints VALIDATE OK, and splits the corpus steps as SKIPPED.
$cleanCase = New-Case -Name 'clean'
$clean = Invoke-Gate -CaseRoot $cleanCase -DenylistPath $emptyDenylist
$cleanOk = Report -Name 'clean' -Result $clean -ExpectedExit 0 -Phrase 'VALIDATE OK' -Kind 'summary'
if ($cleanOk -and $clean.Output.IndexOf('SKIPPED: pack', [System.StringComparison]::Ordinal) -lt 0) {
    Write-Host 'clean: FAILED: the absent-pack SKIPPED path was not exercised'
    $script:Failures.Add('clean: the absent-pack SKIPPED path was not exercised') | Out-Null
}

# 1. Extension rule: a .tga under hero/.
$case = New-Case -Name 'mutation-extension'
Copy-Item -LiteralPath $seedPng -Destination (Join-Path $case 'content\hero\dielectric\stray.tga') -Force
Report -Name 'extension' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $emptyDenylist) `
    -ExpectedExit 1 -Phrase 'extension not allowed' | Out-Null

# 2. Denylist rule: the clean hero hash on a pack denylist.
$case = New-Case -Name 'mutation-denylist'
$denyPath = Join-Path $script:WorkDir 'denylist-with-fixture-hash.json'
Write-Json -Object ([pscustomobject]@{ schema = 'doom3-content-denylist/1'; sha256 = @($seedHash) }) -Path $denyPath
Report -Name 'denylist' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $denyPath) `
    -ExpectedExit 1 -Phrase 'pack denylist' | Out-Null

# 3. Provenance rule: the hero file exists but has no record.
$case = New-Case -Name 'mutation-provenance'
$provPath = Join-Path $case 'content\hero\dielectric\provenance.json'
Write-Json -Object ([pscustomobject]@{ schema = 'doom3-provenance/1'; material = 'dielectric'; assets = @() }) -Path $provPath
Report -Name 'provenance' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $emptyDenylist) `
    -ExpectedExit 1 -Phrase 'no provenance record' | Out-Null

# 4. Licence rule: LICENSE present but not CC0.
$case = New-Case -Name 'mutation-licence'
$licencePath = Join-Path $case 'content\hero\dielectric\LICENSE'
[System.IO.File]::WriteAllText($licencePath, "Copyright 2026. All rights reserved. Licensed for review only.`n",
    (New-Object System.Text.UTF8Encoding($false)))
Report -Name 'licence' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $emptyDenylist) `
    -ExpectedExit 1 -Phrase 'LICENSE does not declare CC0' | Out-Null

# 5. Channel state rule: metalness has no state.
$case = New-Case -Name 'mutation-channel-state'
$sidecarPath = Join-Path $case 'content\materials\hero_dielectric.mat.json'
$sidecar = Read-Json -Path $sidecarPath
$sidecar.channels.metalness.PSObject.Properties.Remove('state')
Write-Json -Object $sidecar -Path $sidecarPath
Report -Name 'channel-state' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $emptyDenylist) `
    -ExpectedExit 1 -Phrase 'channel metalness has no state' | Out-Null

# 6. mtr pairing rule: the sidecar names an .mtr that does not exist.
$case = New-Case -Name 'mutation-mtr'
$sidecarPath = Join-Path $case 'content\materials\hero_dielectric.mat.json'
$sidecar = Read-Json -Path $sidecarPath
$sidecar.mtr = 'content/materials/does_not_exist.mtr'
Write-Json -Object $sidecar -Path $sidecarPath
Report -Name 'mtr' -Result (Invoke-Gate -CaseRoot $case -DenylistPath $emptyDenylist) `
    -ExpectedExit 1 -Phrase 'mtr file does not exist' | Out-Null

if ($script:Failures.Count -gt 0) {
    Write-Host "GATES FAILED: $($script:Failures.Count) case(s):"
    foreach ($failure in $script:Failures) { Write-Host " - $failure" }
    exit 1
}

Write-Host 'GATES OK: 1 clean + 6 negative controls'
exit 0
