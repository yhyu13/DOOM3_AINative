#Requires -Version 5.1
<#
.SYNOPSIS
  Convert a legacy normal map to a linear tangent-space normal map (spec 009, FR-008 / T-026).
.DESCRIPTION
  The classic corpus packs normal maps in the project's "RXGB" arrangement; the RHI-era pipeline needs a
  plain linear normal map. This tool performs that conversion and can verify itself against a committed
  fixture within a stated tolerance, so the conversion is a checked claim rather than a comment.

  Layouts are declared, never guessed:

    rxgb      x = alpha, y = red, z = sqrt(1 - x^2 - y^2)      (the classic RXGB arrangement)
    rgb       x = red,   y = green, z = sqrt(1 - x^2 - y^2)
    directx   x = red,   y = green, z = blue, then y is negated (DirectX -Y up -> OpenGL +Y up)

  The `rxgb` arrangement is the project's documented convention, NOT verified against an authorized
  retail sample (spec 009 open question 6). Verifying it is a separate review, and this tool says so
  rather than implying the mapping is proven.

  Output is an 8-bit RGB PNG whose blue channel is the reconstructed Z, so no consumer has to know how
  the input was packed.

  Exit 0 = converted (and verified when -Verify is used), 1 = verification failed, 2 = environment error.
.EXAMPLE
  .\content\tools\convert-normal.ps1 -Source tests\content\fixtures\golden-rxgb.png -Output out\content-test\golden-linear.png -Layout rxgb -Verify tests\content\fixtures\golden-linear.png -Tolerance 2
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Output,
    [ValidateSet('rxgb', 'rgb', 'directx')][string]$Layout = 'rxgb',
    [string]$Verify,
    [int]$Tolerance = 2
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $Source)) { Write-Host "ERROR: source not found: $Source" -ForegroundColor Red; exit 2 }

function Read-Png([string]$Path) {
    $bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path).Path)
    try {
        $w = $bmp.Width; $h = $bmp.Height
        $px = New-Object 'long[,]' $w, $h
        for ($y = 0; $y -lt $h; $y++) {
            for ($x = 0; $x -lt $w; $x++) {
                $c = $bmp.GetPixel($x, $y)
                $px[$x, $y] = ([long]$c.A -shl 24) -bor ([long]$c.R -shl 16) -bor ([long]$c.G -shl 8) -bor [long]$c.B
            }
        }
        return @{ width = $w; height = $h; pixels = $px }
    } finally { $bmp.Dispose() }
}

function Write-Png([string]$Path, $Image) {
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $bmp = New-Object System.Drawing.Bitmap($Image.width, $Image.height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        for ($y = 0; $y -lt $Image.height; $y++) {
            for ($x = 0; $x -lt $Image.width; $x++) {
                $v = $Image.pixels[$x, $y]
                $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, (($v -shr 16) -band 0xFF), (($v -shr 8) -band 0xFF), ($v -band 0xFF)))
            }
        }
        $bmp.Save((Join-Path (Get-Location) $Path), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $bmp.Dispose() }
}

$src = Read-Png $Source
$out = @{ width = $src.width; height = $src.height; pixels = (New-Object 'long[,]' $src.width, $src.height) }

for ($y = 0; $y -lt $src.height; $y++) {
    for ($x = 0; $x -lt $src.width; $x++) {
        $v = $src.pixels[$x, $y]
        $r = ($v -shr 16) -band 0xFF
        $g = ($v -shr 8) -band 0xFF
        $b = $v -band 0xFF
        $a = ($v -shr 24) -band 0xFF

        switch ($Layout) {
            'rxgb' {
                $nx = ($a / 255.0) * 2.0 - 1.0
                $ny = ($r / 255.0) * 2.0 - 1.0
            }
            'rgb' {
                $nx = ($r / 255.0) * 2.0 - 1.0
                $ny = ($g / 255.0) * 2.0 - 1.0
            }
            'directx' {
                $nx = ($r / 255.0) * 2.0 - 1.0
                $ny = -((($g / 255.0) * 2.0 - 1.0))
            }
        }
        $sum = $nx * $nx + $ny * $ny
        if ($sum -gt 1.0) { $sum = 1.0 }
        $nz = [Math]::Sqrt(1.0 - $sum)
        $rr = [int][Math]::Round((($nx * 0.5 + 0.5) * 255.0))
        $gg = [int][Math]::Round((($ny * 0.5 + 0.5) * 255.0))
        $bb = [int][Math]::Round((($nz * 0.5 + 0.5) * 255.0))
        $out.pixels[$x, $y] = ([long]$rr -shl 16) -bor ([long]$gg -shl 8) -bor [long]$bb -bor ([long]255 -shl 24)
    }
}

Write-Png -Path $Output -Image $out
Write-Host "CONVERTED: $Source ($Layout) -> $Output ($($src.width)x$($src.height))"

if ($Verify) {
    if (-not (Test-Path -LiteralPath $Verify)) { Write-Host "ERROR: expected image not found: $Verify" -ForegroundColor Red; exit 2 }
    $exp = Read-Png $Verify
    if ($exp.width -ne $out.width -or $exp.height -ne $out.height) {
        Write-Host "VERIFY FAILED: size mismatch ($($exp.width)x$($exp.height) vs $($out.width)x$($out.height))" -ForegroundColor Red
        exit 1
    }
    $worst = 0
    $worstAt = ''
    for ($y = 0; $y -lt $out.height; $y++) {
        for ($x = 0; $x -lt $out.width; $x++) {
            $a = $out.pixels[$x, $y]
            $e = $exp.pixels[$x, $y]
            for ($s = 0; $s -le 16; $s += 8) {
                $d = [Math]::Abs((($a -shr $s) -band 0xFF) - (($e -shr $s) -band 0xFF))
                if ($d -gt $worst) { $worst = $d; $worstAt = "($x,$y) shift $s" }
            }
        }
    }
    if ($worst -gt $Tolerance) {
        Write-Host "VERIFY FAILED: max channel error $worst > $Tolerance at $worstAt" -ForegroundColor Red
        exit 1
    }
    Write-Host "VERIFY OK: max channel error $worst <= $Tolerance (layout $Layout)"
}
exit 0
