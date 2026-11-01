# Q6 DXR capability and dispatch probe

Evidence tooling for spec 004 T069 (issue #68). This is **not** a renderer, not a Q6 engine
frame and not Q6 acceptance. It answers one question with a command and an exit code: does this
host's D3D12 adapter really expose a ray-tracing tier, create acceleration structures and execute
a dispatch?

The Q6 CPU harness (`tests/dxr`) deliberately links no device, window or shader compiler. This
project is separate so that stays true.

## What it reports

- every DXGI adapter (name, LUID, vendor/device id, dedicated VRAM, software flag);
- the selected adapter, its LUID and UMD driver version;
- `D3D12_FEATURE_D3D12_OPTIONS5.RaytracingTier` (queried, never inferred from the product name);
- the highest `D3D12_FEATURE_SHADER_MODEL`;
- whether `D3D12CreateDevice` and `ID3D12Device5` succeed;
- whether a BLAS and a TLAS can be built;
- whether a raytracing state object and shader binding table can be created;
- the result of an 8x8 `DispatchRays` at one triangle, read back from a UAV (`64/64` expected);
- a GPU timestamp for that dispatch only (a diagnostic, **not** a Q6 frame-pacing result);
- debug-layer error/warning counts (the error count is a pass condition).

## Build

Requires Visual Studio 2022 (Desktop C++), the Windows SDK with `dxc.exe`, and CMake 3.21+.
`CMakeLists.txt` finds `dxc.exe` in the Windows SDK; pass `-DDXC_EXE=<path>` to override.

```powershell
cmake -S tests/dxr-probe -B build-dxr-probe -G "Visual Studio 17 2022" -A x64
cmake --build build-dxr-probe --config Release -- -m
```

## Run

```powershell
.\build-dxr-probe\Release\q6_dxr_probe.exe
```

Exit codes:

| Code | `RESULT` | Meaning |
|---|---|---|
| 0 | `PASS` | device, tier, acceleration structures and dispatch all executed; `DISPATCH_HIT_PIXELS=64/64` **and** `DEBUG_ERRORS=0` |
| 1 | `ERROR` / `FAIL` | the probe failed: an unexpected HRESULT or map failure (`ERROR`), or a readback that is not 64/64 or `DEBUG_ERRORS>0` (`FAIL`) |
| 2 | `UNSUPPORTED` | device or ray-tracing tier unsupported; no dispatch attempted |
| 3 | `NOT RUN` | the debug layer, and therefore `DEBUG_ERRORS`, is unavailable; the dispatch may have executed, but the documented pass condition cannot be evaluated |

`DISPATCH_HIT_PIXELS=64/64` and `DEBUG_ERRORS=0` are the pass conditions, and the exit code is
derived from both. The shader writes green on a hit and red on a miss, so `DISPATCH_HIT_PIXELS`
counts hits. A host without the D3D12 debug layer ends as `NOT RUN` (exit 3), never as a pass
with the condition unevaluated; `DEBUG_LAYER_MESSAGES=unavailable` marks that case.

## Self-test hooks (negative controls)

The pass condition is only useful if its failing branch is exercised. Two environment variables
force that without a faulting GPU. Both print `SELFTEST_MODE=<name>` in the output, and a run that
uses one is a control, not capability evidence:

| Variable | Effect | Expected result |
|---|---|---|
| `Q6_DXR_PROBE_SELFTEST=debug-error` | adds one synthetic debug-layer error after the real message count and prints `SELFTEST_SYNTHETIC_DEBUG_ERRORS=1` | `RESULT=FAIL`, exit 1 |
| `Q6_DXR_PROBE_SELFTEST=no-debug-layer` | does not enable the debug layer, so `DEBUG_ERRORS` is never printed | `RESULT=NOT RUN`, exit 3 |

An unrecognised value is itself an error (`SELFTEST_MODE=invalid`, exit 1).

## What it does not prove

- No Q6 scene, content, material, denoiser or engine call site is loaded.
- `DISPATCH_GPU_MS` is a timestamp around one trivial dispatch, not a frame time, not a p50/p95,
  and not comparable with the hybrid renderer.
- A tier of 1.1 is a device fact, not a Q6 visual or playability verdict.
