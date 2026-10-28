# Traceability: Quality-first rendering

**Status**: Planning matrix maintained by T087. A row maps a requirement to its primary tasks and the artifact that will hold evidence. Draft contract/template artifacts are not stage execution or review evidence; coverage remains planned or partial until the named checks run.

Evidence files: `evidence-q0.md`, `evidence-q1a.md`, `evidence-q1.md`, `evidence-q2.md`, `evidence-q3.md`, `evidence-q4.md`, `evidence-q5.md`, `evidence-q6.md`, `evidence-q7.md`. Review records: `review-q1a.md`, `review-q1.md`, `review-q2.md`, `review-q3.md`, `review-q4.md`, `review-q6.md`, `review-q7.md`, `review-programme.md`.

## Functional requirements

| Requirement | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| FR-001 | T006, T009, T010, T011 | `baseline-procedure.md`, `evidence-q0.md` | Planned |
| FR-002 | T007, T010 | `reference-scenes.md`, `evidence-q0.md` | Planned; data-dependent cases may be SKIPPED |
| FR-003 | T004, T012, T014, T035 | `contracts/rhi-foundation.md`, `engine-integration.md` | Planned; T004's two-tier seam requires independent review before Q2; Q1a is not portability evidence |
| FR-004 | T004, T012, T022–T029 | `contracts/rhi-foundation.md`, `data-model.md` | Planned; Q1b consumes the portable contract; native extensions are optional |
| FR-005 | T013–T019, T024–T028, T030 | `evidence-q1a.md`, `evidence-q1.md` | Planned; **partial** — Q1a covers only the first slice; proposed T013 hash/tolerance policy requires independent review |
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
| FR-017 | T004, T071–T075 | `contracts/rhi-foundation.md`, `portable-profile-contract.md`, `evidence-q7.md` | Planned; two-tier contract is design only; per-target implementation and real-scene validation remain required |
| FR-018 | T005, T056, T060, T069, T074 | `evidence-template.md` and each evidence file | Template drafted; independent review and stage evidence remain open |
| FR-019 | T001–T003, T033, T040, T084 | `review-q2.md`, repository history | Planned; no exception to the constitution |

## Success criteria

| Criterion | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| SC-001 | T009–T011 | `evidence-q0.md` | Planned; two captures or explicit SKIPPED |
| SC-002 | T013–T031 | `evidence-q1a.md`, `evidence-q1.md` | Planned; includes pitch and outstanding-fence controls |
| SC-003 | T032, T037, T038, T039, T040 | `classic-parity.md`, `evidence-q2.md` | Planned; T080–T082 required for production behavior, T083 for integration |
| SC-004 | T041–T048, T085 | `motion-fixtures.md`, `evidence-q3.md` | Planned; T041–T047 and T085 precede T048 execution/review, including T043 isolation, T045 invalidation and T047 overlay ordering |
| SC-005 | T049–T057, T086 | `quality-rubric.md`, `evidence-q4.md` | Planned; budgets set after measurement; T086 independent ordering precedes Q4 review, optional provider branch does not block it |
| SC-006 | T055, T058–T062 | `provider-clearance.md`, `evidence-q5.md` | Planned; T059 observes clean default configure/build/run for attempted NVIDIA downloads and inspects dependencies/artifacts, separately from the off-path test; incomplete observation is NOT RUN; optional provider claim requires actual authorization and evidence |
| SC-007 | T063–T070 | `evidence-q6.md` | Planned |
| SC-008 | T071–T075 | `evidence-q7.md` | Planned; per claimed target only |

## Known coverage limits

- T055 (non-proprietary reconstruction/off path) is a Q4/US5 task, not a Q5 task. T086's independent ordering branch follows T047/T055 and is reviewed with Q4; its provider branch follows T062 only if authorized and executed. An unavailable provider is NOT RUN and does not gate Q4 or the independently testable off path. SC-006 additionally requires the observed clean default configure/build/run without attempted NVIDIA downloads; an off-path pass or artifact inspection alone cannot satisfy it.
- T048's Q3 execution and review follow T041–T047 and T085; fixture design T041 may be prepared earlier. T013 proposes canonical expected-row hash integrity plus independent ±1 byte acceptance and diagnostic actual hashes; the policy and any implementation still require independent review.
- T004 is not complete without independent technical review. T005 has a drafted template, not stage evidence; its required-field validator checks structure rather than the truth of results or permissions.
- Only T033 and T040 of T033–T040 concern FR-019 (delivery/ownership). The rest of that range concerns Q2 behavior.
- The "38 raw findings" mentioned in earlier planning is a process observation, not a repository fact; it is not traceable here.
- A task ID in these tables is a plan reference. It becomes evidence only when its own command output exists and is recorded.
