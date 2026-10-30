#Requires -Version 5.1
<#
.SYNOPSIS
  Q4 capture stage: drive a Q4-capable build, verify its identity, keep the image, write capture.json.
.DESCRIPTION
  Spec 009 stage 6. A capture is only evidence when it names the backend and the adapter it came from and
  the image is real, so this script refuses to print CAPTURE OK unless all of that is true.

  The contract a Q4 build must satisfy (nothing here is satisfied by today's legacy GL build):

    1. It accepts the arguments the caller passes and exits 0.
    2. It prints one line:  `RHI backend: <backend> (<adapter>)`
       matching the -Backend and -Adapter the caller claims. A build that renders through the classic GL
       path cannot print this, which is why the legacy executable is not evidence.
    3. It writes the screenshot to the path the caller passes in -Args (for example a `+` command the Q4
       build implements), and that file is a PNG larger than 64x64 with at least 16 distinct colours.

  When those hold, the script writes out/content/<material>/capture.json with the image hash, the
  executable hash, the identity line and the arguments, and prints `CAPTURE OK: <material> <backend>
  <adapter>`. The matrix report reads that file; it does not trust a console string.

  Exit codes:
    3  BLOCKED: no Q4 build was supplied (the only outcome today)
    4  identity: the build did not report the claimed backend/adapter
    5  capture: the image is missing, too small, or blank
    6  engine: the build exited non-zero
    2  environment error
.EXAMPLE
  .\content\tools\capture-q4.ps1 -Material hero_dielectric -Exe build-rhi\bin\Release\DOOM3.exe -DataDir <data>\run-rhi -Backend d3d12 -Adapter "AMD Radeon RX 7900" -EngineArgs "-Mode;ok;-Capture;<path>.png"
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Material,
    [string]$Exe,
    [string]$DataDir,
    [string]$Backend,
    [string]$Adapter,
    [string]$EngineArgs = '',
    [string]$Screenshot,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$repo = Get-RepoRoot -Start $PSScriptRoot

function Test-NonEmptyString {
    param($Value)
    return (($Value -is [string]) -and -not [string]::IsNullOrWhiteSpace($Value))
}
if (-not $OutDir) { $OutDir = Join-Path $repo "out\content\$Material" }
$packagePath = Join-Path $OutDir 'package.json'

function Write-Blocked {
    param([string]$Reason)
    Write-Host 'BLOCKED: spec 004 has not reached Q4'
    Write-Host "  material: $Material"
    Write-Host "  reason:   $Reason"
    Write-Host '  note:     a legacy GL capture is not Q4 evidence (spec 009 C-09)'
    exit 3
}

if (-not (Test-NonEmptyString $Exe)) {
    Write-Blocked 'no -Exe was supplied; there is no RHI path that renders a PBR material'
}
if (-not (Test-NonEmptyString $Backend) -or -not (Test-NonEmptyString $Adapter)) {
    Write-Blocked '-Backend and -Adapter are required: an unnamed capture is not evidence'
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    Write-Host "ERROR: executable not found: $Exe" -ForegroundColor Red
    exit 2
}
if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf)) {
    Write-Host "ERROR: no package at $packagePath; build the material first" -ForegroundColor Red
    exit 2
}
if (-not (Test-NonEmptyString $Screenshot)) {
    Write-Host 'ERROR: -Screenshot is required; pass the path the Q4 build will write' -ForegroundColor Red
    exit 2
}
$screenshotPath = if ([System.IO.Path]::IsPathRooted($Screenshot)) { $Screenshot } else { Join-Path $repo $Screenshot }
$screenshotDir = Split-Path -Parent $screenshotPath
if ($screenshotDir -and -not (Test-Path -LiteralPath $screenshotDir)) {
    New-Item -ItemType Directory -Force -Path $screenshotDir | Out-Null
}
if (Test-Path -LiteralPath $screenshotPath) { Remove-Item -LiteralPath $screenshotPath -Force }

$logPath = Join-Path $OutDir 'capture.log'
$engineArgList = @()
if (Test-NonEmptyString $DataDir) { $engineArgList = @('+set', 'fs_basepath', $DataDir) }
foreach ($token in ($EngineArgs -split ';')) { if ($token -ne '') { $engineArgList = $engineArgList + $token } }


$started = Get-Date
# Invoke synchronously: '&' runs a .cmd through cmd.exe and an .exe directly, and $LASTEXITCODE is the
# real exit code in both cases, and a .cmd wrapper reports its own exit code through it.
$output = & $Exe @engineArgList 2>&1 | Out-String
$exitCode = $LASTEXITCODE
[System.IO.File]::WriteAllText($logPath, $output, (New-Object System.Text.UTF8Encoding($false)))

$log = ''
foreach ($candidate in @($logPath)) {
    if (Test-Path -LiteralPath $candidate) { $log += [System.IO.File]::ReadAllText($candidate) }
}
if ($exitCode -ne 0) {
    Write-Host "ERROR: the build exited $exitCode; see $logPath" -ForegroundColor Red
    exit 6
}

# Identity: the build must name the backend and adapter the caller claims. A classic GL build cannot.
# The adapter name may itself contain parentheses (for example
# "Intel(R) Graphics"), so the adapter group is greedy to the last ')' on the
# line rather than stopping at the first one.
$identityPattern = 'RHI backend:\s*(?<backend>[^\r\n(]+?)\s*\((?<adapter>[^\r\n]+)\)'
if ($log -notmatch $identityPattern) {
    Write-Host 'IDENTITY FAILED: the build did not print "RHI backend: <backend> (<adapter>)"' -ForegroundColor Red
    Write-Host "  log: $logPath"
    exit 4
}
$reportedBackend = $Matches['backend'].Trim()
$reportedAdapter = $Matches['adapter'].Trim()
$identityLine = ($log -split "`n" | Where-Object { $_ -match $identityPattern } | Select-Object -First 1).Trim()
if ($reportedBackend -ne $Backend -or $reportedAdapter -ne $Adapter) {
    Write-Host 'IDENTITY FAILED: the build reported a different backend or adapter' -ForegroundColor Red
    Write-Host "  claimed:  $Backend ($Adapter)"
    Write-Host "  reported: $reportedBackend ($reportedAdapter)"
    exit 4
}

if (-not (Test-Path -LiteralPath $screenshotPath -PathType Leaf)) {
    Write-Host "CAPTURE FAILED: the build wrote no screenshot at $screenshotPath" -ForegroundColor Red
    exit 5
}

# A blank or tiny image is not a capture. Dimensions and distinct-colour count come from Pillow.
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ('doom3-capture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
try {
    $checker = Join-Path $tempDir 'check.py'
    $result = Join-Path $tempDir 'result.json'
    $script = @"
import json, sys
from PIL import Image
p = sys.argv[1]
try:
    im = Image.open(p)
    im.load()
    colors = im.convert('RGB').getcolors(maxcolors=1_000_000)
    out = {'ok': True, 'width': im.width, 'height': im.height, 'distinct': len(colors) if colors else 0}
except Exception as e:
    out = {'ok': False, 'error': str(e)}
with open(sys.argv[2], 'w') as fh:
    json.dump(out, fh)
"@
    [System.IO.File]::WriteAllText($checker, $script, (New-Object System.Text.UTF8Encoding($false)))
    & py -3 $checker $screenshotPath $result 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $result)) {
        Write-Host 'CAPTURE FAILED: the screenshot is not a readable image' -ForegroundColor Red
        exit 5
    }
    $info = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
} finally {
    Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}
if (-not $info.ok) {
    Write-Host "CAPTURE FAILED: $($info.error)" -ForegroundColor Red
    exit 5
}
if ($info.width -lt 64 -or $info.height -lt 64) {
    Write-Host "CAPTURE FAILED: $($info.width)x$($info.height) is too small to be a frame" -ForegroundColor Red
    exit 5
}
if ($info.distinct -lt 16) {
    Write-Host "CAPTURE FAILED: only $($info.distinct) distinct colours; the frame is blank" -ForegroundColor Red
    exit 5
}

$capture = [pscustomobject]@{
    schema          = 'doom3-q4-capture/1'
    material        = $Material
    backend         = $Backend
    adapter         = $Adapter
    identity_line   = $identityLine
    image           = 'capture.png'
    image_sha256    = Get-Sha256 $screenshotPath
    image_width     = [int]$info.width
    image_height    = [int]$info.height
    image_distinct  = [int]$info.distinct
    executable      = $Exe
    executable_sha256 = Get-Sha256 $Exe
    arguments       = $engineArgList
    started_utc     = $started.ToUniversalTime().ToString('o')
    log             = 'capture.log'
    package_sha256  = (Read-Json $packagePath).package_sha256
}
$finalImage = Join-Path $OutDir 'capture.png'
if ($screenshotPath -ne $finalImage) { Copy-Item -LiteralPath $screenshotPath -Destination $finalImage -Force }
Write-Json -Object $capture -Path (Join-Path $OutDir 'capture.json')

Write-Host "CAPTURE OK: $Material $Backend $Adapter"
Write-Host "  image: $finalImage ($($info.width)x$($info.height), $($info.distinct) colours)"
Write-Host "  record: $(Join-Path $OutDir 'capture.json')"
exit 0
