# Q2 engine-adapter and classic-path evidence (T034, T035, T037, T038, T080–T082, T039)

Status: author evidence for issue #53, written 2026-10-30. This is **not** Q2 acceptance, not a
classic-parity result and not an independent review. It records what the merged superproject code
does, what was observed on this host, and what could not run. **Updated 2026-11-02:** the engine fork
(PR #1) merged and PR #113 bumps the submodule to `cd6ae660`; the target patch was re-exported and
the target was rebuilt from the bumped tree (§4.5). **Second pass:** after #115 merged at `b7c1a68`,
#113 was rebased onto it and the configure, build, Q2 and contract checks were rerun; both contract
gates are now green. **Third pass:** the same checks were rerun after rebasing onto `c63f57a`
(#118); the patch/manifest did not change. T083 remains NOT RUN. **Fourth pass (2026-11-02, branch `feat/q2-draw-texture-stencil`):** the T080–T082 superproject slices are delivered and device-tested; the fork call sites and every retail-data row remain NOT RUN. See §10. Q2 is still not accepted.

**Headline: the adapter, the material plan and the light plan exist and are exercised; no classic
frame was drawn and no GL-vs-D3D12 image comparison exists.** Every image row below is `NOT RUN` or
`SKIPPED` with a reason, and none of them counts as parity.

## 1. Scope and claim

| Item | Value |
|---|---|
| Stage and claim | Q2 classic path: ordered scene stream, material/light classification, overlay split. Claim: order and classification behave as the contracts require; **no** parity, capture, selector or fallback claim |
| Issue | #53 (implementation container; stays open) |
| Branch / base | `feat/q2-engine-adapter`, started from `main` at `2487baf` (S2 resolver wiring merged, P1 closed) |
| Submodule | Author run (2026-10-30) @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, `git -C doom3_orig/doom3 status --porcelain` empty; the Q2 adapter PR made no submodule change. **2026-11-02:** fork PR #1 merged and PR #113 bumps the gitlink to `cd6ae660f4b8de2cd9b7624017e17c89124e8ccb`; the target patch is re-exported against that tree |
| Data identity and rights | **None available.** No `.pk4`/`.pk3`/`.pak` exists in this worktree; the Q0 demo (`base/demo00.pk4`, SHA-256 `e373ec70…`) is not present. No game data is committed |
| Scene and inputs | Synthetic CPU fixtures only (`tests/renderer/q2-fixtures.md`); no scene, camera or frame was captured |
| Exclusions | GL capture, D3D12 capture, HUD metrics, stencil shadows, selector/fallback, engine call site |

## 2. Environment and configuration

| Item | Value |
|---|---|
| OS / arch | Windows 10.0.22631, x64 |
| Compiler | MSVC 19.44.35228.0 (VS 2022 Professional, toolset v143), Windows SDK 10.0.26100.0 |
| Build tool | CMake 3.31.6-msvc6 (VS-bundled), generator `Visual Studio 17 2022 -A x64` |
| Runtime library | `/MT` Release and `/MTd` Debug (directory-level `CMAKE_MSVC_RUNTIME_LIBRARY`) |
| D3D12 backend | Present and exercised by the Q1b ctest fixtures on this host; adapter uses no device |
| MFC / ATL | **MFC is not installed.** `atlmfc/include` holds ATL headers only; no `afxwin.h` exists anywhere under `Program Files\Microsoft Visual Studio`. The delivered tree does not reach it: the tracked target patch deletes the `tools/comafx/StdAfx.h` include from `neo/idlib/precompiled.h` (the active `#else` branch of the `#if 0` at line 47) and replaces `#include <afxwin.h>` with `<windows.h>` in `neo/sys/win32/win_taskkeyhook.cpp` |
| Target copy | Created with `scripts/new-target.ps1 -Name win11-amd64`, then **patch-applied** with `scripts/apply-target-patches.ps1 -Name win11-amd64`, then built. The 2026-10-30 run applied 72 files; after the 2026-11-02 re-export against `cd6ae660` the same flow applies and verifies **74 files, 0 mismatches** (§4.5). No engine source is edited by hand |

**Correction to an earlier revision of this file.** A first run built an *unpatched* `new-target`
copy and reported 353 × `C1083 afxwin.h`. That run describes the pristine copy, **not** the delivered
tree, and its conclusion ("every engine translation unit needs MFC") was wrong. The pristine tree
reaches MFC through the active `#else` branch at `neo/idlib/precompiled.h:59-61`; the patch deletes
that include, so the delivered tree links both executables (§4.3). The pristine run is kept below
only as a labelled control.

The patch also adds `&& !defined( ID_NO_TOOLS )` to the `ID_ALLOW_TOOLS` condition in
`neo/framework/BuildDefines.h:95`. `ID_NO_TOOLS` **is** defined today, at
`targets/win11-amd64/CMakeLists.txt:82`, so that guard is active: it removes the tools/editor code
path. It is **not** what removes the MFC include — the two deletions named above are — and it would be
wrong to call the guard inert.

### 2.1 Q1 residual (disclosure, not a pass)

The Q1b fixtures are merged (`f1e4feb`) and ran here as part of the 26/26 ctest, but the debug layer
and GPU-based validation still do **not** execute on this host: the diagnostics fixture reports
`D3D12GetDebugInterface failed (hr=0x887A002D)` and `validation_executed=0`, exactly as recorded in
`evidence-q1.md` T030/T031. The owner accepted that residual on 2026-10-29 as a recorded gap; that
acceptance does not turn a skip into a pass. Nothing in this file depends on Q1 being green, and no
statement here implies that it is.

## 3. What was implemented

| Task | File | Behaviour |
|---|---|---|
| T035/T080 | `src/renderer/rhi/engine_scene_adapter.{h,cpp}` | FIFO queue over `BeginView / DrawPacket / SetColorTarget / CopyToImage / SwapBuffers`; mid-frame split; view identity trace; overlay split; boundary rejection of unknown selectors and unbalanced streams |
| T081 | `src/renderer/rhi/materials.{h,cpp}` | canonical bump → diffuse → specular plan, coverage class, translucent/decal/post-process split, megatexture refusal, resolver-gated refusal |
| T082 | `src/renderer/rhi/lighting.h`, `src/renderer/d3d12/lighting.cpp` | ambient/interaction/fog/blend pass classification, multi-frustum flag, global/local shadow chains and the three suppressions |
| T034 | `tests/renderer/engine_command_order_test.cpp` | order, flush, append and rejection fixtures |
| T037 | `tests/renderer/classic_compatibility_test.cpp` | material, light, shadow and depth-hack fixtures; reads the shipping manifest through the merged resolver |
| T038 | `tests/renderer/classic_gui_test.cpp` | overlay view mapping, HUD-after-scene, world-GUI-in-view, target stability |
| — | `targets/win11-amd64/CMakeLists.txt` | `doom3_rhi` static library, explicit source list with a fail-closed `EXISTS` guard, link into both executables |

The merged spec 008 resolver (`src/renderer/shader/shader_resolver.{h,cpp}`) is consumed, not
modified. `shaders/`, `src/renderer/shader/engine/` and every S2 file are untouched.

### 3.1 Boundary rule

`setBufferCommand_t.buffer` arrives as a plain `uint32_t` and is accepted only as `0x0404`
(current) or `0x0405` (offscreen); `copyRenderCommand_t.image` arrives as a registered numeric
identity and `0` is refused; `tri->ambientCache` never appears. No GL enum, `idImage*` or
vertex-cache handle is named in `src/renderer/rhi/**` (`validate-rhi-contract.py` passes).

### 3.2 What this slice deliberately does not do

The Q1b portable seam composes a colour frame but accepts **no vertex or index data**
(`DrawFrameDesc` has passes and tints, not geometry). A draw packet can therefore be extracted,
counted, classified and refused, and cannot be submitted. `SceneExecuteResult::drawSubmissionUnavailable`
is set whenever a packet was extracted so an extraction run cannot be read as a rendered frame. This
is the T080 blocker in §7, not a hidden shortcut.

## 4. Commands and exits

All commands ran from the worktree root. Logs are worktree-local and git-ignored under `build-q2/`
and `build/`; the paths below are the retained references.

| # | Command | Exit | Reference |
|---|---|---|---|
| 1 | `cmake -S tests/renderer -B build-q2 -G "Visual Studio 17 2022" -A x64` | 0 | — |
| 2 | build the three fixtures against the **stub** implementations | 0 | `build-q2/_red-build.log` |
| 3 | `q2_engine_command_order_test.exe` (stub) | **1** | `build-q2/red-q2_engine_command_order_test.log` |
| 4 | `q2_classic_compatibility_test.exe` (stub) | **1** | `build-q2/red-q2_classic_compatibility_test.log` |
| 5 | `q2_classic_gui_test.exe` (stub) | **1** | `build-q2/red-q2_classic_gui_test.log` |
| 6 | build the real implementation | 0 | `build-q2/_green2-build.log` |
| 7 | the same three executables (real) | **0 / 0 / 0** | `build-q2/green-*.log` |
| 8 | `ctest --test-dir build-q2 -C Release --no-tests=error --output-on-failure` | 0, **26/26** | `build-q2/ctest-Release.log` |
| 9 | `cmake --build build-q2 --config Debug -- -m` then `ctest -C Debug` | 0, **26/26** | `build-q2/ctest-Debug.log` |
| 10 | sensitivity: swap the bump/diffuse canonical order, rebuild, run T037 | **1** (`ASSERTION FAILED: bump is first`) | `build-q2/sens-classic.log` |
| 11 | restore, rebuild, run T037 | 0 | `build-q2/restore-classic.log` |
| 12 | `scripts/apply-target-patches.ps1 -Name win11-amd64` | 0, `72 files verified, 0 mismatches` | delivered tree setup |
| 13 | `cmake --preset win11-amd64` | 0 | `build-configure.log` |
| 14 | `cmake --build --preset win11-amd64-release -- -m` | **0** | `build/_build.log`; both executables produced (§4.3) |
| 15 | `dumpbin /headers` + `/dependents` on both executables | 0 | §4.3 |
| 16 | guard: rename the adapter, configure | **1**, `Portable RHI source is missing` | `build-guard.log` |
| 17 | control: build the unpatched `new-target` copy | **1**, 353 × `C1083 afxwin.h` | `build/_build.log` retained only as the §4.4 control |
| 18 | `py -3 -I scripts/validate-rhi-contract.py` | 0 | header scan over 11 headers, 26 public test files |
| 19 | `py -3 -I scripts/validate-webgpu-mapping.py` | **1**, 62 problems | `build-q2/webgpu-mapping.log` (pre-existing, #36/T089) |

### 4.1 Red evidence (recorded before the implementation)

```
EXEC FIFO failed: stub: ordered execution is not implemented
ASSERTION FAILED: three stage runs: expected 3, observed 0
ASSERTION FAILED: display overlay is flagged as an overlay
```

Each executable exited 1 and linked successfully, so the red is a behavioural failure, not a link
failure.

### 4.2 Green evidence

```
ORDER fifo: BeginView > DrawPacket > SetColorTarget > CopyToImage > SwapBuffers
ORDER capture-to-file prefix: BeginView > DrawPacket > CopyToImage
ORDER remainder after flush: DrawPacket
ORDER capture-to-image: BeginView > DrawPacket > CopyToImage > DrawPacket
DISCLOSURE structural mode: extracted=1 submitted=0 refused=1

MATERIAL order: bump > diffuse > specular
MATERIAL resolver refusal: classic program refused by the resolver gate (not owned)
SHADOW chains: global+local, local-only for self, explicit suppressions

OVERLAY 640x480 ortho mapped onto 1280x720 display
CONTROL overlay-before-scene is rejected
WORLD GUI stays in the 3D view
```

### 4.3 Engine build (delivered tree)

Sequence, all from the worktree root:

| Command | Exit | Result |
|---|---|---|
| `scripts/apply-target-patches.ps1 -Name win11-amd64` | 0 | `Applied patch: 72 files verified, 0 mismatches` |
| `cmake --preset win11-amd64` | 0 | configures `build/` |
| `cmake --build --preset win11-amd64-release -- -m` | **0** | builds `doom3_rhi.lib`, `doom3_engine.lib`, `DOOM3.exe`, `DOOM3XP.exe` |

Artefacts (worktree-local, git-ignored):

| Artefact | Size | `dumpbin /headers` | `dumpbin /dependents` |
|---|---|---|---|
| `build/bin/Release/DOOM3.exe` | 17,304,576 B | `8664 machine (x64)`, subsystem `Windows GUI` | no `MSVCP140.dll`, no `VCRUNTIME140.dll` (static `/MT`) |
| `build/bin/Release/DOOM3XP.exe` | 17,488,384 B | `8664 machine (x64)`, subsystem `Windows GUI` | no `MSVCP140.dll`, no `VCRUNTIME140.dll` (static `/MT`) |
| `build/targets/win11-amd64/Release/doom3_rhi.lib` | 1,217,792 B | — | — |

**`d3d12.dll` and `dxgi.dll` do not appear in the dependents list, and that is reported rather than
hidden.** `doom3_rhi` is on both link lines through `DOOM3_LINK_LIBS`, but no engine code calls into
it yet (the T036 call site does not exist), so the linker pulls no object from the static library and
its imports never reach the executable. The issue's dependents row is therefore **PARTIAL**: static
CRT and x64 hold, the D3D12 imports do not, and they will only appear when an engine call site
references the adapter. Forcing a dummy reference to satisfy the checklist would be link-line
theatre, not a delivered path.

### 4.4 Control: unpatched `new-target` copy

An earlier run built a copy created only by `scripts/new-target.ps1` (no `apply-target-patches`) and
failed with **353 × `error C1083: cannot open include file: afxwin.h`** in `doom3_engine.vcxproj` /
`doom3_game_base.vcxproj`. That is the **pristine copy**, not the delivered tree: without the patch the
active `#else` branch at `neo/idlib/precompiled.h:59-61` includes `tools/comafx/StdAfx.h`, which
includes `<afxwin.h>` at line 38, and MFC is not installed here. The patch deletes that include (and
the one in `win_taskkeyhook.cpp`), which is what removes the dependency. Recording this run as a
delivered-tree limitation was a defect in the previous revision of this file; it is corrected in §2.

### 4.5 Re-export and rebuild against the bumped engine tree (2026-11-02)

The fork merge (fork PR #1, engine tree `cd6ae660`) changed 10 engine files, four of which the
target patch also modified (`GuiModel.cpp`, `RenderSystem.cpp`, `RenderSystem_init.cpp`,
`tr_local.h`). The old patch did not apply: `R_InitCommands` now contains `renderapi_status` at
`RenderSystem_init.cpp:2278` between the function opening and `MakeMegaTexture`. The target tree was
reconstructed as a three-way merge (port edits + fork edits) and re-exported; the only textual
conflict was the `R_InitCommands` insertion point, resolved by keeping both command sets.

| # | Command | Exit | Result |
|---|---|---|---|
| 20 | `git rebase origin/main` on the PR #113 head | 0 | onto `a2b4613`, then `b7c1a68` after #115, then `c63f57a` after #118; all rebases clean, no conflicts |
| 21 | `git -C doom3_orig/doom3 checkout cd6ae660…`; `git status --porcelain` | 0 | gitlink at `cd6ae660`, submodule tree clean |
| 22 | `scripts/export-target-patches.ps1 -Name win11-amd64` | 0 | `Modified 74, Added 0, Removed 0`; patch 768,242 B; manifest 74 M rows |
| 23 | fresh `new-target` then `scripts/apply-target-patches.ps1 -Name win11-amd64` | 0 | `Applied patch: 74 files verified, 0 mismatches` |
| 24 | re-run `apply-target-patches.ps1` on the built tree | 0 | `Target already patched (74 files match)` |
| 25 | `cmake --preset win11-amd64` | 0 | configures `build/` |
| 26 | clean `cmake --build --preset win11-amd64-release -- -m` | **0** | 12,875 log lines, **0** `error C`/`error LNK`/`error MSB` lines |
| 27 | `dumpbin /headers` + `/dependents` on `DOOM3.exe` | 0 | `8664 machine (x64)`, `Windows GUI`, no `VCRUNTIME140`/`MSVCP140`; **no** `d3d12.dll`/`dxgi.dll` |
| 28 | ASCII scan of `build/bin/Release/DOOM3.exe` | — | `renderapi_status`, `r_renderApi` and `D3D12 classic path` are present in the binary |
| 29 | `ctest -R "renderer\.q2\." -C Release` | 0 | **3/3** (`command_order`, `classic_compatibility`, `classic_gui`) |
| 30 | full `ctest -C Release` | 8 | **37/38**; only `rhi.q1a.diagnostics` fails with the known `D3D12GetDebugInterface failed (hr=0x887A002D)` Q1a residual (§2.1) |
| 31 | `scripts/test-target-patches.ps1` | 0 | **39 passed, 0 failed** |
| 32 | `py -3 -I scripts/validate-rhi-contract.py` | 0 | **PASS** after #115 (`b7c1a68`): structure and lexical gate over 25 headers and 30 public test files |
| 33 | `py -3 -I scripts/validate-webgpu-mapping.py` | 0 | **OK**: 119/119 public declarations classified; statuses, check rows and justifications valid |

Artefacts (worktree-local, git-ignored): `DOOM3.exe` 17,316,352 B and `DOOM3XP.exe`
17,499,648 B in `build/bin/Release/`. The D3D12 imports are still absent because no engine call site
installs the scene sink, so the B4 acceptance row remains PARTIAL, not PASS.

## 5. Negative controls

| Control | Expected | Observed |
|---|---|---|
| Reorder `BeginView/DrawPacket/SwapBuffers` | comparator rejects | rejected (`CONTROL comparator rejects a reordered stream`) |
| Overlay drawn before the scene | view-identity order rejects | rejected (`CONTROL overlay-before-scene is rejected`) |
| Unknown set-buffer value `0x1234` | `Unsupported` with an actionable error | rejected |
| Copy with destination identity 0 | refused at the queue boundary | refused |
| Unbalanced `EndView` | `InvalidArgument`, nothing consumed | rejected |
| Megatexture material | refused, zero stages | refused |
| Projected light with zero falloff | `InvalidArgument` | rejected |
| Supported material / plain light | **not** refused | not refused |
| Deliberate stage-order mutation | T037 fails | `ASSERTION FAILED: bump is first`; restored run passes |

## 6. Per-check results

Legend: **Classification** is an executed CPU check of the plan/trace; **Structural (T-07)** is exact
equality of an engine-produced command/stage/chain trace; **Image** is a GL-vs-new-path comparison.
A classification PASS is not structural evidence and neither is parity.

| CP | Case | Classification | Structural (T-07) | Image | Reason / observation |
|---|---|---|---|---|---|
| CP-01 | Opaque depth prepass and equal-depth colour | NOT RUN | NOT RUN | NOT RUN | No geometry submission, no data |
| CP-02 | Interaction stage order and accumulation | **PASS** (bump→diffuse→specular) | NOT RUN | NOT RUN | Plan order fixed; no engine trace |
| CP-03 | Alpha-tested / perforated coverage | **PASS** (coverage class) | NOT RUN | NOT RUN | Mask comparison needs a capture |
| CP-04 | Projected light cookie and falloff | **PASS** (classification) | NOT RUN | NOT RUN | Cookie sampling not implemented |
| CP-05 | Point light multi-frustum falloff | **PASS** (flag) | NOT RUN | NOT RUN | Frusta are not submitted |
| CP-06 | Ambient light path | **PASS** (separate pass) | NOT RUN | NOT RUN | No ambient draw |
| CP-07 | Stencil shadow volume | NOT RUN | NOT RUN | NOT RUN | Stencil path is backend work, not in this slice |
| CP-08 | Global vs local shadow chains | **PASS** (chain plan) | NOT RUN | NOT RUN | Membership only; volumes not rendered |
| CP-09 | `noShadows` material/light | **PASS** (suppression) | NOT RUN | NOT RUN | — |
| CP-10 | Translucent interactions | **PASS** (translucent class) | NOT RUN | NOT RUN | Blend order not executed |
| CP-11 | Fog light | **PASS** (separate pass) | NOT RUN | NOT RUN | — |
| CP-12 | Blend light | **PASS** (separate pass) | NOT RUN | NOT RUN | — |
| CP-13 | Light scale / overbright | NOT RUN | NOT RUN | NOT RUN | No value is invented; needs a capture |
| CP-14 | Mirror / subview | NOT RUN | NOT RUN | NOT RUN | Needs an engine stream |
| CP-15 | Portal sky | NOT RUN | NOT RUN | NOT RUN | Needs an engine stream |
| CP-16 | Xray subview | NOT RUN | NOT RUN | NOT RUN | Needs an engine stream |
| CP-17 | World GUI (in-world screen) | **PASS** (stays in 3D view) | NOT RUN | NOT RUN | No GUI content drawn |
| CP-18 | Display HUD / fullscreen GUI | **PASS** (separate overlay view) | NOT RUN | NOT RUN | T-05/T-09 pending |
| CP-19 | Viewmodel + `weaponDepthHack` | **PASS** (projection z) | NOT RUN | NOT RUN | x/y NDC unchanged |
| CP-20 | Model depth hack | **PASS** (projection z) | NOT RUN | NOT RUN | — |
| CP-21 | Post-process `_currentRender` | **PASS** (class + sort 100) | NOT RUN | NOT RUN | Copy point not executed |
| CP-22 | Skybox / wobble sky texgen | **PASS** (stage class) | NOT RUN | NOT RUN | Texgen not implemented |
| CP-23 | Glasswarp texgen | **PASS** (stage class) | NOT RUN | NOT RUN | — |
| CP-24 | MegaTexture | **UNSUPPORTED** (refused, reported) | — | — | Expected unsupported case; reviewer decision required |
| CP-25 | Decals and overlays | **PASS** (decal class) | NOT RUN | NOT RUN | Polygon offset not executed |
| CP-26 | Particles, beams, sprites | NOT RUN | NOT RUN | NOT RUN | Not modelled in this slice |
| CP-27 | Cinematics | NOT RUN | NOT RUN | NOT RUN | SKIP candidate; no data |
| CP-28 | Front-end sort order | **PASS** (sort key preserved) | NOT RUN | NOT RUN | No engine sort trace |
| CP-29 | Remote / security view | NOT RUN | NOT RUN | NOT RUN | Needs an engine stream |
| CP-30 | Non-interaction ambient stage order | **PASS** (plan) | NOT RUN | NOT RUN | — |

Every `NOT RUN` row is `NOT RUN`, not a pass. No threshold was chosen or changed; T-01…T-04 remain
the frozen Q0 ceilings, and T-05/T-06/T-08/T-09 remain unmeasured.

## 7. Blockers (all open)

| # | Blocker | Effect | Owner |
|---|---|---|---|
| B1 | No legal demo data on this host (no `.pk4` in the worktree) | Every image row is `NOT RUN`; stop condition S13 | Data provisioning |
| B2 | The shipping manifest has **0 `owned`** programs (6 `stub`, 1 `unsupported`, 7 `absent`) | The draw gate refuses every classic material by design; a classic frame cannot be drawn even with data (stop condition S1 remains open) | Spec 008 / T-027 |
| B3 | The portable seam accepts no vertex/index geometry | T080 world draw submission is extraction-only; `drawSubmissionUnavailable` is set | New Q1b-extension task |
| B4 | No engine call site yet (T036), so the linker pulls nothing from `doom3_rhi` | Both executables link, but `d3d12.dll`/`dxgi.dll` are absent from `dumpbin /dependents`; that acceptance row is PARTIAL, not PASS | Q2 fork work (B5) |
| B5 | T036 engine-side selector/typed failure landed in fork PR #1 and is present in the bumped tree (`renderapi_status` and `r_renderApi` are compiled into `DOOM3.exe`, §4.5). The portable sink install (`R_SetQ2SceneSink`) and `RC_*` dispatch call site are still absent | `r_renderApi 1` reaches the typed "no scene consumer" failure; `d3d12.dll`/`dxgi.dll` remain absent from `dumpbin /dependents` (B4 stays PARTIAL); T083 cannot run without legal data | Q2 port integration |
| B6 | ~~`validate-webgpu-mapping.py` exits 1 with 62 problems on current `main`~~ | **Resolved by #115 (`b7c1a68`)**: `validate-webgpu-mapping.py` now exits 0 with 119/119 declarations classified, and `validate-rhi-contract.py` also exits 0 | #36 / T089 (closed) |
| B7 | Build-system change (`targets/win11-amd64/CMakeLists.txt`, resolved 2026-10-30) | Reviewer reran the build and tests and recorded the evidence (§4.3); no owner sign-off under `AGENTS.md` §11 | Reviewer |

Stop conditions triggered or relevant: **S13** (no legal data → SKIPPED), **S3** (mapping gate red,
recorded; no capture attempted), **S1** still open (no `owned` program), **S8/S12** not triggered (no
fork change, no second writer on `engine_scene_adapter.cpp`), **S10** not triggered (boundary clean).

## 8. T083 forced failure

**NOT RUN.** The selector now exists in the rebuilt binary (`renderapi_status` and `r_renderApi` are
present, §4.5), but there is still no legal game data in this worktree to boot, so the forced-failure
launch, the unchanged-cvar observation and the `r_renderApi 0` GL launch were not executed. The
failed-initialization *unit* test required by T036 is also absent from `tests/renderer`. Nothing here
claims a typed-error observation.

## 9. Independent review

**NOT RUN.** T040 requires a reviewer that is not the author, with the reviewed SHA recorded
verbatim in `review-q2.md`. Nothing in this file is a review.

## 10. T080–T082 production-slice evidence (2026-11-02)

Status: author evidence on branch `feat/q2-draw-texture-stencil` (worktree
`feat-q2-draw-texture-stencil-c7b90452a8c513d7`), based on `main` at `32e9bd7`. This is **not** Q2
acceptance, not classic parity and not an independent review. It records what the superproject RHI
now does and what ran on this host. The engine-fork half of each task and every retail-data row
remain **NOT RUN / BLOCKED**, and no statement here implies otherwise.

### 10.1 What changed

| Task | File | Behaviour now proved |
|---|---|---|
| T080 | `src/renderer/rhi/rhi.h`, `src/renderer/d3d12/rhi_attachments.cpp`, `src/renderer/d3d12/rhi_pipeline.cpp` | `CreateBuffer` (vertex/index), `DrawItem` with per-item transform/tint/stencil reference, vertex-input graphics pipelines, ordered indexed and non-indexed draws. A frame can now submit real geometry instead of only a procedural fullscreen triangle. |
| T080 | `src/renderer/rhi/engine_scene_adapter.{h,cpp}` | World surfaces are extracted *and submitted* in stream order through a `SceneGeometryBackend`; `drawSubmissionUnavailable` is false when the frame was submitted and `drawsSubmitted` counts it. `Device` is not named by the CPU adapter build; the backend interface keeps the structural tests free of the native library. |
| T081 | `rhi.h`, `rhi_attachments.cpp`, `rhi_pipeline.cpp` | `Device::CreateTexture` uploads RGBA8/RGBA8-sRGB/R8 and the Q4 pins BC4_UNORM, BC5_UNORM and BC7_UNORM_SRGB, with a validated full mip chain; a draw item binds one SRV through a per-submission shader-visible heap; the static point sampler is declared in the root signature. |
| T081 | `src/renderer/rhi/materials.{h,cpp}` | `BindMaterialTextures` maps bump/diffuse/specular to uploaded textures and returns `InvalidArgument` for a declared map with no texture instead of sampling an implicit white/black substitute. |
| T082 | `rhi.h`, `rhi_pipeline.cpp`, `lighting.{h,cpp}` | Front/back stencil face states, per-item `OMSetStencilRef`, stencil-enabled PSOs using a D24 depth-stencil attachment, stencil-only passes with zero colour targets, and `PlanShadowVolume`: back-increment/front-decrement pairs, no volume for a suppressed light or ambient/fog/blend pass. `depthClampUsed = false` and `requiresClosedVolume = true` are recorded rather than assumed. |

No target `CMakeLists.txt`, submodule, Q4 harness, Q5/Q6/Q7 file or engine source was changed.

### 10.2 Commands and exits

All commands ran in this worktree on 2026-11-02. Logs are in the git-ignored `build-q2/` and
`build/` directories; the machine-readable exit is what is recorded.

| Command | Exit | Observed |
|---|---|---|
| `cmake -S tests/renderer -B build-q2 -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 |
| `cmake --build build-q2 --config Release -- -m` | 0 | every renderer test executable built |
| `ctest -C Release --output-on-failure` (in `build-q2`) | 0 | **42/42 passed**, including `renderer.q2.draw_geometry`, `texture_sampling`, `stencil_shadow`, `scene_submit` |
| `cmake --build build-q2 --config Debug -- -m` | 0 | every Debug executable built |
| `ctest -C Debug --output-on-failure` | 0 | **42/42 passed** |
| `q2_gpu_test.exe geometry` | 0 | hardware PASS, software PASS (`SUMMARY pass=2 fail=0 skip=0`) |
| `q2_gpu_test.exe texture` | 0 | hardware PASS, software PASS |
| `q2_gpu_test.exe stencil` | 0 | hardware PASS, software PASS |
| `q2_gpu_test.exe scene` | 0 | hardware PASS, software PASS |
| `cmake --preset win11-amd64` | 0 | `new-target` + `apply-target-patches` exit 0, configure done |
| `cmake --build --preset win11-amd64-release -- -m` | 0 | `doom3_rhi.lib`, `DOOM3.exe` and `DOOM3XP.exe` linked |
| `py -3 scripts/validate-rhi-contract.py` | 0 | 25 headers / 32 public test files scanned, PASS |

Host adapters observed by the fixtures: `Intel(R) Graphics` (vendor `0x8086`, device `0x7D67`) as
hardware index 0, and `Microsoft Basic Render Driver` (WARP). The AMD RX 9070 XT named for the host
is not adapter 0 in this run and was not separately selected; no NVIDIA SDK, driver extension or
vendor-specific path is used.

### 10.3 Synthetic offscreen frames

Each frame is the 8x8 readback of an offscreen draw, upscaled 16x by the test helper. They are
synthetic evidence of the RHI path, not a classic scene and not parity.

| Frame | Proves |
|---|---|
| `evidence/q2-t080-draw-order.png` | green drawn after red wins the left half; the reversed submission flips the winner |
| `evidence/q2-t081-texture-sampling.png` | a 2x2 RGBA8 texture samples as four exact quadrants through a point sampler |
| `evidence/q2-t082-stencil-shadow.png` | CP-07 synthetic: back-only columns keep the clear colour (stencil 1), the overlap and the outside are lit (stencil 0); swapped ops flip it |
| `evidence/q2-t082-scene-surfaces.png` | the scene adapter submits two ordered world surfaces (green left-half over red full-screen) |
| `evidence/q2-t082-scene-light.png` | the adapter submits the light frame after the surfaces; one casting light shades, and the `noShadows` packet adds no stencil work |

### 10.4 Exact expectations, stated before the readback

- RGBA8: red `(255,0,0)`, green `(0,255,0)`, blue `(0,0,255)`, white `(255,255,255)` at the four
  quadrant centres of the 8x8 target.
- sRGB: `128/255` decodes to `0.21586` linear = `55/255`; the UNORM control stays `128`.
- BC4: an all-zero-index block with endpoints `(0,255)` samples `0`; with `(255,0)` it samples `255`.
- BC5: red block endpoint `255`, green block endpoint `0` samples `(255,0,0)`.
- BC7: a uniform `0xFF` block is the fixed test vector and samples opaque white. This proves format
  acceptance, mip-chain handling and sampling; it does **not** prove arbitrary BC7 texel decoding,
  because no block encoder is part of this slice.
- Material binding: a diffuse stage with `hasDiffuseMap` and no uploaded texture is `InvalidArgument`.

### 10.5 Blocked halves and limits

1. **Engine fork.** T080/T081/T082 still require the reviewed fork call sites
   (`engine-fork:neo/renderer/RenderSystem.cpp`, material/image upload and the interaction path). The
   fork was not edited; the submodule gitlink is unchanged. Those halves are **BLOCKED** and cannot be
   claimed from this superproject PR.
2. **Retail data.** There is still no `.pk4`/`.pk3`/`.pak` in the worktree, so no CP-01…CP-30 image row,
   no T083 forced-failure launch and no classic frame were produced. Every such row remains `NOT RUN`
   or `SKIPPED`.
3. **One chain per light packet.** A packet carries one shadow chain; the adapter returns
   `Unsupported` for a two-chain light instead of drawing only the first, and the engine is expected
   to emit one packet per chain. That is a contract decision, not a tested engine behaviour.
4. **Depth sampling + stencil.** A frame may not mix a sampled depth SRV with per-item texture views,
   and depth sampling in a stencil frame is refused; neither combination is needed by this slice.
5. **Hardware coverage.** Both the Intel adapter and WARP pass; the AMD adapter was not selected. No
   statement here generalises beyond the adapters named in §10.2.

## 11. What would make this claim stronger

1. ~~Link both executables and record the `dumpbin` rows.~~ **Done** in §4.3; the remaining gap is the
   `d3d12.dll`/`dxgi.dll` dependents row, which needs an engine call site (B4/B5), not a build change.
2. Extend the seam with vertex/index submission so a draw packet can actually be submitted, then
   re-run CP-01…CP-30 structurally against an engine-produced stream.
3. Provision the Q0 demo data and measure T-05/T-06/T-08/T-09 with a GL-vs-GL control **before** any
   comparison.
4. Land T036 and T083 with the fork PR and submodule bump.



## 12. T080–T082 engine-fork call sites (2026-11-02)

Status: author evidence for the fork half on branch `spec-004-q2-q3-engine`, merged to fork `master`
at `c87e87b` (`https://github.com/yhyu13/DOOM-3/pull/2`). This section **corrects** §10.5's
"fork was not edited" statement for that branch only. It is not Q2 acceptance, not classic parity and
not an independent review.

### 12.1 What the fork now emits

| Task | Fork file | Behaviour |
|---|---|---|
| T080 | `neo/renderer/tr_backend.cpp`, `RenderApi.h` | `surfacePacket` precedes `drawPacket` for the same id with the `srfTriangles_t` vertex/index snapshot (offset/stride layout), the model-to-world transform and a 2- or 4-byte index size. The consumer stores it and `SceneGeometrySource::ResolveSurface` returns it. |
| T081 | `neo/renderer/tr_backend.cpp`, `Image_load.cpp`, `Image.h` | `q2MaterialFacts_t` carries up to eight stage image identities and the image's filter/repeat. `GenerateImage` forwards tightly packed RGBA8 bytes and the sampler metadata before the GL-only `glConfig.isInitialized` early return, so the D3D12 path receives the texture without a GL upload. `R_Q2ImageIdentity` is one namespace shared with `CopyRender`. |
| T082 | `neo/renderer/tr_backend.cpp` | One `lightPacket` per global/local shadow chain instead of one aggregate packet. `chainKind`, the chain surface count, `noSelfShadow`/`noShadows` facts and an explicit exclusion kind travel with the packet. Ambient/fog/blend and noShadows/no-chain lights emit one excluded packet and no volume. |
| T044 | `neo/renderer/Model_md5.cpp` | A private `(viewId, stable surface id)` store with a one-frame previous copy and a generation that advances only when the shared surface is regenerated. `UpdateSurface` commits after the posed vertices exist; the copy owns its memory, so freeing the shared cache does not invalidate it. |

The superproject consumer mirrors these values and now executes a synthetic frame through them. It
still installs no device backend in `DOOM3.exe`; `R_InitRenderApi` refuses the modern path with the
typed no-consumer message, so the fork call sites are compiled, linked and dormant until a consumer
with device pipelines exists.

### 12.2 Commands and exits

| Command | Exit | Observed |
|---|---:|---|
| `git -C <fork> push -u origin spec-004-q2-q3-engine` | 0 | branch `99ef55a`; fork PR https://github.com/yhyu13/DOOM-3/pull/2 later merged to `master` at `c87e87b` |
| `cmake --build --preset win11-amd64-release -- -m` (target copy with the fork diff) | 0 | `DOOM3.exe` and `DOOM3XP.exe` linked; no `error C`/`error LNK` |
| `scripts/export-target-patches.ps1 -Name win11-amd64` (submodule at `c87e87b`) | 0 | `Modified 74, Added 0, Removed 0`; patch 768,242 bytes |
| `scripts/new-target.ps1 -Name win11-amd64 -Force` | 0 | 2,157 files / 33.8 MB copied from `c87e87b` |
| `scripts/apply-target-patches.ps1 -Name win11-amd64` | 0 | `Applied patch: 74 files verified, 0 mismatches` |
| `cmake --preset win11-amd64` | 0 | configure done after the reconstruction |
| `cmake --build --preset win11-amd64-release -- -m` | 0 | `DOOM3.exe` 17,605,120 bytes; `DOOM3XP.exe` 17,788,416 bytes |
| `dumpbin /symbols ...\doom3_engine.lib` | 0 | `R_Q2ImageIdentity`, `R_Q2EmitImageUpload`, `R_Q3PreviousPrivatePose`, `R_Q3ResetPrivatePoses` defined in the engine object library |
| `ctest --test-dir build-q2 -C Release` | 0 | **45/45 passed** |
| `ctest --test-dir build-q2 -C Debug` | 0 | **45/45 passed** |
| `q2_fork_seam_test.exe` | 0 | hardware and software adapters: `FORK PASS`; `SUMMARY pass=2 fail=0 skip=0` |

The new `renderer.q2.fork_seam` fixture drives the extended callback table with synthetic content:
`surfacePacket` (4 vertices / 12 indices), a 2×2 RGBA8 upload, one local shadow chain, one excluded
light and a private pose. It records `draws extracted=1 submitted=1 refused=0 lights=2 excluded=1
passes=3 submissions=2 unavailable=0`; the surface readback is white and the light readback is black
in the shadowed column and red elsewhere. The frames are
`specs/004-quality-renderer/evidence/q2-fork-seam-surface.png` and `q2-fork-seam-light.png` in the
test's evidence directory, not parity captures.

### 12.3 Limits (unchanged acceptance)

1. **No live engine frame.** The engine target has no device consumer/pipelines, so `r_renderApi 1`
   still reaches the typed refusal. The fork call sites are compile/link evidence, not a rendered
   engine frame.
2. **No retail data.** No `.pk4`/`.pk3`/`.pak` is present, so CP-07 and every T039 image row remain
   `NOT RUN`/`SKIPPED`. The fixture's shadow frame is synthetic.
3. **Index width.** The portable RHI vertex layout uses `uint16_t` indices. The registration unit
   refuses an index above 65535 instead of truncating it; a real large surface needs a 32-bit index
   path or per-surface splitting.
4. **Shadow volume source.** The engine seam still does not send shadow-volume meshes; the fixture
   supplies the closed back/front volume. T082's exclusion facts and chain counts are delivered, but
   the real volume geometry is future work.
5. **No shade-only interaction pass.** The seam now forwards the exclusion facts, but the portable
   adapter treats a light packet with no shadow chain as `lightsExcluded` and does not draw a
   shade-only interaction frame. A real `noShadows` light that still has lit surfaces therefore needs
   an adapter path before its interaction is claimed. The synthetic fixture's excluded light has no
   interaction surfaces, so it does not exercise that case.
6. **Reviews.** T040 is still NOT RUN and Q2 is not accepted. The fork PR and the bump PR require an
   independent reviewer; this file is author evidence only.


## 13. Engine-side device consumer, texture cache and shadow-volume packets (2026-11-03)

Status: author evidence for issue #142 on branch `feat/q2-device-consumer`. It addresses the three
remaining T080-T082 notes at source level and with synthetic readbacks. It is **not** a live engine
frame, not retail-data evidence, and not Q2 acceptance.

### 13.1 What was added

| Task | File | Behaviour |
|---|---|---|
| T080 | `src/renderer/rhi/engine_q2_sink_device.h`, `src/renderer/d3d12/engine_q2_device_consumer.cpp` | `EngineQ2DeviceBackend` implements `SceneGeometryBackend` with `Device::CreateBuffer` and `Device::SubmitDrawFrame`; `EngineQ2DeviceConsumer` owns the portable sink, the backend and the cache. It is compiled into `doom3_rhi` and a synthetic ordered surface frame is drawn and read back. |
| T081 | same files, `src/renderer/rhi/engine_q2_sink.{h,cpp}`, `src/renderer/rhi/materials.{h,cpp}` | `Q2TextureUploader` plus `EngineQ2DeviceTextureCache` turn an `imageUpload` into a device texture (RGBA8/sRGB, R8, BC5, BC7; BC1/BC3 and R8-sRGB are refused with a reason). `SelectStageTextureForDraw` binds the material's diffuse slot and refuses a declared map with no uploaded texture. |
| T082 | `src/renderer/rhi/engine_q2_sink.{h,cpp}` | `Q2SinkShadowVolumePacket` is sent immediately before its light packet; the consumer attaches it by the light identity it assigns and `ResolveLight` returns the back/front/shade meshes. The adapter then submits the two stencil passes and the shade pass. |

No `doom3_orig/doom3` pointer, root `CMakeLists.txt`, `engine_scene_adapter.{h,cpp}`, Q4/Q5/Q6/Q7
file or Q2 residual test was changed. One source entry was added to
`targets/win11-amd64/CMakeLists.txt` (`DOOM3_RHI_SOURCES`); that makes this a build-system change
whose reviewer must rerun the target build.

### 13.2 Commands and exits

All commands ran in this worktree on 2026-11-03. Build logs are in the git-ignored `build-q2/` and
`build/`.

| Command | Exit | Observed |
|---|---:|---|
| `cmake -S tests/renderer -B build-q2 -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 |
| `cmake --build build-q2 --config Release -- -m` | 0 | all renderer test executables built |
| `ctest --test-dir build-q2 -C Release --output-on-failure` | 0 | **48/48 passed**, including `renderer.q2.device_consumer.texture`, `.shadow` and `.stream` |
| `cmake --build build-q2 --config Debug -- -m` | 0 | all Debug executables built |
| `ctest --test-dir build-q2 -C Debug --output-on-failure` | 0 | **48/48 passed** |
| `q2_device_consumer_test.exe texture` | 0 | hardware and WARP: `SUMMARY pass=2 fail=0 skip=0` |
| `q2_device_consumer_test.exe shadow` | 0 | hardware and WARP: `SUMMARY pass=2 fail=0 skip=0` |
| `q2_device_consumer_test.exe stream` | 0 | hardware and WARP: `SUMMARY pass=2 fail=0 skip=0` |
| `cmake --preset win11-amd64` | 0 | `new-target` and `apply-target-patches` exit 0 |
| `cmake --build --preset win11-amd64-release -- -m` | 0 | `doom3_rhi.lib`, `DOOM3.exe` (17,626,112 bytes) and `DOOM3XP.exe` (17,810,432 bytes) built |
| `dumpbin /symbols build/targets/win11-amd64/Release/doom3_rhi.lib` | 0 | `EngineQ2DeviceConsumer`, `EngineQ2DeviceBackend` and `EngineQ2DeviceTextureCache` are defined in the engine library |
| `py -3 scripts/validate-rhi-contract.py` | 1 | the new files pass; the only failure is the pre-existing `src/renderer/dxr/q6_motion_texture.h` D3D12-vocabulary hit, which this branch does not modify |

Host adapters: `Intel(R) Graphics` (vendor `0x8086`, device `0x7D67`) as hardware index 0 and
`Microsoft Basic Render Driver` (WARP). No discrete adapter is claimed.

### 13.3 Synthetic frames

Each frame is the 8x8 readback of an offscreen draw, upscaled 16x by the fixture. They are not a
classic scene, not parity and not an engine frame.

| Frame | Proves |
|---|---|
| `evidence/q2-device-texture.png` | a 2x2 RGBA8 engine image becomes a device texture; the material stage binds it and the four quadrants sample exactly |
| `evidence/q2-device-texture-replaced.png` | a repeated upload under the same image id replaces the cached texture and the next frame samples the replacement |
| `evidence/q2-device-shadow.png` | a shadow-volume packet attached to its light produces the back-increment/front-decrement pattern; the excluded light adds no stencil work |
| `evidence/q2-device-stream.png` | two ordered textured surfaces; the two-stage material samples its diffuse slot (stage 1) and the first surface keeps its own image |

Negative controls executed by the same fixtures: a declared stage image with no upload is refused
(`material stage 0 declares an image but no device texture was uploaded`); a volume packet for chain
2 followed by a chain-1 light packet is refused rather than dropped. A swap of the stencil ops is
covered by `renderer.q2.stencil_shadow`, not re-run here.

### 13.4 Limits and non-claims

1. **No live engine frame.** `R_InitRenderApi` still creates no Device and no pipelines, so
   `r_renderApi 1` reaches the typed refusal. The new consumer is linked into `doom3_rhi` but no
   engine call site invokes it. The "device consumer exists in the engine library" claim is source
   and link evidence, not a rendered frame.
2. **The fork seam still does not emit a shadow-volume packet.** The consumer accepts and submits
   one; a fork call site that fills `Q2SinkShadowVolumePacket` is future work. The shadow frame is
   synthetic.
3. **One texture per draw.** The portable `DrawItem` samples one texture. The stage rule picks the
   diffuse slot (stage 1 for two or more stages, otherwise stage 0); it is not full multi-stage
   bump/diffuse/specular evaluation. Per-image filter/repeat facts are retained but the portable
   sampler is a static point sampler. Only mip 0 is forwarded by the engine seam; the device texture
   has one mip and records the declared count.
4. **No retail data.** No `.pk4`/`.pk3`/`.pak` is present, so CP-07 and every T039 image row remain
   `NOT RUN`/`SKIPPED`.
5. **Validator baseline.** The contract validator's only remaining failure is the pre-existing
   `q6_motion_texture.h` hit from merged PR #138; fixing it is Q6 scope and is not part of this
   branch.
6. **Reviews.** T040 is still NOT RUN and Q2 is not accepted. This file is author evidence only.
## 14. Q2 residual tests T036/T037/T038 (2026-11-03)

Addendum for issue #143. This section adds the three residual test cases and records exactly what
they prove. It does not change Q2 acceptance, does not run with retail data, and does not produce an
engine frame or a parity image.

### 14.1 What changed

| Row | File | Added |
|---|---|---|
| T036 | `tests/renderer/q2_render_api_fallback_test.cpp` | A classic consumer asked for real submission without a backend, or without a surface pipeline, refuses to initialize with an actionable reason; the refusal does not rewrite the request to structural-only; the reference/structural path still initializes and executes in the same process; a zero display extent is refused and the consumer is recoverable |
| T037 | `tests/renderer/classic_compatibility_test.cpp` | CP-14 mirror subview (separate non-overlay view before the main view), CP-15 portal sky (one `Skybox`/`environment` stage, no sampled upload, manifest gate refuses the stub), CP-16 xray subview (separate non-overlay view, blended surface classified translucent) |
| T038 | `tests/renderer/classic_gui_test.cpp` | World-GUI view → `CopyToImage` → overlay order, with a negative control that rejects a copy recorded after the overlay begins |

`tests/renderer/CMakeLists.txt` adds the T036 executable and the `renderer.q2.render_api_fallback`
CTest entry. The engine fork gains a `tr_guisurf.cpp` change on branch `spec-004-q2-q3-guisurf`
(PR yhyu13/DOOM-3#3): the quads emitted by `R_RenderGuiSurf` are marked `DSF_GUI_SURFACE`, so the
ordered seam forwards `worldGui=true` for the GUI content. `EmitFullScreen` is unchanged and remains
the display overlay. The commit is `54d26a6`; the superproject still pins `c87e87b`, so this change
is **not** in the reviewed tree and no bump was created.

### 14.2 Commands and exits

Run from this worktree on 2026-11-03. `cmake` was not on `PATH`; the VS2022-bundled binary was
used. Logs are worktree-local and git-ignored: `build-q2-residual-configure.log`,
`build-q2-residual-release.log`, `build-q2-residual-debug.log`.

| # | Command | Exit | Observed |
|---|---|---|---|
| 1 | `cmake -S tests/renderer -B build-q2-residual -G "Visual Studio 17 2022" -A x64` | 0 | configured with MSVC 19.44.35228.0 |
| 2 | `cmake --build build-q2-residual --config Release -- -m` | 0 | all test executables built, including `q2_render_api_fallback_test.exe` |
| 3 | `cmake --build build-q2-residual --config Debug -- -m` | 0 | all test executables built |
| 4 | `build-q2-residual\Release\q2_render_api_fallback_test.exe` | 0 | `T036 PASS (portable failed-init/fallback contract; engine r_renderApi launch and T083 NOT RUN)` |
| 5 | `build-q2-residual\Debug\q2_render_api_fallback_test.exe` | 0 | same T036 line |
| 6 | `build-q2-residual\Release\q2_classic_compatibility_test.exe` | 0 | `CP-14 mirror subview`, `CP-15 portal sky`, `CP-16 xray subview`, then `T037 PASS (structural/classification only; CP-14/15/16 structural; image parity NOT RUN)` |
| 7 | `build-q2-residual\Release\q2_classic_gui_test.exe` | 0 | `ORDER world-gui capture before the overlay: BeginView > DrawPacket > CopyToImage > EndView > BeginView > DrawPacket > EndView > SwapBuffers`, `CONTROL copy-after-overlay is rejected`, `T038 PASS (structure only; HUD image metrics NOT RUN)` |
| 8 | `ctest --test-dir build-q2-residual -C Release -L q2 --output-on-failure` | 0, **12/12** | includes `renderer.q2.render_api_fallback` and the three #145 device-consumer tests |
| 9 | `ctest --test-dir build-q2-residual -C Debug -L q2 --output-on-failure` | 0, **12/12** | includes `renderer.q2.render_api_fallback` and the three #145 device-consumer tests |
| 10 | `git -C doom3_orig/doom3 push origin HEAD:refs/heads/spec-004-q2-q3-guisurf` | 0 | fork branch created; fork PR https://github.com/yhyu13/DOOM-3/pull/3 |

The Debug run of the existing Q2 executables is included in the 12/12 CTest runs (5
structural/fork-seam cases, 3 device-consumer cases from #145, and 4 synthetic GPU fixtures); their
direct output matches the Release lines above. The rebase onto `8a58e43` kept both the #145
device-consumer additions and the T036–T038 additions; neither was reverted.

### 14.3 What this proves and what it does not

- **T036 is still PARTIAL, not ticked.** The test proves the portable failure/fallback contract:
  an actionable refusal, no rewrite into structural-only mode, and a usable reference path. It does
  not boot the engine, does not set `r_renderApi`, and does not prove the GL path renders. The
  engine-side selector is still evidenced only by the fork source and the PR #113 string check;
  T083's forced-failure launch remains NOT RUN because there is no legal game data.
- **T037 is ticked for its test deliverable.** CP-14/CP-15/CP-16 now have structural/classification
  cases. Mirror recursion, portal-sky content, xray surface indexing and all image comparisons are
  still `NOT RUN`; T039 owns those results. The production halves of T080–T082 are unchanged.
- **T038 is still PARTIAL, not ticked.** The `CopyToImage` ordering case and its negative control
  pass, and the fork change exists on `spec-004-q2-q3-guisurf`. The fork change is **not compiled in
  this worktree**: building the target copy from the modified submodule would make
  `targets/win11-amd64/patches/all-changes.patch` fail its result-hash verification (the patch is
  tied to `c87e87b`). A coordinated bump is required before the target build can prove the fork
  change compiles; no second bump was created here. HUD image metrics remain T039.
- **No acceptances.** Q2 is not accepted. No classic frame, GL-vs-D3D12 comparison, HUD metric or
  retail scene is claimed. The Q2 GPU fixtures that ran here (`renderer.q2.draw_geometry`,
  `texture_sampling`, `stencil_shadow`, `scene_submit`) are synthetic frames, as recorded in §10.
- **Reviews.** T040 is still NOT RUN. The superproject PR and the fork PR require an independent
  reviewer; this section is author evidence only.

### 14.4 Header scan note

`py -3 -I scripts/validate-rhi-contract.py` exits **1** on this branch, but only for the pre-existing
`src/renderer/dxr/q6_motion_texture.h` (`D3D12` vocabulary). That file is untouched by this branch
and belongs to Q6, which this assignment excludes. The new public test unit
`tests/renderer/q2_render_api_fallback_test.cpp` is not reported by the scan after removing the
prohibited term from its comment; the exit-0 result quoted in §4 command 18 should not be read as
still green until the Q6 row is fixed.


## 15. Q2 device follow-up: Device/pipeline creation, live sink/texture path, T038/T082 fork (2026-11-03)

Status: author evidence for the follow-up after `c87e87b`/`ddaea6a`. It closes the source-level
notes that §13 left open and adds the two fork changes, with synthetic readbacks and compile/link
evidence. It is **not** a live retail engine frame, not retail-data evidence, and not Q2 acceptance.
The submodule bump and patch re-export were **not** performed (see §15.5).

### 15.1 What changed

| Row | File | Behaviour |
|---|---|---|
| T080/T081 | `src/renderer/rhi/engine_q2_device_bootstrap.{h,cpp}` (new) | `CreateEngineQ2ClassicPipelines` compiles the five classic programs and creates the surface, shadow-back (increment), shadow-front (decrement) and shade pipelines. `EngineQ2DeviceRuntime::Create` creates the hardware Device at adapter 0, creates those pipelines and initializes `EngineQ2DeviceConsumer`; `Shutdown` retires the consumer (and its textures) before the Device. |
| T080/T081 | `src/renderer/rhi/engine_q2_sink_install.cpp` | The installed initialize callback is now the device-creation call site that `R_InitRenderApi` invokes: it creates the runtime and switches the forwarding table to that consumer. `imageUpload` reaches `EngineQ2DeviceTextureCache`, `surfacePacket`/`drawPacket` reach the device backend, and `lightPacket` reaches the adapter. `BridgeShadowVolumePacket` converts the fork packet (concatenating chain surfaces with their transforms baked) and attaches it to the next light. |
| T038 | fork `neo/renderer/tr_guisurf.cpp`, `GuiModel.cpp` (commit `a91a70f`) | `R_RenderGuiSurf` marks the quads it emits `DSF_GUI_SURFACE`; `EmitFullScreen` stays the display overlay. Same commit as fork PR #3, carried in #4. |
| T082 | fork `neo/renderer/RenderApi.h`, `tr_backend.cpp`, `RenderSystem_init.cpp` (commit `46f9242`) | `q2ShadowVolumePacket_t` plus a `shadowVolumePacket` callback. `RB_Q2EmitViewLight` emits one packet immediately before the light packet for the same chain: back/front are the front end's caster chain snapshot (the stencil pass projects and draws it twice), shade is the interaction chain, each surface keeps its transform. An excluded light emits no packet, and an absent callback keeps the old behaviour. |
| T080-T082 | `tests/renderer/q2_engine_device_bootstrap_test.cpp` (new) | Drives `EngineQ2DeviceRuntime` exactly as the engine callback does, then submits a textured surface and a shadow-volume/light frame through the same callback table. Reads both frames back. |
| build | `targets/win11-amd64/CMakeLists.txt`, `tests/renderer/CMakeLists.txt` | Adds the bootstrap source to `doom3_rhi` and the new test executable/CTest entry. |

`Q2_SEAM_SHADOW_VOLUME` is defined by the fork header. The installer registers
`BridgeShadowVolumePacket` only when that marker exists, so this superproject still builds against
the pinned `c87e87b` header and registers the callback automatically after the coordinated bump.

### 15.2 Commands and exits

All commands ran in this worktree on 2026-11-03. Logs are worktree-local and git-ignored.

| Command | Exit | Observed |
|---|---:|---|
| `cmake -S tests/renderer -B build-q2-followup -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 |
| `cmake --build build-q2-followup --config Release -- -m` | 0 | all fixtures built, including `q2_engine_device_bootstrap_test.exe` |
| `ctest --test-dir build-q2-followup -C Release --output-on-failure` | 0 | **50/50 passed**, including `renderer.q2.engine_device_bootstrap` |
| `cmake --build build-q2-followup --config Debug -- -m` | 0 | all Debug fixtures built |
| `ctest --test-dir build-q2-followup -C Debug --output-on-failure` | 0 | **50/50 passed** |
| `q2_engine_device_bootstrap_test.exe` | 0 | hardware `Intel(R) Graphics`; `draws extracted=1 submitted=1 refused=0 submissions=1`; texture quadrants 255,0,0 / 0,255,0 / 0,0,255 / 255,255,255; `lights extracted=2 excluded=1 refused=0 passes=3 submissions=1`; shadow (4,1)=0,0,0,255 and (1,1)=255,0,0,255 |
| `cmake --preset win11-amd64` | 0 | `new-target` 0, `apply-target-patches` 0 |
| `cmake --build --preset win11-amd64-release -- -m` (pin `c87e87b`, marker absent) | 0 | `DOOM3.exe` 17,647,104 bytes; `DOOM3XP.exe` 17,830,912 bytes |
| fork diff `c87e87b..46f9242` applied to the generated target copy, same build (marker present) | 0 | `DOOM3.exe` 17,650,176 bytes; `DOOM3XP.exe` 17,835,008 bytes |
| `dumpbin /symbols build/targets/win11-amd64/Release/doom3_rhi.lib` | 0 | `CreateEngineQ2ClassicPipelines`, `EngineQ2DeviceRuntime::{Create,Shutdown}` defined |
| `py -3 -I scripts/validate-rhi-contract.py` | 1 | the new header passes after removing backend-private vocabulary; the only remaining failure is the pre-existing `src/renderer/dxr/q6_motion_texture.h` hit, which this branch does not modify |
| `git push` fork branch `spec-004-q2-device-followup` | 0 | fork PR https://github.com/yhyu13/DOOM-3/pull/4 |

The fork changes were compiled against the real engine target copy, not inspected only: the target
copy is git-ignored, the five touched engine files were restored byte-for-byte from backups after
the build, and the submodule pointer was not moved.

### 15.3 Synthetic frames

`build-q2-followup/q2-evidence/q2-engine-device-bootstrap-texture.bmp` and
`...-shadow.bmp` are 8x8 offscreen readbacks upscaled by the fixture. They are not a classic scene,
not parity and not an engine frame. They prove that the device and pipelines created by the
`R_InitRenderApi` callback draw the same ordered textured surface and the same stencil/shade pattern
as the earlier fixtures, and that the texture cache receives the engine's `imageUpload`.

Negative control: `CreateEngineQ2ClassicPipelines(device, nullptr, &error)` returns false rather than
a half-filled descriptor.

### 15.4 Resolved notes versus data-blocked notes

Resolved at source level by this branch:

1. **T080/T081 "`R_InitRenderApi` creates no Device/pipelines".** The installed initialize callback
   now creates the hardware Device, the four classic pipelines and the device consumer, and the
   synthetic frames above are submitted through them.
2. **T081 "no engine Device exists to receive a live upload".** The same callback installs the device
   texture cache; `imageUpload` reaches it and the draw samples the uploaded texture.
3. **T082 "the fork seam does not emit the packet".** The fork emits one packet per chain, and the
   superproject installer converts and submits it to the consumer's stencil/shade passes.
4. **T038 "`tr_guisurf.cpp` is not touched".** The fork commit exists and compiled in the target-copy
   build.

Still data-blocked or process-blocked:

1. **No live retail engine frame.** No legal `.pk4`/`.pk3`/`.pak` exists in this worktree, so the
   selector cannot boot a level and no engine-emitted frame is claimed. T083 remains NOT RUN.
2. **Every T039 image row and every retail CP-07 row remain NOT RUN.** The readbacks above are
   synthetic.
3. **The submodule bump and patch re-export were not performed.** The fork change is on PR #4 and
   is not merged. The assignment prohibits merging, and non-negotiable 11 requires an independent
   reviewer before a self-merge; a bump cannot point at a merge SHA that does not exist. The pin
   therefore stays `c87e87b`, the exported patch/manifest still describe that tree, and the
   coordinated bump is the one remaining step after fork review and merge.
4. **Portable limits unchanged:** 16-bit indices, one sampled texture per draw with a static point
   sampler, and only mip 0 forwarded by the image seam.

### 15.5 Non-claims

- Q2 is not accepted; T040's independent review of the Q2 behaviour is recorded elsewhere and is not
  extended by this section.
- The shadow-volume packet carries the caster chain the classic stencil pass consumes; it does not
  claim a port-side volume extrusion beyond what the adapter already did with the synthetic packet.
- `owner-sign-off:` lines in the PRs quote the owner's verbatim authorization for Q2 fork branches
  and bumps ("me grant right"); they are not Q2 acceptance.
