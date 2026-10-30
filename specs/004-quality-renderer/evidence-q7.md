# Q7 evidence: browser profile host slice (T071-T075)

**Stage and claim**: spec 004 US8 / T071–T075, issue #67. This record covers a **host slice** of the
`webgpu-browser` profile: the portable-profile contract, capability table, VFS/file abstraction,
cooperative main loop, canvas/swapchain, input/audio bridges and a pinned-browser harness, exercised
with a **procedural test scene**.

It does **not** claim a WebGPU implementation of `doom3::rhi::Device`, browser support, classic
renderer parity, or real-scene acceptance. The C++ profile backend and `tests/renderer` paths named
by T072/T073 were not touched; the owner instruction for this slice froze those paths. T074's
real-scene run is `NOT RUN` because no legal game data is present. T075's independent review is
`NOT RUN` at the time of writing; the author does not write the approval.

## Identity and scope

- **Source identity**: base `f203805b1f59f2ccf24026c557ff4becba76bf6e` (`main`), branch
  `feat/67-q7-webgpu-profile`; the commit that carries this record is in the PR for issue #67.
  `doom3_orig/doom3` untouched and clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`.
- **Data identity and rights**: synthetic only. The procedural WGSL, synthetic VFS file and every
  expected value are authored here. No game data, retail shader, `glprogs`, derived pack or
  third-party SDK source is copied or required.
  Content hashes at the time of the run:
  - `web/profile/scene.mjs` `5F1BFD08F4440440AE63B87CD4BD97B64FE1E6FAE1A075F430119AA1C8D775E1`
  - `web/profile/capability.mjs` `CC4637A64176DA7BF131399DBDB1FAED7C80E00EC9C7096C2AB1FA63001CA22B`
  - `tests/webgpu/profile.test.mjs` `7CDE37DED0BF21C5986B22C6A6DEE29DEBF9D787908FEED02DE8FDAAD8241F60`
  - `tests/webgpu/profile-source-guard.test.mjs` `48AC15E2BA5DC4A8DA24DCE24F1C4431E13804772CF22559DDAA2631CB34587C`
- **Scene and inputs**: one full-screen procedural field (`q7-procedural-field/1`) on a 480×270 CSS
  canvas (drawing buffer 478×268 at DPR 1 because the CSS border is inside `box-sizing: border-box`),
  animated by a time uniform. Keyboard input is exercised synthetically (`KeyW` down/up). Pointer
  lock, browser gesture audio and device loss are not induced.
- **Exclusions and limitations**: main-thread canvas only (no worker/OffscreenCanvas host); no pak/zip
  parser; no BC assets; no audio output or pointer lock in headless mode; no performance, memory or
  startup budget; no real scene. These are recorded, not hidden.

## Environment and configuration

| Item | Value |
|---|---|
| Host | Windows 11 x64, Node v24.19.0 |
| Browser | Google Chrome `156.0.8078.76`, protocol 1.3, headless (`--headless=new --enable-unsafe-webgpu`) |
| Browser pin | `tests/webgpu/browser-pin.json`, major `156`, Q7a evidence version `156.0.8078.73`; pin satisfied at major level, exact patch recorded |
| Adapter | `intel` / `xe-lpg`, `isFallbackAdapter=false`; vendorId/deviceID report `0` → `reportedFacts=0x0`; no software adapter requested |
| Device features | `core-features-and-limits` only |
| Capability status | `DEGRADED` — all 14 required limit rows PASS; four optional features use declared fallbacks, one of them quality-affecting (`texture-compression-bc` → `bc-transcode-required`) |
| Shader provenance | project-authored WGSL in `web/profile/scene.mjs`; no retail shader text |
| Storage | full browser log, JS report and procedural PNG stay in the git-ignored `build/webgpu-profile/`; the tracked evidence is the text report below and `evidence/q7-profile.json` |

## Procedure and retained artifacts

- **Predeclared thresholds**: required profile checks must be `PASS` twice; the per-run nonce must be
  echoed; `R1-real-scene` must be `NOT RUN`; canvas readback must contain at least one non-black
  pixel; zero uncaptured WebGPU errors; pinned browser major 156.
- **Commands and exits**:

| Command | Exit | Observation |
|---|---|---|
| `node --test tests/webgpu/profile.test.mjs tests/webgpu/profile-source-guard.test.mjs` | 0 | 21 tests, 21 pass, 0 fail |
| `node tests/webgpu/run-profile.mjs --browser "C:\Program Files\Google\Chrome\Application\chrome.exe" --repeat 2 --out build/webgpu-profile --evidence specs/004-quality-renderer/evidence` | 0 | 11 PASS / 0 FAIL / 4 NOT RUN twice; repeatable; pinned major 156 true; artifact `evidence/q7-profile.json` |
| `git grep --no-index -n -E "dxr|dlss|raytracing|ray query|acceleration structure" -- web/profile tests/webgpu/profile.test.mjs` | 1 (expected: no match) | zero hits; grep exits 1 when nothing matches |
| `git status --short doom3_orig/doom3` | 0 | no output; submodule clean at `a9c49da5` |

- **Negative controls**: unit tests include a missing VFS file (`ERR_NOT_FOUND`), path traversal
  (`ERR_PATH_TRAVERSAL`), a short required limit (`UNSUPPORTED`), a Windows target id
  (`UNSUPPORTED`), a quality-feature fallback (`DEGRADED`), an AudioContext that stays suspended
  (`BLOCKED`), and playback before a gesture (`NOT RUN`). The browser page repeats the capability
  classification in `P9-capability-fallback`.
- **Known browser warning**: Chrome logs that `powerPreference` is ignored on Windows. It does not
  affect the adapter selection or any check; it is retained in the ignored `run.log`.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| T071 portable-profile contract | RUN | PASS | contract defines identity, result vocabulary, 14 required limits, 4 optional fallbacks and the real-scene rule | `specs/004-quality-renderer/portable-profile-contract.md` |
| Capability table | RUN | PASS | 14/14 required rows PASS; 4 optional features FALLBACK; overall `DEGRADED` | `evidence/q7-profile.json` |
| P1 device | RUN | PASS | adapter `intel`/`xe-lpg`, no fallback adapter, device created | `q7-profile.json` |
| P2 capability table | RUN | PASS | status `DEGRADED`, 0 unsupported rows, 4 declared fallbacks | `q7-profile.json` |
| P3 swapchain | RUN | PASS | `bgra8unorm`, opaque, usage 17, 478×268, first frame acquired | `q7-profile.json` |
| P4 procedural frame | RUN | PASS | one draw at warm-up; loop adds frames | `q7-profile.json` |
| P4b canvas readback | RUN | PASS | 128104 non-black pixels of 128104; both repetitions | `q7-profile.json`, ignored `build/webgpu-profile/profile.png` |
| P5 VFS | RUN | PASS | synthetic read, typed miss, traversal refusal; game data `NOT RUN`; pak parsing `UNSUPPORTED` | `q7-profile.json` |
| P6 main loop | RUN | PASS | attempt 1: 41 ticks / 32 frames; attempt 2: 39 ticks / 40 frames; no dropped time | `q7-profile.json` |
| P7 input | RUN | PASS | synthetic `KeyW` down/up changed and cleared the bridge state | `q7-profile.json` |
| P8 audio gate | RUN | PASS | `playCue()` before unlock returned `NOT RUN` with the gesture reason | `q7-profile.json` |
| P9 capability fallback | RUN | PASS | READY / DEGRADED / UNSUPPORTED / wrong-target UNSUPPORTED classified | `q7-profile.json` |
| E uncaptured errors | RUN | PASS | 0 uncaptured WebGPU errors in both repetitions | `q7-profile.json` |
| P7b pointer lock | NOT RUN | — | headless run has no trusted user gesture; request path covered by input unit tests | `q7-profile.json` |
| P8b browser audio gesture | NOT RUN | — | headless run has no trusted user gesture; unlock path covered by fake-AudioContext unit tests | `q7-profile.json` |
| E device loss | NOT RUN | — | not induced in this harness; Q7a's C8 covers the loss path | `q7-profile.json` |
| R1 real scene | NOT RUN | — | `no legal DOOM 3 game data is present in this repository; retail pk4/pk3 assets are EULA-bound and were not supplied for this slice` | `q7-profile.json` |
| T072 portable RHI capability fixtures | NOT RUN | — | `tests/renderer/**` and `src/renderer/rhi/**` were frozen for this slice; no C++ profile selection was implemented | — |
| T073 target adapter/backend | NOT RUN | — | the host slice adapts browser APIs, not `src/renderer/profiles/profile_backend.cpp`; that route remains open | — |
| T074 real scene in a claimed target | NOT RUN | — | no legal game data and no claim of a supported target; procedural scene is not a pass | this record, R1 row |
| T075 independent review | NOT RUN | — | reviewer not yet assigned; the author must not write it | PR review pending |

## Per-check summary

| Target / item | Execution | Outcome |
|---|---|---|
| `webgpu-browser` host slice (procedural) | RUN | PASS for host plumbing only |
| `webgpu-browser` real scene | NOT RUN | — (no legal data) |
| Native D3D12 profile | not exercised | — (unchanged by this slice) |
| `doom3::rhi` WebGPU backend | NOT RUN | — (no backend exists) |

## Independent review

- **Independent reviewer**: NOT RUN.
- **Review scope and commands**: pending; the reviewer must read the PR diff, rerun both test
  commands, inspect the capability/fallback table, and record `APPROVED`/`BLOCKED` at the reviewed
  SHA in its own session.
- **Review decision and unresolved items**: none yet. Until that review exists, T075 is not done and
  no browser support claim is made.
