# Q4 hybrid-quality evidence (T049–T055, T056, T057, T086)

Status: author evidence for issue [#65](https://github.com/yhyu13/DOOM3_AINative/issues/65), written
2026-10-30. This is **not** Q4 acceptance, not an engine frame and not an independent review. It records
what the portable slices do on this host and what could not run.

**Headline: T049–T055 and the T086 off-path branch are implemented as CPU-testable, backend-neutral
slices and their tests are green in Debug and Release (32/32 CTest, 0 failures). A device frame now
exists for all seven content cells: the standalone D3D12 harness captured the four scene categories on
Intel(R) Graphics and WARP, including a real skinned character, a display-resolution HUD, emissive
radiance above 1.0 read back from the scene target, and a thin-slab screen-space refraction for the
glass. T056 is now **complete**: the process VRAM peak is measured at 107,110,400 bytes with DXGI
`QueryVideoMemoryInfo` (§5.9) and B4 is set. T057 round 2 (PR #84) is BLOCKED only on that missing
measurement; a re-run or addendum (r3) decides Q4, so Q4 is **not green**. The T086 provider branch is
NOT RUN (no authorized provider).

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
| T056 | `content/tools/{get-encoder,build-material,capture-q4}.ps1`, `content/tools/pins.json`, `src/renderer/q4_harness/`, `specs/004-quality-renderer/evidence/t056/` | **Complete** (§5): 7/7 packages built; four category frames plus WARP parity; skinned draw, display HUD, HDR emissive and glass refraction; process VRAM peak measured at 107,110,400 bytes (§5.9) |
| T057 | — | **ran twice**; round 2 (PR #84) is BLOCKED only on the (now measured) VRAM peak, so r3 decides (§5.9). |

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
| T056 | **complete** | All seven cells build a package and all four category frames exist on `Intel(R) Graphics`, with WARP parity for static and transmissive. The B3–B6 session added a real skinned draw (805 vertices, 12 bones, 2-bone linear blend), a display-resolution HUD with an in-world GUI, an HDR-emissive scene-target probe (peak 15.21 at the authored 16× scale) and a thin-slab screen-space glass refraction with a no-glass reference. The 2026-10-31 session measured the process VRAM peak (107,110,400 bytes LOCAL `CurrentUsage`) and sets B4 (§5.9). Committed texture bytes and frame envelopes are in §5.3; the numeric budgets are in `quality-rubric.md` §5. |
| T057 | **ran twice, r3 pending** | The first point-in-time review at `50efd1a` is in `review-q4.md`. B-1/B-2 were closed in that session; the B3–B6 session closes B-3/B-4/B-5/B-6 with artifacts (§5.8). Round 2 (PR #84, `review-q4-r2.md`) closes all six and accepts 4.1–4.7 with limits, but is BLOCKED for Q4 on the unmeasured VRAM peak only. §5.9 measures that peak; **T057 r3/addendum** must record the final Q4 verdict. |

### 5.1 What was built and run

This record was **re-taken at `4229bd5` for the B3–B6 coverage session** (§5.8). Recapturing exposed a
data-map encoding defect, so the packages were rebuilt and every frame below was produced from one
binary afterwards. The previous B1/B2-session record is at `50efd1a` in git history.

```text
powershell -NoProfile -File content/tools/get-encoder.ps1                              -> 0
  ENCODER OK: texconv 2026.5.8.1, 966480 bytes, sha256 dcfdec10...ef06
powershell -NoProfile -File content/tools/validate.ps1                                 -> 0
  VALIDATE OK: 29 hero files, 7 sidecars, 7 Q4 materials, 1 packs (1 absent)
content/tools/build-material.ps1 -Material <m> -OutDir out/content/<m> -Force (7)        -> 0 each
content/tools/verify-package.ps1 -OutDir out/content/<m>                 (7 materials)   -> VERIFY OK each
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64 -> 0
cmake --build build-q4-harness --config Release -- -m                                  -> 0
content/tools/capture-q4.ps1 ... (eleven invocations; eleven records in evidence/t056/captures) -> CAPTURE OK each
q4_harness.exe -Scene <s> -Frames 30 -AdapterIndex 0  (4 scenes, 3 windows)             -> 0 each
q4_harness.exe -Scene <s> -Frames 5  -Warp            (4 scenes)                        -> 0 each
q4_harness.exe -Scene moving -Frames 1..8 -AdapterIndex 0                               -> 0 each
q4_harness.exe -Scene alpha_emissive -HdrProbe (scale 1, 16, 2)                         -> peaks 1.0059 / 15.2109 / 1.9531
```

Artifacts: harness `build-q4-harness/bin/Release/q4_harness.exe`,
SHA-256 `4a8d931de4cba415522f8e8a69eca377fb9ddbc2d49c7f13aea2ee2269a3b3ea`; toolchain CMake 3.31.6
(VS-bundled), MSVC 14.44.35207, content toolchain Python 3.14.4 / Pillow 12.3.0 / texconv 2026.5.8.1.
Host: Windows 11 10.0.22631 x64. Adapter `Intel(R) Graphics` vendor 0x8086 device 0x7D67, reported
dedicated memory 134,217,728 bytes, driver 32.0.101.8331 (2025-11-26); software adapter
`Microsoft Basic Render Driver` vendor 0x1414 device 0x008C.

Every number below is in `evidence/t056/measurements.json`; the derived ones (image hashes, package
hashes, byte totals, frame diffs, coverage metrics) are produced by the scripts in
`evidence/t056/tools/`, so no figure is hand-copied.

**Data-map correction.** `content/tools/pins.json` applied `-srgbi` to normal/roughness/metalness/ao,
linearising authored data: a flat normal stored 0.5 as 0.214, decoded by the shader as `2v-1 = -0.57`,
i.e. a ~54° tilt on every surface. That is why the first glass attempt washed out (its Fresnel saw a
grazing normal) and why the dielectric looked flatter than authored. The pin now leaves data channels
linear; all seven packages were rebuilt, verified, and every frame in this record was recaptured. The
B1/B2 metrics in `measurements.json` are marked historical for the same reason.

### 5.2 Scene coverage

| Category | Scene | Cells drawn | Cells NOT RUN | Intel frame | WARP frame | Category state |
|---|---|---|---|---|---|---|
| static opaque | `static_opaque` | `opaque_dielectric` (plane, cube, sphere) + `opaque_metal` (sphere) | — | `01-static-opaque-dielectric-metal.png` | `06-warp-static-opaque.png` | **RUN** |
| alpha / emissive | `alpha_emissive` | `alpha_tested` (cutout quad) + `emissive` (sign quad, authored 16× HDR scale) | — | `02-alpha-tested-emissive.png`; controls `10-emissive-sdr-reference.png`, `11-emissive-hdr-rolloff.png` | timing only (WARP frame not committed) | **RUN**, HDR > 1.0 probed |
| moving | `moving` | `opaque_dielectric` (rotating cube + static sphere/ground) + `skinned_character` (12-bone procedural humanoid, host linear blend) | — | `03-moving-frame-1.png`, `04-moving-frame-8.png` | timing only (WARP frame not committed) | **RUN** |
| transmissive / large environment | `transmissive_environment` | `large_environment` (24×24 ground plane, 4096² atlas) + `opaque_dielectric` marker cube + `transmissive_glass` (2.4×2.4 pane, thin-slab refraction) | — | `05-transmissive-environment.png`; reference `09-transmission-reference.png` | `07-warp-transmissive-environment.png` | **RUN** |
| display overlay | `static_opaque` + `-Hud` | the static scene + in-world GUI at render extent + display HUD | — | `08-hud-overlay-display.png` (render 960×540 → display 1280×720) | not run | **RUN** |

The category frames are captures, not generated placeholders. `08` exists to exercise ordering at a
non-1.0 render scale: the scene and the in-world GUI are rendered at 960×540, then the HUD is drawn at
1280×720 after tonemap. `09` is the same scene with the glass draw skipped, so the transmission metric
has a reference; `10`/`11` isolate the emissive tone-map behaviour.

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
| `static_opaque` | 3 × 25 / 5 | 2.18 (1.55-2.31), 1.31 / 5.20 | 23.12 / 26.28 / 65.83 |
| `alpha_emissive` | 3 × 25 / 5 | 1.56 (1.51-2.06), 1.29 / 5.51 | 23.61 / 24.00 / 67.85 |
| `moving` | 3 × 25 / 5 | 2.19 (1.59-2.20), 1.41 / 5.24 | 23.33 / 25.37 / 64.73 |
| `transmissive_environment` | 3 × 25 / 5 | 1.68 (1.50-1.70), 1.35 / 5.59 | 25.57 / 27.68 / 79.09 |
| `static_opaque` + HUD, 0.75 render scale | 3 × 25 / — | 1.48, 1.39 / 2.73 | — |

The shadow pass now has a pixel shader; its cost was measured in the B1/B2 session (1.07 ms median
with no shadow draws against 1.47 ms with them) and was not re-measured this session, because the
data-map rebuild changes shading, not the shadow pass. The four scene medians (1.56-2.19 ms) are
envelopes: the window-to-window spread (0.14-0.76 ms) is comparable to the differences, so this session
claims no frame-time improvement, and the added skinned/glass/HUD draws did not push any median above
the recorded 2.2 ms ceiling.

WARP versus hardware on the same scene identity, `d = max over R,G,B of |a-b|` and "changed" meaning
`d > 2` (`tools/coverage_metrics.py`): `static_opaque` differs at **4 of 921,600 pixels (0.000 %,
mean 0.152)** and `transmissive_environment` at **1 pixel (mean 0.129)**. Both rasterisers produce the
same image, not a blank or a fallback frame. The WARP transmissive frame includes the refraction and
the marker cube; the old "byte-identical to pre-fix" statement no longer applies because the packages
and the shader changed.

Committed texture payload (DDS bytes minus the 148-byte header per texture; the harness `MATERIAL`
lines report the same values). All seven package hashes changed because the data maps were re-encoded;
the payload sizes did not:

| Cell | Material | Textures | Payload bytes | `package_sha256` |
|---|---|---|---|---|
| `opaque_dielectric` | `hero_dielectric` | 3 | 873,880 | `52951c51…` |
| `opaque_metal` | `hero_metal` | 4 | 1,048,656 | `cc4861cb…` |
| `alpha_tested` | `hero_grating` | 4 | 1,048,656 | `ad68e7e6…` |
| `emissive` | `hero_emissive_sign` | 5 | 1,398,208 | `42aa309c…` |
| `skinned_character` | `hero_character_skin` | 3 | 873,880 | `fca5d611…` |
| `transmissive_glass` | `hero_glass` | 3 | 873,880 | `750c4664…` |
| `large_environment` | `hero_environment` | 4 | 33,554,512 | `61d8a700…` |
| **total** | | **26** | **39,671,672** | |

`hero_environment`'s package hash moved from `c5abb831…` to `61d8a700…` on 2026-10-31 because its
sidecar's stale residency record was corrected. The package was rebuilt from the same four DDS files
(texture SHA-256 identical before and after), so frame 07 is byte-identical and was not recaptured; the
capture record keeps the previous hash with a note.

Residency, logged once per process: `budget=268435456 uploadPerFrame=67108864
resident=33554512 uploaded=33554512 deferred=0`. `deferred` is the tick's deferred **upload** count, not a
release count. The scope is the environment cell only — the harness
builds the hooks once and the last material loaded overwrites the accounting
(`harness.cpp:728-734`) — so this is `ResidencyPolicy` bookkeeping for 33,554,512 bytes at a 256 MiB
budget with a 64 MiB per-frame upload throttle. It is not a claim that the policy owns the D3D12
allocation, and it is not a VRAM peak.

**Device VRAM peak (2026-10-31).** The harness now samples
`IDXGIAdapter3::QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL)` for the calling process after
initialization, after the material upload, at scene start and after every rendered frame (`-VramProbe`,
`src/renderer/q4_harness/vram_probe.*`). Across four scenes × three 30-frame runs, every run reports the
same LOCAL `CurrentUsage` peak, **107,110,400 bytes** (102.15 MiB), at `materials_loaded`; the harness
uploads all seven cells in every process, so the peak is scene-independent, and the per-frame samples
after upload are lower (106,971,136 bytes). NON_LOCAL usage is 0. The reported 134,217,728 bytes is the
adapter's *reported dedicated memory* (identity only) and is not the use figure. Full record: §5.9,
`evidence/t056/vram.json`.

Motion: eight frames of `moving`, cube rotation `20° + 25° × frameIndex` and the character pose
advancing with the same index. Adjacent frames differ over 4.79–7.09 % of pixels (mean over-pixel
max-channel difference 2.29–3.69) and the difference box covers the cube, its shadow and the
character. Within the character-only box (x 850–1020, y 150–470) **5,758 pixels change above 2/255
(10.6 % of the box)** between frames 1 and 8, with the arm and elbow moving while the material and
silhouette stay identifiable. There is no temporal reprojection in the harness, so this measures
per-frame identity and the absence of frame-to-frame material drift, not history rejection. The metric
is stated in `evidence/t056/tools/coverage_metrics.py` and `frame_diff.py`.

### 5.4 Accepted and rejected examples (candidate rows for T057)

These are observations with artifacts. They are **not** verdicts; T057 records the verdicts in
`review-q4.md`. That review is a point-in-time record: it cites the pre-fix SHA-256 of frames 01-04
and 06, which this session replaced. The rejected frames remain reachable at `50efd1a`.

| Dimension | Artifact | Observation | Candidate |
|---|---|---|---|
| 4.1 materials | `01-static-opaque-dielectric-metal.png` | The dielectric plane/cube/sphere reads as a painted panel with a broad, coherent highlight and a subtle orange-peel bump; the metal sphere is darker, tinted, and carries a tight bright specular lobe. The two are distinguishable without moving the camera. The metal cannot show reflected scene structure because the reflection source is the analytic environment gradient, not a scene probe. | accept (distinctness); the reflection-source limit is 4.3, not a metalness failure |
| 4.1 / 4.2 alpha cutout | `02-alpha-tested-emissive.png` | Light passes through the grating holes: the clear-colour background `(0.012, 0.014, 0.020)` above the horizon and the ground below it are visible through them; the hole edges are clean at this extent. No sky is drawn. | accept (cutout) |
| 4.2 shadows | `crops/grating-shadow-cutout.png` | The grating's cast shadow now carries the caster's hole pattern instead of its silhouette. The holes are soft rather than crisp: §5.7 records the **B1/B2-session** measurement and the reason; `measurements.json b1.current` is the recomputation on the committed frame, and the visible evidence is this crop plus the projected 8×8 hole grid in the round-2 review (PR #84). | accept (shape matches the caster); softness recorded as a residual |
| 4.2 / 4.1 transmission | `05-transmissive-environment.png` vs `09-transmission-reference.png` | The pane transmits the scene behind it: luminance correlation 0.9336, variance ratio 0.3929, 58.5 % of pane pixels within 20/255 of the no-glass reference. Fitting the background displacement gives a best shift of −9 px (13.5 % SAD improvement over zero shift), so the pane is refracting, not copying. The Fresnel reflection lifts the background (the clear colour, not a drawn
sky), and the model's limits (single slab, no dispersion/caustics) are recorded. | accept (bounded transmission); engine-side transmission remains a separate limit |
| 4.4 darkness / readability | `02-alpha-tested-emissive.png`, `11-emissive-hdr-rolloff.png` | The scene-target probe reads peak 15.2109 at the authored 16× scale (5,541 px > 8.0) while the scale-1 control peaks at 1.0059 (0 px > 8.0); the tonemap therefore receives radiance above 1.0. At 16× the 8-bit display saturates by design; the scale-2 diagnostic shows ACES rolling off with **0 saturated pixels** and a non-zero spread, so detail is compressed, not hard-clipped. | accept (readability + HDR exercised) |
| 4.5 identity under skinning | `03-moving-frame-1.png`, `04-moving-frame-8.png`, `crops/skinned-pose-2x.png` | The skinned character keeps the woven suit material and its silhouette while the right arm and elbow change pose between frames. No weight-smear or material swim is visible in the 2× crop; the rig is procedural (805 vertices, 12 bones) and is labelled as such rather than presented as a retail asset. | accept (identity stable on a procedural rig) |
| 4.6 motion | the eight-frame `moving` series; `coverage.skinned_motion` | 4.79–7.09 % of pixels change per adjacent pair; 5,758 px (10.6 %) change inside the character box between frames 1 and 8. No ghost or smear exists on this path (no history), and no material swim was observed. | accept (rigid + skinned motion) |
| 4.7 HUD / overlay | `08-hud-overlay-display.png`, `crops/hud-texel-4x.png` | The scene and in-world GUI render at 960×540, the HUD at 1280×720 after tonemap. The 4× crop shows an exact 2-px border and 20-px cells; the HUD samples no scene texture, is not fogged or upscaled, and lies outside motion history. Ordering: scene → copy → glass → tonemap → HUD. | accept (display-resolution overlay; ordering recorded) |

### 5.5 What these frames do not show

- **A retail rig or animation**: the skinned character is a 12-bone procedural humanoid with a
  two-bone linear-blend pose generated by the harness. It proves the skinning path and material
  stability, not a retail mesh or a game animation.
- **A full transmission model**: the glass is a single thin slab with a screen-space refraction, a
  five-tap frost and a Fresnel reflection. It does not model dispersion, caustics, multiple internal
  reflections, or per-layer depth rejection at silhouettes, and the pane is excluded from the opaque
  shadow pass. Engine-side transmission is still unimplemented.
- **A packaged HDR texture**: the emissive radiance above 1.0 is produced by applying the sidecar scale
  after sRGB decode into the HDR scene target; the texture container remains the C-10 decision.
- **Engine integration**: the harness is standalone. These frames are not engine frames, not classic
  parity, and not the fork's call sites.
- **VRAM peak**: measured at 107,110,400 bytes LOCAL `CurrentUsage` per harness process (§5.9); it is
  not the adapter's 134,217,728-byte capacity and not a device-wide figure.

### 5.6 Measured basis for the budgets

`quality-rubric.md` §5 carries the per-cell committed texture bytes and the per-scene frame envelope
from the tables above. B4 is set to the measured 107,110,400-byte LOCAL `CurrentUsage` peak (§5.9). All
seven rows in
`content/manifest/budget-rows.json` are now `recorded`, including `skinned_character`, whose artifact
is this file's `measurements.json`.

### 5.7 B1/B2 fix record

Scope: this subsection is the **B1/B2 session at `50efd1a`**. The packages and frames were recaptured
after the data-map fix (§5.8), so statements here about unchanged hashes or byte-identical frames
describe that session, not the current tree.

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
shadow and eroded by 2 px, 2,385 px). The `fixed` column is the B1/B2-session frame; the `committed`
column is the current `02` and is also `measurements.json b1.current`:

| | rejected `50efd1a` | fixed at `50efd1a` (B1/B2 session) | committed `02` (current) |
|---|---|---|---|
| dark pieces inside the footprint (`dark_components`) | **1** (one 2,385 px block) | **4** (1,779 + 86 + 32 + 3) | **1** (1,895 px) |
| spread of median-filtered luminance inside it (`sd`) | 1.53 | 6.90 | 15.39 |
| range | 145-156 | 138-173 | 105-178 |
| share of footprint back at unshadowed level | 0.0 % | 0.46 % | 5.95 % |

The same region is shown at 2× in `crops/grating-shadow-solid.png` (rejected) and
`crops/grating-shadow-cutout.png` (committed). The committed `dark_components` stays 1 because the
soft, foreshortened holes do not all separate at the 160 luminance threshold; it is **not** the
closure evidence. The closure evidence is the visible perforated lattice in the committed crop and the
projected 8×8 hole grid recorded by the round-2 review (PR #84), together with the rise in `sd`
(6.90 → 15.39) and `lit_share` (0.46 % → 5.95 %).

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

Frame measurement (`tools/defect_metrics.py --only b2`), `highpass_sd` then `isolated_spikes`. The
`fixed` column is the B1/B2-session frame; the `committed` column is the current `01` and is also
`measurements.json b2.current`:

| Region | rejected (B1/B2) | fixed at `50efd1a` (B1/B2 session) | committed `01` (current) |
|---|---|---|---|
| dielectric sphere | 16.21 / 550 | **2.39 / 2** | **0.947 / 0** |
| dielectric cube face | 13.31 / 425 | **2.85 / 0** | **3.354 / 0** |
| ground, near | 11.13 / 0 | **0.76 / 0** | **0.402 / 0** |
| ground, mid-distance | 5.71 / 0 | **0.66 / 0** | **1.151 / 0** |

In the B1/B2-session `fixed` frame, two isolated spikes remained on the sphere's Fresnel band. The
committed `01` recomputes to **0 spikes on all four ROIs**; the committed frame is the current record,
and the B1/B2 `fixed` row is retained only as history.

**Filtering was tested and rejected.** The review also named the sampler (`MaxAnisotropy = 1`). Setting
it to anisotropic 8× was implemented and captured: it does not touch per-texel noise, and it raised
high-frequency energy 3-4× in the `large_environment` cell's far field (far-field `highpass_sd` 0.73 →
2.36 left, 0.92 → 3.59 right) because that content's mip chain does not filter its tile grid, turning
over-blur into shimmer. That cell is out of scope for this session, so the change was reverted and the
reason is recorded in a comment at the sampler. The proof that the revert is clean is that frames 05
and 07 are **byte-identical** to the pre-fix record.

**What did not change (B1/B2 session only):** target and root `CMakeLists.txt`, `tests/renderer/`, Q6/Q7
content or code, and every material other than `hero_dielectric` were untouched *in that session*.
The later B3–B6 session rebuilt all packages for the data-map encoding fix (§5.8), so the byte-identity
statements above describe the B1/B2 frame set, not the current one.

### 5.8 B3–B6 closure (base `4229bd5`)

This session closes the four review blockers that `review-q4.md` left open. It does not close the
review: T057 must be re-run against the new frames and hashes, and Q4 is still not green.

| Blocker | Change | Artifact | Measurement | State |
|---|---|---|---|---|
| B-3 skinned motion | `mesh.{h,cpp}` builds a 12-bone procedural humanoid (805 vertices, 4,224 indices, 4 influences/vertex, smooth two-bone blend at elbows/knees); `VSMain`/`VSShadow` apply a host linear-blend palette; `moving` now draws `skinned_character` and advances the pose with the frame index. | `03-moving-frame-1.png`, `04-moving-frame-8.png`, `crops/skinned-pose-2x.png` | 5,758 px (10.6 % of the character box) change between frames 1 and 8; the woven material and silhouette stay identifiable; the shadow pass uses the same skinning. | **CLOSED** (artifact exists) |
| B-4 HUD | `q4_hud.hlsl` draws a display-extent HUD after tonemap at integer pixels with no scene sampler; the in-world GUI uses `PSWorldGui` at render extent; `-Hud` and `-RenderScale` exercise the ordering. | `08-hud-overlay-display.png`, `crops/hud-texel-4x.png` | scene 960×540 → copy → glass → tonemap → HUD 1280×720; 2-px border at x 24–25, 20-px cell pitch, cyan mask x 24–1256 / y 24–695. | **CLOSED** (artifact exists) |
| B-5 HDR emissive | the sidecar `emissiveScale` (16.0) is applied after sRGB decode into the R16G16B16A16_FLOAT target; `-HdrProbe` reads the scene target back before tonemap; `-EmissiveScale` provides controls. | `02-alpha-tested-emissive.png`, `10-emissive-sdr-reference.png`, `11-emissive-hdr-rolloff.png` | probe peak **15.2109**, 5,541 px > 8.0 at 16×; scale 1 peak 1.0059 with 0 px > 8.0; scale-2 diagnostic has 0 saturated display pixels and a 5.33 std, so ACES rolls off. | **CLOSED** (artifact exists) |
| B-6 transmission | a scene copy is taken after the opaque pass; `PSGlass` refracts the view ray through a 0.35-unit slab, samples the copy with a five-tap frost, tints by baseColor/alpha, adds a Fresnel environment reflection, composites opaquely and is excluded from the opaque shadow pass and depth writes. `-NoTransmission` gives the reference. | `05-transmissive-environment.png`, `09-transmission-reference.png`, `crops/glass-refraction-2x.png` | corr 0.9336, variance ratio 0.3929, 58.5 % of pane pixels within 20/255 of the reference, best background shift −9 px (13.5 % SAD improvement). | **CLOSED** (bounded model; limits stated) |

**Root cause found while closing B-6.** `content/tools/pins.json` passed `-srgbi` to every data channel,
so normal/roughness/metalness/ao were linearised at encode (0.5 → 0.214) and every normal decoded with a
~54° tilt. The glass Fresnel exposed it as a milky, grazing-angle wash. The pin now leaves data channels
linear; all seven packages were rebuilt and verified, the harness was rebuilt, and all eleven captures
plus the timing and motion series were re-taken from the new binary
(`4a8d931d…a3ea`). The B1/B2 metrics in `measurements.json` are marked historical for this reason. This
also changes the lighting of frames 01–04, which is why the previous hashes cannot be used to judge this
session.

**Not done / not claimed.** No engine or fork integration; no retail rig; no packaged HDR texture; no
dispersion/caustics. The VRAM peak is measured in §5.9. T057 r3/addendum is required, and this PR is
not merged.

### 5.9 VRAM peak (2026-10-31)

The T057 round-2 review's only remaining Q4 blocker was that no DXGI budget or process GPU-memory
counter had been sampled. This section records that measurement.

**Method.** `src/renderer/q4_harness/vram_probe.*` resolves the DXGI adapter that owns the RHI-selected
D3D12 device by `GetAdapterLuid()` + `IDXGIFactory4::EnumAdapterByLuid`, queries `IDXGIAdapter3`, and
calls `QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL)` and
`(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)` for the calling process. The harness samples after
initialization, after the material upload, at scene start and after every rendered frame; `main.cpp`
prints the identity, every sample and both peaks as one `VRAM_JSON` line when `-VramProbe` is set.
`vram_measure.py` runs each scene three times at 30 frames, parses that line, records the maximum
`CurrentUsage`, and merges the compact record into `measurements.json`.

**Commands and exits** (this worktree, hardware adapter index 0):

```text
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/validate.ps1            -> 0 (VALIDATE OK: 29 hero files, 7 sidecars, 7 Q4 materials)
powershell ... -File content/tools/build-material.ps1 -Material <m> -OutDir out/content/<m> -Force
    (7 materials: dielectric, metal, grating, emissive_sign, character_skin, glass, environment) -> 0 each
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64     -> 0
cmake --build build-q4-harness --config Release -- -m                                    -> 0
py -3 specs/004-quality-renderer/evidence/t056/tools/vram_measure.py --runs 3 --frames 30 -> 0 (12/12 runs)
```

**Recorded values.** Adapter `Intel(R) Graphics` (LUID `0x00000000:0001725F`), driver 32.0.101.8331,
harness `994c9126…d7a9`, render/display 1280×720.

| Quantity | Value |
|---|---|
| process LOCAL `CurrentUsage` peak | **107,110,400 bytes** (102.15 MiB) |
| where the peak occurs | `materials_loaded` (all seven cells uploaded; the peak is scene-independent) |
| per-frame `CurrentUsage` after upload | 106,971,136 bytes (flat across the 30-frame window) |
| process NON_LOCAL `CurrentUsage` peak | 0 bytes |
| LOCAL budget reported by DXGI (UMA host) | 37,979,832,729 bytes (a budget, not a use figure) |
| adapter dedicated memory reported by DXGI | 134,217,728 bytes (identity only, **not** the use figure) |
| runs | 4 scenes × 3 runs × 30 frames; all 12 runs report 107,110,400 bytes |

**Artifacts.** `evidence/t056/vram.json` holds identity, every raw sample and each run's peak;
`measurements.json` `vram` holds the compact per-scene table and the peak. B4 in
`quality-rubric.md` §5.2 is set from this measurement.

**Scope and limits.** This is the harness process's LOCAL usage on this host, including driver
overhead; it is not a device-wide total, not a cross-vendor claim, and not a guarantee about another
process or another adapter. Sampling is at the listed points (after each completed frame), not at
sub-frame granularity. On this UMA host the LOCAL budget is shared system memory, so "over budget" is
not a meaningful test; the recorded use figure is. A driver, content or harness-allocation change that
raises the peak requires a new T056 measurement before B4 moves.

## 6. What is not claimed

- Q4 is **not green**. T056 is complete: all four categories and the four B3–B6 artifacts exist, and
  the VRAM peak is measured (§5.9). T057 round 2 (PR #84) accepts 4.1–4.7 with limits but is blocked
  only on that measurement; **T057 r3/addendum** must record the final Q4 verdict before any dimension
  moves from accept-with-limits to a green claim. The provider branch of T086 is NOT RUN.
- No modern-look acceptance, no classic parity, no DXR, no DLSS. There is a measured frame envelope and
  a process VRAM peak on one adapter; both are regression ceilings for this host, not a performance or
  memory claim for any other device.
- No capture from spec 006 is cited or reused; no `-HdTextures` switch is involved.
- The portable slices do not prove the engine/fork wiring. That needs its own fork PR, submodule bump
  and independent review.
