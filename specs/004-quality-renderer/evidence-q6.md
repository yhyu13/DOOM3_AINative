# Q6 path-traced reference/showcase evidence (T063–T070)

Status: author evidence for issue [#68](https://github.com/yhyu13/DOOM3_AINative/issues/68). The Q6
device-probe and CPU-harness record was authored on `spec-004-q6-dxr-t069` from `origin/main` at
`b244458`; the denoiser repair in §4.3 was authored on `spec-004-q6-denoise` from `origin/main` at
`1fafb81`, which is also the commit that recorded the defect. This is **not** Q6 acceptance, not an
engine frame and not an independent review. It records what the CPU reference slice and a standalone
D3D12 probe do on this host, and what could not run.

**Headline: the Q6 CPU slice is implemented (T063, T065–T068, and T064's selection half). A new
standalone probe proves this host's adapter reports D3D12 ray-tracing tier 1.1, creates a DXR device,
builds a BLAS and TLAS and executes a trivial `DispatchRays` (64/64 pixels, 0 debug errors). There is
still no Q6 scene renderer, capture, image, frame time, VRAM or input-latency measurement. Every
visual row is NOT RUN. The CPU harness passes 5/5 in both Debug and Release, 249 checks each. The
Release denoiser failure recorded at `1fafb81` is reproduced, diagnosed as an MSVC 19.38 codegen
defect that broke the *entire* depth-guided path (not one pixel), and repaired, with the correct
expected value, its independent float64 reference and a sensitivity control in §4.3. T069 is
PARTIAL; T070 is NOT RUN. The non-path-traced renderer remains available.**

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
| Adapter and driver | The probe enumerates **four** adapters (`ADAPTER_COUNT=4`) and selects `SELECTED_ADAPTER_INDEX=0`: **AMD Radeon RX 9070 XT**, LUID `0x000000000000E83C`, vendor `0x1002`, device `0x7550`, 16,304 MB dedicated, `SOFTWARE=0`, UMD `32.0.21030.31`. The other three are `ADAPTER[1]` integrated `AMD Radeon(TM) Graphics` (485 MB, `SOFTWARE=0`), `ADAPTER[2]` a second `AMD Radeon RX 9070 XT` (LUID `0x0000000000088C66`), and `ADAPTER[3]` **`Microsoft Basic Render Driver`, `SOFTWARE=1`**, 0 MB dedicated. A probe result is attributable only through `SELECTED_ADAPTER_INDEX` / `SELECTED_ADAPTER_NAME` / `SELECTED_ADAPTER_LUID`: `ADAPTER[0]` is a listing position, and one of the four entries is a software rasteriser that cannot report a hardware ray-tracing tier. |
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
ADAPTER[1]=AMD Radeon(TM) Graphics
ADAPTER[1].LUID=0x0000000000012F19
ADAPTER[1].VENDOR_ID=0x1002 DEVICE_ID=0x164E
ADAPTER[1].DEDICATED_VRAM_MB=485 SOFTWARE=0
ADAPTER[2]=AMD Radeon RX 9070 XT
ADAPTER[2].LUID=0x0000000000088C66
ADAPTER[2].VENDOR_ID=0x1002 DEVICE_ID=0x7550
ADAPTER[2].DEDICATED_VRAM_MB=16304 SOFTWARE=0
ADAPTER[3]=Microsoft Basic Render Driver
ADAPTER[3].LUID=0x0000000000012EC5
ADAPTER[3].VENDOR_ID=0x1414 DEVICE_ID=0x008C
ADAPTER[3].DEDICATED_VRAM_MB=0 SOFTWARE=1
ADAPTER_COUNT=4
SELECTED_ADAPTER_INDEX=0
SELECTED_ADAPTER_NAME=AMD Radeon RX 9070 XT
SELECTED_ADAPTER_LUID=0x000000000000E83C
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
DISPATCH_GPU_MS=0.009080
DISPATCH_HIT_PIXELS=64/64
DEBUG_ERRORS=0
RESULT=PASS
EXIT_CODE=0
```

`ADAPTER_COUNT=4` is an enumeration, not a candidate list: `ADAPTER[3]` is `Microsoft Basic Render
Driver` with `SOFTWARE=1`, a software rasteriser that cannot report a hardware ray-tracing tier. Every
result here is attributed by `SELECTED_ADAPTER_INDEX=0`, `SELECTED_ADAPTER_NAME=AMD Radeon RX 9070 XT`
and `SELECTED_ADAPTER_LUID=0x000000000000E83C`, which is the adapter the device, BLAS, TLAS, state
object and dispatch below were created on. The probe prefers a named discrete adapter and otherwise the
non-software adapter with the most dedicated VRAM; it reports `SOFTWARE=1` on the entry itself instead
of hiding it, and it never silently falls back to a software adapter while a hardware one is present.

`DISPATCH_GPU_MS` is a GPU timestamp around one 8x8 dispatch at one triangle, taken once per run: it
read `0.005440` at `b244458` and `0.009080` on this run, so the two samples disagree by 67% and neither
is a p50, a p95 or a comparison with the hybrid renderer.

### 4.2 Q6 CPU harness

```text
cmake -S tests/dxr -B build-dxr -G "Visual Studio 17 2022" -A x64                     -> 0
cmake --build build-dxr --config Release -- -m                                       -> 0
ctest --test-dir build-dxr -C Release --no-tests=error --output-on-failure           -> 0 (5/5 passed)
cmake --build build-dxr --config Debug -- -m                                         -> 0
ctest --test-dir build-dxr -C Debug --no-tests=error --output-on-failure             -> 0 (5/5 passed)
```

Per executable (Release / Debug), with the exact check counts and the process exit code:

| Executable | Release | Debug |
|---|---|---|
| `q6_raytracing_capability_test` | 37 checks / 0 failures, exit 0 | 37 / 0, exit 0 |
| `q6_pathtrace_static_test` | 68 / 0, exit 0 | 68 / 0, exit 0 |
| `q6_pathtrace_dynamic_test` | 38 / 0, exit 0 | 38 / 0, exit 0 |
| `q6_pathtrace_material_test` | 45 / 0, exit 0 | 45 / 0, exit 0 |
| `q6_pathtrace_denoise_test` | 61 / 0, exit 0 | 61 / 0, exit 0 |

249 checks pass in each configuration. The denoise executable carried 45 checks before §4.3 added the
flat depth-guided case, the per-channel far-side assertions and the control; it now carries 61.

Before the §4.3 repair, the same commands on the same host produced `ctest -C Release` -> **8**
(4/5 passed) and `q6_pathtrace_denoise_test` **45 / 1, exit 1**, with the single failure at
`tests/dxr/pathtrace_denoise_test.cpp:255`, `the depth guide keeps the far side from bleeding`. Debug
was 5/5, exit 0, in the same run. That failure is reproduced, diagnosed and repaired in §4.3 rather
than deleted or tolerated.

### 4.3 Denoiser codegen defect: root cause, repair and control

**Reproduction (before the repair).**

```text
.\build-dxr\Release\q6_pathtrace_denoise_test.exe   -> 1
FAIL the depth guide keeps the far side from bleeding
SUMMARY q6_pathtrace_denoise_test checks=45 failures=1
```

The fixture is an 8x1 row with radius 2, `sigmaColor = 10`, `sigmaDepth = 0.05`, the near side
(x=0..3) at depth 1.0 carrying noise around 0.3, and the far side (x=4..7) at depth 5.0 carrying 0.7
in all three channels.

**Blast radius: the whole depth-guided path was wrong, not one pixel.** Red and green were exactly
zero at *every* pixel of every depth-guided result, and blue was exactly right. A dump harness over
the same source and input (`/MD`, C++17) shows the four cases that matter:

| Case | Correct (float64 reference) | `/O2 /fp:precise` at `1fafb81` |
|---|---|---|
| 8x1 depth edge, near side x=0..3 | `(0.2764, 0.2764, 0.2764)` ... `(0.2680, 0.2680, 0.2680)` | `(0.0000, 0.0000, 0.2764)` ... `(0.0000, 0.0000, 0.2680)` |
| 8x1 depth edge, far side x=4..7 | `(0.7000, 0.7000, 0.7000)` | **`(0.0000, 0.0000, 0.7000)`** |
| flat 0.7, constant depth 3.0 | `(0.7000, 0.7000, 0.7000)` | **`(0.0000, 0.0000, 0.7000)`** |
| flat 0.7, **no** depth guide | `(0.7000, 0.7000, 0.7000)` | `(0.7000, 0.7000, 0.7000)` |

Two consequences, both recorded rather than smoothed over:

- The defect is confined to the `depth != nullptr` inner loop. The depth-free loop is correct in every
  configuration, including the failing one. "44/45 checks pass" therefore said nothing about the
  depth-guided path: it measured the depth-free path plus a depth-guided assertion that could not
  discriminate.
- The pre-repair near-side check (`pathtrace_denoise_test.cpp:251`, `luma < 0.35`) was satisfied by
  both the correct luma `0.267982` and the broken `0.019348`, so it passed while the value it claimed
  to protect was wrong by a factor of 13.8. A check that accepts both the right and the wrong value is
  not a check; this section replaces it with the reference value and its tolerance, and adds a
  flat-field depth-guided case that has no edge to blame.

Narrowing the toolchain with the dump harness:

| Compiler configuration | Far-side pixel (R, G, B) |
|---|---|
| `/Od` | `(0.7000, 0.7000, 0.7000)` |
| `/O1` | `(0.7000, 0.7000, 0.7000)` |
| `/O2 /fp:strict` | `(0.7000, 0.7000, 0.7000)` |
| `/O2 /fp:precise` (Release default) | **`(0.0000, 0.0000, 0.7000)`** |
| `/O2 /fp:precise /Qvec-` | `(0.0000, 0.0000, 0.7000)` |
| `/O2 /fp:precise /Ob0` | `(0.0000, 0.0000, 0.7000)` |
| `/O2 /fp:precise /Os` | `(0.0000, 0.0000, 0.7000)` |
| `/O2 /fp:precise /GL` | `(0.0000, 0.0000, 0.7000)` |
| `/Ox` (14.38) | `(0.0000, 0.0000, 0.7000)` |
| `/O2 /fp:precise`, **toolset 14.44.35207** | `(0.7000, 0.7000, 0.7000)` |

**Root cause: the delivered binary did not compute the source, and the source is not at fault.** The
`/O2` optimizer emitted, in the depth-guided branch of the inner loop, a body that accumulates `sumB`
and drops the other two accumulators entirely. From `cl /O2 /fp:precise /FAs` on
`src/renderer/d3d12/pathtrace_denoise.cpp` at `1fafb81`, the whole per-neighbour accumulation is:

```asm
; const double weight = static_cast<double>(spatial * colorWeight * depthWeight);
	movss	xmm0, DWORD PTR [r15+rdi*4+8]   ; rgb[neighbour + 2] only
	cvtps2pd xmm0, xmm0
	cvtss2sd xmm1, xmm6                      ; the weight
	mulsd	xmm0, xmm1
	addsd	xmm7, xmm1                       ; weightSum
	addsd	xmm8, xmm0                       ; sumB
```

There is no load of `rgb[neighbour + 0]` or `rgb[neighbour + 1]` and no `addsd` into the red or green
accumulator, so the final `sumR * inverse` and `sumG * inverse` are zero for every pixel. The same
source compiles correctly with toolset 14.44 at the same `/O2 /fp:precise`, and also at `/Od`, `/O1`
and `/O2 /fp:strict` with 14.38, so the defect is in 14.38's loop optimisation of this translation
unit rather than in the algorithm, the fixture or the compiler flags. It is specific to the branch the
loop takes when a depth guide is supplied -- the one that evaluates the additional depth weight; the
depth-free branch of the same loop keeps all three channels. The region has no undefined behaviour to
exploit: every index is bounds-checked or provably in range, the accumulators are ordinary `double`
locals reset per pixel, and the input arrays are read-only and distinct from the output
(`DenoiseBilateral` refuses aliasing). The toolchain matrix above also rules out the auto-vectorizer
(`/Qvec-`), inlining (`/Ob0`), size optimisation (`/Os`) and link-time codegen (`/GL`) as the trigger.

**The expectation was right; the denoiser was wrong.** The far-side value is not a judgement call:

- Every neighbour that keeps a non-zero weight sits on the same side of the depth edge and carries
  exactly 0.7 in all three channels.
- The opposite side's weight is bounded by `exp(-|5.0 - 1.0| / 0.05) = 1.8e-35`, so it cannot move
  the result.
- A normalised weighted mean of equal values is that value, and a grey input stays grey because the
  weight of a neighbour is independent of the channel it is applied to.

Independent float64 reference for the exact fixture (numPy and `math.exp` in double over the exact
float32 inputs; it never calls the implementation). The harness is 45 lines including the float32
fixture builder; this is its essential loop, with the bounds checks and the fixture construction
elided:

```python
def reference(image, depth, use_depth, radius=2, sigma_color=10.0, sigma_depth=0.05):
    luma = lambda c: 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]   # c is float64
    for y, x in pixels:
        weight_sum, accum = 0.0, np.zeros(3)
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                n = image[y + dy, x + dx].astype(np.float64)
                w = (1.0 / (1.0 + dx * dx + dy * dy)) * math.exp(
                    -abs(luma(n) - luma(centre)) / sigma_color)
                if use_depth:
                    w *= math.exp(-abs(depth[y + dy, x + dx] - depth[y, x]) / sigma_depth)
                weight_sum += w
                accum += w * n
        out[y, x] = accum / weight_sum
```

| Fixture | Independent float64 result |
|---|---|
| depth-guided, x=0..3 | 0.276396, 0.276691, 0.276151, 0.267982 |
| depth-guided, x=4..7 | 0.700000 in every channel; max difference from 0.7 is 1.192e-08 |
| depth-free, x=3 | 0.390166 |
| depth-free, x=4 | 0.577112 |
| flat 0.5, constant depth guide | 0.500000 in every channel |

Tolerances, stated rather than tuned: **1e-6** for the far side and for the flat depth-guided field,
where the only error sources are the 1.8e-35 opposite-side weight and the 1.2e-8 by which the decimal
`0.7` differs from `0.7f`; and **2e-3** for the exp-weighted averages, where the float build lands
within 1e-7 of the float64 reference but another runtime's `expf` may differ. The repair makes the
assertion stricter than the one it replaces (`> 0.65` became `|out - 0.7| <= 1e-6`, checked per
channel); no tolerance was widened.

**Repair.** `DenoiseBilateral` now keeps its three channel sums in `double sumRgb[3]` and touches them
through a channel index, in both the accumulation and the store, instead of three separately named
scalars. The arithmetic is unchanged: same weights, same order of accumulation per channel, same
`double` precision. This is a codegen workaround in the source, recorded as such in the comment at the
accumulation site, and it is verified rather than assumed:

- 14.38: `/Od`, `/O1`, `/O2 /fp:precise`, `/O2 /fp:strict`, `/Qvec-`, `/Ob0`, `/GL`, `/Ox` all give
  `(0.7000, 0.7000, 0.7000)` on the depth-edge fixture *and* on a flat field with a constant depth
  guide, where the pre-repair build returned `(0.0000, 0.0000, 0.7000)` at every pixel.
- 14.44: `/O2 /fp:precise` gives `(0.7000, 0.7000, 0.7000)`.
- The fixed inner loop loads `[base+0]`, `[base+4]` and `[base+8]` and adds into three separate
  `addsd` accumulators (`xmm12`, `xmm13`, `xmm11`) plus `weightSum` (`xmm10`).

**Control.** The far-side assertion must move when its input moves. The test re-runs the same fixture
with the far-side source changed from 0.7 to 0.4 -- 0.3, or 3e5 times the stated tolerance -- and
requires the filtered far side to follow to 0.4 within 1e-6 in all three channels *and* to sit more
than 1e-6 away from 0.7. A constant, stale or channel-less result cannot satisfy both that and the
unperturbed assertion.

**The control was exercised against the defect.** Rebuilding only `q6_pathtrace_denoise_test` with the
pre-repair `pathtrace_denoise.cpp` and running the new test:

```text
.\build-dxr\Release\q6_pathtrace_denoise_test.exe   -> 1
FAIL a flat image with a depth guide keeps its value (red channel): expected 0.500000, observed 0.000000
FAIL a flat image with a depth guide keeps its value (green channel): expected 0.500000, observed 0.000000
FAIL a flat image with a depth guide keeps its corner (red channel): expected 0.500000, observed 0.000000
FAIL a flat image with a depth guide keeps its corner (green channel): expected 0.500000, observed 0.000000
FAIL the depth guide keeps the near side from bleeding: expected 0.267982, observed 0.019348
FAIL the depth guide keeps the far side from bleeding (red channel): expected 0.700000, observed 0.000000
FAIL the depth guide keeps the far side from bleeding (green channel): expected 0.700000, observed 0.000000
FAIL control: the far side follows a perturbed source (red channel): expected 0.400000, observed 0.000000
FAIL control: the far side follows a perturbed source (green channel): expected 0.400000, observed 0.000000
SUMMARY q6_pathtrace_denoise_test checks=61 failures=9
```

Nine checks fail on the pre-repair object, and the repaired source restores 61 / 0 in both
configurations. The assertions that discriminate are the flat depth-guided field (no edge, no
threshold), the near-side reference value (which the old `< 0.35` bound could not separate from
`0.0193`), the far-side red and green channels separately, and the control. What this does **not**
establish: that 14.38 cannot miscompile some other loop, or that the Q6 CPU slice is a renderer. It
establishes that this defect is detected, that the expectation is the correct value, and that the
build in this worktree computes it.

### 4.4 Gates

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

1. **Release denoiser output (found at `1fafb81`, repaired in §4.3).** Release `/O2 /fp:precise` zeroed
   the R/G channels of *every* depth-guided bilateral result; Debug, `/O1` and `/O2 /fp:strict` were
   correct. The cause is MSVC 19.38 codegen, not the expectation. The repair restores both
   configurations to 5/5.
2. **Undertested near side and a non-discriminating threshold (found in review of `1fafb81`, fixed in
   §4.3).** `pathtrace_denoise_test.cpp:251` accepted both the correct near-side luma `0.267982` and
   the broken `0.019348`, so "44/45 checks pass" was not evidence for the depth-guided path. The
   threshold is replaced by the reference value with a stated tolerance, and a flat depth-guided case
   was added.
3. **Probe adapter attribution (documentation, corrected in §2 and §4.1).** `ADAPTER_COUNT=4` includes
   `ADAPTER[3]=Microsoft Basic Render Driver` with `SOFTWARE=1`. A result must be attributed through
   `SELECTED_ADAPTER_INDEX` / `_NAME` / `_LUID`, never by the listing position `ADAPTER[0]`.
4. **RHI contract gate (pre-existing).** Five `src/renderer/q4_harness` headers are reported for
   prohibited D3D12 vocabulary (26 lines). Not caused by this diff.
5. **Content-state correction (documentation).** The earlier "1 of 7 authored" reason for the visual
   rows is wrong; 7/7 cells are authored. The rows stay NOT RUN because there is no Q6 device
   renderer, no Q6 frame and no Q6 capture path.

**Logs** (git-ignored): `build-dxr-probe/_configure.log`, `build-dxr-probe/_build-Release.log`,
`build-dxr-probe/_probe.log`, `build-dxr/_configure.log`, `build-dxr/_build-Release.log`,
`build-dxr/_build-Debug.log`, `build-dxr/_ctest-Release.log`, `build-dxr/_ctest-Debug.log`,
`build-dxr/_validate-rhi.log`, `build-dxr-probe/_matrix.log`,
`build-dxr/_q6_denoise_test-unfixed-denoiser.log` (the pre-repair object under the repaired test,
§4.3), and the per-executable `build-dxr/_q6_*-release.log` / `build-dxr/_q6_*-debug.log` files.

## 5. T069 result

| Check | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| device tier and adapter query | EXECUTED | PASS | `RaytracingTier = 1_1`, SM 6.5, driver `32.0.21030.31`, attributed to `SELECTED_ADAPTER_INDEX=0` / LUID `0x000000000000E83C` out of `ADAPTER_COUNT=4`, one of which is `SOFTWARE=1` | `build-dxr-probe/_probe.log` |
| DXR device, BLAS/TLAS and trivial dispatch | EXECUTED | PASS | device created, BLAS 4752 B, TLAS 4804 B, state object and SBT created, `64/64` green, 0 debug errors | `build-dxr-probe/_probe.log` |
| static convergence capture (PT-S1) | NOT RUN | NOT RUN | No Q6 device renderer or Q6 frame. The CPU static test passes 68/68, but it is an analytic fixture, not a capture. Dielectric and metal cells are authored | `build-dxr/_q6_pathtrace_static_test-release.log`, §4.2 |
| dynamic/skinned capture (PT-S3) | NOT RUN | NOT RUN | No rigged mesh/animation exists (`skinnedPath: blocked`) and no Q6 frame; the CPU refit test passes 38/38 | §4.2, `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| material limitations capture (PT-S2, PT-S4) | NOT RUN | NOT RUN | No Q6 frame; emissive HDR above 1.0 is not authored and the glass cell has no transmission shader. CPU material fixtures pass 45/45 | §4.2, `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| large-environment capture (PT-S5) | NOT RUN | NOT RUN | No Q6 frame; the cell is authored and residency was measured on the hybrid path, not path-traced | `specs/009-rhi-hd-content/evidence/hero-cells.md` |
| frame pacing (reference and showcase) | NOT RUN | NOT RUN | No Q6 frame. The probe's `0.005440 ms` is one trivial dispatch, not a frame or p95 | §4.1 |
| input latency | NOT RUN | NOT RUN | No playable Q6 frame and no input path | §4.1 |
| hardware ray tracing on this host | EXECUTED | PASS | Tier and a real dispatch are reported on the adapter named by `SELECTED_ADAPTER_INDEX=0` (LUID `0x000000000000E83C`), not on the `SOFTWARE=1` entry; this is a capability result, not a Q6 visual result | `build-dxr-probe/_probe.log` |
| CPU harness, Release | EXECUTED | PASS | 5/5 tests, 249 checks, 0 failures | §4.2 |
| CPU harness, Debug | EXECUTED | PASS | 5/5 tests, 249 checks, 0 failures | §4.2 |
| fallback behaviour | EXECUTED (CPU) | PASS | Unsupported device and failed showcase gate return `Unsupported` without a silent switch; explicit fallback approval selects `Hybrid` and keeps the limitation visible | 37 checks, `q6_raytracing_capability_test` |
| convergence of the CPU estimator | EXECUTED (CPU) | PASS | 4096-sample mean is within three standard errors of `albedo * 0.5`; observed variance matches `albedo^2 * 0.25`; the 64-sample estimate is not better | 68 checks, `q6_pathtrace_static_test` |
| content matrix | EXECUTED | PASS (report) | 7/7 authored, 0/7 captured in this worktree, 7/7 budget rows | `build-dxr-probe/_matrix.log` |
| T070 independent review | NOT RUN | NOT RUN | No independent reviewer and no captures to review; the implementer cannot author the verdict | — |

T069 stays **PARTIAL and unticked**. A device capability and dispatch result exists, but every
required visual/capture/latency row is NOT RUN. The CPU harness is now green in both configurations,
which clears the code-health objection recorded at `1fafb81`; it does not produce a capture, an image
or a latency sample, so it does not move T069. T070 remains **NOT RUN** and is not authored here: the
implementer cannot write the independent verdict.

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
- The Release denoiser defect is repaired in §4.3 as a source-level workaround for one MSVC 19.38
  codegen bug. That is a build-health fix, not a Q6 result, and it changes neither T069 nor T070.
- The probe result is attributable to `SELECTED_ADAPTER_INDEX=0` only. It says nothing about the
  `SOFTWARE=1` entry and nothing about the second discrete adapter's capabilities.
