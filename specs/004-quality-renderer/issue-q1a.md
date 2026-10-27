# Issue #11: D3D12 RHI clear/readback foundation

**Status**: GitHub issue [#11](https://github.com/yhyu13/DOOM3_AINative/issues/11). The specification is published by [#10](https://github.com/yhyu13/DOOM3_AINative/issues/10). No implementation worktree, commit or PR has been created yet.

## Context

The quality-first rendering programme chooses a small custom RHI and a D3D12-first backend. The first deliverable is an isolated, synthetic GPU foundation, not game integration or the complete graphics/compute/MRT/depth gate.

The published design is in `specs/004-quality-renderer/spec.md`, `plan.md`, `tasks.md`, `contracts/rhi-foundation.md` and `quickstart.md` (published by #10). This issue owns only Q1a: the first offscreen clear/readback slice.

## Acceptance criteria

- [ ] Configure and build the standalone Windows x64 CMake/CTest fixture in a fresh worktree-local build directory without creating or patching an engine target copy and without game data or proprietary SDKs. The installed Windows SDK D3D12/DXGI and Visual Studio C++ tools are required.
- [ ] `ctest --test-dir <fresh-build> -C Debug --no-tests=error -V` exits 0 and the retained log shows at least one adapter actually executed the clear/readback test. An all-skipped run, device creation alone, or `--output-on-failure` without the full log is not a pass.
- [ ] The test `rhi.q1a.clear_readback` exits 0: a synthetic 4×4 RGBA8 texture with a per-row distinct pattern is cleared in `RENDER_TARGET`, transitioned to `COPY_SOURCE`, copied to a `COPY_DEST` readback buffer, and every row is compared with independently derived bytes and the documented UNORM rounding tolerance. The log contains the adapter class and the readback result.
- [ ] The test `rhi.q1a.state_journal` exits 0 and asserts the recorded sequence `RENDER_TARGET` → `COPY_SOURCE`, with the readback buffer in `COPY_DEST`.
- [ ] The test `rhi.q1a.negative_controls` exits 0 and internally requires three failures: (a) `width * 4` used as the staging row stride when the returned row pitch is larger, (b) an off-by-one source-row offset, and (c) repeating row 0 for every row. A genuinely inapplicable control is reported NOT APPLICABLE with the reason, never counted as a pass.
- [ ] The test `rhi.q1a.fence_outstanding_release` exits 0: at least two submissions run, and a release/reset attempt occurs while `GetCompletedValue() < signalValue`. The log contains both values. A negative control shows the test fails if wait-idle precedes every release. Incomplete-initialization shutdown is also exercised.
- [ ] Debug layer and GPU-based validation are opt-in requests; the fixture attempts enablement, captures messages into the retained log, and records RUN/SKIPPED/NOT RUN distinctly. No check is marked passing without executing.
- [ ] `specs/004-quality-renderer/evidence-q1a.md` records the configure/build/CTest commands, exit codes, effective configuration, adapter class, skipped checks, the full log path, the red/green/mutation TDD evidence, and the changed files.
- [ ] `git -C doom3_orig/doom3 status --porcelain` is empty; no game data, binaries, build output or NVIDIA SDK component is tracked.
- [ ] An independent reviewer who did not write the code records a review in `specs/004-quality-renderer/review-q1a.md`.

## In scope

- `src/renderer/rhi/**` — public bounded first-slice RHI interface and ownership rules (new).
- `src/renderer/d3d12/**` — D3D12 device, texture, submission, fence and readback (new).
- `tests/renderer/**` — standalone CMake/CTest project and synthetic fixtures (new).
- `specs/004-quality-renderer/evidence-q1a.md` (new).
- `specs/004-quality-renderer/review-q1a.md` (new).
- Read-only references: `spec.md`, `plan.md`, `tasks.md`, `contracts/rhi-foundation.md`, `quickstart.md`. T005 owns `evidence-template.md`; T087 owns `traceability.md`; do not edit them in this issue.

## Out of scope

Engine-source/submodule changes, integration into either DOOM executable, root build rewrites, game assets, shaders/compute/MRT/readable-depth integration, native window/presentation, full Q1 validation, a frame graph, PBR/GI, DXR, DLSS, browser support, and performance claims. These remain separately gated tasks.

## Verification command

Run the Q1a configure/build/CTest sequence in `specs/004-quality-renderer/quickstart.md` from the dedicated worktree, then:

```powershell
ctest --test-dir <fresh-build> -C Debug --no-tests=error -V
```

Retain the full log (`-V` and/or `--output-junit`/`--output-log`); `--output-on-failure` alone keeps only failures. Review the per-test summary: `--no-tests=error` does not fail merely because all selected tests were skipped.

## Blocking edges and ownership

- Blocked by #10 (the specification must be on `origin/main` before the worktree is created).
- The public first-slice contract must be reviewed and agreed before implementation starts.
- One issue, one branch, one worktree and one writer. Do not run two agents on the same branch or worktree. Split into separate issues/worktrees if the work needs parallel writers.
- Independent reviewer is not the implementer.
- Missing debug/GBV is an explicit skipped validation check and blocks the **full Q1** gate, not an excuse to mark it passing.
- Fresh worktrees do not inherit `.specify/feature.json`; set `SPECIFY_FEATURE_DIRECTORY=specs/004-quality-renderer` before `/speckit.*`.
