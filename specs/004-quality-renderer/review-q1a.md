# Independent review — Issue #11 (Q1a): D3D12 RHI clear/readback foundation

**Date**: 2026-10-27
**Reviewer**: Claude Code subagent (not the implementer)
**Reviewed branch/worktree**: `feat/11-q1a-rhi-clear-readback`
**Review round**: final review after the N1–N7 fixes and the adversarial-audit
observations. This file replaces the first-round review record.

## 1. Independence, method and pinned state

I did not write, edit or design this implementation. I reviewed the final on-disk
state of the working tree, which has no implementation commit or pull request:

- the public header and D3D12 backend;
- all four test files and `tests/renderer/CMakeLists.txt`;
- `specs/004-quality-renderer/evidence-q1a.md` against the retained logs and BMPs
  in `build-rhi-004-fresh/`, `build-rhi-004/mutations/` and
  `build-rhi-004/q1a-red.log`;
- the read-only specification artifacts: `spec.md`, `plan.md`, `tasks.md`
  (T012–T021), `issue-q1a.md`, `contracts/rhi-foundation.md`, `quickstart.md` and
  `traceability.md`.

The reviewed state is pinned by SHA-256:

| File | SHA-256 |
|---|---|
| `specs/004-quality-renderer/evidence-q1a.md` (21,383 bytes, 381 lines) | `3b5c7c499b079c1cad9479bd895e3fae059ffbac070495aa756a966a1870a117` |
| `src/renderer/d3d12/rhi_d3d12.cpp` | `fc1357bb6d4bedbdbddf6e8705ca352fed711b9955d8c82dcddeb6f2edd19614` |
| `src/renderer/rhi/rhi.h` | `312e9f6bd158c9b79b89f66dd8ed440cf3ca9d4959efa231090357b049bc3352` |
| `tests/renderer/rhi_clear_test.cpp` | `8b3fd9ce65acfef8490f5981bd7d75c2c43b7c419be2b94dbbd591e06af29f6f` |
| `tests/renderer/rhi_lifetime_test.cpp` | `52f49f8905a9a638cd394edc1abac2561d89736e4ad5c1dc83184fc94ad1157a` |
| `tests/renderer/rhi_diagnostics_test.cpp` | `05e390e83ca398edf3b20b5be9919aea7d75d1aec74b6cd58fef22e1c3d338e8` |
| `tests/renderer/q1a_test_support.h` | `99bcb102c9cc972af491236c4402c305675daeedf8fda53e7f1e2791c706b50d` |
| `tests/renderer/CMakeLists.txt` | `b1c83f95996e4a00a02cbda47b686f71ad321bc061dadb2e7e6bce8cc56eaac5` |

The evidence file is the final revision, last modified 2026-10-27 23:00; I re-read
it at this hash after its last wording-only edits and re-checked the cited claims.
I re-hashed all seven source and test files at the same time: none changed after my
independent build, so the test results in section 3 remain valid for these bytes.

I deleted and recreated my own build directory, `build-rhi-004-review`, configured
it from `tests/renderer/CMakeLists.txt`, built Debug and Release, and ran CTest for
both with `-V --output-on-failure --output-junit`, recording exit codes and logs.
I did not trust the evidence file for any value that I could reproduce myself.

I did not edit `src/`, `tests/`, `evidence-q1a.md` or any other tracked file. I did
not run the mutation drivers in `build-rhi-004/` because they modify
`src/renderer/d3d12/rhi_d3d12.cpp`; I inspected their scripts, their retained logs
and the restored source. I did not touch `doom3_orig/doom3`, commit, push, create
branches or close issues. This file is the only tracked file I wrote.

`specs/004-quality-renderer/evidence-template.md` is still absent from this
worktree and branch, so no template-by-template comparison was possible. I reviewed
the evidence against issue #11, the RHI foundation contract, tasks T012–T021 and
`quickstart.md`.

## 2. Verdict and scope

**Q1a verdict: PASS** for issue #11's acceptance criteria and tasks T012–T021, at
the pinned state in section 1 (evidence SHA-256
`3b5c7c499b079c1cad9479bd895e3fae059ffbac070495aa756a966a1870a117`).
There are **no blocking findings and no open non-blocking findings**. The seven
first-round non-blocking findings N1–N7 are resolved in the final code, tests and
logs. The residual N8 wording concern is also resolved: the mutation sentence is
now scoped to the seven table mutants, and the following paragraph explicitly
identifies the discarded M5a log as the CTest-exit-0 exception and states that its
exact source change is not retained. The four
adversarial-audit observations raised after the first review are adjudicated in
section 5.1; none is blocking for Q1a.

The functional clear/readback, exact three-entry state journal, three negative
controls, fence-based lifetime refusal, shutdown with outstanding work,
incomplete-initialization shutdown, adapter reporting and the four diagnostic
request combinations all reproduce independently in Debug and Release.

**Scope: Q1a only.** This review does not approve full Q1, User Story 2, the
renderer programme, engine integration, or any WebGPU/Vulkan/browser path.

**Full Q1 remains BLOCKED.** The D3D12 debug layer and GPU-based validation did not
execute on this machine. `D3D12GetDebugInterface` returned `hr=0x887A002D`
(`DXGI_ERROR_SDK_COMPONENT_MISSING`) for both adapter classes, and the fixture
reports both as `SKIPPED`, never `PASS`. The evidence says so explicitly. Q1 cannot
be declared complete until those checks actually run with zero unwaived errors. A
software functional pass and an absent validation layer are not correctness or
performance evidence.

## 3. Independent environment, commands and results

Environment observed in my own logs and on this machine:

- Windows 11 Pro 10.0.22631, x64.
- MSVC 19.44.35228.0 (toolset 14.44.35207), Visual Studio 2022 Professional.
- Windows SDK 10.0.26100.0 (D3D12/DXGI headers).
- CMake/CTest 3.31.6-msvc6, Visual Studio-bundled. Neither is on the Git Bash
  `PATH`; I used the Visual Studio-bundled executables.
- Hardware adapter: `Intel(R) Graphics`, vendor `0x8086`, device `0x7d67`,
  dedicated video memory `134217728`, LUID `0x000000000001725f`.
- Software adapter: `Microsoft Basic Render Driver`, vendor `0x1414`, device
  `0x008c`, dedicated video memory `0`, LUID `0x00000000000176b1`.
- No game data, no engine target copy, no NVIDIA/DLSS SDK component.

| Step | Command (abbreviated) | Exit | Retained log |
|---|---|---|---|
| Configure (fresh) | `cmake -S tests/renderer -B build-rhi-004-review -G "Visual Studio 17 2022" -A x64` | 0 | `build-rhi-004-review/configure.log` |
| Build Debug | `cmake --build build-rhi-004-review --config Debug -- -m` | 0; no `warning C####` or `error C####` | `build-rhi-004-review/debug-build.log` |
| CTest Debug | `ctest --test-dir build-rhi-004-review -C Debug --no-tests=error -V --output-on-failure --output-junit <build-dir>/q1a-junit-debug.xml` | 0; 5/5 passed in 1.09 s | `build-rhi-004-review/ctest-debug.log`, `build-rhi-004-review/q1a-junit-debug.xml` |
| Build Release | `cmake --build build-rhi-004-review --config Release -- -m` | 0; no warnings | `build-rhi-004-review/release-build.log` |
| CTest Release | same as Debug with `-C Release` and the Release JUnit path | 0; 5/5 passed in 1.07 s | `build-rhi-004-review/ctest-release.log`, `build-rhi-004-review/q1a-junit-release.xml` |

Both JUnit files report `tests="5" failures="0" disabled="0" skipped="0"`.
`skipped="0"` is the CTest-test count; the in-test debug-layer and GBV checks are
`SKIPPED` and appear only in the verbose log (O1). CTest exit 0 does not by itself
prove execution because a test returning 77 is treated as skipped and still leaves
CTest successful; the per-adapter log is what proves execution (A4), and I checked
it.

Key reproduced values:

- `rhi.q1a.clear_readback`, both adapters and both submissions: `submit_status=Ok`;
  ids 1 and 2; completion serials 1 and 2; `gated=false`;
  `completed_before_wait=false`; `row_pitch=256`, `tight=16`,
  `staging_bytes=1024`; every row `mismatches=0 max_delta=0`;
  `release_status=Released`.
- Exact row bytes and hashes: `004080ff...` = `0x06b23876a7a2a8c5`;
  `ffbf8040...` = `0xa5581568c728313d`; `2060a0e0...` = `0x6c69b69da236ac65`;
  `1080f030...` = `0xf0fbaa39868107a5`. The expected rows are literal constants in
  `tests/renderer/q1a_test_support.h:29-34`, not values derived from the backend.
  The test's pass condition is byte comparison at a one-UNORM-step tolerance; in
  this run every actual hash also equalled its expected hash.
- `rhi.q1a.state_journal`, both adapters: `journal_size=3`, with exactly
  `render-target Unknown -> RENDER_TARGET`,
  `render-target RENDER_TARGET -> COPY_SOURCE`,
  `readback-buffer Unknown -> COPY_DEST`, in that order.
- `rhi.q1a.negative_controls`, both adapters: `wrong_stride` 3/4 rows,
  `off_by_one_row` 4/4 rows, `repeat_row_0` 3/4 rows;
  `NEGATIVE_CONTROL_SUMMARY applicable=3 all_detected=true`.
- `rhi.q1a.fence_outstanding_release`, both adapters: `gated_serial=1
  completed_before_release=0`; gated and ungated outstanding releases both
  `RefusedOutstanding`; `wait_before_gate_status=InvalidArgument`;
  `release_gate_status=Ok reason=` and `wait_gated_status=Ok reason=` (the
  stale-error assertions pass); gated readback rows OK;
  `release_bogus=InvalidSubmission`.
- The same test, lifetime edges: `SHUTDOWN submit_status=Ok serial=1 gated=true`,
  `SHUTDOWN status=Ok`, `SHUTDOWN second_status=Ok`; out-of-range adapter index
  returns `Unsupported` with "found 4 adapter(s)" for hardware and "found 1
  adapter(s)" for software, and two consecutive `Shutdown()` calls return `Ok`.
- `rhi.q1a.diagnostics`, both adapters and all four combinations: `none` =
  `NOT_REQUESTED`/`NOT_REQUESTED`; `debug_only` = `SKIPPED`/`NOT_REQUESTED`;
  `gbv_only` = `NOT_REQUESTED`/`SKIPPED`; `both` = `SKIPPED`/`SKIPPED`;
  `DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`.
  `debug_detail=D3D12GetDebugInterface failed (hr=0x887A002D)` and
  `gpu_validation_detail=D3D12GetDebugInterface (GPU-based validation) failed
  (hr=0x887A002D)`. Each requested case still executed a real clear/readback
  (`work_status=Ok`) and compared it. Captured messages were `captured=0
  message_count=0` in all eight cases.
- BMP evidence: my run wrote four 12,342-byte files. All four retained implementer
  files and all four of mine are 64x64, 24-bit, positive height (bottom-up), row
  stride 192, with four 16-row bands, top to bottom `#004080`, `#FFBF80`,
  `#2060A0`, `#1080F0`. I parsed headers and pixels programmatically with a
  disposable script, and separately viewed
  `build-rhi-004-fresh/q1a-evidence/q1a-clear-hardware-0.png` as an image: four
  distinct horizontal bands in that order. The four PNGs are 241 bytes each.
- Submodule `doom3_orig/doom3` is clean at
  `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`; `git status --porcelain -uall`
  lists exactly the nine new source, test and evidence files, all untracked.
- `0x887A002D` is defined as `DXGI_ERROR_SDK_COMPONENT_MISSING` in the installed
  Windows SDK (`shared/winerror.h`).

The evidence's final fresh run is a post-mutation serial rerun: the retained
`build-rhi-004-fresh` configure/build/CTest logs are timestamped 22:43, after the
last mutation log (22:37), and the current logs show the totals the evidence now
states, 1.02 s Debug and 1.06 s Release, with per-test times 0.14, 0.12, 0.12,
0.23, 0.41 s and 0.16, 0.14, 0.14, 0.24, 0.37 s. My independent run has slightly
different durations under different machine load, which is expected.

## 4. Acceptance criteria (issue #11 and T012–T021)

### Issue #11 acceptance criteria

| Criterion | Result | Concrete evidence |
|---|---|---|
| Standalone Windows x64 configure/build in a fresh worktree-local directory; no engine target copy, game data or proprietary SDK | PASS | Configure and both builds exit 0 in `build-rhi-004-review`; `tests/renderer/CMakeLists.txt:12-16` links only `d3d12 dxgi`; no FetchContent/ExternalProject; `git status` shows no `targets/` change. |
| `ctest --test-dir <fresh> -C Debug --no-tests=error -V` exits 0 and at least one adapter actually executed the clear/readback | PASS | `build-rhi-004-review/ctest-debug.log`: 5/5 passed; both `ADAPTER hardware RESULT PASS` and `ADAPTER software RESULT PASS`; four `ROW ... OK` lines per adapter. |
| `rhi.q1a.clear_readback` exits 0 with a 4x4 RGBA8 per-row pattern, correct states, row-pitched readback and independently derived expected bytes | PASS | Log: `row_pitch=256 tight=16 staging_bytes=1024`, `mismatches=0 max_delta=0` on every row; expectations are literal constants in `tests/renderer/q1a_test_support.h:29-34`; BMPs confirm the bands. |
| `rhi.q1a.state_journal` exits 0 and asserts `RENDER_TARGET -> COPY_SOURCE` and the readback buffer in `COPY_DEST` | PASS | `tests/renderer/rhi_clear_test.cpp:155-183` asserts exactly three entries and each entry's resource, `from` and `to` in order; log shows `journal_size=3` and the exact sequence on both adapters. |
| `rhi.q1a.negative_controls` exits 0 and internally requires three failures, with inapplicable controls reported NOT APPLICABLE and never counted as a pass | PASS | Log: all three `APPLICABLE detected=YES`; counts 3/4, 4/4, 3/4; `applicable=3 all_detected=true`. `tests/renderer/q1a_test_support.h:239-246` logs NOT APPLICABLE with a reason and excludes it from the result. |
| `rhi.q1a.fence_outstanding_release` exits 0 with at least two submissions and a release attempt while `GetCompletedValue() < signalValue`, both values logged; wait-idle shortcut control; incomplete-init shutdown exercised | PASS | Log: `gated_serial=1 completed_before_release=0`; both releases `RefusedOutstanding`; `wait_before_gate_status=InvalidArgument`; `SHUTDOWN` with unreleased gated work returns `Ok`; `INCOMPLETE_INIT ... Unsupported` for both classes; two `Shutdown()` calls safe. M5b is the wait-idle negative control. |
| Debug layer and GPU-based validation are opt-in, attempted, messages captured into the retained log, RUN/SKIPPED/NOT RUN recorded distinctly; no check marked passing without executing | PASS (attempt/reporting only) | `src/renderer/d3d12/rhi_d3d12.cpp:485-529` attempts `D3D12GetDebugInterface` when either flag is set, including GBV-only; `tests/renderer/rhi_diagnostics_test.cpp:64-91,126-137` runs four combinations per adapter on fresh devices and enforces the GBV-to-debug dependency; the log records `SKIPPED` with `hr=0x887A002D`. They are not reported as passing. |
| Debug layer and GPU-based validation actually execute | NOT RUN (SKIPPED) | `hr=0x887A002D` for both adapters and both requested classes. This blocks full Q1; it is not a Q1a pass. |
| `evidence-q1a.md` records commands, exit codes, configuration, adapter class, skipped checks, full log path, red/green/mutation evidence and changed files | PASS | All sections are present, every retained path and quoted value matches the current logs and artifacts, the final serial rerun is documented, and the red-evidence caveat is honest. |
| Submodule clean; no game data, binaries, build output or NVIDIA SDK tracked | PASS | `git -C doom3_orig/doom3 status --porcelain` is empty; `git status --porcelain -uall` lists only the nine new source/test/evidence files; no suspicious tracked extensions; CMake links no NVIDIA component. |
| Independent reviewer who did not write the code records a review | PASS | This file. |

### T012–T021

| Task | Result | Concrete evidence |
|---|---|---|
| T012 standalone opt-in CMake/CTest project and compilable public seam, no root engine configure, no game data, no proprietary SDK | PASS | `tests/renderer/CMakeLists.txt:1-54`; fresh configure/build exit 0; `src/renderer/rhi/rhi.h` includes no Windows/D3D12/DXGI header and exposes no native handle. |
| T013 public-seam clear test with per-row pattern, independent bytes/hashes and three controls; real assertion failure recorded | PASS | `tests/renderer/rhi_clear_test.cpp:14-92`; `q1a_test_support.h:29-34,174-237`; `build-rhi-004/q1a-red.log` shows five real behavioral failures, not compile errors; green run passes. |
| T014 adapter selection, actionable errors, resource creation with declared state sequence, journal, submission, fence wait, row-pitched readback, backend-neutral ownership | PASS | `src/renderer/d3d12/rhi_d3d12.cpp:235-379,594-819,872-927`; public header has no native types; logs show the sequence, pitch and adapter identities. |
| T015 run green, break row indexing/stride/state in turn, restore, record controls | PASS | Mutation logs M1, M2, M3, M3c and M3b all fail the unchanged test; the current source contains both refusal strings, no mutation marker and no mutation code; my fresh build is green. |
| T016 lifetime test with two submissions, unsignalled release attempt, incomplete-init/shutdown safety; real red failure recorded | PASS | `tests/renderer/rhi_lifetime_test.cpp:16-201`; red log test 4; green log with `gated_serial=1 completed_before_release=0`, both refusals, stale-error assertions and incomplete-init entries. Caveat O7: the red run predates the final shutdown-with-outstanding and `error.empty()` additions. |
| T017 fence-associated resource/allocator/readback lifetime; no wait-idle-before-every-release | PASS | `TryRelease` refuses while the gate is closed or `GetCompletedValue() < serial` (`rhi_d3d12.cpp:929-955`); no wait inside `TryRelease`; M5b proves a wait-idle shortcut fails. |
| T018 green, deliberate retirement break plus wait-idle shortcut failure recorded | PASS | `build-rhi-004/mutations/M4_release_outstanding_shortcut.log` (12 failed assertions per adapter, `***Timeout 120.03 sec`); `M5b_wait_idle_then_release.log` (`release_ungated_outstanding=Released`, 4 failed assertions per adapter, exit 8). |
| T019 opt-in debug layer/GBV and message capture, explicit adapter reporting, at least one actual execution, never report unavailable as pass | PASS | `tests/renderer/rhi_diagnostics_test.cpp:19-26,64-121,126-137`; logs show both adapters, `work_status=Ok`, four combinations, `SKIPPED` diagnostics with reasons, and `DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`; evidence states full Q1 is blocked. |
| T020 run quickstart commands in a fresh tree with `-V`/JUnit, record full log, config, adapter identities, exit codes, executed/skipped cases, actual execution | PASS | My independent run and logs in section 3; the evidence's final fresh logs and artifacts, including the post-mutation serial rerun, were checked value by value. |
| T021 independent review and rerun, including clean submodule and negative-control evidence | PASS | This file, the rerun in section 3, the submodule check and the mutation-log inspection. |

## 5. Findings

### Blocking

None. No defect prevents Q1a from meeting its acceptance criteria. Full Q1 remains
blocked by the external validation-layer limitation, not by a Q1a defect.

### Non-blocking

None open. The first-round findings are resolved as follows.

**N1 — Resolved: successful calls no longer leave stale error text.**
`src/renderer/d3d12/rhi_d3d12.cpp:539-542` (`Initialize`), `:815-818`
(`SubmitClearReadback`), `:831-836` and `:846-849` (`ReleaseGate`), `:866-869`
(`Wait`), `:923-926` (`Readback`) and `:951-954` (`TryRelease`) clear `*error` on
success. The lifetime test asserts `error.empty()` after the successful
`ReleaseGate` and both successful `Wait` calls
(`tests/renderer/rhi_lifetime_test.cpp:78-85,102-106`). My logs show
`release_gate_status=Ok reason=` and `wait_gated_status=Ok reason=` with no
`ASSERTION FAILED` lines.

**N2 — Resolved: evidence retained-log paths resolve.**
`specs/004-quality-renderer/evidence-q1a.md:53-58` names
`build-rhi-004-fresh/build-configure.log`, `build-debug.log`, `build-release.log`,
`q1a-ctest-debug.log`, `q1a-ctest-release.log`, `q1a-junit-debug.xml` and
`q1a-junit-release.xml`. All seven files exist at those paths. CTest resolves the
relative `--output-junit` name inside `--test-dir`, which yields exactly the
claimed `build-rhi-004-fresh/q1a-junit-*.xml`. My own run used an explicit path
under `build-rhi-004-review`, so no ambiguity remains.

**N3 — Resolved, including the former N8 residual.**
`specs/004-quality-renderer/evidence-q1a.md:278-284` now says "Every log for the
seven mutations in the table contains `=== BUILD exit=0 ===`, `=== CTEST exit=8
===` and `=== RESTORE build exit=0 ===`", scoping the quantifier correctly, and
`:296-302` explicitly identifies the discarded M5a log as the CTest-exit-0
exception retained only as a rejected attempt and states that its driver was
overwritten when M5b was defined, so its exact source change is not retained. I
verified that all seven table logs (M1, M2, M3, M3c, M3b, M4, M5b) contain all
three markers and that the M5a log contains `BUILD exit=0` and `CTEST exit=0`
with no restore line. The source on disk is restored: both refusal strings are
present and no `Deliberate mutation` marker remains. The final fresh run is a serial post-mutation rerun (logs at 22:43,
last mutation at 22:37). No wording issue remains.

**N4 — Resolved: M3b description and M5a/M5b naming corrected.**
`evidence-q1a.md:292` says the five positive journal assertions fail and the
negative not-copied check is vacuous on an empty journal.
`build-rhi-004/mutations/M3b_barrier_state.log` matches: `submit_status=Error`,
`journal_size=0`, five positive journal assertion failures. The table uses M5b and
the text explains why the earlier M5a attempt was discarded (`:296-302`).

**N5 — Resolved: four diagnostic combinations per adapter and the dependency
assertion exist.**
`tests/renderer/rhi_diagnostics_test.cpp:126-137` runs `none`, `debug_only`,
`gbv_only` and `both` for both adapter classes, each on a fresh device
(`:30-49`). `:80-91` asserts that `gpuBasedValidation == Run` implies
`debugLayer == Run`, and `:64-78` constrains the GBV-only debug-layer state to
`NotRequested` or `Run`, never an unattempted skip. `:106-113` asserts zero errors
when the debug layer actually runs. My run produced
`DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`.

**N6 — Resolved: `Shutdown()` reports a failed teardown and is tested with
outstanding work.**
`src/renderer/rhi/rhi.h:161-165` documents the return value;
`src/renderer/d3d12/rhi_d3d12.cpp:545-576` signals unreleased gates, waits for each
unreleased serial, sets `Status::Error` when a wait fails, and still drops
references because this is test-process teardown. `RunShutdownWithOutstanding`
(`tests/renderer/rhi_lifetime_test.cpp:126-155`) submits gated work that is not
complete, asserts `Shutdown()` returns `Ok`, `IsInitialized()` is false and a
second `Shutdown()` is `Ok`. `ForEachAdapter` additionally asserts a clean
`Shutdown()` after every callback
(`tests/renderer/q1a_test_support.h:367-374`). The forced wait-failure path is
implemented but not triggered, and the evidence says so honestly
(`evidence-q1a.md:210-215`; see A3).

**N7 — Resolved: a GBV-only request attempts debug-layer enablement.**
`src/renderer/d3d12/rhi_d3d12.cpp:485-529` enters the enablement block when either
flag is set. On success it enables the debug layer, records it as `Run` with the
reason "invoked implicitly because GPU-based validation was requested", and then
enables GBV. On failure it records GBV as `Skipped` with the
`D3D12GetDebugInterface (GPU-based validation)` detail; a GBV-only request leaves
the unrequested debug loop at `NotRequested`. The log shows exactly that for
`gbv_only` on both adapters, and the test accepts only `NotRequested` or `Run` for
the debug layer in that combination
(`tests/renderer/rhi_diagnostics_test.cpp:67-78`).

### 5.1 Adversarial-audit observations (adjudicated)

**A1 — The clear test asserts a valid pitch relationship, not the literal
256/1024 values. Not blocking for Q1a.**
`tests/renderer/rhi_clear_test.cpp:54-59` asserts `rowPitch >= tightRowPitch` and
`staging.size() == rowPitch * height`, and the per-row comparison uses the tight
packing that the backend produced from the reported pitch. The evidence records
the observed exact values, `row_pitch=256`, `tight=16`, `staging_bytes=1024`
(`evidence-q1a.md:127`), and I reproduced them. Why this is not blocking: the Q1a
contract requires accounting for the returned D3D12 row pitch, not a hard-coded
pitch; `GetCopyableFootprints` fixes the pitch for this texture, and a misreported
pitch corrupts the tight packing and fails the row comparison. Hard-coding 256
would over-specify the test and make it brittle on a conforming D3D12
implementation. A tighter assertion could be added in Q1b, but nothing in the
current run is ambiguous.

**A2 — A requested diagnostics combination returning `Unsupported` is reported as
SKIPPED, so an aggregate pass could in theory include skipped requested cases. Not
blocking for Q1a.**
`tests/renderer/rhi_diagnostics_test.cpp:39-43` returns 77 for an `Unsupported`
initialize, and `main` (`:141-166`) counts passes, failures and skips. On this
machine all eight cases executed: `DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`, and
the per-case log shows `work_status=Ok` for every requested combination. Why this
is not blocking: adapter unavailability comes from `SelectAdapter`
(`rhi_d3d12.cpp:235-336`), which is independent of diagnostic flags, so an
unavailable adapter class also skips its `none` case; the current backend never
returns `Unsupported` merely because a diagnostic failed, it records `Skipped`
(`rhi_d3d12.cpp:485-529`). An unavailable check is never reported as PASS, which
is the contract requirement. The residual scenario requires a future backend that
fails device creation only when diagnostics are enabled; for Q1b the test should
fail such a requested case instead of skipping it, or require `skip=0` whenever
`pass>0`.

**A3 — The `Shutdown()` failed-wait `Error` path and the software-adapter
device-creation `Unsupported` mapping are untested. Not blocking for Q1a.**
`src/renderer/d3d12/rhi_d3d12.cpp:563-568` sets `Status::Error` when an outstanding
serial does not complete, and `:342-347` maps a software device-creation failure to
`Unsupported`. Neither path is triggered on this machine: the gated shutdown
completes, and WARP creates a device. The normal and incomplete-initialization
shutdown paths are tested and pass. Why this is not blocking: the failed-wait path
needs a hung or removed device, which is not safely inducible in this synthetic
Q1a test; the evidence states the path was not triggered
(`evidence-q1a.md:213-215`). Both behaviors are straightforward by inspection and
belong to device-removal coverage in Q1b.

**A4 — CTest exit 0 with `SKIP_RETURN_CODE 77` does not by itself prove that an
adapter executed. Not blocking for Q1a.**
`tests/renderer/CMakeLists.txt:44-54` sets `SKIP_RETURN_CODE 77`, and
`ForEachAdapter` (`q1a_test_support.h:340-394`) returns 0 only when at least one
adapter passed, 77 when all were skipped. CTest treats 77 as skipped and can still
exit 0. Why this is not blocking: the issue criterion is explicitly log-based
("the retained log shows at least one adapter actually executed"), the evidence
does not rely on the exit code alone, and I verified both
`ADAPTER hardware RESULT PASS` and `ADAPTER software RESULT PASS` with per-row
`OK` lines in both configurations. Any future CI gate should assert that log
content rather than the CTest exit alone; the evidence's acceptance mapping
already cites the per-adapter lines.

### Observations

- **O1 — "100% tests passed" and JUnit `skipped="0"` are not validation evidence.**
  `tests/renderer/CMakeLists.txt:44-54` registers five CTest tests, all of which
  ran; the in-test debug-layer and GBV checks are `SKIPPED` (section 3). The
  evidence is explicit about this. Required action: none for Q1a; keep the
  distinction in Q1 reporting.
- **O2 — Unused public enum values.**
  `Status::DeviceLost` and `Status::OutOfMemory`
  (`src/renderer/rhi/rhi.h:31-32`) and `DiagnosticState::Failed`
  (`src/renderer/rhi/rhi.h:55`) are never produced. `DiagnosticState::NotRun` is
  used transiently during `Initialize`. These are not operations and do not breach
  the contract. Required action: Q1 should use or remove them.
- **O3 — Resolved: padding bytes in the diagnostic staging copy are documented.**
  `src/renderer/rhi/rhi.h:116-121` states that `ReadbackResult::staging` holds
  `height * rowPitch` bytes and that bytes beyond each row's `tightRowPitch` are
  padding and not guaranteed written. The controls read only the first 16 bytes of
  each row, so no assertion depends on padding. No action needed.
- **O4 — Mutation evidence is not repository-reproducible.**
  The mutation scripts and logs exist only under `build-rhi-004/`, which is ignored
  by `.gitignore:25` (`build-*/`) and `:71` (`*.log`), and the scripts contain
  machine-specific paths. This is not a tracked-file violation, but a clean
  checkout cannot rerun the mutations. Required action: Q1b should keep the
  mutation driver in the repository or record the exact patch/hunk.
- **O5 — Resolved: a passing test shuts down with work still outstanding.**
  `RunShutdownWithOutstanding`
  (`tests/renderer/rhi_lifetime_test.cpp:126-155`) submits gated work that has not
  completed, verifies it is incomplete, then requires `Shutdown()` to return `Ok`,
  the device to be uninitialized and a second `Shutdown()` to be `Ok`. The logs
  show `SHUTDOWN status=Ok` and `SHUTDOWN second_status=Ok` on both adapters.
- **O6 — Resolved: the PNG visual check is independently reproducible.**
  The four PNGs exist at 241 bytes each in
  `build-rhi-004-fresh/q1a-evidence/`. I viewed
  `q1a-clear-hardware-0.png` as an image and saw four distinct horizontal bands in
  the expected order, and I parsed all four BMPs programmatically. The byte
  comparison remains the authoritative check, and the PNGs are still confined to a
  git-ignored build directory.
- **O7 — The red log's lifetime test also predates the final test additions.**
  The evidence explicitly and honestly caveats that test 5 in
  `build-rhi-004/q1a-red.log` predates the four-combination diagnostics rewrite
  (`evidence-q1a.md:96-102`). The same is true of test 4: the red section contains
  no `SHUTDOWN` lines, so it predates `RunShutdownWithOutstanding` and the
  `error.empty()` assertions added with the N1/N6 fixes. The evidence does not
  claim the final form of test 4 was red; it says the red record for T013/T016 is
  tests 1-4, which is true for the tests as they existed then. Required action:
  none for Q1a; if the project wants red-first evidence for every final assertion,
  the new shutdown and stale-error assertions need their own red run.

## 6. Verification of the evidence document's claims

### Reproduced independently

- Debug and Release configure/build/CTest exit 0 and 5/5 passed in
  `build-rhi-004-review`; exact per-test times and totals differ slightly from the
  implementer's final serial run, as expected.
- The evidence's final retained logs are the post-mutation serial rerun: configure,
  build and CTest logs at 22:43 after the last mutation at 22:37, with totals 1.02 s
  Debug and 1.06 s Release and per-test times exactly as quoted
  (`evidence-q1a.md:60-68`).
- Adapter identities exactly as tabulated; four hardware candidates and index 0;
  one software candidate after WARP de-duplication against the software-flagged
  adapter.
- `row_pitch=256`, `tight=16`, `staging_bytes=1024`; all rows `mismatches=0
  max_delta=0`; `release_status=Released`. The evidence now states correctly that
  the pass condition is `mismatches=0` at a one-UNORM-step tolerance and that the
  recorded hashes are diagnostics, not an equality requirement
  (`evidence-q1a.md:132-139`); in this run the hashes nevertheless matched exactly.
- The exact three-entry state journal on both adapters, including the recorded
  `RENDER_TARGET -> COPY_SOURCE` transition and the exact ordered-sequence
  assertions.
- The three negative controls and their claimed mismatch counts
  (3/4, 4/4, 3/4; `applicable=3 all_detected=true`).
- `gated_serial=1`, `completed_before_release=0`, both outstanding releases
  `RefusedOutstanding`, gate-before-wait refusal, bogus-release rejection,
  `error.empty()` after successful calls, shutdown with unreleased gated work,
  incomplete-init `Unsupported` for both classes and two safe `Shutdown()` calls.
  The gated readback is compared with the same tolerance-1 assertion, as the
  evidence now says (`evidence-q1a.md:191-196`).
- All four diagnostic combinations per adapter: `SKIPPED` states and details with
  `hr=0x887A002D`, `NOT_REQUESTED` for unrequested diagnostics, `work_status=Ok`
  in every requested case, and `DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`
  (`evidence-q1a.md:219-253`).
- BMPs: 12,342 bytes, 64x64, 24-bit, positive height (bottom-up), row stride 192,
  four 16-row bands `#004080`, `#FFBF80`, `#2060A0`, `#1080F0`, for all four files
  in my run and all four retained implementer files; parsed programmatically, with
  one PNG also viewed directly. The PNGs are 241 bytes each in the same directory,
  as the evidence now states (`evidence-q1a.md:265-271`).
- Red evidence: `build-rhi-004/q1a-red.log` shows five failed tests with
  `ASSERTION FAILED: adapter initialization failed ...` and `0% tests passed, 5
  tests failed out of 5`. The raw log does not record the shell exit code 8; 8 is
  CTest's standard test-failure code, and the evidence flags that limitation
  rather than hiding it. The evidence's added statement that test 4 also logged
  two `out-of-range adapter index did not return Unsupported` assertion failures
  with `INCOMPLETE_INIT ... status=Error reason=Q1a backend stub: not implemented`
  is present in the red log. Test 5's `ASSERTION FAILED: diagnostics Initialize
  failed` and `DIAGNOSTICS_SUMMARY pass=0 fail=2 skip=0` match the evidence, and
  the stated caveat that it predates the four-combination rewrite is confirmed by
  the red section containing no combination labels.
- Mutation logs M1, M2, M3, M3c, M3b, M4 and M5b match the claimed failures:
  M1 rows 1-3 `mismatches=16` with actual zeros; M2 row 1 `mismatches=12`, rows
  2-3 `mismatches=16`; M3 `journal_size=2` and three failed assertions; M3c
  `journal_size=4` and both count/sequence assertions fail; M3b submission Error,
  `journal_size=0` and five positive assertions fail; M4 both outstanding releases
  `Released`, 24 total failed assertions (12 per adapter) and
  `***Timeout 120.03 sec`; M5b exit 8 in 60.47 s,
  `release_ungated_outstanding=Released`, 4 failed assertions per adapter. The
  seven table logs contain `BUILD exit=0`, `CTEST exit=8` and
  `RESTORE build exit=0`; the discarded M5a log
  (`build-rhi-004/mutations/M5_wait_idle_before_release.log`) contains
  `BUILD exit=0` and `CTEST exit=0` with no restore marker, exactly as the scoped
  sentence and the following paragraph now say.
- Ten retained `repeat-*.log` files all show 5/5 and `repeat-summary.log` records
  exit 0 for each.
- Every evidence path exists and every quoted value checked matches, including the
  final serial rerun times, the post-mutation ordering and the PNG paths.
- Debug layer and GPU-based validation are reported as `SKIPPED` with
  `hr=0x887A002D`, never `PASS`, in both the evidence and my run.
- No Playwright, browser, WebGPU or Vulkan claim is made. The evidence states that
  those checks are not applicable to Q1a, and my run contains no such test.

### Not reproduced

- The implementer's exact `build-rhi-004-fresh` directory: I created my own
  `build-rhi-004-review` and compared values.
- The full ten-run repeat: I read all ten retained logs and ran one fresh
  Debug/Release pair rather than ten fresh repeats.
- The mutation scripts: not executed because they modify `src/`; only their
  scripts, retained logs and the restored source were inspected.
- The historical claim that the worktree was clean at the baseline commit before
  the Q1a files were added: this cannot be verified from the current working tree,
  which has no commit for the implementation.
- The discarded M5a mutation's exact source change: its driver was overwritten
  when M5b was defined and is not retained, so what M5a changed cannot be verified
  from an artifact. The evidence now says exactly this and presents "waiting on
  the gate before applying the fence refusal" as a consistency explanation for
  the 60.12 s passing run, not as a proven fact (`evidence-q1a.md:296-302`). The
  M5b driver and log on disk match the wait-idle shortcut, and M5b is the control
  the table relies on.
- `evidence-template.md`: absent from this worktree, so no template comparison was
  possible.

### Inaccurate as written

None at SHA-256 `3b5c7c499b079c1cad9479bd895e3fae059ffbac070495aa756a966a1870a117`.
The earlier N8 wording issue is fixed: the mutation-RESTORE sentence is scoped to
the seven table mutants, and the discarded M5a exception is stated in the
following paragraph, including that its exact source change is not retained. The
tolerance wording now matches the test's actual `mismatches=0`/one-UNORM-step pass
condition rather than claiming hash equality, and the PNG paths resolve.

## 7. Limitations

- Debug layer and GPU-based validation did not run; both are `SKIPPED` with
  `hr=0x887A002D`. Full Q1 is blocked until they execute with zero unwaived errors.
- The hardware result is from an Intel integrated GPU; the software result is WARP.
  A software pass is a functional check only and is not performance evidence.
- No performance, memory, resize, device-removal, presentation, draw/compute/MRT/
  readable-depth, temporal, engine-integration, game-data, parity or visual-quality
  claim is made or supported by this review.
- No WebGPU, Vulkan, browser or emscripten path is reviewed or claimed; browsers do
  not expose D3D12. Playwright and browser tests are not applicable to Q1a because
  there is no web page, DOM or WebGPU backend in this slice.
- The reviewed implementation is the untracked working-tree content of
  `feat/11-q1a-rhi-clear-readback`; no implementation commit or PR existed at
  review time. The SHA-256 table in section 1 pins the exact reviewed bytes.
- Mutation evidence lives only in a git-ignored build directory; a clean checkout
  cannot rerun it.
- The red run predates the final forms of tests 4 and 5 (O7); only the assertions
  that existed at red-run time have a red record.
- The `Shutdown()` failed-wait `Error` path and the software device-creation
  `Unsupported` mapping are untested (A3); both require fault injection that this
  synthetic fixture does not provide.

## 8. Signature

Independent reviewer: Claude Code subagent (not the implementer).
