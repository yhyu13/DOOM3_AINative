# Review: three-tier RHI contract and evidence template (T004, T005)

**Status**: Review record for issue #20. It records reviews by agents that did not author the change, the findings they raised, and what was changed in response. It is **not** a human sign-off and it does not check T004 or T005. The final decision is pending; unresolved items are listed at the end.

**Revision reviewed**: the uncommitted working tree of `docs/20-rhi-contract-foundation`, based on `origin/main` `f2589e6`, plus `specs/005-web-renderer/research.md` (untracked at review time).

## Reviewers and scope

| Reviewer | Independence | Scope | Decision |
|---|---|---|---|
| Gap audit (fresh-context agent) | Did not author; separate context | GitHub issue #20 acceptance criteria 1–7 against the committed draft and the live tree | CHANGES REQUESTED; re-audit pending |
| Consistency audit (fresh-context agent) | Did not author; separate context | 004 documents, 003 §11, 005 research, scripts, cross-references, stale claims | CHANGES REQUESTED; fixes applied, re-audit pending |
| Engine-citation audit (fresh-context agent) | Did not author; separate context | Every `File.cpp:line` citation in 005 §2 against submodule `a9c49da` | CHANGES REQUESTED; fixes applied and spot-checked |
| Contract review (fresh-context agent) | Did not author; separate context | Issue #20 criteria, tiers, matrix, Q1a seam, header gate, T013, validator claims | running |
| Cross-document review (fresh-context agent) | Did not author; separate context | Contradictions, anchors, task IDs, status claims, scope drift | running |
| WebGPU claims review (fresh-context agent) | Did not author; separate context | Limits, usage scopes, shader input, ray tracing, availability claims | running |

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
- **Resolution**: renamed to `evidence-rhi-contract.md`, refreshed identity, counts (35 tests, 16 self-tests, 10 mutants), new logs, per-check outcomes, and the issue-#11 permission failure. The historical red log is labelled historical.
- **Status**: resolved.

### 4. Five stale 005 claims and one wrong tense

- **Finding**: 005 still said 003/004 were untracked, described a two-tier design, claimed 004 had no device-loss/surface concept, and used present-tense claims that the amendment has made false.
- **Resolution**: 005 now records publication, names the three tiers, states what the amended core does and does not contain, and keeps the historical audit in the past tense. The S0 row and Appendix C distinguish completed from open items.
- **Status**: resolved.

### 5. Engine citations were partly wrong

- **Finding**: `_currentRender` was attributed to `RC_COPY_RENDER`; TMU0/TMU6 and vertex attributes were attributed to `DrawInteraction`; several line ranges were off by a few lines; the NDC range was overstated as exactly `[-1, 1]`.
- **Resolution**: nine citations corrected, including `_currentRender` (`draw_common.cpp:984-996`), the per-light split (`draw_arb2.cpp:165-198`), the stencil-op selection (`RenderSystem_init.cpp:418-424`), `GuiModel.cpp:226-297` and the `[-1.001, 0.999]` NDC range. The corrected lines were spot-checked directly in the submodule.
- **Status**: resolved; re-audit pending.

## Checks run for this review

- `py -3 -I scripts/validate-rhi-contract.py --self-test` → exit 0; 16/16 in-memory negative controls detected.
- `py -3 -I scripts/test-validate-rhi-contract.py` → exit 0; 35 tests, `OK`.
- `py -3 -I scripts/test-validate-rhi-contract-mutations.py` → exit 0; 10/10 mutants rejected; restored validator passes 35 tests.
- Temporary-header gate: a header containing `ID3D12Device` → exit 1 with the expected diagnostic; a clean header → exit 0 and `header scan: RUN over 1 file(s)`; the temporary directory was removed.
- `git diff --check` → exit 0; new files checked with `--no-index` (exit 1 = difference).
- `git -C doom3_orig/doom3 status --porcelain` → empty.
- 34 Markdown tables → consistent cell counts and adjacent separators; 19 relative links → all resolve.

## Unresolved items

1. **Human/independent sign-off is not recorded.** The reviewers above are agents; the repository's rule is an independent reviewer who did not write the change. T004 and T005 stay unchecked until that sign-off is recorded.
2. **Issue #11 body still needs the mirrored text.** `gh issue edit 11` failed with GraphQL `UpdateIssue` permission denied. Apply `specs/004-quality-renderer/issue-q1a.md` with an account that can edit issues.
3. **The header gate has not scanned a real public header.** The scan works end-to-end on a temporary header, but `src/renderer/rhi/**` does not exist, so the Q7a/T088 gate remains NOT RUN for the actual implementation.
4. **Q7a conformance spike is not built.** The plan now names it (T088); no Dawn/pinned-browser fixture, shader compile or browser frame exists.
5. **The final contract/consistency/WebGPU review results are pending** and must be appended before this record is presented as complete.

## Decision

**CHANGES REQUESTED → re-review pending.** The blocking findings above are addressed in the reviewed tree; the outstanding items are the independent sign-off, the live issue update, and the three final reviewer passes.
