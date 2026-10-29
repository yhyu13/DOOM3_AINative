# Review: RHI-era HD content specification

**Date**: 2026-10-29
**Scope**: every file under `specs/009-rhi-hd-content/`.
**Question**: is this package true, internally consistent, and safe to hand to an implementer?

This file records a self-review with mechanical checks, then an independent review. It is not an
approval by a human, and the repository's rule still applies: the first pipeline PR needs a reviewer
who did not write it.

---

## 1. Mechanical checks

| Check | Command / method | Result |
|---|---|---|
| Decision count | count `^\| C-` rows in `boundary.md` | 15 (C-01a, C-01b, C-02…C-14) |
| Requirement count | count `**FR-nn**` ids in `spec.md` | 16 (FR-001…FR-016, no gaps) |
| Success criteria | count `\| SC-nn \|` rows | 10 (SC-001…SC-010) |
| User stories | count `### USn` headings | 4 (P1…P3, US4 at P3) |
| Matrix cells | count rows in the US2 table | 7, matching C-12 (a)–(g) |
| Task count | unique `T-nnn` ids in `tasks.md` | 29; summary says 29 (27 not started + 2 blocked) |
| Table shape | every table row has the same pipe count as its header | 0 mismatches in all four files |
| Relative links | resolve every `[](…)` target on disk | 0 broken |
| Decision references | every `C-nn` cited in spec/plan/tasks exists in `boundary.md` | 15/15 defined |
| Requirement coverage | map each FR to at least one task | 16/16 covered (table in §3) |
| Citations | each `file:line` points at a line saying what the prose claims | checked by hand on the newer base `29a1e37`; see §2.2 |

Counts that changed during this round were corrected where they appeared: the package says "fifteen
decisions" and "29 tasks" everywhere, and the US2 row now counts 9 tasks (it previously said 8).

---

## 2. Findings and dispositions

### 2.1 Self-review findings (fixed before the first push)

| Id | Severity | Finding | Fix |
|---|---|---|---|
| S-1 | blocking | C-14 said "WebGPU has no toolchain (`emcc` absent)". The base commit `29a1e37` (PR #37) added a browser WebGPU conformance spike that passes 12/12 on Chrome 156 / Intel Xe-LPG, so the claim was overstated in one direction even though the conclusion (no engine backend) held. | C-14 and spec §8 assumption 1 now say: browser spike exists, no engine toolchain (`emcc`/`emsdk`/Dawn absent), Q7a's Emscripten leg NOT RUN, Q2 still blocked. |
| S-2 | blocking | FR-005 made the builder fail on "a `.mtr` whose material name has no sidecar when that material is in the Q4 set", but "the Q4 set" was never defined — an implementer could read it as "every `.mtr`". | the Q4 set is now the explicit list in `content/manifest/q4-materials.json`; glossary entry added; T-005 and T-012 carry it. |
| S-3 | non-blocking | SC-001 named three negative controls while T-006 requires at least five, so the two could disagree about a pass. | SC-001 now names the same five controls T-006 requires. |
| S-4 | non-blocking | C-08 forbids numeric budgets while C-01b caps committed assets at 2K per axis; a reviewer could read the cap as a forbidden budget. | C-08 now states that the cap is an authoring constraint on committed sources, not a VRAM/timing budget, and that nothing enforces a runtime number. |
| S-5 | non-blocking | C-09 says spec 006 is not evidence, while T-026 converts a normal map from the 006 corpus. Read together they looked contradictory. | T-026 now says explicitly: a conversion test, not a Q4 capture; the corpus is stress data and the output is not RHI evidence. |
| S-6 | nit | The boundary header said the plan forbids edits to `003`/`004` but did not cite where 004 requires D3D12, so the "conflict" paragraph read as an opinion. | §1.1 now cites `specs/004-quality-renderer/spec.md:144` (FR-003), `plan.md:19` and `research.md:7`, and calls 003 D4 *Proposed* against a *required* FR. |

### 2.2 Citation re-check against the newer base

The package was written against `d9bf1ca`; the PR branch starts from `29a1e37`, which changed
`specs/004-quality-renderer/plan.md`. Re-checked on the new base:

| Citation | New base | Result |
|---|---|---|
| `specs/004-quality-renderer/plan.md:87` | still the Q4 row | valid |
| `specs/003-render-modernization/spec.md:62,63,65` | D4, D5, D7 unchanged | valid |
| `specs/003-render-modernization/spec.md:115-132` | US3 unchanged | valid |
| `specs/004-quality-renderer/spec.md:144` | FR-003 "D3D12-first Windows backend" | valid |
| `doom3_orig/doom3/README.txt:15` | the "no game data" sentence | valid |
| `docs/ROADMAP.md:17,42` | asset rows unchanged | valid |

### 2.3 Independent review

Pending at the time this file was written; findings and dispositions will be appended in the same PR
before merge. The reviewer was asked to verify anchors, numbers, the backend claim, internal
consistency, testability, five named contradictions, and scope discipline.

---

## 3. Requirement-to-task coverage

| Requirement | Tasks |
|---|---|
| FR-001 asset bytes and the `original` allowance | T-003, T-004 |
| FR-002 staged corpus manifest | T-003 |
| FR-003 provenance records | T-004, T-011 |
| FR-004 hero claim must be original CC0 | T-004 |
| FR-005 `.mtr` + sidecar, Q4 set | T-005, T-012 |
| FR-006 channel states | T-005 |
| FR-007 formats, mips, sRGB | T-013, T-014 |
| FR-008 RXGB conversion | T-026 |
| FR-009 package manifest | T-013 |
| FR-010 seven cells | T-020…T-025 |
| FR-011 per-cell evidence | T-031 |
| FR-012 budget rows | T-027 |
| FR-013 backend + adapter in evidence | T-031 |
| FR-014 clean-checkout run | T-030 |
| FR-015 no spec-006 reuse | T-031, and T-026's explicit note |
| FR-016 no machine paths | T-003 |

---

## 4. Known weaknesses

| Weakness | Impact | Plan |
|---|---|---|
| No capture can exist until spec 004 reaches Q4 | authored content has no visual proof for a long time | keep capture `BLOCKED`; art review works on sources |
| Hero authoring is not done | the matrix is specified, not populated | US1 first, then T-020…T-025 |
| 2K source cap may be too small for one cell | the large environment cell needs assembled sources | C-01b allows assembly; if it proves unworkable, amend the boundary rather than commit a 4K source |
| Budget rows depend on 004 task numbering | T-027 cannot be filled in yet | name the measurement task once 004's Q4 tasks exist |
| Pillow version is observed, not pinned | a silent channel-assembly change | the plan requires the version in the package manifest; pin it when T-013 lands |

---

## 5. Reviewer checklist

1. Does every FR map to a task that can be checked without the Q4 renderer, or is it honestly blocked?
2. Are the five negative controls in SC-001 the same five T-006 builds?
3. Is "the Q4 set" unambiguous everywhere it appears?
4. Does any statement imply a capture exists today?
5. Is the 003-vs-004 conflict described as a conflict rather than resolved by assertion?
6. Does anything commit a third-party byte or depend on the 006 corpus as evidence?
7. Are the pinned encoder's version and hash reproducible from `pins.json` (once T-002 exists)?
