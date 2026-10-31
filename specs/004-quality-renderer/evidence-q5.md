# Q5 optional-provider off-path evidence (T059)

**Status: off path PASS; the three-phase no-download observation is NOT RUN.** This record separates
the two claims T059 asks for: the off/missing-provider behavioural test executed and passed, while
the clean-default-build "no attempted NVIDIA download" property was **not** observed completely and
is recorded as `NOT RUN` for every phase. A passing off-path test does not prove SC-006.

## Identity and scope

- **Stage and claim**: T059 (issue #94), the non-proprietary optional-provider boundary. Claimed:
  with no provider, or with it disabled, unauthorized, unbuilt or unsupported, the feature is off,
  the off path is named rather than silently entered, the result is actionable and nothing crashes.
  Not claimed: that any provider exists, is authorized, is supported on this backend, or would
  improve quality or performance.
- **Source identity**: branch `spec-004-provider-offpath` from `origin/main` at `7c83cfd`
  (PR #92, T058 contract). Submodule `doom3_orig/doom3` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`,
  clean. New paths: `src/renderer/ext/provider.h`, `src/renderer/ext/provider.cpp`,
  `tests/renderer/ext/provider_offpath_test.cpp`, `tests/renderer/ext/CMakeLists.txt`, this file.
- **Data identity and rights**: none. The behavioural test uses synthetic values only; no game data
  was supplied, copied or committed.
- **Scene and inputs**: N/A. There is no scene, camera or capture. The default build produced
  `build/bin/Release/DOOM3.exe` and `DOOM3XP.exe`; the behavioural test is CPU-only.
- **Exclusions and limitations**:
  - No provider SDK, header, binary, authorization or hardware is present; the contract's U1-U15
    remain `Unknown` by design (see `provider-contract.md` §7).
  - The session is **not elevated**, so no complete process-trace or packet-capture observer is
    available (see §Available observers). This is why the download observation is `NOT RUN`.
  - No legal game data is present, so the default `run` phase is `NOT RUN` for "works".
  - The test project is deliberately stand-alone (`tests/renderer/ext/CMakeLists.txt`) and is **not**
    registered in `tests/renderer/CMakeLists.txt`; see §Registration.

## Environment and configuration

- **Toolchain and configuration**: Windows 10 19045 (the preset is named `win11-amd64`; the host is
  Windows 10), PowerShell 5.1, CMake 4.1.2, Visual Studio 2022 17.14.36518.9 with MSVC
  19.38.33145.0, Windows SDK 10.0.26100.0. Generator `Visual Studio 17 2022`, `x64`. Effective
  environment: `D3_DATA_DIR` unset, `WORKTREE_PATH` unset, session not elevated, no `py` launcher
  (`python` 3.13.12 used for the documentation validator).
- **Adapter and driver**: N/A. The default build is the existing OpenGL engine; no D3D12 device and
  no provider device is created by this task. The behavioural test opens no device.
- **Capability and fallback**: `ProviderCapability()` returns `supported=false` with all 15 register
  rows `Unknown`. Every selection resolves to `NoRequest`, `DisabledByRequest`, `NotAuthorized`,
  `NotBuilt` or `Unsupported`; `Ready` is unreachable and the test asserts it. The fallback is the
  non-proprietary reconstruction off path (`PlanReconstruction: Off`), selected and named.
- **NVIDIA SDK presence**: none observed. `CUDA_PATH` is unset, `nvcc` is not on `PATH`,
  `%ProgramFiles%\NVIDIA Corporation`, `%ProgramFiles%\NVIDIA GPU Computing Toolkit` and
  `%ProgramFiles(x86)%\NVIDIA Corporation` are absent, and no `nvapi*.h` was found under the
  installed program directories (depth 4).
- **Shader provenance and build**: N/A for this task; no shader was authored, compiled or changed.

## Predeclared observation method

Fixed before the first phase ran. It defines what would count as a `PASS`, so a later convenience
cannot redefine it.

### What must be observed

For each of the three phases (configure, build, run) of the **default** build, whether any process in
the phase's process tree attempted an NVIDIA download, including failed or blocked attempts and child
processes, and whether the phase's network traffic contains such an attempt.

### Available observers on this host

| Capability | Tool | State on this host |
|---|---|---|
| Complete process-tree event trace | WMI `Win32_ProcessStartTrace`, ETW `Microsoft-Windows-Kernel-Process`, Sysmon | **Unavailable**: the `Win32_ProcessStartTrace` subscription fails with `Access denied`; the session is not elevated |
| Complete packet capture | `pktmon`, `netsh trace`, `wpr` | **Unavailable**: `pktmon filter list` exits 2 without elevation |
| Process snapshot | `Get-CimInstance Win32_Process` | Available, **sampled** |
| Socket snapshot with owner PID | `Get-NetTCPConnection`, `Get-NetUDPEndpoint` | Available, **sampled**; short-lived and UDP traffic can be missed |
| Name-resolution history | `Get-DnsClientCache` | Available, a cache, not an audit log |

### Probe

One probe wraps each phase:

1. Launch the phase with a child process and take the root PID.
2. Until the root exits or the timeout expires, every iteration: snapshot `Win32_Process`, compute
   the root's descendant set, record every descendant name; record every non-time-wait TCP
   connection and every UDP endpoint whose owning PID is in that set; diff `Get-DnsClientCache`
   before and after.
3. Record the sample count and the observed process names, connections and DNS additions.

The measured sampling interval was **2-4 s**, not the 100 ms target, because the WMI/CIM and socket
queries dominate each iteration. This is recorded as a limitation, not hidden.

### Grading rules, fixed in advance

- A phase is `PASS` for "no attempted NVIDIA download" **only if both** the process-tree trace and
  the network capture are **complete**. The table above shows neither is available.
- Therefore every phase's no-download observation is `NOT RUN`; the sampled probe result is
  supplementary evidence and is never promoted to a pass.
- A phase whose command did not execute is `NOT RUN` for execution as well.
- The off/missing-provider behavioural test is graded separately, on its own exit code, and does not
  by itself prove the no-download property (T059, FR-014, SC-006).
- Artifact and dependency inspection is a separate check, not a substitute for phase observation.

## Procedure and retained artifacts

- **Predeclared tolerances**: N/A. The behavioural test asserts exact enum/boolean/string outcomes;
  no numeric tolerance is involved.
- **Commands and exits**: run from the worktree root. Full logs are in the git-ignored
  `build-observe/`, `build-ext-provider/` and `build/`.

  | # | Command | Exit | Observation |
  |---|---|---|---|
  | 1 | `cmake -S tests/renderer/ext -B build-ext-provider -G "Visual Studio 17 2022" -A x64` | 0 | fresh directory; MSVC 19.38.33145.0, SDK 10.0.26100.0 |
  | 2 | `cmake --build build-ext-provider --config Release -- -m` | 0 | `ext_provider.lib`, `provider_offpath_test.exe`; no warning line |
  | 3 | `.\build-ext-provider\Release\provider_offpath_test.exe` | **0** | `SUMMARY T059 provider off path checks=84 failures=0` |
  | 3a | `cmake --build build-ext-provider --config Debug -- -m` | 0 | same two targets, no warning line |
  | 3b | `.\build-ext-provider\Debug\provider_offpath_test.exe` | **0** | `SUMMARY T059 provider off path checks=84 failures=0` |
  | 4 | `ctest --test-dir build-ext-provider -C Release --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 1` |
  | 5 | `cmake --preset win11-amd64` (clean, `build/` removed first) | 0 | `Build files have been written to: build` |
  | 6 | `cmake --build --preset win11-amd64-release -- -m` (`> build\_build.log 2>&1`) | **0** | `DOOM3.exe` and `DOOM3XP.exe` linked |
  | 7 | `build\bin\Release\DOOM3.exe`, bounded 20 s launch | **-1073741819** (`0xC0000005`) | exited after a `WerFault.exe` child; no data load reached |
  | 8 | `dumpbin /dependents build\bin\Release\DOOM3.exe` (and `DOOM3XP.exe`) | 0 | imports listed below; no provider DLL |
  | 9 | `python -I scripts/validate-rhi-contract.py` | **1** | 26 pre-existing failures, all in `src/renderer/q4_harness/`; none in `src/renderer/ext/` |
  | 10 | `git diff --check` | 0 | no whitespace error |

  The first `cmake --preset win11-amd64` execution (not row 5) also created the git-ignored target
  copy through `new-target.ps1`; its probe recorded `cmake.exe, MSBuild.exe, powershell.exe,
  Robocopy.exe` and no connections. Row 5 is the clean re-run used for the exit code.

- **Retained logs and artifacts** (worktree-local, git-ignored):
  `build-observe/configure-clean.out`, `configure-clean-observation.log`, `build-observation.log`,
  `run-observation.log`, `dependents-doom3.txt`, `dependents-doom3xp.txt`, `validate-rhi.log`;
  `build-ext-provider-configure.log`, `build-ext-provider-build.log`, `build-ext-provider-run.log`,
  `build-ext-provider-build-debug.log`, `build-ext-provider-run-debug.log`,
  `build-ext-provider-ctest.log`; `build/_build.log`. Machine-specific artifacts are not committed.
- **Negative controls**: the behavioural test itself carries the refusal controls: a missing lease,
  an overlay input, an unspecified mask, a zero extent and a smaller display extent must each be
  refused. A mutation that made `PlanProvider` return `Ready` would fail
  "no authority/link combination reaches Ready". No separate harness mutation was run.

## Per-check results

`Execution` is `RUN` or `NOT RUN`; `Outcome` is `PASS`/`FAIL` only for a RUN check.

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| T059 off/missing-provider behavioural test | RUN | **PASS** | 84 checks, 0 failures, exit 0 in Release and Debug | `build-ext-provider-run.log`, `build-ext-provider-run-debug.log` |
| CTest registration of `provider.off_path` | RUN | **PASS** | 1/1 passed, exit 0 | `build-ext-provider-ctest.log` |
| Clean default configure without an NVIDIA SDK | RUN | **PASS** | exit 0; SDK absent per §Environment | `build-observe/configure-clean.out` |
| Clean default build without an NVIDIA SDK | RUN | **PASS** | exit 0; both executables linked | `build/_build.log` |
| Default engine **run** without an NVIDIA SDK | **NOT RUN** | — | no legal game data; the bounded launch faulted with `0xC0000005` before any data load, so it is not a working-run claim and the fault is not attributed to the provider work | `build-observe/run-observation.log` |
| Configure phase: process-tree download observation | **NOT RUN** | — | sampled only: 2 samples, ~2.0 s interval, 8 PIDs (`cmake.exe`, `cmd.exe`, `conhost.exe`, `MSBuild.exe`, `Tracker.exe`); no complete trace without elevation | `build-observe/configure-clean-observation.log` |
| Configure phase: network download observation | **NOT RUN** | — | no packet capture without elevation; sampled TCP/UDP: 0; DNS: none added | same |
| Build phase: process-tree download observation | **NOT RUN** | — | sampled only: 8 samples, ~4.0 s interval, 46 PIDs (`cl.exe`, `cmake.exe`, `cmd.exe`, `conhost.exe`, `MSBuild.exe`, `mspdbsrv.exe`, `Tracker.exe`); no download tool observed, but sampling is not proof of absence | `build-observe/build-observation.log` |
| Build phase: network download observation | **NOT RUN** | — | sampled TCP/UDP: 0; DNS: none added; incomplete coverage | same |
| Run phase: process/network observation | **NOT RUN** | — | the run phase itself is `NOT RUN`; the bounded launch sample saw only `DOOM3.exe` and `WerFault.exe`, 0 connections | `build-observe/run-observation.log` |
| Artifact inspection: NVIDIA-named files in `build/` and the target copy | RUN | **PASS** (artifact check only) | none matched `nvapi`, `nvcuda`, `nvngx`, `dlss`, `nvidia` | `dumpbin` output above |
| Dependency inspection: imports of both executables | RUN | **PASS** (artifact check only) | no provider DLL in the import table | `build-observe/dependents-*.txt` |
| Binary token scan for `nvapi64`, `nvcuda`, `nvngx`, `dlss` | RUN | **PASS** (artifact check only) | none found | this record |
| Download indicators in the configure and build logs | RUN | **PASS** (log scan only) | no `download`/`FetchContent`/`ExternalProject`/`git clone`/`curl`/`wget` line; the only `git clone` match is an instructional comment in `CMakeLists.txt` | `build-observe/configure-clean.out`, `build/_build.log` |
| **SC-006 / FR-014 no-download property** | **NOT RUN** | — | no complete process and network observation; the artifact checks cannot see an attempted, failed or in-process download | this record |

`dumpbin /dependents` for both executables reports only system components: `dbghelp.dll`,
`DINPUT8.dll`, `DSOUND.dll`, `IPHLPAPI.DLL`, `OPENGL32.dll`, `WINMM.dll`, `WS2_32.dll`,
`KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `SHELL32.dll`, `ole32.dll`, `OLEAUT32.dll`,
`ADVAPI32.dll`. No DLL is present beside the executables.

The byte scan of both executables finds the token `nvidia` but not `nvapi64`, `nvcuda`, `nvngx` or
`dlss`. The token comes from legacy OpenGL source comments and identifiers on the NV1x/NV2x
register-combiner path (`draw_nv10.cpp`, `draw_nv20.cpp`, `Session.cpp`), not from a linked or
bundled component. It is recorded here so the finding is not mistaken for a provider dependency.

## Registration

The behavioural test is registered by its own stand-alone CMake project at
`tests/renderer/ext/CMakeLists.txt`. `tests/renderer/CMakeLists.txt` was **not** modified, so the
test does not participate in `cmake -S tests/renderer`/`ctest --test-dir build-q*`. The exact
commands are rows 1-4 above. This was a deliberate file-ownership choice: another session needs the
shared test CMake file, and the ext tests need no device or backend library.

## Independent review

- **Independent reviewer**: NOT RUN, unassigned at the time of writing.
- **Review scope and commands**: none yet.
- **Review decision and unresolved items**: NOT RUN. The two open items are (a) the three-phase
  no-download observation needs an elevated session with `pktmon`/ETW and a complete process trace,
  or a host runner that provides one, and (b) the default engine run needs legal game data before
  SC-006's "run" can be recorded.

## Probe source

The exact probe used for rows 5-7 (retained in the git-ignored `build-observe/`):

```powershell
param(
    [Parameter(Mandatory = $true)][int]$RootPid,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$TimeoutSec = 900
)
$ErrorActionPreference = 'SilentlyContinue'
$deadline = (Get-Date).AddSeconds($TimeoutSec)
$seen = @{}
$net = New-Object System.Collections.ArrayList
$dnsBefore = @(Get-DnsClientCache | ForEach-Object { $_.Entry }) | Sort-Object -Unique
$samples = 0
$intervals = New-Object System.Collections.ArrayList
$last = Get-Date

while ((Get-Date) -lt $deadline) {
    if ($null -eq (Get-Process -Id $RootPid -ErrorAction SilentlyContinue)) { break }
    $all = @(Get-CimInstance -ClassName Win32_Process -Property ProcessId, ParentProcessId, Name)
    $tree = @($RootPid)
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($p in $all) {
            if (($tree -contains $p.ParentProcessId) -and -not ($tree -contains $p.ProcessId)) {
                $tree += [int]$p.ProcessId
                $changed = $true
            }
        }
    }
    foreach ($p in $all) {
        if ($tree -contains $p.ProcessId) { $seen[[int]$p.ProcessId] = $p.Name }
    }
    foreach ($c in @(Get-NetTCPConnection -ErrorAction SilentlyContinue)) {
        if (($tree -contains [int]$c.OwningProcess) -and ($c.State -ne 'TimeWait') -and ($c.State -ne 'CloseWait')) {
            $key = "TCP|" + $c.OwningProcess + "|" + $c.RemoteAddress + "|" + $c.RemotePort + "|" + $c.State
            if (-not $net.Contains($key)) { [void]$net.Add($key) }
        }
    }
    foreach ($u in @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue)) {
        if ($tree -contains [int]$u.OwningProcess) {
            $key = "UDP|" + $u.OwningProcess + "|" + $u.LocalAddress + "|" + $u.LocalPort
            if (-not $net.Contains($key)) { [void]$net.Add($key) }
        }
    }
    $now = Get-Date
    [void]$intervals.Add([int]($now - $last).TotalMilliseconds)
    $last = $now
    $samples++
    Start-Sleep -Milliseconds 100
}

$dnsAfter = @(Get-DnsClientCache | ForEach-Object { $_.Entry }) | Sort-Object -Unique
$dnsNew = @($dnsAfter | Where-Object { $dnsBefore -notcontains $_ })
$meanMs = 0
if ($intervals.Count -gt 0) {
    $sum = 0
    foreach ($i in $intervals) { $sum += $i }
    $meanMs = [int]($sum / $intervals.Count)
}
$lines = @()
$lines += "root_pid=$RootPid samples=$samples mean_interval_ms=$meanMs observed_pids=$($seen.Count)"
$lines += "observed_processes: " + (($seen.Values | Sort-Object -Unique) -join ', ')
$lines += "connections_and_endpoints: " + ($net.Count)
foreach ($n in $net) { $lines += "  $n" }
$lines += "dns_entry_added: " + ($dnsNew -join ', ')
$lines | Out-File -FilePath $Out -Encoding utf8
```
