# Q2/Q3 design review log (S4: T032, T033, T041 design)

Status: **author-recorded review log**, 2026-10-28. The reviews below were performed by
fresh-context AI subagents, not by a human and not by a different team. They are useful adversarial
checks; they are **not** independent sign-off. No reviewer approved T032, T033 or T041.

The reviewed artifacts are `classic-parity.md`, `engine-integration.md`, `motion-fixtures.md` and
`tools/motion_fixtures.py`. The execution record is `evidence-q2-design.md`.

---

## 1. Round 1 — two reviewers, both BLOCKED

Two reviewers ran in parallel: one on parity/integration, one on the motion math. Both returned
**BLOCKED**.

| Finding | Severity | Disposition |
|---|---|---|
| Q0 was described as "review 2 pending" and the exploratory `final4` values as `PASS`; the current Q0 was `final9` with all four categories `SKIPPED` | BLOCKER | Rewrote §2 against the current `evidence-q0.md`: all four `SKIPPED`, only `control-interior` `PASS`; Q0 not established; review history corrected; no category threshold is called measured |
| The delivery procedure told the reader to branch inside `doom3_orig/doom3` | BLOCKER | Rewrote §3.2: clone/checkout the fork elsewhere, push there, then `git add doom3_orig/doom3` for the bump; no commit in the pristine submodule |
| The CMake snippet declared `doom3_engine` twice, appended `DOOM3_LINK_LIBS` before the `set()`, omitted `d3d12`/`dxgi`, and never wired the adapter | BLOCKER | Split the snippet by insertion point; use `target_sources` after `doom3_engine` exists; define `RHI_ADAPTER_SOURCES` with an `EXISTS` guard; append after the existing `set()`; add `d3d12 dxgi` |
| "Four `glConfig.isInitialized` gates" was false; there are 30 references | BLOCKER | Counted 30 (26 reads, 1 comment, 3 writes), grouped all 30 by disposition, and added the GL coupling table (`RB_SetDefaultGLState`, `RB_STD_DrawView`, `qglReadPixels`, `R_VidRestart_f`, `CopyFramebuffer`, GUI, stencil, depth range) |
| The "neutral" stream ignored `GLenum buffer` and `idImage*` in the command structs | MAJOR | Added the boundary mapping table; no GL enum or `idImage*` goes to the RHI; `tri->ambientCache` is not a portable buffer |
| The fallback was untestable ("remains able to run GL" without a state/command) | MAJOR | Defined the exact test: failed D3D12 launch reports a typed error, does not change `r_renderApi`/config; a subsequent `r_renderApi 0` launch renders GL; no automatic switch |
| FR-007 was missing remote/security views and non-interaction ambient stage order; result record could not express `UNSUPPORTED` or split S/I | BLOCKER | Added CP-29/CP-30; added `UNSUPPORTED` and separate Structural/Image columns; `S` is explicitly T-07; evidence column marks tests as planned |
| Motion used a generic 90° fov, not the engine 16:9 pair; RG16F hex was a byte string; SKN-03 weights were unstated; M1 was tautological; FR-010 policy rows were missing; `research.md` §5.1/§5.2 did not exist | MAJOR/MINOR | Switched to `fov_x=106.260205°`, `fov_y=73.739795°`, `tan_x=4/3`, `tan_y=3/4`; emit the 16-bit word; state SKN-03 weight 1; make M1 a real state swap; add `POL-01..06`; cite `research.md` D4 and `data-model.md` |
| `evidence-q2-design.md` was referenced but did not exist | MAJOR | Created this evidence file's companion (the execution record) and recorded the actual checks; no engine build is claimed |

The reviewers independently recomputed the motion vectors and found no arithmetic error in the
then-current 17 rows; the defects were representation, conventions, coverage and missing evidence,
not the `du`/`dv` values themselves.

---

## 2. Round 2 — re-review, BLOCKED

A second fresh-context reviewer checked whether the Round 1 blockers were actually fixed. Verdict:
**BLOCKED**, with these residual items:

| Finding | Severity | Disposition |
|---|---|---|
| Q0 values were stale again (`final8`/0.4543/0.3363 vs current `final9`/0.6123/0.6456) | BLOCKER | Updated to `baseline-final9` and added a freshness rule: Q0 is a moving S1 artifact; the category status is stable, but the run label/control pair must be taken from the current `evidence-q0.md` |
| CMake snippet re-declared `doom3_engine` and appended before the `set()` | BLOCKER | Rewrote with two explicit insertion points and `target_sources`; the `EXISTS` guard fails configure if T035's **superproject** adapter is missing (not a fork bump) |
| `glConfig` count said 28 but the table listed 30 | MAJOR | Corrected to 30 references (26 reads, 1 comment, 3 writes); the frame group now includes `RenderWorld.cpp` |
| `research.md` D5/D6 do not support history/UV/storage | MAJOR | Replaced with D4 + `data-model.md` |
| `O-01` cited a commented-out Material hash block | MINOR | Re-anchored to `tr_light.cpp:1195-1200` (`tr.sortOffset`) |
| `corridor` status mixed `SKIPPED` and `BLOCKED` | MINOR | Standardized to `SKIPPED — BLOCKED` and defined the distinction (Q0 execution vs gate) |
| `evidence-q2.md` was listed as evidence without "planned" | MINOR | Marked planned/not created |
| `RHI_ADAPTER_SOURCES` was undefined in the snippet | MINOR | Defined it with an `EXISTS` guard |

---

## 3. Round 3 — final blocker check, BLOCKED

A third fresh-context pass verified the Round 2 fixes and found four remaining items:

| Finding | Severity | Disposition |
|---|---|---|
| `review-q2-design.md` did not exist, though the evidence referenced it | BLOCKER | This file is that record |
| Playwright HTML/screenshots predated the final documents (11 vs 12 tables) | MAJOR | Re-rendered and re-captured after the final edits; the evidence table now reports the final counts |
| `corridor` still mixed wording (`SKIPPED` vs `SKIPPED — BLOCKED`) | MAJOR | Standardized in §2, §3 and T-04; the field meaning is Q0 execution `SKIPPED`, gate `BLOCKED` |
| Q0 label `final9` vs `baseline-final9` | MINOR | Added the exact label with `final9` as shorthand |

The reviewer independently verified the CMake ordering, the 30-reference count, the 16:9 pair,
the RG16F word, the new fixtures, the self-test, and the O-01 anchor.

At that point, no fourth independent pass had been run after the Round 3 dispositions. This file does **not** claim
that the Round 3 findings are externally verified; it records what was changed.

### 3.1 Post-review change: Q1a merged

Q1a merged on `main` (PR #18), so `tests/renderer/` now exists. The T041 fixture document was copied
byte-identically to `tests/renderer/motion-fixtures.md`; the verifier checks the mirror and the
generator table on both copies. Q1a's standalone CTest project was independently built and run here:
7/7 passed in Debug and Release; debug-layer/GPU-validation checks are `SKIPPED`
(`hr=0x887A002D`) and are not counted as passes.

A target-structure build probe also copied the real target CMake, pointed `NEO_DIR` at the existing
target copy, and added a stub adapter. Both `DOOM3.exe` and `DOOM3XP.exe` built in Debug and Release
with `/MT`/`/MTd`, `doom3_rhi.lib` on the link line, and `d3d12.dll`/`dxgi.dll` imports. The first
attempt failed C2589 because `NOMINMAX` was missing; the snippet now sets it. This proves build
wiring, not a real adapter, engine call site, game run, or frame.

These changes do not alter any vector, threshold, or Q2 claim, and they do not constitute an
independent review.

---

## 3.2 Round 4 — post-Q1a verification, BLOCKED

A fourth fresh-context pass checked the Q1a-merge changes. Verdict **BLOCKED**; findings and
dispositions:

| Finding | Disposition |
|---|---|
| A bare anchor at the wrong path made the verifier exit 1 | Changed to `src/renderer/d3d12/rhi_d3d12.cpp:34`; verifier green |
| Artifact hashes and the verifier transcript were stale after edits | Recomputed all hashes and pasted the current run |
| Adapter ownership contradicted itself: `src/renderer/rhi/engine_scene_adapter.cpp` vs `neo/renderer/d3d12/...` | Standardized on the T035 superproject adapter compiled into `doom3_engine`; the fork only owns dispatch/glue |
| The visual pack predated the final documents | Re-rendered and re-captured after the final doc edits |
| C2589 was attributed to `std::max`/`std::min` | Corrected to the `max` macro breaking `std::numeric_limits<uint64_t>::max()` |
| Q1a merged but T012–T021 unchecked | Disclosed as tracker drift; later resolved by PR #30 |

## 3.3 Round 5 — final blocker check, BLOCKED

A fifth pass found two remaining fork/superproject wording contradictions and three minor staleness
items. All were fixed: "a new adapter" and "future fork file" now refer to engine-side files only;
the historical review row says "superproject adapter"; a historical S0 "absent" row is annotated as
superseded; the two top crops were regenerated; the `NOMINMAX` explanation was checked. At that
point, no sixth independent pass had been run. The verification is recorded here, not turned into an
acceptance claim.

## 3.4 Round 6 — final13 parity update, BLOCKED

A sixth fresh-context pass checked the final11 update. `baseline-final13` later superseded it; the
ceilings did not change. The pass confirmed the four recorded values and
ceilings, all-four PASS, T004/T005 checked, the scene/matrix/checklist updates, and the T041 mirror.
It returned **BLOCKED** on four artifact/state issues: the `classic-parity.md` hash was stale; the
verifier transcript still said `review-q2-design.md: 4 tables, 2 anchors`; the evidence claimed a
commit before one existed; and a Round 5 note read as current when it was historical. The hash,
transcript, commit/statements and historical wording were corrected. A seventh pass was then run on
the committed state.

## 3.5 Round 7 — committed-state check, BLOCKED

A seventh pass reviewed commit `a2915ef`. It found that the S1 Q0 evidence had advanced to
`baseline-final13` two seconds before the S4 commit, so the committed `final11` values were
superseded (ceilings and all-four PASS unchanged), and that the Q0 files were committed on the S1
branch rather than untracked. The S4 docs were corrected to `final13`, its four recorded values, and
"committed on the S1 branch, not merged to `main`"; a second commit records the correction. Q0 was
subsequently merged by PR #31 and T006–T011 were checked. An eighth pass was run on that state.

## 3.6 Round 8 — final13 committed, PASS (minor findings)

The eighth fresh-context pass reviewed the then-current final13 commit and returned
**PASS** on substance, with minor findings: trailing blank lines at EOF (so the whitespace claim did
not reproduce), the first commit message still named final11 (true at that commit), the Round 8
verdict was not yet recorded, the S2 `megaTexture` row cited an untracked path, and the T009–T011
tracker drift was not disclosed. The EOFs, S2 citation, Q0 drift disclosure and this section were
fixed; the first commit message is retained as the historical first commit rather than rewritten.
No ninth pass was run. After the PASS, T032/T033/T041 were ticked as design deliverables; this does not tick any Q2/Q3 execution task.

---

## 4. Residual findings / not approved

| Item | Status |
|---|---|
| Q0 `baseline-final13` establishes all four categories with measured ceilings | **Resolved**; T032 category thresholds are fixed. HUD/ROI controls (T-05/06/08/09) remain pending |
| Engine build wiring is probed with a stub adapter (both EXEs, `/MT`); real adapter/engine run/selection and T035/T036/T083 are **not done**. Q1a is merged/tested but Q1b/Q1/T088 are not run | Build wiring is not engine integration; out of S4 scope |
| T037/T038 fixtures (`fixture-*`, `hud-control`) | **Not authored**; rows are `NOT RUN` |
| T041 fixture document at `tests/renderer/motion-fixtures.md` | **Delivered** byte-identically; C++ consumer tests (T042/T044/T046) and T048 remain pending |
| Q1a tracker drift: merged PR #18, but T012–T021 remain unchecked | **Resolved** by PR #30 (T012–T021 checked) |
| T004/T005 sign-off | **Resolved** by PR #27; both tasks checked on `main` |
| Independent human review | **None** |
| T032/T033/T041 design deliverables | **Complete and ticked** after Round 8 PASS; Q2/Q3 implementation/execution and human sign-off are not claimed |

## 5. What the reviews did verify

- No arithmetic error in the final 20 `du`/`dv` rows or their ULP tolerances; independent
  recomputation of CAM-01, CAM-03, RIG-03, SKN-01/03, VM-01, SEQ-01/02/03 matched the generator.
- The engine 16:9 `CalcFov` pair and the half-word representation are correct.
- The `glConfig.isInitialized` count and the CMake insertion points match the source.
- O-01's `tr.sortOffset` anchor matches `tr_light.cpp:1195-1200`.
- At the time of Round 3, the documents correctly described the then-current Q0 as not established, with all four categories skipped; `baseline-final13` later superseded that state.
- The documents do not claim an engine build, a Q2 capture, a Q3 execution, or acceptance.

**Verdict**: execution/completion not approved. The T032/T033/T041 design deliverables are
reviewed and ticked; Q2/Q3 execution remains unapproved.
