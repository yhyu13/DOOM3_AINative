# Q3 motion, history and HUD evidence (T043, T044, T045, T047, T048, T085)

Status: author evidence for issue [#62](https://github.com/yhyu13/DOOM3_AINative/issues/62) and
the residual tracking issue [#120](https://github.com/yhyu13/DOOM3_AINative/issues/120), written
2026-11-02. This is **not** Q3 acceptance, not an engine frame and not an independent review. It
records what the portable slices do on this host and exactly what could not run.

**Headline: T044's portable half now covers shared-geometry regeneration, deletion and the
`generation` supersession path, and T085's portable half executes the actual Q2 ordered stream
before taking the current/previous pose and writing per-pixel motion/validity. The full Q3 set is
green in Debug and Release. T048's analytic checks ran; its moving-scene check and independent
review are still blocked. The engine-fork halves of T044 and T085 remain BLOCKED for the exact
reasons in §6. No engine frame was executed and no image was captured.**

## Identity and scope

- **Stage and claim**: Q3 (US4) residual portable slice. Claim: rendered-frame history, view
  isolation, invalidation reasons, private skinned-pose retention across shared-cache
  regeneration/deletion, the Q2-executed current/previous pose and per-pixel motion/validity path,
  post-reconstruction display-HUD ordering and the analytic T041 fixtures behave as the tests
  specify. **No** engine-frame, parity, capture or HUD-image claim.
- **Source identity**: branch `feat/q3-residual`, rebased onto `main` at
  `c4509cd` (`feat(web): verify the Q7 target adapter and record T072/T073 evidence`, PR #119);
  the earlier base was `8815239`. PR #119's T072/T073 updates are preserved.
  The commit under test is the PR head; the reviewer records that SHA. Submodule
  `doom3_orig/doom3` @ `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb`
  (`remotes/origin/spec-004-q2-production`), `git -C doom3_orig/doom3 status --porcelain` empty;
  **no** submodule or fork change in this slice.
- **Data identity and rights**: synthetic CPU fixtures (`tests/renderer/motion-fixtures.md`,
  `q2-fixtures.md`) only. **No `.pk4`/`.pk3`/`.pak` is present and no game data is committed.**
  Legal-data captures are `SKIPPED — BLOCKED` on authorized data; none is fabricated.
- **Scene and inputs**: analytic engine 16:9 camera (`tanX=4/3`, `tanY=3/4`), the T041 rows
  CAM-01..07, RIG-01..04, SKN-01..03, DEF-01, VM-01..02 and SEQ-01..03, and the Q2 command stream
  from `SceneAdapter` fixtures. No camera path, demo or frame.
- **Exclusions and limitations**: the engine fork (`neo/renderer/Model_md5.cpp`, `RC_*` dispatch),
  `targets/win11-amd64/CMakeLists.txt`, the root build, the Q2 adapter implementation, S2 shaders,
  the Q4 harness, Q5, Q6/Q7 and all image comparison are out of scope. No hardware adapter row
  belongs to this slice because no device frame is produced.

## Environment and configuration

- **Toolchain and configuration**: Windows 10.0.22631 x64; MSVC 19.44.35207 (VS 2022
  Professional, v143); Windows SDK 10.0.26100.0; VS-bundled CMake 3.31.6 at
  `C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`;
  generator `Visual Studio 17 2022 -A x64`; `/W4 /permissive- /EHsc`; Debug and Release.
- **Adapter and driver**: N/A — no window, shader or GPU frame is claimed. The Q2 adapter is
  exercised with `structuralOnly = true`; the integration test asserts
  `drawsSubmitted == 0` and `drawSubmissionUnavailable == true` for every executed fixture row.
- **Capability and fallback**: none exercised beyond C++17 CPU code; no fallback is selected.
- **Shader provenance and build**: N/A — no shader is loaded or compiled in this slice.

## Procedure and retained artifacts

- **Predeclared tolerances**: T041 keeps the §5.1 rule, `max(1 ULP_half(|expected|), 1e-7)` per
  component. The integration storage round-trip is `float`, compared at `1.0e-6`; that extra slack
  applies only to the storage round-trip, never to the reprojection check.
- **Commands and exits** (worktree root; ignored `build-q3-residual/` holds the logs):

```text
cmake -S tests/renderer -B build-q3-residual -G "Visual Studio 17 2022" -A x64  -> 0  build-q3-residual-configure.log
cmake --build build-q3-residual --config Release -- -m                          -> 0  build-q3-residual-build-Release.log
cmake --build build-q3-residual --config Debug   -- -m                          -> 0  build-q3-residual-build-Debug.log
ctest --test-dir build-q3-residual -C Release -L q3 --output-on-failure         -> 0, 4/4   build-q3-residual/ctest-Release-q3.log
ctest --test-dir build-q3-residual -C Debug   -L q3 --output-on-failure         -> 0, 4/4   build-q3-residual/ctest-Debug-q3.log
ctest --test-dir build-q3-residual -C Release --no-tests=error --output-on-failure
                                                                                -> 8, 37/38 build-q3-residual/ctest-Release.log
```

  The three unlabelled Q3 executables and the four labelled tests were also run directly; every
  summary below is an exit code, not a parsed CTest label:

```text
Release and Debug, exit 0 each:
q3_motion_reprojection_test   checks=109 failures=0   Release-q3_motion_reprojection_test.log
q3_skinned_history_test       checks=58  failures=0   Release-q3_skinned_history_test.log
q3_motion_validity_test       checks=77  failures=0   Release-q3_motion_validity_test.log
q3_view_history_test          checks=47  failures=0   Release-q3_view_history_test.log
q3_history_reset_test         checks=55  failures=0   Release-q3_history_reset_test.log
q3_overlay_composition_test   checks=33  failures=0   Release-q3_overlay_composition_test.log
q3_frame_integration_test     checks=211 failures=0   Release-q3_frame_integration_test.log
```

```text
py -3 -I scripts/validate-rhi-contract.py                                       -> 0  build-q3-residual/validate-rhi-contract.log
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --check tests/renderer/motion-fixtures.md -> 0  fixture-check.log
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --self-test        -> 0  fixture-self-test.log
```

  Contract gate result: `header scan: RUN over 25 header(s) and 30 public test file(s)`; fixture
  result: `20 fixture rows and 10 validity rows match the generator`; self-test result:
  `5 numeric mutations and the zero-valid contract are detected`.

- **Retained logs and artifacts**: worktree-local, git-ignored `build-q3-residual/`:
  the configure/build logs, `ctest-*.log`, `Release-q3_*.log`, `Debug-q3_*.log`,
  `mutation-skinned-history.log`, `restore-skinned-history.log`,
  `validate-rhi-contract.log`, `fixture-check.log`, `fixture-self-test.log`. Machine-specific
  paths stay in these ignored logs; none is tracked. No screenshots or frame captures exist.
- **Negative controls**:
  - Removing the `generation` check in `SkinnedPoseHistory::PreviousPose` makes
    `q3_skinned_history_test` exit 1 with 3 failures (`regeneration: superseded generation is
    rejected: expected GENERATION actual VALID`, `main generation 3 superseded`, and the older
    `stale generation rejected`). Source restored, rebuilt, re-run to 58/0 exit 0
    (`mutation-skinned-history.log`, `restore-skinned-history.log`).
  - The HUD-before-scene stream in the integration test is refused before the adapter consumes
    anything; a display-overlay commit is refused; a non-Valid sample is stored invalid rather
    than as a valid zero; an out-of-range motion write is refused; a resize advances the per-pixel
    generation.
  - The full CTest run is **not** cited as a full pass: `rhi.q1a.diagnostics` fails on this host
    because `D3D12GetDebugInterface` returns `hr=0x887A002D` (SDK debug layer unavailable) and the
    test expects four ID-820 warnings per cleared row while the layer is active. The failing target
    does not compile either changed test file; every Q3 test passes. The one failure is recorded,
    not waived or re-labelled.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| `rhi.q3.motion_reprojection` (T042) | RUN | PASS | `checks=109 failures=0`, exit 0, Release and Debug: CAM/RIG rows, valid zero, behind-camera rejection | `build-q3-residual/Release-q3_motion_reprojection_test.log`, `Debug-q3_motion_reprojection_test.log` |
| `rhi.q3.skinned_history` (T044) | RUN | PASS | `checks=58 failures=0`, exit 0, Release and Debug: SKN-01..03, shared cache overwrite/delete, regeneration with a freed/reallocated source, generation supersession, subview isolation, clear | `build-q3-residual/Release-q3_skinned_history_test.log`, `Debug-q3_skinned_history_test.log` |
| `rhi.q3.motion_validity` (T046) | RUN | PASS | `checks=77 failures=0`, exit 0, Release and Debug | `build-q3-residual/Release-q3_motion_validity_test.log`, `Debug-q3_motion_validity_test.log` |
| `rhi.q3.view_history` (T043) | RUN | PASS | `checks=47 failures=0`, exit 0, Release and Debug | `build-q3-residual/Release-q3_view_history_test.log`, `Debug-q3_view_history_test.log` |
| `rhi.q3.history_reset` (T045) | RUN | PASS | `checks=55 failures=0`, exit 0, Release and Debug | `build-q3-residual/Release-q3_history_reset_test.log`, `Debug-q3_history_reset_test.log` |
| `rhi.q3.overlay_composition` (T047) | RUN | PASS | `checks=33 failures=0`, exit 0, Release and Debug | `build-q3-residual/Release-q3_overlay_composition_test.log`, `Debug-q3_overlay_composition_test.log` |
| `rhi.q3.frame_integration` (T085 portable half) | RUN | PASS | `checks=211 failures=0`, exit 0, Release and Debug: actual Q2 stream executed, then SKN-01..03/DEF-01 previous pose committed, current pose reprojected, per-pixel motion/validity stored, HUD refused, second Q2 frame advances the previous pose, structural-only draw count is zero | `build-q3-residual/Release-q3_frame_integration_test.log`, `Debug-q3_frame_integration_test.log` |
| Full CTest Release | RUN | **PARTIAL** | 37/38, exit 8. Only `rhi.q1a.diagnostics` fails (`hr=0x887A002D`, no D3D12 debug layer on this host); all Q3 verdicts are real pass exits | `build-q3-residual/ctest-Release.log` |
| Q3 label CTest Release/Debug | RUN | PASS | 4/4 each, exit 0 | `build-q3-residual/ctest-Release-q3.log`, `ctest-Debug-q3.log` |
| Contract header/test gate | RUN | PASS | `header scan: RUN over 25 header(s) and 30 public test file(s)`, exit 0 | `build-q3-residual/validate-rhi-contract.log` |
| Fixture document + self-test | RUN | PASS | `20 fixture rows and 10 validity rows match the generator`; `5 numeric mutations and the zero-valid contract are detected` | `build-q3-residual/fixture-check.log`, `fixture-self-test.log` |
| Negative control: generation check removed | RUN | **FAIL (intended)** | `q3_skinned_history_test` exit 1, 3 failures; restored run 58/0 exit 0 | `build-q3-residual/mutation-skinned-history.log`, `restore-skinned-history.log` |
| Engine-fork private pose snapshot (`Model_md5.cpp`) | NOT RUN | **BLOCKED** | See §6.1; no fork Q3 branch, submodule bump needs owner sign-off, no data to exercise an md5 model | — |
| Engine-fork / actual frame resources (T085 fork half) | NOT RUN | **BLOCKED** | See §6.2; the superproject installs no Q2 sink consumer and links no Q3 sources into `doom3_engine` | — |
| Legal-data moving-scene capture (T048) | SKIPPED | **BLOCKED** | No authorized `.pk4`/`.pk3`/`.pak`, no engine binary and no engine call site; image comparison not attempted | — |
| Independent review (`review-q3.md`) | NOT RUN | — | Requested in the PR; a non-author must rerun the commands at the reviewed SHA and record the verdict | — |

### Per-case tolerances used by the analytic run

The tolerance column is the predeclared fixture value, `max(1 ULP_half(|expected|), 1e-7)` per
component. Invalid rows have no numeric tolerance; the expected validity reason is the check.

| Case | du | dv | tolerance (du, dv) | Invalid reason |
|---|---:|---:|---|---|
| `CAM-01` | 0.007500000 | 0.000000000 | (3.81e-06, 1.00e-07) | — |
| `CAM-02` | 0.013095289 | 0.000000000 | (7.63e-06, 1.00e-07) | — |
| `CAM-03` | 0.000000000 | -0.006666667 | (1.00e-07, 3.81e-06) | — |
| `CAM-04` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — (valid zero) |
| `CAM-05` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — (jitter excluded) |
| `CAM-06` | — | — | — | `BEHIND_CAMERA` |
| `CAM-07` | — | — | — | `BEHIND_CAMERA` |
| `RIG-01` | -0.001875000 | 0.000000000 | (9.54e-07, 1.00e-07) | — |
| `RIG-02` | -0.006545649 | 0.000000000 | (3.81e-06, 1.00e-07) | — |
| `RIG-03` | 0.000037129 | 0.000000000 | (1.00e-07, 1.00e-07) | — |
| `RIG-04` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — |
| `SKN-01` | 0.000000000 | -0.003333333 | (1.00e-07, 1.91e-06) | — |
| `SKN-02` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — |
| `SKN-03` | -0.013095289 | -0.000004064 | (7.63e-06, 1.00e-07) | — |
| `DEF-01` | 0.000000000 | -0.003333333 | (1.00e-07, 1.91e-06) | — |
| `VM-01` | 0.000000000 | -0.003333333 | (1.00e-07, 1.91e-06) | — |
| `VM-02` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — |
| `SEQ-01` | 0.000000000 | 0.003333333 | (1.00e-07, 1.91e-06) | — |
| `SEQ-02` | 0.000000000 | 0.006666667 | (1.00e-07, 3.81e-06) | — |
| `SEQ-03` | 0.000000000 | 0.000000000 | (1.00e-07, 1.00e-07) | — |

Reset reasons exercised by `q3_history_reset_test`: `Cut`, `Teleport` and `LevelLoad` map to
`Reset`; `ProviderSwitch` maps to `ProviderSwitch`; an extent change maps to `Resize`; an entity
identity change maps to `EntityReuse`; a topology change maps to `Topology`; a view generation
change maps to `Generation`; an unrendered view reports `NoHistory`. A `Valid` reason passed to
`InvalidateView` is a no-op.

### What the green runs actually prove

- The history key is `(ViewKind, instanceId)`; main, mirror, camera and shadow renders keep
  separate records, and an unrendered simulation tick never replaces the last rendered pose.
- A rejected lookup reports exactly one reason in the documented precedence and leaves the
  caller's output untouched.
- A private skinned pose is a copy: freeing and reallocating the shared posed array, then deleting
  it, does not change the stored previous pose; a new generation supersedes the old one without
  serving stale positions; a subview commit does not replace the main record.
- The display HUD in an executed Q2 stream never enters history; the executed scene view is the
  only record that can advance the previous pose.
- The Q2 stream is validated before the adapter consumes it, so a HUD-before-scene stream cannot
  be drawn even once.
- Per-pixel validity is separate from the vector: an invalid sample is stored invalid and its
  components are not consumed; a valid stationary vector remains valid.

## Task status contributed by this slice

| Task | Status after this slice | Note |
|---|---|---|
| T044 | **PORTABLE HALF PASS; engine half BLOCKED** | 58/0 in Debug and Release; regeneration/deletion/subview tests; `Model_md5.cpp` snapshot blocked per §6.1 |
| T048 | **ANALYTIC PASS; MOVING-SCENE BLOCKED; REVIEW NOT RUN** | All Q3 analytic suites green; no legal data, no engine frame; independent review requested in the PR |
| T085 | **PORTABLE HALF PASS; engine half BLOCKED** | Q2-executed stream -> previous/current pose -> per-pixel motion/validity, 211/0; engine call sites blocked per §6.2 |

The task rows are updated in `tasks.md` with these qualifications. A checked portable row does not
claim the fork half, a real frame or Q3 acceptance.

## Blockers and findings

### 6.1 T044 engine-fork private pose snapshot

Recorded evidence, not an assumption:

1. `git submodule status` reports `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb doom3_orig/doom3
   (remotes/origin/spec-004-q2-production)` and `git -C doom3_orig/doom3 status --porcelain` is
   empty. The submodule is read-only (AGENTS.md non-negotiable 1); a snapshot cannot be added in
   place.
2. `gh pr list --repo yhyu13/DOOM-3 --state all` reports fork PR #1 **MERGED** on
   2026-11-01T06:13:16Z. The earlier "blocked on fork PR #1" wording is stale.
3. `git ls-remote --heads https://github.com/yhyu13/DOOM-3.git` lists only `master`
   (`6e0fddb...`) and `spec-004-q2-production` (`cd6ae66...`). There is no Q3 fork branch and no
   Q3 snapshot commit to review.
4. A submodule bump is excluded from self-merge and requires **owner sign-off** in addition to
   independent review (AGENTS.md non-negotiable 11). No owner sign-off is recorded for a Q3 bump.
5. No authorized game data exists in the worktree: a bounded recursive search for
   `*.pk4`/`*.pk3`/`*.pak` returns zero files, and no `DOOM3.exe` is built. Even a fork snapshot
   could not be exercised on an md5 model here.
6. `targets/win11-amd64/CMakeLists.txt` and the root CMake are out of scope for this assignment;
   the engine does not link the Q3 integration.

The portable copy semantics (regeneration, deletion, generation supersession, subview isolation)
are proven by `q3_skinned_history_test`; the engine `UpdateSurface` call site is not.

### 6.2 T085 engine-fork / actual-frame-resource wiring

Recorded evidence:

1. `grep` over `src/` for `R_SetQ2SceneSink`/`q2SceneSink_t` returns no consumer. The fork emits
   the Q2 sink, but the superproject never installs one, so there is no engine path from
   `RB_*`/`RC_*` into `doom3::rhi::SceneAdapter`.
2. No Q3 source is linked into `doom3_engine`; the target/root CMake is untouched by assignment.
   The Q3 integration is compiled only by `tests/renderer/CMakeLists.txt`.
3. The Q2 adapter is `structuralOnly`; the new test asserts `drawsSubmitted == 0` and
   `drawSubmissionUnavailable == true`. It extracts and classifies, but cannot submit geometry.
4. No legal data and no engine frame (see §6.1 items 5 and 6), so no GPU motion/validity resource
   is written and no image can be captured.

The superproject sequence (Q2 stream executed -> current/previous pose -> per-pixel motion and
validity) is proven by `q3_frame_integration_test`; the engine call site and a real frame are not.

### 6.3 Other findings

- The public-header collision from issue #58 remains fixed by the `PerPixelMotionSample` rename;
  issue #58 stays open until its owner accepts it.
- `rhi.q1a.diagnostics` is host-blocked (`hr=0x887A002D`) and is recorded as a failure, not a pass.
  It does not touch the changed Q3 tests.

## Independent review

- **Independent reviewer**: NOT RUN — requested in the PR. The author must not merge without an
  `APPROVED` verdict from a non-author at the reviewed SHA, per AGENTS.md §11.
- **Review scope and commands**: rerun the seven Q3 executables in Debug and Release
  (`build-q3-residual/{Release,Debug}/q3_*_test.exe`), the Q3 label CTest, the contract validator
  and the fixture self-test; inspect the new `RunRegenerationAndDeletion` and
  `RunQ2ScenePoseWiring` cases; confirm no `targets/`/root CMake, Q4/Q5/Q6/Q7 or submodule change.
  The `rhi.q1a.diagnostics` host failure must be reported, not waived.
- **Review decision and unresolved items**: NOT RUN.
