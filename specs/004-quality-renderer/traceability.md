# Traceability: Quality-first rendering

**Status**: Planning matrix maintained by T087. A row maps a requirement to its primary tasks and the artifact that will hold evidence. Draft contract/template artifacts are not stage execution or review evidence; coverage remains planned or partial until the named checks run. Q1a T012–T021 are complete and merged in `8638db8` (PR #18); `evidence-q1a.md` and `review-q1a.md` record the execution and independent agent review. Q1b T022–T031 and all later stages remain planned or partial. Q7a T088 is complete (issue #35): the lexical and structural mapping gates, the browser spike and the Emscripten encoder leg all executed, and T089 completed the header reconciliation.

Evidence files: `evidence-rhi-contract.md`, `evidence-q0.md`, `evidence-q1a.md`, `evidence-q1.md`, `evidence-q2.md`, `evidence-q3.md`, `evidence-q4.md`, `evidence-q5.md`, `evidence-q6.md`, `evidence-q7.md`, `evidence-q7a.md`, `evidence/q7a-emscripten.json`, `evidence-t089.md`. Review records: `review-rhi-contract.md`, `review-q1a.md`, `review-q1.md`, `review-q2.md`, `review-q3.md`, `review-q4.md`, `review-q6.md`, `review-q7.md`, `review-q7a.md`, `review-spec-consistency.md`, `review-t089.md`, `review-t088-encoder.md`, `review-programme.md`.

## Functional requirements

| Requirement | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| FR-001 | T006, T009, T010, T011 | `baseline-procedure.md`, `evidence-q0.md` | **Established (2026-10-29)** — all four categories captured twice; see `evidence-q0.md` §8 for the T011 review |
| FR-002 | T007, T010 | `reference-scenes.md`, `evidence-q0.md` | **Established (2026-10-29)** — scene identities and tolerances recorded; no case SKIPPED on this host |
| FR-003 | T004, T012, T014, T035 | `contracts/rhi-foundation.md`, `engine-integration.md` | T004 signed off 2026-10-28 on 11ea64a (agent reviewer; human caveat); Q1a T012–T021 complete and merged in `8638db8`; Q2's Q7a/T088 gate is complete (both legs, T089 reconciled the header); Q1a is not portability evidence |
| FR-004 | T004, T012, T022–T029 | `contracts/rhi-foundation.md`, `data-model.md`, `evidence-q1.md` | **Partial** — Q1a complete and merged; Q1b T022–T029 implemented and tested on Intel and WARP; T030/T031 open; native extensions are optional |
| FR-005 | T013–T019, T024–T028, T030 | `evidence-q1a.md`, `evidence-q1.md` | **Partial** — Q1a complete and merged (`evidence-q1a.md`, `review-q1a.md`); Q1b fixtures and fault-injected removal pass (`evidence-q1.md`); debug layer and GPU-based validation SKIPPED (`0x887A002D`), so T030/T031 and full Q1 remain blocked |
| FR-006 | T036, T037, T039, T080–T083 | `classic-parity.md`, `evidence-q2.md` | Planned; T080–T082 production, T083 integration check |
| FR-007 | T034, T037, T038, T080–T082 | `classic-parity.md`, `evidence-q2.md` | Planned; T037 tests, T080–T082 production |
| FR-008 | T041–T046 | `motion-fixtures.md`, `evidence-q3.md` | Planned |
| FR-009 | T043–T045, T085 | `evidence-q3.md` | Planned; **partial** — T085 adds engine-side wiring |
| FR-010 | T041, T046 | `motion-fixtures.md`, `evidence-q3.md` | Planned |
| FR-011 | T038, T047, T055, T086 | `evidence-q3.md`, `evidence-q4.md`, `evidence-q5.md` | Planned; T086 off-path ordering follows T047/T055; its provider branch is conditional and may be NOT RUN |
| FR-012 | T050–T053 | `evidence-q4.md` | Planned |
| FR-013 | T049, T054, T056 | `quality-rubric.md`, `evidence-q4.md` | Planned; budgets follow T056 measurements |
| FR-014 | T055, T058–T061 | `provider-clearance.md`, `evidence-q5.md` | Planned; **partial** — T055 supplies the independent off path; T059 separately observes clean configure/build and run, and inspects dependencies/artifacts |
| FR-015 | T060–T062 | `provider-clearance.md`, `evidence-q5.md` | Planned; NOT RUN until access and clearance exist |
| FR-016 | T063–T070 | `path-tracing-rubric.md`, `evidence-q6.md` | Planned |
| FR-017 | T004, T088, T089, T071–T075 | `contracts/rhi-foundation.md`, `portable-profile-contract.md`, `evidence-q7a.md`, `evidence-q7.md` | Planned; T004 signed off 2026-10-28 on 11ea64a (agent reviewer; human caveat); three-tier contract is design only; T088 is a pre-Q2 header/mapping and WebGPU conformance spike, not target support — **complete (2026-10-29)**: lexical and structural gates, a pinned-browser spike and the Emscripten encoder leg all executed; no WebGPU `doom3::rhi::Device` implementation and no software adapter exist (`evidence-q7a.md`); T089 completed the header reconciliation (`evidence-t089.md`); per-target implementation and real-scene validation remain required |
| FR-018 | T005, T056, T060, T069, T074 | `evidence-template.md` and each evidence file | Template and validator signed off 2026-10-28 on 11ea64a (agent reviewer; human caveat); stage evidence remains open |
| FR-019 | T001–T003, T033, T040, T084 | `review-q2.md`, repository history | Planned; no exception to the constitution |

## Success criteria

| Criterion | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| SC-001 | T009–T011 | `evidence-q0.md` | **Met on this host (2026-10-29)** — two captures per category inside the frozen ceilings; a different host or data set still needs its own run |
| SC-002 | T013–T031 | `evidence-q1a.md`, `evidence-q1.md` | **Partial** — Q1a complete and merged; Q1b draw/compute/MRT/depth/frame-slot/motion/surface fixtures pass 17/17 in Debug and Release on Intel and WARP; full SC-002 still needs executed debug layer plus GPU-based validation and T031 |
| SC-003 | T032, T037, T038, T039, T040 | `classic-parity.md`, `evidence-q2.md` | Planned; T080–T082 required for production behavior, T083 for integration |
| SC-004 | T041–T048, T085 | `motion-fixtures.md`, `evidence-q3.md` | Planned; T041–T047 and T085 precede T048 execution/review, including T043 isolation, T045 invalidation and T047 overlay ordering |
| SC-005 | T049–T057, T086 | `quality-rubric.md`, `evidence-q4.md` | Planned; budgets set after measurement; T086 independent ordering precedes Q4 review, optional provider branch does not block it |
| SC-006 | T055, T058–T062 | `provider-clearance.md`, `evidence-q5.md` | Planned; T059 observes clean default configure/build/run for attempted NVIDIA downloads and inspects dependencies/artifacts, separately from the off-path test; incomplete observation is NOT RUN; optional provider claim requires actual authorization and evidence |
| SC-007 | T063–T070 | `evidence-q6.md` | Planned |
| SC-008 | T071–T075 | `evidence-q7.md` | Planned; per claimed target only. T088 is a pre-Q2 conformance spike, not target-support evidence, so it is not an SC-008 task. |

## Known coverage limits

- T055 (non-proprietary reconstruction/off path) is a Q4/US5 task, not a Q5 task. T086's independent ordering branch follows T047/T055 and is reviewed with Q4; its provider branch follows T062 only if authorized and executed. An unavailable provider is NOT RUN and does not gate Q4 or the independently testable off path. SC-006 additionally requires the observed clean default configure/build/run without attempted NVIDIA downloads; an off-path pass or artifact inspection alone cannot satisfy it.
- T048's Q3 execution and review follow T041–T047 and T085; fixture design T041 may be prepared earlier. T013's canonical expected-row hash integrity, independent ±1 byte acceptance and diagnostic actual hashes are implemented and recorded in `evidence-q1a.md`; an independent agent review accepted the policy and implementation at `369c47b` (human sign-off remains a caveat).
- Q1a T012–T021 are recorded as complete in `tasks.md` and `evidence-q1a.md`. Debug layer and GPU-based validation were attempted and SKIPPED with `hr=0x887A002D`; this blocks full Q1 (T030/T031), not the Q1a slice. The Q1a review is agent-authored; no human sign-off is recorded.
- T004 and T005 were signed off on 2026-10-28 on `11ea64a` by a non-author agent reviewer, after an earlier attempt was withdrawn and the gate bypasses were fixed. A human/owner sign-off is not recorded and remains a caveat. T005's validator checks structure rather than the truth of results or permissions; stage evidence remains open.
- Q1b T022–T029 ran 17 CTest cases in Debug and Release on both adapters; `evidence-q1.md` records the red/mutation controls, the fixture validator and the surface captures. T030 stays open because the debug layer and GPU-based validation are SKIPPED (`0x887A002D`), and T031 is a self-review only. The four-slot bound is asserted by the private gated test; the public test no longer uses a timing-sensitive fifth-frame probe. Q1b shader modules cross the seam as opaque bytes, but the C++ fixtures embed a native program language; the Q7a gate still owns the portable format.
- Only T033 and T040 of T033–T040 concern FR-019 (delivery/ownership). The rest of that range concerns Q2 behavior.
- The "38 raw findings" mentioned in earlier planning is a process observation, not a repository fact; it is not traceable here.
- A task ID in these tables is a plan reference. It becomes evidence only when its own command output exists and is recorded.
