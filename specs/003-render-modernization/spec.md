# Feature Specification: Rendering modernization path for DOOM 3 on Windows 11 x64

**Feature Branch**: none (historical draft; no branch was created)
**Created**: 2026-10-27
**Status**: Historical draft — superseded by [the quality-first specification](../004-quality-renderer/spec.md)
**Issue**: none — historical draft; not a work item
**Historical accuracy notice (2026-10-27)**: This draft is retained for provenance. The following statements are withdrawn or corrected by [`research.md`](./research.md) and are not current facts: the claim that the x64 port is not green or stops on OggVorbis (the native port now builds and has demo-map evidence); the claim that no native x64 build exists; the 1–2 day DLSS experiment estimate; and the 16–32 / 40–68 engineer-week estimates. Use 004 for planning.
**Input**: "为了 Win11 AMD64 贴图着色器现代化高清化，可能渲染后端也要升级（Vulkan/DX12？），需要源码抽象出 RenderRHI？以后 Web 端甚至移动端都可能用到。系统研究 Doom 3 引擎升级画质，以及不升级引擎只升级材质（包括着色器），或者用 DLSS 5 或其他 AI 渲染方案。"

**Research**: [`research.md`](./research.md) — evidence, prior art, licence analysis, and the option matrix.
This spec turns that research into gated, verifiable work. The research document is the authority for facts;
this document is the authority for scope and acceptance.

## Problem

DOOM 3's renderer is from 2004: OpenGL 1.x plus ARB assembly, five shading backends, client-side arrays, and
seven fixed texture units feeding a diffuse + specular-LUT model. The GPL source ships **no shader sources**
(they live in the game data), and an earlier x64 port attempt stopped on 32-bit OggVorbis inline assembly
(historical premise; see the accuracy notice above).

The project now has three goals that pull in different directions:

1. **Look modern on Windows 11** — high-resolution textures and modern shading.
2. **Do not paint itself into a corner** — a possible Vulkan/DX12 backend and a render-hardware-interface (RHI)
   abstraction, because web and mobile are stated future targets.
3. **Do not buy the wrong thing** — AI upscaling (DLSS 5, RTX Remix) and "just upgrade the materials" are both
   plausible-sounding shortcuts that may not deliver.

Without a decision record and a parity harness, three teams will independently rewrite the renderer in three
incompatible ways, and "looks better" will remain unprovable.

## Research summary and proposed decisions

Full evidence in [`research.md`](./research.md). The short version:

- **Material-only is capped.** Textures + `.mtr` give a sharper, better-authored 2004 renderer — a good
  2006-era HD remaster. PBR, IBL, AO, shadow maps, and sRGB need engine work. Adding ARB `glprogs` raises the
  ceiling but is ARB2-only and unavailable in the GPL tree, so it is a dead end for web/mobile.
- **The cheapest engine win is a base change, not a renderer rewrite.** dhewm3 1.5.5 already supplies 64-bit,
  CMake, SDL, OpenAL, restored depth-fail stencil shadows, and BC7 DDS. DUDE (a dhewm3 fork) already has an
  ARB→GLSL translator, GL 3.3 core, Vulkan, and an RHI.
- **One shader rewrite should pay for three platforms.** GL 3.3 core with GLSL written to the ES 3.00 floor
  reaches Windows, WebGL2, and GLES3. DX12 is Windows-only and buys nothing for web/mobile. Vulkan is the valid
  second backend: DUDE already has one, and NVIDIA's own Vulkan sample proves DLSS-NR on Vulkan. WebGPU is not
  Baseline.
- **A thin RHI is justified only at frame/pass/pipeline level, and only when a second backend ships.**
  DOOM 3's 130 legacy GL symbols collapse to roughly 60–80 entries, and its `GLS_*` state bits already form a
  pipeline key.
- **DLSS 5 needs a cheap experiment before any architecture commitment.** It shipped 2026-09-01 on RTX 50 and
  takes the final LDR tonemapped display frame, dense motion vectors, and depth — **no TAA history, jitter,
  exposure, albedo, or normals** at runtime. NVIDIA's `vk_gltf_renderer` sample proves the Vulkan path via a
  beta NGX SDK; the public Streamline package is still scaffolding only. An external ReShade-based bridge can
  produce a DLSS 5 frame from the 32-bit GL build with D3HDP in 1–2 days (estimated MVs, HUD processed); the
  official route needs a Vulkan backend, real MVs, HUD separation, and authorisation — 16–32 engineer-weeks from
  a Vulkan fork, 40–68 from today. RTX Remix does not support OpenGL. FSR 1 is the only clean, cheap stopgap.

| # | Proposed decision | Status |
|---|---|---|
| D1 | Re-base the engine on dhewm3 1.5.5 (or a fork of it); retire the 2011-tree port as the primary base | **Proposed** — needs a timeboxed spike |
| D2 | First modern renderer is GL 3.3 core + GLSL with an ES 3.00 floor; legacy ARB stays as parity reference | **Proposed** |
| D3 | Introduce an RHI seam at frame/pass/pipeline/binding level; do not wrap every `gl*` call | **Proposed** |
| D4 | GL 3.3 + ES 3.00 is the portable baseline. **Vulkan is the preferred optional native backend (DLSS-NR proven); DX12 is a Windows-only fallback, not a target.** WebGPU deferred | **Proposed** — Vulkan only after a DLSS 5 go/no-go |
| D5 | Material-only HD work runs in parallel as a bounded track, not as the modernization | **Proposed** |
| D6 | DLSS/RTX SDK/RTX Remix stay out of the default GPL build; evaluate DLSS 5 externally first; an approved DLSS module is out-of-tree and runs on the Vulkan profile (DX12 fallback) | **Proposed** |
| D7 | D3HDP is local test data, not repository content; use the DXT path, not BC7-only; keep its Sikkmod lighting layer optional | **Proposed** |

These are not ratified. D1 and D6 are the two that most change the project's shape and require explicit
approval and an independent review.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Decide the engine base with evidence (Priority: P1)

A developer can say, with a build log and a screenshot, whether dhewm3 1.5.5 is a viable base on this machine,
and the project can either adopt it or record why not. This unblocks every rendering task and removes the
current red-build ambiguity.

**Why this priority**: Every renderer, material, and web plan depends on a base that actually builds and runs.

**Independent Test**: A timeboxed spike builds the candidate base with VS2022 x64, boots with legal demo data,
captures a screenshot and a log, and records PASS/FAIL against the same checks used by spec 001.

**Acceptance Scenarios**:
1. **Given** a clean worktree and legal demo data, **When** the spike build command runs, **Then** it exits 0
   and produces an x64 executable and game module.
2. **Given** the built engine, **When** it starts with the demo data, **Then** it reaches the main menu and a
   1280×720 capture is non-blank.
3. **Given** the spike result, **When** the decision is recorded, **Then** the record names the base, commit,
   licence, command, artifacts, and the reason the alternative was rejected.

---

### User Story 2 - Prove the visual baseline and find the bottleneck (Priority: P1)

A developer can render the same three views twice and get the same image, and can state with numbers whether the
engine is CPU-bound or GPU-bound at 1080p and 4K on named hardware.

**Why this priority**: Without a reproducible golden set, "HD" and "parity" are opinions; without a bottleneck
classification, upscaling and backend work are guesses.

**Independent Test**: A harness renders three fixed views (a light-heavy corridor, an open area, a combat scene
with multiple dynamic lights), captures PNGs, and runs a `timedemo` at three resolutions; the report contains
the commands and numbers.

**Acceptance Scenarios**:
1. **Given** the same build, data, and view, **When** the harness runs twice, **Then** the two captures differ by
   at most 1/255 mean absolute error per channel.
2. **Given** the resolution sweep, **When** frame time is plotted against pixel count, **Then** the report
   classifies the workload as CPU-bound, GPU-bound, or mixed, with the GPU-busy evidence attached.
3. **Given** missing data or a missing GPU counter, **When** the harness runs, **Then** the check is reported
   SKIPPED with the reason, never PASS.

---

### User Story 3 - Ship a legal, high-resolution material pack without changing the renderer (Priority: P2)

An artist can take a user-owned DOOM 3 install, produce a BC7/DDS texture set with correct RXGB normal maps, and
see a sharper image with the same lighting — while the repository ships only tooling, manifests, and original
`.mtr` text.

**Why this priority**: It is an independent, visible win and a ready-made test corpus for the new renderer.

**Independent Test**: Run the material pipeline on a fixed list of materials, load the result in the
baseline engine, and compare A/B captures plus VRAM and load time.

**Acceptance Scenarios**:
1. **Given** a user-owned install, **When** the pipeline runs, **Then** it emits a manifest of source hashes,
   output files, format, dimensions, and mip count, and no game asset is written into the repository.
2. **Given** the produced pack, **When** it loads, **Then** diffuse, specular, and normal maps show no axis swap
   (RXGB preserved), no seam at tile edges, and no visible normal-map faceting.
3. **Given** the pack, **When** VRAM and load time are measured, **Then** both are recorded against the baseline
   and stay within the agreed budget.

---

### User Story 4 - Render on a modern GL path with proven parity (Priority: P2)

A player can run DOOM 3 through a GL 3.3 core renderer with GLSL shaders, and a reviewer can show that it
matches the legacy path on the golden set — including stencil shadows, mirrors, perforated alpha, and the HUD.

**Why this priority**: This is the actual modernization; it is also the prerequisite for Vulkan and web.

**Independent Test**: Switch the renderer at runtime, capture the golden set on both paths, and diff them; run
the shadow/portal/material regression checks.

**Acceptance Scenarios**:
1. **Given** the legacy and GL 3.3 paths, **When** the golden set is captured, **Then** mean absolute error is at
   most 2/255 per channel on every view, with the diff artifact archived.
2. **Given** the GL 3.3 path, **When** the renderer log is scanned, **Then** zero ARB program binds and zero
   fixed-function client-array draws occur on that path.
3. **Given** `r_showShadows`, `r_showPortals`, and a perforated material (grate/foliage), **When** each is
   exercised, **Then** there is no volume leakage, no area pop-in, and no alpha-test shadow artifact.
4. **Given** named hardware, **When** the same `timedemo` runs on both paths, **Then** the GL 3.3 path is within
   10% of the legacy path or the regression is recorded and accepted.

---

### User Story 5 - Add a second backend only when it earns its keep (Priority: P3)

A maintainer can add Vulkan behind the same seam without touching game logic, and can demonstrate that the
legacy, GL 3.3, and Vulkan paths agree on the golden set.

**Why this priority**: It buys Android/Linux reach and a future RT path, but it is large and only valuable after
GL parity.

**Independent Test**: Build with the Vulkan backend, render the golden set, and diff against GL 3.3; run the
Vulkan validation layer with no errors.

**Acceptance Scenarios**:
1. **Given** a Vulkan-capable GPU, **When** the golden set renders, **Then** parity is within the agreed
   threshold and the validation layer reports zero errors.
2. **Given** a GPU without the optional feature (depth clamp, BC compression, stencil format), **When** the
   backend initialises, **Then** it selects a documented fallback rather than crashing.
3. **Given** a build without Vulkan, **When** the engine starts, **Then** GL 3.3 remains the default and no
   Vulkan symbols are required.

---

### User Story 6 - Reach WebGL2/mobile from the same shader source (Priority: P3)

A developer can build the same renderer to WebGL2 (and GLES3 later) with the same GLSL source tree, and prove
one map renders in a browser at a usable frame rate.

**Why this priority**: It is the stated future target and the main reason to choose the ES 3.00 shader floor.

**Independent Test**: Build with Emscripten, serve locally, load a demo map, capture a browser screenshot, and
record console/GL errors and frame time.

**Acceptance Scenarios**:
1. **Given** the web build and legal demo data, **When** a demo map loads in Chromium, **Then** the canvas is
   non-blank and the console has no GL error storm.
2. **Given** a 1280×720 viewport on named hardware, **When** the map runs for 60 seconds, **Then** the median
   frame rate is at least 30 FPS, or the measured shortfall is recorded with the bottleneck.
3. **Given** no WebGPU support, **When** the WebGL2 build runs, **Then** it works without a WebGPU fallback path.

---

### User Story 7 - Decide DLSS 5 with a cheap experiment before funding a native backend (Priority: P2)

A maintainer can produce a DLSS 5 frame from the current 32-bit GL build, with the D3HDP texture pack, using an
external ReShade-based bridge in one to two days — and can decide GO/NO-GO from captured evidence instead of
committing months to a Vulkan backend on a hunch.

**Why this priority**: DLSS 5 needs a native Vulkan/D3D12 backend, a beta NGX SDK, RTX 50 hardware, and motion
vectors the engine does not have. The external experiment is the cheapest way to test the one thing that
matters: whether the neural look is worth the cost.

**Independent Test**: Install dhewm3 1.5.5 (32-bit is acceptable), D3HDP, ReShade, DLSS5-Feeder and a neural
consumer outside the repository; capture three A/B scenes; record HUD, ghosting, alpha-test and frame cost.

**Acceptance Scenarios**:
1. **Given** an RTX 50 GPU and legal data, **When** the experiment runs, **Then** three fixed scenes are
   captured A/B at 1080p, and the report records HUD treatment, ghosting on animated monsters/weapon, thin
   geometry, perforated surfaces, frame cost, and crashes.
2. **Given** no RTX 50 GPU or no NVIDIA GPU, **When** the experiment is attempted, **Then** the result is
   SKIPPED with the reason; no inference is recorded as PASS.
3. **Given** the experiment, **When** the repository is checked, **Then** no NVIDIA DLL, neural consumer, or
   asset from the pack is tracked; the install lives in a disposable profile/VM.
4. **Given** the captures, **When** the decision is recorded, **Then** it states GO or NO-GO with the visual
   evidence, the 16–32 engineer-week cost from a Vulkan fork, and the partner-access dependency.

---

### User Story 8 - Add an optional Vulkan DLSS profile only after a GO (Priority: P3)

A maintainer can add a Vulkan backend behind the RHI, feed real motion vectors, depth and a HUD-less LDR frame to
NGX DLSS-NR, and run DLSS 5 without changing the portable GL path or the default build.

**Why this priority**: Vulkan is the preferred native route — DUDE already has a backend and NVIDIA's own Vulkan
sample proves DLSS-NR — but it is still expensive and must be earned by the experiment and by partner access.

**Independent Test**: Build with the Vulkan profile, render the golden set, run the validation layer, and compare
against GL 3.3; run a DLSS 5 frame with real MVs, depth, and a separate HUD composite.

**Acceptance Scenarios**:
1. **Given** a Vulkan-capable GPU, **When** the golden set renders on Vulkan, **Then** it matches GL 3.3 within
   the agreed threshold and the validation layer reports zero errors.
2. **Given** static geometry, animated MD5 meshes, particles, the viewmodel, and alpha-tested grates, **When**
   the velocity buffer is captured, **Then** each class has a documented motion-vector source and no class is
   silently zero.
3. **Given** a DLSS 5 frame, **When** the image is composed, **Then** DLSS-NR receives tonemapped LDR colour at
   display resolution, depth, and dense MVs, and the HUD is drawn after NR at native resolution.
4. **Given** a build without Vulkan or without an NVIDIA GPU, **When** the engine starts, **Then** GL 3.3
   remains the default and no Vulkan/DLSS symbols are required.

---

### Edge Cases

- Legal data is absent: every data-dependent check reports SKIPPED with a reason, never PASS.
- The user owns the BFG Edition instead of the 2004 game: the base must state which data it supports and fail
  with a clear message for the other.
- A mod ships custom ARB `vertexProgram`/`fragmentProgram` stages: the GL 3.3 path must either translate them at
  runtime or fail the material loudly; silent fallback is not acceptable.
- The GPU lacks a depth-stencil format, depth clamp, BC compression, or anisotropic filtering: each is probed
  and has a documented fallback.
- Intel integrated graphics on Win11: the legacy GL path must not hard-fail at start, and the modern path must
  report the capability gap.
- A view contains a mirror, a security camera, a cinematic, and a perforated grate: all must survive the GL 3.3
  path, not just the main world view.
- A texture pack uses BC7, DXT5nm, or an AI-upscaled normal map: the loader must reject or convert it with a
  clear warning, not render wrong lighting.
- A demo recorded by the legacy build is played on the modern path: divergence is either accepted and documented
  or the demo path is gated.
- The engine is re-based on dhewm3: existing 1.3.1 mod DLLs may not load; the base change must document the ABI
  break and the supported mod set.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST record an engine-base decision as a dated ADR/commit, with a build log, a
  screenshot, a licence statement, and the rejected alternative. The decision MUST NOT be made from opinion
  alone.
- **FR-002**: The project MUST provide a reproducible golden-image harness for at least three fixed views, using
  engine commands and fixed view parameters, and MUST archive the captures with the build identity.
- **FR-003**: The harness MUST classify the workload as CPU-bound, GPU-bound, or mixed, using a resolution sweep
  and GPU-busy evidence, on named hardware.
- **FR-004**: Every data-dependent check MUST report SKIPPED with a reason when legal data is absent; no
  data-dependent check may report PASS without running.
- **FR-005**: The project MUST provide a material pipeline that produces DDS output with full mip chains
  (BC7 where the base supports it, DXT/S3TC otherwise), preserves the RXGB normal-map convention, and emits a
  manifest of source hashes and output metadata.
- **FR-006**: The repository MUST NOT track or redistribute game assets, derived texture packs, or binary
  patches that encode them; only tooling, manifests, original material text, and new art are allowed.
- **FR-007**: The legacy ARB path MUST remain available as the reference until the modern path passes the parity
  gate, and the renderer MUST allow switching paths at runtime.
- **FR-008**: The modern path MUST render with a GL 3.3 core-class feature set and GLSL source that is valid at
  the GLSL ES 3.00 floor, without ARB program binds or client-side array draws.
- **FR-009**: The modern path MUST preserve the engine's existing stencil-shadow invariants, depth convention,
  alpha-test behaviour in the depth prepass, mirror/subview behaviour, and HUD composition.
- **FR-010**: The renderer MUST expose a backend-neutral seam at frame, pass, pipeline, binding, resource, and
  draw granularity before a second backend is added; the seam MUST NOT be a wrapper around individual `gl*`
  calls.
- **FR-011**: The seam MUST express pipeline state as the existing `GLS_*` vocabulary plus explicit stencil,
  shader, vertex-layout, and topology inputs, and MUST support cached pipeline objects.
- **FR-012**: No second native backend may ship before the modern GL path passes parity; when added, it MUST be
  selectable at runtime and MUST have a documented fallback.
- **FR-013**: The default plan MUST NOT target D3D12. **Vulkan** is the preferred optional native backend
  (NVIDIA's sample proves DLSS-NR on Vulkan); a Windows-only DX12 backend is a fallback only if Vulkan NR is
  blocked. Either MUST NOT become the default or a requirement for the GL/web/mobile paths. WebGPU MUST remain
  deferred until it is Baseline in the supported browsers.
- **FR-014**: The web build MUST reuse the same shader source tree as the native GL path, with only a version
  header and platform-specific prelude difference.
- **FR-015**: The default build MUST NOT link or bundle NVIDIA RTX SDK components, DLSS, or the RTX Remix
  runtime. An approved DLSS module MUST be out-of-tree, user-installed, and absent from the default build and
  repository; a licence check MUST prove the separation.
- **FR-016**: FSR 1 MAY be offered as an optional post-process; FSR 2+/TAAU MUST NOT be enabled until velocity,
  jitter, HDR, and HUD-less composition exist and are demonstrated.
- **FR-017**: Every performance or parity claim MUST name the hardware, resolution, build SHA, data set, and
  command used to produce it.
- **FR-018**: The project MUST document the supported data set (2004 game vs BFG Edition) and fail with a clear
  message for unsupported data.
- **FR-019**: A base change or submodule change MUST be reviewed independently and MUST record the old/new
  provenance and licence.
- **FR-020**: The decision record MUST state which of D1–D7 were accepted, rejected, or deferred, and what
  evidence would reverse each one.
- **FR-021**: The project MUST run the DLSS 5 external experiment before any native DLSS backend work, and MUST
  record GO/NO-GO with A/B captures, HUD/ghosting notes, frame cost, and hardware. If no RTX 50 GPU is
  available, the gate is SKIPPED, not assumed.
- **FR-022**: If a native DLSS backend is approved, it MUST provide dense per-pixel motion vectors for static
  and animated geometry, expose depth (linear ViewZ or reversed-Z with the inverted flag), render a HUD-less
  tonemapped LDR frame at display resolution, and reset NR history on level load/cutscene before DLSS 5 is
  enabled. Camera jitter, TAA history, exposure, albedo, and normals are NOT required by DLSS-NR.
- **FR-023**: The Vulkan/DX12 DLSS profile MUST NOT change the GL 3.3 default, the web/mobile shader source, or
  the data formats; it MUST pass the same golden-parity gate and be validation/debug-layer clean.
- **FR-024**: D3HDP MUST be treated as external test data: a manifest with the ModDB MD5 and per-pak SHA-256 is
  allowed, the assets are not, and the pack MUST NOT enable dhewm3's BC7-only path (`image_usePrecompressedTextures 2`).
- **FR-025**: The optional Sikkmod/D3HDP lighting override MUST be a separately tested layer; its absence MUST
  leave a playable HD-texture path, and its presence MUST NOT be required by the modern renderer.

### Key Entities

- **Engine base**: the source tree the renderer work builds on, with commit, licence, data compatibility, and
  build command.
- **Golden set**: three fixed views with camera parameters, build identity, data identity, and captured images.
- **Capability profile**: probed features (GL version, texture units, stencil format, depth clamp, BC support,
  anisotropy) with the selected fallback for each.
- **Material manifest**: source texture hash, output format/dimensions/mips, material name, and transform
  applied (upscale, height→normal, de-light).
- **RHI contract**: the backend-neutral set of frame/pass/pipeline/binding/draw operations and their ownership
  rules.
- **Parity report**: two images, a diff image, a metric, and the commands that produced them.
- **DLSS experiment record**: hardware, install layout, motion-vector provider, A/B captures, HUD/ghosting
  notes, frame cost, and GO/NO-GO.
- **Velocity contract**: which geometry classes write motion vectors, from which previous transform, and how
  missing classes are detected.
- **Decision record**: dated ADR with decision, alternatives, evidence links, approver, and reversal condition.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A documented x64 build command exits 0, produces an executable and game module, and the PE
  machine type is `0x8664`.
- **SC-002**: With legal data present, the engine reaches the main menu and a 1280×720 capture is non-blank
  within 120 seconds.
- **SC-003**: Two consecutive runs of the golden harness differ by at most 1/255 mean absolute error per
  channel on every view.
- **SC-004**: The bottleneck report names the hardware and classifies the workload with numbers at 640×480,
  1080p, and 4K.
- **SC-005**: The material pack covers at least the agreed hero set, preserves RXGB normals, and stays within
  the recorded VRAM and load-time budget.
- **SC-006**: The GL 3.3 path differs from the legacy path by at most 2/255 mean absolute error on every golden
  view, with zero ARB binds and zero client-array draws on that path.
- **SC-007**: The GL 3.3 path is within 10% of the legacy path on the same `timedemo`, or the regression is
  recorded and explicitly accepted.
- **SC-008**: A WebGL2 build renders one demo map in a browser at ≥30 FPS median at 1280×720 on named hardware,
  or the measured shortfall and its cause are recorded.
- **SC-009**: The licence/asset check passes: no game assets, no derived packs, no RTX SDK binaries, and no
  DLSS/Remix linkage in tracked files or the default build.
- **SC-010**: Every decision D1–D7 has a dated record with its evidence and reversal condition.
- **SC-011**: The DLSS 5 external experiment produces three A/B scenes, a HUD/ghosting/frame-cost report, and a
  GO/NO-GO within two working days — or a SKIPPED record naming the missing hardware/access.
- **SC-012**: If a native DLSS backend is approved, it matches GL 3.3 within the agreed threshold on every
  golden view, is validation-layer clean, and leaves the GL/web/mobile default build unchanged.
- **SC-013**: The D3HDP manifest matches the downloaded pack (ModDB MD5 and per-pak SHA-256), and the BC7-only
  path is not enabled; a clean install reaches a playable HD-texture scene without the Sikkmod layer.

## Assumptions

- The user owns a legal DOOM 3 installation (2004 or demo) and will not commit its data; BFG Edition data is a
  separate compatibility question, not a silent fallback.
- The target machine has a modern Win11 x64 GPU with OpenGL 3.3+; the exact GPU is not yet named, so performance
  thresholds are placeholders until measured. The DLSS 5 experiment additionally requires an NVIDIA RTX 50 GPU;
  if the target is not RTX 50, the experiment is SKIPPED and DLSS 5 is not a product requirement.
- DLSS 5 integration requires NVIDIA authorisation and a beta NGX SDK that is not publicly downloadable; if it
  is not granted, the Vulkan/DX12 profile is limited to a technical prototype and DLSS 5 is not shipped.
- The DLSS 5 external experiment uses community tools and NVIDIA binaries outside the repository, in a disposable
  profile/VM; it is evidence, not a dependency.
- D3HDP is used locally as the HD corpus; it is not committed, and its Sikkmod lighting layer is optional.
- dhewm3 1.5.5 is GPLv3-compatible and may be forked; the project will not attempt to upstream to dhewm3, whose
  README bans AI-assisted contributions.
- DUDE is a reference and a pinned code source, not a trusted dependency; adopting any of its code requires a
  parity test and an independent review.
- The existing spec 001 work is not wasted: its verification scripts, target-copy workflow, and asset checks are
  reusable even if the base changes.
- No golden image or performance baseline was recorded when this draft was written; the claim that no native
  x64 build exists is withdrawn (see the accuracy notice above). The first phase creates the missing baseline.

## Out of Scope

- Gameplay, AI, audio, networking, and UI feature changes.
- A path-traced or RTX Remix-style renderer.
- A **DX12-first** renderer, console targets, or an iOS App Store release. (An optional Vulkan DLSS profile is
  in scope only after FR-021 is a GO; DX12 is a fallback only if Vulkan NR is blocked.)
- Redistributing game data, HD texture packs, or derived assets.
- Shipping DLSS, XeSS, or any proprietary SDK in the default GPL build.
- Rewriting the game module or changing save-game/demo formats unless the base change forces it.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Re-basing on dhewm3 invalidates spec 001 work | Lost time, churn | Timeboxed spike; keep 001's harness and asset checks; independent review |
| DUDE is two months old and unproven | Adopting broken code | Pin a commit; keep the legacy path; require pixel parity before merge |
| Shader sources are absent | The port cannot be mechanical | Re-author from `PP_*`; use a full install's `glprogs` only as local ground truth |
| Base change breaks mod DLLs | Existing mods stop loading | Document the ABI break; ship the supported mod set; keep 1.3.1 as a compatibility note |
| Alpha-test/depth-prepass regression | Shadow acne or light leaks | One shader variant used by both passes; perforated-material regression test |
| Web memory ceiling | Full game cannot load | Design a 2 GB wasm32 budget; measure demo first; WebGPU/memory64 not assumed |
| Legal contamination from texture packs | Repository cannot be distributed | Tooling/manifests only; no derived assets; licence check in CI |
| DLSS 5 is inaccessible or unconvincing | Months of native-backend work for nothing | Run the 1–2 day external experiment first; NO-GO leaves GL/Web path intact |
| Community DLSS bridge is untrusted | Malware or instability on the dev machine | Disposable VM/profile; no repository changes; hash/verify downloads |
| Vulkan/DX12 backend cost overruns | Schedule and maintenance burden | DUDE/NVRHI references, minimum viable slice, parity gate before DLSS |
| Estimated motion vectors / processed HUD | Ghosting, soft UI, wrong look | Treat as experiment only; official path needs real MVs and HUD separation |
| "Looks better" is subjective | Endless iteration | Golden set + parity metric + recorded acceptance |

## Dependencies

- **Blocking**: a green native baseline (either spec 001's fix or the dhewm3 spike) and legal demo data.
- **Tooling**: VS2022 x64, CMake, RenderDoc, GPU profiler, Playwright/browser for the web phase.
- **Governance**: an ADR for the base change; independent review for submodule/build-system changes.
- **Related**: `specs/001-win11-amd64-native-build` (baseline, verification scripts), `docs/ROADMAP.md` M1–M4,
  `doom3_orig/doc/02_renderer.md` (renderer analysis), `docs/GIT_WORKFLOW.md` (branch/PR rules).
