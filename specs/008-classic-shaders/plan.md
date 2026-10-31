# Implementation plan: classic shaders

**Status**: plan **complete for review**; Phase C and part of Phase D are implemented (2026-10-29),
including the engine wiring delivered as a tracked target patch, and all three toolchain identities are
pinned (2026-11-01). The remaining open items are the visible tolerant-mode error material and Phase D
parity — both need authorized game data and a running classic backend — see `tasks.md` section B.
**Companion**: [`spec.md`](spec.md) (requirements), [`tasks.md`](tasks.md) (execution).

This plan converts the S2 workstream — program census, provenance, fail-loud resolver, HLSL library
ownership — into an ordered, reviewable set of decisions and tasks. It deliberately stops before
engine code: the value of S2 is that Q1a/Q2 can start shader work without guessing.

---

## 1. Context and constraints

| Constraint | Consequence |
|---|---|
| Retail shader text is absent and EULA-bound | author from the ABI; runtime census is blocked until legal local data exists |
| The RHI is a D3D12-first design with a WebGPU-shaped portable core | programs need DXIL and WGSL; one HLSL source, two compiled targets |
| The engine resolves programs by string key at runtime | the port must preserve key semantics to keep retail `.mtr` files working |
| The engine draws even when a program failed | the resolver must gate the draw, not just log |
| S0 owns `specs/003`/`004`/`005` | this spec references them and writes nothing there |
| One writer per file | the whole package is a new directory; no shared-file edits |

---

## 2. Deliverables

| File | Purpose | Status |
|---|---|---|
| [`spec.md`](spec.md) | requirements, scenarios, success criteria | complete |
| [`plan.md`](plan.md) | this file: decisions, phases, risks | complete |
| [`tasks.md`](tasks.md) | ordered tasks with evidence and review state | complete |
| [`census.md`](census.md) | classified program inventory, runtime procedure | complete (runtime **skipped**) |
| [`census.json`](census.json) | machine-readable census | generated, verified |
| [`provenance.md`](provenance.md) | legal boundary, register, record template | complete |
| [`resolver.md`](resolver.md) | fail-loud contract, codes, manifest, integration | complete |
| [`abi.md`](abi.md) | engine → program interface | complete |
| [`ownership.md`](ownership.md) | owner role, layout, review contract, build graph | complete |
| [`review.md`](review.md) | self-review findings and reviewer checklist | complete |
| [`evidence/verification.md`](evidence/verification.md) | commands and captured output | complete |
| `tools/*.ps1`, `tools/resolver-vectors.json` | executable checks | complete, passing |
| `tools/resolver-model.ps1` | reusable phase-1 model used by every resolver test | complete |
| `tools/authoring-plan.json` | census-key coverage plan, not a shipping manifest | complete |
| `tools/integration-check.ps1` | census → plan → resolver cross-check | complete, passing |
| `tools/fuzz-resolver.ps1` | deterministic property test | complete, passing |
| `tools/anchors.json` | 70 content assertions for critical anchors | complete, passing |
| `tools/report.ps1`, `evidence/report.html/.png` | browser-verified review report | complete |

---

## 3. Decisions

| Id | Decision | Rationale | Alternatives rejected |
|---|---|---|---|
| D-001 | The static census is derived from source by a checked-in script; the runtime census is a separate, explicitly skippable step | a prose list drifts and cannot be trusted; a skip must be visible | hand-maintained table; claiming completeness from source |
| D-002 | Resolution is manifest-driven and no draw runs without an `OK` status | the current silent draw is the defect | logging only; asserting on `id != 0` |
| D-003 | `megaTexture.vfp` is **unsupported in milestone 1**; materials that request it are reported | virtual texturing needs tiled/sparse feedback and uploads that the portable RHI does not model yet; a stub would hide the gap | stub it; implement it now |
| D-004 | Dead and removed-backend programs (`ambientLight`, `arbVP_glasswarp`, R200, NV20) are classified `absent`, never authored | no compiled bind site exists; authoring them wastes the scarcest work | author "for completeness" |
| D-005 | Key semantics are preserved, but duplicate keys are `AMBIGUOUS` rather than first-wins | data compatibility without reproducing a bug | keep first-wins; case-sensitive keys |
| D-006 | Constant buffers are per program family, not one global frame UBO | `c[6..8]` and fragment `c[0..1]` are dual-written with different meanings | one UBO with mode flags |
| D-007 | Provenance is recorded per program, including `retail_text_consulted` | a batch record hides which file was studied | one project-level statement |
| D-008 | HLSL is the single source; DXIL for D3D12 and SPIR-V → Tint → WGSL for WebGPU | D3D12 is the flagship; a GLSL source adds a second semantics | GLSL source; Slang now |
| D-009 | The library has two tiers: named classic programs and bounded generic stage shaders | the classic path draws ordinary stages too | one undifferentiated pile; unbounded variants |
| D-010 | The resolver contract ships with executable vectors | prose decision tables are ambiguous; vectors are a test seam | prose only |
| D-011 | `strict` is the default for Q1/Q2 evidence builds; `tolerant` is the release fallback | an evidence build must not hide failures | tolerant by default |
| D-012 | This spec does not edit `003`/`004`/`005` | those files belong to the publish/contract workstream; parallel edits break one-writer | patching the RHI contract from here |

Sub-decisions defined elsewhere: D-L1 (HLSL single source) in [`ownership.md`](ownership.md) §4;
D-P1..D-P4 (provenance) in [`provenance.md`](provenance.md) §6.

---

## 4. Phases

### Phase A — Specification and harness (this session, complete)

1. Read the pristine source; extract the static table and bind sites.
2. Write the census, resolver, ABI, provenance and ownership documents.
3. Build six executable gate checks (census drift, resolver vectors, census-plan integration, property test, doc anchors/links, provenance) plus two review-artifact generators (census HTML, report).
4. Self-review and capture evidence.

Exit: all checks exit 0; every number in prose is reproducible.

### Phase B — Runtime census (blocked)

Blocked on authorized local data and, before that, on a legal position. The procedure is in
[`census.md`](census.md) §7. Do not start by downloading data "to check"; that is the risk the
provenance rules exist to prevent.

Exit: requested set ⊆ manifest; zero unresolved on the parity map set; evidence file records counts
and names only.

### Phase C — Resolver core and library skeleton (next implementation)

Ordered so that each step is independently testable:

1. Manifest loader + record type + status enum (T-021).
2. Material parse made atomic; stage handle instead of raw int (T-022).
3. Draw gating; bind-0 becomes an assertion (T-021/T-024).
4. Init-order fix and reload re-resolution (T-021).
5. `shaders/` skeleton + ABI header + toolchain pins (T-020, T-023).
6. Engine tests for missing, one-sided, compile-failed and tolerant paths (T-024).

Exit: a synthetic material with a missing program fails loudly in strict mode and draws the error
material in tolerant mode; no frame draws an unresolved stage.

### Phase D — Author the required programs

Order by testability, not by importance in the frame:

| Order | Program | Why this position |
|---:|---|---|
| 1 | `shadow.vp` | vertex-only, isolated pass, easiest to validate against a stencil-volume capture |
| 2 | `interaction.vfp` | critical path; uses the full seven-texture and colour-policy contract |
| 3 | `environment.vfp` | cube reflection without bump; shares code with the next item |
| 4 | `bumpyEnvironment.vfp` | same family, adds the bump matrix |
| 5 | `arbFP_glasswarp.txt` | fragment-only, needs scratch textures and screen texgen |
| 6 | `test.vfp` (optional) | second implementation; drop it if it stops earning its keep |

Each program ships with: manifest entry, provenance record, DXIL + SPIR-V/WGSL compile, reflection
check, and a parity or behavioural test.

Exit: `shader_status` shows `owned=5` (or 6), `unresolved=0` on the test maps.

### Phase E — Tier B and parity

1. Design the generic stage variants within the 42 bound (T-028).
2. Capture parity per pass: depth prepass, shadows, interactions, light scale, opaque stages, fog,
   post-process, GUI (T-027).
3. Revisit `megaTexture` with measured data from the runtime census (decision D-003).

Exit: classic parity measured separately from the modern path, with named hardware and a reviewer
decision (this is the Q3/Q4 gate, not this spec's exit).

---

## 5. Interfaces with other workstreams

| Workstream | Interface | What it gets from this spec |
|---|---|---|
| S0 (RHI contract/publish) | `specs/004` RHI tasks | the shader side of the contract: program status, manifest, per-family CBs |
| S1 (Q0 baseline) | `scripts/test-render-baseline.ps1` | `shader_status` as an additional baseline signal, once T-021 exists |
| Q1a (clear/readback) | no dependency | nothing; do not couple the clear test to shaders |
| Q1b (fixtures) | `tests/renderer/**` | a minimal VS/PS fixture can use the ABI header once it exists |
| Q2/Q3 (classic path) | engine fork renderer | the resolver + programs |
| WebGPU profile | shader build graph | WGSL route and validation requirement |

File ownership: this spec documents `specs/008-classic-shaders/**`; the implementation lives under
`src/renderer/shader/**`, `shaders/**` and `tests/shader/**`. Engine call sites are modified engine
files delivered as hunks in the tracked `targets/win11-amd64/patches/all-changes.patch` (the target's
established delivery path for modified files), not as edits to the pristine submodule and not as
added/deleted engine files.

---

## 6. Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Authoring starts before the ABI is stable | medium | high (rewrite every program) | ABI header + reflection test gate |
| Manifest/table drift | medium | medium (silent gap) | FR-009 check in CI |
| "Owned" claimed without tests | medium | high | definition of owned; review contract |
| Strict mode blocks a mod/map unexpectedly | low | medium | tolerant fallback + counters; strict default only for evidence builds |
| Resolver scope creeps into a pipeline system | medium | medium | §12 non-goals; separate renderer work |
| Toolchain pin churn | medium | medium | pin once, verify goldens |
| Legal position changes after authoring | low | high | provenance records make the history auditable |
| The census's five-file set is incomplete | medium | high | runtime census; never claim completeness before it |

---

## 7. Verification plan

```powershell
# 1. census drift (source -> json -> markdown)
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/census.ps1 -Verify

# 2. resolver decision table
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/resolver-conformance.ps1

# 3. anchors and relative links
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/verify-docs.ps1

# 4. no retail/shader text in tracked files
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/provenance-check.ps1

# 5. cross-artifact coverage and resolver invariants
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/integration-check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/fuzz-resolver.ps1

# 6. review artifacts
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/census.ps1 -Html specs/008-classic-shaders/evidence/census.html
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/report.ps1
```

Run 1–5 before every merge to this directory. Run 6 when the census or the gate status changes, and
update the screenshots in [`evidence/`](evidence/).

---

## 8. Exit criteria

**For this specification**: all six checks pass; the self-review is complete; an independent
reviewer can answer the authoring-set question from `census.md`; the runtime census is recorded as
`SKIPPED` with a reason.

**For the shader work**: five programs `owned`; zero unresolved on the parity maps; error material
verified in tolerant mode; provenance records complete; both backends validated; parity captured per
pass.

---

## 9. Non-goals

- A pipeline-variant system, PSO cache or bindless design.
- A runtime ARB-to-HLSL translator.
- Acquiring, extracting or studying retail shader text.
- The modern renderer's shaders (that is a different library and a different spec).
- Scheduling agent sessions or worktrees; the S2 wave order belongs to the coordinator.
