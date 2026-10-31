# Traceability: Quality-first rendering (literal matrix)

**Owner**: T087. This file maps each requirement to its tasks, artifact and evidence file, and states
one status per row. It is an index, not evidence: a row is only as good as the evidence file it
names, and a task id is a plan reference that becomes coverage only when its own command output
exists.

**Last reconciled**: 2026-10-31, against `origin/main` `f9e84c8`, by the documentation cluster for
issue #91.

## Status labels

| Label | Meaning |
|---|---|
| `PASS` | Every check the requirement names has executed on a recorded host, its output is retained in the named evidence file, and no part of the requirement is unexecuted. A PASS is per recorded host and data set, never universal. |
| `PARTIAL` | At least one named check executed and is recorded, and at least one named check has not executed or has executed only in a non-production form (CPU fixture, standalone harness, software adapter). The evidence file must say which half ran. |
| `NOT RUN` | No check that the requirement names has executed. Artifacts may exist (rubrics, contracts, tests, design notes); they are not coverage. |
| `UNPROVEN` | An artifact or claim exists whose truth no named check can settle, and no executed result is claimed. |

`UNPROVEN` currently holds **0 rows**: every row below either has an executed result or has nothing
executed. It is kept in the legend so that a future row cannot quietly become "assumed".

## Counts

| Group | PASS | PARTIAL | NOT RUN | UNPROVEN | Total |
|---|---|---|---|---|---|
| Functional requirements (FR-001…FR-019) | 2 | 15 | 2 | 0 | 19 |
| Success criteria (SC-001…SC-008) | 1 | 3 | 4 | 0 | 8 |

## Functional requirements

| ID | Task ids | Artifact path | Evidence file | Status |
|---|---|---|---|---|
| FR-001 | T006, T009, T010, T011 | `specs/004-quality-renderer/baseline-procedure.md`, `specs/004-quality-renderer/reference-scenes.md` | `specs/004-quality-renderer/evidence-q0.md` | `PASS` — fresh build identity, source/submodule revision, data identity, hardware, commands and outcomes; T011 review in §8. Limit: one host, one GL renderer, one demo data set |
| FR-002 | T007, T010 | `specs/004-quality-renderer/reference-scenes.md` | `specs/004-quality-renderer/evidence-q0.md` | `PASS` — four categories captured twice with scene ids and frozen ceilings; no case SKIPPED on that host. Limit: demo data, not retail scenes |
| FR-003 | T004, T012, T014, T035 | `specs/004-quality-renderer/contracts/rhi-foundation.md`, `specs/004-quality-renderer/engine-integration.md` | `specs/004-quality-renderer/evidence-rhi-contract.md`, `specs/004-quality-renderer/review-rhi-contract.md`, `specs/004-quality-renderer/evidence-q1a.md` | `PARTIAL` — contract signed off 2026-10-28 on `11ea64a`; Q1a/Q1b run through the portable seam; **the header gate is red on `main`** (26 violations, all `src/renderer/q4_harness/*.h`, see §Red gates); no engine scene adapter (T035) |
| FR-004 | T004, T012, T022–T029, T085 | `specs/004-quality-renderer/contracts/rhi-foundation.md`, `specs/004-quality-renderer/data-model.md` | `specs/004-quality-renderer/evidence-q1a.md`, `specs/004-quality-renderer/evidence-q1.md`, `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — draw, compute, MRT, readable depth, velocity/validity, history, present and separate overlay run as fixtures (32/32 CTest); T085's engine call sites absent |
| FR-005 | T013–T019, T024–T028, T030 | `src/renderer/rhi/`, `src/renderer/d3d12/`, `tests/renderer/` | `specs/004-quality-renderer/evidence-q1a.md`, `specs/004-quality-renderer/evidence-q1.md`, `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — clear/copy transitions, fence retirement, resize/shutdown and fault-injected removal pass; on 2026-10-31 the debug layer and GPU-based validation actually executed here (`errors=0`, 4 unwaived warnings per case) but T030/T031 remain open and that run has no independent review |
| FR-006 | T036, T037, T039, T080–T083 | `specs/004-quality-renderer/classic-parity.md` | `specs/004-quality-renderer/evidence-q2.md` | `NOT RUN` — no selector, no failed-initialization test, no engine path; `evidence-q2.md` records no classic frame |
| FR-007 | T034, T037, T038, T080–T082 | `specs/004-quality-renderer/classic-parity.md` | `specs/004-quality-renderer/evidence-q2.md` | `PARTIAL` — CPU fixtures for stage order, alpha coverage, lights, shadow exclusions, fog/blend, subviews and the GUI split pass (`renderer.q2.*`); the production slices T080–T082 and every engine-side case are absent |
| FR-008 | T041–T046 | `tests/renderer/motion-fixtures.md`, `src/renderer/rhi/motion_history.cpp`, `src/renderer/rhi/view_history.cpp` | `specs/004-quality-renderer/evidence-q3-tests.md`, `specs/004-quality-renderer/evidence-q3.md` | `PARTIAL` — reprojection, rendered-frame identity, generation and every invalidation trigger pass as fixtures; real-scene motion is T048 (`NOT RUN`) |
| FR-009 | T043–T045, T085 | `src/renderer/rhi/view_history.cpp`, `src/renderer/rhi/skinned_history.cpp`, `src/renderer/rhi/frame_integration.cpp` | `specs/004-quality-renderer/evidence-q3.md` | `PARTIAL` — view isolation and a 43-check private-pose retention test pass; the `engine-fork:neo/renderer/Model_md5.cpp` snapshot needs a fork PR and submodule bump, and T085's engine call sites are absent |
| FR-010 | T041, T046 | `src/renderer/rhi/motion_history.cpp`, `tests/renderer/motion_validity_test.cpp` | `specs/004-quality-renderer/evidence-q3-tests.md` | `PARTIAL` — explicit correspondence/rejection policies for viewmodels, particles, transparency, reflections, deformation and newly visible pixels pass (77 checks); no scene-level confirmation |
| FR-011 | T038, T047, T055, T086 | `src/renderer/rhi/overlay_composition.cpp`, `specs/004-quality-renderer/data-model.md` | `specs/004-quality-renderer/evidence-q3.md`, `specs/004-quality-renderer/evidence-q4.md` | `PARTIAL` — post-reconstruction overlay ordering passes as a fixture (33 checks); the Q4 harness captured a real 960×540 render composed to a 1280×720 display with a 2-px HUD border; the engine-side world-GUI path (T038) is absent |
| FR-012 | T050–T053 | `specs/004-quality-renderer/quality-rubric.md`, `src/renderer/rhi/materials.cpp`, `src/renderer/d3d12/` | `specs/004-quality-renderer/evidence-q4.md`, `specs/004-quality-renderer/evidence/t056/` | `PARTIAL` — material, direct-light/shadow and indirect/atmosphere slices pass as CPU fixtures (41 + 54 + 32 = 127 checks) and the standalone D3D12 harness renders all seven content cells; not a playable engine path |
| FR-013 | T049, T054, T056 | `specs/004-quality-renderer/quality-rubric.md`, `src/renderer/rhi/residency.cpp` | `specs/004-quality-renderer/evidence-q4.md`, `specs/004-quality-renderer/evidence/t056/measurements.json`, `specs/004-quality-renderer/evidence/t056/vram.json` | `PARTIAL` — provenance recorded, fence-safe eviction tested (37 checks), seven budget rows `recorded`, VRAM peak 107,110,400 B over 12/12 runs; no engine residency path and no quality-tier activation on a real frame |
| FR-014 | T055, T058–T061 | `specs/004-quality-renderer/provider-contract.md`, `src/renderer/rhi/reconstruction.cpp` | `specs/004-quality-renderer/evidence-q4.md` (T055/T086 rows), `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — the non-proprietary off path is byte-exact identity and passes 35 checks; T059's observed clean default configure/build/**run** for attempted NVIDIA downloads has not been performed, and no `specs/004-quality-renderer/evidence-q5.md` exists |
| FR-015 | T058, T060–T062 | `specs/004-quality-renderer/provider-contract.md` | none | `NOT RUN` — no authorization, no hardware claim, no create/evaluate. `specs/004-quality-renderer/provider-clearance.md` does not exist. No DLSS 5 support or legal compatibility is claimed |
| FR-016 | T063–T070 | `specs/004-quality-renderer/path-tracing-rubric.md`, `src/renderer/d3d12/pathtrace*.cpp` | `specs/004-quality-renderer/evidence-q6.md` | `PARTIAL` — CPU integrator, dynamic-update, material and denoiser cases pass (5/5 CTest, 233 checks) and the fallback is explicit; no DXR device, no converged capture, T069 PARTIAL, T070 NOT RUN |
| FR-017 | T004, T088, T089, T071–T075 | `specs/004-quality-renderer/portable-profile-contract.md`, `specs/004-quality-renderer/webgpu-mapping.md`, `web/profile/capability.mjs` | `specs/004-quality-renderer/evidence-q7a.md`, `specs/004-quality-renderer/evidence-t089.md`, `specs/004-quality-renderer/evidence-q7.md` | `PARTIAL` — three-tier contract, capability/fallback table and a procedural browser host slice exist; no WebGPU `doom3::rhi::Device` implementation, no real scene (T074 NOT RUN), no profile review (T075 NOT RUN); **the mapping gate is red on `main`** (62 problems, see §Red gates) |
| FR-018 | T005, T056, T060, T069, T074 | `specs/004-quality-renderer/evidence-template.md` | `specs/004-quality-renderer/evidence-q0.md` … `evidence-q4.md`, `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — template and validator signed off; Q0/Q1a/Q1b/Q3/Q4 records carry identity, commands, exits and thresholds. Q5/Q6/Q7 have no stage record with executed results, so the requirement is not uniformly met |
| FR-019 | T001–T003, T033, T040, T084 | `specs/004-quality-renderer/engine-integration.md`, `AGENTS.md`, `docs/GIT_WORKFLOW.md` | `git submodule status` / `git -C doom3_orig/doom3 status --porcelain` recorded in `specs/004-quality-renderer/evidence-q0.md` and re-run in `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — submodule clean at `a9c49da` and unchanged on 2026-10-31 (0 porcelain lines, no engine edit in this cluster); review coverage is partial (several review records named by earlier revisions of this file do not exist, §Missing records) and no human sign-off is recorded |

## Success criteria

| ID | Task ids | Artifact path | Evidence file | Status |
|---|---|---|---|---|
| SC-001 | T009–T011 | `specs/004-quality-renderer/reference-scenes.md` | `specs/004-quality-renderer/evidence-q0.md` | `PASS` — all four categories captured twice inside the pre-frozen ceilings (meanAbs 0.183/0.226/0.108/0.055; diff 0.208%/0.210%/0.098%/0.032%). Limit: that host and data set; a different host needs its own run |
| SC-002 | T013–T031 | `specs/004-quality-renderer/contracts/rhi-foundation.md`, `tests/renderer/` | `specs/004-quality-renderer/evidence-q1a.md`, `specs/004-quality-renderer/evidence-q1.md`, `specs/004-quality-renderer/evidence-doc-cluster.md` | `PARTIAL` — 32/32 CTest in Debug and Release (exit 0) covering draw, compute, MRT, depth, motion/validity, overlay separation and fence-safe retirement including an outstanding-release attempt and row-pitch correction; on 2026-10-31 validation executed with `errors=0` and 4 unwaived warnings per enabled case; T030/T031 stay open and that observation is unreviewed |
| SC-003 | T032, T037–T040, T080–T083 | `specs/004-quality-renderer/classic-parity.md` | `specs/004-quality-renderer/evidence-q2.md` | `NOT RUN` — no playable classic-data scene on the new path and no GL-vs-new comparison exists |
| SC-004 | T041–T048, T085 | `tests/renderer/motion-fixtures.md` | `specs/004-quality-renderer/evidence-q3.md`, `specs/004-quality-renderer/evidence-q3-tests.md` | `PARTIAL` — analytic camera/rigid/skinned/deformation vectors, invalidation, subview isolation and overlay ordering pass; no real-scene run (T048 NOT RUN), so the criterion's separate real-scene half is unexecuted |
| SC-005 | T049–T057, T086 | `specs/004-quality-renderer/quality-rubric.md` | `specs/004-quality-renderer/evidence-q4.md`, `specs/004-quality-renderer/review-q4.md`, `specs/004-quality-renderer/review-q4-r2.md`, `specs/004-quality-renderer/review-q4-r3.md` | `PARTIAL` — T057 r3 accepts with limits (4.1–4.7 `accept`, 4.3/4.5/4.6 limited) on Intel Graphics 32.0.101.8331 plus WARP, with B4 set at 107,110,400 B; the review says plainly it is "not an unrestricted Q4-green claim", Q4 is not green, and no engine frame exists |
| SC-006 | T055, T058–T062 | `specs/004-quality-renderer/provider-contract.md` | `specs/004-quality-renderer/evidence-q4.md` (off-path rows only) | `NOT RUN` — the off path works, but the required clean default configure/build/**run** observation for attempted NVIDIA downloads has not been made, and no authorized provider exists. The off-path pass does not satisfy this criterion |
| SC-007 | T063–T070 | `specs/004-quality-renderer/path-tracing-rubric.md` | `specs/004-quality-renderer/evidence-q6.md` | `NOT RUN` — no converged static record and no moving/skinned capture; every visual row is NOT RUN with a reason |
| SC-008 | T071–T075 | `specs/004-quality-renderer/portable-profile-contract.md`, `web/profile/capability.mjs` | `specs/004-quality-renderer/evidence-q7.md` | `NOT RUN` — no claimed target has a target-specific real-scene test; T074/T075 are NOT RUN. The Q7a spike is a conformance gate, not target support |

## Red gates on `main` at `f9e84c8`

Two documented gate commands fail today. Both are recorded with command and exit code in
[`evidence-doc-cluster.md`](./evidence-doc-cluster.md); neither is a documentation defect, so neither
was fixed in a docs-only change.

1. `python -I scripts/validate-rhi-contract.py --self-test` → **exit 1**, 26 failures, every one of
   the form `prohibited D3D12 vocabulary in engine-facing header src\renderer\q4_harness\*.h`
   (`harness.h`, `harness_impl.h`, `vram_probe.h`, `dds.h`, `content.h`). The
   [contract](./contracts/rhi-foundation.md) excludes only `src/renderer/d3d12/` and
   `src/renderer/ext/` from the scan, so Q4's harness headers sit outside the allowed tier. The
   header gate — and therefore FR-003's "no Windows-native handle in scene/material interfaces" and
   FR-017's lexical half — is failing on the current tree. Resolving it means either moving those
   headers under `src/renderer/d3d12/`, placing them under `src/renderer/ext/`, or amending the scan
   roots by a reviewed contract change. That is a `src/renderer/**` owner's decision, not a docs one.
2. `python -I scripts/validate-webgpu-mapping.py` → **exit 1**,
   `INVALID: 62 problem(s); 57/119 declarations mapped` (62 `declaration not mapped` lines, e.g.
   `DrawFrameDesc.passes`, `DrawPassDesc.colorClears`, `SurfaceDesc.width`). T089 reconciled five
   declarations; later Q1b/Q2/Q3 declarations were added without mappings. FR-017's mapping half is
   not currently satisfied on `main`.

## Missing records named elsewhere

Earlier revisions of this file and of `traceability.md`'s evidence/review lists named paths that do
not exist in the tree at `f9e84c8`. A missing record is not coverage:

| Named path | State |
|---|---|
| `specs/004-quality-renderer/evidence-q5.md` | does not exist (Q5 has no stage evidence) |
| `specs/004-quality-renderer/provider-clearance.md` | does not exist (T060 NOT RUN) |
| `specs/004-quality-renderer/review-q2.md` | does not exist (Q2 has no independent review) |
| `specs/004-quality-renderer/review-q3.md` | does not exist (Q3 has no independent review) |
| `specs/004-quality-renderer/review-q6.md` | does not exist (T070 NOT RUN) |
| `specs/004-quality-renderer/review-q7.md` | does not exist (T075 NOT RUN) |
| `specs/004-quality-renderer/review-programme.md` | does not exist (T078 NOT RUN) |

## Known coverage limits

- **Q1a is not Q1.** Q1a proves offscreen clear/readback through the portable seam on one hardware and
  one software adapter. It is not portability evidence, not a renderer and not Q1. Q1b adds
  programmable draw/compute/MRT/depth/temporal/surface fixtures. Full Q1 still requires T030's gate
  and T031's independent review.
- **The recorded `0x887A002D` skip is host-specific.** Every earlier record says
  `D3D12GetDebugInterface` returned `0x887A002D` and validation was SKIPPED. On the 2026-10-31 host
  the diagnostics fixture reports `debug=RUN`, `gpu_validation=RUN` and `validation_executed=true`
  for 6 of 8 cases with `errors=0` and 4 unwaived `ClearRenderTargetView` warnings per case. This
  contradicts the recorded reason for T030 being open; it does not close T030, because the gate must
  be re-run in its own stage worktree and independently reviewed, and the 4 warnings are unwaived.
- **Fixtures are not scenes.** A green CPU or standalone-harness suite demonstrates arithmetic,
  ordering or policy. It does not demonstrate an engine frame, parity, art acceptance or performance.
- **Q4 is accepted with limits, not green.** `review-q4-r3.md` states "Q4: ACCEPTED WITH LIMITS" and
  "not an unrestricted Q4-green claim"; its §6 limits are not upgraded to passes. No engine frame
  exists.
- **Q6 and Q7 are partial.** Q6 has CPU reference slices only, with T069 PARTIAL and T070 NOT RUN.
  Q7 has a contract, a capability table and a procedural browser host slice, with T072/T073 partly
  open, T074/T075 NOT RUN. No DXR, DLSS or web-support claim follows from either.
- **T055 is a Q4/US5 task, not Q5.** T086's provider branch follows T062 only if authorized and
  executed; it is NOT RUN and does not gate Q4 or the off path. SC-006 additionally requires the
  observed clean default configure/build/run, which a passing off-path test cannot substitute for.
- **T048's review follows T041–T047 and T085.** T043/T045/T047 now pass as portable slices, but
  T044/T085's engine-fork halves are missing, so T048 is NOT RUN on that and on the absence of legal
  data. Synthetic vectors alone cannot satisfy it.
- **T004/T005 sign-off is agent-authored** on `11ea64a` with a human sign-off caveat; the validator
  checks structure, not the truth of results or permissions.
- **Only T033 and T040 of T033–T040 concern FR-019**; the rest of that range concerns Q2 behaviour.
- **A task id in these tables is a plan reference**, not coverage. The "38 raw findings" figure from
  earlier planning is a process observation, not a repository fact, and is not traceable here.
