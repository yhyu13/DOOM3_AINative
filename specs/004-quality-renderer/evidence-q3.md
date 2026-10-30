# Q3 motion, history and HUD evidence (T043, T044, T045, T047, T048, T085)

Status: author evidence for issue [#62](https://github.com/yhyu13/DOOM3_AINative/issues/62), written
2026-10-30. This is **not** Q3 acceptance, not an engine frame and not an independent review. It
records what the merged portable slices do on this host and what could not run.

**Headline: the T043/T045/T047 production slices exist and their tests are green in Debug and
Release with the expected-red inversion removed. The Q2 ordered stream now drives Q3 history,
private pose and per-pixel motion through `src/renderer/rhi/frame_integration.cpp`. No engine frame
was executed and no image was captured: the engine-fork call sites (private pose snapshot in
`Model_md5.cpp`, `RC_*` dispatch) are blocked on a fork PR plus submodule bump, and legal game data
is absent. T044 and T085 stay partial; T048 is NOT RUN.**

## Identity and scope

- **Stage and claim**: Q3 (US4) portable slice. Claim: rendered-frame history, view isolation,
  invalidation reasons, private skinned pose retention, post-reconstruction display-HUD ordering and
  the Q2-stream-to-Q3 integration behave as the tests specify. **No** engine-frame, parity, capture
  or HUD-image claim.
- **Source identity**: branch `feat/q3-motion-hud-integration`, based on `main` at `9bb40c9`.
  Submodule `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, `git -C
  doom3_orig/doom3 status --porcelain` empty; **no** submodule or fork change in this slice. The
  tracked tree is dirty only with the files named in §3 before the commit under test.
- **Data identity and rights**: synthetic CPU fixtures (`tests/renderer/motion-fixtures.md`,
  `q2-fixtures.md`) only. **No `.pk4`/`.pk3`/`.pak` is present and no game data is committed.**
  Legal-data captures are `SKIPPED — BLOCKED` on authorized data; none is fabricated.
- **Scene and inputs**: analytic engine 16:9 camera (`tanX=4/3`, `tanY=3/4`) and the T041 rows
  SKN-01/SKN-02/SKN-03/DEF-01; the Q2 command stream from `SceneAdapter` fixtures. No camera path,
  demo or frame.
- **Exclusions and limitations**: the engine fork (`neo/renderer/Model_md5.cpp`, `RB_*` dispatch),
  `targets/win11-amd64/CMakeLists.txt`, the Q2 adapter implementation, S2 shaders and all image
  comparison are out of scope. No hardware adapter row belongs to this slice because no device call
  is made.

## Environment and configuration

- **Toolchain and configuration**: Windows 10.0.22631 x64; MSVC 19.44.35207 (VS 2022
  Professional, v143); Windows SDK 10.0.26100.0; VS-bundled CMake 3.31.6, generator
  `Visual Studio 17 2022 -A x64`; `/W4 /permissive- /EHsc`; Debug and Release.
- **Adapter and driver**: N/A — no device, window, shader or GPU call. The Q2 adapter is exercised
  in `structuralOnly` mode and reports `drawSubmissionUnavailable`, which is why no draw is claimed.
- **Capability and fallback**: none exercised beyond C++17 CPU code; no fallback is selected.
- **Shader provenance and build**: N/A — no shader is loaded or compiled in this slice.

## Procedure and retained artifacts

- **Predeclared tolerances**: T041 rows keep the predeclared §4 tolerances (for example SKN-01
  `1.00e-07`/`1.91e-06`, SKN-03 `7.63e-06`/`1.00e-07`). The integration test compares the stored
  per-pixel value, which is `float`, with the fixture `double` at `1.0e-6`; that extra slack is
  declared here and applies only to the storage round-trip, never to the reprojection check.
- **Commands and exits** (worktree root, retained under git-ignored `build-q3/` and
  `build-q3-clean/`):

```text
cmake -S tests/renderer -B build-q3 -G "Visual Studio 17 2022" -A x64            -> 0  build-q3-configure.log
cmake --build build-q3 --config Release -- -m                                    -> 0  build-q3-build-Release.log
cmake --build build-q3 --config Debug   -- -m                                    -> 0  build-q3-build-Debug.log
ctest --test-dir build-q3 -C Release --no-tests=error --output-on-failure        -> 0, 27/27  build-q3/ctest-Release-final.log
ctest --test-dir build-q3 -C Debug   --no-tests=error --output-on-failure        -> 0, 27/27  build-q3/ctest-Debug.log
q3_view_history_test.exe                                                         -> 0, 47/0   build-q3/q3_view_history_test.log
q3_history_reset_test.exe                                                        -> 0, 55/0   build-q3/q3_history_reset_test.log
q3_overlay_composition_test.exe                                                  -> 0, 33/0   build-q3/q3_overlay_composition_test.log
q3_frame_integration_test.exe                                                    -> 0, 119/0  build-q3/q3_frame_integration_test.log
py -3 -I scripts/validate-rhi-contract.py                                        -> 0  build-q3/validate-rhi-contract.log
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --check tests/renderer/motion-fixtures.md -> 0
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --self-test         -> 0
git diff --check                                                                 -> 0
```

  Clean-tree re-run from an empty directory, so the result does not depend on incremental state:

```text
cmake -S tests/renderer -B build-q3-clean -G "Visual Studio 17 2022" -A x64      -> 0
cmake --build build-q3-clean --config Release -- -m                              -> 0 (no error/warning lines)
ctest --test-dir build-q3-clean -C Release --no-tests=error --output-on-failure  -> 0, 27/27
```

- **Retained logs and artifacts**: worktree-local, git-ignored `build-q3/` and `build-q3-clean/`:
  `build-q3-*.log`, `ctest-*.log`, `ctest-*-junit.xml`, `q3_*.log`, `Debug-q3_*.log`,
  `mutation-history_reset.log`, `restore-history_reset2.log`, `validate-rhi-contract.log`,
  `fixture-check.log`, `fixture-self-test.log`. The JUnit XML holds the 27-test names and verdicts.
  Machine-specific paths stay in these ignored logs; none is tracked.
- **Negative controls**:
  - The prior red state (merged `633182d`) is the control for an absent production slice:
    `q3_view_history_test` 46 checks/35 failures, `q3_history_reset_test` 55/52,
    `q3_overlay_composition_test` 33/25, all exit 1. Those raw exits are recorded in
    [evidence-q3-tests.md](./evidence-q3-tests.md). They are not re-labelled as passes.
  - Mutation: removing the pending-reason branch in `RenderedViewHistory::Previous` makes
    `q3_history_reset_test` exit 1 with 14 failures (`cut invalidates main: expected RESET actual
    VALID`, explicit resize, teleport, …). Source restored, rebuilt, and re-run to 55/0 exit 0;
    logs `build-q3/mutation-history_reset.log`, `build-q3/restore-history_reset2.log`.
  - The integration test carries its own controls: a HUD-before-scene stream is refused and not
    consumed, a display-overlay commit is refused, an invalid sample is stored invalid rather than
    as a valid zero, and a resize advances the per-pixel generation.
  - Issue #58's collision is the control for the header fix: the new integration translation unit
    includes `frame_resources.h` and `motion_history.h` together and compiles; before the rename it
    did not (MSVC C2011).

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| `rhi.q3.view_history` (T043) | RUN | PASS | `checks=47 failures=0`, exit 0, Release and Debug. One more check than the red run because the usable-state branch executes instead of short-circuiting. | `build-q3/q3_view_history_test.log`, `build-q3/Debug-q3_view_history_test.log` |
| `rhi.q3.history_reset` (T045) | RUN | PASS | `checks=55 failures=0`, exit 0, Release and Debug | `build-q3/q3_history_reset_test.log`, `build-q3/Debug-q3_history_reset_test.log` |
| `rhi.q3.overlay_composition` (T047) | RUN | PASS | `checks=33 failures=0`, exit 0, Release and Debug | `build-q3/q3_overlay_composition_test.log`, `build-q3/Debug-q3_overlay_composition_test.log` |
| `rhi.q3.frame_integration` (T085 superproject half) | RUN | PASS | `checks=119 failures=0`, exit 0: SKN-01/02/03 and DEF-01 through history and per-pixel storage, Q2 stream order, invalidation events, private-pose copy | `build-q3/q3_frame_integration_test.log`, `build-q3/Debug-q3_frame_integration_test.log` |
| Full CTest Release | RUN | PASS | 27/27, no `WILL_FAIL` inverted test; `rhi.q3.*` verdicts are real exit codes | `build-q3/ctest-Release-final.log`, `build-q3/ctest-Release-final-junit.xml` |
| Full CTest Debug | RUN | PASS | 27/27 | `build-q3/ctest-Debug.log`, `build-q3/ctest-Debug-junit.xml` |
| Clean-tree Release | RUN | PASS | 27/27 from an empty build directory | `build-q3-clean/ctest-Release.log` |
| Contract header/test gate | RUN | PASS | `header scan: RUN over 14 header(s) and 23 public test file(s)`, exit 0 | `build-q3/validate-rhi-contract.log` |
| Fixture document + self-test | RUN | PASS | `20 fixture rows and 10 validity rows match the generator`; `5 numeric mutations and the zero-valid contract are detected` | `build-q3/fixture-check.log`, `build-q3/fixture-self-test.log` |
| Negative control: pending reason removed | RUN | **FAIL (intended)** | `q3_history_reset_test` exit 1, 14 failures; restored run 55/0 exit 0 | `build-q3/mutation-history_reset.log`, `build-q3/restore-history_reset2.log` |
| Engine-fork private pose snapshot (`Model_md5.cpp`) | NOT RUN | — | Blocked: `doom3_orig/doom3` is read-only; needs a fork PR + reviewed submodule bump + owner sign-off (AGENTS.md §11). The superproject copy semantics are proven; the engine call site is not. | — |
| Engine frame / GPU per-pixel resources | NOT RUN | — | Blocked: no fork call site and no device frame in this slice; the Q2 adapter is structural-only. Not claimed. | — |
| Legal-data scene capture (T048) | SKIPPED — BLOCKED | — | No authorized `.pk4`/demo in the worktree; image comparison is not attempted. | — |
| Independent review (`review-q3.md`) | NOT RUN | — | Reviewer not yet assigned. The PR must be reviewed by a non-author at the PR head/base before merge. | — |

### What the green runs actually prove

- The history key is `(ViewKind, instanceId)`; a main, mirror, camera and shadow render keep separate
  records, and an unrendered simulation tick never replaces the last rendered pose (the
  reprojection stays at SEQ-01/SEQ-02 as the fixture requires).
- A rejected lookup reports exactly one reason in the documented precedence and leaves the caller's
  output untouched; the reason persists until the next commit.
- Display overlays are excluded from history and from the reconstruction inputs; a HUD declared at
  the render extent, a scene/world GUI at the display extent, a missing or duplicate scene and a
  zero extent are refused.
- The Q2 view stream is validated before the adapter consumes it, so a HUD-before-scene stream
  cannot be drawn even once.
- A private pose is a copy: rewriting the shared array after `CommitPrivatePose` does not change the
  stored previous pose.

## Task status contributed by this slice

| Task | Status after this slice | Note |
|---|---|---|
| T043 | **PASS (portable slice)** | `view_history.cpp` implemented; 47/0 in Debug and Release |
| T044 | **PARTIAL / OPEN** | private copy + test pass; the `engine-fork:Model_md5.cpp` snapshot is still blocked |
| T045 | **PASS (portable slice)** | `view_history.cpp` invalidation; 55/0 |
| T047 | **PASS (portable slice)** | `overlay_composition.cpp`; 33/0 |
| T048 | **NOT RUN** | waits for the T044/T085 fork half and an independent review; no legal-data capture |
| T085 | **PARTIAL / OPEN** | Q2 stream → history/pose/per-pixel motion proven in `frame_integration.cpp` + 119/0 test; the reviewed engine fork and real frame resources are not delivered |

The task rows are updated in `tasks.md` with these qualifications. A checked row means the portable
slice named by the task is delivered and tested; it does not mean the fork half or Q4 exists.

## Blockers and findings

1. **Engine-fork delivery (T044/T085).** The pristine submodule cannot be edited in place, and a
   submodule bump is excluded from self-merge and needs owner sign-off (AGENTS.md §11). The fork PR
   must add the `Model_md5.cpp` private-pose snapshot and the `RC_*` dispatch to the adapter; this
   record does not claim either.
2. **Build-system.** `targets/win11-amd64/CMakeLists.txt` is **not** modified. The new sources are
   compiled by `tests/renderer/CMakeLists.txt` only, so the engine executable does not yet link the
   Q3 integration. Adding it is a build-system change with its own review.
3. **Issue #58 collision fixed as a minimal rename.** `frame_resources.h`'s per-pixel type is now
   `PerPixelMotionSample`; `motion_history.h` keeps `MotionSample`. `rhi_temporal_test.cpp` was
   updated. This is a public-header change in a Q1b file and is disclosed here as a real integration
   defect, not an ownership extension; issue #58 remains open until its owner accepts the rename.
4. **No legal data, no frame.** Every image/scene row above is `SKIPPED — BLOCKED` or `NOT RUN`;
   nothing here is parity or playability evidence.

## Independent review

- **Independent reviewer**: NOT RUN — no reviewer assigned to this slice. The author must not merge
  without an `APPROVED` verdict from a non-author at the reviewed SHA, per AGENTS.md §11.
- **Review scope and commands**: none yet. Suggested: rerun the four `rhi.q3.*` executables and the
  full CTest in both configurations, inspect the `frame_integration.cpp` ordering check, and confirm
  the no-WILL_FAIL and no-`targets/` changes.
- **Review decision and unresolved items**: NOT RUN.
