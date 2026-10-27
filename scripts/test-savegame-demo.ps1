#Requires -Version 5.1
<#
.SYNOPSIS
  Tests: savegame round trip, old-version rejection, render-demo record/playback on x64 (tasks T062/T063).

.DESCRIPTION
  Every phase starts a fresh engine process from a uniquely renamed copy of the exe
  (DOOM3_savedemo_<pid>.exe in -OutDir) and kills it by PID on timeout. Commands are fed through
  +exec <cfg> files written to the sandbox (long +wait command lines crash the engine).

  Phases
    0  menu reference screenshot (no map), taken by this script, same resolution as playback shots
    1  devmap, getviewpos (spawn P0), setviewpos to a different point, getviewpos (P1), savegame
    2  new process: loadgame, getviewpos (P2) -> P2 must equal P1 and differ from P0 (position
       round trip); post-load screenshot must show the map (many colours, differs from menu)
    3  copy of the save with header version patched 18 -> 17 is rejected (warning, exit 0, no ERROR)
    4  record a ~900 frame demo
    5  playDemo, screenshot while playback is still running. Proven from the log order:
       "reading a v2 render demo" < "Wrote shots/play_a" < "stopped playing". The frame must have
       many colours and must differ from the menu reference (mean abs diff threshold). A second
       shot later in playback must differ from the first (the demo advances).

  Not testable from the console: player health. There is no command that prints health, so only
  the view position (origin + yaw) is round-tripped. If the cinematic of the demo map owns the
  render view, getviewpos reports the cinematic camera; the script prints P0/P1/P2 so this is visible.

.PARAMETER Exe      Engine exe. Default: Resolve-Doom3Exe.
.PARAMETER DataDir  Directory containing base\ . Default: $env:D3_DATA_DIR.
.PARAMETER OutDir   Scratch dir. Default: <repo-root>\build\test-savegame-demo.
.PARAMETER Map      Map for the phases. Default game/demo_mars_city1.
.PARAMETER TimeoutSec  Per-process timeout (default 240).
.PARAMETER Sound       Leave sound enabled (default is s_noSound 1) so the sound-channel save/restore path runs.

.EXAMPLE
  $env:D3_DATA_DIR = '<dir containing base>'
  .\scripts\test-savegame-demo.ps1

.NOTES
  Exit codes: 0 all checks passed, 1 at least one check failed, 3 setup problem.
#>
param(
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [string]$Map = 'game/demo_mars_city1',
    [int]$TimeoutSec = 240,
    [switch]$Sound
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_common.ps1')
$Repo = Split-Path -Parent $PSScriptRoot
if (-not $Exe)    { $Exe = Resolve-Doom3Exe -Repo $Repo }
if (-not $OutDir) { $OutDir = Join-Path $Repo 'build\test-savegame-demo' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "FAIL: exe missing: $Exe"; exit 3 }
if (-not $DataDir -or -not (Test-Path -LiteralPath (Join-Path $DataDir 'base'))) { Write-Host 'FAIL: -DataDir (or D3_DATA_DIR) must contain base\'; exit 3 }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

$script:fails = 0
function Pass([string]$m) { Write-Host "PASS: $m" }
function Fail([string]$m) { Write-Host "FAIL: $m"; $script:fails++ }
function Check([bool]$ok, [string]$m) { if ($ok) { Pass $m } else { Fail $m } }

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
public static class TgaTools {
    // returns BGR(A) pixel bytes, width, height, bytes per pixel; only uncompressed true-colour (type 2)
    public static byte[] Load(string path, out int w, out int h, out int bpp) {
        byte[] b = File.ReadAllBytes(path);
        if (b.Length < 18 || b[2] != 2) throw new Exception("unsupported TGA type " + (b.Length > 2 ? b[2] : -1));
        w = b[12] | (b[13] << 8); h = b[14] | (b[15] << 8); bpp = b[16] / 8;
        int off = 18 + b[0];
        byte[] px = new byte[w * h * bpp];
        Array.Copy(b, off, px, 0, px.Length);
        return px;
    }
    public static int Colors(string path) {
        int w, h, bpp; byte[] p = Load(path, out w, out h, out bpp);
        HashSet<int> s = new HashSet<int>();
        for (int i = 0; i + 2 < p.Length; i += bpp) s.Add(p[i] | (p[i + 1] << 8) | (p[i + 2] << 16));
        return s.Count;
    }
    // mean absolute difference over the colour channels, 0..255; -1 when sizes differ
    public static double MeanDiff(string a, string b) {
        int w1, h1, b1, w2, h2, b2;
        byte[] p = Load(a, out w1, out h1, out b1); byte[] q = Load(b, out w2, out h2, out b2);
        if (w1 != w2 || h1 != h2 || b1 != b2) return -1;
        double sum = 0; long n = 0;
        for (int i = 0; i + 2 < p.Length; i += b1) { for (int c = 0; c < 3; c++) { sum += Math.Abs(p[i + c] - q[i + c]); n++; } }
        return sum / n;
    }
}
'@

$copy = Join-Path $OutDir ("DOOM3_savedemo_{0}.exe" -f $PID)
Copy-Item -LiteralPath $Exe -Destination $copy -Force
$save = Join-Path $OutDir 'save'
if (Test-Path -LiteralPath $save) { Remove-Item -Recurse -Force -LiteralPath $save }
New-Item -ItemType Directory -Force -Path (Join-Path $save 'base') | Out-Null
$logPath = Join-Path $save 'base\qconsole.log'
$errPat = '(?m)^(ERROR|FATAL ERROR)'

function Read-Shared([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    try {
        $fs = New-Object IO.FileStream($path, 'Open', 'Read', 'ReadWrite,Delete')
        $sr = New-Object IO.StreamReader($fs, [Text.Encoding]::GetEncoding(28591))
        try { return $sr.ReadToEnd() } finally { $sr.Dispose() }
    } catch { return '' }
}
# Run one engine process to completion. $cfgLines go to base\<name>.cfg and are run with +exec.
function Run-Engine([string]$name, [string[]]$pre, [string[]]$cfgLines) {
    Remove-Item -LiteralPath $logPath -ErrorAction SilentlyContinue
    Set-Content -LiteralPath (Join-Path $save ("base\" + $name + ".cfg")) -Value $cfgLines -Encoding ASCII
    $a = @('+set','logFile','2','+set','r_fullscreen','0','+set','r_mode','-1','+set','r_customWidth','1280','+set','r_customHeight','720',
           '+set','fs_basepath',$DataDir,'+set','fs_savepath',$save,'+set','s_noSound',$(if ($Sound) { '0' } else { '1' }),'+set','developer','1',
           '+set','win_allowMultipleInstances','1') + $pre + @('+exec', $name)
    $p = Start-Process -FilePath $copy -ArgumentList $a -PassThru
    $ok = $p.WaitForExit($TimeoutSec * 1000)
    if (-not $ok) { Write-Host "  [$name] timeout, killing PID $($p.Id)"; Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; $exit = 'timeout' } else { $exit = $p.ExitCode }
    $txt = Read-Shared $logPath
    Set-Content -LiteralPath (Join-Path $OutDir ($name + '.log')) -Value $txt -Encoding ASCII
    [pscustomobject]@{ Exit = $exit; Log = $txt; Name = $name }
}
function Grab([string]$shot) {
    $f = Join-Path $save ("base\shots\" + $shot + ".tga")
    if (Test-Path -LiteralPath $f) { Copy-Item -LiteralPath $f -Destination (Join-Path $OutDir ($shot + '.tga')) -Force; return (Join-Path $OutDir ($shot + '.tga')) }
    return $null
}
function View-Positions([string]$log) {
    $res = @()
    foreach ($l in ($log -split "`n")) {
        if ($l -match '^\((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\) (-?[\d.]+)\s*$') {
            $res += , @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3], [double]$Matches[4])
        }
    }
    return , $res
}
function Fmt($v) { if ($v) { return ('({0} {1} {2}) yaw {3}' -f $v[0], $v[1], $v[2], $v[3]) } else { return '<none>' } }
function Max-Delta($a, $b) { $m = 0.0; for ($i = 0; $i -lt 4; $i++) { $d = [math]::Abs($a[$i] - $b[$i]); if ($d -gt $m) { $m = $d } }; return $m }

# --- Phase 0: menu reference ---
$r = Run-Engine 'p0_menu' @() @('wait 400', 'screenshot shots/menu.tga', 'wait 5', 'quit')
$menu = Grab 'menu'
Check ([bool]$menu) 'p0 menu reference screenshot written'

# --- Phase 1: spawn position, move, savegame ---
# The demo map opens with a long cinematic during which getviewpos reports the cinematic camera.
# $gameWait frames are enough on the reference machine; two probes 200 frames apart must agree
# (player stationary = cinematic over), otherwise P0 is flagged as unreliable.
$gameWait = 8000
$cfg1 = @("devmap $Map", "wait $gameWait", 'getviewpos', 'wait 200', 'getviewpos', 'quit')
$r = Run-Engine 'p1_probe' @() $cfg1
$vp0 = View-Positions $r.Log
$pos0 = if ($vp0.Count -ge 2) { $vp0[$vp0.Count - 1] } else { $null }
Check ($pos0 -and $vp0.Count -ge 2 -and ((Max-Delta $vp0[$vp0.Count - 2] $pos0) -le 0.01)) 'p1 view is stable after the intro cinematic (two probes 200 frames apart agree)'
Check ([bool]$pos0) ("p1 getviewpos works, spawn view P0 = " + (Fmt $pos0))
if ($pos0) { $nx = $pos0[0]; $ny = $pos0[1] + 64; $nz = $pos0[2]; $nyaw = 90 } else { $nx = 0; $ny = 0; $nz = 0; $nyaw = 123 }
$cfg = @("devmap $Map", "wait $gameWait", 'screenshot shots/pre_save.tga', 'wait 5',
         ("setviewpos {0} {1} {2} {3}" -f $nx, $ny, $nz, $nyaw), 'wait 30', 'getviewpos', 'wait 5', 'savegame tsave', 'wait 60', 'getviewpos', 'wait 5', 'quit')
$r = Run-Engine 'p1_save' @() $cfg
Check ($r.Log -match 'SpawnPlayer') 'p1 map spawned player'
Check ($r.Log -match 'Saved tsave') 'p1 savegame ran ("Saved tsave")'
Check ($r.Log -notmatch $errPat) 'p1 no ERROR in log'
$sv = Join-Path $save 'base\savegames\tsave.save'
Check ((Test-Path -LiteralPath $sv) -and (Get-Item -LiteralPath $sv).Length -gt 100000) ("p1 save file exists and is large" + $(if (Test-Path -LiteralPath $sv) { " (" + (Get-Item -LiteralPath $sv).Length + " bytes)" } else { '' }))
$vp = View-Positions $r.Log
$pos1 = if ($vp.Count -ge 1) { $vp[0] } else { $null }
Write-Host ("  P1 (before save, after setviewpos) = " + (Fmt $pos1))
$moved = $pos0 -and $pos1 -and ((Max-Delta $pos0 $pos1) -gt 1.0)
Check ([bool]$moved) ("p1 setviewpos changed the reported view (P0 -> P1), so the round trip is not trivial")
$pre = Grab 'pre_save'
if ($pre) { $c = [TgaTools]::Colors($pre); Check ($c -gt 2000) "p1 pre-save screenshot has content ($c distinct colours)" } else { Fail 'p1 no pre-save screenshot' }

# --- Phase 2: load in a new process ---
$r = Run-Engine 'p2_load' @() @('wait 30', 'loadgame tsave', 'wait 40', 'getviewpos', 'wait 5', 'wait 500', 'screenshot shots/post_load.tga', 'wait 5', 'quit')
Check ($r.Log -match 'Game Map Init SaveGame') 'p2 game restore path ran (Game Map Init SaveGame)'
Check ($r.Log -match ('msec to load ' + [regex]::Escape($Map))) 'p2 map load completed'
Check ($r.Log -notmatch 'Savegame Version mismatch') 'p2 no version mismatch'
Check ($r.Log -notmatch $errPat) 'p2 no ERROR in log'
$vp2 = View-Positions $r.Log
$pos2 = if ($vp2.Count -ge 1) { $vp2[0] } else { $null }
Write-Host ("  P2 (after load) = " + (Fmt $pos2))
if ($pos1 -and $pos2) {
    $dl = Max-Delta $pos1 $pos2
    Check ($dl -le 1.0) ("p2 view position round trip: P1 vs P2 max delta {0:N2} (<= 1.0)" -f $dl)
    if ($pos0) { Check ((Max-Delta $pos0 $pos2) -gt 1.0) 'p2 loaded position differs from the plain spawn position (not a re-spawn)' }
} else { Fail 'p2 position round trip not evaluable (missing getviewpos output)' }
$post = Grab 'post_load'
if ($post) {
    $c = [TgaTools]::Colors($post); Check ($c -gt 2000) "p2 post-load screenshot shows the map ($c distinct colours)"
    if ($menu) { $d = [TgaTools]::MeanDiff($post, $menu); Check ($d -gt 8) ("p2 post-load screenshot differs from the menu reference (mean abs diff {0:N1})" -f $d) }
} else { Fail 'p2 no post-load screenshot' }

# --- Phase 3: old-version save is rejected ---
if (Test-Path -LiteralPath $sv) {
    $bytes = [IO.File]::ReadAllBytes($sv)
    $len = [BitConverter]::ToInt32($bytes, 0); $off = 4 + $len
    $ver = [BitConverter]::ToInt32($bytes, $off)
    Check ($ver -eq 18) "p3 save header version == 18 at offset $off (found $ver)"
    [BitConverter]::GetBytes([int]17).CopyTo($bytes, $off)
    [IO.File]::WriteAllBytes((Join-Path $save 'base\savegames\told.save'), $bytes)
    $r = Run-Engine 'p3_old' @() @('wait 30', 'loadgame told', 'wait 300', 'quit')
    Check ($r.Log -match 'Savegame Version mismatch') 'p3 v17 save rejected with version-mismatch warning'
    Check ($r.Exit -eq 0) "p3 engine exited cleanly (exit $($r.Exit))"
    Check ($r.Log -notmatch $errPat) 'p3 no ERROR in log'
} else { Fail 'p3 skipped: no save file from phase 1' }

# --- Phase 4: record a long demo (the intro cinematic is moving content) ---
$r = Run-Engine 'p4_record' @() @("devmap $Map", 'wait 300', 'recordDemo tdemo', 'wait 3000', 'stopRecording', 'wait 5', 'quit')
Check ($r.Log -match 'recording to demos/tdemo') 'p4 recording started'
Check ($r.Log -notmatch $errPat) 'p4 no ERROR in log'
$dm = Join-Path $save 'base\demos\tdemo.demo'
Check ((Test-Path -LiteralPath $dm) -and (Get-Item -LiteralPath $dm).Length -gt 10000) ("p4 demo file exists" + $(if (Test-Path -LiteralPath $dm) { " (" + (Get-Item -LiteralPath $dm).Length + " bytes)" } else { '' }))

# --- Phase 5: playback, screenshots taken WHILE playing ---
# A shot counts as "live" only if its "Wrote" line precedes "stopped playing" in the log (log order is
# frame order). Early frames of a playback can be black (map load / fade-in), so several shots are taken
# and the proof is: >= 2 live shots, at least one live shot with real content that differs from the menu
# reference, and at least two live shots that differ from each other (the demo advances).
$offs = @(100, 250, 400, 600, 900, 1300)
$cfg5 = @('wait 30', 'playDemo tdemo')
$prev = 0
foreach ($o in $offs) { $cfg5 += ("wait {0}" -f ($o - $prev)); $cfg5 += ("screenshot shots/play_{0}.tga" -f $o); $cfg5 += 'wait 5'; $prev = $o + 5 }
$cfg5 += @('wait 2000', 'quit')
$r = Run-Engine 'p5_play' @() $cfg5
$iRead = $r.Log.IndexOf('reading a v2 render demo')
$iStop = $r.Log.IndexOf('stopped playing')
Check ($iRead -ge 0) 'p5 log: reading a v2 render demo'
Check ($iStop -gt $iRead) ("p5 log: playback ran to the end ('stopped playing' at $iStop after start at $iRead)")
Check ($r.Log -notmatch $errPat) 'p5 no ERROR in log'
Check ($r.Exit -eq 0) "p5 engine exited cleanly (exit $($r.Exit))"
$live = @()
foreach ($o in $offs) {
    $iS = $r.Log.IndexOf("Wrote shots/play_$o.tga")
    $f = Grab ("play_$o")
    $isLive = ($iS -gt $iRead -and ($iStop -lt 0 -or $iS -lt $iStop) -and $f)
    $cols = if ($f) { [TgaTools]::Colors($f) } else { -1 }
    $dm0 = if ($f -and $menu) { [TgaTools]::MeanDiff($f, $menu) } else { -1 }
    Write-Host ("  shot +{0}: log pos {1}, live={2}, colours={3}, diff-to-menu={4:N1}" -f $o, $iS, [bool]$isLive, $cols, $dm0)
    if ($isLive) { $live += , @($o, $f, $cols, $dm0) }
}
Check ($live.Count -ge 2) ("p5 at least 2 screenshots were taken during playback (live shots: $($live.Count))")
$good = @($live | Where-Object { $_[2] -gt 2000 -and $_[3] -gt 8 })
Check ($good.Count -ge 1) ("p5 a live playback frame has real content (> 2000 colours) and differs from the menu ({0} such frame(s))" -f $good.Count)
$adv = $false
for ($x = 0; $x -lt $live.Count; $x++) { for ($y = $x + 1; $y -lt $live.Count; $y++) { if ([TgaTools]::MeanDiff($live[$x][1], $live[$y][1]) -gt 0.5) { $adv = $true } } }
Check $adv 'p5 two live frames differ from each other: playback advances'

Remove-Item -LiteralPath $copy -Force -ErrorAction SilentlyContinue
Write-Host "RESULT: $script:fails failure(s)"
if ($script:fails -gt 0) { exit 1 } else { exit 0 }
