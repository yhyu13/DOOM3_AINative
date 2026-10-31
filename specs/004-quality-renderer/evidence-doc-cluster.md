# Documentation-cluster gate re-run (issue #91)

**Status**: author record of a re-run. It is not an independent review and it does not upgrade any
stage gate. It records, for the documentation cluster of issue #91, which of the commands documented
in [quickstart.md](./quickstart.md) this host can actually run, with the exit code each produced, and
which commands were not run and why.

**Scope note.** The issue brief assigns this re-run as "T077". In [tasks.md](./tasks.md), T077 is
worded as a tracked-path/provenance audit, and T079 is the task that re-runs documented quickstart
commands per stage. This record follows the brief's assignment (re-run) and reports the mismatch so
it is not silently renumbered; the coordinator should decide which task id owns it before ticking
either box.

## Identity and scope

- **Stage and claim**: documentation gates plus the runnable renderer fixtures. Claim: the commands
  below produced the stated exit codes on this host at this revision. **No** stage status is
  upgraded by this record.
- **Source identity**: `origin/main` `f9e84c8698727faeba55e19684fa3c8befa8f5d2`, branch
  `spec-004-docs`, no code, CMake, target-patch or submodule change in this cluster.
- **Submodule**: `doom3_orig/doom3` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` (`heads/master`),
  `git -C doom3_orig/doom3 status --porcelain` → 0 lines.
- **Data identity and rights**: no game data. The renderer fixtures are synthetic; the conformance
  spike serves a project-authored page from `tests/webgpu`. No `.pk4`/`.pk3`/`.pak` was read, copied
  or committed.
- **Environment**: different host from the Q0/Q4 records. Windows 10 Pro 19045; MSVC 19.38.33145.0;
  Windows SDK 10.0.26100.0; standalone CMake 4.1.2 and CTest 4.1.2 on `PATH` (not the VS-bundled
  copies); PowerShell 5.1; Node v22.16.0; `python` 3.13.12 with **no `py` launcher**; Chrome
  151.0.7922.76; GPUs `AMD Radeon RX 9070 XT` (32.0.21030.31) and `AMD Radeon(TM) Graphics`
  (32.0.21043.12001), plus an `OrayIddDriver` virtual display. The fixture prints the adapter
  **class** (`hardware`/`software`), not the adapter name, so the class is recorded and the device is
  identified only by the host listing.
- **Exclusions**: no engine build, no target copy, no DXR device, no provider, no Q0 capture, no
  Emscripten leg (§4).

## 1. Commands and exits

Run from the root of this worktree. Full logs are retained in the git-ignored
`build-q1b-rerun/` directory named in the last column.

| # | Command | Exit | Observed result | Retained log |
|---|---|---|---|---|
| 1 | `git rev-parse HEAD` | 0 | `f9e84c8698727faeba55e19684fa3c8befa8f5d2` | — |
| 2 | `git submodule status` | 0 | `a9c49da5afb18201d31e3f0a429a037e56ce2b9a doom3_orig/doom3 (heads/master)` | — |
| 3 | `git -C doom3_orig/doom3 status --porcelain` | 0 | 0 lines | — |
| 4 | `python -I scripts/validate-rhi-contract.py --self-test` | **1** | 26 failures, every one `prohibited D3D12 vocabulary in engine-facing header src\renderer\q4_harness\*.h` — see §3.1 | `g1.log` |
| 5 | `python -I scripts/test-validate-rhi-contract.py` | 0 | `Ran 42 tests … OK` | `g2.log` |
| 6 | `python -I scripts/test-validate-rhi-contract-mutations.py` | 0 | 11 protected behaviours disabled one at a time, each rejected; `restored: 42 tests, exit=0; tracked validator unchanged` | `g3.log` |
| 7 | `git diff --check` | 0 | clean at run time; one `LF will be replaced by CRLF` warning for a newly written Markdown file, not a whitespace error | — |
| 8 | `git diff --no-index --check -- /dev/null specs/004-quality-renderer/evidence-template.md` | 1 | file difference, no whitespace error (exit 1 is the documented expectation) | `w1.log` |
| 9 | `git diff --no-index --check -- /dev/null scripts/validate-rhi-contract.py` | 1 | file difference, no whitespace error | `w2.log` |
| 10 | `git diff --no-index --check -- /dev/null scripts/test-validate-rhi-contract.py` | 1 | file difference, no whitespace error | `w3.log` |
| 11 | `git diff --no-index --check -- /dev/null scripts/test-validate-rhi-contract-mutations.py` | 1 | file difference, no whitespace error | `w4.log` |
| 12 | `cmake -S tests/renderer -B build-q1b-rerun -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.38.33145.0, SDK 10.0.26100.0 selected; fresh directory | `configure.log` |
| 13 | `cmake --build build-q1b-rerun --config Debug -- -m` | 0 | all fixtures linked | `build-debug.log` |
| 14 | `ctest --test-dir build-q1b-rerun -C Debug --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 32` | `ctest-debug-2.log` |
| 15 | `cmake --build build-q1b-rerun --config Release -- -m` | 0 | all fixtures linked | `build-release.log` |
| 16 | `ctest --test-dir build-q1b-rerun -C Release --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 32` | `ctest-release-2.log` |
| 17 | `python -I scripts/validate-q1b-fixtures.py` | 0 | `PASS q1b-fixtures.md: pattern, tints, compute, MRT, depth and controls` | `g4.log` |
| 18 | `python -I scripts/validate-webgpu-mapping.py` | **1** | `INVALID: 62 problem(s); 57/119 declarations mapped` — see §3.2 | `g5.log` |
| 19 | `python -I scripts/validate-webgpu-mapping.py --self-test` | 0 | `baseline valid, 13 declarations, 14 negative controls detected` | `g6.log` |
| 20 | `python -I scripts/test-validate-webgpu-mapping.py` | **1** | same 62 unmapped declarations as #18 | `g7.log` |
| 21 | `node tests/webgpu/run-conformance.mjs --repeat 2 --out build/webgpu-q7a` | **2** | `attempt 1 failed: timed out waiting for conformance result`; Chrome 151 launched headless over the DevTools protocol and never posted a result | `webgpu-conformance.log`, `build/webgpu-q7a/run.log` |

The `-I` form is not the quickstart's literal `py -3 -I …`; see §2 row 1.

## 2. Not run, with the reason

| Documented command or block | State | Reason |
|---|---|---|
| `py -3 -I scripts/…` (three validator commands, quickstart's literal form) | `NOT RUN` as written | `py` is not on `PATH` on this host (`Get-Command py` fails); the equivalent `python -I …` was executed instead (rows 4–6, 17–20). Recorded as a substitution, not as the documented command. |
| Q1a discovery + configure block (VS-bundled CMake ≥ 3.21, `$selected` loop) | `NOT RUN` as written | The one Visual Studio installation with the C++ tools (`2022\Community`) has no `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, so the documented loop throws before any test runs. The configure/build/test **intent** was executed with the standalone CMake 4.1.2 on `PATH` (rows 12–16). |
| Emscripten encoder leg (`tests/webgpu/emscripten/build.ps1`, `--page emscripten/index.html`) | `NOT RUN` | `emcc` is not installed and `EMSDK` is unset. A missing or unauthorized toolchain is not a pass. |
| Q0 native build/capture | `NOT RUN` | No legal game data and no target copy; out of scope for a docs-only cluster and unchanged by it. |
| Q2/Q3/Q4/Q5/Q6 device and engine commands | `NOT RUN` | Not documented as runnable in `quickstart.md` (it says so explicitly); Q4's harness is its own binary and was not rebuilt here. |

## 3. Findings

### 3.1 The RHI header gate is red on `main`

Row 4 fails with 26 findings, all in `src/renderer/q4_harness/`: `harness.h`, `harness_impl.h`,
`content.h`, `dds.h`, `vram_probe.h` name `D3D12`, `ID3D12*`, `DXGI`, `windows.h`, descriptor
heap/range, root signature, LUID-family and `transition`. The contract
([contracts/rhi-foundation.md](./contracts/rhi-foundation.md) §Header gate) scans every engine-facing
header under `src/renderer/` except `src/renderer/d3d12/` and `src/renderer/ext/`, so the Q4 harness
sits outside the tier it is allowed to occupy. Consequences:

- FR-003's "scene and material interfaces must not depend on Windows-native handle types" and
  FR-017's lexical half are **not currently satisfied** on this tree, whatever the earlier records
  say;
- the fix is a `src/renderer/**` ownership decision (move the headers under `src/renderer/d3d12/` or
  `src/renderer/ext/`, or amend the scan roots by a reviewed contract change), so it was not done in
  this docs-only change.

### 3.2 The WebGPU mapping gate is red on `main`

Rows 18 and 20 fail identically: `INVALID: 62 problem(s); 57/119 declarations mapped`, with 62
`declaration not mapped` lines such as `DrawFrameDesc.passes`, `DrawPassDesc.colorClears`,
`SurfaceDesc.width`, `ShaderStage.Pixel`. T089 reconciled five declarations; declarations added by
the Q1b/Q2/Q3 slices afterwards were not mapped. The gate's own self-test (row 19) still passes, so
the validator is healthy and the mapping data is incomplete. FR-017 is therefore partial, and the
`54/54`-style mapping claim in earlier records does not hold on the current tree.

### 3.3 The recorded `0x887A002D` validation skip did not reproduce on this host

The Q1a/Q1b records, `tasks.md` and `plan.md` all state that the debug layer and GPU-based validation
are SKIPPED because `D3D12GetDebugInterface` returns `0x887A002D`, and that this is why T030/T031 stay
open. In row 14's run, `rhi.q1a.diagnostics` reports (quoted from the verbose run retained as
`ctest-debug.log`; rows 14/16 run the same suite without `-V`):

```
DIAGNOSTICS_SUMMARY cases_pass=8 fail=0 skip=0 validation_executed=6
DIAGNOSTICS class=hardware combo=debug_only debug=RUN gpu_validation=NOT_REQUESTED …
DIAGNOSTICS class=hardware combo=gpu_validation_only debug=RUN gpu_validation=RUN …
DIAGNOSTICS class=software combo=both debug=RUN gpu_validation=RUN …
```

with `errors=0` and 4 unwaived warnings per validation-enabled case, all of the form
`ID3D12CommandList::ClearRenderTargetView: The clear values do not match those passed to resource
creation`. This is a **different host** from the recorded one, so it does not contradict the earlier
observation; it does mean the stated reason for T030 being open is host-specific rather than a
property of the code. It does **not** close T030 or T031: the gate needs a re-run in its own stage
worktree and an independent review, and the four warnings are unwaived (SC-002 says zero unwaived
errors; the reviewer must decide whether warnings count). T030/T031 stay unchecked, and Q1 stays
incomplete.

## 4. What this record does and does not close

| Item | Effect |
|---|---|
| The three Python validator commands and the whitespace checks | Executed (rows 4–11). The structural checks pass; the header scan inside row 4 does not. |
| Renderer fixture suite on this host | Executed (rows 12–17): 32/32 CTest in Debug and Release, exit 0, plus the Q1b fixture validator. This is a fresh run at `f9e84c8`, not a re-statement of an earlier stage record. |
| Stage gates (Q1, Q2, Q3, Q4, Q5, Q6, Q7) | Unchanged. Nothing here upgrades a stage, and no DXR, DLSS or web claim follows. |
| Independent review of this record | `NOT RUN` — no reviewer has been assigned to this cluster; the reviewer named by the issue signs the PR, not this table. |

## 5. Retained artifacts

All under the git-ignored `build-q1b-rerun/` (relative to this worktree): `configure.log`,
`build-debug.log`, `build-release.log`, `ctest-debug.log` (verbose, the source of the §3.3 quote),
`ctest-release.log`, `ctest-debug-2.log`, `ctest-release-2.log`, `g1.log`–`g7.log`,
`w1.log`–`w4.log`, `webgpu-conformance.log`, and `build/webgpu-q7a/run.log`. Nothing in this list is
tracked, and no absolute machine path is written into a tracked file.
