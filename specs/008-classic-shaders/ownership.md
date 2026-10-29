# HLSL library ownership

**Status**: ownership model **decided**; the library **does not exist yet**. Directory paths below
are the approved target, not a claim that files are present.

This file answers the one question a shader library cannot leave ambiguous: **who decides what a
program may depend on, and who signs off a change?**

---

## 1. Owner role

| Field | Value |
|---|---|
| Role | `renderer-shader-library` |
| Accountable for | HLSL sources, ABI header, manifest, build graph, provenance records, shader tests |
| Current holder | the S2 workstream (this spec) until the first implementation PR merges |
| Transfer | requires a PR that edits this file and names the successor; no verbal handover |
| Escalation | renderer owner for behaviour, provenance reviewer for legal questions |

The owner is a **role, not a person**. A person leaving does not orphan the library; the role
transfers by PR.

This repository has no `CODEOWNERS` file. The table in §6 is the review contract instead: a PR that
touches an owned area needs the owner's review even if nobody's name is attached to the path.

---

## 2. Scope

### Owned

- `shaders/classic/**` — program sources.
- `shaders/classic/include/**` — the ABI header and ARB-emulation helpers. The highest-risk files
  in the library.
- `shaders/manifest/classic-programs.json` — the single source of truth for status and stages
  ([`resolver.md`](resolver.md) §4).
- `shaders/tools/**` — compile and reflection scripts.
- Provenance records ([`provenance.md`](provenance.md)).
- Shader-level tests and golden data.

### Not owned

| Area | Owner | Boundary |
|---|---|---|
| resolver, material parser, draw gating | renderer | the library provides programs; it does not decide when to draw |
| RHI, backends, PSO cache | RHI/renderer | the library provides entry points and constant layouts |
| material/asset pipeline | renderer/content | the library never reads game data |
| legal sign-off | provenance reviewer / counsel | the owner prepares records, does not self-approve the legal position |

The boundary matters: a shader owner who starts editing draw code becomes a second renderer owner,
and the "one writer per file" rule is broken by role creep.

---

## 3. Library layout (approved target)

```text
shaders/
  classic/
    include/
      d3_abi.hlsli          # constant-buffer structs, bindings, packing rules (abi.md)
      d3_input.hlsli        # VS input / inter-stage structs
      d3_arb.hlsli          # ARB opcode emulation: TXP divide, LIT, DST, SCS, CMP, LRP, saturate
    programs/
      interaction.hlsl
      environment.hlsl
      bumpyenvironment.hlsl
      shadow.hlsl
      arbfp_glasswarp.hlsl
      test.hlsl
    stages/                 # Tier B: state-derived generic stage shaders
      generic_texgen.hlsl
  manifest/
    classic-programs.json
  tools/
    compile.ps1             # DXC -> DXIL; DXC -> SPIR-V -> Tint -> WGSL
    reflect.ps1             # declared-vs-used inputs and constant offsets
    verify-golden.ps1       # deterministic compile hashes per toolchain pin
```

### 3.1 Two tiers

| Tier | What | Variants | Why |
|---|---|---|---|
| **A — classic programs** | the five required census programs, the optional `test`, and `megaTexture` if ever supported | one file per key, no runtime permutations | each is a distinct algorithm, not a state permutation |
| **B — generic stages** | the non-program stage path: blend/depth state plus texgen | texgen × alpha-test × vertex-colour | the classic renderer draws many ordinary stages; these need VS/PS too |

Tier B bounds:

- `texgen_t` has 8 values (`neo/renderer/Material.h:148`): explicit, diffuse cube, reflect cube,
  skybox cube, wobblesky cube, screen, screen2, glasswarp.
- alpha test: on/off.
- vertex colour: ignore / modulate / inverse-modulate.

`glasswarp` appears in both lists: Tier A owns the program the engine binds for `TG_GLASSWARP`
(`neo/renderer/draw_common.cpp:155`), so Tier B must not duplicate it. That removes one texgen value
from Tier B:

Upper bound is 7 × 2 × 3 = **42** variants. Blend, depth, stencil and cull are **pipeline state**,
not shader variants; they must not multiply this number. The actual shipped subset is whatever the
material census observes, and it is recorded rather than guessed.

---

## 4. Source language and build graph

**Decision D-L1: HLSL is the single source language.**

```text
shaders/classic/**/*.hlsl
  -> DXC -> DXIL (SM 6.x)                  -> D3D12
  -> DXC -> SPIR-V (Vulkan 1.1 target)
       -> Tint (pinned) -> WGSL            -> WebGPU
```

Why not GLSL: the D3D12 flagship needs DXIL, and a GLSL source would add a second translator and a
second set of semantics to keep in parity.

Why not Slang (yet): it can emit WGSL directly, but its WGSL target has documented gaps; adopting it
is a separate decision with its own evidence. HLSL → SPIR-V → Tint is the route with the most
existing tooling.

Non-negotiables:

- Pin DXC, Tint and Emscripten/Dawn versions in `shaders/tools/compile.ps1` and the build preset.
  An unpinned shader toolchain makes "it compiled yesterday" meaningless.
- Validate every generated WGSL module (Tint parse + validation), not just the DXIL.
- Compile every program for both targets in CI; a D3D12-only pass is not a shader pass.
- Generated blobs are build artifacts, not tracked sources (decision D-P3).

Open: the exact DXC/Tint versions are not chosen in this document. Pinning them is T-023.

---

## 5. Definition of "owned"

A manifest entry may say `owned` only when all of these are true:

1. The `.hlsl` file exists under `shaders/classic/programs/` with the key as its file name.
2. It compiles to DXIL and to SPIR-V, and the SPIR-V validates through Tint to WGSL.
3. A reflection check proves it declares no input outside `d3_abi.hlsli` (T-026).
4. A provenance record exists with `status: authored`, a matching hash, and
   `retail_text_consulted` answered (T-025).
5. A test exercises the program: at minimum a compile-and-reflect test; for parity programs, a
   rendered comparison (T-027).
6. The manifest entry has `source`, `entry` and `provenance` filled.

Anything less is `stub`, or the program is not listed in the shipping manifest at all. `planned` is
not a resolver status: a plan is not a program, and a shipping manifest that names one would be
rejected as `MANIFEST_INVALID`.

---

## 6. Review contract

| Change | Required review | Evidence in the PR |
|---|---|---|
| `d3_abi.hlsli` (any field, offset, binding) | shader owner **and** renderer owner | reflection diff, both-backend compile, affected program list |
| new program file | shader owner | provenance record, manifest entry, compile + reflect output |
| program behaviour change | shader owner + renderer owner | before/after frame or a stated reason parity cannot be captured yet |
| manifest status change to `owned` | shader owner | §5 checklist |
| manifest status change to `unsupported`/`absent` | shader owner | the decision and its reason in `plan.md` |
| Tier B variant added | shader owner | the material census row that requires it |
| build graph / toolchain pin | shader owner + build owner | version numbers and a clean compile log |
| delete a program | shader owner + renderer owner | proof no data references it |

One writer per file is not optional. Two agents editing `d3_abi.hlsli` in parallel will produce a
layout that neither tested.

---

## 7. File and naming rules

| Rule | Reason |
|---|---|
| file name = manifest key + `.hlsl` | the resolver and the file tree cannot drift |
| entry points `vs_main` / `ps_main` | one convention, declared in the manifest |
| include prefix `d3_` | no collision with DXC or platform headers |
| include guards `D3_ABI_HLSLI` style | DXC still needs them |
| no numeric register names (`c4`, `cb0`) in program code | names carry meaning; numbers are a backend detail |
| fp32 only | parity with the ARB programs; no half |
| no time, random, or driver queries | deterministic compile and test |
| no `#include` of engine headers | the shader compiles without the engine tree |
| one program per file | simplifies manifest, provenance and review |

---

## 8. Change process

1. Work on a branch/worktree for the issue ([`../../docs/GIT_WORKFLOW.md`](../../docs/GIT_WORKFLOW.md)).
2. Add or edit the program, the manifest entry, and the provenance record in the same PR.
3. Run `shaders/tools/compile.ps1 -All` and `reflect.ps1`; attach the summary, not the full log.
4. Run the resolver vectors and census verification (they must still pass).
5. Request the reviews in §6.
6. Merge only into a clean, current `main`; never push to `main`.

No shader change is "docs only". Even a comment change in `d3_abi.hlsli` can alter a hash and
invalidate golden data.

---

## 9. Current state

| Item | State |
|---|---|
| owner role | assigned to the S2 workstream |
| `shaders/` tree | does not exist |
| manifest | exists only as the fixture in [`tools/resolver-vectors.json`](tools/resolver-vectors.json) |
| programs authored | 0 of 6 (5 required + optional `test`) |
| provenance records with a hash | 0 |
| toolchain pins | not chosen |
| Tier B variants | not designed beyond the bound in §3.1 |
| tests | none; the vectors test the resolver model, not a shader |

---

## 10. Open questions

1. Which DXC and Tint versions become the pin, and which DXC SPIR-V target environment
   (`vulkan1.1` vs `vulkan1.2`) is required by the Dawn/Tint version in use?
2. Does Tier B generate one shader per texgen mode, or one shader with a uniform branch? The first
   is simpler to validate; the second risks non-uniform control flow around sampling.
3. Should `test.vfp` be authored at all, or is the second reference implementation a waste of
   effort? (Current plan: optional, PV-006.)
4. Who reviews the first `d3_abi.hlsli` when the renderer owner and shader owner are the same
   workstream? The first implementation PR needs an independent reviewer by the repository's review
   rule, not a self-approval.
