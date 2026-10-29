# Implementation plan: RHI-era HD content pipeline and Q4 content set

**Branch**: `docs/38-rhi-hd-content-boundary` | **Date**: 2026-10-29 | **Spec**: [`spec.md`](spec.md)
**Input**: `spec.md`, bound by [`boundary.md`](boundary.md) (C-01a…C-14).

---

## 1. Summary

Build the content contract and the pipeline that enforces it, then author one hero material end to end,
then the seven-cell matrix. The pipeline must be provable **today**, while every step that needs the Q4
renderer stays `BLOCKED` and says so.

The technical approach is deliberately small:

- **PowerShell 5.1** for orchestration, validation and manifest manipulation (the repository's language
  for gates; no build system needed).
- **Python 3.14 (`py -3`) + Pillow 12.3.0** for channel assembly and pixel assertions.
- **texconv 2026.5.8.1** (DirectXTex, pinned by SHA-256) for BC7/BC5/BC4 encoding and mip generation.
- **JSON manifests** as the only machine-readable contract; packages and caches stay out of git.

No renderer code, no shader code, no engine edit. This workstream never writes outside `content/`,
`tests/content/` and `specs/009-rhi-hd-content/`.

---

## 2. Technical context

| Field | Value |
|---|---|
| Language/Version | PowerShell 5.1 (Windows built-in); Python 3.14.4 via `py -3` |
| Primary dependencies | `texconv` (DirectXTex `may2026`, 2026.5.8.1, 966,480 B, sha256 `DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06`); Pillow 12.3.0 |
| Storage | tracked: JSON manifests, `.mtr` text, PNG/EXR sources ≤ 2K per axis. Untracked: `content/tools/.cache/`, built packages, staged corpora |
| Testing | gate scripts with negative controls (repository precedent: `specs/008-classic-shaders/tools/`); no test framework |
| Target platform | Windows 11 x64 only (the only target that builds) |
| Performance goals | none. No VRAM or timing number may appear here (C-08); each budget row links to a spec-004 measurement task |
| Constraints | CC0-1.0 hero assets only; no third-party bytes; BC7/BC5/BC4 + full mips; `.mtr` identity + `.mat.json` sidecar; 2K per axis; seven matrix cells |
| Scale | 7 cells, one material per cell to start, ≤ 10 textures per material |

### 2.1 Pinned encoder, verified

```powershell
gh release download may2026 --repo microsoft/DirectXTex --pattern "texconv.exe" --dir content/tools/.cache --clobber
Get-FileHash content/tools/.cache/texconv.exe -Algorithm SHA256
```

```text
966480 bytes
DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06
Microsoft (R) DirectX Texture Converter [DirectXTex] Version 2026.5.8.1
```

A probe on 2026-10-29 encoded an 8×8 PNG with `-f BC7_UNORM_SRGB -m 0 -srgb -y` and produced a
260-byte DDS with a DX10 extended header, i.e. the flags the builder will use are the flags that work.
The download is not committed: `.cache/` is git-ignored, and the pin is the hash.

---

## 3. Constitution check

| Rule (AGENTS.md) | How this plan complies |
|---|---|
| 1. Pristine submodule read-only | no engine file is read for writing; the pipeline never touches `doom3_orig/doom3/` |
| 2. One copy per target | no target copy is created or edited |
| 3. Never commit game data or build output | C-01a; validator rejects third-party bytes; packages and caches are git-ignored |
| 4. No direct commits to `main` | work lands on a branch + PR |
| 5. Evidence before victory | every task carries a command; capture tasks are `BLOCKED`, never "pass" |
| 6. No secrets or machine paths | manifests store hashes and relative paths only (FR-016) |
| 7. One writer per file | the gates are separate scripts; the boundary, spec, plan and tasks are edited in one workstream |
| 9. Start from `main` | branch off `main`; the 008 work must land first (see §7) |

---

## 4. Project structure

```text
content/
  hero/
    <cell>/                      # one directory per matrix cell
      LICENSE                    # CC0-1.0 text, required (C-01b)
      provenance.json            # AssetRecord: hash, author, retail_text_consulted
      *.png                      # authored source, <= 2K per axis
  materials/
    <name>.mtr                   # material identity, stages, program keys (C-13)
    <name>.mat.json              # PBR sidecar: channel states + paths
  manifest/
    content-manifest.json        # staged corpora (never the bytes) + search order
    q4-materials.json            # the Q4 set: which materials require a sidecar (FR-005)
  tools/
    validate.ps1                 # contract gate (FR-001..FR-016)
    convert-normal.ps1           # RXGB -> linear normal, golden surface (FR-008)
    build-material.ps1           # sidecar + mtr -> package + package manifest
    verify-package.ps1           # re-hash, format/mip/sRGB, determinism
    matrix-report.ps1            # seven-cell table with states (FR-010..FR-012)
    capture-q4.ps1               # BLOCKED until spec 004 reaches Q4 (FR-013)
    .cache/                      # texconv, git-ignored
tests/content/
  fixtures/                      # tiny synthetic PNGs, a fake mtr, a broken sidecar
  run-gates.ps1                  # positive + negative controls
specs/009-rhi-hd-content/
  boundary.md  spec.md  plan.md  tasks.md  evidence/
```

Rationale for `content/tools/` rather than `scripts/`: spec 008 puts its tooling beside the tree it
builds (`shaders/tools/`), and these scripts must not be mistaken for the general workflow helpers in
`scripts/`.

---

## 5. Pipeline

| Stage | Script | Reads | Writes | Fails when |
|---|---|---|---|---|
| 1. Contract | `validate.ps1` | tracked files, `content/**`, `git ls-files` | nothing | third-party byte, hero without CC0+provenance+hash, sidecar without `.mtr`, channel without a state, tracked pack |
| 2. Normal conversion | `convert-normal.ps1` | a legacy RXGB image + a handedness flag | linear normal PNG | handedness unstated; golden-surface diff exceeds the recorded tolerance |
| 3. Build | `build-material.ps1` | `.mtr`, `.mat.json`, sources | BC7/BC5/BC4 DDS + `package.json` | missing channel state, non-2K-power dimension, texconv hash mismatch, encoder failure |
| 4. Verify | `verify-package.ps1` | `package.json` + DDS files | report | hash mismatch, wrong format, missing mip, wrong sRGB flag, non-deterministic second build |
| 5. Matrix | `matrix-report.ps1` | packages + sidecars + budget links | table | a cell without source/package/budget row |
| 6. Capture | `capture-q4.ps1` | a built package + a Q4 build | capture + evidence line | **always**, today: exits 3 with `BLOCKED: spec 004 has not reached Q4` |

Stage 6 is the only one that may not succeed, and it must fail loudly rather than silently skip. That is
the mechanism that keeps this spec honest: the pipeline can be complete while Q4 is absent.

### 5.1 Channel states

Each channel is `authored`, `derived:<method>` or `unavailable` (C-05). The builder maps:

| Channel | Format | sRGB | Source |
|---|---|---|---|
| baseColor | BC7 | yes | authored albedo |
| normal | BC5 | no | authored tangent-space normal, or `convert-normal.ps1` output |
| roughness | BC4 | no | authored, or `derived:invert-gloss` |
| metalness | BC4 | no | authored, or `unavailable` |
| AO | BC4 | no | authored, or `unavailable` |
| emissive | BC7 | yes | authored, or `unavailable` with the material marked non-emissive |

A material whose metalness is `unavailable` is still valid; a material whose metalness is *absent without
a state* is not.

---

## 6. Phases

| Phase | Content | Exit |
|---|---|---|
| P1 Contract | validator, manifest schema, sidecar schema, asset classification, negative controls | `validate.ps1` exit 0 on a clean tree; 5 negative controls exit 1 |
| P2 One material (US1) | one dielectric hero: source → sidecar → package → verify → capture attempt | package reproducibly built twice; `capture-q4.ps1` exits 3 with `BLOCKED` |
| P3 Matrix (US2) | remaining six cells; matrix report; budget-row citations | all seven cells resolve; report has no gap |
| P4 Reproduction (US3, US4) | clean-checkout run, `SKIPPED` semantics, evidence index, legacy separation | second run from clean clone reports the same states |
| P5 Review | self-review, independent review, evidence file | findings fixed; evidence cites commands |

---

## 7. Ordering and dependencies

1. **The 008 work must land first.** It is uncommitted in the `main` worktree; 009 must not be mixed into
   that PR. Land 008 on its own branch, then branch 009 from the updated `main`.
2. P1 depends on nothing but the repository. P2 depends on P1. P3 depends on P2. P4 depends on P2 and P3.
3. Stage 6 depends entirely on spec 004 reaching Q4 (which itself depends on Q2/Q3). Nothing in 009 can
   accelerate that, and nothing in 009 should pretend to.
4. The RXGB converter (P1) can be validated against the 006 corpus **locally** — it is a conversion tool
   test, not a Q4 capture, and the corpus stays outside git (C-04).

---

## 8. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Capture stage is blocked for a long time | authored content has no visual proof | the contract, pipeline and matrix report are independently useful; art review can inspect sources |
| texconv output differs across machines | package hashes drift | pinned binary + hash; determinism is part of `verify-package.ps1` |
| Pillow version drift changes channel assembly | silent visual change | record the Pillow version in the package manifest; fail on a mismatch |
| Hero quality is judged before the renderer exists | rework | US1 first, art review on source images, not on captures |
| `content/` grows into an asset dump | repository bloat | C-01b caps size and requires provenance; the validator fails a hero file without a record |
| Someone "saves time" by copying a 006 capture into 009 evidence | a false Q4 claim | SC-010, plus `capture-q4.ps1` being the only writer of capture evidence |

---

## 9. Verification

```powershell
# contract (must pass on a clean checkout)
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/validate.ps1

# negative controls (each must fail)
powershell -NoProfile -ExecutionPolicy Bypass -File tests/content/run-gates.ps1

# one material, twice, for determinism
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content-a
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content-b
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/verify-package.ps1 -OutDir out/content-a -Compare out/content-b

# matrix
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/matrix-report.ps1

# captures: expected to refuse today
powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/capture-q4.ps1 -Material hero_dielectric
# -> exit 3, "BLOCKED: spec 004 has not reached Q4"
```

Evidence for every command goes to `specs/009-rhi-hd-content/evidence/implementation.md` with real exit
codes, in the same style as `specs/008-classic-shaders/evidence/implementation.md`.

---

## 10. Complexity tracking

| Complexity | Why it is justified | Simpler alternative rejected |
|---|---|---|
| A sidecar format beside `.mtr` | C-13 keeps material identity while adding PBR; the engine has no PBR material language | extending `.mtr` syntax would fork the parser that spec 008 just specified |
| A BLOCKED capture stage | the honest state is "no renderer", and a silent skip would become a pass | skipping when the renderer is absent hides progress from review |
| Seven cells rather than one showpiece | Q4 must exercise PBR, alpha test, skinning, transmission and residency | one hero prop proves none of them |
| Pinned external encoder | BC7 cannot be produced by Pillow; a hand-rolled encoder would be unreviewable | recommit pre-encoded DDS, which hides the source and breaks reproducibility |
