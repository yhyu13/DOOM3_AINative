# Q4 hybrid-quality fixtures

Status: fixture definitions and observed values for spec 004 Q4 (T050–T055, T086), written 2026-10-30.
This document does **not** close Q4, does not claim T056/T057, and contains **no capture**. Every row
below executes on the CPU; there is no device, window, shader compiler or GPU work, and therefore no
image, frame time or VRAM measurement.

The executable implementations are:

| Task | Test | Linked production slice |
|---|---|---|
| T050 | `material_contract_test.cpp` | `rhi/materials.cpp`, `d3d12/hybrid_opaque.cpp` |
| T051 | `material_contract_test.cpp` | `rhi/materials.cpp`, `d3d12/hybrid_opaque.cpp` |
| T052 | `lighting_test.cpp` | `d3d12/lighting.cpp` |
| T053 | `indirect_atmosphere_test.cpp` | `d3d12/indirect_lighting.cpp` |
| T054 | `residency_test.cpp` | `rhi/residency.cpp` |
| T055/T086 | `reconstruction_test.cpp` | `rhi/reconstruction.cpp`, `rhi/overlay_composition.cpp` |

Every expected value is an independent literal or a formula documented in the corresponding header.
No expectation is copied from an implementation output.

## Prerequisites

| Item | Requirement | Missing outcome |
|---|---|---|
| Compiler | MSVC v143, `/W4 /permissive- /EHsc`, C++17 | Configure or build fails |
| Device | None | These fixtures allocate no device, surface or shader |
| Content | None | The material/budget state is cited from `content/` but no file is read at run time |
| Provider | None | The reconstruction path is portable; a missing provider is the tested case |

## T050 — material conversion and legacy fallback

| Case | Input | Expected | Observed |
|---|---|---|---|
| sRGB decode table | 0.0, 0.04045, 0.25, 0.5, 0.75, 1.0 | 0, 0.0031308, 0.050876, 0.214041, 0.522522, 1 | pass, ≤ 1e-6 |
| channel order | sRGB (0.5, 0.25, 0.75) | decoded r/g/b keep their order | pass |
| dielectric hero | base + roughness + declared-dielectric metalness + declared-handedness normal | `HybridOpaque`, sRGB base, no forward reason | pass |
| legacy specular | base colour, no roughness, legacy specular map | `ForwardCompatibility`, reason names specular | pass |
| missing roughness | no roughness, no legacy specular | `ForwardCompatibility` | pass |
| undeclared normal | authored normal, handedness undeclared | `ForwardCompatibility`, reason names handedness | pass |
| undeclared metalness | metalness unavailable, not dielectric | `ForwardCompatibility` | pass |
| translucent / special | coverage `Translucent`, or a special stage | `ForwardCompatibility` | pass |
| missing base colour | no base colour | `ForwardCompatibility` | pass |
| HDR emissive without channel | `emissiveHdr` set, no emissive | `ForwardCompatibility` | pass |
| alpha-tested | coverage `AlphaTested` | stays `HybridOpaque` | pass |
| opaque values | sRGB 0.5 base, HDR emissive 4.0, alpha 0.4 cutoff 0.5 | linear base 0.214041, emissive 4.0 preserved, alpha discarded | pass |
| refusals | encoded base 1.5, emissive 2.0 without HDR, roughness 1.5 | `InvalidArgument` | pass |

## T052 — direct light and shadow

| Case | Input | Expected | Observed |
|---|---|---|---|
| attenuation | d=0, r=6 | 1 | pass |
| attenuation | d=3, r=6 | 0.09765625 | pass |
| attenuation | d≥r | 0 | pass |
| directional | N·L = 1, albedo 0.5, roughness 1 | diffuse 0.5, specular 0 | pass |
| directional | N·L = 0.5 | diffuse 0.25 | pass |
| back-facing | N·L < 0 | valid unlit result | pass |
| point light | d=3, r=6 | attenuation 0.09765625, diffuse 0.048828 | pass |
| metal | albedo 1, metalness 1, roughness 0 | diffuse 0, specular 1 | pass |
| shadow bias | depth inside bias | shadow factor 1 | pass |
| PCF | 4 taps, 1 blocker | 0.75, diffuse scaled to 0.375 | pass |
| PCF | 4 taps, 4 blockers | 0 | pass |
| unsupported | translucent, blend, fog, glasswarp, skybox, decal, post-process | `Unsupported` naming the stage | pass |
| alpha test | alpha-tested miss | `Ok`, `discarded`, unshaded | pass |
| refusals | zero normal, negative radiance, inner cone < outer cone | `InvalidArgument` | pass |
| spot | outside the cone | `Ok`, unlit, attenuation 0 | pass |

## T053 — indirect, reflection and atmosphere

| Case | Input | Expected | Observed |
|---|---|---|---|
| constant probe | c0 = 1 | irradiance 0.282095 | pass |
| +Y probe | c1 = 1, normal +Y | 0.488603; normal +X = 0 | pass |
| negative probe | c0 = −1 | clamped to 0 and reported | pass |
| mirror | incident −Y, normal +Y, roughness 0 | zenith 2.0, no fallback, no blur | pass |
| below horizon | incident +Y, normal +Y | ground 0.1, `horizonFallback` | pass |
| rough | roughness 1 | sky average 0.866667, `blurred` | pass |
| refusals | roughness 1.5, negative environment | `InvalidArgument` | pass |
| atmosphere | distance 1, density 1 | transmittance e⁻¹, HDR fog colour preserved | pass |
| atmosphere | distance 0 / disabled | transmittance 1, zero in-scatter | pass |
| height falloff | camera 10, fog 2, falloff 1 | transmittance exp(−exp(−8)) | pass |
| refusals | negative distance, NaN height | `InvalidArgument` | pass |

## T054 — residency

| Case | Input | Expected | Observed |
|---|---|---|---|
| mip selection | 400/100/25 B, available 500 | level 0, 400 B | pass |
| mip selection | available 250 | level 1, 100 B, reduced | pass |
| mip selection | available 50 | level 2, 25 B | pass |
| quality floor | coarsest allowed 1, available 50 | `Unsupported` with reason | pass |
| malformed | id 0, empty chain, floor outside chain | `InvalidArgument` | pass |
| throttle | 150 B/frame, two 100 B uploads | frame 1 starts 1, frame 2 starts 1 | pass |
| budget | 1000 B full, 100 B request | deferred and `budgetBlocked`, no eviction | pass |
| early release | last use 5, completed 4 | deferred, resource stays resident | pass |
| safe release | completed 5 | 400 B freed, waiting upload admitted | pass |
| policy mip | 700/1000 resident | level 1 selected | pass |
| config | zero budget / zero upload | `InvalidArgument` | pass |

## T055/T086 — reconstruction and ordering

| Case | Input | Expected | Observed |
|---|---|---|---|
| upscale plan | 640×360 → 1280×720 | `SpatialUpscale`, no provider required | pass |
| provider fallback | provider preferred, unavailable | `providerFellBack`, plan still Ok | pass |
| off plan | equal extents | `Off`, identity | pass |
| refusals | zero extent, smaller display | `InvalidArgument` | pass |
| off image | 2×2 RGBA | byte-identical copy | pass |
| horizontal upscale | 2×1 → 4×1, {0,1} | 0, 0.25, 0.75, 1 | pass |
| 2D upscale | 2×2 → 4×4, {0,1;1,0} | texel (1,1) = 0.375 | pass |
| refusals | NaN input, mismatched off extents, null input, downscale | `InvalidArgument` | pass |
| ordering | scene + world GUI at render, HUD at display | order scene, GUI, HUD; 2 reconstruction inputs | pass |
| mapping | scene (100,50) at 2× | (50,25) | pass |
| mapping | HUD (100,50) | (100,50), one to one | pass |
| blurred HUD | HUD declared at render extent | `InvalidArgument` with reason | pass |
| off ordering | equal extents | same order, identity reconstruction | pass |

The provider-specific display-stage position is **NOT RUN**: no authorized provider exists (T060) and
T062 has not executed. That branch is not a pass and does not block the off-path check above.
