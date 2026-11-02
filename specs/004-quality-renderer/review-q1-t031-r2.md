# Q1 T031 round-2 independent review

**Verdict: APPROVED.** The fix at `tests/renderer/d3d12/rhi_diagnostics_test.cpp` resolves the
skipped-layer defect: on a host without `d3d12SDKLayers.dll` the layer is reported as SKIPPED, never
as active, and the Q1 suite is green.

Reviewer: an independent agent reviewer, separate session and worktree from the Q1 implementers. I
added only this file; no prior review and no code was edited. Reviewed `origin/main` at `e4ff6e9`
(PR #123, the fix for the finding in `review-q1-final.md`).

## Host and defect context

Host: Windows 11 amd64; MSVC 19.44 (VS 2022 Professional 17.14); VS-bundled CMake 3.31.6, generator
`Visual Studio 17 2022`, `-A x64`; Python 3.14.4. **`d3d12SDKLayers.dll` is absent** from
`C:\Windows\System32` and `C:\Windows\SysWOW64` (`D3D12Core.dll` is present), so
`D3D12GetDebugInterface` returns `0x887A002D`.

The previous review (`review-q1-final.md`, merged at `ac65556`) blocked because a *requested* but
*skipped* layer was treated as active (`layerActive = gLayerEnabledOnce || requestDebug ||
requestGpuValidation`), so seven cases asserted four ID-820 warnings that could not exist and the
suite was 37/38. The fix now derives activity only from a `Run` report
(`rhi_diagnostics_test.cpp:183-195`) and returns the skip exit code when a requested layer came back
`Skipped` (`:357-360`), with `gLayerActiveCases` incremented only for a genuinely active layer
(`:280-281`).

## Commands and exits

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build -G "Visual Studio 17 2022" -A x64` | 0 | configured |
| `cmake --build build --config Release -- -m` | 0 | clean |
| `cmake --build build --config Debug -- -m` | 0 | clean |
| `build/Release/q1a_diagnostics_test.exe` | **0** | `cases_pass=2 fail=0 skip=6 validation_executed=0 layer_active_cases=0 clear_820_total=0 clear_820_expected=0` |
| `build/Debug/q1a_diagnostics_test.exe` | **0** | same summary |
| `ctest --test-dir build -C Release --no-tests=error` | **0** | `100% tests passed, 0 tests failed out of 38` |
| `ctest --test-dir build -C Debug --no-tests=error` | **0** | `100% tests passed, 0 tests failed out of 38` |
| `build/Release/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `py -3 -I scripts/validate-rhi-contract.py` (`--self-test`) | 0 (0) | PASS; 25 headers / 30 public test files |
| `py -3 -I scripts/validate-q1b-fixtures.py` (`--self-test`) | 0 (0) | PASS; altered values rejected |
| `git diff --check` | 0 | clean |

Every diagnostics case now reports honestly: two functional cases pass (`hardware/none` and its
software counterpart), six validation-requesting cases skip with `debug=SKIPPED` /
`gpu_validation=SKIPPED`, `validation_executed=0`, `layer_active_cases=0`, and the ID-820 budget is
`0/0` instead of the impossible `0/28`. The suite is 38/38 in Debug and Release.

## Limits recorded

- **The layer still does not execute on this host.** `d3d12SDKLayers.dll` is not installed, so
  `debug=RUN` and `gpu_validation=RUN` cannot be reproduced here. The only layer-executed evidence
  remains the recorded 2026-11-01 Graphics Tools host in `evidence-q1.md` §T030, with its accepted
  W-820 and W-1016 waivers. This review therefore confirms the *skipped* behaviour and the suite
  honesty; it does not add a second layer-executed run.
- `ctest` reports `rhi.q1a.diagnostics` as **Passed** even though six of its eight cases skipped.
  That is the coarse CTest status: the authoritative result is the fixture summary
  (`pass=2 fail=0 skip=6 validation_executed=0`), which is printed and honest. A stricter option
  would return the skip code from `main()` so CTest shows Skipped; it is not required for this fix.
- T031 in `tasks.md` stays unchecked until this record is merged; this review does not tick it.
- Q1 remains limited to the fixtures and adapters recorded in `evidence-q1.md`; no engine frame,
  parity, DXR, DLSS or WebGPU claim is made.
