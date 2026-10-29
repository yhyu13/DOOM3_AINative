# Classic compatibility parity contract (T032)

**Status**: Q2 design and pre-registration. **No capture has run on the new path.** Q0
`baseline-final11` is **established**: all four categories were captured twice at a fixed game tic
with the engine framebuffer at 1280×720 and pass their ceilings; T011 is complete with residual gaps.
The Q0 files are still **untracked in the S1 `feat/21-q0-safe-baseline` worktree** and must be
reconciled when they land. T032 is **design-complete with measured Q0 ceilings**, but **not accepted**
until an independent reviewer signs it and the Q2 fixtures/captures exist. No engine code is written
by this task and no game data is stored here.

**Scope owner**: T032. Related tasks: T034/T037/T038 (tests/fixtures), T039 (evidence), T040
(review), T080–T083 (production/integration). Requirements: FR-006, FR-007, FR-018, SC-003.

**Pinned source**: `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`. Line numbers
drift on a submodule bump; re-anchor before capture.

---

## 1. What "parity" means here

Classic parity means: **the same legal data, scene, camera, resolution, pass order and material/light
semantics produce an image and a command stream that a reviewer accepts as the retained GL path's
classic rendering, within thresholds fixed before the new-path result is seen.**

It is not:

- modern-look acceptance (Q4 owns that; changed PBR lighting is not parity);
- performance, frame-time or memory acceptance;
- whole-game, mod or retail coverage;
- a claim that the new path is selectable or usable before T036/T083 are done;
- a claim that an unsupported feature is covered. Unsupported is reported, never counted as a pass.

The GL path is the reference. A deliberate difference from it is either a parity failure or an
explicitly listed unsupported case with a reviewer decision. "It looks better" is not a tolerance.

---

## 2. Q0 input and its real status

Q0 `baseline-final11` is the recorded baseline (2026-10-28). It is an **OpenGL-only** baseline on a
virtual-display host, using the official free demo data only. All four categories were captured
**twice**, at a fixed game tic, by the engine's own framebuffer screenshot at 1280×720; no category
was skipped.

| Item | Value |
|---|---|
| Source | `specs/004-quality-renderer/evidence-q0.md` and `reference-scenes.md` in the S1 worktree; **untracked** at the time of writing |
| Q0 outcome | **Established**: `corridor`, `open-view`, `close-character`, `moving-light-combat` all `PASS` twice; no control scene |
| Independent review | Review 1 `BLOCKED`, review 2 `BLOCKED`, review 3 `PASS-WITH-FINDINGS` (OS-window state); review 4 `PASS-WITH-FINDINGS` (fixed-tic `final10`); `final11` re-ran after a comment-only script change; T011 is complete with residual gaps |
| Data | `base/demo00.pk4`, 483,534,533 bytes, SHA-256 `e373ec70b35cf72852a1f6014c2c2dc21f890a03cd41b21a9524755f951f64f9`, plus two local non-redistributed shims |
| Capture | Engine framebuffer, 1280×720, `com_fixedTic 1`, `+wait 600` (720 for combat), `+screenshot`, `+quit`; no OS-window capture |
| Metric | `delta=8`; `meanAbs`; `diffPercent`; a scene passes only when both limits hold and the markers match |
| Renderer | `using ARB renderSystem` / ARB2; engine reports 64 MB video memory and "Low quality" |
| Adapter | Intel Graphics driver 32.0.101.8331, with virtual display adapters present; not a discrete-GPU quality/performance baseline |

The ceilings were tightened after the fixed-tic probes; the `reference-scenes.md` §6 disclosure lists
the earlier, looser final9 cells, and final11 also passes those. The values below are the ceilings
frozen **before the recorded final11 run**.

| Scene | Ceiling (`meanAbs` / `diffPercent`) | `final11` recorded | Status |
|---|---:|---:|---|
| `corridor` | 0.5 / 0.5 | 0.0848 / 0.0905 | **PASS** |
| `open-view` | 2.0 / 2.0 | 0.1767 / 0.1430 | **PASS** |
| `close-character` | 0.5 / 0.5 | 0.1665 / 0.2088 | **PASS** |
| `moving-light-combat` | 3.0 / 5.0 | 0.4853 / 0.7490 | **PASS** |

These are **same-renderer repeatability measurements**, not cross-renderer parity evidence. Q2 still
has to compare the GL reference against the new path; the Q0 values supply the predeclared ceiling,
not the new-path result.

Residual Q0 gaps (do not hide them): the baseline is valid for this host/demo/GL renderer, not retail
or performance; `Compare` records 1280×720 but does not yet reject a different size; the synthetic
suite does not exercise the engine capture path; real process start/stop and `-Clean` archive are not
behavior-tested; two builds differ in hash. T011 is complete with those gaps.

If the final11 values or label change on merge, this document is invalid until reconciled. The
ceilings frozen before the recorded run remain the contract; the recorded run is not.

---

## 3. Scene and camera identity

T007 owns the canonical scene identities in `specs/004-quality-renderer/reference-scenes.md`. This
table is the **parity projection** of those identities; if the two disagree, T007 wins and this file
must be updated before capture. Do not invent a scene, map or model name.

| ID | Category | Map / drive | Parity use | Q0 status |
|---|---|---|---|---|
| `open-view` | open view | `game/demo_mars_city2`, `+wait 600`; `ui_showGun 0`, HUD/FX/particles off, `SCENE_READY_open-view` | Whole-frame and structural | **PASS** |
| `close-character` | close character | `game/demo_mars_city2`, `+wait 600`; `pm_thirdPerson 1`, range 80, angle 180; HUD/FX/particles off | Whole-frame, character ROI | **PASS** |
| `moving-light-combat` | moving light/combat | `game/demo_mars_city2`, `+wait 600`; `god`, `spawn monster_demon_imp`, `+wait 120`; no HUD | Structural; image parity only at the recorded fixed tic | **PASS** |
| `corridor` | corridor | `game/demo_mc_underground`, `+wait 600` after the doors open; no HUD | Whole-frame and structural | **PASS** |

### 3.1 Q2 fixtures that do not exist yet

Feature cases need a deterministic fixture that the four demo scenes do not provide. T037/T038 own
these; T032 names them so the coverage matrix is executable, not a wish list. Until they exist, the
rows are `NOT RUN`.

| Fixture ID | Owner | Purpose | Status |
|---|---|---|---|
| `fixture-interaction` | T037 | Canonical bump/diffuse/specular material and one light; draw-order trace | Not authored |
| `fixture-shadow` | T037 | `noSelfShadow`, `noShadows`, global/local chains | Not authored |
| `fixture-subview` | T037 | Mirror, portal sky, xray, remote/security view | Not authored |
| `fixture-post` | T037 | `_currentRender`, glasswarp, post-process split | Not authored |
| `fixture-effects` | T037 | Fog, blend, particles, decals, overlays, depth hack | Not authored |
| `fixture-viewmodel` | T037 | First-person weapon with `weaponDepthHack` | Not authored |
| `hud-control` | T038 | A Q2 scene with HUD on, display-resolution overlay; not a Q0 control | Not authored |

A fixture must be deterministic (recorded demo or fixed tick/camera), use legal data, and record its
camera, frame index, data identity and thresholds before it is used.

### 3.2 Determinism rule

A parity image requires the same rendered state on both paths. Acceptable determinism sources are,
in order:

1. a recorded demo (`+playDemo` / `+playCmdDemo`) with a documented frame/time stop;
2. a test map or script whose camera and entities are fixed for the capture;
3. a settled static view with no AI, animation, particle or moving-light state left, verified by a
   GL-vs-GL repeatability run.

`g_stopTime 1` is **not** a determinism control in this build: the Q0 scene specification records
that it leaves the 3D view black while the HUD remains. Do not use it.

If none of 1–3 is available, the case is **SKIPPED** for image parity. Structural checks (pass order,
stage submittal, chain membership, counts) may still run and be reported separately. A wide numeric
tolerance on a non-deterministic frame is not a parity gate.

---

## 4. Command and pass order under test

These are the semantics T034/T080–T082 must preserve. The new classic path must produce the same
ordered stream, not merely the same final still.

| # | Stage | Source anchor | What must be preserved |
|---|---|---|---|
| O-01 | Front-end sort | `neo/renderer/tr_main.cpp:1049-1074`; per-draw `tr.sortOffset` `neo/renderer/tr_light.cpp:1195-1200` | `drawSurf_t.sort` order, including the monotonically increasing `tr.sortOffset` that stabilizes equal sort values; decals, translucent layers and post-process order |
| O-02 | Command FIFO | `neo/renderer/tr_local.h:472-479`; `neo/renderer/tr_backend.cpp:600-645` | `RC_DRAW_VIEW`, `RC_SET_BUFFER`, `RC_COPY_RENDER`, `RC_SWAP_BUFFERS` execute in list order; unknown command is fatal |
| O-03 | View pass order | `neo/renderer/draw_common.cpp:1666-1725` | depth fill → interactions → stencil reset → light scale → ambient/non-light passes → fog/blend → post-process → debug tools |
| O-04 | Light-scale decision vs application | `neo/renderer/draw_common.cpp:1681`; `neo/renderer/draw_common.cpp:1598-1656` | `RB_DetermineLightScale` decides before interactions; the screen-space multiply happens after; do not collapse the two |
| O-05 | Depth fill | `neo/renderer/draw_common.cpp:350-577` | equal-depth opaque prepass, alpha-test discard, scissor, polygon offset |
| O-06 | Interaction order | `neo/renderer/draw_arb2.cpp:243-323` | per light: stencil clear if needed, global shadows, local interactions, local shadows, global interactions, translucent interactions |
| O-07 | Interaction stage submittal | `neo/renderer/tr_render.cpp:689-842`; normalized stage order `neo/renderer/Material.cpp:1742-1771` | one draw per accumulated bump/diffuse/specular run per light stage; bump→diffuse→specular within a bump-delimited group; a run with no bump image is dropped (`tr_render.cpp:654-667`) |
| O-08 | Fog/blend | `neo/renderer/draw_common.cpp:1532-1596`; skip from main loop `neo/renderer/draw_arb2.cpp:256-262` | fog and blend lights are excluded from interactions and drawn in their later pass, over global/local interactions only |
| O-09 | Post-process `_currentRender` | `neo/renderer/draw_common.cpp:972-1034`; `neo/renderer/Material.cpp:2218-2250` | `CopyFramebuffer` is an immediate backend copy before the first `SS_POST_PROCESS` surface; `SS_POST_PROCESS = 100`, not 8 |
| O-10 | Mid-frame capture | `neo/renderer/RenderSystem.cpp:952-984`; `RC_COPY_RENDER` `neo/renderer/RenderSystem.cpp:912-944` | `CaptureRenderToFile` executes and clears the queue mid-frame; `CaptureRenderToImage` appends a copy without flushing; do not defer everything to swap |
| O-11 | Display HUD | `neo/renderer/GuiModel.cpp:226-297` | a separate 2D view, 640×480 ortho mapping outside the editor, added as `RC_DRAW_VIEW`; not part of the 3D scene |
| O-12 | In-world GUI | `neo/renderer/tr_guisurf.cpp:128-181` | emitted into the current 3D view with the surface's matrix; not moved to the final HUD |
| O-13 | Depth hacks | `neo/renderer/tr_render.cpp:176-220` | weapon hack changes depth range to 0..0.5 and projection z term ×0.25; model hack subtracts from the z term; x/y NDC is unchanged |
| O-14 | Projection and jitter | `neo/renderer/tr_main.cpp:895-958` | infinite-far projection; `r_jitter` (default 0) adds a one-sided [0,1) pixel frustum shift to the x/y bounds, not a centred ±0.5 px offset |

The **front-end** order is part of parity: re-sorting in the backend can change decal and translucent
results even if every individual draw is correct.

---

## 5. Coverage matrix

`Threshold` uses the IDs in §6. `S` means the exact T-07 trace; `I` means an image/ROI check; `U`
means explicitly unsupported/blocked. `Scene / ROI` uses §3 IDs or a §3.1 fixture. Every row must end
as `PASS`, `FAIL`, `SKIPPED` with a reason, `NOT RUN`, or `UNSUPPORTED` with a documented fallback.
A skipped, not-run or unsupported row never counts as parity.

| ID | Case | Source anchor | Scene / ROI | Observable | Threshold | Owner |
|---|---|---|---|---|---|---|
| CP-01 | Opaque depth prepass and equal-depth colour | `draw_common.cpp:350-577` | `open-view`, `close-character`, `corridor` / whole frame | depth/colour, z-fighting samples | S + I(T-01/02/04) | T037 |
| CP-02 | Interaction stage order and accumulation | `tr_render.cpp:689-842`; `Material.cpp:1742-1771` | `fixture-interaction` / draw trace | submittal order and draw count | S | T037 |
| CP-03 | Alpha-tested / perforated coverage | `Material.cpp:1432-1435`; `draw_common.cpp:435-489` | `open-view` grating / alpha mask; `fixture-interaction` fallback | coverage mask, holes | S + I(T-08) | T037 |
| CP-04 | Projected light cookie and falloff | `draw_arb2.cpp:69-135`; `tr_render.cpp:738-758` | `open-view` light pool / wall ROI; `fixture-interaction` | light pool edge, cookie scale | I(T-06) | T037/T082 |
| CP-05 | Point light multi-frustum falloff | `draw_arb2.cpp:243-323` | `open-view` corner ROI; `fixture-interaction` | falloff continuity, no seam | I(T-06) | T037/T082 |
| CP-06 | Ambient light path | `draw_arb2.cpp:171-177` | `close-character` lit surface ROI; `fixture-interaction` | brightness/colour, no specular | I(T-06) | T037/T082 |
| CP-07 | Stencil shadow volume | `draw_common.cpp:1176-1231`; `draw_arb2.cpp:288-303` | `close-character` shadow ROI; `fixture-shadow` | shadow silhouette and interior | S + I(T-06) | T037/T082 |
| CP-08 | Global vs local shadow chains | `Interaction.cpp:1159-1241` | `fixture-shadow` / chain trace | self/non-self shadow membership | S | T037/T082 |
| CP-09 | `noShadows` material/light | `Material.h:392,405,468`; `Interaction.cpp:1162` | `fixture-shadow` / no-shadow ROI | no shadow, lighting still present | S + I(T-06) | T037/T082 |
| CP-10 | Translucent interactions | `draw_arb2.cpp:305-315`; `Material.h:219-224` | `open-view` glass ROI; `fixture-effects` | blend order, depth func LESS | S + I(T-01/02) | T037/T081 |
| CP-11 | Fog light | `draw_common.cpp:1378-1596` | `fixture-effects` / fog ROI; `corridor` if fogged | fog gradient, no interaction pass | I(T-06) | T037/T082 |
| CP-12 | Blend light | `draw_common.cpp:1249-1376` | `fixture-effects` / blend ROI | projection/falloff, no surface interaction | I(T-06) | T037/T082 |
| CP-13 | Light scale / overbright | `draw_common.cpp:1598-1656` | `open-view` highlight ROI | highlight clipping and scale | I(T-01/02) | T037 |
| CP-14 | Mirror / subview | `tr_subview.cpp:217-268`; `Material.h:227`; `draw_common.cpp:414-419` | `fixture-subview` / mirror | subview content, colour, recursion | S + I(T-06) | T037/T080 |
| CP-15 | Portal sky | `Material.cpp:1854-1855`; `draw_common.cpp:707-709`; `RenderWorld_load.cpp:686-696` | `open-view` sky ROI; `fixture-subview` | sky content and depth | I(T-06) | T037/T080 |
| CP-16 | Xray subview | `tr_subview.cpp:271-283`; `draw_common.cpp:1015-1019` | `fixture-subview` / xray | only xray-index surfaces | S + I(T-06) | T037/T080 |
| CP-17 | World GUI (in-world screen) | `tr_guisurf.cpp:128-181` | `open-view` terminal ROI; `fixture-subview` | text placement, depth, no HUD move | I(T-06) | T038 |
| CP-18 | Display HUD / fullscreen GUI | `GuiModel.cpp:226-297` | `hud-control` / display resolution | display-resolution crispness, after scene | I(T-05/T-09) | T038 |
| CP-19 | Viewmodel + `weaponDepthHack` | `tr_render.cpp:176-188`; `draw_common.cpp:749-755` | `fixture-viewmodel` / weapon ROI | x/y position, depth range, no world poke-through | S + I(T-06) | T037/T080 |
| CP-20 | Model depth hack | `tr_render.cpp:195-207`; `RenderWorld.h:138` | `fixture-effects` / particles near wall | x/y unchanged, z remapped | I(T-06) | T037/T081 |
| CP-21 | Post-process `_currentRender` | `draw_common.cpp:972-1034`; `Material.cpp:2218-2250` | `fixture-post` / screen effect | copy point, no feedback, sort 100 | S + I(T-06) | T037/T081 |
| CP-22 | Skybox / wobble sky texgen | `draw_common.cpp:96-98,259-262`; `Material.cpp:1296` | `open-view` skybox | orientation, seams, no depth write | I(T-06) | T037/T081 |
| CP-23 | Glasswarp texgen | `draw_common.cpp:153-155`; `draw_arb2.cpp:354` | `fixture-post` / glasswarp | screen-space warp, no vertex program | I(T-06) | T037/T081 |
| CP-24 | MegaTexture | `Material.cpp:1509-1519` | Any material requesting it | explicit unsupported report | **U** | T037 |
| CP-25 | Decals and overlays | `Material.cpp:2209-2210`; `ModelDecal.h:63`; `ModelOverlay.h:62` | `open-view` blood/scorch; `fixture-effects` | polygon offset, sort, projection | I(T-06) | T037/T081 |
| CP-26 | Particles, beams, sprites, liquids | `Model_local.h:287,311,377,232`; `Model_prt.cpp:268` | `fixture-effects` / effects near geometry | sort, depth hack, alpha | I(T-06) | T037/T081 |
| CP-27 | Cinematics | `Cinematic.cpp:383` | demo intro cinematic / frame | frame content, no 3D interaction | I(T-06) or SKIP with reason | T037 |
| CP-28 | Front-end sort order | `tr_main.cpp:1049-1074` | `open-view` + `fixture-*` / draw trace | draw order trace | S | T034/T037 |
| CP-29 | Remote / security view | `tr_subview.cpp:292-350` | `fixture-subview` / remote view | remote camera content, no main-history commit | S + I(T-06) | T037/T080 |
| CP-30 | Non-interaction ambient stage order | `draw_common.cpp:761-800` | `fixture-interaction` / ambient draw trace | ambient stage submittal order | S | T037 |

### 5.1 Explicit unsupported cases

| Case | Decision | Consequence |
|---|---|---|
| `megaTexture.vfp` | **Unsupported in milestone 1** (the S2 classic-shader decision D-003, `specs/008-classic-shaders/plan.md`; not yet merged). The portable RHI does not model tiled/sparse feedback and uploads. | A material requesting it must report `UNSUPPORTED` with material/stage/key. It is not drawn as a silent fallback and does not count as parity. |
| Any material-declared program outside the manifest | Fail-loud resolver (S2) | Report the material, stage, key and code; no draw. |
| Removed backends (R200/NV20/NV10) | Not authored | Report `UNSUPPORTED` if data references them; do not invent parity. |
| Retail RoE / mod content | Not tested | Out of scope; no claim. |

`specs/008-classic-shaders/` is an S2 artifact and is untracked at the time of writing. Do not depend
on it until it lands; this row is a decision reference, not an implementation claim.

---

## 6. Thresholds

### 6.1 Status: category ceilings fixed; ROI/HUD still pending

| ID | Metric | Gate | Basis | State |
|---|---|---|---|---|
| T-01 | Whole-frame `meanAbs` / `diffPercent` at `delta=8` | `open-view` **2.0 / 2.0** | Q0 final11 recorded 0.1767 / 0.1430 | **FIXED** |
| T-02 | Same | `close-character` **0.5 / 0.5** | Q0 final11 recorded 0.1665 / 0.2088 | **FIXED** |
| T-03 | Same | `moving-light-combat` **3.0 / 5.0** | Q0 final11 recorded 0.4853 / 0.7490 | **FIXED only at the recorded fixed tic**; a live-AI run is not a parity gate |
| T-04 | Same | `corridor` **0.5 / 0.5** | Q0 final11 recorded 0.0848 / 0.0905 | **FIXED** |
| T-05 | Same, HUD region | `hud-control` upper bound **0.5 / 0.5** | Closest static scene ceiling (T-02); **HUD not measured** | **PENDING HUD CONTROL** |
| T-06 | Same, predeclared ROI | ≤ the selected scene ceiling; ROI and mask recorded before capture | Measured scene ceilings; ROI not measured | **PENDING GL-vs-GL ROI**; the control may only tighten |
| T-07 | Command/stage/chain trace | Exact equality of the recorded sequence | Source semantics in §4 | **FIXED** |
| T-08 | Character/alpha ROI `diffPercent` | ≤ T-02 (`0.5`) on the predeclared mask | Measured close-character ceiling; ROI not measured | **PENDING ROI CONTROL** |
| T-09 | HUD text bounding boxes and edge sharpness | Same text boxes and no softened/double-composited edges; pixel metric ≤ T-05 | Display-resolution composition requirement | **PENDING METHOD**; a difference is a finding, not a tolerance |
| T-10 | Q0 final11 whole-frame repeatability | Recorded `meanAbs` / `diffPercent` must be ≤ T-01–T-04; final11 values in §2 | Q0 `baseline-final11` | **FIXED AS BASELINE EVIDENCE**; not a cross-renderer parity result |

The four category ceilings are now measured repeatability limits, not guesses. **T-05/T-06/T-08/T-09
remain pending** because no HUD/ROI control has been measured. T032 is still not accepted: the new
path has not captured anything, the T037/T038 fixtures do not exist, and no independent reviewer has
signed the document.

### 6.2 Rules that prevent a "pass by tolerance"

1. **No threshold is set after seeing a new-path result.** A change after capture invalidates that
   capture. Any change needs a new pre-registration, a reviewer, and a new baseline.
2. **A GL-vs-GL control may only tighten.** The control runs the same deterministic drive twice on
   the GL path and records the measured `meanAbs`/`diffPercent` per ROI. If it is larger than the
   predeclared ceiling, the scene is not deterministic enough; report SKIPPED, do not widen the
   parity gate.
3. **A cross-renderer difference above the gate is a FAIL** unless it is a listed unsupported case
   with a reviewer decision. "Known difference" without a row is not a pass.
4. **No silent exclusion.** Every excluded region (HUD, letterbox, debug overlay, animated light)
   must be listed with its rectangle/mask, area fraction, reason, and the same exclusion on both
   paths.
5. **Structural checks cannot be waived by a good image.** A wrong pass order that happens to look
   close is a failure.
6. **The same capture method on both sides.** Q0 uses the engine framebuffer at 1280×720; an
   OS client-area screenshot is a different measurement and is not comparable. If the method changes,
   re-baseline.
7. **A `SKIPPED` or `NOT RUN` category is not a pass.** Q0 now passes all four categories; a future
   skipped row or a missing fixture is still not a pass, and a control frame is not a substitute.

### 6.3 Threshold decision record (fill before Q2 capture)

| Field | Value |
|---|---|
| Scene / case | `<CP-id and scene>` |
| Metric | `<T-01..T-10>` |
| GL-vs-GL control (run 1/run 2) | `<meanAbs / diffPercent or structural result>` |
| Chosen gate | `<value; may be tighter than the predeclared upper bound>` |
| ROI / mask | `<rectangle or mask identity, area fraction>` |
| Exclusions | `<reason and area>` |
| Fixed at (UTC) | `<date-time>` |
| Reviewer | `<identity; independent of the author>` |
| Result captured after fixing? | `<no; a yes invalidates the run>` |

---

## 7. Capture and evidence protocol

Use `specs/004-quality-renderer/evidence-template.md` for every record. At minimum:

1. **Build/source identity**: repository revision, branch, dirty paths, submodule SHA/status, target
   patch SHA, executable SHA. A rebuild may change the executable hash; record the captured one.
2. **Data identity and rights**: the demo `pk4` hash and any local shims; no retail pack, extracted
   asset or derived pack is tracked. State the permission boundary.
3. **Effective configuration**: `r_renderer`/`r_mode`/resolution/window mode, HUD/FX/viewmodel cvars,
   `r_jitter`, `r_useSMP`, `r_multiSamples`, `r_shadows`, `r_useDepthBoundsTest`, image settings, and
   every cvar that changes the compared output. Record the actual values, not a profile name.
4. **Adapter/driver**: named hardware or software adapter, driver, and whether the result is a
   functional or performance result. A software adapter is labelled.
5. **Commands and exits**: exact ordered commands, working directory, exit code, and the full log.
   The GL side may use the Q0 driver; the new side's command must be recorded when it exists. Do not
   invent a command here.
6. **Capture**: 1280×720, windowed, one capture at a time; method (OS client area or engine
   framebuffer), process ID/focus, settle/marker, frame index. Run each side twice for repeatability.
7. **Comparison**: per case, both runs' `meanAbs`/`diffPercent`, the structural trace, the threshold
   ID and value, the result, and the artifact reference. A missing run is `NOT RUN`.
8. **Negative controls**: at least one deliberately wrong case (e.g., swapped frame order, wrong
   scene) must fail the gate. A gate that passes everything is not evidence.
9. **Review**: independent reviewer, diff reviewed, commands rerun, decision, unresolved items.

The Q0 script's `-MaxMeanAbs` / `-MaxDiffPercent` overrides are recorded per scene in its manifest.
A Q2 override is not silently weaker: it must be fixed in §6.3 before capture and recorded in the
evidence.

---

## 8. Per-case result record (T039)

One row per case in §5. `Execution` is `RUN`, `SKIPPED`, `NOT RUN` or `UNSUPPORTED`. `Structural` and
`Image` are separate; `—` means not applicable. A composite `S + I` case must record both. `Outcome`
is not a single value: a case can pass structure and fail image, and that is a failure for parity.

| Case | Execution | Structural (`S`/T-07) | Image (`I`) | Metrics / trace | Threshold | Reason / observation | Artifact |
|---|---|---|---|---|---|---|---|
| `CP-01` | `<RUN/SKIPPED/NOT RUN/UNSUPPORTED>` | `<PASS/FAIL/—>` | `<PASS/FAIL/SKIPPED/NOT RUN/—>` | `<values>` | `<T-id>` | `<reason>` | `<relative reference>` |
| `CP-02` |  |  |  |  |  |  |  |
| `CP-03` |  |  |  |  |  |  |  |
| `CP-04` |  |  |  |  |  |  |  |
| `CP-05` |  |  |  |  |  |  |  |
| `CP-06` |  |  |  |  |  |  |  |
| `CP-07` |  |  |  |  |  |  |  |
| `CP-08` |  |  |  |  |  |  |  |
| `CP-09` |  |  |  |  |  |  |  |
| `CP-10` |  |  |  |  |  |  |  |
| `CP-11` |  |  |  |  |  |  |  |
| `CP-12` |  |  |  |  |  |  |  |
| `CP-13` |  |  |  |  |  |  |  |
| `CP-14` |  |  |  |  |  |  |  |
| `CP-15` |  |  |  |  |  |  |  |
| `CP-16` |  |  |  |  |  |  |  |
| `CP-17` |  |  |  |  |  |  |  |
| `CP-18` |  |  |  |  |  |  |  |
| `CP-19` |  |  |  |  |  |  |  |
| `CP-20` |  |  |  |  |  |  |  |
| `CP-21` |  |  |  |  |  |  |  |
| `CP-22` |  |  |  |  |  |  |  |
| `CP-23` |  |  |  |  |  |  |  |
| `CP-24` |  |  |  |  |  |  |  |
| `CP-25` |  |  |  |  |  |  |  |
| `CP-26` |  |  |  |  |  |  |  |
| `CP-27` |  |  |  |  |  |  |  |
| `CP-28` |  |  |  |  |  |  |  |
| `CP-29` |  |  |  |  |  |  |  |
| `CP-30` |  |  |  |  |  |  |  |

---

## 9. What Q2 would have to prove

Q2 is claimable only when all of the following hold:

- [x] **Q0 is established**: `baseline-final11` captures all four categories twice under deterministic
      fixed-tic drives and the reviews accept the method (review 4 `PASS-WITH-FINDINGS`; T011 complete
      with residual gaps).
- [ ] Q0 is merged; its values are reconciled here; the review decision is recorded on `main`.
- [x] The three-tier RHI contract (T004) and the evidence template (T005) are independently approved
      (PR #27; both tasks checked).
- [ ] Q1 capabilities required by the classic path exist; T088/Q7a is run or Q2 is explicitly
      re-scoped in writing (a header scan is not execution).
- [ ] The §3.1 fixtures exist and are deterministic; their cameras, data, frames and thresholds are
      recorded before capture.
- [ ] The engine adapter (T035) and production slices (T080–T082) exist in the reviewed superproject
      and fork, with a submodule bump; no change exists only in a scratch target copy.
- [ ] The GL and new-path captures use the same legal data, drive, resolution, method, and
      predeclared thresholds.
- [ ] Every §5 row is PASS, or an explicit `SKIPPED`/`NOT RUN`/`UNSUPPORTED` with a reason and a
      reviewer decision; unsupported rows are not counted as parity.
- [ ] Structural traces are exact; image results are within §6.
- [ ] T083's forced-failure check proves the existing path remains selectable and usable; no
      automatic switch is claimed.
- [ ] T040's independent review records the decision and the remaining unsupported cases.

The current state satisfies **Q0 and T004/T005**; the rest are not satisfied. That is why T032 remains
unchecked.

---

## 10. Known blockers and limits

| Blocker | Effect | Next action |
|---|---|---|
| Q0 final11 is untracked in the S1 worktree; reviews are AI-only | The input is not on `main`; no human sign-off | Land Q0, reconcile values, record the reviewer |
| HUD and ROI controls are not measured (T-05/T-06/T-08/T-09) | HUD/ROI image gates cannot pass yet | T037/T038 measure the HUD and ROI controls; record before Q2 capture |
| Q2 fixtures (§3.1) not authored | 30-row matrix cannot run | T037/T038 author deterministic fixtures and record thresholds |
| Q1 incomplete (Q1a merged, Q1b/Q1 not done); T088 not run | Q2 is not permitted | Finish Q1, run T088, or re-scope Q2 in writing |
| No real engine adapter/production slices | No new-path image exists | T035/T080–T082 in the superproject + fork |
| No independent T032/T039/T040 review | T032/T039/T040 cannot complete | Assign a reviewer who did not author the change |
| Q0's host is a virtual display | Any performance/quality inference is invalid | Use a named discrete GPU for Q2 if claimed; record it |
| Q0's fixed-tic scene is build/tic-dependent | A different engine build or tic moves the image | Record executable hash and tic; re-baseline on a build change |

---

## 11. Traceability

| Requirement | Where this document addresses it | Evidence (planned, not current) |
|---|---|---|
| FR-006 selectable classic path + retained fallback | §3, §9 (T036/T083) | `engine-integration.md`; `evidence-q2.md` |
| FR-007 material stages, alpha, lights, shadows, fog/blend, portals, GUI, viewmodel, remote view | §5 (CP-01–CP-30) | `tests/renderer/classic_compatibility_test.cpp`, `classic_gui_test.cpp` (T037/T038; **do not exist yet**) |
| SC-003 same scene on both paths, thresholds fixed before comparison | §2, §6, §7 | `evidence-q2.md`; T039/T040 |
| FR-018 named scene/build/data/hardware/config | §3, §7 | `evidence-q2.md`; evidence template |
| T032 output | this document | T032 review record (pending) |

**Self-check**: the numbers in §2 are copied from the S1 Q0 `baseline-final11` report, not re-measured
here. The category thresholds in §6 are the measured final11 ceilings; HUD/ROI thresholds are still
pending and are marked so. The source anchors are from the pinned submodule and should be re-checked
on a bump. The test files named in §11 are planned and are explicitly marked as not existing.





