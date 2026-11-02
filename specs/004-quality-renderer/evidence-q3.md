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
| `renderer.q3.engine_frame` (T085 engine link) | RUN | PASS | Sink callbacks executed through the real adapter on hardware and software adapters; `draws=2 submitted=2 submissions=1`, green-left/red-right readback; BMP/PNG captured | `build-tests/q3-engine-frame.log`, `specs/004-quality-renderer/evidence/q3-engine-sink-frame.png` |
| `renderer.q3.motion_resources` (T085 engine link) | RUN | PASS | Previous/current pose reprojected; motion bytes `0,0,212,154`, valid pixel `255`, invalid pixel `0`; both textures read back from the device on both adapters | `build-tests/q3-engine-motion.log`, `specs/004-quality-renderer/evidence/q3-engine-motion-resources.png` |
| Engine link (T085) | RUN | PASS | Q3 sources + installer compiled into `doom3_engine`; `R_SetQ2SceneSink` referenced from the installer object and its strings present in `DOOM3.exe` | `build/_installer_symbols.log`, `build/_build.log` |
| Full renderer CTest on this branch, Release and Debug | RUN | PASS | 44/44 in each configuration, exit 0; Q3 label 6/6 in each | `build-tests/_ctest-all-release.log`, `build-tests/_ctest-all-debug.log` |
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
| T044 | **PORTABLE HALF PASS; engine half BLOCKED** | 58/0 in Debug and Release; regeneration/deletion/subview tests; `Model_md5.cpp` snapshot blocked per §6.1. Rechecked on this branch: the fork still has no Q3 branch and no owner sign-off |
| T048 | **ANALYTIC PASS; MOVING-SCENE BLOCKED; REVIEW NOT RUN** | All Q3 analytic suites green; the T085 sink path now submits a synthetic frame, but no legal-data moving scene and no independent review; requested in the PR |
| T085 | **DONE (engine-link half)** | `R_SetQ2SceneSink` consumer installed, Q3 sources linked into `doom3_engine`, synthetic Q2-executed frame submitted and read back, per-pixel motion/validity uploaded as device textures; 44/44 Debug and Release. No retail-engine frame is claimed |

The task rows are updated in `tasks.md` with these qualifications. T085 claims the superproject
engine-link half only: the installed consumer and frame resources are exercised by a synthetic Q2
stream, not by a retail `.pk4` scene, and T044's fork snapshot remains unbuilt. Q3 acceptance is still
not claimed.

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
6. `targets/win11-amd64/CMakeLists.txt` now links the Q3 sources and the sink registration unit into
   `doom3_engine` for T085, but that does not add the `Model_md5.cpp` previous-pose snapshot. T044
   still needs a fork branch and a reviewed, owner-signed submodule bump; editing the read-only
   submodule or the target copy is not a substitute.

The portable copy semantics (regeneration, deletion, generation supersession, subview isolation)
are proven by `q3_skinned_history_test`; the engine `UpdateSurface` call site is not.

### 6.2 T085 engine-link / actual-frame-resource wiring

Delivered on this branch (2026-11-02):

1. `src/renderer/rhi/engine_q2_sink.{h,cpp}` is the portable consumer. It translates the ordered
   `beginView`/`drawPacket`/`lightPacket`/`endView`/`setColorTarget`/`copyToImage`/`swapBuffers`
   sequence into a `SceneCommandQueue`, executes it through `FrameIntegration`, and refuses a frame
   that would submit only part of the extracted draws or lights.
2. `src/renderer/rhi/engine_q2_sink_install.cpp` includes the fork `renderer/RenderApi.h`, bridges
   the engine's value structs to the portable ones, and installs the consumer with
   `R_SetQ2SceneSink` from a static object in `doom3_engine` before `R_InitRenderApi` runs.
3. `targets/win11-amd64/CMakeLists.txt` compiles the installer into `doom3_engine` and adds the Q3
   production sources (`motion_history`, `skinned_history`, `view_history`, `overlay_composition`,
   `frame_integration`, `frame_motion_resources`, `engine_q2_sink`) to `doom3_rhi`, which
   `doom3_engine` links and `DOOM3_LINK_LIBS` places on both executable lines.
4. `src/renderer/rhi/frame_motion_resources.cpp` writes the `HistoryBuffer` into two device textures:
   RGBA8 motion (binary16 du in R/G, dv in B/A) and R8 validity (255/0). This is the actual
   frame-resource write behind `FrameIntegration::UploadPerPixelMotion`.
5. `tests/renderer/q3_engine_frame_test.cpp` drives the same callback table: `frame` submits a real
   ordered two-surface frame (`drawsSubmitted=2`, one submission, green-left/red-right readback) and
   `motion` reprojects the previous/current pose, stores a valid sample and an invalid sample,
   uploads the two textures, renders them through a vertex-input fullscreen quad and reads the exact
   bytes back.

What is still not claimed: the fork call sites that supply a live geometry source and the private
`Model_md5.cpp` pose are absent, so the engine executable cannot render a retail scene and no
retail `.pk4` frame is captured. That remains T044/T048 work and is recorded as BLOCKED there.

### 6.4 Commands and exits for the T085 engine-link run

Run on this host on 2026-11-02; logs are worktree-local and git-ignored.

| Command | Exit | Observed |
|---|---:|---|
| `cmake --preset win11-amd64` | 0 | target copy created/patched; C++ compiler MSVC 19.44.35228.0 |
| `cmake --build --preset win11-amd64-release -- -m` | 0 | `build/bin/Release/DOOM3.exe` 17,580,032 bytes and `DOOM3XP.exe` 17,763,328 bytes; no `error C`/`error LNK` |
| `dumpbin /symbols ...engine_q2_sink_install.obj` | 0 | `g_installer` defined and `R_SetQ2SceneSink` referenced |
| `findstr` for the consumer's refusal strings in `DOOM3.exe` | 0 | strings present, so `engine_q2_sink.cpp` and its dependencies were linked |
| `ctest --test-dir build-tests -C Release` | 0 | 44/44 passed |
| `ctest --test-dir build-tests -C Debug` | 0 | 44/44 passed |
| `q3_frame_integration_test.exe` | 0 | `checks=211 failures=0` |
| `q3_engine_frame_test.exe frame` | 0 | hardware and software adapters PASS; `draws=2 submitted=2 submissions=1`; pixels green-left/red-right |
| `q3_engine_frame_test.exe motion` | 0 | both adapters PASS; motion bytes `0,0,212,154`, validity `255` then `0` |
| `python scripts/validate-rhi-contract.py` | 0 | header scan over 27 headers and 33 public test files |

Artifacts: `specs/004-quality-renderer/evidence/q3-engine-sink-frame.png` and
`q3-engine-motion-resources.png` (8x8 readbacks scaled to 128x128 nearest).

### 6.3 Other findings

- The public-header collision from issue #58 remains fixed by the `PerPixelMotionSample` rename;
  issue #58 stays open until its owner accepts it.
- `rhi.q1a.diagnostics` failed on the earlier residual host (`hr=0x887A002D`, no debug layer) and is
  recorded there as a failure. On this branch's host it passed as part of 44/44; that does not waive
  the earlier host's result.

## Independent review

- **Independent reviewer**: NOT RUN — requested in the PR. The author must not merge without an
  `APPROVED` verdict from a non-author at the reviewed SHA, per AGENTS.md §11.
- **Review scope and commands**: rerun the Q3 executables in Debug and Release, including
  `q3_engine_frame_test frame` and `q3_engine_frame_test motion`; rerun the Q3 label CTest, the full
  renderer suite, the contract validator and the fixture self-test; rebuild
  `cmake --build --preset win11-amd64-release -- -m` and confirm the installer object references
  `R_SetQ2SceneSink` and the consumer strings are present in `DOOM3.exe`; inspect
  `RunQ2ScenePoseWiring`, the new sink consumer and `frame_motion_resources.cpp`; confirm no
  Q4/Q5/Q6/Q7 harness change and no submodule or retail-data change. The `rhi.q1a.diagnostics`
  result must be reported for the reviewer's host, not waived.
- **Review decision and unresolved items**: NOT RUN.
