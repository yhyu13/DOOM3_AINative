#Requires -Version 5.1
<#
.SYNOPSIS
  Measure the real audio output of a running process via the Windows audio-session peak meter (0.0 = silence, 1.0 = full scale).
.DESCRIPTION
  Exit 0 = the process has an audio session on the default render device and its peak was above 0.
  5 = process not running, 6 = no audio session for the process (it never opened an audio stream), 7 = session exists but silent.
.EXAMPLE
  .\scripts\audio-peak.ps1 -ProcessName DOOM3 -Ms 5000
#>
[CmdletBinding()]
param([string]$ProcessName = 'DOOM3', [int]$Ms = 5000)

$src = @"
using System; using System.Runtime.InteropServices; using System.Diagnostics;
[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IMMDeviceEnumerator { int EnumAudioEndpoints(int f,int s,out IntPtr d); int GetDefaultAudioEndpoint(int f,int r,out IMMDevice d); }
[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IMMDevice { int Activate(ref Guid iid,int ctx,IntPtr p,[MarshalAs(UnmanagedType.IUnknown)] out object o); }
[Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IAudioSessionManager2 { int a(); int b(); int GetSessionEnumerator(out IAudioSessionEnumerator e); }
[Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IAudioSessionEnumerator { int GetCount(out int c); int GetSession(int i,out IAudioSessionControl s); }
[Guid("F4B1A599-7266-4319-A8CA-E70ACB11E8CD"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IAudioSessionControl { int s0(); int s1(); int s2(); int s3(); int s4(); int s5(); int s6(); int s7(); int s8(); }
[Guid("bfb7ff88-7239-4fc9-8fa2-07c950be9c6d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IAudioSessionControl2 { int s0(); int s1(); int s2(); int s3(); int s4(); int s5(); int s6(); int s7(); int s8(); int GetSessionIdentifier(out IntPtr id); int GetSessionInstanceIdentifier(out IntPtr id); int GetProcessId(out uint pid); }
[Guid("C02216F6-8C67-4B5B-9D00-D008E73E0064"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface IAudioMeterInformation { int GetPeakValue(out float p); }
[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator { }
public static class AudioPeak {
  public static string Sample(int pid, int ms) {
    var en=(IMMDeviceEnumerator)new MMDeviceEnumerator(); IMMDevice dev; en.GetDefaultAudioEndpoint(0,1,out dev);
    Guid iid=typeof(IAudioSessionManager2).GUID; object o; dev.Activate(ref iid,23,IntPtr.Zero,out o);
    var mgr=(IAudioSessionManager2)o; IAudioSessionEnumerator se; mgr.GetSessionEnumerator(out se); int n; se.GetCount(out n);
    float max=0; int hits=0; var sw=Stopwatch.StartNew();
    while(sw.ElapsedMilliseconds<ms){
      for(int i=0;i<n;i++){ IAudioSessionControl c; se.GetSession(i,out c); var c2=(IAudioSessionControl2)c; uint p; c2.GetProcessId(out p);
        if(p==pid){ hits++; float v; ((IAudioMeterInformation)c).GetPeakValue(out v); if(v>max) max=v; } }
      System.Threading.Thread.Sleep(20);
    }
    return "sessions=" + n + " sessionForPid=" + (hits>0) + " maxPeak=" + max.ToString("0.0000");
  }
}
"@
Add-Type -TypeDefinition $src
$p = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Host "AUDIO no process '$ProcessName'"; exit 5 }
$r = [AudioPeak]::Sample($p.Id, $Ms)
Write-Host "AUDIO $r"
if ($r -match 'sessionForPid=False') { exit 6 }
if ($r -match 'maxPeak=0\.0000') { exit 7 }
exit 0
