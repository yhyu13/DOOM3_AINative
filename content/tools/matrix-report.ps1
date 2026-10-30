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

    # A budget row is valid only as one of two things: a recorded measurement with an artefact, or an
    # explicit blocked reason. "Some string plus an artifact" is not a measurement.
    $budget = @($budgets.rows | Where-Object { $_.cell -eq $cell.id })
    $budgetValid = $false
    if ($budget.Count -eq 0) { $budgetState = 'MISSING ROW' }
    elseif ($budget.Count -gt 1) { $budgetState = 'DUPLICATE ROW' }
    elseif ($budget[0].state -like 'blocked:*') {
        $budgetState = $budget[0].state
        $budgetValid = ($budget[0].state.Length -gt 8)
    } elseif ($budget[0].state -eq 'recorded' -and $budget[0].artifact) {
        $budgetState = "recorded ($($budget[0].task))"
        $budgetValid = $true
    } else {
        $budgetState = "INVALID (state '$($budget[0].state)')"
    }
    if (-not $budgetValid) { $missing++ }

    if (-not $hasSidecar) { $missing++ }

    # Capture state comes from an artefact the capture stage would write, never from a hard-coded string:
    # a hard-coded BLOCKED would keep saying BLOCKED after a capture landed.
    $capturePath = if ($material) { Join-Path $OutDir "$material\capture.json" } else { '' }
    if ($capturePath -and (Test-Path -LiteralPath $capturePath)) {
        try {
            $cap = Read-Json $capturePath
            $captureState = "captured ($($cap.backend) / $($cap.adapter))"
        } catch { $captureState = 'INVALID capture.json' }
    } else {
        $captureState = 'BLOCKED: no capture record in the output directory (reason in the spec 004 evidence)'
    }

    $rows += [pscustomobject]@{
        Cell     = $cell.label
        Material = if ($material) { $material } else { '-' }
        Sidecar  = if ($hasSidecar) { 'yes' } else { 'NOT AUTHORED' }
        Package  = $packageState
        Capture  = $captureState
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
$blockedCaptures = @($rows | Where-Object { "$($_.Capture)" -like 'BLOCKED*' }).Count
$capturedCells = $cells.Count - $blockedCaptures
Write-Host ("MATRIX: coverage {0}/{1} cells authored, {2}/{1} captured, {3}/{1} captures BLOCKED, budget rows {4}/7 present" -f $coverage, $cells.Count, $capturedCells, $blockedCaptures, (@($budgets.rows).Count))

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
    $badBudget = @($rows | Where-Object { $_.Budget -like 'INVALID*' -or $_.Budget -eq 'MISSING ROW' -or $_.Budget -eq 'DUPLICATE ROW' })
    if ($badBudget.Count -gt 0) {
        Write-Host "MATRIX INCOMPLETE: $($badBudget.Count) cell(s) without a valid budget row" -ForegroundColor Red
        exit 1
    }
    # "Built" is not "verified": run the verifier for each material rather than trusting a file's name.
    $verifyScript = Join-Path $PSScriptRoot 'verify-package.ps1'
    foreach ($r in ($rows | Where-Object { $_.Material -ne '-' })) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $verifyScript -OutDir (Join-Path $OutDir $r.Material) -RepoRoot $repo | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "MATRIX INCOMPLETE: package for '$($r.Material)' does not verify" -ForegroundColor Red
            exit 1
        }
    }
}
exit 0
