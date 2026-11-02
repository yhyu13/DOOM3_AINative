# Q7 evidence: browser profile host slice (T071-T075)

**Stage and claim**: spec 004 US8 / T071–T075, issue #67. This record covers the portable
capability/fallback contract (T072), the explicitly selected `webgpu-browser` target adapter and
pinned-browser harness (T073), and a **procedural test scene**. T074's real-scene run is
`NOT RUN` because no legal game data is present. T075's independent review is `NOT RUN`; the author
does not write the approval.

It does **not** claim a WebGPU implementation of `doom3::rhi::Device`, browser support, classic
renderer parity, or real-scene acceptance. The C++ `src/renderer/profiles/profile_backend.cpp` route
named by T073 was frozen for this slice; the chosen `webgpu-browser` target is adapted and tested
under `web/profile/**` and `tests/webgpu/**`, as the portable-profile contract §5 assigns. The
`doom3::rhi` capability tables (T072) are backend-neutral and were executed as a separate CPU-only
fixture.

## Identity and scope

- **Source identity**: base `b7c1a68` (`origin/main`, the S0 gate repair), branch
  `feat/q7-t072-t075`; the commit that carries this record is in the PR for issue #67.
  `doom3_orig/doom3` untouched and clean at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`.
- **Data identity and rights**: synthetic only. The procedural WGSL, synthetic VFS file and every
  expected value are authored here. No game data, retail shader, `glprogs`, derived pack or
  third-party SDK source is copied or required.
  Content hashes at the time of the run:
  - `web/profile/capability.mjs` `39ED4973689A2CCE6176A618BF82DAC2BCEC6B19F0A930FE3C732ACDF62295F1`
  - `web/profile/profile.mjs` `F6B97E17AAA3739A5172B552F5412FF218B9769BC3926EC317F7F8EE44E337EC`
  - `web/profile/scene.mjs` `EBFE8DA443E62D1508632916F0E56DBBAD3E24F0F4B8F31F4B531F6CA80B9875`
  - `tests/webgpu/run-profile.mjs` `073D5266F190BDF5132C32E99735719E9BC88D21B9FDCED7E00CA02B2A4F7916`
  - `tests/webgpu/profile.test.mjs` `A3827577F003E1AB49F33ECB97556D2001F03648F41BB085D022DC033313FA98`
  - `tests/webgpu/profile-source-guard.test.mjs` `F994E0A6BB48ECD88EBBF9903D6F223B71B501F59B290B6073723355862E296F`
  - `tests/webgpu/browser-pin.json` `13A07A1245E166F6A42FDD79060DF3DE78E41DF5A40CDC4B9D9DFC0069EE0249`
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
| Browser selection | `$env:WGPU_BROWSER` or `--browser`; the report records only `executableName=chrome.exe`, not a host path |
| Browser pin | `tests/webgpu/browser-pin.json`, major `156`, Q7a evidence version `156.0.8078.73`; pin satisfied at major level, exact patch recorded |
| Adapter | `intel` / `xe-lpg`, `isFallbackAdapter=false`; vendorId/deviceID report `0` → `reportedFacts=0x0`; no software adapter requested |
| Device features | none of the four optional features advertised |
| Capability status | `DEGRADED` — all 14 required limit rows PASS; four optional features use declared fallbacks, one of them quality-affecting (`texture-compression-bc` → `bc-transcode-required`) |
| Shader provenance | project-authored WGSL in `web/profile/scene.mjs`; no retail shader text |
| Storage | full browser log, JS report and procedural PNG stay in the git-ignored `build/webgpu-profile/`; the tracked evidence is the text report below and `evidence/q7-profile.json` |

## Procedure and retained artifacts

- **Predeclared thresholds**: required profile checks must be `PASS` twice; the per-run nonce must be
  echoed; `R1-real-scene` must be `NOT RUN`; the live capability table must name every required limit
  and optional feature; canvas readback must contain at least one non-black pixel; zero uncaptured
  WebGPU errors; pinned browser major 156.
- **Commands and exits**:

| Command | Exit | Observation |
|---|---|---|
| `node --test tests/webgpu/profile.test.mjs tests/webgpu/profile-source-guard.test.mjs` | 0 | 25 tests, 25 pass, 0 fail |
| `cmake -S tests/renderer -B build-q7 -G "Visual Studio 17 2022" -A x64` | 0 | configured the CPU-only renderer fixtures |
| `cmake --build build-q7 --config Release --target q7_portable_profile_test -- -m` | 0 | built `q7_profile.lib` and `q7_portable_profile_test.exe` |
| `ctest --test-dir build-q7 -C Release --no-tests=error --output-on-failure -R portable_profile` | 0 | 6/6 tests, 0 failures |
| `build-q7/Release/q7_portable_profile_test.exe all` | 0 | `SUMMARY portable_profile checks=537 failures=0` |
| `node tests/webgpu/run-profile.mjs --browser "$env:WGPU_BROWSER" --repeat 2 --out build/webgpu-profile --evidence specs/004-quality-renderer/evidence` | 0 | 11 PASS / 0 FAIL / 4 NOT RUN twice; repeatable; pinned major 156 true; artifact `evidence/q7-profile.json` |
| `git grep --no-index -n -E "dxr|dlss|raytracing|ray query|acceleration structure" -- web/profile tests/webgpu/profile.test.mjs` | 1 (expected: no match) | zero hits; grep exits 1 when nothing matches |
| `git status --short doom3_orig/doom3` | 0 | no output; submodule clean at `a9c49da5` |

- **Negative controls**: the T072 fixture encodes the contract independently of the implementation
  (14 required limits, 4 fallback ids, 14 excluded classes) and its `negative_controls` group refuses
  an advertised-but-excluded class, a short required limit behind an available feature, and a quality
  fallback reported as full support. The web unit tests include a missing VFS file (`ERR_NOT_FOUND`),
  path traversal (`ERR_PATH_TRAVERSAL`), a short required limit (`UNSUPPORTED`), a Windows target id
  (`UNSUPPORTED`), a quality-feature fallback (`DEGRADED`), an AudioContext that stays suspended
  (`BLOCKED`), and playback before a gesture (`NOT RUN`). The browser page repeats the capability
  classification in `P9-capability-fallback`.
- **Known browser warning**: Chrome logs that `powerPreference` is ignored on Windows. It does not
  affect the adapter selection or any check; it is retained in the ignored `run.log`.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| T071 portable-profile contract | RUN | PASS | contract defines identity, result vocabulary, 14 required limits, 4 optional fallbacks and the real-scene rule | `specs/004-quality-renderer/portable-profile-contract.md` |
| T072 portable RHI contract tables | RUN | PASS | 6/6 CTest groups, 537 checks, 0 failures; the fixture's independent literals match `capability_profile.cpp` | `tests/renderer/portable_profile_test.cpp`, ignored `build-q7` |
| T073 `webgpu-browser` target adapter | RUN | PASS | explicit `targetId=webgpu-browser`; adapter/device created; live capability table recorded with 18 rows (14 limits + 4 features) in `q7-profile.json` | `web/profile/profile.mjs`, `web/profile/capability.mjs`, `evidence/q7-profile.json` |
| T073 pinned-browser harness | RUN | PASS | pinned major 156 satisfied; nonce echoed; 11 required checks PASS twice; 4 expected NOT RUN rows; no auto-substitution | `tests/webgpu/run-profile.mjs`, `tests/webgpu/browser-pin.json` |
| Capability table | RUN | PASS | 14/14 required rows PASS; 4 optional features FALLBACK; overall `DEGRADED` | `evidence/q7-profile.json` |
| P1 device | RUN | PASS | adapter `intel`/`xe-lpg`, no fallback adapter, device created | `q7-profile.json` |
| P2 capability table | RUN | PASS | status `DEGRADED`, 0 unsupported rows, 4 declared fallbacks | `q7-profile.json` |
| P3 swapchain | RUN | PASS | `bgra8unorm`, opaque, usage 17, 478×268, first frame acquired | `q7-profile.json` |
| P4 procedural frame | RUN | PASS | one draw at warm-up; loop adds frames | `q7-profile.json` |
| P4b canvas readback | RUN | PASS | 128104 non-black pixels of 128104; both repetitions | `q7-profile.json`, ignored `build/webgpu-profile/profile.png` |
| P5 VFS | RUN | PASS | synthetic read, typed miss, traversal refusal; game data `NOT RUN`; pak parsing `UNSUPPORTED` | `q7-profile.json` |
| P6 main loop | RUN | PASS | attempt 1: 46 ticks / 27 frames; attempt 2: 45 ticks / 24 frames; no dropped time | `q7-profile.json` |
| P7 input | RUN | PASS | synthetic `KeyW` down/up changed and cleared the bridge state | `q7-profile.json` |
| P8 audio gate | RUN | PASS | `playCue()` before unlock returned `NOT RUN` with the gesture reason | `q7-profile.json` |
| P9 capability fallback | RUN | PASS | READY / DEGRADED / UNSUPPORTED / wrong-target UNSUPPORTED classified | `q7-profile.json` |
| E uncaptured errors | RUN | PASS | 0 uncaptured WebGPU errors in both repetitions | `q7-profile.json` |
| P7b pointer lock | NOT RUN | — | headless run has no trusted user gesture; request path covered by input unit tests | `q7-profile.json` |
| P8b browser audio gesture | NOT RUN | — | headless run has no trusted user gesture; unlock path covered by fake-AudioContext unit tests | `q7-profile.json` |
| E device loss | NOT RUN | — | not induced in this harness; Q7a's C8 covers the loss path | `q7-profile.json` |
| R1 real scene | NOT RUN | — | `no legal DOOM 3 game data is present in this repository; retail pk4/pk3 assets are EULA-bound and were not supplied for this slice` | `q7-profile.json` |
| T074 real scene in a claimed target | NOT RUN | — | no legal game data and no claim of a supported target; the procedural scene is not a pass | this record, R1 row |
| T075 independent review | NOT RUN | — | reviewer not yet assigned; the author must not write it | PR review pending |

## Per-check summary

| Target / item | Execution | Outcome |
|---|---|---|
| `doom3::rhi` portable capability tables (T072) | RUN | PASS for the CPU-only contract fixture |
| `webgpu-browser` target adapter (T073) | RUN | PASS for the host adapter and pinned harness |
| `webgpu-browser` real scene (T074) | NOT RUN | — (no legal data) |
| Native D3D12 profile | not exercised | — (unchanged by this slice) |
| `doom3::rhi` WebGPU backend | NOT RUN | — (no backend exists) |

## Independent review

- **Independent reviewer**: NOT RUN.
- **Review scope and commands**: pending; the reviewer must read the PR diff, rerun the Node and CTest
  commands above, inspect the capability/fallback table in `evidence/q7-profile.json`, and record
  `APPROVED`/`BLOCKED` at the reviewed SHA in its own session.
- **Review decision and unresolved items**: none yet. Until that review exists, T075 is not done and
  no browser support claim is made.
