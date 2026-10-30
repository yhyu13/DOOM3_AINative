#Requires -Version 5.1
<#
.SYNOPSIS
  End-to-end gate for the snapshot/migration scripts (spec 010, issue #77).

.DESCRIPTION
  Proves both directions on this machine, without game data:

    1. migrate -Profile portable          exit 0, no archive/, both bundles verify
    2. migrate -Profile snapshot          exit 0, archive/, inventory, notes, facts, checksums exist
    3. snapshot contents                  no *.pk4/*.pk3/*.pak, no CMakeFiles
    4. deploy -CheckOnly                  checksums verified, exit 0
    5. deploy full restore (-NoNetwork)   exit 0, gitlink SHA, HEAD^{tree} equality,
                                          "0 mismatches" for both target patches,
                                          "25 skills checked, 25 OK", clean status
    6. verify-skills in the restore       exit 0 (explicit skills gate)

  Everything is written under a unique directory in %TEMP% and removed in `finally` unless
  -KeepTemp is passed. Builds are deliberately excluded: -Build needs VS2022 + SDK 10.0.26100,
  and this gate must run without them (T026 records build evidence separately).

.PARAMETER KeepTemp
  Keep the snapshot and restore directories and print their paths, even on success.

.PARAMETER WorkDir
  Use this directory instead of a generated one under %TEMP%. Must be empty or absent.

.EXAMPLE
  .\scripts\test-migration.ps1

.EXAMPLE
  .\scripts\test-migration.ps1 -KeepTemp

.NOTES
  Exit 0 = every gate passed; 1 = at least one gate failed. The failing gate is named in the output.
#>
[CmdletBinding()]
param(
    [switch]$KeepTemp,
    [string]$WorkDir
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$script:results = @()
$script:failures = 0

function Write-Gate {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][bool]$Passed,
        [Parameter(Mandatory = $true)][string]$Detail
    )
    $label = if ($Passed) { 'PASS' } else { 'FAIL' }
    $color = if ($Passed) { 'Green' } else { 'Red' }
    Write-Host ("{0} {1,-34} {2}" -f $label, $Name, $Detail) -ForegroundColor $color
    $script:results += ("{0} {1}: {2}" -f $label, $Name, $Detail)
    if (-not $Passed) { $script:failures++ }
}

function Invoke-Script {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [string[]]$ScriptArgs = @()
    )
    Push-Location -LiteralPath $WorkingDirectory
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'   # native stderr under EAP=Stop throws NativeCommandError in PS 5.1
    try {
        $out = @(& powershell -NoProfile -ExecutionPolicy Bypass -File $Path @ScriptArgs 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
        Pop-Location
    }
    return [pscustomobject]@{ Lines = $out; Code = $code; Text = ($out -join "`n") }
}

function Invoke-Git {
    param([Parameter(Mandatory = $true)][string]$Repository, [Parameter(Mandatory = $true)][string[]]$GitArgs)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = @(& git -C $Repository @GitArgs 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    return [pscustomobject]@{ Lines = $out; Code = $code; Text = ($out -join "`n") }
}

function Remove-TreeRobust {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    try { Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop }
    catch {
        # Deep build/restore trees can defeat the provider; cmd's rd is the fallback.
        & cmd /c ('rd /s /q "' + $Path + '"') | Out-Null
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $repo 'scripts\migrate-repo.ps1'))) {
    Write-Host "ERROR: scripts/migrate-repo.ps1 not found beside this script ($repo)" -ForegroundColor Red
    exit 1
}

$root = if ($WorkDir) { $WorkDir } else { Join-Path $env:TEMP ('d3mig-gate-' + [guid]::NewGuid().ToString('N').Substring(0, 8)) }
if (Test-Path -LiteralPath $root) {
    $existing = @(Get-ChildItem -LiteralPath $root -Force)
    if ($existing.Count -gt 0) {
        Write-Host "ERROR: WorkDir is not empty: $root" -ForegroundColor Red
        exit 1
    }
} else {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
}
$portableDir = Join-Path $root 'snap-portable'
$snapDir = Join-Path $root 'snap'
$restoreDir = Join-Path $root 'restore'
$migrate = Join-Path $repo 'scripts\migrate-repo.ps1'

# A tiny ignored artifact proves the archive -> restore path on a fresh worktree, where the
# tracked tree carries no evidence or caches. `out/` is git-ignored, so the worktree stays clean.
$markerPath = Join-Path $repo 'out\gate-marker.txt'
$markerText = 'spec-010 gate marker'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $markerPath) | Out-Null
[IO.File]::WriteAllText($markerPath, $markerText, (New-Object Text.UTF8Encoding($false)))

Write-Host "repo  : $repo"
Write-Host "temp  : $root"
Write-Host ""

try {
    # ---- 1. portable profile -------------------------------------------------
    $r = Invoke-Script -Path $migrate -WorkingDirectory $repo -ScriptArgs @('-Profile', 'portable', '-OutDir', $portableDir)
    if ($r.Code -ne 0) {
        $r.Lines | Select-Object -Last 8 | ForEach-Object { Write-Host "    $_" }
        Write-Gate 'migrate portable' $false "exit $($r.Code)"
    } else {
        Write-Gate 'migrate portable' $true 'exit 0'
    }
    Write-Gate 'portable has no archive/' (-not (Test-Path -LiteralPath (Join-Path $portableDir 'archive'))) $(if (Test-Path -LiteralPath (Join-Path $portableDir 'archive')) { 'archive/ present' } else { 'archive/ absent' })

    $portableBundle = Join-Path $portableDir 'repo-all.bundle'
    if (Test-Path -LiteralPath $portableBundle) {
        $v = Invoke-Git -Repository $repo -GitArgs @('bundle', 'verify', $portableBundle)
        Write-Gate 'portable bundle verify' ($v.Code -eq 0) "exit $($v.Code)"
    } else {
        Write-Gate 'portable bundle verify' $false 'repo-all.bundle missing'
    }

    # ---- 2. snapshot profile -------------------------------------------------
    $r = Invoke-Script -Path $migrate -WorkingDirectory $repo -ScriptArgs @('-Profile', 'snapshot', '-NoDanglingBundle', '-OutDir', $snapDir)
    if ($r.Code -ne 0) {
        $r.Lines | Select-Object -Last 8 | ForEach-Object { Write-Host "    $_" }
        Write-Gate 'migrate snapshot' $false "exit $($r.Code)"
    } else {
        Write-Gate 'migrate snapshot' $true 'exit 0'
    }
    $required = @('archive', 'IGNORED-INVENTORY.txt', 'MIGRATION-NOTES.md', 'machine-facts.json', 'checksums.sha256', 'repo-all.bundle', 'doom3-submodule.bundle', 'deploy-repo.ps1')
    $missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $snapDir $_)) })
    Write-Gate 'snapshot layout' ($missing.Count -eq 0) $(if ($missing.Count -eq 0) { "$($required.Count)/$($required.Count) present" } else { 'missing: ' + ($missing -join ', ') })

    $archivedMarker = Join-Path $snapDir 'archive\out\gate-marker.txt'
    Write-Gate 'archive captures ignored data' (Test-Path -LiteralPath $archivedMarker) 'archive/out/gate-marker.txt'

    $bundle = Join-Path $snapDir 'repo-all.bundle'
    $v = Invoke-Git -Repository $repo -GitArgs @('bundle', 'verify', $bundle)
    Write-Gate 'snapshot bundle verify' ($v.Code -eq 0) "exit $($v.Code)"
    $subBundle = Join-Path $snapDir 'doom3-submodule.bundle'
    if (Test-Path -LiteralPath $subBundle) {
        $v = Invoke-Git -Repository $repo -GitArgs @('bundle', 'verify', $subBundle)
        Write-Gate 'submodule bundle verify' ($v.Code -eq 0) "exit $($v.Code)"
    } else {
        Write-Gate 'submodule bundle verify' $true 'submodule not initialised; skipped'
    }

    $checksumFile = Join-Path $snapDir 'checksums.sha256'
    $checksumLines = if (Test-Path -LiteralPath $checksumFile) { @(Get-Content -LiteralPath $checksumFile | Where-Object { $_ -and $_.Trim() }).Count } else { 0 }
    $snapshotFiles = @(Get-ChildItem -LiteralPath $snapDir -Recurse -File -Force -ErrorAction SilentlyContinue).Count
    # The manifest must cover every produced file except itself.
    Write-Gate 'checksum coverage' ($checksumLines -eq ($snapshotFiles - 1)) "$checksumLines entries for $snapshotFiles files"

    # ---- 3. no game data or build intermediates ------------------------------
    $forbidden = @(Get-ChildItem -LiteralPath $snapDir -Recurse -File -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @('.pk4', '.pk3', '.pak') })
    Write-Gate 'no game data in snapshot' ($forbidden.Count -eq 0) "$($forbidden.Count) pk4/pk3/pak"
    $intermediates = @(Get-ChildItem -LiteralPath $snapDir -Recurse -Directory -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -in @('CMakeFiles', '__pycache__') })
    Write-Gate 'no build intermediates' ($intermediates.Count -eq 0) "$($intermediates.Count) CMakeFiles/__pycache__"

    # ---- 4. checksum check without writing -----------------------------------
    $deploy = Join-Path $snapDir 'deploy-repo.ps1'
    $r = Invoke-Script -Path $deploy -WorkingDirectory $repo -ScriptArgs @('-Bundle', $snapDir, '-Dest', (Join-Path $root 'check'), '-CheckOnly')
    $checksumsVerified = $r.Text -match 'checksums verified'
    Write-Gate 'deploy -CheckOnly' ($r.Code -eq 0 -and $checksumsVerified) "exit $($r.Code), checksums verified=$checksumsVerified"

    # ---- 5. full offline restore (targets included) --------------------------
    $r = Invoke-Script -Path $deploy -WorkingDirectory $repo -ScriptArgs @('-Bundle', $snapDir, '-Dest', $restoreDir, '-NoNetwork')
    Write-Gate 'deploy restore' ($r.Code -eq 0) "exit $($r.Code)"
    if ($r.Code -ne 0) { $r.Lines | Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" } }

    $patchLines = @($r.Lines | Where-Object { $_ -match '0 mismatches' }).Count
    Write-Gate 'target patches verified' ($patchLines -ge 2) "$patchLines target(s) reported 0 mismatches"
    Write-Gate 'skills gate in restore' (($r.Text -match '25 skills checked, 25 OK') -and ($r.Text -match 'byte-identical')) '25/25, byte-identical'

    if (Test-Path -LiteralPath (Join-Path $restoreDir '.git')) {
        $srcTree = (Invoke-Git -Repository $repo -GitArgs @('rev-parse', 'HEAD^{tree}')).Text.Trim()
        $dstTree = (Invoke-Git -Repository $restoreDir -GitArgs @('rev-parse', 'HEAD^{tree}')).Text.Trim()
        Write-Gate 'HEAD tree equality' ($srcTree -eq $dstTree) "$srcTree / $dstTree"

        $status = @(Invoke-Git -Repository $restoreDir -GitArgs @('status', '--porcelain')).Lines | Where-Object { $_ -and $_.Trim() }
        Write-Gate 'restored status clean' (@($status).Count -eq 0) "$(@($status).Count) dirty path(s)"

        $subDest = Join-Path $restoreDir 'doom3_orig\doom3'
        if (Test-Path -LiteralPath (Join-Path $subDest '.git')) {
            $expected = (Invoke-Git -Repository $repo -GitArgs @('ls-tree', 'HEAD', '--', 'doom3_orig/doom3')).Text
            $pinned = if ($expected -match 'commit\s+([0-9a-f]{40})') { $Matches[1] } else { '' }
            $actual = (Invoke-Git -Repository $subDest -GitArgs @('rev-parse', 'HEAD')).Text.Trim()
            Write-Gate 'submodule gitlink' ($pinned -and $pinned -eq $actual) "$($pinned.Substring(0, [Math]::Min(12, $pinned.Length)))"
        } else {
            Write-Gate 'submodule gitlink' $false 'submodule missing in restore'
        }
        $restoredMarker = Join-Path $restoreDir 'out\gate-marker.txt'
        $markerSame = (Test-Path -LiteralPath $restoredMarker) -and ((Get-Content -LiteralPath $restoredMarker -Raw).Trim() -eq $markerText)
        Write-Gate 'archive restored' $markerSame 'out/gate-marker.txt byte-equal'
    } else {
        Write-Gate 'HEAD tree equality' $false 'restore produced no repository'
        Write-Gate 'restored status clean' $false 'restore produced no repository'
    }

    # ---- 6. explicit skills gate in the restored tree ------------------------
    $verifySkills = Join-Path $restoreDir 'scripts\verify-skills.ps1'
    if (Test-Path -LiteralPath $verifySkills) {
        $r = Invoke-Script -Path $verifySkills -WorkingDirectory $restoreDir
        Write-Gate 'verify-skills (restored)' ($r.Code -eq 0) "exit $($r.Code)"
    } else {
        Write-Gate 'verify-skills (restored)' $false 'scripts/verify-skills.ps1 missing'
    }
} finally {
    if (Test-Path -LiteralPath $markerPath) { Remove-Item -LiteralPath $markerPath -Force -ErrorAction SilentlyContinue }
    $markerParent = Split-Path -Parent $markerPath
    if ((Test-Path -LiteralPath $markerParent) -and @(Get-ChildItem -LiteralPath $markerParent -Force).Count -eq 0) {
        Remove-Item -LiteralPath $markerParent -Force -ErrorAction SilentlyContinue
    }
    if ($KeepTemp) {
        Write-Host ''
        Write-Host "Kept: $root" -ForegroundColor Yellow
    } else {
        Remove-TreeRobust -Path $root
    }
}

Write-Host ''
Write-Host ("{0} gate(s) passed, {1} failed" -f ($script:results.Count - $script:failures), $script:failures)
if ($script:failures -gt 0) { exit 1 }
exit 0
