# targets/win11-amd64-hd

Native Windows 11 x64 build of the **2011 OpenGL DOOM 3 engine** with HD textures supplied at
runtime by the Wulfen Texture Pack 2.0. The engine tree is a git-ignored copy (`doom3/`, created
by `scripts/new-target.ps1`); this directory and the patch files are tracked.

## No new RHI

This target does **not** contain or use the project's planned D3D12 RHI. The source is the
pristine GPL release at submodule `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` (2011-11-22), and
the renderer is the classic ARB/ARB2 OpenGL path (`neo/renderer`, `r_renderer = best|arb|arb2|...`).
A search for `RHI`, `Vulkan`, `D3D12`, `ID3D12`, or `IDirect3DDevice` under `neo/` returns zero
matches. The planned RHI exists only as design documentation in `specs/003-render-modernization`
and `specs/004-quality-renderer`.

HD assets are therefore a **data overlay**, not a renderer change: the Wulfen pk4 is loaded after
`demo00.pk4` and overrides textures by the engine's normal pk4 search order.

## Layout

| Path | Tracked | Purpose |
|---|---|---|
| `CMakeLists.txt`, `sources.cmake` | yes | copied from `targets/win11-amd64`; same TUs, same static CRT, same x64 flags |
| `src/eaxguid_x64.cpp` | yes | target-owned x64 shim required by the build |
| `patches/all-changes.patch`, `patches/all-changes.manifest.txt` | yes | the shared 66-file x64 port patch |
| `doom3/` | no | scratch source copy, created and patched at configure time |

## Configure and build

From the repository root:

```powershell
cmake --preset win11-amd64-hd
cmake --build --preset win11-amd64-hd-release
```

Output: `build-hd/bin/Release/DOOM3.exe` and `DOOM3XP.exe` (x64, PE machine `0x8664`).
`DOOM3XP.exe` contains the d3xp game code but needs retail Resurrection of Evil data to play;
the base game runs through `DOOM3.exe`. `build-hd/` is per-worktree and git-ignored. Configure
creates `targets/win11-amd64-hd/doom3` and applies `patches/all-changes.patch` when the copy is
missing, so a fresh clone only needs `git submodule update --init --recursive` first.

## Data (never committed)

The free demo (`demo00.pk4`) and the Wulfen pack (`wulfen_texture_pack_2.0.pk4`,
853,775,687 bytes) are **not** in this repository. Put them under an ignored data root
(`<data-root>` below), then stage:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/prepare-hd-data.ps1 `
  -DemoPk4 <data-root>\demo00.pk4 `
  -HdPk4   <data-root>\wulfen_texture_pack_2.0.pk4 `
  -RunDir  <data-root>\run-hd
```

The script calls `prepare-demo-data.ps1` (hard-links `demo00.pk4` and writes the two local
compatibility shims), then hard-links the pack as
`base/zzz_wulfen_texture_pack_2.0.pk4`. The `zzz_` prefix makes it sort after `demo00.pk4`;
DOOM 3 gives later alphabetic pk4 names priority. It also writes `hd-manifest.txt` with sizes,
SHA-256, and pack-entry counts.

### The precompressed-DDS and downsize gates

`demo00.pk4` ships a `dds/` tree of precompressed textures. The engine checks those first and
never reads a same-named `.tga` from a higher-priority pk4, so a TGA texture pack can be mounted,
listed in the log, and still not be sampled. The demo's config also sets `image_downSize 1`,
which clamps diffuse maps to 256 pixels. Pass `-HdTextures` to set
`image_usePrecompressedTextures 0` and disable all image downsampling before the map loads.
Use the same switch for the baseline run so the only difference between the two runs is the pack.
Use the same `build-hd` executable and the same `run-base`/`run-hd` layout for both; do not
compare against a separately built `build/` executable.

**Licence:** the Wulfen pack is marked Proprietary on ModDB, and the demo is non-redistributable.
Both are for local testing only. Never commit, package, or redistribute them.

## Run

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-win11-amd64.ps1 `
  -Exe build-hd\bin\Release\DOOM3.exe `
  -DataDir <data-root>\run-hd `
  -OutDir  <data-root>\out-hd -Clean `
  -HdTextures -GfxInfo -ListImages -WaitSec 168
```

Do **not** pass `-Build` with `-Exe build-hd\...`: `-Build` builds the default target
(`build/`). Build with the `win11-amd64-hd` preset first, then run.

For the base-vs-HD comparison, also run the demo-only directory
(`scripts/prepare-demo-data.ps1 -Pk4 ... -RunDir ...\run-base`) with the **same executable** and
the same evidence switches. The HD run loads textures more slowly; the longer wait below puts
both captures at roughly the same point after map load. Check the `msec to load` line in each
log and confirm both screenshots show the same scene before comparing.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-win11-amd64.ps1 `
  -Exe build-hd\bin\Release\DOOM3.exe `
  -DataDir <data-root>\run-base `
  -OutDir  <data-root>\out-base -Clean `
  -HdTextures -GfxInfo -ListImages -WaitSec 150
```

## Verify

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify-hd-run.ps1 `
  -DataDir <data-root>\run-hd `
  -OutDir  <data-root>\out-hd `
  -BaseOutDir <data-root>\out-base
```

Checks: x64 PE, zero RHI symbols, `GL_VENDOR`/`GL_RENDERER`/`GL_VERSION` plus
`ARB2 path ENABLED`, both pk4 files loaded with the pack after `demo00.pk4`, demo map spawn,
non-blank screenshot, baseline log pack-free with the same forced image settings, base-vs-HD
pixel difference (default 1% at delta 8/255), at least 10 textures uploaded at a larger
resolution than the baseline (SC-008), no `.pk4` or >1 MB file in git status, and staged demo/pack
hashes matching the manifest. The texture check needs `+listImages` in both logs; the pixel
check runs `py -3 scripts/compare-screenshots.py`, which needs Pillow. If Pillow is missing,
install it into the Python 3 environment (`py -3 -m pip install pillow`).

## Known limits

- Only the free demo map `game/demo_mars_city1` is available; HD coverage is partial and retail
  campaign/RoE behaviour is unverified.
- The pack may not cover every demo texture; missing coverage is expected.
- The demo map opens with a cinematic and the HD run loads more slowly, so a fixed wall-clock
  wait can capture a different camera shot. Use the `msec to load` lines and confirm both
  screenshots show the same scene; `listImages` upload sizes are the quantitative sampling proof.
- `-HdTextures` disables DDS and image downsampling for every texture in the run, not just the
  pack. Load time and texture memory rise; this is a local-test switch, not a shipping default.
- No retail DOOM 3 or Resurrection of Evil data is used.
- The pack targets the vanilla engine; if a future pack revision ships shader overrides for a
  source port, test the ARB2 path before assuming it works.
