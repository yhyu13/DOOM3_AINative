# Q2 independent review (T040)

Status: independent review, 2026-11-03. This is the T040 artifact. It is not Q2 acceptance.

| Item | Value |
|---|---|
| Reviewer | fresh Agent Manager session `ses_e839e13ecffer9zdg7lJkG6RP9`, own worktree; not the author of Q2 |
| Reviewed head | `f9aac61991b220795e8fba2ab3f16ff80d72075c` (= `origin/main` at review time) |
| Base pin | `doom3_orig/doom3` at `c87e87b4ab806e8543fc652bc33cd57df1c0a3e5`; `git -C doom3_orig/doom3 status --porcelain` empty |
| Reviewed PRs | #134 (T080–T082 superproject), #141 (fork bump), #145 (device consumer), #144 (T036–T038 residual tests); fork `yhyu13/DOOM-3` PR #1 and #2 merged, PR #3 open |
| Scope | Q2 behaviour and delivery evidence: adapter, material/light semantics, draw/texture/stencil device consumer, GUI split, failed-init, portals/subviews, parity limits, fork/submodule delivery |
| Out of scope | Spec 008 shader programs, Q4–Q7, GL reference acceptance, and any retail-data capture (none is possible here) |

## 1. Verdict

Q2 is *BLOCKED*. Q2 is not accepted, and nothing in this review should be read as a Q2
acceptance, a classic-parity result or a rendered engine frame.

The delivery record is materially better than the earlier ledger: the submodule bump is real and
reproducible, the target builds, the Q2 fixtures pass in both configurations, and the evidence says
`NOT RUN` where it should. Three independent blocker classes remain.

1. *Data block (expected).* T039's image parity and T083's forced-failure launch are blocked by the
   absence of authorized retail/demo data (#105). This review confirms that every CP-01–CP-30 image
   row is `NOT RUN`/`SKIPPED`, not parity.
2. *The modern path cannot initialize in this build, with or without data.* `r_renderApi 1` stops at
   `RA_STAGE_SURFACE` because no D3D12 window/surface path exists and `glConfig.vidWidth` is not
   populated before `R_InitOpenGL` (which the D3D12 branch never calls). The installed scene
   consumer is never reached. T083 is therefore not
   only data-blocked; the failure it would observe is an earlier stage than the one the Q2 evidence
   describes.
3. *The device consumer does not produce a composed classic frame.* Target/copy commands are
   recorded but never executed, surface and light passes write separate attachments with no
   composite, the material plan and program gate are not on the engine path, multi-view/overlay
   views are concatenated into one draw list, and the pipelines disable blending. The synthetic
   frames prove device operations, not a classic frame.

The review artifact required by T040 now exists at this SHA. Whether the T040 checkbox is ticked is
the ledger owner's decision; ticking it must not be read as Q2 acceptance.

## 2. Evidence rerun in this worktree

All commands were run from the reviewed head. Logs are worktree-local and git-ignored
(`build-review-q2/`, `build/`). The worktree was clean before and after.

| # | Command | Exit | Observed |
|---|---|---:|---|
| 1 | `cmake -S tests/renderer -B build-review-q2 -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 |
| 2 | `cmake --build build-review-q2 --config Release -- -m` | 0 | all renderer test executables built |
| 3 | `ctest --test-dir build-review-q2 -C Release -L q2 --output-on-failure` | 0 | *12/12 passed* |
| 4 | `cmake --build build-review-q2 --config Debug -- -m` | 0 | all Debug executables built |
| 5 | `ctest --test-dir build-review-q2 -C Debug -L q2 --output-on-failure` | 0 | *12/12 passed* |
| 6 | `ctest --test-dir build-review-q2 -C Release --output-on-failure` | 0 | *49/49 passed* |
| 7 | `cmake --preset win11-amd64` | 0 | `new-target: exit 0`, `apply-target-patches: exit 0` |
| 8 | `cmake --build --preset win11-amd64-release -- -m` | 0 | `DOOM3.exe` 17,626,112 B and `DOOM3XP.exe` 17,809,920 B produced |
| 9 | `dumpbin /dependents build/bin/Release/DOOM3.exe` | 0 | no `d3d12.dll` and no `dxgi.dll` import |
| 10 | `dumpbin /symbols build/targets/win11-amd64/Release/doom3_rhi.lib` | 0 | `EngineQ2DeviceConsumer`, `EngineQ2DeviceBackend` and `EngineQ2DeviceTextureCache` defined |
| 11 | `py -3 -I scripts/validate-rhi-contract.py` | 1 | one failure: `src/renderer/dxr/q6_motion_texture.h` names `D3D12` |
| 12 | `py -3 -I scripts/validate-webgpu-mapping.py` | 1 | `INVALID: 76 problem(s); 119/195 declarations mapped` |

Direct runs of the four residual executables printed the same claims recorded in
`evidence-q2.md` §14: `T036 PASS`, `T037 PASS (structural/classification only)`, `T038 PASS
(structure only)`, and `T080–T082` device cases `SUMMARY pass=2 fail=0 skip=0` on the hardware
adapter and WARP. The four committed PNGs are consistent with the asserted 8×8 synthetic readbacks
(for example `q2-device-shadow.png` is red with a black centre column; `q2-device-stream.png` is
green-left over red-right).

Command 8 differs from `evidence-q2.md` §13.2 on `DOOM3XP.exe` by 512 B (17,809,920 vs 17,810,432).
That is a build-reproducibility note, not a behavioural finding; `DOOM3.exe` matches exactly.

## 3. What this review verifies as delivered

- The submodule pin is exactly `c87e87b`, the fork `master` tip after fork PR #2, and the submodule
  worktree is clean. The target patch applies on a fresh copy and the Release target builds with
  zero `error C`/`LNK`/`MSB` lines. PR #141 carries an independent `APPROVED` at its head
  (`6a3e4c9`, equal to the PR head at merge) and an `owner-sign-off:` line; on the record, the bump
  process is satisfied. The shared-account caveat from `AGENTS.md` applies and is not repeated as
  proof of human typing.
- `SceneCommandQueue`/`SceneAdapter` implement the FIFO, the mid-frame `CaptureToFile` split and the
  validation that refuses an unknown selector, a zero copy identity, or an `EndView` without a
  `BeginView` (`src/renderer/rhi/engine_scene_adapter.cpp:104-197,266-332`).
- `PlanMaterial` implements the canonical bump → diffuse → specular order, coverage classes,
  megatexture refusal and a fail-loud resolver refusal (`src/renderer/rhi/materials.cpp:78-180`).
  `PlanLight` implements ambient/interaction/fog/blend classification, the global/local chain plan
  and the three suppressions (`src/renderer/d3d12/lighting.cpp:50-172`).
- `Device::CreateBuffer`, `Device::CreateTexture`, stencil PSOs and per-item stencil reference are
  exercised by real device submissions and readbacks; the texture cache uploads/replaces images and
  refuses a declared map without an upload; a shadow-volume packet is attached to its light and
  submitted as back/front stencil plus shade. These are genuine device operations, not CPU-only
  plans, and the negative controls in `evidence-q2.md` §13.3 reproduce.
- The display-overlay-after-scene order is enforced on the engine path by
  `FrameIntegration::ExecuteFrame` (`src/renderer/rhi/frame_integration.cpp:207-232`), not only by a
  test comparator. The focused `CopyToImage` ordering case in `classic_gui_test.cpp` and its
  negative control pass.
- T036's portable failed-init contract is real: a consumer asked for real submission with no backend
  or no surface pipeline refuses with an actionable reason, does not rewrite itself into
  structural-only mode, and the structural path still initializes and executes.
- The evidence is honest about its largest limits: no live engine frame, no retail data, no parity
  image, no fork-emitted shadow volume, no full multi-stage material, and Q2 not accepted.

## 4. Blocking findings

### B-1. `r_renderApi 1` cannot start: the D3D12 window/surface path is absent

Severity: BLOCKER (Q2 goal "selectable and playable").

Evidence: the fork's D3D12 branch requires a non-zero `glConfig.vidWidth` before it will touch the
sink (`doom3_orig/doom3/neo/renderer/RenderSystem_init.cpp:2014-2018`), but `glConfig.vidWidth` is
first populated by `R_GetModeInfo` inside `R_InitOpenGL`
(`doom3_orig/doom3/neo/renderer/RenderSystem_init.cpp:644`); before that it is zero. The D3D12
branch never calls `GLimp_Init`/`R_InitOpenGL`, so on a fresh launch the value stays zero and every
`r_renderApi 1` launch
fails at `RA_STAGE_SURFACE` with "no window or surface exists" and the installed consumer's
`initialize` is never called. The fork's own message states that no D3D12 surface path is delivered.

The sink is in fact installed: `src/renderer/rhi/engine_q2_sink_install.cpp:297-301` constructs a
static installer, and `targets/win11-amd64/CMakeLists.txt:136-137` compiles that unit into
`doom3_engine`, so it is linked into both executables. But the installed `g_consumer` is a default
`EngineQ2SinkConsumer` in an anonymous namespace with no way to configure a backend, and
`EngineQ2DeviceConsumer` has no call site outside its own file and the tests
(`git grep EngineQ2DeviceConsumer -- src`). The device consumer is dead code in the engine.

Effect: T083 is not only data-blocked. Even with legal data, the Q2 path cannot initialize; the
typed failure T036/T083 would observe is `RA_STAGE_SURFACE`, not the consumer/device failure the Q2
evidence discusses. The "reference path remains selectable and usable" requirement is met only as a
*subsequent launch with `r_renderApi 0`*, which `engine-integration.md` §5.3 allows, but that is not
the same as a modern path that can run.

Unblock: a reviewed D3D12 window/surface path (device, swap chain, resize/minimize, present) and a
call site that constructs `EngineQ2DeviceConsumer` with a Device and the four pipelines, then
installs that consumer through `R_SetQ2SceneSink`; then T083.

### B-2. No composed classic frame: `SetColorTarget`, `CopyToImage` and `SwapBuffers` are recorded but never executed

Severity: BLOCKER (Q2 "classic path" and T035's mid-frame capture condition).

Evidence: `SceneAdapter::Execute` handles `BeginView`, `DrawPacket` and `LightPacket`; every other
kind, including `SetColorTarget`, `CopyToImage` and `SwapBuffers`, reaches `default: break`
(`src/renderer/rhi/engine_scene_adapter.cpp:407-572`, `:570-571`). The adapter therefore never
changes the target, never copies the frame into the capture image, and never presents. The tests
that "pass" the capture cases assert only that the command kind is present in `result.executed`.

The surface flush clears its attachment and submits one pass
(`engine_scene_adapter.cpp:369-402`). The light frame submits three passes: two stencil-only and a
shade pass that *clears* `lightAttachmentIndex` and writes there
(`engine_scene_adapter.cpp:523-568`). `rhi.h` states that each attachment is written by exactly one
pass (`src/renderer/rhi/rhi.h:311-315`), and there is no composite step anywhere in `src/renderer`
(`git grep -i composite` finds only Q4 harness code). The device fixtures set
`surfaceAttachmentIndex = lightAttachmentIndex = 0`
(`tests/renderer/q2_device_consumer_test.cpp:100-101`), so the light pass clears the surface result.
When the indices differ, the two attachments are never combined.

`rhi_pipeline.cpp:275` sets `BlendEnable = FALSE` for every render target, so there is no additive
light accumulation and no translucent blend either. Each light's shade pass overwrites the previous
light.

Effect: the port has no frame image that a screenshot, a `_currentRender` post-process copy, or a
HUD can consume. The committed `q2-t082-scene-surfaces.png` and `q2-t082-scene-light.png` are two
separate readbacks, not one composed frame. T034's ordering tests remain valid as ordering tests;
T035's "preserve mid-frame flush/capture behavior" is not delivered as a copy.

Unblock: execute `SetColorTarget` as a real target switch, implement `CopyToImage` against a
device copy, and add a documented composite (or a single accumulating target with blending) before
any frame/parity claim.

### B-3. The material plan and program gate are not on the engine path

Severity: BLOCKER (T081 classic material semantics).

Evidence: `EngineQ2SinkConsumer::DrawPacket` takes `material.numStages` as the stage count and calls
`BindSurfaceTexture`, which calls only `SelectStageTextureForDraw`
(`src/renderer/rhi/engine_q2_sink.cpp:299-339`, `:224-262`). It never calls `PlanMaterial` and never
consults a `ProgramGate`. `PlanMaterial` and `BindMaterialTextures` are referenced only from
`tests/renderer/classic_compatibility_test.cpp` and `q2_geometry_texture_stencil_test.cpp`
(`git grep PlanMaterial -- src tests`). The "one refused stage refuses the material" resolver gate
that `evidence-q2.md` §3 and B2 describe therefore does not protect the engine consumer. Once the
path runs (B-1), a classic material with no owned program would be queued and drawn.

Further, the portable `DrawItem` carries one `Texture` and one tint
(`src/renderer/rhi/rhi.h:294-309`); the adapter binds the same `surfacePipeline` for every draw
regardless of `alphaTested`/`translucent` (`engine_scene_adapter.cpp:379-385`); and blending is
disabled globally (`rhi_pipeline.cpp:275`). Multi-stage bump/diffuse/specular evaluation,
alpha-tested pipelines and translucent blending are not implemented. `evidence-q2.md` §13.4
discloses "one texture per draw" and "not full multi-stage", but not that the program gate is
bypassed.

Effect: T081's production half is a single-texture draw, not classic stage evaluation. CP-03/CP-10
are classification-only, and the manifest's 0 `owned` programs is not enforced at the engine
boundary.

Unblock: carry a `MaterialPlan` (or its program ids and stage order) through the sink, call
`PlanMaterial` with the spec-008 gate in the engine call site, and add alpha-test/translucent
pipeline selection and a real multi-stage or explicitly documented single-stage contract.

### B-4. Multi-view/portal/subview/overlay streams are concatenated into one surface draw

Severity: BLOCKER (T037 portals/subviews, T038 GUI split).

Evidence: `BeginView`/`EndView` only increment counters and record view identities
(`engine_scene_adapter.cpp:404-417`); draw items accumulate in `pendingItems` and are flushed only
at a light packet or the end of execution
(`engine_scene_adapter.cpp:367,439,446,575-581`). The submitted frame always uses
`desc_.displayWidth/Height` (`:387-391`); the view's width, height, x and y are not applied —
`EngineQ2SinkConsumer::BeginView` even discards x/y (`engine_q2_sink.cpp:264-267`). There is no
per-view target or viewport in `DrawFrameDesc`. `overlay` and `worldGui` affect only the
post-reconstruction *plan* in `FrameIntegration`, not the adapter's submission.

The residual CP-14/CP-15/CP-16 cases are hand-built queues: `classic_compatibility_test.cpp` creates
the mirror/xray/main views itself and asserts the adapter preserves them
(`tests/renderer/classic_compatibility_test.cpp:544-568`). They are adapter-level classification and
order tests, not engine-produced traces. `evidence-q2.md` §6 defines "Structural (T-07)" as "exact
equality of an engine-produced command/stage/chain trace"; calling these cases structural conflicts
with that definition.

Effect: at the device level a mirror, portal, xray view and the display HUD all draw into the same
full-display surface list with one clear and no viewport; the GUI split exists in the planner, not
in the renderer. T037's test-deliverable tick is defensible as test coverage, but it must not be
read as subview rendering.

Unblock: submit per-view targets/viewports (or a documented single-target layered plan), then an
engine-produced trace that names CP-14/15/16; only after that re-label the rows as structural.

### B-5. Light semantics do not match the classic interaction model

Severity: BLOCKER (CP-04/05/07/09/10).

Evidence, each point is code-level:

- The sink maps a `parallel` light to `LightKind::Projected` and a `pointLight` to `Point`
  (`engine_q2_sink.cpp:45-59`). `PlanLight` then requires a projection cookie and a positive falloff
  radius for every `Projected` light (`lighting.cpp:91-106`). The fork sets `hasProjection` from
  `(target - origin)` and `falloffRadius` from `lightRadius`
  (`doom3_orig/doom3/neo/renderer/tr_backend.cpp:804-808`). A parallel light without a target pair
  is therefore refused as `InvalidArgument`, and a spot light with `pointLight == true` is mapped to
  `Point`, never to `Projected`.
- The fork counts `localInteractions`, `globalInteractions` and `translucentInteractions`
  (`tr_backend.cpp:810-814`) but emits packets only for `globalShadows` and `localShadows`
  (`:827-861`). The interaction surface lists are never sent. The adapter's shade pass is one flat
  fullscreen tint (`SceneLightGeometry::tint`), not per-surface interaction shading.
- A `noShadows` light with lit surfaces emits one excluded packet; `PlanShadowVolume` disables the
  plan and the adapter counts `lightsExcluded` with no shade pass
  (`engine_scene_adapter.cpp:475-480`). CP-09 requires "no shadow, lighting still present"; the
  present path produces no lighting. `evidence-q2.md` §12.3 limit 5 discloses the missing
  shade-only pass, but the CP-09 row is still written as a suppression PASS.
- With blending disabled, multiple lights overwrite rather than accumulate (B-2).

Effect: the light plan is a useful classifier, but the rendered light path is a synthetic
stencil-pattern + flat tint, not the classic interaction. CP-04/CP-05/CP-07/CP-09/CP-10 cannot be
parity rows yet, and the light-kind mapping is likely inverted between parallel and projected/spot.

Unblock: map engine light facts to the classic classification with an engine-truth test (parallel,
spot/projected, point, ambient), emit the interaction lists, draw per-surface interactions, and add
accumulating blending; a shade-only path for `noShadows` lights is required for CP-09.

### B-6. Retail parity and T083 are data-blocked (accepted as a block, not as a pass)

Severity: BLOCKER (T039, T083), explicitly allowed to remain blocked.

Evidence: no `.pk4`/`.pk3`/`.pak` exists in this worktree; issue #105 records the data block. `evidence-q2.md` §6 has a row for every CP-01–CP-30 case, and every image column is
`NOT RUN`/`SKIPPED`. No GL-vs-D3D12 comparison, HUD metric, gameplay sequence or retail scene was
captured. The classification PASS entries are CPU checks and are correctly labelled as such.

Effect: Q2 cannot satisfy its independent test ("compare the same named legal-data scenes and
gameplay sequence on both paths"). This block is not a reason to accept Q2; it is a reason to keep
it unaccepted.

Unblock: an authorized retail/demo install under an untracked data directory, then the T032 matrix
captured on both paths with predeclared tolerances. T083 additionally needs B-1 resolved first.

### B-7. T038's fork change is not in the reviewed tree

Severity: BLOCKER for T038's row; not a defect in the merged pin.

Evidence: the pin is `c87e87b`. The pin already marks the space's GUI surface in
`neo/renderer/tr_light.cpp:1303`, and `tr_backend.cpp` derives per-packet `worldGui`. The change that
marks the quads emitted by `R_RenderGuiSurf` lives on fork branch `spec-004-q2-q3-guisurf` (fork PR
#3, commit `54d26a6`), which is *OPEN* (`gh pr view 3 --repo yhyu13/DOOM-3`). It was not compiled
here: building from the modified submodule would invalidate the patch result hash tied to `c87e87b`.
`evidence-q2.md` §14.3 says this correctly.

Effect: the GUI-quad half of T038 is fork-only, unreviewed and not delivered by the pin. T038 stays
partial; any bump would also need owner sign-off and a coordinated patch re-export.

Unblock: independent review of fork PR #3, merge it, re-export the patch, bump the submodule with
owner sign-off, rebuild, and add a structural case that uses an engine-emitted `worldGui` packet.

### B-8. The WebGPU mapping contract gate is red on the reviewed head

Severity: BLOCKER for delivery evidence; the regression enters with the Q2 public RHI additions and
no later Q2 PR reran the gate.

Evidence: `py -3 -I scripts/validate-webgpu-mapping.py` exits 1 with
`INVALID: 76 problem(s); 119/195 declarations mapped`, listing `Buffer`, `BufferDesc`, `Texture`,
`TextureMip`, `Vertex` and their fields as unmapped. `evidence-q2.md` §4.5 command 33 records this
gate as exit 0 with *119/119* declarations classified. That record predates the public RHI type
additions; #134 is the last commit that touched `src/renderer/rhi/rhi.h`
(`git log -1 --oneline -- src/renderer/rhi/rhi.h`). The Q2 evidence does not rerun this gate after
the additions, and §13.2/§14.4 mention only the `validate-rhi-contract.py` failure. The two
validators are different gates; a green `validate-rhi-contract.py` run would not excuse the red
mapping gate.

`py -3 -I scripts/validate-rhi-contract.py` also exits 1 on the pre-existing
`src/renderer/dxr/q6_motion_texture.h` `D3D12` vocabulary, as the earlier reviews disclosed.

Effect: a required contract gate is red at the reviewed SHA, and the Q2 evidence contains a claim
that no longer reproduces. The 119/119 result that closed T089 is no longer the state.

Unblock: map every new public declaration in the WebGPU mapping document (or move the Q2-only
helpers out of the public header), rerun both validators, and record the exits in the Q2 evidence.
Fixing the Q6 header is Q6 scope and should be its own change.

### B-9. The engine consumer retains every surface forever

Severity: MAJOR (engine integration; latent because no call site exists).

Evidence: `EngineQ2SinkConsumer::SwapBuffers` clears the queue and `volumes_` but not `surfaces_` or
`imageUploads_` (`engine_q2_sink.cpp:426-464`); `Initialize` clears them only once
(`:99-114`). The fork assigns a new `surfaceId` per draw per frame (`++r_q2SurfaceSerial` in
`tr_backend.cpp:750`), so the map grows by every draw surface of every frame. The header says
retaining every copy is "wrong for a long-running engine frame loop unless the caller opted in"
(`engine_q2_sink.h:198-200`), but `EngineQ2DeviceConsumer::Initialize` sets
`captureGeometry = true` unconditionally (`engine_q2_device_consumer.cpp:367`).

Effect: a live engine frame loop would leak the vertex/index snapshot of every surface of every
frame. This must be fixed before B-1's call site is written.

Unblock: clear per-frame surface/image retention at `SwapBuffers` (with any needed keyframe/lifetime
rule) or stream geometry to uploads without retaining the CPU copy.

### B-10. Light identities can collide across views

Severity: MAJOR (latent; carried from the #145 review as note 6).

Evidence: `BeginView` resets `lightSerial_` to 0 (`engine_q2_sink.cpp:291`), while `volumes_` is
keyed by the assigned `lightId` and is cleared only at `SwapBuffers` (`:346,441`). A frame with two
views and volume packets in both assigns light id 1 in each view; `ResolveLight` then returns the
first view's volume for the second view.

Effect: wrong shadow geometry once a fork call site emits shadow-volume packets.

Unblock: make the light key `(viewIdentity, perViewSerial)` or clear `volumes_` at `BeginView` and
key the volume packet by view.

## 5. Area disposition

| Area | Disposition | Basis |
|---|---|---|
| Ordered adapter / FIFO / validation | Delivered at the portable level | `engine_scene_adapter.cpp:104-332`; tests pass |
| Mid-frame capture | Recorded order only; no copy executes | B-2 |
| Material stage plan / resolver gate | Plan exists; not wired to the engine consumer | B-3 |
| Draw submission / texture upload | Device operations delivered and read back synthetically | `q2_device_consumer_test`; §2 commands 3/5 |
| Stencil shadow volume | Synthetic CP-07 pass; no fork volume, no rendered interaction | B-5, `evidence-q2.md` §12.3/§13.4 |
| Light classification | Plan exists; light-kind mapping and interaction emission incomplete | B-5 |
| GUI split | Order plan enforced; device separation absent; fork quads not in pin | B-4, B-7 |
| Failed modern init | Portable contract passes; engine path fails earlier than the consumer | B-1 |
| Portals / subviews | Classification/order tests only; no rendered subview | B-4 |
| Parity limits | All image rows `NOT RUN`; data-blocked | B-6 |
| Fork/submodule delivery | Bump process satisfied on the record; PR #3 open | §3, B-7 |
| Contract gates | `validate-rhi-contract` red (pre-existing Q6); `validate-webgpu-mapping` red (Q2 types) | B-8 |

## 6. T040 condition matrix

| T040 condition | Status | Evidence |
|---|---|---|
| A non-author reviewer, separate session/worktree, records the reviewed SHA | MET | this file; session `ses_e839e13ecffer9zdg7lJkG6RP9`; SHA `f9aac61` |
| Review covers adapter, material/light semantics, draw/texture/stencil device consumer, GUI split, failed-init, portals/subviews, parity limits | MET | §4–§5 |
| Submodule bump delivered, patch applies, target builds, fork PR #2 merged, owner sign-off and independent review recorded | MET on the record | §2 commands 7–8; PR #141 approvals; shared-account caveat |
| No scratch-copy-only change counts as delivered | MET for merged code; NOT MET for the T038 fork change | B-7 |
| The device consumer produces a classic frame | NOT MET | B-2, B-3, B-4 |
| The modern path is selectable and can run | NOT MET | B-1 |
| Retail parity / T039 image evidence | NOT MET (data-blocked) | B-6 |
| Engine failed-init end-to-end (T083) | NOT MET | B-1, B-6 |
| Required contract gates green at the reviewed SHA | NOT MET | B-8 |
| Q2 accepted | NOT MET | verdict |

## 7. Unblock order

The dependencies are strict; doing 4–7 before 1–3 would produce more synthetic evidence without a
frame.

1. Add the D3D12 window/surface path and a real `EngineQ2DeviceConsumer` installer call site (B-1).
2. Fix frame composition and execute target/copy/present, plus blending/accumulation (B-2).
3. Wire the material plan and program gate into the sink; add alpha/translucent pipelines (B-3).
4. Submit per-view targets/viewports for portal/mirror/xray/overlay (B-4).
5. Fix light-kind mapping, emit interaction lists, add the `noShadows` shade-only path (B-5).
6. Clear per-frame surface retention and key volumes per view (B-9, B-10).
7. Review and merge fork PR #3, re-export the patch and bump (B-7).
8. Map the new public declarations and rerun both contract gates (B-8).
9. Provision authorized data, run T039 parity and T083 (B-6), then request a fresh T040 review. Any
   commit after this review voids it for the next verdict.

## 8. Non-claims and limits of this review

- I did not run the engine with retail data; none exists here. I therefore claim no rendered engine
  frame, no gameplay sequence, no HUD metric and no parity.
- I reran the renderer suites in Release and Debug (Q2 label 12/12) and the full Release suite
  (49/49), the target configure and Release build, `dumpbin` checks and both validators. I did not
  rerun the full Debug suite.
- I did not modify code, images, tests, prior reviews or the ledger. This review writes only
  `review-q2.md`.
- The prior PR reviews (#134/#145/#144/#141) are author-adjacent independent records; this review is
  the separate T040 verdict. I did not treat any peer message or prior approval as authorization.
- The `/MT` executable claim, the x64 architecture and the absence of `VCRUNTIME140`/`MSVCP140` were
  not rechecked with `dumpbin /headers` in this pass; the build and dependents were.

## 9. Carried-forward review notes

The independent #145 review recorded six non-blocking notes. At this head:

- Stale comment path: still present. `src/renderer/rhi/engine_q2_sink_install.cpp:16-17` cites
  `rhi/d3d12/engine_q2_device_consumer.h` and `EngineQ2D3D12Consumer`; the real header is
  `src/renderer/rhi/engine_q2_sink_device.h` and the classes are `EngineQ2DeviceConsumer` /
  `EngineQ2DeviceBackend`.
- `renderer.q2.fork_seam` still carries only the `q2` label, so it no longer runs under `-L q3`
  unless `tests/renderer/CMakeLists.txt` restores `q3`.
- Texture binding is per surface rather than per draw; the fork assigns a unique surface id per
  draw, which masks it, but the portable contract is still per surface.
- The multi-view light-id collision is confirmed by inspection and promoted to B-10 because a
  volume packet now exists on the consumer side.
- The pre-existing validator failure is confirmed; the additional mapping-gate regression is B-8.
- The synthetic-frame scale limitation is unchanged and consistent with the evidence wording.

## 10. Documentation drift found while reviewing

| Location | Claim | Observed |
|---|---|---|
| `tasks.md:104` (T035) | "no sink consumer exists in `src/`" | `src/renderer/rhi/engine_q2_sink_install.cpp:298` installs one and `targets/win11-amd64/CMakeLists.txt:136-137` compiles it into `doom3_engine` |
| `ledger-q2-q3-reconcile.md:70` vs `:230` | T037 row says CP-14/15/16 absent, later addendum says covered | both rows remain in one file; the addendum is newer but the base row was not superseded |
| `evidence-q2.md` §8 (T083) | block attributed to "no legal game data" | the first failure is `RA_STAGE_SURFACE` because no D3D12 window/surface exists (B-1); data is a second block |
| `evidence-q2.md` §4.5 command 33 | `validate-webgpu-mapping.py` exit 0, 119/119 | exit 1, 76 problems, 119/195 at this head |
| `evidence-q2.md` §13.2 | `DOOM3XP.exe` 17,810,432 B | this build produced 17,809,920 B (512 B difference) |
| `evidence-q2.md` §6 CP-14/15/16 and §14.1 | called "structural" | the cases are test-authored command streams, not engine-produced traces per the §6 legend |

These are record-accuracy issues, not new behavioural claims. They should be corrected when the
corresponding blocker is fixed, not pre-emptively ticked.

---

Verdict: BLOCKED. Q2 is not accepted. The T040 review artifact is delivered at
`f9aac61991b220795e8fba2ab3f16ff80d72075c`.

*This output was AI-assisted and is a review record, not a professional certification.*
