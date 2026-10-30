# Q6 path-traced reference/showcase evidence (T063–T070)

Status: author evidence for issue [#68](https://github.com/yhyu13/DOOM3_AINative/issues/68), written
2026-10-30. This is **not** Q6 acceptance, not an engine frame and not an independent review. It
records what the CPU reference slice does on this host and what could not run.

**Headline: T063, T065–T068 and the capability/fallback half of T064 are implemented as
CPU-testable, backend-neutral slices; their tests pass in Release and Debug (5/5 CTest, 233 checks,
0 failures). No DXR device path, capture, image, frame time, VRAM or input-latency measurement exists.
T069 is PARTIAL and every visual row is NOT RUN. T070 is NOT RUN. The non-path-traced renderer remains
available; the selection tests prove it is never replaced automatically.**

## 1. Identity and scope

| Field | Value |
|---|---|
| Stage and claim | Q6 CPU reference slice. Claim: the path integrator converges to independently computed expectations, the material transport (alpha, transmission, emission) and the acceleration-structure updates behave as specified, the denoiser reduces variance without smearing edges, and mode selection is explicit with a live fallback. **No** frame, art, parity, capture, budget, device-tier or performance claim. |
| Issue / branch | #68, `feat/q6-dxr-showcase`. Authored against `main` at `f203805`; rebased onto `c0f0bf3` (the Q7b merge) before review. The rebase touched only `tasks.md`, where the Q6 and Q7 status lines merged cleanly. |
| Submodule | `doom3_orig/doom3` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`; `git -C doom3_orig/doom3 status --porcelain` is empty. No submodule or fork change in this PR. |
| Data identity and rights | Synthetic CPU fixtures only. No `.pk4`/`.pk3`/`.pak` is present or committed. No Q6 capture reads `content/`; the only authored content in the tree (`content/hero/dielectric/`, CC0) is not read by a test. |
| Exclusions | The engine fork, ray-tracing device objects, all images and captures, DLSS/NR (Q5), Q7 profiles, and the target build (`targets/win11-amd64/CMakeLists.txt` untouched). |

Task-path deviation, recorded rather than hidden: T064–T068 name `tests/renderer/*.cpp`. The issue
constraint for #68 is that new Q6 tests live in `tests/dxr` with their own CMake project, and that
`tests/renderer/CMakeLists.txt` and the target CMake are not edited. The behaviour is therefore tested
in `tests/dxr`, and the implementation files keep the task names under `src/renderer/d3d12/`.

## 2. Environment and configuration

| Item | Value |
|---|---|
| OS / arch | Windows 10.0.22631, x64 |
| Compiler | MSVC 19.44.35228.0 (VS 2022 Professional, toolset v143), Windows SDK 10.0.26100.0 |
| Build tool | CMake 3.31.6 (VS-bundled), generator `Visual Studio 17 2022 -A x64` |
| Build root | `build-dxr/` (git-ignored) |
| Adapter and driver | **No device query and no ray-tracing call are made by this slice.** For context only, `Win32_VideoController` reports `Intel(R) Graphics`, driver `32.0.101.8331`. That is not a DXR tier or capability query and is not used as one. |
| Shader provenance | N/A — no shader is compiled or loaded |
| Linked libraries | None beyond the C++ runtime. The CMake target has no `d3d12`, `dxgi` or provider link; the D3D12-named directory holds backend-neutral arithmetic only. |

## 3. What was implemented

| Task | Files | Behaviour |
|---|---|---|
| T063 | `specs/004-quality-renderer/path-tracing-rubric.md` | Five scene identities, configuration identity, convergence/noise/latency metrics and pre-review thresholds. No threshold is measured. |
| T064 | `src/renderer/rhi/raytracing_capability.h`, `src/renderer/d3d12/raytracing_capability.cpp` | Explicit `Hybrid` / `Reference` / `Showcase` selection; device support and shader-model 6.3 requirement; showcase needs the visual, performance and motion gates; a failed requirement is `Unsupported` unless the caller explicitly approves the hybrid fallback. |
| T065 | `src/renderer/rhi/pathtrace.h`, `src/renderer/d3d12/pathtrace.cpp` | Camera rays, deterministic per-pixel/sample seeds, iterative path integrator, next-event estimation against point lights, two-hemisphere environment, alpha-aware shadow rays. |
| T066 | `src/renderer/rhi/raytracing_scene.h`, `src/renderer/d3d12/raytracing_scene.cpp` | Triangle hierarchy with median split, bottom-up refit for posed/skinned vertices, one affine instance placement (rigid motion without rebuild), topology-change refusal, nearest-hit query with barycentrics and uv. |
| T067 | `src/renderer/rhi/pathtrace_material.h`, `src/renderer/d3d12/pathtrace_material.cpp` | Lambertian cosine sampling, bounded two-lobe conductor, Schlick/Snell dielectric with transmission tint, emission, deterministic checker alpha mask. |
| T068 | `src/renderer/rhi/pathtrace_denoise.h`, `src/renderer/d3d12/pathtrace_denoise.cpp` | Reference accumulation, Q3-`MotionValidity` history rejection, edge-aware bilateral colour/depth denoise. |
| T069 | this file | Static/dynamic/material/frame-pacing/input rows recorded; every visual row NOT RUN. |
| T070 | — | **NOT RUN**: needs an independent reviewer; the implementer cannot author the verdict. |

Design boundaries worth stating:

- The tests exercise production sources, not test-owned copies: `tests/dxr/CMakeLists.txt` builds
  `q6_dxr` from the five `src/renderer/d3d12` slices plus the existing `motion_history.cpp`.
- The material model is bounded and documented; it is not a physically exact BRDF claim. Q6 is where a
  higher-ceiling reference is judged, and the CPU slice does not substitute for that judgement.
- Alpha testing uses a deterministic checker mask so "passes through a hole" is checkable without an
  image. It is not a claim about engine texture sampling.

## 4. Procedure and retained artifacts

All commands ran from the worktree root. Full logs are retained in the git-ignored `build-dxr/`.

```text
cmake -S tests/dxr -B build-dxr -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-dxr --config Release -- -m                     -> 0 (no error/warning lines)
cmake --build build-dxr --config Debug   -- -m                     -> 0 (no error/warning lines)
ctest --test-dir build-dxr -C Release --no-tests=error --output-on-failure -> 0, 5/5
ctest --test-dir build-dxr -C Debug   --no-tests=error --output-on-failure -> 0, 5/5
q6_raytracing_capability_test  -> 0, 37 checks / 0 failures (Release and Debug)
q6_pathtrace_static_test       -> 0, 68 checks / 0 failures (Release and Debug)
q6_pathtrace_dynamic_test      -> 0, 38 checks / 0 failures (Release and Debug)
q6_pathtrace_material_test     -> 0, 45 checks / 0 failures (Release and Debug)
q6_pathtrace_denoise_test      -> 0, 45 checks / 0 failures (Release and Debug)
py -3 -I scripts/validate-rhi-contract.py -> 0
```

`validate-rhi-contract.py` reports `header scan: RUN over 24 header(s) and 29 public test file(s)`.
The five new engine-facing headers pass the lexical gate; the first run failed because
`raytracing_scene.h` named the native API in a comment, and the comment was reworded.

**Defects found during verification.**

1. **Denoise colour weight (fixed).** The bilateral filter used
   `exp((neighbourLuma - centreLuma) * -1/sigma)`, which amplified a darker neighbour instead of
   penalising it; the bright side of a colour edge was pulled from 2.0 to 0.000036. The colour-edge
   check caught it. The weight is now `exp(-|Δluma| / sigma)`.
2. **Test depth-buffer overrun (fixed).** The empty-scene check passed one `float` as the depth array
   for a 2x2 camera. Debug `/RTC1` reported `_RTC_StackFailure` from `_RTC_CheckStackVars` in `main`
   and the process blocked in a runtime-check message box; Release silently tolerated the overrun. The
   buffer is now four floats. Debug is run for this reason, not as a formality.

**Logs** (git-ignored): `build-dxr/_build-Release.log`, `build-dxr/_build-Debug.log`,
`build-dxr/_static_debug.log`, `build-dxr/_static_release.log`.

## 5. T069 result

| Check | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| static convergence capture (PT-S1) | NOT RUN | NOT RUN | No Q6 engine frame and only 1 of 7 Q4 content cells is authored; a capture cannot be produced | `path-tracing-rubric.md` §2, §7 |
| dynamic/skinned capture (PT-S3) | NOT RUN | NOT RUN | The skinned character cell is not authored and no engine call site exists | `path-tracing-rubric.md` §2 |
| material limitations capture (PT-S2, PT-S4) | NOT RUN | NOT RUN | Alpha/emissive and transmission cells are not authored; only CPU fixtures ran | §3, `cpu-alpha-*`, `cpu-transmission` |
| frame pacing (reference and showcase) | NOT RUN | NOT RUN | No device path is implemented in this slice; a CPU reference has no frame pacing | §2 |
| input latency | NOT RUN | NOT RUN | No playable Q6 frame exists; a CPU reference has no input path | §2 |
| hardware ray tracing on this host | SKIPPED | SKIPPED | No DXR capability query and no device call are made; the adapter fact in §2 is not a tier claim. The non-path-traced renderer remains available | §2, `q6_raytracing_capability_test` |
| fallback behaviour | EXECUTED (CPU) | PASS | Unsupported device and failed showcase gate return `Unsupported` without a silent switch; explicit fallback approval selects `Hybrid` and keeps the limitation visible | 37 checks, `q6_raytracing_capability_test` |
| convergence of the CPU estimator | EXECUTED (CPU) | PASS | 4096-sample mean is within three standard errors of `albedo * 0.5`; observed variance matches `albedo^2 * 0.25`; the 64-sample estimate is not better | `q6_pathtrace_static_test` |

No number in this file is a device, image or latency result. The only measured quantities are CPU
test counts and the analytic CPU expectations written in the tests.

## 6. What is not claimed

- Q6 is **not complete**. T069 is PARTIAL, T070 is NOT RUN, and T064's behaviour is tested without a
  device.
- No DXR tier, ray-tracing device execution, capture, converged image, frame time, VRAM or input
  latency is claimed.
- No DLSS/NR or other proprietary provider is present, linked, downloaded or exercised. Q5 remains
  gated.
- The CPU slice does not prove the engine/fork wiring. That needs its own fork PR, submodule bump and
  independent review.
- No Q4 capture is reused; the Q4 art review remains BLOCKED and is not a Q6 input.
