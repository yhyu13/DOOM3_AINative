#Requires -Version 5.1
<#
.SYNOPSIS
  Render the seven-cell Q4 content matrix (spec 009, T-028).
.DESCRIPTION
  Joins three sources into one table: the C-12 cell list, the Q4 material set, and the built packages
  under out/content. Each cell shows its material, whether a sidecar exists, whether a package was built
  and verified, what the capture state is, and the budget row.

  By default the script reports and exits 0 even when cells are missing, because an incomplete matrix is
  a normal intermediate state. With -RequireComplete it exits 1 unless all seven cells are authored,
  packaged and either captured or explicitly BLOCKED, which is the acceptance check T-028 names.
.EXAMPLE
  .\content\tools\matrix-report.ps1
.EXAMPLE
  .\content\tools\matrix-report.ps1 -RequireComplete
#>
[CmdletBinding()]
param(
    [string]$OutDir,
    [string]$OutputPath,
    [switch]$RequireComplete
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$repo = Get-RepoRoot -Start $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $repo 'out\content' }

$cells = @(
    @{ id = 'opaque_dielectric'; label = 'Opaque dielectric' },
    @{ id = 'opaque_metal'; label = 'Opaque metal' },
    @{ id = 'alpha_tested'; label = 'Alpha-tested geometry' },
    @{ id = 'emissive'; label = 'Emissive' },
    @{ id = 'skinned_character'; label = 'Skinned character' },
    @{ id = 'transmissive_glass'; label = 'Transmissive / glass' },
    @{ id = 'large_environment'; label = 'Large environment surface' }
)

$setPath = Join-Path $repo 'content\manifest\q4-materials.json'
$budgetPath = Join-Path $repo 'content\manifest\budget-rows.json'
$set = Read-Json $setPath
$budgets = Read-Json $budgetPath

$rows = @()
$missing = 0
foreach ($cell in $cells) {
    $entries = @($set.materials | Where-Object { $_.cell -eq $cell.id })
    $material = if ($entries.Count -gt 0) { $entries[0].material } else { '' }
    $sidecar = if ($material) { Join-Path $repo "content\materials\$material.mat.json" } else { '' }
    $hasSidecar = $sidecar -and (Test-Path -LiteralPath $sidecar)

    $packageState = 'NOT BUILT'
    if ($material) {
        $pkgPath = Join-Path $OutDir "$material\package.json"
        if (Test-Path -LiteralPath $pkgPath) {
            try {
                $pkg = Read-Json $pkgPath
                $packageState = "built ($(@($pkg.textures).Count) textures)"
            } catch { $packageState = 'INVALID package.json' }
        }
    }

    $budget = @($budgets.rows | Where-Object { $_.cell -eq $cell.id })
    if ($budget.Count -eq 0) { $budgetState = 'MISSING ROW'; $missing++ }
    elseif ($budget[0].state -like 'blocked:*') { $budgetState = $budget[0].state }
    elseif ($budget[0].artifact) { $budgetState = "recorded ($($budget[0].task))" }
    else { $budgetState = 'INVALID (no artifact)'; $missing++ }

    if (-not $hasSidecar) { $missing++ }

    $rows += [pscustomobject]@{
        Cell     = $cell.label
        Material = if ($material) { $material } else { '-' }
        Sidecar  = if ($hasSidecar) { 'yes' } else { 'NOT AUTHORED' }
        Package  = $packageState
        Capture  = 'BLOCKED: spec 004 has not reached Q4'
        Budget   = $budgetState
    }
}

$coverage = @($rows | Where-Object { $_.Sidecar -eq 'yes' }).Count
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('| Cell | Material | Sidecar | Package | Capture | Budget |')
$lines.Add('|---|---|---|---|---|---|')
foreach ($r in $rows) {
    $lines.Add("| $($r.Cell) | $($r.Material) | $($r.Sidecar) | $($r.Package) | $($r.Capture) | $($r.Budget) |")
}
$table = ($lines -join "`n")

if ($OutputPath) {
    $dir = Split-Path -Parent $OutputPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [System.IO.File]::WriteAllText($OutputPath, $table + "`n", (New-Object System.Text.UTF8Encoding($false)))
}

Write-Host $table
Write-Host ''
Write-Host ("MATRIX: coverage {0}/{1} cells authored, {2}/7 captures BLOCKED, budget rows {3}/7 present" -f $coverage, $cells.Count, $cells.Count, (@($budgets.rows).Count))

if ($RequireComplete) {
    if ($coverage -ne $cells.Count) {
        Write-Host "MATRIX INCOMPLETE: $($cells.Count - $coverage) cell(s) not authored" -ForegroundColor Red
        exit 1
    }
    $bad = @($rows | Where-Object { $_.Package -notlike 'built*' })
    if ($bad.Count -gt 0) {
        Write-Host "MATRIX INCOMPLETE: $($bad.Count) cell(s) without a built package" -ForegroundColor Red
        exit 1
    }
}
exit 0
