#Requires -Version 5.1
<#
.SYNOPSIS
  Compile the classic shader library for both targets (spec 008, T-023).
.DESCRIPTION
  Uses the pinned DXC from shaders/tools/pins.json (fetching it with get-toolchain.ps1 when missing)
  and builds, for every shipping manifest entry that declares a source:

      DXIL   <out>/<key>/<stage>.dxil   plus the disassembly <stage>.asm used by reflect.ps1
      SPIR-V <out>/<key>/<stage>.spv    with -fvk-use-dx-layout -fspv-target-env=vulkan1.1

  Tier B variants listed in shaders/classic/stages/variants.json are compiled with their defines.
  -ProveBound ignores that list and compiles the full 7 x 2 x 3 = 42 cross product into a scratch
  directory, which proves the bound is real without shipping unobserved variants.

  Fail-closed: any compile error, any missing pinned compiler, any declared entry point that does not
  exist exits non-zero. The summary is written to <out>/compile-summary.json.

  Output lives under out/ (git-ignored): generated blobs are build artifacts, not sources (D-P3).
.EXAMPLE
  powershell -NoProfile -File shaders/tools/compile.ps1 -All
.EXAMPLE
  powershell -NoProfile -File shaders/tools/compile.ps1 -ProveBound
#>
[CmdletBinding()]
param(
    [switch]$All,
    [switch]$ProveBound,
    [string]$Program,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $repo 'shaders\classic\include'
$programsDir = Join-Path $repo 'shaders\classic\programs'
$stagesDir = Join-Path $repo 'shaders\classic\stages'
$manifestPath = Join-Path $repo 'shaders\manifest\classic-programs.json'
$pinsPath = Join-Path $PSScriptRoot 'pins.json'
$variantsPath = Join-Path $stagesDir 'variants.json'
if (-not $OutDir) { $OutDir = Join-Path $repo 'out\shaders' }
if ($ProveBound) { $OutDir = Join-Path $OutDir 'bound-proof' }

foreach ($p in @($manifestPath, $pinsPath, $include, $programsDir)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR: missing $p" -ForegroundColor Red; exit 2 }
}

$pins = Get-Content -LiteralPath $pinsPath -Raw | ConvertFrom-Json
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$profiles = $pins.profiles

# --- pinned compiler ------------------------------------------------------------------------------
$dxc = Join-Path $repo ($pins.dxc.cacheRelativePath -replace '/', '\')
$dxc = Join-Path $dxc 'bin\x64\dxc.exe'
if (-not (Test-Path -LiteralPath $dxc)) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'get-toolchain.ps1')
    if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: toolchain unavailable' -ForegroundColor Red; exit 2 }
}
$versionLine = (& $dxc --version 2>&1 | Out-String).Trim()
if ($versionLine -notlike "*$($pins.dxc.compilerVersion)*") {
    Write-Host "ERROR: dxc reports '$versionLine', pin expects $($pins.dxc.compilerVersion)" -ForegroundColor Red
    exit 3
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$results = New-Object System.Collections.Generic.List[object]
$failures = 0

function Invoke-Compile {
    param(
        [string]$Key,
        [string]$Stage,
        [string]$Source,
        [string]$Profile,
        [string[]]$ExtraArgs,
        [string]$DestDir
    )
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    $dxil = Join-Path $DestDir "$Stage.dxil"
    $asm = Join-Path $DestDir "$Stage.asm"
    $spv = Join-Path $DestDir "$Stage.spv"

    $dxilArgs = @('-T', $Profile, '-E', "${Stage}_main", '-I', $include, '-Fo', $dxil, '-Fc', $asm) + $ExtraArgs + @($Source)
    $null = & $dxc @dxilArgs 2>&1
    $dxilExit = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $asm)) { $dxilExit = 1 }

    $spvArgs = @('-T', $Profile, '-E', "${Stage}_main", '-I', $include) + $profiles.spirv.arguments + @('-Fo', $spv) + $ExtraArgs + @($Source)
    $null = & $dxc @spvArgs 2>&1
    $spvExit = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $spv)) { $spvExit = 1 }

    $ok = ($dxilExit -eq 0) -and ($spvExit -eq 0)
    $results.Add([pscustomobject]@{
            key       = $Key
            stage     = $Stage
            profile   = $Profile
            source    = $Source
            defines   = ($ExtraArgs -join ' ')
            dxil_exit = $dxilExit
            spv_exit  = $spvExit
            dxil_sha  = if (Test-Path -LiteralPath $dxil) { (Get-FileHash -LiteralPath $dxil -Algorithm SHA256).Hash } else { '' }
            spv_sha   = if (Test-Path -LiteralPath $spv) { (Get-FileHash -LiteralPath $spv -Algorithm SHA256).Hash } else { '' }
            source_sha = if (Test-Path -LiteralPath $Source) { (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash } else { '' }
        })
    if ($ok) {
        Write-Host ("  OK    {0,-20} {1,-3} dxil={2} spv={3}" -f $Key, $Stage, $dxilExit, $spvExit)
    } else {
        Write-Host ("  FAIL  {0,-20} {1,-3} dxil={2} spv={3}" -f $Key, $Stage, $dxilExit, $spvExit) -ForegroundColor Red
        $script:failures++
    }
}

if ($ProveBound) {
    # 7 texgen modes x 2 alpha-test states x 3 vertex-colour modes. Nothing is shipped from here.
    $count = 0
    foreach ($m in 0..6) {
        foreach ($a in 0..1) {
            foreach ($v in 0..2) {
                $name = "generic_texgen_t${m}_a${a}_v${v}"
                $dest = Join-Path $OutDir $name
                $defines = @("-DD3_TEXGEN_MODE=$m", "-DD3_ALPHATEST=$a", "-DD3_VERTEXCOLOR=$v")
                Invoke-Compile -Key $name -Stage 'vs' -Source (Join-Path $stagesDir 'generic_texgen.hlsl') `
                    -Profile $profiles.dxil.vs -ExtraArgs $defines -DestDir $dest
                Invoke-Compile -Key $name -Stage 'ps' -Source (Join-Path $stagesDir 'generic_texgen.hlsl') `
                    -Profile $profiles.dxil.ps -ExtraArgs $defines -DestDir $dest
                $count++
            }
        }
    }
    $summary = [pscustomobject]@{
        schema    = 'doom3-shader-compile/1'
        mode      = 'bound-proof'
        dxc       = $pins.dxc.compilerVersion
        variants  = $count
        compiles  = $results.Count
        failures  = $failures
        results   = $results
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'compile-summary.json')
    if ($failures -ne 0) { Write-Host "BOUND PROOF FAILED: $failures compile(s) failed" -ForegroundColor Red; exit 1 }
    Write-Host "BOUND PROOF OK: $count variants x 2 stages = $($results.Count) compiles, 0 failures"
    exit 0
}

# --- Tier A: every shipping manifest entry that has a source --------------------------------------
$selected = @($manifest.programs | Where-Object { $_.source })
if ($Program) { $selected = @($selected | Where-Object { $_.key -eq $Program }) }
if ($selected.Count -eq 0) {
    Write-Host "ERROR: no program to compile (Program='$Program')" -ForegroundColor Red
    exit 2
}

foreach ($entry in $selected) {
    $source = Join-Path $repo ($entry.source -replace '/', '\')
    if (-not (Test-Path -LiteralPath $source)) {
        Write-Host "  FAIL  $($entry.key): source missing: $($entry.source)" -ForegroundColor Red
        $failures++
        continue
    }
    foreach ($stage in @('vs', 'ps')) {
        $declared = $false
        if ($entry.entry -and ($entry.entry.PSObject.Properties.Name -contains $stage)) { $declared = $true }
        if (-not $declared) { continue }
        $profile = $profiles.dxil.$stage
        Invoke-Compile -Key $entry.key -Stage $stage -Source $source -Profile $profile -ExtraArgs @() `
            -DestDir (Join-Path $OutDir $entry.key)
    }
}

# --- Tier B: only census-observed variants --------------------------------------------------------
$variantCount = 0
if ((-not $Program) -and (-not $ProveBound)) {
    # The generic vertex stage is compiled unconditionally: arbFP_glasswarp is a fragment-only program
    # in the engine, so the port must supply its vertex stage. This is an ABI pairing artifact used by
    # reflect.ps1, not a shipped Tier B variant.
    Invoke-Compile -Key 'generic_texgen' -Stage 'vs' -Source (Join-Path $stagesDir 'generic_texgen.hlsl') `
        -Profile $profiles.dxil.vs -ExtraArgs @() -DestDir (Join-Path $OutDir 'generic_texgen')
}

if ((Test-Path -LiteralPath $variantsPath) -and (-not $Program)) {
    $variants = Get-Content -LiteralPath $variantsPath -Raw | ConvertFrom-Json
    foreach ($v in @($variants.variants)) {
        if ($null -eq $v) { continue }
        $name = "generic_texgen_t$($v.texgen)_a$($v.alphatest)_v$($v.vertexcolor)"
        $defines = @("-DD3_TEXGEN_MODE=$($v.texgen)", "-DD3_ALPHATEST=$($v.alphatest)", "-DD3_VERTEXCOLOR=$($v.vertexcolor)")
        $dest = Join-Path (Join-Path $OutDir 'variants') $name
        Invoke-Compile -Key $name -Stage 'vs' -Source (Join-Path $stagesDir 'generic_texgen.hlsl') `
            -Profile $profiles.dxil.vs -ExtraArgs $defines -DestDir $dest
        Invoke-Compile -Key $name -Stage 'ps' -Source (Join-Path $stagesDir 'generic_texgen.hlsl') `
            -Profile $profiles.dxil.ps -ExtraArgs $defines -DestDir $dest
        $variantCount++
    }
}

$summary = [pscustomobject]@{
    schema      = 'doom3-shader-compile/1'
    mode        = 'shipping'
    dxc         = $pins.dxc.compilerVersion
    programs    = $selected.Count
    variants    = $variantCount
    compiles    = $results.Count
    failures    = $failures
    results     = $results
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutDir 'compile-summary.json')

if ($failures -ne 0) {
    Write-Host "COMPILE FAILED: $failures of $($results.Count) stage compiles" -ForegroundColor Red
    exit 1
}
Write-Host "COMPILE OK: $($selected.Count) programs, $variantCount Tier B variants, $($results.Count) stage compiles, dxc $($pins.dxc.compilerVersion)"
exit 0
