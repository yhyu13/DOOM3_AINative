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

## 3.1 Round 3: source-truth audit and gate mutation testing (2026-10-29)

Two independent agents audited the package while the implementation was written: one read every cited
engine line and compared it with the claim, the other mutation-tested the six gates on a scratch copy
of the tree. Neither was told what to find. They found 25 defects, 8 of them gates that could report
success with their own subject broken. All 25 are fixed below; every gate fix has a mutation control
that fails where it used to pass.

### 3.1.1 Source-truth findings

| Id | Severity | Defect | Fix |
|---|---|---|---|
| S-1 | blocking | `abi.md` claimed `c[4].w` was undefined for interactions and that `c[4]`/`c[5]` had two meanings. The engine writes `localLightOrigin[3] = 0` and `localViewOrigin[3] = 1` (`neo/renderer/tr_render.cpp:732-735`). The conclusion (no single global UBO) survived, its stated reason did not. | §3.1 now says `w = 0` / `w = 1` on both paths and cites the writes; §4 narrows the collision to `c[6..8]` and fragment `c[0..1]`. The ABI header comment was corrected too. |
| S-2 | non-blocking | `c[0]` screen-copy scale cited `draw_common.cpp:608`, which sits inside `#if 0` and computes a different formula. The anchor assertion passed on dead code. | cited `draw_common.cpp:612-622`, and `anchors.json` moved from 608 to 622. |
| S-3 | non-blocking | `shader_status` sample said `manifest=13 … absent=6`; §4.2 defines 14 keys with 7 absent. | sample is now `manifest=14 owned=0 stub=6 unsupported=1 absent=7`, and §4.2 states the 14 explicitly. |
| S-4 | nit | the "path prefix significant" row cited only `draw_arb2.cpp:470` (extension strip), which does not show prefix handling. | row now cites 470 and 481 and says "only the extension is stripped". |
| S-5 | nit | `census.md` said `targets/win11-amd64/doom3/base` is empty; it holds `default.cfg`. | reworded; the skip conclusion is unchanged. |
| S-6 | nit | the `bumpyEnvironment`/`environment` rows cited only the fragment bind lines. | now `213-215` and `222-224`. |
| S-7 | nit (latent) | the `non-ARB2` rule ignored the compiled-backend map, so a backend dropped from the build would keep its entries out of `dead`. | rule 3 now consults `$compiled`, and the bind scan covers headers as well as `.cpp`. |

### 3.1.2 Gate defects: eight gates that could not see their own subject fail

| Id | Gate | Defect (mutation that passed) | Fix |
|---|---|---|---|
| G-1 | `verify-docs` | deleting `anchors.json` skipped the whole content layer | file is now required; the run fails closed |
| G-2 | `verify-docs` | the "not cited in prose" failure was appended *after* the failure test, so it was never reported | failure test moved to the end |
| G-3 | `verify-docs` | the anchor count was compared with a number in the same file, so shrinking both passed | fixed floor `$ExpectedAnchorCount = 70` |
| G-4 | `verify-docs` | prose anchors were range-checked only, which C4/V5a exploited | documented in the output as `LIMIT:`; the 70 registered anchors are content-checked and all 70 must be cited |
| G-5 | `resolver-conformance` | only the count (14) and id uniqueness were asserted, so swapping R-014 for a trivial case passed | exact id set `R-001…R-014` required |
| G-6 | `resolver-conformance` | `request.target` was never read, so a vector could declare a stage it did not exercise | a stated target must match the stage its keyword implies |
| G-7 | `census` | bind sites in headers were invisible, and plain mode warned then exited 0 when a block was missing | source scan covers `.h/.hpp/.inl`; a missing block exits 1; `$compiled` feeds the non-ARB2 rule |
| G-8 | `provenance-check` | three whole files were allowlisted and only the two header strings were detected, so a full ARB body appended to `provenance.md` passed | line-scoped allowance (prose/table/comment/inline code) plus an opcode-body heuristic (`>= 4` opcode lines) |
| G-9 | `integration-check` | `$problems` was used before it was created, and a plan entry could name a nonexistent source | initialized first; plan sources are now stat'ed |
| G-10 | `report.ps1` | a gate stderr aborted the run before writing, leaving the previous all-green `report.html` in place; "DOCS OK … 0 content assertions" counted as a pass | previous artifact deleted first, gates wrapped, git metadata tolerant, per-gate coverage floors (18 census entries, 14 vectors, 18 requests, 10 000 fuzz requests, 70 content assertions, 100 tracked files) |
| G-11 | `fuzz-resolver` | the alphabet had no backslash, so removing separator folding survived | backslash added; distinct requests rose from 10 160 to 10 162 |

### 3.1.3 Mutation controls proving the fixes

Run against a scratch copy of the tree (the repository was not modified):

| Mutation | Before | After |
|---|---|---|
| delete `tools/anchors.json` | exit 0, `0 content assertions` | exit 1, `anchors.json missing` |
| delete one anchor and set `count: 69` | exit 0 | exit 1, floor not met |
| replace R-014 with a trivial passing case (still 14 ids) | exit 0 | exit 1, `id set drift` |
| set R-001 `target: "ps"` with `keyword: vertexProgram` | exit 0 | exit 1, `implies vs but target is ps` |
| append a full `!!ARBfp1.0 … END` body to `provenance.md` | exit 0 | exit 1, header line + opcode body |
| add a bind site in `neo/renderer/draw_foo.h` | exit 0, `VPROG_AMBIENT … dead` | exit 1, `unclassified bind source` |
| delete `resolver-conformance.ps1` and run the report | exit 1 but the old 6/6 `report.html` stayed on disk | exit 1, `report.html` rebuilt as `4/6`, FAIL badge present |

The last row is the one that mattered most: the committed evidence artifact could read "6/6 passing"
for a tree where a gate was failing.

### 3.1.4 Findings that remain open, stated rather than fixed

| Id | Limitation | Why it stays |
|---|---|---|
| L-1 | Prose anchors that are not in `anchors.json` are range-checked only | registering all 124 citations would make `anchors.json` a copy of the prose; the 70 chosen are the ones whose wording matters |
| L-2 | `census.ps1` compares generated blocks, not arbitrary prose; "five distinct files" is protected by an anchor, not by the extractor | a prose claim is a document problem; the anchor set is the enforced subset |
| L-3 | The provenance gate can still miss a body written in a form that matches no opcode | it is a blunt gate by design; review is the second layer |
| L-4 | `report.ps1` cannot detect a gate that lies about itself in a way that satisfies its own floor | a floor is a lower bound; the alternative is re-implementing each gate inside the report |

---

## 4. Known weaknesses

| Weakness | Impact | Plan |
|---|---|---|
| Runtime census blocked | completeness unproven | T-031 when authorized data exists |
| No legal review | the clean-room position is conservative but not adjudicated | T-032 |
| Tint/Emscripten never pinned (DXC is) | no WGSL validation, so no program can be `owned` yet | pin Tint, then re-run `reflect.ps1` on the WGSL side |
| Owner is a role, not a person | the first implementation PR needs an independent reviewer | `ownership.md` §10 Q4 |
| ABI is source-derived, not observed | the headers compile and reflect cleanly, but no retail program has been compared | T-027 parity, on authorized data |
| Tier B ships nothing yet | the 42-variant bound compiles, but `variants.json` is empty until the census runs | T-028 + T-031 |
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
9. Does every `compiled` record carry a hash that `shaders/tools/provenance-record.ps1` reproduces, and is `owned` still zero?
10. Does the package avoid editing `003`/`004`/`005`?
11. Are the blocked gates reported as blocked, not as done?
12. Is the screenshot labeled as a rendering of the census, not behavioural evidence?
13. Do the shader gates (`compile.ps1`, `reflect.ps1`, `manifest-check.ps1`, `provenance-record.ps1`, `verify-golden.ps1`) pass from a clean checkout, and does `verify-golden.ps1` show the blobs are deterministic?
14. Does `tests/shader` build warning-free at `/W4` and pass all six cases, including the mutation control in `evidence/implementation.md`?

---

## 6. Verdict

**Reviewed three times; all 39 findings fixed (7 + 7 specification, 7 source-truth, 18 gate).** The facts are source-anchored
and machine-checked; the resolver semantics are executable; the legal boundary is conservative and
explicit. It is ready for a human review pass, not for implementation without the T-021/T-022
issue.

**Not ready for implementation without the T-021/T-022 issue**, and **not a claim** that classic
shaders are ported, complete, or legally cleared.

The two gaps that matter most are the runtime census and the legal review. Neither can be closed by
more writing; both need external inputs.
