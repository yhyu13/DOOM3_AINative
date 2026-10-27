# Feature Specification: Quality-first rendering for DOOM 3

**Feature Branch**: Not created; implementation tickets require their own issue and worktree.
**Created**: 2026-10-27
**Status**: Draft — staged programme; no new renderer capability is implied by this document.
**Input**: Build the quality-first renderer described in the existing research: a small custom RHI with D3D12 first, a playable hybrid renderer, and an eventual path-traced showcase/reference mode. Optional DLSS 5 must not block the independent renderer.

This specification supersedes the conflicting decisions in the earlier [003 draft](../003-render-modernization/spec.md), not its research. In particular, GL 3.3 first, a mandatory external injector, Vulkan-only neural rendering, and the exclusion of D3D12 and path tracing are **not** requirements of this feature. The [reviewed research](../003-render-modernization/research.md) explains the decision and its evidence limits. The existing GL renderer remains the compatibility reference; the browser goals in the [roadmap](../../docs/ROADMAP.md) and [showcase specification](../002-project-showcase/spec.md) remain future work, not cancelled work.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Establish a reproducible reference (Priority: P1)

A developer can reproduce the current native renderer and capture representative scenes without mistaking historical screenshots for a current test run.

**Why this priority**: Renderer parity, quality and performance decisions need a known build, data set and scene.

**Independent Test**: Run a safe native build and verification procedure, then capture the agreed scene set twice when legally supplied data is available. Compare repeatability against tolerances recorded before the run; archive the commands, results and environment, and report unavailable prerequisites as SKIPPED.

**Acceptance Scenarios**:

1. **Given** the documented toolchain, **When** the existing native build is run, **Then** its exit code, executable identity and submodule/patch revisions are recorded.
2. **Given** legally supplied compatible game data, **When** the reference capture is run, **Then** it records a corridor, an open view, a close character or viewmodel, and a moving-light/combat sequence, together with build, data, hardware and camera identities.
3. **Given** missing data, an unavailable GPU counter or a broken demo path, **When** a dependent check is attempted, **Then** it is SKIPPED with its reason; a synthetic or scripted alternative is identified without calling the skipped check a pass.

### User Story 2 - Verify the foundation without game data (Priority: P1)

A renderer developer can exercise a small, owned hardware interface on Windows and check actual rendering, compute and resource lifetime with synthetic fixtures before porting the game.

**Why this priority**: Device creation or a triangle alone does not establish the resources, synchronization and temporal inputs required by the renderer.

**Independent Test**: Build and run deterministic synthetic fixtures, validate their GPU readbacks and diagnostics, and separately exercise restart, resizing and resource retirement. No retail assets are required.

**Acceptance Scenarios**:

1. **Given** a supported D3D12 environment, **When** synthetic draw and compute fixtures run, **Then** checked readbacks demonstrate colour, multiple render targets and shader-readable depth, and all resource use obeys the declared lifetime and state rules.
2. **Given** multiple frames in flight, resize and shutdown, **When** resources and descriptors are retired, **Then** the tests wait for the corresponding GPU work and the enabled validation layer reports no unwaived errors.
3. **Given** a synthetic moving scene, **When** temporal inputs and the overlay are captured, **Then** velocity, validity, reset and scene-versus-overlay separation can be checked numerically; a zero vector is not treated as proof of valid history.
4. **Given** unavailable required hardware, SDK tooling or validation, **When** the fixture runs, **Then** it reports the specific unsupported or skipped check. A software adapter result is identified as such and is not reported as a hardware performance result.

### User Story 3 - Play a classic-compatible scene on the new path (Priority: P1)

A player can select the new Windows renderer and play a representative classic-data scene while retaining the existing renderer as a reference and fallback.

**Why this priority**: The foundation only becomes an engine renderer when it handles the game's scene, material and interaction semantics.

**Independent Test**: Capture the same named reference scenes on both renderers and exercise a recorded gameplay sequence, including the cases below. Compare against per-scene tolerances and exclusions fixed before reviewing the results.

**Acceptance Scenarios**:

1. **Given** a compatible local data set, **When** the new compatibility path renders the reference scenes, **Then** its documented image comparisons and gameplay checks meet the pre-recorded acceptance criteria.
2. **Given** mirrors or security views, perforated materials, projected/falloff lights, shadow exclusions, fog/blend stages, world GUIs and a viewmodel, **When** each is exercised, **Then** its behavior is preserved or an explicitly listed unsupported case is reported; it is never silently claimed as parity.
3. **Given** an unsupported GPU or an unavailable modern renderer, **When** a player launches the existing path, **Then** it remains usable without the modern backend or any optional neural provider.

### User Story 4 - Stabilize motion and keep the interface readable (Priority: P1)

A player sees temporally stable motion, readable terminals and a crisp overlay during movement rather than only in still images.

**Why this priority**: Correct previous rendered geometry, disocclusion and composition underpin reconstruction and later optional providers.

**Independent Test**: Use analytic motion fixtures and captured motion sequences to check reprojection, history invalidation, subview isolation and display-resolution overlay composition.

**Acceptance Scenarios**:

1. **Given** camera motion, rigid objects, animated characters and supported deformations, **When** motion is tested, **Then** previous-rendered positions reproject to the expected locations under a documented sign, units, jitter convention and predeclared numerical tolerance.
2. **Given** a cut, teleport, level change, resize, topology change, entity reuse or renderer/provider switch, **When** the next frame is rendered, **Then** incompatible history is invalidated rather than reused.
3. **Given** a mirror or shadow request and a skinned character, **When** shared geometry is regenerated, **Then** main-view previous-pose history stays private and advances once per rendered main frame.
4. **Given** HUD and world GUI text, **When** reconstruction or an authorized provider is enabled, **Then** the display overlay is composed afterward at display resolution and in-world text follows a documented, tested scene policy.
5. **Given** particles, transparency, reflections, newly visible pixels or unstable topology, **When** exact correspondence is unavailable, **Then** the renderer uses a documented rejection/mask policy rather than fabricating valid dense motion.

### User Story 5 - Deliver a quality-first playable look (Priority: P2)

A player can experience modern materials, lighting, shadows and atmosphere that preserve DOOM 3's recognizable dark art direction.

**Why this priority**: Visual quality comes from authored content and stable lighting, not from selecting a graphics API.

**Independent Test**: Review a local, legally usable hero set and moving scenes with the compatibility path and modern path identified in the evidence, using a recorded rubric for materials, lighting, darkness/readability, identity, motion and HUD. Record both accepted and rejected examples.

**Acceptance Scenarios**:

1. **Given** ordinary opaque materials and legacy special/translucent materials, **When** the modern path renders them, **Then** the former can use modern lighting while the latter retain a suitable forward or compatibility path.
2. **Given** authored materials, geometry, direct and indirect light, **When** the hero set and moving scenes are reviewed, **Then** the report records the material provenance, shadow/GI artifacts, readability, identity, motion stability, named hardware and the independent reviewer decision.
3. **Given** constrained memory, **When** the renderer exceeds its recorded resource budget, **Then** its documented residency or quality policy activates without unsafe eviction or an unreported failure.
4. **Given** no authorized neural SDK or supported neural hardware, **When** the modern renderer runs, **Then** its own reconstruction and complete off path remain available.

### User Story 6 - Gate optional neural finishing honestly (Priority: P3)

An integrator can evaluate DLSS 5 only when the actual selected backend, SDK, hardware and distribution terms are authorized, without making it a dependency of the default renderer.

**Why this priority**: A public sample on another API is evidence of that sample, not proof that this backend has an accessible or distributable integration.

**Independent Test**: Build and run the default renderer without NVIDIA components; separately, where access and clearance exist, demonstrate actual create/evaluate, masks, reset and on/off captures on the selected backend.

**Acceptance Scenarios**:

1. **Given** the default build, **When** its dependency and runtime artifacts are inspected, **Then** it neither bundles nor links a proprietary NVIDIA SDK and does not download one automatically.
2. **Given** an authorized provider and supported hardware, **When** the chosen backend's create/evaluate test runs, **Then** the report names the actual API, SDK version, driver, device, inputs, reset behavior and result; another API's sample does not count as this test.
3. **Given** missing access, hardware, permission or distribution clearance, **When** the provider is requested, **Then** the result is NOT RUN or unsupported, the independent renderer stays usable, and no DLSS 5 support or legal compatibility is claimed.
4. **Given** a proposed binary distribution, **When** its license review is recorded, **Then** it addresses the actual combined work and applicable SDK terms; out-of-tree placement or dynamic loading alone does not establish GPL compatibility.

### User Story 7 - Produce a path-traced showcase and reference (Priority: P3)

An artist or developer can compare the playable renderer with a higher-ceiling path-traced view and, once quality and motion gates pass, offer a playable showcase mode on capable hardware.

**Why this priority**: It is the requested long-term visual destination and a reference for diagnosing approximations, not a prerequisite for a playable hybrid renderer.

**Independent Test**: On named capable hardware, capture a converged static reference and a moving/skinned scene; inspect alpha-tested surfaces, emissive transport, noise, denoising, responsiveness and art direction against criteria recorded before review.

**Acceptance Scenarios**:

1. **Given** a static scene, **When** reference samples accumulate, **Then** captures and convergence measurements are archived with scene and configuration identity.
2. **Given** moving rigid and skinned geometry, alpha-tested materials and emissive surfaces, **When** the showcase runs, **Then** updates, material treatment, noise and latency are documented and tested; unsupported cases are explicit.
3. **Given** unavailable ray-tracing hardware or a failed visual/performance gate, **When** the showcase is selected, **Then** it reports the limitation and the playable non-path-traced renderer remains available.

### User Story 8 - Retain future portable profiles (Priority: P3)

A maintainer can add a later native or browser profile without changing game semantics or claiming the Windows backend automatically works on the web.

**Why this priority**: The repository's web objective remains important but must not force the Windows flagship to the lowest common denominator.

**Independent Test**: For each future profile, publish its capability/fallback table and run its own shader, build and real-scene checks on the target before claiming support.

**Acceptance Scenarios**:

1. **Given** a feature absent from a future profile, **When** that profile is selected, **Then** the documented fallback or explicit unsupported result is used.
2. **Given** only a desktop build or translated shader, **When** browser support is assessed, **Then** the project does not claim a web port until a browser build and real scene have been tested.

### Edge Cases

- A retail game, demo, BFG Edition or custom mod is not the supported data set: report the exact support boundary; never redistribute its assets or silently assume format compatibility.
- A custom legacy shader stage, unsupported material or light rule has no modern equivalent: retain a tested compatibility route or identify the unsupported case.
- A frame is minimized, resized repeatedly, or the device is removed: preserve safe resource lifetime and report the failure/recovery outcome.
- A scene view is rendered more or fewer times than simulation ticks, or appears in multiple subviews: keep history tied to rendered view identity and invalidate it when correspondence is lost.
- A particle, translucent layer or reflection has no unique previous surface: mark/reject it according to the documented policy, not as confidently valid motion.
- Debug validation, a supported adapter, legal data, proprietary authorization or a target-specific browser is absent: affected checks are SKIPPED/NOT RUN with a reason, never PASS.
- The target GPU, resolution and performance budget are not yet agreed: record measurements on named hardware before accepting a numerical budget; do not substitute an invented target.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST retain a runnable existing-renderer reference and record build, source/patch revision, data identity, hardware, commands and outcomes for each baseline claim.
- **FR-002**: The reference set MUST include the four scene categories in User Story 1, with scene/view identities and reproducible capture instructions; data-dependent tests MUST state SKIPPED when not run.
- **FR-003**: The flagship MUST use a bounded, backend-neutral custom RHI with a D3D12-first Windows backend. Scene and material interfaces MUST NOT depend on Windows-native handle types; any native provider bridge MUST be explicit and narrowly scoped.
- **FR-004**: The RHI and frame-resource design MUST express graphics, compute, multiple colour attachments, shader-readable depth, velocity/validity, persistent history, presentation and a separate overlay, without defining deferred shading or a neural provider as mandatory device operations.
- **FR-005**: The backend MUST define and test resource states, descriptor and upload ownership, command submission, fence-based retirement, resize/shutdown, and errors. Resources MUST NOT be reused or released while submitted GPU work still references them. The first executable slice MUST perform and test its own clear/copy transitions and fence retirement even though a general transition API belongs to a later slice.
- **FR-006**: The new renderer MUST provide a classic-compatible path and a documented way to select the existing path; classic parity MUST be measured separately from intentionally changed modern appearance. A failed modern-backend initialization MUST report an actionable result and leave the existing path selectable and usable; automatic switching is not assumed.
- **FR-007**: Compatibility checks MUST cover material stage ordering, alpha-tested coverage, projected/falloff lights, shadow and interaction exclusions, portals/subviews, world GUIs, fog/blend and viewmodel behavior; unsupported cases MUST be reported.
- **FR-008**: Motion MUST use previous **rendered** view and geometry state, a documented coordinate/sign/jitter convention and validity separate from zero motion. History MUST use stable identity and generation, and be invalidated on cuts, teleport, level, resize, topology, entity reuse and renderer/provider changes.
- **FR-009**: Main-view and subview histories MUST be isolated; previous skinned positions MUST be a private persistent copy that survives shared-geometry updates and cache deletion, and main history MUST advance once per rendered main frame.
- **FR-010**: Temporal handling MUST define and test policies for static, rigid, skinned, viewmodel, particle, transparent, deformed and newly visible content; absent correspondence MUST NOT be mislabeled as valid motion.
- **FR-011**: HUD/overlay composition MUST occur at display resolution after scene reconstruction and any authorized display-stage provider; in-world GUI treatment MUST be separately documented and tested.
- **FR-012**: The modern quality mode MUST support a hybrid opaque and forward/compatibility material path, linear HDR lighting, authored material inputs, shadowing, indirect lighting/reflections and controlled atmosphere. Legacy specular content MUST NOT be assumed to be ground-truth PBR data.
- **FR-013**: The project MUST define material/geometry provenance and a measurable residency policy covering mip or quality selection, upload throttling, resource/history budgets and fence-safe eviction. Game data and derived packs MUST NOT be tracked.
- **FR-014**: The independent renderer MUST have a usable non-proprietary reconstruction/off path. NVIDIA SDKs and binaries MUST NOT be linked, bundled or automatically downloaded in the default build. Evidence MUST include observing a clean default configure/build for attempted downloads, not only inspecting the final artifact or dependency list.
- **FR-015**: A DLSS 5 claim MUST require authorized SDK access, actual create/evaluate on the selected backend and hardware, a documented input/mask/reset contract, on/off evidence and a distribution/legal review. A sample on another backend or out-of-tree loading alone MUST NOT satisfy these gates.
- **FR-016**: The programme MUST include a path-traced reference/showcase with static and dynamic/skinned geometry, material-aware alpha/transmission and emissive handling, documented denoising, convergence and latency tests, and a non-path-traced fallback. It MUST NOT block earlier playable milestones.
- **FR-017**: Each future native or browser profile MUST publish feature capabilities, explicit fallbacks and target-specific validation; no desktop compilation or shader translation alone constitutes a web-port claim.
- **FR-018**: Every parity, visual-quality or performance result MUST name the scene, build, relevant data identity, hardware, driver, configuration and command; thresholds and exclusions MUST be recorded before a result is judged.
- **FR-019**: The pristine engine submodule MUST remain read-only; engine changes, target copies, reviews, issue/worktree ownership and eventual submodule updates MUST follow the repository constitution and workflow.

### Key Entities

- **Reference set**: Scene/view identifiers, reproducible commands, legal local data identity, captures and build/hardware metadata.
- **Capability profile**: A target backend's supported operations, tested fallback decisions and explicit unsupported features.
- **Frame and view history**: Per-view current/previous rendered state, stable geometry identity, generation, validity and reset reason.
- **Material and content record**: Source provenance, permitted use, authored parameters, quality tier and residency metadata; not the game asset itself.
- **Provider evaluation**: Actual API, SDK/runtime version, authorization, hardware, inputs, reset behavior, results and distribution review.
- **Evidence record**: Commands, exit status, diagnostics, captured artifacts, thresholds, exclusions and reviewer decision.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A fresh native baseline report records the build result and all four reference scene categories captured twice against predeclared repeatability tolerances; each unavailable data-dependent capture is explicitly SKIPPED rather than counted as captured.
- **SC-002**: Synthetic foundation tests independently verify draw, compute, multiple colour outputs, readable depth, motion/validity, overlay separation and fence-safe retirement, including readback row-pitch correction and a release attempt while a fence is still outstanding; every enabled validation check reports zero unwaived errors.
- **SC-003**: A representative playable classic-data scene runs on the new path and on the retained reference; all cases in FR-007 have an evidence entry marked pass or explicitly unsupported, with acceptance thresholds fixed before comparison.
- **SC-004**: Analytic camera, rigid, skinned and supported-deformation motion tests verify reprojection against predeclared tolerances and invalidation; separate tests verify subview isolation, shared-pose lifetime and display-resolution overlay ordering.
- **SC-005**: The modern path has a reviewed hero content set and the four scene categories, with recorded decisions for the six quality dimensions in User Story 5 and measured resource/frame behavior on named hardware.
- **SC-006**: A clean default configure/build and run are observed to work without proprietary NVIDIA SDKs or attempted NVIDIA downloads; an authorized provider, if claimed, has a successful create/evaluate record for its actual backend and separate distribution clearance.
- **SC-007**: The path-traced mode produces a static convergence record and a moving/skinned capture with noise, material and latency evaluation; its non-path-traced fallback remains usable.
- **SC-008**: Each claimed future target has its own capability/fallback table and a successful target-specific real-scene test; absent checks are not counted as support.

These are final programme outcomes, not claims that every stage has already been built. The plan and task list MUST expose stage-level prerequisites and leave unimplemented outcomes unchecked.

## Assumptions

- Windows 11 x64 is the first flagship environment; the GPU, driver, memory, frame-time and resolution budgets remain to be measured and agreed.
- The existing native port and its historical demo captures provide a reference, but a fresh baseline and whole-game compatibility are not established by those historical records.
- Game/demo data and any HD content are supplied locally under appropriate rights. Ownership alone does not grant modification or redistribution rights; the project distributes neither those assets nor derived packs.
- A small custom RHI is a deliberate ownership choice, not evidence that it produces better pixels than NVRHI. The implementation may revisit that decision after comparative evidence.
- Access to an authorized neural SDK, its actual backend interface, appropriate hardware and distribution clearance is unconfirmed. A failed or absent provider gate does not block independent rendering work.
- Vulkan, WebGPU and WebGL/GLES are possible future profiles, each requiring its own implementation and evidence; the existing web roadmap is not fulfilled by this feature's Windows foundation.
- Reference image tolerances, visual acceptance and performance budgets are chosen and recorded before the respective review, after baseline measurements; no target is inferred from vendor claims.

## Out of Scope

- Changing gameplay, audio, networking or save formats as part of renderer modernization.
- Committing game data, derived texture packs, GPU binaries, build output, secrets or machine-specific paths.
- Promising DLSS 5 availability, API signatures, distribution rights or RTX 50 performance without the separate evidence and legal gates.
- Treating an optional external injector, a GL3-first rewrite or a Vulkan backend as a prerequisite for the D3D12 flagship.
- Claiming that a synthetic GPU fixture, path-traced still or desktop shader compile constitutes a playable renderer, complete-game compatibility or a browser port.
