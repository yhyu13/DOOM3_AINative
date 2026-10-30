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

All commands ran from the worktree root at `348487c`; full logs are in the git-ignored `out/logs/` and
`build-q4-harness/_build.log`.

```text
powershell -NoProfile -File content/tools/get-encoder.ps1                              -> 0
  ENCODER OK: texconv 2026.5.8.1, 966480 bytes, sha256 dcfdec10...ef06
content/tools/build-material.ps1 -Material <m> -OutDir out/content/<m>  (7 materials)   -> 0 each
  BUILD OK: 3, 4, 4, 5, 3, 3, 4 textures  (dielectric, metal, grating, emissive, skin, glass, environment)
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64 -> 0
cmake --build build-q4-harness --config Release -- -m                                  -> 0
q4_harness.exe -ListAdapters                                                           -> 0
content/tools/capture-q4.ps1 ... (eleven invocations; nine records kept in evidence/t056/captures) -> 0 each
q4_harness.exe -Scene <s> -Frames 30 -AdapterIndex 0  (4 scenes)                        -> 0 each
q4_harness.exe -Scene <s> -Frames 5  -Warp            (4 scenes)                        -> 0 each
q4_harness.exe -Scene moving -Frames 1..8 -AdapterIndex 0                               -> 0 each
```

Artifacts: harness `build-q4-harness/bin/Release/q4_harness.exe`,
SHA-256 `562a20d68b739c201da261da4f5988e1fd6168a429a4f7f211bb20a41289eb69`; toolchain CMake 3.31.6
(VS-bundled), MSVC 14.44.35207, content toolchain Python 3.14.4 / Pillow 12.3.0 / texconv 2026.5.8.1.
Host: Windows 11 10.0.22631 x64. Adapter `Intel(R) Graphics` vendor 0x8086 device 0x7D67, reported
dedicated memory 134,217,728 bytes, driver 32.0.101.8331 (2025-11-26); software adapter
`Microsoft Basic Render Driver` vendor 0x1414 device 0x008C.

Every number below is in `evidence/t056/measurements.json`, generated by parsing the retained harness
logs and the package manifests, so no figure is hand-copied.

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
There is no swapchain, so no vsync or present cost is included. The window is 30 consecutive frames per
scene for the hardware adapter and 5 for WARP. Frame 0 is the first rendered frame and is included in
`max`; on `static_opaque` it is the slowest sample (5.17 ms against a 2.12 ms median).

| Scene | Frames | Intel min / median / max (ms) | WARP min / median / max (ms) |
|---|---|---|---|
| `static_opaque` | 30 / 5 | 1.34 / 2.12 / 5.17 | 23.45 / 26.68 / 56.73 |
| `alpha_emissive` | 30 / 5 | 1.32 / 1.43 / 3.30 | 23.37 / 24.29 / 55.70 |
| `moving` | 30 / 5 | 1.33 / 1.73 / 4.01 | 23.60 / 23.91 / 63.99 |
| `transmissive_environment` | 30 / 5 | 1.49 / 1.83 / 3.44 | 24.54 / 26.21 / 64.88 |

WARP versus hardware on the same scene identity: `static_opaque` differs at 329 of 921,600 pixels
(0.036 % above a 2/255 threshold, mean absolute difference 0.081, per-pixel max 168 at high-contrast
edges); `transmissive_environment` differs at 1 pixel (mean 0.070). The software path renders the same
image, not a blank or a fallback frame.

Committed texture payload (DDS bytes minus the 148-byte header per texture; the harness `MATERIAL`
lines report the same values):

| Cell | Material | Textures | Payload bytes | `package_sha256` |
|---|---|---|---|---|
| `opaque_dielectric` | `hero_dielectric` | 3 | 873,880 | `8aaeb732…1608` |
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
4.43–6.65 % of pixels (mean absolute difference 2.42–3.47) and every changed pixel lies inside
x ∈ [433, 781], y ∈ [238, 494] — the rotating cube and its shadow. Everything outside that box is
byte-identical between frames. There is no temporal reprojection in the harness, so this measures
per-frame identity and the absence of frame-to-frame material drift, not history rejection.

### 5.4 Accepted and rejected examples (candidate rows for T057)

These are observations with artifacts, offered as input to the independent review. They are **not**
verdicts; T057 records the verdicts in `review-q4.md`.

| Dimension | Artifact | Observation | Candidate |
|---|---|---|---|
| 4.1 materials | `01-static-opaque-dielectric-metal.png` | The dielectric plane/cube/sphere reads matte-rough with visible normal detail; the metal sphere is darker, tinted, and carries a tight bright specular lobe. The two are distinguishable without moving the camera. The metal cannot show reflected scene structure because the reflection source is the analytic sky gradient, not a scene probe. | accept (distinctness); the reflection-source limit is 4.3, not a metalness failure |
| 4.1 / 4.2 alpha cutout | `02-alpha-tested-emissive.png` | Light passes through the grating holes: sky above the horizon and the ground below it are visible through them; the hole edges are clean at this extent. | accept (cutout) |
| 4.2 shadows | `crops/grating-shadow-solid.png` | The grating's cast shadow is a solid rectangle — the shadow pass has no pixel shader, so the alpha test is not applied when casting (`harness_pipeline.cpp:332-345`). | **reject** (shadow shape does not match the caster) |
| 4.2 / 4.1 transmission | `05-transmissive-environment.png` | The panel is a smooth tinted surface; the floor and horizon do not show through it, and there is no refraction or thickness. The shader documents this as a bounded approximation (`q4_hybrid.hlsl:287-294`). | **reject** for the transmission claim; accept only as "the transmissive route renders and does not take the full opaque direct term" |
| 4.4 darkness / readability | `02-alpha-tested-emissive.png` | The emissive bars are the brightest element but the panel frame and the surrounding ground keep their separation; no clipped white blob swallows neighbouring detail. The emissive is SDR: radiance above 1.0 is blocked on the C-10 format decision, so "HDR emissive keeps detail above 1.0" is **not tested**. | accept (readability); HDR part NOT TESTED |
| 4.5 identity / reproducibility | `04-moving-frame-8.png` | Byte-identical (SHA-256 `5d5dd3c9…fbb4`) to `evidence/q4-harness-moving.png` from the pre-merge one-cell run, although the executable was rebuilt and six more packages are loaded. The pre-merge committed static frame (`abe8f482…`) is likewise byte-identical to this run's `moving` frame 1. | accept (deterministic frame identity across the content merge) |
| 4.6 motion | `03-moving-frame-1.png` → `04-moving-frame-8.png` | 4.43–6.65 % of pixels change per adjacent pair, all inside the cube/shadow box; the background is unchanged. No ghost or smear is possible on this path (no history), and no material swim was observed. | accept (rigid stability); skinned motion is NOT RUN |
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

## 6. What is not claimed

- Q4 is **not complete** and not green. T056 is PARTIAL (skinned motion, transmission and VRAM peak
  missing); T057 is NOT RUN; the provider branch of T086 is NOT RUN.
- No modern-look acceptance, no classic parity, no DXR, no DLSS. There is a measured frame envelope on
  one adapter and there is still **no VRAM figure**; the frame numbers are a regression ceiling for this
  host, not a performance claim for any other device.
- No capture from spec 006 is cited or reused; no `-HdTextures` switch is involved.
- The portable slices do not prove the engine/fork wiring. That needs its own fork PR, submodule bump
  and independent review.
