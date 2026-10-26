#Requires -Version 5.1
<#
.SYNOPSIS
  Remove one git worktree, prune stale metadata, and optionally delete the merged branch.

.DESCRIPTION
  Refuses to remove the main worktree or the worktree containing this script.
  Refuses paths outside .kilo/worktrees unless -Force is given.
  Refuses a dirty worktree unless -Force is given; -Force discards uncommitted work.
  Deletes the local branch only when it is fully merged into origin/main.

.EXAMPLE
  .\scripts\remove-worktree.ps1 -Path .kilo\worktrees\feat-42-webgl-audio

.EXAMPLE
  .\scripts\remove-worktree.ps1 -Path .kilo\worktrees\feat-42-webgl-audio -Force
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

function Fail {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string[]]$GitArgs,
        [switch]$AllowFail
    )
    $output = & git -C $Repo @GitArgs
    $code = $LASTEXITCODE
    if ($code -ne 0 -and -not $AllowFail) {
        Fail ("git {0} failed in {1} (exit {2})" -f ($GitArgs -join ' '), $Repo, $code)
    }
    return [pscustomobject]@{ Output = @($output); ExitCode = $code }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$probe = Invoke-Git -Repo $scriptDir -GitArgs @('rev-parse', '--show-toplevel') -AllowFail
if ($probe.ExitCode -ne 0) {
    Fail "scripts/remove-worktree.ps1 must live inside a git checkout."
}
$RepoRoot = (($probe.Output[0]).Trim()) -replace '/', '\'

if (-not [System.IO.Path]::IsPathRooted($Path)) {
    $Path = Join-Path $RepoRoot $Path
}
$target = [System.IO.Path]::GetFullPath($Path)
$target = $target.TrimEnd('\', '/')

# --- read worktree registry ---------------------------------------------------------
$porcelain = Invoke-Git -Repo $RepoRoot -GitArgs @('worktree', 'list', '--porcelain')
$entries = @()
$current = $null
foreach ($line in $porcelain.Output) {
    if ($line -like 'worktree *') {
        if ($null -ne $current) { $entries += (New-Object psobject -Property $current) }
        $current = [ordered]@{
            Path     = ($line.Substring(9).Trim() -replace '/', '\')
            Branch   = ''
            Detached = $false
            Locked   = $false
        }
    }
    elseif ($null -eq $current) {
        continue
    }
    elseif ($line -like 'branch *') {
        $current.Branch = $line.Substring(7).Trim() -replace '^refs/heads/', ''
    }
    elseif ($line -eq 'detached') {
        $current.Detached = $true
    }
    elseif ($line -eq 'locked' -or $line -like 'locked *') {
        $current.Locked = $true
    }
}
if ($null -ne $current) { $entries += (New-Object psobject -Property $current) }

$mainEntry = $entries[0]
$mainPath = $mainEntry.Path.TrimEnd('\', '/')

if ($target -ieq $mainPath) {
    Fail "Refusing to remove the main worktree ($mainPath)."
}
if ($target -ieq ($RepoRoot.TrimEnd('\', '/'))) {
    Fail "Refusing to remove the worktree that contains this script ($RepoRoot). Run the script from the main checkout."
}

$entry = $entries | Where-Object { $_.Path.TrimEnd('\', '/') -ieq $target } | Select-Object -First 1
if ($null -eq $entry) {
    Fail "'$target' is not a registered worktree. If the directory is already gone, run: git -C `"$RepoRoot`" worktree prune"
}

$conventionRoot = (Join-Path $mainPath '.kilo\worktrees').TrimEnd('\', '/')
$insideConvention = $target.StartsWith($conventionRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)
if (-not $insideConvention -and -not $Force) {
    Fail "'$target' is outside .kilo\worktrees. Re-run with -Force only if you are sure it is disposable."
}

if ($entry.Locked -and -not $Force) {
    Fail "'$target' is locked. Unlock it with: git -C `"$RepoRoot`" worktree unlock `"$target`" (or use -Force)."
}

# --- dirty guard --------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $target)) {
    Fail "Directory '$target' is missing. Run: git -C `"$RepoRoot`" worktree prune"
}

$status = Invoke-Git -Repo $target -GitArgs @('status', '--porcelain') -AllowFail
if ($status.ExitCode -ne 0) {
    Fail "git status failed in '$target' (exit $($status.ExitCode)); refusing to remove."
}
$changedLines = @($status.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

# Superproject status ignores untracked files inside submodules; check them explicitly.
$submoduleLines = @()
if (Test-Path -LiteralPath (Join-Path $target '.gitmodules')) {
    $subStatus = Invoke-Git -Repo $target -GitArgs @('submodule', 'foreach', '--recursive', '--quiet', 'git status --porcelain') -AllowFail
    if ($subStatus.ExitCode -eq 0) {
        $submoduleLines = @($subStatus.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    else {
        Write-Host "WARNING: could not inspect submodule status (exit $($subStatus.ExitCode)); proceeding with superproject status only." -ForegroundColor Yellow
    }
}

$dirtyCount = $changedLines.Count + $submoduleLines.Count
if ($dirtyCount -gt 0 -and -not $Force) {
    Write-Host "Worktree has $dirtyCount uncommitted/untracked path(s):" -ForegroundColor Yellow
    @($changedLines + $submoduleLines) | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" }
    if ($dirtyCount -gt 10) { Write-Host "  ... ($($dirtyCount - 10) more)" }
    Fail "Refusing to delete uncommitted work. Commit/push, or re-run with -Force to discard it."
}
if ($dirtyCount -gt 0 -and $Force) {
    Write-Host "WARNING: -Force discards $dirtyCount uncommitted/untracked path(s) in $target" -ForegroundColor Yellow
}

# --- remove -------------------------------------------------------------------------
$removeArgs = @('worktree', 'remove')
if ($Force) { $removeArgs += @('--force', '--force') }
$removeArgs += $target

Write-Host "Removing worktree $target"
$remove = Invoke-Git -Repo $RepoRoot -GitArgs $removeArgs -AllowFail
if ($remove.ExitCode -ne 0 -and -not $Force) {
    # git 2.25 refuses to remove any worktree whose tree contains submodules.
    # The tree was verified clean above, so retrying with force only discards ignored output.
    Write-Host "git 2.25 cannot remove a submodule worktree without force; retrying after the clean check..." -ForegroundColor Yellow
    $remove = Invoke-Git -Repo $RepoRoot -GitArgs @('worktree', 'remove', '--force', '--force', $target) -AllowFail
}
if ($remove.ExitCode -ne 0) {
    Fail "git worktree remove failed (exit $($remove.ExitCode)). Close editors/processes using the path, then retry."
}

$prune = Invoke-Git -Repo $RepoRoot -GitArgs @('worktree', 'prune') -AllowFail
if ($prune.ExitCode -ne 0) {
    Write-Host "WARNING: git worktree prune failed (exit $($prune.ExitCode)); run it manually." -ForegroundColor Yellow
}

# --- branch cleanup (merged only) ---------------------------------------------------
$branchState = 'no branch to delete'
if (-not $entry.Detached -and -not [string]::IsNullOrWhiteSpace($entry.Branch) -and $entry.Branch -ne 'main') {
    $branchState = "kept local branch '$($entry.Branch)' (check manually)"
    $originMain = Invoke-Git -Repo $RepoRoot -GitArgs @('rev-parse', '--verify', '--quiet', 'refs/remotes/origin/main') -AllowFail
    if ($originMain.ExitCode -eq 0) {
        $ancestor = Invoke-Git -Repo $RepoRoot -GitArgs @('merge-base', '--is-ancestor', $entry.Branch, 'refs/remotes/origin/main') -AllowFail
        if ($ancestor.ExitCode -eq 0) {
            $delete = Invoke-Git -Repo $RepoRoot -GitArgs @('branch', '-d', $entry.Branch) -AllowFail
            if ($delete.ExitCode -eq 0) {
                $branchState = "deleted merged local branch '$($entry.Branch)'"
            }
            else {
                $branchState = "worktree removed; 'git branch -d $($entry.Branch)' failed (branch may be checked out elsewhere)"
            }
        }
        elseif ($ancestor.ExitCode -eq 1) {
            $branchState = "kept unmerged local branch '$($entry.Branch)' (git branch -D it after confirming it is disposable)"
        }
    }
    else {
        $branchState = "kept local branch '$($entry.Branch)' (origin/main missing, merge check skipped)"
    }
}

Write-Host ""
Write-Host "Removed : $target" -ForegroundColor Green
Write-Host "Branch  : $branchState"
Write-Host "Prune   : done"
