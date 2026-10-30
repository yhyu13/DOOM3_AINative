# Q4 art review, round 2 (T057 re-run), independent

**Verdict: BLOCKED for Q4 — on one remaining non-art gate, the unmeasured VRAM peak (B4).** All six T057
blockers (B-1 … B-6) are closed with visible artifacts in the committed frames at `29af718`, and all
seven rubric dimensions are `accept` — 4.3, 4.5 and 4.6 only with the limits recorded below. No art
defect blocks the frames.

**Q4 is not green.** The only remaining gate is a measurement that does not exist: the VRAM peak is
unset (`quality-rubric.md:226,244`; `tasks.md:166` says T056 "cannot be ticked. This is not Q4 green").
The other limits — procedural rig, screen-space glass model, SDR emissive container — are recorded
model/content bounds, not missing artifacts; they are kept as limits, not passes. 4.3's sky/atmosphere
and 4.6's history/teleport sub-criteria are likewise not tested and are not counted as passes.

Reviewer: an independent agent reviewer dispatched for the T057 re-run. I did not author T056, T057 or
the B1–B6 fixes, did not write the harness, content, evidence or capture records, and share no session or
worktree with their authors. I reviewed branch `review/q4-t057-r2` at `29af718`, which is the current
`origin/main`, in `.kilo/worktrees/review-q4-t057-r2-6bc038f659f81fd8`. I changed only this file. No
harness/content code, image, evidence file or prior review was edited, and nothing was rebuilt or
re-captured.

## 1. Review inputs and method

Read: `AGENTS.md`, `docs/GIT_WORKFLOW.md`, the Q4 sections of
`specs/004-quality-renderer/{quality-rubric,evidence-q4,evidence-q4-harness,review-q4,tasks}.md`,
`evidence/t056/README.md`, `measurements.json` (including the coverage, b1/b2, motion and residency
blocks), the eleven capture records (adapter, arguments, executable/package hashes, image hashes), the
eleven committed frames, the six committed crops, the seven material sidecars and the harness sources
cited below.

What I did beyond reading:

- Hashed all eleven committed frames; every SHA-256 matches `measurements.json` `images` and every
  capture record's `image_sha256` (§2). Opened the six committed crops as images.
- Re-ran the committed tools: `coverage_metrics.py` (glass, HDR, HUD runs, skinned motion, WARP
  parity) and `defect_metrics.py --only b2` on the committed `01` — the numbers below are reproduced
  from the PNGs, not copied from the record.
- Inspected the frames at 1× and in my own 2×/3×/4×/8× crops (generated outside the repository) of the
  grating shadow, cube front face, dielectric and metal spheres, both skinned poses, the glass edge and
  the HUD.
- Re-derived the grating shadow's screen geometry from the documented scene identity (eye
  `(4.5, 2.8, 6.5)`, look-at `(0, 0.8, 0)`, 45° vertical FOV, light direction `(0.45, -1, -0.30)`,
  16-unit ortho light extent) to check that the visible shadow pattern is the caster's 8×8 hole grid.
- Measured the glass-induced displacement and blur of the marker cube's right edge independently of the
  recorded correlation fit.
- Sampled the HUD's pixel runs directly (border, cells, meters, crosshair).

What this review is not: I did not rebuild `q4_harness.exe`, did not re-run the capture set and did not
reproduce the pre-tonemap `-HdrProbe` readback (no harness binary or `out/content` packages exist in
this worktree). The probe numbers in §3 (B-5) are therefore read from the record; the display consequence
is measured from the committed `02`/`10`/`11`. The probe is not an independent readback.

## 2. Artifact identity

| File | SHA-256 (recomputed, matches record) | Record | Adapter |
|---|---|---|---|
| `01-static-opaque-dielectric-metal.png` | `e686a8d7…b5e57` | `hero_dielectric-static_opaque.json` | Intel(R) Graphics |
| `02-alpha-tested-emissive.png` | `a1abea25…d0a8c` | `hero_emissive_sign-alpha_emissive-hdr.json` | Intel(R) Graphics |
| `03-moving-frame-1.png` | `d43bf34c…a33f6` | `hero_character_skin-moving-frame1.json` | Intel(R) Graphics |
| `04-moving-frame-8.png` | `6dc8e676…05afc` | `hero_character_skin-moving-frame8.json` | Intel(R) Graphics |
| `05-transmissive-environment.png` | `22fb8933…a92ba` | `hero_glass-transmissive_environment.json` | Intel(R) Graphics |
| `06-warp-static-opaque.png` | `fba5eb59…fcc979` | `hero_dielectric-static_opaque-warp.json` | Microsoft Basic Render Driver |
| `07-warp-transmissive-environment.png` | `6509a33d…8ce3b95` | `hero_environment-transmissive_environment-warp.json` | Microsoft Basic Render Driver |
| `08-hud-overlay-display.png` | `118b5c8b…4c3d27` | `hero_dielectric-hud-overlay.json` | Intel(R) Graphics |
| `09-transmission-reference.png` | `6d816953…febca` | `hero_glass-transmission-reference.json` | Intel(R) Graphics |
| `10-emissive-sdr-reference.png` | `d8ee494e…b6fe` | `hero_emissive_sign-alpha_emissive-sdr.json` | Intel(R) Graphics |
| `11-emissive-hdr-rolloff.png` | `34168a5e…d5a997` | `hero_emissive_sign-alpha_emissive-rolloff.json` | Intel(R) Graphics |

All eleven records name backend `d3d12`, a 1280×720 image, the same executable SHA
(`4a8d931d…a3ea`) and a `package_sha256`. `08` is the only frame that renders the scene at 960×540
(`-RenderScale 0.75`) and composites the HUD at 1280×720; the other ten render and display at 1280×720.
`05`/`09` are the same scene with and without the glass draw (`-NoTransmission`), so they are a valid
pair.

Two traceability notes, neither an image defect:

- `03` and `04` are **two separate single-frame runs** (`-Frames 1` and `-Frames 8`), not frames 1 and 8
  of one process. The pose is a function of the frame index and the two images are deterministic, so the
  pair is still a valid motion pair, but the prior review's "one 8-frame process" description (and the
  record's "eight consecutive" phrasing) is not what the capture records show.
- The eight-frame adjacent-pair table in `measurements.json` (`motion.adjacent`) comes from
  `out/motion2/f1..f8.png`, which are **not committed** (the commands record the `out/` path). Only the
  endpoints `03`/`04` are committed; the committed pair reproduces the key figures I use in §3 (B-3),
  but the adjacent-pair numbers are not reproducible from the repository.

## 3. Re-evaluation of B-1 … B-6

Each row states the prior blocker, the visible evidence in the committed frames, the source mechanism,
and whether the blocker is closed. "Closed" means the artifact exists and the defect is no longer
visible; it is not a dimension verdict (those are §4).

### B-1 — alpha-tested caster casts a solid shadow — **CLOSED (visible, soft)**

`crops/grating-shadow-cutout.png` and `02` show the grating's shadow as a **perforated lattice**, not a
solid rectangle. At 8× the shadow band is a grid of small light/dark cells; the sign's shadow next to it
is still solid, which is the expected difference between an alpha-tested and an opaque caster.

The mechanism is in the fixed source, not inferred from the caption:
`shaders/q4_hybrid.hlsl:105-127` passes the UV into the shadow pass and `PSShadowAlpha` clips on
`baseColor.a` against the same cutoff the visible pass uses; `harness_pipeline.cpp:160-180` gives the
shadow root signature the material table and the linear sampler; `:330` compiles `PSShadowAlpha`; `:434-450`
names it in the shadow PSO; `harness_render.cpp:317-345` binds each draw's material in the shadow loop.
The pane and the in-world GUI are excluded from that pass (`harness_render.cpp:328`), so the only
perforated shadow is the grating's.

I re-derived the projection: the 8×8 hole centres land on a 227×31 px band at x≈570–796, y≈406–437, with
pitches ≈19.8 px and ≈13.2 px; the region I sampled (561–804 × 397–444, that band plus 8 px) contains
exactly that lattice. The band is heavily foreshortened because the light grazes the vertical panel
(`N·L ≈ 0.26`), so the holes read as a soft moiré rather than eight crisp circles. That is the recorded
residual, not a remaining solid-shadow defect: the shadow map is 2048² over 16 units and the bars are
only 2–3 shadow texels wide (`evidence-q4.md:314-319`; 3×3 PCF at `q4_hybrid.hlsl:198-219`).

**Record caveat.** The `b1` entry in `measurements.json` (`dark_components` 4, `lit_share` 0.46 %) is
the **B1/B2-session** frame, and its own `scope` says the numbers no longer describe the committed
frames. The current `02`'s shadow is visibly more perforated than that row; I rely on the committed crop
and the projected geometry, not on the stale count. `evidence-q4.md:251` still points at §5.7's
historical measurement for this row; that pointer should be updated (N-2 below).

### B-2 — dielectric surface response is speckle — **CLOSED**

`01` (and the shared ground in `03`/`04`) no longer carries the 1–2 px white dot field. Recomputed on the
committed `01` with `defect_metrics.py --only b2`:

| Region | high-pass sd | isolated spikes |
|---|---|---|
| dielectric sphere | 0.947 | 0 |
| dielectric cube face | 3.354 | 0 |
| ground, near | 0.402 | 0 |
| ground, mid-distance | 1.151 | 0 |

The rejected frame's corresponding values were 16.21 / 13.31 / 11.13 / 5.71 with 550 / 425 / 0 / 0
isolated spikes (`measurements.json b2.before`). The authored normal is now smooth: recomputing the tilt of the committed
`content/hero/dielectric/normal.png` gives a mean of **4.15°** (p95 8.4°), against the 58.08° broken
helper recorded in `evidence-q4.md:321-340`. The sidecar's "painted, slightly worn panel" intent
(`content/tools/generators/dielectric.py:8-9,43-55,78`) is consistent with a 4° normal.

**Residual, not a blocker.** At 1:1 the cube's front face still shows strong relief (the 2× crop reads
as rough cast/plaster). That is a 4° normal lit at a grazing angle, not the old per-texel hash; the
high-pass energy is under 4/255 and there are no isolated spikes. The recorded `b2.after` row (2.394 /
2.852 / 0.757 / 0.662; 2 spikes, all on the sphere) is the superseded B1/B2 frame; the committed frame is
smooth on all four ROIs with zero spikes.

### B-3 — sparsely skinned category / skinned motion missing — **CLOSED, mannequin-limited**

`03`/`04` and `crops/skinned-pose-2x.png` show a skinned humanoid whose right arm and elbow move between
frames 1 and 8. Recomputed from the committed pair (`coverage.skinned_motion`): **5 758 px change above
2/255 (10.6 %) inside the character box (850–1020 × 150–470)**, 47 732 px outside it (5.2 %, the rigid
cube and shadows). The left arm's elbow deforms smoothly (no candy-wrapper pinch); the raised arm's
elbow at frame 8 is a hard ~90° bend, and its outer side shows a facet/crease from the two-influence
linear blend. The silhouette and suit material stay attached; no weight smear or material swim is
visible at 2×.

Mechanism: `mesh.cpp:306-336` builds a 12-bone humanoid (805 vertices, 4 224 indices, 4 influences), and
`shaders/q4_hybrid.hlsl:105-114` applies the same host linear-blend palette in the shadow pass as
`VSMain` does in the visible pass, so the character's shadow follows the pose. The 4× crops show a
smooth elbow and a stable head/torso between the two poses.

**Limits.** This is a procedural mannequin, not a retail mesh or game animation: the torso's horizontal
bands and the sub-pixel weave read as a ribbed suit; at ~90° of elbow bend the two-influence LBS produces
a hard crease/facet on the outer elbow; the head shows the skin material's tiling seam. Those are
consequences of the documented procedural rig, not weight smear, and they do not block 4.5/4.6 as
"material and silhouette stability on a procedural rig". They are not an authored-character identity pass.

### B-4 — no HUD in any capture — **CLOSED**

`08` and `crops/hud-texel-4x.png` contain a HUD drawn at the display extent after tonemap, plus an
in-world GUI. My direct pixel sample of `08`:

- health plate border runs at x **24–25** and **236–237**, exactly 2 px;
- eight filled cells at x 35–51, 55–71, … 175–191: 17 px wide on a 20 px pitch;
- top-left meter rows at y 24–31 and 38–45, 56 px and 80 px long (the 0.35/0.50 fills of 160);
- centre crosshair arms 6 px long at 1 px thickness;
- the cyan mask spans x 24–1256, y 24–695, all integer-aligned.

Mechanism: `shaders/q4_hud.hlsl:1-13,34-126` is a display-space overlay with **no scene sampler, no
fog and no upscale**; `harness_render.cpp:495-517` runs it after the tonemap pass into the 1280×720
display target; `harness_pipeline.cpp:503-514` creates the HUD PSO with depth writes off. The scene is
rendered at 960×540 (`-RenderScale 0.75`), so the upscaled scene edges are soft (frame 08's cube edge
ramps over 2–3 px where `01` steps in 1 px) while the HUD border stays exactly 2 px — the required
contrast between a render-extent GUI and a display-extent HUD. The in-world GUI is drawn in the scene
pass at render extent (`PSWorldGui`, `harness_render.cpp:383-394`), which is why it is the soft element.

**Residual.** "Not fed into motion history" is **structural, not tested**: the harness has no temporal
history at all, so the HUD cannot enter a history the path does not have. It is not evidence of a
history-exclusion test.

### B-5 — HDR emissive > 1.0 / ACES not exercised — **CLOSED as exercised (display clips at 16×)**

`02` (authored 16×), `10` (scale 1) and `11` (scale 2) exist. Recorded pre-tonemap probe values
(`measurements.json coverage.hdr_probe`, not re-run here): scale 1 peak **1.0059** with 0 px > 8.0;
authored 16× peak **15.2109** with **5 541 px > 8.0**; scale 2 peak **1.9531** with 0 px > 8.0. So
radiance above 1.0 is preserved into the R16G16B16A16_FLOAT scene target rather than clamped before the
tonemap. The scale is the sidecar value applied after sRGB decode (`q4_hybrid.hlsl:315-317`;
`harness.cpp:339-345`; `main.cpp:316-322`), not a metadata-only field.

**Display residual.** At the authored 16× the 8-bit display is **fully saturated**: recomputed from `02`
and `10`, the 5 920 mask pixels are at mean **254.948** with sd **0.282** and **5 920 / 5 920**
saturated, versus `10`'s mean 227.74 / sd 8.965 / 0 saturated. The **2× diagnostic** (`11`) is the
frame that shows ACES rolling off: mean 242.989, sd 5.326, **0 saturated**. The rubric's "keeps detail
above 1.0 rather than clipping to a flat white" is therefore evidenced by the scene-target probe and the
2× control, **not by the authored `02` alone**; `02`'s bars are flat white by design at that exposure.
The packaged texture is still an SDR mask (the C-10 container decision), which remains a limit.

### B-6 — no transmission result; glass was a tinted card — **CLOSED as a bounded screen-space model**

`05` vs `09` (no glass) and `crops/glass-refraction-2x.png`: the pane is now see-through, refracting and
frosted. Independent of the recorded correlation, I measured the marker cube's right edge inside the pane:

| Row (y) | `09` edge x | `05` edge x | displacement (05 − 09) | luminance ramp, `09` → `05` |
|---|---|---|---|---|
| 260 | 599 | 606 | +7 px | 1 px step → ~7 px ramp |
| 280 | 599 | 607 | +8 px | 1 px step → ~10 px ramp |
| 300 | 599 | 606 | +7 px | 1 px step → ~9 px ramp |
| 320 | 599 | 606 | +7 px | 1 px step → ~9 px ramp |

The cube's edge is displaced by the refraction and smeared by the five-tap frost, and the pane lifts the
cube's dark face by ≈+10 to +20/255. The recorded fit: luminance corr **0.9336**, variance ratio
**0.3929**, 58.5 % of pane pixels within 20/255, best background shift **−9 px** (13.5 % SAD
improvement). The recorded −9 px is measured over the whole `[540,190,700,330]` region, which mixes the
Fresnel-lifted sky with the ground; it is a different aggregate from the cube-edge displacement, and its
sign need not match. Both are non-zero displacements at the same scale, so the pane is refracting, not
copying. (The 20–80 % band of the edge is 1–2 px; the ~7–10 px ramp is the total smeared transition.)

Mechanism: `q4_hybrid.hlsl:433-475` traces one refraction event through a 0.35-unit slab, samples the
pre-glass scene copy with a five-tap frost, tints by `baseColor`/alpha, adds a Fresnel environment
reflection and composites opaquely; `harness_render.cpp:408-446` copies the opaque scene, draws the glass
after it, and `harness_pipeline.cpp:426` disables depth writes. The pane is deliberately **excluded from
the opaque shadow pass** (`harness_render.cpp:328`, `evidence-q4.md:265-266`), which is the recorded
policy: no caustics, no attenuated glass shadow.

**Limits.** Single thin slab, one refraction event, five-tap frost, no dispersion, no caustics, no
per-layer depth rejection, and no engine-side transmission. The pane's upper half is a Fresnel grey card
over the **clear-colour** background (no sky is drawn), and the headline `mean_abs` 25.888 is dominated
by that Fresnel lift over the black upper region, not by scene transmission; the transmission evidence is
the edge displacement/blur in the lower half and the corr/shift fit, not the mean absolute difference
alone. The pane's silhouette edges are aliased (no MSAA), like every other edge in this harness.

## 4. Verdicts by dimension

| Dimension | Verdict | Artifact and reason |
|---|---|---|
| **4.1 Materials** | **accept** (with the documented limits) | `01`: dielectric and metal are distinguishable without moving the camera; the metal is darker/tinted with a tight specular lobe, the dielectric is a painted panel with a 4.15° normal and no speckle. `02`: the grating's cutout is clean and the sign's bars are distinct. `10`/`11`: the pipeline carries >1.0 into the tonemap and ACES rolls off at the 2× control. `05`: glass is a bounded screen-space material, not an engine transmission. Limits: SDR emissive container, authored 16× display clipping, analytic-env-only metal reflection, screen-space glass. |
| **4.2 Lighting and shadows** | **accept** | `01`/`03`/`04`: one key from the upper left, contact shadows touch, no visible acne or peter-panning at 1280×720; shadow luminance is stable across the moving pair. `02`: the cutout now casts a perforated shadow (B-1); the sign next to it still casts a solid one. `05`: the pane does not receive the opaque direct term and is excluded from the shadow pass by recorded policy. Residual: the cutout shadow's holes are soft/foreshortened (the recorded B-1 residual), and the glass casts no caustic/attenuated shadow (the model does not exist). |
| **4.3 Indirect light, reflections, atmosphere** | **accept (limited)** | `01`: the dielectric sphere's shadow side stays in the ~70–98/255 range (sampled mean 98, min 73) with the terminator and silhouette readable, and the shadowed ground separates from the ≈166/255 lit ground. `05`: below-horizon reflections return the warm `envGround (0.12,0.10,0.08)` (`harness_render.cpp:218-220`), not black; the far ground converges to a depth-consistent fog band. `08`: the HUD over the far ground is crisp and un-tinted. **Not tested:** a drawn sky / atmospheric HDR range — the scene target is cleared to `(0.012, 0.014, 0.020)` (`harness_render.cpp:364`), while the analytic environment is only the reflection/probe source; and reflection of scene content — the metal reflects the analytic gradient only. The frames show a black backdrop, not a sky, and the evidence's "sky" wording at `evidence-q4.md:250,252` overstates it (N-3). |
| **4.4 Darkness and readability** | **accept** | `01`: the darkest surface areas keep separation and silhouette; the backdrop is a void, not a surface. `02`: the clamped-16× bars are flat white but remain **distinct bars**, not one bloom-like blob; the panel, its frame and the ground stay separated, and no halo swallows detail. `11`: the 2× control keeps a non-zero spread with 0 saturated pixels, so the tonemap compresses rather than truncates. No numeric contrast threshold is invented (the rubric forbids one); this is a visual verdict with sampled values. |
| **4.5 Identity** | **accept (limited)** | `01`: metal vs dielectric, `02`: perforated grating and framed emissive sign, `05`: a recognisable glass pane, `03`/`04`: the skinned humanoid keeps its suit and silhouette through the pose change. Limits: the character is a **procedural mannequin** (ribbed torso, tiling seam on the head, sub-pixel weave at 1×), not an authored character; the large environment is authored as an industrial wall with rivets but mapped as the ground, so at this tiling the rivets are below the texel density and identity is **not proven**; the glass's upper half is a grey card over the void. |
| **4.6 Motion** | **accept (limited)** | `03`→`04`: only the cube/shadow and the character change; 5 758 px inside the character box (10.6 %), 47 732 outside (5.2 %); no material swim or weight smear observed at 2×; the skinned shadow uses the same palette. The eight-frame adjacent-pair table in `measurements.json` reports 4.79–7.09 % of pixels changing per pair, but those `out/motion2` frames are uncommitted (N-4). **Not tested:** history/reprojection ghosting and cut/teleport reset — the harness has no temporal history, so this is the absence of a mechanism, not a pass. |
| **4.7 HUD / overlay** | **accept** | `08`: display 1280×720 after a 960×540 scene; the HUD's 2 px border, 20 px cell pitch and integer-aligned crosshair are exact; no scene sampler, no fog, no upscale (`q4_hud.hlsl:1-13`); the in-world GUI stays at render extent and is visibly softer. The T086 off-path CPU ordering result (35/35) is still mechanism evidence only; the provider branch remains NOT RUN. **Residual:** motion-history exclusion is structural/untested, because no history exists. |

## 5. Documented limits (recorded as limits, not passes)

1. **Procedural rig.** The skinned character is a 12-bone procedural humanoid with a host linear-blend
   pose, 805 vertices and 4 influences (`mesh.cpp:306-336`; `measurements.json coverage.skinned_mesh`).
   It proves the skinning path and material/silhouette stability, not a retail mesh or animation.
2. **Screen-space glass model.** Single thin slab, one refraction, five-tap frost, no dispersion,
   caustics, multiple internal reflections or per-layer depth rejection; the pane is excluded from the
   shadow pass (`evidence-q4.md:264-266`; `measurements.json coverage.glass_model_limits`).
3. **SDR emissive container.** The >1.0 radiance is the sidecar `emissiveScale` applied after sRGB decode
   into the HDR scene target; the packaged texture is still an SDR mask (the C-10 container decision).
   The authored 16× display is fully saturated; the detail-above-1.0 case is the probe plus the 2×
   control.
4. **VRAM peak unset.** No DXGI budget or process GPU-memory counter was sampled; the adapter's reported
   134,217,728 bytes is dedicated memory, not use (`quality-rubric.md:226,244`; T056 PARTIAL at
   `tasks.md:166`). The residency bookkeeping (33,554,512 bytes uploaded, 0 deferred, 0 over budget at a
   256 MiB budget / 64 MiB throttle) is `ResidencyPolicy` accounting, not a VRAM measurement.

Two further coverage limits, not passes: **no sky is drawn** (the backdrop is the clear colour), so
4.3's atmospheric-range and scene-reflection clauses are NOT TESTED; and **no temporal history exists**,
so 4.6's ghosting/teleport clauses are NOT TESTED.

## 6. Non-blocking findings

**N-1. Stale environment sidecar.** `content/materials/hero_environment.mat.json:7-9` still records
`residency.state: "blocked"` with "no Q4 frame exists", while `measurements.json` residency and
`quality-rubric.md:214-216` record 33,554,512 resident bytes, 0 deferred and 0 over budget, and
`content/manifest/budget-rows.json` records the cell. The statement is conservative but false; correct
it when the sidecar is next touched. This is a record defect, not an image defect.

**N-2. The B1/B2 metric rows are historical.** `measurements.json b1.scope` and `b2.scope` say the
numbers no longer describe the committed frames; `evidence-q4.md:251` still points at §5.7's historical
measurement for the current cutout-shadow row. The current `02`'s shadow is visible in
`crops/grating-shadow-cutout.png` and matches the projected 8×8 hole grid (B-1), and the current `01`
recomputes to 0.947 / 3.354 / 0.402 / 1.151 with 0 spikes (B-2). Update the pointers so a later reader
does not quote the superseded counts.

**N-3. "Sky" wording.** `evidence-q4.md:250` says the grating's holes show "sky above the horizon"; the
region is the **clear colour** `(0.012, 0.014, 0.020)` (`harness_render.cpp:364`), and `:252` says the
Fresnel reflection lifts the "sky region". The analytic environment (`envZenith`/`envHorizon`/`envGround`,
`harness_render.cpp:218-220`) is used for reflections, not drawn. Rename the background or draw the sky;
until then the frames cannot support an atmospheric/sky claim.

**N-4. Motion adjacent-pair provenance.** The `motion.adjacent` table comes from uncommitted
`out/motion2/f1..f8.png` (`measurements.json commands`). The committed `03`/`04` support the 4.6
conclusion, but the eight-frame statistics are not reproducible from the repository. Commit the series
or state that the committed pair is the evidence.

**N-5. HUD history exclusion is structural.** The 4.7 clause "not fed into motion history" is not
image-tested: the harness has no temporal history (`evidence-q4.md:235-237`). The display pass cannot be
in a history the path does not have.

**N-6. Cutout shadow is heavily foreshortened.** The key grazes the vertical grating (`N·L ≈ 0.26`), so
the 8×8 holes project into a 227×31 px band and the 2–3 texel bars plus 3×3 PCF make the holes soft. A
capture with the grating turned toward the key would show the hole shape unambiguously; the current
frame shows the cutout topology but not eight round holes. The residual is recorded at
`evidence-q4.md:314-319`.

**N-7. The glass's mean absolute difference is not the transmission signal.** The recorded 25.888 mean
abs over `[540,190,700,330]` is dominated by the Fresnel reflection over the black upper region; the
transmission evidence is the +7/+8 px cube-edge displacement and the 1 px → ~7–10 px edge ramp I
measured, plus the corr/shift fit. Do not cite the mean abs as "how much is seen through the glass".

**N-8. Character material at 1×.** The woven normal is sub-pixel at the captured size; the visible
material is dominated by the torso's tiling bands and the head's seam. The suit reads as a dark ribbed
suit rather than a woven hero asset. This is the procedural-rig/content-use limit, not a weight smear.

**N-9. Environment identity is not proven.** `hero_environment` is authored as an industrial wall with
rivets, grime and rust streaks, but it is mapped as the 24-unit ground; at this tiling the rivets are
below the texel density and the panel repetition is visible. Keep the frame as evidence that the large
cell renders and its residency accounting works, not as identity proof.

**N-10. Prior non-blocking findings, status.** The prior N2 (emissive scale metadata-only) is fixed:
the scale is applied at `q4_hybrid.hlsl:315-317`. N3/N4 (motion and WARP counts not exactly
reproducible) are superseded: the current WARP parity recomputes to 4 px (static) and 1 px
(transmissive), and the current `03`/`04` pair recomputes to 5 758 + 47 732 = 53 490 changed px
(5.804 %). N5 (N-1 here) and N6 (N-9 here) remain. N7 (glass shadow policy) is now explicit and
implemented. N8 (floating props) is still geometry, not bias.

## 7. What this review does not establish

No engine or fork frame, no retail character or animation, no drawn sky, no scene-content reflection, no
history/reprojection or cut/teleport behaviour, no VRAM peak, no dispersion/caustics/multiple internal
reflections, no second host and no camera path. The eleven frames are one pose per category (the moving
pair apart), 1280×720, one Intel(R) Graphics adapter at driver 32.0.101.8331, with WARP frames for
categories 1 and 4 only. The pre-tonemap probe values are the record's, not an independent readback.

## 8. Verdict

**Overall: BLOCKED for Q4 — on the unmeasured VRAM peak (B4), not on the art.** B-1 … B-6 are closed on the
committed frames; 4.1–4.7 are `accept` (4.3, 4.5, 4.6 limited; 4.7 with a structural note). **No
remaining art blocker.**

**The one remaining blocker.** The VRAM peak is not measured: `specs/004-quality-renderer/quality-rubric.md:226`
("VRAM peak | not measured — no DXGI budget or process GPU-memory counter was sampled") and `:244`
("B4 VRAM peak | **unset**"), with `specs/004-quality-renderer/tasks.md:166` recording T056 as PARTIAL and
"cannot be ticked". Measuring a DXGI budget or a process GPU-memory counter, recording it in
`measurements.json` and setting or explicitly declining B4 would lift it. The adapter's reported
134,217,728 bytes is dedicated memory, not use, and must not be substituted.

**If the frame review is read on its own, it is ACCEPTED WITH LIMITS**: the procedural rig,
screen-space glass and SDR emissive container are recorded model/content limits, and the untested
sky/atmosphere and history/teleport clauses are recorded as not tested. It is recorded as BLOCKED for
Q4 because the task asks for the Q4 verdict and the project's own T056 row says Q4 is not green without
the VRAM peak.

Three record defects should be corrected before the Q4 record is final; they do not change the image
verdicts: the stale environment residency (`content/materials/hero_environment.mat.json:7-9`), the
"sky" wording (`specs/004-quality-renderer/evidence-q4.md:250,252`), and the historical B1/B2 pointers
(`specs/004-quality-renderer/evidence-q4.md:251,340-343`; `measurements.json b1.scope,b2.scope`).

## 9. Recomputed numbers (for a later reviewer)

| Quantity | Value | Source |
|---|---|---|
| b2, committed `01`, sphere / cube / near ground / mid ground | 0.947 / 3.354 / 0.402 / 1.151 high-pass sd; 0 / 0 / 0 / 0 isolated spikes | `defect_metrics.py --only b2 --profile static 01…`, this review |
| dielectric normal mean tilt | 4.15° (p95 8.42°, max 14.77°) | recomputed from `content/hero/dielectric/normal.png` |
| glass edge, `09` → `05` | +7 to +8 px displacement; 1 px step → ~7–10 px ramp | this review, rows y 260–320 |
| HUD health border / cells | 2 px at x 24–25 and 236–237; eight 17 px cells at 20 px pitch | this review, frame 08, y 678–680 |
| HUD meters / crosshair | 56 px and 80 px fills; 6 px × 1 px arms | this review, frame 08 |
| skinned pair change | 5 758 px inside the character box (10.6 %), 47 732 outside (5.2 %) | `coverage_metrics.py`, this review |
| WARP parity | 4 px (static), 1 px (transmissive), max abs 182 / 158 | `coverage_metrics.py`, this review |
| emissive, authored 16× vs 2× | 254.948 ± 0.282 (5 920/5 920 saturated) vs 242.989 ± 5.326 (0 saturated) | `coverage_metrics.py`, this review |
| grating shadow projection | hole-centre band 227×31 px at x 570–796, y 406–437; pitches 19.8 px / 13.2 px | this review, from the documented camera/light identity |
