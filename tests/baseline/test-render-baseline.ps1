#Requires -Version 5.1
<#
.SYNOPSIS
  Synthetic, game-data-free tests for scripts/test-render-baseline.ps1 (Q0 T008).

.DESCRIPTION
  Every case runs the public entry point only; no engine is started and no game data is read.
  The fixtures are a fake executable, a tiny zip used as base\demo00.pk4, and generated PNGs.
  The suite also mutates a temporary copy of the refusal block and asserts that the mutation is
  caught, so the refusal behaviour is not merely asserted by inspection.

  Exit 0 = all cases passed, 1 = at least one case failed.
#>
[CmdletBinding()]
param([string]$LogPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$target = Join-Path $repo 'scripts\test-render-baseline.ps1'
if (-not (Test-Path -LiteralPath $target)) { Write-Host "TEST FAIL missing target: $target"; exit 1 }

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Join-Path $env:TEMP ('q0-baseline-tests-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $root 'fixture'
$outRoot = Join-Path $root 'outroot'
$out = Join-Path $outRoot 'out'
New-Item -ItemType Directory -Force -Path $fixture, $outRoot, $out | Out-Null

$passes = 0
$failures = 0
function Write-Case([string]$Name, [bool]$Ok, [string]$Detail) {
    if ($Ok) { $script:passes++; Write-Host "PASS $Name $Detail" }
    else { $script:failures++; Write-Host "FAIL $Name $Detail" }
}
function Invoke-Baseline([string[]]$Arguments, [string]$ScriptPath = $target) {
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments 2>&1
    return [pscustomobject]@{ Code = $LASTEXITCODE; Output = ($output | Out-String) }
}
function New-Png([string]$Path, [System.Drawing.Color]$Colour) {
    $bmp = New-Object System.Drawing.Bitmap 64, 64
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear($Colour)
    $g.Dispose()
    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
function New-FakePk4([string]$Path, [string[]]$Entries) {
    $dir = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Entries) {
            $entry = $zip.CreateEntry($name)
            $stream = $entry.Open()
            $stream.WriteByte(0)
            $stream.Dispose()
        }
    } finally { $zip.Dispose() }
}

try {
    # --- fixtures ---------------------------------------------------------
    $exe = Join-Path $fixture 'DOOM3.exe'
    [IO.File]::WriteAllBytes($exe, [Text.Encoding]::ASCII.GetBytes('MZ synthetic fixture; never executed'))
    $exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()

    $data = Join-Path $fixture 'data\run'
    $pk4 = Join-Path $data 'base\demo00.pk4'
    $maps = @(
        'maps/game/demo_mars_city1.map',
        'maps/game/demo_mars_city2.map',
        'maps/game/demo_mc_underground.map'
    )
    New-FakePk4 $pk4 $maps
    $dataHash = (Get-FileHash -LiteralPath $pk4 -Algorithm SHA256).Hash.ToLowerInvariant()

    $dataMissing = Join-Path $fixture 'data-missing\run'
    New-FakePk4 (Join-Path $dataMissing 'base\demo00.pk4') @('maps/game/demo_mars_city1.map')

    $common = @('-Exe', $exe, '-DataDir', $data, '-OutRoot', $outRoot)

    # --- ListScenes -------------------------------------------------------
    $r = Invoke-Baseline @('-Mode', 'ListScenes')
    Write-Case 'ListScenes' ($r.Code -eq 0 -and $r.Output -match 'SCENE corridor' -and $r.Output -match 'SCENE open-view' -and $r.Output -match 'SCENE close-character' -and $r.Output -match 'SCENE moving-light-combat') "exit=$($r.Code)"

    # --- Check passes -----------------------------------------------------
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', (Join-Path $out 'check'), '-Scenes', 'open-view') + $common)
    Write-Case 'Check-valid' ($r.Code -eq 0 -and $r.Output -match 'result=PASS') "exit=$($r.Code)"

    # --- missing exe ------------------------------------------------------
    $r = Invoke-Baseline @('-Mode', 'Check', '-Exe', (Join-Path $fixture 'missing.exe'), '-DataDir', $data, '-OutDir', (Join-Path $out 'missing-exe'), '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Check-missing-exe' ($r.Code -eq 3) "exit=$($r.Code)"

    # --- missing data -----------------------------------------------------
    $r = Invoke-Baseline @('-Mode', 'Check', '-Exe', $exe, '-DataDir', (Join-Path $fixture 'no-data'), '-OutDir', (Join-Path $out 'missing-data'), '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Check-missing-data' ($r.Code -eq 4) "exit=$($r.Code)"

    # --- empty data (base dir exists, no pk4) -----------------------------
    $emptyData = Join-Path $fixture 'empty\run'
    New-Item -ItemType Directory -Force -Path (Join-Path $emptyData 'base') | Out-Null
    $r = Invoke-Baseline @('-Mode', 'Check', '-Exe', $exe, '-DataDir', $emptyData, '-OutDir', (Join-Path $out 'empty-data'), '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Check-empty-data' ($r.Code -eq 4) "exit=$($r.Code)"

    # --- non-empty output is refused and untouched -----------------------
    $nonEmpty = Join-Path $out 'nonempty'
    New-Item -ItemType Directory -Force -Path $nonEmpty | Out-Null
    $sentinel = Join-Path $nonEmpty 'sentinel.txt'
    [IO.File]::WriteAllText($sentinel, 'keep me')
    $before = @(Get-ChildItem -LiteralPath $nonEmpty -Force).Count
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', $nonEmpty, '-Scenes', 'open-view') + $common)
    $after = @(Get-ChildItem -LiteralPath $nonEmpty -Force).Count
    Write-Case 'Refuse-nonempty' ($r.Code -eq 5 -and (Test-Path -LiteralPath $sentinel) -and $before -eq $after) "exit=$($r.Code) files=$before->$after"

    # --- unsafe output: repository root ----------------------------------
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', $repo, '-Scenes', 'open-view') + $common)
    Write-Case 'Refuse-repo-root' ($r.Code -eq 2) "exit=$($r.Code)"

    # --- unknown scene ----------------------------------------------------
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', (Join-Path $out 'unknown'), '-Scenes', 'not-a-scene') + $common)
    Write-Case 'Reject-unknown-scene' ($r.Code -eq 6) "exit=$($r.Code)"

    # --- missing scene input (open-view map absent) ----------------------
    $r = Invoke-Baseline @('-Mode', 'Check', '-Exe', $exe, '-DataDir', $dataMissing, '-OutDir', (Join-Path $out 'missing-map'), '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Reject-missing-map' ($r.Code -eq 6) "exit=$($r.Code)"

    # --- expected hashes --------------------------------------------------
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', (Join-Path $out 'bad-exe-hash'), '-Scenes', 'open-view', '-ExpectedExeSha256', ('0' * 64)) + $common)
    Write-Case 'Reject-exe-hash' ($r.Code -eq 7) "exit=$($r.Code)"
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', (Join-Path $out 'bad-data-hash'), '-Scenes', 'open-view', '-ExpectedDataSha256', ('0' * 64)) + $common)
    Write-Case 'Reject-data-hash' ($r.Code -eq 4) "exit=$($r.Code)"

    # --- refusal is first: non-empty output beats a missing executable ----
    $nonEmpty2 = Join-Path $out 'nonempty-missing-exe'
    New-Item -ItemType Directory -Force -Path $nonEmpty2 | Out-Null
    [IO.File]::WriteAllText((Join-Path $nonEmpty2 'sentinel.txt'), 'keep me')
    $r = Invoke-Baseline @('-Mode', 'Check', '-Exe', (Join-Path $fixture 'missing.exe'), '-DataDir', $data, '-OutDir', $nonEmpty2, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Refusal-before-exe-check' ($r.Code -eq 5) "exit=$($r.Code)"

    # --- Capture mode uses the same preflight ----------------------------
    $nonEmpty3 = Join-Path $out 'nonempty-capture'
    New-Item -ItemType Directory -Force -Path $nonEmpty3 | Out-Null
    [IO.File]::WriteAllText((Join-Path $nonEmpty3 'sentinel.txt'), 'keep me')
    $r = Invoke-Baseline (@('-Mode', 'Capture', '-OutDir', $nonEmpty3, '-Scenes', 'open-view') + $common)
    Write-Case 'Capture-refuse-nonempty' ($r.Code -eq 5) "exit=$($r.Code)"
    $r = Invoke-Baseline (@('-Mode', 'Capture', '-OutDir', (Join-Path $out 'capture-fake-exe'), '-Scenes', 'open-view') + $common)
    Write-Case 'Capture-reject-fake-exe' ($r.Code -eq 3) "exit=$($r.Code)"

    # --- compare: identical and different --------------------------------
    function New-CompareFixture([string]$Dir, [System.Drawing.Color]$First, [System.Drawing.Color]$Second, [string]$ExtraLog = '', [string]$Marker = 'SCENE_READY_open-view', [string]$SceneId = 'open-view') {
        New-Png (Join-Path $Dir "$SceneId\run1\screenshot.png") $First
        New-Png (Join-Path $Dir "$SceneId\run2\screenshot.png") $Second
        foreach ($n in 1, 2) {
            [IO.File]::WriteAllText((Join-Path $Dir "$SceneId\run$n\qconsole.log"), "entities spawned`nSpawnPlayer: 0`n$Marker`n$ExtraLog")
        }
    }

    $cmp = Join-Path $out 'compare'
    New-CompareFixture $cmp ([System.Drawing.Color]::FromArgb(20, 20, 20)) ([System.Drawing.Color]::FromArgb(20, 20, 20))
    $r = Invoke-Baseline @('-Mode', 'Compare', '-OutDir', $cmp, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Compare-identical' ($r.Code -eq 0 -and $r.Output -match 'status=PASS') "exit=$($r.Code)"
    $reportHash = (Get-FileHash -LiteralPath (Join-Path $cmp 'comparison.json') -Algorithm SHA256).Hash
    $r = Invoke-Baseline @('-Mode', 'Compare', '-OutDir', $cmp, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    $reportHashAfter = (Get-FileHash -LiteralPath (Join-Path $cmp 'comparison.json') -Algorithm SHA256).Hash
    Write-Case 'Compare-refuse-overwrite' ($r.Code -eq 5 -and $reportHash -eq $reportHashAfter) "exit=$($r.Code)"

    $cmpDiff = Join-Path $out 'compare-different'
    New-CompareFixture $cmpDiff ([System.Drawing.Color]::FromArgb(20, 20, 20)) ([System.Drawing.Color]::White)
    $r = Invoke-Baseline @('-Mode', 'Compare', '-OutDir', $cmpDiff, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Compare-different' ($r.Code -eq 9 -and $r.Output -match 'status=FAIL') "exit=$($r.Code)"

    $cmpForbidden = Join-Path $out 'compare-forbidden'
    New-CompareFixture -Dir $cmpForbidden -First ([System.Drawing.Color]::FromArgb(20, 20, 20)) -Second ([System.Drawing.Color]::FromArgb(20, 20, 20)) -ExtraLog "ERROR: synthetic forbidden line`n"
    $r = Invoke-Baseline @('-Mode', 'Compare', '-OutDir', $cmpForbidden, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Compare-rejects-forbidden-log' ($r.Code -eq 9 -and $r.Output -match 'status=FAIL') "exit=$($r.Code)"

    $cmpMissing = Join-Path $out 'compare-missing'
    New-Png (Join-Path $cmpMissing 'open-view\run1\screenshot.png') ([System.Drawing.Color]::White)
    [IO.File]::WriteAllText((Join-Path $cmpMissing 'open-view\run1\qconsole.log'), "entities spawned`nSpawnPlayer: 0`nSCENE_READY_open-view`n")
    $r = Invoke-Baseline @('-Mode', 'Compare', '-OutDir', $cmpMissing, '-OutRoot', $outRoot, '-Scenes', 'open-view')
    Write-Case 'Compare-missing-run' ($r.Code -eq 6) "exit=$($r.Code)"

    # --- compare cannot write into the data/exe tree ---------------------
    $cmpInData = Join-Path $data 'build\out'
    New-Item -ItemType Directory -Force -Path $cmpInData | Out-Null
    $r = Invoke-Baseline @('-Mode', 'Compare', '-DataDir', $data, '-OutDir', $cmpInData, '-Scenes', 'open-view')
    Write-Case 'Compare-refuse-data-tree' ($r.Code -eq 2) "exit=$($r.Code)"
    $r = Invoke-Baseline @('-Mode', 'Compare', '-DataDir', $data, '-OutDir', $data, '-OutRoot', $fixture, '-Scenes', 'open-view')
    Write-Case 'Compare-refuse-data-root' ($r.Code -eq 2) "exit=$($r.Code)"
    $r = Invoke-Baseline @('-Mode', 'Compare', '-DataDir', $data, '-OutDir', (Join-Path $fixture 'data'), '-OutRoot', $fixture, '-Scenes', 'open-view')
    Write-Case 'Compare-refuse-data-parent' ($r.Code -eq 2) "exit=$($r.Code)"
    $exeOut = Join-Path $fixture 'build\out'
    New-Item -ItemType Directory -Force -Path $exeOut | Out-Null
    $r = Invoke-Baseline @('-Mode', 'Compare', '-Exe', $exe, '-OutDir', $exeOut, '-OutRoot', $fixture, '-Scenes', 'open-view')
    Write-Case 'Compare-refuse-exe-tree' ($r.Code -eq 2) "exit=$($r.Code)"

    # --- mutation harness: fresh fixture per mutation --------------------
    $source = Get-Content -LiteralPath $target -Raw
    $patternCheck = '(?s)if \(\$Mode -eq ''Check''\) \{\s*Write-Line "BASELINE refusing to inspect a non-empty OutDir: \$script:OutDirFull \(pass -Clean only with Capture/All\)"\s*exit 5\s*\}'
    $patternClean = '(?s)if \(-not \$Clean\) \{\s*Write-Line "BASELINE refusing to overwrite non-empty OutDir: \$script:OutDirFull \(pass -Clean\)"\s*exit 5\s*\}'
    $mutant = [regex]::Replace($source, $patternCheck, 'if ($false) { }', 1)
    $mutant = [regex]::Replace($mutant, $patternClean, 'if ($false) { }', 1)
    Write-Case 'Mutation-anchor-present' ($mutant -ne $source) 'both refusal blocks located exactly once'
    if ($mutant -ne $source) {
        $mutA = Join-Path $out 'mutant-a'
        New-Item -ItemType Directory -Force -Path $mutA | Out-Null
        [IO.File]::WriteAllText((Join-Path $mutA 'sentinel.txt'), 'keep me')
        $mutantPath = Join-Path $root 'mutant-a.ps1'
        [IO.File]::WriteAllText($mutantPath, $mutant, (New-Object Text.UTF8Encoding($false)))
        $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', $mutA, '-Scenes', 'open-view') + $common) $mutantPath
        $sentinelA = Test-Path -LiteralPath (Join-Path $mutA 'sentinel.txt')
        $safeA = ($r.Code -eq 5) -and $sentinelA
        Write-Case 'Mutation-refusal-removed' (-not $safeA) "mutation detected: exit=$($r.Code) sentinel=$sentinelA (safe path would be exit=5, sentinel=True)"
    }

    $mutant2 = [regex]::Replace($source, $patternCheck, { param($m) $m.Value -replace 'exit 5', 'exit 0' }, 1)
    $mutant2 = [regex]::Replace($mutant2, $patternClean, { param($m) $m.Value -replace 'exit 5', 'exit 0' }, 1)
    $mutB = Join-Path $out 'mutant-b'
    New-Item -ItemType Directory -Force -Path $mutB | Out-Null
    [IO.File]::WriteAllText((Join-Path $mutB 'sentinel.txt'), 'keep me')
    $mutantPath2 = Join-Path $root 'mutant-b.ps1'
    [IO.File]::WriteAllText($mutantPath2, $mutant2, (New-Object Text.UTF8Encoding($false)))
    $r = Invoke-Baseline (@('-Mode', 'Check', '-OutDir', $mutB, '-Scenes', 'open-view') + $common) $mutantPath2
    $sentinelB = Test-Path -LiteralPath (Join-Path $mutB 'sentinel.txt')
    $safeB = ($r.Code -eq 5) -and $sentinelB
    Write-Case 'Mutation-exit-code-removed' (-not $safeB) "mutation detected: exit=$($r.Code) sentinel=$sentinelB (safe path would be exit=5, sentinel=True)"

    # --- summary ---------------------------------------------------------
    Write-Host ("SUMMARY pass={0} fail={1}" -f $passes, $failures)
    if ($LogPath) {
        $text = "pass=$passes fail=$failures`n"
        [IO.File]::WriteAllText($LogPath, $text, (New-Object Text.UTF8Encoding($false)))
    }
    if ($failures -gt 0) { exit 1 }
    exit 0
} catch {
    Write-Host "TEST ERROR $($_.Exception.Message)"
    exit 1
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
