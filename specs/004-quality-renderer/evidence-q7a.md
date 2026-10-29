# Q7a evidence: WebGPU mapping gate and provider-free conformance spike

**Stage and claim**: spec 004 task T088 (Q7a), issue #35. Claim: every public declaration of the
portable RHI header is either mapped to a WebGPU-shaped operation or recorded as unmappable with the
behaviour a WebGPU backend must produce; the lexical header gate scans the core header and the public
test units with no prohibited noun; and the mapped operations execute in a real browser WebGPU
implementation on named hardware (twelve required checks plus a recorded software-adapter attempt). This record does **not** claim a WebGPU backend, a `wasm32` build,
browser parity, Q2 or Q7. The Emscripten-encoder leg required by T088 is NOT RUN, so Q2 remains
blocked.

## Identity and scope

- **Source identity**: base revision `d9bf1ca` (`main` at branch point), branch
  `docs/35-webgpu-conformance`. This record is committed together with the files it lists; `git log --oneline -- specs/004-quality-renderer/evidence-q7a.md` names the commit that carries it. `doom3_orig/doom3` is untouched and clean at `a9c49da5` (`git -C doom3_orig/doom3 status --porcelain` empty); no engine, target or submodule change. Header reconciliation is owned by T089 (issue #36). Fixture identities: `tests/webgpu/conformance.html` SHA-256 `87e8e1de…`, `tests/webgpu/run-conformance.mjs` `a2318c6e…`, `scripts/validate-webgpu-mapping.py` `7620465f…`, `webgpu-mapping.md` `058ef5b2…` (first 16 hex).
- **Data identity and rights**: synthetic only. All WGSL and all expected bytes are authored in this
  repository (`tests/webgpu/conformance.html`); no game data, retail shader, `glprogs`, derived asset
  or third-party SDK is used, copied or translated.
- **Scene and inputs**: a 4×4 RGBA8 texture with four literal rows, a 64-element `u32` storage buffer,
  two 4×4 RGBA8 MRT targets, a 16-byte buffer round trip and a 64×64 canvas. No seed; every expected
  value is a literal in the page.
- **Exclusions and limitations**: no `emcc`, `emsdk` or Dawn on this host; no software WebGPU adapter
  was exposed; no out-of-memory induction; no performance, resize, multi-adapter or persistence
  measurement; no C++ compilation for WebAssembly.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 x64. Node v24.19.0 (built-in `fetch`, `WebSocket` and
  `http`; no npm package installed or downloaded). Python 3.14.4 through `py -3`. Visual Studio 2022
  Professional C++ tools with bundled CMake ≥ 3.21 and CTest, used only for the supporting Q1a rebuild.
- **Adapter and driver**: WebGPU adapter `intel` / `xe-lpg` (`isFallbackAdapter=false`) through Chrome
  `156.0.8078.73`, DevTools protocol 1.3, headless. The supporting Q1a run reported the D3D12 hardware
  adapter `Intel(R) Graphics` vendor `0x8086` device `0x7D67`, vram `134217728`, and the software
  adapter `Microsoft Basic Render Driver` vendor `0x1414` device `0x008C`.
- **Capability and fallback**: advertised and exercised — `maxTextureDimension2D=16384`,
  `maxColorAttachments=8`, `maxComputeWorkgroupSizeX=1024`, `maxBindGroups=4`,
  `minUniformBufferOffsetAlignment=256`, `minStorageBufferOffsetAlignment=256`, preferred canvas
  format `bgra8unorm`. `requestAdapter({forceFallbackAdapter:true})` returned no adapter, so no
  software WebGPU leg exists to select; no automatic switch is claimed.
- **Browser licence and acquisition**: Google Chrome 156.0.8078.73 was already installed at its
  standard program-files location; this session downloaded nothing. Chromium is BSD-3-Clause; the
  Chrome binary is proprietary and is not redistributed, committed or cached by this repository. The
  retained JSON records only `executableName: chrome.exe`, the version and the protocol; the full path
  stays in the ignored worktree-local `run.log`. `--browser`/`WGPU_BROWSER` let a reviewer pin another
  binary.
- **Shader provenance and build**: WGSL only, authored here, compiled by the browser's WebGPU
  implementation at page load. No DXC, HLSL, translator or external compiler is involved; entry points
  are `vs`/`fs` for the MRT pass and `main` for compute.

## Procedure and retained artifacts

- **Predeclared tolerances**: exact byte equality for every readback (expected bytes are integers, so
  no UNORM tolerance is needed). Canvas clear `1.0/0.5/0.0/1.0` converts to `255/128/0/255` and is
  byte-swapped for `bgra8unorm`. Device-loss liveness window 200 ms, per-check timeout 5 s, page result
  timeout 90 s. All values were written before the first run.
- **Commands and exits** (from the worktree root):

  | Command | Exit | Observation |
  |---|---|---|
  | `node tests/webgpu/run-conformance.mjs --repeat 2 --out build/webgpu-q7a --evidence specs/004-quality-renderer/evidence` | 0 | 12 required checks PASS on both attempts (567 ms, 585 ms) plus the software-adapter attempt recorded NOT RUN; `repeatable across 2 attempt(s): true` |
  | `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | `OK: 52/52 public declarations classified; statuses, check rows and justifications valid` |
  | `py -3 -I scripts/validate-webgpu-mapping.py --self-test` | 0 | baseline valid, 14 negative controls detected |
  | `py -3 -I scripts/test-validate-webgpu-mapping.py` | 0 | 14 document mutations and 13 validator mutants behaved as required |
  | `py -3 -I scripts/validate-rhi-contract.py` | 0 | `header scan: RUN over 1 header(s) and 3 public test file(s)`, zero prohibited hits |
  | `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | the validator's own negative controls detected each injected violation |
  | `cmake -S tests/renderer -B build-rhi-q7a -G "Visual Studio 17 2022" -A x64` | 0 | supporting Q1a rebuild in a fresh worktree-local directory |
  | `cmake --build build-rhi-q7a --config Debug` | 0 | supporting |
  | `ctest --test-dir build-rhi-q7a -C Debug --no-tests=error -V --output-on-failure --output-junit build-rhi-q7a/ctest-junit.xml` | 0 | `100% tests passed, 0 tests failed out of 7`, 2.03 s |
  | `git diff --cached --check` after staging every changed file | 0 | whitespace gate; new files are covered because they are staged |
- **Retained logs and artifacts**: tracked — `evidence/q7a-conformance.json` (per-check report,
  browser/adapter identity, both attempts, sanitized executable name) and `evidence/q7a-conformance.png`
  (screenshot of the result table and the presented canvas). Ignored worktree-local —
  `build/webgpu-q7a/{results.json,conformance.png,run.log}` and
  `build-rhi-q7a/{*.log,ctest-junit.xml}`.
- **Negative controls**: (1) in-page N1–N3 (mutated expectation rejected, packed offset rejected, a
  live device does not report loss); (2) expectation mutation — changing `i*3+1` to `i*3+2` in the
  expected array makes `C3-bind-group` **and** `C5-compute` fail and the runner exit 1, and restoring
  the file returns the original SHA-256 and a green run. This control found a real defect on its first
  use: `C5` tested the truthiness of the comparison result object instead of its `.equal` field, so it
  could not fail. The defect is fixed and the final tree fails both checks under the mutation.
  (3) nonce guard — removing the page's nonce echo makes the runner exit 2 with "a static page cannot
  stand in for an executed run"; restoring returns a green run. (4) validator mutation — 14 document
  mutations and 13 source mutants, each rejected. (5) the lexical gate runs over the header and public
  tests, so a D3D12 noun in either fails it.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| C1 device and adapter identity | RUN | PASS | `intel` / `xe-lpg`, `isFallbackAdapter=false` | `evidence/q7a-conformance.json` |
| C2 buffer `mapAsync` round trip | RUN | PASS | 16 literal bytes equal | same |
| C3 immutable bind group reused | RUN | PASS | `[1,4,7,10]` on both dispatches | same |
| C4 one MRT pass, two attachments | RUN | PASS | A `[64,128,191,255]`, B `[255,0,128,64]` | same |
| C5 compute dispatch, 64 `u32` | RUN | PASS | first eight `[1,4,7,10,13,16,19,22]`, expected equal | same |
| C6 `bytesPerRow=256` plus `mapAsync` | RUN | PASS | rows 0 and 3 exact, padding `0xAA`, packed offset rejected | same |
| C7 surface configure/acquire/commit | RUN | PASS | `bgra8unorm`, pixel `[0,128,255,255]` | `evidence/q7a-conformance.png` |
| C8 device loss | RUN | PASS | `device.lost` reason `destroyed` | `evidence/q7a-conformance.json` |
| N1 comparator sensitivity | RUN | PASS | one-unit mutation rejected at byte 20 | same |
| N2 pitch sensitivity | RUN | PASS | row 1 only correct at offset 256 | same |
| N3 loss-check liveness | RUN | PASS | live device pending after 200 ms | same |
| E uncaptured errors | RUN | PASS | count 0 for the whole run | same |
| A1 software-adapter attempt | NOT RUN | — | `requestAdapter({forceFallbackAdapter:true})` returned no adapter; the null result is retained in the JSON and is not a pass | same |
| Field-to-WebGPU classification | RUN | PASS | 52/52 declarations classified; statuses, check rows and justifications valid | `webgpu-mapping.md` |
| Lexical header gate (Q7a) | RUN | PASS | 1 header + 3 public test files scanned, zero hits | `validate-rhi-contract.py` output |
| Supporting Q1a rebuild and suite | RUN | PASS | 7/7 CTest cases on hardware and software D3D12 adapters | `build-rhi-q7a` logs, `evidence-q1a.md` |
| Emscripten or Dawn encoder leg | NOT RUN | — | no `emcc`, `emsdk` or Dawn on the host; no toolchain downloaded; Q2 stays blocked | `webgpu-mapping.md` §NOT RUN |
| Software WebGPU adapter | NOT RUN | — | `requestAdapter({forceFallbackAdapter:true})` returned no adapter | mapping table, `AdapterClass.Software` |
| Out-of-memory classification | NOT RUN | — | no OOM induced; mapping only | mapping table, `Status.OutOfMemory` |
| `wasm32` compile of the header | NOT RUN | — | requires the missing encoder toolchain | `webgpu-mapping.md` §NOT RUN |
| Performance, resize, multi-adapter | NOT RUN | — | out of scope for a conformance spike | same |

## Independent review

- **Independent reviewer**: a non-author agent reviewer with no authorship of the reviewed files; it
  wrote `review-q7a.md` and reran the commands itself. `review-spec-consistency.md` records a separate
  spec audit. No human sign-off is recorded.
- **Review scope and commands**: the reviewer reran the conformance runner, the mapping validator and
  its self-test and mutation suite, the header gate and `git status`; it also attempted falsification
  (mutated header, harder document mutations, a fake page) and checked the mapping against the WebGPU
  specification.
- **Review decision and unresolved items**: the first pass requested changes (whitespace, a
  machine-specific path in the retained JSON, a timing mismatch, overclaiming language); after the
  fixes the reviewer reran everything and accepted with limitations, with no blocking or major
  finding left in `review-q7a.md`. The falsification findings — a page that hardcodes results (now
  partially mitigated by the per-run nonce, not eliminated), the mapping validator's inability to
  detect a status flip, and the lexical gate's known synonym gaps — are recorded as limitations in
  `webgpu-mapping.md` ("What this gate cannot decide") rather than concealed. The spec audit's
  dispositions are in `review-spec-consistency.md`; its one remaining blocking item is the
  header/contract conflict, which T089 (issue #36) owns and which blocks Q7, not Q2.
