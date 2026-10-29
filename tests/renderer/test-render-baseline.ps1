#Requires -Version 5.1
# Compatibility entry point required by issue #21. The Q0 test logic and synthetic fixtures live in
# tests/baseline/ so this file does not add anything to tests/renderer/CMakeLists.txt or collide with
# the Q1a renderer fixture suite.
& (Join-Path $PSScriptRoot '..\baseline\test-render-baseline.ps1') @args
exit $LASTEXITCODE
