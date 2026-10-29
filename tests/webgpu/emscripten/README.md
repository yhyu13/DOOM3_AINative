# Q7a Emscripten encoder leg

The C++ half of the Q7a conformance gate. It compiles the portable header
(`src/renderer/rhi/rhi.h`) for `wasm32` and executes the same twelve checks as
`tests/webgpu/conformance.html` from C++ through Dawn's `webgpu.h`, instead of from hand-written
JavaScript. The result is the second half of spec 004 task T088: the browser page proves the WebGPU
operations behave as mapped, and this build proves a C++ encoder can drive them.

## Toolchain (recorded for the accepted run, 2026-10-29)

| Item | Value |
|---|---|
| Emscripten | 6.0.13 (`e13ee988ce3d87a738dd8f3661d02223308c412a`) |
| emsdk | installed outside the repository and discovered via `EMSDK` or `%USERPROFILE%\\emsdk`; the path is per-host and deliberately not recorded here |
| WebGPU bindings | the `emdawnwebgpu` port, which resolves to Dawn release `v20260423.175430` and is cached under `<emsdk>/upstream/emscripten/cache/ports/emdawnwebgpu` |
| Licences | Emscripten: MIT / University of Illinois-NCSA. Dawn and the emdawnwebgpu package: BSD-3-Clause. Neither is vendored in this repository |
| Acquisition | `git clone https://github.com/emscripten-core/emsdk`, `emsdk.py install 6.0.13`, and the port is fetched by Emscripten on first build from the Dawn release page |
| Authorization | installed on the owner's instruction ("只补 Emscripten leg") on 2026-10-29; nothing is downloaded at test time |

The recorded host needed the bundled Python 3 ahead of a system Python 2 on `PATH`;
`build.ps1` does that automatically. `em++.exe` must be used rather than `emcc.exe`, because the port
requires C++ linkage.

## Build and run

```powershell
# from the repository root, on the issue's own worktree
.\tests\webgpu\emscripten\build.ps1
node tests\webgpu\run-conformance.mjs --page emscripten/index.html --repeat 2 --out build\webgpu-emscripten
```

The runner serves `tests/webgpu` on `127.0.0.1` (WebGPU needs a secure context), launches the
discovered Chrome/Edge headlessly over the DevTools protocol, waits for `window.__conformanceResult`
and fails unless every required check passes on every repetition. The Emscripten build publishes the
same result shape as the JavaScript page, so no second harness is needed; `--page` and `--root` are the
only additions.

## What the checks prove

- **C1** adapter and device creation, adapter identity, and the T089 availability outcome in the real
  browser: `vendorId`/`deviceID` are `0`, so `reportedFacts` is `0x0` — the browser reports "not
  available" instead of inventing ids, and the check asserts that any claimed fact has a non-zero value.
- **C2** buffer write plus `mapAsync` readback, byte-exact.
- **C3/C5** one bind group reused across two compute dispatches, and a 64-element dispatch.
- **C4** one render pass with two colour attachments (MRT).
- **C6** `copyTextureToBuffer` with a 256-byte `bytesPerRow` for a 16-byte row, plus `mapAsync`;
  padding stays untouched and the packed offset is rejected.
- **C7** canvas surface configure, acquire, clear, commit, readback, and a visible present.
- **C8** `device.destroy()` reports loss reason `destroyed`.
- **N1–N3, E** the comparator, pitch and liveness controls, and zero uncaptured errors.

## Limits

- The checks call the WebGPU C API directly. A WebGPU *backend implementation* of the RHI does not
  exist; the header is compiled and its `AdapterFact`/`reportedFacts` rule is exercised, but no
  `doom3::rhi::Device` is implemented on WebGPU.
- The software/fallback adapter is NOT RUN: the browser exposes none (`A1`).
- Performance, resize, multi-adapter and OOM classification are out of scope.
- The recorded run used Chrome 156.0.8078.73 on an Intel Xe-LPG adapter; a different browser or GPU
  needs its own run, and the page prints the values it observed.
