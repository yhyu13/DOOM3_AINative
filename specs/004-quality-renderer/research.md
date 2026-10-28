# Research decisions: Quality-first rendering

**Status**: Design decisions for [spec.md](./spec.md), not evidence of an implemented renderer.

**Primary research**: [reviewed 003 research](../003-render-modernization/research.md), particularly sections 2–11. Its external references are retained there with their stated scope and confidence. This record reconciles that research with the new request; it does not turn vendor samples into local test results.

## D1 — Preserve the current engine, select D3D12-first

**Decision**: Keep the existing classic-data x64 port and GL renderer as reference/fallback; start a Windows D3D12 flagship behind a bounded backend-neutral custom RHI. Keep other native/browser profiles separate and future.

**Rationale**: The user selected ownership and maximum visual ceiling over shortest development time. Native rendering evidence exists historically; a mandatory base change and GL3 → Vulkan → D3D12 sequence are not supported prerequisites. The new [spec](./spec.md) explicitly supersedes the earlier 003 draft's contradictory decisions.

**Alternatives**: NVRHI plus a project-owned frame graph is a credible reuse-first design and is not inferior in image quality by definition. Revisit custom ownership if an evaluated prototype fails to justify its maintenance. A material-only GL experiment remains useful but is not the chosen foundation.

## D2 — Independent synthetic first slice

**Decision**: Keep initial RHI/backend code in the superproject and compile a standalone synthetic test project; start with an offscreen clear/readback and fence-backed retirement, then add one verified capability at a time.

**Rationale**: The root native build has configure-time target-copy/patch work. A game-data-free fixture isolates device, state and lifetime correctness without claiming game integration. The exact public boundary and incomplete Q1a/Q1 distinction are in [the foundation contract](./contracts/rhi-foundation.md).

**Alternatives**: Modifying ignored target copies alone is not reproducible delivery. The [exporter](../../scripts/export-target-patches.ps1) adds `A`/`D` rows to its manifest but emits patch hunks only for modified files; the [apply script](../../scripts/apply-target-patches.ps1) now fails closed when such rows exist and only reports success when every `M` row matches, and the [verifier](../../scripts/verify-win11-amd64.ps1) exports to a temporary directory and fails `SC-006b` on any non-`M` row in that export, but both the patch and its expected hashes are regenerated from the same target copy, so a pass cannot show that a required tracked modification is present (T084). Added/deleted engine files must still use the reviewed fork/submodule process; the scripts do not reconstruct them.

## D3 — Explicit verification and safe local execution

**Decision**: Test actual outputs and fence/resource behavior on hardware and software adapters separately; report debug-layer and GPU-based-validation availability independently. Build in a fresh worktree-local directory and do not blindly run existing smoke/export helpers.

**Rationale**: During local investigation the root `build/` cache pointed at another worktree, so it is not a valid fresh baseline. The [native verifier](../../scripts/verify-win11-amd64.ps1) exports patches to a temporary directory instead of overwriting tracked patch/manifest files, and fails closed on `A`/`D` manifest rows of that temporary export (its expectations are self-generated, so this is not proof of canonical-patch provenance; see T084); [smoke](../../scripts/smoke-win11-amd64.ps1) and [run](../../scripts/run-win11-amd64.ps1) stop only processes whose executable path matches the resolved executable, require `-StopExisting` to do so, and refuse a non-empty output directory without `-Clean`. A zero verifier exit may still coexist with a SKIPPED smoke. These behaviors must not be hidden or triggered against unrelated user work.

A read-only local probe observed Visual Studio 2022, an installed Windows SDK and D3D12-capable hardware and software adapters; `D3D12GetDebugInterface` reported `0x887A002D` (SDK component missing). It did not execute a draw, readback, validation-layer test or game. The debug/GBV gate remains unverified and must be re-probed on the actual worktree/device; do not install operating-system components without appropriate authorization.

**Alternatives**: Reusing a preexisting executable, software-adapter result or device-capability bit as a fresh hardware or game pass is rejected. Historical [001 re-review](../001-win11-amd64-native-build/review.md#re-review-2026-10-27-after-the-setup-stage-and-the-engine-follow-ups) reports save/demo fixes and a sound-on record/play check; it does not prove a fresh deterministic timedemo in this session. Do not repeat the earlier claim that its old pointer defect is necessarily still open.

## D4 — Rendered history, composition and engine seam

**Decision**: Adapt the ordered view/set/copy/present command stream, preserve mid-frame flush/capture order, and keep stable per-view previous-rendered geometry/transform history with separate validity. Use a private persistent copy for previous skinned poses and composite the display overlay after reconstruction.

**Rationale**: The pristine engine can flush command streams within a frame for captures as well as at frame end. Fullscreen GUI views and in-world GUI surfaces have different semantics; blindly moving every 2D view to final HUD changes output. Render surfaces and dynamic-model caches are not stable previous-frame identities. See [the foundation contract](./contracts/rhi-foundation.md) and [data model](./data-model.md).

**Alternatives**: Wrapping each GL call, storing only previous camera matrices, treating zero motion as invalid or valid by itself, and deriving provider contracts from a different API's sample are rejected.

## D5 — Playable hybrid first, path-traced destination

**Decision**: Retain a classic compatibility route; build an HDR PBR hybrid renderer with a forward path for special/translucent content, controlled light transport and authored legal assets. Follow it with a DXR static reference and dynamic/skinned showcase, with independent motion, noise, convergence, latency and art gates.

**Rationale**: Renderer algorithms, material/geometry authorship and stable motion control visual quality; the API name alone does not. A converged reference can diagnose hybrid approximations, while a single attractive still cannot establish playable quality or preservation of the game's darkness.

**Alternatives**: Forcing every legacy material into one G-buffer, claiming pixel parity for intentionally new lighting, or requiring path tracing before any playable hybrid result are rejected.

## D6 — Optional neural provider and legal gate

**Decision**: The default renderer works without NVIDIA components. A DLSS 5 claim requires authorized access and actual create/evaluate on the selected backend, correct SDK-specific inputs/reset/masks, hardware tests, off-path tests and independent distribution/license review.

**Rationale**: The reviewed Vulkan sample demonstrates its own API usage, not availability of a usable D3D12 NR SDK or the right to distribute it. Dynamic or out-of-tree loading does not itself establish GPL compatibility. Access, device support, visual quality and legal distribution are separate questions; none is assumed.

**Alternatives**: An external injector is an optional experiment, not a prerequisite. Vulkan may be selected later for a specifically authorized provider if its actual SDK warrants it; that does not prove D3D12 NR.

## D7 — Performance and portable profiles

**Decision**: Measure named hardware, frame pacing, memory, latency, scenes and review tolerances before setting thresholds. Keep capability/fallback tables and target-specific real-scene checks for every later Vulkan or web/mobile profile.

**Rationale**: No current baseline, performance budget or visual threshold was established by this planning exercise. DXC/SPIR-V compilation or a neutral header is not a browser port.

**Alternatives**: Invented FPS/time estimates, a single shader floor for all profiles, and silent removal of unsupported features are rejected.

## Open evidence, not unresolved implementation guesses

- Fresh game-data baseline, deterministic scene recordings, legal local hero assets, visual thresholds and performance targets.
- Full Q1 draw/compute/MRT/depth/history validation, including debug layer and GPU-based validation on an equipped machine.
- Engine command/material integration, patch reproducibility, independent submodule review and whole-game/mod support.
- Actual authorized neural SDK/backend interface, device capability and distribution clearance.
- Dynamic DXR performance, convergence, motion stability and preservation of art direction.

Unavailable evidence blocks or skips its own gate; it does not block designing or testing the standalone first slice.
