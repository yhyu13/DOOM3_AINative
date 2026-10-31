# Feature specification: classic shader census, provenance and fail-loud resolution

**Feature**: `008-classic-shaders`
**Status**: specification **complete and reviewed three times**; implementation **6 of 11 tasks done,
5 blocked** (2026-11-01). Six authored programs compile for DXIL, SPIR-V and validated WGSL; the resolver
core is implemented and tested standalone; all three toolchain identities are pinned (DXC, Tint,
Emscripten); and the resolver is wired into the renderer through the tracked target patch (call sites,
atomic material parse, draw gate, `shader_status`/`shader_reload`, in-engine self-test). Five tasks are
`BLOCKED` on one input — authorized game data plus a running classic backend — so no program is
`owned`, tolerant mode does not yet draw the visible error material, and no parity capture or runtime
census exists. See `tasks.md` section B and `evidence/implementation.md` §12–§14.
**Input**: the S2 workstream of the renderer plan: program census, provenance, fail-loud resolver,
HLSL library ownership.

---

## 1. Problem

DOOM 3's classic ARB2 renderer cannot be ported by translation, because its shader programs are not
in the GPL source release. They live in EULA-bound game data as ARB assembly and are loaded by name
at runtime (`neo/renderer/draw_arb2.cpp:368`). The port must author its own programs.

Three things are missing before a single shader can be written responsibly:

1. **A truthful inventory.** Which programs does the engine actually require, which are dead, and
   which can arrive from data and therefore cannot be enumerated from source?
2. **A failure model.** Today a missing program prints a line and the renderer draws anyway
   (`neo/renderer/draw_arb2.cpp:379`, `neo/renderer/draw_common.cpp:837`). Unlit geometry looks
   like a lighting bug, not a missing file.
3. **Ownership and provenance.** Who owns the HLSL library, what may a program depend on, and where
   did its code come from?

Without these, the first shader PR becomes an unreviewable mix of reverse-engineering, ABI drift
and silent regressions.

---

## 2. What this feature is, and is not

**Is**: a source-anchored plan, an executable census/verification harness, a resolver contract with
conformance vectors, an ABI contract, and an ownership model.

**Is not**:

- an implementation of the renderer or of any shader (tasks T-020 onward);
- a claim that classic parity is achieved or achievable without retail data;
- a licence opinion;
- an edit to `specs/003-render-modernization/**` or `specs/004-quality-renderer/**` (owned by the
  publish/contract workstream);
- a browser or WebGPU implementation.

---

## 3. Glossary

| Term | Meaning |
|---|---|
| **program** | a vertex or fragment shader pair named by the engine, e.g. `interaction.vfp` |
| **key** | normalized lookup name: last extension stripped, lowercased, path preserved |
| **census** | the classified inventory of programs the engine can request |
| **manifest** | versioned JSON that maps every key to stages, status and provenance |
| **resolver** | the component that turns a request into a status before any draw |
| **owned** | a program that exists, compiles on both backends, is tested and has provenance |
| **Tier A / Tier B** | named classic programs / generic stage shaders driven by texgen and state |
| **SKIPPED** | a check that could not run; never equivalent to a pass |

---

## 4. Requirements

### 4.1 Census

- **FR-001** — A checked-in script MUST derive the static program table, its bind sites and the
  material program keywords from the pristine engine source, and MUST fail when the checked-in
  census drifts from the source.
- **FR-002** — Every static entry MUST be classified as `live-default`, `debug-only`, `non-ARB2` or
  `dead`, with the classification rule stated and mechanically applied.
- **FR-003** — The census MUST state which programs are outside the static table and cannot be
  enumerated from source, including material-declared names and `megaTexture.vfp`.
- **FR-004** — A runtime census procedure on authorized local data MUST be documented, including
  what may be recorded (names, counts) and what may not (shader text, archives). Its status MUST be
  `SKIPPED` when it has not run, with the reason; a skip MUST NOT be reported as a pass.

### 4.2 Fail-loud resolution

- **FR-005** — No draw MUST execute with a program whose status is not `OK`. This MUST hold in both
  `strict` and `tolerant` modes.
- **FR-006** — Every non-OK record MUST carry material, stage index, keyword, program name,
  normalized key, target, code and mode.
- **FR-007** — The resolver MUST be manifest-driven and MUST NOT probe the filesystem or compile at
  draw time.
- **FR-008** — The `program` keyword MUST be atomic: a file that provides only one stage MUST NOT
  install either stage.
- **FR-009** — The manifest MUST be the single source of truth for stages, status and provenance,
  and a check MUST prove the compiled engine table is covered by it.
- **FR-010** — The resolver MUST preserve DOOM 3 key semantics (extension-insensitive,
  case-insensitive, path-sensitive) and MUST reject duplicate keys as `AMBIGUOUS` instead of
  silently taking the first.
- **FR-011** — Resolution MUST be valid across the engine's initialization order: a material parsed
  before GL initialization MUST NOT bake a permanent unresolved state, and reload MUST re-resolve
  every request.
- **FR-012** — `shader_status` MUST report manifest counts, request counts, non-OK counts and the
  current mode, and a reload MUST report a before/after change.
- **FR-023** — Resolution MUST be two-phase: name/manifest resolution at material parse (no device),
  blob binding at device init and reload. A draw MUST gate on both phases, so a material parsed
  before the device exists cannot bake a false success or a permanent failure.

### 4.3 Provenance

- **FR-013** — Every program MUST have a provenance record before its first merge, with a
  `retail_text_consulted` answer and a hash of the authored file.
- **FR-014** — No retail shader text, translated text, or derived fixture MUST be committed, and a
  checked-in script MUST fail when a tracked file looks like shader source or an ARB program body.
- **FR-015** — Visual parity MUST NOT be treated as evidence of provenance.

### 4.4 Ownership and ABI

- **FR-016** — A named owner role MUST exist for the HLSL library, with an owned/not-owned scope
  split, a review contract and a definition of `owned`.
- **FR-017** — The engine-to-program ABI MUST be documented from source: vertex attributes, texture
  bindings, constant registers and their lifetimes, local parameters and fragment maps.
- **FR-018** — The ABI MUST state which values are dual-written and MUST require per-program-family
  constant buffers, so a single global frame UBO cannot silently change meaning.
- **FR-019** — The source language and build graph MUST be decided, with DXIL and SPIR-V → WGSL
  targets, pinned tool versions, and validation on both backends.
- **FR-020** — The generic stage path MUST be bounded: texgen × alpha-test × vertex-colour only,
  with blend/depth/stencil/cull as pipeline state, not shader variants.

### 4.5 Decisions that must be explicit

- **FR-021** — `megaTexture.vfp` MUST have an explicit support, stub or unsupported decision with a
  reason.
- **FR-022** — Dead and removed-backend programs MUST be classified, not authored, and a data
  reference to them MUST be reported rather than silently bound.

---

## 5. User scenarios

### US1 — Reviewing shader completeness

A reviewer asks "what must we write?" and gets a five-file answer with bind-site evidence and a
counts block that a script re-derives. They also get an explicit list of what will **not** be
written and why. No retail data is needed.

### US2 — Adding a shader

A developer adds `interaction.hlsl`. The manifest entry, provenance record, ABI conformance and
two-backend compile are part of the same PR. The review contract tells them who signs off.

### US3 — A material references a missing program

In `strict` mode the material is rejected with one structured error naming the material, stage,
program, key and code; the frame does not draw the broken stage. `shader_status` shows the count.
In `tolerant` mode a magenta/black error material appears instead of an unlit surface.

### US4 — A CI gate on legal and factual drift

CI runs the census check, the doc-anchor check, the resolver vectors and the provenance gate. A
source change that adds a program without updating the manifest, or a commit that adds a `.vfp`,
fails the build.

---

## 6. Success criteria

| ID | Criterion | Evidence |
|---|---|---|
| SC-001 | Static census is reproducible and drift-detecting | `census.ps1 -Verify` exits 0 |
| SC-002 | The census numbers in prose match the extraction | generated blocks in `census.md` |
| SC-003 | The resolver decision table is executable | `resolver-conformance.ps1` 14/14 |
| SC-004 | Every source anchor points at an existing file within the cited range; 70 critical anchors match an expected token on the exact line; every relative link resolves | `verify-docs.ps1` exits 0 (`CONTENT: 70 asserted, 0 failed`) |
| SC-005 | The provenance gate rejects shader source and ARB bodies | `provenance-check.ps1` exits 0 |
| SC-006 | The package is reviewable as a rendered artifact | `census.html` + screenshot in `evidence/` |
| SC-007 | A reviewer can answer "what do we author first" from `census.md` §5.1 without reading code | review |
| SC-008 | Every check task has a command and its output; document tasks have an artifact plus the package-level doc check | `tasks.md` evidence column; T-013 |
| SC-009 | Every census key is covered once by the authoring plan with matching stages and a provenance id, and every static request resolves to the expected code | `integration-check.ps1` exits 0 |
| SC-010 | Resolver invariants hold over at least 10,000 distinct (name, keyword) requests | `fuzz-resolver.ps1` exits 0 with `distinct requests >= 10000` |

---

## 7. Out of scope

- Implementing the resolver in C++ (T-021 onward).
- Authoring any shader (T-025 onward).
- Obtaining, extracting or committing retail data.
- A legal opinion on derivative works.
- Changing `specs/003` or `specs/004`.
- WebGPU/WebGL build work; the shader build graph is defined, not executed.
- Performance or visual parity measurement.

---

## 8. Assumptions

1. The pristine submodule is the authoritative engine source; line numbers are valid for the pinned
   SHA and will drift on a bump.
2. Game data is not available in the repository and must not be added.
3. The five live-default programs are the correct first authoring set, subject to the runtime
   census.
4. HLSL is an acceptable single source for both DXIL and SPIR-V → WGSL. This is a decision
   (D-L1), not a measured result.
5. The renderer plan's "six live programs" figure is a lower bound; this spec corrects it to five
   live-default files plus one optional debug file, and records the dead entries separately.

---

## 9. Dependencies and risks

| Risk | Impact | Mitigation |
|---|---|---|
| Retail data unavailable | runtime census cannot run; completeness unproven | procedure ready; report SKIPPED, never a pass |
| Legal position unresolved | authoring may need a different clean-room process | provenance rules; counsel gate before retail study |
| ABI guessed wrong | every shader wrong in the same way | ABI derived from source; reflection test; one header |
| Manifest and engine table drift | silent coverage gap | FR-009 check |
| Resolver over-engineered | blocks Q1a/Q1b | contract only; vectors prove semantics; implementation is separable |
| Toolchain unpinned | non-reproducible shader builds | pin in T-023 before the first program |
| "Owned" claimed too early | provenance and tests skipped | §5 definition in `ownership.md` |

---

## 10. Traceability

| Requirement | Artifact | Verification |
|---|---|---|
| FR-001..FR-003 | `census.md`, `census.json`, `tools/census.ps1` | SC-001, SC-002 |
| FR-004 | `census.md` §7 | review; blocked run |
| FR-005..FR-012, FR-023 | `resolver.md`, `tools/resolver-vectors.json` | SC-003 |
| FR-013..FR-015 | `provenance.md`, `tools/provenance-check.ps1` | SC-005 |
| FR-016..FR-020 | `ownership.md`, `abi.md` | SC-004, review |
| FR-021..FR-022 | `plan.md` decisions, `census.md` §5.2 | review |
| FR-009, FR-023 | `tools/authoring-plan.json`, `tools/integration-check.ps1` | SC-009 |
| FR-005..FR-012 | `tools/fuzz-resolver.ps1` | SC-010 |
| all | `review.md`, `evidence/verification.md`, `evidence/report.html` | SC-006, SC-008 |

Upstream: `specs/003-render-modernization/research.md` (renderer analysis),
`specs/004-quality-renderer/` (RHI and quality plan, Q1a contract). This spec supplies the shader
work those documents schedule but do not specify. It does not edit them.

---

## 11. Open questions

1. Does retail data reference `megaTexture.vfp`, and how often? (Runtime census.)
2. Does any material use the `program` keyword (both stages) rather than the per-stage keywords?
3. Is a 63-byte name rejection safe against retail data that currently truncates?
4. Which DXC/Tint versions become the pin?
5. Who is the independent reviewer for the first implementation PR?
