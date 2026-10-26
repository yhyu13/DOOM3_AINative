#Requires -Version 5.1
<#
.SYNOPSIS
  List every git worktree with branch, HEAD, state, and merge status.

.DESCRIPTION
  Reads `git worktree list --porcelain`, then reports for each worktree:
    Branch  - short branch name, or (detached)
    HEAD    - first 8 chars of the commit
    State   - clean | dirty (n paths) | missing | prunable | locked
    Merged  - yes/no vs origin/main; trunk for main; n/a if origin/main is absent
    Path    - absolute worktree path

.EXAMPLE
  .\scripts\list-worktrees.ps1
#>
[CmdletBinding()]
param()

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
    Fail "scripts/list-worktrees.ps1 must live inside a git checkout."
}
$RepoRoot = ($probe.Output[0]).Trim() -replace '/', '\'

$porcelain = Invoke-Git -Repo $RepoRoot -GitArgs @('worktree', 'list', '--porcelain')

$entries = @()
$current = $null
foreach ($line in $porcelain.Output) {
    if ($line -like 'worktree *') {
        if ($null -ne $current) { $entries += (New-Object psobject -Property $current) }
        $current = [ordered]@{
            Path     = $line.Substring(9).Trim()
            Head     = ''
            Branch   = ''
            Detached = $false
            Locked   = $false
            Prunable = $false
        }
    }
    elseif ($null -eq $current) {
        continue
    }
    elseif ($line -like 'HEAD *') {
        $current.Head = $line.Substring(5).Trim()
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
    elseif ($line -eq 'prunable' -or $line -like 'prunable *') {
        $current.Prunable = $true
    }
}
if ($null -ne $current) { $entries += (New-Object psobject -Property $current) }

if ($entries.Count -eq 0) {
    Fail "git worktree list returned nothing (is $RepoRoot a git checkout?)."
}

$originMain = Invoke-Git -Repo $RepoRoot -GitArgs @('rev-parse', '--verify', '--quiet', 'refs/remotes/origin/main') -AllowFail
$hasOriginMain = ($originMain.ExitCode -eq 0)

$rows = @()
$removable = @()
foreach ($entry in $entries) {
    $nativePath = $entry.Path -replace '/', '\'
    $branchLabel = if ([string]::IsNullOrWhiteSpace($entry.Branch)) { '(detached)' } else { $entry.Branch }
    $shortHead = if ($entry.Head.Length -ge 8) { $entry.Head.Substring(0, 8) } else { $entry.Head }
    if ([string]::IsNullOrWhiteSpace($shortHead)) { $shortHead = '-' }

    $state = 'clean'
    if ($entry.Prunable) {
        $state = 'prunable'
    }
    elseif (-not (Test-Path -LiteralPath $nativePath)) {
        $state = 'missing'
    }
    else {
        $status = Invoke-Git -Repo $nativePath -GitArgs @('status', '--porcelain') -AllowFail
        if ($status.ExitCode -ne 0) {
            $state = 'error'
        }
        else {
            $changed = @($status.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }).Count
            if ($changed -gt 0) { $state = "dirty ($changed path(s))" }
        }
    }
    if ($entry.Locked) { $state = "$state, locked" }

    $merged = 'n/a'
    $isMerged = $false
    if ($entry.Detached) {
        $merged = 'n/a'
    }
    elseif ($entry.Branch -eq 'main') {
        $merged = 'trunk'
    }
    elseif ($hasOriginMain) {
        $ancestor = Invoke-Git -Repo $RepoRoot -GitArgs @('merge-base', '--is-ancestor', $entry.Branch, 'refs/remotes/origin/main') -AllowFail
        if ($ancestor.ExitCode -eq 0) {
            $merged = 'yes'
            $isMerged = $true
        }
        elseif ($ancestor.ExitCode -eq 1) {
            $merged = 'no'
        }
        else {
            $merged = 'n/a'
        }
    }

    if ($isMerged -and $state -eq 'clean' -and $branchLabel -ne 'main') {
        $removable += $branchLabel
    }

    $rows += [pscustomobject]@{
        Branch = $branchLabel
        HEAD   = $shortHead
        State  = $state
        Merged = $merged
        Path   = $nativePath
    }
}

Write-Host "Repo: $RepoRoot"
Write-Host ""
$rows | Format-Table -Property Branch, HEAD, State, Merged, Path -AutoSize

Write-Host "Merged=yes: branch is fully contained in origin/main (safe to remove after PR merge)."
if (-not $hasOriginMain) {
    Write-Host "origin/main is missing: Merged is n/a. Push main before relying on merge checks." -ForegroundColor Yellow
}
if ($removable.Count -gt 0) {
    Write-Host ""
    Write-Host "Merged and clean, candidates for cleanup:" -ForegroundColor Yellow
    foreach ($b in $removable) { Write-Host "  $b" }
}
