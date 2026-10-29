# Implementation Plan: Quality-first rendering

**Branch**: None (planning artifact; implementation branches are created per issue, starting with #11) | **Date**: 2026-10-27 | **Spec**: [spec.md](./spec.md)

**Input**: `specs/004-quality-renderer/spec.md`; evidence and alternatives in [research.md](./research.md).

## Summary

Keep the native classic-data renderer as a reference. Define a WebGPU-subset-oriented portable core plus separately negotiated native extensions, and build a D3D12-first Windows backend in independently testable increments. The same eventual ordered engine adapter targets the core; a future web WebGPU backend is a separate implementation and validation effort. Then add a classic-compatible engine adapter, reliable rendered-frame history and overlay composition, a playable hybrid PBR renderer, and an eventual path-traced showcase/reference. DLSS 5 is a separately authorized optional provider, never a foundation dependency. No exact WebGPU support or browser implementation is claimed.

The first implementation is **Q1a**, a standalone synthetic offscreen clear/readback and completion-token lifetime slice through the portable public RHI seam, with a D3D12-private state-journal and fence hook for its own tests. It does not claim draw/compute/MRT/depth, game integration, validation-layer success, browser support or a completed renderer. Full Q1 and the later visual goals remain separate tasks and gates.

## Technical Context

- **Language/Version**: C++ for RHI/backend/fixtures; HLSL and DXC are candidates for subsequent native programmable rendering and are a D3D12 build detail, not the public RHI shader input and not proof of a browser shader path. Each target discovers and records its authorized shader source, compiler/validator, versions, options and target-specific validation. PowerShell 5.1 is used for Windows orchestration. Match the existing engine's MSVC runtime configuration when linked.
- **Primary Dependencies**: Windows SDK D3D12/DXGI and the installed Visual Studio C++ toolchain. The initial clear/readback slice requires no shader compiler, allocator package, game data or NVIDIA SDK. DXC and a pinned, license-reviewed D3D12 allocator are candidates for later slices; no unreviewed download is required.
- **Storage**: Local source, build tree and transient test results; no game assets or binaries in git. A data identity/manifest may describe permitted local data without containing it.
- **Testing**: Standalone CMake/CTest and synthetic GPU readback first; later engine fixtures, analytic reprojection, visual capture and independent review. Distinguish functional, debug-layer, GPU-based validation, hardware, software and data-dependent results.
- **Target Platform**: Windows 11 x64, D3D12 first. Vulkan, WebGPU and GL/GLES profiles are later targets, not implied by desktop success.
- **Project Type**: Native renderer library/backend, test fixtures and eventually an engine adapter.
- **Performance Goals**: Record frame times, pacing, memory and latency on named hardware before setting review budgets. No unsupported FPS, schedule or quality figure is assumed.
- **Constraints**: Pristine submodule read-only; no game data, proprietary SDK or build output tracked; full Q1 requires actual GPU-based validation; no unsafe resource reuse; evidence scripts are refusal-first (no implicit overwrite or process kill), with flags and exit codes documented in [quickstart.md](./quickstart.md).
- **Scale/Scope**: One direct queue and bounded frames in flight initially. No speculative bindless, multi-queue, async compute, RT, full frame graph or provider implementation in Q1a; those are independently gated later.

### Current local observations, not completion evidence

The installed environment was probed during planning: Visual Studio 2022 and Windows SDK are available, but CMake, `cl` and DXC are not on the ordinary shell PATH. The VS-bundled CMake and SDK DXC are discoverable without downloading them. A capability probe reported D3D12-capable Intel hardware and a software adapter, but did not run a draw, compute dispatch, readback or presentation. `D3D12GetDebugInterface` reported `0x887A002D` (SDK component missing); GPU-based validation has **not** run. These observations are environment-specific and must be re-probed when executing.

The root checkout's existing `build/` cache was found to refer to a different worktree; it is not this feature's fresh baseline and must not be reused. The current target copy is absent, and legally supplied game data was not established. Historical demo captures and later save/demo fixes exist, but neither a fresh deterministic timedemo nor current parity/performance has been established. The verifier exports patches to a temporary directory and fails closed on added/deleted rows of that export; because it generates both patch and expectations from the same target copy, a pass is self-consistency, not canonical-patch provenance (T084); `smoke-win11-amd64.ps1` and `run-win11-amd64.ps1` refuse a non-empty output directory without `-Clean`, and the smoke/run/capture scripts stop only processes whose executable path matches the resolved executable. Their flags and exit codes are in [quickstart.md](./quickstart.md). Do not invoke them blindly.

## Constitution Check

| Gate | Before design | After design / implementation condition |
|---|---|---|
| I. Pristine source | PASS as a design: new fixture code is outside the submodule | PASS only while submodule stays clean; engine changes require a fork branch and reviewed bump, never a scratch-copy-only delivery. |
| II. Evidence | PASS as a design: every stage has a named check and unavailable checks are skipped | Actual status is conditional on fresh commands; capability probes and historical screenshots are not runtime passes. |
| III. Spec, issue, worktree, review | PASS for draft spec/plan/tasks | Each implementation issue needs its own worktree and independent review. Publishing an issue, commit, push or PR is not authorized merely by writing this plan. |
| IV. Legal hygiene | PASS as a design | No game assets, derived packs, SDK binaries, secrets or machine-specific paths; neural distribution has a separate legal gate. |
| V. Fallbacks | PASS as a design | Existing GL path and non-proprietary reconstruction remain available; unsupported target features are explicit. |

There is no approved exception to the constitution. An unavailable issue/worktree, legal data, SDK or validation environment is a blocker or skipped check for the affected stage, not a reason to bypass the gate.

## Project Structure

### Documentation (this feature)

```text
specs/004-quality-renderer/
├── spec.md                 # User outcomes and programme acceptance
├── research.md             # Decision record and evidence boundaries
├── plan.md                 # This plan and stage gates
├── data-model.md           # Frame/resource/history ownership
├── contracts/
│   └── rhi-foundation.md   # Public first seam and growth requirements
├── quickstart.md           # Stage-specific commands and expected evidence
├── evidence-template.md    # Reusable record; not itself execution evidence
├── traceability.md         # Literal FR/SC → task/evidence matrix (T087)
├── checklists/
│   └── requirements.md     # Specification-quality review, not implementation status
└── tasks.md                # Executable, staged implementation tasks
```

### Source and build boundaries

```text
src/renderer/rhi/           # Planned backend-neutral public contract and ownership types
src/renderer/d3d12/         # Planned Windows-only device/resource/submission lowering
tests/renderer/             # Planned standalone CMake project and synthetic fixtures
build-<worktree-local>/     # Ignored, fresh test output; never a shared build cache
targets/win11-amd64/        # Existing native build/patch machinery; unchanged by Q1a
doom3_orig/doom3/           # Pristine read-only engine submodule
```

**Structure decision**: Keep the first executable RHI slice in the superproject and compile it using an isolated test project; it must not trigger root configure-time creation or patching of an engine copy. The later engine adapter is a separate reviewed integration. The current exporter represents modifications but does not reproduce added/deleted target-copy files, so a new RHI file left only in the ignored target copy is not a deliverable. The existing engine object library is consumed through `$<TARGET_OBJECTS:doom3_engine>` by two executables; later integration must explicitly propagate/link RHI and system libraries to both executables or deliberately change that linkage while preserving `/MT`/`/MTd` compatibility.

## Design and Execution Stages

| Gate | Deliverable and check | Status / dependency |
|---|---|---|
| Q0 | Fresh native build identity and four reproducible scene categories; legal-data-dependent captures and counters are SKIPPED if unavailable. Preserve exact code/data/hardware identities. | **Q0 established (2026-10-29)**: all four categories were captured twice with the engine framebuffer screenshot at a fixed game tic (`com_fixedTic 1`) and pass the ceilings frozen before the recorded run (tightened after the fixed-tic probes; final13 also passes the earlier, looser cells) (meanAbs 0.183 / 0.226 / 0.108 / 0.055; diff 0.208% / 0.210% / 0.098% / 0.032%). The OS-window attempts are retained as calibration history. The safe entry point and refusal-first procedure are in `scripts/test-render-baseline.ps1` and `baseline-procedure.md`; see `evidence-q0.md` for the identities. |
| Q1a | Standalone offscreen RGBA8 clear, submission through the portable seam with an opaque completion token, asynchronous readback and repeated frame/resource-lifetime test; independently specified expected pixel bytes with a predeclared tolerance; separate hardware/software/debug outcomes; D3D12-private state journal and fence values asserted only through the backend hook. | First implementation slice. Requires the installed Windows SDK D3D12/DXGI and Visual Studio C++ tools; no engine target copy, game data or proprietary NVIDIA SDK. Passing it is not Q1 and not portability evidence. |
| Q1b | Build programmable draw and compute fixtures, MRT, readable depth, velocity/validity, overlay separation, declared uses and pass dependencies (no public barrier API), frame slots, surface acquire/minimize, resize and history-reset tests. | Incremental slices after Q1a; contract requirements are in [rhi-foundation.md](./contracts/rhi-foundation.md). |
| Q1 complete | Synthetic tests for all Q1b capabilities, safe retirement under stress, actual debug layer and GPU-based validation with zero unwaived errors, and device-removal diagnostics where available. | Blocked until the necessary validation components are available and executed; a skip is not a pass. This gate is D3D12-private. |
| Q7a (pre-Q2) | Header/mapping gate and a pre-Q2 WebGPU conformance spike: every public core field maps to a WebGPU-shaped operation, the lexical scan finds no D3D12 noun in the core headers or public test units, and a **named** encoder implementation (Dawn native, or a pinned browser plus the Emscripten WebGPU library) exercises device, buffer, bind group, one MRT pass, one compute dispatch, 256-byte-pitch `copyTextureToBuffer` plus `mapAsync`, surface configure/acquire and a device-loss path. | **Complete (2026-10-29, issue #35)**: the lexical gate scans 3 headers and 3 public test files with zero hits; the structural mapping gate classifies 57/57 declarations; the pinned-browser spike passes 12 required checks twice; and the Emscripten encoder leg runs the same 12 checks from C++/wasm through the emdawnwebgpu port with the header compiled for wasm32 (`evidence-q7a.md`, `webgpu-mapping.md`, `review-q7a.md`, `review-t088-encoder.md`). T089 reconciled the five-declaration gap with a capability signal. Software adapter, OOM and performance are NOT RUN, and no WebGPU implementation of `doom3::rhi::Device` exists, so this is not evidence that a web profile is shipped. |
| Q2 | Selectable classic-compatible path; staged material/light/shadow/subview/GUI/viewmodel coverage and predeclared scene parity tolerances against GL, including world draw submission, texture/material-stage evaluation, projected/falloff lighting and stencil shadows. | Requires independently reviewed T004 three-tier contract and header gate (Q7a), an engine adapter plus legal data and an independently reviewed fork/engine integration. Unsupported cases remain visible. T080–T082 are the production slices; T083 is the integration check; T037 supplies their tests and T084 guards patch delivery. |
| Q3 | Analytic camera/rigid/skinned reprojection; persistent private pose survives cache deletion/shared subviews; history reset/validity and display HUD ordering; real scene motion/validity wired into frame resources. | Fixture design T041 may be prepared earlier. Q3 execution/review T048 follows T041–T047 and T085 end-to-end wiring, including T043 view isolation, T045 invalidation and T047 overlay ordering; it does not require an optional provider. Synthetic vectors alone do not satisfy Q3. |
| Q4 | Hybrid deferred opaque + forward/compatibility, HDR PBR, shadows, indirect light/reflections/atmosphere, authored legal hero assets, budget/residency and moving-scene art review. | Requires Q2/Q3 and independently recorded acceptance/budgets. Numeric budgets are set from T056 measurements, not guessed in T049. T086's independent reconstruction/off ordering follows T047/T055; its separate provider branch is conditional, not a Q4 prerequisite. Modern look is not pixel parity. |
| Q5 | Optional authorized NR provider: actual backend-specific create/evaluate, documented inputs/masks/reset, on/off captures, SDK/hardware/access and distribution review. | Independent of baseline quality work. Provider-specific T061/T062 require T060's actual clearance and execution; the provider branch of T086 follows T062 only when available. If unavailable, record NOT RUN without blocking the off path or Q4 review; do not manufacture API signatures or bundle binaries. |
| Q6 | DXR path-traced converged static reference and dynamic/skinned showcase, material-aware alpha/emissive transport, denoising, noise/motion/latency and artistic acceptance. | Long-term destination, not a prerequisite for Q4. Capabilities alone are not an RT test. |
| Q7 | Separate future Vulkan/web/mobile profiles, feature/fallback table and target-specific real-scene validation. | Does not cancel the existing browser roadmap; requires its own engine, build, data, browser and presentation work. |

### Contract and review gate before Q2

T004 was signed off on 2026-10-28 on `11ea64a` by a non-author agent reviewer, with a human sign-off caveat; that review examined the three-tier matrix (portable core, D3D12-private, native extensions), the declared-use/binding and lifecycle/visibility rules, async completion, explicit fallbacks, shader rights/target stages, the header gate and the proposed T013 expected-row-integrity and byte-tolerance policy, recording decisions and remaining blockers. T005 was signed off the same day after the real template and its documented field validator had been run and independently reviewed. Automated structure checks cannot approve semantics or invent target evidence. These additions do not change issue #11's Q1a implementation scope or substitute for its independent review. Stabilize the reviewed shared boundary before Q2 or new dependent parallel implementations; the separately owned Q1a work is not retroactively claimed complete or portable. T088/Q7a is the pre-Q2 conformance check and is likewise not a browser-support claim.

### Engine adapter and frame ordering

Preserve the ordered engine command stream rather than translating individual `qgl*` calls. The engine submits at end of frame and may flush mid-frame for capture; view, set-buffer, copy and swap ordering must survive. Display overlays, world GUI surfaces, cinematics and other 2D views must be classified by semantics rather than blindly treating every 2D view as a final HUD. Track persistent previous rendered state in renderer-owned side tables with stable identity/generation, and capture private MD5 posed positions before shared cached geometry can be regenerated or deleted.

### Verification and execution strategy

1. Establish a disposable, worktree-local build directory; never reuse the root `build/` cache identified above.
2. Write one behavioral test at the public RHI seam, observe a real assertion failure, implement the minimum, rerun to green, and deliberately break the behavior to prove the test detects it. Repeat for each vertical slice; compile errors or skipped tests are not red evidence.
3. Run software and hardware checks separately where available. Make missing debug/GBV components a distinct skipped validation gate, not a functional pass or hidden success.
4. Keep each implementation issue in its own worktree with one writer; never run two agents on one branch or one worktree. Split into separate issues/worktrees when work would otherwise need parallel writers on shared files. Stabilize shared contracts before separate issues consume them; a different reviewer independently checks the diff and reruns commands. At most 3–4 concurrent worktrees per machine.
5. Record test commands, exit codes, adapter class, effective configuration, validation availability and changed files. Retain the full CTest log (`-V` or `--output-junit`/`--output-log`), because `--output-on-failure` alone keeps only failures. Never claim Q1, a playable renderer, DXR or DLSS based solely on Q1a.
6. For later engine integration, fix or avoid target-patch limitations, preserve the legacy path, and use the required fork branch/submodule-bump process. A local scratch copy is not the shipped source.
7. Keep each later feature behind its own capability, legal and visual/performance gates. No proprietary SDK access is assumed and no provider download is automatic.
8. For the shared documentation contract, run the commands in [quickstart.md](./quickstart.md): `scripts/validate-rhi-contract.py --self-test`, `scripts/test-validate-rhi-contract.py`, `scripts/test-validate-rhi-contract-mutations.py`, `git diff --check` and the no-index checks for new files. The validator is a structural and lexical gate only: it does not prove the contract is implementable, that a matrix row is achievable, or that an unsupported claim is false. The Q7a header/test scan reports NOT RUN while the paths are absent, and a header directory with no scannable header is a failure. A documentation pass never substitutes for an independent semantic review.

## Dependencies and Open Measurements

- A fresh safe native build and legal data are required for Q0 scene evidence and Q2 parity, but not for synthetic Q1a.
- A D3D12-capable device or clearly labeled software adapter is required to run Q1a. The debug layer and GPU-based validation are independent prerequisites for full Q1.
- The target GPU, frame budget, visual tolerances, data set, supported mods and authorized provider contract remain unmeasured or unconfirmed. They are deliberately not guessed and do not prevent the first independent fixture.
- The 003 research and this 004 specification are published by issue #10's PR; the 005 web-renderer research and this three-tier contract amendment are published by the S0 contract PR (Refs #20). Create implementation worktrees from `origin/main` after they merge; do not rely on files that exist only in a local checkout. The 005 document remains research, not an approved specification.
- Fresh worktrees do not inherit the git-ignored `.specify/feature.json`. Before running `/speckit.*` in a worktree, set `SPECIFY_FEATURE_DIRECTORY=specs/004-quality-renderer` or create the pointer; otherwise the helper may target the wrong feature or exit 1.
- Each stage issue owns updating the exact commands in `quickstart.md` for its own scope. T079 only re-runs commands that are already documented; it does not invent missing ones.

## Complexity Tracking

No constitutional violation is proposed. The additional RHI/backend and frame-history boundaries are required by the user's selected design, while the isolated Q1a build prevents the much larger and currently unnecessary engine-build/asset dependency from obscuring initial GPU correctness.
