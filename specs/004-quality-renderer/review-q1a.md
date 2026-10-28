# Independent review: Q1a D3D12 RHI clear/readback

**Date**: 2026-10-28. **Status**: final review record for issue #11. This file **replaces** the 2026-10-27
review, which covered the superseded pre-amendment seam. The reviewed code is commit
`1c0187b` and the evidence/fix commit `369c47b`; the round-2 reviewer accepted both. This
review was performed by an agent with a fresh context that did not write the code. It is
independent of the author but is **not a human sign-off**; a human reviewer must still own
the merge decision for a production claim.

**Verdict**: **ACCEPT** for Q1a code and evidence. No blocking code defect was found. The
remaining items are an allocation guard added after acceptance (N2, disclosed below) and the
normal human sign-off. Full Q1 is still **blocked** because the debug layer and GPU-based
validation are SKIPPED on this machine (`hr=0x887A002D`), not passed.

## Independence and scope

- **Reviewer**: independent agent reviewer, no authorship of the implementation, tests,
  contract amendment or evidence. Round 1 reviewed commit `1c0187b` plus the first corrected
  evidence draft; round 2 reviewed `369c47b` with the corrections committed.
- **Scope**: issue #11 (Q1a) only, against `issue-q1a.md`, the amended three-tier contract
  (`contracts/rhi-foundation.md`: Public boundary, Header gate, First vertical-slice
  acceptance, T013 policy) and tasks T012–T021. Q1b–Q7, full Q1, engine integration, browser
  portability and performance are out of scope.
- **Independence limits**: the reviewer did not verify a historical pre-implementation red
  checkout (none exists for the amended 7-test suite; the evidence says so), device-removal
  fault injection, an actual debug-layer/GBV execution, the live GitHub issue body, or
  engine/browser integration.

## Commands rerun (round 2, fresh directory)

| Command | Exit | Result |
|---|---|---|
| `py -3 -I scripts/validate-rhi-contract.py` | 0 | header scan RUN over 1 header and 3 public test files |
| `py -3 -I scripts/validate-rhi-contract.py --self-test` | 0 | 16/16 negative controls |
| `py -3 -I scripts/test-validate-rhi-contract.py` | 0 | 38 tests OK |
| `py -3 -I scripts/test-validate-rhi-contract-mutations.py` | 0 | 11 protected behaviours rejected; restored 38 tests exit 0 |
| `cmake -S tests/renderer -B build-q1a-review2 -G "Visual Studio 17 2022" -A x64` | 0 | fresh directory |
| Debug build | 0 | 0 warning/error lines |
| Debug CTest, `-V` + JUnit | 0 | 7/7; `tests=7 failures=0 skipped=0` |
| Release build | 0 | 0 warning/error lines |
| Release CTest, `-V` + JUnit | 0 | 7/7; `tests=7 failures=0 skipped=0` |
| Mutation M5 (outstanding refusal removed) | 8 | named test failed with the intended assertions; restored byte-identical and 7/7 green |

## Adapters and executed versus skipped

| Adapter | Identity | Executed | Skipped |
|---|---|---|---|
| Hardware | Intel(R) Graphics, vendor 0x8086, device 0x7D67, driver 32.0.101.8331 | all 7 CTest cases, both configurations; clear/readback and private cases | none |
| Software | Microsoft Basic Render Driver (WARP), vendor 0x1414, device 0x008C | same | none |

Debug layer and GPU-based validation were requested, attempted, and recorded **SKIPPED** on
both adapters with `hr=0x887A002D`; `validation_executed=0`. JUnit `skipped=0` is a CTest
case count, not a claim that those checks ran. The diagnostics case passes because it tests
the opt-in reporting behavior, not because validation executed.

## Round-1 findings and disposition

| # | Finding | Disposition |
|---|---|---|
| 1 | Reviewed evidence was uncommitted; HEAD's evidence said remediation was incomplete | Fixed: code and corrected evidence are committed; working tree clean |
| 2 | The 2026-10-27 review was stale and claimed PASS for the old seam | Fixed: this file replaces it |
| 3 | No recorded red assertion for the amended suite (T013/T016) | Fixed: stub and mutation reds are recorded, with an explicit note that no historical pre-implementation checkout exists |
| 4 | Changed-files list missing | Fixed: 13-path list added to the evidence |
| 5 | Validator self-tests not recorded | Fixed: 16/16, 38 tests, 11 mutation behaviours, all exit 0 |
| 6 | "One ordered queue" wording ignored the private gate queue | Fixed: one work queue plus a private gate-signal queue |
| 7 | M4 described imprecisely | Fixed: bounded wait/timeout then release |
| 8 | M2 deltas and M6 timing overstated | Fixed: 255/224/240 and "continues after the integrity failure" |
| 9 | Public in-flight test could release a submission and then wait on it | Fixed: released submissions are tracked and skipped; at least one readback is required |
| 10 | Incomplete-init failure happens before backend objects exist | Fixed by scope note: out-of-range adapter index is the exercised failed-setup path; post-device failure is not fault-injected |
| 11 | Quarantine store could overwrite an earlier set | Fixed: one leaked store per quarantined device |
| 12 | Removed-device sentinel collapsed to 0 | Fixed: `FenceObservation::deviceRemoved` is separate and asserted false in the run |
| 13 | Second Shutdown after quarantine returned Ok | Fixed: `cleanupIncomplete` is sticky until a new Initialize |
| 14 | Branch behind its remote | Process: publish with a force-push of the feature branch and update PR #18 |
| 15 | `DIAGNOSTICS_SUMMARY pass=8` invited misreading | Fixed: `cases_pass` and `validation_executed` are reported separately |

## New findings

| ID | Severity | Finding | Disposition |
|---|---|---|---|
| N1 | Low, record-keeping | The evidence said a round-2 review was recorded while the 2026-10-27 file was still present | Fixed by replacing this file and correcting the evidence sentence |
| N2 | Low, latent robustness | `Submit` did not guard `bad_alloc` around journal/map bookkeeping | Fixed after round 2 with a catch returning `OutOfMemory`; the author rebuilt and reran Debug/Release 7/7. This delta was then verified by a final delta review at `b059007`; it is a defensive path with no behavior change on the tested route |

## What was verified

- The public header and the three public test units contain no resource-state, journal,
  fence-value, staging-pitch or debug-layer/GBV vocabulary; the validator scans them and
  excludes `tests/renderer/d3d12/`.
- The private journal records `UNKNOWN -> RENDER_TARGET`, `RENDER_TARGET -> COPY_SOURCE`,
  `UNKNOWN -> COPY_DEST` in that order on both adapters.
- The public readback is tightly packed (`rowStride=16` for 4×4 RGBA8); the private raw path
  sees pitch 256 and detects the wrong-stride control on 3 rows.
- Published FNV-1a digests match the literal expected rows; one-step differences pass and
  two-step differences fail.
- The public in-flight test observes `RefusedOutstanding` on an incomplete token; the private
  test additionally observes `completedValue=0 < signalValue=1`.
- The release path refuses while outstanding and never waits first; M4 and M5 both fail the
  named tests when that property is broken.
- Both adapters execute the clear/readback; the software result is functional only.
- The submodule is clean and no game data, binary, build output or proprietary SDK is tracked.

## What was not verified

- A historical pre-implementation red for the amended suite; the reds are controlled
  stubs/mutations of the final suite, as the evidence states.
- Device removal, TDR recovery, debug-layer/GBV execution, crash dumps or DRED.
- Engine integration, the classic renderer, browser/WebGPU, performance or visual quality.
- The live GitHub issue body; the mirror is authoritative because the authenticated account
  cannot update the issue.
- Any human sign-off. This document records an agent review and must not be presented as a
  human approval.
