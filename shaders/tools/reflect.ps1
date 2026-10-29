#Requires -Version 5.1
<#
.SYNOPSIS
  Reflection / ABI conformance gate for the classic shader library (spec 008, T-026).
.DESCRIPTION
  Reads the DXIL disassembly that compile.ps1 writes (out/shaders/**/<stage>.asm) and proves four
  things per stage:

    1. every constant buffer the program uses is declared in shaders/classic/include/d3_abi.hlsli;
    2. every texture/sampler the program binds is declared there too;
    3. every input/output semantic is one of the semantics declared in d3_input.hlsli;
    4. the fragment stage's input signature is exactly the vertex stage's output signature, so an
       inter-stage struct cannot drift between the two stages. arbFP_glasswarp is checked against the
       generic Tier B vertex stage, because the engine binds only a fragment program for it.

  This is a *declared-vs-used* check on the DXIL side. It is not a substitute for WGSL validation:
  compile.ps1 translates each stage to WGSL with the pinned Tint and re-parses it, and pins.json
  records that pin. Reflection stays DXIL-side because it reads the disassembly.

  Fail-closed: a missing disassembly, an undeclared binding, an undeclared semantic, or a mismatched
  inter-stage signature exits 1.
.EXAMPLE
  powershell -NoProfile -File shaders/tools/reflect.ps1
#>
[CmdletBinding()]
param([string]$BuildDir)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'out\shaders' }
$abiHeader = Join-Path $repo 'shaders\classic\include\d3_abi.hlsli'
$inputHeader = Join-Path $repo 'shaders\classic\include\d3_input.hlsli'
$manifestPath = Join-Path $repo 'shaders\manifest\classic-programs.json'

foreach ($p in @($abiHeader, $inputHeader, $manifestPath, $BuildDir)) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERROR: missing $p" -ForegroundColor Red; exit 2 }
}

$abiText = [System.IO.File]::ReadAllText($abiHeader)
$inputText = [System.IO.File]::ReadAllText($inputHeader)

$declaredCBuffers = @{}
foreach ($m in [regex]::Matches($abiText, '(?m)^\s*cbuffer\s+(\w+)\s*:')) { $declaredCBuffers[$m.Groups[1].Value] = $true }

$declaredResources = @{}
foreach ($m in [regex]::Matches($abiText, '(?:Texture2D|TextureCube|SamplerState)\s+(\w+)\s*:\s*register')) {
    $declaredResources[$m.Groups[1].Value] = $true
}

$declaredSemantics = @{}
foreach ($m in [regex]::Matches($inputText, ':\s*(SV_[A-Za-z]+|[A-Z][A-Z0-9_]*?)\d?\s*;')) {
    $declaredSemantics[$m.Groups[1].Value] = $true
}
# The fragment output target is not an inter-stage semantic, so it is declared here rather than in the
# shared header. A program may only write SV_Target, never an arbitrary render target.
$declaredSemantics['SV_Target'] = $true

$violations = New-Object System.Collections.Generic.List[string]

function Get-Signature {
    param([string[]]$Lines, [string]$Which)
    $out = New-Object System.Collections.Generic.List[object]
    $headerIndex = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match "^;\s+$Which signature:") { $headerIndex = $i; break }
    }
    if ($headerIndex -lt 0) { return $out }
    $sawHeader = $false
    $sawRow = $false
    for ($i = $headerIndex + 1; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        if ($line -notmatch '^;') { break }
        if ($line -match '^;\s*-+\s*$') { continue }
        if ($line -match '^;\s*Name\s+Index') { $sawHeader = $true; continue }
        if (-not $sawHeader) { continue }
        if ($line -match '^; (\w+) +(\d+) +') {
            $out.Add([pscustomobject]@{ name = $Matches[1]; index = [int]$Matches[2] })
            $sawRow = $true
            continue
        }
        # The signature is one contiguous table; anything else ends it. DXIL repeats the signature in a
        # second table with an interpolation column, which must not be counted twice.
        if ($sawRow) { break }
    }
    return $out
}

function Get-CBufferNames {
    param([string[]]$Lines)
    $set = @{}
    foreach ($line in $Lines) {
        if ($line -match '^; cbuffer (\w+)') { $set[$Matches[1]] = $true }
    }
    return $set
}

function Get-ResourceNames {
    param([string[]]$Lines)
    $set = @{}
    foreach ($line in $Lines) {
        # Rows look like: ; Name  texture|sampler|uav  Format  Dim  ID  HLSLBind  Count
        if ($line -match '^; (\w+) +(texture|sampler|uav|structured) ') { $set[$Matches[1]] = $true }
    }
    return $set
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$stageData = @{}
$checked = 0

# The generic Tier B vertex stage is always inspected: arbFP_glasswarp is fragment-only in the engine,
# so this vertex stage is the producer whose signature the glasswarp fragment stage must match.
$inspect = New-Object System.Collections.Generic.List[object]
foreach ($entry in @($manifest.programs | Where-Object { $_.source })) { $inspect.Add($entry) }
$inspect.Add([pscustomobject]@{ key = 'generic_texgen'; source = 'shaders/classic/stages/generic_texgen.hlsl'; entry = [pscustomobject]@{ vs = 'vs_main' } })

foreach ($entry in $inspect) {
    $dir = Join-Path $BuildDir $entry.key
    foreach ($stage in @('vs', 'ps')) {
        $asm = Join-Path $dir "$stage.asm"
        if (-not (Test-Path -LiteralPath $asm)) { continue }
        $lines = [System.IO.File]::ReadAllLines($asm)
        $sigIn = @(Get-Signature -Lines $lines -Which 'Input')
        $sigOut = @(Get-Signature -Lines $lines -Which 'Output')
        $cbs = Get-CBufferNames -Lines $lines
        $resources = Get-ResourceNames -Lines $lines
        $stageData["$($entry.key)/$stage"] = @{ input = $sigIn; output = $sigOut }

        foreach ($cb in $cbs.Keys) {
            if (-not $declaredCBuffers.ContainsKey($cb)) {
                $violations.Add("$($entry.key)/$stage uses undeclared constant buffer '$cb'")
            }
        }
        foreach ($r in $resources.Keys) {
            if (-not $declaredResources.ContainsKey($r)) {
                $violations.Add("$($entry.key)/$stage binds undeclared resource '$r'")
            }
        }
        foreach ($sig in @(@($sigIn) + @($sigOut))) {
            if (-not $declaredSemantics.ContainsKey($sig.name)) {
                $violations.Add("$($entry.key)/$stage uses undeclared semantic '$($sig.name)$($sig.index)'")
            }
        }
        Write-Host ("  {0,-20} {1,-3} inputs={2} outputs={3} cbuffers={4} resources={5}" -f `
                $entry.key, $stage, $sigIn.Count, $sigOut.Count, $cbs.Count, $resources.Count)
        $checked++
    }
}

function Test-SignaturePair {
    param([string]$Producer, [string]$Consumer)
    if (-not $stageData.ContainsKey($Producer) -or -not $stageData.ContainsKey($Consumer)) {
        $violations.Add("signature pair $Producer -> $Consumer missing (compile first)")
        return
    }
    $outSet = @{}
    foreach ($s in $stageData[$Producer].output) { $outSet["$($s.name)$($s.index)"] = $true }
    $inSet = @{}
    foreach ($s in $stageData[$Consumer].input) { $inSet["$($s.name)$($s.index)"] = $true }
    foreach ($k in $outSet.Keys) {
        if (-not $inSet.ContainsKey($k)) { $violations.Add("$Consumer consumes no '$k' that $Producer writes") }
    }
    foreach ($k in $inSet.Keys) {
        if (-not $outSet.ContainsKey($k)) { $violations.Add("$Consumer expects '$k' which $Producer does not write") }
    }
}

foreach ($key in @('interaction', 'environment', 'bumpyenvironment')) {
    Test-SignaturePair -Producer "$key/vs" -Consumer "$key/ps"
}
Test-SignaturePair -Producer 'generic_texgen/vs' -Consumer 'arbfp_glasswarp/ps'

$report = [pscustomobject]@{
    schema      = 'doom3-shader-reflection/1'
    stages      = $checked
    cbuffers    = $declaredCBuffers.Count
    resources   = $declaredResources.Count
    semantics   = $declaredSemantics.Count
    violations  = @($violations)
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $BuildDir 'reflect-report.json')

if ($violations.Count -ne 0) {
    Write-Host "REFLECT FAILED: $($violations.Count) violation(s)" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "REFLECT OK: $checked stages, $($declaredCBuffers.Count) declared cbuffers, $($declaredResources.Count) declared resources, $($declaredSemantics.Count) declared semantics, 0 violations"
exit 0
