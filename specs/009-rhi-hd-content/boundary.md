# Q4 content boundary: what the RHI-era HD content spec may consume, produce and claim

**Status**: boundary **decided** (2026-10-29). This is the input to `spec.md`, not the spec itself.
**Scope**: the S7 workstream — Q4 ("hybrid deferred opaque + forward/compatibility, HDR PBR, shadows,
indirect light/reflections/atmosphere, authored legal hero assets, budget/residency and moving-scene
art review", `specs/004-quality-renderer/plan.md:87`).
**Why this file exists before the spec**: Q4's content requirements are downstream of three decisions
that were never made together — what may legally exist in the repository, what the local corpus
actually is, and which backend defines "the RHI era". Writing the spec first would have baked in an
answer to each without anyone choosing it.

---

## 1. What actually exists (evidence, not assumption)

| Fact | Evidence |
|---|---|
| The legacy GL path already runs HD textures | `specs/006-hd-assets/tasks.md` T014–T017: `verify-hd-run.ps1` exit 0, **121 textures uploaded larger** (e.g. `textures/base_door/delelev1_s 256x256 -> 1024x1024`), baseline-vs-HD `DIFF_PERCENT=10.8636`, `RESULT=PASS` |
| The corpus is one third-party texture pack plus the free demo | Wulfen Texture Pack 2.0: 853,775,687 bytes, 933 entries, 906 files, **898 TGA** (874 `textures/`, 24 `models/`), **8 `.mtr`**, **0 `glprogs`/shader files**, sha256 `616774f6…`; base data is `demo00.pk4` only (`specs/006-hd-assets/tasks.md` T010–T013) |
| The corpus has **no** PBR input channels | the pack is TGA diffuse replacement plus 8 material files; there is no normal, roughness, metalness, AO or emissive map in the inventory |
| The repository may not carry assets | `AGENTS.md` non-negotiable 3; `docs/ROADMAP.md:17,42`; `.gitignore` blocks `.pk4/.pk3/.pak` |
| Material-only HD work is already scoped as a bounded track, not the modernization | `specs/003-render-modernization/spec.md:63` (D5) |
| a second pack (D3HDP) is referenced but has no record here | `specs/003-render-modernization/spec.md:65` (D7, "Proposed"); no manifest, hash or capture in this repository |
| The material pipeline is already specified as manifest + tooling only | `specs/003-render-modernization/spec.md:115-132` (US3): repo ships tooling, manifests and original `.mtr` text; RXGB must be preserved; VRAM/load time recorded |

### 1.1 The conflict that must not be papered over

`specs/003-render-modernization/spec.md:62` (D4, **Proposed**) says **Vulkan is the preferred native
backend, DX12 is a fallback, WebGPU deferred**. Spec 004 says the opposite in three places —
`specs/004-quality-renderer/spec.md:144` (FR-003: "a D3D12-first Windows backend"),
`specs/004-quality-renderer/plan.md:19` ("Windows 11 x64, D3D12 first"), and
`specs/004-quality-renderer/research.md:7` (decision D1). A *Proposed* decision and a *required*
requirement contradict each other on `main`; neither has been withdrawn.

Content cannot be format-agnostic if the backend is not decided: BC7 + BC5 is safe everywhere, but
container layout, streaming granularity and any vendor format are not. The boundary therefore fixes
only the portable subset and defers the rest to Q1 (see C-10).

---

## 2. Decisions

Each decision is binding on `spec.md`. A spec that contradicts one must amend this file in the same PR.

| Id | Decision | Why (evidence) | Enforcement |
|---|---|---|---|
| C-01a | **No third-party or retail asset bytes, ever.** No `.pk4/.pk3/.pak`, nothing extracted from an install, nothing repacked from a fan pack. | `AGENTS.md` rule 3; the GPL release conveys source only (`doom3_orig/doom3/README.txt:15`) | existing asset check plus the asset-class gate in C-02 |
| C-01b | **Original assets may be committed**, under `content/hero/`, only when they are CC0-1.0, carry a provenance record with a hash, and are small enough to review (2K per axis, one material per directory). This allowance exists because C-03 requires original hero assets; a blanket byte ban and an authorship requirement cannot both hold. The 2K cap applies to **committed source files**; a package texture larger than that must be assembled from several committed sources, and the package manifest records the assembly. | the boundary's own C-03 | the asset-class gate distinguishes `original` (allowed) from everything else (rejected), provenance hashes are re-checked, and a source file over 2K per axis is rejected |
| C-02 | **Run-time content is staged outside git from a user-owned install, described by a `content-manifest.json`**: pack name, byte size, SHA-256, entry/file/texture counts, search order, licence note, and the source install's identifying facts. Never the bytes. | `scripts/prepare-hd-data.ps1` already emits `hd-manifest.txt` with size/hash/order; 006 proved the pattern | a checked-in verifier fails when a manifest entry cannot be reproduced locally, and refuses to treat a missing pack as a pass |
| C-03 | **Q4's "authored legal hero assets" must be original work** — self-authored or CC0 — with a provenance record per asset. A third-party fan pack cannot satisfy the Q4 deliverable. | Q4's own wording ("authored legal hero assets"); the Wulfen pack is a third-party derivative texture set with 8 material-file overrides and no licence record in this repo | provenance record per hero asset, same posture as `specs/008-classic-shaders/provenance.md` |
| C-04 | **The Wulfen corpus is a local stress/parity corpus only.** It may be loaded by an RHI-path build to stress texture upload and material override, and it must never appear as Q4 acceptance content, as provenance evidence, or as a screenshot in a PR. | it has zero PBR channels, so it cannot demonstrate PBR; its licence status is unrecorded | any evidence file citing it must say `corpus: wulfen (stress only)` |
| C-05 | **PBR input contract**: `baseColor` (sRGB), `normal` (linear, explicit handedness), `roughness`, `metalness`, `AO`, `emissive`; a packed ORM texture is acceptable. A channel that does not exist is declared `unavailable` — never synthesised and then presented as authored. | Q4 requires HDR PBR; silently generating roughness from luminance would be a fake authoring claim | the manifest records a per-channel state: `authored` / `derived:<method>` / `unavailable` |
| C-06 | **Legacy RXGB normals are not RHI input.** Preserving RXGB is a 006/003-legacy requirement; the RHI path needs a recorded conversion (channel order, handedness, reconstruction of Z) plus a visual check on a known surface. | `specs/003-render-modernization/spec.md:115-132` (US3) explicitly preserves RXGB for the legacy path; the two conventions cannot both be "the" normal map | conversion is a named tool with a golden surface and a before/after capture |
| C-07 | **Formats**: BC7 colour, BC5 two-channel normal, BC4 single channel, full mip chains, sRGB/linear tagging recorded. DXT/BC1–3 remains valid for the legacy path only. | portable across the backends still in play (C-10); matches 003 D7's "DXT path, not BC7-only" for the legacy track while the RHI track gets modern formats | the manifest records format and mip count per texture; the verifier fails on a missing mip chain |
| C-08 | **No numeric budget is written here.** VRAM, residency and streaming budgets are *recorded* from Q4 measurements (`specs/004-quality-renderer/plan.md:87`, T056), not guessed. The 2K cap in C-01b is not a budget: it is an authoring constraint on what may be committed for review, and it says nothing about runtime memory. | 004 is explicit that budgets come from measurement | `spec.md` must cite a measurement task, not a number; the validator enforces the 2K cap, nothing enforces a VRAM number |
| C-09 | **The 006 legacy gate is not evidence for 009.** Different renderer, `-HdTextures` forces the TGA path and disables downsampling, and there are no PBR maps. | 006's own clarification: `image_usePrecompressedTextures 0` was required precisely because the demo ships DDS | 009 produces its own RHI-path captures; citing a 006 capture is a review failure |
| C-10 | **Only the portable subset is fixed now.** Content requirements that depend on the backend (container, streaming granularity, vendor formats such as BC6H/DDS-specific layouts) are deferred to Q1's backend decision. | the 003 D4 vs 004 conflict in §1.1 | `spec.md` may not make a backend-specific format an acceptance criterion |
| C-11 | **No retail-derived bytes, ever.** Studying an authorized local retail texture to derive a hero asset requires the same counsel gate that blocks `specs/008-classic-shaders` T-032, and the study itself may not be committed. | 008 P-1…P-5; the derivative-work position is unresolved | a provenance record with `retail_text_consulted` answered; a blocked gate reported as blocked |
| C-12 | **Q4 content coverage is a feature matrix, not an asset count.** The matrix Q4 must demonstrate: (a) opaque dielectric, (b) opaque metal, (c) alpha-tested geometry (foliage/grating), (d) emissive, (e) skinned character, (f) transmissive/glass, (g) one large environment surface with a residency budget row. Names and counts are set in `spec.md`. | Q4 must show PBR, shadows, GI/reflections and residency; a single hero prop cannot exercise any of those | each matrix cell needs a legal source, a budget row, and one capture |
| C-13 | **Material identity stays `.mtr`; PBR lives in a sidecar.** A material's identity, stage composition and program keys remain `.mtr`, so 008's program keys and retail/mod materials keep their meaning. PBR channels are described in `content/materials/<name>.mat.json`, compiled by a tool into a GPU-ready material package plus a package manifest. A glTF-class description may later be an *import* into the sidecar format, never the runtime contract. | no renderer material language exists; inventing one inside a content spec would predetermine spec 004's design | the build fails on a sidecar without a matching `.mtr`, and on a channel without a declared state |
| C-14 | **D3D12 is the acceptance backend for Q4 content until a decision PR reconciles 003 D4 with 004.** Content stays within the portable subset (C-07), so a later Vulkan ratification re-encodes containers without re-authoring art. Every acceptance capture names its backend and adapter. | D3D12 is the only *engine* backend with an implementation (`src/renderer/rhi/rhi.h`, `src/renderer/d3d12/rhi_d3d12.cpp`, Q1a merged and rerun 7/7). Vulkan has zero source lines here. WebGPU has a browser conformance spike (`tests/webgpu/`, 12/12 on Chrome 156 / Intel Xe-LPG, `specs/004-quality-renderer/evidence-q7a.md`) but no engine toolchain: `emcc`/`emsdk`/Dawn are absent and Q7a's Emscripten encoder leg is NOT RUN, so Q2 stays blocked. | an evidence file that does not name backend + adapter is rejected at review |

---

## 3. What Q4 may claim, once the spec exists

| Claim | Allowed when |
|---|---|
| "The RHI path renders PBR content" | every matrix cell in C-12 has a capture on the RHI path, with the backend named |
| "Content stays within budget" | a measurement exists for VRAM and load time and the comparison is to a recorded baseline, not a guess |
| "Hero assets are legal and authored" | each has a provenance record with a hash, an author, and `retail_text_consulted` answered |
| "The legacy pack also works" | only as a stress statement, labelled per C-04; it is not a PBR claim |
| "Parity with the classic renderer" | **not** — Q4's exit is a modern look, and `004` states modern look is not pixel parity |

---

## 4. Deferred, with the reason

| Item | Deferred because |
|---|---|
| Numeric budgets | no Q4 measurements exist (C-08) |
| Streaming granularity / virtual texturing | depends on residency design that does not exist; `megaTexture` is already `unsupported` in 008 |
| Vendor formats (BC6H, platform containers) | backend undecided (C-10) |
| Any retail texture study | counsel gate (C-11) |
| Animating foliage / destruction | outside the Q4 matrix; would need its own feature row |
| Authoring DCC pipeline (Blender/Maya export) | the hero set may be produced by any tool; the *contract* is the output manifest, not the tool |

---

## 5. The three blocking questions, answered

| Id | Question | Answer | Reversal cost |
|---|---|---|---|
| Q1 | Which backend defines "RHI era" for Q4 content acceptance — Vulkan (003 D4) or D3D12 (004)? | **D3D12 for acceptance** (C-14). It is the only backend with code: `src/renderer/rhi/rhi.h` plus `src/renderer/d3d12/rhi_d3d12.cpp`, Q1a merged and reviewed. Vulkan has zero source lines here and 003 D4 is still `Proposed`; WebGPU has no toolchain (`emcc` absent). Content stays portable (C-07), so this decision scopes *evidence*, not art. | re-encode containers and re-capture; no re-authoring. Kept small on purpose. A decision PR must still reconcile 003 D4 with 004 — 009 does not silently overrule 003. |
| Q3 | Who authors the hero assets, under which licence? | **This workstream authors them; licence CC0-1.0**, with a provenance record per asset and a `LICENSE` file per hero directory. The repository owner ratifies the licence in the PR that adds the first asset. No third-party pack may be promoted to hero status (C-04). | deleting committed hero assets; no downstream dependency, because C-02 keeps run-time packs separate from authored sources. |
| Q5 | `.mtr` + sidecar maps, or a glTF-class material description? | **`.mtr` keeps material identity; PBR lives in a `.mat.json` sidecar** (C-13), compiled to a GPU-ready package. glTF-class may later be an importer, not the runtime contract. | if 004 later adopts a different material language, the sidecars are mechanical input to a converter; the `.mtr` identity is what survives either way. |

These three answers also forced two amendments to §2: C-01 split into C-01a/C-01b, because C-03
(original hero assets) and a blanket byte ban cannot both hold; and C-13/C-14 were added to carry the
Q1 and Q5 answers.

### 5.1 Still open, and what each one changes

| Id | Question | Owner | Consequence if it stays open |
|---|---|---|---|
| Q2 | Does a local D3HDP corpus exist, and does it carry normal/specular maps? 003 D7 mentions it; this repository has no record. | content owner | no second stress corpus; the matrix is still satisfiable with authored assets, so this is a convenience, not a blocker |
| Q4 | One content pack with two material resolutions, or two packs (legacy and RHI)? | renderer + content owner | C-06's conversion runs at build time until decided; the spec records it as a design input, not an acceptance criterion |
| Q6 | Does Q4 need retail data at all, given hero assets are authored? | user | if yes, the Q4 gate inherits 008's T-031/T-032 blockers; until then 009 assumes **no** retail dependency |

---

## 6. Delivered

| Artifact | What it carries |
|---|---|
| [`spec.md`](spec.md) | 16 requirements, 4 user stories, 10 success criteria, all bound to C-01a…C-14 |
| [`plan.md`](plan.md) | pipeline stages, the pinned encoder (verified: texconv 2026.5.8.1, sha256 recorded), phase order, risks |
| [`tasks.md`](tasks.md) | 29 tasks with evidence columns: 27 not started, 2 blocked (Q4 captures, retail study) |

What is deliberately **not** delivered: any pipeline script, any asset, any capture. The specification is
complete; implementation starts at `tasks.md` T-001.

This file deliberately contains no renderer design. Q4's renderer work is `specs/004`; this workstream
owns only what content may exist and how it is proven.
