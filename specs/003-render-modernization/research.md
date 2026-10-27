# Rendering Modernization Research — Quality-First DOOM 3

**Reviewed**: 2026-10-27

**Pristine source**: `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, read-only
**Question**: What are the shortest experiment, shortest integrated route, and highest-ceiling route to a
modern DOOM 3 renderer, when development time is not the deciding constraint? Can a custom RHI with a minimal
D3D12 backend be the foundation for motion vectors, deferred shading, and DLSS 5?

**Status**: Research recommendation, not an implementation plan or approval to modify the engine.
The accompanying `spec.md` still mandates GL-first and explicitly excludes DX12-first. Section 11 lists the
changes needed before `/speckit.plan`; this review changes only `research.md`.

## Evidence convention

`[repo: path:line]` identifies locally inspected source or tracked evidence. Paths under `neo/` are relative to
the pinned pristine submodule. Recorded runtime evidence is not a fresh build or run in this review.
`[S-name]` links to the source table in section 13. `[proposal]` identifies an architectural or aesthetic
judgement, not demonstrated functionality.

| Research grade | Meaning | Use |
|---|---|---|
| R1 | Licence text / governing terms | Cite exact section; interpretation is not a legal ruling |
| R2 | Published technical research | Can support an algorithm, not this engine's unmeasured performance |
| R3 | Vendor/project documentation, source, releases | Direct implementation evidence; **for reference only** for broader quality/performance claims |
| R4 | Community reports or unverified prior claims | **For reference only**; never sufficient for a product promise |

The earlier document's G1 label mixed licences, vendor marketing, SDK source, and mod pages. Those are not
equally strong evidence. A sample's API call proves an implementation exists; it does not prove access,
licence compatibility, runtime correctness here, or artistic improvement.

---

## 0. Conclusion first

**[proposal] Choose a custom, bounded RHI with D3D12 as the first flagship backend. Build motion vectors and
explicit frame resources from the beginning, preserve a classic compatibility rendering mode, then add a
hybrid deferred/forward PBR renderer, controlled indirect lighting, and optional DLSS 5 Neural Rendering.**

This is not a recommendation to spend the most effort. It is a recommendation to own the parts that control
the result: scene/material semantics, frame composition, lighting, temporal correctness, and diagnostics.
Reuse DXC, D3D12MA, and proven implementation references rather than writing a compiler or allocator.
There is no demonstrated NVRHI deficiency forcing this choice. Custom RHI is a deliberate architecture-
ownership choice under the requested design-foundation goal, not a prerequisite for superior pixels.
If the sole objective is final image quality, NVRHI plus the same renderer algorithms is equally credible.

1. **Shortest visual experiment**: existing native GL build + legal local HD assets; optionally an independently
   vetted external neural-rendering bridge. Useful for hypotheses, not a required gate for engine design.
2. **Shortest native integrated route**: existing classic-data engine + NVRHI D3D12 + re-authored HLSL
   interactions + real depth/MVs + separate HUD + an authorised NR adapter. Deferred shading is not required
   to feed NR. A proven classic-data Vulkan fork could be shorter if its runtime parity is actually verified;
   that verification is missing, so no unconditional winner is established.
3. **Recommended high-ceiling route**: custom RHI/D3D12 + classic compatibility + hybrid deferred/forward +
   PBR/HD geometry and materials + dynamic shadows/GI/reflections/volumetrics + temporal reconstruction +
   optional NR. Keep a high-quality result with NR off.
4. **Longest meaningful extension / maximum transport ceiling**: the same foundation grows a DXR path-traced showcase/reference mode with
   physically authored materials, emissive transport, dynamic geometry, and denoising. It is a stretch mode,
   not a mandatory detour before a playable remaster.

**[proposal] If “most impressive” means maximum ceiling with no schedule constraint, choose route 4 as the
destination. If it means the strongest playable DOOM 3 remaster, route 3 is the foundation and route 4 its
showcase/reference extension.** Neither path tracing nor NR is guaranteed to win the moving-scene art review.

**MV means motion vectors here. MV, deferred shading, and DLSS 5 are different axes, not alternatives.**
Forward rendering can emit MVs; deferred rendering does not magically provide correct previous animation.
The NVIDIA sample passes depth and MVs to NR without passing its PBR G-buffer [S-nr-source].

**What makes it impressive [proposal]:** a moving, playable scene with convincing metal, skin, light transport,
stable shadows, legible darkness and crisp HUD—not a D3D12 triangle, more API backends, or an attractive still
whose details drift during combat. API choice enables the work; it is not an aesthetic result.

## 1. Review: what the previous recommendation got wrong

| Previous claim/default | Review finding | Correction |
|---|---|---|
| Native x64 is red on OggVorbis assembly | Stale: `ea2e02e` landed native port work; README has demo captures and working ARB2 status | Preserve the successful port; re-run its verifier before implementation, not before admitting its existence |
| Rebase on dhewm3 to solve the red build | Its motivating failure no longer describes the repository | A base change is optional; require concrete benefits and classic-data/mod compatibility evidence |
| DX12 is invalid because it excludes web/mobile | Confuses backend reach with renderer architecture | Desktop D3D12-first is valid; retain separate portable profiles and a future backend boundary |
| RHI only when a second modern backend ships | Too restrictive for this goal | Resource ownership, frame graph, temporal buffers and SDK interop justify a bounded RHI from the first backend |
| GL3 parity must precede Vulkan/DX12 | An unnecessary serial detour for a Windows flagship | Retain the existing GL reference; implement D3D12 directly alongside it |
| Vulkan is preferred solely because NR exists there | Vulkan support disproves DX12 exclusivity; it does not select the best development platform | Pick DX12 for this Windows-first programme and PIX-oriented workflow; Vulkan remains a valid later backend |
| External injector must prove the look before any backend work | An estimated-MV experiment cannot invalidate the value of correct temporal rendering or PBR | Optional experiment; no dependency from D3D12 bring-up to injector success |
| NR/SR/RR/MFG share one D3D12-only contract | Incorrect grouping; Streamline also supports Vulkan, and NR differs from SR/RR | Separate contracts and independently probe capability/access |
| Out-of-tree, user-installed DLSS module solves GPL | File location/loading mechanism is not a legal exemption | Keep default build clean; SDK access and distribution clearance are independent gates |
| 4–8 / 9–18 months attributed to NVIDIA's sample | Sample supplies code, not engineering-effort estimates | Remove the ranges and unsupported attribution; order work by dependency and evidence |

This audit includes **underclaims**: the former document omitted a credible custom DX12-first design,
deferred/forward coexistence, temporal animation ownership, and a path-traced reference mode. Adding those
options is not permission to claim they are built.

## 2. What exists, and what must change

### 2.1 Native baseline

`README.md:3-8,23-27` records the native x64 CMake/VS2022 build, ARB2-rendered demo levels, cinematics, HUD and
stencil shadows. `docs/media/01-mars-city-control-room.jpg` and five further captures document output.
`specs/001-win11-amd64-native-build/review.md:27-39,104-109` records an x64 PE (`0x8664`) and a running demo, but
also historical verification gaps and no fresh rebuild by that reviewer. Do not equate it to full-game,
save-game, render-demo playback, or cross-GPU validation.

This review checked the pristine SHA and clean submodule status. It did **not** build/run the engine, provision
data, capture new frames, or measure performance. Tracked screenshot evidence means the old blanket claim
that no data or rendered baseline exists was wrong; current local data availability still needs checking.

Before implementation, re-run the existing native verification with legally supplied data and preserve its
exact command, build/patch identity, GPU/driver and output. Do not depend on `timedemo` until the known demo
serialization limitations are resolved or an alternative scripted harness is proven
[repo: README.md:27,38-44; specs/001-win11-amd64-native-build/review.md:88-94].

### 2.2 Renderer integration seam

| Existing structure | Evidence | New renderer responsibility [proposal] |
|---|---|---|
| `drawSurf_t`: geometry, space, material, evaluated registers, scissor, light link | `neo/renderer/tr_local.h:101-122` | Extract immutable draw packets with typed buffers/materials; preserve stage ordering |
| View command queue (`RC_DRAW_VIEW`, set/copy/swap) | `tr_local.h:472-501` | Adapt view/copy/present commands, not every `qgl*` call; replace raw GL buffer enum |
| `viewDef_t`: projection, mirror/subview, lights, portals | `tr_local.h:367-431` | Explicit view identity, resources, depth convention and history; preserve portal visibility |
| `viewEntity_t`: current model/model-view and depth hacks | `tr_local.h:338-362` | Previous rendered transforms/geometry and viewmodel projection policy |
| Entity definition stores current parameters/model matrix and dynamic cache | `tr_local.h:237-290` | Stable entity identity plus generation; history must outlive frame-allocated surfaces |
| Light carries projection, falloff, shadow/interaction groups | `tr_local.h:298-335,434-459` | Preserve cookies, falloff, exclusion rules and per-stage colours; do not turn every light into a generic point light |
| MD5 CPU skinning reuses and overwrites triangle vertices | `Model_md5.cpp:275-347` | Snapshot previous posed positions before overwrite, or later skin current/previous joint palettes on GPU |
| Screen GUI becomes a 2D view command | `GuiModel.cpp:226-297` | Route overlay views to a display-resolution composite, after scene reconstruction/NR |
| In-world GUI is emitted into the current scene | `tr_guisurf.cpp:128-181` | Keep terminals in-world; separately mask readable text for NR if needed |

`VertexCache::Position` represents an offset as a pointer; `idImage` owns GL binding/copy behaviour. Those
must become typed buffer slices and backend-owned texture resources, not D3D12 casts of existing GL handles
[repo: neo/renderer/VertexCache.h:74-76; Image_load.cpp:1697-1783,1853-1918].

No numeric GL-call or RHI-function budget is used: the earlier audit's compilation/counting method was not
preserved well enough to reproduce it in this review. Missing ARB shader sources remain a migration issue:
use legally supplied shader data as local ground truth or re-author shaders; do not commit retail shader data.

## 3. Alternatives: distinguish route length from quality ceiling

All rankings in this table are **[proposal]**, not benchmark results or measured schedules.

| Route | What is owned/reused | Intended result (not built here) | Main limitation | Role |
|---|---|---|---|---|
| A: existing GL + HD materials | Existing native build; local assets | Sharper classic appearance | No new physical lighting model merely from textures | Shortest visual baseline |
| B: external NR bridge | Injector/interoperability tooling | Possible NR A/B experiment | Access, trusted binaries, estimated MVs, HUD and driver dependence; untested here | Optional hypothesis test |
| C: classic engine + NVRHI D3D12 | Reuse backend/lifetime machinery; own engine adapter/HLSL/temporal passes | Native explicit API + real MVs + NR-ready frame | Still must port material semantics; not automatic NR support | Shortest bounded Windows integration candidate |
| D: adopt verified modern classic-data fork | Reuse more renderer implementation | Potentially fewer missing steps | Runtime parity/build quality/provenance not established here | Compare only after a reproducible adoption spike |
| **E: custom RHI + D3D12 + hybrid renderer** | Own contracts, frame graph, passes, material/lighting pipeline; reuse tools | **Architectural control and a distinct playable remaster** | Greater responsibility for GPU correctness and maintenance | **Recommended flagship** |
| F: E + path-traced showcase | Own full transport/material integration and denoiser contracts | Highest transport/reference ambition | Noise, dynamic BLAS, legacy-material conversion and art direction | Longest meaningful extension |

NVRHI is a particularly useful R3 reference: its MIT implementation covers D3D11/D3D12/Vulkan, graphics,
compute/RT pipelines, optional state tracking, upload management and safe resource lifetime. It does not
provide GL/WebGL; its device is wrapped around an application-created native device [S-nvrhi]. It is **not**
disqualified for the desktop profile just because it lacks web. Conversely, replacing it with custom code
does not inherently improve pixels.

### 3.1 Custom RHI versus NVRHI [proposal]

| Criterion | NVRHI + own frame graph/renderer | Custom bounded RHI |
|---|---|---|
| Same PBR/GI/NR algorithms | Can implement them | Can implement them; no inherent image advantage |
| Ownership | Own DOOM adapter and passes; depend on NVRHI contracts | Own resource/binding/submission contracts and diagnostics |
| GPU correctness | Reuse lifetime/barrier machinery; still validate engine integration | Prove lifetime/barrier correctness ourselves; larger maintenance responsibility |
| Native provider interop | Documented native-object access exists | Define a constrained native bridge; not a unique feature advantage |
| Future constrained web profile | Separate backend/profile required; NVRHI has no GL/WebGL | Can design neutral contracts, but still must build/prove each backend/profile |
| Decision for this request | Best reuse-first alternative, not rejected on image quality | Preferred design-foundation exercise; keep small and prove it with Q1–Q3 |

Do not invent a missing NVRHI feature to justify this preference. Compare a representative fixture/prototype
and architecture surface in the later plan; allow NVRHI to replace the custom implementation if ownership
brings no demonstrated benefit. The shortest integrated candidate and maximum visual ceiling remain separate.

dhewm3, DUDE and RBDOOM-3-BFG remain prior-art candidates, not accepted dependencies. BFG-based rendering
algorithms can inform the design without assuming classic DOOM 3 data is drop-in compatible. Their previously
listed release dates, performance and effort numbers are not revalidated by this review.
NVRHI's README names RBDOOM-3-BFG as a user [S-nvrhi], making it a directly relevant DOOM 3-family integration
reference for the reuse-first option; that does not resolve BFG-versus-classic data/game compatibility.

## 4. Custom RHI: minimal implementation, modern contract

### 4.1 Keep three layers separate [proposal]

```text
idTech4 scene/material adapter
  -> immutable frame/view/draw/light packets + persistent history
  -> renderer passes / frame graph / quality profiles
  -> RHI: resources, views, pipelines, bindings, commands, submission, capabilities
  -> D3D12 backend first; Vulkan or a constrained web backend later

optional reconstruction / neural providers
  -> declared external frame-graph passes, not hardcoded RHI methods
```

Deferred shading is a **renderer algorithm**, not an RHI method. DLSS 5 is a **provider integration**, not a
device requirement. The RHI exposes the resources/commands they need while a default renderer operates
without NVIDIA components. Keep Windows handles and native COM types out of ordinary renderer packets.

### 4.2 First D3D12 slice

These are proposed requirements for a later plan, not implemented API names.

| Component | Minimum to implement first | Growth without redesign |
|---|---|---|
| Host/device | Adapter selection, capability report, DXGI flip presentation, resize/minimize, named failures | Optional Agility version when a chosen feature requires it |
| Submission | One direct queue, bounded frames in flight, allocators/lists, fence retirement | Multiple recording threads/queues later; no async-compute requirement initially |
| Resources | Typed buffers/textures, upload/readback, 2D/cube/mips, RTV/DSV/SRV/UAV views, explicit ownership | Persistent histories, structured light data, optional acceleration structures |
| Bindings | Documented layouts, root signatures, versioned descriptor allocation, buffer slices | Capability-gated bindless; no global mutable texture-unit emulation |
| Pipelines | HLSL via DXC, graphics and compute PSOs, depth/stencil/blend/raster/layout/RT-format keys | Shader/cache versioning; RT pipelines only with DXR profile |
| State | Per-resource/subresource transitions and UAV ordering; start without aliasing | Frame graph compiles declared accesses; aliasing only with lifetime tests |
| Frame outputs | Scene colour offscreen, shader-readable depth, RG16F-class velocity, overlay target | Multiple G-buffer targets, reconstruction/NR input/output, control masks |
| Diagnostics | Names/markers, GPU timestamps, readback, debug layer, GBV fixtures, DRED crash record | Frame graph/resource/history viewer and capture automation |

Use D3D12MA for allocation and DXC for shader compilation; their documented capabilities are rechecked at
pinned commits [S-d3d12ma; S-dxc]. The smallest triangle spike may use committed resources; a production slice
still needs ownership/budget accounting. D3D12MA does not solve descriptors, barriers or frame-history lifetime.

**Non-negotiable lifetime rule:** allocators, descriptor ranges, upload pages and resources cannot be recycled
while submitted GPU work references them. Microsoft's allocator and descriptor guidance makes those explicit
application responsibilities [S-ms-basic; S-ms-descriptors; S-ms-fences]. A C++ pointer's lifetime is not a fence.

**Native interop [proposal]:** an authorised provider may need native device/list/resource handles. Expose
those through a restricted backend bridge: declare inputs/outputs and required states, invoke the provider,
then restore or invalidate cached binding/state assumptions. Do not hand SDK calls invisible ownership of
the frame graph. This bridge is a technical boundary, not a GPL exemption.

### 4.3 Do not make the minimum a dead end

Include compute dispatch, explicit resource usages, multiple colour attachments, readable depth,
persistent histories, capability negotiation and native-provider boundaries in the contract early. The first
playable implementation does **not** need bindless, mesh shaders, sparse textures, multi-queue scheduling or
path tracing. The previous GL-only interface sketch (texture units 0..7, gamma LUT, swap) is too narrow for
this flagship; there is no evidence for a fixed 60–80-entry interface budget.
The table above describes **D3D12 lowering**, not neutral public RHI vocabulary: use attachment views,
binding layouts/sets and presentation targets in the RHI, and translate them to RTV/DSV, root signatures,
descriptor heaps and DXGI in the backend. Do not expose those Windows types to scene/material packets.

## 5. Motion vectors are a foundation, not a later patch

### 5.1 Proposed engine velocity ABI

- Store **current and previous rendered** camera state and posed geometry. Simulation tick history alone is
  insufficient when a tick is rendered multiple times or several ticks occur before one render.
- Key history by stable world/entity/surface identity and generation, not `drawSurf_t*` or recycled pointers.
- Keep this history in renderer-owned side tables, not fields added blindly to `renderEntity_t`: entity
  slots are reused and current parameters are compared bytewise [repo: RenderWorld.cpp:180-193,224-232,299-329].
- Advance main-view history once per rendered frame. Mirrors/security views must not overwrite it.
- Define velocity as **previous unjittered UV minus current unjittered UV**, origin top-left; convert sign,
  units and jitter treatment in each provider adapter after checking that provider's actual SDK contract.
- Track validity/disocclusion separately from a zero MV: a stationary surface can legitimately have zero.
- Reset/invalidate on cuts, teleports, level load, entity reuse, topology changes, resize/resolution-mode changes
  and renderer/provider switches; do not reuse incompatible history silently.

Illustrative mathematics (not existing shader code):

```text
c = current_unjittered_view_projection * current_model * current_posed_position
p = previous_unjittered_view_projection * previous_model * previous_posed_position
uv(v) = (0.5 * v.x / v.w + 0.5, 0.5 - 0.5 * v.y / v.w)
velocity_uv = uv(p) - uv(c)
provider_velocity = convert_sign_units_and_jitter(velocity_uv, provider_contract)
```

Handle invalid `w`, clipping and newly visible samples explicitly. The GPU test compares reprojection against
analytic positions; colour-wheel inspection alone does not prove the sign or scale is correct.

### 5.2 Geometry coverage

| Geometry | Required previous state [proposal] | First implementation |
|---|---|---|
| Static world | Previous camera; static local/world position | Depth-derived camera MV or dedicated velocity pass |
| Moving rigid props | Previous camera and object transform | Per-draw current/previous matrices |
| MD5 characters | Previous posed vertex positions or previous joints **and** object transform | Retain CPU-skinned positions before overwrite; GPU skinning is a later optimisation |
| Weapon/viewmodel | Previous pose plus matching depth-hack/projection convention | Dedicated viewmodel velocity policy; never blindly reuse world depth reconstruction |
| Particles/billboards | Stable particle identity, previous orientation/position/time | Explicit coverage and rejection/reactive policy when no correspondence exists |
| Deforms/overlays/decals | Previous deformation/time, attachment/topology identity | Snapshot or recompute consistently; invalidate topology changes |
| Mirrors/security screens | Per-subview state if temporally processed | Main display NR only initially; don't run NR recursively |
| Transparent layers | A single velocity/depth cannot represent all blended surfaces | Define dominant-surface/reconstruction mask policy; expose limitations |

The source's in-place MD5 skinning is why “just save previous matrices” is inadequate
[repo: Model_md5.cpp:285-329]. Alpha-tested coverage must agree between depth, colour and velocity; the
background should supply velocity where a grate is discarded. Reflective apparent motion and transparency
are not solved by ordinary surface MVs—handle them in reconstruction policy, not false claims of dense truth.

**MD5 ownership/order [proposal]:** previous posed positions must be a private, persistent history copy from
the last rendered frame, never an alias into `cachedDynamicModel`. Freeze it before the first model
instantiation of a new frame, including shadow/subview requests: `InstantiateDynamicModel` can delete the
cache (`Model_md5.cpp:722-745`), and surfaces can be shared across views (`tr_local.h:101-106`). Capture a
private current pose for the main view as it is generated, before a later mirror/shadow/subview can rewrite
shared geometry. Commit that copy once after main-frame extraction; never advance history per view or per
simulation update. Invalid/new correspondence starts invalid history, not borrowed stale vertices.

## 6. Hybrid deferred/forward: recommended flagship shading

**[proposal] Use deferred shading for ordinary opaque PBR surfaces, and a forward/compatibility path for
transparency, unusual legacy stages, world GUIs and special materials.** Do not force all `.mtr` stages into
a fixed G-buffer. Clustered light assignment can serve both paths; the published method covers deferred and
forward rendering [S-clustered, R2]. That supports feasibility, not a DOOM 3 speedup prediction.

### 6.1 Two rendering modes, one foundation

1. **Classic compatibility mode** re-authors the original interactions in HLSL, preserving stage registers,
   projected lights/falloff, stencil/exclusion rules, portal/subview behaviour and blend order. Compare against
   the working GL path. Keep the legacy depth convention at the boundary until the new convention is tested.
2. **Modern quality mode** opts materials/lights into linear HDR PBR, roughness/metalness or specular workflow,
   emissive transport, shadow maps and indirect lighting. It has its own reviewed reference images. Pixel
   equality to the classic frame is not a quality criterion once the lighting model intentionally changes.

Preserve `noSelfShadow`/local/global interaction intent, light-linked surfaces and animated light materials.
Naive clustered “all lights touch all surfaces in the cell” breaks those semantics. Either encode membership
and exclusion data or retain special lights/materials on compatibility passes
[repo: tr_local.h:330-334,434-459]. Document intentional changes rather than calling them parity.

### 6.2 Proposed frame flow

```text
scene extraction + rendered-history snapshot
  -> dependent subviews / dynamic geometry preparation
  -> depth + alpha coverage + velocity
  -> opaque G-buffer
  -> light assignment + shadow visibility (maps initially; DXR optional later)
  -> direct PBR + indirect diffuse/specular lighting
  -> forward transparency / legacy specials / in-world GUIs
  -> volumetrics + scene effects
  -> own TAA/TAAU OR authorised SR/RR appropriate to the rendering mode
  -> tone map / colour management -> display-resolution scene LDR
  -> optional authorised DLSS 5 NR -> display-resolution overlay/HUD -> present
```

This is logical dependency order, not a fixed scheduling mandate. Some work may overlap. The classic path
can skip G-buffer/GI and use the same colour/depth/MV/HUD resources. RR is a separate denoising/reconstruction
branch for suitable ray-traced signals, not a second stage to blindly stack after ordinary TAA.

### 6.3 Where the visual gain comes from [proposal]

| Layer | High-value work | Proof needed |
|---|---|---|
| Materials | De-lit base colour, authored roughness/specular, normal conventions, metal/dielectric separation, emissive, mip stability | Turntables in multiple lighting conditions; BRDF/furnace fixtures; tiling/mip captures |
| Geometry | Better silhouettes, believable bevels, selective character/prop refinement, tangent/skin consistency | Close and moving views; preserve gameplay collision/identity |
| Direct lighting | Artist-controlled contrast, soft shadow quality, projection/falloff fidelity | Moving lights, doors, alpha-test shadows, noSelfShadow fixtures |
| Indirect lighting | Local probes/occlusion baseline; dynamic RT GI/reflections later | Door-state light leakage tests, offscreen reflectors, emissive response |
| Atmosphere | Restrained fog/shafts, exposure and bloom | Preserve darkness and readability; no flat grey lift of all shadows |
| Temporal image | Correct animation MVs, stable AA, disocclusion handling | Combat/movement video, not just stills |
| Neural finishing | Explicit per-material exclusion/control, NR on/off comparison | Characters/monsters retain identity; terminals/HUD stay readable; no invented combat cues |

Filament's documented BRDF/material/IBL implementation is a reference [S-filament, R3, for reference only];
use the underlying equations and validation rather than copying its mobile quality choices as a desktop
ceiling. A legacy specular texture is not ground-truth roughness/metalness, and upscaling cannot recover missing
silhouette geometry. D3HDP or other user-owned HD packs can be local corpora, not licensed repository assets.
Their DDS formats must be inspected from headers; old pack-size-to-VRAM guesses are not budgets.
Add an explicit residency/streaming policy in the plan: texture priority/mip selection, upload throttling,
history/render-target budget accounting, over-budget behaviour and fence-safe eviction. D3D12MA allocation
and budget reporting are inputs to that policy, not a streaming implementation. Quality-first does not mean
unbounded VRAM or paging hitches.

## 7. DLSS 5: design for it without pretending access exists

### 7.1 Current evidence (2026-10-27)

- NVIDIA announced DLSS 5 in March; its September 1 article reports availability in NBA 2K27 on RTX 50 and
  GeForce NOW [S-nr-launch, R3, for reference only]. This is vendor release evidence, not our quality test.
- NVIDIA's current hardware table marks 3D-Guided NR for RTX 50, not RTX 40/30/20 [S-nr-hardware]. The review
  also found an October RTX Spark availability update [S-nr-update]. Probe the actual device/driver and feature;
  do not treat those services/products as a custom-engine deployment guarantee.
- The official `vk_gltf_renderer` source at `10f2de09dfa3dca04f14e485580862f17c05752f` includes
  `nvsdk_ngx_helpers_dlssnr_vk.h`, probes `NVSDK_NGX_Feature_DLSSNR`, and calls Vulkan NR create/evaluate
  [S-nr-source]. Thus **NR is not D3D12-only**.
- Its CMake sets `DLSS_VERSION` to `310.9.16`, but disables NR when the beta header is missing; it even
  references an internal beta SDK override [S-nr-cmake]. Public source is not a turnkey public NR SDK.
- Public Streamline **2.14.16** exposes `kFeatureDLSS_NR = 1004` and uplift input/output/mask tags. The public
  package review found no dedicated NR plugin/API header/guide. The
  core header does exist and reserves NR identifiers; that is not a complete NR API header. Its general API
  supports Vulkan as well as DirectX, not OpenGL [S-streamline]. NR-specific D3D11 support is unverified.

**D3D12 NR remains a target integration, not something built or API-tested here.** Use the SDK/version actually
provided under authorisation; do not invent D3D12 NR function signatures by renaming the Vulkan sample calls.
**Backend-specific NR gate [proposal]:** obtain and demonstrate NR create/evaluate on D3D12 before claiming
the D3D12 flagship supports DLSS 5. If the authorised drop is Vulkan-only, either add/select a Vulkan NR
profile (if DLSS 5 is mandatory) or explicitly ship the D3D12 flagship without DLSS 5 pending access.
Neither a Vulkan sample nor a generic Streamline D3D12 capability flag satisfies this gate. Backend-neutral
RHI contracts allow this response without discarding the material/lighting/history work.

### 7.2 Sample-derived contract—not a normative cross-API spec

| Item | What the pinned Vulkan source actually does |
|---|---|
| Colour | Tonemapped LDR scene at display size; source uses UNORM image, separate float output, then blits back |
| Depth | Render-resolution depth: linear ViewZ on RR host; reversed hardware depth on SR host, with the corresponding inversion flag |
| MVs | Render-resolution RG16F screen-space MVs; adapter supplies display/render scaling |
| Mask | Optional per-material control mask plus automatic-mask/art controls |
| History | NR owns internal state; explicit reset and feature recreation |
| Not passed to NR | Jitter, TAA history texture, exposure texture, albedo, normals or roughness |

Evidence: `src/dlss.cpp:468-485,518-618,625-651`; tonemap-before-NR in `src/renderer.cpp:1652-1661`
[S-nr-source; S-nr-order]. “One frame in/out” does **not** mean stateless; the source explicitly manages reset.
For a clean raster frame the sample allows NR independently of SR. Its path-tracer host blocks NR when RR
is off because it needs denoised input. This host policy is not a requirement that every engine implement RR.

| Feature | Distinct responsibility | This design |
|---|---|---|
| NR / DLSS 5 | Neural appearance enhancement after tone mapping | Optional display-stage provider; depth/MVs/masks; authorised SDK needed |
| SR / DLAA | Temporal AA/reconstruction/upscaling | Separate provider; current colour/depth/MVs and documented jitter/reset/exposure handling; SDK owns internal history |
| RR | Ray-traced signal denoising/reconstruction | Optional RT branch with guide buffers; ordinary G-buffer is not automatically sufficient |
| FG / MFG | Generated presentation frames | Separate latency, HUD and pacing integration; generated FPS is not simulation FPS |

The renderer may own exposure/jitter/history for its own TAA, HDR and GI even though NR does not consume those
as explicit inputs. “NR does not need a G-buffer” is not a reason to omit the G-buffer needed by deferred PBR.

### 7.3 Access, hardware, licensing and art are separate gates

1. **Access:** obtain authorised SDK/plugin, supported API contract and runtime; do not source neural consumers
   or DLLs from anonymous downloads, bypass checks, or assume a feature ID is the feature implementation.
2. **Hardware:** query feature capability on supported NVIDIA hardware/driver. No hardware means **NOT RUN**,
   not PASS or a fake software replacement claiming DLSS 5.
3. **Distribution:** NVIDIA RTX SDK section 4e restricts uses subjecting the SDK to open-source terms;
   supplement section 1 restricts DLSS/NGX to NVIDIA GPUs [S-rtx-licence, R1]. GPLv3 sections 1/5/6/10 govern
   corresponding source, combined works and restrictions [S-gpl, R1]. Out-of-tree/dynamic loading/user install
   do not themselves establish compatibility. Separate-process generic interop is a risk-reduction candidate,
   not a proven legal solution. Seek applicable permissions and counsel before distribution; NVIDIA agreement
   alone cannot waive rights of other copyright holders.
4. **Art:** record masks/style settings, identity/readability failures and on/off videos. Vendor claims of
   stability/photorealism are not our acceptance evidence.

Default build: complete renderer, no NVIDIA SDK linkage/bundled binary/auto-download, native AA/reconstruction
fallback. If NR access or legal clearance fails, continue PBR/GI/temporal development. An NR-required product
claim remains blocked until a real, authorised integration passes its own tests.

## 8. Re-listed paths: shortest, longest, and recommended

“Shortest” means fewer prerequisite transformations, **not a time promise**. Current workspace evidence is
insufficient to prove one third-party fork is the absolute shortest implementation. No AI acceleration factor
or engineer-week conversion is asserted.

### 8.1 Shortest visible route — an experiment, not the foundation

```text
existing native x64 GL baseline -> verified local HD material corpus -> A/B stills + moving capture
  -> optional vetted external NR experiment if authorised access/hardware/tooling exist
```

No RHI, deferred shading, or full engine rewrite is needed to test sharper materials. External bridge claims
are R4 and untested here [S-feeder, for reference only]. Estimated optical-flow MVs are not equivalent to
engine geometry MVs. A bad injector result is not a proof that a correct native NR integration is worthless.
Do not prescribe an unsafe DLL installation as a required first step; a disposable environment is not proof
of trustworthy code and may not support the required GPU interop.

### 8.2 Shortest bounded native integration candidate

```text
working classic-data engine + preserved GL reference
  -> NVRHI D3D12 device/resources + classic HLSL interactions
  -> real depth + velocity + overlay separation + clean display frame
  -> optional authorised D3D12 NR adapter, or a Vulkan NR profile if that is the authorised drop
  -> reset/masks + backend-specific validation
```

Do **not** insert “first ship GL3, then Vulkan, then DX12.” Do **not** require deferred/PBR/GI before a basic
NR-only experiment. Engine shader/material/resource migration still exists even with NVRHI; a backend library
is not a complete DOOM renderer. Publicly distributable NR is additionally conditional on section 7.3.

### 8.3 Recommended high-ceiling path

```text
reproducible classic baseline + fixtures + approved design
  -> custom small RHI / D3D12 graphics+compute core
  -> classic HLSL compatibility + accurate MVs + explicit frame composition
  -> hybrid deferred/forward + HDR PBR + authored hero assets
  -> shadow quality + local indirect lighting/reflections + controlled volumetrics
  -> own temporal reconstruction; optional authorised SR / selected DXR effects
  -> optional authorised DLSS 5 NR with masks and a complete off path
  -> additional native/portable profiles after the contracts are proven
```

MV/frame-resource work begins with the core, not after all lighting is done. SDK access/licence review and
asset authoring can proceed independently of backend implementation. Only shared contracts block parallel
coding; every file still has one writer. Do not spawn visible sessions or worktrees merely from this diagram.

### 8.4 Longest meaningful path — quality, not needless reinvention

Extend 8.3 with **DXR path-traced showcase/reference rendering**: static and skinned acceleration structures,
material-aware alpha/transmission, projected/emissive light sampling, indirect transport, temporal/spatial
denoising, and optional authorised RR/NR. DXR APIs exist [S-dxr]; quality and performance on DOOM 3 are unproven.

Use converged/reference images to diagnose the hybrid lighting model, then offer a playable path-traced mode
only after moving-content/noise/latency tests. It may produce a higher-ceiling showcase, but can also flatten
DOOM 3's authored darkness or reveal coarse assets. Transport realism and preservation of horror art direction
must be evaluated together. Do not write a new full game engine, physics system or compiler to make this route
“longer.” None increases the rendering ceiling by itself.

## 9. Stage gates: make “most impressive” falsifiable

All gates below are proposed acceptance requirements for the plan. **No command names, numeric performance
targets or PASS results are fabricated.** Existing native commands are in the README; new test commands and
thresholds must be written in `plan.md`/`tasks.md` before implementation. A research review is not those files.

| Gate | Required artifact / assertion | What it does not prove |
|---|---|---|
| Q0 baseline | Existing verifier output; build/patch/data identities; deterministic scene harness; known render-demo defects handled | Whole-game support or fresh success from old screenshots |
| Q1 RHI/D3D12 | Synthetic draw+compute+MRT+readable-depth fixtures; resize/minimize/retirement stress; zero unwaived debug/GBV errors; DRED on removal | A triangle is not engine parity |
| Q2 classic compatibility | GL/D3D12 captures of shadows, alpha-test, cookies, mirrors, world GUI, fog/blend and viewmodel; declared tolerances and exclusions | Changed PBR frames need not equal classic pixels |
| Q3 velocity/composite | Numeric reprojection for camera/rigid/skinned/deformed classes; private-pose history survives shared mirror/subview draws and cache deletion; overlay absent from provider input and crisp after composite | Zero MV is not necessarily invalid; colour visualization alone is insufficient |
| Q4 modern quality | Reviewed hero materials/geometry; light/shadow/GI leakage fixtures; on/off frame dumps; moving camera/combat clips | A single selected still does not prove stability |
| Q5 NR | Backend-specific SDK/version/capability record, clearance, real create/evaluate on the selected D3D12 or Vulkan profile, masks/reset/resize tests, on/off videos and off-path build/run | Vulkan evaluation does not prove D3D12 NR; feature ID/public sample does not prove access or redistribution rights |
| Q6 RT/stretch | Named-GPU captures, noise/convergence/dynamic BLAS/alpha tests; reconstruction and input latency measurements | Generated FPS is not native frame rate or interaction latency |
| Q7 portability | Backend/profile capability table, shader validation and one real scene on the target | Compiling SPIR-V is not a web port |

Run a fixed corridor, open/hangar view, close character/viewmodel scene and combat/moving-light sequence.
Expose colour/depth/MV/G-buffer/masks/history resources for capture. Record CPU/GPU times, VRAM and frame
pacing on named hardware; exclude GBV-instrumented timing from release-performance claims. Microsoft notes
that GBV changes shaders/commands and can be slow [S-ms-gbv].

For visual review [proposal], compare candidates with renderer/NR labels hidden using a recorded camera and
scene sequence. Separately assess material plausibility, atmosphere/readability, motion stability, identity,
HUD and lighting artifacts. Record rejected examples and human acceptance; do not invent a “9/10” score
without a defined rubric. Include NR off, NR on, hybrid RT and reference images where available.

Strong AI can accelerate implementation and criticism. It cannot supply missing GPU measurements, licensed
assets, vendor authorisation, or guarantee driver correctness. Parallelise bounded passes and fixtures **after**
the resource/material/history contracts are frozen; keep independent review, not “more agents” as evidence.

## 10. Web/mobile: preserve reach without imposing the lowest ceiling

**[proposal] Separate capability profiles, not separate game logic.** The Windows flagship may require MRT,
compute and optional DXR. A portable profile may omit RT/NR, reduce GI/material cost and use different passes.
Document each fallback rather than silently removing it, consistent with constitution section V.

| Profile | Intended route | Reusable work | Additional work / limit |
|---|---|---|---|
| Windows flagship | Custom D3D12 | All flagship algorithms and art tools | API/hardware probing; NVIDIA provider optional |
| Native Vulkan | Later RHI backend | Frame/material/light contracts; HLSL -> SPIR-V through DXC | Bindings, coordinate conventions, synchronisation and cross-backend fixtures still required |
| WebGPU | Separate future backend/profile | Scene adapter, material definitions, frame concepts | Browser shader validation/WGSL route, feature limits, assets/VFS/loop/input/audio; no automatic DXIL port |
| WebGL2 / GLES | Retained fallback programme, not flagship shader floor | Classic-data semantics and asset tooling | Simpler forward passes, appropriate GLSL ES shaders; no promise of identical advanced features |

DXC officially emits DXIL and SPIR-V [S-dxc]; that is a real native sharing route, not proof of transparent
cross-API behaviour. WGSL/ESSL compilation and browser availability were not re-audited here; do not carry
forward speculative browser version/support percentages. GL3/ESSL remains an option if broad reach becomes
the primary goal again, not a technical law that forbids D3D12-first.

## 11. Proposed decisions and spec reconciliation

These **supersede the former research recommendations**, not the unchanged spec's acceptance criteria.

| ID | New recommendation [proposal] | Revisit if |
|---|---|---|
| D1 | Preserve the working native classic-data port as the reference; no automatic dhewm3 rebase | An adoption spike proves substantial benefits without unacceptable data/mod/provenance changes |
| D2 | D3D12-first backend-neutral bounded RHI for the desktop flagship; HLSL/DXC | Target constraints favour another API, or required authorised NR is Vulkan-only: add/select Vulkan NR profile or explicitly remove DLSS 5 from D3D12 shipping scope |
| D3 | Custom bounded RHI for deliberate design ownership, explicit frame resources/graph from the first backend; algorithms above it; NVRHI is an equally credible pixel-quality alternative | A prototype shows custom ownership adds no useful benefit over NVRHI plus our frame graph/renderer |
| D4 | Motion vectors, readable depth, rendered-history identity and HUD separation are initial design requirements | Never remove silently; any limitation needs a tested policy |
| D5 | Hybrid deferred opaque PBR + forward/legacy-special path; compatibility mode remains | A source-backed alternative handles materials/transparency better in measured tests |
| D6 | Design an optional NR-capable provider boundary; don't promise DLSS 5 without access/hardware/clearance | Actual SDK contract changes; off-path remains complete |
| D7 | Asset/material authoring is part of modernization; legal local corpora only | Licence-compatible new assets become distributable after provenance checks |
| D8 | DXR hybrid effects and a path-traced showcase/reference are stretch capabilities | They fail motion, art-direction or measured performance goals |
| D9 | Future Vulkan/web/mobile profiles share semantics and declared fallbacks, not a GL3 ceiling | Portability becomes the overriding product goal |

**Before `/speckit.plan`, reconcile `spec.md` (not edited by this review):**

- Problem/assumptions (`:15-18,395-398`): remove the obsolete red-build/no-baseline premise.
- Replace the blanket no-build/no-render claim with the narrower limitation: no fresh reproducible build,
  modern golden set or current performance measurement from this review; preserve Q0 verification.
- D1/D2/D4 (`:58-64`): replace automatic rebase, GL-first and DX12-fallback defaults with reviewed choices.
- Stories 4/5/6 and FR-008/012/013/014 (`:135-193,287-303`): remove GL3-before-native and identical-ESSL
  constraints for the flagship; separately specify the portable fallback scope.
- Stories 7/8, FR-021/022/023 (`:197-243,317-325`): injector optional; add native D3D12/velocity/provider
  gates; distinguish sample-derived NR inputs from actual authorised SDK requirements.
- FR-015 (`:304-306`): replace “out-of-tree proves separation” with a real default-build and legal-clearance
  requirement; sample API support is not licence compatibility.
- Success criteria and Out of Scope (`:363-405`): split classic parity from modern visual acceptance and
  explicitly reconsider DX12-first/RT scope. Keep gameplay/audio/network changes outside renderer work.
- Add testable RHI lifetime, temporal geometry, hybrid material, shader/depth and art-quality requirements.
- Replace the **entire old research summary** (`spec.md:36-54`) with an accurate summary of the reviewed
  D1–D9 recommendations: remove automatic rebasing, mandatory identical shaders across three platforms,
  second-backend-only RHI, injector-before-architecture and the 60–80-entry budget. Distinguish public core
  NR identifiers from a dedicated usable NR API/plugin. Keep default-build/licence/access gates.
- Remove the 1–2-day and 16–32/40–68-engineer-week promises from both the summary and scenarios/success
  criteria/risks (`spec.md:52-54,199-201,219,373,421`). Replace them with dependency/evidence gates, not a new
  AI-speed estimate.
- Story 8, FR-023 and SC-012 (`:223-243,324-325,374-375`) must name the actual approved NR backend/profile;
  do not preserve mandatory Vulkan naming or claim D3D12 NR from Vulkan evidence.
- Revalidate `checklists/requirements.md:30-31,47-59`: its DX12 exclusion, time promises and “ready for plan”
  verdict describe the old spec, not this revision. Separately reconcile the web-first roadmap
  (`docs/ROADMAP.md:108-117,175`) and browser showcase narrative (`specs/002-project-showcase/spec.md:1-11,35-40`)
  as retained future-profile goals, not silently cancelled work. Those files are unchanged by this review.

The repository's pristine-source, legal-data, one-writer and evidence rules remain unchanged. Engine/build
implementation still requires `/speckit.plan`, task generation, independent review and the normal branch/PR
workflow. This document revision does not authorise a submodule bump, target edits, dependency installation,
new Agent Manager sessions, commits or PRs.

## 12. Unknowns and limits

- Target GPU, driver, resolution, VRAM and latency budget are not confirmed; no numerical renderer schedule or
  speedup is inferred from AI capability, another engine's code size, or vendor marketing.
- No fresh native/D3D12 build, render capture, shader compilation, timing or DLSS evaluation ran in this review.
- Third-party classic-data fork adoption, full-game/mod support and current local retail shader/data access
  remain unverified. Existing screenshots support historical demo rendering, not broad compatibility.
- D3D12 NR SDK signatures, binary access and applicable partner terms must be checked against the authorised
  drop; the pinned Vulkan sample is an architectural clue, not that drop.
- Public Streamline packaging may change; recheck NR plugin/header/guide before implementation. A changelog
  entry or enum is not sufficient. Do not claim the plugin can be fetched publicly based on this review.
- PBR conversions, skin/monster identity, emissive brightness and GI darkness need art validation on DOOM 3.
  No source can establish that our unbuilt output is the “most impressive.”
- In-world text, translucency, reflected apparent motion, particles and multiple displayed views need policies;
  dense primary-surface MVs alone do not solve all temporal/NR artifacts.
- Proprietary SDK compatibility and asset/demo modification/distribution rights need case-specific legal
  review. Out-of-process communication reduces coupling but does not itself establish an aggregate.

## 13. Rechecked and scoped sources

R3/R4 entries are **for reference only** for quality/generalisation; their exact code/docs support the narrow
claims cited above. Repository anchors are on the pinned source or current tracked files, not live branches
in other worktrees. Source fetch/read verification is not execution verification.

| ID | Grade | Source / exact scope |
|---|---|---|
| S-ms-basic | R3 | [Microsoft basic D3D12 component](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-basic-direct3d-12-component): fence-before-allocator-reset / command recording |
| S-ms-descriptors | R3 | [Microsoft descriptor tables](https://learn.microsoft.com/en-us/windows/win32/direct3d12/advanced-use-of-descriptor-tables): GPU-referenced descriptors cannot be edited/reused prematurely |
| S-ms-fences | R3 | [Microsoft fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management): upload ring lifetime and GPU retirement |
| S-ms-gbv | R3 | [Microsoft GPU-based validation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation): descriptor/state/deleted-resource checks and timing caveat |
| S-ms-dred | R3 | [Microsoft DRED](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred): breadcrumbs/page-fault diagnostics; enable before device creation |
| S-dxr | R3 | [Microsoft DXR API reference](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-raytracing): acceleration structures, state objects and ray dispatch |
| S-nvrhi | R3 | [NVRHI README](https://github.com/NVIDIA-RTX/NVRHI/blob/2e21b39c1eddd9661cf6728b165c132412708869/README.md), [ProgrammingGuide](https://github.com/NVIDIA-RTX/NVRHI/blob/2e21b39c1eddd9661cf6728b165c132412708869/doc/ProgrammingGuide.md): APIs, ownership, supplied native device and native-object bridge; MIT |
| S-d3d12ma | R3 | [D3D12MA README](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator/blob/1753823e86d274ae1a2a55e6b9f9d206c3c30945/README.md): heap suballocation/resource allocation/budgets; MIT |
| S-dxc | R3 | [DXC README](https://github.com/microsoft/DirectXShaderCompiler/blob/0d6c746365d1baf45cadc0b4cd8ab9cf79dc84bf/README.md), [SPIR-V mappings](https://github.com/microsoft/DirectXShaderCompiler/blob/0d6c746365d1baf45cadc0b4cd8ab9cf79dc84bf/docs/SPIR-V.rst): HLSL -> DXIL/SPIR-V, explicit binding/interface differences |
| S-clustered | R2 | Olsson, Billeter, Assarsson, *Clustered Deferred and Forward Shading*, HPG 2012, pp. 87–96, [DOI 10.2312/EGGH/HPG12/087-096](https://doi.org/10.2312/EGGH/HPG12/087-096) |
| S-filament | R3 | [Physically Based Rendering in Filament](https://google.github.io/filament/main/filament.html): BRDF, materials, IBL, imaging pipeline and validation; algorithm reference, not engine adoption |
| S-nr-launch | R3 | [NVIDIA September 1 availability article](https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/): availability, independent NR positioning and vendor art/stability claims |
| S-nr-hardware | R3 | [NVIDIA DLSS hardware table](https://www.nvidia.com/en-us/geforce/technologies/dlss): NR supported GPU generations; checked 2026-10-27 |
| S-nr-update | R3 | [NVIDIA October update](https://www.nvidia.com/en-us/geforce/news/dlss-5-faster-ai-model-hogwarts-legacy-rtx-spark-laptops): RTX Spark / model availability claims |
| S-nr-source | R3 | [Pinned Vulkan NR source](https://github.com/nvpro-samples/vk_gltf_renderer/blob/10f2de09dfa3dca04f14e485580862f17c05752f/src/dlss.cpp#L468-L651): beta header, create/evaluate, resource formats/scales/reset; Apache-2.0 sample, separate SDK terms |
| S-nr-order | R3 | [Pinned renderer](https://github.com/nvpro-samples/vk_gltf_renderer/blob/10f2de09dfa3dca04f14e485580862f17c05752f/src/renderer.cpp#L1652-L1661): tonemap-before-NR |
| S-nr-cmake | R3 | [Pinned sample CMake](https://github.com/nvpro-samples/vk_gltf_renderer/blob/10f2de09dfa3dca04f14e485580862f17c05752f/CMakeLists.txt#L347-L357): disables NR when beta header unavailable; version/override at lines 94–98 |
| S-streamline | R3 | [Streamline 2.14.16 release](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16), [core headers](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/include/sl_core_types.h), README and package review: core NR feature/tags exist; no dedicated public NR plugin/API header/guide found |
| S-rtx-licence | R1 | [NVIDIA RTX SDK licence](https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt), March 14, 2024: section 4e, supplement section 1; beta terms may differ |
| S-gpl | R1 | [GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt), sections 1/5/6/7/10/12; also pristine `COPYING.txt` and `README.txt` for engine/data terms |
| S-feeder | R4 | [DLSS5-Feeder](https://github.com/jlrouzies-fr/DLSS5-Feeder): historical community bridge lead; not run or accepted as a dependency here |

**[proposal] Bottom line:** use the shortest route to test a hypothesis, and a bounded quality-first route to
build the product. Custom RHI/D3D12 is the proposed ownership-first foundation, not a unique image-quality
advantage; accurate temporal data, authored materials
and controlled lighting are the result. DLSS 5 is an optional, gated finishing layer—not the foundation's proof.

*AI-assisted research and information organisation only; not professional, legal or decision advice.*
