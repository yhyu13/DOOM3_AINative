#Requires -Version 5.1
<#
.SYNOPSIS
  Capture engine-rendered showcase screenshots using the engine's own `screenshot` command.
.DESCRIPTION
  Each entry in -Scenes is `map|name|x y z yaw|settleSec`; leave the third field empty to keep the spawn
  view. The script starts the win11-amd64 engine once per scene, writes a showcase.cfg that is executed by
  the engine's own command buffer (`devmap`, `wait`, `god`, `notarget`, `setviewpos`, `screenshot`, `quit`),
  converts the TGA output to PNG and copies it to -OutDir. No synthetic keystrokes are used. Needs the demo
  run directory (prepare-demo-data.ps1).
  Exit 0 = every scene produced a frame; 6 = at least one scene failed (including a conversion error);
  3 = exe missing; 9 = no data dir; 1 = unexpected error (including a process-start failure inside a scene); 8 = an instance of the resolved
  executable is already running and -StopExisting was not supplied. Only processes whose executable path
  matches the resolved executable are stopped. showcase.cfg is removed on both success and error.
.EXAMPLE
  .\scripts\capture-showcase.ps1 -DataDir <data-root>\run -OutDir docs\media
#>
[CmdletBinding()]
param(
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [string]$Exe,
    [string[]]$Scenes,
    [int]$Width = 1280,
    [int]$Height = 720,
    [switch]$StopExisting  # allow stopping instances of the same resolved executable
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot '_common.ps1')
$exe = if ($Exe) { $Exe } else { Resolve-Doom3Exe -Repo $repo }
if (-not $OutDir) { $OutDir = Join-Path $repo 'docs\media' }
if (-not $DataDir -or -not (Test-Path $DataDir)) { Write-Host 'CAPTURE no data dir'; exit 9 }
if (-not (Test-Path $exe)) { Write-Host "CAPTURE exe missing: $exe"; exit 3 }
$exe = [IO.Path]::GetFullPath($exe)
$running = @(Get-ProcessesUsingExe -Path $exe)
if ($running.Count -gt 0) {
    if (-not $StopExisting) {
        Write-Host ("CAPTURE {0} instance(s) of {1} already running; pass -StopExisting to stop them" -f $running.Count, $exe)
        exit 8
    }
    Stop-ProcessesUsingExe -Path $exe
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# NOTE: file names must not contain '-': the engine tokenizer splits it, and screenshot a-b.tga silently writes an empty shot00001.tga.
# map | file name | "x y z yaw" or empty (keep spawn view) | settle seconds after the level is up
if (-not $Scenes) {
    $Scenes = @(
        'game/demo_mars_city1|mars_city_control_room||34',     # cinematic: the briefing room, rendered with real characters
        'game/demo_mars_city2|mars_city_corridor|-224 -2236 24 0|8',
        'game/demo_mars_city2|mars_city_imp|-1900 -2912 186 0|8',
        'game/demo_mc_underground|underground_hangar|3848 -1216 240 180|8',
        'game/demo_mc_underground|underground_pipes|64 400 140 90|8'
    )
}

function ConvertTo-Png([string]$Tga, [string]$Dst) {
    $bytes = [IO.File]::ReadAllBytes($Tga)
    $w = [BitConverter]::ToUInt16($bytes, 12); $h = [BitConverter]::ToUInt16($bytes, 14); $bpp = $bytes[16]
    if ($bytes[2] -ne 2 -or ($bpp -ne 24 -and $bpp -ne 32)) { throw "unsupported TGA type $($bytes[2]) / $bpp bpp" }
    $top = ($bytes[17] -band 0x20) -ne 0; $bp = $bpp / 8; $off = 18 + $bytes[0]
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bd = $bmp.LockBits((New-Object System.Drawing.Rectangle 0, 0, $w, $h), [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bmp.PixelFormat)
    $row = New-Object byte[] ($w * 4)
    for ($y = 0; $y -lt $h; $y++) {
        $si = $off + $(if ($top) { $y } else { $h - 1 - $y }) * $w * $bp
        for ($x = 0; $x -lt $w; $x++) {
            $row[$x * 4] = $bytes[$si + $x * $bp]; $row[$x * 4 + 1] = $bytes[$si + $x * $bp + 1]
            $row[$x * 4 + 2] = $bytes[$si + $x * $bp + 2]; $row[$x * 4 + 3] = 255
        }
        [System.Runtime.InteropServices.Marshal]::Copy($row, 0, [IntPtr]($bd.Scan0.ToInt64() + $y * $bd.Stride), $w * 4)
    }
    $bmp.UnlockBits($bd); $bmp.Save($Dst, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}

$failed = 0
try {
    foreach ($scene in $Scenes) {
        $p = $scene -split '\|'
        if ($p.Count -lt 4) { Write-Host ("CAPTURE scene malformed (expected map|name|view|settleSec): {0}" -f $scene); $failed++; continue }
        $map = $p[0]; $name = $p[1]; $view = $p[2]; $settle = [int]$p[3]
        $save = Join-Path $repo "targets\win11-amd64\build\showcase-$name"
        if (Test-Path $save) { Remove-Item $save -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $save | Out-Null

        $cfg = New-Object System.Collections.Generic.List[string]
        $cfg.Add("devmap $map")
        for ($w = 0; $w -lt 120; $w++) { $cfg.Add('wait') }                       # level up
        if ($view) {
            $cfg.Add('god'); $cfg.Add('notarget')
            $cfg.Add("setviewpos $view")
        }
        for ($w = 0; $w -lt ($settle * 60); $w++) { $cfg.Add('wait') }
        $cfg.Add("screenshot screenshots/$name.tga")
        for ($w = 0; $w -lt 30; $w++) { $cfg.Add('wait') }
        $cfg.Add('quit')
        Set-Content -Path (Join-Path $DataDir 'base\showcase.cfg') -Value $cfg -Encoding ASCII

        $a = @('+set', 'logFile', '2', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '-1',
            '+set', 'r_customWidth', "$Width", '+set', 'r_customHeight', "$Height",
            '+set', 'fs_basepath', $DataDir, '+set', 'fs_savepath', $save, '+exec', 'showcase')
        $proc = Start-Process -FilePath $exe -ArgumentList $a -PassThru
        $limit = 60 + $settle + 60
        if (-not $proc.WaitForExit($limit * 1000)) { Write-Host "CAPTURE[$name] still running after ${limit}s, stopping"; Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Seconds 1
        $tga = Join-Path $save "base\screenshots\$name.tga"
        if (-not (Test-Path $tga)) { Write-Host "CAPTURE[$name] FAILED: no screenshot written"; $failed++; continue }
        $dst = Join-Path $OutDir "$name.png"
        try { ConvertTo-Png $tga $dst } catch { Write-Host "CAPTURE[$name] FAILED: $($_.Exception.Message)"; $failed++; continue }
        Write-Host ("CAPTURE[{0}] {1} ({2} bytes)" -f $name, $dst, (Get-Item $dst).Length)
    }
} finally {
    Remove-Item (Join-Path $DataDir 'base\showcase.cfg') -ErrorAction SilentlyContinue
}
if ($failed) { exit 6 }
exit 0
