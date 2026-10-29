#Requires -Version 5.1
<#
.SYNOPSIS
    Content contract gate for spec 009 (tasks T-003, T-004, T-005).

.DESCRIPTION
    Enforces the mechanical half of the content contract and says so, because byte origin cannot be
    proven from a file:

      1. tracked pack files      no tracked *.pk4/*.pk3/*.pak anywhere in the repository
      2. asset allowlist         only .png and .exr under content/hero/** (LICENSE and provenance.json excepted)
      3. provenance              one matching AssetRecord per hero file, per hero directory
      4. licence                 a LICENSE file containing CC0 in every hero directory
      5. size                    every hero PNG is <= 2048 px on each axis (dimensions read with Pillow)
      6. denylist                no hero file whose SHA-256 is listed in the pack denylist
      7. sidecars                every materials/*.mat.json is a complete, explicit channel declaration
      8. Q4 set pairing          every Q4 set entry has an .mtr and a sidecar, and no sidecar is outside the set
      9. corpus manifest         every pack is described; a staged pack must be present; search_order resolves
     10. hygiene                 no absolute machine path, key block or credential pattern in content/**

    Exit codes: 0 pass, 1 one or more violations, 2 environment error (missing git, content root, py -3
    or Pillow, unreadable policy file). Violations print as one " - <diagnostic>" line each.

    The tracked-file checks (1 and the tracked form of 10) run only when -ContentRoot is this
    repository's own content directory; a fixture content root elsewhere is scanned on disk instead.
    Use -SkipTracked to force the on-disk form, for example on a fixture tree with no git repository.

.PARAMETER RepoRoot
    Repository root used to resolve repo-relative manifest paths. Defaults to the nearest ancestor
    of this script that holds .git.

.PARAMETER ContentRoot
    Content directory to validate. Defaults to <RepoRoot>/content.

.PARAMETER Denylist
    Pack-member SHA-256 denylist. Defaults to <ContentRoot>/manifest/pack-denylist.json; a missing file
    means an empty list.

.PARAMETER SkipTracked
    Skip the tracked pack check and scan the content tree on disk for the hygiene check.

.EXAMPLE
    powershell -NoProfile -File content/tools/validate.ps1

.EXAMPLE
    powershell -NoProfile -File content/tools/validate.ps1 -RepoRoot out/fixture -ContentRoot out/fixture/content -SkipTracked
#>
[CmdletBinding()]
param(
    [string]$RepoRoot,
    [string]$ContentRoot,
    [string]$Denylist,
    [switch]$SkipTracked
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$script:Violations = New-Object 'System.Collections.Generic.List[string]'
$script:HashCache = @{}
$script:HeroFileCount = 0
$script:SidecarCount = 0
$script:Q4Count = 0
$script:PackCount = 0
$script:PackAbsentCount = 0
$script:TrackedMode = $false

function Stop-Environment {
    param([string]$Message)
    Write-Host "ENVIRONMENT ERROR: $Message"
    exit 2
}

function Add-Violation {
    param([string]$Message)
    $script:Violations.Add($Message) | Out-Null
}

function Test-HasProp {
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $false }
    return ($null -ne $Object.PSObject.Properties[$Name])
}

function Get-Prop {
    param($Object, [string]$Name, $Default = $null)
    if (-not (Test-HasProp $Object $Name)) { return $Default }
    $value = $Object.PSObject.Properties[$Name].Value
    if ($null -eq $value) { return $Default }
    return $value
}

function Test-NonEmptyString {
    param($Value)
    return (($Value -is [string]) -and ($Value.Trim().Length -gt 0))
}

function Get-DisplayPath {
    param([string]$FullPath)
    if (-not $FullPath) { return '<unknown>' }
    $full = [System.IO.Path]::GetFullPath($FullPath)
    $root = $script:ContentRoot
    if ($full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        $rel = $full.Substring($root.Length).TrimStart([char[]]@('\', '/'))
        return ('content/' + ($rel -replace '\\', '/'))
    }
    return ($FullPath -replace '\\', '/')
}

function Get-CachedSha256 {
    param([string]$Path)
    if (-not $script:HashCache.ContainsKey($Path)) {
        $script:HashCache[$Path] = Get-Sha256 -Path $Path
    }
    return $script:HashCache[$Path]
}

function Test-HeroMetadataFile {
    param([System.IO.FileInfo]$File)
    return (($File.Name -ieq 'LICENSE') -or ($File.Name -ieq 'provenance.json'))
}

function Test-Hex64 {
    param($Value)
    return (($Value -is [string]) -and ($Value -cmatch '^[0-9a-f]{64}$'))
}

function Test-PositiveInteger {
    param($Value)
    if ($Value -is [int]) { return ($Value -gt 0) }
    if ($Value -is [long]) { return ($Value -gt 0) }
    return $false
}

# --- Environment ---------------------------------------------------------------

$libPath = Join-Path $PSScriptRoot 'content-lib.ps1'
if (-not (Test-Path -LiteralPath $libPath -PathType Leaf)) {
    Stop-Environment "shared library not found: $libPath"
}
. $libPath

if (-not $RepoRoot) {
    try {
        $RepoRoot = Get-RepoRoot
    } catch {
        Stop-Environment "cannot locate the repository root (no .git above $PSScriptRoot); pass -RepoRoot"
    }
}
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
    Stop-Environment "repository root not found: $RepoRoot"
}
$script:RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)

if (-not $ContentRoot) {
    $ContentRoot = Join-Path $script:RepoRoot 'content'
}
if (-not (Test-Path -LiteralPath $ContentRoot -PathType Container)) {
    Stop-Environment "content root not found: $ContentRoot"
}
$script:ContentRoot = [System.IO.Path]::GetFullPath($ContentRoot).TrimEnd([char[]]@('\', '/'))

if (-not $Denylist) {
    $Denylist = Join-Path $script:ContentRoot 'manifest\pack-denylist.json'
}
$script:DenylistPath = [System.IO.Path]::GetFullPath($Denylist)

# Pinned encoder formats, so a sidecar cannot claim a format the builder will not produce.
$script:PinFormats = @{}
$pinsPath = Join-Path $PSScriptRoot 'pins.json'
if (Test-Path -LiteralPath $pinsPath -PathType Leaf) {
    try {
        $pins = Get-Content -LiteralPath $pinsPath -Raw | ConvertFrom-Json
        foreach ($channelName in @('baseColor', 'normal', 'roughness', 'metalness', 'ao', 'emissive')) {
            $flags = @($pins.encoderFlags.$channelName)
            $formatIndex = [array]::IndexOf($flags, '-f')
            if ($formatIndex -ge 0 -and ($formatIndex + 1) -lt $flags.Count) {
                $script:PinFormats[$channelName] = $flags[$formatIndex + 1]
            }
        }
    } catch {
        Stop-Environment "cannot read pins.json: $($_.Exception.Message)"
    }
}

# The seven C-12 cells, and which material claims each. A typo must fail rather than silently drop a
# cell from the matrix.
$script:CellIds = @('opaque_dielectric', 'opaque_metal', 'alpha_tested', 'emissive', 'skinned_character', 'transmissive_glass', 'large_environment')
$script:CellSeen = @{}

# --- Check 1: tracked pack files ----------------------------------------------

if (-not $SkipTracked) {
    $expectedContentRoot = [System.IO.Path]::GetFullPath((Join-Path $script:RepoRoot 'content')).TrimEnd([char[]]@('\', '/'))
    if ($script:ContentRoot -ieq $expectedContentRoot) {
        if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
            Stop-Environment 'git is required for the tracked-file checks; use -SkipTracked to run without it'
        }
        try {
            $script:TrackedFiles = @(Get-TrackedFiles -RepoRoot $script:RepoRoot)
        } catch {
            Stop-Environment "git ls-files failed in $($script:RepoRoot): $($_.Exception.Message)"
        }
        $script:TrackedMode = $true
        foreach ($trackedFile in $script:TrackedFiles) {
            if ($trackedFile -match '\.(pk4|pk3|pak)$') {
                Add-Violation "tracked pack file must not be committed: $trackedFile"
            }
        }
    }
}

# --- Hero inventory (checks 2, 3, 4, 5) ----------------------------------------

$script:HeroRoot = Join-Path $script:ContentRoot 'hero'
$heroDirs = @()
$heroFiles = @()
if (Test-Path -LiteralPath $script:HeroRoot -PathType Container) {
    $heroDirs = @(Get-ChildItem -LiteralPath $script:HeroRoot -Directory -Force | Sort-Object Name)
    $heroFiles = @(Get-ChildItem -LiteralPath $script:HeroRoot -File -Recurse -Force | Sort-Object FullName)
}

# Check 2: asset allowlist. LICENSE and provenance.json are the directory's own bookkeeping, not assets.
foreach ($heroFile in $heroFiles) {
    if (Test-HeroMetadataFile -File $heroFile) { continue }
    $script:HeroFileCount++
    if ($heroFile.Extension -notmatch '^\.(png|exr)$') {
        Add-Violation "$(Get-DisplayPath $heroFile.FullName): extension not allowed (only .png and .exr under content/hero)"
    }
    if ($heroFile.DirectoryName -ieq $script:HeroRoot) {
        Add-Violation "$(Get-DisplayPath $heroFile.FullName): hero file is not inside a hero/<material>/ directory"
    }
}

# Check 3: provenance, one directory at a time.
foreach ($heroDir in $heroDirs) {
    $prefix = $heroDir.FullName.TrimEnd([char[]]@('\', '/')) + '\'
    $assets = @($heroFiles | Where-Object {
        $_.FullName.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) -and -not (Test-HeroMetadataFile -File $_)
    })
    $provPath = Join-Path $heroDir.FullName 'provenance.json'
    $provRel = Get-DisplayPath $provPath

    if (-not (Test-Path -LiteralPath $provPath -PathType Leaf)) {
        Add-Violation "$provRel`: missing (every hero directory needs a record per file)"
        continue
    }
    try {
        $prov = Read-Json -Path $provPath
    } catch {
        Add-Violation "$provRel`: $($_.Exception.Message)"
        continue
    }

    if (-not (Test-HasProp $prov 'assets')) {
        Add-Violation "$provRel`: assets[] is missing"
        continue
    }
    $records = @(Get-Prop $prov 'assets' @())
    $byPath = @{}
    $index = 0
    foreach ($record in $records) {
        $index++
        $recordPath = Get-Prop $record 'path'
        $label = if (Test-NonEmptyString $recordPath) { $recordPath } else { "record $index" }
        if (-not (Test-NonEmptyString $recordPath)) {
            Add-Violation "$provRel`: $label lacks 'path'"
        } elseif (-not (Test-RepoRelativePath -RelativePath $recordPath)) {
            Add-Violation "$provRel`: $label path must be relative to the hero directory"
        }
        $recordSha = Get-Prop $record 'sha256'
        if (-not (Test-Hex64 $recordSha)) {
            Add-Violation "$provRel`: $label sha256 must be 64 lowercase hex characters"
        }
        if (-not (Test-NonEmptyString (Get-Prop $record 'author'))) {
            Add-Violation "$provRel`: $label lacks 'author'"
        }
        $recordLicence = Get-Prop $record 'licence'
        if (-not (Test-NonEmptyString $recordLicence)) {
            Add-Violation "$provRel`: $label lacks 'licence'"
        } elseif ($recordLicence -ne 'CC0-1.0') {
            Add-Violation "$provRel`: $label licence must be CC0-1.0 for a hero asset"
        }
        $consulted = Get-Prop $record 'retail_text_consulted'
        if ($consulted -ne 'yes' -and $consulted -ne 'no') {
            Add-Violation "$provRel`: $label retail_text_consulted must be 'yes' or 'no'"
        }
        if (-not (Test-NonEmptyString (Get-Prop $record 'id'))) {
            Add-Violation "$provRel`: $label lacks 'id' (FR-001 needs a stable record identity)"
        }
        $inputs = @(Get-Prop $record 'inputs' @())
        if ($inputs.Count -eq 0) {
            Add-Violation "$provRel`: $label has no 'inputs'; an asset must name the source it was authored from"
        } else {
            foreach ($entry in $inputs) {
                if (-not (Test-NonEmptyString $entry)) {
                    Add-Violation "$provRel`: $label has an empty inputs entry"
                }
            }
        }
        if (Test-NonEmptyString $recordPath) {
            $key = ($recordPath -replace '\\', '/').ToLowerInvariant()
            if ($key.StartsWith('./')) { $key = $key.Substring(2) }
            if (-not $byPath.ContainsKey($key)) { $byPath[$key] = $record }
        }
    }

    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($asset in $assets) {
        $rel = $asset.FullName.Substring($heroDir.FullName.Length).TrimStart([char[]]@('\', '/'))
        $key = ($rel -replace '\\', '/').ToLowerInvariant()
        [void]$seen.Add($key)
        if (-not $byPath.ContainsKey($key)) {
            Add-Violation "$(Get-DisplayPath $asset.FullName): no provenance record"
            continue
        }
        $expected = Get-Prop $byPath[$key] 'sha256'
        $actual = Get-CachedSha256 -Path $asset.FullName
        if (-not (Test-Hex64 $expected) -or ($expected -cne $actual)) {
            Add-Violation "$(Get-DisplayPath $asset.FullName): sha256 mismatch (provenance $expected, file $actual)"
        }
    }
    foreach ($key in $byPath.Keys) {
        if (-not $seen.Contains($key)) {
            Add-Violation "$provRel`: record '$key' has no matching file"
        }
    }
}

# Check 4: licence.
foreach ($heroDir in $heroDirs) {
    $licencePath = Join-Path $heroDir.FullName 'LICENSE'
    if (-not (Test-Path -LiteralPath $licencePath -PathType Leaf)) {
        Add-Violation "$(Get-DisplayPath $licencePath): missing (hero content must be CC0)"
        continue
    }
    $licenceText = [System.IO.File]::ReadAllText($licencePath)
    # A bare substring would accept "this is not CC0"; require the declaration itself.
    if ($licenceText -notmatch 'CC0\s*1\.0') {
        Add-Violation "$(Get-DisplayPath $licencePath): LICENSE does not declare CC0 1.0"
    }
}

# Check 5: size. Dimensions come from Pillow, never from a hand-written PNG parser.
foreach ($heroFile in $heroFiles) {
    if ($heroFile.Extension -ieq '.exr') {
        # Fail closed: an unchecked dimension is not an acceptable gap for a committed asset.
        Add-Violation "$(Get-DisplayPath $heroFile.FullName): .exr sources are not supported by the dimension check yet; use .png"
    }
}
$pngFiles = @($heroFiles | Where-Object { $_.Extension -ieq '.png' })
if ($pngFiles.Count -gt 0) {
    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ('doom3-content-validate-' + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
        $pythonFile = Join-Path $tempDir 'dimensions.py'
        $pathsFile = Join-Path $tempDir 'paths.json'
        $outFile = Join-Path $tempDir 'out.json'
        $errFile = Join-Path $tempDir 'err.txt'
        $pythonSource = @'
import json
import sys
from PIL import Image

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    paths = json.load(handle)

rows = []
for path in paths:
    try:
        with Image.open(path) as image:
            image.load()
            rows.append({"path": path, "ok": True, "width": int(image.width), "height": int(image.height)})
    except Exception as exc:
        rows.append({"path": path, "ok": False, "error": str(exc)})

print(json.dumps(rows))
'@
        [System.IO.File]::WriteAllText($pythonFile, $pythonSource + "`n", (New-Object System.Text.UTF8Encoding($false)))
        $pathsJson = ConvertTo-Json -InputObject @($pngFiles | ForEach-Object { $_.FullName }) -Compress
        [System.IO.File]::WriteAllText($pathsFile, $pathsJson, (New-Object System.Text.UTF8Encoding($false)))

        if (-not (Get-Command py -ErrorAction SilentlyContinue)) {
            Stop-Environment 'py -3 with Pillow is required for the PNG dimension check'
        }
        $arguments = ('-3 "{0}" "{1}"' -f $pythonFile, $pathsFile)
        $process = Start-Process -FilePath 'py' -ArgumentList $arguments -NoNewWindow -Wait -PassThru -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        if ($process.ExitCode -ne 0) {
            $detail = ''
            if (Test-Path -LiteralPath $errFile) { $detail = ([System.IO.File]::ReadAllText($errFile)).Trim() }
            if ($detail.Length -gt 400) { $detail = $detail.Substring(0, 400) }
            Stop-Environment "py -3 with Pillow failed (exit $($process.ExitCode)); the PNG dimension check cannot run: $detail"
        }
        try {
            $dimensionRows = ConvertFrom-Json -InputObject ([System.IO.File]::ReadAllText($outFile))
        } catch {
            Stop-Environment "py -3 produced output that is not valid JSON: $($_.Exception.Message)"
        }
        $rows = @($dimensionRows)
        foreach ($row in $rows) {
            $rowPath = Get-Prop $row 'path'
            if (-not (Get-Prop $row 'ok' $false)) {
                Add-Violation "$(Get-DisplayPath $rowPath): cannot read PNG dimensions: $(Get-Prop $row 'error')"
                continue
            }
            $width = [int](Get-Prop $row 'width' 0)
            $height = [int](Get-Prop $row 'height' 0)
            if ($width -gt 2048 -or $height -gt 2048) {
                Add-Violation "$(Get-DisplayPath $rowPath): ${width}x${height} exceeds 2048 px on an axis"
            }
            if ($width -ne $height) {
                Add-Violation "$(Get-DisplayPath $rowPath): ${width}x${height} is not square"
            }
            if (($width % 4) -ne 0 -or ($height % 4) -ne 0) {
                Add-Violation "$(Get-DisplayPath $rowPath): ${width}x${height} is not a multiple of 4 on both axes (block compression needs it)"
            }
        }
    } finally {
        if (Test-Path -LiteralPath $tempDir) { Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

# --- Check 6: denylist ---------------------------------------------------------

$denyHashes = New-Object 'System.Collections.Generic.HashSet[string]'
if (Test-Path -LiteralPath $script:DenylistPath -PathType Leaf) {
    try {
        $deny = Read-Json -Path $script:DenylistPath
    } catch {
        Stop-Environment "cannot read denylist $($script:DenylistPath): $($_.Exception.Message)"
    }
    foreach ($hash in @(Get-Prop $deny 'sha256' @())) {
        if ($hash -is [string]) { [void]$denyHashes.Add($hash.ToLowerInvariant()) }
    }
}
foreach ($heroFile in $heroFiles) {
    if ($denyHashes.Contains((Get-CachedSha256 -Path $heroFile.FullName))) {
        Add-Violation "$(Get-DisplayPath $heroFile.FullName): sha256 is on the pack denylist"
    }
}
Write-Host 'denylist: byte origin is otherwise established by provenance and review (mechanical check only)'

# --- Check 7: material sidecars ------------------------------------------------

$channelNames = @('baseColor', 'normal', 'roughness', 'metalness', 'ao', 'emissive')
$materialsDir = Join-Path $script:ContentRoot 'materials'
$sidecars = @()
if (Test-Path -LiteralPath $materialsDir -PathType Container) {
    $sidecars = @(Get-ChildItem -LiteralPath $materialsDir -File -Force | Where-Object { $_.Name -like '*.mat.json' } | Sort-Object Name)
}
$script:SidecarCount = $sidecars.Count
$sidecarByMaterial = @{}

foreach ($sidecar in $sidecars) {
    $sidecarRel = Get-DisplayPath $sidecar.FullName
    try {
        $sidecarJson = Read-Json -Path $sidecar.FullName
    } catch {
        Add-Violation "$sidecarRel`: $($_.Exception.Message)"
        continue
    }

    if ((Get-Prop $sidecarJson 'schema') -ne 'doom3-material-sidecar/1') {
        Add-Violation "$sidecarRel`: schema must be doom3-material-sidecar/1"
    }

    $material = Get-Prop $sidecarJson 'material'
    if (-not (Test-NonEmptyString $material)) {
        Add-Violation "$sidecarRel`: material must be a non-empty string"
        $material = $null
    }

    $mtr = Get-Prop $sidecarJson 'mtr'
    if (-not (Test-NonEmptyString $mtr)) {
        Add-Violation "$sidecarRel`: mtr must be a non-empty repo-relative path"
    } elseif (-not (Test-RepoRelativePath -RelativePath $mtr)) {
        Add-Violation "$sidecarRel`: mtr must be a repo-relative path: '$mtr'"
    } elseif (-not (Test-Path -LiteralPath (Join-Path $script:RepoRoot ($mtr -replace '/', '\')) -PathType Leaf)) {
        Add-Violation "$sidecarRel`: mtr file does not exist: $mtr"
    }

    $channels = Get-Prop $sidecarJson 'channels'
    if ($null -eq $channels) {
        Add-Violation "$sidecarRel`: channels object is missing"
    } else {
        foreach ($channelName in $channelNames) {
            if (-not (Test-HasProp $channels $channelName)) {
                Add-Violation "$sidecarRel`: channel $channelName is missing"
                continue
            }
            $channel = Get-Prop $channels $channelName
            $state = Get-Prop $channel 'state'
            if (-not (Test-NonEmptyString $state)) {
                Add-Violation "$sidecarRel`: channel $channelName has no state"
                continue
            }
            if ($state -notmatch '^(authored|derived:.+|unavailable)$') {
                Add-Violation "$sidecarRel`: channel $channelName has invalid state '$state'"
                continue
            }
            if ($state -eq 'unavailable') {
                if (-not (Test-NonEmptyString (Get-Prop $channel 'why'))) {
                    Add-Violation "$sidecarRel`: channel $channelName is unavailable without a why"
                }
                continue
            }
            $channelPath = Get-Prop $channel 'path'
            if (-not (Test-NonEmptyString $channelPath)) {
                Add-Violation "$sidecarRel`: channel $channelName has state '$state' but no path"
            } elseif (-not (Test-RepoRelativePath -RelativePath $channelPath)) {
                Add-Violation "$sidecarRel`: channel $channelName path must be repo-relative: '$channelPath'"
            } elseif (-not (Test-Path -LiteralPath (Join-Path $script:RepoRoot ($channelPath -replace '/', '\')) -PathType Leaf)) {
                Add-Violation "$sidecarRel`: channel $channelName path does not exist: '$channelPath'"
            }
            $declaredFormat = Get-Prop $channel 'format'
            if (-not (Test-NonEmptyString $declaredFormat)) {
                Add-Violation "$sidecarRel`: channel $channelName has no format"
            } elseif ($script:PinFormats.ContainsKey($channelName) -and $script:PinFormats[$channelName] -and $declaredFormat -ne $script:PinFormats[$channelName]) {
                # The builder encodes with the pinned flags, so a sidecar that claims another format would
                # be a claim the package contradicts. Compare against the pin, not against the builder.
                Add-Violation "$sidecarRel`: channel $channelName declares format '$declaredFormat' but the pinned encoder produces '$($script:PinFormats[$channelName])'"
            }
            $srgb = Get-Prop $channel 'srgb'
            if (-not ($srgb -is [bool])) {
                Add-Violation "$sidecarRel`: channel $channelName must have a boolean srgb"
            }
        }
        foreach ($property in $channels.PSObject.Properties) {
            if ($channelNames -notcontains $property.Name) {
                Add-Violation "$sidecarRel`: channel '$($property.Name)' is not one of the six required channels"
            }
        }
    }

    if ($material) {
        $materialKey = $material.ToLowerInvariant()
        if ($sidecarByMaterial.ContainsKey($materialKey)) {
            Add-Violation "$sidecarRel`: duplicate sidecar for material '$material'"
        } else {
            $sidecarByMaterial[$materialKey] = @{ Json = $sidecarJson; Rel = $sidecarRel }
        }
    }
}

# --- Check 8: Q4 set pairing ---------------------------------------------------

$q4Path = Join-Path $script:ContentRoot 'manifest\q4-materials.json'
$q4Rel = Get-DisplayPath $q4Path
$q4Materials = @()
if (-not (Test-Path -LiteralPath $q4Path -PathType Leaf)) {
    Add-Violation "$q4Rel`: missing (the Q4 set is the only definition of a sidecar-bearing material)"
} else {
    try {
        $q4 = Read-Json -Path $q4Path
    } catch {
        Add-Violation "$q4Rel`: $($_.Exception.Message)"
        $q4 = $null
    }
    if ($null -ne $q4) {
        if ((Get-Prop $q4 'schema') -ne 'doom3-q4-material-set/1') {
            Add-Violation "$q4Rel`: schema must be doom3-q4-material-set/1"
        }
        $q4Materials = @(Get-Prop $q4 'materials' @())
        foreach ($entry in $q4Materials) {
            $name = Get-Prop $entry 'material'
            $mtr = Get-Prop $entry 'mtr'
            if (-not (Test-NonEmptyString $name)) {
                Add-Violation "$q4Rel`: entry lacks material"
                continue
            }
            $cell = Get-Prop $entry 'cell'
            if (-not (Test-NonEmptyString $cell)) {
                Add-Violation "$q4Rel`: entry '$name' lacks cell"
            } elseif ($script:CellIds -notcontains $cell) {
                Add-Violation "$q4Rel`: entry '$name' has cell '$cell', which is not one of the seven C-12 cells"
            } elseif ($script:CellSeen.ContainsKey($cell)) {
                Add-Violation "$q4Rel`: cell '$cell' is claimed by both '$($script:CellSeen[$cell])' and '$name'"
            } else {
                $script:CellSeen[$cell] = $name
            }
            if (-not (Test-NonEmptyString $mtr) -or -not (Test-RepoRelativePath -RelativePath $mtr)) {
                Add-Violation "$q4Rel`: entry '$name' mtr must be a repo-relative path"
                continue
            }
            $mtrFull = Join-Path $script:RepoRoot ($mtr -replace '/', '\')
            if (-not (Test-Path -LiteralPath $mtrFull -PathType Leaf)) {
                Add-Violation "Q4 material '$name': mtr file does not exist: $mtr"
            } else {
                $mtrText = [System.IO.File]::ReadAllText($mtrFull)
                $declaration = '(?im)^[ \t]*material[ \t]+' + [regex]::Escape($name) + '([ \t\r\n{]|$)'
                if ($mtrText -notmatch $declaration) {
                    Add-Violation "Q4 material '$name': $mtr has no 'material $name' declaration"
                }
            }
            $materialKey = $name.ToLowerInvariant()
            if (-not $sidecarByMaterial.ContainsKey($materialKey)) {
                Add-Violation "Q4 material '$name': sidecar does not exist"
            } else {
                $sidecarMtr = Get-Prop $sidecarByMaterial[$materialKey].Json 'mtr'
                if (($sidecarMtr -replace '\\', '/') -ne ($mtr -replace '\\', '/')) {
                    Add-Violation "Q4 material '$name': sidecar mtr '$sidecarMtr' does not match the set mtr '$mtr'"
                }
            }
        }
        $setNames = @($q4Materials | ForEach-Object { Get-Prop $_ 'material' } |
            Where-Object { Test-NonEmptyString $_ } | ForEach-Object { $_.ToLowerInvariant() })
        foreach ($materialKey in $sidecarByMaterial.Keys) {
            if ($setNames -notcontains $materialKey) {
                # Not a violation: FR-005 makes a sidecar mandatory for Q4 materials, it does not forbid one
            # for another material. It is still worth saying out loud, because such a sidecar is inert.
            Write-Host "WARNING: $($sidecarByMaterial[$materialKey].Rel): material '$materialKey' is not in the Q4 set; its sidecar is inert"
            }
        }
    }
}
$script:Q4Count = $q4Materials.Count

# --- Check 9: corpus manifest --------------------------------------------------

$manifestPath = Join-Path $script:ContentRoot 'manifest\content-manifest.json'
$manifestRel = Get-DisplayPath $manifestPath
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    Add-Violation "$manifestRel`: missing (every staged corpus must be described)"
} else {
    try {
        $manifest = Read-Json -Path $manifestPath
    } catch {
        $manifest = $null
        Add-Violation "$manifestRel`: $($_.Exception.Message)"
    }
    if ($null -ne $manifest) {
        if ((Get-Prop $manifest 'schema') -ne 'doom3-content-manifest/1') {
            Add-Violation "$manifestRel`: schema must be doom3-content-manifest/1"
        }
        $packs = @(Get-Prop $manifest 'packs' @())
        $script:PackCount = $packs.Count
        $packNames = @()
        $stagedCount = 0
        foreach ($pack in $packs) {
            $name = Get-Prop $pack 'name'
            if (-not (Test-NonEmptyString $name)) {
                Add-Violation "$manifestRel`: pack entry lacks name"
                continue
            }
            $packNames += $name
            if (-not (Test-PositiveInteger (Get-Prop $pack 'bytes'))) {
                Add-Violation "pack '$name': bytes must be a positive integer"
            }
            if (-not (Test-Hex64 (Get-Prop $pack 'sha256'))) {
                Add-Violation "pack '$name': sha256 must be 64 lowercase hex characters"
            }
            $state = Get-Prop $pack 'state'
            $stateValid = $false
            if (Test-NonEmptyString $state) {
                if ($state -eq 'staged' -or $state -eq 'absent') {
                    $stateValid = $true
                } elseif ($state -match '^skipped:.+') {
                    $stateValid = $true
                }
            }
            if (-not $stateValid) {
                Add-Violation "pack '$name': state must be staged, absent or skipped:<reason>"
            }
            if (-not (Test-NonEmptyString (Get-Prop $pack 'licence_note'))) {
                Add-Violation "pack '$name': licence_note is required"
            }
            if ($state -eq 'staged') {
                $stagedCount++
                $contentDir = $env:D3_CONTENT_DIR
                $stagedPath = if (Test-NonEmptyString $contentDir) { Join-Path $contentDir $name } else { '' }
                if (-not (Test-NonEmptyString $contentDir)) {
                    Add-Violation "pack '$name': state is staged but D3_CONTENT_DIR is not set"
                } elseif (-not (Test-Path -LiteralPath $stagedPath -PathType Leaf)) {
                    Add-Violation "pack '$name': state is staged but the file is absent under D3_CONTENT_DIR"
                } else {
                    # Existence is not enough: a renamed or truncated pack would otherwise pass while the
                    # manifest claims a size and a hash. FR-002 says the hashes are verified.
                    $actualBytes = (Get-Item -LiteralPath $stagedPath).Length
                    $declaredBytes = Get-Prop $pack 'bytes'
                    if ($actualBytes -ne $declaredBytes) {
                        Add-Violation "pack '$name': staged file is $actualBytes bytes, the manifest declares $declaredBytes"
                    }
                    $actualHash = (Get-FileHash -LiteralPath $stagedPath -Algorithm SHA256).Hash.ToLowerInvariant()
                    $declaredHash = Get-Prop $pack 'sha256'
                    if ($actualHash -ne $declaredHash) {
                        Add-Violation "pack '$name': staged file hashes to $actualHash, the manifest declares $declaredHash"
                    }
                    Write-Host "VERIFIED: pack '$name' matches its manifest size and hash"
                }
            } else {
                $script:PackAbsentCount++
                Write-Host "SKIPPED: pack '$name' is $state"
            }
        }
        $searchOrder = @(Get-Prop $manifest 'search_order' @())
        if ($searchOrder.Count -eq 0) {
            Add-Violation "$manifestRel`: search_order must not be empty"
        }
        foreach ($orderName in $searchOrder) {
            if ($packNames -notcontains $orderName) {
                Add-Violation "$manifestRel`: search_order names '$orderName', which is not a pack entry"
            }
        }
        if ($stagedCount -eq 0) {
            Write-Host 'SKIPPED: corpus verification (no staged pack)'
        }
    }
}

# --- Check 10: hygiene ---------------------------------------------------------

$hygienePatterns = @(
    @{ Name = 'absolute-windows-path'; Pattern = '(?<![A-Za-z0-9_])[A-Za-z]:\\[A-Za-z0-9_.$\\-]' },
    @{ Name = 'absolute-unix-path'; Pattern = '/Users/|/home/' },
    @{ Name = 'private-key-block'; Pattern = '-----BEGIN' },
    @{ Name = 'aws-access-key-id'; Pattern = 'AKIA[0-9A-Z]{16}' },
    @{ Name = 'github-token'; Pattern = 'ghp_[A-Za-z0-9]{20,}' },
    @{ Name = 'credential-assignment'; Pattern = '(?i)\b(api[_-]?key|secret|password|passwd|access[_-]?token|client[_-]?secret)\b\s*[:=]\s*"[^"]{8,}"' }
)
# Binary files must never be searched as text in either mode: a PNG's compressed bytes can contain a
# byte sequence that looks like "C:\", which produced a false positive on the committed tree. Text
# extensions are searched, including .mtr and .ps1, because a machine path or a token in a script is
# exactly what FR-016 forbids.
$binaryExtensions = '^\.(png|exr|dds|tga|jpg|jpeg|gif|pk4|pk3|pak|exe|dll|lib|so|bin|zip|gz|7z|pdf|woff2?|ttf)$'
$hygieneFiles = @()
if ($script:TrackedMode) {
    foreach ($trackedFile in $script:TrackedFiles) {
        if (-not $trackedFile.StartsWith('content/', [System.StringComparison]::OrdinalIgnoreCase)) { continue }
        # Self-exemption: this file holds the pattern table itself, so scanning it would always match.
        # Every other script under content/ is scanned; a secret in this one is a review matter.
        if ($trackedFile -ieq 'content/tools/validate.ps1') { continue }
        $ext = [System.IO.Path]::GetExtension($trackedFile).ToLowerInvariant()
        if ($ext -match $binaryExtensions) { continue }
        $full = Join-Path $script:RepoRoot ($trackedFile -replace '/', '\')
        if (Test-Path -LiteralPath $full -PathType Leaf) { $hygieneFiles += $full }
    }
} else {
    foreach ($file in (Get-ChildItem -LiteralPath $script:ContentRoot -File -Recurse -Force)) {
        if ($file.FullName -ieq $PSCommandPath) { continue }
        if ($file.Extension -match $binaryExtensions) { continue }
        $hygieneFiles += $file.FullName
    }
}
foreach ($file in $hygieneFiles) {
    if ((Get-Item -LiteralPath $file).Length -gt 4MB) { continue }
    $text = [System.IO.File]::ReadAllText($file)
    foreach ($pattern in $hygienePatterns) {
        if ($text -cmatch $pattern.Pattern) {
            Add-Violation "$(Get-DisplayPath $file): hygiene violation (pattern: $($pattern.Name))"
        }
    }
}

# --- Report --------------------------------------------------------------------

if ($script:Violations.Count -gt 0) {
    Write-Host 'VIOLATIONS:'
    foreach ($violation in $script:Violations) {
        Write-Host (" - " + $violation)
    }
    Write-Host "VALIDATE FAILED: $($script:Violations.Count) violation(s)"
    exit 1
}

Write-Host ("VALIDATE OK: {0} hero files, {1} sidecars, {2} Q4 materials, {3} packs ({4} absent)" -f `
        $script:HeroFileCount, $script:SidecarCount, $script:Q4Count, $script:PackCount, $script:PackAbsentCount)
exit 0
