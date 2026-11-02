# Q2 engine-adapter implementation brief (T034–T040, T080–T083)

**Status**: dispatch brief for issue [#53](https://github.com/yhyu13/DOOM3_AINative/issues/53), written
2026-10-29 and rechecked 2026-11-02 after PR #113. This is **not** a specification, an implementation,
a capture, or an acceptance record. **Q2 is not ready and not complete**: S2 resolver engine wiring
now lands (P1 closed), Q1 closed with a host-specific caveat (P3), but the five classic programs are
still `stub` (P2), T036's failed-initialization test is absent, T039 and T083 have no legal data, and
no classic frame has been drawn. The design contracts are
`classic-parity.md` (T032) and `engine-integration.md` (T033); this file decides only the order,
ownership, gates and stop conditions for executing them. Where this file and those contracts
disagree, the contracts win and this file is corrected.

**Baseline inspected**: `main` at `86e4eb3` on 2026-11-02, which contains the #113 merge
(`8815239`) and the T031 tick (#128). The 2026-10-29 snapshot was `4a2fd7c`; every task state and
prerequisite below was rechecked against `86e4eb3`.

---

## 1. What this brief adds

| Contract | Owns | This brief |
|---|---|---|
| `classic-parity.md` | parity meaning, scenes, CP-01…CP-30, T-01…T-10, capture/evidence protocol | points to it; does not restate thresholds |
| `engine-integration.md` | delivery model, both-executable linkage, `glConfig` dispositions, `r_renderApi`, `RC_*` dispatch | points to it; does not restate the CMake or dispatch design |
| `docs/GIT_WORKFLOW.md` | issue → spec PR → worktree → PR → review → merge | applies it; issue #53 is the implementation container |

Nothing here authorizes engine code. The implementation issue is open and blocked.

---

## 2. Prerequisites (snapshot 2026-10-29, rechecked 2026-11-02)

| # | Prerequisite | State | Evidence |
|---|---|---|---|
| P1 | **S2 resolver engine wiring** | **MET (2026-11-02).** `src/renderer/shader/engine/shader_engine.cpp` and the target `CMakeLists.txt` wiring are on `main` (PR #59, `2487baf`). The four console commands (`shader_status`, `shader_reload`, `reloadARBprograms`, `shader_selftest`) are in the target patch re-exported by #113, which configured and built. | `git log --oneline -- src/renderer/shader/engine`; PR #59; `targets/win11-amd64/patches/all-changes.patch` |
| P2 | **Five classic programs** | **PARTIAL.** `interaction`, `environment`, `bumpyenvironment`, `arbfp_glasswarp` and `shadow` are authored and compile to DXIL + SPIR-V + validated WGSL; the optional `test` program also compiles. The manifest (`shaders/manifest/classic-programs.json`) holds **14 entries**: 6 `stub` (`interaction`, `environment`, `bumpyenvironment`, `shadow`, `arbfp_glasswarp`, `test`), 1 `unsupported` (`megatexture`) and 7 `absent` (`ambientlight`, `arbvp_glasswarp`, `r200_interaction`, `nv20_bumpandlight`, `nv20_diffusecolor`, `nv20_specularcolor`, `nv20_diffuseandspecularcolor`). **None is `owned`**; the rendered comparison (T-027) is `BLOCKED` on authorised data and a running classic backend. A `stub` program must not be drawn as if it were owned. | `specs/008-classic-shaders/tasks.md` T-025/T-030; `ownership.md` §5 |
| P3 | **Q1** | **DONE (2026-11-02).** Q1b fixtures are merged (`f1e4feb`, 17/17 Release and Debug). T030 and T031 are both checked (`tasks.md`): T030 ran the debug layer and GPU-based validation on the Graphics Tools host (`debug=RUN`, `gpu_validation=RUN`, `errors=0`, unwaived messages 0); T031 is ticked after the skipped-layer fix. Caveat: T030's RUN evidence is host-specific; non-GDK hosts report SKIPPED (`0x887A002D`), so Q2 must not universalize Q1. | `specs/004-quality-renderer/evidence-q1.md`; `tasks.md` T030/T031 notes |
| P4 | **Q7a / T088** | **DONE.** Mapping gate 57/57, browser spike 12 required checks twice, Emscripten/wasm leg twice. The 2026-10-29 caveat (mapping gate exiting 1 with 62 problems) was cleared by #115; on 2026-11-02 `py -3 -I scripts/validate-webgpu-mapping.py` prints `OK: 119/119 public declarations classified` and exits 0. | `specs/004-quality-renderer/evidence-q7a.md`; `plan.md` Q7a row; `tasks.md` Q1 status note |
| P5 | **Q0** | **DONE and merged** (PR #31): `baseline-final13`, all four categories `PASS` twice under fixed-tic drives. The values are same-renderer repeatability, not cross-renderer parity. Host is a virtual display with an Intel adapter; not a discrete-GPU quality baseline. | `specs/004-quality-renderer/classic-parity.md` §2; `evidence-q0.md` |
| P6 | **T004 / T005** | **DONE and independently reviewed** (PR #27): three-tier RHI contract, header gate, evidence template. | `contracts/rhi-foundation.md`; `review-rhi-contract.md`; `evidence-template.md` |
| P7 | **T032 / T033** | **DONE as design only** (ticked after the Round 8 PASS in `review-q2-design.md` §3.6). No engine build, capture, or independent Q2 sign-off exists. | `classic-parity.md`; `engine-integration.md`; `review-q2-design.md` §4 |

**Dispatch rule.** P1 is closed; P2 now gates any adapter work that draws classic materials. P3 is
closed but its caveat still governs the Q2 validation wording, not the adapter compile: Q2 may build
and capture, but may never state that Q1 passed universally — T030's RUN evidence is host-specific
and non-GDK hosts report SKIPPED. No Q2 execution that draws classic
materials starts while P2 is unresolved unless the owner records a written re-scope in issue #53 —
a decision, not an inference.

**Coordination.** The S2 wiring is merged, so it no longer holds `targets/win11-amd64/CMakeLists.txt`
or `src/renderer/shader/engine/**` as an uncommitted writer. T036's linkage edit to the target
`CMakeLists.txt` is unblocked, but it stays one writer per file with any other active worktree.

---

## 3. File ownership (one writer per file)

Delivery classes are defined in `engine-integration.md` §3.1. This is the Q2 split.

| Work | Owner (write) | Must not touch | Delivery |
|---|---|---|---|
| T035 ordered scene adapter | superproject `src/renderer/rhi/engine_scene_adapter.cpp` | fork, `tri->ambientCache` as a pointer, GL enums | normal PR |
| T035 fork dispatch | `neo/renderer/RenderSystem.cpp` (command dispatch), `tr_backend.cpp` (`RB_ExecuteBackEndCommands` branch) | superproject adapter/RHI | fork branch → fork PR → submodule bump |
| T036 selection + fallback | fork `RenderSystem_init.cpp`, `RenderSystem.h`/`tr_local.h`, `r_renderApi`; superproject selector test | `RB_STD_DrawView` on the modern path | fork PR + superproject PR |
| T036 linkage | `targets/win11-amd64/CMakeLists.txt` (`doom3_rhi`, `RHI_ADAPTER_SOURCES`, `DOOM3_LINK_LIBS`), `sources.cmake` only if regenerated | engine source | build-system PR, independent reviewer |
| T037 compatibility fixtures/tests | `tests/renderer/classic_compatibility_test.cpp` + its fixture documents | production slices | normal PR |
| T038 GUI/overlay fixtures/tests | `tests/renderer/classic_gui_test.cpp`; fork `tr_guisurf.cpp`, `GuiModel.cpp` | scene adapter | normal PR + fork PR |
| T080 world draw submission | **same file as T035**: `src/renderer/rhi/engine_scene_adapter.cpp` + fork `neo/renderer/` | T035 while it is open | sequence, or split the file before dispatch |
| T081 material stages | `src/renderer/rhi/materials.cpp` + fork material/image code | resolver core (`src/renderer/shader/**`) | normal PR + fork PR |
| T082 lighting/shadows | `src/renderer/d3d12/lighting.cpp` + fork interaction code | scene adapter | normal PR + fork PR |
| T083 forced-failure check | `specs/004-quality-renderer/evidence-q2.md` | production code | evidence PR |
| T039 evidence | `specs/004-quality-renderer/evidence-q2.md` | another task's evidence file | evidence PR |
| T040 review | `specs/004-quality-renderer/review-q2.md` | implementation files | reviewer only, never the author |
| Submodule bump | `doom3_orig/doom3` pointer only | any other path | separate reviewed PR; fork URL + old/new SHA |

Hard rules from `engine-integration.md` §3–§4:

- The pristine submodule is read-only; engine work happens on a fork branch outside
  `doom3_orig/doom3`, then a bump. A target-copy-only change is not delivered.
- `tri->ambientCache` is a GL vertex-cache handle, not a portable buffer; `setBufferCommand_t.buffer`
  is a `GLenum` and `copyRenderCommand_t.image` is an `idImage*` — all three are translated at the
  adapter boundary, never passed into `doom3::rhi`.
- Game objects (`doom3_game_base`, `doom3_game_d3xp`) never link the RHI.
- Do not add the adapter to both `ENGINE_SOURCES` and `RHI_ADAPTER_SOURCES`.

---

## 4. Required order

The ordering is a dependency chain, not a preference. Fixtures are authored red before the code that
satisfies them.

1. **Close P2** (P1 closed 2026-11-02; the manifest state still needs agreement). Resolve P3 in
   writing. Nothing below starts on a second `CMakeLists.txt` writer.
2. **T034 — command-stream fixtures, red.** `tests/renderer/engine_command_order_test.cpp` covers
   view, set-buffer, copy and present order plus the mid-frame split:
   `CaptureRenderToFile` executes and clears the queue; `CaptureRenderToImage` appends a copy without
   flushing. It must fail against the current tree before T035 starts. Expected order is
   `classic-parity.md` §4 (O-01…O-14) and `engine-integration.md` §6.
3. **T035 — adapter, green.** Land `src/renderer/rhi/engine_scene_adapter.cpp` first; T034 turns green
   on the neutral stream. Then open the fork PR for the `RC_*` dispatch. The fork PR declares its
   dependency on the superproject header and cannot merge standalone.
4. **T036 — selection, fallback, linkage.** `r_renderApi` (`0` GL, `1` D3D12 classic), the
   `R_RenderSystemRunning()` predicate resolving all 30 `glConfig.isInitialized` sites by group, the
   modern `RB_EXECUTE` branch, and both-executable `/MT` linkage. The unit-level test proves a failed
   modern init returns an actionable result and leaves `r_renderApi` unchanged and the GL path usable;
   no automatic switch is invented.
5. **T037 — classic coverage fixtures, red then green.** Author `fixture-interaction`,
   `fixture-shadow`, `fixture-subview`, `fixture-post`, `fixture-effects`, `fixture-viewmodel` and
   `classic_compatibility_test.cpp` in that order, because each later fixture depends on the earlier
   material/light/shadow classification. Measure the GL-vs-GL ROI controls (T-06/T-08) **before**
   capture. Then land production slices **T080 (after T035)**, **T081**, **T082**; a test double is
   not production.
6. **T038 — GUI and overlay.** Author `hud-control` and `classic_gui_test.cpp`; world GUI stays in the
   3D view, display HUD is a separate 2D view at display resolution, added as `RC_DRAW_VIEW` after
   the scene. Measure T-05/T-09 here. Fork-side handling in `tr_guisurf.cpp` / `GuiModel.cpp` ships
   through its own fork PR.
7. **T083 — forced failure in a real build**, behind T035–T037. Record the typed error, the unchanged
   `r_renderApi`, the absence of an automatic switch, and a subsequent `r_renderApi 0` GL launch.
8. **T039 — evidence per CP row** in `evidence-q2.md`, both sides captured with the same method.
9. **T040 — independent review** in `review-q2.md`, reviewer not the author, commands rerun.

Why this order and not another:

- T034 before T035 because a fixture written after the adapter cannot prove it was ever red.
- T035 before T080 because they share one file; T080 without T035 is a second writer.
- T037 before T083 because T083 is the integration check of a path T037 has already exercised.
- T038 after T037 because the HUD control reuses the scene capture and the measured display size.
- T039 after every fixture; a row captured before a threshold is pre-registered is not evidence.
- T040 last, and it cannot be the author.

---

## 5. Evidence and parity gates

1. **Structural gate (T-07)**: exact equality of the recorded command/stage/chain sequence. A correct
   still with a reordered stream is a failure (`classic-parity.md` §6.2 rule 5).
2. **Image gates (T-01…T-04)**: fixed before capture — `open-view` 2.0/2.0, `close-character`
   0.5/0.5, `moving-light-combat` 3.0/5.0 only at the recorded fixed tic, `corridor` 0.5/0.5.
   `delta=8`, `meanAbs` and `diffPercent`, both limits.
3. **Pending controls**: T-05 (HUD region), T-06 (predeclared ROI), T-08 (character/alpha ROI) and
   T-09 (HUD text boxes/edges) are **not measured**. They must be measured by a GL-vs-GL control
   before any Q2 image is judged; a control may only tighten the gate.
4. **Every CP-01…CP-30 row** ends as `PASS`, `FAIL`, `SKIPPED` with a reason, `NOT RUN`, or
   `UNSUPPORTED` with a fallback and a reviewer decision. Structural and Image are recorded
   separately. A skipped or unsupported row is never parity.
5. **Same capture on both sides**: legal demo data, same drive, same camera, 1280×720, the engine's own
   framebuffer, twice for repeatability. An OS client-area screenshot is a different measurement and
   is not comparable. `g_stopTime 1` is not a determinism control.
6. **Identity fields**: executable hash, repository revision, branch, dirty paths, submodule SHA,
   target-patch SHA, data hash, adapter/driver, every cvar that changes the compared output,
   `r_renderApi`, command and exit code. Use `evidence-template.md`; the Q0 script overrides are not
   silently weakened.
7. **Negative control**: at least one deliberately wrong case (swapped frame order or wrong scene)
   must fail the gate. A gate that passes everything is not evidence.
8. **Q1 residual**: the debug-layer/GBV skip is disclosed in `evidence-q2.md`; it is not converted to
   a pass, and it is not a Q2 acceptance claim.
9. **Delivery gate**: `git submodule status` shows the fork SHA in the PR body, the pristine tree is
   clean, and no change exists only in the ignored target copy. T084's manifest controls are complete ([evidence-t084.md](./evidence-t084.md)); a green `SC-006b`
   is self-consistency, not canonical provenance.
10. **Fallback gate (T083)**: a failed modern init is actionable, does not poison `r_renderApi`, and
    the next `r_renderApi 0` launch renders GL. No automatic switch is claimed.
11. **Review gate (T040)**: an independent reviewer reruns the commands and records the decision and
    remaining unsupported cases. AI-assisted review is recorded as such; it is not human sign-off.

---

## 6. Stop conditions that force re-planning

Stop the issue and return to the coordinator — do not widen scope, lower a threshold, or claim
"mostly green" — when any of these occurs.

| # | Condition | Consequence | Required action |
|---|---|---|---|
| S1 | A `stub` program is drawn, or P2 is still unresolved when T035 lands | The classic path draws unowned programs; parity is meaningless | Stop; reconcile the manifest state, or record a written owner re-scope in #53 |
| S2 | A Q2 statement would imply Q1 passed universally, ignoring the host-specific SKIPPED result | False validation claim | Reword to the host-specific fact (T030 RUN on GDK; SKIPPED elsewhere); do not universalize Q1 |
| S3 | `validate-webgpu-mapping.py` still exits 1 at capture time and no waiver exists | The pre-Q2 header gate is red | Reconcile via #36/T089, or record an explicit reviewer waiver before capture |
| S4 | Q0 label, values, executable, tic, host or capture method changes | The frozen ceilings no longer describe the reference | Re-baseline Q0 before any comparison; do not reuse the old ceilings |
| S5 | A GL-vs-GL control exceeds its scene ceiling | The scene is not deterministic enough for image parity | Mark the row `SKIPPED` with the reason; do not widen the tolerance |
| S6 | A threshold, ROI or mask is chosen after seeing a new-path result | The capture was not pre-registered | Invalidate the capture; re-pre-register and recapture |
| S7 | T-05/T-06/T-08/T-09 are still unmeasured when HUD/ROI cases are judged | No valid HUD/ROI image gate | Stop those rows; measure the control first |
| S8 | The engine change cannot be delivered outside `targets/win11-amd64/doom3/` (new file only in the patch exporter, or fork depends on an unmerged header) | A scratch-copy-only change is not delivered | Stop; re-plan as fork PR + submodule bump, or merge the dependency first |
| S9 | Any of the 30 `glConfig.isInitialized` sites is left unclassified, or the modern path still calls `RB_STD_DrawView` | Fabricated state or a GL pass on the modern path | Stop; resolve every row by group |
| S10 | A `GLenum`, `idImage*`, `qgl*` call or `tri->ambientCache` pointer reaches `doom3::rhi` | Breaks the portable boundary and the header gate | Stop; translate at the boundary |
| S11 | Both executables cannot link under `/MT`/`/MTd` (`LNK2038`), or `doom3_rhi` is missing from one EXE | One executable lacks the backend | Stop; fix linkage before any capture |
| S12 | T035 and T080 would be edited concurrently, or two worktrees would write the target `CMakeLists.txt` | Two writers on one file | Split ownership or sequence; do not merge over it |
| S13 | Legal demo data is unavailable, or a fixture needs invented/retail content | Image parity cannot run | `SKIPPED` with reason; Q2 is not claimable |
| S14 | A case exceeds its gate and is not a listed unsupported case with a reviewer decision | Parity failure | `FAIL`; fix or re-plan, never "known difference" without a row |
| S15 | The GL path changes in a way that moves Q0, or the modern path introduces an automatic switch | The reference moved, or FR-006 is violated | Stop; revert the GL change / remove the switch |

---

## 7. References

- Design: `spec.md` (FR-003/006/007/018, SC-003), `plan.md` (Q2 gate, §92 contract gate),
  `tasks.md` T034–T040 and T080–T083, `contracts/rhi-foundation.md`.
- Parity: `classic-parity.md` §2 (Q0), §3 (scenes/fixtures), §4 (order), §5 (CP matrix), §6
  (thresholds), §7 (capture protocol), §8 (result record), §9 (Q2 conditions).
- Engine: `engine-integration.md` §5 (selection/fallback), §6 (command order), §8 (verification),
  §9 (ownership/order), §10 (risks); `targets/win11-amd64/README.md`.
- Evidence: `evidence-q0.md`, `evidence-q1.md`, `evidence-q1a.md`, `evidence-q7a.md`,
  `evidence-q2-design.md`, `evidence-template.md`, `review-q2-design.md`, `review-q1.md`.
- S2: `specs/008-classic-shaders/tasks.md`, `resolver.md` §10, `ownership.md`, `provenance.md`,
  `shaders/manifest/classic-programs.json`.
- Workflow: `docs/GIT_WORKFLOW.md` §§3–8; issue #53.

---

## 8. Self-check

- This file adds no threshold, scene, program or engine claim; it points at the contracts.
- Q2 is not described as ready, selectable, captured, or complete.
- P1 is recorded as **met** (2026-11-02) with the merged wiring named as evidence; the 2026-10-29
  snapshot is preserved in the row history, not presented as current.
- The recheck is pinned to `main` at `86e4eb3`; it changes no threshold, scene, program or engine
  claim.
- The manifest is recorded as 6 `stub` (the five required programs plus the optional `test`), 1
  `unsupported` (`megatexture`) and 7 `absent`; none is `owned`.
- The Q7a caveat is recorded as cleared by #115 (mapping gate `OK: 119/119`, exit 0), not as a
  current red gate.
- The Q1 result is recorded as host-specific (T030 RUN on the Graphics Tools host, SKIPPED elsewhere), not as a
  universal pass.
