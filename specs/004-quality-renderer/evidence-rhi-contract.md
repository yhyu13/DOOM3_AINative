# Evidence: RHI contract and evidence template (T004, T005)

**Status**: Author evidence for issue #20, refreshed on the sign-off branch. It is not a review record; the sign-off decision and reviewer are in `review-rhi-contract.md`.

## Identity and scope

- **Stage and claim**: Spec 004 Q0/Q1 prerequisites — the three-tier RHI contract (T004) and the reusable evidence record (T005). Claim: the portable core, D3D12-private tier, native extensions, capability/fallback matrix, shader contract, header/test gate, amended Q1a seam and evidence fields are defined, and their structure and lexical boundaries are validated. No renderer operation is claimed by this evidence.
- **Source identity**: worktree `docs-20-t004-t005-signoff`, branch `docs/20-t004-t005-signoff`, base `origin/main` `8638db8` (the merged Q1a change). The content under review is the commit named in the sign-off block; this evidence-only edit follows that commit. Submodule `doom3_orig/doom3` is initialized at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` (`heads/master`); its porcelain is empty.
- **Data identity and rights**: no game data used. Inputs are tracked Markdown and Python sources plus the merged Q1a public header and tests.
- **Scene and inputs**: none; this stage has no renderer, device or scene.
- **Exclusions and limitations**: no GPU, build, engine integration or browser run. The validator checks document structure and a lexical vocabulary boundary only — not technical truth, permissions, whether any matrix row is implementable, or whether a noun list is complete for a future API. A renamed private concept can still pass; the semantic field mapping review is the control. Q1a execution evidence is `evidence-q1a.md`.
- **Machine-local artifacts**: full output is retained under the ignored `build-t004-evidence/` and `build-rhi-contract-mutations/` directories; paths here are relative to this worktree.

## Environment and configuration

- **Toolchain and configuration**: Windows 11 x64; `py -3` resolves Python 3.14.4; commands run with `-I` (isolated mode). Git 2.25.0.windows.1. PowerShell 5.1.
- **Adapter and driver**: none (no device created by this evidence).
- **Capability and fallback**: not applicable at this stage; the matrix defines selection policy only.
- **Shader provenance and build**: not applicable; the shader contract defines the requirements but no shader was compiled.

## Procedure and retained artifacts

- **Predeclared tolerances**: exact-string structural assertions and exact lexical matches only. There is no numeric tolerance.
- **Commands and exits** (all run from this worktree; logs in `build-t004-evidence/`):

  | # | Command | Exit | Log |
  |---|---|---|---|
  | 1 | `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | `signoff-validate.log` |
  | 2 | `py -3 -I scripts/test-validate-rhi-contract.py` | 0 | `signoff-tests.log` |
  | 3 | `py -3 -I scripts/test-validate-rhi-contract-mutations.py` | 0 | `signoff-mutations.log` |
  | 4 | `git diff --check` | 0 | `signoff-check.log` (contains the exit line) |
  | 5 | `git diff --no-index --check -- /dev/null <new file>` | 1 each | expected: exit 1 is the difference |
  | 6 | `git submodule status -- doom3_orig/doom3`; `git -C doom3_orig/doom3 status --porcelain` | 0; empty | `a9c49da`, initialized |
  | 7 | `py -3 -I build-t004-evidence/check-tables.py` | 0 | `signoff-tables.log` |
  | 8 | `py -3 -I build-t004-evidence/check-links.py` | 0 | `signoff-links.log` |
  | 9 | `(Get-FileHash scripts/validate-rhi-contract.py).Hash` | 0 | `signoff-validator-sha.log` |
  | 10 | Header/test scan over the merged Q1a files | 0 | `signoff-validate.log`: `RUN over 1 header(s) and 3 public test file(s)` |

- **Retained logs and artifacts**: `signoff-validate.log` (17 in-memory controls, 0 failed), `signoff-tests.log` (40 behavioral tests, `OK`), `signoff-mutations.log` (11 mutants rejected; restored validator passes 40 tests), `signoff-check.log` (with `exit=0`), `signoff-tables.log` (34 tables, 0 malformed), `signoff-links.log` (19 links, 0 missing), `signoff-validator-sha.log` (SHA-256 `C3B4C162C4E695462D8BC6027CB1467D96C046193BE352F795721354CFE642A3`), and `build-rhi-contract-mutations/`.
- **Negative controls**: 17 in-memory self-tests (missing file, missing field, empty field, missing results columns, missing contract section, wrong tier, empty cell, six separator forms, missing surface block, prohibited noun in the surface, prohibited LUID in the surface, drive-letter path); 11 source mutants; 40 behavioral tests including 14 identifier-embedded private forms (`AdapterLuid`, `luid_high`, `GPUOnlyValidation`, `ValidationFlags`, `CopySource`, `RenderTarget`, `Presentation`, `PresentFrame`, `CmdList`, `ResetAllocator`, `completionSerial`, `nativeHandle`, `dx12` include, `DebugFlags`) and clean controls (`CompletionToken`, `CopyTexture`, `fluidDensity`, `presentationTime`, `resetFrameCounter`); plus directory-gating fixtures for a prohibited include, a prohibited public test, the `d3d12/`/`ext/` exclusions and an empty header directory.

## Per-check results

| Check / target | Execution | Outcome | Reason or observed result | Artifact / log reference |
|---|---|---|---|---|
| Validator structure and lexical gate | RUN | PASS | Seven files present and nonempty; required sections and eleven matrix rows; template fields; portable surface clean; no drive-letter path in the owned files | `build-t004-evidence/signoff-validate.log` |
| Validator behavioral suite | RUN | PASS | 40 tests, `OK` | `build-t004-evidence/signoff-tests.log` |
| Validator in-memory negative controls | RUN | PASS | 17/17 detected, 0 failed | `build-t004-evidence/signoff-validate.log` |
| Source mutation controls | RUN | PASS | 11/11 mutants rejected by the expected test; restored validator passes 40 tests; digest retained | `build-t004-evidence/signoff-mutations.log`, `signoff-validator-sha.log` |
| Identifier-embedded gate controls | RUN | PASS | 14/14 private spellings rejected; 5 clean portable names accepted | `build-t004-evidence/signoff-tests.log` |
| Whitespace, tracked diff | RUN | PASS | `git diff --check` exit 0; the CRLF notice is not a whitespace failure | `build-t004-evidence/signoff-check.log` |
| Whitespace, new files | RUN | PASS | `--no-index --check` exit 1 for each new file, as expected for a difference | command table above |
| Machine-specific absolute paths | RUN | PASS | No drive-letter path in the seven owned files | `build-t004-evidence/signoff-validate.log` |
| Markdown table shape | RUN | PASS | 34 tables; each row has its header's cell count and an adjacent separator; helpers are worktree-local and not tracked | `build-t004-evidence/signoff-tables.log` |
| Relative-link resolution | RUN | PASS | 19 relative links resolve | `build-t004-evidence/signoff-links.log` |
| Merged Q1a header/test scan | RUN | PASS | Real `src/renderer/rhi/rhi.h` and three public test files from `8638db8` contain no prohibited noun; `d3d12/` and `ext/` are excluded | `build-t004-evidence/signoff-validate.log` |
| LUID / GPU-validation negative | RUN | PASS | `luid` and `enableGpuBasedValidation` → exit 1 with `LUID family` and `GPU validation/based` | `build-t004-evidence/luid-gpubased-negative.log` |
| Header gate, prohibited include | RUN | PASS | Temporary `rhi.h` with `#include <d3d12.h>` and `ID3D12Device` → exit 1; removed afterwards | `build-t004-evidence/header-negative2.log` |
| Public test scan, prohibited noun | RUN | PASS | Temporary `tests/renderer/rhi_clear_test.cpp` with `ID3D12Device` → exit 1 under `public test unit`; removed afterwards | `build-t004-evidence/public-test-negative.log` |
| Private test exclusion | RUN | PASS | Same content under `tests/renderer/d3d12/` and `tests/renderer/ext/` → exit 0 | `build-t004-evidence/private-test-positive.log` |
| Empty header directory | RUN | PASS | An existing `src/renderer/rhi/` with no header → exit 1, `contains no scannable header` | `build-t004-evidence/empty-header-dir.log` |
| Pristine submodule | RUN | PASS | Initialized at `a9c49da`; porcelain empty | command table above |
| GitHub issue #11 body sync | RUN | PASS | Amended body applied with the owner account; the blocking edge still names the T004/T005 sign-off and is removed after it | issue #11 |
| Adversarial reviews | RUN | — | Six agent reviews returned changes-requested; findings resolved. Two sign-off reviewers also returned CHANGES REQUESTED; the final sign-off is pending | `review-rhi-contract.md` |
| GPU, build, engine or browser execution | NOT RUN | — | Out of scope for T004/T005; Q1a evidence is separate | `evidence-q1a.md` |

## Independent review

- **Independent reviewer**: see `review-rhi-contract.md`; the sign-off must be by a reviewer who did not author this change.
- **Review scope and commands**: contract, matrix, shader contract, Q1a seam, evidence, validator and the commands above, including an explicit decision on the T013 expected-row/digest/tolerance policy.
- **Review decision and unresolved items**: recorded in `review-rhi-contract.md`; a structural pass is not this review.
