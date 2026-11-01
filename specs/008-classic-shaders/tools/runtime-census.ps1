#Requires -Version 5.1
<#
.SYNOPSIS
  Enumerate the program requests a local, authorised data set can make (census.md 搂7 steps 2-3).
.DESCRIPTION
  Input : one or more .pk4 files holding base/ materials and glprogs, and the shipping manifest.
  Output: names and counts only. Material text is read in memory to find the program keywords; no
          shader text, no material text and no pak content is written or committed.
  The output JSON is the left-hand side of the census comparison: every requested (keyword, name)
  becomes a request, normalised with the engine's key rules (last extension stripped, lowercase,
  path kept). Step 6 of the procedure compares it with `shader_status` from a real run.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/runtime-census.ps1 `
    -Pk4 _data/run/base/demo00.pk4 -OutDir build/_census
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$Pk4,
    [string]$OutDir = 'build/_census',
    [string]$Manifest = 'shaders/manifest/classic-programs.json'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-NormalizedKey([string]$Name) {
    $n = $Name.Trim()
    $dot = $n.LastIndexOf('.')
    if ($dot -ge 0) { $n = $n.Substring(0, $dot) }
    return $n.ToLowerInvariant()
}

$requests = New-Object System.Collections.Generic.List[object]
$datasets = New-Object System.Collections.Generic.List[object]
$glprogs = New-Object System.Collections.Generic.List[string]
$maps = New-Object System.Collections.Generic.List[string]
$materialFiles = 0
$materialDeclarations = 0
$postProcess = 0
$linesRead = 0
$stageVariants = [ordered]@{}
$glasswarpStages = 0
$tierBStages = 0
$postProcessStages = 0

# One material declaration per block; a request belongs to the most recent `material` line.
$materialRe = [regex]'(?i)^\s*material\s+([^\s{]+)'
# Retail-era .mtr files may omit the `material` keyword: the name is a bare token at brace depth 0
# immediately before its opening block. Tracking depth is what keeps a nested property line such as
# `noShadows` (also followed by `{`) from being read as a material name.
$bareNameRe = [regex]'^([^\s{}]+)\s*\{?\s*$'
$keywordRe = [regex]'(?i)(?<![A-Za-z0-9_])(program|vertexProgram|fragmentProgram|megaTexture)\s+([^\s;{}]+)'
$postProcessRe = [regex]'(?i)(?<![A-Za-z0-9_])postProcess\s+([^\s;{}]+)'

foreach ($path in $Pk4) {
    if (-not (Test-Path -LiteralPath $path)) { Write-Host "RUNTIME CENSUS: pk4 not found: $path"; exit 2 }
    $full = (Resolve-Path -LiteralPath $path).Path
    $hash = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    $size = (Get-Item -LiteralPath $full).Length
    $zip = [IO.Compression.ZipFile]::OpenRead($full)
    try {
        $entries = @($zip.Entries)
        $fileEntries = @($entries | Where-Object { $_.Name -ne '' })
        $mapsHere = @($fileEntries | Where-Object { $_.FullName -match '^maps/.*\.map$' } | ForEach-Object { $_.FullName })
        $glHere = @($fileEntries | Where-Object { $_.FullName -like 'glprogs/*' } | ForEach-Object { $_.FullName })
        $mtrHere = @($fileEntries | Where-Object { $_.FullName -like 'materials/*.mtr' })
        $mapsHere | Sort-Object | ForEach-Object { if (-not $maps.Contains($_)) { $maps.Add($_) } }
        $glHere | Sort-Object | ForEach-Object { if (-not $glprogs.Contains($_)) { $glprogs.Add($_) } }
        $materialFiles += $mtrHere.Count
        $datasets.Add([ordered]@{
            pk4 = [IO.Path]::GetFileName($full)
            bytes = $size
            sha256 = $hash
            entries = $entries.Count
            maps = @($mapsHere | Sort-Object)
            glprogs = @($glHere | Sort-Object)
            materialFiles = $mtrHere.Count
        })

        foreach ($entry in $mtrHere) {
            $reader = New-Object IO.StreamReader($entry.Open())
            try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
            $lineNo = 0
            $current = ''
            $depth = 0
            foreach ($rawLine in ($text -split "`r?`n")) {
                ++$lineNo
                ++$linesRead
                $line = $rawLine
                $comment = $line.IndexOf('//')
                if ($comment -ge 0) { $line = $line.Substring(0, $comment) }
                $trim = $line.Trim()
                if ($depth -eq 0 -and $trim.Length -gt 0 -and -not $trim.StartsWith('{') -and -not $trim.StartsWith('}')) {
                    $m = $materialRe.Match($trim)
                    if ($m.Success) {
                        $current = $m.Groups[1].Value
                        ++$materialDeclarations
                    } else {
                        $b = $bareNameRe.Match($trim)
                        if ($b.Success) {
                            $token = $b.Groups[1].Value
                            if ($token -ine 'table' -and $token -ine 'material') {
                                $current = $token
                                ++$materialDeclarations
                            }
                        }
                    }
                }
                $depth += ([regex]::Matches($trim, '\{').Count - [regex]::Matches($trim, '\}').Count)
                if ($depth -lt 0) { $depth = 0 }
                foreach ($pp in $postProcessRe.Matches($line)) { ++$postProcess }
                foreach ($k in $keywordRe.Matches($line)) {
                    $keyword = $k.Groups[1].Value
                    $name = $k.Groups[2].Value
                    $megaFile = ''
                    $targets = if ($keyword -ieq 'program' -or $keyword -ieq 'megaTexture') { @('vs', 'ps') }
                               elseif ($keyword -ieq 'vertexProgram') { @('vs') }
                               else { @('ps') }
                    if ($keyword -ieq 'megaTexture') {
                        # Material.cpp forces megaTexture.vfp for this keyword (D-003); the file named
                        # after the keyword is the mega file, not the program.
                        $megaFile = $name
                        $name = 'megaTexture.vfp'
                    }
                    $requests.Add([ordered]@{
                        material = $current
                        file = $entry.FullName
                        line = $lineNo
                        keyword = $keyword
                        name = $name
                        key = (Get-NormalizedKey $name)
                        megaFile = $megaFile
                        targets = $targets
                    })
                }
            }

            # --- Tier B observation (ownership.md 3.1) -------------------------------------------------
            # An ordinary stage is one material stage block with no program keyword. Its variant is the
            # tuple (texgen, alpha test, vertex colour). Blend/depth/stencil/cull are pipeline state and
            # do not multiply the set. glassWarp belongs to Tier A, postProcess is a screen effect.
            $depth = 0
            $current = ''
            $post = $false
            $inStage = $false
            $st = $null
            $lineNo = 0
            foreach ($rawLine in ($text -split "`r?`n")) {
                ++$lineNo
                $line = $rawLine
                $comment = $line.IndexOf('//')
                if ($comment -ge 0) { $line = $line.Substring(0, $comment) }
                $trim = $line.Trim()
                if ($depth -eq 0 -and $trim.Length -gt 0 -and -not $trim.StartsWith('{') -and -not $trim.StartsWith('}')) {
                    $m = $materialRe.Match($trim)
                    if ($m.Success) { $current = $m.Groups[1].Value; $post = $false }
                    else {
                        $b = $bareNameRe.Match($trim)
                        if ($b.Success -and $b.Groups[1].Value -ine 'table' -and $b.Groups[1].Value -ine 'material') {
                            $current = $b.Groups[1].Value; $post = $false
                        }
                    }
                }
                if ($depth -eq 1 -and -not $inStage -and $trim -match '^(?i)postProcess(\s|$)') { $post = $true }
                if ($depth -eq 1 -and -not $inStage -and $trim.StartsWith('{')) {
                    $inStage = $true
                    $st = @{ texgen = 0; alpha = 0; vc = 0; program = $false; line = $lineNo }
                }
                if ($inStage) {
                    if ($trim -match '(?i)(?<![A-Za-z0-9_])(program|vertexProgram|fragmentProgram|megaTexture)\b') { $st.program = $true }
                    if ($trim -match '(?i)(?<![A-Za-z0-9_])texgen\s+(\S+)') {
                        switch -Regex ($Matches[1]) {
                            '^(?i)normal$' { $st.texgen = 1 }
                            '^(?i)reflect$' { $st.texgen = 2 }
                            '^(?i)skybox$' { $st.texgen = 3 }
                            '^(?i)wobbleSky$' { $st.texgen = 4 }
                        }
                    }
                    if ($trim -match '(?i)^screen(\s|$)') { $st.texgen = 5 }
                    if ($trim -match '(?i)^screen2(\s|$)') { $st.texgen = 6 }
                    if ($trim -match '(?i)^glassWarp(\s|$)') { $st.texgen = 7 }
                    if ($trim -match '(?i)^(video|sound)map(\s|$)') { $st.texgen = 5 }
                    if ($trim -match '(?i)(?<![A-Za-z0-9_])alphaTest\b') { $st.alpha = 1 }
                    if ($trim -match '(?i)(?<![A-Za-z0-9_])inverseVertexColor\b') { $st.vc = 2 }
                    elseif ($trim -match '(?i)(?<![A-Za-z0-9_])vertexColor\b') { $st.vc = 1 }
                }
                $newDepth = $depth + ([regex]::Matches($trim, '\{').Count - [regex]::Matches($trim, '\}').Count)
                if ($inStage -and $newDepth -le 1) {
                    if ($st.program) {
                        # Tier A owns program stages; they are not Tier B variants.
                    } elseif ($post) {
                        ++$postProcessStages
                    } elseif ($st.texgen -eq 7) {
                        ++$glasswarpStages
                    } else {
                        ++$tierBStages
                        $key = "t$($st.texgen)_a$($st.alpha)_v$($st.vc)"
                        if (-not $stageVariants.Contains($key)) {
                            $stageVariants[$key] = [ordered]@{
                                texgen = $st.texgen; alphatest = $st.alpha; vertexcolor = $st.vc;
                                stages = 0; materials = New-Object System.Collections.Generic.List[string]
                            }
                        }
                        $v = $stageVariants[$key]
                        $v.stages++
                        if ($current -and $v.materials.Count -lt 5 -and -not $v.materials.Contains($current)) { $v.materials.Add($current) }
                    }
                    $inStage = $false
                    $st = $null
                }
                $depth = $newDepth
                if ($depth -lt 0) { $depth = 0 }
            }
        }
    } finally { $zip.Dispose() }
}

# Aggregate by key; collisions matter because the resolver rejects duplicate keys (resolver.md 搂9).
$byKey = [ordered]@{}
foreach ($r in $requests) {
    if (-not $byKey.Contains($r.key)) {
        $byKey[$r.key] = [ordered]@{ key = $r.key; names = New-Object System.Collections.Generic.List[string]; requests = 0; materials = New-Object System.Collections.Generic.HashSet[string] }
    }
    $agg = $byKey[$r.key]
    if (-not $agg.names.Contains($r.name)) { $agg.names.Add($r.name) }
    $agg.requests++
    [void]$agg.materials.Add($r.material)
}
$keys = @()
$collisions = @()
$over63 = @()
foreach ($k in $byKey.Keys) {
    $agg = $byKey[$k]
    $keys += [ordered]@{
        key = $agg.key
        names = @($agg.names)
        requests = $agg.requests
        materialCount = $agg.materials.Count
    }
    if ($agg.names.Count -gt 1) { $collisions += [ordered]@{ key = $k; names = @($agg.names) } }
    foreach ($n in $agg.names) { if ($n.Length -gt 63) { $over63 += [ordered]@{ key = $k; name = $n; length = $n.Length } } }
}

# Step 6 comparison: manifest is the single source of truth (resolver.md I-7).
$unresolved = @()
$stageMissing = @()
$manifestKeys = @()
if (Test-Path -LiteralPath $Manifest) {
    $doc = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
    $manifestKeys = @($doc.programs | ForEach-Object { $_.key })
    $stagesByKey = @{}
    foreach ($p in $doc.programs) { $stagesByKey[$p.key.ToLowerInvariant()] = @($p.stages) }
    foreach ($k in $byKey.Keys) {
        if (-not $stagesByKey.ContainsKey($k)) { $unresolved += $k; continue }
        $have = $stagesByKey[$k]
        $want = @()
        foreach ($r in $requests) { if ($r.key -eq $k) { $want += $r.targets } }
        foreach ($t in ($want | Select-Object -Unique)) {
            if ($have -notcontains $t) { $stageMissing += [ordered]@{ key = $k; target = $t } }
        }
    }
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$result = New-Object System.Collections.Specialized.OrderedDictionary
$result['schema'] = 'doom3-runtime-census/1'
$result['generatedUtc'] = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
$result['datasets'] = $datasets.ToArray()
$result['materialFiles'] = $materialFiles
$result['materialDeclarations'] = $materialDeclarations
$result['linesRead'] = $linesRead
$result['requestCount'] = $requests.Count
$result['postProcessCount'] = $postProcess
$result['megaTextureFiles'] = @($requests | Where-Object { $_.megaFile } | ForEach-Object { $_.megaFile } | Sort-Object -Unique)
$variantList = @()
foreach ($k in ($stageVariants.Keys | Sort-Object)) {
    $v = $stageVariants[$k]
    $variantList += [ordered]@{
        texgen = $v.texgen
        alphatest = $v.alphatest
        vertexcolor = $v.vertexcolor
        stages = $v.stages
        materials = $v.materials.ToArray()
    }
}
$result['tierB'] = [ordered]@{
    boundProduct = 42
    observedVariants = $variantList.Count
    ordinaryStages = $tierBStages
    glasswarpStages = $glasswarpStages
    postProcessStages = $postProcessStages
    variants = $variantList
}
$result['distinctKeys'] = $byKey.Count
$result['manifest'] = $Manifest
$result['manifestKeys'] = @($manifestKeys)
$result['keys'] = @($keys)
$result['collisions'] = @($collisions)
$result['namesOver63'] = @($over63)
$result['unresolvedAgainstManifest'] = @($unresolved)
$result['stageMissingAgainstManifest'] = @($stageMissing)
$result['requests'] = $requests.ToArray()
$json = $result | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText((Join-Path $OutDir 'requests.json'), $json, (New-Object Text.UTF8Encoding($false)))

Write-Host ("RUNTIME CENSUS: pk4={0} entries={1} maps={2} glprogs={3} materialFiles={4}" -f `
    (($datasets | ForEach-Object { $_.pk4 }) -join ','), (($datasets | ForEach-Object { $_.entries }) -join ','), $maps.Count, $glprogs.Count, $materialFiles)
Write-Host ("RUNTIME CENSUS: materials={0} programRequests={1} distinctKeys={2} postProcess={3}" -f `
    $materialDeclarations, $requests.Count, $byKey.Count, $postProcess)
Write-Host ("RUNTIME CENSUS: collisions={0} namesOver63={1} unresolved={2} stageMissing={3}" -f `
    $collisions.Count, $over63.Count, $unresolved.Count, $stageMissing.Count)
if ($unresolved.Count -gt 0) { Write-Host ("RUNTIME CENSUS: unresolved keys: {0}" -f ($unresolved -join ', ')) }
if ($stageMissing.Count -gt 0) {
    Write-Host ("RUNTIME CENSUS: stage-missing: {0}" -f (($stageMissing | ForEach-Object { "$($_.key)/$($_.target)" }) -join ', '))
}
Write-Host "RUNTIME CENSUS OK: names and counts only written to $OutDir\requests.json"
exit 0

