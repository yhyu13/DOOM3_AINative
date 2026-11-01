# Q1b / Q1 evidence

Status: Q1b implementation and fixtures T022–T029 pass. On 2026-11-01 the
debug-layer and GPU-based-validation gate was re-run on a host where
`d3d12SDKLayers.dll` is installed. It executes: `debug=RUN`,
`gpu_validation=RUN`, `errors=0`, and every remaining message is a documented
waiver (four performance advisories per validation-enabled case and one layer
startup notice). The gate also found and this change fixes three latent defects:
a fixed optimized clear value that did not match the frame's clear, an
in-flight back-buffer release in `ResizeSurface`/`DestroySurface` that the layer
reported as `id=921 CORRUPTION`, and a diagnostics harness that could not see a
process-global layer enabled by an earlier case. **T030 is accepted**: the
unwaived count is zero and both waivers are technically accepted by the
independent review on PR #101 (R9) at `67a108d`, with that verdict's MAJOR-1
implementation objection resolved by the scoped, exact-count control approved by
the independent review on PR #106 (R11) at `2a09c71`. Both waivers and their
review points are recorded below. **T031 remains open**: its earlier review
covers `35ce54e` and does not cover this change; a fresh independent review of
full Q1 is still required. No claim is made for Q1 as a whole, game parity, DXR,
DLSS, WebGPU or a shipped renderer. The 2026-10-29 record is retained below as
history.

The earlier 2026-10-29 status was: Q1b implementation and fixtures T022–T029
executed locally, plus the fault-injected device-removal part of T030; the
debug layer and GPU-based validation were SKIPPED (`0x887A002D`) on that host,
so no zero-unwaived-error result existed then. That record and its owner
acceptance are unchanged; the host-specific reason for the skip did not
reproduce on this machine.

Worktree: `feat/22-q1b-q1-rhi`, rebased onto `origin/main` at
`582b3ff` (the branch base, after T089; `main` has since advanced to
`f1e4feb`), after the earlier `9f103e6` and `29a1e37` bases; the previous
`docs-22-q1b-fixture-contract` checkout at `964d358` was removed and not reused.
One writer, one worktree, one build directory. The post-merge re-run below was
executed in the main worktree at `f1e4feb` with the git-ignored
`build-q1-accept-20261029`.

## Environment (2026-10-29 host)

| Item | Observed |
|---|---|
| OS / shell | Windows 11 amd64; PowerShell 5.1 |
| Compiler | MSVC 19.44 (Visual Studio 2022 Professional 17.14) |
| Windows SDK | 10.0.26100 D3D12 / DXGI headers and libraries |
| Shader build | `d3dcompiler_47` via `D3DCompile`, targets `vs_5_0`, `ps_5_0`, `cs_5_0` |
| CMake | VS-bundled CMake 3.31.6, generator `Visual Studio 17 2022`, `-A x64` |
| Hardware adapter | `Intel(R) Graphics`, vendor `0x8086`, device `0x7D67`, 128 MiB |
| Software adapter | `Microsoft Basic Render Driver`, vendor `0x1414`, device `0x008C` |
| Debug layer | SKIPPED: `D3D12GetDebugInterface failed (hr=0x887A002D)` on both adapters |
| GPU-based validation | SKIPPED: same missing SDK component |

A software-adapter pass is a functional check only. A missing adapter class is
SKIPPED, not PASS.

## What was added

| Area | Files |
|---|---|
| Public seam | `src/renderer/rhi/rhi.h` (shader modules, pipelines, draw frames, compute, buffer/attachment readback, surface, four-slot bound) |
| D3D12 state | `src/renderer/d3d12/d3d12_impl.h`, `rhi_d3d12.cpp` (shared submission record, frame slots, Q1b host boundary, lifecycle) |
| Graphics | `src/renderer/d3d12/rhi_pipeline.cpp` (strictness compile, root layout, PSO, ordered passes, depth sample, per-attachment readback) |
| Compute | `src/renderer/d3d12/rhi_compute.cpp` (compute PSO, structured UAV, dispatch, result copy) |
| Attachments | `src/renderer/d3d12/rhi_attachments.cpp` (RGBA8 colour + D32 depth storage, pitched staging) |
| Frame resources | `src/renderer/d3d12/frame_resources.cpp` (four slots, descriptor heaps, upload storage, slot report, readbacks) |
| Surface | `src/renderer/d3d12/rhi_present.cpp` (window, flip-model chain, compose, resize, minimize/restore, destroy, restart) |
| Portable graph | `src/renderer/rhi/frame_graph.h`, `frame_graph.cpp` (declared accesses, order, invalid-use rejection) |
| Portable motion | `src/renderer/rhi/frame_resources.h` (analytic motion, separate validity, bounded history, composition) |
| Fixtures | `tests/renderer/q1b-fixtures.md`, `q1b_test_support.h`, seven public tests and three D3D12-private tests |
| Validator | `scripts/validate-q1b-fixtures.py` |

No engine source, submodule, target copy, root CMake file, game data or
proprietary SDK is used. The fixtures link only `d3d12`, `dxgi` and
`d3dcompiler`.

## Commands and exits

Fresh worktree-local build directory `build-q1b`:

```text
cmake -S tests/renderer -B build-q1b -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-q1b --config Debug -- -m                            -> 0
ctest --test-dir build-q1b -C Debug --no-tests=error -V --output-on-failure
      --output-junit build-q1b/ctest-junit.xml                          -> 0, 17/17
cmake --build build-q1b --config Release -- -m                          -> 0
ctest --test-dir build-q1b -C Release --no-tests=error -V --output-on-failure
      --output-junit build-q1b/ctest-Release-junit.xml                  -> 0, 17/17
py -3 -I scripts/validate-rhi-contract.py --self-test                   -> 0
py -3 -I scripts/validate-q1b-fixtures.py                               -> 0
py -3 -I scripts/validate-q1b-fixtures.py --self-test                   -> 0
git diff --check                                                        -> 0
```

Retained logs (worktree-local, git-ignored):
`build-q1b/build.log`, `build-q1b/build-Release.log`,
`build-q1b/ctest-Debug.log`, `build-q1b/ctest-Release.log`,
`build-q1b/ctest-junit.xml`, `build-q1b/ctest-Release-junit.xml`,
`build-q1b/validator.log`.

The contract validator reports:

```text
header scan: RUN over 3 header(s) and 11 public test file(s)
PASS self-test: all structural and lexical families
```

The header gate covers `src/renderer/rhi/rhi.h`, `frame_graph.h` and
`frame_resources.h`; the public-test scan covers the Q1a and Q1b public units
and excludes `tests/renderer/d3d12/` by contract.

## Fixture results

Each adapter-looped test ran once per adapter class. `SUMMARY pass=2 fail=0`
means both the Intel and WARP runs executed and passed.

| Test | Hardware | WARP | Execution |
|---|---|---|---|
| `rhi.q1a.clear_readback` | RUN PASS | RUN PASS | clean/readback unchanged from Q1a |
| `rhi.q1a.negative_controls` | RUN PASS | RUN PASS | three public controls detected |
| `rhi.q1a.state_journal` | RUN PASS | RUN PASS | private journal unchanged |
| `rhi.q1a.staging_pitch` | RUN PASS | RUN PASS | private pitch control unchanged |
| `rhi.q1a.outstanding_release` | RUN PASS | RUN PASS | two submissions, in-flight release refused |
| `rhi.q1a.outstanding_release_private` | RUN PASS | RUN PASS | completion value 0 < signal 1 at release |
| `rhi.q1a.diagnostics` | RUN PASS | RUN PASS | opt-in requests attempted; debug/GBV SKIPPED |
| `rhi.q1b.draw` | RUN PASS | RUN PASS | 8×8 pattern at tint 1.0 and 0.5, controls detected, BMP written |
| `rhi.q1b.compute` | RUN PASS | RUN PASS | 4×4 literals, transposed control rejected, zero groups rejected |
| `rhi.q1b.mrt_depth` | RUN PASS | RUN PASS | 3 distinct colours, sampled depth 64/191, 4 BMPs |
| `rhi.q1b.frame_resources` | RUN PASS | RUN PASS | four tints isolated, bound probe recorded, reuse after completion |
| `rhi.q1b.temporal` | RUN PASS (no device) | — | motion 25,0; invalid depth/off-frame; history generations; composition order |
| `rhi.q1b.frame_graph` | RUN PASS (no device) | — | order preserved; read-before-write and malformed declarations rejected |
| `rhi.q1b.surface` | RUN PASS | RUN PASS | 64×48 and 96×64 frames, minimize NotReady, destroy, restart, BMPs |
| `rhi.q1b.frame_resources_private` | RUN PASS | RUN PASS | four distinct slots held on a gate; fifth refused; reuse after release |
| `rhi.q1b.device_removal` | RUN PASS | RUN PASS | injected removal: DeviceLost, refused release, teardown 0 ms, restart renders |
| `rhi.q1b.surface_private` | RUN PASS | RUN PASS | native minimize/restore, NotReady while minimized, window captures written |

The four-slot bound is asserted only by the private gated companion, where
four draws are held on a backend gate. The public test deliberately has no
timing-sensitive fifth submission; it proves per-frame tint isolation and reuse
after completion. The private test also requires an in-flight readback to be
`NotReady`.

### Observed values

- Draw: `(0,0) = 0,0,128,255`; `(7,7) = 224,224,128,255`; tint 0.5 verified at
  every pixel; wrong-tint control rejected.
- Compute: `162,178,130,146,175,191,143,159,168,184,136,152,181,133,149,229`.
- MRT/depth: attachments `10,20,30,255`, `40,50,60,255`, `70,80,90,255`;
  sampled depth `64` for `x<4` and `191` for `x>=4`; cross-attachment and
  depth-disabled controls rejected.
- Motion: previous-minus-current `x=25, y=0, valid=true`; static camera gives a
  valid zero distinct from an invalid sample; depth 0 and −1 and an off-frame
  projection are invalid.
- Slots: private observations `slot=0,1,2,3`, each `in_use=true` while gated;
  fifth submission `NotReady`; slots reusable after gate release.

### Artifacts (git-ignored build tree)

`build-q1b/q1a-evidence/`:

- `q1b-draw-pattern.bmp`
- `q1b-attachment-0.bmp` … `q1b-attachment-3.bmp`
- `q1b-surface-first.bmp`, `q1b-surface-second.bmp`,
  `q1b-surface-resized.bmp`, `q1b-surface-restored.bmp`,
  `q1b-surface-restarted.bmp` (composed 8-bit backbuffer readbacks)
- `q1b-surface-*-window.bmp` (window-level capture written with `PrintWindow`)

The composed readbacks are the authoritative images: each is a tightly packed
copy of the buffer that was shown, and every pixel was compared with the
submitted colour. The window-level captures are supplementary; their pixels are
not asserted because a flip-model window may return a platform-composed copy.

## Red, green and mutation controls

Each control was applied to the tracked source, built, executed, and then the
file was restored from `build-q1b/red/*.bak` and rebuilt.

| Control | Mutation | Observed | Log |
|---|---|---|---|
| Draw red | `SubmitDrawFrame` returns Unsupported before recording | both adapters FAIL at submit; exit 1 | `build-q1b/red/red-draw.log` |
| Compute red | `SubmitCompute` returns Unsupported | both adapters FAIL at submit; exit 1 | `build-q1b/red/red-compute.log` |
| Depth mutation | `DepthEnable = FALSE` in the PSO | depth sample reads the 1.0 clear as `255`, expected `64`; exit 1 | `build-q1b/red/red-depth.log` |
| Slot mutation | Ignore `slot.inUse` during acquisition | all four submissions reuse slot 0; private assertion fails; exit 1 | `build-q1b/red/red-slot.log` |
| Motion mutation | Return an invalid sample from `ProjectMotion` | analytic `25,0` becomes `0,0,invalid`; exit 1 | `build-q1b/red/red-temporal.log` |
| Graph mutation | Accept a read before any write | read-before-write control fails; exit 1 | `build-q1b/red/red-graph.log` |
| Surface red | `SubmitSurfaceFrame` returns Unsupported | first composed frame fails to submit; exit 1 | `build-q1b/red/red-surface.log` |

The restored build then passed 17/17 in Debug and Release. The negative
controls inside the fixtures (adjacent pixel, wrong tint, transposed compute,
cross-attachment, depth-disabled, duplicate write, minimized surface) were also
observed to fail the intended comparison; their assertions are in the CTest log.

## Review fixes

A second, read-only audit of the new backend and public seam produced findings
that were fixed before this evidence was finalised:

| Finding | Fix |
|---|---|
| A failed `Signal` left a slot at serial 0, allowing an executing allocator to be reset | The slot's real serial is published before `ExecuteCommandLists`; an unproven slot is never reused |
| A failed resize left a null back buffer that the next submit dereferenced | `SubmitSurfaceFrame` checks the buffer and returns an actionable Error |
| A failed `CreateQ1bState` left a Q1b state over released device objects | The state is reset on slot-initialisation failure; `RequireQ1b` also requires `initialized` |
| Two serial allocations per submission burned a fence value | One allocation per submission; `LastSerial` now reports the highest in-use slot serial, not the allocation counter |
| The public test embedded Win32 handles | Minimize/restore and capture moved to the private surface test; the gate now rejects `HWND`/`HDC`/`HBITMAP`/`windows.h` |
| The draw expectation recomputed the shader rule | Replaced by literal RGBA8 tables for all four tints |
| The depth fixture never exercised a failing depth comparison | The `x = 0` fragment writes depth 1.0 and must be discarded; expected values updated |
| A graphics pipeline could be passed to a dispatch unnoticed | The compute test now requires `InvalidArgument` |
| In-flight readback was not tested for the new entry points | The private gated test requires `NotReady` |
| The public fifth-frame probe could pass while the bound was absent | Removed; the private gated test is the bound control |
| `reuseCount` described more than it measured | Renamed to `readAfterWriteCount` |

Remaining open findings: an OOM while creating a gate fence after the record is
inserted can still force a device quarantine (rare, no memory unsafety); view
heaps are per submission; compute sizing is conservative; one uniform block per
pass. These are recorded as Q2 work or accepted limits, not Q1 evidence.

## T089 rebase and T030 re-run

The branch was rebased onto `9f103e6` (T089, PR #42). The only code conflict
was `rhi_d3d12.cpp`: the Q1b refactor moved the backend implementation into
`d3d12_impl.h`, so T089's `AdapterFact` reporting was ported there
(`reportedFacts = VendorId | DeviceId | DedicatedMemory`). T089's header
changes and the Q1b declarations coexist; the Q1a suite still passes.

Post-rebase verification: Debug and Release 17/17; contract and fixture
validators 0; `git diff --check` 0.

T030 was re-run on this machine after the rebase:

```text
DIAGNOSTICS validation=SKIPPED debug=SKIPPED gpu_validation=SKIPPED
DIAGNOSTICS debug_detail=D3D12GetDebugInterface failed (hr=0x887A002D)
DIAGNOSTICS gpu_validation_detail=D3D12GetDebugInterface (GPU-based validation) failed (hr=0x887A002D)
DIAGNOSTICS_SUMMARY cases_pass=8 fail=0 skip=0 validation_executed=0
```

The debug layer and GPU-based validation still do not execute. T030 stays
open; real device removal remains NOT RUN. Therefore T031 is not satisfied,
even though its independent review is complete.

### Debug-layer environment attempt

This host is not elevated, so `Get-WindowsCapability -Online` fails with
"requires elevation"; the Graphics Tools optional feature is not installed and
`d3d12SDKLayers.dll` is absent from `C:\Windows\System32`. A local, untracked
attempt with the DirectX 12 Agility SDK 1.620.1 was made: the application
exported `D3D12SDKVersion`/`D3D12SDKPath`, `D3D12Core.dll` loaded (confirmed by
`GetModuleHandleW`), but `D3D12GetDebugInterface` still returned `0x887A002D`;
`d3d12SDKLayers.dll` was not loaded by the system runtime, and a manual load of
the SDK layers returned `E_NOINTERFACE`. No OS feature was installed and no
downloaded binary was committed.

T030 therefore remains SKIPPED, not passed. A machine with the Graphics Tools
feature (or an elevated install) is required. The exact next step is:

```powershell
# Elevated PowerShell
Add-WindowsCapability -Online -Name Tools.Graphics.DirectX~~~~0.0.1.0
```

Then rerun `rhi.q1a.diagnostics` and the Q1b fixtures. Only a run with
`debug=RUN` and `gpu_validation=RUN` (or a documented waiver) can satisfy
T030/T031.

### WebGPU mapping after the rebase

T089's mapping gate reported 57/57 for the pre-Q1b header. The Q1b public
declarations are not in `webgpu-mapping.md`; `validate-webgpu-mapping.py`
now exits 1 with the new `ShaderModule`, pipeline, draw/compute, `BufferResult`
and `Surface` declarations unmapped. This is a mapping/documentation gap, not
a Q1b functional failure and not a WebGPU claim. A follow-up mapping update
(T089b or the Q7a owner) is required before any Q7/WebGPU statement; merging
this PR without it leaves that gate red.

## Post-merge gate verification on `main` (2026-10-29)

Q1b was merged as `f1e4feb` (PR #39). T030 was re-run on the merged `main`, not
on the pre-merge branch, because a pass on a feature branch says nothing about
what `main` executes.

| Item | Observed |
|---|---|
| Checkout | `main` at `f1e4feb666d24f44b3515d583afe338d3493a0af`; `git status --porcelain` empty |
| Configure | `cmake -S tests/renderer -B build-q1-accept-20261029 -G "Visual Studio 17 2022" -A x64` → 0 |
| Build | `cmake --build build-q1-accept-20261029 --config Release -- -m` → 0 (0 `error`, 0 `warning C` lines) |
| Suite | `ctest --test-dir build-q1-accept-20261029 -C Release --no-tests=error` → 0, 17/17 |
| Diagnostics | `q1a_diagnostics_test.exe` → 0; `debug=SKIPPED`, `gpu_validation=SKIPPED`, `debug_detail=D3D12GetDebugInterface failed (hr=0x887A002D)`, `validation_executed=0`, `cases_pass=8 fail=0 skip=0` |
| Adapters | hardware (`Intel(R) Graphics`) and software (`Microsoft Basic Render Driver`) both return `0x887A002D` |
| Contract validator | `py -3 -I scripts/validate-rhi-contract.py --self-test` → 0 |
| Fixture validator | `py -3 -I scripts/validate-q1b-fixtures.py` → 0 |
| Mapping validator | `py -3 -I scripts/validate-webgpu-mapping.py` → **1**: `INVALID: 62 problem(s); 57/119 declarations mapped` |

`DIAGNOSTICS_CASE ... result=PASS` in that output means the opt-in reporting was
correct; it is not a validation run. The same case reports
`validation_executed=false`. Logs are worktree-local and git-ignored:
`build-q1-accept-20261029/build-Release.log`, `ctest-Release.log`,
`ctest-Release-junit.xml`, `diagnostics.log`.

### Why the environment cannot be fixed from this session

`d3d12SDKLayers.dll` is absent from `C:\Windows\System32`, and the shell is not
elevated (`IsInRole(Administrator)` is false; `Get-WindowsOptionalFeature
-Online` fails with "requires elevation"). The Graphics Tools capability
therefore cannot be installed here. The unblock remains, on an elevated host:

```powershell
Add-WindowsCapability -Online -Name Tools.Graphics.DirectX~~~~0.0.1.0
```

then rerun `rhi.q1a.diagnostics` and the Q1b fixtures and require `debug=RUN`
and `gpu_validation=RUN`.

### Owner acceptance of the residual (2026-10-29)

The repository owner explicitly accepted the residual on 2026-10-29 during the
Q1 gate review, on condition that it is recorded as a residual and not as a
pass.

- Accepted residual: the debug layer and GPU-based validation were never
  executed on this host, so **no zero-unwaived-error result exists for Q1**.
- Not accepted: any statement that Q1 passed, that T030/T031 are satisfied, or
  that validated D3D12 usage underpins S8/WebGPU. Those remain false.
- Scope: the Q1 gate only. It does not waive the red mapping gate below, which
  stays owned by the open issue #36 (T089/T089b).
- Re-open condition: any later Q1, S8 or WebGPU claim that depends on validated
  D3D12 usage must re-run the gate on a host with Graphics Tools installed.

### Mapping gate on merged `main`

`validate-webgpu-mapping.py` exits 1 on `f1e4feb`: 62 unmapped declarations,
`57/119` mapped, including `ShaderModule`, `Pipeline`, `Surface`,
`GraphicsPipelineDesc` and their fields. This is the same gap named above; the
merge landed the declarations without the mapping table, so the gate is red on
`main`, not only on the branch. Issue #36 (T089) remains open and owns the
reconciliation; no new issue is opened for it. Until it lands, no Q7/S8
statement may cite the mapping gate as green.

## Fixture document validation

`tests/renderer/q1b-fixtures.md` is the T022 worked-expectation document. It
predeclares: exact program text and tool targets, the 8×8 pattern table, four
tint variants, the 16 compute literals, MRT colours, the depth byte conversion,
tolerance, buffer shape, ordering, negative controls, and the NOT RUN rules for
missing tools and adapters.

`scripts/validate-q1b-fixtures.py` parses the P/C/D expectation tables and the
M/G/S worked tables, compares them with independent literals held in the script,
and checks the Fixture F tint sentence. Both the normal run and `--self-test`
exit 0; the self-test alters a pattern byte and a compute value in memory and
requires rejection. It now also mutates the M motion, G order, S resize and F
tint values and requires each to fail. An altered tracked document fails the
first command.

## Task status

| Task | Status | Note |
|---|---|---|
| T022 | PASS | Fixtures document and validator exit 0; altered values fail |
| T023 | PASS | Draw/pipeline red, green and mutation recorded |
| T024 | PASS | Compute/UAV red, green and mutation recorded |
| T025 | PASS | MRT and sampled depth red, green and mutation recorded |
| T026 | PASS | Frame slots, descriptor and upload ownership; private gated bound |
| T027 | PASS | Motion/validity, history reset and composition order |
| T028 | PASS | Window surface, resize, minimize/restore, destroy, restart |
| T029 | PASS | Declared accesses, pass order and invalid uses |
| T030 | PASS | All fixtures pass. On 2026-11-01 the debug layer and GPU-based validation executed on a Graphics Tools host: `debug=RUN`, `gpu_validation=RUN`, `errors=0`, unwaived messages 0. W-820 and W-1016 are accepted with review points (waiver table above): the independent review on PR #101 (R9) accepted both waivers at `67a108d`, and the independent review on PR #106 (R11) approved the scoped exact-count control at `2a09c71`. The gate also found and the change fixes a Q1b optimized-clear mismatch and an in-flight back-buffer release in surface resize/destroy (`id=921`). Ticked on that acceptance |
| T031 | REVIEW STALE / OPEN | `review-q1.md` covers `35ce54e`; the 2026-11-01 backend and fixture changes require a fresh independent review of this PR's head |

## Limitations and open items

1. **Validation executes on a host with Graphics Tools; T030 is accepted, T031
   is not.** The debug layer and GPU-based validation ran on 2026-11-01 with
   `errors=0` and no unwaived messages; the two waivers are accepted with review
   points, so T030 is ticked. Full Q1 still needs the fresh independent review of
   T031. The 2026-10-29 host could not enable the layer (`0x887A002D`); that
   history is retained above and is not rewritten.
2. **The independent review is stale for this change.** `review-q1.md` reviewed
   `35ce54e`; the 2026-11-01 change modifies the backend and fixtures, so T031
   needs a fresh independent review of this PR's head. The author self-review is
   `review-q1-self.md` and does not replace it.
3. **Shader input is backend-declared program text.** The public seam carries
   opaque bytes, but the C++ fixtures embed a program whose language is a
   D3D12-backend capability. The Q7a mapping gate still has to decide the
   portable shader-module format; this is not a portability result.
4. **The surface is a native window.** The public seam exposes no native handle,
   but the browser profile (Q7) needs its own surface implementation and
   evidence.
5. **Window-level capture is supplementary and private.** `PrintWindow` wrote a
   BMP on this machine, but its pixels are not asserted; the composed backbuffer
   readback is the evidence. Native handles and the capture live in the
   D3D12-private test, and the public-test gate now rejects Win32 vocabulary.
6. **The four-slot bound is covered privately.** The public test no longer
   probes a fifth frame because tiny frames can finish first. The private gated
   test is the deterministic control for the bound and for in-flight readback.
7. **No engine integration.** Q1b remains a synthetic seam; Q2 consumes it in a
   separate branch and issue.
8. **One uniform per pass.** Constants are a single 256-byte block; a real
   renderer will need bounded descriptor/upload growth, which is Q2+ work.
9. **Issue #22 scope.** Issue #22 lists T023–T031 implementation as out of
   scope. The S5 dispatch authorized the slice; this PR is a partial slice under
   `Refs #22`, not issue closure, and T031 remains open.
10. **Bounded error-path record retention.** If `Signal` or the display call
    fails after a record is inserted, the record is retained until teardown.
    The slot is not reused, so this is a bounded leak, not memory unsafety.

## T030 validation re-run on a Graphics Tools host (2026-11-01)

Worktree `spec-004-t030-q1`, rebased onto `origin/main` at `b244458` (T072)
after the first gate run; the gate below was re-run on the rebased head. One
writer, one build directory `build-q1-t030` (git-ignored). This host has
`d3d12SDKLayers.dll`, so the recorded `0x887A002D` skip does not reproduce.

### Host and layer

| Item | Observed |
|---|---|
| OS | Windows 10 Pro 10.0.19045 |
| Hardware adapter | `AMD Radeon RX 9070 XT`, vendor `0x1002`, device `0x7550`, 17,095,983,104 bytes |
| Hardware driver | `32.0.21030.31`, dated 2025-09-02 |
| Software adapter | `Microsoft Basic Render Driver`, vendor `0x1414`, device `0x008C` |
| Debug layer / core | `d3d12SDKLayers.dll` and `D3D12Core.dll` 10.0.19041.5794 |
| Compiler | MSVC 19.38.33145 (Visual Studio 2022 Community) |
| Windows SDK | 10.0.26100.0, targeting 10.0.19045 |
| CMake | 4.1.2, generator `Visual Studio 17 2022`, `-A x64` |
| Python (validators) | 3.13.12 |

### Commands and exits

All commands ran from the worktree root. Logs are retained under the
git-ignored `build-q1-t030/`.

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build-q1-t030 -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.38.33145, SDK 10.0.26100 |
| `cmake --build build-q1-t030 --config Debug -- -m` | 0 | all fixtures linked |
| `ctest --test-dir build-q1-t030 -C Debug --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 38` (32 Q1–Q4 fixtures plus 6 T072 portable-profile tests) |
| `build-q1-t030/Debug/q1a_diagnostics_test.exe` | 0 | `cases_pass=8 fail=0 skip=0 validation_executed=6` |
| `build-q1-t030/Debug/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `cmake --build build-q1-t030 --config Release -- -m` | 0 | all fixtures linked |
| `ctest --test-dir build-q1-t030 -C Release --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 38` (same split) |
| `build-q1-t030/Release/q1a_diagnostics_test.exe` | 0 | `cases_pass=8 fail=0 skip=0 validation_executed=6` |
| `build-q1-t030/Release/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `python -I scripts/validate-q1b-fixtures.py` | 0 | `PASS q1b-fixtures.md: pattern, tints, compute, MRT, depth and controls` |
| `python -I scripts/validate-q1b-fixtures.py --self-test` | 0 | altered values rejected |
| `python -I scripts/validate-rhi-contract.py --self-test` | **1** | 26 pre-existing forbidden-vocabulary failures in `src/renderer/q4_harness/*.h`; unchanged by this change |
| `git diff --check` | 0 | no whitespace error |

The contract-validator failure is the gap already recorded in
`evidence-doc-cluster.md` §3.1; this change touches no `q4_harness` file.

### Messages after the fixes

Debug and Release produce the same result. Every message is listed with the
numeric ID and severity from `d3d12sdklayers.h`.

| ID | Severity | Text (abridged) | Trigger | Final count | Decision |
|---|---|---|---|---|---|
| 820 | WARNING | `ID3D12CommandList::ClearRenderTargetView: The application did not pass any clear value to resource creation. ... typically slower ... will still clear to the desired value.` | `ClearRenderTargetView` in the Q1a row clear (`d3d12_impl.h`), once per row of the 4-row fixture | 28 (4 per case that ran under the layer, 7 cases) | Waived — W-820 |
| 1016 | MESSAGE | `Device Debug Layer Startup Options: GPU-Based Validation is enabled (disabled by default). ...` | layer device creation when GPU-based validation is on | 6 | Waived — W-1016 |

No `ERROR` or `CORRUPTION` message remains; `errors=0` in every case.

### Waivers

Waiving is not silence: the test logs every message with its ID and severity
before the waiver predicate is consulted, and the gate fails on any message not
in this table.

| Waiver | Reason | Accepted by | Revisit |
|---|---|---|---|
| W-820 | The Q1a submission clears a different colour per row, so no single optimized clear value can match all four `ClearRenderTargetView` calls. The message is a performance advisory — the clear still writes the requested value — and the fixture asserts the exact bytes. The gate is scoped to this phase and asserts the exact count (2026-11-01 follow-up). | Independent review R9 on PR #101 (session `ses_e6a3c642fffeIQbRkvuSgXssSX`) at `67a108d` accepted the waiver reason. That verdict's MAJOR-1 (the waiver was keyed on ID and severity only) was fixed by PR #106, whose independent review R11 (session `ses_e69f1eb07ffe0qBQJTggaWlzKs`) approved the scoped exact-count control at `2a09c71` (merged `04c6180`). | Remove or re-scope if Q1a stops clearing per row; the diagnostics gate pins the per-phase count on every run; re-check on any layer or driver update that changes the ID, severity or text |
| W-1016 | Informational layer notice emitted at device creation whenever GPU-based validation is enabled; it confirms the requested mode is active. | Independent review R9 on PR #101 (session `ses_e6a3c642fffeIQbRkvuSgXssSX`) at `67a108d` accepted the notice as an informational, mode-confirming message; independent review R11 on PR #106 re-observed it at `2a09c71`. | Remove/rephrase if the layer stops emitting it or changes its severity |

### Defects found and fixed by the gate

1. **Mismatching optimized clear value (Q1b).** `CreateColorAttachment` and
   `CreateDepthAttachment` declared fixed clear values while the frame chose its
   own. `SubmitDrawFrame` now derives each attachment's first clear and
   `DrawFrameDesc::depthClear` and passes them to creation. The Q1b draw and
   depth paths produce no ID-820 message.
2. **In-flight back-buffer release on resize (Q1b surface).** The debug layer
   reported `id=921 CORRUPTION: An ID3D12Resource ... is referenced by GPU
   operations in-flight ...`, and `ResizeBuffers` then terminated the process
   with `0x87D`. `LastSerial()` is zero after a submission is released, so the
   previous wait did not drain the queue. `ResizeSurface`/`DestroySurface` now
   call `DrainQueue`, which signals a fresh fence value and waits, and the
   resize path resets and closes each command list to drop recorded references.
   The surface fixture now passes under debug + GPU-based validation on both
   adapters with no ID-921 message.
3. **Blind spot in the diagnostics harness.** Messages were read only when the
   case explicitly requested the debug layer; the layer is process-global, so a
   later `none` case ran under a layer enabled by an earlier case without being
   gated. The test now reads and gates the queue for every case, captures
   numeric IDs and severities, asserts the capture was not truncated, and fails
   on any message outside the waiver table.

### Regression control for the ID-820 waiver (MAJOR-1 follow-up, 2026-11-01)

PR #101's post-merge review recorded **MAJOR-1**: the ID-820 waiver was keyed
only on ID and severity, so a Q1b colour-attachment regression would emit the
same warning and be waived silently. The gate now attributes the warning to the
phase that produced it and pins its count.

Rule, in `tests/renderer/d3d12/private_test_support.h` and
`tests/renderer/d3d12/rhi_diagnostics_test.cpp`:

- The info queue is cumulative: `UpdateDiagnostics` reads every stored message
  and never clears it. The gate compares two reads taken around one operation,
  so its expectation does not depend on the process-global layer or on messages
  inherited from an earlier case.
- **Q1a row clear:** the ID-820 delta must equal `q1a_test::kHeight` (4) in a
  case that runs under the layer, and `0` in the one case that runs before the
  layer is enabled. Observed: `0` for hardware `none`, `4` for the other seven.
- **Q1b draw:** the ID-820 delta and ID-821 delta across the draw must both be
  `0`. The cumulative budget passed to the post-draw gate stays at the Q1a
  count, so a Q1b 820 is unwaived even if the delta check were bypassed.
- **Total:** `clear_820_total == kHeight * layer_active_cases`, which reproduces
  the evidence's 28 = 7 × 4 without hard-coding either constant.

Why this is the safer of the two suggested rules: an exact count alone pins the
total but cannot attribute a warning to an attachment, and scoping the waiver by
label alone still needs a count to notice a change in how many rows are cleared.
The delta assertions do both: they prove the Q1b draw adds none, and they pin
the Q1a phase to one warning per cleared row. A hand-passed phase label could be
wrong; an exact delta cannot.

Depth half: `frame.depthClear` is now the named constant
`kQ1bDepthClear = 0.5f`, with a `static_assert` that it differs from the pre-fix
creation value `1.0f`, and the draw asserts `821_delta == 0` with a message that
names the value and the pre-fix default. The control is still layer-dependent by
construction, but it is no longer incidental: the assertion states the value it
protects, and it cannot become vacuous without the `static_assert` failing.

Mutation controls, both run in worktree `spec-004-waiver-control` (build
directory `build-waiver`, git-ignored) and reverted before commit; no mutated
source was committed:

| Mutation | Command | Exit | Key line (verbatim) |
|---|---|---|---|
| Colour: `CreateColorAttachment` reverted to the pre-fix zeroed clear value | `build-waiver/Debug/q1a_diagnostics_test.exe` | 1 | `ASSERTION FAILED: Q1b draw added 1 ID-820 warning(s); the waiver is scoped to the Q1a row clear, so a colour attachment created without the first pass's clear value is a failure` |
| Depth: `CreateDepthAttachment` reverted to the fixed `1.0f` creation value | `build-waiver/Debug/q1a_diagnostics_test.exe` | 1 | `ASSERTION FAILED: Q1b draw added 1 ID-821 warning(s); the depth attachment must be created with DrawFrameDesc::depthClear = 0.500000 (the pre-fix creation value was 1.0f)` |

Both red runs also logged `UNWAIVED after message[4] id=820` (colour) and
`id=821` (depth) at severity 2 with the layer's mismatching-clear text, so the
cumulative gate fired independently of the delta assertion. The mutated
expressions were: colour, `const D3D12_CLEAR_VALUE* optimized = &clearValue;`
with a zeroed `clearValue` instead of `nullptr` plus the frame's first clear;
depth, `clearValue.DepthStencil.Depth = 1.0f;` instead of `optimizedDepth`.

Restoring the source and forcing a rebuild returns both binaries to exit 0 with
28/28. A rebuild is required: restoring a saved copy preserves its older
timestamp, so MSBuild can keep the mutated object linked and report a false
failure — touch the restored file or delete its object before rebuilding.

Green re-run after the control, same build directory:

| Command | Exit | Observed |
|---|---|---|
| `build-waiver/Debug/q1a_diagnostics_test.exe` | 0 | `cases_pass=8 fail=0 skip=0 validation_executed=6 layer_active_cases=7 clear_820_total=28 clear_820_expected=28` |
| `build-waiver/Debug/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `ctest --test-dir build-waiver -C Debug --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 38` |
| `build-waiver/Release/q1a_diagnostics_test.exe` | 0 | same summary as Debug |
| `build-waiver/Release/q1b_surface_private.exe` | 0 | `PRIVATE_SUMMARY pass=2 fail=0 skip=0` |
| `ctest --test-dir build-waiver -C Release --no-tests=error --output-on-failure` | 0 | `100% tests passed, 0 tests failed out of 38` |

The surface fixture now passes an explicit zero ID-820 budget: its back buffer
is created by DXGI and the observed queue contains only the ID-1016 startup
notice, so any ID-820 there is a new unwaived message rather than an inherited
allowance.

This was a test-gate change only. No engine source, submodule, target copy or
game data was touched. Its independent review (R11 at `2a09c71`) approved the
control; the waiver acceptance and the T030 tick are recorded in the ledger
below.

Open findings from that review, recorded so the accepted state is not read as
"every gate is now ideal":

- **Portability is fail-closed, not proven.** The expected count follows the
  case's request plus the process-global layer flag, not the layer's reported
  state. On a host where the layer cannot be enabled, a requested case would
  fail the `expected=4` assertion instead of taking the documented skip path
  (`AllowedWhenRequested` accepts `Skipped`). This opens no waiver hole, but it
  is a behaviour change relative to the old gate that has only been exercised
  on a Graphics Tools host.
- **W-1016 is still an unbounded boolean.** `WaiverBudget::allowStartupOptions1016`
  waives every severity-MESSAGE ID-1016 in a report. It is the same shape
  MAJOR-1 objected to for ID 820, at far lower risk because the message is
  emitted once per layer device creation; budgeting it the way ID 820 is now
  budgeted remains a follow-up.

### Coverage and limits of this run

- `rhi.q1a.diagnostics` is the validation vehicle. It now exercises the Q1a
  clear/readback and a Q1b draw with a depth attachment (colour clear, depth
  clear, PSO, draw, readback) in every case, so both clear paths are validated.
- `q1b_surface_private` exercises create, compose, minimize, restore, resize,
  capture, destroy and restart under debug + GPU-based validation.
- The other fixture binaries (draw, compute, MRT/depth, frame resources,
  temporal, frame graph, public surface, device removal, lifetime) ran normally
  in the 38/38 suite on the rebased head (the additional six are T072's
  portable-profile tests, outside Q1); the public seam has no validation opt-in,
  so they did not execute under the layer in this run. This is a coverage
  boundary, not a pass claim for them.
- The debug layer is process-global. The software `none` case inherits it from
  the earlier hardware cases; its request report still says `NOT_REQUESTED`, and
  its queue contains four waived ID-820 warnings plus one ID-1016 message. The
  gate is unconditional, so this inheritance cannot hide a message.
- Real device removal and the layer's device-removal diagnostics remain out of
  scope for this run; the injected-removal fixture is unchanged.

### Task status after this run

T030 is **PASS**. Unwaived errors: 0. Unwaived warnings: 0. W-820 and W-1016
are accepted with review points: the independent review on PR #101 (R9)
accepted both waivers at `67a108d`, and the independent review on PR #106 (R11)
approved the scoped exact-count control at `2a09c71` (merged `04c6180`) while
expressly leaving the ledger decision to the ledger owner. This tick records
that acceptance; it does not claim R11 accepted the waivers. T031 remains open
and needs a fresh, independent review of full Q1; `review-q1.md` covers
`35ce54e`, not this change.
