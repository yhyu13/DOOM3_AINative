# Tasks: RHI-era HD content pipeline and Q4 content set

**Input**: [`spec.md`](spec.md), [`plan.md`](plan.md), [`boundary.md`](boundary.md)
**Issue**: #38
**Format**: `[P]` = can run in parallel with its siblings (different files, no dependency).
**Evidence rule**: a task is `DONE` only with a command and its output, or an artifact plus the gate that
checks it. A task that needs the Q4 renderer is `BLOCKED`, never "pass".

---

## Summary

| Scope | Done | Total | State |
|---|---|---:|---|
| Setup | 0 | 2 | not started |
| Foundational (blocks every story) | 0 | 5 | not started |
| US1 — one hero material end to end | 0 | 6 | not started |
| US2 — seven-cell matrix | 0 | 9 | not started |
| US3/US4 — reproduction and separation | 0 | 3 | not started |
| Polish and review | 0 | 3 | not started |
| Gates | 0 | 2 | blocked |
| **All** | **0** | **30** | specification only; no pipeline, no asset, no capture |

Nothing here is implemented. The boundary, spec and plan are written; the encoder pin was verified
ad hoc (see T-002), and no script exists yet.

---

## Phase 1 — Setup

| Id | Task | Status | Acceptance / evidence |
|---|---|---|---|
| T-001 | Create `content/{hero,materials,manifest,tools}` and `tests/content/fixtures`; add `content/README.md` stating C-01a/C-01b in one screen. No `.gitignore` edit is needed: the root file already ignores `out/` and any `.cache/` directory | NOT STARTED | `git check-ignore -v content/tools/.cache/x` resolves; `git ls-files content` shows no binary |
| T-002 | Pin the encoder: `content/tools/pins.json` + `get-encoder.ps1` (download, SHA-256 verify, version check, fail closed) | NOT STARTED | `get-encoder.ps1` prints `ENCODER OK` and the version; a wrong hash exits 3. Pre-verified ad hoc on 2026-10-29: `texconv` 2026.5.8.1, 966,480 B, sha256 `DCFDEC10…5EEF06`, BC7 probe produced a 260-byte DX10 DDS |

---

## Phase 2 — Foundational (blocks US1–US4)

| Id | Task | Depends on | Status | Acceptance |
|---|---|---|---|---|
| T-003 | `validate.ps1` contract gate: extension allowlist under `content/hero/**`, pack-hash denylist, corpus-manifest presence, machine-path and credential patterns | T-001 | NOT STARTED | exit 0 on a clean tree; exit 1 for a tracked pack, a `.tga`/`.dds` source, a denylisted hash, an absolute path, a credential pattern, or a manifest naming an absent corpus without `SKIPPED` |
| T-004 | Provenance gate: `AssetRecord` per hero file, CC0 licence file present, hash reproduced | T-003 | NOT STARTED | a hero PNG without a record fails; a changed PNG fails on hash; a non-CC0 licence string fails |
| T-005 | Sidecar gate: `.mat.json` schema, channel states, `.mtr` pairing, ≤ 2048 px per axis, dimensions a multiple of 4; reads the Q4 set from `content/manifest/q4-materials.json` (never "every `.mtr`") | T-003 | NOT STARTED | sidecar without `.mtr` fails; a `.mtr` listed in the Q4 set without a sidecar fails; a `.mtr` outside the set may exist without one; a channel with no state fails |
| T-006 | `tests/content/run-gates.ps1`: positive run plus the six negative controls SC-001 names (extension, denylist hash, missing provenance, non-CC0 licence, channel without a state, sidecar without `.mtr`) | T-003, T-004, T-005 | NOT STARTED | positive exits 0; each of the six exits 1 with a named diagnostic; the denylist fixture is a recorded pack-member hash |
| T-007 | Corpus suffix inventory: count _d, _s, _local, _bump, _h suffixes in the locally staged pack by name only (no bytes, no extraction) and record the counts in census-style markdown | T-003 | NOT STARTED | with a staged pack: a suffix table with counts; without one: SKIPPED with the reason. Names and counts only, never file content |

---

## Phase 3 — US1: one dielectric hero material, end to end (P1, MVP)

| Id | Task | Depends on | Status | Acceptance |
|---|---|---|---|---|
| T-010 [P] | Author `content/hero/dielectric/` baseColor + roughness + authored normal (2K, CC0), plus `LICENSE` | T-001 | NOT STARTED | files ≤ 2K per axis; `LICENSE` is CC0-1.0 text |
| T-011 [P] | `content/hero/dielectric/provenance.json` (`AssetRecord` per file) | T-004, T-010 | NOT STARTED | T-004 passes on the directory |
| T-012 [P] | `content/materials/hero_dielectric.mtr` + `.mat.json` with every channel state, and the first entry in `content/manifest/q4-materials.json` (`Q4MaterialSet` schema) | T-005, T-010 | NOT STARTED | T-005 passes; `metalness: unavailable` is stated, not omitted; the material is listed in the Q4 set |
| T-013 | `build-material.ps1`: assemble channels with Pillow, encode with the pinned texconv (`-f BC7_UNORM_SRGB/-f BC5_UNORM/-f BC4_UNORM -m 0`), write `package.json`; add `content/tools/requirements-content.txt` pinning `Pillow==12.3.0` | T-002, T-010, T-012 | NOT STARTED | one package; `package.json` records format, dimensions, mips, sRGB flag, every hash, and the Python/Pillow/texconv versions; `package_sha256` equals the sorted-hash digest of SC-003 |
| T-014 | `verify-package.ps1`: re-hash, format/mip/sRGB assertions, determinism against a second build | T-013 | NOT STARTED | two builds into different directories produce identical `package_sha256`; a mutated DDS fails |
| T-015 | `capture-q4.ps1`: refuse to run without a Q4 build, print `BLOCKED: spec 004 has not reached Q4`, exit 3 | T-013 | NOT STARTED | exit 3 with the exact reason; never exit 0 without a capture naming backend + adapter |

---

## Phase 4 — US2: the seven-cell matrix (P2)

Each cell repeats the US1 shape: source + provenance + sidecar + package + budget-row link. They are
parallel by construction; the list is the matrix, not a sequence.

| Id | Cell | Depends on | Status | Acceptance |
|---|---|---|---|---|
| T-020 [P] | opaque metal (authored metalness) | T-013 | NOT STARTED | package + sidecar; `metalness: authored` |
| T-021 [P] | alpha-tested geometry (grating/foliage) | T-013 | NOT STARTED | baseColor with alpha; engine-side alpha-test note recorded, no shader change here |
| T-022 [P] | emissive | T-013 | NOT STARTED | `emissive: authored`, BC7 sRGB; if only SDR is authored, that is stated |
| T-023 [P] | skinned character surface | T-013 | NOT STARTED | package plus a note that the skinned path is exercised by spec 004's Q3/Q4 fixtures, not here |
| T-024 [P] | transmissive/glass | T-013 | NOT STARTED | `baseColor` + `roughness`; opacity modelling left to spec 004, recorded as a dependency |
| T-025 [P] | large environment surface with a residency test surface | T-013 | NOT STARTED | a package of at least 4096 px per axis assembled from committed ≤ 2K sources, with the assembly and the assembled dimensions recorded in the package manifest (no "2K caps the cell" fallback) |
| T-026 | RXGB conversion: `convert-normal.ps1` + the committed fixture `tests/content/fixtures/golden-rxgb.png` | T-013 | NOT STARTED | the fixture converts within ≤ 2/255 per channel including reconstructed Z, with no axis swap; before/after images are derived from the fixture and are the only ones committed. A local corpus conversion is an optional extra check, never committed and never cited (C-04, C-09) |
| T-027 | Budget rows for all seven cells: `{task: T056, artifact, state}` | T-020…T-025 | NOT STARTED | every cell has a row with `T056`, an artifact and a state of `recorded` or `blocked:<reason>`; a citation without an artifact fails; **no number is written in 009** |
| T-028 | `matrix-report.ps1`: render the seven-cell table with states and blockers | T-027 | NOT STARTED | table has no empty cell; a missing budget row fails the report |

---

## Phase 5 — US3/US4: reproduction and separation from the legacy track (P3)

| Id | Task | Depends on | Status | Acceptance |
|---|---|---|---|---|
| T-030 | Clean-checkout run: `validate.ps1` with no staged corpus must `SKIP` corpus steps with a reason and still exit 0 | T-003, T-006 | NOT STARTED | a clean clone run prints `SKIPPED` per corpus step and exits 0; with a staged corpus the same command verifies hashes |
| T-031 | Evidence index: every capture names backend + adapter; no spec-006 capture, no `-HdTextures` and no `image_usePrecompressedTextures` dependency | T-015, T-028 | NOT STARTED | index lists 0 captures today and states why; a grep for `-HdTextures` and for `image_usePrecompressedTextures` in 009 evidence returns nothing |
| T-032 | `content/README.md`: how to author, build, verify, and what may never be committed | T-013 | NOT STARTED | a reader can run the pipeline from it without reading the spec |

---

## Phase 6 — Polish and review

| Id | Task | Status | Acceptance |
|---|---|---|---|
| T-040 | Self-review of this package against `boundary.md` (does any FR contradict a C-xx?) | NOT STARTED | findings table with fixes |
| T-041 | Independent review of the first pipeline PR (repository rule: an implementer does not self-approve) | NOT STARTED | reviewer verdict recorded |
| T-042 | `evidence/implementation.md` with real commands, exit codes and outputs | NOT STARTED | same shape as `specs/008-classic-shaders/evidence/implementation.md` |

---

## Phase 7 — Gates and blockers

| Id | Gate | Status | Why blocked | What unblocks it |
|---|---|---|---|---|
| T-050 | Execute the seven captures on the RHI path; `capture-q4.ps1` then exits 0 and records backend + adapter | BLOCKED | spec 004 has not reached Q2/Q3/Q4; there is no renderer to capture from | 004 delivery of Q4, or at minimum a Q2 classic-compatible path for the non-PBR cells. Acceptance: seven captures, each naming backend + adapter |
| T-051 | Retail texture study, if Q6 answers "yes" | BLOCKED | no counsel review; same position as spec 008 T-032 | a reviewer decision; P-series rules in `specs/008-classic-shaders/provenance.md` |

A blocked gate is never reported as passed. The matrix report must show both gates by name.

---

## Dependencies and execution order

```text
T-001 ─┬─ T-002 ─────────────┐
       ├─ T-003 ─ T-004 ─ T-011
       │        ├─ T-005 ─ T-012 ─ T-013 ─ T-014 ─ T-015 ─ T-031
       │        └─ T-006                        └─ T-026
       └─ T-010 ─────────────────────────────────────┘
T-020…T-025 (after T-013) ─ T-027 ─ T-028 ─ T-031
T-007 (suffix inventory, SKIPPED without a pack)
T-003 + T-006 ─ T-030
T-050 (blocked by spec 004 Q4) · T-051 (blocked by counsel)
```

### Parallel opportunities

- **Phase 4 cells T-020…T-025** are six independent authoring jobs; each touches its own directory.
- **T-010 / T-011 / T-012** can be authored together but must be reviewed as one material.
- **T-002** (encoder) is independent of the validator work and can run in parallel with T-003.

### Serialisation constraints

- T-004 and T-005 both edit `validate.ps1`'s call graph; one writer at a time.
- T-013 owns `build-material.ps1`; T-026 owns `convert-normal.ps1` and must not edit the builder.
- T-027 owns the budget rows; no other task may write a numeric budget.

---

## Notes

- Nothing in this list touches `src/`, `shaders/` or `doom3_orig/`. A task that needs to is out of scope
  and belongs to spec 004.
- The first pipeline PR needs the 008 work landed first: both live in the same `main` worktree today and
  must not share a PR.
