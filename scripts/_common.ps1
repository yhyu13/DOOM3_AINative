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

function Get-ProcessesUsingExe {
    <#
      Return processes whose loaded executable path resolves to exactly $Path.
      Unlike Get-Process -Name, this cannot match a different build, a different
      executable (e.g. DOOM3XP.exe) or an unrelated copy of the game.
      Access-denied processes are skipped rather than treated as matches.
    #>
    param([Parameter(Mandatory = $true)][string]$Path)
    $full = [IO.Path]::GetFullPath($Path)
    @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        try { $candidate = $_.Path; $candidate -and ([IO.Path]::GetFullPath($candidate) -ieq $full) } catch { $false }
    })
}

function Stop-ProcessesUsingExe {
    <#
      Stop only the processes whose executable path matches $Path, then wait (bounded)
      for them to exit. Never stops every process with the same image name.
    #>
    param([Parameter(Mandatory = $true)][string]$Path, [int]$WaitSec = 10)
    $running = @(Get-ProcessesUsingExe -Path $Path)
    if ($running.Count -eq 0) { return }
    $running | Stop-Process -Force -ErrorAction SilentlyContinue
    $deadline = [DateTime]::UtcNow.AddSeconds($WaitSec)
    while ((Get-ProcessesUsingExe -Path $Path).Count -gt 0 -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 200 }
}
