#Requires -Version 5.1
<#
.SYNOPSIS
  Stub renderer used only by tests/content/run-capture-harness.ps1.
.DESCRIPTION
  It imitates the three things a Q4 build must do -- exit 0, print an RHI identity line, write a frame --
  and can be told to break exactly one of them. Nothing it prints or writes is evidence; the harness
  asserts that capture-q4.ps1 accepts the good case and refuses every broken one.
.PARAMETER Mode
  ok | no-identity | wrong-adapter | blank | no-image | exit-1
.PARAMETER Capture
  Path of the PNG to write (ignored by no-image and exit-1).
#>
[CmdletBinding()]
param(
    [string]$Mode = 'ok',
    [string]$Capture = 'capture.png'
)
$ErrorActionPreference = 'Stop'

function Write-Frame([string]$Path, [switch]$Blank) {
    Add-Type -AssemblyName System.Drawing
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $bmp = New-Object System.Drawing.Bitmap(160, 120, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        for ($y = 0; $y -lt 120; $y++) {
            for ($x = 0; $x -lt 160; $x++) {
                if ($Blank) {
                    $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, 20, 20, 20))
                } else {
                    # A gradient plus a checker block: guarantees well over 16 distinct colours.
                    $r = 20 + ($x * 200 / 160)
                    $g = 20 + ($y * 200 / 120)
                    $b = if ((($x -band 16) -ne 0) -and (($y -band 16) -ne 0)) { 220 } else { 60 }
                    $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, [int]$r, [int]$g, [int]$b))
                }
            }
        }
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $bmp.Dispose() }
}

switch ($Mode) {
    'ok' {
        Write-Output 'RHI backend: d3d12 (Fake RHI Adapter)'
        Write-Frame $Capture
        exit 0
    }
    'no-identity' {
        Write-Output 'GL_RENDERER: Fake legacy renderer'
        Write-Frame $Capture
        exit 0
    }
    'wrong-adapter' {
        Write-Output 'RHI backend: d3d12 (Some Other Adapter)'
        Write-Frame $Capture
        exit 0
    }
    'blank' {
        Write-Output 'RHI backend: d3d12 (Fake RHI Adapter)'
        Write-Frame $Capture -Blank
        exit 0
    }
    'no-image' {
        Write-Output 'RHI backend: d3d12 (Fake RHI Adapter)'
        exit 0
    }
    'exit-1' {
        Write-Output 'RHI backend: d3d12 (Fake RHI Adapter)'
        exit 1
    }
    default {
        Write-Error "unknown mode: $Mode"
        exit 2
    }
}
