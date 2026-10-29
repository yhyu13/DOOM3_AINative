<#
.SYNOPSIS
  Property/fuzz test for the resolver reference model.

.DESCRIPTION
  Deterministic (seeded) generation over names that stress the key rules: case, extensions,
  path prefixes, dots inside keys, empty names and the 63-byte boundary. For every generated
  request it checks invariants that must hold for any manifest:

    I-1  draw is true iff code is OK
    I-2  a non-OK code never yields 'draw' in strict or tolerant mode
    I-3  NAME_TOO_LONG is decided before manifest lookup (a 63-byte key resolves, 64 is rejected)
    I-4  resolution is deterministic
    I-5  the key is the normalized name; path sensitivity is tested separately, including the
        last-dot-anywhere edge case

  The coverage floor is on distinct (name, keyword) requests, not on repeated calls. The default
  floor is 10,000, matching spec.md SC-010.

  Exit 1 on the first violated invariant or when the distinct-request floor is not met.
#>
[CmdletBinding()]
param(
    [int]$Iterations = 6000,
    [int]$Seed = 20261028,
    [int]$MinDistinct = 10000
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir 'resolver-model.ps1')

$key63 = 'c' * 63
$manifest = @(
    [pscustomobject]@{ key = 'alpha';   stages = @('vs', 'ps'); status = 'owned';       provenance = 'PV-T01'; source = 'x.hlsl'; entry = @{ vs = 'vs_main'; ps = 'ps_main' } },
    [pscustomobject]@{ key = 'beta';    stages = @('vs');       status = 'owned';       provenance = 'PV-T02'; source = 'x.hlsl'; entry = @{ vs = 'vs_main' } },
    [pscustomobject]@{ key = 'gamma';   stages = @('ps');       status = 'owned';       provenance = 'PV-T03'; source = 'x.hlsl'; entry = @{ ps = 'ps_main' } },
    [pscustomobject]@{ key = 'delta';   stages = @('vs', 'ps'); status = 'stub';        provenance = 'PV-T04'; notes = 'stub' },
    [pscustomobject]@{ key = 'epsilon'; stages = @('vs', 'ps'); status = 'unsupported'; provenance = 'PV-T05'; notes = 'unsupported' },
    [pscustomobject]@{ key = 'zeta';    stages = @('vs');       status = 'absent';      provenance = 'PV-T06'; notes = 'absent' },
    [pscustomobject]@{ key = 'with.dot'; stages = @('vs');      status = 'owned';       provenance = 'PV-T07'; source = 'x.hlsl'; entry = @{ vs = 'vs_main' } },
    [pscustomobject]@{ key = 'dup';     stages = @('vs');       status = 'owned';       provenance = 'PV-T08'; source = 'x.hlsl'; entry = @{ vs = 'vs_main' } },
    [pscustomobject]@{ key = 'dup';     stages = @('ps');       status = 'owned';       provenance = 'PV-T09'; source = 'x.hlsl'; entry = @{ ps = 'ps_main' } },
    [pscustomobject]@{ key = $key63;    stages = @('vs');       status = 'owned';       provenance = 'PV-T10'; source = 'x.hlsl'; entry = @{ vs = 'vs_main' } }
)

$manifestProblems = @(Test-Manifest -Manifest $manifest)
if ($manifestProblems.Count -gt 0) {
    $manifestProblems | ForEach-Object { Write-Host "FIXTURE INVALID: $_" -ForegroundColor Red }
    exit 1
}

# MANIFEST_INVALID is a manifest-level code, not a request-level one. Exercise the validator.
$validatorCases = 0
$badStatus = @([pscustomobject]@{ key = 'bad'; stages = @('vs'); status = 'bogus'; provenance = 'PV-T11' })
if (@(Test-Manifest -Manifest $badStatus).Count -eq 0) { Write-Host 'FAIL: manifest validator accepted an unknown status' -ForegroundColor Red; exit 1 }
$validatorCases++
$badOwned = @([pscustomobject]@{ key = 'nofile'; stages = @('vs'); status = 'owned'; provenance = 'PV-T12' })
if (@(Test-Manifest -Manifest $badOwned).Count -eq 0) { Write-Host 'FAIL: manifest validator accepted owned without source/entry' -ForegroundColor Red; exit 1 }
$validatorCases++

$allowed = @('OK', 'UNRESOLVED', 'STAGE_MISSING', 'STUB', 'UNSUPPORTED', 'ABSENT', 'AMBIGUOUS', 'NAME_TOO_LONG', 'MANIFEST_INVALID')
$failures = New-Object System.Collections.ArrayList
$counts = @{}
foreach ($c in $allowed) { $counts[$c] = 0 }

function Assert-Invariant {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { [void]$script:failures.Add($Message) }
}

# ---------------------------------------------------------------- targeted cases
$targeted = @(
    @{ name = 'alpha.vfp';       target = 'vs'; expect = 'OK';            why = 'plain' },
    @{ name = 'Alpha.VFP';       target = 'vs'; expect = 'OK';            why = 'case+extension' },
    @{ name = 'alpha';           target = 'ps'; expect = 'OK';            why = 'no extension' },
    @{ name = 'ALPHA.VP';        target = 'vs'; expect = 'OK';            why = 'case alias of another extension' },
    @{ name = 'dir/alpha.vfp';   target = 'vs'; expect = 'UNRESOLVED';    why = 'path is significant'; key = 'dir/alpha' },
    @{ name = 'dir.name/file';   target = 'vs'; expect = 'UNRESOLVED';    why = 'last dot anywhere, even in a directory name'; key = 'dir' },
    @{ name = 'with.dot.vfp';    target = 'vs'; expect = 'OK';            why = 'dot inside key' },
    @{ name = 'with.dot.extra.vfp'; target = 'vs'; expect = 'UNRESOLVED'; why = 'only last extension is stripped' },
    @{ name = 'beta.vfp';        target = 'ps'; expect = 'STAGE_MISSING'; why = 'vs-only program' },
    @{ name = 'dup.vfp';         target = 'vs'; expect = 'AMBIGUOUS';     why = 'duplicate key' },
    @{ name = $key63;            target = 'vs'; expect = 'OK';            why = '63-byte key resolves before lookup fails' },
    @{ name = ('b' * 63);        target = 'vs'; expect = 'UNRESOLVED';    why = '63 bytes is allowed' },
    @{ name = ('b' * 64);        target = 'vs'; expect = 'NAME_TOO_LONG'; why = '64 bytes is rejected' }
)
foreach ($t in $targeted) {
    $kw = if ($t.target -eq 'vs') { 'vertexProgram' } else { 'fragmentProgram' }
    $r = Invoke-ResolverRequest -Manifest $manifest -Keyword $kw -Name $t.name
    $stageResult = $r.stages[$t.target]
    Assert-Invariant ($stageResult.code -eq $t.expect) "targeted '$($t.name)' ($($t.why)): got $($stageResult.code), expected $($t.expect)"
    Assert-Invariant (([bool]$r.draw) -eq ($r.code -eq 'OK')) "targeted '$($t.name)': draw/code mismatch"
    $expectedKey = if ($t.ContainsKey('key')) { $t.key } else { Get-ProgramKey $t.name }
    Assert-Invariant ($stageResult.key -eq $expectedKey) "targeted '$($t.name)': key '$($stageResult.key)', expected '$expectedKey'"
}

# Alias group: every form must produce the same key.
$aliasKeys = @()
foreach ($n in @('alpha', 'alpha.vfp', 'Alpha.VFP', 'ALPHA.VP')) { $aliasKeys += (Get-ProgramKey $n) }
Assert-Invariant (@($aliasKeys | Sort-Object -Unique).Count -eq 1) "case/extension aliases produced different keys: $($aliasKeys -join ',')"

# ---------------------------------------------------------------- generated cases
$rng = New-Object System.Random($Seed)
$alphabet = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./_-'
$stems = @('alpha', 'beta', 'gamma', 'delta', 'epsilon', 'zeta', 'dup', 'with.dot', 'missing')
$exts = @('', '.vfp', '.vp', '.txt', '.VFP', '.hlsl')
$distinct = New-Object 'System.Collections.Generic.HashSet[string]'
$calls = 0

for ($i = 0; $i -lt $Iterations; $i++) {
    if (($i % 6) -eq 0) {
        $name = ($stems[$rng.Next($stems.Count)]) + ($exts[$rng.Next($exts.Count)])
        if ($rng.Next(4) -eq 0) { $name = $name.ToUpperInvariant() }
        if ($rng.Next(5) -eq 0) { $name = 'dir/' + $name }
    } else {
        $len = $rng.Next(0, 81)
        $chars = New-Object char[] $len
        for ($j = 0; $j -lt $len; $j++) { $chars[$j] = $alphabet[$rng.Next($alphabet.Length)] }
        $name = -join $chars
    }

    foreach ($keyword in @('vertexProgram', 'fragmentProgram')) {
        [void]$distinct.Add("$keyword|$name")
        $r1 = Invoke-ResolverRequest -Manifest $manifest -Keyword $keyword -Name $name
        $r2 = Invoke-ResolverRequest -Manifest $manifest -Keyword $keyword -Name $name
        $calls += 2

        Assert-Invariant ($allowed -contains $r1.code) "code '$($r1.code)' not in the enum for '$name'"
        Assert-Invariant ($r1.code -eq $r2.code) "non-deterministic code for '$name'"
        Assert-Invariant (([bool]$r1.draw) -eq ($r1.code -eq 'OK')) "draw/code mismatch for '$name' [$keyword]"
        if ($name.Length -gt 63) {
            Assert-Invariant ($r1.code -eq 'NAME_TOO_LONG') "length $($name.Length) did not produce NAME_TOO_LONG"
        }

        $stage = if ($keyword -eq 'vertexProgram') { 'vs' } else { 'ps' }
        Assert-Invariant ($r1.stages[$stage].key -eq (Get-ProgramKey $name)) "key mismatch for '$name' [$keyword]"

        if ($r1.code -ne 'OK') {
            foreach ($mode in @('strict', 'tolerant')) {
                $d = Get-ResolverDecision -Code $r1.code -Mode $mode
                Assert-Invariant ($d -ne 'draw') "non-OK code '$($r1.code)' decided 'draw' in $mode"
            }
        }
        $counts[$r1.code] = $counts[$r1.code] + 1

        if ($failures.Count -gt 0) { break }
    }
    if ($failures.Count -gt 0) { break }
}

if ($distinct.Count -lt $MinDistinct) {
    [void]$failures.Add("distinct requests $($distinct.Count) is below the floor $MinDistinct")
}

if ($failures.Count -gt 0) {
    Write-Host "FUZZ FAILED ($($failures.Count))" -ForegroundColor Red
    $failures | Select-Object -First 20 | ForEach-Object { Write-Host " - $_" }
    exit 1
}

$summary = ($allowed | ForEach-Object { "$_=$($counts[$_])" }) -join ' '
Write-Host "  $summary"
Write-Host "  manifest validator cases=$validatorCases, MANIFEST_INVALID not generated by requests"
Write-Host "FUZZ OK: $($distinct.Count) distinct requests ($calls calls) over $Iterations names (seed $Seed), 0 failures"
exit 0
