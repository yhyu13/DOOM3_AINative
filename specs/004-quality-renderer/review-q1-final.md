# Q1 final independent review (T031)

**Verdict: BLOCKED — one remaining Q1 defect: the diagnostics fixture treats a requested-but-skipped
debug layer as active, so the Q1 suite is red on any host without `d3d12SDKLayers.dll`.**

Reviewer: an independent agent reviewer, separate session and worktree from the Q1 implementers. I read
the Q1 evidence, the prior reviews and the harness sources; I did not author Q1, T030, the waivers or the
harness. I added only this file. Reviewed `origin/main` at `b7c1a68`.

## What I ran (this host)

Host: Windows 11 amd64; MSVC 19.44 (VS 2022 Professional 17.14); VS-bundled CMake 3.31.6, generator
`Visual Studio 17 2022`, `-A x64`; Python 3.14.4. **`d3d12SDKLayers.dll` is absent** from
`C:\Windows\System32` and `C:\Windows\SysWOW64` (`D3D12Core.dll` is present), so `D3D12GetDebugInterface`
returns `0x887A002D`, the documented "layer not installed" condition.

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build -G "Visual Studio 17 2022" -A x64` | 0 | configured |
| `cmake --build build --config Debug -- -m` | 0 | all fixtures linked |
| `ctest --test-dir build -C Debug --no-tests=error` | **8** | **37/38 passed; `rhi.q1a.diagnostics` FAILED** |
| `build/Debug/q1a_diagnostics_test.exe` | **1** | `cases_pass=1 fail=7 skip=0 validation_executed=0 layer_active_cases=7 clear_820_total=0 clear_820_expected=28` |
| `build/Debug/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `cmake --build build --config Release -- -m` | 0 | linked |
| `ctest --test-dir build -C Release --no-tests=error` | **8** | same failure, 37/38 |
| `build/Release/q1a_diagnostics_test.exe` | **-1** | same `ASSERTION FAILED` in 7 cases |
| `py -3 -I scripts/validate-q1b-fixtures.py` | 0 | `PASS q1b-fixtures.md: pattern, tints, compute, MRT, depth and controls` |
| `py -3 -I scripts/validate-q1b-fixtures.py --self-test` | 0 | `PASS self-test: altered pattern, compute, motion, graph, surface and tint values were rejected` |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | `PASS header gate self-test: engine-facing and name-prefix headers scanned, private harness excluded` |
| `git diff --check` | 0 | clean |

## Blocker B-1 — skipped layer is treated as active

- `tests/renderer/d3d12/rhi_diagnostics_test.cpp:166`:
  `const bool layerActive = gLayerEnabledOnce || requestDebug || requestGpuValidation;`
  This is true when the case *requested* the layer, not when the layer *ran*.
- `:259`: `const uint32_t expectedClear820 = layerActive ? q1a_test::kHeight : 0u;` — so a case whose
  request came back `SKIPPED` still asserts four ID-820 warnings.
- `:378`: `expectedTotal820 = q1a_test::kHeight * gLayerActiveCases` — `gLayerActiveCases` (7) counts the
  skipped cases, so the total expects 28 while nothing was emitted.
- Observed, verbatim, on this host:
  `DIAGNOSTICS debug_detail=D3D12GetDebugInterface failed (hr=0x887A002D)`, `debug=SKIPPED`, then
  `DIAGNOSTICS q1a_clear_820 delta=0 expected=4 layer_active=true`, then
  `ASSERTION FAILED: Q1a row clear emitted 0 ID-820 warning(s), expected 4 (one per cleared row while the debug layer is active)`.
- This contradicts the project's own rule at `specs/004-quality-renderer/evidence-q1.md:52-53`
  ("A software-adapter pass is a functional check only. A missing adapter class is SKIPPED, not PASS."),
  and it regresses the 2026-10-29 record, where the same `0x887A002D` host produced
  `cases_pass=8 fail=0 skip=0` and exit 0.
- Introduced by `test(renderer): scope the ID-820 waiver to the Q1a clear and pin its count (#106)`
  (`2a09c71`, merged `04c6180`), which added the exact-count control without distinguishing
  "requested" from "running".

Fix: derive activity from the result, not the request — for example
`layerActive = requested.debugLayer == CheckState::Run || requested.gpuBasedValidation == CheckState::Run`
(plus `gLayerEnabledOnce` only when a previous case actually ran), count `gLayerActiveCases` only for
those cases, and return `q1a_test::kSkipExitCode` for a case where both requests came back
`CheckState::Skipped`. The exact-count delta assertions and the depth `static_assert` should stay for
genuinely active cases; they are not the defect.

## The waivers themselves

- **W-820** (ID 820, `ClearRenderTargetView` without an optimized clear value): a performance advisory,
  inherent to the Q1a submission that clears a different colour per row; the waiver is phase-scoped, the
  Q1b draw must add zero, and the count is pinned (`clear_820_total == kHeight * layer_active_cases`).
  I inspected the control (`rhi_diagnostics_test.cpp:259-318,378`, `private_test_support.h:33-45`) and
  the mutation controls recorded at `evidence-q1.md:532-551`; the reasoning holds on a host where the
  layer runs.
- **W-1016** (startup notice): informational, emitted when GPU-based validation is enabled; it confirms
  the requested mode. Valid.
- Both waivers name a reason and a revisit point (`evidence-q1.md:467-470`) and were accepted by the
  independent reviews R9 (PR #101) and R11 (PR #106); I confirmed those PRs merged with APPROVED verdicts.
  The blocker above is not a waiver problem: it is the fixture's state logic on a host where the layer
  cannot load.

## T030 / T031 status

- T030's recorded acceptance on the 2026-11-01 Graphics Tools host (`debug=RUN`, `gpu_validation=RUN`,
  `errors=0`, zero unwaived messages, message table at `evidence-q1.md:454-458`) stands **for that host**.
  The three defects it found and fixed (optimized-clear mismatch, in-flight back-buffer release /
  ID-921, diagnostics blind spot) are real and their controls are in the code.
- T031 cannot pass while `rhi.q1a.diagnostics` is red on a host where the same `0x887A002D` condition the
  project already documented occurs. `tasks.md:74` stays unchecked; this review does not tick it.
- No Q1 defect other than B-1 was found: 37 of 38 fixtures pass here, the Q1b private surface fixture
  passes on both adapter classes, and all three validators pass.

## Remaining gaps (recorded, not silently accepted)

- The layer-enabled run is **not reproducible on this review host** (no SDK layer); it rests on the
  recorded 2026-11-01 host, whose logs are git-ignored and whose JUnit output is not committed. That is a
  single recorded host, not a second independent run.
- The Q1 suite's behaviour on layer-absent hosts is the defect above; after the fix, a layer-absent host
  should report the diagnostics case as SKIPPED and the suite should stay green.
- Q1 remains limited to the fixtures and adapters recorded in `evidence-q1.md`; no engine/fork frame,
  game parity, DXR, DLSS or WebGPU claim is made here.
