# T056 capture artifacts

Everything here was produced on 2026-10-30 from base `50efd1a` (the B1/B2 fix session) by
`build-q4-harness/bin/Release/q4_harness.exe`
(`6a2d50bb2e23670e3334b43cace3962dfa1a2bca86819bcd429030a5d9690748`) through
`content/tools/capture-q4.ps1`. The numbers, commands and hashes live in
[`measurements.json`](./measurements.json); the narrative is
[`../../evidence-q4.md`](../../evidence-q4.md) §5. The pre-fix run at `348487c` — the artifact that
`review-q4.md` judged — is still reachable at `50efd1a`; §5.7 records what was fixed and what was not.

## Frames

| File | Scene | Backend / adapter | SHA-256 |
|---|---|---|---|
| `01-static-opaque-dielectric-metal.png` | `static_opaque` (dielectric plane/cube/sphere + metal sphere) | d3d12 / Intel(R) Graphics | `0574ac6639…a136` |
| `02-alpha-tested-emissive.png` | `alpha_emissive` (cutout quad + emissive sign) | d3d12 / Intel(R) Graphics | `07d82c9a17…116b` |
| `03-moving-frame-1.png` | `moving`, frame 0 (cube 20°) | d3d12 / Intel(R) Graphics | `746aba6333…a9ef` |
| `04-moving-frame-8.png` | `moving`, frame 7 (cube 195°) | d3d12 / Intel(R) Graphics | `1cf980a10d…02c9` |
| `05-transmissive-environment.png` | `transmissive_environment` (environment plane + glass quad) | d3d12 / Intel(R) Graphics | `1aedf7ff3d…b08b` |
| `06-warp-static-opaque.png` | `static_opaque` | d3d12 / Microsoft Basic Render Driver | `46ac9241b9…cd61` |
| `07-warp-transmissive-environment.png` | `transmissive_environment` | d3d12 / Microsoft Basic Render Driver | `2ef8b5e93e…5c53` |

`05` and `07` are **byte-identical to the pre-fix record**. That is the isolation check for this
session: the two fixes touch the dielectric material and alpha-tested shadow casting, and nothing else
in those two frames changed. Frames `01`-`04` and `06` carry the dielectric and/or the cutout shadow,
so they changed.

`03` and `04` are the first and last of eight consecutive frames of the same scene; the per-pair
difference is recorded numerically in `measurements.json` (`motion.adjacent`) instead of committing
eight near-identical images. They are produced by `-Frames 1` and `-Frames 8` runs, which is what
`captures/hero_dielectric-moving-frame1.json` and `captures/hero_dielectric-moving.json` record.

## Crops

| File | Shows |
|---|---|
| `crops/grating-shadow-solid.png` | 2× nearest-neighbour crop of the **rejected** `02` (at `50efd1a`): the alpha-tested grating casting a solid parallelogram shadow. Kept because `review-q4.md` §5 cites it. |
| `crops/grating-shadow-cutout.png` | The same region at the same 2× scale from the current `02`: the shadow now carries the caster's hole pattern. |

## Tools

`tools/defect_metrics.py` and `tools/frame_diff.py` recompute the B1/B2 numbers and the frame
differences in `measurements.json` from these PNGs (plus, for the "rejected" columns, the same frames
from `50efd1a`). They are standard-library only, and each states the metric it applies, so the numbers
are checkable without rebuilding the harness or trusting this file:

```bash
git show 50efd1a:specs/004-quality-renderer/evidence/t056/02-alpha-tested-emissive.png > /tmp/old-02.png
py -3 specs/004-quality-renderer/evidence/t056/tools/defect_metrics.py --only b1 \
      --mask-from /tmp/old-02.png specs/004-quality-renderer/evidence/t056/02-alpha-tested-emissive.png
```

## Capture records

`captures/*.json` are verbatim `capture.json` files written by `capture-q4.ps1`. `image` and `log` in
those records are relative to `out/content/<material>/`, which is git-ignored; the committed frame is the
copy named in the table above (identical SHA-256, verified). `image_sha256` in each record therefore
matches the committed PNG of the same scene.

| Record | Material | Adapter | Committed frame |
|---|---|---|---|
| `hero_dielectric-static_opaque.json` | `hero_dielectric` | Intel(R) Graphics | `01-…` |
| `hero_metal-static_opaque.json` | `hero_metal` | Intel(R) Graphics | `01-…` |
| `hero_grating-alpha_emissive.json` | `hero_grating` | Intel(R) Graphics | `02-…` |
| `hero_emissive_sign-alpha_emissive.json` | `hero_emissive_sign` | Intel(R) Graphics | `02-…` |
| `hero_dielectric-moving-frame1.json` | `hero_dielectric` | Intel(R) Graphics | `03-…` (the `-Frames 1` capture) |
| `hero_dielectric-moving.json` | `hero_dielectric` | Intel(R) Graphics | `04-…` (the `-Frames 8` capture) |
| `hero_glass-transmissive_environment.json` | `hero_glass` | Intel(R) Graphics | `05-…` |
| `hero_environment-transmissive_environment.json` | `hero_environment` | Intel(R) Graphics | `05-…` |
| `hero_dielectric-static_opaque-warp.json` | `hero_dielectric` | Microsoft Basic Render Driver | `06-…` |
| `hero_environment-transmissive_environment-warp.json` | `hero_environment` | Microsoft Basic Render Driver | `07-…` |

There is no `skinned_character` record: the scene never draws that cell, so there is nothing to capture.
