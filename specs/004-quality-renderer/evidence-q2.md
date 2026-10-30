# Q2 engine-adapter and classic-path evidence (T034, T035, T037, T038, T080–T082, T039)

Status: author evidence for issue #53, written 2026-10-30. This is **not** Q2 acceptance, not a
classic-parity result and not an independent review. It records what the merged superproject code
does, what was observed on this host, and what could not run.

**Headline: the adapter, the material plan and the light plan exist and are exercised; no classic
frame was drawn and no GL-vs-D3D12 image comparison exists.** Every image row below is `NOT RUN` or
`SKIPPED` with a reason, and none of them counts as parity.

## 1. Scope and claim

| Item | Value |
|---|---|
| Stage and claim | Q2 classic path: ordered scene stream, material/light classification, overlay split. Claim: order and classification behave as the contracts require; **no** parity, capture, selector or fallback claim |
| Issue | #53 (implementation container; stays open) |
| Branch / base | `feat/q2-engine-adapter`, started from `main` at `2487baf` (S2 resolver wiring merged, P1 closed) |
| Submodule | `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, `git -C doom3_orig/doom3 status --porcelain` empty; **no** submodule change, **no** fork change in this PR |
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
| MFC / ATL | **MFC not installed.** `atlmfc/include` holds ATL headers only; no `afxwin.h` exists anywhere under `Program Files\Microsoft Visual Studio` |
| Target copy | Created with `scripts/new-target.ps1 -Name win11-amd64` (git-ignored). No engine source was edited |

The missing MFC component is a host gap, not a property of this change: `neo/idlib/precompiled.h:56,61`
includes `tools/comafx/StdAfx.h`, which includes `<afxwin.h>` at line 38, so every engine translation
unit needs MFC.

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
| 12 | `cmake --preset win11-amd64` | 0 | `build-configure.log` |
| 13 | `cmake --build --preset win11-amd64-release -- -m` | **1** | `build/_build.log` |
| 14 | guard: rename the adapter, configure | **1**, `Portable RHI source is missing` | `build-guard.log` |
| 15 | `py -3 -I scripts/validate-rhi-contract.py` | 0 | header scan over 11 headers, 26 public test files |
| 16 | `py -3 -I scripts/validate-webgpu-mapping.py` | **1**, 62 problems | `build-q2/webgpu-mapping.log` (pre-existing, #36/T089) |

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

### 4.3 Engine build

`cmake --preset win11-amd64` exits 0 with the new wiring and the fail-closed guard. The Release
build produces `build/targets/win11-amd64/Release/doom3_rhi.lib` (1,217,792 bytes) with
`engine_scene_adapter.cpp`, `materials.cpp` and `lighting.cpp` compiled under the engine's `/MT`
settings, then fails with **353 × `error C1083: cannot open include file: afxwin.h`** in
`doom3_engine.vcxproj` / `doom3_game_base.vcxproj` (untouched engine sources reaching MFC through
`neo/idlib/precompiled.h`). Consequence: `DOOM3.exe` / `DOOM3XP.exe` were **not** produced, and the
`dumpbin` machine/dependents rows are **NOT RUN**. Nothing in the failure touches a file this PR
changes.

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
| B4 | MFC component missing on the host | Engine executables do not link; `dumpbin` and any engine run are `NOT RUN` | Host toolchain |
| B5 | T036 (`r_renderApi`, `R_RenderSystemRunning()`, `RC_*` dispatch) is not implemented and needs a fork PR plus a submodule bump | No engine call site, no selector, no fallback; T083 cannot start | Q2 fork work |
| B6 | `validate-webgpu-mapping.py` exits 1 with 62 problems on current `main` | Pre-Q2 header gate is red (S3); unchanged by this PR | #36 / T089 |
| B7 | `targets/win11-amd64/CMakeLists.txt` is a build-system change | Needs owner sign-off in addition to independent review before merge | Owner |

Stop conditions triggered or relevant: **S13** (no legal data → SKIPPED), **S3** (mapping gate red,
recorded; no capture attempted), **S1** still open (no `owned` program), **S8/S12** not triggered (no
fork change, no second writer on `engine_scene_adapter.cpp`), **S10** not triggered (boundary clean).

## 8. T083 forced failure

**NOT RUN.** There is no engine executable and no `r_renderApi` selector to force. No typed error, no
unchanged-cvar observation and no `r_renderApi 0` GL launch are claimed.

## 9. Independent review

**NOT RUN.** T040 requires a reviewer that is not the author, with the reviewed SHA recorded
verbatim in `review-q2.md`. Nothing in this file is a review.

## 10. What would make this claim stronger

1. Install the MFC component and link both executables; record `dumpbin /headers` (`0x8664`),
   `dumpbin /dependents` (no `MSVCP140.dll`/`VCRUNTIME140.dll`) and both link lines.
2. Extend the seam with vertex/index submission so a draw packet can actually be submitted, then
   re-run CP-01…CP-30 structurally against an engine-produced stream.
3. Provision the Q0 demo data and measure T-05/T-06/T-08/T-09 with a GL-vs-GL control **before** any
   comparison.
4. Land T036 and T083 with the fork PR and submodule bump.
