# Q4 hero cells: the six authored cells (T-020…T-025)

Issue: #69. Date: 2026-10-30. This record covers the six cells that were missing after the
dielectric MVP. It is a content record only: **no capture and no budget exists**, so no image or
number below is a quality claim.

## 1. What was authored

| Cell | Material | Authored sources | Channels authored | Generator |
|---|---|---|---|---|
| opaque metal | `hero_metal` | 4 × 512² PNG | baseColor, normal, roughness, metalness | `content/tools/generators/metal.py` |
| alpha-tested | `hero_grating` | 4 × 512² PNG | baseColor (with alpha cutout), normal, roughness, metalness | `content/tools/generators/grating.py` |
| emissive | `hero_emissive_sign` | 5 × 512² PNG | baseColor, normal, roughness, metalness, emissive (SDR mask) | `content/tools/generators/emissive_sign.py` |
| skinned character | `hero_character_skin` | 3 × 512² PNG | baseColor, normal, roughness | `content/tools/generators/character_skin.py` |
| transmissive glass | `hero_glass` | 3 × 512² PNG | baseColor (with alpha), normal, roughness | `content/tools/generators/glass.py` |
| large environment | `hero_environment` | 4 × 2048² baseColor tiles + 3 × 2048² maps | baseColor (assembled 4096²), normal, roughness, metalness | `content/tools/generators/environment.py` |

The dielectric MVP already existed. Together the seven cells are 29 committed PNGs, all at or below
the C-01b 2048-px-per-axis cap, plus the seven existing `.mtr`/`.mat.json` pairs and the one dielectric
pair. Every new hero directory has a `LICENSE` (CC0 1.0) and a `provenance.json` with one record per
file, the generator path, generator version, generator SHA-256, runtime and
`retail_text_consulted: "no"`.

`content/tools/generators/hero_common.py` holds the shared deterministic PNG writer, tileable value
noise, bilinear sampling and height-to-normal conversion. It is named in every provenance record.

## 2. What each cell records, and what stays blocked

| Cell | Recorded state | Not claimed |
|---|---|---|
| opaque metal | metalness is authored; normal handedness is `opengl`; AO and emissive are `unavailable` with reasons | No capture; no statement that it reads as steel on the RHI path |
| alpha-tested | cutout is in baseColor alpha; the `.mtr` records `alphaTest 0.5`; engine-side alpha test stays with spec 004/008 | No shader change; no capture |
| emissive | emissive is authored as an SDR mask with bar and glow detail; `emissiveScale` is declared; `hdr` is `blocked` on the C-10 format decision | Radiance above 1.0 is not authored or encoded; the pinned BC7_UNORM_SRGB path clamps |
| skinned character | the surface material is authored; normal handedness is `opengl`; `skinnedPath` is `blocked` | No rigged mesh, skeleton, animation or skinned capture; those are outside the 009 material contract |
| transmissive glass | baseColor.alpha and roughness are authored; `transmission` records that opacity/refraction are spec 004's model | No transmission shader; no refraction; no capture |
| large environment | baseColor is a declared 4096² assembly of four 2048² tiles; the built package records `assembled: 4096×4096`, 13 mips; `residency` is `blocked` on T056 | No residency measurement; no budget number; no capture |

No numeric VRAM, residency, timing or streaming budget was written. `content/manifest/budget-rows.json`
still has seven `blocked:` rows.

## 3. Commands and results

All commands were run from the worktree root. Full output was kept bounded; the exit codes are the
evidence.

```text
powershell -NoProfile -File content/tools/validate.ps1 -SkipTracked
VALIDATE OK: 29 hero files, 7 sidecars, 7 Q4 materials, 1 packs (1 absent)
exit 0
```

```text
powershell -NoProfile -File tests/content/run-gates.ps1
GATES OK: 1 clean + 8 negative controls
exit 0
```

```text
powershell -NoProfile -File content/tools/get-encoder.ps1
ENCODER OK: texconv 2026.5.8.1, 966480 bytes, sha256 dcfdec...
exit 0
```

Each of the six materials was built with the pinned encoder into a git-ignored `out/` directory and
then verified:

```text
BUILD OK: hero_metal 4 textures
BUILD OK: hero_grating 4 textures
BUILD OK: hero_emissive_sign 5 textures
BUILD OK: hero_character_skin 3 textures
BUILD OK: hero_glass 3 textures
BUILD OK: hero_environment 4 textures

VERIFY OK: hero_metal 4 textures
VERIFY OK: hero_grating 4 textures
VERIFY OK: hero_emissive_sign 5 textures
VERIFY OK: hero_character_skin 3 textures
VERIFY OK: hero_glass 3 textures
VERIFY OK: hero_environment 4 textures
```

The environment package records the T-025 assembly:

```text
channel baseColor: BC7_UNORM_SRGB 4096x4096, 13 mips, assembled 4096x4096
sources: baseColor_x0_y0.png, baseColor_x1_y0.png, baseColor_x0_y1.png, baseColor_x1_y1.png
```

With the dielectric package also built into `out/content`, the complete matrix check passes:

```text
powershell -NoProfile -File content/tools/matrix-report.ps1 -RequireComplete
MATRIX: coverage 7/7 cells authored, 7/7 captures BLOCKED, budget rows 7/7 present
exit 0
```

Every generator was rerun into a separate directory and every committed PNG was compared by SHA-256:

```text
REPRO OK: all six generators reproduce every committed PNG byte-for-byte
```

## 4. Contract change made for T-025

`content/tools/validate.ps1` check 7 previously required every authored channel to have `path`, even
though `build-material.ps1` already supported `assembly` + `tiles`. The large-environment cell cannot
satisfy T-025 without that form: a single committed source is capped at 2048 px, so 4096² must be an
assembly. The validator now accepts either `path` or `assembly`, checks the assembly dimensions, tile
presence and tile existence, and still rejects a channel with neither. The builder continues to check
exact tile geometry. The eight negative controls still pass.

## 5. What is not claimed

- **Superseded 2026-10-30**: captures now exist — six frames covering seven cells and four categories
  under `specs/004-quality-renderer/evidence/t056/`, indexed in
  [`captures.md`](./captures.md#captures). Before that run this section said no capture and no budget
  row existed.
- No numeric budget exists in this spec, by design; the rows in
  `content/manifest/budget-rows.json` are now `recorded` for six cells against
  `specs/004-quality-renderer/evidence/t056/measurements.json`, and `skinned_character` stays `blocked`
  because it is never drawn.
- The skinned character surface has no rigged mesh or animation, so it has no frame at all.
- The emissive cell has no authored representation above 1.0 radiance; its captured frame is SDR.
- The glass cell has no transmission model; spec 004 owns it, and the captured panel is a tinted
  environment approximation.
- The large environment residency is now measured (33,554,512 committed texture bytes at a 256 MiB
  budget); no device VRAM peak was measured.
- The art review has not run. Sizes and dimensions are authoring facts, not quality verdicts.
