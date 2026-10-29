# Classic shader ABI: engine → program

**Status**: documented from source (2026-10-28). No authored shader exists yet, so the *consumer*
side is a specification, not an observation.

**Purpose**: fix the interface the HLSL library must implement, before anyone writes a shader. If
this file is wrong, every program is wrong in the same way.

**Evidence grades**: register and binding facts are grade 1 (pristine engine source). ARB opcode
semantics are grade 2 (Khronos `ARB_vertex_program` / `ARB_fragment_program`, external and not
fetched here). Statements about which built-in GL state the **retail** programs consumed are
explicitly marked **inference** — the retail program text is not in the repository and must not be
read without legal clearance ([`provenance.md`](provenance.md)).

---

## 1. Vertex input

`idDrawVert` is 60 bytes, in this order (`neo/idlib/geometry/DrawVert.h:40`):

| Offset | Field | Type | Bytes |
|---:|---|---|---:|
| 0 | `xyz` | `idVec3` | 12 |
| 12 | `st` | `idVec2` | 8 |
| 20 | `normal` | `idVec3` | 12 |
| 32 | `tangents[0]` | `idVec3` | 12 |
| 44 | `tangents[1]` | `idVec3` | 12 |
| 56 | `color` | `byte[4]` | 4 |

Attribute slots as bound by the engine:

| Input | Interaction path | Generic / cube path | Evidence |
|---|---:|---:|---|
| `xyz` | 0 | 0 | `qglVertexPointer`: `neo/renderer/draw_arb2.cpp:198`, `neo/renderer/draw_common.cpp:758` |
| `st` | 8 | 8 | `neo/renderer/draw_arb2.cpp:197`, `qglTexCoordPointer` at `neo/renderer/draw_common.cpp:759` |
| `tangents[0]` | 9 | 9 | `neo/renderer/draw_arb2.cpp:196`, `neo/renderer/draw_common.cpp:796` |
| `tangents[1]` | 10 | 10 | `neo/renderer/draw_arb2.cpp:195`, `neo/renderer/draw_common.cpp:797` |
| `normal` | **11** | **2** | `neo/renderer/draw_arb2.cpp:194`; `qglNormalPointer` at `neo/renderer/draw_common.cpp:798` |
| `color` | **3** | **3** | `glColorPointer`: `neo/renderer/draw_arb2.cpp:193`, `neo/renderer/draw_common.cpp:795` |

The two paths are **not** interchangeable. The interaction path explicitly binds the normal to
generic attribute 11 (`qglVertexAttribPointerARB(11, ...)`). The cube/environment path uses
`qglNormalPointer`, which ARB_vertex_program maps to generic attribute **2**, and `glColorPointer`,
which maps to attribute **3** (grade 2, public ARB specification). An environment or bumpy-environment
shader that reads NORMAL from slot 11 will read an unbound attribute. The port must renumber
deliberately and declare the mapping once in the ABI header; do not copy the numbers above.

The shadow pass has a different input entirely: `RB_StencilShadowPass` binds
`qglVertexPointer(4, GL_FLOAT, sizeof(shadowCache_t), ...)` (`neo/renderer/draw_common.cpp:1072`),
and `shadowCache_t` is a single `idVec4` (`neo/renderer/Model.h:79-80`). The shadow vertex program reads
only attribute 0 with four components (`w` carries the homogeneous trick); it receives no normal,
tangent, or colour. Do not reuse the surface input layout for it.


---

## 2. Texture bindings

### 2.1 Per-light interaction (`neo/renderer/draw_arb2.cpp:67`)

| Unit | Image | Set at |
|---:|---|---|
| 0 | `normalCubeMapImage`, or `ambientNormalMap` for ambient lights | `neo/renderer/draw_arb2.cpp:172` |
| 1 | per-surface bump map | `neo/renderer/draw_arb2.cpp:114` |
| 2 | light falloff (1D table) | `neo/renderer/draw_arb2.cpp:118` |
| 3 | light projection / cube | `neo/renderer/draw_arb2.cpp:122` |
| 4 | per-surface diffuse | `neo/renderer/draw_arb2.cpp:126` |
| 5 | per-surface specular | `neo/renderer/draw_arb2.cpp:130` |
| 6 | `specularTableImage`, or `specular2DTableImage` under `r_testARBProgram` | `neo/renderer/draw_arb2.cpp:180` |

Seven units are assigned. Which stage samples which unit is **not** observable from the engine
source, because the program text is absent; the unit assignments above are the observable contract,
and the authored programs decide the split. A faithful replacement normally samples the surface and
light textures in the fragment stage and uses the normal cube for light-vector normalization, but
that is an implementation choice to be verified by parity, not a fact from this tree.

### 2.2 Cube reflection (`RB_PrepareStageTexturing`, `neo/renderer/draw_common.cpp:193`)

| Unit | Image |
|---:|---|
| 0 | the stage's cube map |
| 1 | the bump stage image, when the material has a bump stage |

### 2.3 Glass warp (`RB_PrepareStageTexturing`, `neo/renderer/draw_common.cpp:153`)

| Unit | Image |
|---:|---|
| 0 | the stage image |
| 1 | `scratchImage2` |
| 2 | `scratchImage` |

---

## 3. Constant registers the engine writes

### 3.1 Vertex program environment

| Slot | Writer | Value | Lifetime |
|---:|---|---|---|
| c[0] | `RB_SetProgramEnvironment` | `(w / potW, h / potH, 0, 1)` screen-copy scale (live write at `neo/renderer/draw_common.cpp:612-622`; the `#if 0` branch at `neo/renderer/draw_common.cpp:587-609` computes `(w-1)/pot` and is never compiled) | per frame |
| c[1] | `RB_SetProgramEnvironment` | world-space view origin, `w=1` (write at `neo/renderer/draw_common.cpp:641`) | per frame |
| c[4] | **dual writer** | light origin in model space; `w = 0` on both paths (`neo/renderer/tr_render.cpp:732-735`, shadow at `neo/renderer/draw_common.cpp:1063`) | per interaction and per shadow space |
| c[5] | **dual writer** | view origin in model space, `w=1` on both paths (space write at `neo/renderer/draw_common.cpp:664`) | per space and per interaction |
| c[6..8] | **dual writer** | light projection S/T/Q per interaction; model-matrix columns per space | see §4 |
| c[9] | `RB_ARB2_DrawInteraction` | falloff S plane | per interaction |
| c[10..11] | `RB_ARB2_DrawInteraction` | bump texture matrix S/T | per interaction |
| c[12..13] | `RB_ARB2_DrawInteraction` | diffuse texture matrix S/T | per interaction |
| c[14..15] | `RB_ARB2_DrawInteraction` | specular texture matrix S/T | per interaction |
| c[16] | `RB_ARB2_DrawInteraction` | vertex-colour modulate | per interaction |
| c[17] | `RB_ARB2_DrawInteraction` | vertex-colour add | per interaction |
| c[20] | NV20 programs only | falloff TQ | not used by ARB2 |

The slot numbers are declared in `neo/renderer/tr_local.h:1368`. Anchors: `neo/renderer/draw_common.cpp:579` (frame), `neo/renderer/draw_common.cpp:653` (space),
`neo/renderer/draw_arb2.cpp:67` (interaction), `neo/renderer/draw_arb2.cpp:94` (colour policy).
The shadow-space write of c[4] with `w = 0` is at `neo/renderer/draw_common.cpp:1063`; the
per-interaction writes of c[4] and c[5] are at `neo/renderer/draw_arb2.cpp:69` and
`neo/renderer/draw_arb2.cpp:70`.

Both paths write `c[4].w = 0` and `c[5].w = 1`: the interaction builder sets them explicitly
(`neo/renderer/tr_render.cpp:734-735`) and the shadow path sets `w = 0` again on purpose. The `w`
component is therefore *not* a reason to split the constant buffer; the dual writers of `c[6..8]` and
fragment `c[0..1]` are (see §4).

### 3.2 Fragment program environment

| Slot | Writer | Value | Lifetime |
|---:|---|---|---|
| c[0] | `RB_SetProgramEnvironment` | screen-copy scale | per frame |
| c[0] | `RB_ARB2_DrawInteraction` | diffuse colour | per interaction |
| c[1] | `RB_SetProgramEnvironment` | `(1/w, 1/h, 0, 1)` window → [0,1] | per frame |
| c[1] | `RB_ARB2_DrawInteraction` | specular colour | per interaction |
| c[2] | `RB_ARB2_DrawInteraction`, `r_testARBProgram` only | light origin | debug |
| c[3] | `RB_ARB2_DrawInteraction`, `r_testARBProgram` only | view origin | debug |

Anchors: `neo/renderer/draw_common.cpp:625` (fragment c[0]), `neo/renderer/draw_common.cpp:632`
(fragment c[1]), `neo/renderer/draw_arb2.cpp:108` (diffuse), `neo/renderer/draw_arb2.cpp:109`
(specular), `neo/renderer/draw_arb2.cpp:84` (debug c[2]), `neo/renderer/draw_arb2.cpp:85`
(debug c[3]). The NV20-only slot is declared at `neo/renderer/tr_local.h:1383`.

---

## 4. The collision that forbids one global constant buffer

`c[6..8]` and fragment `c[0..1]` are written by different producers with different meanings:

- During an interaction, `c[6..8]` are light projection planes S/T/Q
  (`neo/renderer/draw_arb2.cpp:71`).
- During a generic new-style stage, `c[6..8]` are model-matrix columns for local → global
  transforms (`neo/renderer/draw_common.cpp:672`).
- Fragment `c[0..1]` are screen-copy/window scale per frame, then diffuse/specular colour per
  interaction (`neo/renderer/draw_common.cpp:625`, `neo/renderer/draw_arb2.cpp:108`).

`c[4]`, `c[5]`, `c[9..17]` keep one meaning across the interaction and shadow paths (`w` included, as
corrected in §3.1); `c[4].w = 0` and `c[5].w = 1` on both. The collision that forbids a single global
buffer is the overlapping, differently-typed use of `c[6..8]` and of fragment `c[0..1]`.

A single "frame constants" UBO cannot express this. Each program family gets its own constant
buffer layout:

| Program family | Constant buffer | Contents |
|---|---|---|
| interaction | `D3InteractionCB` | light origin/view origin (model space), projection planes, falloff S, three texture matrices, colour policy, diffuse/specular colour |
| environment / bumpy environment | `D3EnvironmentCB` | local view origin, model matrix, view-projection, cube/bump bindings |
| shadow | `D3ShadowCB` | light projection, model matrix, view-projection |
| generic stage | `D3StageCB` | model matrix, view-projection, local params from `vertexParm`, fog |
| glasswarp | `D3GlasswarpCB` | screen matrices, viewport scale |

This is a design decision, not an observed original layout: the original programs took the same
values from ARB environment/local registers, which are per-program state by construction.

---

## 5. Local parameters and data-authored stages

`vertexParm <index> <expr>...` evaluates to local vertex-program constants:

- the stage stores up to four local parameter slots in `newShaderStage_t.vertexParms` (`neo/renderer/Material.h:194`); local slot `i` for `i = 0 .. numVertexParms-1` is written at `neo/renderer/draw_common.cpp:818`, value built
  from four material registers and written to the slot (`neo/renderer/draw_common.cpp:824`).
- local parameters are a **separate namespace** from environment parameters. When mapped to HLSL,
  they must be packed into a distinct range with explicit names; never merged by numeric slot with
  the `c[]` table above.
- `fragmentMap <unit> <image>` binds texture units `0 .. numFragmentProgramImages-1`
  (`neo/renderer/Material.cpp:924`). A data-authored stage therefore needs a bind-group layout that
  is stable for an arbitrary unit count, not the fixed seven of the interaction pass.

The optional `arity` field in the manifest records the local-parameter and fragment-map counts a
program expects, so the resolver can reject a material that under-supplies them.

---

## 6. Implicit GL state that must become explicit

The ARB programs had access to built-in GL state. The authored library must not rely on it, because
the portable RHI has no implicit matrices or fog. The port supplies:

| Input | Source on the C++ side | Note |
|---|---|---|
| model matrix | `surf->space->modelMatrix` | used for local → global |
| model-view matrix | `surf->space->modelViewMatrix` | loaded by `qglLoadMatrixf` |
| projection matrix | `backEnd.viewDef->projectionMatrix` | fixed-function |
| view-projection | product | **inference**: the original may have used `gl_ModelViewProjectionMatrix` |
| texture matrices | `RB_LoadShaderTextureMatrix` | used by fixed-function texgen and `TG_*` modes |
| fog colour/density | `qglFog*` | fixed-function fog is not available in HLSL |

Which of these the retail programs actually read cannot be answered from this repository. The
authored programs define their own inputs, so the question is closed by the library rather than by
reverse-engineering: a program declares its inputs in the ABI header and the manifest, and a
reflection test (T-026) proves it uses nothing else.

---

## 7. ARB opcode semantics a replacement may need

Grade 2 (Khronos specifications). This is the semantic vocabulary available to the original
programs; the authored library must implement any opcode it chooses to use with these meanings. It
is not a claim that every opcode below appeared in the retail set, which cannot be inspected here.

| Opcode | Semantics to reproduce |
|---|---|
| `TEX` / `TXB` / `TXP` | sample; `TXP` divides by `q` and has no direct HLSL builtin — divide, then sample with an explicit LOD |
| `KIL` | `discard`; sample before the discard; use an explicit LOD where control flow could be non-uniform |
| `LIT` | the ARB lighting helper (x, y, z, w components) |
| `DST` | distance vector |
| `SCS` | sine/cosine pair |
| `CMP` | component-wise compare/select |
| `LRP` | linear interpolate |
| saturating writes | clamp to [0,1] where the ARB destination saturates |
| env / local parameter arrays | replaced by the constant buffers of §4, never by globals |

Precision is fp32. Do not inherit half precision from a later id Tech 4 release: the ARB programs
were fp32 and half precision changes parity.

---

## 8. Packing rules for the portable backends

| Rule | Reason |
|---|---|
| every constant slot is `float4`-aligned | D3D12 cbuffer packing and WGSL uniform layout both require 16-byte alignment for `vec4`/arrays |
| no implicit padding inside a struct that crosses backends | HLSL and WGSL pad differently for `float3` |
| matrices are passed as four `float4` rows in a declared order | avoids row/column-major ambiguity; the original GL state is column-major |
| texture/sampler pairs are declared once per bind group | D3D12 descriptor ranges and WebGPU bind groups both need a fixed layout |
| ARB `c[n]` numbers never appear in HLSL names | the mapping is per-family; names carry meaning (`LightProjectionS`) |

The ABI header is the only place these rules are encoded. Programs include it; they do not
hand-declare constants.

---

## 9. Verification plan

| Check | Method | Task |
|---|---|---|
| every program declares only ABI inputs | DXC/SPIR-V reflection against a declared input list | T-026 |
| constant layouts match across D3D12 and WebGPU | compare reflected offsets, fail on mismatch | T-026 |
| interaction math matches the C++ inputs | unit test with a known light/surface, compare to the CPU reference in `abi.md` | T-027 |
| no fixed-function texgen remains in a program path | grep the renderer + a scene test with mirrors and fog | T-027 |

---

## 10. Open questions

1. Did the retail interaction program use `gl_ModelViewProjectionMatrix`, or reconstruct the
   transform from the texture matrices? (Unknowable from source; the authored program can choose,
   but parity tests must confirm the choice.)
2. Are `TG_SCREEN` / `TG_SCREEN2` used by any new-style stage, or only by fixed-function stages?
   (Runtime census; changes whether the stage CB needs projection planes.)
3. Does any data-authored stage use more than seven fragment maps? (Runtime census; bind-group
   arity.)
