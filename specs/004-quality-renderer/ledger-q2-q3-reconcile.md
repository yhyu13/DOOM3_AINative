# Q2/Q3 ledger reconciliation (2026-11-02)

Status: reconciliation of the Q2 (US3) and Q3 (US4) task rows against merged work. This is
**not** Q2/Q3 acceptance, not a parity or capture result, and not an independent review. It changes
the ledger and this record only; no code, test, target CMake, Q4/Q5/Q6/Q7 file is touched.

## Source identity and method

- Branch `docs/q2-q3-ledger`, based on `main` at `08de566` (the #121 merge).
- Submodule `doom3_orig/doom3` at `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb`;
  `git -C doom3_orig/doom3 status --porcelain` is empty.
- Fork PR #1 (`yhyu13/DOOM-3`) is **MERGED** (verified with `gh pr view 1 --repo yhyu13/DOOM-3`).
  `git ls-remote --heads https://github.com/yhyu13/DOOM-3.git` lists only `master`
  (`6e0fddbf20880ca1ebea5e3445d263133451f39f`) and `spec-004-q2-production`
  (`cd6ae660...`). There is no Q3 fork branch.
- The renderer tests were rebuilt and rerun in this worktree. Logs are in the git-ignored
  `build-ledger/` directory.

### Commands run here

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build-ledger -G "Visual Studio 17 2022" -A x64` | 0 | configured with MSVC 19.44.35228.0 |
| `cmake --build build-ledger --config Release -- -m` | 0 | all test executables built |
| `cmake --build build-ledger --config Debug -- -m` | 0 | all test executables built |
| `q2_engine_command_order_test` (Debug and Release) | 0 / 0 | `T034 PASS` |
| `q2_classic_compatibility_test` (Debug and Release) | 0 / 0 | `T037 PASS (structural/classification only; image parity NOT RUN)` |
| `q2_classic_gui_test` (Debug and Release) | 0 / 0 | `T038 PASS (structure only; HUD image metrics NOT RUN)` |
| `q3_motion_reprojection_test` (Debug and Release) | 0 / 0 | `checks=109 failures=0` |
| `q3_skinned_history_test` (Debug and Release) | 0 / 0 | `checks=58 failures=0` |
| `q3_motion_validity_test` (Debug and Release) | 0 / 0 | `checks=77 failures=0` |
| `q3_view_history_test` (Debug and Release) | 0 / 0 | `checks=47 failures=0` |
| `q3_history_reset_test` (Debug and Release) | 0 / 0 | `checks=55 failures=0` |
| `q3_overlay_composition_test` (Debug and Release) | 0 / 0 | `checks=33 failures=0` |
| `q3_frame_integration_test` (Debug and Release) | 0 / 0 | `checks=211 failures=0` |

These reruns reproduce the Q3 counts quoted in `evidence-q3.md` and the Q2 green exits quoted in
`evidence-q2.md` §4.2. They do not reproduce the original red runs; the red record remains the one in
`evidence-q2.md` §4.1.

### Merged review evidence

- **PR #113** carries an independent `APPROVED` comment at reviewed SHA
  `444ea73b14cd6f473eaddf83b273170b455dd28a` (reviewer session
  `ses_e782c579bffe020RqOqN1Qx0EK`). It covers the gitlink, the re-exported patch/manifest, a fresh
  target apply, configure/build and the selector strings in `DOOM3.exe`. It explicitly makes no Q2
  acceptance or parity claim.
- **PR #121** carries an independent `APPROVED` comment at reviewed SHA
  `98ef7ffd00e25166f3b2769fda2fa7eca36f695a` (same reviewer session), but the comment records that
  the PR was already merged at `08de566` before the verdict and that the base pin was not met. It
  reviews the Q3 content, not a moving-scene capture.
- `specs/004-quality-renderer/review-q2.md` and `specs/004-quality-renderer/review-q3.md` do not
  exist (`Test-Path` returns `False` for both).
- `specs/004-quality-renderer/review-q2-design.md` is the S4 design-review log for T032/T033/T041.
  Its Round 8 PASS covers the design deliverables only and states that "Q2/Q3 implementation/execution
  and human sign-off are not claimed"; it is not T040 evidence.

## Corrected ledger

Status vocabulary: **DONE** = every deliverable and stated condition in the row is evidenced;
**PARTIAL** = at least one stated deliverable is delivered and at least one is absent or unreviewed;
**BLOCKED** = the remaining work cannot proceed with the data, hardware, branch or review available
here.

| Task | Status | Evidenced now | Unmet condition |
|---|---|---|---|
| T034 | **DONE** | `tests/renderer/engine_command_order_test.cpp` covers BeginView → DrawPacket → SetColorTarget → CopyToImage → SwapBuffers, the `CaptureToFile` flush prefix and the `CaptureToImage` append. Rerun exits 0 in Debug and Release; red record in `evidence-q2.md` §4.1. | — |
| T035 | **PARTIAL** | `src/renderer/rhi/engine_scene_adapter.cpp` is implemented and exercised. Fork `RenderSystem.cpp` replaces the `glConfig.isInitialized` gates with `R_RenderSystemRunning()` and keeps `R_IssueRenderCommands()` in `CaptureRenderToFile`, so a mid-frame capture keeps its position in the ordered stream. | The row's "separately reviewed" condition is not met: T040 is NOT RUN and review-q2.md is absent. The superproject still installs no sink consumer (`git grep R_SetQ2SceneSink -- src` returns nothing), which is T080/T085 scope. |
| T036 | **PARTIAL** | The fork selector and typed failure are in the bumped tree; `targets/win11-amd64/CMakeLists.txt:153-174` builds `doom3_rhi` and appends it to both link lines; PR #113 found `renderapi_status` and `r_renderApi` in `DOOM3.exe`. | The required failed-initialization test is absent: `git grep -n "failed initialization\|renderapi\|InitializationFailure" -- tests/renderer` returns no match. |
| T037 | **PARTIAL** | `tests/renderer/classic_compatibility_test.cpp` covers stage order, alpha coverage, projected/falloff lights, fog/blend, shadow chains and suppressions, and viewmodel/model depth hack; rerun exits 0 in both configurations. | Portals/subviews are absent. The file header lists CP-02–CP-12, CP-19–CP-21 and CP-24; `git grep -n "portal\|subview\|mirror"` over the Q2 tests returns no match, so CP-14/CP-15/CP-16 are not covered. |
| T038 | **PARTIAL** | `tests/renderer/classic_gui_test.cpp` covers overlay view mapping, HUD-after-scene, world-GUI-in-view and target stability; rerun exits 0 in both configurations. Fork handling exists: `tr_light.cpp` sets `DSF_GUI_SURFACE`, `tr_backend.cpp` derives per-packet `worldGui` and per-view `overlay`, and `GuiModel.cpp` gates changed to `R_RenderSystemRunning()`. | The row names `tr_guisurf.cpp`, which the fork merge does not touch, and no `CopyToImage` capture-ordering case exists in `classic_gui_test.cpp` (the capture ordering is in T034). HUD image metrics are NOT RUN (T039). |
| T039 | **BLOCKED** | `evidence-q2.md` §6 records an entry for every CP-01–CP-30 case, with classification PASS/UNSUPPORTED where a CPU check exists and `NOT RUN`/`SKIPPED` where an image is required. | No fresh gameplay, named-scene parity or image comparison exists. No `.pk4`/`.pk3`/`.pak` is present in this worktree, no `DOOM3.exe` is built here, and issue #105 records the retail/demo data block. Every image row remains `NOT RUN`/`SKIPPED`, not parity. |
| T040 | **BLOCKED / NOT RUN** | The PR #113 verdict independently checked the submodule bump, patch apply, build and selector strings. | `review-q2.md` does not exist, and the PR #113 verdict explicitly disclaims Q2 acceptance and parity. No independent review of the Q2 behaviour (adapter, material/light semantics, GUI split) has been recorded. |
| T080 | **PARTIAL** | The fork emits world surfaces (`RB_Q2EmitDrawSurf`, `RB_Q2EmitViewLight` in `tr_backend.cpp`), and the adapter extracts/classifies draw packets in order. | Draw submission is absent. `engine_scene_adapter.cpp` sets `drawSubmissionUnavailable` whenever a live stage run is extracted; `q3_integration_test.cpp:312-313` asserts `drawsSubmitted == 0` and `drawSubmissionUnavailable`. No classic frame is drawn. |
| T081 | **PARTIAL** | `materials.cpp` builds the bump → diffuse → specular plan, coverage class, draw class and resolver-gated refusal; T037's stage-order and alpha cases pass. | Texture upload/sampling is absent: the `materials.h` API exposes only `PlanMaterial` and no upload/sampler entry point. The fork merge does not change `Image.cpp`/`Image.h`, so the "reviewed engine fork material/image code" half is absent. |
| T082 | **PARTIAL** | `src/renderer/d3d12/lighting.cpp` classifies ambient/interaction/fog/blend and global/local shadow chains with the three suppressions; T037's light and shadow-exclusion cases pass. | The stencil-shadow path is explicitly not implemented. The file header says the slice is "deliberately a classification with no stencil state" and that "stencil ops, two-sided volume, depth clamp" are not implemented. CP-07 remains `NOT RUN`. |
| T083 | **BLOCKED / NOT RUN** | The selector is compiled into the rebuilt binary per PR #113 (`renderapi_status`, `r_renderApi`). | No forced-failure launch ran: there is no legal game data in this worktree to boot and no `build/bin/Release/DOOM3.exe`. `evidence-q2.md` §8 records NOT RUN. |
| T044 | **PARTIAL** | `q3_skinned_history_test` covers shared-cache regeneration with a freed/reallocated source, deletion, generation supersession and subview isolation; 58/0 in Debug and Release, with a generation-removal mutation control. | The `engine-fork:neo/renderer/Model_md5.cpp` private pose snapshot is absent: `git -C doom3_orig/doom3 grep` finds no previous-pose/prevPose symbol in `Model_md5.cpp`. The submodule is read-only, the merged fork has no Q3 branch, a bump needs owner sign-off, and no authorized data exists to exercise an md5 model. |
| T048 | **PARTIAL** | All seven Q3 executables pass in Debug and Release (109/58/77/47/55/33/211 checks, 0 failures), including the Q2-executed T085 path. `evidence-q3.md` records per-case tolerances and reset reasons. | The moving-scene check has no legal data, engine call site, sink consumer or engine binary. `review-q3.md` does not exist. Synthetic vectors alone do not satisfy the row. |
| T085 | **PARTIAL** | `q3_frame_integration_test` executes the actual Q2 ordered stream, commits the executed scene view's previous pose, reprojects the current pose, stores per-pixel motion/validity, refuses the executed HUD and advances the previous pose on the second Q2 frame; 211/0 in both configurations. | The engine half is absent: no `R_SetQ2SceneSink` consumer exists in `src/`, no Q3 source is linked into `doom3_engine`, and no engine frame resource is written. |

Only T034 is ticked by this reconciliation. The other rows stay unchecked with the status recorded
above; a checked portable row would still not claim the fork half, a real frame or acceptance.

## Corrections to the previous annotations

- T034, T037, T038 and T080–T082 had no status note despite PR #60 (portable implementation) and
  #113 (fork bump) being merged. They now carry the statuses above.
- T036 and T083 already said "open"/"NOT RUN"; those notes were correct and are retained.
- T044's note said the engine half was blocked because "fork PR #1 is already merged (so the earlier
  reason is stale)". The merge is real, but it is not the blocker: the blocker is that no Q3 fork
  branch exists, the submodule is read-only, a bump needs owner sign-off, and no data is present.
  The note is corrected accordingly.
- T048/T085 already said "portable half complete, engine half BLOCKED"; that is correct. This
  record adds that `review-q3.md` does not exist and that PR #121 was merged before its independent
  verdict was posted.
- T039 and T040 had no note. T039 is BLOCKED on the data recorded in issue #105; T040 is NOT RUN
  because `review-q2.md` was never written.

## Remaining blockers

| # | Blocker | Rows affected | Unblock condition |
|---|---|---|---|
| B1 | No authorized retail/demo data in the worktree; issue #105 is open | T039, T048 (moving scene), T083 | A legitimate owned install or licensed source under `_data/` (untracked), or the rows are permanently marked out of scope |
| B2 | No independent Q2 review artifact | T040 (and T035's "separately reviewed" condition) | A non-author reviewer writes `review-q2.md` at the reviewed SHA |
| B3 | No independent Q3 review artifact | T048 (review half) | A non-author reviewer writes `review-q3.md` at the reviewed SHA |
| B4 | No Q3 fork branch; submodule is read-only and a bump needs owner sign-off | T044 (engine half) | Owner-authorized Q3 fork branch with the `Model_md5.cpp` snapshot, then a reviewed bump |
| B5 | No superproject sink consumer, no Q3 linkage into `doom3_engine`, no frame-resource write | T085 (engine half), T080 (submission) | Reviewed engine integration plus target/root CMake work, which this assignment excludes |
| B6 | The portable seam has no vertex/index submission | T080 | A seam extension that can submit geometry, then a real frame |
| B7 | No texture upload/sampling and no stencil state in the portable slices | T081, T082 | The missing production slices, which this assignment excludes |

## Non-claims

- Q2 and Q3 are **not accepted**. No classic frame, no GL-vs-D3D12 image comparison, no HUD metric,
  no moving-scene capture and no engine frame resource is claimed.
- The green reruns prove the CPU/structural fixtures only. `q2_classic_compatibility_test` and
  `q2_classic_gui_test` print the same "image parity NOT RUN" qualifier.
- PR #121's independent verdict arrived after its merge; that is recorded as a process breach, not
  as a content defect. It does not satisfy T048's `review-q3.md` requirement.
- The `rhi.q1a.diagnostics` host failure (`hr=0x887A002D`) is unchanged and is not cited as a pass.

## Addendum 2026-11-02 — T080–T082 superproject slice (branch `feat/q2-draw-texture-stencil`)

This addendum corrects the blocker table above for the superproject half only. It does not change
any acceptance, parity or review claim.

| Row | Previous state | State after this branch | Remaining block |
|---|---|---|---|
| T080 | PARTIAL; no geometry submission | Superproject **delivered and device-tested** (`renderer.q2.draw_geometry`, `renderer.q2.scene_submit`) | Engine-fork call site (B5) |
| T081 | PARTIAL; no texture upload/sampling | Superproject **delivered and device-tested** (`renderer.q2.texture_sampling`, `CreateTexture` for RGBA8/sRGB/R8/BC4/BC5/BC7_UNORM_SRGB) | Engine-fork `Image*` upload call site |
| T082 | PARTIAL; no stencil state | Superproject **delivered**; synthetic CP-07 PASS plus a swapped-op negative control (`renderer.q2.stencil_shadow`) | Engine-fork interaction call site; every retail-data CP-07 row (B1) |

B5 still holds for the engine integration. B6 and B7 described the portable seam's *missing* code and
are now stale for the superproject half: the seam has vertex/index submission, texture sampling and
stencil state. They remain accurate for the fork half. B1 (no data) and B2 (no Q2 review) are
unchanged, so Q2 is still not accepted and no CP row becomes parity.

## Addendum 2026-11-02 — T085 engine-link slice (branch `feat/q3-t085-engine-link`)

This addendum corrects the T085 row for the superproject engine-link half only. It does not change
Q3 acceptance, T044's block or T048's moving-scene/review block.

| Row | Previous state | State after this branch | Remaining block |
|---|---|---|---|
| T085 | PARTIAL; no sink consumer, no Q3 linkage, no frame resource | **DONE for the engine-link half**: `EngineQ2SinkConsumer` installed through `R_SetQ2SceneSink`; Q3 sources linked into `doom3_engine` via `doom3_rhi`; `renderer.q3.engine_frame` submits a real ordered two-surface frame; `renderer.q3.motion_resources` uploads/readbacks motion and validity; 44/44 tests in Debug and Release | A fork call site supplying the live geometry source and private pose (T044), plus legal data for a moving-scene capture (B1) |

B4 (no Q3 fork branch / no owner sign-off) and B5 (superproject sink linkage) are now split: B5's
superproject half is resolved, while B4 still blocks T044's `Model_md5.cpp` snapshot and therefore the
retail-engine frame. B1 is unchanged, so no moving-scene capture or Q3 acceptance is claimed.

## Addendum 2026-11-02 — Q2/Q3 fork call sites and bump preparation

Branch `spec-004-q2-q3-engine` (fork PR https://github.com/yhyu13/DOOM-3/pull/2, merged to fork
`master` at `c87e87b`) implements the engine half the earlier ledger marked absent. The superproject bump points
`doom3_orig/doom3` at `c87e87b` (from `cd6ae660`), re-exports the target patch and contract-tests the
seam. It does **not** accept Q2 or Q3 and is not merged.

| Row | Previous state | State after fork PR #2 + bump | Remaining |
|---|---|---|---|
| T080 | PARTIAL; no geometry submission | Fork emits `surfacePacket` with the `srfTriangles_t` snapshot and model transform; target build links it; `renderer.q2.fork_seam` submits the synthetic surface | Live engine frame: no device consumer in `DOOM3.exe`; no retail data |
| T081 | PARTIAL; no texture upload | Fork forwards `GenerateImage` RGBA8 bytes, dimensions, filter/repeat and stable identity, plus per-stage image ids; target build links `R_Q2ImageIdentity`/`R_Q2EmitImageUpload` | No device texture cache in the engine; no live engine upload |
| T082 | PARTIAL; no stencil call site | Fork emits one light packet per global/local chain with `chainKind`, surface count, noShadow/noSelfShadow and exclusion facts; `renderer.q2.fork_seam` proves one local chain + one excluded light | Engine sends no shadow-volume mesh; retail CP-07 NOT RUN |
| T044 | PARTIAL; engine half BLOCKED | Fork `Model_md5.cpp` keeps a private `(viewId, stable surface id)` previous pose with generation handling; target build defines `R_Q3PreviousPrivatePose`/`R_Q3ResetPrivatePoses`; `privatePose` reaches `FrameIntegration` | Engine path needs a device consumer and an md5 asset to run; runtime BLOCKED |
| T048 | PARTIAL; moving scene BLOCKED | Unchanged: analytic checks pass, moving scene still needs a live engine frame | `review-q3.md` absent |
| T085 | DONE (engine-link half) | Sink accepts surfaces/images/poses; the fork call sites now feed it | Still no device backend/pipelines in the engine |
| T039/T040/T083 | BLOCKED/NOT RUN | Unchanged | Retail data (B1) and independent review (B2) |

### Corrected blockers

- **B4** (no Q3 fork branch / no owner sign-off) is **resolved for this branch**: the owner authorized
  the branch and bump on 2026-11-02 (verbatim "me grant right"), and the branch exists. The remaining
  T044 block is data plus a device consumer, not permission.
- **B5** (no sink consumer) was already split by the T085 addendum; the fork call sites now supply the
  values. The engine still installs no device backend/pipelines, so no live frame is claimed.
- **B6/B7** (portable seam missing vertex/texture/stencil) are stale: the seam has all three.
  The engine-side equivalents are delivered on the fork; their runtime remains unproven.
- **B1** (retail/demo data) is unchanged. Every CP-07 and parity image row stays `NOT RUN`/`SKIPPED`.
- **B2/B3** (no independent `review-q2.md`/`review-q3.md`) are unchanged. Neither PR may merge without
  an independent review; the bump additionally needs the recorded owner sign-off.

### Non-claims

- No engine frame, retail scene, CP-07 row, texture parity, moving-scene capture or pose stream is
  claimed. The `renderer.q2.fork_seam` frames are synthetic.
- A light packet with no shadow chain is `lightsExcluded`; the portable adapter does not yet draw a
  shade-only interaction frame for a real `noShadows` light with lit surfaces. The fixture's excluded
  light has no interaction surfaces, so that adapter gap is not claimed as fixed.
- The target patch is re-exported against `c87e87b`; `apply-target-patches.ps1` verified 74/74 modified
  rows on a fresh copy. The patch remains a modified-files-only reconstruction.
- The fork and bump PRs are open for review. Nothing here is Q2/Q3 acceptance.


## Addendum 2026-11-03 — Q2 device consumer, texture cache and shadow-volume packets

Branch `feat/q2-device-consumer`, issue #142. This addendum narrows the three remaining T080-T082
notes at source level; it does not accept Q2 and does not claim a live engine frame.

| Row | Previous remaining note | State after this branch | Still blocked |
|---|---|---|---|
| T080 | no device consumer in the engine | `EngineQ2DeviceConsumer` + `EngineQ2DeviceBackend` are compiled into `doom3_rhi`; a synthetic ordered surface frame is submitted and read back | `R_InitRenderApi` creates no Device/pipelines and no retail data exists, so no live engine frame |
| T081 | no live texture cache in the engine | `EngineQ2DeviceTextureCache` turns `imageUpload` into a device texture; the material stage selects the diffuse slot and a synthetic readback samples the uploaded image and a replacement | no engine Device exists to receive a live upload; one texture per draw and a static point sampler remain portable limits |
| T082 | engine sends no shadow-volume meshes | `Q2SinkShadowVolumePacket` is attached to its light identity and submitted as back/front stencil plus shade; chain mismatch is refused | the fork seam still does not emit the packet; no live engine frame and every retail-data CP-07 row is `NOT RUN` |

### Corrected blockers

- **B5** (no sink/device consumer) is split again: the superproject now links a concrete device
  backend and texture cache, but the engine still has no Device/pipeline creation call site. Source
  and link evidence are not a rendered engine frame.
- **B1** (retail data) and **B2** (independent Q2 review) are unchanged. T040 is still NOT RUN.
- The contract validator's only red item is the pre-existing `src/renderer/dxr/q6_motion_texture.h`
  D3D12-vocabulary hit from merged PR #138; this branch does not modify Q6.

### Non-claims

- No engine frame, retail scene, CP-07 row, texture parity or parity image is claimed. The
  `q2-device-*.png` frames are synthetic offscreen readbacks.
- The stage rule samples one diffuse-slot texture; it is not full multi-stage bump/diffuse/specular
  evaluation.
## Addendum 2026-11-03 — Q2 residual tests T036/T037/T038

Branch `feat/q2-residual-tests`, based on `main` at `8a58e43` (after the #145 device-consumer
merge; both addenda coexist). Submodule `doom3_orig/doom3` is at
`c87e87b` before and after this branch (the fork change below is on the fork only; the worktree was
restored to the pin). This addendum changes only the three rows and records the new test evidence; it
does not accept Q2 and does not run with retail data.

| Row | Previous state | State after this branch | Remaining condition |
|---|---|---|---|
| T036 | PARTIAL; failed-init test absent | **PARTIAL — test added and passing.** `tests/renderer/q2_render_api_fallback_test.cpp` proves the portable contract: actionable refusal without a backend/pipeline, no rewrite to structural-only, reference path still initializes and executes, bad extent refused then recovered. 2/2 configurations pass. | It does not execute the engine `r_renderApi` selector or the GL path. T083 (forced-failure launch) needs legal data and is NOT RUN; T040 is absent. |
| T037 | PARTIAL; CP-14/15/16 absent | **DONE for the test deliverable.** `classic_compatibility_test.cpp` adds CP-14 mirror subview (separate non-overlay view), CP-15 portal sky (one environment stage; manifest refuses the stub) and CP-16 xray subview (separate non-overlay view, blended surface). 2/2 configurations pass. | Image parity, mirror recursion, portal-sky content and xray surface indexing remain NOT RUN (T039). T080–T082 production halves unchanged. |
| T038 | PARTIAL; no `CopyToImage` case, `tr_guisurf.cpp` untouched | **PARTIAL — case added and fork change staged.** `classic_gui_test.cpp` records a mid-frame `CopyToImage` inside the world-GUI view, asserts it precedes the display overlay and rejects the copy-after-overlay order (negative control). Fork branch `spec-004-q2-q3-guisurf` (PR yhyu13/DOOM-3#3, commit `54d26a6`) marks `R_RenderGuiSurf`'s emitted quads `DSF_GUI_SURFACE`; `EmitFullScreen` stays an overlay. 2/2 configurations pass for the portable case. | The fork change is not in the pinned submodule and was not compiled here: a target build from the modified submodule would fail the patch result-hash check tied to `c87e87b`. A coordinated bump is required. HUD image metrics remain T039. |

### Evidence

`ctest --test-dir build-q2-residual -C Release -L q2` and `-C Debug -L q2` both report **12/12
passed** after the rebase onto `8a58e43`: the #145 device-consumer tests plus these T036–T038
additions; neither set was reverted. `q2_render_api_fallback_test`,
`q2_classic_compatibility_test` and `q2_classic_gui_test` exit 0 in both configurations. The full
commands and outputs are in
[evidence-q2.md](./evidence-q2.md) §14.

### Blockers

- **B1** (retail/demo data) is unchanged and still blocks T039, T048's moving scene and T083.
- **B2/B3** (no independent `review-q2.md`/`review-q3.md`) are unchanged; T040 is still NOT RUN.
- **B8 (new)**: fork PR yhyu13/DOOM-3#3 needs independent review, then the superproject bump needs
  owner sign-off and coordination. No second bump was created; the pinned SHA stays `c87e87b`.
  Until then the `tr_guisurf.cpp` change is not in the reviewed tree and is not compiled by the
  target build.

### Non-claims

- T037 being ticked covers its test-definition deliverable only; it is not a parity or acceptance
  claim. Every CP image row in T039 remains `NOT RUN`/`SKIPPED`.
- The T036 test proves the portable failure/fallback contract, not the engine selector launch.
- The T038 fork change is not compiled or run in this worktree; only the portable ordering case ran.

## Addendum 2026-11-03 — Q2 device follow-up (Device/pipelines, live sink, T038/T082 fork)

Branch `feat/q2-engine-device-followup` (superproject) plus fork branch
`spec-004-q2-device-followup` (PR yhyu13/DOOM-3#4). This addendum closes the source-level notes
that the previous addendum left, and records exactly what remains. It does not accept Q2, does not
move the submodule pin, and does not re-export the patches.

| Row | Previous remaining note | State after this branch | Still blocked |
|---|---|---|---|
| T038 | `tr_guisurf.cpp` untouched; fork change staged but not compiled | Fork commit `a91a70f` (PR #4) marks the `R_RenderGuiSurf` quads; compiled in the generated target copy with this patch set (build exit 0) | The pin is still `c87e87b`; the coordinated bump is pending fork review/merge and was not performed. HUD image metrics remain T039 |
| T080 | `R_InitRenderApi` creates no Device/pipelines | The installed initialize callback creates the hardware Device, the four classic pipelines and `EngineQ2DeviceConsumer`; `renderer.q2.engine_device_bootstrap` submits and reads back a textured frame | No retail data, so no engine frame; the pin does not include the fork branch until the bump |
| T081 | no engine Device exists to receive a live upload | The same callback installs `EngineQ2DeviceTextureCache`; `imageUpload` reaches a device texture and the draw samples it | One texture per draw, static point sampler and mip 0 only remain portable limits; no live upload until the bump |
| T082 | the fork seam does not emit the volume packet | Fork commit `46f9242` emits one `q2ShadowVolumePacket_t` per chain before its light packet; the installer converts it and the device consumer submits back/front stencil plus shade | No live engine frame and every retail CP-07 row remains `NOT RUN` |

### Corrected blockers

- **B5** (no sink/device consumer) is resolved at source level: the engine's initialize callback now
  creates the Device and pipelines. It is not a rendered engine frame; B1 still blocks that.
- **B8** (fork PR #3) is superseded by fork PR #4, which contains both the T038 commit and the T082
  packet. It requires independent review; the coordinated bump and patch re-export are the remaining
  steps and were **not** performed here because merging is prohibited for this task.
- **B1** (retail/demo data) is unchanged and still blocks T039, T048's moving scene and T083.
- **B2/B3** (no independent `review-q2.md`/`review-q3.md`) are unchanged; the T040 review recorded in
  PR #146 is not extended by this branch.

### Evidence

Renderer fixtures: **50/50 passed in Debug and Release** with MSVC 19.44.35228.0, including the new
`renderer.q2.engine_device_bootstrap`. Target builds: pin `c87e87b` exit 0 (`DOOM3.exe` 17,647,104
bytes) and fork diff applied exit 0 (`DOOM3.exe` 17,650,176 bytes); `dumpbin` shows
`EngineQ2DeviceRuntime` and `CreateEngineQ2ClassicPipelines` in `doom3_rhi.lib`. Full commands and
readback values are in [evidence-q2.md](./evidence-q2.md) §15.

### Non-claims

- No engine frame, retail scene, CP-07 row, parity image or Q2 acceptance is claimed.
- The shadow-volume packet is the caster chain the classic stencil pass draws, not a claim of a new
  port-side volume extrusion.
- `owner-sign-off:` in the PRs quotes the owner's verbatim authorization for Q2 fork branches and
  bumps ("me grant right"); it is not Q2 acceptance.
