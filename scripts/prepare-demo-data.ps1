#Requires -Version 5.1
<#
.SYNOPSIS
  Build an untracked run directory from the official free DOOM 3 demo so the GPL engine can load it.
.DESCRIPTION
  Input : demo00.pk4 from the official demo installer (D3Demo.exe, id Software 2004, SHA256 in docs).
  Output: <RunDir>\base\demo00.pk4 (hard link, no copy) plus two loose compatibility shims:
    base\guis\mainmenu.gui          = guis/demo_mainmenu.gui from the pk4 (the 2011 engine asks for mainmenu.gui)
    base\script\weapon_base.script  = pk4 script + 'boolean WEAPON_NETFIRING;' (the 2011 game code links that
                                      field; the 2004 demo scripts predate it)
  The demo licence forbids modifying the demo: these shims are LOCAL TEST FILES, never committed or redistributed.
.EXAMPLE
  .\scripts\prepare-demo-data.ps1 -Pk4 <data-root>\demo00.pk4 -RunDir <data-root>\run
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Pk4, [Parameter(Mandatory = $true)][string]$RunDir)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not (Test-Path -LiteralPath $Pk4)) { Write-Host "ERROR pk4 not found: $Pk4"; exit 2 }
$base = Join-Path $RunDir 'base'
New-Item -ItemType Directory -Force -Path (Join-Path $base 'guis'), (Join-Path $base 'script') | Out-Null
$dst = Join-Path $base 'demo00.pk4'
if (-not (Test-Path $dst)) {
    cmd /c mklink /H "$dst" "$Pk4" | Out-Null
    if (-not (Test-Path $dst)) { Copy-Item $Pk4 $dst }
}
$z = [IO.Compression.ZipFile]::OpenRead($Pk4)
try {
    $e = $z.GetEntry('guis/demo_mainmenu.gui')
    [IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $base 'guis\mainmenu.gui'), $true)
    $e = $z.GetEntry('script/weapon_base.script')
    $sr = New-Object IO.StreamReader($e.Open()); $txt = $sr.ReadToEnd(); $sr.Close()
} finally { $z.Dispose() }
if ($txt -notmatch 'WEAPON_NETFIRING') {
    $txt = $txt.Replace('boolean		WEAPON_NETRELOAD;', "boolean`t`tWEAPON_NETFIRING;`r`nboolean`t`tWEAPON_NETRELOAD;")
}
[IO.File]::WriteAllText((Join-Path $base 'script\weapon_base.script'), $txt, (New-Object Text.UTF8Encoding($false)))
Write-Host "PREPARED $RunDir : pk4 + 2 shims"
exit 0
