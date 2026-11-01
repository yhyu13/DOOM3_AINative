#Requires -Version 5.1
<#
.SYNOPSIS
  Verify the HD-asset run and the legacy-OpenGL gate with PASS/FAIL/SKIPPED lines.
.DESCRIPTION
  Requires the run output produced by scripts/run-win11-amd64.ps1 (qconsole.log + screenshot.png)
  for the HD run and, for the image comparison, a baseline run without the Wulfen pack.
  Exit 0 only when every required check passes; 1 when any check fails. -BaseOutDir is mandatory.
.EXAMPLE
  .\scripts\verify-hd-run.ps1 -DataDir <data-root>\run-hd -OutDir <data-root>\out-hd -BaseOutDir <data-root>\out-base
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [Parameter(Mandatory = $true)][string]$BaseOutDir,
    [string]$Exe,
    [double]$Threshold = 1.0
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $repo 'build-hd\bin\Release\DOOM3.exe' }
$targetNeo = Join-Path $repo 'targets\win11-amd64-hd\doom3\neo'
$log = Join-Path $OutDir 'qconsole.log'
$shot = Join-Path $OutDir 'screenshot.png'
$fails = 0
function Report([string]$Id, [string]$Result, [string]$Detail) {
    Write-Host ("{0,-8} {1} {2}" -f $Id, $Result, $Detail)
    if ($Result -eq 'FAIL') { $script:fails++ }
}
function Read-Log([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    return (Get-Content -LiteralPath $Path -Raw)
}
function Invoke-PythonProbe([string]$Exe, [string[]]$Prefix, [string]$Code) {
    # Native stderr redirected through 2>&1 becomes a terminating NativeCommandError under
    # $ErrorActionPreference = 'Stop' on PowerShell 5.1, so relax the preference for the probe.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $text = (& $Exe @Prefix -c $Code 2>&1 | Out-String).Trim()
        $code = $LASTEXITCODE
    } catch {
        $text = $_.Exception.Message
        $code = 1
    } finally {
        $ErrorActionPreference = $previous
    }
    $lastLine = @($text -split "`r?`n" | Where-Object { $_ -ne '' } | Select-Object -Last 1)
    return [pscustomobject]@{ Code = $code; Text = $text; LastLine = $(if ($lastLine.Count) { $lastLine[0] } else { '' }) }
}
function Resolve-Python3WithPillow {
    <#
      Resolve the interpreter for compare-screenshots.py: prefer the py launcher with -3, then a
      python on PATH. A candidate counts only when it is Python 3 and can import Pillow, so a
      failure names the missing interpreter or package instead of a launcher quirk.
    #>
    $candidates = @()
    $launcher = Get-Command py -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($launcher) { $candidates += [pscustomobject]@{ Label = 'py -3'; Exe = $launcher.Source; Prefix = @('-3') } }
    $onPath = Get-Command python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($onPath) { $candidates += [pscustomobject]@{ Label = 'python'; Exe = $onPath.Source; Prefix = @() } }

    $problems = @()
    foreach ($candidate in $candidates) {
        $version = Invoke-PythonProbe $candidate.Exe $candidate.Prefix 'import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)'
        if ($version.Code -ne 0) {
            $problems += ("{0} ({1}) is not Python 3{2}" -f $candidate.Label, $candidate.Exe, $(if ($version.LastLine) { ": $($version.LastLine)" } else { '' }))
            continue
        }
        $pillow = Invoke-PythonProbe $candidate.Exe $candidate.Prefix 'import PIL; print(PIL.__version__)'
        if ($pillow.Code -ne 0) {
            $problems += ("{0} ({1}) cannot import Pillow{2}" -f $candidate.Label, $candidate.Exe, $(if ($pillow.LastLine) { ": $($pillow.LastLine)" } else { '' }))
            continue
        }
        $pythonVersion = Invoke-PythonProbe $candidate.Exe $candidate.Prefix 'import sys; print(sys.version.split()[0])'
        return [pscustomobject]@{
            Exe   = $candidate.Exe
            Prefix = $candidate.Prefix
            Label = ("{0} ({1}, Python {2}, Pillow {3})" -f $candidate.Label, $candidate.Exe, $pythonVersion.Text, $pillow.Text)
        }
    }
    if ($candidates.Count -eq 0) { $problems += 'no Python launcher on PATH: py.exe (py -3) and python.exe are both missing' }
    return [pscustomobject]@{ Exe = $null; Prefix = @(); Label = ''; Problems = $problems }
}

# SC-001: executable exists and is x64.
if (-not (Test-Path -LiteralPath $Exe)) {
    Report 'SC-001' 'FAIL' "exe missing: $Exe"
} else {
    $bytes = [IO.File]::ReadAllBytes($Exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $size = (Get-Item -LiteralPath $Exe).Length
    if ($machine -eq 0x8664) { Report 'SC-001' 'PASS' ("x64 PE 0x8664, {0} bytes" -f $size) }
    else { Report 'SC-001' 'FAIL' ("PE machine 0x{0:X4}, expected 0x8664" -f $machine) }
}

# SC-002a: no new RHI symbols in the engine tree.
if (-not (Test-Path -LiteralPath $targetNeo)) {
    Report 'SC-002a' 'FAIL' "target tree missing: $targetNeo"
} else {
    $pattern = '\bRHI\b|Vulkan|D3D12|ID3D12|IDirect3DDevice'
    $files = @(Get-ChildItem -LiteralPath $targetNeo -Recurse -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.inl', '.c' })
    $hits = @($files | Select-String -Pattern $pattern -CaseSensitive:$false -ErrorAction SilentlyContinue)
    if ($hits.Count -eq 0) { Report 'SC-002a' 'PASS' '0 RHI/Vulkan/D3D12 matches under neo/' }
    else { Report 'SC-002a' 'FAIL' ("{0} RHI-like matches: {1}" -f $hits.Count, (($hits | Select-Object -First 3 | ForEach-Object { $_.Path + ':' + $_.LineNumber }) -join ', ')) }
}

$text = Read-Log $log
if (-not $text) {
    Report 'SC-002b' 'FAIL' "log missing: $log"
    Report 'SC-003' 'FAIL' 'no log'
    Report 'SC-004' 'FAIL' 'no log'
} else {
    # SC-002b: classic OpenGL path actually initialised.
    $gl = @('GL_VENDOR:', 'GL_RENDERER:', 'GL_VERSION:') | Where-Object { $text -notmatch [regex]::Escape($_) }
    if ($gl.Count -gt 0) { Report 'SC-002b' 'FAIL' ("missing log lines: {0}" -f ($gl -join ', ')) }
    elseif ($text -notmatch 'ARB2 path ENABLED') { Report 'SC-002b' 'FAIL' 'ARB2 path not enabled; see log' }
    else {
        $renderer = if ($text -match 'GL_RENDERER:\s*([^\r\n]+)') { $Matches[1].Trim() } else { '?' }
        Report 'SC-002b' 'PASS' ("OpenGL initialised; GL_RENDERER=$renderer; ARB2 enabled")
    }

    # SC-003: both pk4 files loaded, demo first, HD pack staged under the override name.
    $demoLoaded = $text -match 'Loaded pk4[^\r\n]*demo00\.pk4'
    $hdLoaded = $text -match 'Loaded pk4[^\r\n]*zzz_wulfen_texture_pack_2\.0\.pk4'
    $lines = $text -split "`r?`n"
    $demoIdx = -1; $hdIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($demoIdx -lt 0 -and $lines[$i] -match 'Loaded pk4[^\r\n]*demo00\.pk4') { $demoIdx = $i }
        if ($hdIdx -lt 0 -and $lines[$i] -match 'Loaded pk4[^\r\n]*zzz_wulfen_texture_pack_2\.0\.pk4') { $hdIdx = $i }
    }
    $orderOk = ($demoIdx -ge 0 -and $hdIdx -gt $demoIdx)
    $hdPath = Join-Path $DataDir 'base\zzz_wulfen_texture_pack_2.0.pk4'
    $hdFile = Test-Path -LiteralPath $hdPath
    if ($demoLoaded -and $hdLoaded -and $orderOk -and $hdFile) {
        Report 'SC-003' 'PASS' ("demo00.pk4 (line {0}) before zzz_wulfen_texture_pack_2.0.pk4 (line {1})" -f ($demoIdx + 1), ($hdIdx + 1))
    } else {
        Report 'SC-003' 'FAIL' ("demoLoaded={0} hdLoaded={1} order={2} stagedFile={3}" -f $demoLoaded, $hdLoaded, $orderOk, $hdFile)
    }

    # SC-004: map reached the player, process survived (run script would have exited non-zero otherwise).
    if ($text -match 'entities spawned' -and $text -match 'SpawnPlayer') {
        Report 'SC-004a' 'PASS' 'map spawned entities and player'
    } else {
        Report 'SC-004a' 'FAIL' 'entities spawned / SpawnPlayer missing from log'
    }
}

# SC-004b: screenshot is non-blank.
if (-not (Test-Path -LiteralPath $shot)) {
    Report 'SC-004b' 'FAIL' "screenshot missing: $shot"
} else {
    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::FromFile($shot)
    try {
        $set = New-Object 'System.Collections.Generic.HashSet[int]'
        for ($yy = 0; $yy -lt 40; $yy++) {
            for ($xx = 0; $xx -lt 40; $xx++) {
                [void]$set.Add($bmp.GetPixel([int]($xx * ($bmp.Width - 1) / 39), [int]($yy * ($bmp.Height - 1) / 39)).ToArgb())
            }
        }
        if ($set.Count -ge 8) { Report 'SC-004b' 'PASS' ("{0}x{1}, {2} sampled colours" -f $bmp.Width, $bmp.Height, $set.Count) }
        else { Report 'SC-004b' 'FAIL' ("only {0} sampled colours" -f $set.Count) }
    } finally { $bmp.Dispose() }
}

# SC-005: baseline is pack-free with the same forced image settings, then pixel difference.
$baseLog = Join-Path $BaseOutDir 'qconsole.log'
$baseShot = Join-Path $BaseOutDir 'screenshot.png'
function Test-ForcedSettings([string]$CfgPath) {
    if (-not (Test-Path -LiteralPath $CfgPath)) { return "missing config: $CfgPath" }
    $cfg = Get-Content -LiteralPath $CfgPath -Raw
    $missing = @('image_usePrecompressedTextures', 'image_downSize', 'image_downSizeBump', 'image_downSizeSpecular') |
        Where-Object { $cfg -notmatch ('(?m)^seta ' + $_ + '\s+"0"\s*$') }
    if ($missing.Count -gt 0) { return ("cvars not 0: {0}" -f ($missing -join ', ')) }
    return ''
}
$baseProblem = ''
if (-not (Test-Path -LiteralPath $baseLog)) {
    $baseProblem = "baseline log missing under $BaseOutDir"
} else {
    $baseText = Read-Log $baseLog
    if ($baseText -notmatch 'Loaded pk4[^\r\n]*demo00\.pk4') { $baseProblem = 'baseline log does not load demo00.pk4' }
    elseif ($baseText -match 'Loaded pk4[^\r\n]*zzz_wulfen_texture_pack_2\.0\.pk4') { $baseProblem = 'baseline log loads the Wulfen pack' }
}
if (-not $baseProblem) { $baseProblem = Test-ForcedSettings (Join-Path $BaseOutDir 'save\base\DoomConfig.cfg') }
if (-not $baseProblem) { $baseProblem = Test-ForcedSettings (Join-Path $OutDir 'save\base\DoomConfig.cfg') }
if ($baseProblem) {
    Report 'SC-005' 'FAIL' $baseProblem
} elseif (-not (Test-Path -LiteralPath $baseShot)) {
    Report 'SC-005' 'FAIL' "baseline screenshot missing under $BaseOutDir"
} elseif (-not (Test-Path -LiteralPath $shot)) {
    Report 'SC-005' 'FAIL' 'HD screenshot missing'
} else {
    $cmp = Join-Path $PSScriptRoot 'compare-screenshots.py'
    $python = Resolve-Python3WithPillow
    if (-not $python.Exe) {
        Report 'SC-005' 'FAIL' ("no Python 3 interpreter with Pillow for compare-screenshots.py: {0}" -f ($python.Problems -join '; '))
    } else {
        $prefix = $python.Prefix
        $thresholdArg = $Threshold.ToString([System.Globalization.CultureInfo]::InvariantCulture)
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $out = & $python.Exe @prefix $cmp --base $baseShot --hd $shot --threshold $thresholdArg 2>&1
            $code = $LASTEXITCODE
        } catch {
            $out = @($_.Exception.Message)
            $code = 1
        } finally {
            $ErrorActionPreference = $previous
        }
        $out | ForEach-Object { Write-Host "  $_" }
        $resultText = ($out | Out-String)
        $diffPercent = if ($resultText -match 'DIFF_PERCENT=([0-9.]+)') { $Matches[1] } else { $null }
        if ($code -eq 0) {
            Report 'SC-005' 'PASS' ("DIFF_PERCENT={0} >= {1}%; baseline pack-free with forced image settings; interpreter {2}" -f $diffPercent, $thresholdArg, $python.Label)
        } elseif ($diffPercent) {
            Report 'SC-005' 'FAIL' ("DIFF_PERCENT={0} < {1}% or comparison error (exit {2}); interpreter {3}" -f $diffPercent, $thresholdArg, $code, $python.Label)
        } else {
            Report 'SC-005' 'FAIL' ("comparison error (exit {0}); interpreter {1}" -f $code, $python.Label)
        }
    }
}

# SC-006: game data stayed out of git, no >1 MB file was added, pack is ignored, staged data matches manifest.
$sc6 = @()
$staged = Join-Path $DataDir 'base\zzz_wulfen_texture_pack_2.0.pk4'
$manifest = Join-Path $DataDir 'hd-manifest.txt'
$gitPk4 = @(& git -C $repo ls-files '*.pk4' 2>$null)
$pk4Code = $LASTEXITCODE
$gitStatus = @(& git -C $repo status --porcelain -uall 2>$null)
$statusCode = $LASTEXITCODE
if ($pk4Code -ne 0 -or $statusCode -ne 0) { $sc6 += "git failed (ls-files=$pk4Code status=$statusCode)" }
if ($gitPk4.Count -gt 0) { $sc6 += ("{0} .pk4 tracked" -f $gitPk4.Count) }
$statusPk4 = @($gitStatus | Where-Object { $_ -match '\.pk4' })
if ($statusPk4.Count -gt 0) { $sc6 += ("{0} .pk4 in status" -f $statusPk4.Count) }
$big = @()
foreach ($line in $gitStatus) {
    if ($line.Length -lt 4) { continue }
    $path = $line.Substring(3).Trim()
    if ($path -match ' -> ') { $path = ($path -split ' -> ')[-1] }
    $path = $path.Trim('"')
    $full = Join-Path $repo $path
    if ((Test-Path -LiteralPath $full -PathType Leaf) -and ((Get-Item -LiteralPath $full).Length -gt 1MB)) { $big += $path }
}
if ($big.Count -gt 0) { $sc6 += ("files over 1 MB in status: {0}" -f ($big -join ', ')) }
$repoFull = [IO.Path]::GetFullPath($repo).TrimEnd('\', '/')
$stagedFull = [IO.Path]::GetFullPath($staged)
$stagedExists = Test-Path -LiteralPath $staged
if (-not $stagedExists) { $sc6 += 'staged pack missing' }
if ($stagedFull.StartsWith($repoFull, [StringComparison]::OrdinalIgnoreCase)) {
    $rel = $stagedFull.Substring($repoFull.Length).TrimStart('\', '/')
    & git -C $repo check-ignore -q -- $rel
    if ($LASTEXITCODE -ne 0) { $sc6 += 'staged pack is inside the repo and not ignored' }
}
if (-not (Test-Path -LiteralPath $manifest)) {
    $sc6 += 'hd-manifest.txt missing'
} else {
    $mf = Get-Content -LiteralPath $manifest -Raw
    $lenLine = [regex]::Match($mf, '(?m)^hd\.length=(\d+)\s*$')
    if (-not $lenLine.Success) { $sc6 += 'manifest has no hd.length' }
    elseif ($stagedExists -and [int64]$lenLine.Groups[1].Value -ne (Get-Item -LiteralPath $staged).Length) {
        $sc6 += ("staged pack {0} != manifest {1}" -f (Get-Item -LiteralPath $staged).Length, $lenLine.Groups[1].Value)
    }
    foreach ($pair in @(@('hd', $staged), @('demo00', (Join-Path $DataDir 'base\demo00.pk4')))) {
        $name = $pair[0]; $file = $pair[1]
        $hashLine = [regex]::Match($mf, ('(?m)^' + $name + '\.staged_sha256=([0-9a-f]{64})\s*$'))
        if (-not $hashLine.Success) { $sc6 += "manifest has no $name.staged_sha256" }
        elseif (-not (Test-Path -LiteralPath $file)) { $sc6 += "$name staged file missing" }
        else {
            $have = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($have -ne $hashLine.Groups[1].Value) { $sc6 += "$name staged hash mismatch" }
        }
    }
}
if ($sc6.Count -eq 0) { Report 'SC-006' 'PASS' 'no .pk4 in git, no >1 MB file in status, pack ignored, staged hashes match manifest' }
else { Report 'SC-006' 'FAIL' ($sc6 -join '; ') }

# SC-008: the HD run loaded at least some textures at a higher resolution than the baseline.
# Requires +listImages in both runs; without it there is no evidence that the pack was sampled.
function Read-ImageSizes([string]$Path) {
    $sizes = @{}
    if (-not (Test-Path -LiteralPath $Path)) { return $sizes }
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^\s*\d+:[A-Z]*\s+(\d+)\s+(\d+)\s+.*\s+(\d+(?:\.\d+)?[kM])\s+(\S+)\s*$') {
            $sizes[$Matches[4].ToLowerInvariant()] = @([int]$Matches[1], [int]$Matches[2])
        }
    }
    return $sizes
}
$baseLog = Join-Path $BaseOutDir 'qconsole.log'
$hdSizes = Read-ImageSizes $log
$baseSizes = Read-ImageSizes $baseLog
$upscaled = 0
$example = ''
foreach ($name in $hdSizes.Keys) {
    if ($baseSizes.ContainsKey($name)) {
        $ha = [int64]$hdSizes[$name][0] * $hdSizes[$name][1]
        $ba = [int64]$baseSizes[$name][0] * $baseSizes[$name][1]
        if ($ha -gt $ba) {
            $upscaled++
            if (-not $example) {
                $example = ("{0} {1}x{2} -> {3}x{4}" -f $name, $baseSizes[$name][0], $baseSizes[$name][1], $hdSizes[$name][0], $hdSizes[$name][1])
            }
        }
    }
}
if ($baseSizes.Count -eq 0) { Report 'SC-008' 'FAIL' "baseline log has no image listing; rerun with +listImages ($baseLog)" }
elseif ($hdSizes.Count -eq 0) { Report 'SC-008' 'FAIL' "HD log has no image listing; rerun with +listImages ($log)" }
elseif ($upscaled -ge 10) { Report 'SC-008' 'PASS' ("{0} textures loaded larger; e.g. {1}" -f $upscaled, $example) }
else { Report 'SC-008' 'FAIL' "only $upscaled textures larger than baseline; check image_usePrecompressedTextures=0 and pack coverage" }

Write-Host ("SUMMARY fails={0}" -f $fails)
if ($fails -gt 0) { exit 1 }
exit 0
