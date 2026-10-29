# Tasks: classic shaders

**Format**: `[S]` = specification work (this session), `[I]` = implementation, `[G]` = gate/blocker.
Status is `DONE` only with evidence in the evidence column. Review state is separate from status:
a task can be done and still await independent review.

**Rule**: `DONE` for a check requires a command and its output. `DONE` for document authoring requires the artifact plus the package-level `verify-docs.ps1` pass recorded in T-013. Neither means the engine work exists.

---

## Summary

| Scope | Done | Total | State |
|---|---|---:|---|
| Specification (S) | 19 | 19 | complete; self-reviewed; three review rounds |
| Implementation (I) | 3 | 11 | 3 done, 7 partial, 1 blocked |
| Gates (G) | 0 | 2 | blocked |
| **All** | **22** | **32** | 22 done, 7 partial, 3 blocked |

Review: 19 specification tasks self-reviewed, two package-level agent reviews (`review.md` §2.1, §2.2),
and a third round (`review.md` §2.3) that audited source truth and mutation-tested the gates. The
third round found 25 defects, including 8 gates that could pass with their own subject broken; all are
fixed and each fix has a mutation control. No human review yet. 7 implementation tasks are `PARTIAL`:
the work exists and is tested standalone, but the engine wiring or the parity evidence does not.
3 are blocked.

`PARTIAL` means: the artifact exists, its own check passes, and the acceptance criterion is only
partly met, with the missing half named in the evidence column. It is never counted as done.

---

## A. Specification

| Id | Task | Status | Evidence | Review |
|---|---|---|---|---|
| T-001 | Read the pristine source and trace the program table, bind sites, material keywords and init order | DONE | anchors throughout `census.md`, `abi.md`, `resolver.md`; two independent source audits | self-reviewed |
| T-002 | Build a source-derived census extractor with drift detection | DONE | `tools/census.ps1` → `entries=18 files=13 live=8 debug=2 non-arb2=5 dead=3 material_calls=6`; `-Verify` exit 0 | self-reviewed |
| T-003 | Write the census with mechanical classification and the authoring/dead sets | DONE | `census.md`, generated blocks verified | self-reviewed |
| T-004 | Document the runtime census procedure, recording rules and skip semantics | DONE | `census.md` §7 | self-reviewed |
| T-005 | Write the provenance policy, record template and program register | DONE | `provenance.md` §2–§4 | self-reviewed |
| T-006 | Build a gate that rejects tracked shader source and ARB program bodies | DONE | `tools/provenance-check.ps1` → `PROVENANCE OK`; two negative controls in `evidence/implementation.md` | self-reviewed |
| T-007 | Write the fail-loud resolver contract: invariants, codes, modes, timing, diagnostics | DONE | `resolver.md` §2–§9 | self-reviewed |
| T-008 | Turn the resolver contract into executable vectors | DONE | `tools/resolver-conformance.ps1` → `RESOLVER VECTORS: 14/14 passed` | self-reviewed |
| T-009 | Document the engine → program ABI from source | DONE | `abi.md` §1–§7 (c[4]/c[5] semantics corrected in round 3) | self-reviewed |
| T-010 | Define the HLSL library owner, layout, review contract and build graph | DONE | `ownership.md` §1–§9 | self-reviewed |
| T-011 | Write the specification, plan and task list | DONE | `spec.md`, `plan.md`, this file | self-reviewed |
| T-012 | Self-review the package and record findings | DONE | `review.md` | self-reviewed |
| T-013 | Capture verification evidence (commands, exit codes, outputs) | DONE | `evidence/verification.md`, `evidence/implementation.md` | self-reviewed |
| T-014 | Render the census as a review artifact and capture a screenshot | DONE | `evidence/census.html`, `evidence/census.png` | self-reviewed |
| T-015 | Extract a reusable resolver model and refactor the conformance harness onto it | DONE | `tools/resolver-model.ps1`; `resolver-conformance.ps1` still 14/14 | self-reviewed |
| T-016 | Cross-artifact check: census → authoring plan → resolver, with stage and provenance coverage | DONE | `tools/integration-check.ps1` → `18 requests (10 OK, 8 ABSENT), 14 plan keys, 6 authored`; now also validates plan source paths | self-reviewed |
| T-017 | Deterministic property/fuzz test over names, keys, statuses and modes | DONE | `tools/fuzz-resolver.ps1` → `10162 distinct requests (24000 calls), 0 failures` (backslash coverage added in round 3) | self-reviewed |
| T-018 | Content-assert critical anchors, not just line ranges | DONE | `tools/anchors.json`; `verify-docs.ps1` → `70 asserted, 0 failed; cited 70/70`, with a fixed 70-anchor floor | self-reviewed |
| T-019 | Browser-verified package report with a screenshot | DONE | `tools/report.ps1` → `evidence/report.html`; Playwright assertions → `evidence/report.png` | self-reviewed |

---

## B. Implementation

Order follows `plan.md` §4 Phase C/D. Issue #29 authorizes this scope. Engine-file edits (the renderer
wiring) remain outside this workstream by `plan.md` §5, which is why those rows are `PARTIAL` rather
than done: the resolver core, its tests and the shader library are port-owned code under `src/`,
`tests/` and `shaders/`, not edits to the engine fork.

| Id | Task | Depends on | Status | Acceptance / evidence |
|---|---|---|---|---|
| T-020 | Create `shaders/` tree, `d3_abi.hlsli`, `d3_input.hlsli`, `d3_arb.hlsli` skeleton | T-009, T-010 | DONE | all three headers exist under `shaders/classic/include/`; 11 stage compiles succeed for DXIL and SPIR-V; `reflect.ps1` shows only declared inputs and cbuffers |
| T-021 | Implement the resolver core: manifest load, record/status, draw gating, init-order fix, reload re-resolution | T-007, T-008 | PARTIAL | `src/renderer/shader/shader_resolver.cpp` + 6 ctest cases (vectors 14/14, phase 2, diagnostics, reload). **Missing:** the renderer call sites (`R_LoadARBProgram`, `R_FindARBProgram`, `R_InitMaterials` order) are engine files, untouched. |
| T-022 | Make material program parsing atomic; replace raw program ints with a validated handle | T-021 | PARTIAL | atomic pair resolution + `material_atomic` test prove neither stage installs from a one-sided file. **Missing:** `Material.cpp` parse and `newShaderStage_t` still hold raw ints. |
| T-023 | Pin DXC/Tint/Emscripten and implement `compile.ps1` / `reflect.ps1` | T-020 | PARTIAL | `pins.json` pins DXC 1.9.2609.5 and Tint (Dawn `106704a9ac3f`, asset SHA-256 verified); `compile.ps1` emits DXIL + SPIR-V + WGSL and re-parses the WGSL; `verify-golden.ps1` hashes all three. **Missing:** the Emscripten pin, which belongs to the web target and is not invoked by this library. |
| T-024 | Engine tests: missing, one-sided, compile-failed, tolerant, reload | T-021, T-022 | PARTIAL | `tests/shader` covers all five behaviours against the core and a mutation control fails as it must. **Missing:** an in-engine test; no engine wiring exists yet. |
| T-025 | Author `shadow.hlsl` + manifest entry + provenance record | T-020, T-023 | PARTIAL | `shaders/classic/programs/shadow.hlsl` compiles DXIL + SPIR-V, manifest `stub`, PV-004 hashed. **Missing:** the shadow pass has not been observed in a running engine. |
| T-026 | Reflection/ABI conformance test for every program | T-020, T-025, T-030 | DONE | `shaders/tools/reflect.ps1` → `REFLECT OK: 11 stages, 0 violations`; also proves each fragment input signature equals its vertex output signature. Limit: the reflection itself reads DXIL; the WGSL side is validated separately by `compile.ps1` through the pinned Tint. |
| T-027 | Parity capture for the classic passes | T-025, T-028, T-030 | BLOCKED | needs authorized retail data and a running classic backend; neither exists |
| T-028 | Tier B generic stage shaders within the 42-variant bound | T-020, T-023 | PARTIAL | `generic_texgen.hlsl` compiled across the full bound: `BOUND PROOF OK: 42 variants x 2 stages = 84 compiles, 0 failures`. **Missing:** the shipped set is empty because `variants.json` is populated only from the runtime census, which is blocked. |
| T-029 | `shader_status` / `shader_reload` console commands with counters | T-021 | PARTIAL | `StatusLine()` and the reload before/after report are implemented and asserted with computed counts. **Missing:** the console command registration is an engine file. |
| T-030 | Author `interaction.hlsl`, `environment.hlsl`, `bumpyenvironment.hlsl`, `arbfp_glasswarp.hlsl`, optional `test.hlsl` | T-025, T-026 | PARTIAL | all five required programs plus the optional debug program exist, compile to DXIL + SPIR-V + validated WGSL and are hashed (PV-001…PV-006). **Missing:** none is `owned` — the rendered comparison in `ownership.md` §5 criterion 5 (T-027) is blocked, so the manifest says `stub`. |

---

## C. Gates and blockers

| Id | Gate | Status | Why blocked | What unblocks it |
|---|---|---|---|---|
| T-031 | Runtime census on authorized local data | BLOCKED | no `*.pk4` in the worktree or default install path; data is EULA-bound | authorized local install + the procedure in `census.md` §7 |
| T-032 | Legal review of the derivative-work position | BLOCKED | no counsel review has occurred | a reviewer decision; P-1..P-5 remain conservative until then |

A blocked gate is never reported as passed. The package is complete **with** these two known gaps.

---

## D. What "done" does not mean

- The specification being complete does **not** mean classic shaders are ported.
- The resolver core passing its tests does **not** mean the engine uses it.
- Six programs compiling for two targets does **not** mean they are `owned`, and `owned` is required
  before they may be drawn.
- The provenance gate passing does **not** mean the legal position is settled.
- The 42-variant bound compiling does **not** mean Tier B ships anything.

---

## E. Next actions, in order

1. Independent review of the implementation (`review.md` §4 checklist) — the repository rule forbids
   self-approving the first engine PR, so the resolver core needs a reviewer who did not write it.
2. Wire the resolver into the renderer under its own issue: `R_LoadARBProgram`, `R_FindARBProgram`,
   `Material.cpp` atomic parse, the draw gate and the two console commands (the `PARTIAL` half of
   T-021, T-022, T-024, T-029).
3. Pin Emscripten in the web target's toolchain; Tint is pinned and WGSL is validated here, so the last
   missing `owned` criterion is the rendered comparison (T-027), not the shader build.
4. When authorized data and a running classic backend exist, run T-031 before claiming census
   completeness and T-027 before promoting any program to `owned`.
