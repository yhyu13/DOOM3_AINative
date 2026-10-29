# Shader provenance and legal boundary

**Status**: policy and register **complete**. **No shader has been authored**, so every record below
is a slot with a decision, not evidence of authorship. Licence review by counsel has **not**
happened and is not claimed.

This file exists because the hard blocker for classic parity is not the graphics API; it is the
missing, EULA-bound shader text ([`census.md`](census.md) §5). A port that is casual about where
shader code came from is not reviewable.

---

## 1. The legal facts in this repository

| Fact | Evidence | Consequence |
|---|---|---|
| The engine source is GPLv3 | `doom3_orig/doom3/COPYING.txt` | engine changes may be distributed under GPLv3 |
| The GPL release contains no game data | `doom3_orig/doom3/README.txt` ("This source release does not contain any game data, the game data is still covered by the original EULA") | no retail asset, including `glprogs`, may be committed |
| Shader programs are loaded from data at runtime | `neo/renderer/draw_arb2.cpp:368` (`idStr fullPath = "glprogs/";`) | the programs are data; the GPL release does not convey them, and their use remains under the EULA |
| The retail programs are absent from the tree | no `.vfp` / `.vp` / `.fp` under `doom3_orig/doom3` | the port cannot translate what it does not have, and must not fetch it |
| Materials can name arbitrary programs | `neo/renderer/Material.cpp:1490` | the port cannot assume a closed, known set |

The legal reading used here is conservative: **reading retail ARB text and porting it can create a
derivative work of EULA-bound material.** That is a risk statement, not a legal opinion. A lawyer
must decide before any retail-derived study.

---

## 2. Rules

| ID | Rule | Enforcement |
|---|---|---|
| P-1 | No retail shader text, translated retail text, or fixture derived from it is committed. | `tools/provenance-check.ps1`; review |
| P-2 | No automatic translation, decompilation, or "extract then rewrite" pipeline for retail programs. | review; the build has no such stage |
| P-3 | Programs are authored from the C++ ABI, public ARB specifications, and independent implementation. | provenance record `inputs` field |
| P-4 | Inspecting an authorized local retail install is allowed only if recorded honestly (`retail_text_consulted: yes|no`). | provenance record |
| P-5 | Later id Tech 4 releases that ship HLSL (for example a BFG Edition source drop) are **not** present in this repository and must not be copied. If one is ever consulted, record it and confirm licence compatibility first. | review; record |
| P-6 | Every program has a provenance record before its first merge. | PR blocked without it |
| P-7 | Generated DXIL / SPIR-V / WGSL is a build artifact derived from owned HLSL; it is not committed unless a later decision says otherwise. | decision D-P3 |
| P-8 | Visual parity is evidence of behaviour, never evidence of provenance. | this file |
| P-9 | A provenance record that says "consulted: no" must be true; a false negative is worse than a delay. | review |
| P-10 | Screenshots, pak listings, and material names may be recorded; shader bodies and extracted archives may not. | runtime census procedure |

### 2.1 What "authored from the ABI" means

Allowed inputs:

- `neo/renderer/draw_arb2.cpp`, `neo/renderer/draw_common.cpp`, `neo/renderer/Material.cpp`,
  `neo/renderer/tr_local.h` — the interface, not the missing implementation.
- The public `ARB_vertex_program` and `ARB_fragment_program` specifications (external, grade 2).
- The project's own [`abi.md`](abi.md) and [`resolver.md`](resolver.md).
- Independent implementation from the math: normal mapping, cube reflection, stencil-volume
  projection, projective falloff.

Not allowed:

- Copying a `.vfp`/`.vp` file from an install into the repository, a test, or a prompt.
- Committing a "reference" translation "just for tests".
- Deriving a shader from a screenshot by automated means and calling it independent.

---

## 3. Provenance record template

Every authored program gets a record in the register (§4) with these fields:

```yaml
id: PV-001
key: interaction
stages: [vs, ps]
status: compiled           # planned | compiled | authored | reviewed | retired
source_class: authored     # authored only; no other class is distributable
authored_by: <agent or person>
date: <YYYY-MM-DD>
inputs:
  - specs/008-classic-shaders/abi.md
  - ARB_vertex_program (public specification)
  - ARB_fragment_program (public specification)
  - independent implementation
retail_text_consulted: no
derivative_work_review: not-required   # or: counsel-reviewed <date>
files:
  - shaders/classic/programs/interaction.hlsl
sha256: <hash of the authored file>
reviewer: <owner>
notes: ""
```

The ladder is ordered, and each step means something specific:

| Status | Means | Requires |
|---|---|---|
| `planned` | no file exists | an entry in the register |
| `compiled` | the file exists and compiles for both targets, but is not yet `owned` | a matching hash, DXIL + SPIR-V compile, and a stated list of unmet ownership criteria |
| `authored` | every criterion in [`ownership.md`](ownership.md) §5 is met | the resolver manifest entry is `owned` |
| `reviewed` | an independent reviewer signed the record | a named reviewer in the record |
| `retired` | the program was removed | the PR that removed it |

`compiled` was added during implementation: without it, a program that exists and compiles but is not
yet `owned` had no truthful status — calling it `planned` denied the file, and `authored` overstated
the review. The machine-readable records live in `shaders/manifest/provenance.json`, and
`shaders/tools/provenance-record.ps1` verifies every hash and the manifest-to-record mapping.

---

## 4. Register

| Id | Key | Stages | Status | Decision | Retail text consulted |
|---|---|---|---|---|---|
| PV-001 | `interaction` | vs, ps | compiled | author from ABI + ARB specs | no |
| PV-002 | `environment` | vs, ps | compiled | author from ABI + ARB specs | no |
| PV-003 | `bumpyenvironment` | vs, ps | compiled | author from ABI + ARB specs | no |
| PV-004 | `shadow` | vs | compiled | author from ABI + ARB specs | no |
| PV-005 | `arbfp_glasswarp` | ps | compiled | author from ABI + ARB specs | no |
| PV-006 | `test` | vs, ps | compiled | debug-only visualiser, not a parity program | no |
| PV-007 | `megatexture` | vs, ps | decided: unsupported | milestone 1 reports unsupported; no shader authored | no |
| PV-008 | `ambientlight`, `arbvp_glasswarp` | vs, ps / vs | decided: absent | dead engine entries; no implementation planned | no |
| PV-009 | `r200_interaction`, `nv20_bumpandlight`, `nv20_diffusecolor`, `nv20_specularcolor`, `nv20_diffuseandspecularcolor` | vs | decided: absent | backends scheduled for deletion; no implementation planned | no |

PV-008 and PV-009 are group records because nothing is authored for them; a per-file record would
imply work that does not exist.

The manifest fixture in [`tools/resolver-vectors.json`](tools/resolver-vectors.json) contains
synthetic keys (`one_sided`, `stubprogram`, `dupkey`) and synthetic `PV-Txx` provenance ids. They are
test data for the decision table and must never appear in the shipping manifest.

---

## 5. If retail text is ever consulted

The honest path, if a future decision allows it:

1. Obtain written authorization or a legal opinion first.
2. Record `retail_text_consulted: yes` on every affected record, with the date and the material
   examined.
3. Keep the study local; commit no text, no diff, no translation.
4. Require a derivative-work review before the authored program merges.
5. Prefer a clean-room split: one person/agent studies behaviour, another writes from the ABI and
   the behavioural notes, with the notes reviewed to ensure they contain no copied expression.

This is deliberately heavy. The cheap path — author from the ABI — is available and is the plan.

---

## 6. Decisions

| Id | Decision | Rationale | Revisit when |
|---|---|---|---|
| D-P1 | Author from the C++ ABI and public ARB specs; do not translate retail programs | avoids the derivative-work risk and does not need retail data | never, unless counsel says otherwise |
| D-P2 | Do not copy HLSL from any later id Tech 4 release | different ABI, different precision, additional terms; not present here | only with a licence review |
| D-P3 | Do not commit generated shader blobs | build artifacts, not source; avoids a second source of truth | if reproducible builds require pinned blobs |
| D-P4 | Record provenance per program, not per batch | a batch record hides which file was studied | if the record count becomes unmanageable |

---

## 7. Enforcement: `provenance-check.ps1`

The gate fails when a tracked file looks like a shader source or contains an ARB program body:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/provenance-check.ps1
```

Checks:

1. no tracked `*.vfp`, `*.vp`, `*.fp` file;
2. no tracked path under `glprogs/`;
3. no tracked text file containing `ARBvp1.0` or `ARBfp1.0` outside a short, exact allowlist: this
   file, the checker itself, and the existing `doom3_orig/doc/02_renderer.md` (which quotes the
   directive name in a table and contains no program body). Every other tracked file is flagged.

The check is intentionally blunt. A false positive is cheaper than a leaked retail shader.

---

## 8. Open items

1. **Legal review is outstanding.** No counsel has reviewed the derivative-work position.
2. **The runtime census is blocked on authorized data** ([`census.md`](census.md) §7), so the
   material-declared program set is unknown.
3. **Authoring has started and stopped at `compiled`.** All six programs exist, compile for both targets and are hashed; none is `owned`, because WGSL validation through Tint and parity capture are still missing (see `evidence/implementation.md`).
4. **The BFG reference question is unresolved and deliberately unexercised.** Nothing from it has
   been read for this work.
