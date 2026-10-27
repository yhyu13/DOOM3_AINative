# Tasks: Quality-first rendering for DOOM 3

**Input**: [spec.md](./spec.md), [plan.md](./plan.md), [research.md](./research.md), [data-model.md](./data-model.md), [RHI contract](./contracts/rhi-foundation.md) and [quickstart.md](./quickstart.md).

**Status**: Execution backlog, not a claim that any task or gate has passed. Paths under `src/`, `tests/` and `scripts/` are proposed exact delivery locations. Paths prefixed `engine-fork:` name files in a separately reviewed fork branch, **not** permission to edit the pristine `doom3_orig/doom3` submodule. Split future stages into separate real issues/worktrees; do not use one issue for the whole programme.

**Format**: `[P]` means the task may be prepared concurrently only in a **separate issue/worktree**; tasks in the same issue run serially under one writer. `[USn]` maps to the corresponding user story. For each behavioral change, first establish its public test seam, observe a real assertion failure, implement only that slice, run the unchanged test green, then deliberately break and restore production behavior to prove sensitivity. Compiler errors and skipped tests are not red or green evidence. All tasks remain unchecked until their own evidence exists. New remediation tasks T080–T087 are appended rather than renumbering the existing backlog; their phase placement defines execution order, not their numeric suffix.

## Phase 1 — Setup and governance

- [x] T001 Finalize the first-issue contract in `specs/004-quality-renderer/issue-q1a.md`, including scope, public seam, acceptance, validation distinctions and file ownership; filed as GitHub issue #11.
- [ ] T002 Using the filed issue #11, create a dedicated branch/worktree under the repository workflow after #10 merges and the applicable implementation authorization permits it; do not invent an issue number or reuse an unrelated worktree.
- [ ] T003 Record the issue/branch/worktree, baseline Git status, clean submodule status, intended writer/reviewer, fresh worktree-local build directory and effective build/test configuration in `specs/004-quality-renderer/evidence-q1a.md`.

## Phase 2 — Foundational contracts and evidence

- [ ] T004 Review the bounded public RHI client seam and ownership rules in `specs/004-quality-renderer/contracts/rhi-foundation.md` before any parallel implementation; keep native handles backend-private.
- [ ] T005 Define a reusable evidence record in `specs/004-quality-renderer/evidence-template.md` for build/source/data identity, effective configuration (toolchain, SDK, build type, adapter selection), scene, adapter/driver, exact command, exit, retained full test log, executed versus skipped checks, tolerances, exclusions and independent review.

**Checkpoint**: After T001–T005, Q0 (US1) and the data-independent Q1a slice of US2 may start independently in their own properly scoped worktrees. Q1a does not require legal game data or a completed Q0 baseline.

## Phase 3 — US1: Reproducible reference (P1; Q0)

**Goal**: Rebuild the existing reference safely and produce reproducible scene evidence without treating historical captures as a fresh run.

**Independent test**: Record a fresh native build and capture the four reference categories twice using predeclared tolerances, or report each unavailable data-dependent case as SKIPPED with its reason.

- [ ] T006 [P] [US1] Inspect the side effects and acceptance of the existing native scripts, including run, smoke, verify and export, and record a safe isolated baseline procedure in `specs/004-quality-renderer/baseline-procedure.md`; do not terminate unrelated processes or overwrite existing outputs. The procedure must be refusal-first for non-empty output directories and must match the executable actually launched.
- [ ] T007 [P] [US1] Specify the corridor, open view, close character/viewmodel and moving-light/combat scene identities, camera/initial state (`devmap`/`timedemo`/`loadgame`/script), scene-drive inputs and pre-run repeatability tolerances in `specs/004-quality-renderer/reference-scenes.md` without storing game data.
- [ ] T008 [US1] Add a behavior-tested safe baseline capture/check entry point in `scripts/test-render-baseline.ps1` and its synthetic side-effect tests in `tests/renderer/test-render-baseline.ps1`, following T006 and a separate issue contract.
- [ ] T009 [US1] Run a fresh isolated native configure/build and record commands, exits, executable identity, source/submodule and patch revisions in `specs/004-quality-renderer/evidence-q0.md`.
- [ ] T010 [US1] Capture each available T007 scene twice and record data identity, GPU/driver, commands, comparison and tolerances in `specs/004-quality-renderer/evidence-q0.md`; mark each missing prerequisite or scene SKIPPED.
- [ ] T011 [US1] Independently review the Q0 build, scene coverage and skipped checks in `specs/004-quality-renderer/evidence-q0.md`; retain the existing renderer as the reference.

## Phase 4 — US2: Synthetic RHI foundation (P1; Q1a → Q1b → Q1)

**Goal**: Prove actual D3D12 operations and lifetime behavior through a small backend-neutral client seam before game integration.

**Independent test**: Use standalone synthetic fixtures to verify GPU readbacks, resource retirement, motion/overlay, restart/resize and actually executed diagnostics; distinguish hardware, software and missing prerequisites.

### Q1a — first vertical slices (not full US2 or full Q1)

- [ ] T012 [US2] Create a standalone opt-in CMake/CTest project in `tests/renderer/CMakeLists.txt` and a compilable minimal public interface with nonfunctional stubs in `src/renderer/rhi/rhi.h`; it must not configure the root engine, require game data or fetch a proprietary third-party SDK. The installed Windows SDK D3D12/DXGI and Visual Studio C++ tools are required.
- [ ] T013 [US2] Write one public-seam test in `tests/renderer/rhi_clear_test.cpp` that selects an adapter, clears a synthetic 4×4 RGBA8 image using a per-row distinct pattern (at least two rows differ), checks every row against independently specified bytes and row hashes, and includes wrong-stride, off-by-one-row and repeat-row-0 negative controls; run it against T012 and record a real assertion failure in `specs/004-quality-renderer/evidence-q1a.md`.
- [ ] T014 [US2] Implement adapter selection, actionable errors, offscreen resource creation with the declared state sequence (`RENDER_TARGET` clear, `COPY_SOURCE` transfer, `COPY_DEST` readback), a state journal, submission, fence wait and correctly row-pitched readback in `src/renderer/d3d12/rhi_d3d12.cpp`, with backend-neutral client ownership through `src/renderer/rhi/rhi.h`. Readback crosses the public boundary as caller-owned bytes unless a bounded mapped-view contract is implemented and tested.
- [ ] T015 [US2] Run the unchanged T013 test green, intentionally break row indexing, stride and state behavior in turn, restore each and record every negative control plus the restored run and exact commands in `specs/004-quality-renderer/evidence-q1a.md`.
- [ ] T016 [US2] Add an independently sensitive public-seam test for at least two submissions, a release/reset attempt while a fence is still unsignalled (`GetCompletedValue() < signalValue` at release) and incomplete-init/shutdown safety in `tests/renderer/rhi_lifetime_test.cpp`; capture its real pre-implementation assertion failure in `specs/004-quality-renderer/evidence-q1a.md`.
- [ ] T017 [US2] Implement fence-associated resource, command-allocator and readback lifetime in `src/renderer/d3d12/rhi_d3d12.cpp`; an in-flight object must not be reset, reused or released before its last submitted use completes. A wait-idle-before-every-release implementation does not satisfy T016.
- [ ] T018 [US2] Run the unchanged T016 test green, deliberately break and restore retirement behavior, and record red/green/negative-control output, including the wait-idle shortcut failing the outstanding-fence assertion, in `specs/004-quality-renderer/evidence-q1a.md`.
- [ ] T019 [US2] Add opt-in debug-layer/GPU-based-validation enablement and message capture, plus explicit hardware, software and unavailable-adapter reporting in `tests/renderer/rhi_diagnostics_test.cpp` and `tests/renderer/CMakeLists.txt`; attempt each adapter class that is present, require at least one actual clear/readback execution, and never report an unavailable check as a pass.
- [ ] T020 [US2] Run the Q1a commands in `specs/004-quality-renderer/quickstart.md` in a fresh build tree with `-V` and/or `--output-junit`; record the full retained log, effective configuration, adapter identities, exit codes, every executed/skipped case and at least one actual clear/readback execution in `specs/004-quality-renderer/evidence-q1a.md`.
- [ ] T021 [US2] Obtain an independent review and rerun of T020, including the clean submodule and negative-control evidence, in `specs/004-quality-renderer/review-q1a.md`; this closes only Q1a.

### Q1b — programmable and temporal foundation

- [ ] T022 [P] [US2] Define independent pixel/compute/depth/MRT expected values and supported shader/tool prerequisites in `tests/renderer/q1b-fixtures.md` before implementing them.
- [ ] T023 [US2] Add a failing public-seam draw/pipeline fixture in `tests/renderer/rhi_draw_test.cpp`, then implement the smallest programmable draw/pipeline slice in `src/renderer/d3d12/rhi_pipeline.cpp` and prove red/green/mutation behavior.
- [ ] T024 [US2] Add a failing public-seam compute/UAV readback fixture in `tests/renderer/rhi_compute_test.cpp`, then implement dispatch and required ordering in `src/renderer/d3d12/rhi_compute.cpp` and prove red/green/mutation behavior.
- [ ] T025 [US2] Add a failing MRT and shader-readable depth fixture in `tests/renderer/rhi_mrt_depth_test.cpp`, then implement explicitly described attachments and transitions in `src/renderer/d3d12/rhi_attachments.cpp` and prove red/green/mutation behavior.
- [ ] T026 [US2] Add failing frame-slot, descriptor/upload ownership and state-transition tests in `tests/renderer/rhi_frame_resources_test.cpp`, then implement fence-safe frame resources in `src/renderer/d3d12/frame_resources.cpp` and prove red/green/mutation behavior.
- [ ] T027 [US2] Add synthetic numerical motion/validity and scene-versus-overlay tests in `tests/renderer/rhi_temporal_test.cpp`; implement only their bounded resources in `src/renderer/rhi/frame_resources.h`, keeping validity separate from a valid zero vector.
- [ ] T028 [US2] Add window presentation, repeated resize/minimize, restart and history-reset tests in `tests/renderer/rhi_present_test.cpp`, then implement the required backend behavior in `src/renderer/d3d12/rhi_present.cpp` without unsafe retirement.
- [ ] T029 [US2] Implement declared resource accesses and pass ordering, without speculative aliasing or multi-queue execution, in `src/renderer/rhi/frame_graph.cpp`; exercise ordering and invalid uses through `tests/renderer/frame_graph_test.cpp`.
- [ ] T030 [US2] Run all Q1 fixtures, genuine debug-layer and GPU-based validation, plus a fault-injected device-removal safe-failure test (bounded wait, cleanup and actionable reporting) and device-removal diagnostics where available; record unwaived messages, actual versus skipped checks and adapter class in `specs/004-quality-renderer/evidence-q1.md`.
- [ ] T031 [US2] Independently review and rerun Q1 in `specs/004-quality-renderer/review-q1.md`; full Q1 is BLOCKED until debug layer and GBV actually execute with zero unwaived errors.

## Phase 5 — US3: Classic-compatible engine path (P1; Q2)

**Goal**: Make the new path selectable and playable while preserving the GL reference and engine command semantics.

**Independent test**: Compare the same named legal-data scenes and gameplay sequence on both paths against predeclared tolerances; report every required legacy case as passed or explicitly unsupported.

- [ ] T032 [P] [US3] Specify parity thresholds, camera/data identity and the material/light/subview coverage matrix before capture in `specs/004-quality-renderer/classic-parity.md`, based on Q0 evidence.
- [ ] T033 [P] [US3] Specify a durable engine-fork delivery and both-executable linkage strategy, including `/MT`/`/MTd` compatibility and current target-patch limitations, in `specs/004-quality-renderer/engine-integration.md`.
- [ ] T034 [US3] Add failing command-stream and mid-frame capture ordering fixtures in `tests/renderer/engine_command_order_test.cpp`, including view, set-buffer, copy and present ordering.
- [ ] T035 [US3] Implement a backend-neutral ordered scene adapter in `src/renderer/rhi/engine_scene_adapter.cpp` and the separately reviewed fork integration in `engine-fork:neo/renderer/RenderSystem.cpp`; preserve mid-frame flush/capture behavior.
- [ ] T036 [US3] Implement selectable classic/reference fallback and link the RHI and required system libraries to both executables in `targets/win11-amd64/CMakeLists.txt` and the appropriate reviewed engine fork. Add a test that a failed modern-backend initialization reports an actionable result and leaves the existing path selectable and usable; do not invent an automatic-switch requirement.
- [ ] T037 [US3] Add and run coverage for material stage order, alpha coverage, projected/falloff lights, shadow exclusions, fog/blend, portals/subviews and viewmodel behavior in `tests/renderer/classic_compatibility_test.cpp`. This task defines tests; the corresponding production slices are T080–T082.
- [ ] T038 [US3] Add world-GUI versus display-overlay and ordered capture cases in `tests/renderer/classic_gui_test.cpp`, with corresponding fork-side handling in `engine-fork:neo/renderer/tr_guisurf.cpp` and `engine-fork:neo/renderer/GuiModel.cpp`.
- [ ] T039 [US3] Record fresh gameplay, named-scene parity and pass/unsupported entries for every T032 case in `specs/004-quality-renderer/evidence-q2.md`; missing legal data is SKIPPED, not parity.
- [ ] T040 [US3] Independently review Q2 and its fork/submodule delivery evidence in `specs/004-quality-renderer/review-q2.md`; no scratch-copy-only change counts as delivered.
- [ ] T080 [US3] Implement world-surface extraction and classic draw submission into the ordered backend-neutral stream in `src/renderer/rhi/engine_scene_adapter.cpp` and the reviewed engine fork (`engine-fork:neo/renderer/`); satisfy the T037 draw-order cases with production code rather than test doubles.
- [ ] T081 [US3] Implement texture upload/sampling and classic material-stage evaluation in `src/renderer/rhi/materials.cpp` and the reviewed engine fork material/image code; satisfy T037's stage-order and alpha-coverage cases with production code.
- [ ] T082 [US3] Implement projected/falloff lighting and the stencil-shadow/exclusion path in `src/renderer/d3d12/lighting.cpp` and the reviewed engine fork interaction code; satisfy T037's light and shadow-exclusion cases.
- [ ] T083 [US3] Force a modern-backend initialization failure in a real build and verify the actionable result plus the selectable/usable existing path end to end; record commands, exit codes and the observed path in `specs/004-quality-renderer/evidence-q2.md`. This is the integration check; T036 owns the unit-level test.
- [ ] T084 [US3] Add automated negative controls for manifest `A`/`D` handling in `scripts/export-target-patches.ps1`, `scripts/apply-target-patches.ps1` and `scripts/verify-win11-amd64.ps1`: prove a reconstruction containing added/deleted files cannot report PASS and that a partial `M`-row match is refused. If added/deleted target-copy delivery is required, implement reconstruction; otherwise document and enforce fork/submodule-only delivery in `engine-integration.md`. **Partial**: the apply-path controls now exist in `scripts/test-target-patches.ps1` (issue #9); exporter and verifier controls remain open.

## Phase 6 — US4: Motion, history and HUD (P1; Q3)

**Goal**: Use previous rendered geometry correctly, isolate subviews and compose the display HUD after reconstruction.

**Independent test**: Numerically verify camera, rigid, skinned and supported-deformation reprojection; exercise reset cases, private-pose lifetime, subviews and display-resolution overlay ordering.

- [ ] T041 [P] [US4] Write analytic camera/rigid/skinned/deformation expected reprojections and predeclared tolerances in `tests/renderer/motion-fixtures.md`; use previous unjittered UV minus current unjittered UV with top-left origin.
- [ ] T042 [US4] Add failing public-scene-seam camera/rigid reprojection and valid-zero-versus-invalid tests in `tests/renderer/motion_reprojection_test.cpp`, then implement the smallest history calculation in `src/renderer/rhi/motion_history.cpp`.
- [ ] T043 [US4] Add failing rendered-frame-versus-simulation-tick and main/mirror/camera/shadow isolation tests in `tests/renderer/view_history_test.cpp`; implement stable view identity and generation in `src/renderer/rhi/view_history.cpp`.
- [ ] T044 [US4] Add failing skinned previous-pose tests that regenerate/delete shared geometry and process subviews in `tests/renderer/skinned_history_test.cpp`; snapshot private persistent prior positions in `engine-fork:neo/renderer/Model_md5.cpp` and `src/renderer/rhi/skinned_history.cpp`.
- [ ] T045 [US4] Add and satisfy invalidation cases for cut, teleport, level, resize, topology, entity reuse and renderer/provider switch in `tests/renderer/history_reset_test.cpp` and `src/renderer/rhi/view_history.cpp`.
- [ ] T046 [US4] Add and satisfy explicit correspondence/rejection policies for viewmodels, particles, transparency, reflections, supported deformation and newly visible pixels in `tests/renderer/motion_validity_test.cpp` and `src/renderer/rhi/motion_history.cpp`.
- [ ] T047 [US4] Add a failing scene/world-GUI versus display-resolution HUD ordering fixture in `tests/renderer/overlay_composition_test.cpp`; implement post-reconstruction/post-provider overlay composition in `src/renderer/rhi/overlay_composition.cpp`.
- [ ] T048 [US4] Run the analytic and moving-scene checks, record per-case tolerances, reset reasons and reviewer-visible artifacts in `specs/004-quality-renderer/evidence-q3.md`, and independently review them in `specs/004-quality-renderer/review-q3.md`.
- [ ] T085 [US4] Wire the Q2 scene's current/previous pose and per-pixel motion/validity into actual frame resources end to end in the reviewed engine fork and `src/renderer/rhi/frame_resources.h`; the integration test consumes T041's skinned/deformation fixtures rather than only synthetic vectors.

## Phase 7 — US5: Playable modern quality (P2; Q4)

**Goal**: Deliver an art-reviewed, resource-budgeted hybrid renderer independent of proprietary neural finishing.

**Independent test**: Review the legal hero set and four moving/reference categories against a recorded rubric, while measuring resources and frame behavior on named hardware.

- [ ] T049 [P] [US5] Record a pre-review art rubric, hero-set provenance/permissions, scene identities and a measurement plan in `specs/004-quality-renderer/quality-rubric.md`; do not set numeric resource or frame budgets before T056 measurements. Do not track source or derived assets.
- [ ] T050 [P] [US5] Add independently expected material conversion and legacy-special-material fallback fixtures in `tests/renderer/material_contract_test.cpp`; legacy specular is not automatically ground-truth PBR.
- [ ] T051 [US5] Implement linear-HDR opaque material/lighting inputs in `src/renderer/rhi/materials.cpp` and `src/renderer/d3d12/hybrid_opaque.cpp`, retaining a forward/compatibility route for special and transparent materials.
- [ ] T052 [US5] Add direct-light/shadow numerical and unsupported-legacy-stage tests in `tests/renderer/lighting_test.cpp`, then implement the checked paths in `src/renderer/d3d12/lighting.cpp`.
- [ ] T053 [US5] Add indirect-light/reflection and atmosphere artifact cases in `tests/renderer/indirect_atmosphere_test.cpp`, then implement the checked paths in `src/renderer/d3d12/indirect_lighting.cpp`.
- [ ] T054 [US5] Add budget, mip/quality selection, throttled upload and in-flight eviction tests in `tests/renderer/residency_test.cpp`, then implement a fence-safe policy in `src/renderer/rhi/residency.cpp`.
- [ ] T055 [US5] Implement and test a complete non-proprietary reconstruction/off path in `src/renderer/rhi/reconstruction.cpp` and `tests/renderer/reconstruction_test.cpp`.
- [ ] T056 [US5] Capture the local legal hero set and four scene categories, recording accepted/rejected art examples, provenance, memory, frame behavior and named hardware in `specs/004-quality-renderer/evidence-q4.md`; after that measurement, set the numeric budgets in `quality-rubric.md` or record why they remain unset.
- [ ] T057 [US5] Independently review T056 for materials, lighting, darkness/readability, identity, motion and HUD in `specs/004-quality-renderer/review-q4.md`; do not substitute parity for modern-look acceptance.
- [ ] T086 [US5] Exercise overlay/reconstruction ordering from T047/T055 with real scene reconstruction and, only if T060 permits an authorized provider, confirm the provider's display-stage position from T062; record each check as PASS, SKIPPED or NOT RUN in the corresponding evidence file. An unavailable provider is NOT RUN, never a pass.

## Phase 8 — US6: Optional neural provider (P3; Q5)

**Goal**: Keep the renderer usable without NVIDIA components and make any provider claim depend on actual authorization, backend testing and distribution review.

**Independent test**: Build/run with no proprietary dependency; separately perform selected-backend create/evaluate and on/off captures only when SDK access, hardware and legal clearance exist.

- [ ] T058 [P] [US6] Define the generic optional provider boundary, resource ownership, input/mask/reset requirements and off behavior in `specs/004-quality-renderer/provider-contract.md` without inventing an unavailable SDK API.
- [ ] T059 [US6] Add an off/missing-provider behavioral test and a default-build evidence check that observes a clean configure/build for attempted NVIDIA downloads; final artifact or dependency inspection alone does not prove FR-014. Record the observation method and any NOT RUN limitation. Implement the non-proprietary boundary in `src/renderer/rhi/provider.cpp`.
- [ ] T060 [US6] Record actual SDK access, backend, version, device/driver, authorization and applicable distribution review in `specs/004-quality-renderer/provider-clearance.md`; otherwise mark the provider NOT RUN and stop provider-specific work.
- [ ] T061 [US6] Only after T060 permits the selected backend and actual SDK contract, implement the authorized provider adapter outside the default dependency path in `src/renderer/provider/provider_adapter.cpp` and its create/evaluate/reset test in `tests/renderer/provider_integration_test.cpp`.
- [ ] T062 [US6] Record actual selected-backend inputs, masks, reset, on/off evidence and the independently reviewed distribution decision in `specs/004-quality-renderer/evidence-q5.md`; an unrelated API sample or dynamic loading alone is insufficient.

## Phase 9 — US7: Path-traced reference/showcase (P3; Q6)

**Goal**: Deliver the requested long-term path-traced reference and, only after motion/performance gates, a playable showcase with a non-RT fallback.

**Independent test**: Capture static convergence and a dynamic/skinned scene on named capable hardware; review material handling, noise, denoising, responsiveness and the fallback.

- [ ] T063 [P] [US7] Define scene/configuration identities, convergence/noise/latency measurements and pre-review thresholds in `specs/004-quality-renderer/path-tracing-rubric.md`.
- [ ] T064 [US7] Add capability/unavailable-device and non-RT fallback tests in `tests/renderer/raytracing_capability_test.cpp`; add a failed visual/performance-gate selection test that reports the limitation and keeps the non-path-traced renderer available, without inventing an automatic switch. Implement explicit selection in `src/renderer/d3d12/raytracing_capability.cpp`.
- [ ] T065 [US7] Add static geometry and independently checked convergence cases in `tests/renderer/pathtrace_static_test.cpp`, then implement the reference integrator in `src/renderer/d3d12/pathtrace.cpp`.
- [ ] T066 [US7] Add moving rigid/skinned acceleration-structure update tests in `tests/renderer/pathtrace_dynamic_test.cpp` and implement safe updates in `src/renderer/d3d12/raytracing_scene.cpp`.
- [ ] T067 [US7] Add alpha/transmission and emissive-transport tests in `tests/renderer/pathtrace_material_test.cpp` and implement the tested material behavior in `src/renderer/d3d12/pathtrace_material.cpp`.
- [ ] T068 [US7] Add denoising, history-rejection and motion/noise tests in `tests/renderer/pathtrace_denoise_test.cpp`, then implement the documented denoiser path in `src/renderer/d3d12/pathtrace_denoise.cpp`.
- [ ] T069 [US7] Record static convergence, dynamic/skinned captures, material limitations, frame pacing and input latency in `specs/004-quality-renderer/evidence-q6.md`; unavailable RT hardware is SKIPPED and preserves the fallback.
- [ ] T070 [US7] Independently review Q6 against T063 and art direction in `specs/004-quality-renderer/review-q6.md`; a static image or advertised DXR tier alone is not a playable showcase.

## Phase 10 — US8: Independently validated future profiles (P3; Q7)

**Goal**: Preserve portable boundaries without claiming Windows implementation implies a browser, mobile or other native port.

**Independent test**: For each claimed target, publish its own capabilities/fallbacks and run the target's shader, build and real-scene tests.

- [ ] T071 [P] [US8] Define per-target capability, explicit fallback and unsupported-result requirements in `specs/004-quality-renderer/portable-profile-contract.md`, without cancelling the existing browser roadmap.
- [ ] T072 [US8] Add portable RHI capability/fallback contract fixtures in `tests/renderer/portable_profile_test.cpp` and implement profile-neutral selection in `src/renderer/rhi/capability_profile.cpp`.
- [ ] T073 [US8] For each separately scoped target actually chosen, implement its adapter and target-specific build/shader tests in `src/renderer/profiles/profile_backend.cpp` and `tests/renderer/profile_target_test.cpp`; do not infer the target from a Windows pass.
- [ ] T074 [US8] Run a real scene in each claimed target and record target version, capability/fallback table and evidence in `specs/004-quality-renderer/evidence-q7.md`; a desktop compile or shader translation is not a web-port pass.
- [ ] T075 [US8] Independently review each claimed profile and all unsupported cases in `specs/004-quality-renderer/review-q7.md`.

## Phase 11 — Cross-cutting completion

- [ ] T076 [P] Reconcile stage statuses and user-visible documentation with executed evidence in `docs/ROADMAP.md`; do not claim Q1, game parity, DXR, DLSS or web support from Q1a.
- [ ] T077 [P] Audit tracked paths and provenance against `specs/004-quality-renderer/evidence-template.md`, excluding game data, derived packs, binaries, SDKs, secrets and machine-specific absolute paths.
- [ ] T078 Independently check spec/plan/tasks/evidence consistency and all skipped versus passing outcomes in `specs/004-quality-renderer/review-programme.md`.
- [ ] T079 Re-run applicable documented commands from `specs/004-quality-renderer/quickstart.md` in each stage's own worktree and record fresh exits, test counts, adapter class and submodule status in that stage's evidence file.
- [ ] T087 Maintain a literal traceability matrix in `specs/004-quality-renderer/traceability.md` mapping every FR-001–FR-019 and SC-001–SC-008 to its primary tasks, artifact and evidence location; label inferred or partial coverage explicitly instead of implying full coverage.

## Dependencies and execution order

- T001–T005 establish the issue boundary and public/evidence contracts. The independent Q0 work T006–T011 does not gate Q1a T012–T021; Q0 and Q1a require separate issues, worktrees and file owners if both run concurrently.
- T012–T021 produce only Q1a. Q1b T022–T030 grows from it; T031 is full Q1 and requires real debug-layer plus GBV execution. A missing layer or all-skipped CTest run cannot close that gate.
- Q2 T032–T040 requires Q1 capabilities, a safe Q0 comparison baseline, legal data and reviewed engine-fork integration. T080–T082 supply the Q2 production slices; T083 is the integration check, behind T037's tests and requiring T035–T037. T084 guards the repository patch-delivery scripts and follows T033; it needs no GPU run. Q3 T041–T048 needs the Q2 scene adapter; T085 requires T042, T044 and T046. Q4 T049–T057 needs Q2/Q3 and legal authored content; T086 follows T047, T055 and T062.
- Q5 T058–T062 is optional and separately gated. The off path must work regardless; provider-specific execution cannot start until actual access, target hardware and applicable legal clearance are recorded.
- Q6 T063–T070 is the long-term DXR destination, not a blocker for Q4. It depends on the appropriate Q2/Q3 scene and history foundation, while its artistic and playable-showcase acceptance remains separate from the hybrid renderer.
- Q7 T071–T075 is independently implemented and validated per target. Do not claim support for targets whose real-scene checks did not execute.
- Each future issue follows its own branch/worktree/review process. A task's proposed paths may be revised in that issue's plan without relaxing the contracts. Shared public files are stabilized before writers are split; no two agents write the same file or share a build directory. If a stage's files would need parallel writers, split it into separate issues and worktrees instead of sharing one.
- T087 tracks coverage only; it never substitutes for the evidence named in its rows.

## Parallel opportunities and delivery strategy

- After the shared contracts, T006/T007 can be assigned to different file owners while a separate issue/worktree implements Q1a T012–T021. Q1a runs under one writer in one worktree; split it into separate issues/worktrees if its files would otherwise have parallel writers. Never run two agents on one branch or one worktree.
- Once their prerequisites exist, T032/T033, T041's analytic fixture design, T049/T050, T058 and T063 may be prepared in parallel in separate issue/worktree scopes. GPU execution and code integration remain ordered where tests depend on completed production seams.
- First deliverable: **Q1a only** — one real synthetic clear/readback plus meaningful fence-lifetime proof, reviewed with exact hardware/software and validation results. It is neither a complete User Story 2 nor a playable renderer.
- Then finish Q1b/Q1 and the baseline, followed by independently gated Q2 → Q3 → Q4; pursue Q5/Q6/Q7 only on their own prerequisites. Do not automatically publish an issue, commit, push or PR merely because a task exists.
