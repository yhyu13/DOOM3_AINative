# Q3 test slice evidence (T043/T045/T047 red; T042/T044/T046 re-verified)

Status: **test slice only**. This record does not claim Q3, does not close T048 and does not
satisfy T043/T045/T047. The three new fixtures are RED by design: their production slices
(`src/renderer/rhi/view_history.cpp`, `src/renderer/rhi/overlay_composition.cpp`) are outside this
branch's write ownership and do not exist on `main`. The tests are registered as expected-red so the
suite stays usable for other workers; the CTest verdict is inverted by `WILL_FAIL` and is **not** a
pass. Read the raw executable exits below.

Worktree: `.kilo/worktrees/test-q3-motion-hud-remaining-406ccb3b01f09740`, branch
`test/q3-motion-hud-remaining`, rebased onto `main` at `586fe49` (PR #54; earlier bases `4a2fd7c`
and `150a670`, the branch was first built on `150a670`, which contains PR #50, `f703fae`). Every
rebase was conflict-free and changed no file in this slice.
One writer, one worktree, one build directory (`build-q3-tests`, git-ignored).

## Identity and scope

- **Stage and claim**: Q3 (US4) test slice. T042/T044/T046 are re-verified from merged PR #50;
  T043/T045/T047 gain failing public-seam tests only. No Q3 acceptance, no real scene, no
  engine-fork change.
- **Source identity**: branch `test/q3-motion-hud-remaining` rebased onto `main` at `586fe49`, plus
  the committed slice described here; submodule `doom3_orig/doom3` at `a9c49da` with no local
  modification (`git submodule status` has no `+`/`-` prefix, `git status --porcelain` empty).
- **Data identity and rights**: synthetic fixtures only (`tests/renderer/motion-fixtures.md` and the
  literals in the test units). No game data, no engine source, no proprietary SDK.
- **Scene and inputs**: analytic camera at the engine 16:9 pair (`tanX=4/3`, `tanY=3/4`), point at
  depth 100 with `z ∈ {0, 0.25, 0.5, 0.75, 1.0}`, render extent 640×360 and display extent
  1280×720. No rasterization, no device.
- **Exclusions and limitations**: the display-resolution HUD stage is not wired to a real renderer;
  the tick-versus-render call site is T085's job, not this seam's; no GPU, no adapter, no window.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 amd64; MSVC 19.44.35207 (Visual Studio 2022
  Professional 17.14); VS-bundled CMake 3.31.6, generator `Visual Studio 17 2022`, `-A x64`;
  C++17; `Debug` and `Release`; `/W4 /permissive- /EHsc`.
- **Adapter and driver**: N/A — every check in this slice is CPU-only and links no native library.
- **Capability and fallback**: none requested. The debug layer and GPU-based validation are not
  involved and are not claimed.
- **Shader provenance and build**: N/A — no shader is compiled or consumed.

## Procedure and retained artifacts

- **Predeclared tolerances**: analytic `du`/`dv` tolerance `max(1 RG16F ULP, 1e-7)` per component
  from `motion-fixtures.md` §5 (`1.00e-07`, `1.91e-06`, `3.81e-06` as used per row). Composition
  texel mapping is exact integer floor division; no pixel tolerance.
- **Commands and exits**: below, run from the worktree root. `cmake`/`ctest` are invoked from the
  VS-bundled directory because they are not on `PATH` in this shell.
- **Retained logs and artifacts**: worktree-local, git-ignored `build-q3-tests/` and
  `build-q3-clean/`: `configure.log`, `configure-2.log`, `build-Debug.log`, `build-Release.log`,
  `ctest-Debug.log`, `ctest-Release.log`, `ctest-Debug-junit.xml`, `ctest-Release-junit.xml`,
  `ctest-q3-baseline.log`, `raw-<test>.log`, `raw-<test>.log.err`, `raw-Debug-<test>.log`, plus the
  rebase re-runs `*postrebase*.log` and `*586fe49*.log`. Machine-specific paths stay in these logs
  and are not tracked.
- **Negative controls**: the red fixtures are the control for an absent production slice. The
  merged T042/T044/T046 mutation controls were executed in PR #50 and are **not re-run here**
  because `src/renderer/rhi/**` is outside this session's write ownership; the record is the PR #50
  body, not a fresh run.

### Commands

```text
cmake -S tests/renderer -B build-q3-tests -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-q3-tests --config Release -- -m                           -> 0
cmake --build build-q3-tests --config Debug -- -m                             -> 0
ctest --test-dir build-q3-tests -C Release --no-tests=error -V
      --output-on-failure --output-junit build-q3-tests/ctest-Release-junit.xml -> 0, 23/23 (3 inverted)
ctest --test-dir build-q3-tests -C Debug   --no-tests=error -V
      --output-on-failure --output-junit build-q3-tests/ctest-Debug-junit.xml   -> 0, 23/23 (3 inverted)
py -3 -I scripts/validate-rhi-contract.py                                     -> 0
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --check tests/renderer/motion-fixtures.md -> 0
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --self-test      -> 0
git diff --check                                                              -> 0
```

Baseline before this slice, on the same build tree: `ctest -R "^rhi\.q3\."` → 0, **3/3**
(`motion_reprojection`, `skinned_history`, `motion_validity`).

### Clean-tree re-run

The increment above was reproduced from a fresh configure in a second, empty directory so the
result does not depend on incremental build state:

```text
cmake -S tests/renderer -B build-q3-clean -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-q3-clean --config Release -- -m                           -> 0 (0 error/warning lines)
ctest --test-dir build-q3-clean -C Release --no-tests=error -V
      --output-on-failure --output-junit build-q3-clean/ctest-Release-junit.xml -> 0, 23/23 (3 inverted)
```

Raw exits from that tree: `q3_view_history_test` 1 (35 failures), `q3_history_reset_test` 1
(52 failures), `q3_overlay_composition_test` 1 (25 failures), `q3_motion_reprojection_test` 0
(109/0), `q3_skinned_history_test` 0 (43/0), `q3_motion_validity_test` 0 (77/0). Logs:
`build-q3-clean/{configure,build-Release,ctest-Release}.log`, `ctest-Release-junit.xml`,
`raw-<test>.log`.

### Post-rebase re-run (base `4a2fd7c`)

The branch was rebased onto current `origin/main` (`4a2fd7c`) because an independent review blocked
PR #55 on the merge base only (`150a670`); no content change was requested and none was made outside
this record. The rebase was conflict-free. The same build tree was rebuilt and re-run:

```text
cmake --build build-q3-clean --config Release -- -m                     -> 0
ctest --test-dir build-q3-clean -C Release --no-tests=error
      --output-on-failure                                              -> 0, 23/23 (3 inverted)
```

Raw exits after the rebase are unchanged: `q3_view_history_test` 1 (46/35),
`q3_history_reset_test` 1 (55/52), `q3_overlay_composition_test` 1 (33/25),
`q3_motion_reprojection_test` 0 (109/0), `q3_skinned_history_test` 0 (43/0),
`q3_motion_validity_test` 0 (77/0). Logs: `build-q3-clean/build-Release-postrebase.log`,
`ctest-Release-postrebase.log`, `postrebase-<test>.log`.

### Post-rebase re-run (base `586fe49`, PR #54)

`origin/main` advanced to `586fe49` (PR #54) after the review at `8879809`, which voided that
approval under AGENTS.md §11. The branch was rebased again, conflict-free, with no file change in
this slice; the same build tree was rebuilt and re-run:

```text
cmake --build build-q3-clean --config Release -- -m                     -> 0
ctest --test-dir build-q3-clean -C Release --no-tests=error
      --output-on-failure                                              -> 0, 23/23 (3 inverted)
py -3 -I scripts/validate-rhi-contract.py                              -> 0 (7 headers, 22 public test files)
```

Raw exits are unchanged: `q3_view_history_test` 1 (46/35), `q3_history_reset_test` 1 (55/52),
`q3_overlay_composition_test` 1 (33/25), `q3_motion_reprojection_test` 0 (109/0),
`q3_skinned_history_test` 0 (43/0), `q3_motion_validity_test` 0 (77/0). Logs:
`build-q3-clean/build-Release-586fe49.log`, `ctest-Release-586fe49.log`, `586fe49-<test>.log`.

The contract gate reports `header scan: RUN over 7 header(s) and 22 public test file(s)`; the two
proposed seam headers, the two stub units and the three new test units are inside that scan and add
no private vocabulary. The fixture generator reports `PASS: 20 fixture rows and 10 validity rows match the
generator` and `PASS: 5 numeric mutations and the zero-valid contract are detected`.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| `rhi.q3.motion_reprojection` (T042) | RUN | PASS | `SUMMARY q3-motion-reprojection checks=109 failures=0`, exit 0, Debug and Release | `raw-q3_motion_reprojection_test.log`, `raw-Debug-q3_motion_reprojection_test.log` |
| `rhi.q3.skinned_history` (T044) | RUN | PASS | `checks=43 failures=0`, exit 0, Debug and Release | `raw-q3_skinned_history_test.log` |
| `rhi.q3.motion_validity` (T046) | RUN | PASS | `checks=77 failures=0`, exit 0, Debug and Release | `raw-q3_motion_validity_test.log` |
| `rhi.q3.red.view_history` (T043) | RUN | **FAIL** | `checks=46 failures=35`, exit 1; store absent | `raw-q3_view_history_test.log` |
| `rhi.q3.red.history_reset` (T045) | RUN | **FAIL** | `checks=55 failures=52`, exit 1; store absent | `raw-q3_history_reset_test.log` |
| `rhi.q3.red.overlay_composition` (T047) | RUN | **FAIL** | `checks=33 failures=25`, exit 1; post-reconstruction stage absent | `raw-q3_overlay_composition_test.log` |
| CTest Release suite | RUN | PASS (inverted) | 23/23 with `WILL_FAIL` on the three red tests; this is not a green result for T043/T045/T047 | `ctest-Release.log`, `ctest-Release-junit.xml` |
| CTest Debug suite | RUN | PASS (inverted) | 23/23, same inversion | `ctest-Debug.log`, `ctest-Debug-junit.xml` |
| Contract header/test gate | RUN | PASS | 7 headers, 22 public test files, zero prohibited nouns | console output |
| Fixture document check | RUN | PASS | 20 fixture rows + 10 validity rows match the generator | console output |
| Real scene / per-pixel motion | NOT RUN | — | T048 and T085; needs the Q2 scene adapter | — |
| Independent review | NOT RUN | — | no reviewer assigned to this slice | — |

### What the red runs actually prove

Each red executable mixes two kinds of check:

1. **Fixture-baseline checks that run merged production code and pass.** In `view_history_test`,
   the SEQ-01/02/03 reprojections are computed directly through `rhi::ReprojectPoint`
   (`motion_history.cpp`). In `overlay_composition_test`, the five `rhi::PlanComposition` checks
   exercise the merged Q1b planner in `frame_resources.h`. These show the fixture values and the
   existing seam are live, so a later failure cannot be blamed on a dead fixture.
2. **Seam checks that fail because the production slice is absent.** The failure values are the
   expected ones, not crashes: `expected VALID actual NO_HISTORY`, `expected GENERATION actual
   NO_HISTORY`, `status 3 ...` for the unsupported composition plan. The stub prints
   `SEAM-STUB <entry>: production implementation absent` on stderr for every call.

A few red-file checks pass trivially against the stub because they assert a *rejection* (for
example "out-of-range texel is refused untouched" or "reset on an unrendered view is no-history").
They are not counted as T043/T045/T047 evidence; they become meaningful only when the paired
positive check passes.

## Blockers and findings

1. **Write-scope blocker (this session).** T043/T045/T047 each require production code at
   `src/renderer/rhi/view_history.cpp` and `src/renderer/rhi/overlay_composition.cpp`. This session
   owns `tests/renderer/**` only, so the slice stops at the red state. The proposed seams are
   `tests/renderer/q3_view_history_seam.h` and `tests/renderer/q3_overlay_seam.h`; the production
   slice must promote them into `src/renderer/rhi/`, implement them, replace
   `q3_view_history_stub.cpp`/`q3_overlay_stub.cpp` in `tests/renderer/CMakeLists.txt`, and remove
   `WILL_FAIL` from the three tests.
2. **Merged public-header collision (pre-existing, found by this slice).**
   `src/renderer/rhi/frame_resources.h:29` and `src/renderer/rhi/motion_history.h:83` both define
   `doom3::rhi::MotionSample` in namespace `doom3::rhi`. Any translation unit that includes both
   fails with MSVC `C2011` ("redefinition"); this was observed while building the combined stub
   (`q3_proposed_seam.vcxproj`, before the stub was split). The overlay test therefore uses a local
   runner instead of `tests/renderer/motion_test_support.h`. This blocks a natural Q3 integration
   unit that needs both composition and motion types (T047/T085/T048) until one type is renamed by
   the owner of    `src/renderer/rhi/**`. It is not fixed here: that path is out of scope. Tracked as issue #58 for
   the RHI owner.
3. **T044 is partial, not complete.** The seam (`skinned_history.cpp`) and the retention test
   (`skinned_history_test.cpp`, 43 checks) exist and pass, but T044's second half - snapshotting
   private prior positions in `engine-fork:neo/renderer/Model_md5.cpp` - is not delivered (PR #50
   deferred it). The T044 row stays unchecked.
4. **No tracking issue.** No open issue covers T043/T045/T047; `tasks.md` is the tracker. No issue
   number is invented here.

## Negative controls

| Control | Mutation | Expected | Actual |
|---|---|---|---|
| Absent production slice | no `view_history.cpp` / `overlay_composition.cpp`; stub returns "nothing rendered" | every seam assertion fails, exit 1 | 35/52/25 failures, exit 1 in Debug and Release |
| Fixture values live | direct `ReprojectPoint` and `PlanComposition` checks | pass without the new seam | 6/6 and 5/5 pass in the red runs |
| T042/T044/T046 mutation | sign flip, current-pose-as-previous, viewmodel depth policy | assertion failure | recorded in PR #50; **not re-run** here (src is out of scope) |

## Task status contributed by this slice

| Task | Status after this slice | Note |
|---|---|---|
| T042 | **PASS** | `motion_reprojection_test.cpp` merged (`f703fae`), 109/0 here; fixture check and PR #50 red/mutation on record |
| T044 | **PARTIAL / OPEN** | seam and 43/0 test pass; the engine-fork snapshot in `Model_md5.cpp` is not delivered |
| T045 | **RED / OPEN** | 55 checks, 52 fail; production slice absent |
| T046 | **PASS** | `motion_validity_test.cpp` merged, 77/0 here; policy red/mutation in PR #50 |
| T043 | **RED / OPEN** | 46 checks, 35 fail; production slice absent |
| T047 | **RED / OPEN** | 33 checks, 25 fail; production slice absent |
| T048 | **NOT RUN** | waits for T041-T047 and T085 |

## Independent review

- **Independent reviewer**: NOT RUN — no reviewer assigned to this slice.
- **Review scope and commands**: none.
- **Review decision and unresolved items**: NOT RUN. The author-side checks above are structural
  and executable evidence, not review.
