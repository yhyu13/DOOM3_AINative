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
| Task count | unique `T-nnn` ids in `tasks.md` | 30; summary says 30 (28 not started + 2 blocked) |
| Table shape | every table row has the same pipe count as its header | 0 mismatches in all four files |
| Relative links | resolve every `[](…)` target on disk | 0 broken |
| Decision references | every `C-nn` cited in spec/plan/tasks exists in `boundary.md` | 15/15 defined |
| Requirement coverage | map each FR to at least one task | 16/16 covered (table in §3) |
| Citations | each `file:line` points at a line saying what the prose claims | checked by hand on the newer base `29a1e37`; see §2.2 |

Counts that changed during this round were corrected where they appeared: the package says "fifteen
decisions" and "30 tasks" everywhere, and the US2 row counts 9 tasks (it previously said 8).

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

### 2.3 Independent review (round 2)

An independent agent reviewed `9f45509` without being told what to find. It verified anchors, numbers,
the backend claim, internal consistency, SC testability, scope discipline, and reported 21 findings:
3 blocking, 8 non-blocking, 10 nits. All 21 are fixed below. It also confirmed the numbers, the
counts, the backend claim (once "engine backend" was softened) and the acyclic dependency graph.

| Id | Severity | Finding | Disposition |
|---|---|---|---|
| R1 | **blocking** | FR-001 promised a mechanical "reject any third-party asset byte", but the design only classified by path — a renamed fan-pack TGA with a fabricated provenance would pass | FR-001 now names three mechanical rules (extension allowlist `.png`/`.exr`, pack-hash denylist, provenance `inputs` that name an authoring source) and says plainly that byte origin is not decidable from content, so a passing file still needs human review. C-01a carries the same mechanism, SC-001 names the six controls, T-003/T-006 implement them |
| R2 | **blocking** | FR-008/SC-006 required a "before/after capture" of an RXGB conversion, but the only named source was the 006 corpus, which C-04 forbids in a PR; no fixture, no surface, no tolerance | FR-008 now names the committed synthetic fixture `tests/content/fixtures/golden-rxgb.png` with a recorded tolerance (≤ 2/255 per channel including reconstructed Z, no axis swap). Corpus conversion stays a local optional check, never committed, never cited. T-026 and the plan's stage 2 follow |
| R3 | **blocking** | The corpus was described as "diffuse only / no normal, 0 PBR channels", while the package's own evidence quotes a specular `_s` map; `_local` presence was never measured (006 counted extensions, not suffixes) | the inventory row now says `_d` and `_s` are present, `_local` is **unmeasured**, and no roughness/metalness/AO/emissive channel exists in DOOM3 naming. Open question 6 records the gap and T-007 measures suffixes by name only |
| R4 | non-blocking | Assumption 6 claimed Python and Pillow were "pinned in the plan and verified by hash"; only texconv is pinned | assumption 6, the plan's dependency row and T-013 now say Pillow is **observed** (12.3.0) and that `requirements-content.txt` pins it in T-013, with versions recorded in every package |
| R5 | non-blocking | FR-005's first clause said "Every material", contradicting its own "never every `.mtr`" | FR-005 says "Every material **in the Q4 set**"; a `Q4MaterialSet` entity was added |
| R6 | non-blocking | `capture-q4.ps1` "always exits 3" conflicts with SC-008's post-Q4 pass path, which no task owned | the plan scopes "always" to "while 004 is pre-Q4"; T-050 now carries the exit-0 acceptance with backend + adapter |
| R7 | non-blocking | `package_sha256` had no definition, so SC-003's determinism could be circular or path-dependent | defined as SHA-256 over the sorted sidecar/`.mtr`/texture hashes, excluding the field itself; T-013 acceptance states it |
| R8 | non-blocking | Budget rows were "citations", so a cell could pass with no measurement, and open question 4 asked for a task 004 already names (T056) | FR-012 requires `{task: T056, artifact, state}`; SC-007 fails a citation without an artifact; open question 4 is answered |
| R9 | non-blocking | T-011/T-012/T-013 consume T-010's files but did not depend on it | added T-010 to their depends-on; no cycle |
| R10 | non-blocking | "D3D12 is the only engine backend with an implementation" overstated integration: it is a Q1a test library, and the engine still links GL | C-14 and the Q1 row say "only backend with a source implementation … built as the Q1a test library, not yet engine-integrated" |
| R11 | non-blocking | T-031 grepped only `-HdTextures`; FR-016's credential clause had no check | T-031 also greps `image_usePrecompressedTextures`; FR-016 and T-003 add path and credential patterns |
| R12 | nit | SC-009's "no numeric budget" was broader than C-08, which allows the 2K authoring cap | SC-009 says "no numeric VRAM, residency, timing or streaming budget" |
| R13 | nit | T-025's "or an explicit statement that 2K caps the cell" weakened the large-surface cell | T-025 now requires a ≥ 4096 px package assembled from ≤ 2K committed sources, with the assembly recorded |
| R14 | nit | §1.1 framed 003 D4 vs 004 as a standoff, but 004 is a Draft claiming supersession | §1.1 says 004 is a Draft that claims to supersede 003 D4, that 003 D4 has not been withdrawn, and that C-14 is an **interim** ratification |
| R15 | nit | The plan header named the wrong branch | corrected to `docs/38-rhi-hd-content-spec` |
| R16 | nit | T-001 edited `.gitignore`, outside the workstream's stated scope, and the edit was redundant | dropped; the root file already ignores `out/` and any `.cache/` |
| R17 | nit | The plan pointed at `specs/008-classic-shaders/evidence/implementation.md`, which is not on `main` | points at `evidence/verification.md`, which is |
| R18 | nit | "`demo00.pk4` \| free demo, 2004 assets" states a count no artifact supports | "2004-era assets" |
| R19 | nit | "12/12 on Chrome 156" omitted the software-adapter attempt recorded NOT RUN | now "12/12 required checks … the software-adapter attempt is NOT RUN" |
| R20 | nit | "power-of-two" appeared in the plan and T-005 but nowhere in the boundary or spec | replaced with "≤ 2048 per axis and a multiple of 4", which is what BC requires |
| R21 | nit | The demo-only claim cited T010–T013; the evidence is T014 | citation corrected to T014 |

Two of these (R2, R3) were only visible by cross-reading the package against spec 006's own evidence —
the review is worth the cost for that alone.

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
---

## 6. Sign-off

| Role | Who | Date | Scope | Result |
|---|---|---|---|---|
| Author | implementation workstream | 2026-10-29 | boundary, spec, plan, tasks, toolchain | self-review in §2.1 and §2.3 |
| Independent reviewer | separate agent, not the author | 2026-10-29 | the specification package at `9f45509` | 21 findings, 3 blocking, all fixed before merge |
| **Human sign-off** | repository owner | 2026-10-29 | spec 009, before implementation started | **approved to implement the content toolchain; Q4 captures stay BLOCKED** |

The human sign-off covers the *specification*. It is not a review of the implementation PR, which needs
its own reviewer under `docs/GIT_WORKFLOW.md` (reviewer is never the author). The capture stage remains
`BLOCKED` by the same sign-off: no Q4 renderer exists, so nothing may claim a capture.