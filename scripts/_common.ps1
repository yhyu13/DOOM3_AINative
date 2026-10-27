# Shared helpers for the *-win11-amd64.ps1 scripts. Dot-source: . (Join-Path $PSScriptRoot '_common.ps1')

function Resolve-Doom3Exe {
    <#
      Locate DOOM3.exe. Order: $env:D3_EXE, then the newest of the root build
      (build\bin\Release, from the root CMakeLists.txt) and the stand-alone target build
      (targets\win11-amd64\build\Release). Returns the preferred path even when none exists,
      so callers can print a useful "exe missing" message.
    #>
    param([Parameter(Mandatory = $true)][string]$Repo)
    if ($env:D3_EXE) { return $env:D3_EXE }
    $candidates = @(
        (Join-Path $Repo 'build\bin\Release\DOOM3.exe'),
        (Join-Path $Repo 'targets\win11-amd64\build\Release\DOOM3.exe')
    )
    $found = @($candidates | Where-Object { Test-Path -LiteralPath $_ } | Sort-Object { (Get-Item $_).LastWriteTime } -Descending)
    if ($found.Count -gt 0) { return $found[0] }
    return $candidates[0]
}
