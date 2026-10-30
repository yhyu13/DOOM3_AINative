# Q4 D3D12 capture harness

Standalone evidence harness for spec 004 T056. It renders the authored Q4 hero
cell through the hybrid opaque path and proves the capture is real by printing
the RHI-selected backend and adapter and writing a PNG.

This is **not** the engine build, not Q4 acceptance, and not an art verdict
(T057 stays independent). It does not add DLSS, DXR, Q6 or Q7.

## Build

The harness reads the packaged hero cell, so build the content package first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/build-material.ps1 `
  -Material hero_dielectric -OutDir out/content/hero_dielectric
```

Then build the harness:

```powershell
cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64
cmake --build build-q4-harness --config Release -- -m
```

The executable is `build-q4-harness/bin/Release/q4_harness.exe`. The HLSL is
copied next to it by the build.

## Run

```powershell
# show the adapters and the index each class uses
.\build-q4-harness\bin\Release\q4_harness.exe -ListAdapters

# hardware adapter
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene static_opaque `
  -Capture out\content\hero_dielectric\static.png `
  -AdapterIndex 0

# software fallback (WARP)
.\build-q4-harness\bin\Release\q4_harness.exe `
  -Scene static_opaque `
  -Capture out\content\hero_dielectric\warp.png `
  -Warp
```

The harness prints exactly one identity line:

```
RHI backend: d3d12 (<adapter>)
```

## Arguments

| Argument | Meaning |
|---|---|
| `-Scene <id>` | `static_opaque`, `alpha_emissive`, `moving`, `transmissive_environment`, or `all`. |
| `-Capture <png>` | Base output path. Parent directories are created. Required. For every scene other than `static_opaque`, the harness appends `-<scene>` to the stem (for example `moving.png` -> `moving-moving.png`). |
| `-Material <name>` | Material identity for the hook log (default `hero_dielectric`). |
| `-Warp` | Select the software (WARP) adapter. |
| `-AdapterIndex <n>` | Index within the selected adapter class. |
| `-ListAdapters` | Print hardware and software adapters and exit. |
| `-Width/-Height <n>` | Display extent (default 1280x720). |
| `-RenderScale <f>` | Scene extent scale; `< 1` exercises the bilinear reconstruction path. |
| `-Frames <n>` | Frames rendered before capture (default 1). |
| `-Exposure <f>` | Tone-map exposure (default 0.8). |
| `-ContentDir <path>` | Directory that holds `content/`. |
| `-PackageDir <path>` | Directory that holds `<material>/package.json`. |
| `-ShaderDir <path>` | Directory that holds the HLSL. |

Unknown `+set` and engine-style arguments are ignored, so the harness can be
driven by `content/tools/capture-q4.ps1`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/capture-q4.ps1 `
  -Material hero_dielectric `
  -Exe build-q4-harness\bin\Release\q4_harness.exe `
  -Backend d3d12 -Adapter "Intel(R) Graphics" `
  -Screenshot out\content\hero_dielectric\capture.png `
  -EngineArgs "-Scene;static_opaque;-Capture;out\content\hero_dielectric\capture.png;-AdapterIndex;0"
```

## What it renders

- Offscreen `R16G16B16A16_FLOAT` colour plus `D32_FLOAT` depth, a 2048x2048
  directional shadow map with a 3x3 PCF comparison sampler, and a fullscreen
  reconstruction/tone-map pass to `R8G8B8A8_UNORM_SRGB`.
- The hybrid opaque shader samples the packaged BC7/BC5/BC4 channels, decodes
  the declared OpenGL normal handedness, uses a GGX/Smith/Schlick direct term,
  an L1 SH ambient probe, a bounded environment reflection with a below-horizon
  fallback, exponential height fog, alpha test and an emissive path.
- The Q4 portable slices are compiled into the harness and used as hooks:
  `PlanModernMaterial` / `BuildHybridOpaqueInput` for the route,
  `EvaluateDirectLight` for the reference direct term,
  `EvaluateIrradiance` / `EvaluateReflection` / `EvaluateAtmosphere` for the
  indirect reference, `ResidencyPolicy` for texture-byte accounting, and
  `PlanReconstruction` for the render/display plan. The per-pixel form lives in
  the HLSL; the CPU values are logged as bounds, not as image parity claims.
- Four scene categories are defined. A category whose authored cells are absent
  is reported `NOT RUN`; no stand-in material or fabricated frame is produced.

## RHI integration

The public RHI does not yet expose the vertex/index/texture binding this
harness needs. The harness therefore renders with native D3D12 on the device the
RHI selected, through a minimal backend-private accessor
(`BackendAccess::NativeDevice`) in
`src/renderer/d3d12/d3d12_private.h` / `rhi_d3d12.cpp`. Adapter selection,
adapter identity, and device creation still come from the RHI. The public RHI
header and `rhi/raytracing_scene.h` are unchanged.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | capture written |
| 2 | argument/environment error |
| 3 | requested scene is `NOT RUN` (no authored cell) |
| 4 | render/device error |
| 5 | PNG readback/write error |
