# Evidence: RHI contract and evidence template (T004, T005)

**Status**: Author evidence for issue #20. Not a review record and not a claim that T004/T005 are complete; an independent review is required before either task can be checked. No renderer, device or browser operation is claimed.

## Identity and scope

- **Stage and claim**: Spec 004 Q0/Q1 prerequisites — the three-tier RHI contract (T004) and the reusable evidence record (T005). Claim: the portable core, D3D12-private tier, native extensions, capability/fallback matrix, shader contract, header/test gate, amended Q1a seam and evidence fields are defined, and their structure and lexical boundaries are validated. No backend operation is claimed to work.
- **Source identity**: worktree `docs-20-rhi-contract-foundation`, branch `docs/20-rhi-contract-foundation`, base `origin/main` `f2589e6`; the content measured by these runs is commit `77364a00f251b60bb85658b28590f3bd7012bca9` (the evidence-only edit that follows this line changes no validated document or script). Submodule `doom3_orig/doom3` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` (`heads/master`), porcelain empty.
- **Data identity and rights**: no game data used. All inputs are tracked Markdown and Python sources in this repository, including the published draft research in `specs/005-web-renderer/`.
- **Scene and inputs**: none; this stage has no renderer, device or scene.
- **Exclusions and limitations**: no GPU, build, engine integration or browser run. The validator checks document structure and a lexical vocabulary boundary only — not technical truth, permissions, whether any matrix row is implementable, or whether a D3D12 noun list is complete for a future API.
- **Machine-local artifacts**: full command output is retained under the ignored directories `build-t004-evidence/` and `build-rhi-contract-mutations/`; paths here are relative to this worktree.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 x64; `py -3` resolves Python 3.14.4; commands run with `-I` (isolated mode). Git 2.25.0.windows.1. PowerShell 5.1.
- **Adapter and driver**: none (no device created).
- **Capability and fallback**: not applicable at this stage; the matrix defines selection policy only.
- **Shader provenance and build**: not applicable; the shader contract section defines the requirements but no shader was compiled.

## Procedure and retained artifacts

- **Predeclared tolerances**: exact-string structural assertions and exact lexical matches only. A document either has the required section, field, tier and contiguous table shape or it does not, and a surface either contains a prohibited noun or it does not; there is no numeric tolerance.
- **Commands and exits** (all run from this worktree; logs in `build-t004-evidence/`):

  | # | Command | Exit | Log |
  |---|---|---|---|
  | 1 | `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | `final6-validate.log` |
  | 2 | `py -3 -I scripts/test-validate-rhi-contract.py` | 0 | `final6-tests.log` |
  | 3 | `py -3 -I scripts/test-validate-rhi-contract-mutations.py` | 0 | `final6-mutations.log` |
  | 4 | `git diff --check` | 0 | `final6-check.log` |
  | 5 | `git diff --no-index --check -- /dev/null <new file>` for the template, validator, validator test, mutation harness and `specs/005-web-renderer/research.md` | 1 each | expected: exit 1 is the file difference, not a whitespace failure |
  | 6 | `git submodule status -- doom3_orig/doom3`; `git -C doom3_orig/doom3 status --porcelain` | 0; empty | recorded above |
  | 7 | `py -3 -I build-t004-evidence/check-tables.py` | 0 | `final6-tables.log` |
  | 8 | `py -3 -I build-t004-evidence/check-links.py` | 0 | `final6-links.log` |
  | 9 | Header/test scan inside the validator | NOT RUN for real files | `final6-validate.log`; `src/renderer/rhi` and `tests/renderer` are absent. The gate was exercised end-to-end on temporary fixtures below |

The `specs/005-web-renderer/research.md` no-index check is a session addition for this publication; the 004 `quickstart.md` command set covers only the 004 files.

- **Retained logs and artifacts**: `final6-validate.log` (16 in-memory negative controls, 0 failed), `final6-tests.log` (38 behavioral tests, `OK`), `final6-mutations.log` (11 source mutants, each rejected by the named test; restored validator passes 38 tests unchanged), `final6-check.log` (clean), `final6-tables.log` (34 tables, 0 malformed), `final6-links.log` (19 links, 0 missing), and `build-rhi-contract-mutations/` (per-mutant diff/log plus `restored.log`). The earlier `red.log`/`red2.log` are a historical red run of the pre-change validator (23-test suite); they are not evidence for the current 38-test suite.
- **Negative controls**: 16 in-memory self-tests (missing file, missing field, empty field, missing results columns, missing contract section, wrong tier, empty cell, six separator forms, missing surface block, prohibited noun in the surface, drive-letter path) and 11 source mutants (hidden Markdown, extra empty cells, detached rows, invalid separators, indented code, escaped pipes, CRLF normalisation, unknown matrix row, ambiguous tables, portable-surface scan, header scan). Each mutant must be rejected by its named test, and the restored tracked validator must pass the unchanged suite. The harness records both.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| Validator structure and lexical gate | RUN | PASS | Seven files present and nonempty; required sections and eleven matrix rows; template fields; portable surface clean; no drive-letter path in the owned files | `build-t004-evidence/final6-validate.log` |
| Validator behavioral suite | RUN | PASS | 38 tests, `OK` | `build-t004-evidence/final6-tests.log` |
| Validator in-memory negative controls | RUN | PASS | 16/16 detected, 0 failed | `build-t004-evidence/final6-validate.log` |
| Source mutation controls | RUN | PASS | 11/11 mutants rejected by the expected test; restored validator passes 38 tests and its SHA-256 is unchanged | `build-t004-evidence/final6-mutations.log`, `build-rhi-contract-mutations/` |
| Whitespace, tracked diff | RUN | PASS | `git diff --check` exit 0 | `build-t004-evidence/final6-check.log` |
| Whitespace, new files | RUN | PASS | `--no-index --check` exit 1 for each new file, as expected for a difference | command table above |
| Machine-specific absolute paths | RUN | PASS | No drive-letter path in the seven owned files | `build-t004-evidence/final6-validate.log` |
| Markdown table shape | RUN | PASS | 34 tables across 004/005/003; every row has its header's cell count and an adjacent separator, with fenced blocks excluded. The helper is worktree-local and not tracked | `build-t004-evidence/final6-tables.log` |
| Relative-link resolution | RUN | PASS | 19 relative Markdown links in the changed documents resolve to existing paths; the helper is worktree-local and not tracked | `build-t004-evidence/final6-links.log` |
| Engine-facing header scan (Q7a/T088) | NOT RUN | — | `src/renderer/rhi/` does not exist; the gate is defined but no real header was scanned | `build-t004-evidence/final6-validate.log` |
| Header gate, prohibited include | RUN | PASS | Temporary `rhi.h` with `#include <d3d12.h>` and `ID3D12Device` → exit 1 with `D3D12` and `ID3D12*`; removed afterwards | `build-t004-evidence/header-negative2.log` |
| Public test scan, prohibited noun | RUN | PASS | Temporary `tests/renderer/rhi_clear_test.cpp` with `ID3D12Device` → exit 1 under `public test unit`; removed afterwards | `build-t004-evidence/public-test-negative.log` |
| Private test exclusion | RUN | PASS | The same content under `tests/renderer/d3d12/private_test.cpp` and `tests/renderer/ext/provider_test.cpp` → exit 0, `public test scan: RUN over 0 file(s)`; removed afterwards | `build-t004-evidence/private-test-positive.log` |
| Empty header directory | RUN | PASS | An existing `src/renderer/rhi/` with no header → exit 1, `contains no scannable header`; removed afterwards | `build-t004-evidence/empty-header-dir.log` |
| Pristine submodule | RUN | PASS | `git submodule status` has no `+`; porcelain empty | command table above |
| GitHub issue #11 body sync | NOT RUN | — | `gh issue edit 11 --body-file ...` failed: GraphQL `UpdateIssue` permission denied for the authenticated account. The intended body is mirrored in `specs/004-quality-renderer/issue-q1a.md` and must be applied by an account with issue-write permission; the mirror says not to implement from the live body | — |
| Adversarial agent reviews | RUN | — | Three reviewer agents returned changes-requested; their findings are recorded and resolved in `review-rhi-contract.md`, but the consistency re-check and a reviewer who is not the author still need to sign off | `review-rhi-contract.md` |
| GPU, build, engine or browser execution | NOT RUN | — | Out of scope for T004/T005 | — |

## Independent review

- **Independent reviewer**: pending — a reviewer who did not author the amendment must verify the three tiers, matrix rows and fallbacks, usage/binding/ordering rules, Q1a seam, shader rights/stages, header/test gate and the T013 policy, then record each unresolved item. Reviewer agents' findings are in `review-rhi-contract.md`; that is not a human sign-off.
- **Review scope and commands**: pending.
- **Review decision and unresolved items**: NOT RUN. A structural command passing is not this review and cannot mark T004 or T005 complete.
