# Evidence: RHI contract and evidence template (T004, T005)

**Status**: Author evidence for issue #20. Not a review record and not a claim that T004/T005 are complete; independent review is required before either task can be checked.

## Identity and scope

- **Stage and claim**: Spec 004 Q0/Q1 prerequisites — the bounded, backend-neutral RHI contract (T004) and the reusable evidence record (T005). Claim: the two-tier seam, ownership rules, capability/fallback matrix, shader contract and evidence fields are defined and structurally validated. No backend operation is claimed to work.
- **Source identity**: worktree `docs-20-rhi-contract-foundation`, branch `docs/20-rhi-contract-foundation`, based on `964d358`; changes are uncommitted at the time of these runs. Submodule `doom3_orig/doom3` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` (`heads/master`), no dirt.
- **Data identity and rights**: no game data used. All inputs are tracked Markdown and Python sources in this repository.
- **Scene and inputs**: none; this stage has no renderer, device or scene.
- **Exclusions and limitations**: no GPU, build, engine integration or browser run. The validator checks document structure only — not technical truth, permissions, adequacy of the contract or whether any matrix row is implementable.
- **Machine-local artifacts**: full command output is retained under the ignored directory `build-t004-evidence/`; paths here are relative to this worktree.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 x64; `py -3` resolves Python 3.14.4; commands run with `-I` (isolated mode). Git 2.25.0.windows.1.
- **Adapter and driver**: none (no device created).
- **Capability and fallback**: not applicable at this stage; the matrix defines selection policy only.
- **Shader provenance and build**: not applicable; the shader contract section defines the requirements but no shader was compiled.

## Procedure and retained artifacts

- **Predeclared tolerances**: exact-string structural assertions only. A document either has the required section, field, tier and contiguous table shape or it does not; there is no numeric tolerance.
- **Commands and exits** (all run from this worktree; logs in `build-t004-evidence/`):

  | # | Command | Exit | Log |
  |---|---|---|---|
  | 1 | `py -3 -I scripts/test-validate-rhi-contract.py` against the pre-change validator | 1 | `red.log` |
  | 2 | `py -3 -I scripts/test-validate-rhi-contract.py` against the changed validator | 0 | `green.log`, `final-tests.log` |
  | 3 | `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | `self-test.log`, `final-validate.log` |
  | 4 | `py -3 -I build-t004-evidence/run_mutations.py` | 0 | `mutants/*.log` |
  | 5 | `git diff --check` | 0 | `final-check.log` |
  | 6 | `git diff --no-index --check -- /dev/null <new file>` for the template, validator and validator test | 1 each | expected: exit 1 is the file difference, not a whitespace failure |
  | 7 | `git submodule status -- doom3_orig/doom3`, `git -C doom3_orig/doom3 status --porcelain` | 0, empty | recorded above |

- **Retained logs and artifacts**: `red.log` (14 failures of 23 — the behavioral red run), `green.log` and `final-tests.log` (23 passed), `self-test.log` (13 in-memory negative controls passed, 0 failed), `mutants/` (five mutants with diffs and rejection logs plus `restored.log`), `final-validate.log`, `final-check.log`.
- **Negative controls**: five independent mutants of `scripts/validate-rhi-contract.py`, each disabling one protected behavior, were each rejected by the named test: hidden Markdown (fenced tables/headings counted), extra empty edge cells, detached table rows, invalid separators, indented code. The unchanged suite then passed against the restored file and the file hash was verified unchanged.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| Validator behavior suite, pre-change validator | RUN | FAIL | 14 of 23 tests fail; detached rows, separator position/content, hidden Markdown visibility and extra edge cells are accepted | `build-t004-evidence/red.log` |
| Validator behavior suite, changed validator | RUN | PASS | 23 tests pass | `build-t004-evidence/final-tests.log` |
| Validator self-tests | RUN | PASS | 13 negative controls detected, 0 failed | `build-t004-evidence/self-test.log` |
| Mutation controls | RUN | PASS | 5/5 mutants rejected by the expected test; restored file passes 23 tests | `build-t004-evidence/mutants/` |
| Whitespace, tracked diff | RUN | PASS | `git diff --check` exit 0 | `build-t004-evidence/final-check.log` |
| Whitespace, new files | RUN | PASS | `--no-index --check` exit 1 for each new file, as expected for a difference | command table above |
| Pristine submodule | RUN | PASS | `git submodule status` has no `+`, porcelain empty | command table above |
| Tracked absolute-path scan | RUN | PASS | no drive-letter paths in the new or changed documents and scripts | command table above |
| Contract seam and ownership review | NOT RUN | — | requires an independent reviewer | this record's review section |
| GPU, build, engine or browser execution | NOT RUN | — | out of scope for T004/T005 | — |

## Independent review

- **Independent reviewer**: pending — a separate reviewer must verify the matrix, ownership rules, factual claims about the repository scripts, and record unresolved items.
- **Review scope and commands**: pending.
- **Review decision and unresolved items**: NOT RUN.
