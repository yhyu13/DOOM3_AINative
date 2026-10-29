#Requires -Version 5.1
<#
.SYNOPSIS
  Verify a built material package (spec 009, T-014).
.DESCRIPTION
  Re-hashes the package against the repository, checks every texture's format, dimensions, mip chain
  and sRGB flag, recomputes package_sha256 with the rule build-material.ps1 uses, and reconciles the
  packaged channels with the sidecar states. Tiled sources recorded in the package are re-checked
  against the repository (existence, multiples of 4, bounds, overlap, exact coverage), and an
  assembled size must match the texture's width and height. With -Compare it also proves that a
  second build is byte for byte the same package.

  -RepoRoot controls where content/ lives (default: the repository that owns this script).

  Exit 0 = pass, 1 = any mismatch, 2 = environment error.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/verify-package.ps1 -OutDir out/content-a -Compare out/content-b
#>
[CmdletBinding()]
param(
    [string]$OutDir,
    [string]$Compare,
    [string]$RepoRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$script:ChannelNames = @('baseColor', 'normal', 'roughness', 'metalness', 'ao', 'emissive')

trap {
    Write-Host ("ERROR: env-unexpected: {0}" -f $_.Exception.Message) -ForegroundColor Red
    exit 2
}

function Write-Failure {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Detail
    )
    Write-Host ("ERROR: {0}: {1}" -f $Name, $Detail) -ForegroundColor Red
    exit 1
}

function Get-JsonField {
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Get-FullPath {
    param([string]$Path)
    if (-not [System.IO.Path]::IsPathRooted($Path)) {
        $Path = Join-Path (Get-Location).ProviderPath $Path
    }
    $full = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetPathRoot($full)
    if ($full.Length -gt $root.Length) { $full = $full.TrimEnd('\') }
    return $full
}

# The builder's rule, repeated verbatim: sidecar hash, mtr hash, then every texture hash sorted by
# channel name (ordinal), joined with a single LF and no trailing newline.
function Get-PackageSha256 {
    param(
        [Parameter(Mandatory = $true)][string]$SidecarSha256,
        [Parameter(Mandatory = $true)][string]$MtrSha256,
        $Textures
    )
    $byChannel = @{}
    foreach ($texture in @($Textures)) {
        if ($null -eq $texture) { continue }
        # Ordered dictionaries expose their keys through the indexer, not through PSObject.Properties.
        if ($texture -is [System.Collections.IDictionary]) {
            $byChannel[[string]$texture['channel']] = [string]$texture['sha256']
        } else {
            $byChannel[[string](Get-JsonField $texture 'channel')] = [string](Get-JsonField $texture 'sha256')
        }
    }
    $names = [string[]]@($byChannel.Keys)
    [Array]::Sort($names, [System.StringComparer]::Ordinal)
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add($SidecarSha256)
    $lines.Add($MtrSha256)
    foreach ($name in $names) { $lines.Add($byChannel[$name]) }
    $text = [string]::Join("`n", $lines.ToArray())
    $encoding = New-Object System.Text.UTF8Encoding($false)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $bytes = $sha.ComputeHash($encoding.GetBytes($text)) } finally { $sha.Dispose() }
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

# Tile sizes are not recorded in the package (only their origins), so the verifier reads them from the
# PNG sources it is checking. The helper mirrors build-material.ps1: Pillow through pins.json.
function Get-SourceDimensions {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $workDir = Join-Path ([System.IO.Path]::GetTempPath()) ('doom3-verify-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $workDir | Out-Null
    try {
        $helperPath = Join-Path $workDir 'dims.py'
        $requestPath = Join-Path $workDir 'request.json'
        $helperSource = @'
import json, sys

def emit(payload, code):
    sys.stdout.write(json.dumps(payload, sort_keys=True))
    sys.stdout.flush()
    sys.exit(code)

try:
    from PIL import Image
except Exception as exc:
    emit({"envError": "Pillow is not importable: %s" % exc}, 3)

try:
    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        request = json.load(handle)
except Exception as exc:
    emit({"envError": "cannot read request: %s" % exc}, 3)

images = []
for path in request.get("paths", []):
    try:
        with Image.open(path) as image:
            images.append({"path": path, "width": int(image.size[0]), "height": int(image.size[1])})
    except Exception as exc:
        emit({"envError": None, "error": "%s: %s" % (path, exc)}, 4)

emit({"envError": None, "error": None, "images": images}, 0)
'@
        [System.IO.File]::WriteAllText($helperPath, $helperSource, (New-Object System.Text.UTF8Encoding($false)))
        [System.IO.File]::WriteAllText($requestPath, (ConvertTo-Json -InputObject @{ paths = @($Paths) } -Depth 4), (New-Object System.Text.UTF8Encoding($false)))

        $pins = Read-Json (Join-Path $PSScriptRoot 'pins.json')
        $launcher = [string](Get-JsonField (Get-JsonField $pins 'python') 'launcher')
        if ([string]::IsNullOrWhiteSpace($launcher)) { throw 'pins.json records no Python launcher' }
        $parts = @($launcher -split '\s+' | Where-Object { $_ -ne '' })
        $launcherArgs = @()
        if ($parts.Count -gt 1) { $launcherArgs = @($parts[1..($parts.Count - 1)]) }

        $raw = & $parts[0] $launcherArgs $helperPath $requestPath
        $exit = $LASTEXITCODE
        if (-not $raw) { throw "'$launcher' produced no output" }
        $report = ($raw | Out-String) | ConvertFrom-Json
        $envError = [string](Get-JsonField $report 'envError')
        if (-not [string]::IsNullOrWhiteSpace($envError)) { throw $envError }
        if ($exit -ne 0) {
            $errorText = [string](Get-JsonField $report 'error')
            if ([string]::IsNullOrWhiteSpace($errorText)) { $errorText = "exit code $exit" }
            throw $errorText
        }

        $map = @{}
        foreach ($image in @(Get-JsonField $report 'images')) {
            if ($null -eq $image) { continue }
            $map[([string](Get-JsonField $image 'path')).ToLowerInvariant()] = $image
        }
        return $map
    } finally {
        Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Test-TextureSources {
    param(
        $Texture,
        [string]$Channel,
        [string]$Label,
        [int]$Width,
        [int]$Height,
        [string]$RepoRoot
    )

    $assembled = Get-JsonField $Texture 'assembled'
    if ($null -ne $assembled) {
        $assembledWidth = Get-JsonField $assembled 'width'
        $assembledHeight = Get-JsonField $assembled 'height'
        if ($null -eq $assembledWidth -or $null -eq $assembledHeight -or
            [int]$assembledWidth -ne $Width -or [int]$assembledHeight -ne $Height) {
            Write-Failure ($Label + 'assembled-mismatch') "channel '$Channel' records assembled ${assembledWidth}x${assembledHeight}, the texture is ${Width}x${Height}"
        }
    }

    $sources = Get-JsonField $Texture 'sources'
    if ($null -eq $sources) { return }

    # Only tile-shaped entries (they carry x and y) are checked here; a direct source stays {path}.
    $tiled = New-Object System.Collections.Generic.List[object]
    foreach ($source in @($sources)) {
        if ($null -eq $source) { continue }
        if ($null -ne (Get-JsonField $source 'x') -or $null -ne (Get-JsonField $source 'y')) {
            $tiled.Add($source)
        }
    }
    if ($tiled.Count -eq 0) { return }

    $rects = @()
    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($source in $tiled) {
        $relative = [string](Get-JsonField $source 'path')
        $xValue = Get-JsonField $source 'x'
        $yValue = Get-JsonField $source 'y'
        if ([string]::IsNullOrWhiteSpace($relative) -or $null -eq $xValue -or $null -eq $yValue) {
            Write-Failure ($Label + 'source-bounds') "channel '$Channel' has a source with no path, x or y"
        }
        if (-not (Test-RepoRelativePath -RelativePath $relative)) {
            Write-Failure ($Label + 'source-missing') "channel '$Channel' source '$relative' is not a repository-relative path"
        }
        $full = Resolve-RepoPath -RepoRoot $RepoRoot -RelativePath $relative
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            Write-Failure ($Label + 'source-missing') "channel '$Channel' source '$relative' does not exist"
        }
        $paths.Add($full)
        $rects += [pscustomobject]@{ relative = $relative; full = $full; x = [int]$xValue; y = [int]$yValue; width = 0; height = 0 }
    }

    $dimensions = Get-SourceDimensions -Paths $paths.ToArray()
    foreach ($rect in $rects) {
        $image = $dimensions[$rect.full.ToLowerInvariant()]
        if ($null -eq $image) {
            Write-Failure ($Label + 'source-missing') "channel '$Channel' source '$($rect.relative)' could not be read"
        }
        $rect.width = [int](Get-JsonField $image 'width')
        $rect.height = [int](Get-JsonField $image 'height')
        if (($rect.x % 4) -ne 0 -or ($rect.y % 4) -ne 0 -or $rect.x -lt 0 -or $rect.y -lt 0 -or
            ($rect.x + $rect.width) -gt $Width -or ($rect.y + $rect.height) -gt $Height) {
            Write-Failure ($Label + 'source-bounds') "channel '$Channel' source '$($rect.relative)' at ($($rect.x),$($rect.y)) size $($rect.width)x$($rect.height) is not a multiple of 4 inside ${Width}x${Height}"
        }
    }

    for ($i = 0; $i -lt $rects.Count; $i++) {
        for ($j = $i + 1; $j -lt $rects.Count; $j++) {
            $a = $rects[$i]
            $b = $rects[$j]
            if ($a.x -lt ($b.x + $b.width) -and $b.x -lt ($a.x + $a.width) -and
                $a.y -lt ($b.y + $b.height) -and $b.y -lt ($a.y + $a.height)) {
                Write-Failure ($Label + 'source-overlap') "channel '$Channel' sources '$($a.relative)' and '$($b.relative)' overlap"
            }
        }
    }

    $covered = 0
    foreach ($rect in $rects) { $covered += ($rect.width * $rect.height) }
    if ($covered -ne ($Width * $Height)) {
        Write-Failure ($Label + 'source-coverage') "channel '$Channel' sources cover $covered of $($Width * $Height) pixels; a gap or an overflow remains"
    }
}

function Invoke-PackageChecks {
    param(
        [Parameter(Mandatory = $true)][string]$Dir,
        [string]$Label = '',
        [Parameter(Mandatory = $true)][string]$RepoRoot
    )

    $packagePath = Join-Path $Dir 'package.json'
    if (-not (Test-Path -LiteralPath $packagePath)) {
        Write-Failure ($Label + 'package-missing') "no package.json in '$Dir'"
    }
    try { $package = Read-Json $packagePath } catch {
        Write-Failure ($Label + 'package-invalid') $_.Exception.Message
    }

    $schema = [string](Get-JsonField $package 'schema')
    if ($schema -cne 'doom3-content-package/1') {
        Write-Failure ($Label + 'schema-mismatch') "schema is '$schema', expected 'doom3-content-package/1'"
    }
    $material = [string](Get-JsonField $package 'material')
    if ([string]::IsNullOrWhiteSpace($material)) {
        Write-Failure ($Label + 'material-missing') 'the package names no material'
    }
    if ($material -match '[\\/]' -or $material -eq '..') {
        Write-Failure ($Label + 'material-invalid') "material '$material' is not a plain material name"
    }

    # --- 2. sidecar and mtr hashes ---------------------------------------------------------------
    $sidecarPath = Join-Path $RepoRoot ('content\materials\' + $material + '.mat.json')
    if (-not (Test-Path -LiteralPath $sidecarPath)) {
        Write-Failure ($Label + 'sidecar-missing') "no sidecar at content/materials/$material.mat.json"
    }
    try { $sidecar = Read-Json $sidecarPath } catch {
        Write-Failure ($Label + 'sidecar-invalid') $_.Exception.Message
    }
    $sidecarMaterial = [string](Get-JsonField $sidecar 'material')
    if ($sidecarMaterial -cne $material) {
        Write-Failure ($Label + 'sidecar-material-mismatch') "sidecar says '$sidecarMaterial', the package says '$material'"
    }

    $sidecarHash = Get-Sha256Normalized $sidecarPath
    $recordedSidecarHash = [string](Get-JsonField $package 'sidecar_sha256')
    if ($recordedSidecarHash.ToLowerInvariant() -ne $sidecarHash) {
        Write-Failure ($Label + 'sidecar-sha256-mismatch') "recorded $recordedSidecarHash, content/materials/$material.mat.json hashes to $sidecarHash"
    }

    $mtrRelative = [string](Get-JsonField $sidecar 'mtr')
    if ([string]::IsNullOrWhiteSpace($mtrRelative) -or -not (Test-RepoRelativePath -RelativePath $mtrRelative)) {
        Write-Failure ($Label + 'mtr-path-invalid') "sidecar mtr '$mtrRelative' is not a repository-relative path"
    }
    $mtrPath = Resolve-RepoPath -RepoRoot $RepoRoot -RelativePath $mtrRelative
    if (-not (Test-Path -LiteralPath $mtrPath)) {
        Write-Failure ($Label + 'mtr-missing') "mtr '$mtrRelative' does not exist"
    }
    $mtrHash = Get-Sha256Normalized $mtrPath
    $recordedMtrHash = [string](Get-JsonField $package 'mtr_sha256')
    if ($recordedMtrHash.ToLowerInvariant() -ne $mtrHash) {
        Write-Failure ($Label + 'mtr-sha256-mismatch') "recorded $recordedMtrHash, $mtrRelative hashes to $mtrHash"
    }

    # --- 3. every texture ------------------------------------------------------------------------
    $records = @()
    $channelHashes = @{}
    foreach ($texture in @(Get-JsonField $package 'textures')) {
        if ($null -eq $texture) {
            Write-Failure ($Label + 'package-invalid-field') 'textures[] holds a null entry'
        }
        $channel = [string](Get-JsonField $texture 'channel')
        if ([string]::IsNullOrWhiteSpace($channel)) {
            Write-Failure ($Label + 'package-invalid-field') 'a textures[] entry names no channel'
        }
        if ($script:ChannelNames -cnotcontains $channel) {
            Write-Failure ($Label + 'texture-unknown-channel') "textures[] holds unknown channel '$channel'"
        }
        if ($channelHashes.ContainsKey($channel)) {
            Write-Failure ($Label + 'texture-duplicate-channel') "channel '$channel' appears twice in textures[]"
        }

        $file = [string](Get-JsonField $texture 'file')
        if ([string]::IsNullOrWhiteSpace($file) -or $file -match '[\\/]') {
            Write-Failure ($Label + 'texture-file-invalid') "channel '$channel' names '$file', which is not a bare file name"
        }
        $ddsPath = Join-Path $Dir $file
        if (-not (Test-Path -LiteralPath $ddsPath)) {
            Write-Failure ($Label + 'texture-missing') "channel '$channel' file '$file' does not exist in '$Dir'"
        }

        $recordedHash = [string](Get-JsonField $texture 'sha256')
        $actualHash = Get-Sha256 $ddsPath
        if ($recordedHash.ToLowerInvariant() -ne $actualHash) {
            Write-Failure ($Label + 'texture-sha256-mismatch') "channel '$channel' recorded $recordedHash, '$file' hashes to $actualHash"
        }

        try { $info = Get-DdsInfo $ddsPath } catch {
            Write-Failure ($Label + 'texture-dds-unreadable') "channel '$channel': $($_.Exception.Message)"
        }

        foreach ($field in @('format', 'width', 'height', 'mips', 'srgb')) {
            if ($null -eq (Get-JsonField $texture $field)) {
                Write-Failure ($Label + 'package-invalid-field') "channel '$channel' records no '$field'"
            }
        }
        $recordedFormat = [string](Get-JsonField $texture 'format')
        if ($recordedFormat -cne $info.format) {
            Write-Failure ($Label + 'texture-format-mismatch') "channel '$channel' records $recordedFormat, '$file' is $($info.format)"
        }
        $recordedWidth = [int](Get-JsonField $texture 'width')
        $recordedHeight = [int](Get-JsonField $texture 'height')
        if ($recordedWidth -ne $info.width -or $recordedHeight -ne $info.height) {
            Write-Failure ($Label + 'texture-dimension-mismatch') "channel '$channel' records ${recordedWidth}x${recordedHeight}, '$file' is $($info.width)x$($info.height)"
        }
        $recordedSrgb = [bool](Get-JsonField $texture 'srgb')
        if ($recordedSrgb -ne $info.srgb) {
            Write-Failure ($Label + 'texture-srgb-mismatch') "channel '$channel' records srgb=$recordedSrgb, '$file' is srgb=$($info.srgb)"
        }
        $expectedMips = Get-ExpectedMipCount $info.width
        $recordedMips = [int](Get-JsonField $texture 'mips')
        if ($recordedMips -ne $info.mips) {
            Write-Failure ($Label + 'texture-mips-mismatch') "channel '$channel' records $recordedMips mips, '$file' holds $($info.mips)"
        }
        if ($info.mips -ne $expectedMips) {
            Write-Failure ($Label + 'texture-mips-mismatch') "channel '$channel' holds $($info.mips) mips, a full chain for $($info.width) px needs $expectedMips"
        }

        Test-TextureSources -Texture $texture -Channel $channel -Label $Label -Width $recordedWidth -Height $recordedHeight -RepoRoot $RepoRoot

        $channelHashes[$channel] = $actualHash
        $records += [pscustomobject]@{ channel = $channel; sha256 = $actualHash }
    }

    # --- 4. package_sha256 ------------------------------------------------------------------------
    $recordedPackageHash = [string](Get-JsonField $package 'package_sha256')
    $computedPackageHash = Get-PackageSha256 -SidecarSha256 $sidecarHash -MtrSha256 $mtrHash -Textures $records
    if ($recordedPackageHash.ToLowerInvariant() -ne $computedPackageHash) {
        Write-Failure ($Label + 'package-sha256-mismatch') "recorded $recordedPackageHash, the sidecar/mtr/texture hashes give $computedPackageHash"
    }

    # --- 5. channels agree with the sidecar -------------------------------------------------------
    $declared = Get-JsonField $sidecar 'channels'
    if ($null -eq $declared) {
        Write-Failure ($Label + 'sidecar-channels-missing') "sidecar '$material' has no channels object"
    }
    foreach ($channel in $script:ChannelNames) {
        $node = Get-JsonField $declared $channel
        if ($null -eq $node) {
            Write-Failure ($Label + 'sidecar-channel-missing') "sidecar '$material' declares no state for channel '$channel'"
        }
        $state = [string](Get-JsonField $node 'state')
        $packaged = $channelHashes.ContainsKey($channel)
        if ($state -ceq 'authored' -or ($state.StartsWith('derived:', [System.StringComparison]::Ordinal) -and $state.Length -gt 8)) {
            if (-not $packaged) {
                Write-Failure ($Label + 'texture-channel-missing') "sidecar declares channel '$channel' as $state, the package has no texture for it"
            }
        } elseif ($state -ceq 'unavailable') {
            if ($packaged) {
                Write-Failure ($Label + 'texture-channel-unexpected') "sidecar declares channel '$channel' as unavailable, the package ships a texture for it"
            }
        } else {
            Write-Failure ($Label + 'sidecar-channel-state') "channel '$channel' has state '$state'; expected authored, derived:<method> or unavailable"
        }
    }
    foreach ($channel in $channelHashes.Keys) {
        if ($null -eq (Get-JsonField $declared $channel)) {
            Write-Failure ($Label + 'texture-channel-undeclared') "the package holds channel '$channel', the sidecar does not declare it"
        }
    }

    return [pscustomobject]@{
        dir           = $Dir
        material      = $material
        packageSha256 = $computedPackageHash
        channels      = $channelHashes
        count         = $records.Count
    }
}

if ([string]::IsNullOrWhiteSpace($OutDir)) { Write-Failure 'outdir-missing' '-OutDir is required' }

try { $gitRoot = Get-RepoRoot -Start $PSScriptRoot } catch { Write-Failure 'env-repo-root' $_.Exception.Message }
if (-not [string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Get-FullPath $RepoRoot
} else {
    $RepoRoot = $gitRoot
}
$primary = Invoke-PackageChecks -Dir (Get-FullPath $OutDir) -Label '' -RepoRoot $RepoRoot

if ([string]::IsNullOrWhiteSpace($Compare)) {
    Write-Host ("VERIFY OK: {0} {1} textures" -f $primary.material, $primary.count)
    exit 0
}

$compareDir = Get-FullPath $Compare
# Comparing a package with itself is not a determinism test: it would always pass. A real check needs a
# second, independent build.
if ($compareDir.TrimEnd([char[]]@('\', '/')) -ieq (Get-FullPath $OutDir).TrimEnd([char[]]@('\', '/'))) {
    Write-Failure 'compare-same-directory' "-Compare must point at a second build, not at -OutDir"
}
$secondary = Invoke-PackageChecks -Dir $compareDir -Label 'compare-' -RepoRoot $RepoRoot

if ($primary.material -cne $secondary.material) {
    Write-Failure 'compare-material' "'$($primary.material)' and '$($secondary.material)' are different materials"
}
$primaryNames = [string[]]@($primary.channels.Keys)
$secondaryNames = [string[]]@($secondary.channels.Keys)
[Array]::Sort($primaryNames, [System.StringComparer]::Ordinal)
[Array]::Sort($secondaryNames, [System.StringComparer]::Ordinal)
if ([string]::Join(',', $primaryNames) -cne [string]::Join(',', $secondaryNames)) {
    Write-Failure 'compare-channels' "channel sets differ: [$([string]::Join(', ', $primaryNames))] vs [$([string]::Join(', ', $secondaryNames))]"
}
foreach ($name in $primaryNames) {
    if ($primary.channels[$name] -cne $secondary.channels[$name]) {
        Write-Failure 'compare-texture-sha256' "channel '$name' differs: $($primary.channels[$name]) vs $($secondary.channels[$name])"
    }
}
if ($primary.packageSha256 -cne $secondary.packageSha256) {
    Write-Failure 'compare-package-sha256' "$($primary.packageSha256) vs $($secondary.packageSha256)"
}

Write-Host ("VERIFY OK: {0} {1} textures (deterministic against {2})" -f $primary.material, $primary.count, $compareDir)
exit 0
