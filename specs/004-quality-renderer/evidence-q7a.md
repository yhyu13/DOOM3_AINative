# Q7a evidence: WebGPU mapping gate, browser conformance spike and Emscripten encoder leg

**Stage and claim**: spec 004 task T088 (Q7a), issue #35. Both required legs are executed:

1. **Mapping gate** — every public declaration of the portable header is mapped to a WebGPU-shaped
   operation or recorded as unmappable with required backend behaviour, and the lexical header gate
   scans the core header and public test units with no prohibited noun.
2. **Conformance spike in a browser** — twelve required checks pass in a real browser WebGPU
   implementation (`tests/webgpu/conformance.html`, JavaScript).
3. **Emscripten encoder leg** — the same twelve checks pass from C++ compiled to WebAssembly through
   Dawn's `webgpu.h` (the `emdawnwebgpu` port), with the portable header compiled for `wasm32`.

This record does **not** claim a WebGPU backend implementation of `doom3::rhi::Device`, browser parity,
or any Q7 support. The checks call the WebGPU C API directly; no core device is implemented on WebGPU.
Tasks T089 (header reconciliation) and Q1b are unaffected.

## Identity and scope

- **Source identity**: the mapping gate and browser spike landed in `29a1e37`; the Emscripten leg is
  the commit that carries this record (branch `test/35-emscripten-leg`). `doom3_orig/doom3` untouched
  and clean at `a9c49da5`.
- **Data identity and rights**: synthetic only. All WGSL and expected bytes are authored here; no game
  data, retail shader, `glprogs`, derived asset or third-party SDK source is copied.
- **Changed surface**: `specs/004-quality-renderer/{webgpu-mapping.md,evidence-q7a.md,evidence/**}`,
  `scripts/validate-webgpu-mapping.py` and its suite, `tests/webgpu/**` including the new
  `tests/webgpu/emscripten/` harness, `tasks.md`, `traceability.md`, `plan.md`, `quickstart.md`.
- **Exclusions**: no engine or backend change; no renderer performance claim; no software WebGPU
  adapter on either leg.

## Environment and configuration

| Item | Value |
|---|---|
| Host | Windows 11 x64, Node v24.19.0, Python 3.14.4 (`py -3`), Visual Studio 2022 C++ tools |
| Browser (both legs) | Google Chrome `156.0.8078.73`, DevTools protocol 1.3, headless |
| WebGPU adapter | `intel` / `xe-lpg`, `isFallbackAdapter=false`; no software adapter exposed |
| Emscripten | 6.0.13 (`e13ee988ce3d87a738dd8f3661d02223308c412a`), emsdk git checkout `d7b4130` |
| WebGPU bindings | `emdawnwebgpu` port → Dawn release `v20260423.175430`, cached under the emsdk tree |
| Licences | Emscripten MIT / University of Illinois-NCSA; Dawn and emdawnwebgpu BSD-3-Clause; Chrome proprietary binary, Chromium components BSD-3-Clause; none redistributed or vendored here |
| Acquisition | Chrome was pre-installed; emsdk was cloned from `github.com/emscripten-core/emsdk`, installed with `emsdk.py install 6.0.13`, and the port was fetched by Emscripten on first build from the Dawn release page — all on the owner's instruction to add the Emscripten leg (2026-10-29) |
| Storage | emsdk outside the repository (discovered via `EMSDK`); its path is per-host and not recorded; build output stays in the ignored `tests/webgpu/emscripten/index.*` and in `build/` |

## Leg 1 — mapping gate (executed)

| Command | Exit | Observation |
|---|---|---|
| `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `57/57 public declarations classified` |
| `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | 14 negative controls detected |
| `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | 14 document mutations and 14 validator mutants behaved as required |
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | header scan RUN over 3 headers and 3 public test files, zero hits |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | every injected violation detected |

## Leg 2 — browser spike (executed 2026-10-29, `29a1e37`)

`node tests/webgpu/run-conformance.mjs --repeat 2 --out build/webgpu-q7a --evidence specs/004-quality-renderer/evidence`
→ exit 0, twelve required checks PASS twice (567 ms, 585 ms), repeatable, plus the software-adapter
attempt NOT RUN with its retained reason. Artifacts: `evidence/q7a-conformance.json`, `.png`.
Controls: a mutated expectation fails C3 and C5 and exits 1 (it exposed a real C5 false-pass bug, now
fixed); removing the per-run nonce echo exits 2.

## Leg 3 — Emscripten encoder leg (executed 2026-10-29)

Build and run:

| Command | Exit | Observation |
|---|---|---|
| `.\tests\webgpu\emscripten\build.ps1` (with `EMSDK` set) | 0 | `emcc 6.0.13`; port `emdawnwebgpu`; `index.wasm` 146745 B, `index.js` 63530 B (sizes move with the harness source) |
| `node tests/webgpu/run-conformance.mjs --page emscripten/index.html --repeat 2 --out build/webgpu-emscripten` | 0 | twelve required checks PASS twice, repeatable; software adapter NOT RUN |

Artifacts: `evidence/q7a-emscripten.json`, `evidence/q7a-emscripten.png`. The screenshot shows the
rendered result table and the orange canvas from the clear pass, so the surface was presented from the
WebAssembly build.

| Check | Result | Observed |
|---|---|---|
| C1 device | PASS | adapter `intel`/`xe-lpg`; `vendorId=0`, `deviceID=0` → `reportedFacts=0x0`; the harness also asserts that a claimed fact has a non-zero value |
| C2 buffer | PASS | 16 bytes equal |
| C3 bind group | PASS | same bind group drove two dispatches, both `[1,4,7,10]` |
| C4 MRT | PASS | A `64,128,191,255`, B `255,0,128,64` |
| C5 compute | PASS | `[1,4,7,10,13,16,19,22]` |
| C6 pitch | PASS | rows exact, padding untouched, packed offset rejected |
| C7 surface | PASS | `bgra8unorm`, pixel `0,128,255,255` |
| C8 device loss | PASS | loss reason 2 (`destroyed`) |
| N1 – N3, E | PASS | comparator, pitch and liveness controls; zero uncaptured errors |
| A1 software adapter | NOT RUN | `requestAdapter({forceFallbackAdapter:true})` returned no adapter (status 3) |

Controls and findings:

- **Mutation**: changing the expected MRT texel from `64` to `65` makes C4 fail with `byte 0: 64 != 65`
  and the runner exit 1; restoring returns 12/12.
- **The T089 rule's browser outcome is recorded, not proven in the negative**: the adapter reports
  `vendorId=0`/`deviceID=0`, so the harness sets `reportedFacts = 0x0` and the fields stay 0 — the
  behaviour the contract requires of a WebGPU-shaped backend. The harness asserts that a set fact has a
  non-zero value, but no test can prove that a hypothetical browser reporting ids would be handled
  wrongly; the rule's positive path is covered by the Q1a D3D12 assertion (`0x7`).
- **API finding**: in this `webgpu.h`, a `MAP_READ` mapping is const-only; `wgpuBufferGetMappedRange`
  returns null for it and `wgpuBufferGetConstMappedRange` must be used. A future WebGPU backend must
  not assume the write-map accessor works for readback.
- The recorded build needed emsdk's bundled Python 3 ahead of a system Python 2 on `PATH`, and `em++`
  rather than `emcc` because the port requires C++ linkage; `build.ps1` handles both.

## Per-check summary

| Check / target | Execution | Outcome | Artifact |
|---|---|---|---|
| Field-to-WebGPU classification | RUN | PASS | `webgpu-mapping.md` |
| Lexical header gate | RUN | PASS | `validate-rhi-contract.py` |
| C1–C8, N1–N3, E in JavaScript | RUN | PASS (12 required, twice) | `evidence/q7a-conformance.{json,png}` |
| C1–C8, N1–N3, E from C++/wasm | RUN | PASS (12 required, twice) | `evidence/q7a-emscripten.{json,png}` |
| `wasm32` compile of `rhi.h` | RUN | PASS | `static_assert(sizeof(void*) == 4)` in the harness; build succeeds |
| Software WebGPU adapter (both legs) | NOT RUN | — | no fallback adapter exposed; not a pass |
| Out-of-memory classification | NOT RUN | — | mapping only; no OOM induced |
| WebGPU implementation of `doom3::rhi::Device` | NOT RUN | — | no backend exists; the checks use the WebGPU C API |
| Performance, resize, multi-adapter | NOT RUN | — | out of scope |

## Independent review

- **Mapping and browser spike**: `review-q7a.md` — first pass changes requested, second pass accept
  with limitations; `review-spec-consistency.md` records the spec audit, with the header conflict owned
  by T089 (now done).
- **Emscripten leg**: `review-t088-encoder.md` — see it for the reviewer's scope, commands and decision.
- **Remaining limitation**: no WebGPU backend implements the core device, so the leg proves a C++
  encoder can drive the mapped operations and that the header compiles for `wasm32`; it does not prove
  a `doom3::rhi` implementation on WebGPU, and it does not prove a software-adapter path.
