# Issue #11: D3D12 RHI clear/readback foundation

**Status**: GitHub issue [#11](https://github.com/yhyu13/DOOM3_AINative/issues/11). The specification was published by [#10](https://github.com/yhyu13/DOOM3_AINative/issues/10); the three-tier contract, header gate and amended Q1a seam are published by the S0 contract change (Refs [#20](https://github.com/yhyu13/DOOM3_AINative/issues/20)). No implementation worktree, commit or PR has been created yet.

**Mirror note (2026-10-28)**: this file is updated for the amended contract. The GitHub issue body has **not** been updated because the current authenticated account could not execute `UpdateIssue` (permission denied). Apply the text below to #11 when that permission is available; until then the live issue body still describes the pre-amendment Q1a seam — **do not implement from the live body**, use this file.

## Context

The quality-first rendering programme chooses a small custom RHI and a D3D12-first backend. The first deliverable is an isolated, synthetic GPU foundation, not game integration or the complete graphics/compute/MRT/depth gate.

The published design is in `specs/004-quality-renderer/spec.md`, `plan.md`, `tasks.md`, `contracts/rhi-foundation.md` and `quickstart.md` (published by #10). Issue #20 amends that contract into three tiers: a portable WebGPU-subset core, a D3D12-private backend and optional native extensions. This issue owns only Q1a: the first offscreen clear/readback slice, now written against the portable seam with a D3D12-private diagnostic hook. It does not prove a web backend.

## Acceptance criteria

- [ ] Configure and build the standalone Windows x64 CMake/CTest fixture in a fresh worktree-local build directory without creating or patching an engine target copy and without game data or proprietary SDKs. The installed Windows SDK D3D12/DXGI and Visual Studio C++ tools are required.
- [ ] `src/renderer/rhi/rhi.h` and the public test translation units outside `tests/renderer/d3d12/` contain no D3D12/DXGI noun or handle (fence value, resource state/transition, command allocator/list, descriptor heap/range, root signature, swap chain, `ID3D12*`, `IUnknown`, `ComPtr`, `HRESULT`, HLSL/DXC paths). `py -3 -I scripts/validate-rhi-contract.py` exits 0 once `src/renderer/rhi/` exists and contains at least one header; a header directory with no scannable header is a failure, and the public-test scan is reported separately.
- [ ] `ctest --test-dir <fresh-build> -C Debug --no-tests=error -V` exits 0 and the retained log shows at least one adapter actually executed the clear/readback test. An all-skipped run, device creation alone, or `--output-on-failure` without the full log is not a pass.
- [ ] The test `rhi.q1a.clear_readback` exits 0: a synthetic 4×4 RGBA8 texture with a per-row distinct pattern is cleared, copied into a readback buffer through the public seam, and every row is compared with independently derived bytes under the documented UNORM rounding tolerance. The backend performs the correct private render-target → copy-source → copy-destination sequence; the public test names no native state. The log contains the adapter class and the readback result.
- [ ] The test `rhi.q1a.state_journal` exits 0 and asserts the recorded `RENDER_TARGET` → `COPY_SOURCE` sequence and the readback buffer's `COPY_DEST` state through the D3D12-private diagnostic hook, not through the portable core.
- [ ] The test `rhi.q1a.negative_controls` exits 0 and internally requires the public controls to fail: an off-by-one source-row offset, a repeat-row-0 case and a packed-row-length misread. The staging-pitch control (`width * 4` used as the staging row stride when the returned row pitch is larger) is owned by the D3D12-private companion test, because the public seam returns tightly packed bytes and never exposes the pitch. A genuinely inapplicable control is reported NOT APPLICABLE with the reason, never counted as a pass, and `evidence-q1a.md` names the test that owns each control.
- [ ] The test `rhi.q1a.outstanding_release` exits 0: at least two submissions run, and a release attempt occurs while the submission is still in flight. The public test observes, through the completion token only, that the submission was not complete at release; the D3D12-private hook additionally asserts `GetCompletedValue() < signalValue` and the log contains both values. A negative control shows the test fails if wait-idle precedes every release. Incomplete-initialization shutdown is also exercised.
- [ ] T013's tolerance policy is followed: canonical expected row bytes and independently published expected digests are predeclared, each actual byte is compared to its expected byte with absolute difference ≤ 1 UNORM step, and actual raw row hashes are logged diagnostically without being required to equal the expected digests.
- [ ] Debug layer and GPU-based validation are opt-in requests; the fixture attempts enablement, captures messages into the retained log, and records RUN/SKIPPED/NOT RUN distinctly. This is a D3D12-private capability, not a cross-backend gate. No check is marked passing without executing.
- [ ] `specs/004-quality-renderer/evidence-q1a.md` records the configure/build/CTest commands, exit codes, effective configuration, adapter class, skipped checks, the full log path, the red/green/mutation TDD evidence, and the changed files.
- [ ] `git -C doom3_orig/doom3 status --porcelain` is empty; no game data, binaries, build output or NVIDIA SDK component is tracked.
- [ ] An independent reviewer who did not write the code records a review in `specs/004-quality-renderer/review-q1a.md`.

## In scope

- `src/renderer/rhi/**` — public bounded first-slice RHI interface and ownership rules (new); no D3D12 noun.
- `src/renderer/d3d12/**` — D3D12 device, texture, submission, completion token, private fence/state journal and readback (new).
- `tests/renderer/**` — standalone CMake/CTest project and public-seam fixtures; the D3D12-private hook companion test, including the staging-pitch control, lives under `tests/renderer/d3d12/` (new).
- `specs/004-quality-renderer/evidence-q1a.md` (new).
- `specs/004-quality-renderer/review-q1a.md` (new).
- Read-only references: `spec.md`, `plan.md`, `tasks.md`, `contracts/rhi-foundation.md`, `quickstart.md`. T005 owns `evidence-template.md`; T087 owns `traceability.md`; do not edit them in this issue.

## Out of scope

Engine-source/submodule changes, integration into either DOOM executable, root build rewrites, game assets, shaders/compute/MRT/readable-depth integration, native window/presentation, full Q1 validation, a frame graph, PBR/GI, DXR, DLSS, browser/WebGPU implementation, and performance claims. These remain separately gated tasks. A D3D12 Q1a pass is not browser portability evidence.

## Verification command

Run the Q1a configure/build/CTest sequence in `specs/004-quality-renderer/quickstart.md` from the dedicated worktree, then:

```powershell
ctest --test-dir <fresh-build> -C Debug --no-tests=error -V
```

Retain the full log (`-V` and/or `--output-junit`/`--output-log`); `--output-on-failure` alone keeps only failures. Review the per-test summary: `--no-tests=error` does not fail merely because all selected tests were skipped. Once the public headers exist, also run:

```powershell
py -3 -I scripts/validate-rhi-contract.py
```

and retain the header-scan line; a scan over zero headers is not a pass.

## Blocking edges and ownership

- Blocked by #20 (the three-tier contract and header gate must merge before implementation starts) and #10 (already merged).
- The public first-slice contract must be reviewed and agreed before implementation starts.
- One issue, one branch, one worktree and one writer. Do not run two agents on the same branch or worktree. Split into separate issues/worktrees if the work needs parallel writers.
- Independent reviewer is not the implementer.
- Missing debug/GBV is an explicit skipped validation check and blocks the **full Q1** gate, not an excuse to mark it passing.
- Fresh worktrees do not inherit `.specify/feature.json`; set `SPECIFY_FEATURE_DIRECTORY=specs/004-quality-renderer` before `/speckit.*`.
