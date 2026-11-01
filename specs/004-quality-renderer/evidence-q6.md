# Q6 path-traced reference/showcase evidence (T063–T070)

Status: author evidence for issue [#68](https://github.com/yhyu13/DOOM3_AINative/issues/68), updated
2026-11-01 on `spec-004-q6-dxr-t069` from `origin/main` at `b244458`. This is **not** Q6 acceptance,
not an engine frame and not an independent review. It records what the CPU reference slice and a
standalone D3D12 probe do on this host, and what could not run.

**Headline: the Q6 CPU slice is implemented (T063, T065–T068, and T064's selection half). A new
standalone probe proves this host's adapter reports D3D12 ray-tracing tier 1.1, creates a DXR device,
builds a BLAS and TLAS and executes a trivial `DispatchRays` (64/64 pixels, 0 debug errors). There is
still no Q6 scene renderer, capture, image, frame time, VRAM or input-latency measurement. Every
visual row is NOT RUN. The CPU harness passes 5/5 in Debug, but Release fails 1 of 45 checks in the
denoiser on this host's MSVC 19.38 toolchain. T069 is PARTIAL; T070 is NOT RUN. The non-path-traced
renderer remains available.**

## 1. Identity and scope

| Field | Value |
|---|---|
| Stage and claim | Q6 CPU reference slice plus a device capability/dispatch probe. Claim: the probe's reported tier, device, acceleration structures and dispatch are real and reproducible. **No** Q6 frame, art, parity, capture, budget or Q6-performance claim; the device tier is a capability fact, not a Q6 verdict. |
| Issue / branch | #68, `spec-004-q6-dxr-t069`, branched from `origin/main` at `b244458`. The earlier CPU slice was authored in #68 / PR #73 (`feat/q6-dxr-showcase`). |
| Submodule | `doom3_orig/doom3` is untouched; `git -C doom3_orig/doom3 status --porcelain` is empty. No fork or submodule change in this PR. |
| Data identity and rights | Synthetic CPU fixtures and one synthetic triangle in the probe. No `.pk4`/`.pk3`/`.pak` is present or committed. The probe reads no `content/` file and no game data. |
| Exclusions | The engine fork, an engine call site, Q6 content capture, all images, DLSS/NR (Q5), Q7 profiles, and the target build (`targets/win11-amd64/CMakeLists.txt` untouched). |

Task-path deviation, recorded rather than hidden: T064–T068 name `tests/renderer/*.cpp`. The issue
constraint for #68 is that new Q6 tests live in `tests/dxr` with their own CMake project, and that
`tests/renderer/CMakeLists.txt` and the target CMake are not edited. The behaviour is tested in
`tests/dxr`, and the implementation files keep the task names under `src/renderer/d3d12/`. The probe
lives in a second standalone project, `tests/dxr-probe`, so the CPU harness keeps linking no device.

## 2. Environment and configuration

| Item | Value |
|---|---|
| OS / arch | Windows 10 10.0.19045 (build 19045), x64 |
| Compiler | MSVC 19.38.33145.0 (VS 2022 Community, toolset v143), Windows SDK 10.0.26100.0 |
| Build tool | CMake 4.1.2, generator `Visual Studio 17 2022 -A x64` |
| Build roots | `build-dxr/` (CPU harness) and `build-dxr-probe/` (device probe), both git-ignored |
| Adapter and driver | **AMD Radeon RX 9070 XT**, LUID `0x000000000000E83C`, vendor `0x1002`, device `0x7550`, 16,304 MB dedicated, UMD `32.0.21030.31`. The probe also enumerates an integrated `AMD Radeon(TM) Graphics` and a second `AMD Radeon RX 9070 XT` entry; the selected device is the first named adapter. |
| Ray-tracing tier | `D3D12_FEATURE_D3D12_OPTIONS5.RaytracingTier = 1_1` (11), queried at runtime. Highest shader model `6.5`. |
| Shader provenance | Probe only: `tests/dxr-probe/dxr_trivial.hlsl` compiled to DXIL `lib_6_3` by the SDK's `dxc.exe`. The Q6 CPU slice still compiles no shader. |
| Linked libraries | CPU harness `q6_dxr`: C++ runtime only, no `d3d12`, `dxgi` or provider link. Probe: `d3d12` and `dxgi`, and no engine or Q6 library. |

The earlier revision of this file said only 1 of 7 Q4 content cells was authored and cited
`Intel(R) Graphics`. Both statements are now stale: the tree contains seven authored cells
(`content/tools/matrix-report.ps1` reports 7/7 authored), and this host is AMD. The visual rows
remain NOT RUN because a Q6 device renderer and Q6 frame do not exist, not because the content is
missing.

## 3. What was implemented

| Task | Files | Behaviour |
|---|---|---|
| T063 | `specs/004-quality-renderer/path-tracing-rubric.md` | Five scene identities, configuration identity, convergence/noise/latency metrics and pre-review thresholds. No threshold is measured. |
| T064 | `src/renderer/rhi/raytracing_capability.h`, `src/renderer/d3d12/raytracing_capability.cpp` | Explicit `Hybrid` / `Reference` / `Showcase` selection; device support and shader-model 6.3 requirement; showcase needs the visual, performance and motion gates; a failed requirement is `Unsupported` unless the caller explicitly approves the hybrid fallback. |
| T065 | `src/renderer/rhi/pathtrace.h`, `src/renderer/d3d12/pathtrace.cpp` | Camera rays, deterministic per-pixel/sample seeds, iterative path integrator, next-event estimation against point lights, two-hemisphere environment, alpha-aware shadow rays. |
| T066 | `src/renderer/rhi/raytracing_scene.h`, `src/renderer/d3d12/raytracing_scene.cpp` | Triangle hierarchy with median split, bottom-up refit for posed/skinned vertices, one affine instance placement (rigid motion without rebuild), topology-change refusal, nearest-hit query with barycentrics and uv. |
| T067 | `src/renderer/rhi/pathtrace_material.h`, `src/renderer/d3d12/pathtrace_material.cpp` | Lambertian cosine sampling, bounded two-lobe conductor, Schlick/Snell dielectric with transmission tint, emission, deterministic checker alpha mask. |
| T068 | `src/renderer/rhi/pathtrace_denoise.h`, `src/renderer/d3d12/pathtrace_denoise.cpp` | Reference accumulation, Q3-`MotionValidity` history rejection, edge-aware bilateral colour/depth denoise. |
| T069 | this file plus `tests/dxr-probe/` | Device tier, device/AS creation, trivial dispatch, CPU harness status, and every visual row recorded as NOT RUN. |
| T070 | — | **NOT RUN**: needs an independent reviewer; the implementer cannot author the verdict. |

Design boundaries worth stating:

- The tests exercise production sources, not test-owned copies: `tests/dxr/CMakeLists.txt` builds
  `q6_dxr` from the five `src/renderer/d3d12` slices plus the existing `motion_history.cpp`.
- The material model is bounded and documented; it is not a physically exact BRDF claim. Q6 is where a
  higher-ceiling reference is judged, and the CPU slice does not substitute for that judgement.
- Alpha testing uses a deterministic checker mask so "passes through a hole" is checkable without an
  image. It is not a claim about engine texture sampling.
- The probe is not the Q6 renderer. It builds one triangle and one instance, not a scene, and its
  dispatch time is not a frame time.

## 4. Procedure and retained artifacts

All commands ran from the worktree root. Full logs are retained in the git-ignored `build-dxr/` and
`build-dxr-probe/`.

### 4.1 Device probe

```text
cmake -S tests/dxr-probe -B build-dxr-probe -G "Visual Studio 17 2022" -A x64  -> 0
cmake --build build-dxr-probe --config Release -- -m                        -> 0
.\build-dxr-probe\Release\q6_dxr_probe.exe                                  -> 0
```

Selected output (`build-dxr-probe/_probe.log`):

```text
ADAPTER[0]=AMD Radeon RX 9070 XT
ADAPTER[0].LUID=0x000000000000E83C
ADAPTER[0].VENDOR_ID=0x1002 DEVICE_ID=0x7550
ADAPTER[0].DEDICATED_VRAM_MB=16304 SOFTWARE=0
SELECTED_ADAPTER_NAME=AMD Radeon RX 9070 XT
DRIVER_UMD=32.0.21030.31
DEBUG_LAYER_ENABLED=1
DEVICE_CREATED=1
RAYTRACING_TIER=1_1 (11)
SHADER_MODEL=6.5
DXR_DEVICE_CREATED=1
BLAS_CREATED=1
TLAS_CREATED=1
STATE_OBJECT_CREATED=1
SBT_CREATED=1
DISPATCH_EXECUTED=1
DISPATCH_GPU_MS=0.005440
DISPATCH_HIT_PIXELS=64/64
DEBUG_ERRORS=0
RESULT=PASS
EXIT_CODE=0
```

`DISPATCH_GPU_MS` is a GPU timestamp around one 8x8 dispatch at one triangle. It is recorded as a
diagnostic only: it is not a Q6 frame, a p50/p95, or a comparison with the hybrid renderer.

### 4.2 Q6 CPU harness

```text
cmake -S tests/dxr -B build-dxr -G "Visual Studio 17 2022" -A x64      -> 0
cmake --build build-dxr --config Release -- -m                        -> 0
ctest --test-dir build-dxr -C Release --no-tests=error --output-on-failure -> 8 (4/5 passed)
cmake --build build-dxr --config Debug -- -m                          -> 0
ctest --test-dir build-dxr -C Debug --no-tests=error --output-on-failure   -> 0 (5/5 passed)
```

Per executable (Release / Debug), with the exact check counts:

| Executable | Release | Debug |
|---|---|---|
| `q6_raytracing_capability_test` | 37 checks / 0 failures, exit 0 | 37 / 0, exit 0 |
| `q6_pathtrace_static_test` | 68 / 0, exit 0 | 68 / 0, exit 0 |
| `q6_pathtrace_dynamic_test` | 38 / 0, exit 0 | 38 / 0, exit 0 |
| `q6_pathtrace_material_test` | 45 / 0, exit 0 | 45 / 0, exit 0 |
| `q6_pathtrace_denoise_test` | **45 / 1, exit 1** | 45 / 0, exit 0 |

The Release failure is `tests/dxr/pathtrace_denoise_test.cpp:255`,
`the depth guide keeps the far side from bleeding`. Direct reproduction:

```text
.\build-dxr\Release\q6_pathtrace_denoise_test.exe   -> 1
FAIL the depth guide keeps the far side from bleeding
SUMMARY q6_pathtrace_denoise_test checks=45 failures=1
```

Narrowing, run against the same source and input (`/MD`, C++17):

| Compiler configuration | Far-side pixel (R, G, B) |
|---|---|
| `/Od` | `(0.7000, 0.7000, 0.7000)` |
| `/O1` | `(0.7000, 0.7000, 0.7000)` |
| `/O2 /fp:strict` | `(0.7000, 0.7000, 0.7000)` |
| `/O2 /fp:precise` (Release default) | **`(0.0000, 0.0000, 0.7000)`** |
| `/O2 /Qvec-`, `/arch:AVX2`, `/arch:SSE2` | still `(0.0000, 0.0000, 0.7000)` |

The `/O2 /fp:precise` output is wrong, not a threshold miss: R and G are exactly zero while B is
correct. A standalone copy of the same loop also reproduces with `/O2 /fp:precise`; a smaller minimal
loop does not. This is a compiler/codegen-sensitive defect in the current denoiser on MSVC
19.38.33145.0, and it is **not fixed in this evidence PR** (changing the implementation needs its own
issue and independent review). The earlier claim that the Release suite is 5/5 and the denoiser
45/45 is toolchain specific and does not hold here.

### 4.3 Gates

```text
uv run --python 3.12 python scripts/validate-rhi-contract.py  -> 1
```

`py -3` is not installed on this host (`'py' is not recognized`, exit 1), so `uv` was used. The
validator reports 26 failures across five **pre-existing** `src/renderer/q4_harness` headers
(`content.h`, `dds.h`, `harness.h`, `harness_impl.h`, `vram_probe.h`), all outside this diff. The probe
adds no `src/renderer` header, so this is a baseline gate failure at `b244458`, not a result of this
branch. It is recorded rather than hidden; no validator change is made here.

```text
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/matrix-report.ps1 -> 0
MATRIX: coverage 7/7 cells authored, 0/7 captured, 7/7 captures BLOCKED, budget rows 7/7 present
```

This fresh worktree has no built `out/content` packages, so the matrix reports 0/7 captures here. The
mainline spec 009 record has 6/7 **hybrid** captures (`specs/009-rhi-hd-content/evidence/captures.md`);
none of them is a Q6 path-traced frame and none is used as Q6 evidence.

**Defects found during verification.**

1. **Release denoiser output (new, not fixed).** See §4.2. Release `/O2 /fp:precise` zeroes the R/G
   channels of the depth-guided bilateral result; Debug, `/O1` and `/O2 /fp:strict` are correct. This
   blocks any claim that the Q6 CPU suite is green in Release on this host.
2. **RHI contract gate (pre-existing).** Five `src/renderer/q4_harness` headers are reported for
   prohibited D3D12 vocabulary (26 lines). Not caused by this diff.
3. **Content-state correction (documentation).** The earlier "1 of 7 authored" reason for the visual
   rows is wrong; 7/7 cells are authored. The rows stay NOT RUN because there is no Q6 device
   renderer, no Q6 frame and no Q6 capture path.

**Logs** (git-ignored): `build-dxr-probe/_configure.log`, `build-dxr-probe/_build-Release.log`,
`build-dxr-probe/_probe.log`, `build-dxr/_configure.log`, `build-dxr/_build-Release.log`,
`build-dxr/_build-Debug.log`, `build-dxr/_ctest-Release.log`, `build-dxr/_ctest-Debug.log`,
`build-dxr/_validate-rhi.log`, `build-dxr-probe/_matrix.log`, and the per-executable
`build-dxr/_q6_*-release.log` / `build-dxr/_q6_*-debug.log` files.

## 5. T069 result

| Check | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| device tier and adapter query | EXECUTED | PASS | `RaytracingTier = 1_1`, RX 9070 XT, driver `32.0.21030.31`, SM 6.5 | `build-dxr-probe/_probe.log` |
| DXR device, BLAS/TLAS and trivial dispatch | EXECUTED | PASS | device created, BLAS 4752 B, TLAS 4804 B, state object and SBT created, `64/64` green, 0 debug errors | `build-dxr-probe/_probe.log` |
| static convergence capture (PT-S1) | NOT RUN | NOT RUN | No Q6 device renderer or Q6 frame. The CPU static test passes 68/68, but it is an analytic fixture, not a capture. Dielectric and metal cells are authored | `build-dxr/_q6_pathtrace_static_test-release.log`, §4.2 |
| dynamic/skinned capture (PT-S3) | NOT RUN | NOT RUN | No rigged mesh/animation exists (`skinnedPath: blocked`) and no Q6 frame; the CPU refit test passes 38/38 | §4.2, `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| material limitations capture (PT-S2, PT-S4) | NOT RUN | NOT RUN | No Q6 frame; emissive HDR above 1.0 is not authored and the glass cell has no transmission shader. CPU material fixtures pass 45/45 | §4.2, `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| large-environment capture (PT-S5) | NOT RUN | NOT RUN | No Q6 frame; the cell is authored and residency was measured on the hybrid path, not path-traced | `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| frame pacing (reference and showcase) | NOT RUN | NOT RUN | No Q6 frame. The probe's `0.005440 ms` is one trivial dispatch, not a frame or p95 | §4.1 |
| input latency | NOT RUN | NOT RUN | No playable Q6 frame and no input path | §4.1 |
| hardware ray tracing on this host | EXECUTED | PASS | Tier and a real dispatch are reported; this is a capability result, not a Q6 visual result | `build-dxr-probe/_probe.log` |
| CPU harness, Release | EXECUTED | **FAIL** | 4/5 tests, 44/45 denoise checks; the depth-guide far-side check fails | §4.2 |
| CPU harness, Debug | EXECUTED | PASS | 5/5 tests, 233 checks, 0 failures | §4.2 |
| fallback behaviour | EXECUTED (CPU) | PASS | Unsupported device and failed showcase gate return `Unsupported` without a silent switch; explicit fallback approval selects `Hybrid` and keeps the limitation visible | 37 checks, `q6_raytracing_capability_test` |
| convergence of the CPU estimator | EXECUTED (CPU) | PASS | 4096-sample mean is within three standard errors of `albedo * 0.5`; observed variance matches `albedo^2 * 0.25`; the 64-sample estimate is not better | 68 checks, `q6_pathtrace_static_test` |
| content matrix | EXECUTED | PASS (report) | 7/7 authored, 0/7 captured in this worktree, 7/7 budget rows | `build-dxr-probe/_matrix.log` |
| T070 independent review | NOT RUN | NOT RUN | No independent reviewer and no captures to review; the implementer cannot author the verdict | — |

T069 stays **PARTIAL and unticked**. A device capability and dispatch result exists, but every
required visual/capture/latency row is NOT RUN, and the Release CPU harness has one failing check.
Ticking it would report a completion that the evidence does not support.

## 6. What is not claimed

- Q6 is **not complete**. T069 is PARTIAL, T070 is NOT RUN, and T064's behaviour is tested without a
  Q6 engine frame.
- The probe's tier 1.1 and trivial dispatch do **not** prove a Q6 scene renders, converges or is
  playable. No Q6 capture, converged image, frame time, VRAM figure or input-latency sample exists.
- The CPU reference slice does not prove the engine/fork wiring. That needs its own fork PR, submodule
  bump and independent review.
- No DLSS/NR or other proprietary provider is present, linked, downloaded or exercised. Q5 remains
  gated.
- No Q4 capture is reused; the Q4 art review remains BLOCKED and is not a Q6 input.
- The Release denoiser failure is recorded, not repaired. A repair needs its own change and review.
