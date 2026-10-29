# Implementation evidence: content toolchain

**Date**: 2026-10-29
**Spec**: [`../spec.md`](../spec.md), tasks T-001…T-015, T-026…T-032, T-040, T-042.
**Human sign-off**: recorded in [`../review.md`](../review.md) §6 before this round started.
**What this file is**: the commands that were run, their real output, and the decisions the documents did
not settle. Nothing here claims a Q4 capture exists.

---

## 1. What was built

| Area | Path |
|---|---|
| Encoder pin | `content/tools/pins.json`, `get-encoder.ps1` |
| Contract gate | `content/tools/validate.ps1` (ten checks), `content/tools/content-lib.ps1` (shared helpers) |
| Negative controls | `tests/content/run-gates.ps1` (one clean run + six mutations) |
| Normal conversion | `content/tools/convert-normal.ps1`, `tests/content/fixtures/make-golden.py`, `golden-rxgb.png`, `golden-linear.png` |
| Package builder | `content/tools/build-material.ps1`, `content/tools/requirements-content.txt` |
| Package verifier | `content/tools/verify-package.ps1` |
| Corpus tooling | `content/tools/scan-pack.ps1` (denylist + suffix inventory, names and hashes only) |
| Reporting | `content/tools/matrix-report.ps1`, `content/tools/capture-q4.ps1` |
| Content | `content/hero/dielectric/{baseColor,normal,roughness}.png` + `LICENSE` + `provenance.json`; `content/materials/hero_dielectric.{mtr,mat.json}`; `content/manifest/{content-manifest,q4-materials,budget-rows,pack-denylist}.json` |
| Generator | `content/tools/generators/dielectric.py` |
| Docs | `content/README.md`, [`captures.md`](captures.md) |

Build products live in `out/` and the encoder in `content/tools/.cache/`; both are git-ignored, so no
binary artefact is committed (the PNGs under `content/hero/` are authored sources, allowed by C-01b).

---

## 2. Encoder pin (T-002)

```powershell
powershell -NoProfile -File content/tools/get-encoder.ps1
```

```text
ENCODER OK: texconv 2026.5.8.1, 966480 bytes, sha256 dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06
```

Negative control: the pin's `sha256` was temporarily set to zeros and the script re-run.

```text
ERROR: encoder hash mismatch
  expected 0000000000000000000000000000000000000000000000000000000000000000
  actual   dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06
```

Non-zero exit, then the pin was restored and the success path re-verified.

---

## 3. Contract gate (T-003, T-004, T-005)

```powershell
powershell -NoProfile -File content/tools/validate.ps1
```

```text
denylist: byte origin is otherwise established by provenance and review (mechanical check only)
SKIPPED: pack 'wulfen_texture_pack_2.0.pk4' is absent
SKIPPED: corpus verification (no staged pack)
VALIDATE OK: 3 hero files, 1 sidecars, 1 Q4 materials, 1 packs (1 absent)
```

This is the **tracked-mode** run (`git ls-files` is consulted). An earlier version of the gate scanned
binary files as text in tracked mode and went red on the committed PNGs; that is fixed (binary
extensions are skipped, `.mtr` and `.ps1` are now scanned instead of excluded), and the output above is
from the fixed gate on the committed tree at the merge commit of this PR.

The gate runs ten checks: tracked packs, the `hero/**` extension allowlist, provenance records with
`id`, `inputs` and reproduced hashes, the CC0 1.0 licence file, the 2048 px cap plus square and
multiple-of-4, the pack denylist, sidecar schema, channel states and pinned formats, `.mtr` pairing
through the Q4 set (cell ids validated against C-12), the corpus manifest (a staged pack is hash- and
size-checked, not just found), and path/credential hygiene. A missing corpus is `SKIPPED`, never a pass
(FR-014).

### 3.1 Six negative controls (T-006)

```powershell
powershell -NoProfile -File tests/content/run-gates.ps1
```

```text
clean: ok: exit 0 -> VALIDATE OK
extension:      exit 1  content/hero/dielectric/stray.tga: extension not allowed (only .png and .exr under content/hero)
denylist:       exit 1  content/hero/dielectric/hero_dielectric.png: sha256 is on the pack denylist
provenance:     exit 1  content/hero/dielectric/hero_dielectric.png: no provenance record
licence:        exit 1  content/hero/dielectric/LICENSE: LICENSE does not declare CC0
channel-state:  exit 1  content/materials/hero_dielectric.mat.json: channel metalness has no state
mtr:            exit 1  content/materials/hero_dielectric.mat.json: mtr file does not exist: content/materials/does_not_exist.mtr
GATES OK: 1 clean + 6 negative controls
```

Extra controls run during implementation: a 2049×2049 PNG fails the size check; a fixture containing an
absolute Windows path and an `AKIA…` token fails hygiene with the pattern name only (the secret text is
never printed); a missing `content-manifest.json` exits 1; an absent `-ContentRoot` exits 2.

---

## 4. RXGB conversion (T-026)

The fixture is committed and generated, not captured: `tests/content/fixtures/make-golden.py` writes a
16×16 RXGB image and the expected linear result. The expected image is decoded from the **stored bytes**,
so the test measures the conversion, not 8-bit quantisation noise.

```powershell
py -3 tests/content/fixtures/make-golden.py
powershell -NoProfile -File content/tools/convert-normal.ps1 `
  -Source tests/content/fixtures/golden-rxgb.png -Output out/content-test/golden-linear.png `
  -Layout rxgb -Verify tests/content/fixtures/golden-linear.png -Tolerance 2
```

```text
CONVERTED: tests/content/fixtures/golden-rxgb.png (rxgb) -> out/content-test/golden-linear.png (16x16)
VERIFY OK: max channel error 0 <= 2 (layout rxgb)
```

Controls:

| Control | Result |
|---|---|
| wrong layout (`rgb` on an RXGB image) | `VERIFY FAILED: max channel error 217 > 2` |
| wrong expected image | `VERIFY FAILED: max channel error 255 > 2` |
| no `-Layout` argument | the parameter is mandatory; the script refuses to run |

**Caveat, stated rather than buried**: the `rxgb` arrangement (x in alpha, y in red, z reconstructed) is
the project's declared convention. It is **not** verified against an authorized retail sample; that is
spec open question 6 and it is why the converter names the layout explicitly instead of assuming it.

---

## 5. Build and verify one material (T-013, T-014)

```powershell
powershell -NoProfile -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content/hero_dielectric
powershell -NoProfile -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content-test/hero_dielectric-b
powershell -NoProfile -File content/tools/verify-package.ps1 -OutDir out/content/hero_dielectric -Compare out/content-test/hero_dielectric-b
```

```text
BUILD OK: hero_dielectric 3 textures -> out/content/hero_dielectric
BUILD OK: hero_dielectric 3 textures -> out/content-test/hero_dielectric-b
VERIFY OK: hero_dielectric 3 textures (deterministic against out/content-test/hero_dielectric-b)
```

The package records what was built and with what:

```text
material=hero_dielectric  package_sha256=8aaeb7328dd79887…  toolchain={"python":"3.14.4","pillow":"12.3.0","texconv":"2026.5.8.1"}
  baseColor  BC7_UNORM_SRGB  512x512  mips=10  srgb=True
  normal     BC5_UNORM       512x512  mips=10  srgb=False
  roughness  BC4_UNORM       512x512  mips=10  srgb=False
```

Mutation controls (each must fail, and does):

| Mutation | Diagnostic |
|---|---|
| flip one byte in `baseColor.dds` | `texture-sha256-mismatch: channel 'baseColor' recorded 236c2493…, 'baseColor.dds' hashes to 1683f9ef…` |
| change the recorded mip count | `texture-mips-mismatch` |
| change the recorded format | `texture-format-mismatch` |
| `-Compare` against a directory with one changed texture | `compare-texture-sha256-mismatch` |
| `-Compare` pointed at `-OutDir` | `compare-same-directory` (a package compared with itself is not a determinism test) |
| a normal channel without `handedness` | `channel-handedness-missing` |

The builder also refuses an out-dir inside a tracked path, a non-square source, a source whose axis is
not a multiple of 4, a material that is not in the Q4 set, and a sidecar whose `.mtr` disagrees — each
proven during implementation and each leaves nothing half-built.

---

## 6. Matrix, capture refusal and corpus tooling

```powershell
powershell -NoProfile -File content/tools/matrix-report.ps1
powershell -NoProfile -File content/tools/matrix-report.ps1 -RequireComplete
powershell -NoProfile -File content/tools/capture-q4.ps1 -Material hero_dielectric
```

```text
| Opaque dielectric | hero_dielectric | yes | built (3 textures) | BLOCKED: spec 004 has not reached Q4 | blocked: … |
| Opaque metal      | -               | NOT AUTHORED | NOT BUILT | BLOCKED: … | blocked: … |
… (six more rows, all NOT AUTHORED / BLOCKED)

MATRIX: coverage 1/7 cells authored, 7/7 captures BLOCKED, budget rows 7/7 present
MATRIX INCOMPLETE: 6 cell(s) not authored        (exit 1 with -RequireComplete)

BLOCKED: spec 004 has not reached Q4             (capture-q4.ps1, exit 3)
```

`scan-pack.ps1`:

```text
(no -Pack)   SKIPPED: no -Pack given; the corpus is not staged on this host (FR-014)
(synthetic)  PACK SCANNED: fake.pk4 (564 bytes), 4 files, 2 textures, 1 materials
             suffixes: d=1, s=1
             denylist: … (4 hashes)
```

No real corpus is staged on this host, so T-007's suffix inventory is proven against a synthetic pack
only. The real inventory stays `SKIPPED` until a pack is present.

---

## 7. Decisions taken while implementing

| Id | Decision | Why |
|---|---|---|
| I-1 | `-dx10` moved from the builder into `pins.json` `encoderFlags.common` | the verifier parses only a DX10 header, and "flags come from the pin" must be true rather than approximately true |
| I-2 | `LICENSE` and `provenance.json` are exempt from the `hero/**` extension allowlist | a literal allowlist would reject the two files the provenance and licence checks require |
| I-3 | The generator lives in `content/tools/generators/`, not beside the images | `hero/**` is for assets; a `.py` there would (correctly) fail the allowlist |
| I-4 | The golden expected image is derived from the stored bytes, not the float truth | otherwise the test measures 8-bit quantisation error and would need a tolerance so wide it stops catching an axis swap |
| I-5 | Pillow is pinned by version, not by wheel hash | wheel hashes differ per platform; the file says so, and every package records the version it used |
| I-6 | `package_sha256` is the sorted digest of sidecar, `.mtr` and texture hashes, with no paths or timestamps | paths and times would make determinism untestable |
| I-7 | The matrix report exits 0 when incomplete and 1 only with `-RequireComplete` | an incomplete matrix is a normal intermediate state; the acceptance check is a switch, not the default |
| I-8 | `capture-q4.ps1` refuses unconditionally today | a stage that silently skips becomes a stage that silently passes |

---

## 8. Not done, and why

| Item | State | Why |
|---|---|---|
| Six content cells (T-020…T-025) | not started | authoring work, not tooling; each needs its own art pass and review |
| Budget measurements (T-027) | partial | rows exist as `blocked: spec 004 has not reached Q4`; a number without a measurement would violate FR-012 |
| Independent review of this implementation (T-041) | not started | the author may not review their own PR (`docs/GIT_WORKFLOW.md`) |
| Q4 captures (T-050) | BLOCKED | no RHI path renders a PBR material |
| Retail texture study (T-051) | BLOCKED | no counsel review |
| Real corpus suffix inventory (T-007 evidence) | SKIPPED | no pack staged on this host |
| `rxgb` layout verified against authorized data | not done | open question 6; the converter declares the layout instead of assuming it |
