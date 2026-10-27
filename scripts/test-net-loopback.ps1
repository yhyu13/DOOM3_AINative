#Requires -Version 5.1
<#
.SYNOPSIS
  Test: loopback two-instance multiplayer join (listen server A, client B via 127.0.0.1) (task T065).

.DESCRIPTION
  Starts two uniquely renamed copies of the exe (DOOM3_netloop_<pid>_A.exe / _B.exe in -OutDir),
  tracked by PID and killed by PID. A is a deathmatch listen server on the SP demo map (the demo has no
  MP map), B connects to 127.0.0.1:<Port>. Asserts challenge/connect handshake, client entity spawn,
  "joined the game", then the warm-up -> "Game Map Restart" transition and that both processes are
  still alive 20 s afterwards (that restart crashed A before the idPlayer::ExitCinematic guard).

  A loose test-only entityDef override (player_doommarine_mp) is written into the sandbox savepath,
  never into the repo, because the free demo lacks the retail MP marine assets.

  Negative control: with -BaselineExe <old exe without the ExitCinematic fix> the same scenario is run
  and the script EXPECTS A to die after "Game Map Restart". PASS then means "the known crash was
  reproduced on the baseline" (validates that the main test can fail). Steps before the restart are
  reported as NOTE only in that mode. If the baseline never reaches the restart, or survives it, the
  control is reported FAIL (inconclusive), not silently passed.

.PARAMETER Exe          Engine exe under test. Default: Resolve-Doom3Exe.
.PARAMETER DataDir      Directory containing base\ . Default: $env:D3_DATA_DIR.
.PARAMETER OutDir       Scratch dir. Default: <repo-root>\build\test-net-loopback.
.PARAMETER Port         UDP port of A (B uses Port+1). Default 28101.
.PARAMETER BaselineExe  Optional old exe for the negative control (see above).
.PARAMETER KeyFile      Optional CD key file copied to base\doomkey of both instances.
.PARAMETER Map          Map. Default game/demo_mars_city1.

.EXAMPLE
  $env:D3_DATA_DIR = '<dir containing base>'
  .\scripts\test-net-loopback.ps1 -Port 28111
.EXAMPLE
  .\scripts\test-net-loopback.ps1 -BaselineExe <old DOOM3.exe> -Port 28121   # negative control

.NOTES
  Exit codes: 0 all checks passed, 1 at least one check failed, 3 setup problem.
  Takes about 3-5 minutes (waits for the warm-up to end).
#>
param(
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [int]$Port = 28101,
    [string]$BaselineExe = '',
    [string]$KeyFile = '',
    [string]$Map = 'game/demo_mars_city1'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_common.ps1')
$Repo = Split-Path -Parent $PSScriptRoot
if ($BaselineExe) { $Exe = $BaselineExe } elseif (-not $Exe) { $Exe = Resolve-Doom3Exe -Repo $Repo }
if (-not $OutDir) { $OutDir = Join-Path $Repo 'build\test-net-loopback' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "FAIL: exe missing: $Exe"; exit 3 }
if (-not $DataDir -or -not (Test-Path -LiteralPath (Join-Path $DataDir 'base'))) { Write-Host 'FAIL: -DataDir (or D3_DATA_DIR) must contain base\'; exit 3 }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path
$negative = [bool]$BaselineExe
$script:fail = 0
function Pass([string]$m) { Write-Host "PASS: $m" }
function Fail([string]$m) { Write-Host "FAIL: $m"; $script:fail++ }
# In negative-control mode the pre-restart steps are informational only.
function Step([bool]$ok, [string]$m) {
    if ($negative) { if ($ok) { Write-Host "NOTE ok: $m" } else { Write-Host "NOTE not reached: $m" } }
    elseif ($ok) { Pass $m } else { Fail $m }
}
if ($negative) { Write-Host "NEGATIVE CONTROL mode: baseline exe = $Exe" }

$def = @'
entityDef player_doommarine_mp {
	"inherit"	"player_base"
	"model"	"model_sp_marine"
	"bleed"	"1"
	"health"	"100"
	"pm_walkspeed"	"160"
	"pm_runspeed"	"270"
	"maxarmor"	"200"
	"deplete_armor"	"100"
	"deplete_rate"	"1.5"
	"deplete_ammount"	"2"
	"weapon"	"weapon_fists,weapon_pistol,weapon_flashlight"
	"current_weapon"	"1"
	"weapon11_cycle"	"0"
}
'@

function New-Inst([string]$tag, [int]$port, [string[]]$extra) {
    $d = Join-Path $OutDir "inst$tag"
    if (Test-Path -LiteralPath $d) { Remove-Item -Recurse -Force -LiteralPath $d }
    $save = Join-Path $d 'save'
    New-Item -ItemType Directory -Force -Path (Join-Path $save 'base\def') | Out-Null
    Set-Content -LiteralPath (Join-Path $save 'base\def\aaa_netloop_mpfix.def') -Value $def -Encoding ASCII
    if ($KeyFile) { Copy-Item -LiteralPath $KeyFile -Destination (Join-Path $save 'base\doomkey') }
    $dst = Join-Path $d ("DOOM3_netloop_{0}_{1}.exe" -f $PID, $tag)
    Copy-Item -LiteralPath $Exe -Destination $dst -Force
    $a = @('+set','logFile','2','+set','r_fullscreen','0','+set','r_mode','-1','+set','r_customWidth','640','+set','r_customHeight','480',
           '+set','fs_basepath',$DataDir,'+set','fs_savepath',$save,'+set','net_port',"$port",'+set','win_allowMultipleInstances','1') + $extra
    $p = Start-Process -FilePath $dst -ArgumentList $a -PassThru
    [pscustomobject]@{ Tag = $tag; Proc = $p; Log = (Join-Path $save 'base\qconsole.log') }
}
function Read-Log($i) {
    if (-not (Test-Path -LiteralPath $i.Log)) { return '' }
    try {
        $fs = New-Object IO.FileStream($i.Log, 'Open', 'Read', 'ReadWrite,Delete')
        $sr = New-Object IO.StreamReader($fs, [Text.Encoding]::GetEncoding(28591))
        try { return $sr.ReadToEnd() } finally { $sr.Dispose() }
    } catch { return '' }
}
function Wait-Log($i, [string]$pattern, [int]$sec) {
    $end = (Get-Date).AddSeconds($sec)
    while ((Get-Date) -lt $end) {
        if ((Read-Log $i) -match $pattern) { return $true }
        if ($i.Proc.HasExited) { return ((Read-Log $i) -match $pattern) }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

$A = $null; $B = $null
try {
    $A = New-Inst 'A' $Port @('+sets','si_maxPlayers','4','+sets','si_pure','0','+sets','si_gameType','deathmatch','+spawnServer',$Map)
    Write-Host "A pid=$($A.Proc.Id)"
    Step (Wait-Log $A 'Server spawned on port' 120) "A: server spawned on port $Port"
    Step (Wait-Log $A 'client 0 connected\.' 20) 'A: local client 0 connected'
    Step (Wait-Log $A 'SpawnPlayer: 0' 120) 'A: host player spawned in MP map'

    $B = New-Inst 'B' ($Port + 1) @('+connect', "127.0.0.1:$Port")
    Write-Host "B pid=$($B.Proc.Id)"
    Step (Wait-Log $B 'sending challenge to' 60) 'B: challenge sent'
    Step (Wait-Log $B 'received challenge response' 30) 'B: challenge response received'
    Step (Wait-Log $B 'received connect response' 30) 'B: connect response received'
    Step (Wait-Log $A 'challenge from .* connecting with \d+ ping' 30) 'A: challenge accepted (LAN, no CD-key auth)'
    Step (Wait-Log $A 'client 1 connected\.' 60) 'A: remote client 1 connected'
    Step (Wait-Log $A 'SpawnPlayer: 1' 90) 'A: client player entity spawned (SpawnPlayer: 1)'
    Step (Wait-Log $A 'Server: .* joined the game' 60) 'A: "joined the game" broadcast'
    Step (Wait-Log $B ('Map: ' + [regex]::Escape($Map)) 90) 'B: loaded the server map'
    Step (Wait-Log $B 'SpawnPlayer: \d' 120) 'B: entered the game (SpawnPlayer on client)'

    $restart = Wait-Log $A 'Game Map Restart' 150
    if ($negative) {
        if (-not $restart) { Fail 'negative control inconclusive: baseline A never reached "Game Map Restart"' }
        else {
            $dead = $false
            for ($k = 0; $k -lt 60 -and -not $dead; $k++) { if ($A.Proc.HasExited) { $dead = $true } else { Start-Sleep -Seconds 1 } }
            if ($dead) { Pass ("negative control: baseline A died after the map restart as expected (exit code 0x{0:X})" -f $A.Proc.ExitCode) }
            else { Fail 'negative control: baseline A survived the map restart (the known crash did NOT reproduce)' }
        }
    } else {
        if ($restart) { Pass 'A: warm-up ended, map restart executed' } else { Fail 'A: no "Game Map Restart" within 150 s' }
        Start-Sleep -Seconds 20
        if (-not $A.Proc.HasExited) { Pass 'A: alive 20 s after map restart' } else { Fail ("A: exited (code 0x{0:X}) after map restart" -f $A.Proc.ExitCode) }
        if (-not $B.Proc.HasExited) { Pass 'B: alive after map restart' } else { Fail ("B: exited (code 0x{0:X})" -f $B.Proc.ExitCode) }
        if ((Read-Log $A) -notmatch 'ERROR:') { Pass 'A: no ERROR: lines in log' } else { Fail 'A: log contains ERROR:' }
        if ((Read-Log $B) -notmatch 'ERROR:') { Pass 'B: no ERROR: lines in log' } else { Fail 'B: log contains ERROR:' }
    }
}
finally {
    foreach ($i in @($A, $B)) { if ($i -and -not $i.Proc.HasExited) { Stop-Process -Id $i.Proc.Id -Force -ErrorAction SilentlyContinue } }
    Start-Sleep -Seconds 1
    foreach ($i in @($A, $B)) { if ($i) { Write-Host ("ended pid {0} ({1}) hasExited={2}" -f $i.Proc.Id, $i.Tag, $i.Proc.HasExited) } }
}
if ($script:fail -eq 0) { Write-Host 'RESULT: PASS'; exit 0 } else { Write-Host "RESULT: FAIL ($script:fail)"; exit 1 }
