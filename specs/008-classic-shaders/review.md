# Self-review: classic shader package

**Date**: 2026-10-28
**Scope**: every file under `specs/008-classic-shaders/`.
**Question**: is this package true, reviewable, and safe to hand to an implementer?

This is a **self-review**. It is not independent approval. The repository's review rule still
applies: an implementer must not self-approve the first engine PR.

---

## 1. What was checked

| Check | Method | Result |
|---|---|---|
| Program table and counts | script extraction from `draw_arb2.cpp` | 18 entries, 13 files, verified |
| Classification | bind-site scan across `neo/renderer/*.cpp` | 8 live-default, 2 debug, 5 non-ARB2, 3 dead |
| Draw_exp compilation | `targets/win11-amd64/sources.cmake` | not compiled |
| Failure chain | independent trace from load to draw | 11 evidence points, anchored |
| Resolver semantics | 14 executable vectors | all pass |
| Anchors and links | `verify-docs.ps1` | all exist within the cited range; relative links resolve |
| Legal surface | tracked-file scan | no shader source, no ARB bodies |
| Authoring-set claim | cross-checked against two independent source audits | five live-default files, not six |

---

## 2. Findings fixed during this review

| Id | Finding | Fix |
|---|---|---|
| F-1 | The existing renderer analysis (`doom3_orig/doc/02_renderer.md` §7.3) implies "~12 programs must be rewritten" and lists `ambientLight.vfp` as used by the ambient pass | The census distinguishes 18 entries / 13 files / 5 live-default files and classifies `ambientLight.vfp` as dead. The older figure is a lower-bound estimate, not a census. |
| F-2 | `ambientLight.vfp` looked live because `draw_exp.cpp` binds it | `draw_exp.cpp` is not in `sources.cmake`; classified `dead`, with the non-compiled bind recorded |
| F-3 | `arbVP_glasswarp.txt` is in the table but never bound | classified `dead`; only `arbFP_glasswarp.txt` is live |
| F-4 | Several ABI anchors were off by 1–5 lines (vertex pointers, texture units, register writes) | re-derived and range-checked; the checker proves existence/range, not that the line says what the prose claims, so the census and ABI were also spot-checked by reading the cited lines |
| F-5 | An early draft mixed entry counts and file counts | every table now states which unit is counted; `census.json` counts both |
| F-6 | An early draft used `RB_SetupForStage`, which does not exist | the real functions are `RB_PrepareStageTexturing`, `RB_STD_T_RenderShaderPasses`; the package now cites the real names or the exact line |
| F-7 | The resolver draft collapsed `stub`, `unsupported` and `absent` into `UNSUPPORTED` | split into `STUB`, `UNSUPPORTED`, `ABSENT`; vectors R-007, R-011, R-014 pin the difference |
| F-8 | The provenance gate flagged `doom3_orig/doc/02_renderer.md`, which quotes the directive name in a table | allowlisted that exact file with an inline reason; the gate still rejects any new file |
| F-9 | The vector fixture's synthetic keys (`one_sided`, `stubprogram`, `dupkey`) could be mistaken for shipping entries | `provenance.md` §4 and `resolver.md` §4 state they are test-only |

---

## 2.1 Independent review (same day)

A separate agent reviewed the package against the source and ran the four gates that existed then. It found defects
the self-review had missed; all blocking findings are fixed:

| Id | Finding | Fix |
|---|---|---|
| B-1 | The ABI vertex table gave the normal slot as 11 for every path; the cube/environment path uses `glNormalPointer`, i.e. generic attribute 2 | table now has separate interaction/generic columns and warns that the numbers are not interchangeable |
| B-2 | The vector fixture's provenance ids contradicted the register (`megatexture` mapped to the ambient record, `ambientlight` to a nonexistent PV-012) | fixture now uses PV-001…PV-008 for real keys and `PV-Txx` for synthetic ones |
| B-3 | The prose said the vectors pin nine codes, but `MANIFEST_INVALID` had no vector and no branch in `apply()` | the harness now validates the manifest first; prose states nine of eleven are covered and the two runtime codes are not |
| B-4 | The census rule as written would classify `test.vfp` as live-default, contradicting the table | precedence is now explicit: debug-only first, then live-default, non-ARB2, dead |
| B-5 | The provenance gate scanned `.md/.cpp/...` but not `.hlsl/.glsl/...`, so a renamed ARB body could pass | shader-like extensions added; negative control uses `.md` and `.vfp`, and the extension list is stated |
| B-6 | The ABI's dual-write list omitted `c[4]` (interaction vs shadow `w=0`) and `c[5]` (space vs interaction) | added to the register table and the collision section |
| B-7 | `verify-docs` only checked `neo/` anchors and only file bounds; three cited lines were off by one | checker now covers engine and repo paths; the three anchors are fixed; the limits are stated in SC-004 |

Non-blocking findings fixed: stale evidence counts, the `notes`/`note` mismatch, filenames that did
not match their keys (`bumpyenvironment.hlsl`, `arbfp_glasswarp.hlsl`), the missing `arity` field,
the undefined `planned` status, inconsistent `shader_status` arithmetic, the wrong runtime-census
cvar and keyword, `base` containing `default.cfg`, the census preflight, the Tier B bound (42, not
48), strict-at-load made a decision, task dependencies, and the fixture's missing `source`/`entry`.

## 2.2 Second independent review (round 2)

A second agent audited the round-2 checks (reusable resolver model, census-plan integration,
property test, content anchors, report) and ran mutation tests against copies. It found seven
blocking issues; all are fixed:

| Id | Finding | Fix |
|---|---|---|
| R-1 | Provenance was membership-checked only: `interaction` pointing at `PV-002` (the environment record) passed | the integration check parses the register into key→id and requires the id that belongs to the key; the mutation now fails |
| R-2 | Fixed coverage counts were not asserted: the fuzz could run with `-Iterations 1`, a vector could be deleted, an anchor could be removed, all green | fuzz enforces a 10,000 distinct-request floor, the harness pins 14 unique cases, `anchors.json` declares its count and uncited anchors fail |
| R-3 | `evidence/verification.md` carried stale numbers (HEAD, anchors, tracked files, task counts) | refreshed to the current run |
| R-4 | `tasks.md` said 14 tasks self-reviewed while 19 spec rows existed | corrected to 19 |
| R-5 | Tier B bound was 42 in `ownership.md` but 48 elsewhere | corrected to 42 in `plan.md`, `tasks.md`, `review.md` |
| R-6 | T-019 claimed Playwright assertions with no recorded command/output | the assertions and the report screenshot are recorded in `evidence/verification.md` |
| R-7 | The whole 19-task scope was labelled "independently reviewed", but the first review covered only the four original gates | the two review rounds are now distinguished in `tasks.md` and here |

Non-blocking fixes: `report.ps1` now verifies each gate's success marker instead of carrying dead
`expect` strings, the content-assertion KPI is parsed from the gate output, the `megaTexture` prose
anchor points at `Material.cpp:1517`, and the fuzz docstring no longer claims an unasserted
path-prefix invariant.

**A real regression was caught during this round**: an edit intended to add the provenance map
deleted the stage/coverage block in `integration-check.ps1`, and the check still passed. The
mutation tests (shadow stages `vs`→`vs,ps`, and deleting `interaction`) exposed it. The block was
restored, and both mutations now fail with the expected diagnostics. This is the strongest argument
for keeping the mutation controls in the evidence file rather than trusting a green run.

---
---

## 3. Adversarial questions

### Q1: Is the five-file authoring set actually complete?

**No, and the package says so.** It is complete for the *static table's live binds*. Material data
can request arbitrary names (`neo/renderer/Material.cpp:1490`) and `megaTexture.vfp` is forced by a
keyword (`neo/renderer/Material.cpp:1517`). The runtime census is the only way to bound the rest.
`census.md` §8 explicitly marks completeness as unproven.

### Q2: Does the resolver break retail material compatibility?

It preserves extension-insensitive, case-insensitive, path-sensitive lookup. It deliberately changes
one behaviour: duplicate keys become `AMBIGUOUS` instead of first-wins. If retail data relies on the
old first-wins behaviour, the runtime census will show it. The change is intentional and documented
(`resolver.md` §9).

### Q3: Is strict mode safe to ship?

`strict` rejects a material at load. That is the right default for evidence builds and the wrong
default for a mod-friendly release. The package sets `strict` for Q1/Q2 evidence and leaves the
release default as a later, evidence-based decision (`resolver.md` §7). Tolerant mode never draws a
broken stage; it substitutes a visible error material.

### Q4: Is the out-of-bounds table walk real?

Yes: the loop tests `progs[i].name[0]` before the `i == MAX_GLPROGS` check
(`neo/renderer/draw_arb2.cpp:473`, `neo/renderer/draw_arb2.cpp:486`). It requires all 200 slots to be
named, which needs a very large mod, so it is latent rather than everyday. It is listed as an engine
defect for the resolver work, not as a reason the census is wrong.

### Q5: Is the ABI document describing the retail programs or the port?

The port. It records what the engine **provides** (registers, bindings, attributes) and requires the
authored library to declare what it **consumes**. Statements about which built-in GL state the
retail programs used are marked inference, because the program text is not in the repository.

### Q6: Could the screenshot be mistaken for proof that shaders work?

`evidence/census.png` is a rendering of the verified census table. It proves the artifact renders
and the counts match the extraction. It proves nothing about shader behaviour; no shader exists.

### Q7: Is anything claimed that was not run?

- The runtime census: explicitly `SKIPPED`.
- The engine resolver: not implemented; vectors test a reference model.
- Both-backend shader compilation: not run; no shader and no pinned toolchain.
- Legal review: not obtained.

These are the four claims a careless summary would overstate. The documents mark each one.

---

## 4. Known weaknesses

| Weakness | Impact | Plan |
|---|---|---|
| Runtime census blocked | completeness unproven | T-031 when authorized data exists |
| No legal review | the clean-room position is conservative but not adjudicated | T-032 |
| Toolchain not pinned | shader builds are not yet reproducible | T-023 before the first program |
| Owner is a role, not a person | the first implementation PR needs an independent reviewer | `ownership.md` §10 Q4 |
| ABI is source-derived, not compiled | a wrong packing rule surfaces only at first compile | T-020/T-026 |
| Tier B variant bound is arithmetic, not observed | 42 is an upper bound; the shipped subset is unknown | T-028 + runtime census |
| The old renderer doc still says "~12 programs" | a reader may trust the older number | this package supersedes it for the census; do not edit the 003/004 docs from here |

---

## 5. Reviewer checklist

An independent reviewer should be able to answer **yes** to each:

1. Does `census.ps1 -Verify` pass on a clean checkout with the submodule initialized?
2. Do the counts in `census.md` match `census.json` and the source?
3. Is every one of the 18 entries classified, with a bind site or an explicit "none"?
4. Is the five-file authoring set justified, and is the "not proven complete" caveat visible?
5. Does the resolver contract forbid every draw with a non-OK status in both modes?
6. Do the vectors cover unknown, stage-missing, unsupported, stub, absent, ambiguous, too-long and
   tolerant cases?
7. Is the key-normalization rule consistent with `StripFileExtension` and `Icmp`?
8. Is the ABI's dual-write collision explained, and does it forbid a single global UBO?
9. Is the provenance register honest that nothing is authored and no hash exists?
10. Does the package avoid editing `003`/`004`/`005`?
11. Are the blocked gates reported as blocked, not as done?
12. Is the screenshot labeled as a rendering of the census, not behavioural evidence?

---

## 6. Verdict

**Independently reviewed once; all seven blocking findings fixed.** The facts are source-anchored
and machine-checked; the resolver semantics are executable; the legal boundary is conservative and
explicit. It is ready for a human review pass, not for implementation without the T-021/T-022
issue.

**Not ready for implementation without the T-021/T-022 issue**, and **not a claim** that classic
shaders are ported, complete, or legally cleared.

The two gaps that matter most are the runtime census and the legal review. Neither can be closed by
more writing; both need external inputs.
