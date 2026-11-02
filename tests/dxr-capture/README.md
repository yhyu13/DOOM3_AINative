# `tests/dxr-capture` — Q6 DXR path-traced capture (T069)

Standalone D3D12/DXR path tracer used to produce the T069 visual evidence in
`specs/004-quality-renderer/evidence-q6.md` §4.5. It is **capture tooling, not the engine**:

- it renders **synthetic fixtures** built in `capture_scene.cpp`, not the Q4 `content/` cells;
- it produces a **reference-mode** still (offline accumulation), not a Q6 engine frame;
- it is a **separate project**, like `tests/dxr-probe`, so `tests/dxr` stays device-free.

A capture from this tool does **not** satisfy the PT-S1..S5 content identity in
`path-tracing-rubric.md`, and no result here is Q6 acceptance. It does exercise the same
transport model as the merged CPU slice, on a real DXR device, with recorded convergence,
noise, timing and image hashes.

## Build and run

```powershell
cmake -S tests/dxr-capture -B build-dxr-capture -G "Visual Studio 17 2022" -A x64 `
  -DDXC_EXE="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"
cmake --build build-dxr-capture --config Release -- -m
.\build-dxr-capture\Release\q6_dxr_capture.exe --out specs/004-quality-renderer/evidence/t069 --width 320 --height 180
```

`dxc.exe` is required for the shader (the CMake cache variable `DXC_EXE` may be omitted when the
Windows SDK `bin/*/x64/dxc.exe` is found automatically).

| Option | Meaning |
|---|---|
| `--out <dir>` | output directory for PNGs and `report.json` (created if missing) |
| `--width`, `--height` | render extent (default 320x180) |
| `--adapter <substring>` | select an adapter by name substring; default prefers a discrete non-software adapter |
| `--spp-cap <n>` | clamp every sample count (smoke runs); the report records the clamped values |
| `--skip-timing` | skip the separate 640x360 timing run |

Exit codes: `0` every capture executed and passed its declared checks; `1` HRESULT, device removal or
write failure; `2` DXR unavailable on the selected adapter; `3` a capture executed and failed a
declared check. The verdict string and the exit code come from the same recorded values.

## Fixtures

| Id | Fixture | Transport exercised | Declared identity |
|---|---|---|---|
| `00-flat-denoise` | uniform diffuse wall, upper 1 / lower 0 environment | flat-radiance noise field for the denoiser | synthetic |
| `01-calibration` | same wall | analytic mean and single-sample variance | synthetic |
| `02-static` | floor, back/side walls, conductive sphere, dielectric sphere, one point light, environment | direct + indirect, conductor, dielectric | synthetic (PT-S1 shape only) |
| `03`/`04` rigid | lit room plus a red box translated between frame 0 and 8 | TLAS instance placement, no BLAS rebuild | synthetic (PT-S3 rigid half only) |
| `05`/`06` skinned | hanging sheet deformed by two-bone linear blend skinning | BLAS `PERFORM_UPDATE` refit | synthetic; **not** the `skinned_character` content |
| `07` alpha-emissive | HDR emissive sign (radiance 4.0/3.2/2.2) behind a checker-alpha cutout | alpha any-hit, emission above 1.0 | synthetic; the HDR value is not authored content |
| `08` alpha-emissive-control | same scene with the alpha test disabled | negative control: the sign must be hidden | synthetic control |
| `09` transmission | constant 0.8 emissive backdrop, one clear and one tinted single-plane dielectric (IOR 1.5) | Snell/Fresnel transmission and tint | synthetic; no glass thickness |

The rigid fixture moves one instance by translation only; the tool makes no rotation or non-uniform
scale claim. The skinned fixture is a 16x8 sheet with two rotating bones and linear blend weights;
it exercises the refit path, not a rigged character asset.

## Transport and sampling

`pathtrace_capture.hlsl` mirrors the documented Q6 CPU model (`pathtrace_material.h`):

- cosine-weighted Lambertian, bounded two-lobe conductor, Schlick/Snell dielectric with a transmission
  tint, emission, next-event estimation against one point light;
- a two-hemisphere environment split at world `z = 0`;
- alpha testing with the same deterministic 2x2 checker cell as the CPU fixture, evaluated in an
  any-hit shader so both primary and shadow rays see the same holes;
- geometric triangle normals (no vertex normals), matching the CPU slice; the spheres are 24x12, so
  their faceting is visible and is not a smoothing claim;
- iterative path loop in the ray-generation shader (`MaxTraceRecursionDepth = 1`), one `DispatchRays`
  per 1..16 samples, a 64-byte payload, PCG32 seeded from `(pixel, absolute sample index)`;
- progressive mean into an RGBA32F target; the dispatch index is the number of samples already
  accumulated. The CPU slice's splitmix/LCG sequence is **not** reused; equality of sample sequences is
  not claimed.

Acceleration structures: one BLAS per object with `ALLOW_UPDATE`, a TLAS rebuilt per capture. Skinned
frames build the rest pose first, then refit to the target pose with `PERFORM_UPDATE` from the rest
BLAS; the upload content is read at execution time, so the build and the refit are separate
submissions. Rigid motion is an instance translation in the TLAS instance descriptor.

## Declared checks

Every capture must have `nan-pixels == 0` and a byte-identical linear-RGB re-render
(`deterministic-rerun`). Captures with a 4096-spp reference also require:

| Check | Threshold | Source |
|---|---|---|
| `mae` (1024 vs 4096, linear, all pixels) | ≤ 0.01 | rubric §3 |
| `p99-relative` (declared patch) | ≤ 0.05 | rubric §3 |
| `sample-scaling` `mae(512)/mae(1024)` | in [1.5, 2.5] | rubric §3 |
| flat-field denoise `denoised/raw` luminance variance | ≤ 0.25 | rubric §4 |

The 4096-spp reference is a **higher-sample accumulation of the same estimator**, not an independent
implementation. Independence is provided by the analytic checks instead: the calibration wall's mean
must be `albedo * 0.5` within 0.01 and its single-sample variance `albedo^2 * 0.25` within 12%; the
alpha scene must show HDR sign pixels through the holes and none with the alpha test disabled; the
tinted/clear panel ratios must match the declared tint. The static wall patch is reported but **not**
used for the flat-variance gate because it carries a point-light shading gradient; the gate uses the
uniform calibration field.

`edge-bleed`, `history-ghost` and `history-reset` are **not measured** by this tool: the fixtures
declare no step edge, and it has no temporal history/cut path.

## Output

- one PNG per capture at the extent and sample count recorded in the report;
- `00-flat-denoise-64spp-denoised.png`, `02-static-64spp-denoised.png` from the merged Q6
  `DenoiseBilateral` (radius 2, sigmaColor 0.25, sigmaDepth 0.5);
- `report.json`: adapter, LUID, vendor/device, driver, tier, shader model, debug-layer state, extent,
  per-capture sample counts, `mae`, `p99`, scaling, NaN count, determinism, SHA-256 of the PNG and of
  the linear RGB buffer, metric verdict and check detail; timing median/p95 and the explicit
  `input_latency: NOT RUN`; `q6_acceptance: false`.

Display transform: linear radiance → Reinhard `x/(1+x)` → sRGB 8-bit. All metrics are computed on
linear values **before** this transform.

## Reproducibility and limits

- Same adapter and driver, same arguments: the 13 committed PNGs were byte-identical across two
  separate process runs and every in-process re-render matched. Cross-vendor or cross-driver
  bit-identity is **not** claimed.
- Measured on `Intel(R) Graphics` (vendor 0x8086 device 0x7D67, LUID `0x000000000001725F`, UMD
  `32.0.101.8331`, DXR tier 1.1, SM 6.6). The debug layer was unavailable on that host, so there is
  no D3D12 debug-error count for these runs.
- Not the engine, not a Q6 frame, not PT-S1..S5, not a playable showcase, no input latency, no VRAM
  figure, no comparison with the hybrid renderer. T069 remains PARTIAL and T070 remains NOT RUN.
