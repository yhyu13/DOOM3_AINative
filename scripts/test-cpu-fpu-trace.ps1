#Requires -Version 5.1
<#
.SYNOPSIS
  Tests: real x64 CPUID / FPU state reporting and resolved stack traces (task T050).

.DESCRIPTION
  Launches ONLY uniquely renamed copies of the engine exe (DOOM3_cpufpu_<pid>_<run>.exe, copied into
  -OutDir) and kills them by PID. Runs:
    A  +sys_cpuInfo +sys_fpuState +sys_stackTrace : CPU line vs WMI, FPU dump, >=3 resolved frames
    B  +error 1                                  : fatal-error path prints a trace with the call chain
    C  PDB hidden (copy of the exe whose embedded PDB name is patched, no PDB beside it):
       the trace must fall back to module+0xoffset and still print >=3 frames, none with file:line
    D  single-instance mutex: run WITHOUT win_allowMultipleInstances. The first instance must stay
       alive, a second one started the same way must exit, and no stray process may remain.

  Game data is needed only for fs_basepath (the demo is enough). No map is loaded.

.PARAMETER Exe      Engine exe. Default: Resolve-Doom3Exe (D3_EXE, build\bin\Release, target build).
.PARAMETER DataDir  Directory that contains base\ . Default: $env:D3_DATA_DIR.
.PARAMETER OutDir   Scratch/output dir. Default: <repo-root>\build\test-cpu-fpu-trace.
.PARAMETER TimeoutSec  Per-run timeout (default 120).

.EXAMPLE
  $env:D3_DATA_DIR = '<dir containing base>'
  .\scripts\test-cpu-fpu-trace.ps1
.EXAMPLE
  .\scripts\test-cpu-fpu-trace.ps1 -Exe .\build\bin\Release\DOOM3.exe -DataDir <dir> -OutDir .\build\t

.NOTES
  Exit codes: 0 all checks passed, 1 at least one check failed, 3 setup problem (exe/data missing).
  PDB note: runs A/B resolve names through the PDB path embedded in the exe; the exe and its PDB must
  come from the same build (a PDB rewritten by a later build gives module+0xoffset frames and FAILs A/B).
  Mutex note: the engine mutex name is the fixed string "DOOM3"; if another engine started without
  win_allowMultipleInstances is running on the machine, check D cannot be judged and is reported FAIL.
#>
param(
    [string]$Exe,
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir,
    [int]$TimeoutSec = 120
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_common.ps1')
$Repo = Split-Path -Parent $PSScriptRoot
if (-not $Exe)    { $Exe = Resolve-Doom3Exe -Repo $Repo }
if (-not $OutDir) { $OutDir = Join-Path $Repo 'build\test-cpu-fpu-trace' }
if (-not (Test-Path -LiteralPath $Exe)) { Write-Host "FAIL: exe missing: $Exe"; exit 3 }
if (-not $DataDir -or -not (Test-Path -LiteralPath (Join-Path $DataDir 'base'))) { Write-Host 'FAIL: -DataDir (or D3_DATA_DIR) must contain base\'; exit 3 }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

$script:fails = 0
function Check([bool]$ok, [string]$name, [string]$evidence) {
    if ($ok) { Write-Host "PASS: $name -- $evidence" } else { Write-Host "FAIL: $name -- $evidence"; $script:fails++ }
}
function Read-Shared([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    try {
        $fs = New-Object IO.FileStream($path, 'Open', 'Read', 'ReadWrite,Delete')
        $sr = New-Object IO.StreamReader($fs, [Text.Encoding]::GetEncoding(28591))
        try { return $sr.ReadToEnd() } finally { $sr.Dispose() }
    } catch { return '' }
}
# Copy the exe under a unique name. With $hidePdb the embedded PDB file name in the copy is patched
# (same length) so neither the embedded path nor the exe directory can supply symbols.
function New-ExeCopy([string]$tag, [bool]$hidePdb) {
    $dir = Join-Path $OutDir ("exe_" + $tag)
    if (Test-Path -LiteralPath $dir) { Remove-Item -Recurse -Force -LiteralPath $dir }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $dst = Join-Path $dir ("DOOM3_cpufpu_{0}_{1}.exe" -f $PID, $tag)
    Copy-Item -LiteralPath $Exe -Destination $dst -Force
    $patched = $false
    if ($hidePdb) {
        $stem = [IO.Path]::GetFileNameWithoutExtension($Exe)
        $needle = [Text.Encoding]::ASCII.GetBytes($stem + '.pdb') + [byte]0
        $b = [IO.File]::ReadAllBytes($dst)
        for ($i = $b.Length - $needle.Length; $i -ge 0; $i--) {
            $m = $true
            for ($j = 0; $j -lt $needle.Length; $j++) { if ($b[$i + $j] -ne $needle[$j]) { $m = $false; break } }
            if ($m) { $b[$i] = [byte][char]'Z'; $patched = $true; break }
        }
        if ($patched) { [IO.File]::WriteAllBytes($dst, $b) }
    }
    [pscustomobject]@{ Path = $dst; Patched = $patched }
}
function Start-Engine([string]$exePath, [string]$tag, [string[]]$extra, [bool]$multi) {
    $save = Join-Path $OutDir ("save_" + $tag)
    if (Test-Path -LiteralPath $save) { Remove-Item -Recurse -Force -LiteralPath $save }
    New-Item -ItemType Directory -Force -Path $save | Out-Null
    $a = @('+set','fs_basepath',$DataDir,'+set','fs_savepath',$save,'+set','logFile','2','+set','r_fullscreen','0','+set','developer','1')
    if ($multi) { $a += @('+set','win_allowMultipleInstances','1') }
    $p = Start-Process -FilePath $exePath -ArgumentList ($a + $extra) -PassThru -WorkingDirectory (Split-Path $exePath)
    [pscustomobject]@{ Proc = $p; Log = (Join-Path $save 'base\qconsole.log'); Tag = $tag }
}
function Run-Engine([string]$exePath, [string]$tag, [string[]]$extra, [string]$marker) {
    $e = Start-Engine $exePath $tag $extra $true
    $found = $false
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    try {
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 700
            if ((Read-Shared $e.Log).Contains($marker)) { $found = $true; Start-Sleep -Milliseconds 1500; break }
            if ($e.Proc.HasExited) { break }
        }
    } finally {
        if (-not $e.Proc.HasExited) { Stop-Process -Id $e.Proc.Id -Force -ErrorAction SilentlyContinue }
    }
    $txt = Read-Shared $e.Log
    Set-Content -LiteralPath (Join-Path $OutDir ("qconsole_" + $tag + ".log")) -Value $txt -Encoding ASCII
    [pscustomobject]@{ Log = $txt; Found = $found }
}
# Returns the first log line containing $after (the trace is printed on one line).
function Trace-Line([string]$log, [string]$after) {
    foreach ($l in ($log -split "`n")) { if ($l.Contains($after)) { return $l } }
    return ''
}

$ex = New-ExeCopy 'a' $false

# ---- Run A: CPU lines, FPU state, on-demand stack trace
$a = Run-Engine $ex.Path 'a' @('+sys_cpuInfo','+sys_fpuState','+sys_stackTrace') 'Stack trace:'
Check $a.Found 'run A reached sys_stackTrace output' ("marker found=" + $a.Found)
$cpuLine = ($a.Log -split "`n" | Where-Object { $_ -match '^CPU: ' } | Select-Object -First 1)
Check ([bool]$cpuLine) 'startup log has a CPU line' ("[" + "$cpuLine".Trim() + "]")
$wmi = Get-CimInstance Win32_Processor | Select-Object -First 1
$vendor = $wmi.Manufacturer
$expect = if ($vendor -match 'AMD|Authentic') { 'AMD' } elseif ($vendor -match 'Intel') { 'Intel' } else { 'generic' }
Check ("$cpuLine" -match "$expect CPU") 'CPU vendor matches Win32_Processor' ("WMI vendor=$vendor expect=$expect")
Check ("$cpuLine" -match 'MMX' -and "$cpuLine" -match 'SSE' -and "$cpuLine" -match 'SSE2') 'MMX/SSE/SSE2 flags reported (architectural on x64)' "$cpuLine".Trim()
$wmiName = ($wmi.Name -replace '\s+', ' ').Trim()
$infoLine = ($a.Log -split "`n" | Where-Object { $_ -match '^CPU vendor = ' } | Select-Object -First 1)
Check ([bool]$infoLine -and ((("$infoLine" -replace '\s+', ' ')) -like "*brand = $wmiName,*")) 'CPUID brand string equals Win32_Processor.Name' ("WMI=[$wmiName] engine=[" + "$infoLine".Trim() + "]")
if ($expect -eq 'Intel') { Check ("$cpuLine" -notmatch '3DNow') 'no bogus 3DNow! on Intel' "$cpuLine".Trim() }
$k32 = Add-Type -MemberDefinition '[DllImport("kernel32.dll")] public static extern bool IsProcessorFeaturePresent(uint f);' -Name K32 -Namespace T050 -PassThru
$sse3 = $k32::IsProcessorFeaturePresent(13)
Check (("$cpuLine" -match 'SSE3') -eq $sse3) 'SSE3 flag agrees with IsProcessorFeaturePresent(13)' ("OS says SSE3=$sse3")
Check ($a.Log -match 'num values on stack = 0') 'FPU state: x87 stack empty count' 'num values on stack = 0'
Check ($a.Log -match 'Rounding control\s+= Round to nearest') 'FPU state: control word decoded' 'Rounding control = Round to nearest'
Check ($a.Log -match 'MXCSR = [0-9a-f]{8} \(DAZ=\d FTZ=\d') 'FPU state: MXCSR reported' (($a.Log -split "`n" | Where-Object { $_ -match 'MXCSR =' } | Select-Object -First 1))
Check ($a.Log -match 'x87 stack empty = 1') 'Sys_FPU_StackIsEmpty() true' 'x87 stack empty = 1'

$frameRe = '-> (\S+)\+0x[0-9a-f]+ \(([^:)]+):(\d+)\)'
$line = Trace-Line $a.Log 'Stack trace:'
$fr = [regex]::Matches($line, $frameRe)
$names = ($fr | ForEach-Object { $_.Groups[1].Value + '@' + $_.Groups[2].Value + ':' + $_.Groups[3].Value }) -join ' | '
Check ($fr.Count -ge 3) 'sys_stackTrace: >=3 resolved frames (symbol+offset+file:line)' ("$($fr.Count) frames: $names")
Check ($line -match 'Sys_StackTrace_f') 'trace names the calling console command handler' 'Sys_StackTrace_f'

# ---- Run B: fatal error path
$b = Run-Engine $ex.Path 'b' @('+error','1') 'Stack trace at fatal error:'
Check $b.Found 'run B reached the fatal error path' ("marker found=" + $b.Found)
$line2 = Trace-Line $b.Log 'Stack trace at fatal error:'
$fr2 = [regex]::Matches($line2, $frameRe)
$names2 = ($fr2 | ForEach-Object { $_.Groups[1].Value }) -join ' | '
Check ($fr2.Count -ge 3) 'fatal error: >=3 resolved frames in trace' ("$($fr2.Count) frames: $names2")
Check ($names2 -match 'Sys_Error|FatalError|Com_Error_f') 'fatal error trace contains the error call chain' $names2

# ---- Run C: PDB hidden -> module+0xoffset fallback
$exC = New-ExeCopy 'c' $true
$pdbBeside = [bool](Get-ChildItem -LiteralPath (Split-Path $exC.Path) -Filter *.pdb -ErrorAction SilentlyContinue)
Check ($exC.Patched -and -not $pdbBeside) 'run C: embedded PDB name patched, no PDB beside the exe copy' ("patched=" + $exC.Patched + " pdbBeside=$pdbBeside")
$c = Run-Engine $exC.Path 'c' @('+sys_stackTrace') 'Stack trace:'
Check $c.Found 'run C reached sys_stackTrace output' ("marker found=" + $c.Found)
$lineC = Trace-Line $c.Log 'Stack trace:'
$frC = [regex]::Matches($lineC, '-> (\S+)\+0x[0-9a-f]+')
$ownFr = @($frC | Where-Object { $_.Groups[1].Value -like 'DOOM3*' })
$withLine = [regex]::Matches($lineC, '\([^)]*\.(cpp|inl|h|c):\d+\)')
Check ($ownFr.Count -ge 3) 'no PDB: >=3 frames printed as module+0xoffset' ("own-module frames=" + $ownFr.Count + " of " + $frC.Count + ": " + (($frC | ForEach-Object { $_.Value }) -join ' '))
Check ($withLine.Count -eq 0) 'no PDB: no frame carries file:line (symbols really absent)' ("file:line frames=" + $withLine.Count)

# ---- Run D: single-instance mutex (no win_allowMultipleInstances)
$exD = New-ExeCopy 'd' $false
$d1 = $null; $d2 = $null
try {
    $d1 = Start-Engine $exD.Path 'd1' @() $false
    $up = $false
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 700
        if ((Read-Shared $d1.Log) -match 'session initialized|game initialized') { $up = $true; break }
        if ($d1.Proc.HasExited) { break }
    }
    Check $up 'single-instance: first instance (no allowMultipleInstances) reached the menu' ("up=$up exited=" + $d1.Proc.HasExited)
    if ($up) {
        $d2 = Start-Engine $exD.Path 'd2' @() $false
        $null = $d2.Proc.WaitForExit(30000)
        Check $d2.Proc.HasExited 'single-instance: second instance exits (mutex held by the first)' ("second exited=" + $d2.Proc.HasExited)
        Start-Sleep -Seconds 1
        Check (-not $d1.Proc.HasExited) 'single-instance: first instance still alive after second tried to start' ("first exited=" + $d1.Proc.HasExited)
        Check (-not ((Read-Shared $d2.Log) -match 'session initialized')) 'single-instance: second instance never reached the menu' 'no "session initialized" in its log'
    }
} finally {
    foreach ($x in @($d1, $d2)) { if ($x -and -not $x.Proc.HasExited) { Stop-Process -Id $x.Proc.Id -Force -ErrorAction SilentlyContinue } }
    Start-Sleep -Seconds 1
}
$stray = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path -like (Join-Path $OutDir '*') })
Check ($stray.Count -eq 0) 'no stray test processes remain' ("running from OutDir: " + $stray.Count)

if ($script:fails -eq 0) { Write-Host 'ALL PASS'; exit 0 } else { Write-Host "$($script:fails) FAILED"; exit 1 }
