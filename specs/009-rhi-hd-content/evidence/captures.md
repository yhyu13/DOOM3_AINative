# Capture index: what exists, and what is blocked

**Date**: 2026-10-30 (first real captures; the 2026-10-29 revision recorded zero)
**Spec**: [`../spec.md`](../spec.md) FR-011, FR-013, FR-015 (task T-031)

Every Q4 content claim needs a capture that names its backend and adapter, or an explicit `BLOCKED`
reason. This file is the index of those claims. It is deliberately short.

## Captures

The frames live under
[`specs/004-quality-renderer/evidence/t056/`](../../004-quality-renderer/evidence/t056/) with the
measurement record in `measurements.json`; the harness that produced them is described in
[`evidence-q4-harness.md`](../../004-quality-renderer/evidence-q4-harness.md).

| Cell | Material | Capture | Backend | Adapter | Reason |
|---|---|---|---|---|---|
| opaque dielectric | `hero_dielectric` | `01-static-opaque-dielectric-metal.png` | d3d12 | Intel(R) Graphics | static opaque scene (§5.2) |
| opaque metal | `hero_metal` | `01-static-opaque-dielectric-metal.png` | d3d12 | Intel(R) Graphics | same frame; the metal sphere is the second cell |
| alpha-tested geometry | `hero_grating` | `02-alpha-tested-emissive.png` | d3d12 | Intel(R) Graphics | cutout works; **the cast shadow is solid** (no alpha test in the shadow pass) |
| emissive | `hero_emissive_sign` | `02-alpha-tested-emissive.png` | d3d12 | Intel(R) Graphics | SDR emissive; radiance above 1.0 is not represented |
| skinned character | `hero_character_skin` | **BLOCKED** | — | — | no skinned mesh, rig or animation exists in the harness or the content set, so the cell is loaded and accounted but never drawn |
| transmissive / glass | `hero_glass` | `05-transmissive-environment.png` | d3d12 | Intel(R) Graphics | tinted environment approximation, **not** transmission or refraction |
| large environment surface | `hero_environment` | `05-transmissive-environment.png` | d3d12 | Intel(R) Graphics | 4096² assembled atlas, 33,554,512 committed texture bytes |

**Captures on file: 6 distinct frames covering 7 cells and 4 categories**, plus a WARP parity frame for
`static_opaque` (`06-warp-static-opaque.png`, `Microsoft Basic Render Driver`) and for
`transmissive_environment` (`07-warp-transmissive-environment.png`). Every capture names its backend and
adapter, and each has a `capture.json` under `evidence/t056/captures/`. A capture is not an art verdict:
the independent review is T057 and has not run.

## Separation from the legacy track (FR-015)

Spec 006's HD result is a **legacy OpenGL** result and is not Q4 evidence. Two greps prove the content
workstream did not inherit it:

```text
$ git grep -n "HdTextures" -- content
(no output: no script or asset under content/ depends on the legacy switch)

$ git grep -n "image_usePrecompressedTextures" -- content
(no output: no pipeline step forces the legacy texture path)

$ git grep -n "HdTextures" -- specs/009-rhi-hd-content/evidence
specs/009-rhi-hd-content/evidence/captures.md: <this file, describing the rule>
```

The prose documents the rule; the commands prove the pipeline does not depend on it. Earlier versions of
this file quoted a grep over `specs/` as well, which matched the prose itself and proved nothing.

## What would change this file

1. A `skinned_character` frame becomes possible only when the harness (or the engine) has a rigged mesh
   and animation; until then the row stays `BLOCKED`.
2. The glass row changes when spec 004 delivers a transmission model; the current panel writes
   `alpha = 1` with a tinted environment term.
3. The zero-capture guard in `tests/content/run-capture-harness.ps1` now asserts that the **stub**
   adapter never appears here (the stub is not evidence), instead of asserting that the file says zero.
