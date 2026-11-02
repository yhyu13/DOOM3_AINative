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
