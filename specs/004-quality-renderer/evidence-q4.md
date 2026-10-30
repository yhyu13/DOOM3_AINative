# Q4 hybrid-quality evidence (T049–T055, T056, T057, T086)

Status: author evidence for issue [#65](https://github.com/yhyu13/DOOM3_AINative/issues/65), written
2026-10-30. This is **not** Q4 acceptance, not an engine frame and not an independent review. It records
what the portable slices do on this host and what could not run.

**Headline: T049–T055 and the T086 off-path branch are implemented as CPU-testable, backend-neutral
slices and their tests are green in Debug and Release (32/32 CTest, 0 failures). A device frame now
exists for all seven content cells: the standalone D3D12 harness captured four scene categories on
Intel(R) Graphics and WARP after the six-cell content merge. T056 is PARTIAL, not green: the
skinned-character sub-case never draws, the glass panel is a bounded tinted-environment approximation
rather than demonstrated transmission, and no device VRAM peak was measured. T057 is still NOT RUN
(no independent verdict); the T086 provider branch is NOT RUN (no authorized provider). Budgets are now
recorded where a measurement exists and stay unset where one does not.**

## Identity and scope

- **Stage and claim**: Q4 portable slices. Claim: the material route, linear-HDR opaque inputs, direct
  light/shadow arithmetic, indirect/reflection/atmosphere arithmetic, residency policy and
  non-proprietary reconstruction/off ordering behave as the tests specify. **No** frame, art, parity,
  capture, budget, engine-integration or provider claim.
- **Issue and branch**: #65, branch `feat/65-q4-hybrid-quality`, based on `main` at `dae9431`.
- **Submodule**: `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`,
  `git -C doom3_orig/doom3 status --porcelain` empty. No submodule or fork change in this PR.
- **Data identity and rights**: synthetic CPU fixtures only for the portable slices. No `.pk4`/`.pk3`/`.pak`
  is present and no game data is committed. The capture run (§5) reads the seven authored Q4 packages,
  all CC0-1.0 with `retail_text_consulted: no`; the tests themselves read none of them.
- **Exclusions and limitations**: the engine fork (renderer dispatch into the hybrid passes), the target
  build (`targets/win11-amd64/CMakeLists.txt` is untouched), all image comparison, all captures,
  provider work, and the Q6 path tracer.

## Environment and configuration

| Item | Value |
|---|---|
| OS / arch | Windows 10.0.22631, x64 |
| Compiler | MSVC 19.44.35228.0 (VS 2022 Professional, toolset v143), Windows SDK 10.0.26100.0 |
| Build tool | CMake 3.31.6 (VS-bundled), generator `Visual Studio 17 2022 -A x64` |
| Build root | `build-q4/` (git-ignored) |
| Adapter and driver | The portable-slice tests use no device. The capture run (§5) used `Intel(R) Graphics` (vendor 0x8086, device 0x7D67, driver 32.0.101.8331) and `Microsoft Basic Render Driver` (WARP) |
| Shader provenance | The portable slices compile none. The capture harness compiles its own HLSL at startup from `src/renderer/q4_harness/shaders/` (§5, `evidence-q4-harness.md`) |
| Provider | None. No proprietary SDK is present, linked or downloaded; the provider branch is NOT RUN |

## What was implemented

| Task | Files | Behaviour |
|---|---|---|
| T049 | `specs/004-quality-renderer/quality-rubric.md` | Pre-review rubric (materials, lighting, darkness/readability, identity, motion, HUD), hero provenance/permissions, four scene categories, measurement plan. No numeric budget. |
| T050 | `tests/renderer/material_contract_test.cpp` | Independent sRGB decode table, channel order, route fixtures (legacy specular, missing roughness, undeclared normal/metalness, transparent/special, missing base), opaque input refusals. |
| T051 | `src/renderer/rhi/{linear_color,materials,hybrid_opaque}.*`, `src/renderer/d3d12/hybrid_opaque.cpp` | Linear-HDR opaque inputs; sRGB decoded once; HDR emissive preserved; forward/compatibility route retained for special/transparent and undeclared-PBR materials. |
| T052 | `src/renderer/rhi/lighting.h`, `src/renderer/d3d12/lighting.cpp`, `tests/renderer/lighting_test.cpp` | Windowed inverse-square attenuation, `N·L`, bias + PCF shadow factor, metal dielectric split, alpha-test discard, `Unsupported` for every legacy/forward stage. |
| T053 | `src/renderer/rhi/indirect_lighting.h`, `src/renderer/d3d12/indirect_lighting.cpp`, `tests/renderer/indirect_atmosphere_test.cpp` | L1 irradiance with negative clamp, environment reflection with below-horizon fallback and roughness blur, exponential height atmosphere preserving HDR fog colour. |
| T054 | `src/renderer/rhi/residency.*`, `tests/renderer/residency_test.cpp` | Mip/quality selection against remaining budget, per-frame upload throttle, completion-epoch release (deferred while in flight), no eviction to satisfy an upload. |
| T055 | `src/renderer/rhi/reconstruction.*`, `tests/renderer/reconstruction_test.cpp` | Identity off path and bilinear spatial upscale; identity is byte-exact; provider never required. |
| T086 | `tests/renderer/reconstruction_test.cpp` | Real `PlanPostReconstruction` ordering around the reconstruction plan: scene → world GUI → display overlay, overlay excluded from reconstruction, one-to-one HUD texels, refused render-extent HUD, identical order on the off path. |
| T056 | `content/tools/{get-encoder,build-material,capture-q4}.ps1`, `src/renderer/q4_harness/`, `specs/004-quality-renderer/evidence/t056/` | **PARTIAL** (§5): 7/7 packages built; four category frames plus WARP parity on the named adapter; skinned-character sub-case NOT RUN; no VRAM peak |
| T057 | — | **NOT RUN** (see §5). |

Design boundaries worth stating:

- The `d3d12/` path holds arithmetic only. No `ID3D12*`, device, descriptor or queue operation was added;
  the Q4 tests link no native library.
- `materials.h` keeps the Q2 classic plan intact. `PlanModernMaterial` is additive and never promotes a
  material whose PBR channels are not declared.
- The light model is a bounded preview term, documented as such. It is not a claim of a full BRDF; Q6 is
  where a physically based reference is judged.

## Procedure and retained artifacts

All commands ran from the worktree root. Full logs are retained in the git-ignored `build-q4/`.

```text
cmake -S tests/renderer -B build-q4 -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-q4 --config Release -- -m                           -> 0 (no error/warning lines)
cmake --build build-q4 --config Debug   -- -m                           -> 0 (no error/warning lines)
ctest --test-dir build-q4 -C Release --no-tests=error --output-on-failure -> 0, 32/32
ctest --test-dir build-q4 -C Debug   --no-tests=error --output-on-failure -> 0, 32/32
q4_material_contract_test.exe    -> 0, 41 checks / 0 failures
q4_lighting_test.exe             -> 0, 54 checks / 0 failures
q4_indirect_atmosphere_test.exe  -> 0, 32 checks / 0 failures
q4_residency_test.exe            -> 0, 37 checks / 0 failures
q4_reconstruction_test.exe       -> 0, 35 checks / 0 failures
py -3 -I scripts/validate-rhi-contract.py -> 0
```

`validate-rhi-contract.py` reports `header scan: RUN over 19 header(s) and 29 public test file(s)`; the
new headers and public tests contain no private backend vocabulary. The Q4 additions are five CTest
entries labelled `q4`.

**Defect found and fixed during verification.** The first Debug run of `q4_residency_test` hung after
the in-flight release check. The cause was a use-after-erase in `ResidencyPolicy::Tick`: the freed byte
count was read from a `std::map` iterator after `erase`. The bytes are now captured before the erase.
Release had passed by chance; Debug exposed it. The evidence above is from the rebuilt tree.

**Logs** (git-ignored): `build-q4-configure.log`, `build-q4-build-Release.log`,
`build-q4-build-Debug.log`, `build-q4/ctest-Release-final.log`, `build-q4/ctest-Debug-final.log`,
`build-q4/q4_residency_debug.log`.

## 4. T086 result

| Check | Result | Evidence |
|---|---|---|
| overlay/reconstruction ordering with the independent off path | **EXECUTED** | `q4_reconstruction_test.exe` 35/35; order scene → world GUI → display overlay; 2 reconstruction inputs; HUD one-to-one at display extent; render-extent HUD refused |
| provider-specific display-stage position | **NOT RUN** | no authorized provider (T060 not met); T062 has not executed; no API signature was invented |

The provider branch is not a pass and does not block the off-path check.

## 5. T056 capture set and T057

| Task | State | Exact reason |
|---|---|---|
| T056 | **PARTIAL** | All seven cells build a package, and four category frames exist on `Intel(R) Graphics` plus a WARP parity frame. Three coverage gaps remain and are not filled by a stand-in: (a) `skinned_character` is loaded and accounted but never drawn — the harness has no skinned mesh, rig or animation, so the moving category is rigid-only; (b) the `transmissive_glass` panel returns `alpha = 1` with a tinted environment term and no refraction, so no frame shows a background through glass; (c) no device VRAM peak was measured. Committed texture bytes and frame envelopes are recorded; the numeric budgets that follow from them are in `quality-rubric.md` §5. |
| T057 | **NOT RUN** | The four category frames exist, but this file is authored by the implementer and cannot substitute for an independent art verdict. The reviewer still has to judge materials, lighting, darkness/readability, identity, motion and HUD in `review-q4.md`. |

### 5.1 What was built and run

This record was **re-taken at `50efd1a` for the B1/B2 fix session** (see 5.7). The commands below are
the ones that produced the committed artifacts; the superseded run at `348487c` is in git history.

```text
powershell -NoProfile -File content/tools/get-encoder.ps1                              -> 0
  ENCODER OK: texconv 2026.5.8.1, 966480 bytes, sha256 dcfdec10...ef06
py -3 content/tools/generators/dielectric.py --size 512                                -> 0
powershell -NoProfile -File content/tools/validate.ps1                                 -> 0
  VALIDATE OK: 29 hero files, 7 sidecars, 7 Q4 materials, 1 packs (1 absent)
content/tools/build-material.ps1 -Material <m> -OutDir out/content/<m>  (7 materials)   -> 0 each
content/tools/verify-package.ps1 -OutDir out/content/<m>                 (7 materials)   -> 0 each
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64 -> 0
cmake --build build-q4-harness --config Release -- -m                                  -> 0
q4_harness.exe -ListAdapters                                                           -> 0
content/tools/capture-q4.ps1 ... (ten invocations; ten records kept in evidence/t056/captures) -> 0 each
q4_harness.exe -Scene <s> -Frames 30 -AdapterIndex 0  (4 scenes)                        -> 0 each
q4_harness.exe -Scene <s> -Frames 5  -Warp            (4 scenes)                        -> 0 each
q4_harness.exe -Scene moving -Frames 1..8 -AdapterIndex 0                               -> 0 each
```

Artifacts: harness `build-q4-harness/bin/Release/q4_harness.exe`,
SHA-256 `6a2d50bb2e23670e3334b43cace3962dfa1a2bca86819bcd429030a5d9690748`; toolchain CMake 3.31.6
(VS-bundled), MSVC 14.44.35207, content toolchain Python 3.14.4 / Pillow 12.3.0 / texconv 2026.5.8.1.
Host: Windows 11 10.0.22631 x64. Adapter `Intel(R) Graphics` vendor 0x8086 device 0x7D67, reported
dedicated memory 134,217,728 bytes, driver 32.0.101.8331 (2025-11-26); software adapter
`Microsoft Basic Render Driver` vendor 0x1414 device 0x008C.

Every number below is in `evidence/t056/measurements.json`; the derived ones (image hashes, package
hashes, byte totals, frame diffs, B1/B2 metrics) are produced by the scripts in
`evidence/t056/tools/`, so no figure is hand-copied.

### 5.2 Scene coverage

| Category | Scene | Cells drawn | Cells NOT RUN | Intel frame | WARP frame | Category state |
|---|---|---|---|---|---|---|
| static opaque | `static_opaque` | `opaque_dielectric` (plane, cube, sphere) + `opaque_metal` (sphere) | — | `01-static-opaque-dielectric-metal.png` | `06-warp-static-opaque.png` | **RUN** |
| alpha / emissive | `alpha_emissive` | `alpha_tested` (cutout quad) + `emissive` (sign quad) | — | `02-alpha-tested-emissive.png` | timing only (WARP frame not committed) | **RUN**, HDR > 1.0 not demonstrated |
| moving | `moving` | `opaque_dielectric` (rotating cube + static sphere/ground) | `skinned_character`: no skinned mesh, rig or animation exists in the harness draw list (`harness_render.cpp:57-64`) or in the content set | `03-moving-frame-1.png`, `04-moving-frame-8.png` | timing only (WARP frame not committed) | **PARTIAL** (rigid only) |
| transmissive / large environment | `transmissive_environment` | `large_environment` (24×24 ground plane, 4096² atlas) + `transmissive_glass` (2.4×2.4 quad) | true transmission/refraction: the panel writes `alpha = 1` and uses a tinted environment term (`q4_hybrid.hlsl:287-294`, "not a refraction claim") | `05-transmissive-environment.png` | `07-warp-transmissive-environment.png` | **PARTIAL** |

The four category frames are captures, not generated placeholders. The glass and environment captures
are the same frame (one scene draws both cells), so a per-material `capture.json` exists for both
without duplicating the image.

### 5.3 Frame behaviour and resource accounting

Frame time is measured by the harness around submit plus `ExecuteAndWait`, at 1280×720 render and
1280×720 display, `-RenderScale 1.0` (`PlanReconstruction: Off`), ACES tone map at `-Exposure 1.0`.
There is no swapchain, so no vsync or present cost is included. One warm-up run of 5 frames is
discarded first: the first run of a process on this host is 0.3-0.6 ms slower than the rest, and
without that discard the same scene reported a 2.22 ms median on one run and 1.61 ms on the next.
The hardware column is three independent 30-frame windows per scene, each after its own discarded
warm-up run; `median` is the median of the three window medians, and the range beside it is how far
those three medians moved. That range is the honest noise floor on this host and it is wider than any
difference between the four scenes.

| Scene | Windows × frames (hw / WARP) | Intel median (median range), min / max observed | WARP min / median / max |
|---|---|---|---|
| `static_opaque` | 3 × 25 / 5 | 1.88 (1.47-2.21), 1.24 / 4.48 | 24.58 / 26.15 / 58.29 |
| `alpha_emissive` | 3 × 25 / 5 | 1.74 (1.57-2.10), 1.25 / 5.02 | 24.41 / 25.36 / 65.61 |
| `moving` | 3 × 25 / 5 | 2.05 (1.53-2.12), 1.36 / 4.65 | 24.10 / 26.07 / 73.97 |
| `transmissive_environment` | 3 × 25 / 5 | 1.83 (1.54-2.12), 1.24 / 4.07 | 23.97 / 26.15 / 75.69 |

The shadow pass now has a pixel shader, so its cost is measured rather than assumed:
`-Scene alpha_emissive -Frames 30 -AdapterIndex 0 -DebugShadowCount 0` (no shadow draws at all)
gives 1.07 ms median against 1.47 ms with them. That 0.40 ms is the whole shadow pass, three draws
into a 2048² map — not the alpha-test sample, which is one texture fetch plus a `clip` on the
caster's pixels. The four scene medians (1.74-2.05 ms) sit inside the pre-fix record's 1.43-2.12 ms and
inside this host's ±0.35 ms window-to-window noise, so this session claims no frame-time change; it
does not claim the change is free either, because the noise is larger than the shadow-pass delta.

WARP versus hardware on the same scene identity, `d = max over R,G,B of |a-b|` and "changed" meaning
`d > 2` (`tools/frame_diff.py`): `static_opaque` differs at **67 of 921,600 pixels (0.007 %)**, down
from 465 (0.050 %) on the pre-fix frames, because the dielectric surfaces no longer carry per-texel
noise for the two rasterisers to sample differently; `transmissive_environment` differs at 1 pixel
(mean over-pixel max-channel difference 0.188; both `05` and `07` are byte-identical to the pre-fix pair, so this is the same measurement the review saw). The software path renders the same
image, not a blank or a fallback frame.

Committed texture payload (DDS bytes minus the 148-byte header per texture; the harness `MATERIAL`
lines report the same values). The dielectric package hash changes because its texels changed; the
other six are byte-identical to the pre-fix packages, which is the isolation check for this change:

| Cell | Material | Textures | Payload bytes | `package_sha256` |
|---|---|---|---|---|
| `opaque_dielectric` | `hero_dielectric` | 3 | 873,880 | `65281cb4…aee3` |
| `opaque_metal` | `hero_metal` | 4 | 1,048,656 | `7b0b0cc6…19f6` |
| `alpha_tested` | `hero_grating` | 4 | 1,048,656 | `398f7bbe…9f42` |
| `emissive` | `hero_emissive_sign` | 5 | 1,398,208 | `f0524139…a382` |
| `skinned_character` | `hero_character_skin` | 3 | 873,880 | `f3e69aad…e712` |
| `transmissive_glass` | `hero_glass` | 3 | 873,880 | `2abd99e6…d6a7` |
| `large_environment` | `hero_environment` | 4 | 33,554,512 | `5eb7222f…4808` |
| **total** | | **26** | **39,671,672** | |

Residency, logged once per process: `budget=268435456 uploadPerFrame=67108864
resident=33554512 uploaded=33554512 deferred=0`. `deferred` is the tick's deferred **upload** count, not a
release count. The scope is the environment cell only — the harness
builds the hooks once and the last material loaded overwrites the accounting
(`harness.cpp:728-734`) — so this is `ResidencyPolicy` bookkeeping for 33,554,512 bytes at a 256 MiB
budget with a 64 MiB per-frame upload throttle. It is not a claim that the policy owns the D3D12
allocation, and it is not a VRAM peak.

**No device VRAM peak exists.** The harness allocates committed resources from the device and never
queries `IDXGIAdapter3::QueryVideoMemoryInfo` or a process GPU-memory counter, and no external counter
run was recorded. The reported 134,217,728 bytes is the adapter's *reported dedicated memory*, not a
measurement of use.

Motion: eight frames of `moving`, cube rotation `20° + 25° × frameIndex`. Adjacent frames differ over
4.46–6.70 % of pixels (mean over-pixel max-channel difference 2.67–3.83) and every changed pixel lies
inside x ∈ [438, 781], y ∈ [238, 494] — the rotating cube and its shadow. Everything outside that box
is byte-identical between frames. There is no temporal reprojection in the harness, so this measures
per-frame identity and the absence of frame-to-frame material drift, not history rejection. The metric
is stated in `evidence/t056/tools/frame_diff.py` because T057 finding N3 could not reproduce the
older figure from the committed pair; `mean_abs` here is the mean of the per-pixel max channel
difference, which is larger than a mean over all channels and is not directly comparable to the
pre-fix record's 1.47.

### 5.4 Accepted and rejected examples (candidate rows for T057)

These are observations with artifacts. They are **not** verdicts; T057 records the verdicts in
`review-q4.md`. That review is a point-in-time record: it cites the pre-fix SHA-256 of frames 01-04
and 06, which this session replaced. The rejected frames remain reachable at `50efd1a`.

| Dimension | Artifact | Observation | Candidate |
|---|---|---|---|
| 4.1 materials | `01-static-opaque-dielectric-metal.png` | The dielectric plane/cube/sphere reads as a painted panel with a broad, coherent highlight and a subtle orange-peel bump; the metal sphere is darker, tinted, and carries a tight bright specular lobe. The two are distinguishable without moving the camera. The metal cannot show reflected scene structure because the reflection source is the analytic sky gradient, not a scene probe. | accept (distinctness); the reflection-source limit is 4.3, not a metalness failure |
| 4.1 / 4.2 alpha cutout | `02-alpha-tested-emissive.png` | Light passes through the grating holes: sky above the horizon and the ground below it are visible through them; the hole edges are clean at this extent. | accept (cutout) |
| 4.2 shadows | `crops/grating-shadow-cutout.png` | The grating's cast shadow now carries the caster's hole pattern instead of its silhouette. The holes are soft rather than crisp: see 5.7 for the measurement and the reason. | accept (shape matches the caster); softness recorded as a residual |
| 4.2 / 4.1 transmission | `05-transmissive-environment.png` | The panel is a smooth tinted surface; the floor and horizon do not show through it, and there is no refraction or thickness. The shader documents this as a bounded approximation (`q4_hybrid.hlsl:287-294`). | **reject** for the transmission claim; accept only as "the transmissive route renders and does not take the full opaque direct term" |
| 4.4 darkness / readability | `02-alpha-tested-emissive.png` | The emissive bars are the brightest element but the panel frame and the surrounding ground keep their separation; no clipped white blob swallows neighbouring detail. The emissive is SDR: radiance above 1.0 is blocked on the C-10 format decision, so "HDR emissive keeps detail above 1.0" is **not tested**. | accept (readability); HDR part NOT TESTED |
| 4.5 identity / reproducibility | `04-moving-frame-8.png` | Frame identity is deterministic across runs and across the content change: re-running `-Scene moving -Frames 8` after the dielectric regeneration reproduces this exact PNG (SHA-256 `1cf980a1…02c9`), and frames 05/07 are byte-identical to the pre-fix record because neither the sampler nor the shadow depth of non-cutout casters changed. | accept (deterministic frame identity) |
| 4.6 motion | `03-moving-frame-1.png` → `04-moving-frame-8.png` | 4.46–6.70 % of pixels change per adjacent pair, all inside the cube/shadow box; the background is unchanged. No ghost or smear is possible on this path (no history), and no material swim was observed. | accept (rigid stability); skinned motion is NOT RUN |
| 4.7 HUD / overlay | none | The harness draws no HUD; the T086 ordering check is CPU-only. | not applicable / NOT RUN for this capture set |

### 5.5 What these frames do not show

- **Skinned character**: `skinned_character` has a package and is loaded into slot 5, but no draw uses
  it. A skinned frame would require a rigged mesh and animation, which exist in neither the harness nor
  the content set. No stand-in material was substituted.
- **Transmission through glass**: see 5.4. The category frame proves the route renders; it does not
  prove a see-through or refractive glass result.
- **HDR emissive**: the sign is shaded from an SDR mask; the > 1.0 radiance case depends on the C-10
  format decision.
- **Engine integration**: the harness is standalone. These frames are not engine frames, not classic
  parity, and not the fork's call sites.
- **VRAM peak**: not measured (5.3).

### 5.6 Measured basis for the budgets

`quality-rubric.md` §5 now carries the per-cell committed texture bytes and the per-scene frame
envelope from the tables above. VRAM peak stays unset with the reason recorded. The budget rows in
`content/manifest/budget-rows.json` are `recorded` for the six cells a frame or a residency reading
covers, and `blocked` for `skinned_character` with the exact reason.

### 5.7 B1/B2 fix record

The T057 review left two visible defects open (`review-q4.md` §6). Both were reproduced, diagnosed to
a mechanism, fixed, and re-measured. Neither fix moved a byte of content size or a frame-time budget.

**B-1, the cutout shadow was solid.** Cause: the shadow pipeline state had no pixel shader at all
(`harness_pipeline.cpp`, shadow PSO), so every caster wrote its full silhouette into the depth map and
the caster's alpha texture was never consulted. Fix, in three parts:

- `shaders/q4_hybrid.hlsl`: `VSShadow` now passes the vertex UV through, and a new `PSShadowAlpha`
  discards texels below the same `params.x` cutoff the visible pass uses when the draw declares an
  alpha test (`params.y`). Casters without an alpha test keep the full silhouette.
- `harness_pipeline.cpp`: the shadow root signature gains the material descriptor table and the
  linear sampler (previously it had neither), and the shadow PSO names `PSShadowAlpha`.
- `harness_render.cpp`: the shadow loop binds each draw's material table, so the caster's own
  `baseColor` alpha decides the cutout.

Measurement (`tools/defect_metrics.py --only b1`, footprint taken from the rejected frame's solid
shadow and eroded by 2 px, 2,385 px):

| | rejected `50efd1a` | fixed |
|---|---|---|
| dark pieces inside the footprint (`dark_components`) | **1** (one 2,385 px block) | **4** (1,779 + 86 + 32 + 3) |
| spread of median-filtered luminance inside it (`sd`) | 1.53 | 6.90 |
| range | 145-156 | 138-173 |
| share of footprint back at unshadowed level | 0.0 % | 0.46 % |

The same region is shown at 2× in `crops/grating-shadow-solid.png` (rejected) and
`crops/grating-shadow-cutout.png` (fixed).

**Residual, stated rather than hidden:** the holes are soft, not crisp. The shadow map is 2048² over a
16-unit light extent, i.e. 7.8 mm per texel, and the grating's bars are only 2-3 texels wide there, so
PCF both blurs each bar and lets the penumbra of neighbouring bars re-occlude the holes. The best hole
pixel reaches the unshadowed level (173); the typical one reaches 162-168. A crisper cutout shadow
needs a tighter shadow extent or a higher shadow resolution, which would change every shadow in every
frame and belongs to a shadow-quality task, not to B-1.

**B-2, the dielectric surface response was speckle.** Cause: authoring, not filtering, and it is
provable without looking at the frame. `content/tools/generators/dielectric.py` carried a private copy
of the shared noise helper, and the copy was degenerate: its lattice coordinate was written
`(x % period) / period * period`, which is just `x % period`, so the interpolation weight was always
zero and every "smooth" field was a per-texel hash. The committed normal map built from that field had
**a mean tilt of 58.08° and a mean neighbour-to-neighbour angular delta of 73.18°** — white noise, not
orange peel. The albedo added its own per-texel hash term on top (high-pass `sd` 7.81/255).

Fix: the generator now imports `hero_common` (the helper the other six cells already use) and has no
private copy; height, mottle and scuff come from tileable value-noise fields, the per-texel albedo term
is gone, and the normal strength, paint contrast and roughness range were set for "painted, slightly
worn panel". Regenerated images, before → after: normal tilt mean 58.08° → **4.15°**, mean
neighbour-to-neighbour delta 73.18° → **1.42°**, albedo high-pass `sd` 7.81 → **0.18**, roughness
range 0.43-0.81 → 0.50-0.71.

Frame measurement (`tools/defect_metrics.py --only b2`), `highpass_sd` then `isolated_spikes`:

| Region | rejected | fixed |
|---|---|---|
| dielectric sphere | 16.21 / 550 | **2.39 / 2** |
| dielectric cube face | 13.31 / 425 | **2.85 / 0** |
| ground, near | 11.13 / 0 | **0.76 / 0** |
| ground, mid-distance | 5.71 / 0 | **0.66 / 0** |

The two remaining spikes on the sphere are isolated single pixels in the Fresnel band at its lower-left
limb; at 6× they are part of a smooth sheen, not a dot field, and the sphere now reads as a painted
surface rather than sand. They are recorded because they exist, not because they were chased away.

**Filtering was tested and rejected.** The review also named the sampler (`MaxAnisotropy = 1`). Setting
it to anisotropic 8× was implemented and captured: it does not touch per-texel noise, and it raised
high-frequency energy 3-4× in the `large_environment` cell's far field (far-field `highpass_sd` 0.73 →
2.36 left, 0.92 → 3.59 right) because that content's mip chain does not filter its tile grid, turning
over-blur into shimmer. That cell is out of scope for this session, so the change was reverted and the
reason is recorded in a comment at the sampler. The proof that the revert is clean is that frames 05
and 07 are **byte-identical** to the pre-fix record.

**What did not change:** target and root `CMakeLists.txt`, `tests/renderer/`, Q6/Q7 content or code, and
every material other than `hero_dielectric` — all six other `package_sha256` values are unchanged, and
so is every byte count and the residency reading.

## 6. What is not claimed

- Q4 is **not complete** and not green. T056 is PARTIAL (skinned motion, transmission and VRAM peak
  missing). T057 ran and its verdict is **BLOCKED**: B-1/B-2 were closed in this session (5.7), B-3 to B-6 (skinned motion, HUD, HDR emissive, transmission) are still missing artifacts. The provider branch of T086 is NOT RUN.
- No modern-look acceptance, no classic parity, no DXR, no DLSS. There is a measured frame envelope on
  one adapter and there is still **no VRAM figure**; the frame numbers are a regression ceiling for this
  host, not a performance claim for any other device.
- No capture from spec 006 is cited or reused; no `-HdTextures` switch is involved.
- The portable slices do not prove the engine/fork wiring. That needs its own fork PR, submodule bump
  and independent review.
