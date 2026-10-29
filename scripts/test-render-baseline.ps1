#Requires -Version 5.1
<#
.SYNOPSIS
  Refusal-first Q0 baseline check, capture and repeatability comparison for the existing native renderer.

.DESCRIPTION
  This is the only Q0 entry point that starts the reference executable. It is deliberately
  conservative: it never deletes an existing output tree, never stops a process it did not start,
  and never treats smoke output, a historical screenshot or a missing prerequisite as a PASS.

  Modes
    ListScenes  Print the predeclared scene ids and their data requirements. No process, no writes.
    Check       Validate exe, data, output location and scene inputs. No process, no writes.
    Capture     Check, then run each selected scene twice into <OutDir>\<scene>\run1|run2.
    Compare     Compare the two runs of each selected scene against the declared tolerances.
    All         Capture, then Compare.

  Isolation
    * The game writes only below <OutDir>\<scene>\run<n>; fs_basepath points at -DataDir.
    * An existing non-empty OutDir is refused (exit 5) unless -Clean is supplied. -Clean moves
      the old tree to <OutDir>.superseded-<UTC>; it is never deleted.
    * OutDir must be a strict child of -OutRoot (when supplied), or live under %TEMP% / a build
      directory. Repo roots, tracked source directories, the data root and drive roots are refused.
    * A running instance of the resolved executable is refused (exit 7) unless -StopExisting is
      supplied; that flag stops only processes whose image path resolves to the same file.

  Exit codes
    0  PASS (or Check/ListScenes succeeded)
    2  invalid argument or unsafe path
    3  executable missing or not a file
    4  data directory missing, empty, or expected data hash mismatch
    5  output refused (non-empty, file, unsafe, or archive collision)
    6  scene selection invalid, scene input missing, or comparison input missing/mismatched
    7  executable identity mismatch or a conflicting instance is running
    8  capture failed (process died, no window, log assertion, flat screenshot)
    9  repeatability comparison failed
    10 SKIPPED (data unavailable and -AllowUnavailableData was supplied; never a PASS)
    11 unexpected internal error

.PARAMETER Mode
  One of ListScenes, Check, Capture, Compare, All. Default Check.

.PARAMETER Exe
  Path to the reference DOOM3.exe. Defaults to Resolve-Doom3Exe from scripts\_common.ps1.

.PARAMETER DataDir
  Root that contains base\*.pk4. Defaults to $env:D3_DATA_DIR. It is treated as read-only input.

.PARAMETER OutDir
  New output tree. Required for Check, Capture, Compare and All.

.PARAMETER OutRoot
  Optional containment root. When supplied, OutDir must be a strict child of OutRoot.

.PARAMETER Scenes
  Scene ids to select. Default: all scenes in the table. Unknown or duplicate ids are refused.

.PARAMETER ExpectedExeSha256
  Optional expected SHA-256 of the executable. A mismatch exits 7.

.PARAMETER ExpectedDataSha256
  Optional expected SHA-256 of the primary base\*.pk4. A mismatch exits 4.

.PARAMETER Clean
  Allow replacing a non-empty OutDir by moving it aside. The old tree is never deleted.

.PARAMETER StopExisting
  Allow stopping instances of the exact resolved executable before capture.

.PARAMETER AllowUnavailableData
  Return SKIPPED (10) instead of failing (4) when the data root is missing/empty.

.EXAMPLE
  .\scripts\test-render-baseline.ps1 -Mode ListScenes

.EXAMPLE
  .\scripts\test-render-baseline.ps1 -Mode Check -Exe .\build\bin\Release\DOOM3.exe -DataDir $env:D3_DATA_DIR -OutDir .\build\q0\out -OutRoot .\build\q0

.EXAMPLE
  .\scripts\test-render-baseline.ps1 -Mode All -Exe .\build\bin\Release\DOOM3.exe -DataDir $env:D3_DATA_DIR -OutDir .\build\q0\out -OutRoot .\build\q0 -Scenes corridor,open-view
#>
[CmdletBinding()]
param(
    [ValidateSet('ListScenes', 'Check', 'Capture', 'Compare', 'All')]
    [string]$Mode = 'Check',
    [ValidateSet('Window', 'Engine')]
    [string]$CaptureMethod = 'Engine',
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [string]$OutRoot,
    [string[]]$Scenes = @(),
    [string]$ExpectedExeSha256,
    [string]$ExpectedDataSha256,
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$TimeoutSec = 120,
    [int]$SettleSec = 20,
    [int]$Repeat = 2,
    [double]$MaxMeanAbs = 0,
    [double]$MaxDiffPercent = 0,
    [int]$Delta = 8,
    [switch]$Clean,
    [switch]$StopExisting,
    [switch]$AllowUnavailableData
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$script:Schema = 'doom3-q0-baseline/v1'
$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:DataInfo = $null
$script:ExitCode = 0

# Predeclared scenes. The ids, maps and drive inputs are the contract in
# specs/004-quality-renderer/reference-scenes.md; keep both files in step.
$script:SceneTable = @(
    [pscustomobject]@{
        Id = 'corridor'
        Category = 'corridor'
        Map = 'game/demo_mc_underground'
        Status = 'demo-available'
        Description = 'Underground corridor through the opened access doors, captured at a fixed 600 tics with the engine screenshot command.'
        Drive = @('+devmap', 'game/demo_mc_underground', '+wait', '600', '+set', 'g_showHud', '0', '+echo', 'SCENE_READY_corridor')
        Markers = @('entities spawned', 'SpawnPlayer', 'SCENE_READY_corridor')
        Forbidden = @('ERROR:')
        MaxMeanAbs = 0.5
        MaxDiffPercent = 0.5
    },
    [pscustomobject]@{
        Id = 'open-view'
        Category = 'open view'
        Map = 'game/demo_mars_city2'
        Status = 'demo-available'
        Description = 'Open hangar through the opened transfer-elevator doors, captured at a fixed 600 tics.'
        Drive = @('+set', 'ui_showGun', '0', '+devmap', 'game/demo_mars_city2', '+wait', '600', '+set', 'g_showHud', '0', '+set', 'g_skipFX', '1', '+set', 'g_skipParticles', '1', '+echo', 'SCENE_READY_open-view')
        Markers = @('entities spawned', 'SpawnPlayer', 'SCENE_READY_open-view')
        Forbidden = @('ERROR:')
        MaxMeanAbs = 2.0
        MaxDiffPercent = 2.0
    },
    [pscustomobject]@{
        Id = 'close-character'
        Category = 'close character/viewmodel'
        Map = 'game/demo_mars_city2'
        Status = 'demo-available'
        Description = 'Third-person marine close-up in the transfer elevator, captured at a fixed 600 tics.'
        Drive = @('+devmap', 'game/demo_mars_city2', '+wait', '600', '+set', 'pm_thirdPerson', '1', '+set', 'pm_thirdPersonRange', '80', '+set', 'pm_thirdPersonAngle', '180', '+set', 'g_showHud', '0', '+set', 'g_skipFX', '1', '+set', 'g_skipParticles', '1', '+echo', 'SCENE_READY_close-character')
        Markers = @('entities spawned', 'SpawnPlayer', 'SCENE_READY_close-character')
        Forbidden = @('ERROR:')
        MaxMeanAbs = 0.5
        MaxDiffPercent = 0.5
    },
    [pscustomobject]@{
        Id = 'moving-light-combat'
        Category = 'moving light/combat'
        Map = 'game/demo_mars_city2'
        Status = 'demo-available'
        Description = 'Imp attack at the opened elevator doors with god mode, captured at a fixed 720 tics.'
        Drive = @('+devmap', 'game/demo_mars_city2', '+wait', '600', '+set', 'g_showHud', '0', '+god', '+spawn', 'monster_demon_imp', '+wait', '120', '+echo', 'SCENE_READY_moving-light-combat')
        Markers = @('entities spawned', 'SpawnPlayer', 'SCENE_READY_moving-light-combat')
        Forbidden = @('ERROR:', "Unknown classname 'monster_demon_imp'")
        MaxMeanAbs = 3.0
        MaxDiffPercent = 5.0
    }
)

function Write-Line([string]$Text) { Write-Host $Text }

function Fail([int]$Code, [string]$Message) {
    Write-Line "BASELINE result=FAIL code=$Code $Message"
    exit $Code
}

function Skip([string]$Message) {
    Write-Line "BASELINE result=SKIPPED code=10 $Message"
    exit 10
}

function Get-FullPath([string]$Path) {
    if (-not $Path) { return $null }
    return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-SamePath([string]$A, [string]$B) {
    if (-not $A -or -not $B) { return $false }
    return [string]::Equals((Get-FullPath $A), (Get-FullPath $B), [StringComparison]::OrdinalIgnoreCase)
}

function Test-Under([string]$Child, [string]$Parent) {
    $c = Get-FullPath $Child
    $p = Get-FullPath $Parent
    if (-not $c -or -not $p) { return $false }
    if ([string]::Equals($c, $p, [StringComparison]::OrdinalIgnoreCase)) { return $false }
    return $c.StartsWith($p + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Test-TrackedSourcePath([string]$Path) {
    $p = Get-FullPath $Path
    $blocked = @(
        (Join-Path $script:RepoRoot '.git'),
        (Join-Path $script:RepoRoot 'doom3_orig'),
        (Join-Path $script:RepoRoot 'specs'),
        (Join-Path $script:RepoRoot 'scripts'),
        (Join-Path $script:RepoRoot 'src'),
        (Join-Path $script:RepoRoot 'tests'),
        (Join-Path $script:RepoRoot 'docs'),
        (Join-Path $script:RepoRoot 'targets\win11-amd64\doom3'),
        (Join-Path $script:RepoRoot 'targets\win11-amd64-hd\doom3')
    )
    foreach ($b in $blocked) {
        if ((Test-SamePath $p $b) -or (Test-Under $p $b)) { return $true }
    }
    return $false
}

function Assert-SafeOutDir([string]$Path) {
    $full = Get-FullPath $Path
    if (-not $full) { Fail 2 'OutDir is required' }
    if (Test-SamePath $full ([IO.Path]::GetPathRoot($full))) { Fail 2 "OutDir is a drive root: $full" }
    if (Test-SamePath $full $script:RepoRoot) { Fail 2 "OutDir is the repository root: $full" }
    if ($script:DataDirFull -and ((Test-SamePath $full $script:DataDirFull) -or (Test-Under $full $script:DataDirFull) -or (Test-Under $script:DataDirFull $full))) {
        Fail 2 "OutDir is inside, contains or equals the data root: $full"
    }
    $exeDir = if ($script:ExeFull) { Split-Path -Parent $script:ExeFull } else { $null }
    if ($exeDir -and ((Test-SamePath $full $exeDir) -or (Test-Under $full $exeDir) -or (Test-Under $exeDir $full))) {
        Fail 2 "OutDir is inside, contains or equals the executable directory: $full"
    }
    if (Test-TrackedSourcePath $full) { Fail 2 "OutDir is inside a tracked source directory: $full" }

    if ($OutRoot) {
        $root = Get-FullPath $OutRoot
        if (-not (Test-Under $full $root)) { Fail 2 "OutDir must be a strict child of OutRoot ($root): $full" }
        if (Test-TrackedSourcePath $root) { Fail 2 "OutRoot is inside a tracked source directory: $root" }
    } else {
        $underTemp = $env:TEMP -and ((Test-SamePath $full $env:TEMP) -or (Test-Under $full $env:TEMP))
        $buildSegment = $full -match '(?i)(^|\\)build[^\\]*(\\|$)'
        if (-not $underTemp -and -not $buildSegment) {
            Fail 2 "OutDir must be under %TEMP% or contain a build* path segment, or pass -OutRoot: $full"
        }
    }
    return $full
}

function Get-PeMachine([string]$Path) {
    $fs = $null
    try {
        $fs = [IO.File]::OpenRead($Path)
        if ($fs.Length -lt 0x40) { return 0 }
        $br = New-Object IO.BinaryReader $fs
        $fs.Position = 0x3C
        $pe = $br.ReadInt32()
        if ($pe -le 0 -or $pe + 6 -gt $fs.Length) { return 0 }
        $fs.Position = $pe + 4
        return [int]$br.ReadUInt16()
    } catch {
        return 0
    } finally {
        if ($fs) { $fs.Dispose() }
    }
}

function Get-ExeIdentity([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $Path
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
        peMachine = ('0x{0:X4}' -f (Get-PeMachine $Path))
        version = $item.VersionInfo.FileVersion
    }
}

function Get-DataIdentity([string]$Root) {
    $base = Join-Path $Root 'base'
    if (-not (Test-Path -LiteralPath $base -PathType Container)) { return $null }
    $files = @(Get-ChildItem -LiteralPath $base -Recurse -File -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '(?i)\\save\\' } |
        Sort-Object FullName)
    if ($files.Count -eq 0) { return $null }
    $pk4 = @($files | Where-Object { $_.Extension -ieq '.pk4' } | Sort-Object Name)
    $rows = @()
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($base.Length).TrimStart('\', '/') -replace '\\', '/'
        $rows += [ordered]@{
            path = $rel
            bytes = $f.Length
            sha256 = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $primary = $null
    if ($pk4.Count -gt 0) { $primary = $pk4[0].FullName }
    return [ordered]@{
        base = $base
        primaryPk4 = $primary
        primarySha256 = if ($primary) { (Get-FileHash -LiteralPath $primary -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
        files = $rows
    }
}

function Test-Pk4Entry([string]$Pk4, [string]$Entry) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    $z = [IO.Compression.ZipFile]::OpenRead($Pk4)
    try {
        foreach ($e in $z.Entries) {
            if ([string]::Equals($e.FullName, $Entry, [StringComparison]::OrdinalIgnoreCase)) { return $true }
        }
        return $false
    } finally { $z.Dispose() }
}

function Test-SceneData([object]$Scene) {
    $map = $Scene.Map
    $suffixes = @('.map', '.proc', '.cm')
    if ($script:DataInfo -and $script:DataInfo.primaryPk4 -and (Test-Path -LiteralPath $script:DataInfo.primaryPk4)) {
        foreach ($s in $suffixes) {
            if (Test-Pk4Entry $script:DataInfo.primaryPk4 ("maps/$map$s")) { return $true }
        }
        return $false
    }
    if ($script:DataDirFull) {
        foreach ($s in $suffixes) {
            if (Test-Path -LiteralPath (Join-Path $script:DataDirFull ("base\maps\$map$s"))) { return $true }
        }
    }
    return $false
}

function Resolve-Scenes([string[]]$Requested) {
    if (-not $Requested -or $Requested.Count -eq 0) { return @($script:SceneTable) }
    $seen = @{}
    $selected = @()
    foreach ($id in $Requested) {
        if (-not $id -or -not $id.Trim()) { Fail 6 'Scenes contains an empty id' }
        $key = $id.Trim().ToLowerInvariant()
        if ($seen.ContainsKey($key)) { Fail 6 "Scenes contains a duplicate id: $key" }
        $seen[$key] = $true
        $match = @($script:SceneTable | Where-Object { $_.Id -ieq $key })
        if ($match.Count -ne 1) { Fail 6 "Unknown scene id '$key'. Known ids: $((@($script:SceneTable | ForEach-Object { $_.Id })) -join ', ')" }
        $selected += $match[0]
    }
    return $selected
}

function Get-PrimaryPk4([string]$Root) {
    $base = Join-Path $Root 'base'
    if (-not (Test-Path -LiteralPath $base -PathType Container)) { return $null }
    $pk4 = @(Get-ChildItem -LiteralPath $base -File -Filter '*.pk4' -ErrorAction SilentlyContinue | Sort-Object Name)
    if ($pk4.Count -eq 0) { return $null }
    return $pk4[0].FullName
}

function Invoke-Check([object[]]$Selected) {
    if (-not $OutDir) { Fail 2 'OutDir is required' }

    # Resolve the paths used by the output guard before doing any expensive or destructive work.
    if ($Exe -and -not $script:ExeFull) { $script:ExeFull = Get-FullPath $Exe }
    if ($DataDir -and -not $script:DataDirFull) { $script:DataDirFull = Get-FullPath $DataDir }

    if ($Mode -eq 'Compare') {
        # Compare reads an existing capture tree; it does not need a live exe or data root, but it
        # still refuses to write over a previous report or into the data/exe trees.
        $script:OutDirFull = Assert-SafeOutDir $OutDir
        if (-not (Test-Path -LiteralPath $script:OutDirFull -PathType Container)) { Fail 6 "OutDir does not exist: $script:OutDirFull" }
        $report = Join-Path $script:OutDirFull 'comparison.json'
        if (Test-Path -LiteralPath $report -PathType Leaf) {
            Fail 5 "refusing to overwrite existing comparison report: $report (use a new OutDir)"
        }
        Write-Line "BASELINE mode=Compare inputs=OK outdir=$($script:OutDirFull)"
        return $Selected
    }

    # >>> Q0-REFUSAL-ANCHOR (tests/baseline mutation harness depends on this block)
    # Refusal-first: the output location is checked before the 483 MB data hash or any process work.
    $script:OutDirFull = Assert-SafeOutDir $OutDir
    if (Test-Path -LiteralPath $script:OutDirFull -PathType Leaf) { Fail 5 "OutDir is a file: $script:OutDirFull" }
    $parent = Split-Path -Parent $script:OutDirFull
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { Fail 2 "OutDir parent does not exist: $parent" }
    $script:PendingArchive = $false
    if (Test-Path -LiteralPath $script:OutDirFull -PathType Container) {
        $existing = @(Get-ChildItem -LiteralPath $script:OutDirFull -Force -ErrorAction SilentlyContinue)
        if ($existing.Count -gt 0) {
            if ($Mode -eq 'Check') {
                Write-Line "BASELINE refusing to inspect a non-empty OutDir: $script:OutDirFull (pass -Clean only with Capture/All)"
                exit 5
            }
            if (-not $Clean) {
                Write-Line "BASELINE refusing to overwrite non-empty OutDir: $script:OutDirFull (pass -Clean)"
                exit 5
            }
            $script:PendingArchive = $true
        }
    }
    # <<< Q0-REFUSAL-ANCHOR

    if (-not $Exe) {
        . (Join-Path $PSScriptRoot '_common.ps1')
        $Exe = Resolve-Doom3Exe -Repo $script:RepoRoot
    }
    $script:ExeFull = Get-FullPath $Exe
    if (-not (Test-Path -LiteralPath $script:ExeFull -PathType Leaf)) { Fail 3 "executable missing: $script:ExeFull" }
    $exeInfo = Get-Item -LiteralPath $script:ExeFull
    if ($exeInfo.Length -le 0) { Fail 3 "executable is empty: $script:ExeFull" }
    $script:ExeIdentity = Get-ExeIdentity $script:ExeFull
    if ($ExpectedExeSha256) {
        $expected = $ExpectedExeSha256.Trim().ToLowerInvariant()
        if ($script:ExeIdentity.sha256 -ne $expected) {
            Fail 7 "executable SHA-256 mismatch: expected $expected, got $($script:ExeIdentity.sha256)"
        }
    }
    if ($Mode -in @('Capture', 'All') -and $script:ExeIdentity.peMachine -ne '0x8664') {
        Fail 3 "executable is not x64 (PE machine $($script:ExeIdentity.peMachine)): $script:ExeFull"
    }

    if (-not $DataDir) { $DataDir = $env:D3_DATA_DIR }
    if (-not $DataDir -or -not (Test-Path -LiteralPath $DataDir -PathType Container)) {
        if ($AllowUnavailableData) { Skip 'game data directory is unavailable' }
        Fail 4 'game data directory is missing (set -DataDir or D3_DATA_DIR)'
    }
    $script:DataDirFull = Get-FullPath $DataDir
    $script:DataInfo = Get-DataIdentity $script:DataDirFull
    if (-not $script:DataInfo) {
        if ($AllowUnavailableData) { Skip "game data directory has no base files: $script:DataDirFull" }
        Fail 4 "game data directory has no base files: $script:DataDirFull"
    }
    if ($ExpectedDataSha256) {
        $expected = $ExpectedDataSha256.Trim().ToLowerInvariant()
        if (-not $script:DataInfo.primarySha256 -or $script:DataInfo.primarySha256 -ne $expected) {
            Fail 4 "primary pk4 SHA-256 mismatch: expected $expected, got $($script:DataInfo.primarySha256)"
        }
    }

    # Re-check the guard now that the resolved exe/data paths are known (covers the default exe).
    $script:OutDirFull = Assert-SafeOutDir $OutDir

    foreach ($scene in $Selected) {
        if ($scene.Status -like 'blocked*') { continue }
        if (-not (Test-SceneData $scene)) {
            Fail 6 "scene input missing from data: scene=$($scene.Id) map=$($scene.Map)"
        }
    }

    if ($script:PendingArchive -and $Clean -and $Mode -in @('Capture', 'All')) {
        $archive = "$($script:OutDirFull).superseded-" + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
        if (Test-Path -LiteralPath $archive) { Fail 5 "archive already exists: $archive" }
        Move-Item -LiteralPath $script:OutDirFull -Destination $archive
        Write-Line "BASELINE archived previous OutDir to $archive"
    }

    Write-Line "BASELINE mode=$Mode result=PASS exe_sha256=$($script:ExeIdentity.sha256) exe_bytes=$($script:ExeIdentity.bytes) pe=$($script:ExeIdentity.peMachine)"
    Write-Line "BASELINE data_primary=$($script:DataInfo.primarySha256) files=$($script:DataInfo.files.Count) outdir=$($script:OutDirFull)"
    Write-Line "BASELINE scenes=$((@($Selected | ForEach-Object { $_.Id })) -join ',')"
    return $Selected
}

function Quote-NativeArg([string]$Value) {
    if ($Value -match '[\s"]') { return '"' + ($Value -replace '"', '\"') + '"' }
    return $Value
}

function Read-LogText([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    try { return Get-Content -LiteralPath $Path -Raw -ErrorAction Stop } catch { return '' }
}

function Wait-ForLogMarkers([string]$Log, [string[]]$Markers, [int]$Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $text = Read-LogText $Log
        $ok = $true
        foreach ($m in $Markers) { if ($text -notmatch [regex]::Escape($m)) { $ok = $false; break } }
        if ($ok) { return $text }
        Start-Sleep -Milliseconds 500
    }
    return (Read-LogText $Log)
}

function Invoke-EngineCaptureRun([object]$Scene, [string]$RunDir, [int]$RunIndex) {
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    $log = Join-Path $RunDir 'save\base\qconsole.log'
    $shot = Join-Path $RunDir 'screenshot.png'
    $shotName = 'q0_' + ($Scene.Id -replace '[^A-Za-z0-9]', '_')
    $tgaRel = "screenshots/$shotName.tga"
    $tga = Join-Path $RunDir "save\base\screenshots\$shotName.tga"
    $a = @(
        '+set', 'logFile', '2',
        '+set', 'r_fullscreen', '0',
        '+set', 'r_mode', '-1',
        '+set', 'r_customWidth', "$Width",
        '+set', 'r_customHeight', "$Height",
        '+set', 'com_fixedTic', '1',
        '+set', 'fs_basepath', $script:DataDirFull,
        '+set', 'fs_savepath', (Join-Path $RunDir 'save')
    ) + @($Scene.Drive) + @('+screenshot', $tgaRel, '+wait', '30', '+quit')
    $argString = ($a | ForEach-Object { Quote-NativeArg ([string]$_) }) -join ' '
    $started = [DateTime]::UtcNow
    $p = Start-Process -FilePath $script:ExeFull -ArgumentList $argString -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec + 60)
    while ([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited) { Start-Sleep -Milliseconds 500; $p.Refresh() }
    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        $stopDeadline = [DateTime]::UtcNow.AddSeconds(10)
        while (-not $p.HasExited -and [DateTime]::UtcNow -lt $stopDeadline) { Start-Sleep -Milliseconds 200; $p.Refresh() }
    }
    $processExit = if ($p.HasExited) { $p.ExitCode } else { $null }
    $logText = Read-LogText $log
    if ($logText) { [IO.File]::WriteAllText((Join-Path $RunDir 'qconsole.log'), $logText, (New-Object Text.UTF8Encoding($false))) }
    $captureExit = -1
    $colours = 0
    $centreMean = 0.0
    $convertError = $null
    if (Test-Path -LiteralPath $tga) {
        try {
            [BaselineImageDiff]::TgaToPng($tga, $shot)
            $captureExit = 0
            $colours = [BaselineImageDiff]::Distinct($shot)
            $stats = [BaselineImageDiff]::Stats($shot)
            if ($stats -match '^OK \d+ \d+ ([0-9.]+) ([0-9.]+)$') { $centreMean = [double]$Matches[2] }
        } catch {
            $convertError = $_.Exception.Message
        }
    }
    $missing = @()
    foreach ($m in $Scene.Markers) { if ($logText -notmatch [regex]::Escape($m)) { $missing += $m } }
    $forbidden = @()
    foreach ($m in $Scene.Forbidden) { if ($logText -match [regex]::Escape($m)) { $forbidden += $m } }
    $status = 'PASS'
    $reason = ''
    if (-not (Test-Path -LiteralPath $tga)) { $status = 'FAIL'; $reason = "engine screenshot missing: $tgaRel" }
    elseif ($convertError) { $status = 'FAIL'; $reason = "TGA conversion failed: $convertError" }
    elseif ($processExit -ne 0) { $status = 'FAIL'; $reason = "engine exit=$processExit" }
    elseif ($colours -lt 50 -or $centreMean -lt 2.0) { $status = 'FAIL'; $reason = "screenshot rejected (colours=$colours centreMean=$centreMean)" }
    elseif ($missing.Count -gt 0) { $status = 'FAIL'; $reason = "missing log marker(s): $($missing -join ', ')" }
    elseif ($forbidden.Count -gt 0) { $status = 'FAIL'; $reason = "forbidden log text: $($forbidden -join ', ')" }
    $shotHash = if (Test-Path -LiteralPath $shot) { (Get-FileHash -LiteralPath $shot -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    return [ordered]@{
        scene = $Scene.Id; run = $RunIndex; status = $status; reason = $reason
        processExit = $processExit; windowFound = $false
        startedUtc = $started.ToString('o'); endedUtc = [DateTime]::UtcNow.ToString('o')
        command = $argString; captureExit = $captureExit; distinctColours = $colours; centreMean = $centreMean
        captureMethod = 'Engine'
        screenshot = if (Test-Path -LiteralPath $shot) { 'screenshot.png' } else { $null }
        screenshotSha256 = $shotHash
        logMarkers = @($Scene.Markers); logMissing = @($missing); logForbidden = @($forbidden)
    }
}
function Invoke-CaptureRun([object]$Scene, [string]$RunDir, [int]$RunIndex) {
    if ($CaptureMethod -eq 'Engine') { return Invoke-EngineCaptureRun $Scene $RunDir $RunIndex }
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    $log = Join-Path $RunDir 'save\base\qconsole.log'
    $shot = Join-Path $RunDir 'screenshot.png'
    $a = @(
        '+set', 'logFile', '2',
        '+set', 'r_fullscreen', '0',
        '+set', 'r_mode', '-1',
        '+set', 'r_customWidth', "$Width",
        '+set', 'r_customHeight', "$Height",
        '+set', 'fs_basepath', $script:DataDirFull,
        '+set', 'fs_savepath', (Join-Path $RunDir 'save')
    ) + @($Scene.Drive)
    $argString = ($a | ForEach-Object { Quote-NativeArg ([string]$_) }) -join ' '
    $started = [DateTime]::UtcNow
    $p = Start-Process -FilePath $script:ExeFull -ArgumentList $argString -PassThru
    $windowFound = $false
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited) {
        $p.Refresh()
        if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $windowFound = $true; break }
        Start-Sleep -Milliseconds 500
    }
    $logText = ''
    if ($windowFound) {
        $logText = Wait-ForLogMarkers $log $Scene.Markers $TimeoutSec
        Start-Sleep -Seconds $SettleSec
        if ($p.HasExited) {
            $code = $p.ExitCode
            return [ordered]@{
                scene = $Scene.Id; run = $RunIndex; status = 'FAIL'; reason = 'process died during settle'
                processExit = $code; windowFound = $windowFound; startedUtc = $started.ToString('o')
                endedUtc = [DateTime]::UtcNow.ToString('o'); command = $argString; centreMean = $null
            }
        }
        $cap = & (Join-Path $PSScriptRoot 'capture-window.ps1') -ProcessId $p.Id -Out $shot
        $capCode = $LASTEXITCODE
        $colours = 0
        $centreMean = 0.0
        if ("$cap" -match 'distinctColours=(\d+)') { $colours = [int]$Matches[1] }
        if (Test-Path -LiteralPath $shot) {
            $stats = [BaselineImageDiff]::Stats($shot)
            if ($stats -match '^OK \d+ \d+ ([0-9.]+) ([0-9.]+)$') { $centreMean = [double]$Matches[2] }
        }
        $shotOk = ($capCode -eq 0 -and (Test-Path -LiteralPath $shot) -and $colours -ge 50 -and $centreMean -ge 2.0)
    } else {
        $shotOk = $false
        $capCode = -1
        $colours = 0
        $centreMean = 0.0
        $cap = 'no window'
    }
    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        $stopDeadline = [DateTime]::UtcNow.AddSeconds(10)
        while (-not $p.HasExited -and [DateTime]::UtcNow -lt $stopDeadline) { Start-Sleep -Milliseconds 200; $p.Refresh() }
    }
    $processExit = if ($p.HasExited) { $p.ExitCode } else { $null }
    $logText = Read-LogText $log
    if ($logText) { [IO.File]::WriteAllText((Join-Path $RunDir 'qconsole.log'), $logText, (New-Object Text.UTF8Encoding($false))) }
    $missing = @()
    foreach ($m in $Scene.Markers) { if ($logText -notmatch [regex]::Escape($m)) { $missing += $m } }
    $forbidden = @()
    foreach ($m in $Scene.Forbidden) { if ($logText -match [regex]::Escape($m)) { $forbidden += $m } }
    $status = 'PASS'
    $reason = ''
    if (-not $windowFound) { $status = 'FAIL'; $reason = 'no window' }
    elseif (-not $shotOk) { $status = 'FAIL'; $reason = "screenshot failed (capture=$capCode colours=$colours centreMean=$centreMean)" }
    elseif ($missing.Count -gt 0) { $status = 'FAIL'; $reason = "missing log marker(s): $($missing -join ', ')" }
    elseif ($forbidden.Count -gt 0) { $status = 'FAIL'; $reason = "forbidden log text: $($forbidden -join ', ')" }
    $shotHash = if (Test-Path -LiteralPath $shot) { (Get-FileHash -LiteralPath $shot -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    return [ordered]@{
        scene = $Scene.Id; run = $RunIndex; status = $status; reason = $reason
        processExit = $processExit; windowFound = $windowFound
        startedUtc = $started.ToString('o'); endedUtc = [DateTime]::UtcNow.ToString('o')
        command = $argString; captureExit = $capCode; distinctColours = $colours; centreMean = $centreMean
        screenshot = if (Test-Path -LiteralPath $shot) { 'screenshot.png' } else { $null }
        screenshotSha256 = $shotHash
        logMarkers = @($Scene.Markers); logMissing = @($missing); logForbidden = @($forbidden)
    }
}

function Write-Json([string]$Path, [object]$Value) {
    $json = $Value | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText($Path, $json, (New-Object Text.UTF8Encoding($false)))
}

function Invoke-Capture([object[]]$Selected) {
    New-Item -ItemType Directory -Force -Path $script:OutDirFull | Out-Null
    $running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ([IO.Path]::GetFullPath($_.Path) -ieq $script:ExeFull) } catch { $false }
    })
    if ($running.Count -gt 0) {
        if (-not $StopExisting) { Fail 7 "$($running.Count) instance(s) of the reference executable are running; pass -StopExisting to stop only those" }
        . (Join-Path $PSScriptRoot '_common.ps1')
        Stop-ProcessesUsingExe -Path $script:ExeFull
    }

    $results = @()
    foreach ($scene in $Selected) {
        if ($scene.Status -like 'blocked*') {
            $results += [ordered]@{
                scene = $scene.Id; run = 0; status = 'SKIPPED'; reason = $scene.Status
                processExit = $null; windowFound = $false; startedUtc = $null; endedUtc = $null
                command = $null; captureExit = $null; distinctColours = $null
                screenshot = $null; screenshotSha256 = $null
                logMarkers = @(); logMissing = @(); logForbidden = @()
            }
            Write-Line "BASELINE capture scene=$($scene.Id) run=0 status=SKIPPED reason=$($scene.Status)"
            continue
        }
        for ($i = 1; $i -le $Repeat; $i++) {
            $runDir = Join-Path $script:OutDirFull ($scene.Id + '\run' + $i)
            Write-Line "BASELINE capture scene=$($scene.Id) run=$i dir=$runDir"
            $r = Invoke-CaptureRun $scene $runDir $i
            $results += $r
            Write-Line "BASELINE capture scene=$($scene.Id) run=$i status=$($r.status) reason=$($r.reason)"
            if ($r.status -ne 'PASS') { break }
        }
    }
    $sceneIds = (@($Selected | ForEach-Object { $_.Id })) -join ','
    $commandLine = "$($PSCommandPath) -Mode $Mode -CaptureMethod $CaptureMethod -Exe $($script:ExeFull) -DataDir $($script:DataDirFull) -OutDir $($script:OutDirFull) -OutRoot $OutRoot -Scenes $sceneIds -Width $Width -Height $Height -SettleSec $SettleSec -TimeoutSec $TimeoutSec -Repeat $Repeat -Delta $Delta -ExpectedExeSha256 $ExpectedExeSha256 -ExpectedDataSha256 $ExpectedDataSha256 -MaxMeanAbs $MaxMeanAbs -MaxDiffPercent $MaxDiffPercent -Clean:$Clean -StopExisting:$StopExisting -AllowUnavailableData:$AllowUnavailableData"
    $testPath = Join-Path $script:RepoRoot 'tests\baseline\test-render-baseline.ps1'
    $launcherPath = Join-Path $script:RepoRoot 'tests\renderer\test-render-baseline.ps1'
    $scriptIdentity = [ordered]@{
        path = $PSCommandPath
        sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
        testSha256 = if (Test-Path -LiteralPath $testPath) { (Get-FileHash -LiteralPath $testPath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
        launcherSha256 = if (Test-Path -LiteralPath $launcherPath) { (Get-FileHash -LiteralPath $launcherPath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    }
    $manifest = [ordered]@{
        schema = $script:Schema
        generatedUtc = [DateTime]::UtcNow.ToString('o')
        mode = 'Capture'
        commandLine = $commandLine
        script = $scriptIdentity
        exe = $script:ExeIdentity
        data = [ordered]@{ primarySha256 = $script:DataInfo.primarySha256; files = $script:DataInfo.files }
        settings = [ordered]@{ width = $Width; height = $Height; repeat = $Repeat; timeoutSec = $TimeoutSec; settleSec = $SettleSec; maxMeanAbsOverride = $MaxMeanAbs; maxDiffPercentOverride = $MaxDiffPercent; delta = $Delta }
        scenes = @($Selected | ForEach-Object { [ordered]@{ id = $_.Id; category = $_.Category; map = $_.Map; drive = $_.Drive; status = $_.Status; maxMeanAbs = $_.MaxMeanAbs; maxDiffPercent = $_.MaxDiffPercent } })
        results = $results
    }
    Write-Json (Join-Path $script:OutDirFull 'baseline-manifest.json') $manifest
    $failed = @($results | Where-Object { $_.status -ne 'PASS' -and $_.status -ne 'SKIPPED' })
    $skipped = @($results | Where-Object { $_.status -eq 'SKIPPED' })
    if ($failed.Count -gt 0) { Fail 8 "capture failed for $($failed.Count) run(s); see baseline-manifest.json" }
    if ($skipped.Count -gt 0) { Write-Line "BASELINE capture result=PASS-WITH-SKIPS skipped=$($skipped.Count)" }
    else { Write-Line 'BASELINE capture result=PASS' }
}

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.IO;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BaselineImageDiff {
    public static string Compare(string pathA, string pathB, int delta) {
        using (var a = new Bitmap(pathA))
        using (var b = new Bitmap(pathB)) {
            if (a.Width != b.Width || a.Height != b.Height) {
                return string.Format("SIZE_MISMATCH {0}x{1} vs {2}x{3}", a.Width, a.Height, b.Width, b.Height);
            }
            var rect = new Rectangle(0, 0, a.Width, a.Height);
            var da = a.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
            var db = b.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
            try {
                long total = (long)a.Width * a.Height;
                long changed = 0;
                double sum = 0.0;
                int stride = da.Stride;
                byte[] rowA = new byte[stride];
                byte[] rowB = new byte[stride];
                for (int y = 0; y < a.Height; y++) {
                    Marshal.Copy(IntPtr.Add(da.Scan0, y * stride), rowA, 0, stride);
                    Marshal.Copy(IntPtr.Add(db.Scan0, y * stride), rowB, 0, stride);
                    for (int x = 0; x < a.Width; x++) {
                        int o = x * 3;
                        int dr = Math.Abs(rowA[o] - rowB[o]);
                        int dg = Math.Abs(rowA[o + 1] - rowB[o + 1]);
                        int dbv = Math.Abs(rowA[o + 2] - rowB[o + 2]);
                        int m = Math.Max(dr, Math.Max(dg, dbv));
                        sum += (dr + dg + dbv) / 3.0;
                        if (m >= delta) changed++;
                    }
                }
                double percent = total == 0 ? 0.0 : (100.0 * changed / total);
                double meanAbs = total == 0 ? 0.0 : (sum / total);
                return string.Format("OK {0} {1} {2:F4} {3:F4}", a.Width, a.Height, percent, meanAbs);
            } finally {
                a.UnlockBits(da);
                b.UnlockBits(db);
            }
        }
    }

    public static string Stats(string path) {
        using (var bmp = new Bitmap(path)) {
            var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
            var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
            try {
                int stride = data.Stride;
                byte[] row = new byte[stride];
                double sum = 0.0;
                double centre = 0.0;
                long total = 0;
                long centreCount = 0;
                int x0 = bmp.Width / 4;
                int x1 = (bmp.Width * 3) / 4;
                int y0 = bmp.Height / 4;
                int y1 = (bmp.Height * 3) / 4;
                for (int y = 0; y < bmp.Height; y++) {
                    Marshal.Copy(IntPtr.Add(data.Scan0, y * stride), row, 0, stride);
                    for (int x = 0; x < bmp.Width; x++) {
                        int o = x * 3;
                        double luma = 0.2126 * row[o + 2] + 0.7152 * row[o + 1] + 0.0722 * row[o];
                        sum += luma;
                        total++;
                        if (x >= x0 && x < x1 && y >= y0 && y < y1) {
                            centre += luma;
                            centreCount++;
                        }
                    }
                }
                double mean = total == 0 ? 0.0 : (sum / total);
                double centreMean = centreCount == 0 ? 0.0 : (centre / centreCount);
                return string.Format("OK {0} {1} {2:F4} {3:F4}", bmp.Width, bmp.Height, mean, centreMean);
            } finally {
                bmp.UnlockBits(data);
            }
        }
    }

    public static int Distinct(string path) {
        using (var bmp = new Bitmap(path)) {
            var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
            var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
            try {
                var seen = new HashSet<int>();
                int stride = data.Stride;
                byte[] row = new byte[stride];
                for (int y = 0; y < bmp.Height; y++) {
                    Marshal.Copy(IntPtr.Add(data.Scan0, y * stride), row, 0, stride);
                    for (int x = 0; x < bmp.Width; x++) {
                        int o = x * 3;
                        seen.Add((row[o + 2] << 16) | (row[o + 1] << 8) | row[o]);
                    }
                }
                return seen.Count;
            } finally {
                bmp.UnlockBits(data);
            }
        }
    }

    // DOOM3 writes uncompressed true-colour (24- or 32-bit) bottom-left-origin TGA. Convert it once so the existing
    // PNG comparison code can read the engine framebuffer instead of an OS window capture.
    public static void TgaToPng(string tgaPath, string pngPath) {
        byte[] b = File.ReadAllBytes(tgaPath);
        if (b.Length < 18) { throw new InvalidDataException("TGA header too short"); }
        int idLength = b[0];
        int colourMap = b[1];
        int imageType = b[2];
        if (colourMap != 0 || imageType != 2) {
            throw new InvalidDataException("unsupported TGA type " + imageType + " (expected uncompressed true-colour)");
        }
        int width = BitConverter.ToUInt16(b, 12);
        int height = BitConverter.ToUInt16(b, 14);
        int bpp = b[16];
        int desc = b[17];
        if (bpp != 24 && bpp != 32) { throw new InvalidDataException("unsupported TGA bpp " + bpp); }
        int bytesPerPixel = bpp / 8;
        int offset = 18 + idLength;
        int rowBytes = width * bytesPerPixel;
        if (offset + rowBytes * height > b.Length) { throw new InvalidDataException("TGA data truncated"); }
        using (var bmp = new Bitmap(width, height, PixelFormat.Format24bppRgb)) {
            var rect = new Rectangle(0, 0, width, height);
            var data = bmp.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
            try {
                int stride = data.Stride;
                for (int y = 0; y < height; y++) {
                    int srcY = ((desc & 0x20) != 0) ? y : (height - 1 - y);
                    int src = offset + srcY * rowBytes;
                    IntPtr dst = IntPtr.Add(data.Scan0, y * stride);
                    if (bytesPerPixel == 3) {
                        Marshal.Copy(b, src, dst, rowBytes);
                    } else {
                        byte[] row = new byte[width * 3];
                        for (int x = 0; x < width; x++) {
                            row[x * 3] = b[src + x * 4];
                            row[x * 3 + 1] = b[src + x * 4 + 1];
                            row[x * 3 + 2] = b[src + x * 4 + 2];
                        }
                        Marshal.Copy(row, 0, dst, row.Length);
                    }
                }
            } finally {
                bmp.UnlockBits(data);
            }
            bmp.Save(pngPath, ImageFormat.Png);
        }
    }
}
"@

function Invoke-Compare([object[]]$Selected) {
    if (-not (Test-Path -LiteralPath $script:OutDirFull -PathType Container)) { Fail 6 "OutDir does not exist: $script:OutDirFull" }
    $contract = @{}
    $manifestPath = Join-Path $script:OutDirFull 'baseline-manifest.json'
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        foreach ($s in $manifest.scenes) { $contract[$s.id] = $s }
    }
    $rows = @()
    $failed = 0
    foreach ($scene in $Selected) {
        if ($scene.Status -like 'blocked*') {
            $rows += [ordered]@{
                scene = $scene.Id; status = 'SKIPPED'; size = ''; diffPercent = $null; meanAbs = $null
                delta = $Delta; maxMeanAbs = $scene.MaxMeanAbs; maxDiffPercent = $scene.MaxDiffPercent
                logMarkersMatch = $false; reason = $scene.Status
            }
            Write-Line "BASELINE compare scene=$($scene.Id) status=SKIPPED reason=$($scene.Status)"
            continue
        }
        $contractMean = if ($contract.ContainsKey($scene.Id)) { [double]$contract[$scene.Id].maxMeanAbs } else { $scene.MaxMeanAbs }
        $contractDiff = if ($contract.ContainsKey($scene.Id)) { [double]$contract[$scene.Id].maxDiffPercent } else { $scene.MaxDiffPercent }
        if ($contract.ContainsKey($scene.Id) -and ($contractMean -ne $scene.MaxMeanAbs -or $contractDiff -ne $scene.MaxDiffPercent)) {
            Fail 6 "scene contract changed between capture and compare: $($scene.Id) manifest=$contractMean/$contractDiff table=$($scene.MaxMeanAbs)/$($scene.MaxDiffPercent)"
        }
        $meanLimit = if ($MaxMeanAbs -gt 0) { $MaxMeanAbs } else { $contractMean }
        $diffLimit = if ($MaxDiffPercent -gt 0) { $MaxDiffPercent } else { $contractDiff }
        $dir1 = Join-Path $script:OutDirFull ($scene.Id + '\run1')
        $dir2 = Join-Path $script:OutDirFull ($scene.Id + '\run2')
        $shot1 = Join-Path $dir1 'screenshot.png'
        $shot2 = Join-Path $dir2 'screenshot.png'
        $log1 = Join-Path $dir1 'qconsole.log'
        $log2 = Join-Path $dir2 'qconsole.log'
        if (-not (Test-Path -LiteralPath $shot1) -or -not (Test-Path -LiteralPath $shot2)) {
            Fail 6 "comparison input missing for scene $($scene.Id): run1/run2 screenshot.png"
        }
        if (-not (Test-Path -LiteralPath $log1) -or -not (Test-Path -LiteralPath $log2)) {
            Fail 6 "comparison input missing for scene $($scene.Id): run1/run2 qconsole.log"
        }
        $l1 = Read-LogText $log1
        $l2 = Read-LogText $log2
        $logOk = $true
        foreach ($m in $scene.Markers) {
            if (($l1 -match [regex]::Escape($m)) -ne ($l2 -match [regex]::Escape($m))) { $logOk = $false }
            if ($l1 -notmatch [regex]::Escape($m)) { $logOk = $false }
            if ($l2 -notmatch [regex]::Escape($m)) { $logOk = $false }
        }
        $forbiddenFound = @()
        foreach ($m in $scene.Forbidden) {
            if ($l1 -match [regex]::Escape($m) -or $l2 -match [regex]::Escape($m)) { $forbiddenFound += $m; $logOk = $false }
        }
        $raw = [BaselineImageDiff]::Compare($shot1, $shot2, $Delta)
        $status = 'FAIL'
        $diffPercent = $null
        $meanAbs = $null
        $size = ''
        if ($raw -match '^OK (\d+) (\d+) ([0-9.]+) ([0-9.]+)$') {
            $size = "$($Matches[1])x$($Matches[2])"
            $diffPercent = [double]$Matches[3]
            $meanAbs = [double]$Matches[4]
            if ($logOk -and $meanAbs -le $meanLimit -and $diffPercent -le $diffLimit) { $status = 'PASS' }
        }
        if ($status -ne 'PASS') { $failed++ }
        $rows += [ordered]@{
            scene = $scene.Id; status = $status; size = $size; diffPercent = $diffPercent; meanAbs = $meanAbs
            delta = $Delta; maxMeanAbs = $meanLimit; maxDiffPercent = $diffLimit; logMarkersMatch = $logOk
            logForbidden = @($forbiddenFound)
        }
        Write-Line ("BASELINE compare scene={0} status={1} size={2} meanAbs={3} diffPercent={4} maxMeanAbs={5} maxDiffPercent={6} logMarkersMatch={7}" -f $scene.Id, $status, $size, $meanAbs, $diffPercent, $meanLimit, $diffLimit, $logOk)
    }
    $report = [ordered]@{
        schema = $script:Schema
        generatedUtc = [DateTime]::UtcNow.ToString('o')
        mode = 'Compare'
        settings = [ordered]@{ maxMeanAbsOverride = $MaxMeanAbs; maxDiffPercentOverride = $MaxDiffPercent; delta = $Delta }
        rows = $rows
    }
    Write-Json (Join-Path $script:OutDirFull 'comparison.json') $report
    if ($failed -gt 0) { Fail 9 "repeatability comparison failed for $failed scene(s); see comparison.json" }
    Write-Line 'BASELINE compare result=PASS'
}

try {
    $script:ExeFull = $null
    $script:DataDirFull = $null
    if ($Mode -eq 'ListScenes') {
        foreach ($s in $script:SceneTable) {
            Write-Line ("SCENE {0} category='{1}' map='{2}' status='{3}' drive='{4}'" -f $s.Id, $s.Category, $s.Map, $s.Status, ($s.Drive -join ' '))
        }
        Write-Line 'BASELINE mode=ListScenes result=PASS'
        exit 0
    }
    $Selected = Resolve-Scenes $Scenes
    Invoke-Check $Selected | Out-Null
    if ($Mode -eq 'Check') { exit 0 }
    if ($Mode -eq 'Capture') { Invoke-Capture $Selected; exit 0 }
    if ($Mode -eq 'Compare') { Invoke-Compare $Selected; exit 0 }
    if ($Mode -eq 'All') { Invoke-Capture $Selected; Invoke-Compare $Selected; exit 0 }
    Fail 2 "unsupported mode: $Mode"
} catch {
    Write-Line "BASELINE result=ERROR $($_.Exception.Message)"
    exit 11
}
