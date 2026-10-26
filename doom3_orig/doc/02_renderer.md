# 02 — Renderer (`doom3_orig/doom3/neo/renderer`)

Read-only analysis of the DOOM 3 GPL renderer, written to support a port to WebGL2 / WebGPU /
Vulkan / modern GL and to make the renderer legible for AI-assisted modding.

| | |
|---|---|
| Source tree analysed | `doom3_orig/doom3/neo/renderer` (83 files, 2,436,566 bytes), plus `neo/sys/win32/win_qgl.cpp`, `win_glimp.cpp`, `neo/idlib/geometry/DrawVert.h` |
| Build audited | `neo/doomdll.vcxproj` (the only build in the repo; `SConstruct` contains no renderer file list) |
| Method | static reading + grep census of GL entry points; no code was executed or modified |
| Environment | Windows, no game data present (`doom3_orig/doom3/base/` is empty except `default.cfg`) |

**Two facts that shape every migration decision:**

1. **The renderer is a thin, direct-mode GL1.x/GL2-era client.** It has 400 distinct `qgl*`
   entry points and 2,573 textual references, of which 1,540 are in compiled sources
   (2,573 − 394 declarations in `qgl.h` − 342 in `qgl_linked.h` − 297 in the unbuilt
   `draw_exp.cpp`). There is no abstraction layer to swap: `qgl*` **is** the backend API.
2. **The shader programs are not in the repository.** `glprogs/*.vfp|*.vp|*.txt` are loaded at
   runtime from the game pak (`draw_arb2.cpp:365-456`). The repo ships **zero** shader source.
   Any WebGL2/Vulkan port must author a replacement shader set from the C++ side's uniform
   contract (§7.2), not from existing files.

---

## 1. Module map

| File | Lines | Role |
|---|---|---|
| `tr_local.h` | 1687 | The renderer's private universe: `viewDef_t`, `drawSurf_t`, `viewLight_t`, `viewEntity_t`, `drawInteraction_t`, `srfTriangles_t` refs, command structs, all `GLS_*` state bits, all `r_*` cvars |
| `RenderSystem.h` | 276 | Public `idRenderSystem` interface + `glconfig_t` (the capability contract) |
| `RenderWorld.h` | 424 | Public `idRenderWorld` interface + `renderEntity_t`, `renderLight_t`, `renderView_t` |
| `tr_main.cpp` | 1163 | Frame memory (`R_FrameAlloc`), matrix helpers, frustum setup, `R_RenderView` (front-end entry) |
| `tr_light.cpp` | 1633 | Entity/light → view* conversion, interaction creation, ambient drawsurf generation, scissor rects |
| `tr_lightrun.cpp` | 869 | Light/entity derived data (`R_DeriveLightData`, `R_CreateLightRefs`), purge/regenerate |
| `tr_render.cpp` | 890 | Backend view setup (`RB_BeginDrawingView`), draw-element wrappers, interaction submission, `RB_DrawView` |
| `tr_backend.cpp` | 659 | GL state machine (`GL_State`, `GL_Cull`, `GL_SelectTexture`), default state, command dispatch |
| `tr_stencilshadow.cpp` | 1290 | Shadow-volume generation (clipped, per-frustum) |
| `tr_turboshadow.cpp` | 356 | Infinite-projection "turbo" shadow volumes (default path) |
| `tr_shadowbounds.cpp` | 449 | Shadow scissor/bounds bookkeeping |
| `draw_common.cpp` | 1725 | Depth fill, shader passes, **stencil shadow pass**, blend lights, fog lights, light scale, `RB_STD_DrawView` |
| `draw_arb.cpp` | ~560 | `BE_ARB` — fixed-function texenv single/triple-texture interaction |
| `draw_arb2.cpp` | 537 | `BE_ARB2` — the reference path: ARB vertex+fragment programs, `glprogs` loader |
| `draw_nv20.cpp` | ~870 | `BE_NV20` — NV_register_combiners + ARB vertex program |
| `draw_r200.cpp` | ~510 | `BE_R200` — ATI_fragment_shader + ARB vertex program |
| `draw_nv10.cpp` | ~680 | `BE_NV10` — NV_register_combiners only |
| `draw_exp.cpp` | 2618 | **Dead code** — shadow-buffer/deferred research path (pbuffers). Not in the build. |
| `cg_explicit.cpp/.h` | — | **Dead code** — Cg runtime path. Not in the build. |
| `Material.cpp/.h` | 2790 / 700 | `.mtr` parser, expression-register VM, stage/deform tables |
| `Image_*.cpp`, `Image.h` | ~4500 | Image managers, upload, DXT/DDS, image programs, procedural images |
| `Model*.cpp`, `Model.h` | ~4000 | `idRenderModel` + md5/ase/lwo/ma/md3/prt/sprite/beam/liquid, decals, overlays |
| `tr_trisurf.cpp` | 2270 | `srfTriangles_t` allocation, silhouette-edge identification, tangent generation |
| `tr_deform.cpp` | 1260 | Deform vertex shaders on the CPU (sprite, tube, flare, expand, eyeball, particle) |
| `tr_subview.cpp` | 620 | Mirrors, remote cameras, xray subviews (render-to-texture) |
| `tr_guisurf.cpp` | 240 | GUI-on-surface rendering (`R_RenderGuiSurf`) |
| `GuiModel.cpp` | 652 | Batched 2D/3D GUI quad emitter (all 2D drawing funnels through it) |
| `tr_font.cpp` | 560 | FreeType glyph atlas + `RegisterFont` |
| `MegaTexture.cpp` | 700 | Experimental idTech5-style megatexture (`megaTexture` keyword) |
| `Cinematic.cpp` | 1400 | ROQ video decode into a dynamic image |
| `VertexCache.cpp/.h` | 500/143 | VBO arena + frame-temp arena; `Position()` returns an *offset cast to a pointer* |
| `qgl.h`, `qgl_linked.h`, `glext.h`, `wglext.h` | — | Function-pointer declarations / macro aliases / extension enums |
| `../sys/win32/win_qgl.cpp` | 2811 | `QGL_Init` loads `opengl32.dll` and binds every pointer via `wglGetProcAddress` |
| `../sys/win32/win_glimp.cpp` | 1200 | Window, pixel format, WGL extensions, context, gamma ramp, swap |

---

## 2. Front end / back end split

### 2.1 Shape

The renderer is a **deferred command-list renderer**, historically for dual-CPU front/back
threading. That threading was removed (`tr_local.h:525` — *"OBSOLETE: this capability has been
removed"*, `SMP_FRAMES = 1`) but the *structure* survives, and it is the single most valuable
asset for porting: it is already a record-then-submit architecture.

```
game/UI                                                    one frame
   │
   ├─ idRenderWorld::UpdateEntityDef/UpdateLightDef ──► idRenderEntityLocal / idRenderLightLocal (persistent)
   │
   ├─ idRenderWorld::RenderScene(view) ─► R_RenderView(viewDef)      [FRONT END, §2.2]
   │        │  portal flood, culling, interaction creation, shadow volume creation,
   │        │  drawSurf sorting  →  produces a fully self-contained viewDef_t
   │        └─ R_AddDrawViewCmd(viewDef)  → appends RC_DRAW_VIEW to frameData->cmdHead
   │
   ├─ idRenderSystem::DrawStretchPic / DrawSmallChar ... ──► tr.guiModel (batched quads)
   │
   └─ idRenderSystem::EndFrame ─► guiModel->EmitFullScreen()  → another RC_DRAW_VIEW (2D)
                              ─► R_IssueRenderCommands()      [BACK END, §2.3]
                              ─► RB_ExecuteBackEndCommands()  → GL calls
```

Key property: **after `R_RenderView` returns, the front end never needs to touch any
`idRenderEntityLocal`, `idRenderLightLocal`, or model again for this view.** Everything the
backend reads lives in `viewDef_t` / `viewLight_t` / `viewEntity_t` / `drawSurf_t`, all allocated
from frame memory. This is exactly the invariant a modern explicit API needs.

### 2.2 `R_RenderView` pipeline (`tr_main.cpp:1095`)

| # | Step | Code | Effect |
|---|---|---|---|
| 1 | `tr.viewCount++` | `tr_main.cpp:1102` | Invalidates all per-view visibility caches |
| 2 | `R_SetViewMatrix` | `tr_main.cpp:838` | Builds `viewDef->worldSpace.modelViewMatrix` (rotation transpose, translation) |
| 3 | `R_SetupViewFrustum` | `tr_main.cpp:968` | 5 world-space planes (4 sides + front), positive side out |
| 4 | `R_SetupProjection` | `tr_main.cpp:895` | **Infinite-far-z** projection (see §13.3) |
| 5 | `FindViewLightsAndEntities` | `RenderWorld_portals.cpp:840` | Portal flood from `PointInArea(initialViewAreaOrigin)`; marks areas, adds entity/light refs |
| 6 | `R_ConstrainViewFrustum` | `tr_main.cpp:1016` | Shrinks far plane to visible content bounds |
| 7 | `R_AddLightSurfaces` | `tr_light.cpp:875` | Per visible light: create missing interactions, push done shadows, compute scissor |
| 8 | `R_AddModelSurfaces` | `tr_light.cpp:1460` | Per visible entity: ambient drawsurfs + deferred interaction surfaces |
| 9 | `R_RemoveUnecessaryViewLights` | `tr_light.cpp:1570` | Drops shadow chains of lights with no visible surface |
| 10 | `R_SortDrawSurfs` | `tr_main.cpp:1070` | `qsort` on `drawSurf_t::sort` (material sort, stable only by accident) |
| 11 | `R_GenerateSubViews` | `tr_subview.cpp:549` | Recursively renders mirrors / remote cameras (each becomes its own `viewDef_t` + `RC_DRAW_VIEW`) |
| 12 | `R_AddDrawViewCmd` | `RenderSystem.cpp:208` | Appends the command |

### 2.3 Backend command protocol (`tr_local.h:472`)

| Command | Struct | Handler | Notes |
|---|---|---|---|
| `RC_NOP` | `emptyCommand_t` | — | Head sentinel |
| `RC_DRAW_VIEW` | `drawSurfsCommand_t` (`viewDef_t*`) | `RB_DrawView` → `RB_STD_DrawView` | 2D and 3D are the same command type; distinguished by `viewDef->viewEntitys != NULL` |
| `RC_SET_BUFFER` | `setBufferCommand_t` (`GL_FRONT/GL_BACK`) | `RB_SetBuffer` (`tr_backend.cpp:453`) | Emitted once per frame in `BeginFrame` |
| `RC_COPY_RENDER` | `copyRenderCommand_t` | `RB_CopyRender` | `idImage::CopyFramebuffer` (render-to-texture) |
| `RC_SWAP_BUFFERS` | `emptyCommand_t` | `RB_SwapBuffers` | `GLimp_SwapBuffers()` |

Allocation: `R_GetCommandBuffer` (`RenderSystem.cpp:160`) bump-allocates from `frameData` and
links into a singly-linked list. Dispatch: `RB_ExecuteBackEndCommands` (`tr_backend.cpp:600`).
This whole structure maps 1:1 onto a WebGPU `GPUCommandEncoder` and should be preserved, not
deleted.

### 2.4 Frame memory and lifetimes

| Allocator | Where | Lifetime | Used for |
|---|---|---|---|
| `R_FrameAlloc` / `R_ClearedFrameAlloc` (`tr_main.cpp:365,417`) | frame block chain | Until `R_ToggleSmpFrame` after submit | Command buffers, `viewDef_t`, `drawSurf_t`, world-space copies of moved entity parms |
| `R_StaticAlloc` / `R_StaticFree` (`tr_main.cpp:301,335`) | paged static heap | Level lifetime | `srfTriangles_t` bodies, shadow volumes, decals, interaction surfaces |
| `idVertexCache::Alloc*` (`VertexCache.cpp`) | GL VBO arena | Block-RU; frame-temp ring for dynamic | `ambientCache`, `indexCache`, `shadowCache`, `lightingCache` |
| `idBlockAlloc<T,...>` | per-type | Level / world lifetime | `idInteraction`, area refs |

### 2.5 Global mutable state

| Global | Declared | Meaning |
|---|---|---|
| `idRenderSystemLocal tr` | `tr_local.h:809` | Frame counters, `viewDef`, GUI model, crops, viewport offsets, renderer choice, perf counters |
| `backEndState_t backEnd` | `tr_local.h:808`, `tr_backend.cpp:46-110` | `viewDef`, `vLight`, current space/scissor, `glState` shadow copy, light color, `overBright` |
| `glconfig_t glConfig` | `tr_local.h:810` | Capability set; **survives** renderer re-init |
| `frameData_t *frameData` | `tr_local.h:545` | Command chain + frame blocks |
| `idVertexCache vertexCache` | `VertexCache.h:143` | VBO arena |
| `idImageManager *globalImages` | `Image.h` | All textures |

`tr.viewDef` is used as an implicit parameter by dozens of helpers (`R_AddDrawSurf`,
`Material::EvaluateRegisters`, deforms, gui emitter). **Any re-entrant or multi-threaded port must
turn this into an explicit parameter** — it is the main obstacle to parallelizing the front end.

---

## 3. Public interfaces

### 3.1 `idRenderSystem` (`RenderSystem.h:159`)

| Method | Category | Porting note |
|---|---|---|
| `Init`, `Shutdown`, `InitOpenGL`, `ShutdownOpenGL`, `IsOpenGLRunning`, `IsFullScreen`, `GetScreenWidth/Height` | lifecycle/context | `Init` must remain usable headless (dedicated server); `InitOpenGL` is the only GL-touching part |
| `AllocRenderWorld`, `FreeRenderWorld` | world factory | Unchanged |
| `BeginLevelLoad`, `EndLevelLoad` | asset residency | Drives `idImageManager`/`idRenderModelManager` purge policy — keep, it is the streaming contract |
| `RegisterFont` | text | FreeType-based (`tr_font.cpp:283`); replace with a pre-baked atlas |
| `SetColor`, `SetColor4` | 2D | Writes `guiModel->SetColor` |
| `DrawStretchPic` (×2 overloads), `DrawStretchTri` | 2D sprites | Pure geometry into `GuiModel`; **not** GL |
| `DrawSmallChar/StringExt`, `DrawBigChar/StringExt` | text | Composed of `DrawStretchPic` calls |
| `GlobalToNormalizedDeviceCoordinates` | projection helper | Used by tools/trace; delegates to `tr_main.cpp:714` |
| `GetGLSettings`, `PrintMemInfo`, `GetCardCaps` | diagnostics | `GetCardCaps(oldCard, nv10or20)` is a *driver-era hack* — delete in the port |
| `WriteDemoPics`, `DrawDemoPics` | demo | Serialises the GUI model into the demo stream |
| `BeginFrame`, `EndFrame` | frame | Frame ends with `RC_SWAP_BUFFERS` + `vertexCache.EndFrame()` |
| `TakeScreenshot` | capture | Tiled readback (`R_ReadTiledPixels`) for >`MAX_TEXTURE_SIZE` shots; WebGL2 must clamp to canvas/`readPixels` limits |
| `CropRenderSize`, `UnCrop`, `CaptureRenderToImage`, `CaptureRenderToFile` | render-to-texture | Implemented with `renderCrops[]` + `glCopyTexSubImage2D`; **highest-risk API for WebGL2** (§13.3) |
| `UploadImage` | modding hook | Already a data-only texture upload path (`RenderSystem.cpp:1033`) — keep it |

Non-member helpers exported for tools: `R_RenderLightFrustum`, `R_LightProjectionMatrix`,
`R_ScreenshotFilename`.

### 3.2 `idRenderWorld` (`RenderWorld.h:268`)

| Group | Members | Notes |
|---|---|---|
| Lifecycle | `InitFromMap(mapName)` | NULL/empty ⇒ single empty area. Loads `maps/*.proc` (`RenderWorld_load.cpp`) |
| Entity defs | `AddEntityDef`, `UpdateEntityDef`, `FreeEntityDef`, `GetRenderEntity` | `qhandle_t` is `index+1` into `entityDefs` |
| Light defs | `AddLightDef`, `UpdateLightDef`, `FreeLightDef`, `GetRenderLight` | Same |
| Interactions | `GenerateAllInteractions`, `RegenerateWorld`, `CheckAreaForPortalSky` | Pre-baking is a perf option, not a requirement |
| Decals/overlays | `ProjectDecalOntoWorld`, `ProjectDecal`, `ProjectOverlay`, `RemoveDecals` | Produces dynamic `idRenderModelDecal` / `idRenderModelOverlay` |
| Rendering | `SetRenderView`, `RenderScene` | **The only 3D entry point** |
| Portals/areas | `NumPortals`, `FindPortal`, `SetPortalState`, `GetPortalState`, `AreasAreConnected`, `NumAreas`, `PointInArea`, `BoundsInAreas`, `NumPortalsInArea`, `GetPortal` | Game code (doors, sound) depends on these; pure CPU, keeps working |
| Tracing | `GuiTrace`, `ModelTrace`, `Trace`, `FastWorldTrace` | CPU-only; unaffected by API port |
| Demo | `StartWritingDemo`, `StopWritingDemo`, `ProcessDemoCommand` | Renderer state must be demo-serialisable — see `RenderWorld_demo.cpp` and `GuiModel::WriteToDemo` |
| Debug viz | `DebugLine/Arrow/Winding/Circle/Sphere/Bounds/Box/Frustum/Cone/Axis/Polygon`, `DrawText` | Implemented with **immediate mode** (`tr_rendertools.cpp`) — must be rewritten as dynamic buffers |

### 3.3 Public data structs

| Struct | Key fields | Porting relevance |
|---|---|---|
| `renderEntity_t` (`RenderWorld.h:80`) | `hModel`, `origin/axis`, `customShader`, `customSkin`, `shaderParms[12]`, `gui[3]`, `joints` + `numJoints`, `modelDepthHack`, `weaponDepthHack`, `noShadow/ noSelfShadow/ noDynamicInteractions`, `suppress/allowSurfaceInViewID`, `callback` + `callbackData` | The joint matrix array is uploaded per-frame by the caller; **vertex skinning happens on the CPU** (`Model_md5.cpp:253 TransformVerts`) → a modern port should move it to a vertex shader (see §10) |
| `renderLight_t` (`RenderWorld.h:157`) | `pointLight`, `parallel`, `lightRadius`, `lightCenter`, `target/right/up/start/end`, `prelightModel`, `shader`, `shaderParms[12]`, `noShadows/noSpecular` | Fully describes projection; `prelightModel` is a dmap-baked shadow volume |
| `renderView_t` (`RenderWorld.h:206`) | `viewID`, `x/y/width/height` in 640×480 virtual space, `fov_x/fov_y`, `vieworg/viewaxis`, `cramZNear`, `time`, `shaderParms[12]`, `globalMaterial` | Virtual-resolution viewport is converted in `RenderViewToViewport` |
| `exitPortal_t`, `guiPoint_t`, `modelTrace_t`, `portalConnection_t` | — | API-stable |

### 3.4 `glconfig_t` = the capability contract that must be *replaced*, not ported

`glconfig_t` (`RenderSystem.h:44`) is a museum of 2004 GPU features: `registerCombinersAvailable`,
`envDot3Available`, `atiFragmentShaderAvailable`, `atiTwoSidedStencilAvailable`,
`allowNV30Path`/`allowNV20Path`/`allowNV10Path`/`allowR200Path`/`allowARB2Path`,
`sharedTexturePaletteAvailable`, `textureEnvAdd/CombineAvailable`.

For a port, split it in two:

* **`RendererCaps`** (new, small): `maxTextureSize`, `maxTextureUnits`, `maxSamples`,
  `hasDepthTexture`, `hasFloatRenderTarget`, `hasAnisotropy`, `anisotropyMax`, `hasDepthBounds`.
* **Everything else: delete.** A WebGL2/WebGPU backend has exactly one shading path.

---

## 4. Portal / area visibility

### 4.1 Data structures (`RenderWorld_local.h:36-75`)

| Type | Contents |
|---|---|
| `portal_t` | `intoArea`, `w` (winding, CCW seen from this area), `plane` (must be crossed from positive side), `next`, `doublePortal` |
| `doublePortal_t` | The two one-sided portals, `blockingBits` (doors), `fogLight` (fog can seal a portal) |
| `portalArea_t` | `connectedAreaNum[3]`, `viewCount`, `portals`, `entityRefs`, `lightRefs` |
| `areaNode_t` | BSP of areas: `plane`, `children[2]` (negative ⇒ `-1-area`, 0 ⇒ solid), `commonChildrenArea` |

Areas and portals come from `maps/<name>.proc` (`mapProcFile003`, `RenderWorld.h:40-41`);
`RenderWorld_load.cpp:277` reads `numPortalAreas` and `numInterAreaPortals`. **The format is
already a flat text/binary CPU structure — it needs no conversion for a web port.**

### 4.2 Flow

| Step | Function | Behaviour |
|---|---|---|
| Locate start area | `PointInArea` (BSP walk) | `-1` ⇒ outside world ⇒ mark *all* areas visible (`RenderWorld_portals.cpp:299-308`) |
| Flood view | `FlowViewThroughPortals` (`:285`) → `FloodViewThroughArea_r` | Recursive; clips each portal winding by the accumulated frustum planes; `areaScreenRect[]` accumulates the screen-space scissor for each area |
| Flood light | `FlowLightThroughPortals` (`:444`) → `FloodLightThroughArea_r` | Same but from `light->globalLightOrigin`; also clips by the light's own 4 planes |
| Connectedness | `BuildConnectedAreas` / `FloodConnectedAreas` (`:810`,`:935`) | For each of 3 attributes (`PS_BLOCK_VIEW/LOCATION/AIR`), flood ignoring portals whose `blockingBits` has that bit set → powers "light behind a closed door" culling |
| Cull | `CullEntityByPortals` (`:564`), `CullLightByPortals` (`:645`) | Entity/light must intersect the portal stack volume |
| Fog | `PortalIsFoggedOut` | Fog volume endpoints seal portals for visibility/sound |
| Area refs | `AddAreaEntityRefs`, `AddAreaLightRefs`, `AddAreaRefs` (`:591,723,769`) | Maintains the per-area linked lists used by `FindViewLightsAndEntities` |

`PS_BLOCK_*` also gates gameplay (`AreasAreConnected`, `RenderWorld.h:345`) — **do not remove the
portal system when porting.** It is CPU logic and survives any GPU API change; only the
`r_usePortals 0` debug path is GPU-relevant.

---

## 5. Light / surface interaction model

DOOM 3 is **forward, additive, per-light** shading over a depth prepass.

| Stage | Function | Output |
|---|---|---|
| Light derivation | `R_DeriveLightData` (`tr_lightrun.cpp:351`) | `lightProject[4]`, `frustum[6]`, `falloffImage`, `lightShader`, `globalLightOrigin` |
| Light → viewLight | `R_SetLightDefViewLight` (`tr_light.cpp:458`) | Copies everything the backend needs into a frame-allocated `viewLight_t` |
| Interaction existence | `CreateLightDefInteractions` (`tr_light.cpp:541`) | For every entity ref in the light's areas, `idInteraction::AllocAndLink` |
| Interaction build | `idInteraction::CreateInteraction` (`Interaction.cpp:823`) | Culls model & surface boxes vs 6 light planes; builds light tris (`R_CreateLightTris`) and **shadow tris** (`R_CreateShadowVolume`, `Interaction.cpp:914`) |
| Activation | `idInteraction::AddActiveInteraction` (`:1024`) | Rechecks view-time conditions, inserts into one of the 5 `viewLight` chains: `globalInteractions`, `localInteractions`, `globalShadows`, `localShadows`, `translucentInteractions` |
| Scissor | `R_CalcLightScissorRectangle` (`tr_light.cpp:793`), `R_ClippedLightScissorRectangle` (`:713`), `idInteraction::CalcInteractionScissorRectangle` (`Interaction.cpp:686`) | Per-light and per-interaction screen rectangles; carried in `drawSurf_t::scissorRect` |
| Ambient | `R_AddAmbientDrawsurfs` (`tr_light.cpp:1335`) | Non-lit stages into the main drawSurf list |
| Backend | `RB_CreateSingleDrawInteractions` (`tr_render.cpp:689`) | Decomposes a material × light pair into 1..N `drawInteraction_t` passes (§7.2) |

`idRenderEntityLocal` / `idRenderLightLocal` (`tr_local.h:181,237`) are the persistent, mutable
side; `viewEntity_t` / `viewLight_t` (`tr_local.h:344,298`) are the immutable per-frame snapshots.
That duplication exists *only* for SMP and is otherwise pure overhead — but it is also a clean
data boundary, so keep it.

**Sorting is intentionally coarse:** `drawSurf_t::sort` is the material `sort` value, and the
whole list is `qsort`ed once (`tr_main.cpp:1049`). There is no per-object depth sort; translucency
relies on material authoring (`sort far/medium/close`, `sort postProcess`, …).

---

## 6. Shadows: stencil volumes (two generators)

There are **no shadow maps**. Shadows are per-light stencil volumes, computed on the CPU each
time an interaction is created.

### 6.1 Front-end generation

| Path | Function | Geometry |
|---|---|---|
| **Turbo + vertex program** (default when `r_useTurboShadow` and VP available) | `R_CreateVertexProgramTurboShadowVolume` (`tr_turboshadow.cpp:45`) | Duplicates every vertex (`numVerts*2`), 6 indexes per silhouette edge, 6 per shadowing face; **rear vertices projected to infinity in the vertex program** (`shadow.vp`) using `PP_LIGHT_ORIGIN` as a direction (`w=0`). `shadowCapPlaneBits = SHADOW_CAP_INFINITE` |
| **Turbo** (no VP) | `R_CreateTurboShadowVolume` (`tr_turboshadow.cpp:187`) | Same topology, but rear verts projected on the CPU into `shadowVertexes` and uploaded as `shadowCache_t` (4×float) |
| **Clipped / static / offline** | `R_CreateShadowVolume` (`tr_stencilshadow.cpp:1217`) → `R_CreateShadowVolumeInFrustum` (`:811`) | Properly clipped to each of the light's shadow frustums; generates separate front caps, rear caps and sil planes, sorted so the backend can drop caps independently |
| Frustum setup | `R_MakeShadowFrustums` (`tr_stencilshadow.cpp:1032`) | Up to 6 frustums per projected light (`shadowFrustum_t`, `tr_local.h:125`) |
| Silhouette data | `R_IdentifySilEdges` (`tr_trisurf.cpp:1003`), `R_AddSilEdges` (`tr_stencilshadow.cpp:642`), `R_CalcPointCull` (`:751`) | Precomputed at model load / on demand |
| DMAP prebake | `renderLight_t::prelightModel` | `_prelight_<light>` shadow volume baked into the map and reused while the light is unmoved |

Index-count bookkeeping is the core trick and must be preserved exactly
(`Model.h:119-125`): `numIndexes` (all), `numShadowIndexesNoFrontCaps`,
`numShadowIndexesNoCaps`, plus `shadowCapPlaneBits`. These drive `RB_T_Shadow`'s cap decisions.

### 6.2 Backend pass

| Step | Function | GL state |
|---|---|---|
| Prepass writes stencil=128 on lit surfaces | `RB_STD_FillDepthBuffer` (`draw_common.cpp:521`) + `RB_T_FillDepthBuffer` (`:350`) | `GL_STENCIL_TEST` on, `glStencilFunc(GL_ALWAYS,1,255)`; **alpha-tested (perforated) surfaces use `glAlphaFunc` so the stencil is not written through holes** |
| Shadow pass | `RB_StencilShadowPass` (`:1176`) → `RB_T_Shadow` (`:1053`) | `GL_ALWAYS,1,255`; color+alpha mask, depth mask, `GL_LESS`; optional `glPolygonOffset`; optional `GL_DEPTH_BOUNDS_TEST_EXT` |
| "Patent-free" near-plane fixup | `RB_T_Shadow:1147-1156` | If the view is inside the volume, first render front-facing verts with `decr` on depth-fail *and* depth-pass, then back-facing with `incr` — so clipped volumes still balance |
| Counting pass | `RB_T_Shadow:1158-1165` | `CT_FRONT_SIDED` + `GL_INCR`, then `CT_BACK_SIDED` + `GL_DECR` (wrapping variants where available) |
| Consumption | light interaction pass | `glStencilFunc(GL_GEQUAL, 128, 255)` → only pixels in shadow are lit (the light is *subtracted* by not drawing it) |
| Two-sided stencil | `glConfig.twoSidedStencilAvailable`, `qglActiveStencilFaceEXT` (`RenderSystem_init.cpp:451`) | Optional single-pass variant (`r_useTwoSidedStencil`) |
| Depth bounds | `qglDepthBoundsEXT(zmin,zmax)` per `drawSurf_t::scissorRect` (`draw_common.cpp:1105`) | Fill-rate optimisation; **not available in WebGL2** |

Invariants a port must keep:

* Stencil clear value is `1<<(stencilBits-1)` (`tr_render.cpp:582`), and lights test `GEQUAL` that
  value — not a hard-coded 128.
* Wrapping increment/decrement (`GL_INCR_WRAP_EXT`) is required for correctness at silhouette
  overlaps; `GL_INCR`/`GL_DECR` is the fallback (`RenderSystem_init.cpp:419-423`).
* Color writes and depth writes are off during the volume pass.
* Face culling alternates front/back; `GL_Cull` also flips the winding for mirrors
  (`backEnd.viewDef->isMirror`).

### 6.3 `draw_exp.cpp` is a shadow-map prototype

`draw_exp.cpp` (2,618 lines, **not in `doomdll.vcxproj`) implements a full float-pbuffer
shadow-buffer/deferred path: `floatPbuffer`, `shadowPbuffer`, per-light 1024² buffers,
`glReadPixels`/`glDrawPixels` readback, and 297 `qgl*` calls. Treat it as a *design sketch* for a
shadow-map backend (relevant if stencil fill rate becomes a problem on mobile), not as working code.

---

## 7. Interaction shading: five backends, one data contract

`tr.backEndRenderer` is chosen at `RenderSystem.cpp:536` from `r_renderer`
(`arb|arb2|nv10|nv20|r200`), with a fallback order `ARB2 → R200 → NV20 → NV10 → ARB`.

| Enum | File | Vertex stage | Fragment stage | `backEndRendererHasVertexPrograms` | Disposition |
|---|---|---|---|---|---|
| `BE_ARB2` | `draw_arb2.cpp` | `!!ARBvp1.0` from `glprogs/*.vfp` | `!!ARBfp1.0` from `glprogs/*.vfp` | yes, `MaxLight=999` | **The reference path. Port this one.** |
| `BE_ARB` | `draw_arb.cpp` | fixed function | `glTexEnv` combine, 2–3 units, `r_useTripleTextureARB` | no | Reference for the *math*, not the API |
| `BE_NV20` | `draw_nv20.cpp` | ARB VP | `NV_register_combiners` + `NV_texture_shader` (dot3, dependent read) | yes | Delete |
| `BE_R200` | `draw_r200.cpp` | ARB VP | `ATI_fragment_shader` built at runtime (`R_BuildSurfaceFragmentProgram`) | yes | Delete |
| `BE_NV10` | `draw_nv10.cpp` | fixed function | `NV_register_combiners` | no | Delete |

### 7.1 Draw-call waterfall per interaction (`RB_ARB2_DrawInteraction`, `draw_arb2.cpp:67`)

| TMU | Contents | Set by |
|---|---|---|
| 0 | `normalCubeMapImage` (vector→light normalisation), or `ambientNormalMap` when `IsAmbientLight()` | `RB_ARB2_CreateDrawInteractions:172` |
| 1 | per-surface bump map | `:114` |
| 2 | light falloff (1D, 64² table) | `:118` |
| 3 | light projection / cube | `:122` |
| 4 | per-surface diffuse | `:126` |
| 5 | per-surface specular | `:130` |
| 6 | `specularTableImage` / `specular2DTableImage` (1D/2D specular lookup) | `:180` |

**7 texture units, 6 of them sampled inside the fragment program.** ES3 guarantees ≥16 fragment
texture units, so this fits without redesign.

### 7.2 The uniform contract (the actual thing to reimplement)

`programParameter_t` (`tr_local.h:1368`) is the ABI between C++ and the missing shaders. A port
must reproduce these slots as GLSL uniforms / a UBO.

| Enum | Slot | Content |
|---|---|---|
| `PP_LIGHT_ORIGIN` = 4 | VP env 4 | light origin in **model space**, `w=0` for shadow projection |
| `PP_VIEW_ORIGIN` = 5 | VP env 5 | view origin in model space |
| `PP_LIGHT_PROJECT_S/T/Q/FALLOFF_S` = 6..9 | VP env | 4 projected-light planes (model space, texture matrix baked in) |
| `PP_BUMP_MATRIX_S/T` = 10,11 | VP env | 2×3 texture matrix for bump |
| `PP_DIFFUSE_MATRIX_S/T` = 12,13 | VP env | ditto diffuse |
| `PP_SPECULAR_MATRIX_S/T` = 14,15 | VP env | ditto specular |
| `PP_COLOR_MODULATE` = 16, `PP_COLOR_ADD` = 17 | VP env | vertex-colour policy: ignore `(0,1)`, modulate `(1,0)`, inverse `(-1,1)` |
| `FP 0`, `FP 1` | FP env | diffuse colour, specular colour (`< backEndRendererMaxLight`) |
| `FP 2`, `FP 3` | FP env | light/view origin (only for `r_testARBProgram`) |
| `VP 0` | VP env | screen-copy scale `(w/potW, h/potH, 0, 1)` — `RB_SetProgramEnvironment` (`draw_common.cpp:579`) |
| `PP_LIGHT_FALLOFF_TQ` = 20 | VP env | NV-only falloff t/q |

Vertex attributes (hard-coded indices, `draw_arb2.cpp:165-198`):
`0 = xyz`, `8 = st`, `9 = tangent[0]`, `10 = tangent[1]`, `11 = normal`,
plus `glColorPointer` (4×ubyte). Stride is `sizeof(idDrawVert)` = **60 bytes**
(`xyz 12 + st 8 + normal 12 + tangents 24 + color 4`, `DrawVert.h:40`).

A port must also renumber these hard-coded attribute slots. ES3 guarantees 16 attribute
locations, so indices 8..11 are legal but wasteful — use explicit `layout(location=…)` 0..4 or a
VAO bindings table.

### 7.3 `glprogs` dependency (data, not source)

`draw_arb2.cpp:339-356` is the complete list of files the renderer requires at runtime:

| Target | Enum | File (`glprogs/…`) | Used by |
|---|---|---|---|
| VP | `VPROG_INTERACTION` | `interaction.vfp` | `BE_ARB2` light/surface interaction |
| FP | `FPROG_INTERACTION` | `interaction.vfp` (second `!!ARBfp` block in the same file) | ditto |
| VP | `VPROG_BUMPY_ENVIRONMENT` | `bumpyEnvironment.vfp` | env-mapped bump surfaces |
| FP | `FPROG_BUMPY_ENVIRONMENT` | `bumpyEnvironment.vfp` | ditto |
| VP | `VPROG_AMBIENT` | `ambientLight.vfp` | ambient pass |
| FP | `FPROG_AMBIENT` | `ambientLight.vfp` | ditto |
| VP | `VPROG_STENCIL_SHADOW` | `shadow.vp` | turbo shadow projection to infinity |
| VP | `VPROG_R200_INTERACTION` | `R200_interaction.vp` | R200 path |
| VP | `VPROG_NV20_*` (4 files) | `nv20_bumpAndLight.vp`, `nv20_diffuseColor.vp`, `nv20_specularColor.vp`, `nv20_diffuseAndSpecularColor.vp` | NV20 path |
| VP/FP | `VPROG/FPROG_ENVIRONMENT` | `environment.vfp` | environment/cube stages |
| VP/FP | `VPROG/FPROG_GLASSWARP` | `arbVP_glasswarp.txt`, `arbFP_glasswarp.txt` | heat-haze/glass warp |
| VP | (implicit) | `megaTexture.vfp` | `idMegaTexture` (`Material.cpp:1517`) |
| any | `R_FindARBProgram` (`:466`) | anything named by a `.mtr` `vertexProgram`/`fragmentProgram` | data-authored stages |

Loading is by **string search for `!!ARBvp` / `!!ARBfp` … `END`** (`:404-428`), so a single file may
contain both programs. `R_ReloadARBPrograms_f` (`:505`) is the live-reload command — a port should
keep an equivalent `reloadShaders` console command, since it is the main shader iteration loop.

**Consequence:** ~12 shader programs must be rewritten by hand as GLSL ES 3.00 (or SPIR-V), and
their ARB env-parameter slots re-bound to uniforms. This is the single largest concrete piece of
migration work, and it cannot be automated from this repo because the sources are absent.

### 7.4 New-style `.mtr` stages

`newShaderStage_t` (`Material.h:191`) lets a material declare its own program pair and bindings —
this is the **data-only shader surface**:

```
{
    vertexProgram  heatHaze.vfp
    fragmentProgram heatHaze.vfp
    vertexParm 0  time, 0.5
    vertexParm 1  parm3, parm4
    fragmentMap 0  _currentRender
    fragmentMap 1  textures/glass/normal
}
```

Parsed in `ParseStage` (`Material.cpp:1492-1544`), `ParseVertexParm` (`:874`),
`ParseFragmentMap` (`:924`). Restricted to `BE_ARB2` at draw time (`draw_common.cpp:789`).
A GLSL port must keep this grammar and map `vertexParm N` → `uniform vec4 u_vertexParm[N]`,
`fragmentMap N` → `uniform sampler2D u_fragMap[N]`.

---

## 8. Material system (`.mtr`)

### 8.1 Parse flow

| Step | Function | Notes |
|---|---|---|
| Load | `idMaterial::Parse(text,len)` (`Material.cpp:2122`) | `idDecl`-backed; hot-reload via decl system |
| Header | `ParseMaterial` (`:1783`) | `qer_editorimage`, `diffusemap/specularmap/bumpmap`, `decalsize`, `twosided`, `sort`, `noShadows`, `noSelfShadow`, `translucent`, `deform`, `polygonOffset`, `spectrum`, conditions |
| Expressions | `ParseExpression`/`ParseTerm` (`:729`,`:508`) → `expOp_t[]` | **Register VM**: `time`, `parm0..11`, `global0..7`, `sound`, `fragmentPrograms`, math ops, tables (`sinTable`, `cosTable`) |
| Stages | `ParseStage` (`:1079`) | Ordered blend passes |
| Defaults | `AddImplicitStages` (`:1679`) | Synthesises stages from `diffusemap`/`specularmap`/`bumpmap` header keys |
| Ordering | `SortInteractionStages` (`:1742`) | Ambient stage first, then additive/specular stages in the order the interaction shader expects (**must be preserved for ARB2 parity**) |
| Bake | `CheckForConstantRegisters` (`:2605`) | `r_useConstantMaterials` → precompute all registers when they are time-invariant |
| Evaluate | `EvaluateRegisters(out, text, view, soundEmitter)` | Called per drawsurf, into frame memory |

`shaderStage_t` (`Material.h:203`): `conditionRegister`, `drawStateBits` (the `GLS_*` word),
`hasAlphaTest`/`alphaTestRegister`, `ignoreAlphaTest`, `privatePolygonOffset`, `newStage`,
`lighting` (`SL_AMBIENT`/`SL_DIFFUSE`/`SL_SPECULAR`), `texture`, `color`, `vertexColor`,
`texgen`, `deform`.

`textureStage_t` (`Material.h:159`): `image`, `hasMatrix` + `matrix[2][3]` (register indices),
`texgen` (`TG_REFLECT_CUBE`, `TG_SKYBOX_CUBE`, `TG_WOBBLESKY_CUBE`, `TG_DIFFUSE_CUBE`),
`filter` (`TF_NEAREST/LINEAR/DEFAULT`), `repeat` (`TR_REPEAT/CLAMP/ZEROCLAMP/ALPHAGREP`),
`depth` (`TD_*` → controls DXT & mip policy), `allowHighQualityCompression`, `forceHighQuality`,
`noMipmaps`, `vertexColor`.

### 8.2 Keyword → engine effect (the AI-modding surface)

| `.mtr` construct | Implemented at | Effect / caveat |
|---|---|---|
| `blend add/filter/blend/<src>,<dst>` | `ParseBlend` (`:815`), `NameToSrc/DstBlendMode` (`:753`,`:785`) | Maps to `GLS_SRCBLEND_*` / `GLS_DSTBLEND_*` → `glBlendFunc` |
| `map <texture> [nearest|linear] [clamp|noclamp|zeroclamp|alphazeroclamp] [uncompressed|highquality] [nopicmip] [forceHighQuality]` | `ParseStage:1135+` | Texture + sampler state; `uncompressed` sets `TD_HIGH_QUALITY` |
| `cubeMap <name> [Camera...]` | `:1211` | 6-face cube; `CF_NATIVE` vs `CF_CAMERA` orientation (`Image.h:138`) |
| `videomap` / `soundmap` | `:1181`,`:1201` | ROQ video stage / audio-driven register |
| `texGen skybox|wobbleSky|reflect|normal` | `:1289` | **Fixed-function texgen** → must become shader math (skybox/wobble already have C++ fallbacks `R_SkyboxTexGen`, `R_WobbleskyTexGen` in `tr_light.cpp:193,218`) |
| `scroll`, `translate`, `scale`, `centerScale`, `shear`, `rotate` | `:1305-1390` | Compose a 2×3 texture matrix into registers → **portable**, becomes a uniform matrix |
| `vertexColor`, `inverseVertexColor` | `:1267`,`:1271` | Selects `SVC_*` policy (see `PP_COLOR_MODULATE/ADD`) |
| `maskRed/Green/Blue/Alpha` | `:1390+` | `GLS_*MASK` bits → `glColorMask` |
| `alphaTest <expr>` | `:1225` region | Enables `GL_ALPHA_TEST` with `GL_EQUAL 1`/`LESS 0.5`/`GEQUAL 0.5` (`GL_State:379-400`) |
| `if <expr>` (stage condition) | `conditionRegister` | Cheap CPU skip; the GPU never sees a disabled stage |
| `deform <type> <exprs>` | `ParseDeform` (`:1584`) | CPU vertex deformation (§8.4) |
| `sort <subview|opaque|decal|far|medium|close|almostNearest|nearest|postProcess|portalSky>` | `ParseSort` (`:333`) | Drives draw order and the depth-prepass/post-process split |
| `polygonOffset`, `privatePolygonOffset` | `:1277` | Decals; `glPolygonOffset` |
| `noShadows`, `noSelfShadow`, `translucent`, `twoSided`, `noFragment`, `spectrum`, `portalSky`, `suppressInSubview` | various | Surface/material flags |
| `vertexProgram` / `fragmentProgram` / `vertexParm` / `fragmentMap` | §7.4 | Data-defined shaders |
| `megaTexture <file>` | `Material.cpp:1509` | Experimental; needs `.mega` data + `megaTexture.vfp` |

### 8.3 Deforms (`DFRM_*`, `Material.h` enum + `tr_deform.cpp`)

| Deform | Function | Cost |
|---|---|---|
| `sprite` | `R_AutospriteDeform` (`tr_deform.cpp:75`) | rebuilds quads on CPU |
| `tube` | `R_TubeDeform` (`:163`) | per-frame |
| `flare` | `R_FlareDeform` (`:371`, `:509`) | requires view + light, two variants |
| `expand` | `R_ExpandDeform` (`:701`) | along normals |
| `move` | (in `R_DeformDrawSurf`) | shader-param driven |
| `eyeball` | `R_EyeballDeform` (`:870`) | two-island sphere mapping |
| `particle`, `particle2` | `R_ParticleDeform` (`:1018`) | reads `DeclParticle` |
| `turb` | (in `R_DeformDrawSurf`) | noise |

`R_DeformDrawSurf` (`:1228`) is dispatched from `RB_STD_T_RenderShaderPasses` when
`surf->material->Deform() != DFRM_NONE`. **All of these are CPU-side and API-neutral**, but each
forces a per-frame vertex upload; a WebGL2 port should keep them but batch the temp uploads.

### 8.4 Expression register VM (portable, and a good AI-modding target)

`expOp_t` + `EvaluateRegisters` is a tiny stack/SSA interpreter over ~40 opcodes (add, sub, mul,
div, mod, table, sin/cos via table decl, time, parmN, globalN, sound amplitude, comparisons,
logical ops). It is pure C++ and **needs no changes**. Because every animated material effect
(scrolls, pulses, flicker, monster burn-away, particle spawn timing) is expressed here, this VM is
the safest place to add new data-driven behaviour without touching GL.

---

## 9. Images (`Image_*.cpp`)

| Concept | Where | Notes |
|---|---|---|
| `idImage` | `Image.h:146` | `texnum`, `type` (`TT_2D/3D/CUBIC/RECT`), `filter`, `repeat`, `depth`, `uploadWidth/Height`, `refCount`, LRU links |
| State machine | `IS_UNLOADED → IS_PARTIAL → IS_LOADED` (`Image.h:58`) | `partialImage` holds a downsampled version while the full one loads in the background |
| Bind | `idImage::Bind` (`Image_load.cpp:1697`) | LRU touch, lazy load, **per-TMU enable/disable of `GL_TEXTURE_2D/3D/CUBE_MAP`** (`:1742-1758`), bind-cache per TMU |
| Load | `ActuallyLoadImage` (`:1576`) | Chooses precompressed DDS vs source (TGA/JPG/BMP/PCX) |
| Precompressed | `CheckPrecompressedImage` (`:1336`), `UploadPrecompressedImage` (`:1436`), `WritePrecompressedImage` (`:1041`) | DDS/DXTn with `DDSF_ID_INDEXCOLOR` / `DDSF_ID_MONOCHROME` extensions |
| Compression choose | `idImage::SelectInternalFormat` (`Image.h:209`) | `TD_*` → DXT1/3/5, luminance, or RGBA8 |
| DXT normal maps | `UploadCompressedNormalMap` (`:113`) | DXT5 + swizzled → RG (ATI2-style) |
| Mip generation | `R_MipMap`, `R_MipMapWithAlphaSpecularity`, `R_MipMap3D` (`Image.h:462-464`) | CPU; **WebGL2 `generateMipmap` cannot reproduce the specular-preserving variant** — keep CPU mips for `bumpmap` |
| Procedural images | `Image_init.cpp` | `R_RampImage`, `R_SpecularTableImage`, `R_Specular2DTableImage`, `R_AlphaRampImage`, `R_FlatNormalImage`, `R_AmbientNormalImage`, `makeNormalizeVectorCubeMap`, `R_FogImage`, `R_FogEnterImage`, `R_QuadraticImage`, `CreateSquareLight`, `CreateFlashOff`, `R_BorderClampImage`, `R_AlphaNotchImage` |
| Image *programs* | `Image_program.cpp` | `.mtr`-adjacent expression language: `heightmap`, `addnormals`, `smoothnormals`, `add`, `scale`, `invertAlpha`, `invertColor`, `makeIntensity`, `makeAlpha`. Pure CPU — **fully portable and a great data-only modding surface** |
| Async load | `StartBackgroundImageLoad` (`Image_init.cpp:1775`), `CompleteBackgroundImageLoads` (`:1846`, called at the top of `RB_ExecuteBackEndCommands`) | Web port: replace with `createImageBitmap`/fetch + upload at frame boundary |
| Render-to-texture | `CopyFramebuffer` (`Image_load.cpp:1853`) → `glCopyTexImage2D`/`glCopyTexSubImage2D`; `CopyDepthbuffer` (`:1927`) | Used for `_currentRender` (post-process), mirrors, GUIs, cinematics |
| Managers | `AllocImage`, `ImageFromFunction` (`Image_init.cpp:1404,1434`), `PurgeAllImages`, `ReloadAllImages`, `BeginLevelLoad`/`EndLevelLoad` | Level-scoped purge + `r_forceLoadImages` debug path |

Special images the shader contract depends on: `defaultImage`, `whiteImage`, `blackImage`,
`flatNormalMap`, `ambientNormalMap`, `normalCubeMapImage`, `specularTableImage`,
`specular2DTableImage`, `rampImage`, `alphaRampImage`, `alphaNotchImage`,
`borderClampImage`, `fogImage`, `fogEnterImage`, `currentRenderImage`, `cinematicImage`.

For a web port, `_currentRender` + `glCopyTexSubImage2D` is the most invasive piece: replace with
an FBO (`WEBGL2` framebuffer + color attachment) and `blitFramebuffer`/`copyTexSubImage2D` against
`READ_FRAMEBUFFER`. Note the existing code already handles the "oversized / POT" padding because
`glCopyTexSubImage2D` needs a POT-sized target on old drivers — a modern port can drop that whole
class of workarounds (`CopyFramebuffer`'s 4 extra edge blits, `uploadWidth/uploadHeight`).

---

## 10. Models

### 10.1 Formats

`idRenderModelManagerLocal::GetModel` (`ModelManager.cpp:253`) dispatches by extension
(`:290-310`):

| Ext | Class | File | Notes |
|---|---|---|---|
| `.md5mesh` (+`.md5anim`) | `idRenderModelMD5` | `Model_md5.cpp` | Skeletal, CPU-skinned (`TransformVerts:253`, `UpdateSurface:275`), `InstantiateDynamicModel:722`, `DM_CACHED` |
| `.md3` | `idRenderModelMD3` | `Model_md3.cpp` | Quake 3 legacy, per-vertex lerp via `SHADERPARM_MD3_FRAME/LASTFRAME/BACKLERP` |
| `.ase` | `idRenderModelStatic` | `Model_ase.cpp` | 3ds Max ASCII |
| `.lwo` | `idRenderModelStatic` | `Model_lwo.cpp` (105 KB) | Lightwave |
| `.ma` / `.mb` | `idRenderModelStatic` | `Model_ma.cpp` | Maya ASCII |
| `.prt` | `idRenderModelPrt` | `Model_prt.cpp` | Particle system (`DeclParticle`) |
| `.liquid` | `idRenderModelLiquid` | `Model_liquid.cpp` | Animated water surface |
| `_beam`, `_sprite` | `idRenderModelBeam` / `idRenderModelSprite` | `Model_beam.cpp`, `Model_sprite.cpp` | Procedural from shader parms |
| (none) | `idRenderModelStatic` | `Model.cpp` | BSP world geometry (from `.proc`) |

All formats funnel into one geometry type: `srfTriangles_t` (`Model.h:86`) with
`idDrawVert verts` (60 B), `glIndex_t indexes`, `silEdges`, `facePlanes`, `dominantTris`,
`ambientCache/indexCache/shadowCache/lightingCache` handles.

Common pipeline after parse: `FinishSurfaces` → `R_CleanupTriangles` (`tr_trisurf.cpp:2096`) →
`R_IdentifySilEdges` (`:1003`, `omitCoplanarEdges` per material) → `R_DeriveTangents` (`:1647`) or
`R_DeriveUnsmoothedTangents` (`:1578`) → `R_BuildDeformInfo` (`:2147`).

### 10.2 `idRenderModel` (`Model.h:171`)

`InitFromFile`, `PartialInitFromFile` (renderbump), `InitEmpty`, `AddSurface`, `FinishSurfaces`,
`PurgeModel`, `LoadModel`, `Reset`, `IsLoaded`, `TouchData`, `Bounds`, `NumSurfaces`, `Surface`,
`InstantiateDynamicModel`, `IsDynamicModel` (`DM_STATIC`/`DM_CACHED`/`DM_CONTINUOUS`),
`Memory`, `WriteToDemoFile`.

`DM_CACHED` (md5) and `DM_CONTINUOUS` (particles, liquids) are where CPU cost lands: every frame
that an animated model is visible, `TransformVerts` + `UpdateSurface` rewrite `idDrawVert[]` and
`R_CreateLightingCache`/`R_CreateAmbientCache` re-upload. **The highest-value optimisation for a
web port** is moving md5 skinning to a vertex shader (joint palette UBO + `u_jointIndex`/
`u_jointWeight` attributes); DOOM 3 never does this because 2004 vertex shaders had no uniform
arrays of matrices large enough.

### 10.3 Decals and overlays

`idRenderModelDecal` (`ModelDecal.cpp`) projects a winding onto static geometry with depth fade
(`CreateProjectionInfo:88`, `AddWinding:209`, `AddDepthFadedWinding:275`, `AddDecalDrawSurf:464`).
`idRenderModelOverlay` (`ModelOverlay.cpp:104`) projects 2D texture axes onto a *dynamic* model's
surface and re-clips per frame. Both are CPU geometry generators — API-neutral.

---

## 11. GUI, 2D, subviews, video, fonts

| Subsystem | Entry | Notes |
|---|---|---|
| 2D batching | `idGuiModel` (`GuiModel.cpp`) | `MAX_GUI_SURFACES 1024` (`tr_local.h:669`); all `idRenderSystem::Draw*` calls append quads (or arbitrary triangle lists via `DrawStretchPic(verts,indexes,...)`), then `EmitFullScreen` (`:226`) builds an orthographic `viewDef_t` with a hand-written 640×480 projection matrix (`:268-274`) and emits one `RC_DRAW_VIEW` |
| GUI on a surface | `R_RenderGuiSurf` (`tr_guisurf.cpp:128`), `R_SurfaceToTextureAxis` (`:49`) | Renders the interactive GUI into a texture each frame, then binds it as a material stage |
| Mirrors / cameras | `R_MirrorRender` (`tr_subview.cpp:351`), `R_RemoteRender` (`:292`), `R_XrayRender` (`:401`), `R_GenerateSurfaceSubview` (`:451`) | Recursive `R_RenderView` after `idImage::CopyFramebuffer` or direct FBO-less viewport juggling; `MAX_CLIP_PLANES=1` mirror clip is done via a **texgen notch on `alphaNotchImage`**, not `glClipPlane` (`draw_common.cpp:530-536`) |
| ROQ video | `Cinematic.cpp` | Decodes into `cinematicImage` (`idImage`) via `UploadScratch`; `UploadScratch` uses `glTexSubImage2D` |
| Fonts | `tr_font.cpp` | FreeType builds a glyph atlas at `RegisterFont`; `glyphInfo_t.s/t/s2/t2` are atlas UVs (`RenderSystem.h:109`) |

The "notch" trick and the subview recursion are the two places where a modern backend needs real
design work (FBO stack / render-pass nesting) rather than a 1:1 call translation.

---

## 12. The `qgl` wrapper and GL call-site inventory

`qgl.h` declares ~394 function pointers; `qgl_linked.h` provides a compile-time mode where `qglX`
is `#define`d to the real `glX`. `win_qgl.cpp` (`QGL_Init`, line 139) loads `opengl32.dll` and
binds everything with `wglGetProcAddress`, *duplicating* the extension surface declared in
`glext.h`/`wglext.h`. `RenderSystem_init.cpp:232-560` then re-resolves a second, renderer-private
set (`qglDepthBoundsEXT`, `qglActiveStencilFaceEXT`, `qglColorTableEXT`, `qglTexImage3D`,
`qglVertexArrayRangeNV`, `qglCombiner*NV`, `qgl*ATI`, …).

**Coupling census (measured, not estimated):**

| File | `qgl*` refs | Nature | Porting disposition |
|---|---|---|---|
| `qgl.h` / `qgl_linked.h` | 394 / 342 | declarations / aliases | Replace both with one RHI header |
| `tr_rendertools.cpp` | 330 | **debug viz**: `glBegin/glVertex3f/glEnd`, `glReadPixels`, `glDrawPixels`, `glPushAttrib`, `glPolygonMode` | Rewrite as dynamic line/triangle buffers; drop `DrawPixels`/`PushAttrib` |
| `draw_exp.cpp` | 297 | dead shadow-buffer path | Delete |
| `draw_common.cpp` | 254 | core: state, texenv, shadow ops, fog | This is the file the RHI must serve |
| `draw_nv20.cpp` | 160 | register combiners | Delete |
| `RenderSystem_init.cpp` | 128 | extension probing, screenshots, `glReadPixels` | Rewrite against a caps struct |
| `Image_load.cpp` | 104 | `glTexImage*`, `glCopyTex*`, `glTexParameter*` | Map to `texImage*`/`copyTexSubImage2D`/FBO blit |
| `draw_nv10.cpp` | 100 | register combiners | Delete |
| `draw_arb.cpp` | 98 | `glTexEnv*`, multitexture | Delete after ARB2 rewrite |
| `draw_r200.cpp` | 88 | `gl*FragmentShaderATI` | Delete |
| `tr_backend.cpp` | 82 | `GL_State`, `GL_Cull`, `GL_SelectTexture`, clear/swap | Becomes the RHI state cache |
| `tr_render.cpp` | 69 | matrix stack, viewport/scissor, stencil, draw elements | Becomes the RHI command emitter |
| `draw_arb2.cpp` | 65 | `glProgramStringARB`, env params, attrib pointers | Becomes GLSL/shader-object code |
| `VertexCache.cpp` | 15 | `glGenBuffersARB`, `glBufferDataARB`, `glMapBufferARB`, `glVertexArrayRangeNV` | Becomes buffer/VAO allocator |
| `tr_trace.cpp`, `Image_init.cpp`, `MegaTexture.cpp`, `RenderWorld_portals.cpp`, `RenderSystem.cpp`, `GuiModel.cpp` | 13/11/5/5/2/1 | debug, palette, upload, portal debug lines | Mostly delete or trivial |

**Total: 400 distinct `qgl*` symbols, 2,573 references, 1,837 in sources, 1,540 in compiled
sources.** Non-portable constructs that appear in *compiled* code:

| Construct | Where | Count/notes |
|---|---|---|
| Immediate mode (`glBegin/glEnd`/`glVertex*`/`glColor*`) | `tr_rendertools.cpp` | 103 refs — debug only |
| Fixed-function matrix stack (`glMatrixMode`, `glLoadMatrixf`, `glMultMatrixf`) | `tr_render.cpp:557`, `draw_common.cpp:713` | 3 sites; matrices are already computed on the CPU |
| Fixed-function texture env (`glTexEnv*`) | `draw_arb.cpp`, `draw_common.cpp` | ARB path only |
| `glTexGen*` (`GL_OBJECT_PLANE`, `GL_TEXTURE_GEN_S/T/Q`) | `draw_common.cpp:1263-1268`, `:534`, `:369`, `tr_light.cpp` | Blend lights, mirror clip notch, skybox |
| Client-side vertex arrays (`glVertexPointer`, `glTexCoordPointer`, `glEnableClientState`) | everywhere in draw paths | Illegal in WebGL2 — all must become VAO+buffer |
| `glVertexAttribPointerARB` with hard-coded indices 8..11 | `draw_arb2.cpp:165-198`, `draw_common.cpp:796-802` | Renumber to 0..4 |
| `glColor4f` / `glColor3f` per-stage colour | shadow debug, blend lights | Becomes a uniform |
| `glAlphaFunc` (`GL_ALPHA_TEST`) | `GL_State:379`, `RB_T_FillDepthBuffer:445` | Must become `discard` |
| `glPolygonMode(GL_LINE)` | `GL_State:368` (`GLS_POLYMODE_LINE`) | No WebGL2 equivalent |
| `glDrawPixels` / `glReadPixels` | `tr_rendertools.cpp:424,473,2200` | Debug/readback only |
| `glPushAttrib` / `glPopAttrib` | `tr_rendertools.cpp:170` | Debug only |
| `glDepthBoundsEXT` | `draw_common.cpp:1105` | No WebGL2 equivalent |
| `GL_INCR_WRAP_EXT` / `GL_DECR_WRAP_EXT` | `RenderSystem_init.cpp:419` | Core as `GL_INCR_WRAP`/`GL_DECR_WRAP` |
| `glActiveStencilFaceEXT` | `RenderSystem_init.cpp:451` | Core as `stencilOpSeparate` |
| `glColorTableEXT` (shared palette) | `Image_init.cpp:1386` | Drop (palette→RGBA at load) |
| Device gamma ramp (`SetDeviceGammaRamp`) | `win_gamma.cpp`, `GLimp_SetGamma` | No browser API |
| `VertexCache::Position()` returning `(void*)(intptr_t)offset` | `VertexCache.cpp` | Every `glVertexPointer` call site depends on this idiom; must be rewritten mechanically |

**Good news:** the `qgl*` layer is already the *only* abstraction. Replacing `qgl.h` with an RHI
header and `win_qgl.cpp` with a platform loader is a mechanical, verifiable step — no engine logic
lives below it.

---

## 13. Porting plan

### 13.1 Target architecture

```
  ┌──────────────────────── unchanged ────────────────────────┐
  │ Materials (.mtr) · Images · Models · Portals · Interactions│
  │ idRenderWorld public API · demo · trace · decl system      │
  └───────────────────────────┬───────────────────────────────┘
                              │  viewDef_t / drawSurf_t / viewLight_t
                              │  (frame-allocated, backend-safe)
                    ┌─────────▼─────────┐
                    │  RHI (new, thin)  │  ~60 calls:
                    │  buffers, VAO,    │  setState(bits), bindTexture(unit),
                    │  textures, FBO,   │  draw(elements), drawShadowChain,
                    │  shaders, uniforms│  copyToTexture, beginView/endView
                    └────┬────────┬─────┘
              ┌──────────▼──┐  ┌──▼──────────┐
              │ GL2 backend │  │ WebGPU      │
              │ (native +   │  │ backend     │
              │  WebGL2)    │  │ (later)     │
              └─────────────┘  └─────────────┘
```

Rules that make this work:

1. **Keep the command list.** `RC_DRAW_VIEW` → one RHI "begin view / draw list / end view" block.
   A self-contained `viewDef_t` is exactly what a WebGPU render pass wants.
2. **Keep `GL_State(int)` and the `GLS_*` bit vocabulary** as the RHI's pipeline-state key, and
   hash it into cached pipeline objects. It is already a compact, complete description of the
   needed state (blend, depth func/mask, color mask, alpha test, polygon mode).
3. **Keep `drawSurf_t::sort` and the single sort.** Modern APIs just need the same order.
4. **One shading path.** Collapse `BE_ARB/BE_ARB2/BE_NV10/BE_NV20/BE_R200` into `BE_ARB2`
   semantics; delete the other four files and their `draw_*` dispatch in `RB_STD_DrawView:1688`.
5. **Delete `glconfig_t`'s ancient half** and the `r_useCombinerDisplayLists`,
   `r_useNV20MonoLights`, `r_testARBProgram`, `r_useExternalShadows`, `r_useTwoSidedStencil`,
   `r_useDepthBoundsTest` cvars once the new backend is the only one.
6. **Turn `tr.viewDef` into an explicit parameter** (or thread-local) before any parallelisation.

### 13.2 Phases

| Phase | Deliverable | Exit criterion |
|---|---|---|
| **P0 — Freeze contracts** | `RendererCaps` struct; document the 60-call RHI surface; golden-image harness (`r_lockSurfaces` + fixed `renderView_t` + `TakeScreenshot`) | A recorded view renders byte-identically twice on the legacy build |
| **P1 — RHI + native GL2 backend** | Replace `qgl.h`/`win_qgl.cpp` with `rhi.h` + `rhi_gl.c`; port `tr_backend.cpp`, `tr_render.cpp`, `VertexCache.cpp`, `Image_load.cpp` | Native build passes existing golden images |
| **P2 — Shader set** | Author GLSL ES 3.00 for `interaction`, `ambientLight`, `bumpyEnvironment`, `environment`, `shadow`, `glasswarp`, `megaTexture`; convert `PP_*` to a UBO (`Shaders::Params`) | Interactions/shadows match golden images within tolerance |
| **P3 — Render targets** | FBO stack: `_currentRender`, GUI surfaces, mirrors (`R_MirrorRender`), cinematics, `CropRenderSize`/`CaptureRenderToImage` | Mirrors, security cameras, post-process chain work |
| **P4 — Alpha test, texgen, wireframe** | `discard`-based alpha test (shader flag + uniform); texgen in shader (skybox, wobbleSky, blend light, mirror notch); line-index debug buffers | No pixel of `mc_perforated` geometry regresses; `r_showTris` works |
| **P5 — WebGL2 bring-up** | `rhi_webgl2`; async image/model loading; canvas resize/DPR; `requestAnimationFrame` loop; input/UI glue | Full map playable in a browser at target resolution |
| **P6 — Optional WebGPU** | `rhi_webgpu`: pipeline cache, bind groups, render bundles; reverse-Z (or 0..1 depth) projection variant | Parity on the golden set |
| **P7 — Cleanup** | Delete `draw_arb/nv10/nv20/r200/exp`, `cg_explicit`, `qgl_linked.h`, `glext.h`, `wglext.h`, dead cvars | Build has zero GL1.x symbols |

### 13.3 Feature → modern replacement → risk

| Legacy feature | Where | Replacement | Risk |
|---|---|---|---|
| `ARB_vertex_program` / `ARB_fragment_program` text (`!!ARBvp1.0`) | `draw_arb2.cpp:365` | GLSL ES 3.00 / SPIR-V; **sources must be re-authored** (absent from repo) | **High** — math must be re-derived from §7.2; no reference files |
| Program env params (`glProgramEnvParameter4fvARB`) | `draw_arb2.cpp:69-109` | `uniform` block / UBO; all of `PP_*` + `FP 0/1` | Medium — mechanical once the ABI is enumerated |
| `glProgramStringARB` + `GL_PROGRAM_ERROR_STRING_ARB` | `draw_arb2.cpp:433` | `glShaderSource`/`compileShader` + `getShaderInfoLog` | Low |
| Fixed-function blend/depth/cull via `GL_State` | `tr_backend.cpp:239` | Pipeline-state object keyed by `GLS_*` bits | Low |
| `glAlphaFunc` / `GL_ALPHA_TEST` | `GL_State:379`, `RB_T_FillDepthBuffer:445` | `if (a < ref) discard;` — needs a compiled variant or a `uniform` branch | Medium: discard kills early-Z; **must** also apply in the depth prepass or stencil-128 marking breaks |
| `glTexGen` object planes (blend lights, skybox, wobbleSky, mirror notch) | `draw_common.cpp:1263,534,369`, `tr_light.cpp:193,218` | Vertex shader computes `st`/`stq` from `u_lightProject` / `u_cubeRotate`; CPU fallbacks already exist | Medium |
| Fixed-function `glTexEnv` combine (`BE_ARB`) | `draw_arb.cpp` | Drop the backend; ARB2 math is authoritative | Low |
| `NV_register_combiners`, `NV_texture_shader` (`BE_NV20/NV10`) | `draw_nv20.cpp`, `draw_nv10.cpp` | Delete | Low |
| `ATI_fragment_shader` (`BE_R200`) | `draw_r200.cpp` | Delete | Low |
| `NV_vertex_array_range`, `ARB_vertex_buffer_object` | `VertexCache.cpp` | `ARRAY_BUFFER`/`ELEMENT_ARRAY_BUFFER` + VAO | Low |
| `VertexCache::Position()` = offset-cast-to-pointer | `VertexCache.h:76` | Return an explicit `{buffer, offset}` | Low but **touches every draw call** |
| Client-side arrays (`glVertexPointer` + CPU pointer) | all draw paths | VAO + buffer; per-frame temp ring already exists (`AllocFrameTemp`) | Medium (breadth, not difficulty) |
| Hard-coded attribute indices 8–11 | `draw_arb2.cpp:165` | `layout(location=)` 0..4 | Low |
| `GL_POLYGON_OFFSET_FILL` + `glPolygonOffset` | decals/shadow | Core, unchanged | Low |
| Stencil shadows: incr/decr wrap, `stencilOpSeparate` | `draw_common.cpp:1053-1231` | Core in ES3 (`INCR_WRAP`, `stencilOpSeparate`, `stencilFuncSeparate`) | **Low** — the main reason the stencil pipeline survives WebGL2 nearly intact |
| `EXT_depth_bounds_test` | `draw_common.cpp:1105` | **No equivalent.** Options: (a) drop, cost = extra shadow fill; (b) cull caps more aggressively using `shadowCapPlaneBits`; (c) per-light scissor only | **High (perf)**, low correctness |
| `glPolygonMode(GL_LINE)` (`GLS_POLYMODE_LINE`, wireframe) | `GL_State:368` | No equivalent; emit line index buffers for `r_showTris` | Low (debug only) |
| `glCopyTexImage2D` / `glCopyTexSubImage2D` for `_currentRender`, mirrors, GUI, video | `Image_load.cpp:1853` | FBO color attachment + `blitFramebuffer` or `copyTexSubImage2D` against `READ_FRAMEBUFFER`; the POT-padding workarounds become unnecessary | Medium — touches subviews, post-process, GUI |
| `glCopyTexImage2D(GL_DEPTH_COMPONENT)` (`CopyDepthbuffer`) | `Image_load.cpp:1927` | Depth texture attachment + blit (WebGL2: `DEPTH_COMPONENT24`) | Medium |
| 3D textures (`TT_3D`, `glTexImage3D`) | `Image_load.cpp:777` | Core ES3 | Low |
| Cube maps (`GL_TEXTURE_CUBE_MAP_EXT`) | `Image_load.cpp`, `draw_arb2` | Core; drop `_EXT` | Low |
| Shared texture palette (`glColorTableEXT`) | `Image_init.cpp:1386` | Expand palette→RGBA8 at load | Low |
| Device gamma ramp (`SetDeviceGammaRamp`) | `win_gamma.cpp` | No browser API; fold `r_gamma`/`r_brightness` into a final full-screen pass (or CSS filter) | Medium — changes the look unless compensated |
| Infinite-far-z projection (`m[10]=-0.999, m[14]=-2*zNear`) | `tr_main.cpp:936-957` | **WebGL2: unchanged** (same −1..1 clip depth). **WebGPU/Vulkan: must be rewritten** for 0..1 (and prefer reverse-Z) — and every depth-comparison/`glDepthFunc` assumption audited | Medium for WebGPU, low for WebGL2 |
| `glDrawPixels` / `glReadPixels` debug & screenshots | `tr_rendertools.cpp`, `RenderSystem_init.cpp` | Canvas/`readPixels` with async readback; tiled screenshot logic needs clamping | Low |
| `glPushAttrib(GL_ALL_ATTRIB_BITS)` | `tr_rendertools.cpp:170` | Explicit state save/restore in the RHI | Low |
| Front-end/back-end threading (`SMP_FRAMES`, `GLimp_SpawnRenderThread`) | `tr_main.cpp:184`, `win_glimp.cpp:1086` | Already dead; JS is single-threaded. Keep the *command list*, drop the threads; consider a worker for asset decode | Low |
| FreeType glyph rasterisation | `tr_font.cpp` | Pre-baked glyph atlas (PNG+JSON) or OffscreenCanvas text | Low |
| ROQ decode in C++ | `Cinematic.cpp` | Keep (WASM) or transcode to WebM/VP9 | Low |
| CPU md5 skinning | `Model_md5.cpp:253` | Optional: joint palette as a UBO + 4 bone weights → big win on low-end GPUs | Medium (new work, not a port blocker) |

### 13.4 Shader migration specifics

1. Reconstruct `interaction` from `draw_arb2.cpp` + `draw_common.cpp:600-650`:
   inputs `xyz, st, normal, tangent0, tangent1, color`; outputs the additive light term
   `diffuse * (N·L) * falloff * lightProjection + specular * specTable(N·H)`.
   The **normalisation cube map (TMU 0)** exists only because ARBvp1 had no `normalize`-in-fragment
   for arbitrary vectors — in GLSL it becomes `normalize()`, which also removes a texture fetch and
   the `makeNormalizeVectorCubeMap` dependency. Keep the cube map only if bit-exact parity is
   required for the golden set.
2. `shadow.vp` is a one-liner conceptually: `pos = mix(worldPos, worldPos + L*infinity, isRear)`,
   using the sign bit packed into the duplicated vertex index (`v1 ^ 1` trick in
   `tr_turboshadow.cpp:128-133`). Reproduce it exactly, including `w=0` light direction.
3. `ambientLight.vfp` and `bumpyEnvironment.vfp` are the non-light-dependent passes; they need
   `r_skipNewAmbient`-equivalent variants for A/B testing.
4. Keep the `vertexParm`/`fragmentMap` naming (`u_vertexParm[N]`, `u_fragMap[N]`) so existing
   `.mtr` files that author `vertexProgram` stages keep working with only the program file
   swapped — i.e. the *data* stays, only `glprogs/` changes.
5. Prefer one uber-shader + `#define` variants over runtime branches for: alpha test (3 modes),
   vertex-colour policy (3 modes), ambient vs light interaction. That mirrors the existing
   `PP_COLOR_MODULATE/ADD` and `SVC_*` switches, so behaviour stays traceable.

### 13.5 Performance notes for the web target

| Concern | Cause | Mitigation |
|---|---|---|
| Draw-call count | One draw per (surface × light) pass, plus shadow passes per light | Frustum+scissor culling already exists; add per-light batching of identical materials; consider a deferred/clustered path **reusing the `draw_exp.cpp` design** |
| Shadow fill rate without depth bounds | `EXT_depth_bounds_test` gone | Tighten `R_CalcLightScissorRectangle`/`R_ClippedLightScissorRectangle` usage; prefer turbo shadows (fewer caps); optional shadow-map path |
| Alpha-test discard | Kills early-Z | Do the depth prepass with the *same* alpha-test shader (already the design), keep `r_useShadowSurfaceScissor` on |
| Per-frame vertex uploads | Deforms, GUI, md5 skinning | Frame-temp ring exists (`AllocFrameTemp`); move md5 to GPU skinning |
| Texture bandwidth | DXT + 7 TMUs in interaction | Keep DXT1/5 (WebGL2 supports `COMPRESSED_RGBA_S3TC_*` via `WEBGL_compressed_texture_s3tc`); drop the normalisation cube lookup if parity allows |
| JS GC pauses | `R_FrameAlloc` is C++ bump allocation | Keep the same bump allocator in WASM; avoid per-frame JS object creation in the RHI |

### 13.6 Acceptance tests (evidence, not opinion)

| Test | Method | Pass |
|---|---|---|
| Interface parity | `r_lockSurfaces` + fixed `renderView_t` → `TakeScreenshot` | Pixel diff vs legacy ≤ tolerance |
| Shadow correctness | `r_showShadows 1/2/3`, `r_showShadowCount`, stencil readback | No volume leakage at near-plane, in mirrors, and with `r_useExternalShadows 0/1/2` |
| Portal correctness | `r_showPortals 1`, door open/closed, `AreasAreConnected` | Areas match; no pop-in |
| Material fidelity | `testImage`, `r_showSurfaceInfo`, `r_showTexturePolarity` | All shipped `.mtr` render |
| Perf | Frame time on 1 static view, `r_showPrimitives` | Within agreed budget of legacy |

---

## 14. What can be done **data-only** (no engine change, now or after the port)

| Surface | Files | Why it is safe |
|---|---|---|
| Materials | `materials/*.mtr` | Parsed at runtime by `idMaterial::Parse`; hot-reloadable |
| Animated material behaviour | `.mtr` expressions + `deform` + `scroll/rotate/scale` | Interpreted by the register VM (`EvaluateRegisters`), no GL contact |
| Image programs | `addnormals()`, `heightmap()`, `makeIntensity()`, … in material/image decls | Pure CPU pixel math (`Image_program.cpp`) |
| Textures | `textures/**` TGA/JPG/DDS | `R_LoadImage` + `CheckPrecompressedImage`; `UploadImage` API exists for runtime uploads |
| Precompressed DDS | `.dds` alongside sources | `WritePrecompressedImage` / `r_preloadImages` |
| Shaders (post-port only) | `glprogs/*` | **After** the GLSL set exists: `R_FindARBProgram`/`R_ReloadARBPrograms_f` and `.mtr` `vertexProgram` allow data-defined programs **today**; on WebGL2 the same files become GLSL |
| Maps | `maps/*.proc` | `mapProcFile003`; areas/portals/nodes are CPU data |
| Models | `.md5mesh/.md5anim/.md3/.ase/.lwo/.ma/.prt/.liquid` | Loaded through `idRenderModelManager` |
| Skins | `skins/*.skin` | `R_RemapShaderBySkin` (`RenderWorld.cpp:2117`) |
| Particles | `particles/*.prt` + `DeclParticle` | `idRenderModelPrt` + `R_ParticleDeform` |
| GUI | `guis/*.gui` | `idUserInterface` → `R_RenderGuiSurf` |
| Fonts | `fonts/*` | `RegisterFont` (FreeType atlas) |
| Lighting look | `lights/*.mtr`, `r_lightScale`, light `shaderParms` | Shader + register driven |
| cvars | `r_*` | Most are debug/perf; the visual ones are documented in `tr_local.h:816-988` |

**What is *not* data-only:** anything requiring a new GL feature (depth bounds, wireframe, gamma
ramp), any new shading model (needs a shader program), and the `_currentRender`/subview plumbing.

---

## 15. Risks and open questions

| # | Risk | Impact | Evidence | Mitigation |
|---|---|---|---|---|
| 1 | **Shaders are not in the repo** | Cannot mechanically port the reference path | `draw_arb2.cpp:340-356` lists `glprogs/*`; `doom3_orig/doom3/base/` is empty; no `.vfp/.vp` anywhere in the tree | Re-author from `PP_*` + `drawInteraction_t`; use `r_testARBProgram`/`r_showTextureVectors` to reverse-engineer if a full game install is available |
| 2 | Losing `EXT_depth_bounds_test` | Shadow fill cost on mobile | `draw_common.cpp:1105` | Scissor tightening + turbo shadows; measure before optimising |
| 3 | Alpha-test via `discard` in the *prepass* | Breaks stencil-128 marking → shadow acne / light leak | `RB_T_FillDepthBuffer:445` + `RB_StencilShadowPass:1209` | Keep one shader variant used by both passes; regression test with perforated materials (grates, foliage) |
| 4 | Gamma/brightness via hardware ramp | Visual mismatch | `win_gamma.cpp`, `R_SetColorMappings:1738` | Post-process LUT; compare screenshots |
| 5 | `tr.viewDef` global | Blocks parallel/worker rendering and re-entrancy | `tr_local.h:809` + ~dozens of uses | Explicit parameter before P5 |
| 6 | Subview recursion (`R_GenerateSubViews`) needs a render-target stack | Mirrors/cameras break if naively ported | `tr_subview.cpp:451,549` | Build the FBO stack in P3, before WebGL2 |
| 7 | `RB_DrawElementsImmediate` draws from *client* arrays with `vertexCache.Position` offsets | Silent corruption if the idiom is half-migrated | `tr_render.cpp:49` | Mechanical but global change; do it in P1 with a compile-time guard |
| 8 | Demo format encodes renderer state | Playback of old demos may diverge | `RenderWorld_demo.cpp`, `GuiModel::WriteToDemo` | Accept divergence, or keep a `demoCompat` field |
| 9 | `r_useOptimizedShadows` + `prelightModel` depend on dmap-baked volumes | Re-baking may be needed if the shadow math changes | `tr_light.cpp:911` | Keep the turbo path identical first |
| 10 | Void guarantees about `MAX_TEXTURE_UNITS` on WebGL2 mobile | Interaction needs 7 TMUs | `draw_arb2.cpp:114-131`; ES3 guarantees 16 fragment units | Should be safe; assert at init |

Open questions worth resolving before P2:

* Is a full game install (pak000–004, hence `glprogs/`) available for shader ground truth, or must
  the programs be re-derived from the C++ side alone?
* Target baseline: WebGL2 only (keeps stencil shadows and the −1..1 depth convention) or WebGPU
  too (forces a projection rewrite + pipeline objects)?
* Is bit-exact legacy parity a requirement, or is "visually equivalent" acceptable? This decides
  whether the normalisation cube map, the specular LUT, and `RB_STD_LightScale`'s overbright
  emulation survive.

---

## Appendix A — file → responsibility quick index

| Need to change… | Go to |
|---|---|
| Culling, matrices, frame memory, view entry | `tr_main.cpp` |
| What is visible, what interacts | `tr_light.cpp`, `Interaction.cpp`, `RenderWorld_portals.cpp` |
| Per-view GL setup, draw element wrappers | `tr_render.cpp` |
| GL state, clear, swap, command dispatch | `tr_backend.cpp` |
| Depth prepass, shader passes, shadows, fog, light scale | `draw_common.cpp` |
| Light/surface shading math (reference) | `draw_arb2.cpp` (+ `draw_arb.cpp` for the fixed-function version) |
| Shadow volume topology | `tr_stencilshadow.cpp`, `tr_turboshadow.cpp`, `tr_shadowbounds.cpp` |
| Geometry cleanup, silhouette edges, tangents | `tr_trisurf.cpp` |
| CPU vertex deformation | `tr_deform.cpp` |
| `.mtr` grammar and register VM | `Material.cpp` |
| Textures | `Image_init.cpp` (generators), `Image_load.cpp` (upload/bind), `Image_files.cpp` (decoders), `Image_program.cpp` (image expressions) |
| Model formats | `Model_md5.cpp`, `Model_md3.cpp`, `Model_ase.cpp`, `Model_lwo.cpp`, `Model_ma.cpp` |
| Model registry / extension dispatch | `ModelManager.cpp` |
| 2D + GUI batching | `GuiModel.cpp`, `RenderSystem.cpp` |
| Mirrors / cameras | `tr_subview.cpp`, `tr_guisurf.cpp` |
| VBO arena | `VertexCache.cpp` |
| Platform GL loading | `../sys/win32/win_qgl.cpp`, `win_glimp.cpp` |

## Appendix B — most-cited functions

```
Front end   R_RenderView (tr_main.cpp:1095) · FindViewLightsAndEntities (RenderWorld_portals.cpp:840)
            R_AddLightSurfaces (tr_light.cpp:875) · R_AddModelSurfaces (tr_light.cpp:1460)
            idInteraction::CreateInteraction (Interaction.cpp:823) · AddActiveInteraction (:1024)
Visibility  FlowViewThroughPortals (:285) · FloodViewThroughArea_r · CullEntityByPortals (:564)
            CullLightByPortals (:645) · FloodConnectedAreas (:935)
Shadows     R_CreateShadowVolume (tr_stencilshadow.cpp:1217) · R_MakeShadowFrustums (:1032)
            R_CreateVertexProgramTurboShadowVolume (tr_turboshadow.cpp:45)
            RB_StencilShadowPass (draw_common.cpp:1176) · RB_T_Shadow (:1053)
Backend     RB_ExecuteBackEndCommands (tr_backend.cpp:600) · GL_State (:239) · GL_Cull (:161)
            RB_STD_DrawView (draw_common.cpp:1666) · RB_STD_FillDepthBuffer (:521)
            RB_STD_DrawShaderPasses (:972) · RB_BeginDrawingView (tr_render.cpp:555)
            RB_CreateSingleDrawInteractions (:689) · RB_DrawView (:849)
Shading     RB_ARB2_DrawInteraction (draw_arb2.cpp:67) · R_LoadARBProgram (:365)
            R_FindARBProgram (:466) · RB_SetProgramEnvironment (draw_common.cpp:579)
Materials   idMaterial::Parse (Material.cpp:2122) · ParseStage (:1079) · ParseExpression (:729)
            AddImplicitStages (:1679) · SortInteractionStages (:1742)
Images      idImage::Bind (Image_load.cpp:1697) · ActuallyLoadImage (:1576)
            CopyFramebuffer (:1853) · idImageManager::AllocImage (Image_init.cpp:1404)
Models      idRenderModelManagerLocal::GetModel (ModelManager.cpp:253)
            idMD5Mesh::TransformVerts (Model_md5.cpp:253) · R_CleanupTriangles (tr_trisurf.cpp:2096)
2D          idGuiModel::EmitFullScreen (GuiModel.cpp:226) · R_RenderGuiSurf (tr_guisurf.cpp:128)
```
