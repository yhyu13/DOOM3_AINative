# Spec-set consistency review: 004 quality renderer (Q0, Q1a, Q2 design, Q7a)

**Reviewer**: independent spec-consistency reviewer (agent), fresh context, no authorship of
`specs/004-quality-renderer/` or the Q7a work. This is not a human sign-off.

**Date**: 2026-10-29 (re-checked after the author's dispositions on the same day). **Branch**:
`docs/35-webgpu-conformance`. **Repository state reviewed**: `HEAD` `d9bf1ca`
(`docs/35-webgpu-conformance`), with the Q7a change set **staged in the index** (`git status`
reports `A`/`M` entries) and not committed.

**Scope**: `spec.md`, `plan.md`, `tasks.md`, `traceability.md`, `quickstart.md`,
`contracts/rhi-foundation.md`, `evidence-q0.md`, `evidence-q1a.md`, `evidence-q2-design.md`,
`review-q1a.md`, `review-q2-design.md`, `review-rhi-contract.md`, `webgpu-mapping.md`,
`evidence-q7a.md`; `src/renderer/rhi/rhi.h`; the names/targets in `tests/renderer/CMakeLists.txt`;
the scan scope in `scripts/validate-rhi-contract.py`; `docs/ROADMAP.md`; `docs/SKILLS.md`;
`git log --oneline -15`; `git status --porcelain`.

**Commands run**: `git status --porcelain` (Q7a change set staged), `git diff --cached --check`
(exit 0), `py -3 -I scripts/validate-rhi-contract.py` (exit 0, header scan RUN over 1 header and 3
public test files), `py -3 -I scripts/validate-webgpu-mapping.py` (exit 0, 52/52 declarations). No
build, GPU run, browser run, download, commit, or submodule access was performed in either pass.

## Findings

| ID | Severity | file:line | What is inconsistent | Recommended concrete edit | Owner |
|---|---|---|---|---|---|
| B1 | BLOCKING | `contracts/rhi-foundation.md:121`; `webgpu-mapping.md:35,36,55,59,66`; `src/renderer/rhi/rhi.h:47-48,55,134` | The contract says a field with no WebGPU-shaped operation "belongs to the D3D12-private tier or a native extension, not to the core", but the Q7a gate's own output keeps `AdapterInfo.vendorId`/`deviceId`/`dedicatedVideoMemory`, `DeviceOptions.adapterIndex`, `Device.TryRelease` and `ReleaseStatus` in the core as `unsupported`/`mapped-with-gap`. `rhi.h` also has no capability/unsupported signal, so the mapping's required "mark the capability unsupported" (`webgpu-mapping.md:35`) is not expressible. | Amend the header-gate rule to separate "no WebGPU operation but implementable with mandated backend bookkeeping and `Unsupported`/zero semantics" from "must move tier"; list those five fields explicitly; either add a capability signal to `rhi.h` or make `vendorId=0`, `deviceId=0`, `dedicatedVideoMemory=0`, `adapterIndex>0 -> Unsupported` the only legal values. | T004 / T088 |
| B2 | BLOCKING | `tasks.md:176-181`; `evidence-q7a.md:109-115`; `contracts/rhi-foundation.md:123` | `tasks.md` says the semantic field-to-WebGPU mapping gate is "executed and pass" and Q2 is blocked only on the encoder leg, but `evidence-q7a.md` records the independent reviewer as NOT RUN with the decision pending and points at `review-q7a.md`, which does not exist. The contract names the semantic mapping review as the control, and `review-rhi-contract.md:137` still lists Q7a as NOT RUN. | Reword the Q7a note to "lexical + mapping validators RUN; mapping has no independent review (NOT RUN); Q2 blocked on review and encoder", add a review task, and create `review-q7a.md` or remove the reference. | T088 / T004 |
| B3 | BLOCKING | `evidence-q7a.md:11-13,70-73` | "This record ships in the same commit as the files it lists, so `git log -1 ...` names the exact tree" and "Retained logs and artifacts: tracked — `specs/.../evidence/q7a-conformance.json`" are false at this commit. `git status` shows `evidence-q7a.md`, `webgpu-mapping.md`, `evidence/`, `tests/webgpu/`, `scripts/validate-webgpu-mapping.py` and `scripts/test-validate-webgpu-mapping.py` untracked; `git ls-files specs/004-quality-renderer/evidence` is empty. | State "base `d9bf1ca`; Q7a artifacts pending commit; commit SHA to be filled after commit", list every file the record covers, and mark the JSON/PNG as "to be tracked" until they are committed. | T088 |
| M1 | MAJOR | `evidence-q7a.md:3-5`; `webgpu-mapping.md:14`; `contracts/rhi-foundation.md:17,111-117` | Evidence claims "the portable RHI surface is fully mapped to WebGPU-shaped operations", but the mapping covers only the 52 declarations of the Q1a `rhi.h`. The contract's core surface also names `surface`, `queue`, `render-pass`, `compute-pass`, `bind-group`, `pipeline-layout`, `shader-module`, `copy`, `readback`, `completion-token` and the capability profile, none of which appear in the header or the mapping table. | Scope the claim to "the Q1a subset declared in `src/renderer/rhi/rhi.h`"; add a "NOT CHECKED" table/paragraph for the remainder of the portable core surface. | T088 / T071 |
| M2 | MAJOR | `plan.md:94`; `tasks.md:17-18`; `traceability.md:49`; `review-rhi-contract.md:128-138` | Plan still says "T004 remains unchecked until..." and "T005 remains unchecked until...", while both are checked and signed off (agent reviewer, human caveat) at `11ea64a`. | Rewrite in the past tense and link the final sign-off block; keep the human-sign-off caveat. | T004 / T005 |
| M3 | MAJOR | `plan.md:84,47-72` | Plan's Q7a row says "Design only; no browser implementation, compiler or fixture exists", but `tests/webgpu/conformance.html` and `tests/webgpu/run-conformance.mjs` exist and executed 12 checks twice. The project-structure block omits `tests/webgpu/` and the two Q7a scripts. | Change Q7a to "Partial (2026-10-29): 12 browser checks + mapping/lexical gates RUN; encoder leg NOT RUN; Q2 blocked" and add the new paths. | T088 |
| M4 | MAJOR | `quickstart.md:3,42,100` | Quickstart says the standalone RHI source/test project "do not exist until their implementation tasks are completed" and heads the Q1a section "(planned)", although `src/renderer/rhi/rhi.h`, `tests/renderer/CMakeLists.txt` and seven CTest names are merged. It also omits the Q7a commands that T088 requires and that `evidence-q7a.md:60-64` ran. | Split status per stage (Q1a merged; Q1b+ planned), add a Q7a command block (`validate-webgpu-mapping.py`, its `--self-test`, `test-validate-webgpu-mapping.py`, `node tests/webgpu/run-conformance.mjs`), and update the "each stage owns its commands" statement. | T079 / T088 |
| M5 | MAJOR | `traceability.md:3,11,12,35` | Status says only Q1a is complete and "Q1b T022–T031 and all later stages remain planned or partial"; FR-001, FR-002 and SC-001 remain "Planned" while Q0 is established, T006–T011 are checked, all four categories were captured twice and SC-001 is recorded as met (`tasks.md:35-38`, `evidence-q0.md:3-5,154`). | Mark FR-001/FR-002 "Met for host/demo data (Q0, PR #31)" and SC-001 "Met on host; ceilings recorded"; update the status line to include Q0 and the T032/T033/T041 design deliveries. | T087 |
| M6 | MAJOR | `traceability.md:27,42,5` | FR-017/Q7a has no partial label despite the executed lexical/mapping/12-check run; `webgpu-mapping.md`, `review-q7a.md`, `evidence-q2-design.md` and `review-q2-design.md` are missing from the artifact index; no row owns the Q7a gate itself. | FR-017 -> Partial with RUN/NOT RUN legs; add the missing artifacts; add a "Gates" subsection mapping Q7a -> T088 -> FR-017/SC-002, or state explicitly why the gate is outside every SC. | T087 / T088 |
| M7 | MAJOR | `tasks.md:84,90,189,199` | T035 and T080 both claim `src/renderer/rhi/engine_scene_adapter.cpp`. The dependency section never declares T080 after T035, and line 199 permits parallel preparation in separate issue/worktrees, so two writers could target the same file. | Add an explicit T035 -> T080 blocking edge and a single-owner statement, or split the file into adapter shell and extraction. | T035 / T080 |
| M8 | MAJOR | `spec.md:158,171-184` | The spec has no requirement for the Q7a gate. FR-017 only requires per-target capability/fallback/validation; no FR/SC says the core surface must be field-mapped to a WebGPU-shaped operation before Q2, nor records encoder identity/version/licence/acquisition and the NOT RUN/blocked semantics. The gate exists only in plan/tasks. | Extend FR-017 (or add FR-020) and a matching SC/gate clause stating Q7a, its field-mapping requirement, its NOT RUN rule and the Q2 block; add it to `traceability.md`. | spec / T088 |
| M9 | MAJOR | `tasks.md:183`; `evidence-q7a.md:109-115` | T088 names no issue/worktree (issue #35 appears only in the tasks note and the evidence), no validator/harness paths, no evidence subdirectory, and no independent review; `review-q7a.md` is referenced but no task creates it. | Add issue #35 and the exact paths to T088, add a review task (for example T089), and list the required artifacts. | T088 |
| M10 | MAJOR | `evidence-q2-design.md:242`; `review-q2-design.md:149` | Both still say T088/Q7a is "not run"; the lexical, mapping and 12 browser checks ran on 2026-10-29 and only the encoder leg is NOT RUN. | Update to "Q7a partial: mapping + lexical + 12 browser checks RUN; encoder leg NOT RUN; Q2 blocked; independent review not run". | T078 / T088 |
| m1 | MINOR | `quickstart.md:38`; `scripts/validate-rhi-contract.py:449-459` | Quickstart says the scan covers "core headers under `src/renderer/rhi/`"; the validator recursively scans all of `src/renderer` except the four private prefixes. | Say "all engine-facing headers under `src/renderer/` except `src/renderer/d3d12/` and `src/renderer/ext/`". | T005 |
| m2 | MINOR | `tasks.md:183` | T088 says to scan "core headers under `src/renderer/rhi/`" and public tests "outside `tests/renderer/d3d12/`", while the contract and validator also exclude `src/renderer/ext/` and `tests/renderer/ext/`. | Align T088 with `contracts/rhi-foundation.md:109,121`. | T088 |
| m3 | MINOR | `traceability.md:19,24,29` | FR-009 and FR-014 are labelled "**partial**" although T085 and T055 are open and no artifact exists for either; FR-019 remains "Planned" although T001/T033 are checked and T084 is partial. | Use "partial" only when an executed artifact exists; move FR-019 to Partial. | T087 |
| m4 | MINOR | `tasks.md:3` | The blanket status "not a claim that any task or gate has passed" contradicts the 22 checked tasks and the recorded Q0/Q1a results at lines 35-41 and 176-181. | Replace with "live backlog: Q0/Q1a complete with evidence; Q1b+ open". | T087 / T076 |
| m5 | MINOR | `tasks.md:174` | T087 is unchecked but `traceability.md` exists and is actively maintained. | Either define T087 as a recurring review that is never "done", or check it against the reviewed matrix and add a successor. | T087 |
| m6 | MINOR | `contracts/rhi-foundation.md:128`; `src/renderer/rhi/rhi.h:68-70`; `webgpu-mapping.md:109-112` | The contract and header say "clear" a per-row pattern; mapping G1 says WebGPU has no per-row clear and the contract "must keep saying 'produce these row bytes', never 'call clear'". | Reword both to "produce/write the described row bytes" and name `writeTexture`/a position-reading draw as implementation choices. | T004 / T012 |
| m7 | MINOR | `contracts/rhi-foundation.md:99`; `webgpu-mapping.md:50,78` | The public boundary offers "poll, callback or future" completion; mapping shows WebGPU has no pollable fence and a per-submission token is only backend bookkeeping over a queue-wide promise. | State that "poll" is a client-side cached state over an async completion, never a blocking fence wait; reference mapping G3. | T004 |
| m8 | MINOR | `review-q2-design.md:5,140`; `evidence-q2-design.md:258-260` | The review says "No reviewer approved T032, T033 or T041" while also saying they were ticked "after the PASS", and the evidence calls it an "independent PASS". | Say "no human/independent-team approval; the Round 8 agent PASS supports the design tick only". | T032 / T033 / T041 |
| m9 | MINOR | `tasks.md:69,91-92,103-107,110,120-121` | Same-file pairs have no declared blocking edge: T042/T046 (`motion_history.cpp`), T043/T045 (`view_history.cpp`), T027/T085 (`frame_resources.h`), T051/T081 (`materials.cpp`), T052/T082 (`lighting.cpp`); T080/T081/T082/T038/T044/T085 all claim the broad `engine-fork:neo/renderer/` tree. | Add a file-ownership table and explicit edges, or split the broad fork ownership per file. | plan / T078 |
| m10 | MINOR | `docs/ROADMAP.md:3,77`; `evidence-q0.md:93` | The M1 exit criterion still says "the build is 32-bit", while the current verified build is x64 (`0x8664`); the roadmap's evidence date (2026-10-26) predates Q0/Q1a. T076 exists to reconcile this. | Scope M1 to the legacy `doom.sln` and add the current x64 CMake build as a separate, dated fact. | T076 |
| m11 | MINOR | `tasks.md:30`; `tests/renderer/test-render-baseline.ps1:1-7`; `evidence-q0.md:161` | T008 names `tests/renderer/test-render-baseline.ps1` for the synthetic side-effect tests, but the suite lives in `tests/baseline/test-render-baseline.ps1`; the renderer path is a five-line compatibility shim and the evidence runs the baseline path. | Name both paths and the delegation explicitly in T008. | T008 |
| m12 | NIT | `review-rhi-contract.md:137` | The dated sign-off block lists Q7a/T088 as NOT RUN; a reader can take it as current. | Append "superseded 2026-10-29: see `evidence-q7a.md` (partial)". | T004 |
| m13 | NIT | `tasks.md:1` | The working-tree diff adds a UTF-8 BOM (`+\ufeff# Tasks`) and two trailing blank lines, unlike the other spec files. | Save without BOM and strip the trailing blank lines. | T088 |

## What is actually consistent

- Q0: `tasks.md:28-41` marks T006–T011 checked, `evidence-q0.md` records `baseline-final13` with all
  four categories captured twice, and the task/plan ceilings and percentages match.
- Q1a: `tasks.md:51-60` marks T012–T021 checked, `evidence-q1a.md` records 7/7 Debug and Release
  CTest, `review-q1a.md` returns ACCEPT, and the seven test names match
  `tests/renderer/CMakeLists.txt:46-52`.
- Full Q1 is consistently **blocked**: debug layer and GPU-based validation are SKIPPED with
  `hr=0x887A002D` in `evidence-q1a.md:206-207`, `review-q1a.md:51-54` and `tasks.md:73`.
- No document reviewed claims Q1, classic parity, DXR, DLSS, Q2 or a WebGPU backend; Q7a explicitly
  disclaims all of them (`evidence-q7a.md:5-7`, `webgpu-mapping.md:3-6,148`).
- The Q1a header and public tests contain no D3D12 vocabulary; the validator reports
  "header scan: RUN over 1 header(s) and 3 public test file(s)" with exit 0.
- The Q7a mapping validator reports 52/52 declarations mapped with exit 0; the mapping's NOT RUN legs
  (software adapter, OOM, Emscripten/Dawn, `wasm32`, performance) are labelled consistently in
  `webgpu-mapping.md` and `evidence-q7a.md`.
- T004/T005 are checked on a recorded agent sign-off with the human caveat; the T013 row/tolerance
  policy is recorded in the contract and the evidence.
- T032/T033/T041 are ticked only as design deliverables; `evidence-q2-design.md:239-244,258-261`
  explicitly leaves the real adapter, engine run and captures undone.
- T084's "Partial" note matches the tree: the apply-path controls exist in `scripts/test-target-patches.ps1`;
  the exporter/verifier controls remain open.
- `traceability.md` contains a row for every FR-001–FR-019 and SC-001–SC-008, so its defect is
  freshness and labelling, not missing requirements.
- `docs/SKILLS.md`'s 25-skill count matches `.claude/skills` (15 non-`speckit-*` + 10 `speckit-*`), and
  its "emcc is not installed" statement matches the Q7a NOT RUN reason. `docs/ROADMAP.md` does not
  claim Q1, parity, DXR, DLSS or WebGPU support; its WebGPU line remains "after M6".
- Q0's synthetic baseline suite is correctly delegated: `tests/renderer/test-render-baseline.ps1` is a
  documented shim to `tests/baseline/test-render-baseline.ps1`, and the evidence cites the latter.

## Disposition after the author response (2026-10-29)

The author's changes were re-read in the staged tree. "Resolved" means the defect as written no
longer exists; "partial" means the conflict is reduced or explicitly scoped but the normative
defect remains; "open" means unchanged or deliberately deferred, with an owner.

| ID | Status | Change (file:line) | One-line justification |
|---|---|---|---|
| B1 | Open — owned by #36 (Q7/WebGPU blocker only, not this PR) | GitHub issue #36 (verified with `gh issue view 36`); `webgpu-mapping.md:145-153`; `tasks.md:186,192`; `contracts/rhi-foundation.md:121` unchanged | Issue #36's acceptance criteria cover the five declarations, require `rhi.h` and the contract to stop contradicting each other, rerun both validators, and add an independent semantic review; its scope is `rhi.h`, the contract, `webgpu-mapping.md` and `tasks.md`, and it explicitly blocks only Q7/WebGPU. The conflict is therefore owned, but still not fixed. If #36 never lands: the core keeps five fields the contract forbids, a WebGPU backend must return `Unsupported`/zero without a sanctioned capability signal, and every Q7/WebGPU claim stays unsupported. |
| B2 | Resolved (in the worktree) | `review-q7a.md:3` (status marker), `:242` (first pass superseded), `:366-371` (final decision); `evidence-q7a.md:109-123` | The file now opens with an explicit "superseded" marker, records a second pass, and ends "Accept with limitations"; the first-pass blocking items are fixed and the remaining items are documented limitations/minor robustness nits. The current verdict is unambiguous. Note: at re-check time the second pass was present in the worktree but the staged copy was still the first-pass version, so it must be re-staged before the commit. |
| B3 | Resolved | `evidence-q7a.md:13-14,68-71` | The record now names base `d9bf1ca` and the `git log --oneline -- specs/.../evidence-q7a.md` command that identifies the carrying commit; the JSON/PNG are staged. Wording judged, not the pre-commit state, as instructed. |
| M1 | Resolved | `evidence-q7a.md:3-9,100`; `webgpu-mapping.md:26-36` | The claim is now "mapped or explicitly recorded as unmappable", scoped to the `rhi.h` declarations, and the gate's structural limits are stated. |
| M2 | Resolved | `plan.md:94` | T004/T005 are now recorded as signed off on `11ea64a` with the human caveat; the "remains unchecked" wording is gone. |
| M3 | Partially resolved | `plan.md:84`; `plan.md:63-72` | The Q7a row now records the partial state, G8 and T089, but the project-structure block still calls `src/renderer/rhi/` and `tests/renderer/` "Planned" and still omits `tests/webgpu/` and the two Q7a scripts. |
| M4 | Partially resolved | `quickstart.md:3,42,92-111`; `quickstart.md:44` | The status line, Q1a heading and Q7a command section are fixed, but line 44 still says the now-implemented Q1a source paths are "planned" and line 38 still understates the header scan scope (see m1). |
| M5 | Partially resolved | `traceability.md:11-12,35`; `traceability.md:3` | FR-001, FR-002 and SC-001 are now Established/Met, but the status line still presents Q1a as the only complete work and omits Q0 and the T032/T033/T041 design ticks. |
| M6 | Partially resolved | `traceability.md:27,5` | FR-017 is now Partial and `review-q7a.md` was added, but `webgpu-mapping.md`, `evidence-q2-design.md` and `review-q2-design.md` are still absent from the artifact index and there is no gates subsection. |
| M7 | Resolved | `tasks.md:192` | The dependency text now states that T035 and T080 share `engine_scene_adapter.cpp` and must not run concurrently. |
| M8 | Open (deferred) | `spec.md:158,171-184` unchanged | No FR/SC for the Q7a gate was added; the author will report it as an open follow-up rather than edit `spec.md` in this task-scoped PR. Owner: spec/T089. |
| M9 | Resolved | `tasks.md:185` | T088 now names issue #35 and its owned spec, evidence, script and test paths. |
| M10 | Resolved | `evidence-q2-design.md:242`; `review-q2-design.md:149` | Both now record the executed Q7a legs and leave only the encoder leg and Q2 blocked. |
| m1 | Open | `quickstart.md:38`; `scripts/validate-rhi-contract.py:449-459` | Still says "core headers under `src/renderer/rhi/`"; the validator scans all of `src/renderer` except the private prefixes. Owner: T005. |
| m2 | Open | `tasks.md:185` | T088 still says `src/renderer/rhi/` and excludes only `tests/renderer/d3d12/`; the contract also excludes the two `ext/` trees. Owner: T088. |
| m3 | Open | `traceability.md:19,24,29` | FR-009/FR-014 still say "partial" with no executed artifact; FR-019 is still "Planned" although T001/T033 are checked. Owner: T087. |
| m4 | Open | `tasks.md:5` | The file still says it is "not a claim that any task or gate has passed" while Q0/Q1a results are recorded. Owner: T087/T076. |
| m5 | Open | `tasks.md:174` | T087 is still unchecked although `traceability.md` is maintained. Owner: T087. |
| m6 | Open | `contracts/rhi-foundation.md:128`; `rhi.h:68-70`; `webgpu-mapping.md:121-124` | The contract and header still say per-row "clear"; G1 says a WebGPU lowering has no per-row clear. Owner: T004/T089. |
| m7 | Open | `contracts/rhi-foundation.md:99`; `webgpu-mapping.md:62,80,129` | The contract still offers "poll"; the mapping shows WebGPU completion is a queue-wide promise only. Owner: T004. |
| m8 | Open | `review-q2-design.md:5,140` | Still says no reviewer approved T032/T033/T041 and also that they were ticked after the PASS. Owner: T032/T033/T041. |
| m9 | Partially resolved | `tasks.md:192` | T035/T080 is now explicit, but T042/T046, T043/T045, T027/T085, T051/T081 and T052/T082 still have no declared same-file edges. Owner: plan/T078. |
| m10 | Open | `docs/ROADMAP.md:77` | The M1 exit criterion still says the rebuild "is 32-bit" against the recorded x64 `0x8664` build. Owner: T076. |
| m11 | Open | `tasks.md:30` | T008 still names `tests/renderer/test-render-baseline.ps1`; the suite is in `tests/baseline/test-render-baseline.ps1` and the renderer path is a shim. Owner: T008. |
| m12 | Open | `review-rhi-contract.md:137` | The dated sign-off block still lists Q7a/T088 as NOT RUN. Owner: T004. |
| m13 | Resolved | `tasks.md:1`; `git diff --cached --check` exit 0 | The BOM and trailing blank lines are gone; the staged whitespace gate passes. |
| N1 | Partially addressed (open) | GitHub issue #36; `tasks.md:7,186` | Issue #36 exists and its acceptance criteria and in-scope paths cover T089, but `tasks.md:186` still does not name issue #36 or its owned paths and `tasks.md:7` still says "New remediation tasks T080–T088" with T088 as "the one exception". Add "(issue #36; owns ...)" to T089 and extend the remediation sentence to T089. Owner: T087/#36. |
| N2 | Open (new, MINOR) | `evidence-q7a.md:57-67`; `quickstart.md:103` | The evidence command table and quickstart still omit `validate-rhi-contract.py --self-test`, which `review-q7a.md:26,149` records as required by issue #35 and passing. Owner: T088. |

Counts: 8 resolved, 5 partially resolved, 15 open (including the #36-owned B1 and the deferred M8).

## Original decision (first pass)

**CHANGES REQUESTED.** B1–B3 and M1–M10 must be resolved before the Q7a record is treated as a gate
output or before any Q2 work starts. The Q1a and Q0 records themselves are internally consistent;
the defects are in the contract rule, the Q7a evidence identity/review, the stale plan and quickstart,
and the traceability matrix.

## Final decision

**Accept this documentation change; no BLOCKING item remains in this PR.** B1 is an owned follow-up
(issue #36) that blocks only Q7/WebGPU, and B2's second pass now ends "Accept with limitations". The
Q7a record is honestly partial, carries an independent review, and is correctly kept from closing Q2.
Q1b can proceed; Q2 remains blocked by the encoder leg, and Q7/WebGPU remains blocked until #36 lands.
If #36 were never done, the header/contract contradiction and the missing capability signal would
remain, so no WebGPU profile could honestly claim those five fields.
