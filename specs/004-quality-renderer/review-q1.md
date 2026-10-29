# Q1b / Q1 independent review

**Review status: independent review performed on `35ce54e`; Q1 remains blocked
by T030; the review does not upgrade the SKIPPED checks to PASS.**

Reviewer: an independent agent reviewer dispatched by the implementer. I did not
write this change and I am not a human reviewer. I reviewed the detached review
worktree `.kilo/worktrees/review-39` at `35ce54e`, created a **new** build
directory `build-review2` inside that worktree (I did not reuse `build-review`),
and reran the checks below. I did not use `build-q1b`, the author's logs, or the
author worktree as evidence.

This review supersedes the earlier `fc3ffc8` review in this path. The author
self-review is retained as `review-q1-self.md`; I read it but did not adopt its
claims. I changed only this review file: no production code or test was edited,
and nothing was committed or pushed.

## Scope and sources

Checked against:

- `specs/004-quality-renderer/tasks.md` T022–T031 and the Q1b status paragraph.
- `tests/renderer/q1b-fixtures.md` (the T022 fixture document).
- `specs/004-quality-renderer/evidence-q1.md` and
  `specs/004-quality-renderer/review-q1-self.md`.
- `specs/004-quality-renderer/contracts/rhi-foundation.md` (portable/native
  boundary, header gate, Q1 foundation growth).
- Issue #22 acceptance criteria and its declared out-of-scope edges.
- Issue #36 / T089, still open and owning the portable-header reconciliation.
- The PR #39 body and the complete diff from `29a1e37` to `35ce54e`.

## Environment and fresh build

| Item | Observed |
|---|---|
| Worktree | `.kilo/worktrees/review-39`, detached at `35ce54e` |
| Configure generator | Visual Studio 17 2022, x64, VS-bundled CMake 3.31.6-msvc6 |
| Compiler | MSVC 19.44.35228 / 19.44.35207 |
| Windows SDK | 10.0.26100 |
| Fresh build directory | `build-review2` (git-ignored by `build-*/`; not `build-review`) |
| Hardware adapter | `Intel(R) Graphics`, `0x8086:0x7d67`, VRAM 134217728 |
| Software adapter | `Microsoft Basic Render Driver`, `0x1414:0x008c`, VRAM 0 |
| Submodule | `doom3_orig/doom3` clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` |
| Worktree status | clean before this review file was updated |

## Commands, exits and counts

All commands ran from the review worktree root.

| Command | Exit | Result |
|---|---|---|
| `cmake -S tests/renderer -B build-review2 -G "Visual Studio 17 2022" -A x64` | 0 | configured |
| `cmake --build build-review2 --config Debug` | 0 | build succeeded; 0 compiler warnings / errors |
| `ctest --test-dir build-review2 -C Debug --no-tests=error -V --output-on-failure` | 0 | **17/17 passed, 0 failed, 0 skipped**, 4.21 s |
| `cmake --build build-review2 --config Release` | 0 | build succeeded; 0 compiler warnings / errors |
| `ctest --test-dir build-review2 -C Release --no-tests=error -V --output-on-failure` | 0 | **17/17 passed, 0 failed, 0 skipped**, 4.10 s |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | header scan RUN over 3 headers and 11 public test files |
| `py -3 -I scripts/validate-q1b-fixtures.py` | 0 | PASS on pattern, tints, compute, MRT, depth, M/G/S tables, F tint sentence and controls |
| `py -3 -I scripts/validate-q1b-fixtures.py --self-test` | 0 | altered pattern, compute, motion, graph, surface and tint values all rejected |
| `git diff --check` | 0 | no whitespace errors |
| `py -3 -I scripts/validate-webgpu-mapping.py` (context only; not a Q1b gate) | 1 | 52/114 declarations mapped; 62 unmapped; see R4 |

Retained logs in the fresh build tree:
`build-review2/_build-debug.log`, `build-review2/_build-release.log`,
`build-review2/_ctest-debug.log`, `build-review2/_ctest-release.log`,
`build-review2/_validator-rhi.log`, `build-review2/_validator-q1b.log`,
`build-review2/_validator-q1b-selftest.log`.

Test inventory: 7 Q1a cases plus 10 Q1b cases = 17. Every adapter-looped Q1b
test logged `SUMMARY pass=2 fail=0 skip=0`; the two device-free tests
(`rhi.q1b.temporal`, `rhi.q1b.frame_graph`) logged `result=PASS`.

## B2 closure: fixture validator mutation check

I copied the tracked fixture document to the untracked directory
`build-review2/_mut/` (no tracked file was edited) and ran the validator with
`--doc` one mutation at a time.

| Input | Exit | Required |
|---|---|---|
| Unaltered copy | 0 | pass |
| Fixture M motion `x = 25` -> `x = 26` | 1 | fail |
| Fixture G order `Ok, order 0 then 1` -> `Ok, order 1 then 0` | 1 | fail |
| Fixture S resize `Readback dimensions become 96` -> `Readback dimensions stay 64` | 1 | fail |
| Fixture F tints `1.0, 0.5, 0.25 and 0.75` -> `0.1, 0.2, 0.3 and 0.4` | 1 | fail |

**B2 is closed.** The validator now parses the P/C/D tables, the M/G/S worked
tables and the Fixture F tint sentence; its self-test mutates each family and
requires rejection. This matches the PR body's claim.

## Spot-check of the N1–N6/N9/N10 fixes

| Finding | Final tree evidence | Result |
|---|---|---|
| N1 count | `quickstart.md:112` says 17 cases; `:121` says 17/17 | fixed |
| N2 Fixture D prose | `q1b-fixtures.md:105-106` says `SV_Depth = 1.0` for `x = 0`, `0.25` for `1 ≤ x < 4`, else `0.75`; matches the shader and table | fixed |
| N3 evidence base | `evidence-q1.md:11-12` says rebased onto `29a1e37` | fixed |
| N4 draw control | `rhi_draw_test.cpp:127` uses `{0, 1}`; the fresh log says `pass 0 declares 2 colour(s) but pipeline 1 writes 1` | fixed (the intended mismatch path now fires) |
| N5 Fixture F controls | the `Wait-idle shortcut` and `Stale upload` rows are gone; only the implemented `In-flight reuse` control remains | fixed |
| N6 every sample | `rhi_temporal_test.cpp:71-78,93-101` checks every `8x8` sample after resize and reset | fixed |
| N9 issue #22 scope | `evidence-q1.md:249-251` records that #22 lists T023–T031 as out of scope and that the S5 dispatch authorized the slice | fixed |
| N10 error-path record | `evidence-q1.md:252-255` records the bounded `Signal`/display-failure record retention | fixed as a documented limitation |

## What the rerun exercised

- `rhi.q1b.draw`: compiled its own programs, created a graphics pipeline,
  submitted a draw, read back the literal 8x8 RGBA8 pattern at tint 1.0 and 0.5,
  and observed the wrong-tint, out-of-range-attachment and
  pipeline/attachment-mismatch rejections.
- `rhi.q1b.compute`: dispatched one 4x4 group, read back all sixteen literal
  32-bit values, and observed the transposed-expectation, zero-group and
  graphics-pipeline-as-dispatch rejections.
- `rhi.q1b.mrt_depth`: wrote three distinct attachments, exercised a real failing
  `LESS` comparison at the cleared `x = 0` column, sampled the depth attachment,
  and observed cross-attachment and depth-on-colour controls failing.
- `rhi.q1b.frame_resources` plus the private gated companion: per-frame tint
  isolation and reuse after completion; four distinct slots held on a gate, fifth
  refused with `NotReady`, in-flight readback `NotReady`, slots reusable after
  release.
- `rhi.q1b.temporal` and `rhi.q1b.frame_graph`: portable motion/validity, full
  history generation/reset invalidation, composition order, declared order and
  invalid-use rejection; no device.
- `rhi.q1b.surface` plus private companion: window/swap-chain create, compose,
  repeated resize, destroy, device restart, minimize `NotReady`, restore and
  `PrintWindow` captures; both window captures were written, but their pixels
  are not asserted.
- `rhi.q1b.device_removal`: a private fault-injection hook, not a real device
  loss. It produced `DeviceLost`, refused release ownership, returned from
  teardown in 0 ms with a non-Ok status, and rendered after restart.
- `rhi.q1a.*`: all seven previous Q1a cases still passed on both adapters.

## Skipped, NOT RUN and not independently verified

- Debug layer: **SKIPPED**, `D3D12GetDebugInterface failed (hr=0x887A002D)`.
- GPU-based validation: **SKIPPED** with the same HRESULT; the diagnostics
  summary reports `validation_executed=0`. A diagnostics case `PASS` is opt-in
  reporting, not a validation run.
- Real device removal: **NOT RUN**. The only removal evidence is the
  fault-injected private hook.
- Window-level capture pixels: **NOT ASSERTED**. Captures were written, but the
  asserted image is the composed backbuffer readback.
- Author red/green/mutation logs in `build-q1b/red/`: **NOT INDEPENDENTLY
  VERIFIED**. This review is read-only and did not mutate production code. The
  in-test negative controls were observed in this rerun.
- Q7a mapping: `validate-webgpu-mapping.py` was run for context and exits 1
  (52/114 mapped, 62 unmapped). No WebGPU/Q7 claim is made.
- Full Q1: **NOT PROVEN**.

## Remaining findings

### Non-blocking

**R1. Stale evidence limitation contradicts the new review.**
`evidence-q1.md:229-230` still says "The independent review is not done.
`review-q1.md` records the author's self-review and explicitly does not count as
T031." That is no longer true: `review-q1.md` is the independent review,
`review-q1-self.md` is the author self-review, and the same evidence file says
the independent review was performed (`:6-8`, `:221`). This is a documentation
defect; a reader could wrongly conclude T031's review is absent. Fix the
limitation text.

**R2. The retained author self-review has a stale base.**
`review-q1-self.md:5` says `origin/main` at `d9bf1caa`; the final branch is
rebased onto `29a1e37` (`evidence-q1.md:11-12`, `35ce54e`). Label the self-review
as historical or update its base line. Low, documentation only.

**R3. The public shader input remains backend source text.**
`q1b_test_support.h` embeds HLSL program text. The lexical gate
(`scripts/validate-rhi-contract.py:57-94,453-464`) rejects the literal words
`HLSL`/`DXC` but not HLSL syntax, so it reports zero hits. The contract describes
a core shader module as a backend blob (`contracts/rhi-foundation.md:17,33`).
This is a known Q7a/T089 portability limitation, disclosed in
`evidence-q1.md:227-230`; it is not a Q1b/D3D12 defect and no portability claim
is made.

**R4. The Q7a mapping gate is stale for the new public surface.**
`validate-webgpu-mapping.py` currently exits 1 with 52/114 declarations mapped
and 62 unmapped, including the new Q1b shader/pipeline/surface/compute types.
Issue #36/T089 owns the `rhi.h` reconciliation, but its five-declaration scope is
now too small for this header. The branch must be rebased and re-mapped after
T089; Q7/WebGPU claims remain blocked. This is a limitation, not a Q1b blocker.

**R5. Author mutation evidence is not independently verified.**
`evidence-q1.md:146-164` and `tasks.md` Q1b status cite red/green/mutation logs
in the author's `build-q1b/red/` directory. They are author-retained; this
read-only review did not reproduce the production mutations. The clean 17/17
rerun and the in-test negative controls are the independent evidence.

**R6. Low: public frame-slot log wording overstates in-flight isolation.**
`rhi_frame_resources_test.cpp:102` logs "after all four were in flight", but the
public test submits four frames and only then waits; the frames may already have
completed. The private gated test is the deterministic in-flight bound, and the
quickstart/evidence say so. The log text should say "after four submissions"
rather than claiming a timing condition.

No blocking code or test defect remains in the Q1b slice. The only
blocking condition is the T030/T031 gate for full Q1: the debug layer and
GPU-based validation are SKIPPED (`0x887A002D`) and real device removal is NOT
RUN.

## Specific review questions

- **Do the tests exercise the claimed behaviour?** Yes for draw, compute,
  MRT/depth, frame slots (private), motion, frame graph, surface and
  fault-injected removal on this host. The remaining weak point is the public
  frame-slot log wording (R6), not the private bound.
- **Are expected values independent?** The C++ draw/compute/MRT/depth/temporal
  expectations are literals and match independent recomputation. The validator's
  P/C/D/M/G/S/F expectations are now checked, and the B2 mutations fail.
- **Can a test pass while the feature is absent, SKIPPED or Unsupported?**
  Draw/compute/MRT/frame-slot/private tests fail if submission is Unsupported.
  A missing adapter class is SKIPPED; `CreateSurface` returning `Unsupported`
  returns exit 77 and CTest reports the case as Skipped, not Passed.
  `rhi.q1a.diagnostics` passes while validation is SKIPPED, but its output and
  the evidence say `validation_executed=0`; it is a reporting test. Device
  removal is fault-injected, not a real loss. A surface test that skips one
  adapter can still exit 0 when the other adapter passes, so per-test status
  must be read.
- **Do public tests contain native vocabulary?** The lexical gate reports zero
  prohibited nouns over 11 public test files, including `q1b_test_support.h`.
  HLSL syntax is not detected by that gate (R3).
- **Does the evidence match what I observed?** Yes for 17/17 in Debug and
  Release, the adapter identities, the validator outputs, the BMPs, the
  diagnostics SKIP and the fault-injected removal result. It does not match on
  the stale limitation text (R1) or the retained self-review base (R2); the
  author mutation logs were not independently verified (R5).

## T031 verdict

**T031 is NOT satisfied.** The independent T031 review action for `35ce54e` is
complete, but T031's gate requires a Q1 run in which the debug layer and GPU-based
validation actually execute with zero unwaived errors. Both are SKIPPED with
`0x887A002D`, `validation_executed=0`, and real device removal is **NOT RUN**.
Therefore:

- Full Q1 is **NOT proven**.
- No Q1 capability is claimed.
- T030 remains open.
- This is **not a Q1 pass**, and the SKIPPED checks are not upgraded to PASS.

## Overall verdict

The **Q1b functional slice (T022–T029 plus the fault-injected device-removal part
of T030) is acceptable for merge as a partial slice.** The fresh 17/17 Debug and
Release runs on both the Intel hardware adapter and WARP exercise the claimed
draw, compute, MRT/depth, frame-slot, motion, frame-graph, surface and
injected-removal behaviour, and the B2 validator gap is closed. I found no
functional-code blocker in this slice. R1 and R2 are documentation corrections;
R3–R5 are known limitations or unverified author evidence.

Full Q1 remains blocked and must not be reported as a Q1 pass, a playable
renderer, a WebGPU result or a satisfied Q2 prerequisite.

Remaining work before full Q1/T031:

1. **T030:** prepare an environment where the D3D12 debug layer and GPU-based
   validation actually execute (`D3D12GetDebugInterface` succeeds), rerun all
   fixtures, and record unwaived messages, actual versus skipped checks and
   adapter class. Exercise real device-removal diagnostics where available; the
   current removal evidence is fault-injected only.
2. **T031:** obtain a rerun on that environment and record the Q1 decision; full
   Q1 stays blocked until debug-layer and GBV execution is zero-unwaived-error.
3. **T089 / #36:** reconcile `rhi.h` and the contract with the WebGPU mapping
   gate, then rerun `validate-webgpu-mapping.py` and the browser spike. The Q1b
   additions currently leave 62 of 114 declarations unmapped.
4. **Documentation:** fix the stale evidence limitation R1 and the retained
   self-review base R2; keep the Q7a mapping and HLSL/source-text limitations
   explicit.
