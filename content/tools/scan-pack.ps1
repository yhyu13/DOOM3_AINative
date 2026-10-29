#Requires -Version 5.1
<#
.SYNOPSIS
  Inventory a locally staged content pack by name and hash (spec 009, T-007 and the C-01a denylist).
.DESCRIPTION
  Reads a .pk4/.pk3/.pak that the user legally owns, and writes two artefacts containing *names and
  hashes only*: the pack-hash denylist that the content gate enforces, and a suffix inventory that
  answers "which channels does the corpus actually carry".

  It never extracts, copies or rewrites a member. The archive stays where it is; this script only reads
  entry names and computes each member's SHA-256 from the decompressed stream.

  Without a pack it prints SKIPPED and exits 0 with a reason, because the absence of a locally staged
  corpus is not a pipeline failure (FR-014).
.EXAMPLE
  .\content\tools\scan-pack.ps1 -Pack <data>\wulfen_texture_pack_2.0.pk4
.EXAMPLE
  .\content\tools\scan-pack.ps1 -Pack <data>\wulfen_texture_pack_2.0.pk4 -DenylistOut content\manifest\pack-denylist.json
#>
[CmdletBinding()]
param(
    [string]$Pack,
    [string]$DenylistOut,
    [string]$InventoryOut,
    [switch]$Quiet
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'content-lib.ps1')
Set-StrictMode -Version 2.0

$repo = Get-RepoRoot -Start $PSScriptRoot
if (-not $DenylistOut) { $DenylistOut = Join-Path $repo 'content\manifest\pack-denylist.json' }
if (-not $InventoryOut) { $InventoryOut = Join-Path $repo 'out\content\pack-inventory.json' }

if (-not $Pack) {
    Write-Host 'SKIPPED: no -Pack given; the corpus is not staged on this host (FR-014)'
    exit 0
}
if (-not (Test-Path -LiteralPath $Pack)) {
    Write-Host "SKIPPED: pack not found at $Pack"
    exit 0
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $Pack))
try {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $hashes = New-Object System.Collections.Generic.List[string]
    $suffixes = @{}
    $files = 0
    $textures = 0
    $materials = 0
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName.EndsWith('/')) { continue }
        $files++
        $name = $entry.FullName
        $ext = [System.IO.Path]::GetExtension($name).ToLowerInvariant()
        if ($ext -eq '.tga' -or $ext -eq '.dds') { $textures++ }
        if ($ext -eq '.mtr') { $materials++ }
        # Suffix inventory: DOOM3 texture names end in _d, _s, _local, _h, _bump, _spec ...
        if ($ext -eq '.tga' -or $ext -eq '.dds') {
            $base = [System.IO.Path]::GetFileNameWithoutExtension($name)
            if ($base -match '_(d|s|local|h|bump|spec|gloss|fx)$') {
                $suffix = $Matches[1]
                if (-not $suffixes.ContainsKey($suffix)) { $suffixes[$suffix] = 0 }
                $suffixes[$suffix]++
            } else {
                if (-not $suffixes.ContainsKey('<none>')) { $suffixes['<none>'] = 0 }
                $suffixes['<none>']++
            }
        }
        $stream = $entry.Open()
        try {
            $hashBytes = $sha.ComputeHash($stream)
        } finally { $stream.Dispose() }
        $hashes.Add(([System.BitConverter]::ToString($hashBytes) -replace '-', '').ToLowerInvariant())
    }

    $packHash = Get-Sha256 $Pack
    $size = (Get-Item -LiteralPath $Pack).Length

    $denylist = [pscustomobject]@{
        schema       = 'doom3-content-denylist/1'
        generated_by = 'content/tools/scan-pack.ps1'
        generated_from = [pscustomobject]@{ name = [System.IO.Path]::GetFileName($Pack); bytes = $size; sha256 = $packHash }
        sha256       = @($hashes | Sort-Object)
    }
    Write-Json -Object $denylist -Path $DenylistOut

    $inventory = [pscustomobject]@{
        schema        = 'doom3-pack-inventory/1'
        generated_by  = 'content/tools/scan-pack.ps1'
        pack          = [pscustomobject]@{ name = [System.IO.Path]::GetFileName($Pack); bytes = $size; sha256 = $packHash }
        entries       = $zip.Entries.Count
        files         = $files
        textures      = $textures
        materials     = $materials
        suffix_counts = $suffixes
        note          = 'Names, counts and hashes only. No member content is stored, and nothing here is evidence of a Q4 capture.'
    }
    Write-Json -Object $inventory -Path $InventoryOut

    if (-not $Quiet) {
        Write-Host ("PACK SCANNED: {0} ({1} bytes), {2} files, {3} textures, {4} materials" -f [System.IO.Path]::GetFileName($Pack), $size, $files, $textures, $materials)
        Write-Host ('  suffixes: ' + (($suffixes.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ', '))
        Write-Host "  denylist: $DenylistOut ($($hashes.Count) hashes)"
        Write-Host "  inventory: $InventoryOut"
    }
    exit 0
} finally {
    $zip.Dispose()
}
