# Q1b / Q1 independent review

Reviewer: an independent agent reviewer dispatched by the implementer. I did not
write this change and I am not a human reviewer. I reviewed the detached review
worktree `.kilo/worktrees/review-39` at `fc3ffc8`, created a fresh build
directory `build-review` inside that worktree, and reran the checks below. I did
not use `build-q1b`, the author's logs, or the author worktree as evidence.

This file replaces the author's self-review that was in this path. I read that
self-review; its claims are treated as untrusted input, and its overlapping
findings are not restated as mine.

## Scope and sources

Checked against:

- `specs/004-quality-renderer/tasks.md` T022–T031 and the Q1b status paragraph.
- `tests/renderer/q1b-fixtures.md` (the T022 fixture document).
- `specs/004-quality-renderer/evidence-q1.md` (author evidence).
- `specs/004-quality-renderer/contracts/rhi-foundation.md` (portable/native
  boundary, header gate, Q1 foundation growth).
- Issue #22 acceptance criteria and its declared out-of-scope edges.
- Issue #36 / T089, still open and owning the portable-header reconciliation.
- The PR #39 body and the 32 changed files.

Review constraints: no production-code or test edits, no commit, no push, no
access to the author worktree, and no reliance on author-retained results.

## Environment and fresh build

| Item | Observed |
|---|---|
| Worktree | `.kilo/worktrees/review-39`, detached at `fc3ffc8` |
| Configure generator | Visual Studio 17 2022, x64, VS-bundled CMake 3.31.6 |
| Compiler | MSVC 19.44.35228 / 19.44.35207 |
| Windows SDK | 10.0.26100 |
| Fresh build directory | `build-review` (git-ignored by `build-*/`) |
| Hardware adapter | `Intel(R) Graphics`, `0x8086:0x7d67`, VRAM 134217728 |
| Software adapter | `Microsoft Basic Render Driver`, `0x1414:0x008c`, VRAM 0 |
| Submodule | `doom3_orig/doom3` clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` |
| Worktree status | clean before this review file was written |

## Commands, exits and counts

All commands ran from the review worktree root.

| Command | Exit | Result |
|---|---|---|
| `cmake -S tests/renderer -B build-review -G "Visual Studio 17 2022" -A x64` | 0 | configured |
| `cmake --build build-review --config Debug` | 0 | 0 compiler warnings / errors |
| `ctest --test-dir build-review -C Debug --no-tests=error -V --output-on-failure` | 0 | **17/17 passed, 0 failed, 0 skipped**, 4.28 s |
| `cmake --build build-review --config Release` | 0 | 0 compiler warnings / errors |
| `ctest --test-dir build-review -C Release --no-tests=error -V --output-on-failure` | 0 | **17/17 passed, 0 failed, 0 skipped**, 4.28 s |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | header scan RUN over 3 headers and 11 public test files |
| `py -3 -I scripts/validate-q1b-fixtures.py` | 0 | PASS on pattern, tints, compute, MRT, depth and controls |
| `py -3 -I scripts/validate-q1b-fixtures.py --self-test` | 0 | altered pattern/compute values rejected |
| `git diff --check` | 0 | no whitespace errors |
| `py -3 -I scripts/validate-webgpu-mapping.py` (context only; not a Q1b gate) | 1 | 52/114 declarations mapped; 62 unmapped; see finding N8 |

Retained logs in the fresh build tree:
`build-review/_build-debug.log`, `build-review/_build-release.log`,
`build-review/_ctest-debug.log`, `build-review/_ctest-release.log`,
`build-review/_validator-rhi.log`, `build-review/_validator-q1b.log`,
`build-review/_validator-q1b-selftest.log`,
`build-review/_validator-webgpu.log`.

Test inventory: 7 Q1a cases plus 10 Q1b cases = 17. Every adapter-looped Q1b
test logged `SUMMARY pass=2 fail=0 skip=0`; the two device-free tests
(`rhi.q1b.temporal`, `rhi.q1b.frame_graph`) logged `result=PASS`.

## What the rerun actually exercised

- `rhi.q1b.draw`: compiled its own programs through the backend, created a
  graphics pipeline, submitted a draw, read back the literal 8x8 RGBA8 pattern
  at tint 1.0 and 0.5, and observed the wrong-tint, out-of-range-attachment and
  duplicate-write rejections.
- `rhi.q1b.compute`: created a compute pipeline, dispatched one 4x4 group,
  read back all sixteen literal 32-bit values (`162,178,130,...`), and observed
  the transposed-expectation, zero-group and graphics-pipeline-as-dispatch
  rejections.
- `rhi.q1b.mrt_depth`: wrote three distinct attachments, exercised a real
  failing `LESS` comparison at the cleared `x = 0` column, sampled the depth
  attachment as a point texture, and observed cross-attachment and
  depth-on-colour controls failing as expected.
- `rhi.q1b.frame_resources` plus its private gated companion: the public test
  verified per-frame tint isolation and reuse after completion; the private
  test held four frames on a gate, observed distinct slots `0,1,2,3`, refused a
  fifth with `NotReady`, refused an in-flight readback with `NotReady`, and
  reused the slots after release.
- `rhi.q1b.temporal` and `rhi.q1b.frame_graph`: portable motion/validity,
  history generation/reset, composition order, declared access order and
  invalid-use rejection; no device.
- `rhi.q1b.surface` plus its private companion: real window/swap-chain create,
  compose, resize `64x48 -> 96x64 -> 80x50`, destroy, device restart, minimize
  `NotReady`, restore, and `PrintWindow` captures. Both window captures were
  written on this host; their pixels are not asserted.
- `rhi.q1b.device_removal`: a private fault-injection hook, not a real device
  loss. It produced `DeviceLost`, refused release ownership, returned from
  teardown in 0 ms with a non-Ok status, and rendered after restart.
- `rhi.q1a.*`: all seven previous Q1a cases still passed on both adapters.

## Skipped, NOT RUN and not independently verified

- Debug layer: **SKIPPED**, `D3D12GetDebugInterface failed (hr=0x887A002D)`;
  the check did not execute.
- GPU-based validation: **SKIPPED** with the same HRESULT; the diagnostics
  summary reports `validation_executed=0`. A diagnostics case `PASS` is opt-in
  reporting, not a validation run.
- Real device removal: **NOT RUN**. The only removal evidence is the
  fault-injected private hook.
- Window-level capture pixels: **NOT ASSERTED**. Captures were written, but the
  asserted image is the composed backbuffer readback.
- Author red/green/mutation logs in `build-q1b/red/`: **NOT INDEPENDENTLY
  VERIFIED**. This review was read-only and did not mutate production code, so
  the mutation claims remain author-only evidence. The in-test negative
  controls were observed in this rerun and are recorded above.
- WebGPU mapping gate: **NOT RUN as a Q1b gate**, but run for context; it exits
  1 against this header (finding N8).
- Full Q1: **NOT PROVEN**.

## Findings

### Blocking for T031 / full Q1

**B1. T031 is not satisfied, and cannot be while T030's debug layer and
GPU-based validation are SKIPPED.**

Evidence: the fresh Debug log contains
`DIAGNOSTICS ... debug=SKIPPED gpu_validation=SKIPPED`, the diagnostic detail
`D3D12GetDebugInterface failed (hr=0x887A002D)`, and
`DIAGNOSTICS_SUMMARY ... validation_executed=0`; `evidence-q1.md:4-7,24-25`
records the same limitation; `tasks.md:72-73` requires actual execution with
zero unwaived errors for T031. This is a gate limitation, not a defect in the
Q1b code. The independent review requested by T031 was performed, but its
precondition was not met. T030 remains open.

### Blocking for issue #22 acceptance as written

**B2. The fixture validator does not check each worked expectation.**

`issue #22` acceptance criterion 3 requires a validator that "checks each worked
expectation against independent literal inputs" and fails when expected values
are deliberately altered. `scripts/validate-q1b-fixtures.py:109-181` parses only
the Fixture P pattern, tint table, Fixture C compute table, Fixture D MRT/depth
tables, and then checks `REQUIRED` substrings for Fixtures F/G/S
(`:52-64`). Fixture M's worked table and the Fixture F/G/S expectation values
are not parsed. I copied the tracked document to `build-review/_mut/` (no
tracked file was edited) and ran the validator with `--doc`:

| Altered value | Exit | Expected |
|---|---|---|
| Pattern byte `128 -> 127` | 1 | fail (caught) |
| Tint byte `64 -> 63` | 1 | fail (caught) |
| Depth byte `64 -> 65` | 1 | fail (caught) |
| MRT colour `10 -> 11` | 1 | fail (caught) |
| Fixture M motion `25 -> 26` | **0** | fail (not caught) |
| Fixture G order `0 then 1 -> 1 then 0` | **0** | fail (not caught) |
| Fixture F tints `1.0/0.5/0.25/0.75 -> 0.1/0.2/0.3/0.4` | **0** | fail (not caught) |
| Fixture S resize expectation `96x64 -> stay 64x48` | **0** | fail (not caught) |

The executable tests do cover Fixture M (`rhi_temporal_test.cpp:32-36`) and the
Fixture F/G/S behavior, so this is not a functional-code blocker. It is a
blocker for claiming issue #22 criterion 3 as written: fix the validator (or
narrow the criterion and say the executable tests own those rows).

### Non-blocking findings

**N1. Documentation overclaim: wrong CTest count.**
`quickstart.md:112` says "Expected: 16 CTest cases" and `:121` says "16/16
functional cases". The configured suite is 17 cases (`tests/renderer/CMakeLists.txt:64-81`);
both fresh Debug and Release runs reported 17/17. Correct the count or state
that 16 is a Q1b-only count with the Q1a cases excluded.

**N2. Documentation contradiction in Fixture D.**
`q1b-fixtures.md:105-106` says pass 0 writes `SV_Depth = 0.25` for `x < 4`, else
0.75. The embedded shader (`q1b_test_support.h:82-84`) writes `1.0` for
`x < 1`, so the `x = 0` fragment is discarded. The same document later says this
(`q1b-fixtures.md:131`) and the expected table depends on it. The prose and the
table do not agree; the test and table are correct.

**N3. Evidence identity is stale.**
`evidence-q1.md:9-11` says the worktree was branched from `origin/main` at
`d9bf1caa`. The reviewed commit `fc3ffc8` has parent `29a1e37`, and the PR body
says the branch is based on `29a1e37`. Record the actual base/rebase in the
evidence so a later reviewer can reproduce the tree.

**N4. One draw negative control does not test what it claims.**
`rhi_draw_test.cpp:127-145` submits a two-colour pass through a one-colour
pipeline using `colorIndices = {0, 0}`. `SubmitDrawFrame` rejects duplicate
writes first (`rhi_pipeline.cpp:470-483`) and never reaches the
pipeline/attachment-count check (`rhi_pipeline.cpp:339-345`). The observed log
is `colour attachment 0 is written by more than one pass`, so this control can
pass even if the intended pipeline/attachment mismatch check is missing. Use two
distinct in-range indices and a frame wide enough to reach the intended check.

**N5. Fixture F claims controls the tests do not implement.**
`q1b-fixtures.md:153-159` lists "Stale upload" (overwrite a slot's uniform
storage while in flight) and "Wait-idle shortcut". No test overwrites
`slot.mapped` while in flight, and a wait-idle-before-submission implementation
would block on the gated frames rather than accept a fifth submission. The
private gated test (`rhi_frame_resources_private_test.cpp:56-122`) does
implement the in-flight-reuse control and the distinct-slot/fifth-frame checks.
The public test (`rhi_frame_resources_test.cpp:78-105`) submits four frames and
only then waits, so "all four in flight" is not deterministic; the evidence
correctly says the private test is the bound. Remove or implement the two
unexercised controls.

**N6. Fixture M overstates its assertions.**
`q1b-fixtures.md:174-176` says history resize/reset makes "every sample"
invalid. `rhi_temporal_test.cpp:71-73,85-87` checks only `At(0,0)`. Either check
all samples or state that the test samples one pixel.

**N7. The public shader input is source text, and the public fixtures embed
native HLSL.**
`q1b_test_support.h:35-120` contains HLSL program text. The lexical gate
(`scripts/validate-rhi-contract.py:57-94,453-464`) scans the file but only
rejects the literal words `HLSL`/`DXC`, not HLSL syntax, so it reports zero hits.
The contract describes a core shader module as a backend blob
(`contracts/rhi-foundation.md:17,33`). The evidence acknowledges this
(`evidence-q1.md:227-230`). This is a known portability limitation owned by
Q7a/T089, not a Q1b/D3D12 defect.

**N8. The Q7a mapping gate is stale for the new public surface.**
For context I ran `py -3 -I scripts/validate-webgpu-mapping.py`; it exits 1 with
"52/114 declarations mapped" and 62 unmapped declarations, including
`ShaderModuleDesc.source`, `GraphicsPipelineDesc`, `ComputeDispatchDesc`,
`Surface`, `BufferResult` and the new device operations. Issue #36/T089 owns the
reconciliation and its five known declarations; that scope is now too small for
this header. No Q7/WebGPU claim is made here, and this does not block Q1b on
D3D12, but the PR must be rebased and re-mapped after T089.

**N9. Issue #22 scope and traceability conflict.**
Issue #22 says "Out of scope and blocking edges: Implementation of T023–T031"
and "This issue does not close Q1b or full Q1". This PR implements T023–T029
and part of T030 under `Refs #22`, and `tasks.md:64-72` marks those tasks `[x]`.
The code slice may be authorized by the dispatch, but the issue text and task
status should record that authorization rather than implying issue #22 covered
the implementation.

**N10. Low: an error after submission can leave an untracked record.**
If `Signal` fails in `SubmitDrawFrame`/`SubmitCompute`, or `Present` fails in
`SubmitSurfaceFrame`, the record has already been inserted and the slot filled,
but the function returns an error without a submission token
(`rhi_pipeline.cpp:590-629`, `rhi_compute.cpp:240-263`,
`rhi_present.cpp:314-343`). The slot is deliberately not reused, which is safe,
but the host record can only be reclaimed at teardown. This is a bounded
error-path resource leak, not a memory-safety bug.

## Answers to the specific review questions

- **Do the tests exercise the claimed behaviour?** For draw, compute, MRT/depth,
  frame slots (private), motion, frame graph, surface and fault-injected
  removal, yes, on this host. The weak points are N4–N6.
- **Are expected values independent?** The C++ draw/compute/MRT/depth/temporal
  expectations are literals and match independent recomputation. The validator
  expands the 8x8 pattern from the published coordinate rule rather than
  storing separate literals (`scripts/validate-q1b-fixtures.py:22-24`), and its
  coverage gap is B2.
- **Can a test pass while the feature is absent, SKIPPED or Unsupported?**
  Draw/compute/MRT/frame-slot/private tests fail if submission is Unsupported.
  A missing adapter class is SKIPPED; `CreateSurface` returning
  `Unsupported` returns exit 77 and CTest reports the case as Skipped, not
  Passed. `rhi.q1a.diagnostics` does pass while validation is SKIPPED, but its
  output and the evidence explicitly say `validation_executed=0`; it is a
  reporting test, not a validation run. Device removal is fault-injected, not a
  real loss. A surface test that skips one adapter can still exit 0 when the
  other adapter passes, so per-test status must be read.
- **Do public tests contain native vocabulary?** The lexical gate reports zero
  prohibited nouns over 11 public test files, including
  `q1b_test_support.h`. The embedded HLSL syntax is not detected by that gate
  (N7).
- **Does the evidence match what I observed?** Yes for 17/17 in Debug and
  Release, the adapter identities, the validator outputs, the BMPs, the
  diagnostics SKIP and the fault-injected removal result. It does not match on
  the base commit (N3) and the quickstart count (N1); the author mutation logs
  were not independently verified.

## T031 verdict

**T031 is NOT satisfied.** This independent review was performed, but T031's
precondition is a Q1 run in which the debug layer and GPU-based validation
actually execute with zero unwaived errors. Both are SKIPPED with
`0x887A002D` and `validation_executed=0` on this host. T030 therefore remains
open, and full Q1 remains BLOCKED under `tasks.md:73`.

## Overall verdict

The **Q1b functional slice (T023–T029 plus the fault-injected device-removal part
of T030) is acceptable for review and, after the documentation corrections, for
merge as a partial slice.** The fresh 17/17 Debug and Release runs on both the
Intel hardware adapter and WARP exercise the claimed draw, compute, MRT/depth,
frame-slot, motion, frame-graph, surface and injected-removal behaviour. I found
no functional-code blocker in this slice.

**Full Q1 is not proven.** It must not be reported as a Q1 pass, a playable
renderer, a WebGPU result or a Q2 prerequisite satisfied.

Before issue #22 can be accepted as written, B2 must be fixed or the criterion
narrowed. Before full Q1/T031, the remaining work is:

1. **T030:** install/prepare an environment where the D3D12 debug layer and
   GPU-based validation actually execute (`D3D12GetDebugInterface` succeeds),
   rerun all fixtures, and record unwaived messages, actual versus skipped
   checks and adapter class. Exercise device-removal diagnostics where
   available; the current removal evidence is fault-injected only.
2. **T031:** obtain an independent rerun on that environment and record the Q1
   decision; full Q1 stays blocked until debug-layer and GBV execution is
   zero-unwaived-error.
3. **T089:** reconcile `src/renderer/rhi/rhi.h` and the contract with the WebGPU
   mapping gate, then rerun `validate-webgpu-mapping.py` and the browser spike.
   The Q1b additions currently leave 62 of 114 declarations unmapped; Q7/WebGPU
   claims remain blocked.
4. **Documentation/validator:** correct the 16-vs-17 count, the Fixture D
   `SV_Depth` wording, the stale evidence base, and either widen the fixture
   validator (B2) or record that the executable tests own Fixtures F/G/M/S.
