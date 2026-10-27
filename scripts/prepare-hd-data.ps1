#Requires -Version 5.1
<#
.SYNOPSIS
  Build an untracked run directory with the free demo data plus the Wulfen HD texture pack.
.DESCRIPTION
  Input : demo00.pk4 from the official free demo, and wulfen_texture_pack_2.0.pk4 from ModDB.
  Output: <RunDir>\base\demo00.pk4, the two demo compatibility shims (via prepare-demo-data.ps1),
          and <RunDir>\base\zzz_wulfen_texture_pack_2.0.pk4. The zzz_ prefix makes the pack sort
          after demo00.pk4, which is how the engine gives later alphabetic pk4 files priority.
          A small hd-manifest.txt records sizes, SHA-256 and the staged order.

  The pack is proprietary and the demo is non-redistributable. Both are LOCAL TEST DATA: never
  commit them, never put them under a tracked path. This script only links/copies them.
.EXAMPLE
  .\scripts\prepare-hd-data.ps1 -DemoPk4 <data-root>\demo00.pk4 -HdPk4 <data-root>\wulfen_texture_pack_2.0.pk4 -RunDir <data-root>\run-hd
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DemoPk4,
    [Parameter(Mandatory = $true)][string]$HdPk4,
    [Parameter(Mandatory = $true)][string]$RunDir,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$RunDir = [IO.Path]::GetFullPath($RunDir).TrimEnd('\', '/')

function Fail([string]$Message, [int]$Code = 1) {
    Write-Host "ERROR $Message" -ForegroundColor Red
    exit $Code
}
function Get-Sha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }

foreach ($p in @($DemoPk4, $HdPk4)) {
    if (-not (Test-Path -LiteralPath $p)) { Fail "pk4 not found: $p" 2 }
    if ((Get-Item -LiteralPath $p).Length -le 0) { Fail "pk4 is empty: $p" 2 }
}
if ($DemoPk4 -ieq $HdPk4) { Fail "-DemoPk4 and -HdPk4 are the same file" 2 }

# Prove both inputs are readable ZIP archives before touching the run directory.
function Test-Pk4([string]$Path, [string]$Label) {
    try {
        $z = [IO.Compression.ZipFile]::OpenRead($Path)
    } catch {
        Fail "$Label is not a readable pk4/zip: $Path ($($_.Exception.Message))" 2
    }
    try {
        $entries = @($z.Entries)
        if ($entries.Count -eq 0) { Fail "$Label archive has no entries: $Path" 2 }
        return $entries
    } finally { $z.Dispose() }
}
$hdEntries = @(Test-Pk4 $HdPk4 'HD pack')
[void](Test-Pk4 $DemoPk4 'demo pack')

$base = Join-Path $RunDir 'base'
New-Item -ItemType Directory -Force -Path $base | Out-Null

# Reuse the reviewed demo staging logic (demo00.pk4 hard link + two local shims).
$prep = Join-Path $PSScriptRoot 'prepare-demo-data.ps1'
& powershell -NoProfile -ExecutionPolicy Bypass -File $prep -Pk4 $DemoPk4 -RunDir $RunDir
if ($LASTEXITCODE -ne 0) { Fail "prepare-demo-data.ps1 failed ($LASTEXITCODE)" 1 }

$stageName = 'zzz_wulfen_texture_pack_2.0.pk4'
$stagePath = Join-Path $base $stageName
$hdLength = (Get-Item -LiteralPath $HdPk4).Length
if (Test-Path -LiteralPath $stagePath) {
    $existing = (Get-Item -LiteralPath $stagePath).Length
    $existingHash = Get-Sha256 $stagePath
    $sourceHash = Get-Sha256 $HdPk4
    if ($existing -eq $hdLength -and $existingHash -eq $sourceHash -and -not $Force) {
        Write-Host "REUSING $stagePath ($existing bytes)"
    } else {
        if (-not $Force) { Fail "$stagePath differs from the requested pack; pass -Force" 1 }
        Remove-Item -LiteralPath $stagePath -Force
    }
}
if (-not (Test-Path -LiteralPath $stagePath)) {
    try {
        New-Item -ItemType HardLink -Path $stagePath -Target $HdPk4 -ErrorAction Stop | Out-Null
    } catch {
        Write-Host "NOTE hard link unavailable ($($_.Exception.Message)); copying 814 MiB instead"
        Copy-Item -LiteralPath $HdPk4 -Destination $stagePath
    }
}
$stagedLength = (Get-Item -LiteralPath $stagePath).Length
if ($stagedLength -ne $hdLength) { Fail "staged pack size mismatch: $stagedLength != $hdLength" 1 }

# Summarise what the pack will override; makes shader/material surprises visible before the run.
$groups = [ordered]@{
    'textures'  = 0
    'materials' = 0
    'glprogs'   = 0
    'models'    = 0
    'other'     = 0
}
$fileEntries = 0
foreach ($e in $hdEntries) {
    if ($e.FullName.EndsWith('/')) { continue }
    $fileEntries++
    $n = $e.FullName.ToLowerInvariant()
    if ($n -like 'textures/*') { $groups['textures']++ }
    elseif ($n -like 'materials/*') { $groups['materials']++ }
    elseif ($n -like 'glprogs/*' -or $n -like '*.vfp' -or $n -like '*.vp' -or $n -like '*.fp') { $groups['glprogs']++ }
    elseif ($n -like 'models/*') { $groups['models']++ }
    else { $groups['other']++ }
}

$demoStaged = Join-Path $base 'demo00.pk4'
$demoSourceHash = Get-Sha256 $DemoPk4
if (-not (Test-Path -LiteralPath $demoStaged)) { Fail "staged demo00.pk4 missing: $demoStaged" 1 }
$demoStagedHash = Get-Sha256 $demoStaged
if ($demoStagedHash -ne $demoSourceHash) {
    Fail "staged demo00.pk4 hash differs from -DemoPk4; remove $RunDir and restage" 1
}
$hdStagedHash = Get-Sha256 $stagePath
if ($hdStagedHash -ne (Get-Sha256 $HdPk4)) { Fail "staged HD pack hash differs from -HdPk4" 1 }

$manifest = Join-Path $RunDir 'hd-manifest.txt'
$lines = @(
    "created=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "demo00.source=$DemoPk4",
    "demo00.sha256=$demoSourceHash",
    "demo00.staged_sha256=$demoStagedHash",
    "hd.source=$HdPk4",
    "hd.sha256=$(Get-Sha256 $HdPk4)",
    "hd.staged_sha256=$hdStagedHash",
    "hd.length=$hdLength",
    "hd.staged=$stageName",
    "hd.staged_path=$stagePath",
    "hd.entries=$($hdEntries.Count)",
    "hd.files=$fileEntries",
    "hd.textures=$($groups['textures'])",
    "hd.materials=$($groups['materials'])",
    "hd.glprogs=$($groups['glprogs'])",
    "hd.models=$($groups['models'])",
    "hd.other=$($groups['other'])",
    "search_order=demo00.pk4 before zzz_wulfen_texture_pack_2.0.pk4 (later alphabetic wins)",
    "required_cvars=image_usePrecompressedTextures=0; image_downSize=0; image_downSizeBump=0; image_downSizeSpecular=0 (scripts/run-win11-amd64.ps1 -HdTextures)"
)
[IO.File]::WriteAllLines($manifest, $lines, (New-Object Text.UTF8Encoding($false)))
Write-Host ("PREPARED {0} : demo + {1} ({2} bytes, {3} entries, {4} files; textures={5} materials={6} models={7} glprogs={8})" -f `
    $RunDir, $stageName, $stagedLength, $hdEntries.Count, $fileEntries, $groups['textures'], $groups['materials'], $groups['models'], $groups['glprogs'])
Write-Host "MANIFEST $manifest"
exit 0
