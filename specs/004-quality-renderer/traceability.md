# Traceability: Quality-first rendering

**Status**: Planning matrix maintained by T087. A row maps a requirement to its primary tasks and the artifact that will hold evidence. **No row is a claim that the work or the evidence exists.** Coverage is inferred from task text and marked `Planned` or `partial`.

Evidence files: `evidence-q0.md`, `evidence-q1a.md`, `evidence-q1.md`, `evidence-q2.md`, `evidence-q3.md`, `evidence-q4.md`, `evidence-q5.md`, `evidence-q6.md`, `evidence-q7.md`. Review records: `review-q1a.md`, `review-q1.md`, `review-q2.md`, `review-q3.md`, `review-q4.md`, `review-q6.md`, `review-q7.md`, `review-programme.md`.

## Functional requirements

| Requirement | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| FR-001 | T006, T009, T010, T011 | `baseline-procedure.md`, `evidence-q0.md` | Planned |
| FR-002 | T007, T010 | `reference-scenes.md`, `evidence-q0.md` | Planned; data-dependent cases may be SKIPPED |
| FR-003 | T004, T012, T014, T035 | `contracts/rhi-foundation.md`, `engine-integration.md` | Planned |
| FR-004 | T004, T012, T022–T029 | `contracts/rhi-foundation.md`, `data-model.md` | Planned; Q1b consumes the contract |
| FR-005 | T013–T019, T024–T028, T030 | `evidence-q1a.md`, `evidence-q1.md` | Planned; **partial** — Q1a covers only the first slice |
| FR-006 | T036, T037, T039, T080–T083 | `classic-parity.md`, `evidence-q2.md` | Planned; T080–T082 production, T083 integration check |
| FR-007 | T034, T037, T038, T080–T082 | `classic-parity.md`, `evidence-q2.md` | Planned; T037 tests, T080–T082 production |
| FR-008 | T041–T046 | `motion-fixtures.md`, `evidence-q3.md` | Planned |
| FR-009 | T043–T045, T085 | `evidence-q3.md` | Planned; **partial** — T085 adds engine-side wiring |
| FR-010 | T041, T046 | `motion-fixtures.md`, `evidence-q3.md` | Planned |
| FR-011 | T038, T047, T086 | `evidence-q3.md`, `evidence-q4.md` | Planned; provider ordering in T086 |
| FR-012 | T050–T053 | `evidence-q4.md` | Planned |
| FR-013 | T049, T054, T056 | `quality-rubric.md`, `evidence-q4.md` | Planned; budgets follow T056 measurements |
| FR-014 | T058–T061 | `provider-clearance.md`, `evidence-q5.md` | Planned; T059 includes the observed default build |
| FR-015 | T060–T062 | `provider-clearance.md`, `evidence-q5.md` | Planned; NOT RUN until access and clearance exist |
| FR-016 | T063–T070 | `path-tracing-rubric.md`, `evidence-q6.md` | Planned |
| FR-017 | T071–T075 | `portable-profile-contract.md`, `evidence-q7.md` | Planned; per claimed target only |
| FR-018 | T005, T056, T060, T069, T074 | `evidence-template.md` and each evidence file | Planned |
| FR-019 | T001–T003, T033, T040, T084 | `review-q2.md`, repository history | Planned; no exception to the constitution |

## Success criteria

| Criterion | Primary tasks | Evidence / artifact | Coverage note |
|---|---|---|---|
| SC-001 | T009–T011 | `evidence-q0.md` | Planned; two captures or explicit SKIPPED |
| SC-002 | T013–T031 | `evidence-q1a.md`, `evidence-q1.md` | Planned; includes pitch and outstanding-fence controls |
| SC-003 | T032, T037, T038, T039, T040 | `classic-parity.md`, `evidence-q2.md` | Planned; T080–T082 required for production behavior, T083 for integration |
| SC-004 | T041–T048, T085 | `motion-fixtures.md`, `evidence-q3.md` | Planned |
| SC-005 | T049–T057, T086 | `quality-rubric.md`, `evidence-q4.md` | Planned; budgets set after measurement |
| SC-006 | T055, T058–T062 | `provider-clearance.md`, `evidence-q5.md` | Planned; provider claims may remain NOT RUN |
| SC-007 | T063–T070 | `evidence-q6.md` | Planned |
| SC-008 | T071–T075 | `evidence-q7.md` | Planned; per claimed target only |

## Known coverage limits

- T055 (non-proprietary reconstruction/off path) is a Q4/US5 task, not a Q5 task. It is a prerequisite for provider-side ordering in T086 and for SC-006's off-path claim.
- Only T033 and T040 of T033–T040 concern FR-019 (delivery/ownership). The rest of that range concerns Q2 behavior.
- The "38 raw findings" mentioned in earlier planning is a process observation, not a repository fact; it is not traceable here.
- A task ID in these tables is a plan reference. It becomes evidence only when its own command output exists and is recorded.
