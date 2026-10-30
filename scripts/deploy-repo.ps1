#Requires -Version 5.1
<#
.SYNOPSIS
  Restore a repository on a new machine from a bundle set produced by migrate-repo.ps1.

.DESCRIPTION
  Rebuilds the state a plain clone would miss or choke on:
    1. creates the repository and fetches every branch and tag from repo-all.bundle,
    2. restores branch upstreams, the origin remote and the git config the source host relied on,
    3. initialises doom3_orig/doom3 from doom3-submodule.bundle (offline) and verifies the gitlink,
    4. restores the ignored-but-stateful files (spec-kit feature pointer, optional evidence),
    5. regenerates the .kilo/skills mirror and runs scripts/verify-skills.ps1,
    6. recreates every target copy under targets/*/doom3 from the pristine submodule plus its tracked
       patch, and verifies each copy by SHA-256,
    7. optionally configures and builds (cmake --preset win11-amd64), keeping the full log in build/,
    8. proves the result: clean `git status`, submodule SHA, skills gate, patch gate, artifacts.

  Dry-run the prerequisite table with -CheckOnly. Everything is skipped that needs data the bundle
  deliberately does not carry (game assets, DXC cache, Agent Manager worktrees).

.PARAMETER Bundle
  Directory produced by migrate-repo.ps1 (holds repo-all.bundle, machine-facts.json, ...).

.PARAMETER Dest
  New, empty directory for the restored repository.

.PARAMETER Build
  Configure and build the win11-amd64 target after restoring. Requires VS2022 v143 + SDK 10.0.26100.

.PARAMETER NoNetwork
  Do not contact the origin remote. The bundle already holds every commit; only remote-tracking
  refs stay stale until the next fetch.

.PARAMETER SkipTargets
  Do not recreate targets/*/doom3 (they are ~35 MB each and only needed to build or diff).

.PARAMETER SkipArchive
  Do not restore archive/ (evidence, out, tool caches, skills mirror, archived binaries and logs).
  The git bundles, submodule, target copies and gates still restore.

.PARAMETER Origin
  Origin URL to record. Defaults to https://github.com/yhyu13/DOOM3_AINative.git.

.PARAMETER EmsdkPath
  emsdk root to check when it is neither in $env:EMSDK nor %USERPROFILE%\emsdk (e.g. C:\emsdk).

.PARAMETER CheckOnly
  Print the prerequisite table and exit without touching -Dest.

.PARAMETER SkipChecksum
  Skip verifying checksums.sha256 in the bundle directory.

.EXAMPLE
  .\deploy-repo.ps1 -Bundle D:\transfer\doom3 -Dest D:\work\DOOM3_AINative -Build

.NOTES
  Exit 0 = restored and every gate passed; 1 = a gate failed (skills, patch reconstruction,
  build, dirty status); 2 = environment error (missing bundle, git, prerequisites, non-empty Dest).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bundle,
    [Parameter(Mandatory = $true)][string]$Dest,
    [switch]$Build,
    [switch]$NoNetwork,
    [switch]$SkipTargets,
    [switch]$SkipArchive,
    [string]$Origin = 'https://github.com/yhyu13/DOOM3_AINative.git',
    [string]$EmsdkPath,
    [switch]$CheckOnly,
    [switch]$SkipChecksum
)

$ErrorActionPreference = 'Stop'

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message, [int]$Code = 2)
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit $Code
}

function Step {
    param([Parameter(Mandatory = $true)][string]$Title)
    Write-Host ''
    Write-Host "== $Title" -ForegroundColor Cyan
}

function Write-TextFile {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$Text)
    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

function Get-ToolVersion {
    param([Parameter(Mandatory = $true)][string]$Command)
    $c = Get-Command $Command -ErrorAction SilentlyContinue
    if ($null -eq $c) { return $null }
    $v = @(& $Command --version 2>&1 | Select-Object -First 1)
    if ($v.Count -eq 0) { return $null }
    return ([string]$v[0]).Trim()
}

function Get-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
    $path = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null)
    if ($path.Count -eq 0) { return $null }
    $cmake = Join-Path ([string]$path[0]) 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    return [ordered]@{
        path  = [string]$path[0]
        cmake = if (Test-Path -LiteralPath $cmake) { $cmake } else { $null }
    }
}

function Get-WindowsSdks {
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
    if (-not (Test-Path -LiteralPath $kits)) { return @() }
    return @(Get-ChildItem -LiteralPath $kits -Directory | Select-Object -ExpandProperty Name | Sort-Object)
}

function Resolve-Emsdk {
    param([string]$Explicit)
    $candidates = @()
    if ($Explicit) { $candidates += $Explicit }
    if ($env:EMSDK) { $candidates += $env:EMSDK }
    if ($env:USERPROFILE) { $candidates += (Join-Path $env:USERPROFILE 'emsdk') }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return (Resolve-Path -LiteralPath $c).Path }
    }
    return $null
}

function Invoke-Script {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [string[]]$ScriptArgs = @()
    )
    Push-Location -LiteralPath $WorkingDirectory
    try {
        $out = @(& powershell -NoProfile -ExecutionPolicy Bypass -File $Path @ScriptArgs 2>&1)
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    return [pscustomobject]@{ Lines = $out; Code = $code }
}

function Show-Tail {
    param([string[]]$Lines, [int]$Count = 12)
    if (-not $Lines) { return }
    $tail = @($Lines | Select-Object -Last $Count)
    foreach ($l in $tail) { Write-Host "    $l" }
}

# ---------------------------------------------------------------- validate the bundle
Step 'Bundle'

if (-not (Test-Path -LiteralPath $Bundle)) { Fail "Bundle directory not found: $Bundle" 2 }
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$repoBundle = Join-Path $Bundle 'repo-all.bundle'
$subBundle = Join-Path $Bundle 'doom3-submodule.bundle'
$factsPath = Join-Path $Bundle 'machine-facts.json'
if (-not (Test-Path -LiteralPath $repoBundle)) { Fail "Missing repo-all.bundle in $Bundle" 2 }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail 'git is not on PATH.' 2 }

$facts = $null
if (Test-Path -LiteralPath $factsPath) {
    try { $facts = Get-Content -LiteralPath $factsPath -Raw | ConvertFrom-Json } catch { $facts = $null }
}

if (-not $SkipChecksum) {
    $checksums = Join-Path $Bundle 'checksums.sha256'
    if (Test-Path -LiteralPath $checksums) {
        $bad = 0
        foreach ($row in (Get-Content -LiteralPath $checksums | Where-Object { $_ -and $_.Trim() })) {
            $parts = $row -split '\s+', 2
            $file = Join-Path $Bundle ($parts[1] -replace '/', '\')
            if (-not (Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $parts[0]) {
                Write-Host "  MISMATCH $($parts[1])" -ForegroundColor Red
                $bad++
            }
        }
        if ($bad -gt 0) { Fail "$bad file(s) in the bundle failed SHA-256 verification" 2 }
        Write-Host '  checksums verified'
    } else {
        Write-Host '  checksums.sha256 absent; skipping' -ForegroundColor Yellow
    }
}

& git bundle verify $repoBundle | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "repo-all.bundle failed git bundle verify" 2 }
Write-Host "  repo-all.bundle      OK"
if (Test-Path -LiteralPath $subBundle) { Write-Host "  doom3-submodule.bundle present" } else { Write-Host '  doom3-submodule.bundle absent; submodule needs the network' -ForegroundColor Yellow }

# ---------------------------------------------------------------- prerequisites
Step 'Prerequisites'

$vs = Get-VisualStudio
$sdks = @(Get-WindowsSdks)
$emsdk = Resolve-Emsdk -Explicit $EmsdkPath
$cmakeCmd = if ($vs -and $vs.cmake) { $vs.cmake } else { (Get-Command cmake -ErrorAction SilentlyContinue).Source }
$emVersion = $null
if ($emsdk) {
    $f = Join-Path $emsdk 'upstream\emscripten\emscripten-version.txt'
    if (Test-Path -LiteralPath $f) { $emVersion = ((Get-Content -LiteralPath $f -TotalCount 1) -replace '"', '').Trim() }
}

$checks = @(
    @{ Name = 'git';            Value = Get-ToolVersion 'git';        Required = $true },
    @{ Name = 'node';           Value = Get-ToolVersion 'node';       Required = $false },
    @{ Name = 'uv';             Value = Get-ToolVersion 'uv';         Required = $false },
    @{ Name = 'py';             Value = Get-ToolVersion 'py';         Required = $false },
    @{ Name = 'gh';             Value = Get-ToolVersion 'gh';         Required = $false },
    @{ Name = 'Visual Studio';  Value = $(if ($vs) { $vs.path } else { $null }); Required = [bool]$Build },
    @{ Name = 'Windows SDK 10.0.26100'; Value = $(if ($sdks -contains '10.0.26100.0') { '10.0.26100.0' } else { $null }); Required = [bool]$Build },
    @{ Name = 'cmake';          Value = $cmakeCmd;                    Required = [bool]$Build },
    @{ Name = 'emsdk';          Value = $(if ($emsdk) { "$emsdk (Emscripten $emVersion)" } else { $null }); Required = $false }
)

$missingRequired = @()
foreach ($c in $checks) {
    $label = if ($c.Value) { 'OK  ' } elseif ($c.Required) { 'FAIL' } else { 'WARN' }
    $color = if ($c.Value) { 'Green' } elseif ($c.Required) { 'Red' } else { 'Yellow' }
    Write-Host ("  [{0}] {1,-26} {2}" -f $label, $c.Name, $(if ($c.Value) { $c.Value } else { 'not found' })) -ForegroundColor $color
    if (-not $c.Value -and $c.Required) { $missingRequired += $c.Name }
}
if ($missingRequired.Count -gt 0) { Fail ("Missing required prerequisite(s): " + ($missingRequired -join ', ')) 2 }
if ($facts) {
    $recorded = $facts.toolchain.emscripten
    if ($recorded -and $emVersion -and $recorded -ne $emVersion) {
        Write-Host "  [WARN] Emscripten $emVersion differs from the recorded $recorded (web leg only)" -ForegroundColor Yellow
    }
    if ($facts.ignoredState.gameDataTracked -eq $true) {
        Write-Host '  [WARN] The source machine tracked game data; the bundle should never contain it' -ForegroundColor Yellow
    }
}

if ($CheckOnly) {
    Write-Host ''
    Write-Host 'Check-only mode: nothing was written.' -ForegroundColor Cyan
    exit 0
}

# ---------------------------------------------------------------- destination
Step 'Destination'

if (Test-Path -LiteralPath $Dest) {
    $existing = @(Get-ChildItem -LiteralPath $Dest -Force)
    if ($existing.Count -gt 0) {
        Fail "Destination exists and is not empty: $Dest (deploy needs a fresh directory; remove it or choose another)" 2
    }
} else {
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
}
$Dest = (Resolve-Path -LiteralPath $Dest).Path
if ($Dest.Length -gt 150) {
    Write-Host "  [WARN] Destination path is $($Dest.Length) characters; Windows path limits may still bite" -ForegroundColor Yellow
}
Write-Host "  $Dest"

# ---------------------------------------------------------------- git init + fetch
Step 'Creating repository from bundle'

& git -C $Dest init --quiet
if ($LASTEXITCODE -ne 0) { Fail "git init failed in $Dest" 2 }
& git -C $Dest config core.longpaths true
& git -C $Dest config core.autocrlf false
if ($facts -and $facts.repository.gitConfig.ignorecase) {
    & git -C $Dest config core.ignorecase ([string]$facts.repository.gitConfig.ignorecase)
}
if ($facts -and $facts.repository.gitConfig.filemode) {
    & git -C $Dest config core.filemode ([string]$facts.repository.gitConfig.filemode)
}

$fetchArgs = @('-C', $Dest, 'fetch', $repoBundle, '+refs/heads/*:refs/heads/*', '+refs/tags/*:refs/tags/*')
& git @fetchArgs
if ($LASTEXITCODE -ne 0) { Fail "Fetching repo-all.bundle into $Dest failed" 2 }

$branches = @(& git -C $Dest for-each-ref '--format=%(refname:short)' refs/heads)
if ($branches.Count -eq 0) { Fail 'The bundle contained no branches' 2 }

# A snapshot can be taken from a detached checkout (CI, review worktree). Then the facts carry
# branch=null plus the commit SHA, and the restore must detach at that SHA instead of guessing main (B-1).
$recordedBranch = if ($facts) { $facts.repository.branch } else { $null }
$recordedHead = if ($facts) { $facts.repository.head } else { $null }
$default = $null
$checkoutMode = $null
if ($recordedBranch -and ($branches -contains $recordedBranch)) {
    $default = $recordedBranch
    & git -C $Dest symbolic-ref HEAD "refs/heads/$default"
    & git -C $Dest checkout --quiet $default
    if ($LASTEXITCODE -ne 0) { Fail "git checkout $default failed" 2 }
    $checkoutMode = "branch $default"
} elseif ($recordedHead) {
    & git -C $Dest cat-file -e "$recordedHead^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0) { Fail "recorded head $recordedHead is not present in the bundle" 2 }
    & git -C $Dest checkout --quiet --detach $recordedHead
    if ($LASTEXITCODE -ne 0) { Fail "git checkout --detach $recordedHead failed" 2 }
    $checkoutMode = "detached $($recordedHead.Substring(0, 12))"
} else {
    $default = if ($branches -contains 'main') { 'main' } else { $branches[0] }
    & git -C $Dest symbolic-ref HEAD "refs/heads/$default"
    & git -C $Dest checkout --quiet $default
    if ($LASTEXITCODE -ne 0) { Fail "git checkout $default failed" 2 }
    $checkoutMode = "branch $default"
}
Write-Host ("  branches restored: {0}" -f $branches.Count)
Write-Host ("  HEAD             : {0} @ {1}" -f $checkoutMode, (& git -C $Dest rev-parse --short HEAD))

# ---------------------------------------------------------------- origin + upstreams
Step 'Remote and branch upstreams'

if ($facts -and $facts.repository.remote) { $Origin = [string]$facts.repository.remote }
& git -C $Dest remote add origin $Origin
if ($LASTEXITCODE -ne 0) { Fail "git remote add origin $Origin failed" 2 }
Write-Host "  origin -> $Origin"

if (-not $NoNetwork) {
    & git -C $Dest fetch --quiet origin --prune
    if ($LASTEXITCODE -ne 0) {
        Write-Host '  [WARN] origin fetch failed (offline or credentials); bundle refs are still complete' -ForegroundColor Yellow
    } else {
        Write-Host '  origin fetched'
    }
}

if ($facts -and $facts.branches) {
    $upstreamCount = 0
    foreach ($b in $facts.branches) {
        if ($b.upstream -and $b.upstream -like 'origin/*') {
            & git -C $Dest config "branch.$($b.name).remote" 'origin'
            & git -C $Dest config "branch.$($b.name).merge" "refs/heads/$($b.upstream.Substring(7))"
            $upstreamCount++
        }
    }
    Write-Host "  upstreams restored: $upstreamCount"
} else {
    foreach ($name in $branches) {
        & git -C $Dest config "branch.$name.remote" 'origin'
        & git -C $Dest config "branch.$name.merge" "refs/heads/$name"
    }
    Write-Host "  upstreams defaulted to origin/<same-name>"
}

# ---------------------------------------------------------------- submodule
Step 'Pristine submodule'

$subRel = 'doom3_orig/doom3'
$gitlinkLine = @(& git -C $Dest ls-tree HEAD -- $subRel)
if ($gitlinkLine.Count -eq 0) {
    Write-Host '  no submodule gitlink at HEAD; skipped' -ForegroundColor Yellow
} else {
    $pinned = (($gitlinkLine[0] -split "`t")[0] -split ' ')[2]
    Write-Host "  pinned: $($pinned.Substring(0, 12))"
    $subName = 'doom3'
    $subUrl = $null
    if (Test-Path -LiteralPath (Join-Path $Dest '.gitmodules')) {
        $current = $null
        foreach ($line in (Get-Content -LiteralPath (Join-Path $Dest '.gitmodules'))) {
            if ($line -match '^\[submodule\s+"([^"]+)"\]') { $current = $Matches[1] }
            elseif ($current -and $line -match '^\s*path\s*=\s*(.+?)\s*$') {
                if ($Matches[1] -eq $subRel) { $subName = $current }
            } elseif ($current -and $line -match '^\s*url\s*=\s*(.+?)\s*$') {
                $subUrl = $Matches[1]
            }
        }
    }

    $usedBundle = $false
    if (Test-Path -LiteralPath $subBundle) {
        & git -C $Dest -c protocol.file.allow=always -c "submodule.$subName.url=$subBundle" submodule update --init
        if ($LASTEXITCODE -eq 0) { $usedBundle = $true }
        else { Write-Host '  offline submodule init failed; falling back to network' -ForegroundColor Yellow }
    }
    if (-not $usedBundle) {
        & git -C $Dest -c protocol.file.allow=always submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) { Fail 'git submodule update failed (no bundle and no network?)' 2 }
    }

    if ($subUrl) { & git -C $Dest config "submodule.$subName.url" $subUrl }

    $subDest = Join-Path $Dest ($subRel -replace '/', '\')
    if ($subUrl) {
        # The offline init cloned from the bundle file; point the submodule's own origin back at Git.
        & git -C $subDest remote set-url origin $subUrl 2>$null
    }
    $actual = (& git -C $subDest rev-parse HEAD).Trim()
    if ($actual -ne $pinned) {
        Fail "submodule HEAD $($actual.Substring(0,12)) does not match the gitlink $($pinned.Substring(0,12))" 1
    }
    $subDirty = @(& git -C $subDest status --porcelain | Where-Object { $_ -and $_.Trim() })
    if ($subDirty.Count -gt 0) { Fail "pristine submodule is dirty after checkout ($($subDirty.Count) paths)" 1 }
    Write-Host "  checkout verified: $($pinned.Substring(0, 12)), clean"
}

# ---------------------------------------------------------------- ignored state
Step 'Restoring ignored state'

$stateDir = Join-Path $Bundle 'local-state'
$restoredUntracked = @()
$feature = Join-Path $stateDir 'specify-feature.json'
if (Test-Path -LiteralPath $feature) {
    New-Item -ItemType Directory -Force -Path (Join-Path $Dest '.specify') | Out-Null
    Copy-Item -LiteralPath $feature -Destination (Join-Path $Dest '.specify\feature.json') -Force
    Write-Host '  .specify/feature.json'
}
$untrackedRoot = Join-Path $stateDir 'untracked'
if (Test-Path -LiteralPath $untrackedRoot) {
    $copied = 0
    $skipped = @()
    foreach ($f in (Get-ChildItem -LiteralPath $untrackedRoot -Recurse -File -Force)) {
        $rel = $f.FullName.Substring($untrackedRoot.Length).TrimStart('\')
        $target = Join-Path $Dest $rel
        if (Test-Path -LiteralPath $target) {
            $skipped += $rel
            continue
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $f.FullName -Destination $target -Force
        $restoredUntracked += ($rel -replace '\\', '/')
        $copied++
    }
    Write-Host ("  untracked files from the source worktree: restored {0}, skipped {1} that already exist" -f $copied, $skipped.Count)
    if ($skipped.Count -gt 0) { foreach ($s in $skipped) { Write-Host "    skipped: $s" -ForegroundColor Yellow } }
}
$archiveDir = Join-Path $Bundle 'archive'
$restoredArchive = @()
function Restore-ArchiveTree {
    param([Parameter(Mandatory = $true)][string]$Source, [Parameter(Mandatory = $true)][string]$Target, [Parameter(Mandatory = $true)][string]$Label)
    if (-not (Test-Path -LiteralPath $Source)) { return $false }
    New-Item -ItemType Directory -Force -Path $Target | Out-Null
    robocopy $Source $Target /E /NFL /NDL /NJH /NJS /NP /R:2 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { Fail "robocopy failed restoring $Label (exit $LASTEXITCODE)" 2 }
    $script:restoredArchive += $Label
    return $true
}

if (-not $SkipArchive -and (Test-Path -LiteralPath $archiveDir)) {
    $archiveItems = @(
        @{ Rel = 'showcase/evidence';           Label = 'showcase/evidence' },
        @{ Rel = 'out';                         Label = 'out (test evidence)' },
        @{ Rel = 'shaders/tools/.cache';        Label = 'shaders/tools/.cache (DXC)' },
        @{ Rel = 'content/tools/.cache';        Label = 'content/tools/.cache' },
        @{ Rel = '.specify/integrations/.cache'; Label = '.specify/integrations/.cache' },
        @{ Rel = '.kilo/skills';                Label = '.kilo/skills (mirror)' }
    )
    foreach ($item in $archiveItems) {
        $src = Join-Path $archiveDir ($item.Rel -replace '/', '\')
        $dst = Join-Path $Dest ($item.Rel -replace '/', '\')
        Restore-ArchiveTree -Source $src -Target $dst -Label $item.Label | Out-Null
    }

    # Archived binaries: keep the snapshot's build artifacts so a restore is usable without a build.
    foreach ($d in (Get-ChildItem -LiteralPath $archiveDir -Directory -Filter 'build*' | Where-Object { $_.Name -ne 'build-logs' })) {
        $bin = Join-Path $d.FullName 'bin'
        if (-not (Test-Path -LiteralPath $bin)) { continue }
        Restore-ArchiveTree -Source $bin -Target (Join-Path $Dest ($d.Name + '\bin')) -Label ($d.Name + '/bin') | Out-Null
    }

    # Archived logs go back to the repository root and the build directories they came from.
    $logsRoot = Join-Path $archiveDir 'build-logs'
    if (Test-Path -LiteralPath $logsRoot) {
        foreach ($d in (Get-ChildItem -LiteralPath $logsRoot -Directory)) {
            $target = if ($d.Name -eq '_root') { $Dest } else { Join-Path $Dest $d.Name }
            Restore-ArchiveTree -Source $d.FullName -Target $target -Label ('build-logs/' + $d.Name) | Out-Null
        }
    }

    # Target copies are rebuilt from pristine + patch; the archived tree stays for inspection.
    $archTargets = Join-Path $archiveDir 'targets'
    if (Test-Path -LiteralPath $archTargets) {
        Write-Host "  archived target copies kept under $archTargets (not restored; targets are rebuilt from pristine + patch)" -ForegroundColor Yellow
    }
    if ($restoredArchive.Count -gt 0) {
        Write-Host ("  archive restored: {0}" -f ($restoredArchive -join ', '))
    }
} elseif ($SkipArchive) {
    Write-Host '  archive/ skipped (-SkipArchive)' -ForegroundColor Yellow
}

if (Test-Path -LiteralPath (Join-Path $stateDir 'worktree-uncommitted.patch')) {
    $patchSize = (Get-Item -LiteralPath (Join-Path $stateDir 'worktree-uncommitted.patch')).Length
    if ($patchSize -gt 0) {
        Write-Host "  the source had uncommitted tracked changes; the diff is at $stateDir\worktree-uncommitted.patch (not applied automatically)" -ForegroundColor Yellow
    }
}
$evidence = Join-Path $stateDir 'showcase-evidence'
if ((Test-Path -LiteralPath $evidence) -and -not ($restoredArchive -contains 'showcase/evidence')) {
    $destEvidence = Join-Path $Dest 'showcase\evidence'
    New-Item -ItemType Directory -Force -Path $destEvidence | Out-Null
    robocopy $evidence $destEvidence /E /NFL /NDL /NJH /NJS /NP /R:2 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { Fail "robocopy failed restoring showcase/evidence (exit $LASTEXITCODE)" 2 }
    Write-Host '  showcase/evidence'
}
if (Test-Path -LiteralPath (Join-Path $stateDir 'agent-manager.json.reference')) {
    Write-Host '  agent-manager.json kept as a reference; not installed (host-specific paths)' -ForegroundColor Yellow
}

# ---------------------------------------------------------------- skills
Step 'Skills mirror and gate'

$sync = Join-Path $Dest 'scripts\sync-skills.ps1'
$verify = Join-Path $Dest 'scripts\verify-skills.ps1'
if ((Test-Path -LiteralPath $sync) -and (Test-Path -LiteralPath $verify)) {
    $r = Invoke-Script -Path $sync -WorkingDirectory $Dest
    if ($r.Code -ne 0) { Show-Tail $r.Lines; Fail "scripts/sync-skills.ps1 failed (exit $($r.Code))" 1 }
    $r = Invoke-Script -Path $verify -WorkingDirectory $Dest
    if ($r.Code -ne 0) { Show-Tail $r.Lines 20; Fail "scripts/verify-skills.ps1 failed (exit $($r.Code))" 1 }
    $summary = @($r.Lines | Where-Object { $_ -match 'skills checked|byte-identical' })
    foreach ($s in $summary) { Write-Host "  $s" }
} else {
    Write-Host '  skill scripts missing at HEAD; skipped' -ForegroundColor Yellow
}

# ---------------------------------------------------------------- targets
$targetResults = @()
if (-not $SkipTargets) {
    Step 'Target copies from pristine + patch'
    $newTarget = Join-Path $Dest 'scripts\new-target.ps1'
    $applyPatch = Join-Path $Dest 'scripts\apply-target-patches.ps1'
    if (-not (Test-Path -LiteralPath $newTarget) -or -not (Test-Path -LiteralPath $applyPatch)) {
        Write-Host '  target scripts missing at HEAD; skipped' -ForegroundColor Yellow
    } else {
        $targetsRoot = Join-Path $Dest 'targets'
        $targetNames = @()
        if (Test-Path -LiteralPath $targetsRoot) {
            $targetNames = @(Get-ChildItem -LiteralPath $targetsRoot -Directory |
                Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeLists.txt') } |
                Select-Object -ExpandProperty Name)
        }
        foreach ($name in $targetNames) {
            $r = Invoke-Script -Path $newTarget -WorkingDirectory $Dest -ScriptArgs @('-Name', $name)
            if ($r.Code -ne 0) { Show-Tail $r.Lines; Fail "new-target.ps1 -Name $name failed (exit $($r.Code))" 1 }
            $r = Invoke-Script -Path $applyPatch -WorkingDirectory $Dest -ScriptArgs @('-Name', $name)
            if ($r.Code -ne 0) { Show-Tail $r.Lines 20; Fail "apply-target-patches.ps1 -Name $name failed (exit $($r.Code)): the copy does not equal the patched tree" 1 }
            $last = @($r.Lines | Where-Object { $_ -match 'verified|Nothing to do' } | Select-Object -Last 1)
            Write-Host ("  {0,-18} copy + patch verified {1}" -f $name, $(if ($last) { "($($last[0]))" } else { '' }))
            $targetResults += "$name OK"
        }
        if ($targetNames.Count -eq 0) { Write-Host '  no target directories with CMakeLists.txt' -ForegroundColor Yellow }

        $controls = Join-Path $Dest 'scripts\test-target-patches.ps1'
        if (Test-Path -LiteralPath $controls) {
            $r = Invoke-Script -Path $controls -WorkingDirectory $Dest
            if ($r.Code -ne 0) { Show-Tail $r.Lines 20; Fail "test-target-patches.ps1 failed (exit $($r.Code))" 1 }
            $passes = @($r.Lines | Where-Object { $_ -match '^PASS' }).Count
            Write-Host "  patch-apply controls: $passes PASS"
        }
    }
} else {
    Write-Host ''
    Write-Host 'Targets skipped (-SkipTargets); build or diff against pristine will need:' -ForegroundColor Yellow
    Write-Host '  scripts/new-target.ps1 -Name <target>; scripts/apply-target-patches.ps1 -Name <target>'
}

# ---------------------------------------------------------------- optional build
$buildResult = 'not requested'
if ($Build) {
    Step 'Configure and build'
    if (-not $cmakeCmd) { Fail 'cmake not found (required by -Build)' 2 }
    $logDir = Join-Path $Dest 'build'
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    # Redirect inside cmd (not with Start-Process -RedirectStandardOutput): MSBuild node-reuse
    # workers inherit the pipe and keep it open after the build exits, which hangs the caller.
    # -nr:false stops the workers from lingering at all.
    $configureLog = Join-Path $logDir '_configure.log'
    $configureLine = '"' + $cmakeCmd + '" --preset win11-amd64 > "' + $configureLog + '" 2>&1'
    Push-Location -LiteralPath $Dest
    try { & cmd /c $configureLine; $code = $LASTEXITCODE } finally { Pop-Location }
    if ($code -ne 0) {
        Show-Tail @(Get-Content -LiteralPath $configureLog -ErrorAction SilentlyContinue) 20
        Fail "cmake configure failed (exit $code); full log: $configureLog" 1
    }
    Write-Host '  configure OK'

    $buildLog = Join-Path $logDir '_build.log'
    $buildLine = '"' + $cmakeCmd + '" --build --preset win11-amd64-release -- -m -nr:false > "' + $buildLog + '" 2>&1'
    Push-Location -LiteralPath $Dest
    try { & cmd /c $buildLine; $code = $LASTEXITCODE } finally { Pop-Location }
    if ($code -ne 0) {
        Show-Tail @(Get-Content -LiteralPath $buildLog -ErrorAction SilentlyContinue) 40
        Fail "build failed (exit $code); full log: $buildLog" 1
    }
    $exe = Join-Path $Dest 'build\bin\Release\DOOM3.exe'
    $xp = Join-Path $Dest 'build\bin\Release\DOOM3XP.exe'
    $missing = @()
    foreach ($f in @($exe, $xp)) { if (-not (Test-Path -LiteralPath $f)) { $missing += $f } }
    if ($missing.Count -gt 0) { Fail ("build exited 0 but artifacts are missing: " + ($missing -join ', ')) 1 }
    Write-Host ("  DOOM3.exe   {0:N0} KB" -f ((Get-Item -LiteralPath $exe).Length / 1KB))
    Write-Host ("  DOOM3XP.exe {0:N0} KB" -f ((Get-Item -LiteralPath $xp).Length / 1KB))
    Write-Host "  full log: $buildLog"
    $buildResult = 'OK'
}

# ---------------------------------------------------------------- final proof
Step 'Final state'

$status = @(& git -C $Dest status --porcelain | Where-Object { $_ -and $_.Trim() })
$unexpected = @($status | Where-Object {
    $line = [string]$_
    $path = if ($line -match '^\?\?\s+(.+)$') { ($Matches[1].Trim('"') -replace '\\', '/') } else { $null }
    -not ($path -and ($restoredUntracked -contains $path))
})
if ($unexpected.Count -gt 0) {
    Show-Tail $unexpected 20
    Fail "git status shows $($unexpected.Count) unexpected path(s) in the restored checkout; a deploy artifact became tracked" 1
}
$headNow = (& git -C $Dest rev-parse HEAD).Trim()
$branchNow = (& git -C $Dest rev-parse --abbrev-ref HEAD).Trim()
$statusText = if ($status.Count -eq 0) { 'clean' } else { "clean except $($status.Count) restored untracked file(s) from the source" }
Write-Host ("  git status  : {0}" -f $statusText)
$headLabel = if ($branchNow -eq 'HEAD') { "detached $($headNow.Substring(0, 12))" } else { $branchNow }
Write-Host ("  HEAD        : {0} @ {1}" -f $headLabel, $headNow.Substring(0, 12))
if ($facts) {
    $sourceLabel = if ($facts.repository.branch) { $facts.repository.branch } else { '(detached)' }
    Write-Host ("  source HEAD : {0} @ {1}" -f $sourceLabel, $facts.repository.head.Substring(0, 12))
    if ($facts.repository.head -ne $headNow) {
        Write-Host '  [NOTE] HEAD differs from the snapshot HEAD; the bundle was packed at another commit' -ForegroundColor Yellow
    }
}

$report = @()
$report += "DOOM3_AINative deploy report"
$report += "generated : $((Get-Date).ToString('o'))"
$report += "bundle    : $Bundle"
$report += "dest      : $Dest"
$report += "branch    : $headLabel @ $headNow"
$report += "submodule : $(if (Test-Path -LiteralPath (Join-Path $Dest 'doom3_orig\doom3\.git')) { (& git -C (Join-Path $Dest 'doom3_orig\doom3') rev-parse HEAD).Trim() } else { 'absent' })"
$report += "targets   : $(if ($targetResults.Count -gt 0) { $targetResults -join '; ' } else { 'skipped' })"
$report += "build     : $buildResult"
$report += "archive   : $(if ($restoredArchive.Count -gt 0) { $restoredArchive -join '; ' } else { 'not restored' })"
$report += "git status: $statusText"
Write-TextFile (Join-Path $Dest 'out\deploy-report.txt') (($report -join "`r`n") + "`r`n")

Write-Host ''
Write-Host 'Deploy complete' -ForegroundColor Green
Write-Host ''
Write-Host 'Next steps that the bundle cannot provide:' -ForegroundColor Cyan
Write-Host '  game data   : copy retail/demo pk4 files, then scripts/prepare-demo-data.ps1 -Pk4 <demo00.pk4> -RunDir <data-root>\run'
Write-Host '  web leg     : activate emsdk 6.0.13 (tests/webgpu/emscripten/README.md), then tests/webgpu/emscripten/build.ps1'
Write-Host '  shader DXC  : shaders/tools/get-toolchain.ps1 (downloads the pinned toolchain cache)'
Write-Host '  cargo/uv    : content pipeline uses py -3 -m pip install -r content/tools/requirements-content.txt'
Write-Host '  agent state : .kilo/agent-manager.json is host-specific; Agent Manager recreates it on first launch'
Write-Host ''
exit 0
