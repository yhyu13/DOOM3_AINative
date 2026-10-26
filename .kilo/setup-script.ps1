# .kilo/setup-script.ps1
# Agent Manager runs this once per managed worktree, before the agent session starts.
# It materialises the DOOM 3 submodule for that worktree and prints the toolchain facts
# an agent needs. Keep it well under the 5 minute setup budget; do not build anything here.

$ErrorActionPreference = 'Stop'

$root = if ($env:WORKTREE_PATH) { $env:WORKTREE_PATH } else { Split-Path -Parent $PSScriptRoot }
if (-not (Test-Path -LiteralPath (Join-Path $root '.git'))) {
    throw "Not a git worktree: $root"
}

Push-Location -LiteralPath $root
try {
    Write-Host "worktree : $root"
    Write-Host "git-dir  : $((& git rev-parse --git-dir).Trim())"

    if (Test-Path -LiteralPath '.gitmodules') {
        Write-Host '--- submodules'
        & git submodule sync --recursive
        & git submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) { throw "git submodule update failed (exit $LASTEXITCODE)" }
        & git submodule status --recursive
    } else {
        Write-Host '--- no .gitmodules, skipping submodule init'
    }

    Write-Host '--- toolchain'
    $tools = @('git', 'node', 'uv', 'py', 'emcc')
    foreach ($tool in $tools) {
        $cmd = Get-Command $tool -ErrorAction SilentlyContinue
        if ($null -eq $cmd) {
            Write-Host ("{0,-5} missing" -f $tool)
            continue
        }
        $version = (& $tool --version 2>&1 | Select-Object -First 1)
        Write-Host ("{0,-5} {1}" -f $tool, $version)
    }

    Write-Host '--- notes'
    Write-Host 'python on PATH is 2.7: use `uv run --python 3.12 ...` for Python tooling.'
    Write-Host 'setup complete'
}
finally {
    Pop-Location
}
