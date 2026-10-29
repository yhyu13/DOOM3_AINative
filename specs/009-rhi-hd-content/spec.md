# Feature specification: RHI-era HD content pipeline and Q4 content set

**Feature**: `009-rhi-hd-content`
**Status**: specification **complete for review** (2026-10-29). Tooling, manifests and authored sources can be built now; every capture that requires the Q4 renderer is `BLOCKED` (spec 004 has not reached Q4).
**Input**: the S7 workstream: what content the RHI era may consume, produce and claim, and how each claim is proven.
**Boundary**: [`boundary.md`](boundary.md) — fifteen decisions (C-01a…C-14). This spec is bound by them; a spec statement that contradicts one requires amending the boundary in the same PR.

---

## 1. Problem

Q4 asks for "hybrid deferred opaque + forward/compatibility, HDR PBR, shadows, indirect
light/reflections/atmosphere, authored legal hero assets, budget/residency and moving-scene art review"
(`specs/004-quality-renderer/plan.md:87`). The project can render none of that yet, and it owns no
content that could demonstrate it.

What exists today, and why none of it closes the gap:

| Asset | What it is | Why it is not Q4 content |
|---|---|---|
| Wulfen Texture Pack 2.0 | 853,775,687 B, 898 TGA, 8 `.mtr`, no roughness/metalness/AO/emissive channels; `_d` and `_s` maps present, `_local` (normal) presence unmeasured | third-party derivative work; proven against the **legacy GL** renderer (spec 006), not an RHI path |
| `demo00.pk4` | free demo, 2004-era assets | not authored, not PBR, EULA-bound |
| spec 006's HD result | 121 textures uploaded larger, `DIFF_PERCENT=10.8636` | a legacy-path result; `-HdTextures` forces the TGA path and no roughness/metalness map was involved |

So three things are missing, in this order:

1. **A content contract.** What may be committed, what may only be staged locally, what a material
   consists of, and which formats are acceptable — the boundary answers this, the pipeline must enforce it.
2. **A pipeline.** Turning authored source into GPU-ready packages with a manifest that a verifier can
   reproduce, without committing a single third-party byte.
3. **Authored content that exercises Q4.** A small, legal, PBR-capable set covering the feature matrix,
   so that Q4's art review has something real to review.

Without these, the first Q4 content PR is an unreviewable mix of licence risk, format drift and
captures nobody can reproduce.

---

## 2. What this feature is, and is not

**Is**: a content contract, an executable pipeline (validator, converter, material builder, package
manifest), an authored hero set with provenance, and an evidence procedure whose capture steps are
honestly marked blocked until the renderer exists.

**Is not**:

- renderer work of any kind (that is `specs/004`; this spec supplies content and gates);
- a replacement for spec 006's legacy GL track (that track stays valid and separate);
- a claim that Q4 is achieved, or that any capture in this spec exists today;
- a licence opinion on fan packs or retail data (`boundary.md` C-04, C-11);
- virtual texturing, streaming, or any vendor texture format.

---

## 3. Glossary

| Term | Meaning |
|---|---|
| **source asset** | an authored file we own: PNG/EXR image, `.mat.json` sidecar, `.mtr` text, or a generator script |
| **staged corpus** | a pack outside git (demo, fan pack, user install) described by a manifest; never committed |
| **content manifest** | `content-manifest.json`: packs, hashes, entry counts, search order, licence note (`boundary.md` C-02) |
| **material sidecar** | `content/materials/<name>.mat.json`: the PBR channel description for one `.mtr` material (`C-13`) |
| **material package** | the pipeline output: GPU-ready textures + a material record + a package manifest hash |
| **channel state** | `authored` / `derived:<method>` / `unavailable` for each of baseColor, normal, roughness, metalness, AO, emissive (`C-05`) |
| **Q4 set** | the explicit material list in `content/manifest/q4-materials.json`; only these materials require a sidecar (FR-005) |
| **matrix cell** | one of the seven Q4 content coverage cases (`C-12`) |
| **budget row** | a recorded VRAM/load-time measurement for one cell, from a measurement task, never a guess (`C-08`) |
| **BLOCKED** | a check that cannot run because the renderer does not exist; never reported as a pass |

---

## 4. Requirements

### 4.1 Content contract (enforcement)

- **FR-001** — A checked-in validator MUST enforce three mechanical rules for committed assets — an
  extension allowlist (`.png`, `.exr`; an extracted `.tga`/`.dds` cannot pass), a SHA-256 denylist
  generated from any locally staged pack, and a provenance record whose `inputs` name an authoring
  source rather than a pack — and MUST reject a file that fails any of them, lacks a CC0 licence file,
  or exceeds 2048 pixels on an axis (`C-01a`, `C-01b`). Byte origin itself is not decidable from
  content, so a passing file is still subject to human review in its PR; the validator proves the
  mechanical part and says so.
- **FR-002** — Every staged corpus MUST be described by a `content-manifest.json` with pack name, byte
  size, SHA-256, entry/file/texture counts, search order and licence note. A missing pack MUST produce
  `SKIPPED` with the reason, never a pass (`C-02`).
- **FR-003** — Every authored asset MUST have a provenance record answering `retail_text_consulted`,
  naming its inputs, and recording the file hash; a hash mismatch MUST fail (`C-03`, `C-11`).
- **FR-004** — The validator MUST reject a "hero" claim for any asset that is not original CC0, so a
  fan pack cannot be promoted by relabelling (`C-04`).

### 4.2 Material and texture contract

- **FR-005** — Every material **in the Q4 set** MUST have both a `.mtr` (identity, stages, program keys)
  and a `.mat.json` sidecar; the builder MUST fail on a sidecar without a matching `.mtr` and on a `.mtr`
  listed in the Q4 set whose material name has no sidecar (`C-13`). The Q4 set is the explicit list in
  `content/manifest/q4-materials.json` — never "every `.mtr` in the tree", because retail and mod
  material text stays valid without a sidecar.
- **FR-006** — Each PBR channel MUST carry an explicit state — `authored`, `derived:<method>` or
  `unavailable` — and the builder MUST refuse a channel with no state. Silent synthesis is forbidden
  (`C-05`).
- **FR-007** — Textures MUST be BC7 (colour/emissive), BC5 (two-channel normal) or BC4 (single
  channel), with a full mip chain, and the manifest MUST record format, dimensions, mip count and the
  sRGB/linear flag per texture (`C-07`). DXT/BC1-3 is rejected for RHI-era content.
- **FR-008** — A legacy RXGB normal map MUST NOT be consumed directly. A converter MUST produce the
  linear normal with a declared handedness and Z-reconstruction, and MUST be validated on the committed
  synthetic fixture `tests/content/fixtures/golden-rxgb.png` with a recorded tolerance (per-channel
  absolute error ≤ 2/255 including the reconstructed Z, and no axis swap) (`C-06`). Converting a
  locally staged corpus is allowed as an extra local check; its output is never committed and never
  cited as evidence (`C-04`).
- **FR-009** — The builder MUST emit a package manifest binding material name, sidecar hash, texture
  hashes, format/mip facts and the source `.mtr` hash, so a package can be reproduced and diffed.

### 4.3 Authored content and coverage

- **FR-010** — The Q4 content set MUST cover all seven matrix cells: opaque dielectric, opaque metal,
  alpha-tested geometry, emissive, skinned character, transmissive/glass, and one large environment
  surface with a residency test surface (`C-12`).
- **FR-011** — Each cell MUST have: an authored source, a sidecar with every channel state, a package
  manifest, and — once the Q4 renderer exists — a capture naming backend and adapter (`C-14`).
- **FR-012** — A budget row MUST exist per cell and MUST have three fields: the spec-004 measurement
  task (`T056`), the artifact it produced, and a `state` of `recorded` or `blocked`. A bare citation
  without a measurement does not satisfy it. This spec MUST NOT contain a numeric VRAM or timing budget
  of its own (`C-08`).
- **FR-013** — Evidence MUST name the backend and adapter. An evidence file without them is incomplete
  (`C-14`).

### 4.4 Reproduction and separation from the legacy track

- **FR-014** — The pipeline MUST run from a clean checkout with only authored sources present, and MUST
  report `SKIPPED` for every step that needs a staged corpus that is absent.
- **FR-015** — No capture from spec 006 may be cited as Q4 evidence, and no pipeline step may depend on
  `-HdTextures` or on forcing `image_usePrecompressedTextures 0` (`C-09`).
- **FR-016** — No machine-specific absolute path, credential or staged pack may appear in a tracked file.
  The validator checks absolute-path patterns (`[A-Za-z]:\\`, `$env:`, `/home/`, `/Users/`) and
  credential patterns (a token/secret assignment, a `.env` value); a staged pack is checked by path and
  by hash against the manifest.

### 4.5 Key entities

| Entity | Fields (required) |
|---|---|
| `ContentManifest` | `schema`, `packs[]`, `search_order[]`, `licence_notes[]` |
| `PackEntry` | `name`, `bytes`, `sha256`, `entries`, `files`, `textures`, `materials`, `state` (`staged`/`absent`/`skipped:<reason>`) |
| `AssetRecord` | `id`, `path`, `sha256`, `licence` (must be `CC0-1.0` for hero), `author`, `retail_text_consulted` |
| `MaterialSidecar` | `schema`, `material`, `mtr`, `channels{baseColor,normal,roughness,metalness,ao,emissive}`, each with `state` and `path` |
| `MaterialPackage` | `schema`, `material`, `sidecar_sha256`, `mtr_sha256`, `textures[]` (channel, `file` — the name inside the package — format, width, height, mips, srgb, sha256, sources[]), `toolchain{python, pillow, texconv}`, `package_sha256` — defined as SHA-256 over sidecar, then `.mtr`, then each texture hash in channel-name sort order, joined by LF, excluding the field itself |
| `MatrixCell` | `id`, `name`, `sources[]`, `package`, `capture` (path or `BLOCKED:<reason>`), `budget_row{task, artifact, state}` |
| `Q4MaterialSet` | `schema`, `materials[]` (material name + `.mtr` relative path); the only definition of "in the Q4 set" (FR-005) |
| `BudgetRow` | `cell`, `task` (`T056` until 004 renumbers), `artifact`, `state` (`recorded` / `blocked:<reason>`) |

---

## 5. User scenarios

### US1 — One hero material, end to end (P1)

A developer authors one dielectric hero material, runs the pipeline, and gets a package manifest plus a
capture — or an explicit `BLOCKED` naming the missing renderer. This is the MVP: it proves the contract
and the tooling on one material before any breadth.

**Independent test**: `tools/content/validate.ps1` then `tools/content/build-material.ps1 -Material
hero_dielectric` then `tools/content/verify-package.ps1`; every step exits 0, and the capture step
reports `BLOCKED: spec 004 has not reached Q4` rather than passing.

### US2 — The seven-cell matrix has legal sources and budget rows (P2)

A reviewer opens one table and sees, per cell: the authored source, the channel states, the package
hash, the budget row citation, and either a capture or the named blocker.

**Independent test**: `tools/content/matrix-report.ps1` renders the table from the manifests; every
cell resolves, and any cell without a budget row fails the report.

### US3 — A contributor reproduces the pipeline from a clean checkout (P3)

From a clean clone with no staged corpus, the pipeline runs and reports `SKIPPED` per step; with a
legal local corpus staged per the manifest, the same commands run and the verifier checks hashes.

**Independent test**: clean clone → `tools/content/validate.ps1` (exit 0, `SKIPPED` for corpus steps)
→ stage a corpus → rerun → hashes verified.

### US4 — The legacy track stays separate (P3)

A reviewer can see that no 006 artifact is reused as Q4 evidence and that the legacy pack still loads on
the legacy build.

**Independent test**: the evidence index lists every Q4 capture with backend + adapter, and a grep of
the content evidence finds no `-HdTextures` and no 006 capture path.

### Edge cases

- A sidecar declares a normal map whose handedness is not stated → builder fails.
- A pack's bytes change while its size stays the same → hash mismatch fails; size alone is never enough.
- A hero directory contains a PNG with no provenance record → validator fails.
- A `.mtr` in the Q4 set has no sidecar → builder fails; a `.mtr` outside the set may exist without one.
- The Q4 renderer appears mid-work: capture tasks move from `BLOCKED` to runnable, with no change to the
  content contract.
- Two materials share one texture file: allowed, but the package manifest hashes it per package so a
  shared file cannot silently change one package.

---

## 6. Success criteria

| ID | Criterion | Evidence |
|---|---|---|
| SC-001 | The content contract is executable | `tools/content/validate.ps1` exits 0 on a clean checkout and exits 1 on every negative control: a `.tga`/`.dds` under `content/hero/` (extension rule), a file whose hash is on the pack denylist, a hero without provenance, a non-CC0 licence, a channel with no state, a sidecar without a `.mtr` (six controls; `tasks.md` T-006 builds them) |
| SC-002 | A staged corpus is described, never committed | `content-manifest.json` round-trips; `git ls-files` contains no `.pk4/.pk3/.pak` and no corpus path |
| SC-003 | One hero material builds reproducibly | two consecutive `build-material.ps1` runs into separate directories produce identical `package_sha256`, defined as SHA-256 over sidecar, `.mtr`, then each texture hash in channel-name sort order (no timestamps, no paths) |
| SC-004 | Channels are declared, not invented | every sidecar channel carries `authored`/`derived:`/`unavailable`; the builder rejects an undeclared channel |
| SC-005 | Formats are compliant | every packaged texture is BC7/BC5/BC4 with a full mip chain and a recorded sRGB flag |
| SC-006 | The RXGB conversion is proven | the committed fixture `tests/content/fixtures/golden-rxgb.png` converts within the recorded tolerance (≤ 2/255 per channel including reconstructed Z, no axis swap); the before/after images are both derived from that fixture, so nothing third-party is committed |
| SC-007 | The matrix is covered | all seven cells have a source, a sidecar, a package manifest and a budget row with `task` + `artifact` + `state`; a row citing a task without an artifact fails |
| SC-008 | Captures never lie | today every cell states `BLOCKED:<reason>`; once Q4 lands, `capture-q4.ps1` exits 0 only after writing a capture that names backend + adapter |
| SC-009 | Budgets are measured, not invented | no numeric VRAM, residency, timing or streaming budget appears in this spec or the plan; each budget row names the spec-004 measurement task and its artifact |
| SC-010 | The legacy track is not reused | the evidence index shows no spec-006 capture and no `-HdTextures` dependency |

---

## 7. Out of scope

- Any renderer, RHI, pass, resolution or shader change (spec 004 owns those; spec 008 owns programs).
- Virtual texturing, texture streaming, residency design.
- Vendor or platform-specific containers beyond BC formats.
- Retail texture study, extraction, or derived fixtures.
- The legacy GL HD track (006), which keeps its own gate and evidence.
- Performance tuning; only budget *recording* is in scope.
- The look-development decisions themselves (Q4's art review signs those off).

---

## 8. Assumptions

1. D3D12 is the acceptance backend until a decision PR reconciles `specs/003-render-modernization/spec.md:62` (D4) with spec 004 (`boundary.md` C-14). Vulkan has no source here; WebGPU has a browser conformance spike but no engine toolchain, and Q2 stays blocked (`specs/004-quality-renderer/evidence-q7a.md`). Reversing it re-encodes containers, not art.
2. Hero assets are authored by this workstream under CC0-1.0, ratified by the repository owner in the first asset PR (`boundary.md` Q3).
3. `.mtr` remains material identity; PBR lives in `.mat.json` sidecars (`C-13`).
4. Q4 needs no retail data, because hero assets are authored; if that changes, 009 inherits 008's T-031/T-032 blockers (Q6, still open).
5. The renderer does not exist yet, so every capture task is `BLOCKED` today and that is the honest state.
6. Image processing uses a **hash-pinned** `texconv` (DirectXTex 2026.5.8.1, sha256 recorded) for BC
   encoding, plus Python for channel assembly. Python 3.14.4 and Pillow 12.3.0 are **observed** on the
   authoring host, not pinned: a `content/tools/requirements-content.txt` pinning `Pillow==12.3.0`
   lands with T-013, and every package records the Python, Pillow and texconv versions so a mismatch
   is visible rather than silent.

---

## 9. Dependencies and risks

| Risk | Impact | Mitigation |
|---|---|---|
| Q4 renderer never lands | content is authored but unproven visually | every capture is `BLOCKED`, not faked; contract and tooling are still valuable and testable |
| Hero authoring quality is low | Q4's art review rejects the set | start with US1 (one material), get art review early, then breadth |
| Licence mistake on a hero asset | licence exposure | CC0-only rule, per-asset provenance, validator rejects anything else |
| BC7 encoding differences | package hashes drift between machines | pinned encoder + hash-verified pin; reproducibility is SC-003 |
| Sidecar and `.mtr` drift | material identity and PBR description disagree | builder requires both, hashes both, and fails on a staged-only `.mtr` in the Q4 set |
| Budget rows invented under deadline pressure | fake acceptance | SC-009 makes an uncited number a spec violation |
| Legacy captures reused as Q4 evidence | false Q4 claim | SC-010 plus review; C-09 names the failure mode |

---

## 10. Traceability

| Requirement | Artifact | Verification |
|---|---|---|
| FR-001, FR-004 | `tools/content/validate.ps1`, `content/hero/**` | SC-001 |
| FR-002 | `content/manifest/content-manifest.json`, validator | SC-002 |
| FR-003, FR-011 | `content/hero/*/provenance.json` | SC-001, SC-007 |
| FR-005, FR-006, FR-009 | `content/materials/*.mat.json`, `build-material.ps1` | SC-003, SC-004 |
| FR-007 | builder + package manifest | SC-005 |
| FR-008 | `convert-normal.ps1` (or `.py`), golden surface | SC-006 |
| FR-010, FR-011, FR-012 | `matrix-report.ps1`, budget rows | SC-007, SC-009 |
| FR-013 | evidence index | SC-008 |
| FR-014, FR-015, FR-016 | validator + evidence index | SC-010 |

Upstream: `boundary.md` (this feature), `specs/004-quality-renderer/plan.md:87` (Q4), `specs/006-hd-assets/`
(legacy precedent, explicitly not reused), `specs/008-classic-shaders/` (program keys and provenance posture).

---

## 11. Open questions

1. **Q2** — does a local D3HDP corpus exist, and does it carry normal/specular maps? Convenience only.
2. **Q4** — one pack with two material resolutions, or two packs? Decides where the C-06 conversion runs.
3. **Q6** — does Q4 need retail data at all? If yes, the capture tasks inherit 008's T-031/T-032 blockers.
4. Which measurement task in spec 004 supplies each budget row? **Answered:** `T056`
   (`specs/004-quality-renderer/plan.md:87`, `specs/004-quality-renderer/tasks.md:125`); rows carry
   `T056` until 004 renumbers it.
5. Does the Q4 art review accept a procedural source for any cell (for example the grating), or must
   every cell be hand-painted?
6. Do `_local` (normal) maps exist in the local corpus? The 006 inventory counted extensions, not
   suffixes, so this is unmeasured; the converter's committed fixture (FR-008) does not depend on the
   answer.
