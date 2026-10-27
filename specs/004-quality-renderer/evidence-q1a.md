# Q1a evidence: D3D12 RHI clear/readback foundation

**Status**: Implemented and verified locally on 2026-10-27. Independent review is
recorded in [review-q1a.md](./review-q1a.md). This evidence covers **Q1a only**. It
does not complete Q1, User Story 2, the renderer programme or any later gate.

## Issue, branch and baseline

| Item | Value |
|---|---|
| Issue | #11, Q1a slice only (Q1b-Q7 are not started) |
| Branch | `feat/11-q1a-rhi-clear-readback` |
| Baseline commit | `0dd454e7b9acbb4ac30a471c15fc1ed8d68fb258` (= `origin/main` when the branch was cut) |
| Baseline status | clean worktree at the baseline commit; no tracked modification before the Q1a files were added |
| Submodule state | `doom3_orig/doom3` clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` |
| Worktree | dedicated issue worktree; final build in git-ignored `build-rhi-004-fresh/` |
| Writer / reviewer | implementation agent wrote the slice; a separate reviewer agent wrote `review-q1a.md` |

## Scope

The implemented slice is a standalone, synthetic, offscreen D3D12 fixture:

- a backend-neutral public seam in `src/renderer/rhi/rhi.h`;
- a D3D12 backend in `src/renderer/d3d12/rhi_d3d12.cpp`;
- a standalone CMake/CTest project in `tests/renderer/` with five named tests.

It uses no engine target copy, no game data, no proprietary SDK and no network
download. It does not integrate with either DOOM executable, does not create a
window or swap chain, and makes no performance claim.

## Environment

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.22631, x64 |
| Compiler | MSVC 14.44.35207 (Visual Studio 2022 17.14) |
| Windows SDK | 10.0.26100.0 (D3D12 and DXGI headers) |
| CMake / CTest | 3.31.6-msvc6, Visual Studio-bundled |
| Generator | Visual Studio 17 2022, x64 |
| Game data | none |
| NVIDIA SDK / DLSS | not installed, not required |

## Commands, exit codes and retained logs

Run from the repository root of the issue worktree. The build directory is
git-ignored and was deleted and recreated for this final run. These are the Q1a
commands from `quickstart.md`, executed with the Visual Studio-bundled CMake/CTest
and the Windows SDK selected by the generator; the fresh directory is
`build-rhi-004-fresh`, and verbose output plus JUnit retains every successful case.

| Step | Command | Exit | Retained log / artifact |
|---|---|---|---|
| Configure | `cmake -S tests/renderer -B build-rhi-004-fresh -G "Visual Studio 17 2022" -A x64` | 0 | `build-rhi-004-fresh/build-configure.log` |
| Build Debug | `cmake --build build-rhi-004-fresh --config Debug -- -m` | 0 | `build-rhi-004-fresh/build-debug.log` |
| Test Debug | `ctest --test-dir build-rhi-004-fresh -C Debug --no-tests=error -V --output-on-failure --output-junit q1a-junit-debug.xml` | 0 | `build-rhi-004-fresh/q1a-ctest-debug.log`, `build-rhi-004-fresh/q1a-junit-debug.xml` |
| Build Release | `cmake --build build-rhi-004-fresh --config Release -- -m` | 0 | `build-rhi-004-fresh/build-release.log` |
| Test Release | `ctest --test-dir build-rhi-004-fresh -C Release --no-tests=error -V --output-on-failure --output-junit q1a-junit-release.xml` | 0 | `build-rhi-004-fresh/q1a-ctest-release.log`, `build-rhi-004-fresh/q1a-junit-release.xml` |
| Repeat stability | `ctest --test-dir build-rhi-004-fresh -C Debug --no-tests=error`, ten times | 0 each | `build-rhi-004-fresh/repeat-1..10.log`, `repeat-summary.log` |

Debug result: `100% tests passed, 0 tests failed out of 5`, total 1.02 s.
Release result: `100% tests passed, 0 tests failed out of 5`, total 1.06 s.
Ten additional Debug runs had zero failures (`repeat-summary.log`). Both JUnit
files report `tests="5" failures="0" disabled="0" skipped="0"`. The Debug build
and Release build logs contain no `warning C####` or `error C####` line.

Per-test Debug times: 0.14, 0.12, 0.12, 0.23, 0.41 s. Per-test Release times:
0.16, 0.14, 0.14, 0.24, 0.37 s. Each CTest test has `SKIP_RETURN_CODE 77`,
`TIMEOUT 120` and `RHI_EVIDENCE_DIR` pointing inside its own build directory.

## Red evidence before implementation

A compiling stub produced a real behavioral failure, not a compile error. Retained
at `build-rhi-004/q1a-red.log`: the run reported
`0% tests passed, 5 tests failed out of 5` and the invoking shell observed CTest
exit 8 (the raw log does not embed the shell exit code; 8 is CTest's standard
test-failure code, and the mutation logs do record their exits). All five tests
failed:

```
1/5 Test #1: rhi.q1a.clear_readback ..............***Failed
2/5 Test #2: rhi.q1a.state_journal ...............***Failed
3/5 Test #3: rhi.q1a.negative_controls ...........***Failed
4/5 Test #4: rhi.q1a.fence_outstanding_release ...***Failed
5/5 Test #5: rhi.q1a.diagnostics .................***Failed
0% tests passed, 5 tests failed out of 5
```

For tests 1-3 the log shows, for both hardware and software,
`ADAPTER ... FAILED status=Error reason=Q1a backend stub: not implemented` and
`ASSERTION FAILED: adapter initialization failed for <class>`; each summary is
`pass=0 fail=2 skip=0`. Test 4 adds two
`ASSERTION FAILED: out-of-range adapter index did not return Unsupported` failures
from the incomplete-initialization check, with `INCOMPLETE_INIT ... status=Error
reason=Q1a backend stub: not implemented`.

Test 5 in that red log predates the final diagnostics rewrite: it exercised one
request combination (both flags) rather than the final four combinations, and
failed at `ASSERTION FAILED: diagnostics Initialize failed` with
`DIAGNOSTICS_SUMMARY pass=0 fail=2 skip=0`. The four-combination test was written
after the backend existed, so there is no red run for its final form; its
assertions are instead exercised against two adapter classes and the skipped/error
paths described below. The red record for T013/T016 is tests 1-4.

## Green evidence

### Adapters present

| Class | Name | Vendor | Device | Dedicated VRAM | LUID |
|---|---|---|---|---|---|
| hardware | Intel(R) Graphics | 0x8086 | 0x7d67 | 134217728 | 0x000000000001725f |
| software | Microsoft Basic Render Driver (WARP) | 0x1414 | 0x008c | 0 | 0x00000000000176b1 |

The fixture enumerates four hardware adapters and requests index 0; the software
class uses `IDXGIFactory4::EnumWarpAdapter`. Both classes actually executed the
clear/readback path (`ADAPTER hardware RESULT PASS`, `ADAPTER software RESULT
PASS`, `SUMMARY pass=2 fail=0 skip=0` in each affected test). A software pass is a
functional check, not hardware performance evidence.

### `rhi.q1a.clear_readback`

Two independent ungated submissions per adapter. The log contains, for each
adapter and each submission:

- `submit_status=Ok`; ids 1 and 2; completion serials 1 and 2; `gated=false`;
- `completed_before_wait=false` before `Wait` (logged, not asserted, because a
  conforming GPU may finish early);
- `row_pitch=256`, `tight=16`, `staging_bytes=1024` for a 4x4 RGBA8 target;
- four row comparisons against hard-coded expected bytes with tolerance 1 UNORM
  step: `mismatches=0 max_delta=0` on every row, on both adapters;
- `release_status=Released` for both submissions.

The log compares each row against hard-coded expected bytes and records hashes
of both sides: `06b23876a7a2a8c5` (`004080ff...`), `a5581568c728313d`
(`ffbf8040...`), `6c69b69da236ac65` (`2060a0e0...`) and `f0fbaa39868107a5`
(`1080f030...`). In this run every `actual_hash` equalled its `expected_hash`
and every row logged `max_delta=0`, but the test's pass condition is
`mismatches=0` at a tolerance of one UNORM step, not hash equality. The expected
bytes are literal constants in `tests/renderer/q1a_test_support.h`; they are
not derived from the readback.

### `rhi.q1a.state_journal`

The journal records exactly three ordered transitions per adapter:

```
render-target   Unknown       -> RENDER_TARGET  created as a D3D12 render target
render-target   RENDER_TARGET -> COPY_SOURCE    per-row clear recorded; transitioned for texture copy
readback-buffer Unknown       -> COPY_DEST      created as a D3D12 copy destination
```

The test asserts `journal.size() == 3` and each entry's resource, `from` and `to`
state in order, not merely the presence of a transition. The journal entries are
derived from the same `D3D12_RESOURCE_STATES` values passed to
`CreateCommittedResource` and to the transition barrier, not from a separate
hand-written description. Both adapters passed, and submit/wait/readback/release
statuses were logged as `Ok/Ok/Ok/Released`.

### `rhi.q1a.negative_controls`

The controls consume the raw row-pitched staging bytes, not the already-tight
pixels:

| Control | Applicable | Detected | Detail |
|---|---|---|---|
| `wrong_stride` (`width * 4` as staging stride) | yes | yes | mismatched 3/4 rows |
| `off_by_one_row` | yes | yes | mismatched 4/4 rows |
| `repeat_row_0` | yes | yes | mismatched 3/4 rows |

`NEGATIVE_CONTROL_SUMMARY applicable=3 all_detected=true` on both adapters. A
control that were inapplicable would be reported NOT APPLICABLE with its reason
and would not count as a pass; none were inapplicable on these adapters.

### `rhi.q1a.fence_outstanding_release`

The fixture submits a gated submission and a second submission queued behind it on
the same queue. Logged values on both adapters:

```
gated_serial=1 completed_before_release=0
release_gated_outstanding=RefusedOutstanding   reason=submission is gated; completion is outstanding
release_ungated_outstanding=RefusedOutstanding reason=completion serial is outstanding
wait_before_gate_status=InvalidArgument
release_gate_status=Ok                          reason=
wait_gated_status=Ok                            reason=
release_gated_complete=Released                 reason=
wait_ungated_status=Ok                          reason=
release_ungated_complete=Released               reason=
release_bogus=InvalidSubmission                 reason=unknown or already released submission
```

The second release is the required attempt at `GetCompletedValue() < signalValue`
(0 < 1 at the gate; the second submission is deterministically behind it). The
gated submission's readback was also compared with the same per-byte assertion
(`mismatches=0`, `max_delta=0`; tolerance one UNORM step). Successful
`ReleaseGate` and `Wait` calls assert `error.empty()`, so a stale error string
cannot survive a successful call.

The same test also covers two lifetime edges:

```
SHUTDOWN submit_status=Ok serial=1 gated=true reason=
SHUTDOWN status=Ok
SHUTDOWN second_status=Ok
INCOMPLETE_INIT class=hardware status=Unsupported reason=adapterIndex 9999 is out of range for hardware; found 4 adapter(s)
INCOMPLETE_INIT class=hardware first_shutdown=Ok second_shutdown=Ok
INCOMPLETE_INIT class=software status=Unsupported reason=adapterIndex 9999 is out of range for software; found 1 adapter(s)
INCOMPLETE_INIT class=software first_shutdown=Ok second_shutdown=Ok
```

`Shutdown()` with an unreleased gated submission opens the gate, waits for the
serial and returns `Ok`; `IsInitialized()` is false afterwards and a second
`Shutdown()` is also `Ok`. Failed initialization (out-of-range index) leaves the
device uninitialized and two consecutive `Shutdown()` calls clean. If a wait
during teardown fails, `Shutdown()` returns `Error`; that path is implemented but
was not triggered in this run.

### `rhi.q1a.diagnostics`

Each adapter class runs four opt-in combinations, each on a fresh device:

| Class | Combo | debug layer | GPU-based validation | Captured messages |
|---|---|---|---|---|
| hardware | `none` | NOT_REQUESTED | NOT_REQUESTED | 0 |
| hardware | `debug_only` | SKIPPED | NOT_REQUESTED | 0 |
| hardware | `gbv_only` | NOT_REQUESTED | SKIPPED | 0 |
| hardware | `both` | SKIPPED | SKIPPED | 0 |
| software | `none` | NOT_REQUESTED | NOT_REQUESTED | 0 |
| software | `debug_only` | SKIPPED | NOT_REQUESTED | 0 |
| software | `gbv_only` | NOT_REQUESTED | SKIPPED | 0 |
| software | `both` | SKIPPED | SKIPPED | 0 |

`DIAGNOSTICS_SUMMARY pass=8 fail=0 skip=0`. Observed details:

```
debug_detail=D3D12GetDebugInterface failed (hr=0x887A002D)
gpu_validation_detail=D3D12GetDebugInterface (GPU-based validation) failed (hr=0x887A002D)
```

`0x887A002D` is `DXGI_ERROR_SDK_COMPONENT_MISSING`: the debug layer is not
installed on this machine. Every requested case still executed a real
clear/readback and compared it (`work_status=Ok reason=`). The test enforces that
a requested diagnostic is `Run` or `Skipped`, never `NotRun`; that
`GBV=Run` implies `debug=Run`; that an unrequested diagnostic is
`NotRequested`; and that a GBV-only request leaves the debug layer
`NotRequested` or `Run`, never an unattempted skip.

**The debug layer and GPU-based validation are SKIPPED, not PASS.** They do not
count as validation evidence, and they block the full Q1 gate as defined in the
specification. The diagnostics struct also carries a bounded message-text
capture (up to eight `severity: description` strings) from `ID3D12InfoQueue`;
with the debug interface unavailable the device exposes no info queue, so the
observed counts are `captured=0 message_count=0` in all eight cases. No
message-count or message-content claim is made beyond those zeros.

## Screenshot evidence

Each successful submission wrote a 4x4 image scaled 16x into a bottom-up 24-bit
BMP (12,342 bytes each):

- `build-rhi-004-fresh/q1a-evidence/q1a-clear-hardware-0.bmp`
- `build-rhi-004-fresh/q1a-evidence/q1a-clear-hardware-1.bmp`
- `build-rhi-004-fresh/q1a-evidence/q1a-clear-software-0.bmp`
- `build-rhi-004-fresh/q1a-evidence/q1a-clear-software-1.bmp`

A header/pixel parser confirmed for all four: `BM` signature, file size 12,342,
DIB header 40, 64x64, 1 plane, 24 bpp, row stride 192, and top-to-bottom bands
`#004080`, `#FFBF80`, `#2060A0`, `#1080F0`. Each was converted to a 241-byte
PNG in the same directory (same base name, `.png`) and inspected visually: both
adapters show four distinct horizontal bands in that order. This is visual
confirmation of the per-row clear; the byte comparison above is the authoritative
check.

Playwright and browser tests are **not applicable** to Q1a: there is no web page,
DOM, browser build or WebGPU backend in this slice.

## Mutation evidence

Each mutation was applied to `src/renderer/d3d12/rhi_d3d12.cpp`, rebuilt and run
against the unchanged test, then restored and rebuilt. Logs are under
`build-rhi-004/mutations/`. Every log for the seven mutations in the table contains
`=== BUILD exit=0 ===`, `=== CTEST exit=8 ===` and `=== RESTORE build exit=0 ===`;
after the run the source again contains both `TryRelease` refusal strings and no
`Deliberate mutation` marker. The final fresh run above was executed afterwards,
serially, with no mutation script in flight.

| ID | Deliberate break | Test | Result |
|---|---|---|---|
| M1 | Readback packing uses `tightRowPitch` instead of the returned `rowPitch` | `rhi.q1a.clear_readback` | exit 8; row 0 passes, rows 1-3 `mismatches=16` each (`actual` all zero), both adapters FAIL |
| M2 | Every clear uses `desc.rows[0]` instead of `desc.rows[y]` | `rhi.q1a.clear_readback` | exit 8; row 0 passes, row 1 `mismatches=12`, rows 2-3 `mismatches=16`, both adapters FAIL |
| M3 | Remove the `RENDER_TARGET -> COPY_SOURCE` journal record | `rhi.q1a.state_journal` | exit 8; `journal_size=2`; "does not contain exactly three", "not ... in that order" and "lacks RENDER_TARGET -> COPY_SOURCE" all fail on both adapters |
| M3c | Duplicate the readback-buffer journal entry | `rhi.q1a.state_journal` | exit 8; `journal_size=4`; both the exact-count and exact-sequence assertions fail on both adapters |
| M3b | Transition barrier's `StateAfter` changed from `COPY_SOURCE` to `RENDER_TARGET` | `rhi.q1a.state_journal` | exit 8; `SubmitClearReadback failed`, `journal_size=0`; all five positive journal assertions fail on both adapters (the negative not-copied check is vacuous on an empty journal) |
| M4 | Remove both refusals from `TryRelease` | `rhi.q1a.fence_outstanding_release` | exit 8; both outstanding releases wrongly `Released`; 12 failed assertions per adapter; CTest reports `***Timeout 120.03 sec` |
| M5b | Wait-idle shortcut: `TryRelease` waits for the completion serial instead of refusing | `rhi.q1a.fence_outstanding_release` | exit 8 in 60.47 s; gated release still refused, `release_ungated_outstanding=Released` while queued behind the closed gate; 4 failed assertions per adapter |

An earlier attempt (`build-rhi-004/mutations/M5_wait_idle_before_release.log`)
passed in 60.12 s with CTest exit 0. Its driver was overwritten when M5b was
defined, so the exact M5a source change is not retained; the long wait is
consistent with waiting on the gate before applying the fence refusal, which is
not the shortcut the contract forbids, so a pass is the correct outcome for that
mutant. The retained log is kept only as the record of why it was rejected. The
final table uses the actual shortcut, M5b, which the test detects.

After restoration, the fresh Debug and Release runs above are green.

## Acceptance mapping

| Issue #11 criterion | Evidence |
|---|---|
| Standalone configure/build in a fresh worktree-local directory, no target copy, game data or proprietary SDK | Commands above; `build-rhi-004-fresh` created fresh; no `targets/` changes |
| CTest exits 0 with at least one adapter actually executing | 5/5 passed in Debug and Release; per-adapter `RESULT PASS`; adapter identities logged |
| `rhi.q1a.clear_readback` | Four exact row comparisons per adapter, row pitch and staging bytes, BMPs |
| `rhi.q1a.state_journal` | Exact three-entry count and ordered sequence on both adapters |
| `rhi.q1a.negative_controls` | 3/3 applicable and detected on both adapters |
| `rhi.q1a.fence_outstanding_release` | Serial 1 / completed 0; both releases refused; stale-error checks; shutdown with outstanding work; incomplete-init shutdown; M5b negative control |
| Debug layer and GBV opt-in, captured, RUN/SKIPPED/NOT RUN distinct, never passed when unavailable | Four combinations per class; requested diagnostics are SKIPPED with `0x887A002D`, unrequested ones NOT_REQUESTED; functional work still executed |
| This evidence file | This document |
| Clean submodule, no game data/binaries/build output tracked | `git -C doom3_orig/doom3 status --porcelain` empty; `git status --porcelain -uall` lists only source, test and evidence files |
| Independent review | [review-q1a.md](./review-q1a.md) |

## Changed files

- `src/renderer/rhi/rhi.h` (new)
- `src/renderer/d3d12/rhi_d3d12.cpp` (new)
- `tests/renderer/CMakeLists.txt` (new)
- `tests/renderer/q1a_test_support.h` (new)
- `tests/renderer/rhi_clear_test.cpp` (new)
- `tests/renderer/rhi_lifetime_test.cpp` (new)
- `tests/renderer/rhi_diagnostics_test.cpp` (new)
- `specs/004-quality-renderer/evidence-q1a.md` (new, this file)
- `specs/004-quality-renderer/review-q1a.md` (new, independent reviewer)

No engine source, submodule, target copy, root CMake file or game data is changed.

## Repository and legal hygiene

Verified on 2026-10-27 from the issue worktree:

- `git -C doom3_orig/doom3 status --porcelain` and
  `git diff --submodule=short -- doom3_orig/doom3` are both empty; the submodule is
  unmodified at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`.
- `git ls-tree -r HEAD` lists no `.pk4`, `.pk3`, `.pak`, `.exe`, `.dll`, `.lib`,
  `.obj` or `.pdb` path. The only tracked images are six `docs/media/*.jpg` files
  and `showcase/assets/hero-redacted.png` plus `showcase/preview.png`; all predate
  this branch, which adds no binary file.
- No tracked path matches `nvidia`, `ngx`, `nrd` or `dlss`.
- Tracked `targets/` holds only the `win11-amd64` scaffolding (`README.md`,
  `CMakeLists.txt`, `CMakePresets.json`, `sources.cmake`, one `.cpp`, a patch and
  a patch manifest); no `targets/*/doom3` source copy is tracked.
- A drive-letter path scan over the seven new source and test files returns no
  match; the fixture reads `RHI_EVIDENCE_DIR` from the environment instead of
  embedding a machine path.
- At evidence time `git status --porcelain -uall` listed exactly the nine new
  files above, and `origin/main` was
  `0dd454e7b9acbb4ac30a471c15fc1ed8d68fb258`.

## What Q1a does not prove

- It does not prove draw, compute, multiple render targets, readable depth,
  velocity/validity, overlays, presentation, resize, history or frame graphs.
- It does not prove engine integration, parity with the classic renderer, visual
  quality, frames per second, memory use or hardware performance.
- The debug layer and GPU-based validation did not run on this machine. Full Q1
  requires zero-unwaived-error runs on an environment where they actually execute.
- The backend-neutral header is a boundary, not evidence of a WebGPU, Vulkan or
  browser port. Browsers do not expose D3D12; a future web backend would need a
  separately designed and tested backend. Q1a proves only the D3D12 path on this
  machine.
- A software-adapter pass is a functional check only.

## Reproduce

```powershell
cmake -S tests/renderer -B build-rhi-004-fresh -G "Visual Studio 17 2022" -A x64
cmake --build build-rhi-004-fresh --config Debug -- -m
ctest --test-dir build-rhi-004-fresh -C Debug --no-tests=error -V --output-on-failure --output-junit q1a-junit-debug.xml
```

Choose a new, non-existing build directory if the tree is not fresh. The test
fixture itself sets `RHI_EVIDENCE_DIR`; a manual executable run writes BMPs to the
current directory unless that variable is set.
