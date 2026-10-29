# Q1b / Q1 evidence

Status: Q1b implementation and fixtures T022–T029 have been executed locally, plus the fault-injected device-removal part of T030.
**Q1 is not complete.** The debug layer and GPU-based validation cannot be
enabled on this machine (`D3D12GetDebugInterface` returns `0x887A002D`), so both
are recorded as SKIPPED. T031's independent review was performed and is
recorded in `review-q1.md`; T031 remains open because T030's gate is unmet.
The author self-review is `review-q1-self.md`. No claim is
made for Q1, game parity, DXR, DLSS, WebGPU or a shipped renderer.

Worktree: `feat/22-q1b-q1-rhi`, rebased onto `origin/main` at
`29a1e37`; the previous `docs-22-q1b-fixture-contract` checkout at `964d358`
was removed and not reused. One writer, one worktree, one build directory.

## Environment

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
| T030 | PARTIAL | All fixtures ran and fault-injected removal passes; debug layer and GPU-based validation are SKIPPED, so the task stays open |
| T031 | BLOCKED | Independent review performed (`review-q1.md`); remains open because the debug layer and GPU-based validation do not execute |

## Limitations and open items

1. **Full Q1 remains blocked.** The debug layer and GPU-based validation could
   not be enabled (`0x887A002D`). No unwaived-error claim is possible. T030 is
   recorded as executed with those two checks SKIPPED; T031 cannot be satisfied
   by the author.
2. **The independent review is not done.** `review-q1.md` records the author's
   self-review and explicitly does not count as T031.
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
