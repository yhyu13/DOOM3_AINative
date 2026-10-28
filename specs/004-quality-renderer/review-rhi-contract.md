# Review: three-tier RHI contract and evidence template (T004, T005)

**Status**: Review record for issue #20. It records six agent reviews and two sign-off rounds, their findings and resolution. The sign-off is an **agent** review, not a human reviewer; that caveat is carried into the decision. The final sign-off block is appended only after the reviewer approves the frozen commit.

**Revision reviewed**: branch `docs/20-t004-t005-signoff`, based on `origin/main` `8638db8` (the merged Q1a change), at the gate-broadening commit named in the sign-off block. The merged Q1a header and public tests are present and scanned.

## Reviewers and scope

| Reviewer | Independence | Scope | Decision |
|---|---|---|---|
| Gap audit (fresh-context agent) | Did not author; separate context | GitHub issue #20 acceptance criteria 1–7 against the committed draft and the live tree | CHANGES REQUESTED; re-audit pending |
| Consistency audit (fresh-context agent) | Did not author; separate context | 004 documents, 003 §11, 005 research, scripts, cross-references, stale claims | CHANGES REQUESTED; fixes applied, re-audit pending |
| Engine-citation audit (fresh-context agent) | Did not author; separate context | Every `File.cpp:line` citation in 005 §2 against submodule `a9c49da` | CHANGES REQUESTED; fixes applied and spot-checked |
| Contract review (fresh-context agent) | Did not author; separate context | Issue #20 criteria, tiers, matrix, Q1a seam, header gate, T013, validator claims | CHANGES REQUESTED; 16 findings addressed, re-check pending |
| Cross-document review (fresh-context agent) | Did not author; separate context | Contradictions, anchors, task IDs, status claims, scope drift | CHANGES REQUESTED; findings addressed, re-check pending |
| WebGPU claims review (fresh-context agent) | Did not author; separate context | Limits, usage scopes, shader input, ray tracing, availability claims | CHANGES REQUESTED; 3 factual errors corrected |

## Findings and resolution

### 1. The public Q1a seam still exposed D3D12 mechanisms

- **Finding**: the draft kept `RENDER_TARGET`/`COPY_SOURCE`/`COPY_DEST` state assertions, a fence wait and `GetCompletedValue() < signalValue` in the *public* boundary and public acceptance, contradicting the contract's own rule that the caller is not given D3D12 states.
- **Resolution**: the contract now has an explicit **D3D12-private implementation** tier. The public seam returns an opaque completion token; the state journal and fence values are reached only through a backend-private diagnostic hook. The same split is carried into `data-model.md`, `plan.md`, `tasks.md` (T012–T019), `quickstart.md`, `issue-q1a.md` and `traceability.md`.
- **Status**: resolved in the reviewed tree; requires the contract reviewer to confirm.

### 2. Two tiers where the requested boundary has three

- **Finding**: the draft split "portable core + native extensions" and left the D3D12-private implementation implicit, so a reviewer could not trace which mechanisms are private.
- **Resolution**: added the three-tier table, the tier-mapping table from the previous vocabulary, and a note that backend-private mechanisms have no capability-matrix row.
- **Status**: resolved.

### 3. The evidence record was stale

- **Finding**: `evidence-contract.md` described a `964d358` base and a 23-test suite; the suite had grown and the branch had moved. It also did not record the new lexical checks.
- **Resolution**: renamed to `evidence-rhi-contract.md`, refreshed identity, counts (38 tests, 16 self-tests, 11 mutants), new logs, per-check outcomes, and the issue-#11 permission failure. The historical red log is labelled historical.
- **Status**: resolved.

### 4. Five stale 005 claims and one wrong tense

- **Finding**: 005 still said 003/004 were untracked, described a two-tier design, claimed 004 had no device-loss/surface concept, and used present-tense claims that the amendment has made false.
- **Resolution**: 005 now records publication, names the three tiers, states what the amended core does and does not contain, and keeps the historical audit in the past tense. The S0 row and Appendix C distinguish completed from open items.
- **Status**: resolved.

### 5. Engine citations were partly wrong

- **Finding**: `_currentRender` was attributed to `RC_COPY_RENDER`; TMU0/TMU6 and vertex attributes were attributed to `DrawInteraction`; several line ranges were off by a few lines; the NDC range was overstated as exactly `[-1, 1]`.
- **Resolution**: nine citations corrected, including `_currentRender` (`draw_common.cpp:984-996`), the per-light split (`draw_arb2.cpp:165-198`), the stencil-op selection (`RenderSystem_init.cpp:418-424`), `GuiModel.cpp:226-297` and the `[-1.001, 0.999]` NDC range. The corrected lines were spot-checked directly in the submodule.
- **Status**: resolved; re-audit pending.

### 6. Contract review: 16 findings, all addressed in the tree

- **`core-present` contradicted the private-presentation rule.** Renamed to `core-surface-acquire` (surface configure/acquire and frame commit); the matrix, validator map and tests now use the new name, and a test rejects a return to `core-present`.
- **The staging-pitch control was not expressible through the public seam.** The public seam returns tightly packed bytes and never exposes the pitch, so the pitch control now belongs to the D3D12-private companion test; the public test owns the off-by-one row, repeat-row-0 and packed-row-length controls. The contract, T013, T015 and the issue mirror say which test owns which control.
- **T015 broke state in a test that no longer asserts it.** T015 now breaks row indexing and packed-row length in T013 and the state journal in its private companion.
- **The lexical gate was too narrow and skipped public test units.** It now rejects `d3d12`, `dxgi`, `iunknown`, `comptr`, `wrl` and scans public test units under `tests/renderer/` while excluding `tests/renderer/d3d12/`. The contract places native-extension headers outside `src/renderer/rhi/`.
- **A header directory with no scannable header passed.** It is now a failure, and the note distinguishes a missing path (NOT RUN) from an empty one (FAIL).
- **`quickstart.md` claimed the scripts were path-scanned.** The validator scans the seven owned documents; the wording now says so.
- **"The public test asserts only the completed readback" was over-broad.** Scoped to state: the same bullet now points to the completion-token assertion.
- **The evidence identity was stale.** Refreshed after the commit that contains the tested content; the record distinguishes a missing path from an empty one.
- **The live issue #11 still shows the old seam.** The mirror now says explicitly: do not implement from the live body; apply the mirrored text first.
- **T088 was a hard Q2 prerequisite with undefined tooling.** It now requires a named encoder (Dawn native, or a pinned browser plus the Emscripten WebGPU library), version/commit, license, acquisition, storage and authorization, and is fail-closed with NOT RUN.
- **Two traceability rows still said "two-tier".** Corrected to three-tier.
- **`sparse resources and aliasing` had no matrix row.** Added `native-sparse-aliasing` with its fallback and evidence.
- **The data-model GPU resource still carried a private journal.** The journal is now stated as D3D12-private and outside the model.
- **The plan omitted the mutation command.** Added to the verification list.
- **The index/worktree split was a commit hazard.** Resolved by committing the rename and content together.

### 7. WebGPU claims: three factual errors corrected

- `rg16float` is a core renderable/blendable format; only its storage-texture use needs `texture-formats-tier1`. The §3.2 row now says that and agrees with §4.1.
- The caniuse figures were wrong: the live caniuse-db data fetched 2026-10-28 reports 85.8% full and 3.66% partial, not 2.02%/87.82%, and the comparison with an "earlier 3.66%" was backwards. The fixed text records the fetch date.
- The G-buffer byte arithmetic counted an `rgba8unorm` render target as 4 B; the render-target byte cost is 8 B, so the named four-attachment layout is 28 B, not 16 B. The "fits in 32 B" conclusion is unchanged, and depth is explicitly excluded from the limit.
### 8. Cross-document consistency review

- **Counts and logs were stale.** Refreshed to 38 tests, 11 mutants, eleven matrix rows, and `final6-*` logs produced at the tested commit.
- **The zero-file rule was asymmetric.** The contract now fails only an existing header directory with no scannable header; the public-test scan reports its count and may be zero before the first fixture.
- **Scope:** 005, the 003 §11 amendment and the issue #11 mirror are outside issue #20's file list but are explicitly assigned by the coordinating S0 brief; they are included on that authorization and disclosed in the PR rather than treated as issue #20 scope.
- **The gate omitted private nouns.** Added query heap, pipeline library, DRED and debug layer/GPU-based validation. Public-test scanning now excludes both `tests/renderer/d3d12/` and `tests/renderer/ext/`; T016/T019 private tests and T059/T061 provider files were moved to those directories.
- **Core wording still said "reset".** Replaced with discard/release; the data-model command-submission entity now names recording storage and binding allocation, not allocator/descriptor.
- **Evidence list and link artifact.** The self-test list now matches the 17 actual cases; the link check is retained as `signoff-links.log`.
- **SC-008 / T088.** Stated that T088 is a pre-Q2 conformance spike, not target-support evidence.
- **004 research / 003 D2.** Added the backend-proven-retirement wording and a D2 pointer to the dated amendment.
- **T004 review file.** Named `review-rhi-contract.md` in T004.
- **005 terminology.** `core-present` → `core-surface-acquire`, completion handle/handle → completion token, and the public-test claim corrected to "no longer requires them in the public test". Publication and checklist status now say "merge pending".
- **T088 placement.** The task format note records T088 as the one task whose text places it before Q2 despite its cross-cutting phase.
### 9. Sign-off round 1 (two non-author reviewers)

- **Gate overfit.** Identifier-embedded forms (`AdapterLuid`, `luid_high`, `GPUOnlyValidation`, `ValidationFlags`, `CopySource`, `RenderTarget`, `Presentation`, `PresentFrame`, `CmdList`, `ResetAllocator`, `completionSerial`, `nativeHandle`, a `dx12` include, `DebugFlags`) passed the token-exact patterns. The gate now uses identifier-aware patterns, 14 bypass forms are rejected, and five portable names remain clean (40 behavioral tests).
- **Gate scope.** The scan now covers all engine-facing headers under `src/renderer/` except `d3d12/` and `ext/`, and public test units include `.cc`/`.cxx`.
- **Evidence artifacts.** The four fixture logs are regenerated in this worktree, the check log records `exit=0`, the validator SHA-256 is retained, and duplicate/absent references were removed.
- **Submodule claim.** Corrected: initialized at `a9c49da`, porcelain empty.
- **Counts and status.** 17 self-tests, 40 behavioral tests, 11 mutants; six reviews and two sign-off rounds; the premature "final sign-off" wording is removed.
- **Mirror divergence.** `issue-q1a.md` is reverted to `origin/main` in this PR; the live issue #11 body is updated only after the sign-off, so the mirror cannot claim a state that is not live.
- **T013 policy.** The final sign-off scope explicitly requires an APPROVE/REJECT decision on the expected-row/digest/tolerance policy.
- **Other fixes.** Q1a's public boundary now promises adapter info rather than the full capability profile; T012 names the broadened noun set; T005's traceability note is updated with the sign-off.
- **Residual risk.** The gate is a lexical tripwire, not a proof: a renamed private concept or a transitive include can still pass. The semantic field-to-WebGPU mapping (T004/Q7a) remains the control.
## Checks run for this review

- `py -3 -I scripts/validate-rhi-contract.py --self-test` → exit 0; 17/17 in-memory negative controls detected.
- `py -3 -I scripts/test-validate-rhi-contract.py` → exit 0; 40 tests, `OK`.
- `py -3 -I scripts/test-validate-rhi-contract-mutations.py` → exit 0; 11/11 mutants rejected; restored validator passes 40 tests.
- Header/test gate exercised end-to-end on temporary fixtures: a header containing `#include <d3d12.h>` → exit 1 with `D3D12` and `ID3D12*`; a public test unit containing `ID3D12Device` → exit 1; the same content under `tests/renderer/d3d12/` → exit 0; an empty header directory → exit 1 with `contains no scannable header`; a clean header → exit 0. All temporary paths were removed.
- `git diff --check` → exit 0; new files checked with `--no-index` (exit 1 = difference).
- `git -C doom3_orig/doom3 status --porcelain` → empty.
- 34 Markdown tables → consistent cell counts and adjacent separators; 19 relative links → all resolve.

## Unresolved items

1. **Human/independent sign-off is not recorded.** The reviewers above are agents; the repository's rule is an independent reviewer who did not write the change. T004 and T005 stay unchecked until that sign-off is recorded.
2. **Resolved:** the amended issue #11 body was applied with the owner account on 2026-10-28T11:10Z; the mirror is `specs/004-quality-renderer/issue-q1a.md`.
3. **Resolved for the lexical gate:** the merged Q1a header and three public test files are scanned (`RUN`, exit 0). The semantic WebGPU mapping and conformance spike (Q7a/T088) remain NOT RUN.
4. **Q7a conformance spike is not built.** The plan now names it (T088); no Dawn/pinned-browser fixture, shader compile or browser frame exists.
5. **A final re-check by a reviewer who did not write the fixes is pending.** The six reviews and the two sign-off rounds returned changes-requested; their findings are addressed above. This record is not complete until a non-author reviewer approves the frozen commit and a human/owner sign-off is recorded.

## Decision

**CHANGES REQUESTED → re-review pending.** The blocking findings above are addressed in the reviewed tree; the outstanding items are the non-author sign-off and the human/owner caveat.
