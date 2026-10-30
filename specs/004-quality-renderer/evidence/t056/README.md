# T056 capture artifacts

Everything here was produced on 2026-10-30 from base `348487c` by
`build-q4-harness/bin/Release/q4_harness.exe`
(`562a20d68b739c201da261da4f5988e1fd6168a429a4f7f211bb20a41289eb69`) through
`content/tools/capture-q4.ps1`. The numbers, commands and hashes live in
[`measurements.json`](./measurements.json); the narrative is
[`../../evidence-q4.md`](../../evidence-q4.md) §5.

## Frames

| File | Scene | Backend / adapter | SHA-256 |
|---|---|---|---|
| `01-static-opaque-dielectric-metal.png` | `static_opaque` (dielectric plane/cube/sphere + metal sphere) | d3d12 / Intel(R) Graphics | `1c304e1633bd…8102` |
| `02-alpha-tested-emissive.png` | `alpha_emissive` (cutout quad + emissive sign) | d3d12 / Intel(R) Graphics | `819dd294da2d…7186` |
| `03-moving-frame-1.png` | `moving`, frame 0 (cube 20°) | d3d12 / Intel(R) Graphics | `abe8f482a375…5537` |
| `04-moving-frame-8.png` | `moving`, frame 7 (cube 195°) | d3d12 / Intel(R) Graphics | `5d5dd3c9118b…fbb4` |
| `05-transmissive-environment.png` | `transmissive_environment` (environment plane + glass quad) | d3d12 / Intel(R) Graphics | `1aedf7ff3ddc…b08b` |
| `06-warp-static-opaque.png` | `static_opaque` | d3d12 / Microsoft Basic Render Driver | `36db9e832c59…ad79` |
| `07-warp-transmissive-environment.png` | `transmissive_environment` | d3d12 / Microsoft Basic Render Driver | `2ef8b5e93e64…5c53` |

`03` and `04` are the first and last of eight consecutive frames of the same process; they are committed
instead of eight near-identical images because the per-pair difference is recorded numerically in
`measurements.json` (`motion.adjacent`).

`crops/grating-shadow-solid.png` is a 2× nearest-neighbour crop of `02` and exists only to show the
alpha-tested grating casting a **solid rectangular** shadow — the rejected example in
`evidence-q4.md` §5.4.

## Capture records

`captures/*.json` are verbatim `capture.json` files written by `capture-q4.ps1`. `image` and `log` in
those records are relative to `out/content/<material>/`, which is git-ignored; the committed frame is the
copy named in the table above (identical SHA-256, verified). `image_sha256` in each record therefore
matches the committed PNG of the same scene.

| Record | Material | Adapter | Committed frame |
|---|---|---|---|
| `hero_dielectric-static_opaque.json` | `hero_dielectric` | Intel(R) Graphics | `01-…` |
| `hero_metal-static_opaque.json` | `hero_metal` | Intel(R) Graphics | `01-…` |
| `hero_dielectric-moving.json` | `hero_dielectric` | Intel(R) Graphics | `04-…` (the `-Frames 8` capture) |
| `hero_grating-alpha_emissive.json` | `hero_grating` | Intel(R) Graphics | `02-…` |
| `hero_emissive_sign-alpha_emissive.json` | `hero_emissive_sign` | Intel(R) Graphics | `02-…` |
| `hero_glass-transmissive_environment.json` | `hero_glass` | Intel(R) Graphics | `05-…` |
| `hero_environment-transmissive_environment.json` | `hero_environment` | Intel(R) Graphics | `05-…` |
| `hero_dielectric-static_opaque-warp.json` | `hero_dielectric` | Microsoft Basic Render Driver | `06-…` |
| `hero_environment-transmissive_environment-warp.json` | `hero_environment` | Microsoft Basic Render Driver | `07-…` |

There is no `skinned_character` record: the scene never draws that cell, so there is nothing to capture.
