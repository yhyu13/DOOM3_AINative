# Review: Q7 portable profile and `webgpu-browser` host slice (T072-T075, issue #67)

## Verdict

APPROVED, limited to the T072 portable capability/fallback fixtures and the T073 `webgpu-browser`
host slice at the reviewed revision. Every check those rows claim was rerun here and reproduced.

T074 (real scene in a claimed target) remains NOT RUN: no legal DOOM 3 game data is present in this
repository. T075's deliverable is this file, but the Q7 row set is not accepted and no browser
support is claimed. A procedural field is host plumbing, not a renderer profile.

The corrections in "Required corrections" are evidence-record defects. They do not overturn the
host-slice verdict because every affected value was reproduced independently here, but they must be
fixed before this evidence is cited for any Q7 acceptance.

## Independent reviewer

- Session `ses_e839a13ecffer9zdg7lJkG6RP9`, worktree
  `.kilo/worktrees/review-q7-t075-9c7f9d5e540922dd`, branch `review/q7-t075`.
- The Q7 authoring session recorded in `ledger-q6-q7-reconcile.md` is
  `ses_e65dad6f3ffe2SBIN5c0QfKWmr`, worktree `feat-q7-t072-t075-c2443dfcbc9d5a2e`. The sessions and
  worktrees differ. This is process evidence for independence, not cryptographic identity.
- I did not author Q7, did not edit code, tests or images, and modified no tracked file other than
  this review. All scratch output went to the git-ignored `build/` and `build-q7-review/` paths.

## Reviewed revision

- `HEAD` = `2d11cd22ac41dcbb7d6b9d5eef475177c03ae100` (the #129 ledger reconciliation, equal to
  `origin/main` at review time).
- Q7 code last changed at merge `c4509cdef1375af631e8a3cc4d8185231cacb53c` (PR #119); the T072
  fixture and implementation last changed at `b244458` (PR #97). Neither path changed after those
  commits.
- All seven SHA-256 values recorded in `evidence-q7.md` for `capability.mjs`, `profile.mjs`,
  `scene.mjs`, `run-profile.mjs`, `profile.test.mjs`, `profile-source-guard.test.mjs` and
  `browser-pin.json` were recomputed from the git blobs at `HEAD` and match 7/7. The reviewed tree is
  therefore byte-identical to the tree the evidence names.
- Submodule `doom3_orig/doom3` is at `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb` and
  `git status --porcelain doom3_orig/doom3` is empty.

## Environment

- Windows 11 x64, Node v24.19.0.
- Google Chrome `156.0.8078.76` (x64), headless with the flags in `tests/webgpu/browser-pin.json`.
  The pin records major `156` and Q7a evidence version `156.0.8078.73`; this run is a pinned-major
  run, not a pinned-build run.
- Adapter reported `intel` / `xe-lpg`, `fallbackAdapter=false`, vendorId/deviceID `0` so
  `reportedFacts=0x0`; no software adapter requested.

## Commands rerun

All commands were run from the worktree root. Full logs are in the git-ignored `build/` and
`build-q7-review/` directories.

| Command | Exit | Observation |
|---|---:|---|
| `node --test tests/webgpu/profile.test.mjs` | 0 | 23 tests, 23 pass, 0 fail (the exact command named in issue #67) |
| `node --test tests/webgpu/profile.test.mjs tests/webgpu/profile-source-guard.test.mjs` | 0 | 25 tests, 25 pass, 0 fail (matches the evidence) |
| `cmake -S tests/renderer -B build-q7-review -G "Visual Studio 17 2022" -A x64` | 0 | CPU-only renderer fixtures configured |
| `cmake --build build-q7-review --config Release --target q7_portable_profile_test -- -m` | 0 | `q7_profile.lib` and `q7_portable_profile_test.exe` built |
| `build-q7-review\Release\q7_portable_profile_test.exe all` | 0 | `SUMMARY portable_profile checks=537 failures=0` |
| `ctest --test-dir build-q7-review -C Release --no-tests=error --output-on-failure -R portable_profile` | 0 | 6/6 passed, 0 failed |
| `node tests/webgpu/run-profile.mjs --browser <pin chrome> --repeat 2 --out build\webgpu-profile` | 0 | attempt 1 and 2: 11 PASS / 0 FAIL / 4 NOT RUN (15 checks); repeatable; pinned major 156 true |
| `git grep --no-index -n -E "dxr|dlss|raytracing|ray query|acceleration structure" -- web/profile tests/webgpu/profile.test.mjs` | 1 | no hits; exit 1 is the documented no-match result |
| SHA-256 of the seven Q7 blobs named in `evidence-q7.md` | 0 | 7/7 match |
| Negative control: feed `summarize()` a result whose `R1-real-scene` is PASS | 0 | refused: `R1-real-scene:expected-not-run` |
| Recursive scan for `*.pk4` / `*.pk3` / `*.pak`; `Test-Path _data` | 0 | 0 files; `_data` absent |
| `git status --porcelain doom3_orig/doom3`; `git submodule status` | 0 | empty; `cd6ae66...` |

`ctest` is not on this shell's `PATH`; the Visual Studio 2022 bundled `cmake`/`ctest` were used by
full path. The T072 configuration used `build-q7-review`, not the evidence's `build-q7`; both are
git-ignored, so the artifacts are not compared.

## Claim-by-claim verification

| Claim | Evidence | Reproduced here | Judgment |
|---|---|---|---|
| T071 contract defines identity, result vocabulary, required limits, fallbacks and the real-scene rule | `portable-profile-contract.md` | Read in full; every executable row present | PASS for the contract text, with the two wording corrections below |
| T072 portable RHI tables pass six CPU-only groups, 537 checks | `evidence-q7.md`, `portable_profile_test.cpp` | 537 checks, 0 failures; CTest 6/6 | PASS |
| T072 fixture encodes the contract independently of the implementation | Fixture literals at `portable_profile_test.cpp:64-115`; value, direction, fallback-id and impact checks at `:202-276` | Read; values match the contract by hand | PASS |
| T073 adapter is the explicitly named `webgpu-browser`, not a Windows pass | `profile.mjs:341-346`; `capability.mjs:108-113` | Live 18-row table in the harness report; wrong target is UNSUPPORTED in tests | PASS for the host slice |
| T073 pinned harness passes 11 required checks twice | `run-profile.mjs` | 11 PASS / 0 FAIL / 4 NOT RUN on both attempts; repeatable true | PASS |
| Per-run nonce is echoed | `run-profile.mjs:242-245` | Nonce present in `env.runNonce`; a static page cannot satisfy it | PASS |
| Pinned browser major | `browser-pin.json` major 156 | Chrome 156.0.8078.76: pin satisfied true | PASS at major level only |
| Live capability table has 14 limit rows and 4 feature rows | `evidence/q7-profile.json` | 18 rows; 14 PASS limits; 4 FALLBACK features; 0 unsupported | PASS |
| Overall capability status is DEGRADED, not READY | `q7-profile.json` | DEGRADED; quality fallback `bc-transcode-required` selected | PASS |
| Canvas readback contains a non-black frame | `q7-profile.json` | 128104 non-black of 128104 at 478x268 on both attempts | PASS |
| VFS reads, refuses a typed miss and traversal, and refuses game data/pak | `vfs.mjs`; P5 | P5 PASS; reported values match; unit tests cover the same | PASS |
| Cooperative loop advances fixed ticks with bounded catch-up | `main-loop.mjs`; P6 | Ticks and frames greater than zero, `droppedMs=0` | PASS; the recorded prose counts are wrong (finding 1) |
| Input bridge key state | `input.mjs`; P7 | Synthetic `KeyW` down/up PASS | PASS for key events; pointer lock is NOT RUN |
| Audio refuses before a gesture | `audio.mjs`; P8 | `NOT RUN` with the gesture reason | PASS for the gate only |
| R1 real scene is NOT RUN with the exact no-data reason | R1 row | Present with `NO_GAME_DATA_REASON`; a claimed PASS is refused | PASS as a refusal; T074 stays unticked |
| T074 real scene | T074 row | 0 paks, no `_data` mount, no legal data | NOT RUN, correctly |
| No out-of-scope DXR/DLSS vocabulary | grep and source guard | grep exit 1, zero hits | PASS |
| Evidence makes no browser support claim | both spec files | Read; every status is host-slice or NOT RUN | PASS |

## Unsupported and NOT RUN cases

| Case | Declared result | Verified behaviour |
|---|---|---|
| Real scene (T074) | NOT RUN | Reason is the no-legal-data sentence; harness fails `R1-real-scene:expected-not-run` if it is claimed PASS |
| Pak/zip directory parsing | UNSUPPORTED | `readPakDirectory()` returns the declared reason; no parser exists |
| Game data without a user mount | NOT RUN | `openGameData()` returns the no-data reason; a user mount changes only that row and says it was not exercised |
| Pointer lock | NOT RUN in the browser | Headless has no trusted gesture; request path has unit coverage only |
| Browser audio unlock | NOT RUN in the browser | Headless has no trusted gesture; fake-AudioContext unlock is unit-tested |
| Device loss | NOT RUN | Not induced; Q7a's C8 covers the loss path |
| Wrong target id | UNSUPPORTED | `win11-amd64` is refused in unit tests and in P9 |
| Short required limit | UNSUPPORTED | `maxBindGroups=3` is refused, never lowered |
| Quality fallback reported as support | Refused by design | DEGRADED is the result; the fallback is published |
| Excluded classes (ray tracing, neural upscaler, bindless, multiple queues, sparse resources, aliasing, depth bounds, polygon mode, fixed-function alpha test, texgen, L8/LA8/A8, guaranteed BC, native interop) | UNSUPPORTED | All 14 refused by the C++ table, including when the backend advertises them |
| Worker/OffscreenCanvas host | Not implemented | The slice is main-thread only |
| WebGPU `doom3::rhi::Device` | Does not exist | No backend is claimed |
| BC assets | Not exercised | No BC asset is present; transcode behaviour is untested |

## Findings

### Blocking

None for the approved T072/T073 host-slice scope. The real-scene gap is a T074 block, not a defect
in the host slice.

### Required corrections (evidence record, before any Q7 acceptance)

1. The P6 prose counts do not describe the retained artifact. `evidence-q7.md` records
   "46 ticks / 27 frames" and "45 ticks / 24 frames"; the committed
   `evidence/q7-profile.json` records `ticks=48, frames=28` and `ticks=47, frames=28`; this review's
   rerun produced 46/27 and 46/27. All runs satisfy the declared pass condition and `droppedMs=0`,
   but the prose must cite the artifact's run or the artifact must be regenerated from the run the
   prose describes. Frame counts are host-timing dependent and should not be presented as exact
   reproduction.
2. The submodule SHA in the command table is stale. The row says clean at `a9c49da5`; section
   "Identity and scope" says `cd6ae66`, and `git ls-tree` in this review confirms `cd6ae66`.
   `a9c49da5` is the pre-#113 revision.
3. `portable-profile-contract.md` section 3 says the unit tests fail when the document and
   `REQUIRED_LIMITS` drift apart. `tests/webgpu/profile.test.mjs:188-195` only asserts that each row
   name appears in the document; it does not assert the numeric requirement or the direction. The
   current 14 values and directions agree when compared by hand, and the C++ fixture pins the values
   for `capability_profile.cpp`, but the document-to-JS value/direction check that the contract
   promises is not implemented.
4. `portable-profile-contract.md` section 8 says the "input/audio bridge" is exercised in the pinned
   browser. The browser run exercises keyboard events and the pre-gesture audio refusal only: no
   AudioContext is created or unlocked, and pointer lock is not requested. The evidence table is
   precise; the section 8 sentence is not.
5. `docs/ROADMAP.md` line 206 says "21 unit tests" for Q7; the suite is 25 (23 functional plus 2
   source-guard). The Q7 code is unchanged; the roadmap figure is stale.
6. `evidence-q7.md` records SHA-256 values for 7 of the 13 Q7b tracked files. `audio.mjs`,
   `index.html`, `input.mjs`, `main-loop.mjs`, `swapchain.mjs` and `vfs.mjs` are unhashed. Git pins
   them at `c4509cd` and this review reran their tests, but the evidence record does not pin them.
7. The harness's NOT RUN reason check compares the page's detail against a constant imported from
   the same `vfs.mjs`/`profile.mjs` the page uses. The nonce prevents a static replay, but a page
   that imports the module could echo the constant. This is a merge-gate limitation, not a
   cryptographic check, and should not be described as one.

### Not defects

- `P2-capability-table` PASS with a DEGRADED status means the table was produced and the target was
  not refused; it does not mean the profile is READY or supported. The report and this review keep
  that distinction.
- All 14 required limit rows pass exactly at their minimum (for example `maxBufferSize`
  268435456 bytes and `maxTextureDimension2D` 8192). There is no headroom; a real scene may need
  more, which is a T074 question.
- The pin is major-level. Chrome `156.0.8078.76` is not the Q7a evidence build `156.0.8078.73`; the
  exact patch is recorded rather than hidden.

## Exact limits of this approval

- Applies to `webgpu-browser` host plumbing and to the backend-neutral `doom3::rhi` capability
  contract only. It is not a renderer profile, not a `doom3::rhi` WebGPU device, and not a browser
  port of the engine.
- Main-thread canvas only. No worker or OffscreenCanvas host, no pak/zip parser, no BC asset, no
  audio output, no pointer lock, no device-loss exercise, no performance, memory, startup or
  latency measurement.
- The only executed scene is `q7-procedural-field/1`, a synthetic full-screen WGSL field. It proves
  canvas configuration, a shader module, a bind group, a uniform update and a draw call. It does not
  prove classic-renderer parity, a real scene, or Q4 content.
- The profile is DEGRADED because `texture-compression-bc` is absent. The four fallbacks are
  declared and classified, not exercised: BC transcoding, float32 filtering, CPU frame timing and
  clip-plane avoidance have no behavioural evidence on this target.
- The real scene (T074) and any real-scene-dependent part of T075 remain NOT RUN/BLOCKED. T074 must
  not be ticked and no web-support claim may be derived from this review.
- The C++ `src/renderer/profiles/profile_backend.cpp` route named by T073 is frozen and unbuilt for
  this slice; the approved target adapter is the contract-assigned `web/profile/**` route.

## Ledger recommendation

- T072 and T073 stay ticked exactly as they are, limited to `webgpu-browser`.
- T074 stays unticked; `docs/ROADMAP.md` keeps Q7 PARTIAL.
- This file satisfies the T075 artifact requirement, but the ledger should not convert it into Q7
  acceptance: the real-scene case has not executed and the review scope is the host slice. Any
  future tick must record that limit, and Q7 remains PARTIAL until T074 runs on legal data in a
  claimed target.
