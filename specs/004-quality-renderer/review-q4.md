# Q4 art review (T057), independent

**Verdict: BLOCKED — Q4 is not accepted.** Two rubric dimensions are rejected on visible
defects in the committed frames (an alpha-tested caster casts a solid shadow; the dielectric
surface response is high-amplitude speckle, not the authored subtle orange-peel). Three more
dimensions are blocked because the required artifact does not exist (skinned motion in 4.5/4.6,
the HUD in 4.7), and two named criteria were not exercised (HDR emissive, glass transmission).
None of those missing rows is upgraded to a pass here.

Reviewer: an independent agent reviewer dispatched for T057. I did not author T056, did not
write the harness, content, evidence or capture records, and I share no session or worktree
with their author. I reviewed the committed tree at `143a0df` in
`.kilo/worktrees/review-q4-t057-art-fd13f140a693ca11`, branch `review/q4-t057-art`.
I changed only this file. No harness/content code and no evidence image was edited, and
nothing was rebuilt or re-captured.

## 1. Review inputs and method

Read in full: `AGENTS.md`, `docs/GIT_WORKFLOW.md`,
`specs/004-quality-renderer/{quality-rubric,evidence-q4,evidence-q4-harness,tasks}.md`,
`evidence/t056/README.md`, `measurements.json`, the nine `evidence/t056/captures/*.json`
records, the seven committed frames and the grating crop, the seven `content/materials/hero_*.mat.json`
sidecars, the seven `content/hero/*/provenance.json` records, and the harness sources cited
below. I also opened one authored environment tile for a colour/feature comparison.

What I did beyond reading:

- Hashed all seven committed frames and compared them with `evidence/t056/README.md` and the
  `image_sha256` field of every capture record (all match; §2).
- Re-verified all 29 authored assets in the seven `provenance.json` files against their
  recorded SHA-256 and licence/origin fields (29/29 match, all `CC0-1.0`,
  `retail_text_consulted: "no"`).
- Recomputed, from the committed PNGs, the two numeric claims that are images: the
  frame-1/frame-8 difference and the hardware/WARP parity (§2.3–§2.4).
- Sampled pixel values and region luminance in the committed frames where a verdict needed a
  number (§4.4, §5). No numeric contrast threshold is invented; the rubric's §4.4 still has
  none.

What this review is not: I did not rebuild `q4_harness.exe`, did not re-run the capture set,
and did not produce an engine frame. Image verdicts rest on the seven committed frames and
their committed records. Every statement about light direction, shadow shape, geometry and
shading behaviour was checked against the cited source line, not inferred from a caption.

## 2. Artifact identity checks

### 2.1 Frames and capture records

| File | SHA-256 (committed, recomputed) | Record | Adapter |
|---|---|---|---|
| `evidence/t056/01-static-opaque-dielectric-metal.png` | `1c304e1633bd…8102` | `hero_dielectric-static_opaque.json`, `hero_metal-static_opaque.json` | Intel(R) Graphics |
| `evidence/t056/02-alpha-tested-emissive.png` | `819dd294da2d…7186` | `hero_grating-alpha_emissive.json`, `hero_emissive_sign-alpha_emissive.json` | Intel(R) Graphics |
| `evidence/t056/03-moving-frame-1.png` | `abe8f482a375…5537` | `hero_dielectric-moving.json` | Intel(R) Graphics |
| `evidence/t056/04-moving-frame-8.png` | `5d5dd3c9118b…fbb4` | `hero_dielectric-moving.json` | Intel(R) Graphics |
| `evidence/t056/05-transmissive-environment.png` | `1aedf7ff3ddc…b08b` | `hero_glass-…json`, `hero_environment-…json` | Intel(R) Graphics |
| `evidence/t056/06-warp-static-opaque.png` | `36db9e832c59…ad79` | `hero_dielectric-static_opaque-warp.json` | Microsoft Basic Render Driver |
| `evidence/t056/07-warp-transmissive-environment.png` | `2ef8b5e93e64…5c53` | `hero_environment-…-warp.json` | Microsoft Basic Render Driver |
| `evidence/t056/crops/grating-shadow-solid.png` | `4a66052d7f31…0061` | derived 2× crop, `02` | Intel(R) Graphics |

Every record names backend `d3d12`, the adapter above, a 1280×720 image, the same executable
SHA (`562a20d6…eb69`) and a `package_sha256` matching the cell's package. `05` is correctly
the same frame for `hero_glass` and `hero_environment` (one scene draws both); the README says
so. `03`/`04` are frames 0 and 7 of one 8-frame process.

### 2.2 Provenance

`content/hero/<cell>/provenance.json` exists for all seven cells, names a project-owned
generator or generator input, carries per-asset SHA-256, `licence: CC0-1.0` and
`retail_text_consulted: "no"`, and every `LICENSE` beside them is CC0-1.0. All 29 recorded
asset hashes match the committed files. The rubric §2 provenance gate is satisfied for the
cells this review judges.

### 2.3 Motion pair, recomputed

From the committed `03` and `04` (max-channel difference > 2/255, as the record describes):

| Quantity | Recorded | Recomputed | Note |
|---|---|---|---|
| changed pixels | 46,653 (5.062 %) | 48,408 (5.253 %) | counts differ; see N3 |
| mean absolute difference | 1.473 | 1.473 | exact match |
| max channel difference | 172 | 172 | exact match |
| changed bounding box | 442,239 – 775,488 | 442,239 – 774,487 | within 1 px |
| outside the box | "byte-identical" | 2 pixels differ by 1/255 at x = 441 | trivial, but the sentence is literally false |

The qualitative claim holds: the background is stable and all change is the cube and its
shadow. Shadow-region luminance is also stable between frames (mean 154.0 → 153.7,
155.0 → 153.0, 156.1 → 156.6 in three boxes over the cube/shadow; a far-ground control is
identical at 177.9), which is independent evidence against per-frame lighting drift.

### 2.4 WARP parity, recomputed

| Scene | Recorded | Recomputed | Note |
|---|---|---|---|
| `static_opaque` | 329 px (0.036 %), mean abs 0.081 | 465 px (0.0505 %), mean abs 0.081 | mean matches; count differs (N4) |
| `transmissive_environment` | 1 pixel, mean 0.070 | 1 pixel, mean 0.070 | match |

The software path renders the same image rather than a blank or a fallback frame. WARP frames
exist only for categories 1 and 4; category 2 and 3 have WARP **timing only** (no committed
frame). That is a coverage limit, not a parity result for those categories.

## 3. Coverage ledger — what is not evidence

T056 is PARTIAL. The ledger below is authoritative for this review; a blank or "NOT RUN" row
is not a pass.

| Cell | Package | Frame | Category | Judged |
|---|---|---|---|---|
| `opaque_dielectric` | yes | `01`, `03`, `04`, `06` | 1, 3 | partially — 4.1 reject, 4.6 rigid accept |
| `opaque_metal` | yes | `01`, `06` | 1 | accept (4.1, 4.4) |
| `alpha_tested` (grating) | yes | `02` | 2 | cutout accept, shadow **reject** (4.2) |
| `emissive` (sign) | yes | `02` | 2 | SDR readability accept; **HDR emissive NOT TESTED** |
| `skinned_character` | yes | **none** | 3 | **NOT RUN** — 4.5 and 4.6 skinned rows blocked |
| `transmissive_glass` | yes | `05`, `07` | 4 | **NOT TESTED as transmission**; identity limited |
| `large_environment` | yes | `05`, `07` | 4 | renders; identity limited by tiling/minification (N6) |

Recorded explicitly, and not converted to a pass anywhere below:

1. **6 of 7 cells are recorded.** `skinned_character` has a package, a slot and accounting,
   but no draw exists in the harness, in the content set, or in any frame. Category 3
   ("moving content") is **rigid-prop only**.
2. **HDR emissive is not claimed and was not exercised.** The frame is SDR; the sidecar's
   `emissiveScale: 16.0` is not applied by the harness (N2).
3. **Transmission is not claimed and was not exercised.** No frame shows a background through
   the glass; `q4_hybrid.hlsl:287-294` and the `alpha = 1.0` return at `:306` document a
   tinted approximation.
4. **VRAM peak (B4) is unset and not measured.** No DXGI budget or process counter was
   sampled; the adapter's reported 128 MiB is not a use figure.
5. **The HUD is not tested.** No capture contains one; the T086 off-path result is a CPU
   ordering check (35/35), not an image, and the provider branch is NOT RUN.
6. **No history/reprojection exists in the harness**, so the 4.6 ghosting criterion is
   **NOT TESTED**; the rigid sequence proves per-frame stability only.
7. Coverage is one host, one camera pose per category, one 30-frame window, and WARP frames for
   two categories only.

## 4. Verdicts by dimension

Labels are the rubric's `accept` / `reject` / `not applicable`, plus **blocked** where the
required artifact does not exist and **not tested** where a named sub-criterion was not
exercised. A blocked/not-tested row never counts toward acceptance.

### 4.1 Materials — **REJECT** (dielectric), accept (metal, grating cutout), not tested (HDR emissive, glass)

- **Reject — dielectric surface response.** The plane, cube and sphere all carry a
  high-amplitude, ~1–2 px white specular speckle. At 3× it resolves into isolated bright dots,
  not a surface; at 1× it makes the painted cell read as sandpaper/frosted stone. The authored
  intent is "painted, slightly worn … subtle orange-peel normal"
  (`content/tools/generators/dielectric.py:8-9,68-72`), and that is not what the frame shows.
  The speckle is present at every distance tested, including the sphere at roughly 1:1 texel
  density, so this is not only a distant minification artifact; the ground plane additionally
  minifies under a trilinear sampler with `MaxAnisotropy = 1`
  (`src/renderer/q4_harness/harness_pipeline.cpp:87-96`). Whether the root cause is authoring
  or filtering is not settled by the capture, but the cell is not plausible as the stated
  material and the other two cells inherit the same grain through the shared ground plane.
- **Accept — metal.** `01`: the sphere is darker and colour-tinted relative to the dielectric,
  with a tight, high-contrast specular cluster and a Fresnel rim; roughness variation breaks
  the highlight. It reads as metal, not plastic, and is distinguishable from the dielectric
  without moving the camera. The reflection has no scene content — that is §4.3, not a
  metalness failure.
- **Accept — alpha cutout.** `02`: light passes through the holes (near-black backdrop above
  the horizon, ground below); the bar edges are clean at this extent and the rust-tinted rims
  read as authored. That is the material/cutout behaviour, not the shadow (§4.2).
- **Not tested — HDR emissive.** `02`: bars are ~225/255 and the panel ~80/255; nothing clips
  and nothing exceeds SDR. The "keeps detail above 1.0 rather than clipping to flat white"
  criterion was not exercised.
- **Not tested — glass.** `05` shows an opaque-looking pale blue card; the authored
  `baseColor.alpha` (196–242) is not used by the harness. No material judgement of the glass
  cell as glass is possible.
- **Normal handedness.** Sidecar declares `opengl`; the shader flips Y once
  (`q4_hybrid.hlsl:215-219`). The shading is consistent across the cube's faces and the sphere
  with no visible inverted response, but an isotropic noise normal cannot *prove* handedness;
  the T050 fixture remains the authority. Accept as "no contradiction observed".

### 4.2 Lighting and shadows — **REJECT**

- **Reject — alpha-tested caster casts a solid shadow.** `02` and
  `crops/grating-shadow-solid.png`: the perforated grating projects a solid rectangular
  shadow with no holes. Cause is in the source, not the framing: the shadow pipeline state
  binds no pixel shader (`src/renderer/q4_harness/harness_pipeline.cpp:329-343` sets only
  `shadowDesc.VS`), and every draw goes through that state
  (`src/renderer/q4_harness/harness_render.cpp:242-254`). The shadow therefore does not match
  the caster's silhouette, and the alpha-tested category's lighting cannot be accepted.
- **Accept — key, falloff, contact, bias.** `01`, `03`, `04`: one key from the upper left,
  shadows fall consistently left/away (screen-left corresponds to +x for the fixed camera),
  contact shadows touch the sphere and cube base, and no acne or peter-panning gap is visible
  at 1280×720. The shadow-region luminance is stable across the two moving frames (§2.3).
  Note the floating props in `02` (0.4 units above the plane) and `05` (0.3 units): the gap
  between object and shadow there is geometry, not bias (`harness_render.cpp:71-78,86-90`).
- **Observation — glass shadow.** The glass quad is submitted to the shadow pass like an
  opaque caster (`harness_render.cpp:242-254`), and the visible result is a faint sliver of
  shadow at its base-left (≈155–160 vs ≈168 lit). It reads faint because of the ambient lift,
  not because the panel is correctly transmissive; the transmission/shadow policy still has to
  be decided together with the transmission model (N7). Not a second reject while transmission
  is unclaimed.
- **Accept — no `noShadows` light, no legacy stage** in these scenes; nothing to judge.

### 4.3 Indirect light, reflections and atmosphere — **ACCEPT (limited)**; two sub-criteria NOT TESTED

- **Accept — indirect lift.** `01`: the sphere's shadow side sits at 64–77/255 with the
  terminator and silhouette readable, and the cube's dark face is ~80/255 — raised by the SH
  probe without flattening contact. The shadowed ground separates from lit ground (region mean
  151, minimum 72, against a lit mean 178).
- **Accept — ground/horizon fallback and fog.** The reflection environment is the documented
  analytic one — `envZenith (0.25,0.35,0.55)`, `envHorizon (0.55,0.60,0.70)`,
  `envGround (0.12,0.10,0.08)` (`harness_render.cpp:164-166`) — so below-horizon rays return a
  warm dark grey, not black; the metal sphere's lower reflection samples (84–95/255) show it.
  `05`: the far plane converges to a light fog band (the shader's fog colour `(0.35,0.45,0.62)`
  at `q4_hybrid.hlsl:296-305`), which is depth-consistent in the frame. No negative/NaN
  artifacts are visible.
- **Not tested — reflection of scene content.** The reflection source is the analytic
  environment only; the metal sphere reflects a bright sky gradient and no scene structure.
  It is therefore impossible to check the reflection against what the camera sees. It is also
  impossible to check "atmospheric HDR colour keeps its range", because no sky is drawn: the
  scene target is cleared to `(0.012, 0.014, 0.020)`
  (`harness_render.cpp:267`), which tone-maps to the 12–21/255 backdrop in every frame, while
  the reflection samples a much brighter analytic sky. See N1.
- **Not applicable — HUD fogging** (no HUD).

### 4.4 Darkness and readability — **ACCEPT**

- The darkest reviewed *surface* areas (sphere shadow side 64, ground in shadow 72, darkest
  sign-panel sample 55) keep separation and silhouette; the darkest *image* area is the
  backdrop at 14–17/255, which is a void, not a surface.
- The brightest element is not a clipped blob: the emissive bars reach 227/255 with the panel
  frame and adjacent ground still separated, and no bloom halo swallows detail.
- Adjacent materials remain distinguishable without moving the camera (metal vs dielectric by
  value and hue; grating bars vs holes by silhouette; panel vs ground).
- Configuration for this row: ACES at exposure 1.0, 1280×720 render = display,
  `PlanReconstruction: Off`. **No numeric contrast metric was computed** (the rubric forbids
  inventing one), so this is a visual verdict with sampled values, not a measured threshold.

### 4.5 Identity — **BLOCKED** (skinned missing; glass not tested; environment limited); accept for the recorded static/alpha silhouettes

- **Accept — metal dielectric distinctness, grating and sign silhouettes.** The metal sphere
  reads as the authored brushed/cool conductor; the grating reads as a perforated plate; the
  sign reads as a framed panel with a glyph block. All three are recognisable at 1×.
- **Accept with limit — emissive identity.** The sign's *silhouette* is recognisable, but it
  does not read as self-luminous: bars at 225/255 against a 176–185/255 ground, no spill and no
  brightness above the SDR ceiling. Do not cite this frame as proof that the emissive cell is
  emissive; it proves the SDR mask renders.
- **Not tested — glass identity.** An opaque pale card is not glass. `05` cannot be cited for it.
- **Blocked — skinned character.** No frame exists, so neither "keeps its material and
  silhouette through motion" nor its static identity can be judged.
- **Limited — large environment.** The frame shows a pale panel grid with faint stains, but the
  authored "512-px panels, recessed seams, rivets, concrete grime and rust streaks"
  (`content/materials/hero_environment.mat.json`) is resolved neither in value nor in detail at
  this tiling and camera: rivets and seam depth are below the texel density, and the cell is
  authored as a wall but mapped as the ground plane. The panel repetition is visible. This is a
  scene-composition/content-use limit, not a rendering reject, and it means the frame does not
  prove environment identity. Distance stability (mip transitions) is **not tested** — one pose.
- **Correction to the evidence file.** `evidence-q4.md` §5.4 labels "byte-identical to the
  pre-merge capture" as 4.5 identity. That is reproducibility, not identity; I verified it
  (§2.1) but it does not support this dimension.

### 4.6 Motion — **BLOCKED** (skinned NOT RUN; history NOT TESTED); accept for rigid stability

- **Accept — rigid per-frame stability.** `03` → `04`: only the cube and its shadow change
  (~5 % of pixels inside a 333×249 box), the background is stable, and shadow luminance is
  stable (§2.3). No material swim or identity change is visible between the two frames.
- **Blocked — skinned motion.** No skinned frame exists (category 3 is rigid-only).
- **Not tested — history/reprojection ghosting.** The harness has no temporal history at all,
  so "no ghost" here is the absence of the mechanism, not evidence that ghosting is handled.
  `evidence-q4.md` §5.4 states this correctly; the sentence must not be read as a ghosting pass.
- **Not tested — cut/teleport reset**, same reason.

### 4.7 HUD / overlay — **BLOCKED** (no HUD in any capture)

- No committed frame contains a HUD or world GUI, so blur, texel sharpness, fogging, upscaling
  and motion-history inclusion cannot be judged.
- The T086 off-path result (`q4_reconstruction_test.exe` 35/35: scene → world GUI → display
  overlay, overlay excluded from reconstruction, one-to-one HUD texels, render-extent HUD
  refused) is accepted as evidence for the *ordering mechanism*, not as a HUD image verdict.
  The provider branch remains NOT RUN with its reason.

## 5. Accepted / rejected examples

| # | Dimension | Artifact (SHA-256) | Observation | Verdict |
|---|---|---|---|---|
| A1 | 4.1 metal, 4.4 | `01-static-opaque-dielectric-metal.png` (`1c304e16…8102`) | Metal sphere is darker/tinted, tight specular cluster, Fresnel rim; distinct from the dielectric without moving the camera | accept |
| A2 | 4.1 cutout | `02-alpha-tested-emissive.png` (`819dd294…7186`) | Background visible through the holes; clean bar edges at this extent | accept (cutout only) |
| A3 | 4.4 | `02-alpha-tested-emissive.png` (`819dd294…7186`) | Brightest element 227/255, not clipped; panel frame and ground keep separation | accept |
| A4 | 4.3 indirect | `01-static-opaque-dielectric-metal.png` (`1c304e16…8102`) | Shadow side of the sphere 64–77/255 with form intact; shadowed ground mean 151 (min 72) vs lit 178 | accept |
| A5 | 4.3 fallback/fog | `05-transmissive-environment.png` (`1aedf7ff…b08b`) | Below-horizon rays return `envGround (0.12,0.10,0.08)`, not black; far plane converges to a depth-consistent fog band | accept (limited) |
| A6 | 4.6 rigid | `03` (`abe8f482…5537`) → `04` (`5d5dd3c9…fbb4`) | Change confined to the cube/shadow box; background stable; shadow luminance stable | accept (rigid only) |
| A7 | parity | `06` (`36db9e83…ad79`), `07` (`2ef8b5e9…5c53`) | Same image as hardware (mean abs 0.081 / 0.070); not a blank or fallback | accept (categories 1 and 4 only) |
| R1 | **4.2** | `02` (`819dd294…7186`) + `crops/grating-shadow-solid.png` (`4a66052d…0061`) | Grating casts a solid rectangle; no hole-shaped shadow | **reject** |
| R2 | **4.1** | `01` (`1c304e16…8102`); also `03` (`abe8f482…5537`), `04` (`5d5dd3c9…fbb4`) | Dielectric plane/cube/sphere carry high-amplitude 1–2 px white speckle; authored "subtle orange-peel" is not reproduced | **reject** |
| R3 | 4.2 note | `05` (`1aedf7ff…b08b`) | The "transmissive" panel is submitted as an opaque shadow caster; only a faint sliver of shadow is visible (≈155–160 vs ≈168 lit) | observation (see N7) |
| R4 | 4.1/4.5 | `05` (`1aedf7ff…b08b`) | Panel is a flat tinted card; no see-through, no refraction, no thickness | **not accepted as transmission** |
| R5 | 4.1 | `02` (`819dd294…7186`) | Bars ≈225/255, ~ACES(1.0); `emissiveScale: 16.0` is not applied by the harness | **not accepted as HDR emissive** |

## 6. Blocking findings

**B-1 (visible defect, 4.2).** The alpha-tested grating casts a solid rectangular shadow.
`evidence/t056/02-alpha-tested-emissive.png` and `evidence/t056/crops/grating-shadow-solid.png`;
cause `src/renderer/q4_harness/harness_pipeline.cpp:329-343` (shadow PSO has no pixel shader)
with `harness_render.cpp:242-254` (all draws use it). Fixing it requires an alpha-tested shadow
pass (or a documented cutout shadow policy) and a new capture.

**B-2 (visible defect, 4.1).** The dielectric cell does not read as its authored material:
high-amplitude specular speckle across plane, cube and sphere, which resolves to isolated white
dots at 3× and reads as sand/frost at 1×. Authored intent:
`content/tools/generators/dielectric.py:8-9,68-72`; relevant sampling setting
`harness_pipeline.cpp:87-96`. Either the authoring amplitude/frequency or the filtering must
change, or the cell's stated surface must change to match what is rendered; then re-capture
and re-review. The same grain reaches the alpha and moving frames through the shared ground
plane.

**B-3 (missing artifact, 4.5/4.6).** `skinned_character` has no frame: the harness has no
skinned mesh, rig or animation (`harness_render.cpp:57-64`), and the content set has none
either. Category 3 is rigid-only, so "a skinned character keeps its material and silhouette
through motion" is NOT RUN, not passed. This blocks any claim that the moving category or Q4
is complete.

**B-4 (missing artifact, 4.7).** No capture contains a HUD. The 4.7 image criteria (texel
sharpness at display extent, no fog, no upscale, not fed into motion history) are NOT TESTED.
The T086 off-path CPU ordering result does not substitute for an image, and the provider branch
is NOT RUN.

**B-5 (unexercised criterion, 4.1).** The HDR emissive case was not exercised. `02` is SDR; the
harness adds the emissive texture with no scale (`q4_hybrid.hlsl:253-257`) while the sidecar
records `emissiveScale: 16.0` and `hdr.state: blocked`
(`content/materials/hero_emissive_sign.mat.json`). "Detail above 1.0 is preserved rather than
clipping" is NOT TESTED and must not be inferred from this frame.

**B-6 (unexercised claim, 4.1/4.5).** No transmission result exists. The panel writes
`alpha = 1.0` and uses a tinted environment term (`q4_hybrid.hlsl:287-294,306`); `05`/`07`
cannot be cited for glass identity or see-through. This blocks the transmissive half of
category 4.

B-3 to B-6 are coverage/evidence blockers, not rendering defects: they are "no artifact", not
"failed artifact". B-1 and B-2 are visible defects in artifacts that do exist.

## 7. Non-blocking findings

**N1. The scene identity text and the frame disagree about the sky.** `quality-rubric.md` §3
describes the scene as having "the analytic sky/horizon/ground environment". The frame's
background is a fixed clear, `(0.012, 0.014, 0.020)` at `harness_render.cpp:267`, giving the
12–21/255 backdrop, while the reflection samples the analytic environment
`envZenith (0.25,0.35,0.55)` / `envHorizon (0.55,0.60,0.70)` (`harness_render.cpp:164-166`) —
one to two orders of magnitude brighter. Either rename the identity to "analytic environment
used for reflection/probe; background = clear colour", or draw the analytic sky. As it stands
the frames cannot support any atmospheric/sky claim, and the metal reflection cannot be checked
against visible environment content.

**N2. The emissive scale is metadata only.** `emissiveScale: 16.0` appears in the sidecar but
nowhere in `src/renderer/q4_harness/`; the shaded result is the SDR mask at ≈1.0. Record in
`evidence-q4.md` that the harness does not apply the scale, so a reader does not infer that a
16× emissive is on screen.

**N3. The motion statistics are not exactly reproducible from the committed pair.** Recorded
46,653 changed pixels (5.062 %); recomputed 48,408 (5.253 %) under max-channel > 2/255, while
mean absolute difference (1.473) and max (172) match exactly and the bounding box matches within
1 px. Also, "everything outside that box is byte-identical" has two exceptions of 1/255 at
x = 441. State the metric and whether the count came from pre-quantisation buffers, or generate
the number from the committed PNGs.

**N4. Same issue for WARP parity.** Recorded 329 px (0.036 %) vs 465 px (0.0505 %) recomputed
for `static_opaque`; the mean absolute difference (0.081) matches exactly. The transmissive
case matches (1 pixel). The qualitative conclusion is unaffected.

**N5. Stale sidecar text understates the record.** `content/hero/environment.mat.json` still
says the residency budget is `blocked` because "no Q4 frame exists", while
`content/manifest/budget-rows.json` and `quality-rubric.md` §5.1 record the residency
measurement. A conservative statement, but now false; correct it when the sidecar is next
touched.

**N6. The environment frame cannot prove environment identity.** The cell is authored as an
industrial wall with rivets, grime and rust streaks; it is mapped as the ground plane and, at
this tiling, the authored features are below the texel density. Keep the frame as evidence that
the large cell renders and its residency accounting works — not as identity proof.

**N7. The glass shadow and the glass surface must be decided together.** The `alpha = 1.0` panel
is submitted to the depth-only shadow pass like an opaque quad, which is why it casts a shadow
at all; the visible sliver is faint only because the ambient term lifts the ground. When
transmission lands, decide explicitly whether a transmissive pane attenuates the key or discards
for the shadow pass, and record the policy with the transmission capture.

**N8. Floating props are geometry, not bias.** The alpha/emissive quads sit 0.4 units above the
plane and the glass 0.3 units (`harness_render.cpp:71-78,86-90`); their shadows are offset
accordingly. Record this in the scene identity so a later reviewer does not read peter-panning
into it.

## 8. What this review does not establish

No engine frame, no classic parity, no modern-look acceptance, no HUD image, no skinned motion,
no history/reprojection behaviour, no VRAM peak, no HDR radiance, no transmission, no camera
path, no second host and no claim beyond the seven committed frames and their records. T056
remains PARTIAL; this file records T057's independent verdict, and that verdict is not an
acceptance.

## 9. Verdict and exact blockers

**BLOCKED — Q4 is not accepted.** T057's review action is complete; the answer is no.

Blockers, in the order they should be closed:

1. **B-1** — alpha-tested grating casts a solid shadow (4.2 reject). Fix the shadow pass for
   cutouts, re-capture `02`, re-review.
2. **B-2** — dielectric surface response is speckle, not the authored painted panel (4.1
   reject). Resolve authoring vs filtering, re-capture `01`/`03`/`04`, re-review.
3. **B-3** — `skinned_character` has no frame; category 3 is rigid-only (4.5/4.6 blocked).
4. **B-4** — no HUD in any capture (4.7 blocked).
5. **B-5** — HDR emissive not exercised; frame is SDR and the 16× scale is not applied (4.1 not
   tested).
6. **B-6** — no transmission result; the panel is a tinted card (4.1/4.5 not tested).

What is accepted as far as it goes: metal distinctness, the alpha cutout (not its shadow),
darkness/readability, indirect lift and the below-horizon fallback, rigid per-frame stability,
and hardware/WARP parity for categories 1 and 4. None of these converts a blocked row into a
pass, and none of them is a Q4 acceptance.
