#Requires -Version 5.1
<#
.SYNOPSIS
  Snapshot this checkout into a restorable, verifiable bundle set.

.DESCRIPTION
  A plain `git clone` carries only tracked commits. This script also captures what a clone cannot
  reproduce: local-only commits and branches, dangling commits, the pinned submodule's objects, the
  engine target copies, build artifacts and logs, tool caches, test output, and the evidence images
  - plus the host facts the target machine must match.

  Output layout (relative to -OutDir):

    repo-all.bundle            all local branches + tags + HEAD, verified
    doom3-submodule.bundle     the pristine submodule's history (offline restore)
    dangling-commits.bundle    unreachable commits (rebases, removed worktrees), bundle-verified
    archive/                   ignored data preserved with its repository-relative path
      showcase/evidence/       captured posters and logs
      out/                     test/run evidence
      shaders/tools/.cache/    pinned DXC toolchain (offline rebuilds)
      content/tools/.cache/    pinned texture encoder
      .specify/integrations/.cache/
      .kilo/skills/            generated skill mirror
      targets/<name>/doom3/    engine working copies (also reconstructable from patch)
      build*/bin/              built executables (DOOM3.exe, DOOM3XP.exe, pdbs)
      build-logs/              every *.log from the repo root and build dirs
    local-state/               spec pointer, Agent Manager reference, untracked files, diffs
    deploy-repo.ps1            bootstrap copy, so the bundle restores without a checkout
    MIGRATION-NOTES.md         what is archived, what is not, and how to recreate it
    IGNORED-INVENTORY.txt      every ignored top-level path with size and archive status
    machine-facts.json         toolchain, host paths, refs, submodule SHA at snapshot time
    local-only-refs.txt        branches/commits that exist only on this machine
    checksums.sha256           SHA-256 of every file in the set

  -Profile snapshot (default) archives the key ignored data listed above. -Profile portable keeps
  only the git bundles and stateful files, for a quick move when the source machine remains intact.

  Never carried: `.kilo/worktrees` (branches are in the bundle; use -IncludeWorktrees to archive
  them too), build intermediates (CMakeFiles/*.obj - rebuildable in ~25 s), game data
  (`*.pk4`, not redistributable). IGNORED-INVENTORY.txt names the size and reason for each.

.PARAMETER OutDir
  Destination directory. Defaults to a timestamped sibling of the repository.

.PARAMETER Profile
  snapshot (default) = full key-data backup; portable = git bundles + stateful files only.

.PARAMETER IncludeWorktrees
  Also archive `.kilo/worktrees` (multi-GB). Restore does not re-link them; the directory is
  preserved as an archive only, because linked-worktree metadata contains absolute paths.

.PARAMETER IncludeEvidence
  portable profile only: also archive showcase/evidence. Ignored in snapshot (always included).

.PARAMETER AllowDirty
  Continue when the worktree or the pristine submodule has uncommitted changes. The changes are
  saved under local-state/ (patch plus untracked files) instead of being silently lost.

.PARAMETER NoDanglingBundle
  Only list unreachable commits, do not bundle them.

.PARAMETER Force
  Write into an existing, non-empty -OutDir.

.PARAMETER EmsdkPath
  emsdk root to record when it is neither in $env:EMSDK nor %USERPROFILE%\emsdk (e.g. C:\emsdk).

.EXAMPLE
  .\scripts\migrate-repo.ps1

.EXAMPLE
  .\scripts\migrate-repo.ps1 -OutDir D:\backup\doom3-snapshot -EmsdkPath C:\emsdk

.NOTES
  Exit 0 = snapshot written and verified; 1 = preflight rejected (dirty tree or submodule);
  2 = environment error (not a repo, git missing, non-empty -OutDir without -Force, bundle/copy
  failed).
#>
[CmdletBinding()]
param(
    [string]$OutDir,
    [ValidateSet('snapshot', 'portable')][string]$Profile = 'snapshot',
    [switch]$IncludeWorktrees,
    [switch]$IncludeEvidence,
    [switch]$AllowDirty,
    [switch]$NoDanglingBundle,
    [switch]$Force,
    [string]$EmsdkPath
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

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

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @()
    )
    # PowerShell 5.1 promotes every line a native tool writes to stderr into a NativeCommandError.
    # Under the script-wide EAP=Stop that aborts a snapshot on a benign warning (verified: git
    # count-objects -v exits 0 but warns "garbage found" about a stale .git/worktrees entry).
    # Capture stderr as data and let the caller decide from the exit code.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $FilePath @Arguments 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    return [pscustomobject]@{ Lines = $lines; Code = $code; Text = ($lines -join "`n") }
}

function Get-GitOutput {
    param([Parameter(Mandatory = $true)][string[]]$GitArgs)
    return Invoke-Native -FilePath 'git' -Arguments $GitArgs
}

function Get-ToolVersion {
    param([Parameter(Mandatory = $true)][string]$Command)
    $c = Get-Command $Command -ErrorAction SilentlyContinue
    if ($null -eq $c) { return $null }
    $v = @((Invoke-Native -FilePath $Command -Arguments @('--version')).Lines | Select-Object -First 1)
    if ($v.Count -eq 0) { return $null }
    return ([string]$v[0]).Trim()
}

function Get-TreeSize {
    param([Parameter(Mandatory = $true)][string]$Path)
    $items = @(Get-ChildItem -LiteralPath $Path -Recurse -Force -File -ErrorAction SilentlyContinue)
    return [pscustomobject]@{
        Files = $items.Count
        Bytes = [long](($items | Measure-Object -Property Length -Sum).Sum)
    }
}

function Get-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
    $path = @((Invoke-Native -FilePath $vswhere -Arguments @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath')).Lines | Where-Object { $_ })
    if ($path.Count -eq 0) { return $null }
    $display = @((Invoke-Native -FilePath $vswhere -Arguments @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'displayName')).Lines | Where-Object { $_ })
    $toolsets = @()
    $msvc = Join-Path ([string]$path[0]) 'VC\Tools\MSVC'
    if (Test-Path -LiteralPath $msvc) { $toolsets = @(Get-ChildItem -LiteralPath $msvc -Directory | Select-Object -ExpandProperty Name) }
    $cmake = Join-Path ([string]$path[0]) 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    return [ordered]@{
        path     = [string]$path[0]
        display  = if ($display.Count -gt 0) { [string]$display[0] } else { $null }
        toolsets = $toolsets
        cmake    = if (Test-Path -LiteralPath $cmake) { $cmake } else { $null }
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

function Get-EmscriptenVersion {
    param([string]$Emsdk)
    if (-not $Emsdk) { return $null }
    $f = Join-Path $Emsdk 'upstream\emscripten\emscripten-version.txt'
    if (-not (Test-Path -LiteralPath $f)) { return $null }
    return ((Get-Content -LiteralPath $f -TotalCount 1) -replace '"', '').Trim()
}

function Get-Chrome {
    $reg = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe'
    if (-not (Test-Path -LiteralPath $reg)) { return $null }
    $exe = (Get-ItemProperty -LiteralPath $reg -ErrorAction SilentlyContinue).'(default)'
    if (-not $exe -or -not (Test-Path -LiteralPath $exe)) { return $null }
    return [ordered]@{ path = $exe; version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion }
}

# ---------------------------------------------------------------- preflight
Step 'Preflight'

if (-not (Test-Path -LiteralPath (Join-Path $repo '.git'))) { Fail "Not a git checkout: $repo" 2 }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail 'git is not on PATH.' 2 }
if (-not $OutDir) {
    $OutDir = Join-Path (Split-Path -Parent $repo) ('doom3-snapshot-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -LiteralPath $OutDir -Force).Count -gt 0 -and -not $Force) {
    Fail "Output directory is not empty: $OutDir (use -Force to overwrite files in it)" 2
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stateDir = Join-Path $OutDir 'local-state'
New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
$archiveDir = Join-Path $OutDir 'archive'

$status = Get-GitOutput @('-C', $repo, 'status', '--porcelain')
$dirtyEntries = @($status.Lines | Where-Object { $_ -and $_.Trim() })
if ($dirtyEntries.Count -gt 0 -and -not $AllowDirty) {
    Write-Host ($dirtyEntries | Select-Object -First 10 | ForEach-Object { "  $_" }) -ForegroundColor Yellow
    Fail "Worktree has $($dirtyEntries.Count) uncommitted path(s). Commit them, or re-run with -AllowDirty to save the diff and untracked files into local-state/." 1
}

$sub = Join-Path $repo 'doom3_orig\doom3'
$subExists = Test-Path -LiteralPath (Join-Path $sub '.git')
$subDirty = @()
if ($subExists) {
    $subStatus = Get-GitOutput @('-C', $sub, 'status', '--porcelain')
    $subDirty = @($subStatus.Lines | Where-Object { $_ -and $_.Trim() })
    if ($subDirty.Count -gt 0 -and -not $AllowDirty) {
        Write-Host ($subDirty | Select-Object -First 10 | ForEach-Object { "  $_" }) -ForegroundColor Yellow
        Fail "Pristine submodule is dirty ($($subDirty.Count) path(s)). Restore it, or re-run with -AllowDirty to save its diff." 1
    }
}

$headSha = (& git -C $repo rev-parse HEAD).Trim()
# `rev-parse --abbrev-ref HEAD` returns the literal "HEAD" when detached; resolve the ref instead
# so a detached checkout is recorded as branch=null plus its commit SHA (B-1 regression).
$headBranchRef = Invoke-Native -FilePath 'git' -Arguments @('-C', $repo, 'symbolic-ref', '--quiet', '--short', 'HEAD')
$headBranch = if ($headBranchRef.Code -eq 0) { $headBranchRef.Text.Trim() } else { $null }
if (-not $headBranch) { $headBranch = $null }
$branchLabel = if ($headBranch) { $headBranch } else { '(detached)' }
$originRef = Invoke-Native -FilePath 'git' -Arguments @('-C', $repo, 'remote', 'get-url', 'origin')
$originUrl = if ($originRef.Code -eq 0) { $originRef.Text.Trim() } else { $null }
$subSha = if ($subExists) { (& git -C $sub rev-parse HEAD).Trim() } else { $null }
$subOrigin = $null
if ($subExists) {
    $subOriginRef = Invoke-Native -FilePath 'git' -Arguments @('-C', $sub, 'remote', 'get-url', 'origin')
    if ($subOriginRef.Code -eq 0) { $subOrigin = $subOriginRef.Text.Trim() }
}

Write-Host "repo         : $repo"
Write-Host "profile      : $Profile"
Write-Host "HEAD         : $branchLabel @ $($headSha.Substring(0, 12))"
Write-Host "submodule    : $(if ($subSha) { $subSha.Substring(0, 12) } else { 'NOT INITIALISED' })"
Write-Host "output       : $OutDir"

# ---------------------------------------------------------------- bundles
Step 'Bundling repository refs'

$repoBundle = Join-Path $OutDir 'repo-all.bundle'
& git -C $repo bundle create $repoBundle --branches --tags HEAD
if ($LASTEXITCODE -ne 0) { Fail "git bundle create failed (exit $LASTEXITCODE)" 2 }
& git -C $repo bundle verify $repoBundle | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "git bundle verify failed for $repoBundle" 2 }

$subBundle = $null
if ($subExists) {
    Step 'Bundling submodule objects'
    $subBundle = Join-Path $OutDir 'doom3-submodule.bundle'
    & git -C $sub bundle create $subBundle --branches --tags HEAD
    if ($LASTEXITCODE -ne 0) { Fail "submodule bundle failed (exit $LASTEXITCODE)" 2 }
    & git -C $sub bundle verify $subBundle | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "submodule bundle verify failed for $subBundle" 2 }
} else {
    Write-Host 'submodule not initialised; run: git submodule update --init --recursive' -ForegroundColor Yellow
}

# ---------------------------------------------------------------- local-only refs
Step 'Recording local-only refs'

$refLines = @()
$refLines += "Repository: $originUrl"
$refLines += "HEAD: $branchLabel $headSha"
$refLines += ''
$branchRows = @()
foreach ($line in (& git -C $repo for-each-ref '--format=%(refname:short)|%(upstream:short)|%(objectname)' refs/heads)) {
    $parts = $line -split '\|'
    $name = $parts[0]
    $upstream = $parts[1]
    $sha = $parts[2]
    $ahead = 0
    $behind = 0
    if ($upstream) {
        # A...B: left = A-only (upstream-only = behind), right = B-only (local-only = ahead).
        $counts = (& git -C $repo rev-list --left-right --count "$upstream...$sha") -split "`t"
        $behind = [int]$counts[0]
        $ahead = [int]$counts[1]
    }
    # The positive revision must come first: in git 2.25 `--not --remotes <sha>` counts zero.
    $unique = @(& git -C $repo log --oneline -20 $sha --not --remotes)
    $branchRows += [ordered]@{ name = $name; sha = $sha; upstream = $upstream; ahead = $ahead; behind = $behind; localOnlyCommits = $unique.Count }
    $refLines += ("{0}  {1}  upstream={2} ahead={3} behind={4} local-only-commits={5}" -f $name, $sha.Substring(0, 12), $(if ($upstream) { $upstream } else { '(none)' }), $ahead, $behind, $unique.Count)
    foreach ($u in $unique) { $refLines += "    $u" }
}
Write-TextFile (Join-Path $OutDir 'local-only-refs.txt') (($refLines -join "`r`n") + "`r`n")
$localOnly = @($branchRows | Where-Object { $_.localOnlyCommits -gt 0 })
if ($localOnly.Count -gt 0) {
    Write-Host ("branches with commits not on any remote: " + (($localOnly | ForEach-Object { $_.name }) -join ', ')) -ForegroundColor Yellow
} else {
    Write-Host 'every local commit is also reachable from a remote-tracking ref'
}

# ---------------------------------------------------------------- dangling commits
Step 'Scanning for unreachable commits'
$fsck = Get-GitOutput @('-C', $repo, 'fsck', '--unreachable', '--no-reflogs')
$dangling = @($fsck.Lines | Select-String 'unreachable commit' | ForEach-Object { ($_ -split ' ')[2] })
$danglingLines = @("Unreachable commits in $repo at $((Get-Date).ToString('o'))", "Count: $($dangling.Count)", '')
foreach ($d in $dangling) {
    $subjectRef = Invoke-Native -FilePath 'git' -Arguments @('-C', $repo, 'log', '-1', '--format=%h %ad %s', '--date=short', $d)
    if ($subjectRef.Code -eq 0 -and $subjectRef.Text.Trim()) { $danglingLines += "$d  $($subjectRef.Text.Trim())" }
}
Write-TextFile (Join-Path $OutDir 'unreachable-commits.txt') (($danglingLines -join "`r`n") + "`r`n")
Write-Host "unreachable commits: $($dangling.Count)"
$danglingBundled = $false
if (-not $NoDanglingBundle -and $dangling.Count -gt 0) {
    # git 2.25 refuses to bundle commits that no ref reaches ("Refusing to create empty bundle"),
    # so pin each one to a temporary ref, bundle the refs, then delete them again.
    $danglingBundle = Join-Path $OutDir 'dangling-commits.bundle'
    $tempRefs = @()
    try {
        $n = 0
        foreach ($d in $dangling) {
            $ref = "refs/migration/dangling/$n"
            & git -C $repo update-ref $ref $d
            if ($LASTEXITCODE -eq 0) { $tempRefs += $ref }
            $n++
        }
        if ($tempRefs.Count -gt 0) {
            & git -C $repo bundle create $danglingBundle $tempRefs
            if ($LASTEXITCODE -eq 0) {
                & git -C $repo bundle verify $danglingBundle | Out-Null
                if ($LASTEXITCODE -eq 0) { $danglingBundled = $true; Write-Host "  bundled $($tempRefs.Count) commit(s) (git bundle verify OK)" }
                else { Write-Host '  bundled, but verify failed; the list remains authoritative' -ForegroundColor Yellow }
            } else {
                Write-Host '  bundle failed; the list remains authoritative' -ForegroundColor Yellow
            }
        }
    } finally {
        foreach ($r in $tempRefs) { Invoke-Native -FilePath 'git' -Arguments @('-C', $repo, 'update-ref', '-d', $r) | Out-Null }
    }
}

# ---------------------------------------------------------------- local state
Step 'Copying stateful local files'

$featureJson = Join-Path $repo '.specify\feature.json'
if (Test-Path -LiteralPath $featureJson) {
    Copy-Item -LiteralPath $featureJson -Destination (Join-Path $stateDir 'specify-feature.json') -Force
    Write-Host '  .specify/feature.json'
}
$agentState = Join-Path $repo '.kilo\agent-manager.json'
if (Test-Path -LiteralPath $agentState) {
    Copy-Item -LiteralPath $agentState -Destination (Join-Path $stateDir 'agent-manager.json.reference') -Force
    Write-Host '  .kilo/agent-manager.json (reference only, never restored automatically)'
}
$worktreeList = (& git -C $repo worktree list --porcelain)
Write-TextFile (Join-Path $stateDir 'worktree-list.txt') (($worktreeList -join "`r`n") + "`r`n")
Write-Host '  worktree list'

if ($dirtyEntries.Count -gt 0) {
    # --output keeps the patch byte-exact; PowerShell `>` would rewrite it as UTF-16.
    $dirtyPatch = Join-Path $stateDir 'worktree-uncommitted.patch'
    & git -C $repo diff --binary HEAD --output=$dirtyPatch
    $untracked = @(& git -C $repo ls-files --others --exclude-standard)
    Write-TextFile (Join-Path $stateDir 'worktree-untracked.txt') ((($untracked -join "`r`n") + "`r`n"))
    if ($untracked.Count -gt 0) {
        $untrackedRoot = Join-Path $stateDir 'untracked'
        foreach ($rel in $untracked) {
            $src = Join-Path $repo ($rel -replace '/', '\')
            $dst = Join-Path $untrackedRoot ($rel -replace '/', '\')
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
            Copy-Item -LiteralPath $src -Destination $dst -Force -ErrorAction SilentlyContinue
        }
        Write-Host "  $($untracked.Count) untracked file(s) under local-state/untracked/" -ForegroundColor Yellow
    }
    Write-Host '  uncommitted worktree diff (-AllowDirty)' -ForegroundColor Yellow
}
if ($subDirty.Count -gt 0) {
    $subPatch = Join-Path $stateDir 'submodule-uncommitted.patch'
    & git -C $sub diff --binary HEAD --output=$subPatch
    Write-Host '  uncommitted submodule diff (-AllowDirty)' -ForegroundColor Yellow
}

# ---------------------------------------------------------------- archive
$archived = @()
$archiveFailures = @()

function Copy-ArchiveTree {
    param([Parameter(Mandatory = $true)][string]$Relative, [Parameter(Mandatory = $true)][string]$Why)
    $src = Join-Path $repo ($Relative -replace '/', '\')
    if (-not (Test-Path -LiteralPath $src)) { return }
    $dst = Join-Path $archiveDir ($Relative -replace '/', '\')
    if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    robocopy $src $dst /E /NFL /NDL /NJH /NJS /NP /R:2 /W:1 | Out-Null
    $code = $LASTEXITCODE
    if ($code -ge 8) { $script:archiveFailures += "$Relative (robocopy $code)"; return }
    $size = Get-TreeSize -Path $src
    $script:archived += [pscustomobject]@{ Path = $Relative; Files = $size.Files; Bytes = $size.Bytes; Why = $Why }
    Write-Host ("  {0,-38} {1,7:N1} MB {2,6} files" -f $Relative, ($size.Bytes / 1MB), $size.Files)
}

if ($Profile -eq 'snapshot') {
    Step 'Archiving key ignored data'
    # Keep the layout stable even when a fresh worktree has no ignored data yet.
    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null

    Copy-ArchiveTree 'showcase/evidence' 'generated evidence: only reproducible with game data'
    Copy-ArchiveTree 'out' 'test/run evidence and reports'
    Copy-ArchiveTree 'shaders/tools/.cache' 'pinned DXC toolchain; avoids a re-download'
    Copy-ArchiveTree 'content/tools/.cache' 'pinned texture encoder'
    Copy-ArchiveTree '.specify/integrations/.cache' 'spec-kit catalogue cache'
    Copy-ArchiveTree '.kilo/skills' 'generated skill mirror'
    if (@(Get-ChildItem -LiteralPath (Join-Path $repo '.playwright-mcp') -Recurse -File -Force -ErrorAction SilentlyContinue).Count -gt 0) {
        Copy-ArchiveTree '.playwright-mcp' 'browser traces'
    }

    if (Test-Path -LiteralPath (Join-Path $repo 'targets')) {
        foreach ($t in (Get-ChildItem -LiteralPath (Join-Path $repo 'targets') -Directory)) {
            $copy = Join-Path $t.FullName 'doom3'
            if (Test-Path -LiteralPath $copy) {
                Copy-ArchiveTree ("targets/{0}/doom3" -f $t.Name) 'engine working copy (also reconstructable from its patch)'
            }
        }
    }

    foreach ($b in (Get-ChildItem -LiteralPath $repo -Directory -Filter 'build*')) {
        $bin = Join-Path $b.FullName 'bin'
        if (Test-Path -LiteralPath $bin) { Copy-ArchiveTree ("{0}/bin" -f $b.Name) 'built binaries (snapshot proof; rebuild replaces them)' }
    }

    $logsRoot = Join-Path $archiveDir 'build-logs'
    $logCount = 0
    foreach ($f in (Get-ChildItem -LiteralPath $repo -File -Filter '*.log')) {
        $dst = Join-Path $logsRoot ('_root\' + $f.Name)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
        $logCount++
    }
    foreach ($b in (Get-ChildItem -LiteralPath $repo -Directory -Filter 'build*')) {
        foreach ($f in (Get-ChildItem -LiteralPath $b.FullName -File -Filter '*.log' -ErrorAction SilentlyContinue)) {
            $dst = Join-Path $logsRoot ($b.Name + '\' + $f.Name)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
            Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
            $logCount++
        }
    }
    if ($logCount -gt 0) {
        $logBytes = (Get-ChildItem -LiteralPath $logsRoot -Recurse -File | Measure-Object -Property Length -Sum).Sum
        $archived += [pscustomobject]@{ Path = 'build-logs'; Files = $logCount; Bytes = [long]$logBytes; Why = 'build/test logs' }
        Write-Host ("  {0,-38} {1,7:N1} MB {2,6} files" -f 'build-logs', ($logBytes / 1MB), $logCount)
    }

    if ($IncludeWorktrees -and (Test-Path -LiteralPath (Join-Path $repo '.kilo\worktrees'))) {
        Copy-ArchiveTree '.kilo/worktrees' 'linked worktrees (archive only; absolute gitdir paths are not relinked)'
    }
} else {
    Step 'Portable profile: ignored data'
    if ($IncludeEvidence) { Copy-ArchiveTree 'showcase/evidence' 'requested with -IncludeEvidence' }
    Write-Host '  build output, caches, target copies and worktrees are not archived in portable mode'
}

if ($archiveFailures.Count -gt 0) { Fail ("Archive failed for: " + ($archiveFailures -join '; ')) 2 }

# ---------------------------------------------------------------- ignored inventory
Step 'Inventory of ignored paths'

$inventory = @()
$ignoredLines = @(& git -C $repo status --ignored --porcelain)
foreach ($line in $ignoredLines) {
    if ($line -notmatch '^!!\s+(.+)$') { continue }
    $rel = ($Matches[1].Trim('"') -replace '\\', '/').TrimEnd('/')
    $full = Join-Path $repo ($rel -replace '/', '\')
    $size = if (Test-Path -LiteralPath $full) { Get-TreeSize -Path $full } else { [pscustomobject]@{ Files = 0; Bytes = 0 } }
    $matched = @($archived | Where-Object { $_.Path -eq $rel -or $_.Path -like ($rel + '/*') })
    $archBytes = [long](($matched | Measure-Object -Property Bytes -Sum).Sum)
    $status = if ($archBytes -eq 0) { 'excluded' } elseif ($archBytes -ge $size.Bytes) { 'ARCHIVED' } else { 'PARTIAL' }
    $inventory += [pscustomobject]@{ Path = $rel; Files = $size.Files; Bytes = $size.Bytes; ArchivedBytes = $archBytes; Status = $status }
}
$invLines = @("Ignored paths in $repo at $((Get-Date).ToString('o'))", ('Profile: {0}' -f $Profile), '')
foreach ($i in ($inventory | Sort-Object Bytes -Descending)) {
    $invLines += ("{0,-10} {1,8:N1} MB {2,6} files  {3}" -f ('[' + $i.Status + ']'), ($i.Bytes / 1MB), $i.Files, $i.Path)
}
$invLines += ''
$invLines += 'PARTIAL means the directory itself is ignored but only named subpaths were archived'
$invLines += '(for example build/ keeps build/bin but not CMakeFiles or object files).'
$invLines += ''
$invLines += 'Excluded reasons:'
$invLines += '  build*/ (intermediates)  derived CMakeFiles/obj; rebuild with cmake --build --preset win11-amd64-release'
$invLines += '  .kilo/worktrees/         linked checkouts whose branches are in repo-all.bundle; use -IncludeWorktrees to archive'
$invLines += '  targets/*/build, targets/*/out  per-target scratch build directories'
Write-TextFile (Join-Path $OutDir 'IGNORED-INVENTORY.txt') (($invLines -join "`r`n") + "`r`n")
$archivedBytes = [long](($inventory | Measure-Object -Property ArchivedBytes -Sum).Sum)
$excludedBytes = [long](($inventory | Measure-Object -Property Bytes -Sum).Sum) - $archivedBytes
Write-Host ("  ignored on disk: {0:N1} MB; archived: {1:N1} MB; excluded: {2:N1} MB (see IGNORED-INVENTORY.txt)" -f (($archivedBytes + $excludedBytes) / 1MB), ($archivedBytes / 1MB), ($excludedBytes / 1MB))

# ---------------------------------------------------------------- machine facts
Step 'Recording machine facts'

$vs = Get-VisualStudio
$emsdk = Resolve-Emsdk -Explicit $EmsdkPath
$gameData = @(& git -C $repo ls-files '*.pk4' '*.pk3' '*.pak')
$gitDir = Join-Path $repo '.git'
$gitBytes = (Get-ChildItem -LiteralPath $gitDir -Recurse -Force -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum

$facts = [ordered]@{
    schema             = 'doom3-migration/2'
    createdAt          = (Get-Date).ToString('o')
    profile            = $Profile
    host               = [ordered]@{
        os             = [Environment]::OSVersion.VersionString
        computer       = $env:COMPUTERNAME
        powershell     = $PSVersionTable.PSVersion.ToString()
        processorCount = [Environment]::ProcessorCount
    }
    repository         = [ordered]@{
        path           = $repo
        remote         = $(if ($originUrl) { $originUrl } else { $null })
        head           = $headSha
        branch         = $headBranch
        detached       = (-not $headBranch)
        trackedFiles   = [int]((& git -C $repo ls-files | Measure-Object -Line).Lines)
        looseObjects   = (Get-GitOutput @('-C', $repo, 'count-objects', '-v')).Lines
        gitDirBytes    = $gitBytes
        gitConfig      = [ordered]@{
            autocrlf   = (& git -C $repo config --get core.autocrlf)
            longpaths  = (& git -C $repo config --get core.longpaths)
            ignorecase = (& git -C $repo config --get core.ignorecase)
            filemode   = (& git -C $repo config --get core.filemode)
        }
    }
    submodule          = [ordered]@{
        path           = 'doom3_orig/doom3'
        initialised    = $subExists
        sha            = $subSha
        remote         = $subOrigin
        clean          = ($subDirty.Count -eq 0)
    }
    toolchain          = [ordered]@{
        git            = Get-ToolVersion 'git'
        node           = Get-ToolVersion 'node'
        npm            = Get-ToolVersion 'npm'
        uv             = Get-ToolVersion 'uv'
        py             = Get-ToolVersion 'py'
        gh             = Get-ToolVersion 'gh'
        visualStudio   = $vs
        windowsSdk     = @(Get-WindowsSdks)
        emsdkPath      = $emsdk
        emscripten     = Get-EmscriptenVersion -Emsdk $emsdk
        chrome         = Get-Chrome
    }
    branches           = @($branchRows)
    archived           = @($archived)
    ignoredExcludedMB  = [math]::Round($excludedBytes / 1MB, 1)
    danglingCommits    = $dangling.Count
    danglingBundled    = $danglingBundled
    ignoredState       = [ordered]@{
        worktrees        = [int](@(Get-ChildItem -LiteralPath (Join-Path $repo '.kilo\worktrees') -Directory -ErrorAction SilentlyContinue).Count)
        worktreesArchived = [bool]$IncludeWorktrees
        gameDataTracked  = ($gameData.Count -gt 0)
    }
}
$factsJson = $facts | ConvertTo-Json -Depth 8
Write-TextFile (Join-Path $OutDir 'machine-facts.json') ($factsJson + "`r`n")

# ---------------------------------------------------------------- notes
Step 'Writing snapshot notes'

$notes = @'
# MIGRATION-NOTES

Generated by `scripts/migrate-repo.ps1`. Restore with `deploy-repo.ps1` in this directory.

## Carried (snapshot profile)

| File | What it holds |
|---|---|
| `repo-all.bundle` | Every local branch (including branches with no remote), every tag, HEAD. `git bundle verify` OK. |
| `doom3-submodule.bundle` | The pristine submodule's history, so `doom3_orig/doom3` restores offline. |
| `dangling-commits.bundle` | Unreachable commits (rebases, removed worktrees), when any exist and `-NoDanglingBundle` was not used. |
| `archive/` | Key ignored data with its repository-relative path; see the table below. |
| `local-state/specify-feature.json` | Current spec-kit feature pointer. |
| `local-state/agent-manager.json.reference` | Agent Manager recovery state. Reference only: host-specific absolute paths, never installed automatically. |
| `local-state/worktree-list.txt` | `git worktree list --porcelain` at snapshot time. |
| `local-state/untracked/` | Untracked, non-ignored files (only with `-AllowDirty`); restored without clobbering existing files. |
| `local-state/worktree-uncommitted.patch` | Tracked-file diff at snapshot time (only with `-AllowDirty`); not applied automatically. |
| `IGNORED-INVENTORY.txt` | Every ignored path, its size, and whether it was archived. This is the completeness check. |
| `machine-facts.json` | Host toolchain, paths, refs, submodule SHA. |
| `local-only-refs.txt` | Branches and commits that exist only on the source machine. |

## What lives in archive/

| Path | Why it is kept |
|---|---|
| `archive/showcase/evidence/` | Captured posters/logs; only reproducible with game data. |
| `archive/out/` | Test/run evidence and reports. |
| `archive/shaders/tools/.cache/` | Pinned DXC toolchain (~100 MB); avoids a re-download and keeps an old commit buildable. |
| `archive/content/tools/.cache/` | Pinned texture encoder. |
| `archive/.specify/integrations/.cache/` | spec-kit catalogue cache. |
| `archive/.kilo/skills/` | Generated skill mirror. |
| `archive/targets/<name>/doom3/` | Engine working copies; also reconstructable from pristine + patch. |
| `archive/build*/bin/` | Built `DOOM3.exe`/`DOOM3XP.exe` (and pdbs): proof the snapshot builds, usable without rebuilding. |
| `archive/build-logs/` | Every `*.log` from the repo root and the build dirs. |
| `archive/.kilo/worktrees/` | Only with `-IncludeWorktrees`; archive only, not relinked on restore. |

## Not carried, and how to recreate it

| Omitted | Why | Recreate with |
|---|---|---|
| Build intermediates (`build*/CMakeFiles`, `*.obj`) | Pure derivation, ~1 GB; binaries and logs are archived | `cmake --preset win11-amd64` then `cmake --build --preset win11-amd64-release` |
| `.kilo/worktrees/` (default) | Linked checkouts; their branches are in the bundle | `scripts/new-worktree.ps1 -Issue <N> -Slug <slug>` |
| `targets/*/build`, `targets/*/out` | Per-target scratch output | Rebuild from the target copy |
| Game data (`*.pk4`) | Not redistributable, never in git | Copy the retail install or `demo00.pk4`, then `scripts/prepare-demo-data.ps1` |
| Host toolchain | Not repository state | See `machine-facts.json`: VS2022 (v143) + SDK 10.0.26100, emsdk 6.0.13, node, uv, gh |

## Deliberate caveats

* The pristine submodule must be clean at snapshot time. Dirt there is saved as
  `local-state/submodule-uncommitted.patch` only with `-AllowDirty`.
* A fresh checkout can differ from the source worktree in line endings only. The commit content and
  tree hash are identical (`git rev-parse HEAD^{tree}`); use that, not a byte-level directory diff.
* Archived `build*/bin` binaries were built from the state at snapshot time. They are not verified
  against HEAD; run a build to replace them.
* `deploy-repo.ps1` restores the archive's working parts (evidence, out, caches, skills, build bins,
  logs) but always rebuilds `targets/*/doom3` from pristine + patch, because that is the only
  reproducible definition of those copies. The archived target tree stays available for inspection.
* `.kilo/agent-manager.json` contains absolute paths from the source host and is never installed.
'@

Write-TextFile (Join-Path $OutDir 'MIGRATION-NOTES.md') ($notes + "`r`n")

# ---------------------------------------------------------------- bootstrap script
Step 'Copying deploy script'
$deploy = Join-Path $repo 'scripts\deploy-repo.ps1'
if (Test-Path -LiteralPath $deploy) {
    Copy-Item -LiteralPath $deploy -Destination (Join-Path $OutDir 'deploy-repo.ps1') -Force
} else {
    Write-Host 'scripts/deploy-repo.ps1 missing; the bundle has no bootstrap script' -ForegroundColor Yellow
}

# ---------------------------------------------------------------- checksums
Step 'Hashing the snapshot'
$checksumPath = Join-Path $OutDir 'checksums.sha256'
$rows = @()
foreach ($f in (Get-ChildItem -LiteralPath $OutDir -Recurse -File -Force | Where-Object { $_.FullName -ne $checksumPath } | Sort-Object FullName)) {
    $rel = $f.FullName.Substring($OutDir.Length).TrimStart('\')
    $rows += ("{0}  {1}" -f (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), ($rel -replace '\\', '/'))
}
Write-TextFile $checksumPath (($rows -join "`n") + "`n")

# ---------------------------------------------------------------- summary
$size = (Get-ChildItem -LiteralPath $OutDir -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum
Write-Host ''
Write-Host 'Snapshot ready' -ForegroundColor Green
Write-Host ("  path      : {0}" -f $OutDir)
Write-Host ("  size      : {0:N1} MB in {1} files" -f ($size / 1MB), ($rows.Count + 1))
Write-Host ("  profile   : {0}" -f $Profile)
Write-Host ("  HEAD      : {0} @ {1}" -f $branchLabel, $headSha.Substring(0, 12))
Write-Host ("  submodule : {0}" -f $(if ($subSha) { $subSha.Substring(0, 12) } else { 'not bundled' }))
Write-Host ("  dangling  : {0} ({1})" -f $dangling.Count, $(if ($danglingBundled) { 'bundled' } else { 'listed only' }))
Write-Host ("  archive   : {0} trees, {1:N1} MB" -f $archived.Count, (($archived | Measure-Object -Property Bytes -Sum).Sum / 1MB))
Write-Host ("  excluded  : {0:N1} MB (see IGNORED-INVENTORY.txt)" -f ($excludedBytes / 1MB))
Write-Host ('  local-only: {0}' -f $(if ($localOnly.Count -gt 0) { ($localOnly | ForEach-Object { $_.name }) -join ', ' } else { 'none' }))
Write-Host ''
Write-Host 'Verify and restore on any machine:' -ForegroundColor Cyan
Write-Host ("  powershell -ExecutionPolicy Bypass -File deploy-repo.ps1 -Bundle ""{0}"" -Dest <new-repo-path>" -f $OutDir)
Write-Host ''
exit 0
