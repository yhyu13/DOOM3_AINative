# Q2/Q3 design evidence (S4: T032, T033, T041 design)

Status: author evidence for design artifacts, 2026-10-28. This is **not** an engine build, a Q2
capture, a Q3 execution, or an independent sign-off. It proves that the three documents and their
generators/checkers are structurally consistent, that the motion vectors recompute, that the existing
RHI-contract validator still passes, and that the proposed CMake linkage mechanics work in a
synthetic project. It does not prove classic parity, engine linkage, or that T032/T033/T041 are
accepted.

## 1. Scope and inputs

| Item | Value |
|---|---|
| Session | S4, Q2 design and fixtures |
| Documents | `classic-parity.md` (T032), `engine-integration.md` (T033), `motion-fixtures.md` (T041 design) |
| Tools | `tools/motion_fixtures.py`, `tools/verify-q2-design.py` |
| Pinned engine | `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` |
| Q0 input | `main` (PR #31), `evidence-q0.md` (`baseline-final13`) and `reference-scenes.md` |
| Q0 result used | All four categories `PASS` twice at fixed tic: corridor 0.1827/0.2079, open-view 0.2258/0.2103, close-character 0.1082/0.0982, moving-light 0.0550/0.0318; Q0 **established** |
| Review state used | Reviews 1/2 `BLOCKED`, review 3/4 `PASS-WITH-FINDINGS`; final13 after a comment-only script change; T011 complete with residual gaps |

Q0 is a live S1 artifact; it was re-run through `final13`. `classic-parity.md` §2 records the final13
ceilings and the merge/freshness condition.

## 2. Artifact identity

| File | SHA-256 |
|---|---|
| `specs/004-quality-renderer/classic-parity.md` | `D4922A825E33AE550BA2FD4A42B1CACFF2A5E6AC5D64BE3B8C5CE413025C7804` |
| `specs/004-quality-renderer/engine-integration.md` | `B9F7E3F498BF100EAF7FD879629801FDC0AE4B5D54C53E268C578F2577FFBA2F` |
| `specs/004-quality-renderer/motion-fixtures.md` | `12EBB7A8035F09F0620066847328097A05CD652A7593C9E58AA6AADB626D7C81` |
| `tests/renderer/motion-fixtures.md` | `12EBB7A8035F09F0620066847328097A05CD652A7593C9E58AA6AADB626D7C81` |
| `specs/004-quality-renderer/tools/motion_fixtures.py` | `E1E7B2E7B8522624580DDB4D2860B5573D00A6D4F94591031F723A239F50F870` |
| `specs/004-quality-renderer/tools/verify-q2-design.py` | `D4A09B54AC7E365B256F7623413D324617914398AC73FDE1F0FBFD0C86D9616D` |
| `specs/004-quality-renderer/review-q2-design.md` | `35CDDB5DD72144840FFF4A35599F0E25CCDB93933EBFDD2A2C9295C82FE262A5` |

Hashes identify this change set. They will change if the documents are edited; recompute before
using them to identify a different revision.

## 3. Structural and anchor check

Command:

```powershell
py -3 -I specs/004-quality-renderer/tools/verify-q2-design.py
```

Result: exit 0.

```text
classic-parity.md: 12 tables, 71 anchors
engine-integration.md: 11 tables, 62 anchors
motion-fixtures.md: 9 tables, 21 anchors
evidence-q2-design.md: 7 tables, 1 anchors
review-q2-design.md: 5 tables, 3 anchors
specs\004-quality-renderer\motion-fixtures.md: PASS: 20 fixture rows and 10 validity rows match the generator
tests\renderer\motion-fixtures.md: PASS: 20 fixture rows and 10 validity rows match the generator
PASS: Q2 design documents are structurally consistent, anchors resolve, the T041 mirror matches, and motion vectors match
```

The checker verifies: required sections/IDs (CP-01–CP-30, T-01–T-10), Markdown table continuity and
cell counts, relative links, no drive-letter paths, every `file:line` anchor resolves to an existing
file and an in-range line, and the motion table matches the generator. **It does not prove that a
cited line supports the sentence**; that is the reviewer's job. The adversarial review log (`review-q2-design.md`) records Rounds 1–8, including the post-Q1a and final13 checks.

Limit: the anchor check validates file/line existence, not semantic support.

## 4. Motion generator checks

```powershell
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --check specs/004-quality-renderer/motion-fixtures.md
py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --self-test
```

Results:

```text
PASS: 20 fixture rows and 10 validity rows match the generator
PASS: 5 numeric mutations and the zero-valid contract are detected; M7 (depth-based viewmodel
history) and M8 (subview commit) are T043/T046 assertions
```

The numeric mutations are: state swap (sign), bottom-left origin, jitter inclusion, current-pose-as-
previous, and simulation-tick history. M6 is the zero-valid contract. M7/M8 are explicitly deferred,
not claimed as passed.

The generator uses the engine's 16:9 `CalcFov` pair (`fov_x=106.260205°`, `fov_y=73.739795°`,
`tan_x=4/3`, `tan_y=3/4`), not a generic 90° horizontal fov. `RG16F` is printed as a 16-bit word.

## 5. Existing RHI-contract validator

```powershell
py -3 -I scripts/validate-rhi-contract.py
py -3 -I scripts/validate-rhi-contract.py --self-test
py -3 -I scripts/test-validate-rhi-contract.py
py -3 -I scripts/test-validate-rhi-contract-mutations.py
```

Results:

- Live `validate-rhi-contract.py`: PASS; **header scan RUN over 1 header and 3 public test files**
  (Q1a is now merged, so the previous "NOT RUN" state is obsolete).
- `validate-rhi-contract.py --self-test`: PASS, 7 files/template/matrix/tiers/portable surface; 16
  self-tests pass. The structure/lexical note remains: independent semantic review and target
  execution are not proven by this check.
- `test-validate-rhi-contract.py`: 38 tests, `OK`.
- `test-validate-rhi-contract-mutations.py`: 12 mutants rejected; restored suite 38 tests, exit 0.

This confirms the S4 documents did not break the S0 validator and that the merged Q1a public surface
still passes the live header/test scan. It does not validate the S4 documents themselves (the S0
validator's owned-file list does not include them).

Historical note: `evidence-rhi-contract.md` still contains a dated S0 row saying
`src/renderer/rhi` and `tests/renderer` are absent. That was true for the `final6` revision and is
superseded by Q1a/PR #18; the S0 historical row is not rewritten here.

## 6. Whitespace and repository checks

```powershell
git diff --check
foreach ($f in <new files>) { git diff --no-index --check -- /dev/null $f }
```

Result: `git diff --check` produced no output (exit 0). The no-index checks exited 1 because the files
are new; they produced no whitespace diagnostics (only the CRLF-normalization warning). No tracked
file was modified by S4.

Submodule status: not re-run for this evidence; the S4 documents do not edit the submodule.

## 6.1 Q1a merged: live build and CTest

Q1a is merged on `main` (PR #18; `src/renderer/rhi/`, `src/renderer/d3d12/` and `tests/renderer/`
exist). This is what released the T041 delivery path. Independent verification on this host:

| Step | Command | Result |
|---|---|---|
| Configure | VS 2022 CMake, `-S tests/renderer -B build-q2-design/q1a-verify -G "Visual Studio 17 2022" -A x64` | exit 0; Windows SDK 10.0.26100.0; MSVC 19.44.35228.0 |
| Build Debug | `cmake --build ... --config Debug -- -m` | exit 0 |
| Build Release | `cmake --build ... --config Release -- -m` | exit 0 |
| CTest Debug | `ctest --test-dir ... -C Debug -V` | **7/7 passed, 0 failed**, exit 0 |
| CTest Release | `ctest --test-dir ... -C Release -V` | **7/7 passed, 0 failed**, exit 0 |

The diagnostics test reports the debug layer and GPU-based validation as `SKIPPED`
(`D3D12GetDebugInterface` `hr=0x887A002D`) on this host. That is not converted into a pass; it is
recorded as a skipped capability. The Q1a test project is standalone and is **not** linked into
`DOOM3.exe`/`DOOM3XP.exe`; this is not engine integration.

T041 delivery: `tests/renderer/motion-fixtures.md` is a byte-identical copy of
`specs/004-quality-renderer/motion-fixtures.md`; `verify-q2-design.py` checks both copies and the
generator table. The vectors did not change when the file moved.

Logs (git-ignored): `build-q2-design/q1a-verify/configure.log`, `build-debug.log`,
`build-release.log`, `ctest-debug.log`, `ctest-release.log`.

## 7. Synthetic CMake linkage probe

Location: `build-q2-design/cmake-probe/` (git-ignored). It is a synthetic project, **not the engine**.

| Step | Command | Result |
|---|---|---|
| Configure | VS 2022 bundled CMake, `-G "Visual Studio 17 2022" -A x64` | exit 0 |
| Build Release | `cmake --build ... --config Release -- -m` | exit 0 |
| Build Debug | `cmake --build ... --config Debug -- -m` | exit 0 |
| Run | `probe_a.exe`, `probe_b.exe` (both configs) | each prints `43`, exit 0 |

Generated project inspection:

- `probe_rhi.vcxproj`, `probe_core.vcxproj`, `probe_a.vcxproj`, `probe_b.vcxproj` all show
  `MultiThreadedDebug` for Debug and `MultiThreaded` for Release.
- Both `probe_a.vcxproj` and `probe_b.vcxproj` list `probe_rhi.lib` in `AdditionalDependencies`.
- Both executables link and run the same object-library code plus the static library.

What this proves: the CMake shape (`CMAKE_MSVC_RUNTIME_LIBRARY`, static library, object library via
`$<TARGET_OBJECTS:>`, explicit link to both executables) works in a synthetic project.

What this alone does **not** prove: that the real target CMake configures, that the engine links, or
that `/MT` survives the real engine dependency set. §7.1 closes the build-wiring part of that gap;
T035/T036/T083 still own the real adapter and run.

## 7.1 Target-structure CMake probe (real engine sources)

Location: `build-q2-design/target-wiring/` (git-ignored). It is a **copy** of
`targets/win11-amd64/CMakeLists.txt` with the proposed insertion points, `NEO_DIR` set to the existing
target copy, and a **stub** `stub_engine_scene_adapter.cpp` that calls
`doom3::rhi::ToString(doom3::rhi::Status::Ok)`. No tracked file was modified.

| Step | Result |
|---|---|
| Configure | exit 0 (VS 2022, Windows SDK 10.0.26100.0, MSVC 19.44.35228.0) |
| Build `Release` | exit 0; `DOOM3.exe` and `DOOM3XP.exe` produced |
| Build `Debug` | exit 0; both executables produced |
| `DOOM3.exe` headers | `8664 machine (x64)` (both configs) |
| `/MT` / `/MTd` | `MultiThreaded` / `MultiThreadedDebug` in both `.vcxproj` files |
| Link line | `doom3_rhi.lib;d3d12.lib;dxgi.lib` in both executables |
| Dependents | `d3d12.dll`, `dxgi.dll`, `KERNEL32.dll`; no `MSVCP140.dll`/`VCRUNTIME140.dll` |
| First attempt | **failed C2589** at `src/renderer/d3d12/rhi_d3d12.cpp:34` because the `max` macro broke `std::numeric_limits<uint64_t>::max()`; added `WIN32_LEAN_AND_MEAN NOMINMAX` to `doom3_rhi` and rebuilt green |

The stub is intentionally referenced by the engine object set, so the linker pulls `doom3_rhi` and
the executable shows the D3D12/DXGI imports. This proves the **build wiring**, static-library
linkage, both-executable coverage and CRT propagation against the real engine source tree.

It does **not** prove the real adapter (the probe uses a stub), that any engine call site selects the
RHI, that the game starts, or that a D3D12 frame renders. Those remain T035/T036/T083.

Logs: `build-q2-design/target-wiring/build/{configure,build-release,build-release2,build-debug}.log`.

## 8. Visual review pack (Playwright)

The three design documents and the T041 delivery copy were rendered to a static HTML review pack
(`build-q2-design/html/`, git-ignored; renderer `build-q2-design/render_docs.py`) and served on
`127.0.0.1:8791`. Playwright produced full-page screenshots and checked the DOM at **1024, 1440 and
1920 CSS px**:

| Page | Tables | 1024 overflow | 1440 overflow | 1920 overflow | Table overflow |
|---|---:|---|---|---|---|
| `index.html` | 1 | false | false | false | none |
| `classic-parity.html` | 12 | false | false | false | none |
| `engine-integration.html` | 11 | false | false | false | none |
| `motion-fixtures.html` | 9 | false | false | false | none |
| `motion-fixtures-tests.html` | 9 | false | false | false | none |

Screenshots (git-ignored, worktree-local):

- `build-q2-design/screenshots/index.png` (1440×1000)
- `build-q2-design/screenshots/classic-parity.png` (1440×13108)
- `build-q2-design/screenshots/engine-integration.png` (1440×13849)
- `build-q2-design/screenshots/motion-fixtures.png` (1440×10360)
- `build-q2-design/screenshots/motion-fixtures-tests.png` (1440×10360)
- viewport checks: `classic-parity-1024.png` (1024×1000), `motion-fixtures-tests-1920.png` (1920×1080)
- top crops: `classic-parity-top.png`, `motion-fixtures-top.png`

Console: 0 errors/warnings except a `favicon.ico` 404. The visual check covers layout/overflow; it
does not prove semantic correctness.

## 9. Deviations and limits

| Item | Status / reason |
|---|---|
| `motion-fixtures.md` path | **Resolved**: Q1a merged, so `tests/renderer/motion-fixtures.md` now holds the byte-identical T041 delivery copy; the spec copy is the design mirror. The C++ consumer tests (T042/T044/T046) and T048 are still pending |
| Q0 category thresholds | **Fixed** from `baseline-final13`: corridor 0.5/0.5, open-view 2.0/2.0, close-character 0.5/0.5, moving-light 3.0/5.0; HUD/ROI controls (T-05/06/08/09) remain pending |
| Engine build/run | **Build wiring probed** with the real engine source tree and a stub adapter (both EXEs, `/MT`/`/MTd`, RHI linked); **real adapter and game run not done**; T035/T036/T083 |
| Q1/Q1a/T088 | **Q1a** built and CTest 7/7 (Debug/Release); **Q1b/Q1 and the Q7a encoder leg not run**; the Q7a lexical/structural gate and browser spike executed in issue #35 (`evidence-q7a.md`); Q2 remains blocked |
| T037/T038 fixtures | **Not authored**; the matrix rows are `NOT RUN` |
| S4 task completion | **T032/T033/T041 are ticked** after the Round 8 PASS; programme checkbox count is **22/88** after the Q0/Q1a and S4 tasks landed. |
| Q1a tracker drift | **Resolved**: PR #30 checked T012–T021 on `main`. |
| Q0 tracker drift | **Resolved**: PR #31 merged Q0 and checked T006–T011. |
| Independent human review | **None**; the adversarial reviews are AI, recorded in `review-q2-design.md` |
| Delivery | S4 files are committed on branch `docs/s4-q2-design-final13`; **no PR** was requested |
| Game data | None read or stored; Q0 identity is cited only |

## 10. Verdict

The documents are structurally consistent, their source anchors resolve, the motion vectors
recompute with predeclared tolerances, the mutations are detected, the live RHI contract/header gate
passes, Q1a builds and passes 7/7 CTest in Debug and Release, and the proposed CMake linkage shape
builds both executables against the real engine source tree (stub adapter).

T032, T033 and T041 are **ticked as design deliverables** after the Round 8 independent PASS
(22/88 checked overall), but that is not execution acceptance. Q2 fixtures/new-path captures, HUD/ROI
controls and a human sign-off do not exist; T033's real adapter/selector/run do not exist; T041's
C++ consumer tests and T048 have not run.
