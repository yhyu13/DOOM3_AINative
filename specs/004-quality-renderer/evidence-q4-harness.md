# Q4 D3D12 capture harness evidence (T056)

Status: a real Q4 capture was produced on 2026-10-30. This is evidence that a
Q4-capable D3D12 build renders an authored PBR cell, not Q4 acceptance and not
an art verdict (T057 stays independent). The run predates the six-cell content
merge at `127ab39` and therefore covers only `opaque_dielectric`.

> **This file records the pre-merge one-cell run.** The later seven-cell /
> four-category run at `348487c` is recorded in
> [`evidence-q4.md`](./evidence-q4.md) §5 and
> [`evidence/t056/measurements.json`](./evidence/t056/measurements.json), with
> frames under [`evidence/t056/`](./evidence/t056/). Where the two disagree, the
> later run is current: the static frame now includes the metal sphere, and the
> scene-status table below is superseded.
>
> **Superseded again by the B1/B2 fix at `50efd1a`.** The shadow pipeline state
> described below (depth-only, no pixel shader) is now history: the shadow pass
> has an alpha-tested pixel shader (`PSShadowAlpha`) and each caster binds its
> material table, so a cutout casts a perforated shadow. The material sampler
> stayed trilinear after an anisotropic 8× experiment was measured and reverted.
> Current binary SHA-256
> `6a2d50bb2e23670e3334b43cace3962dfa1a2bca86819bcd429030a5d9690748`; what
> changed and what was measured is in [`evidence-q4.md`](./evidence-q4.md) §5.7.

Issue: #70. Base: `127ab39` (PR #72 content merge); the capture run predates
this rebase and used the one-cell content set.
Host: Windows 11 22631, Visual Studio 2022 17.14.35228, Windows SDK 10.0.26100,
Intel(R) Graphics driver 32.0.101.8331, WARP available.

## Build

```powershell
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64
# exit 0

cmake --build build-q4-harness --config Release -- -m
# exit 0; output build-q4-harness/bin/Release/q4_harness.exe
```

The host used the CMake bundled with Visual Studio 2022 by its VS path; the
commands above use `cmake` for portability.

Evidence artifact: `build-q4-harness/bin/Release/q4_harness.exe`,
SHA-256 `eef5da331fd9f8b177d0426538e8e1d314b8e647d402db31fbdcc70aa8b3bc8f`.

## Adapter selection

```powershell
.\build-q4-harness\bin\Release\q4_harness.exe -ListAdapters
# exit 0
```

```
Hardware adapters:
  [0] Intel(R) Graphics (vendor=0x8086 device=0x7D67 memory=134217728)
  [1] Intel(R) Graphics (vendor=0x8086 device=0x7D67 memory=134217728)
  [2] Intel(R) Graphics (vendor=0x8086 device=0x7D67 memory=134217728)
  [3] Intel(R) Graphics (vendor=0x8086 device=0x7D67 memory=134217728)
Software adapters:
  [0] Microsoft Basic Render Driver (vendor=0x1414 device=0x008C memory=0)
```

## Official capture

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/capture-q4.ps1 `
  -Material hero_dielectric `
  -Exe build-q4-harness\bin\Release\q4_harness.exe `
  -Backend d3d12 -Adapter "Intel(R) Graphics" `
  -Screenshot out\content\hero_dielectric\capture.png `
  -EngineArgs "-Scene;static_opaque;-Capture;out\content\hero_dielectric\capture.png;-AdapterIndex;0"
# exit 0
```

```
CAPTURE OK: hero_dielectric d3d12 Intel(R) Graphics
  image: out\content\hero_dielectric\capture.png (1280x720, 6274 colours)
  record: out\content\hero_dielectric\capture.json
```

`capture.json` records:

| Field | Value |
|---|---|
| identity_line | `RHI backend: d3d12 (Intel(R) Graphics)` |
| backend | `d3d12` |
| adapter | `Intel(R) Graphics` |
| image | `capture.png`, 1280x720, 6274 distinct colours |
| image_sha256 | `abe8f482a37514ebaf9ef988a1a355e03ab19b6f2526137cc3e6bc9fc2c5537b` |
| executable_sha256 | `eef5da331fd9f8b177d0426538e8e1d314b8e647d402db31fbdcc70aa8b3bc8f` |
| package_sha256 | `8aaeb7328dd798877afc4e22829e8f4e082f307e7cf9bb506ab24bdda7471608` |

Committed image: `evidence/q4-harness-static.png` (same SHA-256 as
`capture.png`). The frame shows the authored `hero_dielectric` base colour,
normal and roughness channels on a plane, cube and sphere, one directional key
with a 2048x2048 PCF shadow, an SH ambient probe, a bounded environment
reflection and height fog. The image is a capture, not a generated placeholder.

## WARP

```powershell
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene static_opaque -Capture out\content\hero_dielectric\warp.png -Warp
# exit 0; FRAME 0: 66.78 ms; CAPTURE WROTE: out\content\hero_dielectric\warp.png
```

```
RHI backend: d3d12 (Microsoft Basic Render Driver)
```

Committed image: `evidence/q4-harness-warp.png`,
SHA-256 `817635e38478a06e5723c217ed65d2e1a2a9c2cdb2129cd9c33cf2a9c1dfc20f`.

## Moving category (partial)

```powershell
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene moving -Frames 8 -Capture out\content\hero_dielectric\moving.png -AdapterIndex 0
# exit 0; FRAME 0..7 each rendered; CAPTURE WROTE: ...\moving-moving.png
```

The rigid prop uses the authored `opaque_dielectric` cell and rotates between
frames. The run predates the six-cell content merge, so the skinned-character
cell was not in the run's content set; the category is reported `PARTIAL`, not
claimed as complete. `-Capture` is the base path: the harness appends
`-<scene>` for every scene other than `static_opaque`, so this run wrote
`moving-moving.png`. `-Scene all` writes the base path for `static_opaque` and a
`-<scene>` sibling for each other runnable scene. Committed image:
`evidence/q4-harness-moving.png`,
SHA-256 `5d5dd3c9118b5651ef81ea66ba9a8e63214994a924fc3d5e8dd2658a78adfbb4`.

## Reconstruction hook

```powershell
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene static_opaque -RenderScale 0.75 `
  -Capture out\content\hero_dielectric\recon.png -AdapterIndex 0
# exit 0
```

```
Q4 reconstruction: SpatialUpscale
FRAME 0: 2.53 ms
CAPTURE WROTE: out\content\hero_dielectric\recon.png (1280x720)
```

The scene target is 960x540 and the fullscreen pass bilinearly reconstructs to
1280x720. With the default `-RenderScale 1.0`, `PlanReconstruction` reports
`Off` and the same pass runs at identity extent.

## All-scenes pass

```powershell
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene all -Capture out\content\hero_dielectric\all.png -AdapterIndex 0
# exit 0; writes all.png and all-moving.png
```

This exercises the state transitions across two consecutive scenes in one
process (scene colour, shadow map and display target go
`PIXEL_SHADER_RESOURCE`/`COPY_SOURCE` back to `RENDER_TARGET`/`DEPTH_WRITE`).

## Scene status

| Category | Status | Cells used by the run | NOT RUN cells |
|---|---|---|---|
| `static_opaque` | PARTIAL | `opaque_dielectric` | `opaque_metal` (not in the run's content set) |
| `alpha_emissive` | NOT RUN | none | `alpha_tested`, `emissive` (not in the run's content set) |
| `moving` | PARTIAL | `opaque_dielectric` (rigid prop) | `skinned_character` (not in the run's content set) |
| `transmissive_environment` | NOT RUN | none | `transmissive_glass`, `large_environment` (not in the run's content set) |

The run's content set predates the six-cell merge at `127ab39`, so only
`hero_dielectric` was available to it. The cells authored by PR #72 exist in the
repository now but were not captured; they are never replaced by a stand-in
material or a fabricated frame.

## Hooks logged

```
Q4 material: hero_dielectric: HybridOpaque linear_hdr=1 srgb_decoded=1 (opaque inputs accepted)
Q4 direct reference: attenuation=1.000 ndotl=0.880 shadow=1.000 radiance=(0.377, 0.317, 0.251)
Q4 indirect reference: irradiance=(0.141, 0.160, 0.200) reflection=(0.262, 0.283, 0.336) horizonFallback=1 transmittance=0.946
Q4 residency: budget=268435456 uploadPerFrame=67108864 resident=873880 uploaded=873880 deferred=0
Q4 reconstruction: Off
Q4 shadow: 2048x2048 directional, PCF 3x3 comparison sampler
```

These are the portable T051/T052/T053/T054/T055 slices running inside the
harness. The per-pixel shader implements the same material route, SH probe,
environment fallback, fog and shadow contract; the CPU values are bounds and
accounting, not an image-parity claim.

## Shared changes (reported)

1. `src/renderer/d3d12/d3d12_private.h` and `rhi_d3d12.cpp`: additive
   `BackendAccess::NativeDevice(Device&)` so the harness can render its own
   resources on the RHI-selected device. The public RHI header is unchanged and
   `src/renderer/rhi/raytracing_scene.h` is untouched.
2. `content/tools/capture-q4.ps1`: the identity regex now captures an adapter
   name containing parentheses (`Intel(R) Graphics`) by taking the adapter
   group to the last `)` on the line. The previous regex stopped at the first
   `)` and could not verify this host's adapter.
3. `content/tools/generators/dielectric.py`: replaced a private, degenerate copy
   of the tileable-noise helper with the shared `hero_common` one
   (`evidence-q4.md` §5.7, B-2). One implementation instead of two.
4. Shadow pass (`harness_pipeline.cpp`, `harness_render.cpp`,
   `shaders/q4_hybrid.hlsl`): alpha-tested depth for casters that declare an
   alpha test (`evidence-q4.md` §5.7, B-1). No root/target/test CMake change.

## Limitations / blockers

- The capture run used the one-cell content set (`opaque_dielectric`) and
  predates the six-cell content merge at `127ab39`. The metal, alpha-tested,
  emissive, skinned-character, transmissive-glass and large-environment cells
  were not in that run; the corresponding categories are `NOT RUN` or `PARTIAL`.
  They are now authored in the repository but are not captured here.
- No DLSS, DXR, Q6 or Q7 path is added.
- The harness is a standalone evidence target; root/target/test CMake files are
  unchanged.
- Build output, the material package and the extracted captures stay under
  git-ignored `out/` and `build-q4-harness/`; only the three evidence PNGs and
  this record are committed.
