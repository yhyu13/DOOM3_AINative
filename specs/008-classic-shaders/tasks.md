# Tasks: classic shaders

**Format**: `[S]` = specification work (this session), `[I]` = implementation, `[G]` = gate/blocker.
Status is `DONE` only with evidence in the evidence column. Review state is separate from status:
a task can be done and still await independent review.

**Rule**: `DONE` for a check requires a command and its output. `DONE` for document authoring requires the artifact plus the package-level `verify-docs.ps1` pass recorded in T-013. Neither means the engine work exists.

---

## Summary

| Scope | Done | Total | State |
|---|---:|---:|---|
| Specification (S) | 19 | 19 | complete, self-reviewed + independently reviewed |
| Implementation (I) | 0 | 11 | not started |
| Gates (G) | 0 | 2 | blocked |
| **All** | **19** | **32** | spec complete; engine work untouched |

Review: 19 tasks self-reviewed; two independent agent reviews ([`review.md`](review.md) §2.1 and
§2.2). The first found seven blocking defects in the original gates; the second audited the round-2
checks and found seven more, all fixed. No human review yet. 11 tasks are not reviewable yet (no
artifact); 2 are blocked.

---

## A. Specification

| Id | Task | Status | Evidence | Review |
|---|---|---|---|---|
| T-001 | Read the pristine source and trace the program table, bind sites, material keywords and init order | DONE | anchors throughout `census.md`, `abi.md`, `resolver.md`; two independent source audits | self-reviewed |
| T-002 | Build a source-derived census extractor with drift detection | DONE | `tools/census.ps1` → `entries=18 files=13 live=8 debug=2 non-arb2=5 dead=3 material_calls=6`; `-Verify` exit 0 | self-reviewed |
| T-003 | Write the census with mechanical classification and the authoring/dead sets | DONE | `census.md`, generated blocks verified | self-reviewed |
| T-004 | Document the runtime census procedure, recording rules and skip semantics | DONE | `census.md` §7 | self-reviewed |
| T-005 | Write the provenance policy, record template and program register | DONE | `provenance.md` §2–§4 | self-reviewed |
| T-006 | Build a gate that rejects tracked shader source and ARB program bodies | DONE | `tools/provenance-check.ps1` → `PROVENANCE OK` (tracked-file count at run time) | self-reviewed |
| T-007 | Write the fail-loud resolver contract: invariants, codes, modes, timing, diagnostics | DONE | `resolver.md` §2–§9 | self-reviewed |
| T-008 | Turn the resolver contract into executable vectors | DONE | `tools/resolver-conformance.ps1` → `RESOLVER VECTORS: 14/14 passed` | self-reviewed |
| T-009 | Document the engine → program ABI from source | DONE | `abi.md` §1–§7 | self-reviewed |
| T-010 | Define the HLSL library owner, layout, review contract and build graph | DONE | `ownership.md` §1–§9 | self-reviewed |
| T-011 | Write the specification, plan and task list | DONE | `spec.md`, `plan.md`, this file | self-reviewed |
| T-012 | Self-review the package and record findings | DONE | `review.md` | self-reviewed |
| T-013 | Capture verification evidence (commands, exit codes, outputs) | DONE | `evidence/verification.md`: census, resolver, docs, provenance all exit 0; negative controls exit 1 | self-reviewed |
| T-014 | Render the census as a review artifact and capture a screenshot | DONE | `evidence/census.html`, `evidence/census.png` | self-reviewed |
| T-015 | Extract a reusable resolver model and refactor the conformance harness onto it | DONE | `tools/resolver-model.ps1`; `resolver-conformance.ps1` still 14/14 | self-reviewed |
| T-016 | Cross-artifact check: census → authoring plan → resolver, with stage and provenance coverage | DONE | `tools/integration-check.ps1` → `18 requests (10 OK, 8 ABSENT), 14 plan keys, 6 authored` | self-reviewed |
| T-017 | Deterministic property/fuzz test over names, keys, statuses and modes | DONE | `tools/fuzz-resolver.ps1` → `10160 distinct requests (24000 calls), 0 failures` | self-reviewed |
| T-018 | Content-assert critical anchors, not just line ranges | DONE | `tools/anchors.json`; `verify-docs.ps1` → `70 asserted, 0 failed; cited 70/70` | self-reviewed |
| T-019 | Browser-verified package report with a screenshot | DONE | `tools/report.ps1` → `evidence/report.html`; Playwright assertions → `evidence/report.png` | self-reviewed |

---

## B. Implementation (not started)

Order follows `plan.md` §4 Phase C/D. Each task needs its own issue and branch under the repository
workflow; none is authorized by this document.

| Id | Task | Depends on | Status | Acceptance |
|---|---|---|---|---|
| T-020 | Create `shaders/` tree, `d3_abi.hlsli`, `d3_input.hlsli`, `d3_arb.hlsli` skeleton | T-009, T-010 | NOT STARTED | header compiles under DXC for both targets; reflection shows only declared inputs |
| T-021 | Implement the resolver core: manifest load, record/status, draw gating, init-order fix, reload re-resolution | T-007, T-008 | NOT STARTED | a synthetic missing program never draws; `shader_status` counts match |
| T-022 | Make material program parsing atomic; replace raw program ints with a validated handle | T-021 | NOT STARTED | a one-sided file rejects both stages; no path binds 0 |
| T-023 | Pin DXC/Tint/Emscripten and implement `compile.ps1` / `reflect.ps1` | T-020 | NOT STARTED | reproducible DXIL and validated WGSL; versions recorded |
| T-024 | Engine tests: missing, one-sided, compile-failed, tolerant, reload | T-021, T-022 | NOT STARTED | red/green/mutation evidence in an evidence file |
| T-025 | Author `shadow.hlsl` + manifest entry + provenance record | T-020, T-023 | NOT STARTED | both targets compile; shadow pass behaves; PV record hashed |
| T-026 | Reflection/ABI conformance test for every program | T-020, T-025, T-030 | NOT STARTED | any undeclared input fails the build |
| T-027 | Parity capture for the classic passes | T-025, T-028, T-030 | NOT STARTED | per-pass before/after, named hardware, reviewer decision |
| T-028 | Tier B generic stage shaders within the 42-variant bound | T-020, T-023 | NOT STARTED | only census-observed variants are built |
| T-029 | `shader_status` / `shader_reload` console commands with counters | T-021 | NOT STARTED | before/after counts; reload re-resolves |
| T-030 | Author `interaction.hlsl`, `environment.hlsl`, `bumpyenvironment.hlsl`, `arbfp_glasswarp.hlsl`, optional `test.hlsl` | T-025, T-026 | NOT STARTED | five (or six) `owned`; zero unresolved on test maps |

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
- Five live-default programs being identified does **not** mean retail data requests only those.
- The resolver vectors passing does **not** mean the engine implements the resolver.
- The provenance gate passing does **not** mean the legal position is settled.

---

## E. Next actions, in order

1. Independent review of this package (`review.md` checklist).
2. Open the implementation issue for T-021/T-022 (resolver core + atomic material parse), because it
   is the highest-risk change and unblocks every later test.
3. Open the issue for T-020/T-023 (library skeleton + toolchain pin) so shader authoring can start
   with a stable ABI.
4. When authorized data exists, run T-031 before claiming census completeness.
