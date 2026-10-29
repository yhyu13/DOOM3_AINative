#Requires -Version 5.1
<#
.SYNOPSIS
  Shared helpers for the content pipeline (spec 009).
.DESCRIPTION
  Dot-source this file; do not run it. It holds only functions with no policy of their own: path
  resolution, hashing, JSON in/out in a stable form, and DDS header parsing. Every policy (what is
  allowed, what fails) lives in the gate that calls these, so a gate cannot silently change behaviour by
  editing a helper.

  The functions are deliberately strict about failure: a missing file or an unreadable header throws,
  and the caller decides whether that is a violation or an environment error.
#>
Set-StrictMode -Version 2.0

function Get-RepoRoot {
    [CmdletBinding()]
    param([string]$Start = $PSScriptRoot)
    $dir = [System.IO.Path]::GetFullPath($Start)
    while ($dir) {
        if (Test-Path -LiteralPath (Join-Path $dir '.git')) { return $dir }
        $parent = Split-Path -Parent $dir
        if ($parent -eq $dir) { break }
        $dir = $parent
    }
    throw "no .git found above $Start"
}

function Get-Sha256 {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "hash: file not found: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Read-Json {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "json: file not found: $Path" }
    $text = [System.IO.File]::ReadAllText($Path)
    try { return ($text | ConvertFrom-Json) } catch { throw "json: $Path is not valid JSON: $($_.Exception.Message)" }
}

function Write-Json {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Path
    )
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $json = $Object | ConvertTo-Json -Depth 12
    # A trailing newline keeps diffs clean; UTF-8 without BOM keeps the hash stable across editors.
    [System.IO.File]::WriteAllText($Path, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))
}

function Resolve-RepoPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    if ($RelativePath -match '^[A-Za-z]:' -or $RelativePath.StartsWith('/')) {
        throw "absolute path not allowed in a manifest: $RelativePath"
    }
    return (Join-Path $RepoRoot ($RelativePath -replace '/', '\'))
}

function Test-RepoRelativePath {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    if ($RelativePath -match '^[A-Za-z]:') { return $false }
    if ($RelativePath.StartsWith('/')) { return $false }
    if ($RelativePath -match '\\') { return $false }
    if ($RelativePath -match '(^|/)\.\.(/|$)') { return $false }
    return $true
}

function Get-DdsInfo {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "dds: file not found: $Path" }
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 148) { throw "dds: file too small to hold a DX10 header: $Path" }
    $magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    if ($magic -ne 'DDS ') { throw "dds: bad magic in $Path" }
    $height = [BitConverter]::ToUInt32($bytes, 12)
    $width = [BitConverter]::ToUInt32($bytes, 16)
    $mipCount = [BitConverter]::ToUInt32($bytes, 28)
    $fourCC = [System.Text.Encoding]::ASCII.GetString($bytes, 84, 4)
    if ($fourCC -ne 'DX10') { throw "dds: expected a DX10 extended header in $Path, found '$fourCC'" }
    $dxgi = [BitConverter]::ToUInt32($bytes, 128)
    # DXGI format table limited to what this pipeline emits; an unknown value is an error, not a guess.
    $formats = @{
        80 = @{ name = 'BC4_UNORM'; srgb = $false; channels = 1 }
        83 = @{ name = 'BC5_UNORM'; srgb = $false; channels = 2 }
        99 = @{ name = 'BC7_UNORM_SRGB'; srgb = $true; channels = 4 }
    }
    if (-not $formats.ContainsKey([int]$dxgi)) { throw "dds: unsupported DXGI format $dxgi in $Path" }
    return [pscustomobject]@{
        path       = $Path
        width      = [int]$width
        height     = [int]$height
        mips       = [int]$mipCount
        dxgiFormat = [int]$dxgi
        format     = $formats[[int]$dxgi].name
        srgb       = [bool]$formats[[int]$dxgi].srgb
        channels   = [int]$formats[[int]$dxgi].channels
        bytes      = $bytes.Length
    }
}

function Get-ExpectedMipCount {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][int]$Width)
    if ($Width -lt 1) { throw "mip count: width must be positive" }
    return [int]([Math]::Floor([Math]::Log($Width, 2))) + 1
}

function Get-FileListUnder {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [string[]]$Extension = @('*')
    )
    if (-not (Test-Path -LiteralPath $Root)) { return @() }
    $items = Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName
    $out = @()
    foreach ($i in $items) {
        foreach ($e in $Extension) {
            if ($e -eq '*' -or $i.Extension -ieq $e) { $out += $i; break }
        }
    }
    return $out
}

function Get-TrackedFiles {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    Push-Location $RepoRoot
    try {
        $files = @(git ls-files)
        if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed' }
    } finally { Pop-Location }
    return $files
}

function Get-Sha256Normalized {
    <#
      Hash a text file after normalising CRLF to LF. A package records the hash of its sidecar and .mtr, and
      those files are checked out with CRLF on Windows (* text=auto). Without normalisation the same commit
      would produce a different package hash on a fresh checkout, which would make SC-003's determinism claim
      false for everyone but the machine that wrote the files.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "hash: file not found: $Path" }
    $text = [System.IO.File]::ReadAllText($Path) -replace "`r`n", "`n"
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($text)
    $digest = [System.Security.Cryptography.SHA256]::Create().ComputeHash($bytes)
    return (([System.BitConverter]::ToString($digest) -replace '-', '').ToLowerInvariant())
}