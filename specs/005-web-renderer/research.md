# Research: DOOM 3 web rendering on the project RHI

**Status**: Draft research. No implementation, build, browser run, shader compile, GPU capture or
performance measurement was performed. This document is not a port claim and is not an approved
specification.

**Published**: 2026-10-28 as research by the S0 contract change (Refs #20), together with the
three-tier amendment to `specs/004-quality-renderer/contracts/rhi-foundation.md`. Publication does
not approve this research or execute anything in it. The `02_renderer.md`, ROADMAP and AGENTS
corrections listed in §10's S0 row are outside that contract decision and remain open.

**Date**: 2026-10-27
**Question (English rephrase)**: Given the existing PC renderer research (specs 003 and 004) and
the project's three-tier custom RHI (a portable WebGPU-subset core, a D3D12-private backend and
native extensions), how should DOOM 3's **rendering** be built for the web on that RHI?

**Scope**: Rendering architecture (frame submission, classic ARB2 path, WebGPU mapping, shader
strategy, modern path, RHI contract). Platform work (VFS, loop, input, audio, memory) is covered
only where it gates a rendering test. Gameplay, networking, save formats and asset distribution
are out of scope.

---

## 0. Verdict

1. **The browser cannot run the D3D12 backend.** A browser exposes WebGPU, WebGL2/WebGL and no
   native D3D12. "DOOM 3 on our RHI for the web" therefore means **one engine adapter + one
   portable RHI core + two backends**: D3D12 on Windows, WebGPU in the browser. It does not mean
   porting the D3D12 implementation.
2. **The portable RHI core must be a WebGPU-shaped subset.** Fence values, public
   transitions/barriers, command allocators, descriptor heaps/ranges, explicit `Present`,
   HLSL/DXC as the public shader input, and debug-layer/GPU-based-validation gates are D3D12
   implementation details. If Q2 engine code is written against them, a WebGPU backend cannot be
   added without public-header and engine surgery. The contract needs a **three-tier** design:
   portable core + D3D12-private backend + native extension capabilities.
3. **The classic ARB2 path is reproducible on WebGPU in the original pass order, but not by
   translation.** Alpha test becomes `discard`; texgen becomes vertex-shader math; stencil volumes
   keep `increment-wrap`/`decrement-wrap`; seven texture units become one bind group; the
   back-buffer copy / `_currentRender` / mirror / `SS_POST_PROCESS` paths become explicit offscreen
   targets; the GL `[-1, 1]` clip-depth projection is rewritten for WebGPU `[0, 1]`; synchronous
   readback becomes asynchronous.
4. **The hard blocker is shader source, not the graphics API.** DOOM 3's ARB2 renderer loads GL
   assembly from `glprogs/<name>` in retail data at runtime. Those files are absent from the GPL
   source release and are EULA-bound. The project must author its own HLSL library and compile it
   to DXIL for D3D12 and to SPIR-V -> WGSL for WebGPU. The live static ARB2 set is small (six
   programs plus `megaTexture.vfp`), but material stages can name arbitrary programs and missing
   programs currently fail silently. This is authoring and legal work, not a build flag.
5. **The browser can run a modern hybrid renderer, but not the path-4 ceiling.** WebGPU covers
   deferred/forward raster, HDR, MRT, compute GI/reflections, temporal reconstruction and a
   display-resolution HUD within its default limits. It has no ray-tracing API and no NVIDIA
   DLSS/NR provider. The path-4 destination (DXR path tracing + optional DLSS 5) remains
   **native-only**; the web profile is a separate, lower ceiling.
6. **Recommended architecture: A** — one portable RHI, one engine adapter, D3D12 and WebGPU
   backends. Architecture B (separate Emscripten GL -> WebGL2 renderer) is the shortest browser
   proof but is not "on our RHI" and caps quality at WebGL2. Architecture C (WebGPU-shaped RHI with
   Dawn on native) is the cleanest single-conformance model but either caps the native ceiling or
   needs an undesigned RT/DLSS side channel. A is the only design that can serve the native
   flagship and the browser on the same renderer core.
7. **No executed evidence exists yet.** In this checkout `src/` and `tests/` do not exist, the
   Q1a contract is paper, Emscripten is not installed, no shader has been compiled, and no browser
   frame has been rendered. Everything below is source-anchored design reasoning. A future run must
   replace each unverified statement with a command and its output.

---

## 1. What exists today

| Item | State on 2026-10-27 | Consequence for this research |
|---|---|---|
| Pristine engine | `doom3_orig/doom3` submodule, DOOM 3 GPL release 2011-11-22 | Source is ground truth; game data is not present |
| ARB2 shaders | `glprogs/*.vfp` / `*.vp` absent from the GPL tree | Shader text cannot be inspected, translated or committed |
| RHI | `specs/004-quality-renderer/` design only; no `src/renderer` | Q1a is a plan, not a working backend |
| Browser build | No Emscripten preset, no `targets/web`, no wasm artifact | Every browser statement is unexecuted |
| Game data | Not in the repository; EULA-bound | No retail-scene parity, screenshot or timing exists |
| Native build | Win11 x64 CMake flow exists; historical captures exist | Reference renderer is buildable in principle, not re-run for this document |

The 004 contract now defines a three-tier RHI — a portable WebGPU-subset core, a D3D12-private backend and native extensions — while the implementation remains a Windows/D3D12 flagship with the browser at Q7. The browser still has no WebGPU backend design, no probed limits table, no async pipeline-compilation plan, no canvas model and no browser evidence gate; the amended core does specify device loss and a `surface`/`core-present` operation, but no browser implementation exists. This document fills that gap at research level; it does not promote Q7 into an implemented profile.

The 003/004 documents were published by PR #10 (commit `1e15790`), and this 005 research is published by the S0 contract change (Refs #20). A worktree created from `origin/main` can see them only after that change merges; nothing here should be cited as a shared baseline before then. Engine citations in §2 are against the pristine submodule at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`; line numbers drift when the submodule is bumped.

---

## 2. The engine's rendering contract

### 2.1 Frame and command stream

A frame is synchronous:

- `idCommonLocal::Frame` runs events, the fixed 60 Hz game tic (`session->Frame`), then
  `session->UpdateScreen` (`Common.cpp:2439-2489`).
- `UpdateScreen` calls `BeginFrame`, `Draw`, `EndFrame` (`Session.cpp:2495-2503`).
- `BeginFrame` sets the backend, queues `RC_SET_BUFFER`, and seeds crop 0 to the full window
  (`RenderSystem.cpp:627-690`).
- `EndFrame` emits the fullscreen GUI view, queues `RC_SWAP_BUFFERS`, and issues the command list
  (`RenderSystem.cpp:708-758`).
- `RB_ExecuteBackEndCommands` is the single execution point; it drains the list in order
  (`tr_backend.cpp:600-653`).

The command set is five types (`tr_local.h:472-501`):

```text
RC_NOP
RC_DRAW_VIEW
RC_SET_BUFFER
RC_COPY_RENDER
RC_SWAP_BUFFERS
```

The list is a per-frame bump allocator; each command's `next` points at the following command id.
The protocol is linear and backend-neutral. A WebGPU backend can consume the same list unchanged;
the engine adapter, not the game code, maps it to passes, copies and a surface frame boundary.

Every normal frame is two `RC_DRAW_VIEW`s (3D, then 2D GUI) plus one swap. Mid-frame flushes for
screenshots/copies are legal and must preserve ordering.

### 2.2 ARB2 pass order

`RB_STD_DrawView` (`draw_common.cpp:1666-1724`) fixes the order. It is not a suggestion:

| # | Pass | What it does | WebGPU representation | Main issue |
|---|---|---|---|---|
| 1 | Depth prepass | Writes depth, clears colour to black (or down-modulates for subviews), alpha-tested stages discard, stencil test enabled | Render pass with depth/stencil + colour attachment; alpha `discard` in shader; polygon offset | Alpha test must also apply in the prepass; no fixed-function alpha test |
| 2 | Per-light stencil shadows | For each shadowed light: scissored stencil clear, shadow volumes with front/back cull and increment/decrement-wrap | Same pass or a split pass; stencil ops; scissored stencil-fill workaround | WebGPU `loadOp` clear is full-surface and cannot be issued inside a pass |
| 3 | Interactions | Additive blend, depth `EQUAL`, 7 textures, per-light environment parameters | One render pass; one bind group for 7 textures; uniform ring | No fixed-function texgen; no global program env parameters |
| 4 | Light scale | Fullscreen multiply of the scene | Fullscreen triangle, multiply blend | Must run after all interactions |
| 5 | Opaque shader passes | Material stages, blend/fog stages, alpha test | Material pipeline variants | GLS state -> PSO key; texgen -> VS math |
| 6 | Fog / blend lights | Projected fog and blend-light passes | Forward passes, shader math | `TG_FOG`, `TG_BLENDLIGHT` re-authored |
| 7 | Post-process (`SS_POST_PROCESS`) | Samples `_currentRender` after a mid-view copy | Offscreen copy + new pass | Cannot sample the active attachment |
| 8 | GUI / 2D | Fullscreen 640x480 ortho view, blended | Second render pass on the presented target | Scaling/DPI; no offscreen texture needed |
| 9 | Debug tools | `r_showShadows`, `r_showPortals`, etc. | Optional; line geometry | No polygon mode; debug-only |

The order matters. Moving post-process before fog, or merging the two shader-pass loops, changes
visible results and is not safe without an equivalence proof.

### 2.3 Per-light interaction contract

`RB_ARB2_DrawInteraction` (`draw_arb2.cpp:67-135`) sets the per-interaction vertex-program
environment parameters and fragment slots 0/1; the per-light TMU0/TMU6 bindings and the vertex
attribute pointers are set in `RB_ARB2_CreateDrawInteractions` (`draw_arb2.cpp:165-198`). The full
per-light contract is:

- Vertex-program environment parameters: `PP_LIGHT_ORIGIN`, `PP_VIEW_ORIGIN`,
  `PP_LIGHT_PROJECT_S`, `PP_LIGHT_PROJECT_T`, `PP_LIGHT_PROJECT_Q`, `PP_LIGHT_FALLOFF_S`,
  bump/diffuse/specular matrices, colour modulate/add.
- Fragment-program environment slots 0/1 (diffuse, specular).
- Textures:
  - TMU0: normal cube map (or ambient normal map for ambient lights)
  - TMU1: bump map
  - TMU2: falloff map
  - TMU3: light projection
  - TMU4: diffuse
  - TMU5: specular
  - TMU6: specular lookup table
- Vertex attributes: 8 = ST, 9 = tangent0, 10 = tangent1, 11 = normal; plus colour and position
  from `idDrawVert` (60 bytes).

`RB_ARB2_CreateDrawInteractions` (`draw_arb2.cpp:144-203`) uses additive blend
(`ONE, ONE`), depth mask on, and `depthFunc` from `backEnd.depthFunc` (set to
`GLS_DEPTHFUNC_EQUAL` at `draw_common.cpp:1672`; `LESS` for translucent interactions at
`draw_arb2.cpp:312`), binds the interaction programs and walks `surf->nextOnLight`.

On WebGPU:

- Seven sampled textures fit one bind group (limit: 16 sampled textures per stage).
- The per-light parameter block cannot use push constants (64-byte immediate limit); use a
  256-byte-aligned uniform-buffer ring with dynamic offsets.
- Vertex attributes become an explicit vertex layout within 8 vertex buffers and 16 attributes;
  client-side index arrays and offsets-cast-to-pointers are removed.
- Additive blending and `depthCompare: "equal"` map directly.

### 2.4 Stencil-volume shadows

The classic path is stencil shadows, not shadow maps:

- At view start the stencil is cleared to `1 << (stencilBits - 1)`, normally 128
  (`tr_render.cpp:582-583`). The depth prepass does **not** write 128; the earlier renderer note
  that says so is one of four documented errors (section 2.9).
- For each shadowed light, the engine sets the light scissor and calls
  `qglClear(GL_STENCIL_BUFFER_BIT)` (`draw_arb2.cpp:280`), which re-clears the scissored region to
  128.
- `RB_StencilShadowPass` sets `GL_ALWAYS, 1, 255`, disables colour writes, and draws shadow volumes
  with front/back culling (`draw_common.cpp:1176-1248`).
- `RB_T_Shadow` uses the "patent-free workaround": preload clipped volumes with decrement/increment,
  then traditional depth-pass increment/decrement (`draw_common.cpp:1146-1165`);
  `tr.stencilIncr`/`tr.stencilDecr` are `GL_INCR`/`GL_DECR` or the `_WRAP_EXT` variants, chosen in
  `RenderSystem_init.cpp:418-424` (`tr_local.h:795`).
- Interactions then test `GEQUAL 128`: lit pixels pass; shadowed pixels fail.
- The optional `EXT_depth_bounds_test` skips shadow geometry outside a depth range
  (`draw_common.cpp:1103-1106`). WebGPU has no depth-bounds test; this optimization is dropped and
  must be replaced by tighter scissor or another measured optimization.
- The near-plane fixup and shadow-volume generation (`tr_stencilshadow.cpp`) stay CPU-side; they
  produce the same vertex/index buffers. Only the draw state changes.

WebGPU supports `stencil8` and `depth24plus-stencil8`, `increment-wrap`/`decrement-wrap`, and
independent front/back stencil state. The hard part is the **per-light scissored clear**. WebGPU's
attachment `loadOp: "clear"` clears the whole attachment and cannot be issued inside a pass. Two
workable policies:

1. **Scissored stencil-fill draw (recommended first):** keep one scene pass per view; before each
   light, draw a fullscreen (or light-bounds) triangle with colour writes disabled, stencil write
   enabled, stencil replace to 128, and the light scissor. This preserves the GL scissor semantics.
   Cost is unmeasured; measure draw/pass counts and shadow fill on a real map.
2. **Per-light pass with full clear:** end the scene pass and reopen it with `loadOp: "clear"` for
   the stencil attachment. This is only safe if the previous light's stencil marks are dead before
   the next clear; mirrors, `r_useExternalShadows` variants and subviews may violate that
   assumption. It also multiplies render passes.

D3D12 must emulate the chosen full-surface or scissored semantics so both backends agree. This is a
correctness gate, not a micro-optimization.

The GPL release does not contain the depth-fail stencil-shadow variant; do not plan around it
without checking the exact source revision.

### 2.5 Materials, alpha test and texgen

Materials are `.mtr` files (retail data, not in the repository). A material has stages; each stage
has a `GLS_*` state combination, texture maps, texgen modes, deforms, an expression-register VM and
optional `vertexProgram`/`fragmentProgram` names.

The WebGPU replacements are explicit:

| GL fixed function | WebGPU / WGSL |
|---|---|
| `glAlphaFunc(EQUAL 1)`, `LESS 0.5`, `GEQUAL 0.5` | `discard` in the fragment shader, three variants (or one variant with a compare mode constant); must also discard in the depth prepass |
| `TG_SCREEN`, `TG_SCREEN2` | Vertex-shader projection math; CPU-generated texcoords where the original already does so |
| `TG_GLASSWARP` | Vertex-shader distortion; may need a small lookup texture |
| `TG_REFLECT_CUBE`, `TG_DIFFUSE_CUBE` | Vertex-shader reflection vector -> cube sampling |
| `TG_BLENDLIGHT`, `TG_FOG` | Vertex/fragment shader projection |
| Mirror `alphaNotch` clip | Correctness-critical prepass clip (DOOM 3 does not use clip planes for this); implement in the prepass shader |
| `GL_POLYGON_MODE_LINE` | Debug-only; replace with line geometry or drop |
| `glDepthBoundsEXT` | No equivalent; drop or replace with scissor |
| `glLogicOp` | No equivalent; not used on the shipping path |
| Fixed-function blend/depth/stencil | Pipeline state; no issue |

Texgen re-authoring is open-ended: there is no shader source in the repository, and a mistake
changes mirror clipping or fog rather than producing a compile error. It needs retail-data A/B
captures, not just a successful build.

### 2.6 Images, render targets and `_currentRender`

`Image_load.cpp` uploads DXT1/DXT3/DXT5 compressed textures and LUMINANCE8, ALPHA8, INTENSITY8 and
LUMINANCE8_ALPHA8 formats. WebGPU has no L8/LA8/A8 formats and BC is an optional feature
(`texture-compression-bc`). Consequences:

- DXT data either uses the optional BC feature or is transcoded to RGBA8/ETC2/ASTC; memory and load
  time change.
- L8/LA8/A8 images need conversion plus shader swizzles; specular lookup tables and alpha-only
  images are affected.
- sRGB behaviour is controlled by view formats; DOOM 3's gamma/overbright conventions need an
  explicit mapping.

`_currentRender` is filled by `currentRenderImage->CopyFramebuffer(...)` inside
`RB_STD_DrawShaderPasses` (`draw_common.cpp:984-996`), which reads `GL_BACK`
(`Image_load.cpp:1870`). The separate `RC_COPY_RENDER` command (`RenderSystem.cpp:912-944` →
`RB_CopyRender`, `tr_backend.cpp:575-589`) copies into an arbitrary named image and is the path used
by mirrors/subviews and `_scratch` (`tr_subview.cpp:342`, `Session.cpp:471`). `SS_POST_PROCESS` and
mirrors therefore need an explicit mid-view copy offscreen. On WebGPU an attachment cannot be sampled
while it is bound; the renderer must:

1. end the scene pass,
2. copy the scene colour into an offscreen texture (or render the subview into its own target),
3. open a new pass that samples it.

This is the largest structural change to the classic renderer. It changes render order, memory
footprint and subview recursion relative to GL; it needs a material census for `SS_POST_PROCESS`
and tests for mirrors, security cameras and nested subviews.

Depth copies have extra restrictions: `depth24plus` is not copyable, `depth24plus-stencil8` allows
only the stencil aspect, and `depth32float` is a copy source but not a destination. Stencil/depth
debug dumps must choose formats accordingly.

### 2.7 Projection and depth convention

The engine uses a far-plane-at-infinity projection (`tr_main.cpp:936-957`):

```text
m[10] = -0.999f
m[14] = -2.0f * zNear
m[11] = -1
```

This produces GL clip-space depth in approximately `[-1.001w, 0.999w]` (NDC about
`[-1.001, 0.999]`); the `-0.999f` factor deliberately keeps the far value below 1. WebGPU uses
`[0, 1]`. The projection must be rewritten once for both backends. Options:

- Use a `[0, 1]` projection with the same near plane and a far plane at infinity; preserve
  `depthCompare: "less-equal"`.
- Or use reversed-Z with `greater`; this changes depth precision and every depth comparison, so it
  is a separate decision, not a silent side effect.

The renderer also uses a depth hack; map it to viewport `minDepth`/`maxDepth`, not to a projection
edit. `depth32float` cannot be a copy destination; `depth24plus` cannot be copied at all.

### 2.8 GUI, HUD and subviews

- World GUI (`R_RenderGuiSurf`, `tr_guisurf.cpp:128-181`; draw-into-current-view at `177`) is drawn
  into the current 3D view, not into a texture. The earlier note that says it renders to a texture is
  wrong.
- The display GUI is a second fullscreen view with a hardcoded 640x480 orthographic projection
  (`GuiModel.cpp:226-297`; projection at `267-274`); it is scaled to the window.
- Mirrors and remote cameras are subviews with their own viewport/scissor and clip planes; their
  surfaces are drawn into the current target in GL.

On WebGPU:

- The second GUI pass maps directly; no offscreen texture is needed for the classic path.
- Subviews that sample the scene need their own offscreen target and a pass split.
- In the modern path, the display HUD must be a separate display-resolution pass after temporal
  reconstruction; world GUI stays forward in scene passes. Do not merge them.

### 2.9 Corrections to `doom3_orig/doc/02_renderer.md`

The companion analysis contains four material errors. This document uses the source, not the note:

1. The depth prepass does not write stencil 128. The view clear and per-light scissored clear set
   `1 << (stencilBits - 1)` (`tr_render.cpp:582-583`, `draw_arb2.cpp:280`).
2. The shadow-lit polarity is inverted in the note: interactions test `GEQUAL 128`; the stencil is
   preloaded to 128, volumes increment/decrement, and lit pixels remain >= 128.
3. The GUI is not rendered into a texture. World GUI is drawn into the current view; the display
   GUI is a second fullscreen pass (`tr_guisurf.cpp:128-181`, `GuiModel.cpp:219-297`).
4. The classic path is not two-sided stencil. It uses front/back culling with separate
   increment/decrement operations (`draw_common.cpp:1146-1165`).

The note should be fixed before another plan cites it. The errors do not change the high-level
porting plan, but they change the pass and stencil design.

---

## 3. What "on our RHI" must mean

### 3.1 Three-tier contract

The RHI must be split before Q2:

| Tier | Contents | Who may depend on it |
|---|---|---|
| **Portable core** | Adapter/device/capabilities; buffers; textures/views/samplers; shader modules as backend blobs; pipeline layouts and immutable bind groups; one-shot recorders; render/compute passes with declared attachments and bindings; copies; asynchronous pitch-aware readback; opaque submission handles; `Surface`; error scopes; device loss; one queue | Engine adapter, scene/material code, both backends, browser profile |
| **D3D12 private implementation** | Fences and fence values; resource transitions/barriers and state journal; command allocators/lists; descriptor heaps/ranges/root signatures; query heaps; swap chain; `ID3D12PipelineLibrary`; debug layer, GPU-based validation, DRED | D3D12 backend only; tests may inspect through a backend-private hook |
| **Native extension capabilities** | DXR/ray tracing; DLSS/NR provider; multi-queue/async compute; bindless; sparse resources; resource aliasing; native-provider interop | Native-only code behind explicit capability queries; **never** scene/material code and never the web profile |

The core's intended floor is the WebGPU default envelope, not D3D12's. The 004 contract deliberately
does not assert numeric common limits before target execution: each target reports its enabled
capabilities and limits, and a backend that cannot meet a requested operation must reject the
configuration at startup rather than fail at draw time.

### 3.2 Semantic mapping and the 004 contract audit

Before the 2026-10-28 amendment, the 004 contract was D3D12-first in substance even where it said backend-neutral. The amended contract (issue #20) now separates the three tiers; the classification below is retained as the audit that produced the amendment. The 13 named semantics plus four adjacent ones classify as:

- **(a) portable as-is**: one queue after renaming, MRT with limits, motion/validity at data level,
  overlay separation.
- **(b) portable after renaming/redefining**: adapter selection (including software adapter),
  readback row pitch, resize/present, debug layer if per-backend, device removal, error model,
  descriptor/upload ownership if renamed to binding allocations, command allocators if abstracted
  to one-shot recorders.
- **(c) not portable**: public transitions/state journal, fence values and the outstanding-fence
  release assertion, public UAV barriers, HLSL/DXC as the public shader input, native-provider
  interop in the core, and any cross-backend debug-layer/GBV gate.

| 004 / RHI concept | WebGPU reality | Decision |
|---|---|---|
| `RENDER_TARGET` / `COPY_SOURCE` state journal | No app-facing transition API; usage declared at creation/binding and validated in usage scopes | Make the journal a D3D12-backend-internal test hook; public API speaks passes/attachments |
| Fence value + `GetCompletedValue() < signalValue` | One queue; progress only via `onSubmittedWorkDone()` or `mapAsync`; no fence objects or values | `Submit` returns an opaque completion handle; `Wait`/`IsComplete`; ring of handles for frame slots. The outstanding-release test moves inside the D3D12 backend |
| Command allocators/lists | Encoder -> single-use command buffer; no allocator/reset | One-shot recorder allocated per frame from an arena; no public reset |
| Descriptor ranges/root signatures | Bind groups + pipeline layouts; no heaps/ranges | Immutable bind groups; 4 groups, 16 sampled textures/stage, 12 UBOs/stage, 64 KiB UBO, 256-byte dynamic offset |
| Adapter class incl. software adapter | Adapter or null; fallback is a hint; info may be empty/redacted | Capability table; software adapter is labeled, not assumed |
| Readback row pitch | `bytesPerRow` must be a multiple of 256; `mapAsync` is async | Caller-owned bytes after repack; multi-frame readback; no `qglReadPixels` |
| Resize/present | Canvas `configure` / `getCurrentTexture`; no `present()` | `Surface::AcquireTexture()`; present is a frame-boundary semantic |
| Debug layer + GPU-based validation | Error scopes + uncaptured errors; no debug-layer/GBV toggle | D3D12-only capability; web profile has its own zero-uncaptured-error gate; never a cross-backend acceptance gate |
| One direct queue | One `device.queue`; no async compute | One queue in the core; multi-queue is a native extension |
| MRT + shader-readable depth | 8 colour attachments / 32 bytes per sample; depth sampled in a later pass | G-buffer must fit 32 B/sample; no same-pass depth sampling |
| Motion/validity | Data-level contract; `rg16float` renderability needs a capability check | Keep `rg16f` velocity; check `texture-formats-tier1` where required |
| Overlay separation | Application-level composition | Keep; display HUD pass after reconstruction |
| Device removal | `device.lost` + resource recreation; no DRED | Capability-specific recovery; fault-injected test |
| Shader compilation | WGSL only; no HLSL/DXC/SPIR-V input | Shader module = backend blob (DXIL/WGSL); HLSL is not a public RHI input |
| Compute/UAV ordering | No explicit UAV barrier; usage scopes and pass boundaries | Declared storage access + pass dependencies; no public barrier |
| Native-provider interop | No native D3D12 handles in a browser page | Native-only extension; not in the portable core |
| Error model | Null adapter, promise rejection, error scopes, device loss | Typed `validation`/`out-of-memory`/`device-lost`/`unsupported`; no HRESULTs |

**Amendment status (2026-10-28).** This audit is the basis on which Q1a was amended. `specs/004-quality-renderer/contracts/rhi-foundation.md` now carries the three tiers, the tier mapping, the header gate and the amended Q1a seam. Q1a's clear/copy/readback behavior stays; its public acceptance changes:

- The state sequence is still executed and asserted, but as a D3D12-backend-internal hook.
- The outstanding-fence release test asserts `GetCompletedValue() < signalValue` inside the D3D12
  backend; the public test asserts a completion handle is outstanding.
- A WebGPU variant uses `onSubmittedWorkDone`, error scopes, `copyTextureToBuffer` with a 256-byte
  `bytesPerRow`, and `mapAsync`.
- `width * 4` as a staging stride is a D3D12 negative control; on WebGPU it is **not applicable**
  because `bytesPerRow` must be 256-aligned. Do not count it as a pass.
- D3D12 Q1a output is not browser portability evidence.

### 3.3 No D3D12 vocabulary in engine-facing headers

Before Q2, run a header review with one hard rule: no public core type, field or function may
require a D3D12 noun. Prohibited examples:

- `fenceValue`, `GetCompletedValue`, `signalValue`
- `transition`, `barrier`, `ResourceState`, `RENDER_TARGET`, `COPY_SOURCE`
- `commandAllocator`, `commandList`, `Reset`
- `descriptorHeap`, `descriptorRange`, `rootSignature`
- `Present`, `swapChain`
- `D3D12_*`, `ID3D12*`, `HRESULT`
- HLSL source or DXC include paths in a portable shader-module descriptor

A mapping table from every public field to a WebGPU operation is the Q7a gate. If a field has no
mapping, it belongs to the D3D12 private tier or a native extension.

---

## 4. WebGPU capability envelope

### 4.1 What WebGPU covers

The W3C WebGPU Candidate Recommendation Draft of 2026-09-15 provides:

- One queue per device; render and compute passes; draw and dispatch.
- Up to 8 colour attachments within 32 bytes/sample by default (compatibility mode: 4 attachments).
- `rgba16float` HDR scene colour and `rg16float` velocity; both are core plain colour formats.
- Depth sampled as a texture in a later pass; `depth24plus-stencil8` and `depth32float`.
- Stencil `increment-wrap`/`decrement-wrap` with independent front/back state.
- Occlusion queries (they are **core**, not absent; an earlier draft assumption to the contrary is
  wrong).
- Timestamp queries as an optional feature.
- Buffer readback via copy + `mapAsync`; `writeTexture` for unaligned uploads.
- Canvas `configure`/`getCurrentTexture`; implicit compositing.
- Error scopes, uncaptured-error events and `device.lost`.

These are enough for the classic ARB2 path and for a modern hybrid raster renderer.

### 4.2 What WebGPU does not provide

- No explicit resource-transition/barrier API; synchronization is implicit through usage scopes.
- No fence objects or fence values; only queue-wide completion and `mapAsync`.
- No second queue or async compute; no resource aliasing; no sparse resources.
- No ray tracing, acceleration structures or inline ray queries.
- No geometry/tessellation shaders, transform feedback, conditional rendering, logic ops.
- No bindless/binding arrays, no multi-draw indirect.
- No polygon/fill mode, no depth-bounds test, no clip planes (unused by DOOM 3, except the
  alphaNotch workaround that is already shader-side).
- No fixed-function alpha test or texgen; no L8/LA8/A8 formats; BC compression is optional.
- No application-level pipeline cache or PSO serialization; `getCompilationInfo` contents are
  implementation-defined.
- No `present()`; `wgpuSurfacePresent` aborts in the browser port.
- No native D3D12 handles or provider interop.

### 4.3 Default limits that shape the renderer

W3C CRD 2026-09-15 defaults (grade 1):

| Limit | Default | Consequence |
|---|---|---|
| `maxBufferSize` | 256 MiB | Large vertex/index buffers and staging must be chunked |
| `maxStorageBufferBindingSize` | 128 MiB | GI/reflection buffers must fit or use multiple bindings |
| `maxTextureDimension2D` | 8192 | 4K targets fit; very large atlases do not |
| `maxBindGroups` | 4 | Frame / view-light / material / per-draw is the natural split |
| `maxSampledTexturesPerShaderStage` | 16 | Seven interaction textures fit; leave headroom |
| `maxStorageBuffersPerShaderStage` | 8 | Compute GI/reflection jobs must be budgeted |
| `maxUniformBuffersPerShaderStage` | 12 | Fine for the classic path |
| `maxUniformBufferBindingSize` | 64 KiB | Per-light/per-draw data must be packed |
| `minUniformBufferOffsetAlignment` | 256 B | Uniform ring slots are 256-byte aligned |
| `maxVertexBuffers` | 8 | DOOM 3's single interleaved vertex buffer fits |
| `maxVertexAttributes` | 16 | Position/ST/normal/tangents/colour fit |
| `maxColorAttachments` | 8 | G-buffer fits if channel count is controlled |
| `maxColorAttachmentBytesPerSample` | 32 B | 8+8+4+4+4 = 28 B fits; four RGBA16F = 32 B exactly |
| `maxComputeInvocationsPerWorkgroup` | 256 | Compute tile sizes must respect this |
| `maxImmediateSize` | 64 B | Push constants cannot carry the ARB2 parameter block |

A G-buffer such as `rgba8 albedo/AO` (4) + `rgba8 octahedral normal + roughness/metallic` (4) +
`rg16f velocity` (4) + `rgba8 emissive/flags` (4) = 16 B, plus `depth24plus-stencil8`, fits
comfortably. A 28 B layout also fits. The exact channel study is still open.

### 4.4 Browser availability and evidence

As of 2026-10-27:

- Chrome 113+ on ChromeOS/macOS/Windows; Chrome Android 121+ on Android 12+ with Qualcomm/ARM GPUs;
  Chrome 144 adds Linux (Intel Gen12+ initially).
- Firefox 141 on Windows; Firefox 145 on macOS 26 Tahoe Apple Silicon; Firefox Android has no
  WebGPU.
- Safari 26.0 on macOS/iOS/iPadOS/visionOS.
- WebGPU is not Baseline. A September 2026 caniuse snapshot reports 85.8% full support plus 2.02%
  partial support (87.82% total), not the earlier 3.66% partial figure. [Grade 3 — 仅供参考]
- Headless Chromium disables SwiftShader by default (2026-10-27 commit); WebGPU tests need
  `--enable-unsafe-webgpu` and, for software, `--enable-unsafe-swiftshader`. Playwright's default
  `chromium-headless-shell` is not a reliable WebGPU runner; Playwright WebKit has no WebGPU.
- No public DOOM 3 WebGPU port was found. The search is date-scoped and is not proof of novelty.

Browser support is time-sensitive. Every profile claim must be re-probed on the pinned browser
before release.

### 4.5 Emscripten / emdawnwebgpu

- WebGPU in Emscripten is the external **emdawnwebgpu** port, implemented in `google/dawn` and
  integrated as an Emscripten port (`--use-port=emdawnwebgpu`); the old `-sUSE_WEBGPU` path is
  removed. [Grade 3 — 仅供参考]
- The port pins Dawn (currently `v20260423.175430`) and requires Emscripten 4.0.10+.
- It exposes the stable `webgpu/webgpu.h` C API; the Dawn-style C++ bindings are explicitly
  **not stable**.
- `wgpuSurfacePresent` aborts; use canvas `getCurrentTexture` and rAF compositing.
- The earlier claim that emdawnwebgpu's `WaitAny` aborts without Asyncify was **refuted**:
  `timeoutNS == 0` is a synchronous C++ poll; timed waits above zero require Asyncify/JSPI and the
  `TimedWaitAny` instance feature. A rAF-driven renderer should use timeout-0 polling or promises
  and avoid timed waits. This still needs a build/run confirmation before it is frozen into the
  architecture.

---

## 5. Classic ARB2 path on WebGPU, concretely

### 5.1 Command-list mapping

| Engine command | Engine adapter action | WebGPU |
|---|---|---|
| `RC_SET_BUFFER` | Set the current RHI render target and load/store intent | Begin/reuse a render pass on the target |
| `RC_DRAW_VIEW` | Open a render pass with viewport/scissor from `renderCrops`; run the view | `beginRenderPass` with attachments |
| `RC_COPY_RENDER` | Copy the source texture to the current render image | `copyTextureToTexture` or a blit pass |
| `RC_SWAP_BUFFERS` | End recording, submit, mark the surface frame boundary | `queue.submit`; no `present()` |
| `RC_NOP` | No-op | No-op |

The engine adapter preserves order and supports mid-frame flush. It does not wrap every historical
`gl*` call; it consumes the five command types and the pass structure.

### 5.2 Frame graph

```text
acquire surface
  -> depth/stencil prepass (alpha discard, colour black, depth write)
  -> for each light with shadows/interactions:
       [restore stencil to 128, scissored]
       shadow volumes (stencil incr/decr, no colour)
       interactions (additive, depth equal, 7 textures)
  -> light scale (fullscreen multiply)
  -> opaque shader passes (material pipelines)
  -> fog / blend lights
  -> if post-process:
       copy scene colour to offscreen texture
       new pass sampling the copy
  -> GUI pass (640x480 ortho, blended)
  -> end, submit, surface frame boundary
```

The per-light block is the part most likely to force extra passes. The pass graph must be
instrumented before Q2 acceptance: draws, passes, lights, shadow fill and stencil-fill cost per
representative map.

### 5.3 Stencil clear policy — recommended

Use the **scissored stencil-fill draw** first:

- One scene pass per view (or a small number).
- Before each light, draw a fullscreen or light-bounds triangle with:
  - colour writes off,
  - depth test off (or always),
  - stencil test always, reference 128, write mask 0xFF,
  - stencil pass op `replace`,
  - scissor set to the light's scissor.
- Then draw shadow volumes with the original cull/stencil ops.
- Then draw interactions with `GEQUAL 128`.

This preserves the GL scissor semantics without a full-surface clear. The cost is one extra
fullscreen-ish draw per shadowed light; measure it. If it dominates, evaluate per-light passes with
`loadOp: "clear"` only where the stencil marks are provably dead. Do not choose the per-light pass
first and discover mirror/external-shadow breakage later.

D3D12 must implement the same scissored fill (a fullscreen triangle with a scissor) so parity tests
compare the same semantics.

### 5.4 Shadow-volume specifics

- Keep CPU shadow-volume generation and the near-plane fixup unchanged.
- Keep front/back culling and the two-pass preload/increment-decrement sequence.
- Map `GL_INCR`/`GL_DECR` to `increment-clamp`/`decrement-clamp`; wrap variants to
  `increment-wrap`/`decrement-wrap`. Verify which pair the engine selects.
- Drop depth bounds; replace with tighter scissor or a stencil/depth mask. Measure the cost.
- Polygon offset for shadow volumes maps to `depthBias`/`depthBiasSlopeScale`; verify the sign and
  clamp against GL.
- `r_showShadows` debug modes use line geometry or a colour output; no polygon mode.

### 5.5 Interaction specifics

- One bind group with seven textures: normal cube/ambient normal, bump, falloff, light projection,
  diffuse, specular, specular table.
- One uniform slot per light/draw: the ARB2 environment block (light origin, view origin,
  projection S/T/Q, falloff S, bump/diffuse/specular matrices, colour modulate/add). Pack to
  256-byte alignment; no push constants.
- Vertex layout: position, ST, normal, tangent0, tangent1, colour; no client arrays; GPU index
  buffers mandatory.
- Additive blend, depth mask on, depth compare equal for the main interaction; translucent
  interactions use less.
- Pipeline key: interaction shader variant + blend/depth state; cache and precompile.

### 5.6 Light scale, opaque stages, fog

- Light scale is a fullscreen multiply pass; keep the overbright factor as a uniform.
- Opaque stages resolve a material pipeline from `GLS_*` state; alpha-tested stages use the discard
  variant; blend stages keep their blend state.
- Fog/blend lights are forward passes with re-authored projections.
- The second `RB_STD_DrawShaderPasses` loop for post-process surfaces runs after fog and uses the
  copied `_currentRender` texture.

### 5.7 Mirrors, security cameras, subviews

- A mirror or remote camera has its own viewport/scissor and clip plane; its surfaces are rendered
  into the current target in GL.
- On WebGPU, if the subview samples the scene or needs an independent target, render it into an
  offscreen texture, then composite. Preserve recursion order and the clip-plane alphaNotch.
- The main-view history must not be committed by a mirror, shadow or security view (Q3).
- Nested subviews need explicit target lifetimes; do not cache a surface texture across frames.

### 5.8 GUI and scaling

- The display GUI is a second pass with the 640x480 ortho projection scaled to the canvas.
- DPI/resize: configure canvas width/height and device-pixel ratio explicitly; re-acquire the
  surface texture every frame.
- The classic path needs no offscreen GUI texture. The modern path puts the HUD in a separate
  display-resolution pass after reconstruction; world GUI stays in the scene pass.

### 5.9 Readback and screenshots

- Replace synchronous `qglReadPixels`/`qglFinish` with `copyTextureToBuffer` + `mapAsync`.
- `bytesPerRow` must be a multiple of 256; repack into tightly packed caller-owned bytes.
- Tiled screenshots become multi-frame asynchronous jobs; the old single-frame loop cannot survive.
- Canvas capture is asynchronous and is not the same evidence as GPU readback; decide which one
  each test asserts.
- Stencil/depth debug dumps must use copyable formats; `depth24plus` is not copyable.

### 5.10 Residency and upload

- WebGPU cannot create or copy a texture inside a render pass. `idImage::Bind` lazy loading must
  become preload or a deferred upload queue flushed before recording/at pass boundaries.
- A non-resident image binds a documented fallback and logs once; it does not render silent black.
- Watch for memory spikes and hitches that do not exist in the GL build.

---

## 6. Shader strategy

### 6.1 Inventory and provenance

- `R_LoadARBProgram` reads `glprogs/<name>` from game data at runtime and submits the text with
  `qglProgramStringARB` (`draw_arb2.cpp:365-456`). A missing file prints `: File not found` and
  returns; the material can render unshaded/black.
- The static ARB2 program table has 18 entries over 13 distinct files; the compiled ARB2 path binds
  six programs: `interaction.vfp`, `test.vfp`, `shadow.vp`, `bumpyEnvironment.vfp`,
  `environment.vfp`, `arbFP_glasswarp.txt`. `ambientLight.vfp` and `arbVP_glasswarp.txt` are not
  bound. `megaTexture.vfp` is forced by the material keyword, and material stages may name
  arbitrary `vertexProgram`/`fragmentProgram` files.
- The six-program inventory is a lower bound, not a completeness proof. A material/program census
  on retail data is required.

Legal boundary:

- The GPL source release contains no game data; retail `glprogs` are EULA-bound.
- Reading retail ARB text and porting or auto-translating it can create a derivative work. Do not
  commit translated text, fixtures or generated files.
- id's DOOM 3 BFG Edition GPLv3 HLSL programs are a useful reference but carry additional GPL
  terms and are not layout/parity-equivalent to DOOM 3's ARB2 shaders (half types, different
  sampler/texcoord layout, extra passes). Treat them as reference, not a drop-in.
- Author the classic-equivalent shaders from the C++ ABI, public ARB specs and independent
  implementation. Keep a provenance log and obtain counsel review before any retail-derived study.

### 6.2 Build graph

```text
project-owned HLSL
  -> DXC -> DXIL (SM 6+)                     -> D3D12
  -> DXC -> SPIR-V (Vulkan 1.1 target env)
       -> Tint (pinned) -> WGSL              -> WebGPU
```

- There is no official direct HLSL -> WGSL route. Tint has no HLSL reader; Naga has no HLSL
  frontend. The SPIR-V hop is mandatory unless Slang is adopted as the single source.
- Slang can emit WGSL directly, but its WGSL target has documented gaps; it is an option, not a
  free win.
- Pin DXC, Tint, Emscripten and Dawn. Validate every generated WGSL module in CI and test the
  actual DXC output through Tint; the mapping is an ongoing DXC effort, not a compatibility
  guarantee.
- Shader modules are per-backend blobs. HLSL is not a portable RHI input.

### 6.3 ARB semantic emulation

ARB fragment programs are straight-line (no branches/loops). The replacement library must cover:

- `TEX`, `TXB`, `TXP`, `KIL`, `LIT`, `DST`, `SCS`, `CMP`, `LRP`, saturating writes, environment and
  local parameter arrays.
- `TXP` has no projected sample builtin: divide coordinates by `q` and use `textureSampleLevel`.
- Alpha test: `discard`; sample before the discard; use explicit LOD/`textureSampleLevel` where
  control flow could become non-uniform. `textureSample` requires uniform control flow and
  `derivative_uniformity` defaults to an error.
- Use fp32; do not inherit BFG's half precision.
- ARB vertex programs use `ARL` relative addressing; emulate with dynamic indexing (the live set is
  small).
- Seven interaction textures fit one bind group; the uniform block goes in a 256-byte-aligned ring.

### 6.4 Variants, warm-up and failure

- The independent `GLS_*` field combination upper bound is 55,296; do not create all combinations.
  Key pipelines by material stage + blend/depth/stencil + alpha-test mode; cache.
- Precompile at level load. D3D12 can persist a `PipelineLibrary`; WebGPU has only an opaque
  internal cache, so keep the variant count bounded and accept first-run/cache-eviction hitches
  until measured.
- A missing program or failed pipeline is an explicit error with material and program names, not a
  black frame.
- Add a test that every shipped material resolves to an owned shader. A browser run that looks fine
  while materials are unlit is a realistic failure.

### 6.5 `megaTexture.vfp` and arbitrary programs

- Decide explicitly: support, stub, or report unsupported. Virtual texturing needs sparse/tiled
  feedback and uploads; WebGPU's default limits and absent sparse resources make a full
  implementation nontrivial.
- Known program names get a resolver entry; unknown names get a documented fallback and a console
  error.
- An optional runtime ARB -> WGSL translator for **user-owned** data may be offered behind a flag;
  it must never ship translated retail text, fixtures or derived files.

---

## 7. Modern path on WebGPU (the path-3 subset)

The path-3 foundation (hybrid deferred/forward + HDR + GI/reflections + temporal reconstruction)
fits WebGPU if the budget is fixed now:

| Item | WebGPU-compatible design |
|---|---|
| Depth prepass | `depth24plus-stencil8`; alpha discard; no colour |
| G-buffer | <= 8 attachments, <= 32 B/sample; e.g. albedo/AO (RGBA8), octahedral normal + roughness/metallic (RGBA8), velocity (RG16F), emissive/flags (RGBA8) = 16 B + depth |
| Deferred lighting | Reads G-buffer and depth as textures in a later pass; writes `rgba16float` HDR |
| Shadows | Shadow atlas/mask in a separate pass; classic stencil path remains for compatibility |
| GI/reflections/volumetrics | Compute passes with storage buffers; 8 storage buffers/stage; no UAV barrier |
| Transparency/special/world GUI | Forward/compatibility pass |
| Temporal reconstruction | Motion + validity + ping-pong history; in-engine upscaler; no DLSS/NR |
| HUD | Display-resolution pass after reconstruction |
| MSAA | Prefer TAA; if MSAA, depth resolve is manual (no `resolveTarget` for depth) |
| Ray tracing | **Not available**; path-4 is native-only |
| DLSS 5 / NR | **Not available**; native-only, separately authorized |

Native and web share algorithms but not identical presets. The native profile can use DXR, DLSS/NR,
bindless, multi-queue, sparse/aliasing and larger limits; the web profile cannot. Keep those behind
capability interfaces so they do not rot the portable core.

The web ceiling is therefore: modern raster/hybrid look, HDR, GI/reflections, temporal stability,
60 Hz-class target unproven. It is not the path-4 path-traced reference. State that in the product
copy.

---

## 8. Platform gates that affect rendering

These are not the main topic, but a browser rendering test cannot run without them.

- **Worker + OffscreenCanvas.** Put the engine/renderer in a Web Worker that owns the `GPUDevice`
  and `OffscreenCanvas`; the main thread handles input, audio and rAF. Synchronous file APIs and
  OPFS `createSyncAccessHandle` are worker-only.
- **Main-loop inversion.** Replace `while(1) { Win_Frame(); common->Frame(); }` with a frame
  function driven by `emscripten_set_main_loop` or a rAF/message loop, with a 60 Hz tic accumulator.
  Do not assume Asyncify is mandatory; verify the timeout-0 wait path by build/run.
- **Asynchronous initialization.** Adapter/device, surface configure, shader modules and pipeline
  warm-up are asynchronous; show a loading state and do not present before warm-up.
- **VFS and memory.** Retail data is ~2.2 GB; wasm32 is capped at 4 GiB; Emscripten defaults to a
  2 GB maximum; `memory64` is not available in Safari. A full preload is not viable. Use
  user-supplied paks, WORKERFS/OPFS, and a range/streaming layer behind `fopen`/`unzOpen`; keep
  MEMFS for a demo or synthetic fixture. Start with demo data and measure.
- **Static game module.** Browsers have no `dlopen`; link game code statically into the wasm build.
- **Assets.** BC is optional; transcode or enable `texture-compression-bc`. Convert L8/LA8/A8 with
  shader swizzles. Converted assets and paks stay out of git.
- **Input/audio.** Pointer lock needs a gesture; Web Audio starts suspended and needs a gesture.
- **Threading.** Ship single-threaded first; pthreads need `-pthread`, COOP/COEP and
  SharedArrayBuffer. WebGPU itself does not require COOP/COEP.
- **Device loss.** `device.lost` invalidates resources; recreate and warm up. No DRED on the web;
  error scopes and a fault-injected test are the evidence.
- **Browser CI.** Pin Chrome; use a GPU or `--enable-unsafe-swiftshader` +
  `--enable-unsafe-webgpu`; do not use Playwright's default headless shell; capture a frame plus a
  zero-uncaptured-error log.

d3wasm (Emscripten + WebGL2, single-threaded, demo preload) is the closest public precedent:
~400 MB demo data, ~850 MB RAM, ~30-40 FPS on desktop, 40-60% slower than native GLES. These are
the author's self-reported, undated, WebGL2 numbers (grade 4); they are precedent, not a budget for
this port. [Grade 4 — 仅供参考]

---

## 9. Architecture comparison and decision

| Criterion | A: one portable RHI, D3D12 + WebGPU | B: separate Emscripten GL -> WebGL2 | C: WebGPU-shaped RHI, Dawn native + browser |
|---|---|---|---|
| On "our RHI" | Yes, both profiles | No; a separate renderer | Yes, one API shape |
| Reuse | Highest: one engine adapter, material/image/vertex pipeline, one shader source | Data/game logic only; two renderers | High: one shader language, one conformance model |
| Native ceiling | Full path 4 (DXR, DLSS/NR) behind extensions | Unchanged native flagship; web capped | C1 caps native (no RT/NR); C2 needs a side channel |
| Web ceiling | Hybrid raster + compute + TAA; no RT/NR | Classic/GLES2 ceiling; no compute/storage/RT | Same as WebGPU ceiling |
| Effort | Highest: contract rewrite, two backends, shader re-authoring, browser stack | Medium: ARB -> GLSL, Emscripten shell, WebGL2 renderer | Medium-high: Dawn integration + WGSL; C2 adds native RT/DLSS |
| Risk | High: contract leakage, pass cost, shader parity, memory | Medium: WebGL2 dead end, two renderers, shader rewrite | High: caps the stated ceiling; C2 undesigned |
| Maintenance | High but single-source | Two renderers diverge | Medium; one API but Dawn pin |
| Verdict | **Recommended** | Fallback/experiment only | Reject C1 as flagship; C2 only if single-conformance outranks ceiling |

Why A:

- It is the only design that can run the same engine adapter and pass structure on both profiles.
- It preserves the native path-4 ceiling behind explicit extensions.
- It makes the browser a first-class profile instead of a late Q7 appendix.
- It forces the RHI contract to be honest before Q2, which is cheaper than retrofitting later.

Why not B as the primary:

- It is not "on our RHI"; it abandons the portable core for the browser.
- WebGL2 has no compute/storage/indirect/RT; it cannot run the path-3 modern renderer as specified.
- It still requires ARB -> GLSL re-authoring, so it is not a shortcut on the shader problem; it
  just moves it to a dead-end language.
- It is useful as a **shortest browser proof/fallback** if the product priority changes to
  "classic DOOM 3 playable now, native is the showpiece".

Why not C1:

- A WebGPU-shaped RHI with Dawn native is elegant, but it cannot express DXR or DLSS/NR; the
  path-4 endpoint is outside the API, not merely hard.
- C2 (portable core + native RT/DLSS side channel) reintroduces a second synchronization model and
  shader path at the top end; it is more complex than A's three-tier contract, not less.

Decision record needed: state the primary deliverable (native flagship, browser profile, or both)
and that A is chosen with a three-tier contract. Without that, 003's GL-first text and its
still-unamended D2 line continue to conflict with the amended 004 contract.

---

## 10. Sequencing and gates

| Step | Work | Exit evidence |
|---|---|---|
| S0 | Publish 003/004 through a branch/PR; correct `02_renderer.md`'s four errors; mark 003's D3D12 prohibition and GL-first checklist as superseded; rewrite ROADMAP section 5 and AGENTS' web-migration self-description | **Partially done (2026-10-28):** 003/004 are published (#10), and this document plus the three-tier contract amendment and the amended issue #11 mirror are the S0 contract decision (Refs #20). The live issue body is not yet updated (the current token lacks issue-write permission). Still open: `02_renderer.md`, the 003 spec/checklist supersession, ROADMAP section 5 and the AGENTS web-migration wording. Exit evidence: tracked docs; no implementation worktree depends on an untracked spec once this change merges. |
| S1 | Amend Q1a: portable headers, completion handles, D3D12-private fence/state journal, WebGPU-shaped test seam | **Contract amended (Refs #20); implementation and tests remain issue #11.** Exit evidence still required: Q1a test passes on D3D12; header review shows no D3D12 nouns |
| S2 | **Q7a / Q1a.5 WebGPU conformance spike**, before Q2: Dawn-native fixture + pinned-Chrome canvas fixture; adapter/device, buffer, texture, bind group, one MRT pass, one compute dispatch, `copyTextureToBuffer` with 256 pitch + `mapAsync`, surface configure/`getCurrentTexture`, device-loss path | Every public header field maps to a WebGPU operation; no D3D12 noun required; captured browser frame; zero uncaptured errors |
| S3 | Q1b dual-target: draw/compute/MRT/readable depth/motion/validity/overlay/frame slots/resize/history; WGSL module + pipeline-layout checks | D3D12 and WebGPU fixtures; D3D12 GBV gate separate; browser error-scope gate |
| S4 | Q2 engine adapter + classic path on both: port `RB_ExecuteBackEndCommands` and `draw_common`; ARB2 order; offscreen splits; async readback; residency; GUI pass | GL vs D3D12 vs WebGPU parity captures with predeclared tolerances; no-D3D12-vocabulary review; browser classic frame |
| S5 | Q3 motion/history/HUD: velocity/validity, ping-pong history, private skinned poses, per-view history, display HUD | Analytic reprojection; mirror/shadow isolation; HUD ordering |
| S6 | Q4 modern hybrid on both: deferred/forward, HDR PBR, shadows, compute GI/reflections/volumetrics, TAA; web uses in-engine upscaler | G-buffer fits the capability table; visual review; web frame captures |
| S7 | Q5/Q6 native only: optional authorized NR; DXR path-traced reference/showcase | Actual backend create/evaluate; static/dynamic path-traced captures; web reports unsupported |
| S8 | Q7b real browser profile: worker host, VFS, canvas, input/audio, pinned-browser tests; accepted after Q4 | Browser build, real scene, capability table, memory/startup numbers, zero uncaptured errors |
| S9 | Q7c WebGL2/mobile/other browsers, later and separate | Its own capability/fallback table and real-scene test |

Continuous platform track from S2: worker shell, rAF loop, canvas, pak streaming, headless Chrome.
Do not leave the browser shell as a cliff after Q4.

S2 is the most important change to the current plan. The current Q7 is after Q6, which defers the
highest-risk work (shader source, VFS, loop, WGSL, browser CI) until after the native DXR
programme. A conformance spike before Q2 costs little and prevents public-header surgery.

---

## 11. Risks, unknowns and negative results

| Risk | Why it matters | Gate/mitigation |
|---|---|---|
| Contract leakage | Fences/transitions/allocators/descriptor ranges/HLSL in engine-facing headers make WebGPU a rewrite | Q7a mapping table + header review before Q2 |
| Per-light stencil clear | WebGPU cannot clear inside a pass; full clear changes mirror/external-shadow semantics | Scissored fill first; instrument pass/draw counts; test mirrors/subviews |
| Pass splitting at `SS_POST_PROCESS`/mirrors | Changes order, memory and subview recursion; number of shipped materials unknown | Retail material census; nested-subview tests; parity captures |
| Texgen re-authoring | No source in repo; errors show as mirror/fog artifacts, not compile errors | Retail-data A/B; per-mode tests |
| Missing shader programs | Silent black/unshaded rendering today | Fail-loud resolver; test every material resolves |
| Retail shader legality | EULA data; derivative-work risk | Project-owned HLSL; provenance log; counsel; no translated retail text in git |
| Missing retail inventory | Six live programs is a lower bound; `megaTexture` and arbitrary material programs unknown | Census on legal local data; explicit support/stub decision |
| Texture formats | BC optional; no L8/LA8/A8; conversion cost | Capability table; transcode path; measured memory/load |
| Wasm memory/VFS | 2.2 GB retail vs 4 GiB wasm32 vs 2 GB Emscripten default; Safari no memory64 | Demo-first; OPFS/WORKERFS; measured budget; no full-retail claim |
| Browser CI | Headless GPU fragile; SwiftShader off by default; Playwright WebKit no WebGPU | Pinned Chrome; explicit flags; SKIPPED semantics; browser matrix |
| Device loss | No DRED; resources invalid | Fault-injected loss test; error scopes; recovery path |
| Two-backend maintenance | D3D12-only features can rot the web profile; web limits can downgrade native | Capability interfaces; per-profile CI; shared algorithms |
| Documentation drift | ROADMAP/AGENTS/003 still describe GL-first web; agents follow the old plan | S0 publish/supersede; one decision record |

### Evidence still missing

- Per-frame draw calls, render passes and visible-light counts on a representative map.
- Retail material/program census; `SS_POST_PROCESS` usage; `megaTexture` usage.
- Any visual parity measurement for re-authored ARB2 shaders.
- Any WebGPU vs native D3D12 real-engine measurement. Public WebGPU-vs-WebGL results are mixed
  (grade 3-4) and are not DOOM 3 data.
- A build/run of the timeout-0 WaitAny path in a worker/rAF design.
- Headless Chrome WebGPU reliability on the CI GPU; the stable Chrome containing the SwiftShader
  default change.
- G-buffer channel study and MSAA policy.
- Per-browser availability of BC, timestamp-query, clip-distances, float32-filterable,
  OffscreenCanvas WebGPU; Firefox/Safari limits above defaults.
- Wasm peak memory, pak streaming throughput, OPFS sync-handle reliability.
- Browser device-loss/background-tab behavior.
- DOOM 3 EULA text and derivative-work review; shader provenance process.
- Any browser conformance fixture (none exists).

### Do not claim

- A browser port, WebGPU support or browser compatibility from Q1a/Q1b/Q2 or from a
  backend-neutral header.
- That the Q1a/Q1b contract is portable in practice; Q1a still runs only on D3D12, even though the amended public acceptance no longer names fence, state-journal, debug-layer or GBV mechanisms — they are D3D12-private.
- That WebGPU supports DXR, DLSS 5/NR, bindless, multi-queue, sparse resources, resource aliasing,
  depth bounds, polygon mode, fixed-function alpha/texgen, L8/LA8/A8, or guaranteed BC.
- That retail `glprogs` or translated ARB programs are distributable.
- That shader re-authoring is mechanical.
- That full retail data fits wasm32 or meets an FPS/memory budget without measurement.
- That Architecture B is a custom-RHI browser port.
- That Architecture C meets the path-4 endpoint.
- That a skipped adapter/browser/validation/data check is a pass.
- That a Dawn-native or SwiftShader result is browser support or hardware performance.
- That the custom RHI produces better pixels than NVRHI; that is an assumption in 004, not
  evidence.

---

## 12. Source grades

Grades follow the project's research convention: 0 = mathematics/physics; 1 = primary source
(official standard/legal text/official source code); 2 = authoritative specification/reference;
3 = vendor/official site/MDN/caniuse/GitHub repository; 4 = self-report/community/benchmark.
Grade >= 3 is used only as corroboration and is marked "仅供参考".

| ID | Source | Grade | Use in this document |
|---|---|---|---|
| `doom3-src` | Pinned `doom3_orig/doom3` submodule, GPL release | 1 | Frame, command list, ARB2 pass order, stencil shadows, projection, GUI |
| `id-draw-arb2` | https://github.com/id-Software/DOOM-3/blob/master/neo/renderer/draw_arb2.cpp | 1 | ARB program loading, program table, interaction uniforms |
| `id-material` | https://github.com/id-Software/DOOM-3/blob/master/neo/renderer/Material.cpp | 1 | Arbitrary `vertexProgram`/`fragmentProgram` names |
| `id-readme` | https://github.com/id-Software/DOOM-3/blob/master/README.txt | 1 | No game data in GPL release; EULA applies |
| `w3c-webgpu` | https://www.w3.org/TR/webgpu/ (CRD 2026-09-15) | 1 | Limits, formats, synchronization, queries, canvas, errors |
| `w3c-wgsl` | https://www.w3.org/TR/WGSL/ | 1 | `discard`, sampling uniformity, no projected sample, storage access |
| `khronos-arb-fp` | https://registry.khronos.org/OpenGL/extensions/ARB/ARB_fragment_program.txt | 2 | ARB fragment semantics |
| `khronos-arb-vp` | https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_program.txt | 2 | ARL relative addressing |
| `webgpu-headers` | https://webgpu-native.github.io/webgpu-headers/Asynchronous-Operations.html | 2 | Future/wait model |
| `memory64` | https://raw.githubusercontent.com/WebAssembly/memory64/bfd637e13c5a9329c91c09f8d69adab895435d97/proposals/memory64/Overview.md | 2 | 4 GiB wasm32 cap |
| `emscripten-webgpu` | https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html | 3 (仅供参考) | emdawnwebgpu port |
| `dawn-emdawnwebgpu` | https://github.com/google/dawn/tree/main/src/emdawnwebgpu | 3 (仅供参考) | Port location, API stability |
| `dawn-pin` | https://raw.githubusercontent.com/emscripten-core/emscripten/main/tools/ports/emdawnwebgpu.py | 3 (仅供参考) | Pinned Dawn release |
| `mdn-bcd` | https://raw.githubusercontent.com/mdn/browser-compat-data/main/api/Navigator.json | 3 (仅供参考) | Browser versions |
| `caniuse-webgpu` | https://caniuse.com/webgpu | 3 (仅供参考) | Usage share |
| `chrome-webgpu` | https://developer.chrome.com/docs/web-platform/webgpu/overview | 3 (仅供参考) | Platform availability |
| `firefox-141` | https://www.firefox.com/en-US/firefox/141.0/releasenotes/ | 3 (仅供参考) | Firefox Windows support |
| `webkit-26` | https://webkit.org/blog/17333/webkit-features-in-safari-26-0/ | 3 (仅供参考) | Safari 26 support |
| `dxc` | https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst | 2 | HLSL -> SPIR-V target and limits |
| `tint` | https://dawn.googlesource.com/dawn/+/refs/heads/main/docs/tint/arch.md | 2 | Tint readers/writers; no HLSL |
| `naga` | https://github.com/gfx-rs/wgpu/blob/trunk/naga/README.md | 2 | Naga frontends; no HLSL |
| `slang` | https://github.com/shader-slang/slang/blob/master/docs/generated/design/cross-cutting/targets.md | 2 | Slang WGSL target |
| `bfg` | https://github.com/id-Software/DOOM-3-BFG | 1 | GPLv3 HLSL reference; additional terms |
| `d3wasm` | https://github.com/gabrielcuvillier/d3wasm | 3 (仅供参考) | Precedent |
| `d3wasm-perf` | https://www.continuation-labs.com/projects/d3wasm/ | 4 (仅供参考) | Self-reported FPS/RAM |
| `steam-doom3` | https://store.steampowered.com/app/9050/DOOM_3/ | 3 (仅供参考) | 2.2 GB data size |

Time-sensitive facts (browser support, caniuse share, headless behavior, Dawn/Tint/Emscripten pins)
must be re-probed before any implementation decision. No source here guarantees performance, visual
parity or legal clearance.

---

## Appendix A: Q1a contract amendments (summary)

1. Public API: adapter/device, buffer, texture/view/sampler, shader module blob, pipeline layout,
   bind group, one-shot recorder, render/compute pass, copy, async readback, submission handle,
   surface, capabilities, error/device-loss.
2. D3D12-private: fences, transitions/state journal, allocators, descriptor heaps, root signatures,
   query heaps, swap chain, PSO library, debug layer/GBV/DRED.
3. Q1a test: clear a distinct-per-row RGBA8 texture, copy, read back; assert bytes; assert a
   release while the completion handle is outstanding; run hardware/software adapters separately;
   retain the full CTest log.
4. WebGPU variant: same logical test with `copyTextureToBuffer` 256-byte pitch, `mapAsync`, error
   scopes and `onSubmittedWorkDone`; no D3D12 state names.
5. Header gate: a mapping table from every public field to a WebGPU operation; no D3D12 nouns in
   engine-facing headers.
6. Evidence separation: D3D12 Q1a is not browser evidence; a skipped check is not a pass.

## Appendix B: Classic pass mapping (compact)

| Engine pass | D3D12 | WebGPU | Shared adapter decision |
|---|---|---|---|
| Prepass | Depth/stencil target, alpha discard, polygon offset | Same | Keep pass order; alpha in shader |
| Stencil shadow | Stencil ops, scissored clear | Scissored stencil-fill | Same semantics both backends |
| Interaction | 7 textures, additive, depth equal | Bind group + UBO ring | Same material/light data |
| Light scale | Fullscreen multiply | Fullscreen multiply | Same overbright uniform |
| Opaque stages | PSO per GLS state | PSO per GLS state | Same pipeline key |
| Fog/blend | Forward passes | Forward passes | Re-authored texgen |
| Post-process | Offscreen copy + pass | Offscreen copy + pass | Explicit target |
| GUI | Second pass, 640x480 | Second pass, 640x480 | Same projection/scaling |
| Screenshot | Async readback | Async readback | Same caller-owned bytes |

## Appendix C: Decision checklist before implementation

- [x] 003/004 published (#10); 003 §11 carries the dated three-tier refinement.
- [ ] 003's wider superseded text and `02_renderer.md`'s four errors corrected.
- [ ] Primary deliverable stated: native, browser, or both.
- [x] RHI split into portable core + D3D12-private + native extensions — design contract only (issue #20); no implementation.
- [x] Q1a contract amended (public completion token; D3D12-private state/fence hook).
- [ ] WebGPU conformance spike exists before Q2 (T088/Q7a).
- [ ] No D3D12 nouns in engine-facing headers — gate and lexical check defined; no `src/renderer/rhi/**` header exists, so the header scan is NOT RUN.
- [ ] Shader provenance policy and counsel review started.
- [ ] Material/program census plan for legal local data.
- [ ] Demo data set and memory budget named.
- [ ] Browser/version/CI target pinned.
- [ ] Parity tolerances and negative controls recorded before captures.
- [ ] Every claim in this document that is still unexecuted is marked SKIPPED/NOT RUN, not PASS.

---

**Bottom line**: the web renderer is a **new WebGPU backend behind a portable RHI core**, sharing
the engine adapter and pass structure with the native D3D12 renderer. The classic ARB2 path can be
reproduced faithfully enough to preserve DOOM 3's look, and a modern hybrid path fits WebGPU's
limits. The path-4 ceiling (DXR + DLSS 5) does not fit the browser and must be stated as native-only.
The largest risks are the missing shader source, leakage of D3D12-shaped assumptions into the
amended core, and the browser VFS/memory/loop stack; none of them is retired by a desktop build or
a shader translation.
