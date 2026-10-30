# `content/`: the Q4 hero content set and its pipeline

Spec: [`specs/009-rhi-hd-content/`](../specs/009-rhi-hd-content/). Boundary: [`boundary.md`](../specs/009-rhi-hd-content/boundary.md).

This directory holds **original, CC0-1.0** content and the tooling that turns it into GPU-ready
packages. It never holds game data, a third-party pack, or a build product.

## What may live here

| Allowed | Not allowed |
|---|---|
| Authored sources under `hero/<material>/`: `.png` (or `.exr`), ≤ 2048 px per axis, with `LICENSE` (CC0) and `provenance.json` recording each file's SHA-256 | Anything extracted from a `.pk4`, repacked from a fan pack, or copied from a retail install |
| Original text: `.mtr` material files, `.mat.json` PBR sidecars, JSON manifests | A staged pack, an extracted texture, a converted retail normal map |
| Tooling under `tools/`: gates, generators, the package builder | A build product; `out/` is git-ignored |

The gate enforces the mechanical half of that (`tools/validate.ps1`); review covers what no rule can
decide, because byte origin is not visible in pixels.

## The pipeline

```powershell
# 0. one-time: pin the encoder (verifies a SHA-256, fails closed)
powershell -NoProfile -File content/tools/get-encoder.ps1

# 1. contract gate: allowlist, provenance, sidecars, Q4 set, corpus manifest, hygiene
powershell -NoProfile -File content/tools/validate.ps1

# 2. optional: RXGB -> linear normal, verified against the committed fixture
powershell -NoProfile -File content/tools/convert-normal.ps1 `
  -Source tests/content/fixtures/golden-rxgb.png `
  -Output out/content-test/golden-linear.png `
  -Layout rxgb -Verify tests/content/fixtures/golden-linear.png -Tolerance 2

# 3. build one material twice, into separate directories
powershell -NoProfile -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content/hero_dielectric
powershell -NoProfile -File content/tools/build-material.ps1 -Material hero_dielectric -OutDir out/content-test/hero_dielectric-b

# 4. verify it, and prove the two builds are byte-identical (a same-directory compare is rejected)
powershell -NoProfile -File content/tools/verify-package.ps1 -OutDir out/content/hero_dielectric -Compare out/content-test/hero_dielectric-b

# 5. the seven-cell matrix
powershell -NoProfile -File content/tools/matrix-report.ps1

# 6. captures: refuses today, by design
powershell -NoProfile -File content/tools/capture-q4.ps1 -Material hero_dielectric   # exit 3, BLOCKED
```

## What a package is

`out/content/<material>/` holds one DDS per authored channel plus `package.json`:

- formats are fixed by `content/tools/pins.json` (`encoderFlags`), never by the builder;
- every texture records format, dimensions, mip count, sRGB flag and SHA-256;
- `package_sha256` is the SHA-256 of the sidecar, the `.mtr` and every texture hash in channel-name order, so a rebuild that changes
  nothing produces the same value, and a change is visible against a second independent build (comparing a package with itself is refused);
- the toolchain versions that produced it are recorded, so a Pillow upgrade is visible instead of silent.

## Authoring a new material

1. Generate or paint the sources into `content/hero/<material>/` (keep them ≤ 2048 px, square, multiple of 4).
2. Write `LICENSE` (CC0) and `provenance.json` (one record per file, `retail_text_consulted` answered).
3. Add `content/materials/<material>.mtr` (identity and stages) and `<material>.mat.json` (PBR channels).
   Every channel carries a state: `authored`, `derived:<method>` or `unavailable` with a reason.
4. Add the material to `content/manifest/q4-materials.json` — that list, not the presence of a `.mtr`,
   is what makes a sidecar mandatory.
5. Run the pipeline above.

All seven cells are generated, not painted. `tools/generators/dielectric.py` reproduces the dielectric
PNGs byte-for-byte, and `metal.py`, `grating.py`, `emissive_sign.py`, `character_skin.py`, `glass.py` and
`environment.py` do the same for the other six cells; `hero_common.py` holds the shared deterministic
helpers. The committed images therefore have an explanation rather than a mystery.

The six new cells are authored and build into packages, but **no capture and no budget exists**:
`matrix-report.ps1` reports 7/7 sidecars authored and 7/7 captures `BLOCKED`. The skinned-character
mesh/animation, the emissive HDR radiance above 1.0, the glass transmission model and the large
environment residency budget are recorded as dependencies or blocked in their sidecars, not invented.
See `specs/009-rhi-hd-content/evidence/hero-cells.md` for the per-cell record.

## Corpora are described, never committed

`manifest/content-manifest.json` records each pack's name, size, SHA-256, inventory counts, search order
and licence note. `D3_CONTENT_DIR` points at a local install; when a pack is present the gate verifies
its hash, and when it is absent the gate reports `SKIPPED` rather than failing or passing.

`tools/scan-pack.ps1 -Pack <path>` writes two artefacts from a locally staged pack — a hash denylist
(names and hashes only) and a suffix inventory — and reads no member into the repository.

## Captures: blocked today, testable today

`capture-q4.ps1` exits 3 with `BLOCKED: spec 004 has not reached Q4` when no build is supplied. There is
no RHI path that renders a PBR material yet, so there is nothing to capture.

The pass path is implemented and refuses to be satisfied by anything less than a real Q4 build. A build
qualifies only when it:

1. exits 0 for the arguments passed,
2. prints `RHI backend: <backend> (<adapter>)` matching the claimed values, and
3. writes a PNG larger than 64x64 with at least 16 distinct colours.

Then, and only then, the script copies the frame next to the package, writes `capture.json` (image hash,
executable hash, identity line, arguments, package hash) and prints
`CAPTURE OK: <material> <backend> <adapter>`. The matrix report reads that file, not the console string.

```powershell
powershell -NoProfile -File content/tools/capture-q4.ps1 -Material hero_dielectric `
  -Exe <q4 build>\DOOM3.exe -DataDir <data>\run-rhi `
  -Backend d3d12 -Adapter "<adapter name>" `
  -Screenshot out\content\hero_dielectric\frame.png `
  -EngineArgs "+rhi_capture;<path the build writes>"
```

Without a Q4 build the command stays at exit 3. The harness is still tested:
`tests/content/run-capture-harness.ps1` drives the script with a stub renderer
(`tests/content/fixtures/fake-rhi.cmd`) that breaks exactly one precondition at a time, and asserts that
a missing identity, a wrong adapter, a blank frame, a missing frame and a failed build are each refused
with a distinct exit code. The stub is not a renderer and its output is never evidence; the suite also
asserts that `specs/009-rhi-hd-content/evidence/captures.md` still records zero captures.
