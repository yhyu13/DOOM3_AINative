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
| Implementation (I) | 5 | 11 | 5 done, 5 partial, 1 blocked |
| Gates (G) | 0 | 2 | blocked |
| **All** | **24** | **32** | 24 done, 5 partial, 3 blocked |

Review: 19 specification tasks self-reviewed, two package-level agent reviews (`review.md` §2.1, §2.2),
and a third round (`review.md` §2.3) that audited source truth and mutation-tested the gates. The
third round found 25 defects, including 8 gates that could pass with their own subject broken; all are
fixed and each fix has a mutation control. No human review yet. 5 implementation tasks are `PARTIAL`:
the artifact exists and its own check passes, but a named half is missing. 1 implementation task and
both gates are blocked. The engine wiring landed on 2026-10-29 (see `evidence/implementation.md` §12).

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

Order follows `plan.md` §4 Phase C/D. Issue #29 authorizes this scope. Engine call sites were delivered
on 2026-10-29 as modified-file hunks in the tracked target patch
(`targets/win11-amd64/patches/all-changes.patch`), which is the established delivery path for this
target; the pristine submodule is untouched and no added/deleted engine file exists. The classic GL
backend still loads ARB text from data, so the authored HLSL programs are not bound by these call
sites; they gate and report, they do not replace the GL program objects.

| Id | Task | Depends on | Status | Acceptance / evidence |
|---|---|---|---|---|
| T-020 | Create `shaders/` tree, `d3_abi.hlsli`, `d3_input.hlsli`, `d3_arb.hlsli` skeleton | T-009, T-010 | DONE | all three headers exist under `shaders/classic/include/`; 11 stage compiles succeed for DXIL and SPIR-V; `reflect.ps1` shows only declared inputs and cbuffers |
| T-021 | Implement the resolver core: manifest load, record/status, draw gating, init-order fix, reload re-resolution | T-007, T-008 | PARTIAL | Engine call sites now exist through the tracked target patch: `R_LoadARBProgram` publishes an id only after a successful compile, `R_FindARBProgram` is replaced by phase-1 handles, `R_InitShaderResolver` runs before `R_InitMaterials`, and draw/bind sites gate on both phases (evidence §12). **Missing:** tolerant mode logs the error-material decision and skips the stage, but does not yet draw the magenta substitution (I-5); the engine default is `off` while `owned=0`, so strict is an evidence setting, not the shipping default. |
| T-022 | Make material program parsing atomic; replace raw program ints with a validated handle | T-021 | DONE | `newShaderStage_t` carries `shaderProgramHandle_t` (key + code + requested/blocked), `program` resolves as a pair, and a one-sided file installs neither stage. In-engine `shader_selftest` case 2 passes; `material_atomic` ctest passes (evidence §12.3). |
| T-023 | Pin DXC/Tint/Emscripten and implement `compile.ps1` / `reflect.ps1` | T-020 | PARTIAL | `pins.json` pins DXC 1.9.2609.5 and Tint (Dawn `106704a9ac3f`, asset SHA-256 verified); `compile.ps1` emits DXIL + SPIR-V + WGSL and re-parses the WGSL; `verify-golden.ps1` hashes all three. **Missing:** the Emscripten pin, which belongs to the web target and is not invoked by this library. |
| T-024 | Engine tests: missing, one-sided, compile-failed, tolerant, reload | T-021, T-022 | DONE | `shader_selftest` runs the five cases inside `DOOM3.exe` against local resolver instances: `SHADER SELFTEST: PASS 5/5`, 8/8 markers via `tests/shader/run-engine-selftest.ps1` (exit 0). The core ctest suite is still 6/6 (evidence §12.3–§12.4). |
| T-025 | Author `shadow.hlsl` + manifest entry + provenance record | T-020, T-023 | PARTIAL | `shaders/classic/programs/shadow.hlsl` compiles DXIL + SPIR-V, manifest `stub`, PV-004 hashed. The shadow draw path is now gated by the resolver. **Missing:** the shadow pass has not been compared in a running engine; the classic GL backend still consumes retail ARB text, so the authored HLSL is not bound there. |
| T-026 | Reflection/ABI conformance test for every program | T-020, T-025, T-030 | DONE | `shaders/tools/reflect.ps1` → `REFLECT OK: 11 stages, 0 violations`; also proves each fragment input signature equals its vertex output signature. Limit: the reflection itself reads DXIL; the WGSL side is validated separately by `compile.ps1` through the pinned Tint. |
| T-027 | Parity capture for the classic passes | T-025, T-028, T-030 | BLOCKED | needs authorized retail data and a running classic backend; neither exists |
| T-028 | Tier B generic stage shaders within the 42-variant bound | T-020, T-023 | PARTIAL | `generic_texgen.hlsl` compiled across the full bound: `BOUND PROOF OK: 42 variants x 2 stages = 84 compiles, 0 failures`. **Missing:** the shipped set is empty because `variants.json` is populated only from the runtime census, which is blocked. |
| T-029 | `shader_status` / `shader_reload` console commands with counters | T-021 | DONE | Registered in `R_InitCommands`; `shader_status` prints the manifest/request/non-OK counts and `shader_reload` prints the before/after delta. Observed in `qconsole.log` in off and strict modes (evidence §12.2–§12.5). |
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
   self-approving the first engine PR, so the resolver core and wiring need a reviewer who did not
   write them.
2. Tolerant-mode visual substitution: the gate skips a non-OK stage in both modes, but the magenta
   error material required by I-5 is not drawn yet. Wire it before promoting any program to `owned`.
3. Pin Emscripten in the web target's toolchain; Tint is pinned and WGSL is validated here, so the last
   missing `owned` criterion is the rendered comparison (T-027), not the shader build.
4. When authorized data and a running classic backend exist, run T-031 before claiming census
   completeness and T-027 before promoting any program to `owned`.
