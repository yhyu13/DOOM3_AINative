# Engine integration and durable delivery (T033)

**Status**: Q2 design. No engine code, build file, or submodule is changed by this task. All
`path:line` anchors are against the pinned pristine submodule
`doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` and the current superproject
`main`. Line numbers drift on a bump or a CMake edit; re-anchor before implementation.

**Scope owner**: T033. Implemented by T035 (ordered scene adapter + fork integration), T036
(selectable path + both-executable linkage), T080–T082 (production slices), T083 (forced-failure
integration check), T084 (patch-delivery controls; complete for manifest `A`/`D` and partial-`M`
handling, with the provenance caveat in §1.5). Requirements: FR-003, FR-006, FR-019, SC-003.

---

## 1. What this document decides

1. **Delivery**: every engine change goes through a branch of the `yhyu13/DOOM-3` fork (cloned or
   checked out **outside** `doom3_orig/doom3`) and a reviewed submodule bump. The target copy is
   scratch. A new engine file cannot be delivered by
   `targets/win11-amd64/patches/all-changes.patch`, because that exporter reproduces modified files
   only. **Decision (T084): added/deleted target-copy delivery is not implemented.** The exporter
   exits 1 when the manifest contains an `A` or `D` row, the applier exits 1, and the verifier
   reports `SC-006a`/`SC-006b` FAIL. Added/deleted engine files are therefore fork/submodule-only,
   and the scripts enforce that boundary instead of leaving it advisory (§3.3).
2. **Linkage**: a superproject-owned static `doom3_rhi` library is linked into **both** `DOOM3.exe`
   and `DOOM3XP.exe` under the existing `/MT`/`/MTd` setting; the engine adapter compiles against the
   RHI headers; game objects never link the RHI.
3. **Selectable path**: a separate `r_renderApi` selection, a backend-neutral "render system running"
   predicate replacing the **30** `glConfig.isInitialized` references (26 reads, 1 comment, 3 writes; with a per-site disposition),
   an explicit `RC_*` dispatch that bypasses GL-only backend code, and a **non-automatic** fallback
   that leaves the GL path selectable at the next launch.
4. **Ordering**: the backend consumes the ordered `RC_DRAW_VIEW` / `RC_SET_BUFFER` /
   `RC_COPY_RENDER` / `RC_SWAP_BUFFERS` stream, including mid-frame capture, rather than wrapping
   individual `qgl*` calls.
5. **Provenance**: the patch exporter/verifier is self-consistency only (T084); a green `SC-006b`
   is not proof of canonical-patch provenance, and the patch baseline moves with the submodule.

---

## 2. Current build topology (facts)

| Fact | Anchor | Consequence |
|---|---|---|
| Root configure creates/patches the target copy, then includes `targets/<target>` | `CMakeLists.txt:19-83` | Implementation builds from a copy; the copy is not a delivery artifact |
| `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` is set before any target | `targets/win11-amd64/CMakeLists.txt:39` | New targets created after it inherit `/MT` in Release and `/MTd` in Debug |
| `CMP0091` is `NEW` before `project()` | `targets/win11-amd64/CMakeLists.txt:3,9` | The runtime-library setting is effective |
| `doom3_engine` is an OBJECT library shared by both executables | `targets/win11-amd64/CMakeLists.txt:74-77` | Engine code is compiled once; renderer selection is shared |
| `doom3_engine` links only `doom3_common` (INTERFACE includes/defines) | `targets/win11-amd64/CMakeLists.txt:44-77` | RHI include dirs must be added to the engine target, not only to the executables |
| `doom3_game_base` / `doom3_game_d3xp` are separate OBJECT libraries | `targets/win11-amd64/CMakeLists.txt:79-87` | Game code must not depend on the RHI |
| `doom3_add_exe` creates both executables from `$<TARGET_OBJECTS:doom3_engine>` plus game objects, and links `DOOM3_LINK_LIBS` | `targets/win11-amd64/CMakeLists.txt:89-107` | A target appended to `DOOM3_LINK_LIBS` before the two calls links into both; one call site, no per-EXE divergence |
| `sources.cmake` is generated from the target copy's VS projects; no glob | `scripts/gen-sources-win11-amd64.ps1:27-31,50-52,82-109`; `targets/win11-amd64/CMakeLists.txt:27-37` | A fork file absent from the generated list is silently not compiled; a file must be listed explicitly or added to the source project and regenerated |
| The target CMake has no `enable_testing()` or `add_test()`; tests are PowerShell scripts | `targets/win11-amd64/README.md:146-147`; root `CMakeLists.txt:88-100` | Q1a's standalone CTest project stays separate from the engine build |
| Presets use distinct build dirs (`build`, `build-hd`); line 36 is a build preset, not a dir | `CMakePresets.json:12,26,36` | Never share a build dir between worktrees/targets |

### 2.1 What is not in the current build

- Q1a is **merged on `main`**: `src/renderer/rhi/rhi.h`, `src/renderer/d3d12/rhi_d3d12.cpp`
  and the standalone `tests/renderer/CMakeLists.txt` exist. The Q1a CTest project builds and passes
  7/7 in Debug and Release on this host; the debug layer/GPU validation are skipped
  (`hr=0x887A002D`) and are not converted into passes.
- The **engine** build still has no `doom3_rhi` target, no RHI include path and no engine adapter;
  Q1a's standalone tests are not linked into `DOOM3.exe`/`DOOM3XP.exe`.
- `r_renderer` accepts a set of **GL** names (`best, arb, arb2, Cg, exp, nv10, nv20, r200`);
  `SetBackEndRenderer` maps only five paths and falls back to a GL path
  (`RenderSystem_init.cpp:45,88`; `RenderSystem.cpp:536-580`). It is **not** a modern-backend
  selector.
- The engine is two EXEs (`DOOM3.exe`, `DOOM3XP.exe`) with no DLL export surface; the renderer must
  be in-process.

---

## 3. Delivery model

### 3.1 File classes

| Class | Examples | Delivery | Review |
|---|---|---|---|
| Superproject source | `src/renderer/rhi/**`, `src/renderer/d3d12/**`, `src/renderer/rhi/engine_scene_adapter.cpp`, `tests/renderer/**` | Normal PR in this repo | Normal + independent review |
| Superproject build wiring | `targets/win11-amd64/CMakeLists.txt`, `targets/win11-amd64/sources.cmake` (if regenerated), root presets | Normal PR | Build-system change requires an independent reviewer |
| Engine-owned source | `neo/renderer/RenderSystem.cpp`, `RenderSystem_init.cpp`, `tr_local.h`, `tr_backend.cpp`, `draw_common.cpp`, new `neo/renderer/d3d12/**`, `doomdll.vcxproj` if needed | **Fork branch → fork PR → submodule bump** | Fork PR + submodule bump, independent reviewer |
| Target copy | `targets/win11-amd64/doom3/**` | **Never committed**; local iteration only | Not a delivery path |
| Game data | `*.pk4`, `*.pk3`, `*.pak`, extracted assets | **Never tracked** | Legal/EULA |

### 3.2 The only durable engine path

1. Clone or check out the fork (`https://github.com/yhyu13/DOOM-3.git`) in a **separate working
   directory**; never create a branch or commit inside `doom3_orig/doom3` (the pristine submodule is
   read-only).
2. Make and test the engine change on that fork branch; open a fork PR.
3. In this repository, `git add doom3_orig/doom3` and commit `chore(doom3): bump to <sha>`.
4. The bump PR body carries the fork PR URL and the old/new SHA.
5. A submodule bump needs one **independent** reviewer who reruns the verification command **plus
   human owner sign-off**. A build-system change needs an independent reviewer who reruns the build
   and required tests and records the evidence in the APPROVED comment; it may then self-merge
   (`docs/GIT_WORKFLOW.md:160-165,191-192`; `AGENTS.md` §11).

A change that exists only in a target copy is not delivered, even if it builds and renders locally.
The fork PR cannot be merged standalone if it depends on a superproject header that is not yet
merged; record that dependency in both PRs.

### 3.3 Target-patch limitations (must be stated in every PR)

| Limitation | Anchor | Consequence |
|---|---|---|
| Exporter emits hunks only for modified files | `scripts/export-target-patches.ps1:71-82` | `A` and `D` are manifest rows without content |
| `A`/`D` rows are manifest-only | `scripts/export-target-patches.ps1:83-91` | A new engine file cannot be reconstructed from the patch |
| Exporter fails closed on `A`/`D` | `scripts/export-target-patches.ps1:90-91` | Exit 1, not a PASS; the files are still written for inspection |
| Apply fails closed on `A`/`D` | `scripts/apply-target-patches.ps1:33-37,49,72` | `-AllowIncomplete` is inspection-only and still exits non-zero |
| Verifier fails `SC-006a`/`SC-006b` on an `A`/`D` manifest | `scripts/verify-win11-amd64.ps1:94-105` | A green run does not cover added/deleted files |
| Verifier requires every `M` row | `scripts/verify-win11-amd64.ps1:121-130` | A partial match is not counted as verified |
| Patch and expected hashes are regenerated from the same copy | `scripts/export-target-patches.ps1:27`; `scripts/verify-win11-amd64.ps1:124-126` | A pass is self-consistency, not canonical-patch provenance |
| Patch baseline is the pinned submodule | `scripts/export-target-patches.ps1:27` | After a bump, re-export and re-verify; old hashes describe the old baseline |

T084's manifest controls are implemented and mutation-tested in
[`evidence-t084.md`](./evidence-t084.md): an `A`/`D` export exits 1, an `A`/`D` manifest is refused
by the verifier independently of the exporter exit code, a partial `M` match (1/2) is refused, a
patch/manifest hash disagreement fails, and a missing patch fails. Each control was shown red under a
mutation that restored the pre-fix behavior and green after the restore.

The provenance caveat remains: the manifest and its expected hashes are produced from the same target
copy, so `SC-006b` proves self-consistency only. Do not present a green `SC-006b` as canonical-patch
provenance.

Therefore:

- New **engine-side** files (`neo/renderer/d3d12/*.cpp`, new engine headers, `RenderSystem.cpp`
  glue) **must** ship through the fork/submodule path. The ordered scene adapter is not an engine-side
  file: it is the superproject `src/renderer/rhi/engine_scene_adapter.cpp` (T035) compiled into
  `doom3_engine` via `RHI_ADAPTER_SOURCES`.
- `targets/win11-amd64/CMakeLists.txt` is tracked, so adding `doom3_rhi` and an explicit adapter
  source list is a normal superproject change.
- Do not add a new engine file to both the generated `ENGINE_SOURCES` and an explicit list; that
  duplicates definitions at link (see §4.4).

---

## 4. Both-executable linkage

### 4.1 Target shape

```text
doom3_rhi (STATIC, /MT or /MTd)          <-- superproject src/renderer/rhi + d3d12
  PUBLIC include: src/renderer/rhi
  PRIVATE include: src/renderer          <-- d3d12 implementation only
  no engine headers, no game headers

doom3_engine (OBJECT, /MT or /MTd)       <-- engine TUs + RHI_ADAPTER_SOURCES
  private link to doom3_rhi for include dirs and symbols
  compiles the adapter that includes <rhi.h>

doom3_game_base / doom3_game_d3xp (OBJECT)  <-- no RHI dependency
DOOM3.exe / DOOM3XP.exe
  $<TARGET_OBJECTS:doom3_engine> + game objects + DOOM3_LINK_LIBS (includes doom3_rhi)
```

### 4.2 Proposed CMake wiring (not applied)

```cmake
# --- After CMAKE_MSVC_RUNTIME_LIBRARY at line 39, before doom3_engine is created at line 76. ---
add_library(doom3_rhi STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/renderer/d3d12/rhi_d3d12.cpp"
)
target_include_directories(doom3_rhi
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/renderer/rhi"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/renderer"
)
target_compile_definitions(doom3_rhi PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(doom3_rhi PUBLIC d3d12 dxgi)

# --- After doom3_engine exists (line 76). Do not declare it a second time. ---
set(RHI_ADAPTER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/renderer/rhi/engine_scene_adapter.cpp"
)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/renderer/rhi/engine_scene_adapter.cpp")
    message(FATAL_ERROR
        "engine_scene_adapter.cpp is missing; land T035's superproject adapter before configuring the engine")
endif()
target_sources(doom3_engine PRIVATE ${RHI_ADAPTER_SOURCES})
target_link_libraries(doom3_engine PRIVATE doom3_rhi)

# --- After the existing set(DOOM3_LINK_LIBS ...) at line 89, before the two doom3_add_exe calls. ---
list(APPEND DOOM3_LINK_LIBS doom3_rhi d3d12 dxgi)
```

Notes:

- The source list is Q1a's actual static library (`tests/renderer/CMakeLists.txt:12`); there is no `src/renderer/rhi/rhi.cpp`. The adapter path is a future **superproject** file, not a fork file.
- `RHI_ADAPTER_SOURCES` is the **superproject** adapter from T035 (`src/renderer/rhi/engine_scene_adapter.cpp`). It is compiled inside `doom3_engine` because it consumes engine types, but it lives in the superproject, not the fork. The `EXISTS` guard makes a missing adapter a configure error instead of a silent no-adapter build. It does not depend on a submodule bump.
- `src/renderer/rhi` is the only PUBLIC include. `src/renderer` is PRIVATE so `d3d12` headers do not
  leak to the engine adapter or game TUs; the contract's public-surface gate scans
  `src/renderer/rhi/`, so keep private D3D12 headers out of it.
- `doom3_rhi` must set `WIN32_LEAN_AND_MEAN` and `NOMINMAX`. Without `NOMINMAX`, `<windows.h>` defines
  a `max` macro that breaks `std::numeric_limits<uint64_t>::max()` (and `std::max`) with C2589. The
  real target-structure probe reproduced this at `src/renderer/d3d12/rhi_d3d12.cpp:34`, so the snippet
  includes the definitions.
- `target_link_libraries(doom3_engine PRIVATE doom3_rhi)` supplies include directories and link
  semantics to the compiled adapter. `DOOM3_LINK_LIBS` then makes the static library available to
  both executables. Do not rely on `$<TARGET_OBJECTS:doom3_engine>` to propagate usage requirements;
  it does not carry them the way linking the object-library target would.
- If `doom3_engine` is later linked as a library target instead of consumed through
  `$<TARGET_OBJECTS:>`, revisit this; the current code uses `$<TARGET_OBJECTS:>`.
- The `list(APPEND)` must be **after** the existing `set(DOOM3_LINK_LIBS ...)` at line 89 and
  **before** the two `doom3_add_exe` calls. Appending before the `set()` is overwritten; appending
  after the calls does not affect the already-created targets. `DOOM3_LINK_LIBS` is expanded at call
  time.

### 4.3 `/MT` and `/MTd` rules

- The directory-level `CMAKE_MSVC_RUNTIME_LIBRARY` applies to every target created after it,
  including `doom3_rhi`; do not set a per-target override.
- A static library does not link the CRT; the executable does. All objects in the executable must
  agree. A prebuilt library compiled with `/MD`/`/MDd` is not acceptable: rebuild from source with the
  same setting, or introduce an explicitly reviewed DLL boundary (not chosen here).
- D3D12/DXGI are OS DLLs, not CRT-dependent; DXC, if used, is a separate process and not linked into
  the executable.
- Verify with a probe (see §8) and, after implementation, by inspecting the linked executable's
  dependencies: no `MSVCP140.dll`/`VCRUNTIME140.dll` dependency for a `/MT` build, and both
  executables report PE machine `0x8664`.

### 4.4 Duplicate and missing symbols

- Do not add an adapter `.cpp` to both `ENGINE_SOURCES` (generated from the target copy's
  `doomdll.vcxproj`) and `RHI_ADAPTER_SOURCES`. Choose one owner; the recommended owner is
  `RHI_ADAPTER_SOURCES` in `targets/win11-amd64/CMakeLists.txt`, because it is reviewable and does
  not depend on regenerating `sources.cmake` from a VS project.
- If the adapter is added to the fork's `doomdll.vcxproj` instead, regenerate `sources.cmake`; the
  generator throws on a listed-but-missing file and never discovers an unlisted one
  (`scripts/gen-sources-win11-amd64.ps1:27-31,50-52,82-109`).
- Do not rely on static-initializer self-registration in a static library. Linkers pull only
  referenced objects; a factory function called from the engine is deterministic. If registration is
  used, prove it with `/WHOLEARCHIVE` or an explicit reference, and record it.
- `glConfig` is defined in `RenderSystem_init.cpp:41`; do not define a second copy in the RHI.

### 4.5 `DOOM3XP.exe`

- The engine objects are shared; the only difference is the game object library (`_D3XP;CTF`) and the
  runtime `fs_game d3xp` requirement.
- The renderer is engine-level, so both executables get the same selection and fallback. Test both
  link and start. Retail RoE gameplay is not verified and is not a Q2 claim.

---

## 5. Selectable path and fallback

### 5.1 The current gates: 30 references, not four

`glConfig.isInitialized` appears 30 times: 26 reads, one comment (`RenderSystem_init.cpp:613`) and three writes (`:683,1921,2273`). A blanket replacement is unsafe; each group has a
different meaning. The implementation PR must resolve every row; the groups are:

| Group | Sites | Required disposition |
|---|---|---|
| Frame / capture / resize | `RenderSystem.cpp:630,711,803,883,913,953`; `RenderSystem_init.cpp:1879,2282`; `RenderWorld.cpp:679,1446` | Replace with a backend-neutral `R_RenderSystemRunning()` (GL → `glConfig.isInitialized`; modern → device+surface+resources ready) |
| GUI / 2D | `GuiModel.cpp:335,361,463,574`; `draw_arb2.cpp:389` | Use the running predicate, but the modern path needs its own 2D/HUD renderer (see §5.2) |
| Window procedure | `win_wndproc.cpp:70,280` | Use the running predicate **and** valid `vidWidth`/`vidHeight`; do not treat a modern surface as a GL context |
| GL image upload | `Image_init.cpp:362`; `Image_load.cpp:528,746,885,1050,1337` | Keep GL-specific (they call `qglTex*`); the modern path uses its own image registry. Do not route these through the global predicate |
| Lifecycle | `RenderSystem_init.cpp:613,625,683,1921,2185,2250,2273` | Keep GL lifecycle for GL; add explicit modern init/shutdown/purge; `683` must not become "modern is initialized" without resources |

`R_InitOpenGL` also calls `common->FatalError("Unable to initialize OpenGL")` after one retry
(`RenderSystem_init.cpp:625-627,636-662`). A recoverable modern failure must not reuse that fatal
path; it needs its own typed result.

### 5.2 GL coupling that a modern path must bypass

| GL coupling | Anchor | Required action |
|---|---|---|
| `RB_ExecuteBackEndCommands` calls `RB_SetDefaultGLState()` before the FIFO and `qglBindTexture` after it | `tr_backend.cpp:611,648` | Branch: GL runs both; modern skips both and uses its own default state/end-of-frame |
| `RB_DrawView` unconditionally calls `RB_STD_DrawView` | `tr_render.cpp:883` | Route `RC_DRAW_VIEW` to the ordered scene adapter for modern; do not call the GL pass |
| `CaptureRenderToFile` calls `qglReadBuffer`/`qglReadPixels` | `RenderSystem.cpp:963-969` | Provide a modern readback from the current color target; preserve the mid-frame issue at `:961` |
| `R_VidRestart_f` guards and tears down GL | `RenderSystem_init.cpp:1879,1920-1954` | Replace the guard with the running predicate; add modern shutdown/init; do not call `GLimp_Shutdown` for modern |
| `idImage::CopyFramebuffer` is an immediate `qglCopyTex*` | `Image_load.cpp:1853-1918` | Modern copy through the RHI target/copy path; the front-end `RC_COPY_RENDER` still carries the request |
| `GuiModel::EmitFullScreen` uses the GL matrix path | `GuiModel.cpp:268-274`; `draw_common.cpp:972-1034` | Modern 2D/HUD renderer at display resolution; keep the 640×480 ortho mapping semantics |
| `RB_StencilShadowPass` uses `qglStencil*` | `draw_common.cpp:1176-1231` | The adapter must implement the same stencil ops; do not silently drop shadows |
| `RB_SetDefaultGLState` / `qglDepthRange` in the depth hacks | `tr_render.cpp:176-220` | Modern depth-range state equivalent; x/y unchanged |

### 5.3 Proposed selection and dispatch

1. Add `r_renderApi` (archive, default `0`): `0` = GL reference, `1` = D3D12 classic path. Keep
   `r_renderer` for the GL sub-path; do not change its meaning. A test enumerates the accepted
   values and rejects others.
2. Add `R_RenderSystemRunning()` and resolve all 30 references per §5.1. The GL path maps it to
   `glConfig.isInitialized`; the modern path maps it to "device + surface + resources ready".
3. Add `R_InitRenderApi()` / `R_ShutdownRenderApi()` around the existing `R_InitOpenGL()` /
   `ShutdownOpenGL()`:
   - GL: call the existing functions unchanged.
   - D3D12: create the device/surface, load the RHI library, initialize the scene adapter, then set
     the running predicate and reload images through the backend-neutral path.
4. Keep `glConfig` for GL-only fields. Populate the fields the engine actually reads outside the GL
   backend (`vidWidth`, `vidHeight`, `isInitialized` via the predicate, `maxTextureSize`,
   `textureCompressionAvailable`, `depthBoundsTestAvailable`, `allowARB2Path`) from a small
   `renderConfig` or from the modern backend. Do not let a D3D12 path read an uninitialized
   `glConfig` field. The implementation PR must list every field it sets and every reader it leaves
   on the GL path; "the rest are GL-only" is not sufficient.
5. Dispatch the command stream at `RB_ExecuteBackEndCommands`:
   - keep the existing FIFO and `RC_*` order;
   - for `r_renderApi == 1`, route `RC_DRAW_VIEW` to the ordered scene adapter, `RC_SET_BUFFER` to
     the modern surface, `RC_COPY_RENDER` to the modern copy, and `RC_SWAP_BUFFERS` to the modern
     present;
   - do not call `RB_STD_DrawView` (which is GL) for the modern path;
   - preserve the mid-frame issue from `CaptureRenderToFile`.
6. Fallback (exact, testable):
   - A D3D12 initialization failure returns a typed, actionable result (adapter, missing feature,
     device removal, shader/pipeline failure) and **does not** modify `r_renderApi` or write a
     poisoned config.
   - The failed launch may exit to the console/error screen; it need not keep a live window. The
     requirement is that the **existing GL path remains selectable and usable**: a subsequent launch
     with `r_renderApi 0` renders the GL path.
   - If the process stays alive, `vid_restart` after setting `r_renderApi 0` may initialize GL, but
     that is an explicit user action, not an automatic switch. Do not claim automatic switching.
   - T036 tests the failed-launch diagnostic and the `r_renderApi 0` launch; T083 forces the failure
     in a real build.
7. `vid_restart`/shutdown must retire modern resources before recreating them. `R_VidRestart_f` must
   use the running predicate, not the GL-only early return, and call `ShutdownRenderApi` then
   `InitRenderApi`; it must not leave a stale surface or running predicate.

### 5.4 What the adapter consumes

The engine front end still produces `drawSurf_t`, `viewDef_t`, `viewLight_t`, materials and images.
The modern adapter must consume a **backend-neutral ordered stream**, not GL handles:

- view begin/end with projection, viewport/scissor, view identity and depth-hack policy;
- draw packets with `srfTriangles_t`/index data, material stage evaluation, light lists, and sort;
- set-buffer, copy, present;
- no `qgl*` calls in the adapter, and no `GLenum`/`idImage*` passed to the RHI.

The current command structs are engine types, not neutral ones: `setBufferCommand_t` carries a
`GLenum buffer` (`tr_local.h:487`) and `copyRenderCommand_t` carries an `idImage *image`
(`tr_local.h:499`). The adapter must **translate** these at the boundary:

| Legacy field | Neutral meaning | Mapping |
|---|---|---|
| `setBufferCommand_t.buffer` | color target selector | `GL_FRONT`/`GL_BACK` → the current/offscreen color target; reject an unknown enum |
| `copyRenderCommand_t.image` | destination image | resolve the engine `idImage` to the RHI texture registered for that image; no raw pointer |
| `drawSurf_t.sort` | order key | preserve as-is; do not re-sort in the adapter |
| `tri->ambientCache` | GL vertex-cache handle | **not** a portable GPU buffer; use `tri->verts`/indexes or the modern upload cache |

`tri->ambientCache` is a GL vertex-cache handle (`VertexCache.h`), not a portable GPU buffer. The
modern path must use the triangle/index data or its own upload cache; the GL cache remains for the GL
path. This is a Q2 production detail (T080), but the boundary must be stated now so the adapter does
not accidentally read a GL offset as a pointer.

The world-GUI / display-HUD classification and mid-frame copy order are part of the stream
(`GuiModel.cpp:226-297`; `tr_guisurf.cpp:128-181`; `RenderSystem.cpp:952-984`). See
`classic-parity.md` §4 for the expected order.

---

## 6. Command-stream contract

The modern backend must implement the same observable order as the GL backend:

1. **Front-end sort** by `drawSurf_t.sort` (`tr_main.cpp:1049-1074`), with the material/entity
   offsets already applied.
2. **One RC command list** in FIFO order (`tr_local.h:472-479`; `tr_backend.cpp:600-645`).
3. **Within a view**: depth fill → interactions → stencil reset → light scale → non-light passes →
   fog/blend → post-process → debug tools (`draw_common.cpp:1666-1725`).
4. **Within a light**: stencil clear if needed → global shadows → local interactions → local shadows
   → global interactions → translucent (`draw_arb2.cpp:243-323`).
5. **Within an interaction**: one draw per accumulated stage run, with the normalized
   bump/diffuse/specular order (`tr_render.cpp:689-842`; `Material.cpp:1742-1771`).
6. **Mid-frame copy/capture**: `CaptureRenderToFile` executes the queue and reads the back buffer;
   `CaptureRenderToImage` appends `RC_COPY_RENDER` without flushing (`RenderSystem.cpp:912-984`).
7. **Display HUD** is a separate 2D view at display resolution, after the scene
   (`GuiModel.cpp:226-297`); in-world GUI stays in the 3D view (`tr_guisurf.cpp:128-181`).

A backend that renders the right still but reorders these is a parity failure (see
`classic-parity.md` CP-28, CP-29, CP-30 and O-01–O-14).

---

## 7. Failure and rollback

| Failure | Required result | Evidence |
|---|---|---|
| No D3D12-capable adapter | Typed `unsupported`; GL path selectable at next launch | T036 test; T083 real build |
| Device creation fails | Actionable error with stage/adapter; no poisoned config; `r_renderApi` unchanged | T036 |
| Shader/pipeline creation fails | Report the program/key/stage and compiler diagnostic; no draw with a missing program | S2 resolver contract; T036 |
| Device removed mid-frame | Bounded failure, cleanup, actionable report; no fake success | Q1b/Q2 diagnostics |
| Modern init succeeds but surface resize/minimize fails | Report; keep GL selectable; do not silently switch | T036 |
| Rollback | Revert the superproject PR/submodule bump; the GL path is unchanged | PR body |

No secret, SDK binary, game data, or machine-specific path goes into a tracked file or log.

---

## 8. Verification plan

### 8.1 CMake probes (executed)

**Synthetic probe.** A minimal project proves the mechanics independent of the engine:

- a static `rhi` library and two executables that both link it;
- `CMAKE_MSVC_RUNTIME_LIBRARY` set once, inherited by the library, the object library and both
  executables;
- an object library consumed via `$<TARGET_OBJECTS:>`, plus an explicit link to the static library;
- generated projects inspected for `/MT` (Release) and `/MTd` (Debug), and the library link line.

**Target-structure probe.** A git-ignored copy of `targets/win11-amd64/CMakeLists.txt` with the
proposed insertion points, `NEO_DIR` pointed at the existing target copy, and a **stub** adapter that
calls `doom3::rhi::ToString(Status::Ok)`:

| Step | Result |
|---|---|
| Configure | exit 0 (VS 2022, Windows SDK 10.0.26100.0, MSVC 19.44) |
| Build `Release` | exit 0; `DOOM3.exe` and `DOOM3XP.exe` produced |
| Build `Debug` | exit 0; both executables produced |
| `DOOM3.exe` headers | `8664 machine (x64)` |
| `/MT` / `/MTd` | `MultiThreaded` / `MultiThreadedDebug` in both `.vcxproj` files |
| Link line | `doom3_rhi.lib;d3d12.lib;dxgi.lib` in both executables |
| Dependents | `d3d12.dll`, `dxgi.dll`, `KERNEL32.dll`; no `MSVCP140.dll`/`VCRUNTIME140.dll` |
| First attempt | **failed C2589** at `src/renderer/d3d12/rhi_d3d12.cpp:34` because the `max` macro broke `std::numeric_limits<uint64_t>::max()`; added `WIN32_LEAN_AND_MEAN NOMINMAX` and rebuilt green |

The probe proves the **build wiring** and CRT propagation for both executables with the actual
engine source tree. It does **not** prove the real adapter (the probe uses a stub), an engine call
site, a running game, or a D3D12 render. Its commands, exit codes and logs are recorded in
`specs/004-quality-renderer/evidence-q2-design.md`.

Neither probe is an engine-renderer integration or a Q2 result.

### 8.2 After implementation (T035/T036/T083)

1. Configure/build the real target in a fresh worktree-local directory; both `DOOM3.exe` and
   `DOOM3XP.exe` link, exit 0.
2. `dumpbin /headers` reports machine `0x8664`; `dumpbin /dependents` does not show a dynamic CRT for
   a `/MT` build; record the exact command.
3. `git submodule status` matches the fork SHA; `git -C doom3_orig/doom3 status --porcelain` is
   empty.
4. Run `DOOM3.exe` with legal demo data; select GL and D3D12; capture a scene with each; record
   adapter, driver, commands, exits, and the `r_renderApi` value.
5. Force D3D12 initialization failure; confirm the error is actionable, no automatic switch
   occurred, `r_renderApi` is unchanged, and a subsequent `r_renderApi 0` launch renders GL.
6. Re-export the target patch; a zero exit means an `M`-only manifest. An `A`/`D` row makes the
   exporter exit 1 and the delivery is via the fork; `SC-006b` is reported with its self-consistency
   caveat.
7. Repeat on `DOOM3XP.exe` (boot/menu) with the d3xp data if available; retail content is not
   claimed.

### 8.3 What is not evidence

- A configure that only prints targets.
- A static library that compiles but is not linked into both executables.
- A GL run that never selects the modern path.
- A `SC-006b` PASS without the T084 provenance check.
- A software adapter reported as hardware performance.
- A target-copy-only change that is not in the fork/submodule.
- The synthetic CMake probe as proof that the engine links.

---

## 9. File ownership and PR split

| PR | Owns | Must not touch |
|---|---|---|
| Superproject RHI | `src/renderer/rhi/**`, `src/renderer/d3d12/**`, `tests/renderer/**`, `specs/004-quality-renderer/**` (evidence) | `doom3_orig/doom3/**`, target copy |
| Superproject wiring | `targets/win11-amd64/CMakeLists.txt`, `sources.cmake` if regenerated, `CMakePresets.json` only if required | Engine source |
| Fork engine | `neo/renderer/**` dispatch/integration changes (`RenderSystem.cpp`, `tr_local.h`, `tr_backend.cpp`) | Superproject adapter/RHI files |
| Submodule bump | `doom3_orig/doom3` pointer only | Any other path |
| Target copy | Nothing committed | Everything |

One writer per file. Do not let two agents edit `RenderSystem.cpp`, `tr_local.h` or
`targets/win11-amd64/CMakeLists.txt` concurrently. Split T035 and T036 if their file sets overlap.

### 9.1 Recommended order

1. Land the RHI library + tests in the superproject (Q1a/Q1b), with the public header gate green.
2. Land the superproject ordered scene adapter (T035) against that header; then land the fork `RenderSystem.cpp` dispatch and open the fork PR.
3. Bump the submodule; add the wiring and `RHI_ADAPTER_SOURCES`; build both executables.
4. Add the selector/fallback test and the forced-failure integration check.
5. Re-export the patch and run the verifier; record the self-consistency caveat and the
   `evidence-t084.md` control result.
6. Update `classic-parity.md` and `evidence-q2.md` with the actual build/run identities.

The fork PR cannot be merged standalone if it depends on a superproject header that is not yet
merged; record that dependency in both PRs.

---

## 10. Risks and open items

| Risk | Impact | Mitigation |
|---|---|---|
| 30 `glConfig.isInitialized` references with different meanings | Modern backend renders nothing or fabricates state | Resolve every row in §5.1; separate frame, GUI, image, window and lifecycle meanings |
| `r_renderer` is GL-only | Overloading it breaks existing selection | Add `r_renderApi`; keep `r_renderer` semantics |
| `tri->ambientCache` is a GL handle | Wrong pointer in the adapter | Backend-neutral upload cache; no GL handle crossing |
| `/MT` mismatch with a prebuilt library | `LNK2038`/`LNK4098` or subtle CRT bugs | Rebuild from source; probe; no prebuilt `/MD` libs |
| Static library not linked into both EXEs | One executable lacks the backend | Append to `DOOM3_LINK_LIBS` before both calls; probe both |
| Static initializers dropped | Backend never registers | Explicit factory; no linker magic |
| Duplicate adapter TU | Link errors | One source owner; no duplicate in `ENGINE_SOURCES` |
| `sources.cmake` misses the adapter | Silent no-op build | `RHI_ADAPTER_SOURCES` in the tracked CMake |
| Patch self-consistency | False confidence | Manifest controls are in `evidence-t084.md`; fork/submodule for A/D; re-export after bump; a green `SC-006b` is not provenance |
| Fork and superproject diverge | Unbuildable PR | Record the fork SHA in the bump; test in a worktree from `origin/main` |
| `DOOM3XP` game objects differ | Link/run asymmetry | Shared engine objects; test both; no game-side RHI link |
| Target copy edited by mistake | Lost work | Target copy is scratch; restore from submodule; never commit |
| Machine-specific paths | Review/legal issue | Use relative paths/environment variables |

---

## 11. Traceability

| Requirement | Where this document addresses it | Evidence |
|---|---|---|
| FR-003 bounded backend-neutral RHI; no native handles in scene/material interfaces | §4, §5.4 | `contracts/rhi-foundation.md`; T035/T080 |
| FR-006 selectable classic path, retained GL fallback, actionable failed init | §5, §7 | T036 test; T083 evidence |
| FR-019 pristine submodule, fork/submodule bump, issue/worktree/review | §3 | `docs/GIT_WORKFLOW.md`; bump PR |
| SC-003 same scene on both paths | §6, §8.2 | `classic-parity.md`; planned `evidence-q2.md` (**not created yet**) |
| T033 output | this document | T033 review (pending); build-wiring probe `evidence-q2-design.md` §7.1 |

**Self-check**: this document contains no claim that the real adapter exists, the game runs, or a
path is delivered. The CMake wiring is marked proposed; the anchors are current `main`/pinned SHA.
The executed checks are the synthetic probe and the target-structure probe in §8.1, recorded in
`evidence-q2-design.md` §7/§7.1. The target-structure probe used a stub adapter and a git-ignored
copy of the target CMake; it proves build wiring, not engine rendering or a running game.
