#Requires -Version 5.1
<#
.SYNOPSIS
  Build one material into BC7/BC5/BC4 DDS textures plus a package manifest (spec 009, T-013).
.DESCRIPTION
  Reads the Q4 material set (content/manifest/q4-materials.json), the material sidecar
  (content/materials/<name>.mat.json) and the source images it names, encodes every declared channel
  with the pinned texconv, and writes <OutDir>/package.json.

  Contract (spec 009 par.4.2, 4.5):
    - the material must be listed in the Q4 set, and the sidecar must agree with that entry;
    - each of baseColor, normal, roughness, metalness, ao, emissive must carry an explicit state of
      authored, derived:<method> or unavailable; unavailable needs a why and produces no texture;
    - a channel declares either path (one source) or assembly (a tiled canvas); declaring both is an
      error;
    - every declared source must be a PNG, square, a multiple of 4 on each axis and <= 2048 px;
      assembly tiles follow the same rule, sit on multiples of 4, stay in bounds, do not overlap and
      cover the declared canvas exactly;
    - format flags come from pins.json, never from this script;
    - package_sha256 is SHA-256 over the sidecar hash, the .mtr hash and every texture hash sorted by
      channel name, joined with a single LF and no trailing newline.

  -RepoRoot controls where content/ lives and defaults to the repository that owns this script, so a
  fixture tree can be built without touching the authored material tree. pins.json is toolchain policy
  and is always read from this script's directory.

  Exit 0 = package built, 1 = contract violation (nothing built), 2 = environment error.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content-a
#>
[CmdletBinding()]
param(
    [string]$Material,
    [string]$OutDir,
    [string]$RepoRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$script:ChannelNames = @('baseColor', 'normal', 'roughness', 'metalness', 'ao', 'emissive')
$script:TempDirs = New-Object System.Collections.Generic.List[string]

function Write-Failure {
    param(
        [Parameter(Mandatory = $true)][int]$Code,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Detail
    )
    foreach ($dir in $script:TempDirs) {
        if ($dir -and (Test-Path -LiteralPath $dir)) {
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    Write-Host ("ERROR: {0}: {1}" -f $Name, $Detail) -ForegroundColor Red
    exit $Code
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
    # Keep a root path intact (a bare drive or a leading separator), trim the trailing separator otherwise.
    $root = [System.IO.Path]::GetPathRoot($full)
    if ($full.Length -gt $root.Length) { $full = $full.TrimEnd('\') }
    return $full
}

function Test-PathUnder {
    param([string]$Path, [string]$Root)
    $candidate = $Path.TrimEnd('\')
    $base = $Root.TrimEnd('\')
    if ($candidate.Equals($base, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    return $candidate.StartsWith($base + '\', [System.StringComparison]::OrdinalIgnoreCase)
}

# True/false for an ignored path, throws when git itself cannot answer.
function Test-IgnoredPath {
    param([string]$Path, [string]$GitRoot)
    Push-Location -LiteralPath $GitRoot
    try {
        & git check-ignore --quiet -- $Path 2>$null
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($code -eq 0) { return $true }
    if ($code -eq 1) { return $false }
    throw "git check-ignore exited with $code"
}

function Get-FormatFlag {
    param($Flags)
    $list = @($Flags)
    for ($i = 0; $i -lt $list.Count; $i++) {
        if ([string]$list[$i] -ceq '-f' -and ($i + 1) -lt $list.Count) { return [string]$list[$i + 1] }
    }
    return $null
}

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

function Get-LogTail {
    param([string]$Text, [int]$Lines = 6)
    if ([string]::IsNullOrWhiteSpace($Text)) { return '(no output)' }
    $rows = @($Text.Trim() -split "`r?`n" | Where-Object { $_.Trim() -ne '' })
    return (@($rows | Select-Object -Last $Lines) -join ' | ')
}

# Geometry for a tiled channel. Bounds and pairwise non-overlap plus an exact area sum is enough to
# prove exact coverage: a rectangle that is inside the canvas and does not overlap another one covers
# only canvas pixels, so equal areas mean no gap and no overflow remains.
function Test-AssemblyTiles {
    param(
        [Parameter(Mandatory = $true)][string]$Channel,
        [Parameter(Mandatory = $true)]$Assembly,
        [Parameter(Mandatory = $true)]$Tiles
    )
    $rects = @()
    foreach ($tile in $Tiles) {
        if (($tile.x % 4) -ne 0 -or ($tile.y % 4) -ne 0) {
            Write-Failure 1 'assembly-alignment' "channel '$Channel' tile '$($tile.relative)' is at ($($tile.x),$($tile.y)); x and y must be multiples of 4"
        }
        if ($tile.x -lt 0 -or $tile.y -lt 0 -or
            ($tile.x + $tile.width) -gt $Assembly.width -or ($tile.y + $tile.height) -gt $Assembly.height) {
            Write-Failure 1 'assembly-bounds' "channel '$Channel' tile '$($tile.relative)' at ($($tile.x),$($tile.y)) size $($tile.width)x$($tile.height) leaves the $($Assembly.width)x$($Assembly.height) canvas"
        }
        $rects += [pscustomobject]@{ x = $tile.x; y = $tile.y; width = $tile.width; height = $tile.height }
    }
    for ($i = 0; $i -lt $rects.Count; $i++) {
        for ($j = $i + 1; $j -lt $rects.Count; $j++) {
            $a = $rects[$i]
            $b = $rects[$j]
            if ($a.x -lt ($b.x + $b.width) -and $b.x -lt ($a.x + $a.width) -and
                $a.y -lt ($b.y + $b.height) -and $b.y -lt ($a.y + $a.height)) {
                Write-Failure 1 'assembly-overlap' "channel '$Channel' tiles at ($($a.x),$($a.y)) and ($($b.x),$($b.y)) overlap"
            }
        }
    }
    $covered = 0
    foreach ($rect in $rects) { $covered += ($rect.width * $rect.height) }
    $canvas = $Assembly.width * $Assembly.height
    if ($covered -ne $canvas) {
        Write-Failure 1 'assembly-gap' "channel '$Channel' tiles cover $covered of $canvas pixels; a gap or an overflow remains"
    }
}

<#
  A fixture root is not a git repository, so "inside a tracked path" is always decided against the
  repository that owns these tools, not against -RepoRoot.
#>
try { $gitRoot = Get-RepoRoot -Start $PSScriptRoot } catch { Write-Failure 2 'env-repo-root' $_.Exception.Message }

if ([string]::IsNullOrWhiteSpace($Material)) { Write-Failure 1 'material-missing' '-Material is required' }
if ([string]::IsNullOrWhiteSpace($OutDir)) { Write-Failure 1 'outdir-missing' '-OutDir is required' }

$Material = $Material.Trim()
$OutDir = Get-FullPath $OutDir

if ($OutDir -ieq $gitRoot -or (Test-PathUnder $gitRoot $OutDir)) {
    Write-Failure 1 'outdir-forbidden' "OutDir '$OutDir' is the repository root or an ancestor of it"
}
if (Test-PathUnder $OutDir $gitRoot) {
    try {
        $ignored = Test-IgnoredPath -Path $OutDir -GitRoot $gitRoot
    } catch {
        Write-Failure 2 'env-git' "cannot decide whether '$OutDir' is tracked: $($_.Exception.Message)"
    }
    if (-not $ignored) {
        Write-Failure 1 'outdir-tracked' "OutDir '$OutDir' is inside the tracked tree; build under a git-ignored path such as out/content"
    }
}

if (-not [string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Get-FullPath $RepoRoot
    if (-not (Test-Path -LiteralPath $RepoRoot)) {
        Write-Failure 1 'reporoot-missing' "content root '$RepoRoot' does not exist"
    }
} else {
    $RepoRoot = $gitRoot
}
$contentRoot = Join-Path $RepoRoot 'content'

if ((Test-Path -LiteralPath $OutDir) -and -not (Test-Path -LiteralPath $OutDir -PathType Container)) {
    Write-Failure 1 'outdir-not-directory' "OutDir '$OutDir' exists and is not a directory"
}
if ((Test-Path -LiteralPath $OutDir) -and -not $Force) {
    $existing = @(Get-ChildItem -LiteralPath $OutDir -Force | Select-Object -First 1)
    if ($existing.Count -gt 0) {
        Write-Failure 1 'outdir-not-empty' "OutDir '$OutDir' already holds files; pass -Force to replace them"
    }
}

$pinsPath = Join-Path $PSScriptRoot 'pins.json'
if (-not (Test-Path -LiteralPath $pinsPath)) { Write-Failure 2 'env-pins' "missing $pinsPath" }
try { $pins = Read-Json $pinsPath } catch { Write-Failure 2 'env-pins' $_.Exception.Message }

$q4Path = Join-Path $contentRoot 'manifest\q4-materials.json'
if (-not (Test-Path -LiteralPath $q4Path)) {
    Write-Failure 1 'q4-set-missing' "no Q4 material set at $q4Path"
}
try { $q4 = Read-Json $q4Path } catch { Write-Failure 1 'q4-set-invalid' $_.Exception.Message }

$q4Entry = $null
foreach ($entry in @(Get-JsonField $q4 'materials')) {
    if ($null -eq $entry) { continue }
    if ([string](Get-JsonField $entry 'material') -ceq $Material) { $q4Entry = $entry; break }
}
if ($null -eq $q4Entry) {
    Write-Failure 1 'material-not-in-q4-set' "material '$Material' is not listed in content/manifest/q4-materials.json"
}
$q4Mtr = [string](Get-JsonField $q4Entry 'mtr')
if ([string]::IsNullOrWhiteSpace($q4Mtr)) {
    Write-Failure 1 'q4-set-invalid' "the Q4 entry for '$Material' has no mtr path"
}

$sidecarPath = Join-Path $contentRoot ("materials\" + $Material + '.mat.json')
if (-not (Test-Path -LiteralPath $sidecarPath)) {
    Write-Failure 1 'sidecar-missing' "no sidecar for '$Material' at content/materials/$Material.mat.json"
}
try { $sidecar = Read-Json $sidecarPath } catch { Write-Failure 1 'sidecar-invalid' $_.Exception.Message }

$sidecarMaterial = [string](Get-JsonField $sidecar 'material')
if ($sidecarMaterial -cne $Material) {
    Write-Failure 1 'sidecar-material-mismatch' "sidecar says material '$sidecarMaterial', the build asked for '$Material'"
}
$sidecarMtr = [string](Get-JsonField $sidecar 'mtr')
if ($sidecarMtr -cne $q4Mtr) {
    Write-Failure 1 'sidecar-mtr-mismatch' "sidecar says mtr '$sidecarMtr', the Q4 set says '$q4Mtr'"
}

$sidecarHash = Get-Sha256Normalized $sidecarPath
if (-not (Test-RepoRelativePath -RelativePath $q4Mtr)) {
    Write-Failure 1 'mtr-path-invalid' "mtr path '$q4Mtr' is not a repository-relative path"
}
$mtrPath = Resolve-RepoPath -RepoRoot $RepoRoot -RelativePath $q4Mtr
if (-not (Test-Path -LiteralPath $mtrPath)) {
    Write-Failure 1 'mtr-missing' "mtr '$q4Mtr' does not exist"
}
$mtrHash = Get-Sha256Normalized $mtrPath

$declared = Get-JsonField $sidecar 'channels'
if ($null -eq $declared) {
    Write-Failure 1 'channels-missing' "sidecar '$Material' has no channels object"
}

# A sidecar channel this pipeline cannot package is reported, not silently dropped.
foreach ($property in $declared.PSObject.Properties) {
    if ($script:ChannelNames -cnotcontains $property.Name) {
        Write-Host ("WARNING: sidecar declares unsupported channel '{0}'; it is not packaged" -f $property.Name) -ForegroundColor Yellow
    }
}

$channels = New-Object System.Collections.Generic.List[object]
foreach ($name in $script:ChannelNames) {
    $node = Get-JsonField $declared $name
    if ($null -eq $node) {
        Write-Failure 1 'channel-missing' "sidecar '$Material' declares no state for channel '$name'"
    }
    $state = [string](Get-JsonField $node 'state')
    if ([string]::IsNullOrWhiteSpace($state)) {
        Write-Failure 1 'channel-state-missing' "channel '$name' has no state (authored, derived:<method> or unavailable)"
    }
    $kind = $null
    if ($state -ceq 'authored') { $kind = 'authored' }
    elseif ($state.StartsWith('derived:', [System.StringComparison]::Ordinal) -and $state.Length -gt 8) { $kind = 'derived' }
    elseif ($state -ceq 'unavailable') { $kind = 'unavailable' }
    else {
        Write-Failure 1 'channel-state-invalid' "channel '$name' has state '$state'; expected authored, derived:<method> or unavailable"
    }

    if ($kind -eq 'unavailable') {
        $why = [string](Get-JsonField $node 'why')
        if ([string]::IsNullOrWhiteSpace($why)) {
            Write-Failure 1 'channel-why-missing' "channel '$name' is unavailable but gives no why"
        }
        $channels.Add([pscustomobject]@{ name = $name; state = $state; kind = $kind; relative = $null; full = $null; assembly = $null; tiles = $null })
        continue
    }

    $relative = [string](Get-JsonField $node 'path')
    $hasPath = -not [string]::IsNullOrWhiteSpace($relative)
    $assemblyNode = Get-JsonField $node 'assembly'
    $hasAssembly = $null -ne $assemblyNode
    if ($hasPath -and $hasAssembly) {
        Write-Failure 1 'channel-source-ambiguous' "channel '$name' declares both path and assembly; choose one"
    }
    if (-not $hasPath -and -not $hasAssembly) {
        Write-Failure 1 'channel-path-missing' "channel '$name' is $kind but names no path or assembly"
    }
    if ($name -eq 'normal') {
        # Spec edge case: a normal map's handedness is not inferable from the image, so an undeclared
        # one would be a silent axis flip. Require it here rather than hoping the renderer guesses.
        $handedness = [string](Get-JsonField $node 'handedness')
        if ([string]::IsNullOrWhiteSpace($handedness)) {
            Write-Failure 1 'channel-handedness-missing' "channel 'normal' is $kind but declares no handedness (opengl or directx)"
        }
        if ($handedness -cne 'opengl' -and $handedness -cne 'directx') {
            Write-Failure 1 'channel-handedness-invalid' "channel 'normal' handedness '$handedness' is neither opengl nor directx"
        }
    }

    if ($hasPath) {
        if (-not (Test-RepoRelativePath -RelativePath $relative)) {
            Write-Failure 1 'channel-path-invalid' "channel '$name' path '$relative' is not a repository-relative path"
        }
        $full = Resolve-RepoPath -RepoRoot $RepoRoot -RelativePath $relative
        if (-not (Test-Path -LiteralPath $full)) {
            Write-Failure 1 'channel-source-missing' "channel '$name' source '$relative' does not exist"
        }
        if ([System.IO.Path]::GetExtension($full) -ine '.png') {
            Write-Failure 1 'channel-source-format' "channel '$name' source '$relative' is not a PNG; only PNG sources are supported"
        }
        $channels.Add([pscustomobject]@{ name = $name; state = $state; kind = $kind; relative = $relative; full = $full; assembly = $null; tiles = $null })
        continue
    }

    $assemblyWidthValue = Get-JsonField $assemblyNode 'width'
    $assemblyHeightValue = Get-JsonField $assemblyNode 'height'
    if ($null -eq $assemblyWidthValue -or $null -eq $assemblyHeightValue) {
        Write-Failure 1 'assembly-invalid' "channel '$name' assembly declares no width and height"
    }
    $assemblyWidth = [int]$assemblyWidthValue
    $assemblyHeight = [int]$assemblyHeightValue
    if ($assemblyWidth -le 0 -or $assemblyHeight -le 0 -or $assemblyWidth -gt 8192 -or $assemblyHeight -gt 8192 -or
        ($assemblyWidth % 4) -ne 0 -or ($assemblyHeight % 4) -ne 0) {
        Write-Failure 1 'assembly-dimension' "channel '$name' assembly is ${assemblyWidth}x${assemblyHeight}; width and height must be positive multiples of 4 up to 8192"
    }
    $tileNodes = @(Get-JsonField $assemblyNode 'tiles')
    if ($tileNodes.Count -eq 0) {
        Write-Failure 1 'assembly-invalid' "channel '$name' assembly declares no tiles"
    }
    $tiles = New-Object System.Collections.Generic.List[object]
    foreach ($tileNode in $tileNodes) {
        if ($null -eq $tileNode) { Write-Failure 1 'assembly-invalid' "channel '$name' assembly holds a null tile" }
        $tileRelative = [string](Get-JsonField $tileNode 'path')
        $tileXValue = Get-JsonField $tileNode 'x'
        $tileYValue = Get-JsonField $tileNode 'y'
        if ([string]::IsNullOrWhiteSpace($tileRelative) -or $null -eq $tileXValue -or $null -eq $tileYValue) {
            Write-Failure 1 'assembly-tile-invalid' "channel '$name' has a tile with no path, x or y"
        }
        if (-not (Test-RepoRelativePath -RelativePath $tileRelative)) {
            Write-Failure 1 'assembly-tile-invalid' "channel '$name' tile path '$tileRelative' is not a repository-relative path"
        }
        $tileFull = Resolve-RepoPath -RepoRoot $RepoRoot -RelativePath $tileRelative
        if (-not (Test-Path -LiteralPath $tileFull)) {
            Write-Failure 1 'channel-source-missing' "channel '$name' tile '$tileRelative' does not exist"
        }
        if ([System.IO.Path]::GetExtension($tileFull) -ine '.png') {
            Write-Failure 1 'channel-source-format' "channel '$name' tile '$tileRelative' is not a PNG; only PNG sources are supported"
        }
        $tiles.Add([pscustomobject]@{ relative = $tileRelative; full = $tileFull; x = [int]$tileXValue; y = [int]$tileYValue; width = 0; height = 0 })
    }
    $assembly = [pscustomobject]@{ width = $assemblyWidth; height = $assemblyHeight }
    $channels.Add([pscustomobject]@{ name = $name; state = $state; kind = $kind; relative = $null; full = $null; assembly = $assembly; tiles = $tiles })
}

# --- source validation: dimensions come from Pillow, never from parsing the PNG -------------------
$sources = New-Object System.Collections.Generic.List[string]
$seen = @{}
function Add-Source {
    param([string]$Path)
    $key = $Path.ToLowerInvariant()
    if ($seen.ContainsKey($key)) { return }
    $seen[$key] = $true
    $sources.Add($Path)
}
foreach ($channel in $channels) {
    if ($channel.kind -eq 'unavailable') { continue }
    if ($null -ne $channel.assembly) {
        foreach ($tile in $channel.tiles) { Add-Source $tile.full }
    } else {
        Add-Source $channel.full
    }
}

$environment = [System.IO.Path]::GetTempPath()
$workDir = Join-Path $environment ('doom3-content-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$script:TempDirs.Add($workDir)

$helperPath = Join-Path $workDir 'dims.py'
$requestPath = Join-Path $workDir 'dims-request.json'
$helperSource = @'
import json, sys

def emit(payload, code):
    sys.stdout.write(json.dumps(payload, sort_keys=True))
    sys.stdout.flush()
    sys.exit(code)

try:
    from PIL import Image
    import PIL
except Exception as exc:
    emit({"envError": "Pillow is not importable: %s" % exc}, 3)

if len(sys.argv) != 2:
    emit({"envError": "usage: dims.py <request.json>"}, 3)

try:
    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        request = json.load(handle)
except Exception as exc:
    emit({"envError": "cannot read request: %s" % exc}, 3)

images = []
for path in request.get("paths", []):
    try:
        with Image.open(path) as image:
            images.append({
                "path": path,
                "format": image.format or "",
                "width": int(image.size[0]),
                "height": int(image.size[1]),
                "mode": image.mode,
            })
    except Exception as exc:
        emit({"envError": None, "error": "%s: %s" % (path, exc)}, 4)

emit({"envError": None, "error": None,
      "python": ".".join(str(part) for part in sys.version_info[:3]),
      "pillow": getattr(PIL, "__version__", ""),
      "images": images}, 0)
'@
[System.IO.File]::WriteAllText($helperPath, $helperSource, (New-Object System.Text.UTF8Encoding($false)))
[System.IO.File]::WriteAllText($requestPath, (ConvertTo-Json -InputObject @{ paths = @($sources) } -Depth 4), (New-Object System.Text.UTF8Encoding($false)))

$launcher = [string](Get-JsonField (Get-JsonField $pins 'python') 'launcher')
if ([string]::IsNullOrWhiteSpace($launcher)) { Write-Failure 2 'env-python' 'pins.json records no Python launcher' }
$launcherParts = @($launcher -split '\s+' | Where-Object { $_ -ne '' })
$launcherArgs = @()
if ($launcherParts.Count -gt 1) { $launcherArgs = @($launcherParts[1..($launcherParts.Count - 1)]) }

try {
    $raw = & $launcherParts[0] $launcherArgs $helperPath $requestPath
    $pythonExit = $LASTEXITCODE
} catch {
    Write-Failure 2 'env-python' "cannot run '$launcher': $($_.Exception.Message)"
}
if (-not $raw) { Write-Failure 2 'env-python' "'$launcher' produced no output" }
try { $report = ($raw | Out-String) | ConvertFrom-Json } catch {
    Write-Failure 2 'env-python' "'$launcher' did not return JSON: $((($raw | Out-String).Trim()))"
}

$envError = [string](Get-JsonField $report 'envError')
if (-not [string]::IsNullOrWhiteSpace($envError)) { Write-Failure 2 'env-python' $envError }
if ($pythonExit -ne 0) {
    $sourceError = [string](Get-JsonField $report 'error')
    if ([string]::IsNullOrWhiteSpace($sourceError)) { $sourceError = "exit code $pythonExit" }
    Write-Failure 1 'source-unreadable' $sourceError
}

$dimensions = @{}
foreach ($image in @(Get-JsonField $report 'images')) {
    if ($null -eq $image) { continue }
    $dimensions[([string](Get-JsonField $image 'path')).ToLowerInvariant()] = $image
}

foreach ($channel in $channels) {
    if ($channel.kind -eq 'unavailable') { continue }

    if ($null -ne $channel.assembly) {
        foreach ($tile in $channel.tiles) {
            $image = $dimensions[$tile.full.ToLowerInvariant()]
            if ($null -eq $image) { Write-Failure 1 'source-unreadable' "no dimensions returned for '$($tile.relative)'" }

            $format = [string](Get-JsonField $image 'format')
            if ($format -ne 'PNG') {
                Write-Failure 1 'source-format' "channel '$($channel.name)' tile '$($tile.relative)' is $format, not PNG"
            }
            $width = [int](Get-JsonField $image 'width')
            $height = [int](Get-JsonField $image 'height')

            if ($width -ne $height) {
                Write-Failure 1 'source-aspect' "channel '$($channel.name)' tile '$($tile.relative)' is ${width}x${height}; a square source is required"
            }
            if ($width -gt 2048 -or $height -gt 2048) {
                Write-Failure 1 'source-dimension' "channel '$($channel.name)' tile '$($tile.relative)' is ${width}x${height}; the limit is 2048 px per axis"
            }
            if (($width % 4) -ne 0 -or ($height % 4) -ne 0) {
                Write-Failure 1 'source-alignment' "channel '$($channel.name)' tile '$($tile.relative)' is ${width}x${height}; each axis must be a multiple of 4"
            }
            $tile.width = $width
            $tile.height = $height
        }
        Test-AssemblyTiles -Channel $channel.name -Assembly $channel.assembly -Tiles $channel.tiles
        $channel | Add-Member -NotePropertyName width -NotePropertyValue $channel.assembly.width -Force
        $channel | Add-Member -NotePropertyName height -NotePropertyValue $channel.assembly.height -Force
        continue
    }

    $image = $dimensions[$channel.full.ToLowerInvariant()]
    if ($null -eq $image) { Write-Failure 1 'source-unreadable' "no dimensions returned for '$($channel.relative)'" }

    $format = [string](Get-JsonField $image 'format')
    if ($format -ne 'PNG') {
        Write-Failure 1 'source-format' "channel '$($channel.name)' source '$($channel.relative)' is $format, not PNG"
    }
    $width = [int](Get-JsonField $image 'width')
    $height = [int](Get-JsonField $image 'height')

    if ($width -ne $height) {
        Write-Failure 1 'source-aspect' "channel '$($channel.name)' source '$($channel.relative)' is ${width}x${height}; a square source is required"
    }
    if ($width -gt 2048 -or $height -gt 2048) {
        Write-Failure 1 'source-dimension' "channel '$($channel.name)' source '$($channel.relative)' is ${width}x${height}; the limit is 2048 px per axis"
    }
    if (($width % 4) -ne 0 -or ($height % 4) -ne 0) {
        Write-Failure 1 'source-alignment' "channel '$($channel.name)' source '$($channel.relative)' is ${width}x${height}; each axis must be a multiple of 4"
    }
    $channel | Add-Member -NotePropertyName width -NotePropertyValue $width -Force
    $channel | Add-Member -NotePropertyName height -NotePropertyValue $height -Force
}

# --- assemble tiled channels ---------------------------------------------------------------------
$assemblyHelperPath = Join-Path $workDir 'assemble.py'
$assemblyRequestPath = Join-Path $workDir 'assemble-request.json'
$assemblyHelperSource = @'
import json, sys

def emit(payload, code):
    sys.stdout.write(json.dumps(payload, sort_keys=True))
    sys.stdout.flush()
    sys.exit(code)

try:
    from PIL import Image
except Exception as exc:
    emit({"envError": "Pillow is not importable: %s" % exc}, 3)

if len(sys.argv) != 2:
    emit({"envError": "usage: assemble.py <request.json>"}, 3)

try:
    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        request = json.load(handle)
except Exception as exc:
    emit({"envError": "cannot read request: %s" % exc}, 3)

try:
    images = []
    mode = "RGB"
    for tile in request["tiles"]:
        image = Image.open(tile["path"])
        image.load()
        if image.mode in ("RGBA", "LA") or (image.mode == "P" and "transparency" in image.info):
            mode = "RGBA"
        images.append((tile, image))
    canvas = Image.new(mode, (int(request["width"]), int(request["height"])))
    for tile, image in images:
        if image.mode != mode:
            image = image.convert(mode)
        canvas.paste(image, (int(tile["x"]), int(tile["y"])))
    canvas.save(request["output"], format="PNG")
except Exception as exc:
    emit({"envError": None, "error": str(exc)}, 4)

emit({"envError": None, "error": None, "output": request["output"]}, 0)
'@
[System.IO.File]::WriteAllText($assemblyHelperPath, $assemblyHelperSource, (New-Object System.Text.UTF8Encoding($false)))

foreach ($channel in $channels) {
    if ($null -eq $channel.assembly) { continue }
    $assembledPng = Join-Path $workDir ('assembled-' + $channel.name + '.png')
    $assemblyRequest = [ordered]@{
        width  = $channel.assembly.width
        height = $channel.assembly.height
        output = $assembledPng
        tiles  = @($channel.tiles | ForEach-Object { [ordered]@{ path = $_.full; x = $_.x; y = $_.y } })
    }
    [System.IO.File]::WriteAllText($assemblyRequestPath, (ConvertTo-Json -InputObject $assemblyRequest -Depth 6), (New-Object System.Text.UTF8Encoding($false)))
    try {
        $assemblyRaw = & $launcherParts[0] $launcherArgs $assemblyHelperPath $assemblyRequestPath
        $assemblyExit = $LASTEXITCODE
    } catch {
        Write-Failure 2 'env-python' "cannot run '$launcher' while assembling '$($channel.name)': $($_.Exception.Message)"
    }
    if (-not $assemblyRaw) { Write-Failure 2 'env-python' "'$launcher' produced no output while assembling '$($channel.name)'" }
    try { $assemblyReport = ($assemblyRaw | Out-String) | ConvertFrom-Json } catch {
        Write-Failure 2 'env-python' "'$launcher' did not return JSON while assembling '$($channel.name)': $((($assemblyRaw | Out-String).Trim()))"
    }
    $assemblyEnvError = [string](Get-JsonField $assemblyReport 'envError')
    if (-not [string]::IsNullOrWhiteSpace($assemblyEnvError)) { Write-Failure 2 'env-python' $assemblyEnvError }
    if ($assemblyExit -ne 0) {
        $assemblyError = [string](Get-JsonField $assemblyReport 'error')
        if ([string]::IsNullOrWhiteSpace($assemblyError)) { $assemblyError = "exit code $assemblyExit" }
        Write-Failure 1 'assembly-failed' "channel '$($channel.name)': $assemblyError"
    }
    if (-not (Test-Path -LiteralPath $assembledPng)) {
        Write-Failure 1 'assembly-failed' "channel '$($channel.name)' produced no assembled PNG"
    }
    $channel.full = $assembledPng
}

# --- pinned encoder ------------------------------------------------------------------------------
$encoderScript = Join-Path $PSScriptRoot 'get-encoder.ps1'
if (-not (Test-Path -LiteralPath $encoderScript)) { Write-Failure 2 'env-encoder' "missing $encoderScript" }
$encoderLog = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $encoderScript 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    Write-Failure 2 'env-encoder' "get-encoder.ps1 exited $($LASTEXITCODE): $(Get-LogTail $encoderLog)"
}

$texconvPin = Get-JsonField $pins 'texconv'
$cacheRelative = [string](Get-JsonField $texconvPin 'cacheRelativePath')
$texconv = Join-Path $gitRoot ($cacheRelative -replace '/', '\')
if (-not (Test-Path -LiteralPath $texconv)) { Write-Failure 2 'env-encoder' "pinned encoder not found at $texconv" }
$encoderHash = Get-Sha256 $texconv
$pinnedHash = [string](Get-JsonField $texconvPin 'sha256')
if ($encoderHash -ne $pinnedHash) {
    Write-Failure 2 'env-encoder' "encoder hash $encoderHash does not match the pin $pinnedHash"
}
$texconvVersion = [string](Get-JsonField $texconvPin 'version')

# --- encode --------------------------------------------------------------------------------------
$flagsNode = Get-JsonField $pins 'encoderFlags'
if ($null -eq (Get-JsonField $flagsNode 'common')) { Write-Failure 2 'env-pins' 'pins.json encoderFlags has no common flags' }
$commonFlags = @(Get-JsonField $flagsNode 'common')
$buildDir = Join-Path $workDir 'package'
$stageDir = Join-Path $workDir 'staged'
New-Item -ItemType Directory -Force -Path $buildDir, $stageDir | Out-Null

$built = @{}
foreach ($channel in $channels) {
    if ($channel.kind -eq 'unavailable') { continue }

    $flags = @(Get-JsonField $flagsNode $channel.name)
    if ($flags.Count -eq 0) { Write-Failure 2 'env-pins' "pins.json has no encoderFlags for channel '$($channel.name)'" }
    $expectedFormat = Get-FormatFlag $flags
    if ([string]::IsNullOrWhiteSpace($expectedFormat)) {
        Write-Failure 2 'env-pins' "pins.json encoderFlags for '$($channel.name)' name no -f format"
    }

    $staged = Join-Path $stageDir ($channel.name + '.png')
    Copy-Item -LiteralPath $channel.full -Destination $staged -Force

    $arguments = @()
    $arguments += $flags
    $arguments += $commonFlags
    $arguments += @('-o', $buildDir, $staged)
    $encodeLog = & $texconv @arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        Write-Failure 1 'encode-failed' "texconv exited $($LASTEXITCODE) for channel '$($channel.name)': $(Get-LogTail $encodeLog)"
    }

    $dds = Join-Path $buildDir ($channel.name + '.dds')
    if (-not (Test-Path -LiteralPath $dds)) {
        Write-Failure 1 'encode-output-missing' "texconv wrote no '$($channel.name).dds': $(Get-LogTail $encodeLog)"
    }
    try { $info = Get-DdsInfo $dds } catch {
        Write-Failure 1 'encode-output-invalid' "cannot read '$($channel.name).dds': $($_.Exception.Message)"
    }
    if ($info.format -cne $expectedFormat) {
        Write-Failure 1 'encode-format' "texconv wrote $($info.format) for channel '$($channel.name)', the pin asks for $expectedFormat"
    }
    if ($info.width -ne $channel.width -or $info.height -ne $channel.height) {
        Write-Failure 1 'encode-dimension' "channel '$($channel.name)' encoded $($info.width)x$($info.height), the source is $($channel.width)x$($channel.height)"
    }
    $expectedMips = Get-ExpectedMipCount $info.width
    if ($info.mips -ne $expectedMips) {
        Write-Failure 1 'encode-mips' "channel '$($channel.name)' has $($info.mips) mips, a full chain needs $expectedMips"
    }

    $record = [ordered]@{
        channel = $channel.name
        file    = $channel.name + '.dds'
        format  = $info.format
        width   = $info.width
        height  = $info.height
        mips    = $info.mips
        srgb    = $info.srgb
        sha256  = Get-Sha256 $dds
    }
    if ($null -ne $channel.assembly) {
        $record['sources'] = @($channel.tiles | ForEach-Object { [ordered]@{ path = $_.relative; x = $_.x; y = $_.y } })
        $record['assembled'] = [ordered]@{ width = $channel.assembly.width; height = $channel.assembly.height }
    } else {
        $record['sources'] = @([ordered]@{ path = $channel.relative })
    }
    $built[$channel.name] = $record
}

$channelOrder = [string[]]@($built.Keys)
[Array]::Sort($channelOrder, [System.StringComparer]::Ordinal)
$textures = @()
foreach ($name in $channelOrder) { $textures += $built[$name] }

$sidecarHash = Get-Sha256Normalized $sidecarPath
$mtrHash = Get-Sha256Normalized $mtrPath
$package = [ordered]@{
    schema         = 'doom3-content-package/1'
    material       = $Material
    sidecar_sha256 = $sidecarHash
    mtr_sha256     = $mtrHash
    textures       = $textures
    toolchain      = [ordered]@{
        python  = [string](Get-JsonField $report 'python')
        pillow  = [string](Get-JsonField $report 'pillow')
        texconv = $texconvVersion
    }
    package_sha256 = Get-PackageSha256 -SidecarSha256 $sidecarHash -MtrSha256 $mtrHash -Textures $textures
}

# --- publish -------------------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $OutDir)) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
} elseif ($Force) {
    Get-ChildItem -LiteralPath $OutDir -Force | Remove-Item -Recurse -Force
}
foreach ($name in $channelOrder) {
    Copy-Item -LiteralPath (Join-Path $buildDir ($name + '.dds')) -Destination (Join-Path $OutDir ($name + '.dds')) -Force
}
Write-Json -Object $package -Path (Join-Path $OutDir 'package.json')

$script:TempDirs.Remove($workDir) | Out-Null
Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ("BUILD OK: {0} {1} textures -> {2}" -f $Material, $textures.Count, $OutDir)
exit 0
