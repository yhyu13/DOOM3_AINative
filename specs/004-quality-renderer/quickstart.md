# Quickstart: Quality-first rendering

**Status**: Planning guide. The standalone RHI source, test project and test names below do not exist until their implementation tasks are completed. The commands labeled **planned** are not evidence that a build or GPU test passed.

## Safety and prerequisites

- Windows 11 x64, Visual Studio 2022 C++ tools, Windows SDK and the Visual Studio-bundled CMake/CTest; locate installed tools rather than requiring machine-specific tracked paths.
- The standalone first slice uses synthetic data only; it does not require a DOOM 3 target copy, game assets, NVIDIA SDK or Internet download.
- Use a fresh build directory in the implementation worktree. Never use the root checkout's existing `build/` cache: during planning it pointed at another worktree. `run-win11-amd64.ps1 -Build` now refuses a cache whose `CMAKE_HOME_DIRECTORY` is a different checkout.
- Keep `doom3_orig/doom3` clean and do not modify game data. Check `git status --short` and `git -C doom3_orig/doom3 status --porcelain` before and after work.
- Do not run the native evidence scripts blindly. `scripts/verify-win11-amd64.ps1` exports patches to a fresh temporary directory and fails SC-006b on added/deleted rows; `scripts/smoke-win11-amd64.ps1` stops only processes whose executable path matches the resolved executable, requires `-StopExisting` (exit 12), and refuses a non-empty `-OutDir` without `-Clean` (exit 11); `scripts/run-win11-amd64.ps1` checks/stops that same executable before `-Build`, requires `-StopExisting` (exit 8), and refuses a non-empty `-OutDir` without `-Clean` (exit 9). Obtain consent before terminating a process or replacing output, and never delete game data.

## Q1a — Standalone RHI fixture (planned)

From the dedicated implementation worktree, discover the VS-bundled tools and configure a new directory. The source paths below are planned in [plan.md](./plan.md); adjust only after their implementation lands. Select an installation that actually has the C++ tools, a bundled CMake of at least 3.21 and CTest; do not assume the newest `vswhere -latest` result qualifies.

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'vswhere not found; no test was run' }
$installs = @(& $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$selected = $null
foreach ($root in $installs) {
  $candidateTools = Join-Path $root 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
  $cmakeCandidate = Join-Path $candidateTools 'cmake.exe'
  $ctestCandidate = Join-Path $candidateTools 'ctest.exe'
  if (-not (Test-Path $cmakeCandidate) -or -not (Test-Path $ctestCandidate)) { continue }
  if ((& $cmakeCandidate --version | Select-Object -First 1) -notmatch '(\d+)\.(\d+)\.(\d+)') { continue }
  $major = [int]$Matches[1]; $minor = [int]$Matches[2]
  if ($major -gt 3 -or ($major -eq 3 -and $minor -ge 21)) { $selected = $root; break }
}
if (-not $selected) { throw 'No VS installation with C++ tools, CMake >= 3.21 and CTest found; no test was run' }
$tools = Join-Path $selected 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $tools 'cmake.exe'
$ctest = Join-Path $tools 'ctest.exe'
$sdk = @(Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory |
  Sort-Object Name -Descending |
  Where-Object { Test-Path (Join-Path $_.FullName 'um\d3d12.h') } |
  Select-Object -First 1)
if (-not $sdk) { throw 'Windows SDK D3D12 headers not found; no test was run' }
$build = Join-Path (Get-Location) 'build-rhi-004'
if (Test-Path -LiteralPath $build) { throw 'Choose a fresh worktree-local build directory; do not overwrite an existing one' }
& $cmake -S tests/renderer -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw "RHI configure failed: $LASTEXITCODE" }
& $cmake --build $build --config Debug
if ($LASTEXITCODE -ne 0) { throw "RHI build failed: $LASTEXITCODE" }
$junit = Join-Path $build 'ctest-junit.xml'
& $ctest --test-dir $build -C Debug --no-tests=error -V --output-on-failure --output-junit $junit
if ($LASTEXITCODE -ne 0) { throw "RHI tests failed: $LASTEXITCODE" }
```

`--output-on-failure` alone retains only failures. Keep the verbose console output and/or the JUnit file as the full test log, including successful cases, adapter identity and executed/skipped checks.

Expected **after implementation**: the test summary names each hardware/software adapter case and confirms actual expected RGBA8 readback and fence/lifetime tests. Preserve the full summary, adapter identity and exit codes. CTest can exit zero when tests are skipped: inspect the per-test statuses and require the intended adapter case to have actually run. Missing GPU, debug layer or GPU-based validation must be recorded separately as SKIPPED/NOT RUN.

A software adapter functional pass is not a hardware-performance pass. A clear/readback pass is Q1a only: it does not demonstrate programmable draw, compute, MRT, readable depth, temporal inputs, presentation, engine parity or full Q1 validation.

For test-driven development, capture a real failing assertion before each code change, a green run without modifying that test, and a deliberate mutation that makes the test fail; restore the code and rerun the suite. A compiler error or skipped GPU test is not a failing behavioral assertion.

## Q0 — Classic baseline (data-dependent)

Follow the existing [native target documentation](../../targets/win11-amd64/README.md), but first apply the refusal-first flags above and use an explicit executable, dedicated output directory and legally supplied data. Record source/submodule and patch identities, build command, GPU/driver, data identity and the four scene categories specified in [spec.md](./spec.md). Missing data, counters or capture prerequisites produce SKIPPED, not PASS. Historical screenshots and earlier save/demo fixes are not a fresh run.

## Later gate checks

- **Q1b/Q1**: Extend the fixture to draw/compute/MRT/depth/velocity/overlay and resize/history/lifetime tests. The full gate requires the debug layer and GPU-based validation to have actually run with zero unwaived errors.
- **Q2/Q3**: Run documented GL/new-backend scene comparisons and analytic camera/rigid/skinned history tests; predeclare comparison thresholds and unsupported material cases.
- **Q4**: Review the legal hero set and moving sequences with recorded art, resource and performance evidence.
- **Q5**: Run the actual authorized SDK create/evaluate on the chosen backend only after access, hardware and distribution review. Otherwise report NOT RUN and test the complete off path.
- **Q6/Q7**: Capture a converged path-traced reference and dynamic case, or exercise each actual future target with a documented capability/fallback table. A compile or feature flag alone is not a pass.

Exact commands for Q1b–Q7 must be added when their implementations and fixtures exist. Each stage issue owns updating the commands for its own scope; T079 only re-runs commands already documented here. These are acceptance outlines, not already runnable tests.
