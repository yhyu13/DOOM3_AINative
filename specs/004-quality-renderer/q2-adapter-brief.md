# Q2 engine-adapter implementation brief (T034–T040, T080–T083)

**Status**: dispatch brief for issue [#53](https://github.com/yhyu13/DOOM3_AINative/issues/53), written
2026-10-29. This is **not** a specification, an implementation, a capture, or an acceptance record.
**Q2 is not ready and not complete**: one blocking prerequisite (S2 resolver engine wiring) has not
landed, Q1's validation gate is open, and no engine adapter exists. The design contracts are
`classic-parity.md` (T032) and `engine-integration.md` (T033); this file decides only the order,
ownership, gates and stop conditions for executing them. Where this file and those contracts
disagree, the contracts win and this file is corrected.

**Baseline inspected**: `main` at `4a2fd7c`. The S2 worktree state below is a local snapshot and may
move; re-check it before dispatch.

---

## 1. What this brief adds

| Contract | Owns | This brief |
|---|---|---|
| `classic-parity.md` | parity meaning, scenes, CP-01…CP-30, T-01…T-10, capture/evidence protocol | points to it; does not restate thresholds |
| `engine-integration.md` | delivery model, both-executable linkage, `glConfig` dispositions, `r_renderApi`, `RC_*` dispatch | points to it; does not restate the CMake or dispatch design |
| `docs/GIT_WORKFLOW.md` | issue → spec PR → worktree → PR → review → merge | applies it; issue #53 is the implementation container |

Nothing here authorizes engine code. The implementation issue is open and blocked.

---

## 2. Prerequisites (exact state, 2026-10-29)

| # | Prerequisite | State | Evidence |
|---|---|---|---|
| P1 | **S2 resolver engine wiring** | **NOT MET.** The resolver core and its tests are on `main` (`src/renderer/shader/shader_resolver.cpp`), but `R_LoadARBProgram`, `R_FindARBProgram`, the `Material.cpp` atomic parse, the draw gate and the two console commands are engine-side and untouched. An uncommitted attempt exists in the `feat/29-resolver-wiring` worktree: a modified `targets/win11-amd64/CMakeLists.txt` and untracked `src/renderer/shader/engine/{shader_engine.cpp,shader_engine.h}`. It is **not merged and not an issue**. | `specs/008-classic-shaders/tasks.md` §E.2; T-021/T-022/T-024/T-029 marked `PARTIAL`; `resolver.md` §10 |
| P2 | **Five classic programs** | **PARTIAL.** `interaction`, `environment`, `bumpyenvironment`, `arbfp_glasswarp` and `shadow` are authored and compile to DXIL + SPIR-V + validated WGSL; the optional `test` program also compiles. The manifest (`shaders/manifest/classic-programs.json`) holds **14 entries**: 6 `stub` (`interaction`, `environment`, `bumpyenvironment`, `shadow`, `arbfp_glasswarp`, `test`), 1 `unsupported` (`megatexture`) and 7 `absent` (`ambientlight`, `arbvp_glasswarp`, `r200_interaction`, `nv20_bumpandlight`, `nv20_diffusecolor`, `nv20_specularcolor`, `nv20_diffuseandspecularcolor`). **None is `owned`**; the rendered comparison (T-027) is `BLOCKED` on authorised data and a running classic backend. A `stub` program must not be drawn as if it were owned. | `specs/008-classic-shaders/tasks.md` T-025/T-030; `ownership.md` §5 |
| P3 | **Q1** | **RESIDUAL ACCEPTED, GATE OPEN.** Q1b fixtures are merged (`f1e4feb`, 17/17 Release and Debug). T030 and T031 stay open because the debug layer and GPU-based validation are `SKIPPED` (`0x887A002D`, `validation_executed=0`) on both the hardware and software adapter. The owner accepted this on 2026-10-29 as a recorded gap; the acceptance does not turn a skip into a pass. | `specs/004-quality-renderer/evidence-q1.md` (T030/T031 rows, owner-acceptance section) |
| P4 | **Q7a / T088** | **DONE.** Mapping gate 57/57, browser spike 12 required checks twice, Emscripten/wasm leg twice. Caveat: `py -3 -I scripts/validate-webgpu-mapping.py` still exits 1 on current `main` (62 problems), owned by open issue #36 (T089). | `specs/004-quality-renderer/evidence-q7a.md`; `plan.md` Q7a row; `tasks.md` Q1 status note |
| P5 | **Q0** | **DONE and merged** (PR #31): `baseline-final13`, all four categories `PASS` twice under fixed-tic drives. The values are same-renderer repeatability, not cross-renderer parity. Host is a virtual display with an Intel adapter; not a discrete-GPU quality baseline. | `specs/004-quality-renderer/classic-parity.md` §2; `evidence-q0.md` |
| P6 | **T004 / T005** | **DONE and independently reviewed** (PR #27): three-tier RHI contract, header gate, evidence template. | `contracts/rhi-foundation.md`; `review-rhi-contract.md`; `evidence-template.md` |
| P7 | **T032 / T033** | **DONE as design only** (ticked after the Round 8 PASS in `review-q2-design.md` §3.6). No engine build, capture, or independent Q2 sign-off exists. | `classic-parity.md`; `engine-integration.md`; `review-q2-design.md` §4 |

**Dispatch rule.** P1 and P2 gate any adapter work that draws classic materials. P3 gates the Q2
validation wording, not the adapter compile: Q2 may build and capture with the residual disclosed,
but may never state that Q1 passed. No Q2 execution starts while P1 is unmerged unless the owner
records a written re-scope in issue #53 — a decision, not an inference.

**Coordination.** The uncommitted S2 wiring owns `targets/win11-amd64/CMakeLists.txt` and
`src/renderer/shader/engine/**` right now. Q2 must not edit either path, nor open a second
`CMakeLists.txt` writer, until that work lands or is abandoned.

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

1. **Close P1 and P2** (S2 engine wiring merged; the manifest state agreed). Resolve P3 in writing.
   Nothing below starts on an unmerged `CMakeLists.txt` writer.
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
   clean, and no change exists only in the ignored target copy. T084 stays partial: a green `SC-006b`
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
| S1 | P1 (S2 engine wiring) is still unmerged when T035 lands, or a `stub` program is drawn | The classic path draws unowned programs; parity is meaningless | Stop; land the wiring under its own issue, or record a written owner re-scope in #53 |
| S2 | A Q2 statement would imply Q1 passed while T030/T031 are open | False validation claim | Reword to the accepted-residual fact; if a reviewer requires Q1 green, re-plan the host/SDK |
| S3 | `validate-webgpu-mapping.py` still exits 1 at capture time and no waiver exists | The pre-Q2 header gate is red | Reconcile via #36/T089, or record an explicit reviewer waiver before capture |
| S4 | Q0 label, values, executable, tic, host or capture method changes | The frozen ceilings no longer describe the reference | Re-baseline Q0 before any comparison; do not reuse the old ceilings |
| S5 | A GL-vs-GL control exceeds its scene ceiling | The scene is not deterministic enough for image parity | Mark the row `SKIPPED` with the reason; do not widen the tolerance |
| S6 | A threshold, ROI or mask is chosen after seeing a new-path result | The capture was not pre-registered | Invalidate the capture; re-pre-register and recapture |
| S7 | T-05/T-06/T-08/T-09 are still unmeasured when HUD/ROI cases are judged | No valid HUD/ROI image gate | Stop those rows; measure the control first |
| S8 | The engine change cannot be delivered outside `targets/win11-amd64/doom3/` (new file only in the patch exporter, or fork depends on an unmerged header) | A scratch-copy-only change is not delivered | Stop; re-plan as fork PR + submodule bump, or merge the dependency first |
| S9 | Any of the 30 `glConfig.isInitialized` sites is left unclassified, or the modern path still calls `RB_STD_DrawView` | Fabricated state or a GL pass on the modern path | Stop; resolve every row by group |
| S10 | A `GLenum`, `idImage*`, `qgl*` call or `tri->ambientCache` pointer reaches `doom3::rhi` | Breaks the portable boundary and the header gate | Stop; translate at the boundary |
| S11 | Both executables cannot link under `/MT`/`/MTd` (`LNK2038`), or `doom3_rhi` is missing from one EXE | One executable lacks the backend | Stop; fix linkage before any capture |
| S12 | T035 and T080 would be edited concurrently, or the resolver wiring is still the `CMakeLists.txt` writer | Two writers on one file | Split ownership or sequence; do not merge over it |
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
- P1 is recorded as **not landed** because it is not on `main`; the uncommitted worktree is named as
  a coordination constraint, not as evidence.
- The manifest is recorded as 6 `stub` (the five required programs plus the optional `test`), 1
  `unsupported` (`megatexture`) and 7 `absent`; none is `owned`.
- The Q7a caveat (red mapping gate on `main`) is recorded alongside T088 being done.
- The Q1 owner acceptance is recorded as a gap, not a pass.
