# Q1a evidence: D3D12 RHI clear/readback foundation

**Status**: Q1a implemented against the amended three-tier contract (Refs #20, #11).
Debug and Release runs execute the public and private fixtures on both an Intel hardware
adapter and the Microsoft Basic Render Driver (WARP). The header/public-test gate passes.
The debug layer and GPU-based validation were **attempted and are SKIPPED** on this machine
(`hr=0x887A002D`), so full Q1 is still blocked; this record does not mark them passing.
Independent review round 1 requested evidence corrections (no code defect); the corrections
are in this revision; round 2 accepted the Q1a code and evidence at `369c47b` in [review-q1a.md](./review-q1a.md). A post-review allocation guard is disclosed there.

**History and supersession**: the 2026-10-27 acceptance in this file described the
pre-amendment seam, where resource states, the journal, staging pitch and fence values were
public. Issue #20 changed the contract into portable core, D3D12-private backend and native
extensions. That acceptance and its review are superseded. The 2026-10-28 partial
remediation was also incomplete and was never merged. Everything below was measured from
the commit named in "Source identity"; no earlier result is reused as current evidence.

## Identity and scope

- **Stage and claim**: spec 004 Q1a (issue #11), tasks T012–T021. Claim: a standalone,
  synthetic 4×4 RGBA8 per-row clear, a correctly row-pitched readback exposed as tightly
  packed caller bytes, a recorded backend-private state journal, an opaque completion token,
  and release that refuses while the producing submission is still running. Q1a only.
- **Source identity**: worktree `feat-11-q1a-rhi-clear-readback`, branch
  `feat/11-q1a-rhi-clear-readback`, code commit `1c0187bd5309747df3c968b29842c8fc75685421`
  plus the evidence/fix commit that carries this file. Rebased on `origin/main`
  `1703655464a6c93ebf14825b4ee16da456b2650d`. Submodule `doom3_orig/doom3` at
  `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, porcelain empty. No engine target copy, root
  CMake change or engine source edit is part of this claim.
- **Data identity and rights**: no game data. The only inputs are literal 8-bit channel
  values and a synthetic 4×4 or 2048×2048 description generated in the test source. The
  source in this repository is GPL-3.0-or-later; no third-party asset or SDK component is
  redistributed.
- **Scene and inputs**: offscreen only. The small fixture is 4×4 with four distinct literal
  rows; the lifetime fixture submits two 2048×2048 descriptions whose rows cycle through the
  same four colours. No window, camera, map, model or texture file is loaded.
- **Exclusions and limitations**: no draw, compute, multiple attachments, readable depth,
  motion/validity, frame graph, resize, window presentation, engine integration, browser or
  WebGPU backend, PBR/GI, DXR/DLSS, or performance claim. A D3D12 Q1a pass is not browser
  portability evidence. The debug layer and GPU-based validation did not execute here.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 x64; Visual Studio 2022 Professional,
  MSVC 19.44.35228.0; Windows SDK 10.0.26100.0; CMake 3.31.6-msvc6; Git 2.25.0.windows.1;
  PowerShell 5.1; Python 3.14.4 via `py -3 -I`. Generator "Visual Studio 17 2022", `-A x64`;
  Debug and Release; `/W4 /permissive- /EHsc`; C++17.
- **Adapter and driver**: hardware `Intel(R) Graphics`, vendor 0x8086, device 0x7D67,
  134,217,728 bytes reported dedicated video memory, driver 32.0.101.8331; software
  `Microsoft Basic Render Driver` (WARP), vendor 0x1414, device 0x008C. Adapter class and
  index are selected explicitly; out-of-range selection is exercised as a failed setup path.
- **Capability and fallback**: one ordered work queue plus a **private gate-signal queue**
  used only to make the private in-flight test deterministic; committed render-target
  texture, CPU-visible readback buffer, copy, and an opaque completion token. No fallback is
  auto-selected. A missing software adapter is SKIPPED, not passed. Debug layer and GPU-based
  validation are opt-in backend-private requests; absence is recorded, never converted into
  success.
- **Shader provenance and build**: N/A. Q1a records no pipeline, shader module, compiler or
  HLSL/DXC input; the slice uses fixed-function clears and a texture copy.

## Procedure and retained artifacts

- **Predeclared tolerances**: per-channel 8-bit UNORM absolute difference ≤ 1, fixed before
  the run. Rows are tightly packed at exactly `width * 4` bytes, no staging padding. Hash
  rule: FNV-1a 64 over each canonical expected row, published as constants before execution:

  | Row | Canonical bytes (RGBA, repeated) | Published digest |
  |---|---|---|
  | 0 | `00 40 80 FF` | `0x06B23876A7A2A8C5` |
  | 1 | `FF BF 80 40` | `0xA5581568C728313D` |
  | 2 | `20 60 A0 E0` | `0x6C69B69DA236AC65` |
  | 3 | `10 80 F0 30` | `0xF0FBAA39868107A5` |

  The test recomputes each digest from the literal row and fails on mismatch; actual raw row
  hashes are logged for diagnosis only and are not required to equal the expected digest.
  A controlled one-step difference must pass and a controlled two-step difference must fail.
- **Commands and exits** (worktree root, fresh directory `build-q1a-fresh`):
  1. `cmake -S tests/renderer -B build-q1a-fresh -G "Visual Studio 17 2022" -A x64` → 0,
     log `build-q1a-fresh-configure.log`.
  2. `cmake --build build-q1a-fresh --config Debug -- -m` → 0,
     log `build-q1a-fresh/build-Debug.log`; no warning or error line.
  3. `ctest --test-dir build-q1a-fresh -C Debug --no-tests=error -V --output-junit
     build-q1a-fresh/junit-Debug.xml` → 0; 7/7 passed, log
     `build-q1a-fresh/ctest-Debug.log`, JUnit
     `build-q1a-fresh/build-q1a-fresh/junit-Debug.xml` (`tests=7 failures=0 skipped=0`).
  4. Same build and CTest for Release → 0; 7/7 passed; log
     `build-q1a-fresh/build-Release.log`, `build-q1a-fresh/ctest-Release.log`,
     JUnit `build-q1a-fresh/build-q1a-fresh/junit-Release.xml`
     (`tests=7 failures=0 skipped=0`).
  5. After the review-driven robustness edits, both configurations were rebuilt and retested
     in the same directory → 0 each; logs `build-q1a-fresh/rebuild-Debug.log`,
     `rebuild-Release.log`, `ctest-Debug.log`, `ctest-Release.log`; no warning line.
  6. Ten repeated Debug runs → exit 0 each; logs `build-q1a-fresh/repeat-1.log` …
     `repeat-10.log`.
  7. `py -3 -I scripts/validate-rhi-contract.py` → 0; "header scan: RUN over 1 header(s)
     and 3 public test file(s)".
  8. `py -3 -I scripts/validate-rhi-contract.py --self-test` → 0; 16/16 negative controls
     detected; log `build-q1a/validator-selftest.log`.
  9. `py -3 -I scripts/test-validate-rhi-contract.py` → 0; 38 tests `OK`; log
     `build-q1a/validator-tests.log`.
  10. `py -3 -I scripts/test-validate-rhi-contract-mutations.py` → 0; 11 protected behaviours
      rejected one at a time, restored suite 38 tests exit 0; log
      `build-q1a/validator-mutations.log`.
  11. `git -C doom3_orig/doom3 status --porcelain` → empty.
- **Retained logs and artifacts**: all paths above are worktree-local and git-ignored.
  Readback diagnostics: `build-q1a/q1a-evidence/q1a-clear-hardware.bmp` and
  `q1a-clear-software.bmp` (12,342 bytes each), PNG conversions (241 bytes each), a local
  HTML dashboard and a Playwright full-page screenshot `evidence-gallery.png` (80,308 bytes).
  Those images are diagnostic artifacts, not tracked deliverables and not visual-quality
  evidence. JUnit `skipped=0` is a CTest-level count; internal validation checks that were
  SKIPPED are recorded separately below and are not converted into passes.
- **Negative controls**: see the control table below. Each control is required to be
  detected; a genuinely inapplicable control is recorded as NOT APPLICABLE and does not
  count as a pass.

### Red runs without an implemented backend

The amended 7-test suite and the amended implementation were produced in the same rewrite,
so there is no historical "tests committed before code" checkout for this exact suite. To
keep the T013/T016 red requirement real rather than asserted, the final suite was run against
a deliberately stubbed backend and against behavioral mutations:

| Red condition | Test | Exit | Observed assertion |
|---|---|---|---|
| M0a backend stub (submit refuses every request) | `rhi.q1a.clear_readback` | 8 | `SubmitClearReadback failed`; `device is not initialized` |
| M0b backend stub | `rhi.q1a.outstanding_release` | 8 | `first SubmitClearReadback failed` |
| M1 every row cleared with row 0's colour | `rhi.q1a.clear_readback` | 8 | rows 1–3 exceed tolerance |
| M2 public copy walks with tight stride as source pitch | `rhi.q1a.clear_readback` | 8 | rows 1–3 read as zero bytes |
| M5 outstanding refusal removed | `rhi.q1a.outstanding_release` | 8 | no in-flight attempt observed; both large submissions released before readback |
| M6 published digest corrupted | `rhi.q1a.clear_readback` | 8 | expected-data integrity assertion |

Logs: `build-q1a/mutations/M0a_stub_backend_clear-test.log`, `M0b_stub_backend_lifetime-test.log`,
`M1_repeat_row_zero-test.log`, `M2_wrong_source_stride-test.log`,
`M5_no_outstanding_refusal-test.log`, `M6_expected_digest-test.log`.

### Control ownership

| Control | Owner test | Observed |
|---|---|---|
| off-by-one source-row offset | `rhi.q1a.negative_controls` (public) | APPLICABLE, detected on 4/4 rows |
| repeat row 0 | `rhi.q1a.negative_controls` (public) | APPLICABLE, detected on 3/3 rows |
| packed row-length misread (`width * 4 - 1`) | `rhi.q1a.negative_controls` (public) | APPLICABLE, detected on 3 rows |
| staging-pitch misread (`width * 4` over raw staging) | `rhi.q1a.staging_pitch` (D3D12-private) | APPLICABLE; pitch 256 vs tight 16; detected on 3 rows |
| in-flight release, public token half | `rhi.q1a.outstanding_release` | two submissions; both attempts before completion returned `RefusedOutstanding` |
| in-flight release, private fence half | `rhi.q1a.outstanding_release_private` | `completedValue=0 < signalValue=1`, `deviceRemoved=false`; `RefusedOutstanding` |
| wait-idle-before-release shortcut | private control + mutation M4 | the shortcut reports `Released`; the main assertion fails under M4 |
| expected-data digest integrity | `rhi.q1a.clear_readback` | four digests match their literal rows; mutation M6 fails |
| tolerance pair | `rhi.q1a.clear_readback` | one-step accepted, two-step rejected |
| state sequence | `rhi.q1a.state_journal` (D3D12-private) | exact three-entry order below |

State journal observed on both adapters:

```text
[0] render-target    UNKNOWN        -> RENDER_TARGET  created for render-target use
[1] render-target    RENDER_TARGET  -> COPY_SOURCE    per-row clear recorded, moved for the copy
[2] readback-buffer  UNKNOWN        -> COPY_DEST      created as the copy destination
```

The public tests never name a state, journal entry, staging pitch or fence value. The
deterministic in-flight proof uses a backend-private gate; the public test additionally makes
a bounded observation with two queued 2048×2048 submissions and never waits before a release
attempt. Because a release attempt can legitimately succeed on a submission that has already
finished, the public test tracks which submissions were released and only waits on and reads
back the ones that are still owned; it requires at least one readback and at least one
incomplete-token refusal. If both attempts finish before the CPU reaches them, the test fails
a bounded observation (it cannot false-pass).

### Mutation controls (Debug, then restored)

| Mutation | Target test | Build | Test exit | Observed failure |
|---|---|---|---|---|
| M0a submit stub (always the uninitialized error) | `rhi.q1a.clear_readback` | 0 | 8 | `SubmitClearReadback failed` |
| M0b submit stub | `rhi.q1a.outstanding_release` | 0 | 8 | `first SubmitClearReadback failed` |
| M1 clear every row with row 0's colour | `rhi.q1a.clear_readback` | 0 | 8 | rows 1–3 exceed tolerance (`max_delta` 255, 32, 207) |
| M2 walk the public copy with the tight stride as source pitch | `rhi.q1a.clear_readback` | 0 | 8 | rows 1–3 read as zero bytes (`max_delta` 255, 224, 240) |
| M3 record `COPY_DEST` instead of `COPY_SOURCE` in the journal | `rhi.q1a.state_journal` | 0 | 8 | exact-sequence assertion fails; entry 1 is `RENDER_TARGET -> COPY_DEST` |
| M4 bounded wait/timeout before release, then release regardless | `rhi.q1a.outstanding_release_private` | 0 | 8 | `in_flight_release=Released` after the wait; "release was not refused while the completion was outstanding" |
| M5 remove the outstanding refusal | `rhi.q1a.outstanding_release` | 0 | 8 | no in-flight attempt observed; both large submissions released before readback |
| M6 corrupt one published expected digest | `rhi.q1a.clear_readback` | 0 | 8 | "published expected digest does not match its canonical row"; the test process still continues and fails at the end |
| M7 walk raw staging with the real pitch in the control | `rhi.q1a.staging_pitch` | 0 | 8 | `staging_pitch APPLICABLE detected=NO` |

After each mutation the source was restored and rebuilt; the final restored run is 7/7 green
(`build-q1a/after-mutations.log`, exit 0). M4 intentionally spends the 30 s teardown budget
per adapter in the mutated code and took about 180 s; this is a property of the mutation, not
of the restored implementation.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| Standalone configure, Debug build | RUN | PASS | exit 0; no warning or error | `build-q1a-fresh-configure.log`, `build-q1a-fresh/build-Debug.log` |
| Standalone configure, Release build | RUN | PASS | exit 0; no warning or error | `build-q1a-fresh/build-Release.log` |
| Full CTest, Debug | RUN | PASS | 7/7, 0 failed | `build-q1a-fresh/ctest-Debug.log` |
| Full CTest, Release | RUN | PASS | 7/7, 0 failed | `build-q1a-fresh/ctest-Release.log` |
| Repeated Debug runs | RUN | PASS | 10/10 exit 0 | `build-q1a-fresh/repeat-1..10.log` |
| Header/public-test lexical gate | RUN | PASS | scan over 1 header and 3 public test files; exit 0 | validator output above |
| Validator self-tests and mutation suite | RUN | PASS | 16/16, 38 tests, 11 protected behaviours | `build-q1a/validator-*.log` |
| `rhi.q1a.clear_readback`, Intel | RUN | PASS | 4×4 per-row pattern; every row within tolerance; digest integrity OK | test 1 in `ctest-Debug.log` |
| `rhi.q1a.clear_readback`, WARP | RUN | PASS | same bytes; functional check only | test 1 in `ctest-Debug.log` |
| `rhi.q1a.negative_controls`, both adapters | RUN | PASS | three public controls applicable and detected | test 2 in `ctest-Debug.log` |
| `rhi.q1a.state_journal`, both adapters | RUN | PASS | exact three-entry sequence; render target never a copy destination | test 3 in `ctest-Debug.log` |
| `rhi.q1a.staging_pitch`, both adapters | RUN | PASS | pitch 256, tight 16; control detected 3 rows | test 4 in `ctest-Debug.log` |
| `rhi.q1a.outstanding_release`, both adapters | RUN | PASS | two submissions; at least one incomplete-token release attempt; `NotReady` before completion | test 5 in `ctest-Debug.log` |
| `rhi.q1a.outstanding_release_private`, both adapters | RUN | PASS | `completedValue=0 < signalValue=1`, `deviceRemoved=false`; `RefusedOutstanding`; gate released, readback correct | test 6 in `ctest-Debug.log` |
| `rhi.q1a.diagnostics`, four opt-in combinations, both adapters | RUN | PASS | requests attempted and recorded; `cases_pass=8 validation_executed=0`; no unwaived message | test 7 in `ctest-Debug.log` |
| Debug layer actually enabled | SKIPPED | — | `D3D12GetDebugInterface failed (hr=0x887A002D)`, both adapters, all requested combinations | test 7 in `ctest-Debug.log` |
| GPU-based validation actually enabled | SKIPPED | — | same missing SDK component; recorded SKIPPED, never PASS | test 7 in `ctest-Debug.log` |
| Removed-device sentinel handling | NOT RUN | — | static audit only: `UINT64_MAX` is not completion and the private observation carries `deviceRemoved`; no removal fault injection is claimed | source audit |
| Issue #11 body sync | NOT RUN | — | the authenticated account lacks `UpdateIssue`; the mirror `issue-q1a.md` is authoritative | — |
| Independent review, round 1 | RUN | — | changes requested on evidence/process only; no blocking code defect | `review-q1a.md` |
| Independent review, round 2 | RUN | PASS | ACCEPT at `369c47b`; fresh Debug/Release 7/7, validator suite, M5 reproduced | `review-q1a.md` |

### Q1a acceptance mapping

| Acceptance criterion | Result | Evidence |
|---|---|---|
| Configure/build without engine target copy, data or proprietary SDK | PASS | fresh configure/build logs; CMake links only `d3d12 dxgi` |
| No backend noun in the public header or public tests; header gate exits 0 | PASS | validator: 1 header, 3 public test files |
| Debug CTest exits 0 and an adapter actually executed | PASS | 7/7; Intel and WARP both ran the clear/readback |
| 4×4 per-row clear/readback with independent bytes, correct private sequence, no native state in the public test | PASS | `rhi.q1a.clear_readback`; journal test is private |
| State journal through the private hook | PASS | `rhi.q1a.state_journal` |
| Negative controls with named owners; staging pitch private | PASS | three public + one private control; table above |
| Two submissions, in-flight release through the token, private fence assertion, wait-idle control, incomplete-init shutdown | PASS with scope note | public and private lifetime tests; M4. Incomplete setup is exercised by an out-of-range adapter index, which fails before backend objects exist; a post-device-creation failure is not fault-injected |
| T013 digests and tolerance | PASS with scope note | published digests, integrity check, one-step/two-step pair, M6. The red evidence is a stub/mutation of the final suite, not a historical pre-implementation checkout |
| Debug/GBV opt-in, attempted, distinct RUN/SKIPPED/NOT RUN | PASS for reporting; checks SKIPPED | test 7; `hr=0x887A002D` |
| Evidence records commands, exits, config, adapter, skipped, logs, red/green/mutation, changed files | PASS | this file, including "Changed files" |
| Clean submodule; no data/binaries/build output tracked | PASS | submodule porcelain empty; build trees are ignored |
| Independent review | PASS | round 2 ACCEPT at `369c47b`; post-review allocation guard disclosed |

## Changed files

```text
A  specs/004-quality-renderer/evidence-q1a.md
A  specs/004-quality-renderer/review-q1a.md
A  src/renderer/d3d12/d3d12_private.h
A  src/renderer/d3d12/rhi_d3d12.cpp
A  src/renderer/rhi/rhi.h
A  tests/renderer/CMakeLists.txt
A  tests/renderer/d3d12/private_test_support.h
A  tests/renderer/d3d12/rhi_d3d12_test.cpp
A  tests/renderer/d3d12/rhi_diagnostics_test.cpp
A  tests/renderer/d3d12/rhi_lifetime_private_test.cpp
A  tests/renderer/q1a_test_support.h
A  tests/renderer/rhi_clear_test.cpp
A  tests/renderer/rhi_lifetime_test.cpp
```

No engine source, submodule, target copy, root CMake file or game data is changed. The
obsolete public `tests/renderer/rhi_diagnostics_test.cpp` was removed; its private
replacement is `tests/renderer/d3d12/rhi_diagnostics_test.cpp`.

## Task status (T012–T021)

| Task | Status | Note |
|---|---|---|
| T012 standalone CMake/CTest and public seam | PASS | public header contains no backend vocabulary |
| T013 public clear test, digests, tolerance pair, three controls | PASS; red is controlled | stub/mutation reds, not a historical pre-implementation log |
| T014 adapter selection, submission, token, private journal and pitched readback | PASS | journal and pitch are private |
| T015 break/restore row indexing, stride and journal; record controls | PASS | M1, M2, M3, M7 |
| T016 lifetime public test and private companion | PASS; red is controlled | two submissions; public token half; private fence half; stub/M5 reds |
| T017 fence-associated ownership without wait-idle releases | PASS | M4 fails the main assertion |
| T018 retirement red/green and wait-idle shortcut | PASS | M4 and the private wait-idle control |
| T019 opt-in diagnostics and adapter reporting | PASS for behavior; debug/GBV SKIPPED | `validation_executed=0`; full Q1 remains blocked |
| T020 fresh Q1a run with full retained logs | PASS | `build-q1a-fresh`, rebuild after fixes, repeats, validator suite |
| T021 independent review and rerun | PASS | round 2 ACCEPT at `369c47b`; the post-review N2 guard is author-verified only |

## What Q1a does not prove

- It does not prove draw, compute, multiple attachments, readable depth, motion/validity,
  overlays, resize, history, frame graphs or window presentation.
- It does not prove engine integration, classic-renderer parity, visual quality, frame rate,
  memory use or hardware performance.
- The debug layer and GPU-based validation did not run. Full Q1 requires them to execute with
  zero unwaived errors on an environment where they are available.
- The portable header is a boundary, not a WebGPU, Vulkan or browser backend. Browsers do not
  expose D3D12; a web backend needs a separately designed and tested implementation.
- A software-adapter pass is a functional check only.
- The public in-flight observation is bounded; the deterministic proof is the private gated
  test, which still makes its public assertion through the completion token alone.
- Device removal is not fault-injected. A failed signal or unproven wait is quarantined into
  a per-device leaked store, and `Shutdown` keeps returning an error until a new `Initialize`;
  this path is inspected, not exercised by a failing device.

## Reproduce

```powershell
cmake -S tests/renderer -B build-q1a-fresh -G "Visual Studio 17 2022" -A x64
cmake --build build-q1a-fresh --config Debug -- -m
ctest --test-dir build-q1a-fresh -C Debug --no-tests=error -V --output-junit build-q1a-fresh/junit-Debug.xml
cmake --build build-q1a-fresh --config Release -- -m
ctest --test-dir build-q1a-fresh -C Release --no-tests=error -V --output-junit build-q1a-fresh/junit-Release.xml
py -3 -I scripts/validate-rhi-contract.py
py -3 -I scripts/validate-rhi-contract.py --self-test
py -3 -I scripts/test-validate-rhi-contract.py
py -3 -I scripts/test-validate-rhi-contract-mutations.py
```

Use a new, non-existing build directory for a fresh run. The fixture reads
`RHI_EVIDENCE_DIR`; a manual executable run without it writes images to the current directory.

## Independent review

- **Independent reviewer**: round 1 was an agent reviewer with a fresh context that did not
  write the code. It reran the validator and its self-tests, built Debug and Release in its own
  `build-q1a-review` directory, ran all seven CTest cases with JUnit, inspected the public and
  private split, and reproduced M2 and M4 with exact file backups and clean rebuilds. Agent
  review does not replace a human sign-off.
- **Review scope and commands**: code commit `1c0187b`, the amended contract and issue mirror,
  the full public/private test tree, fresh configure/build/CTest in both configurations,
  validator self-tests, evidence path/exit/hash checks, and two reproduced mutations.
- **Review decision and unresolved items**: round 1 requested evidence/process changes only
  (no blocking code defect): commit the corrected evidence, replace the stale review, add the
  red and changed-files records, and fix several wording/robustness items. Those changes are
  applied here. Round 2 independently reran the full suite and returned ACCEPT at `369c47b`. A post-review `std::bad_alloc` guard in `Submit` (finding N2) is a defensive change after that acceptance; the author rebuilt and reran Debug/Release 7/7, but that delta was verified by a final delta review at `b059007`.
