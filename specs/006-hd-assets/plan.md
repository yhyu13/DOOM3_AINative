# Implementation Plan: HD Assets on the Legacy OpenGL Build

**Branch**: `feat/hd-assets` | **Date**: 2026-10-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-hd-assets/spec.md`

## Summary

Build the existing 2011 OpenGL engine from a new target copy at `targets/win11-amd64-hd`,
then load the proprietary Wulfen Texture Pack 2.0 as a runtime `base/*.pk4` override while
running the free demo map. The pack is TGA textures plus eight `.mtr` material overrides, so the
run must force the TGA path (`image_usePrecompressedTextures 0`) to bypass the demo's precompressed
`dds/` tree. No renderer, RHI, or engine behaviour change is planned. The RHI gate is a hard stop:
the audited tree contains the classic ARB/ARB2 renderer only.

## Technical Context

**Language/Version**: C++ (2011 idTech4 source), PowerShell 5.1 for workflow scripts, Python 3.14 (`py -3`) for image comparison only.

**Primary Dependencies**: Visual Studio 2022 v143, Windows SDK 10.0.26100.0, CMake 3.31 (bundled), `doom3_orig/doom3` submodule at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`, official free demo `demo00.pk4`, Wulfen Texture Pack 2.0.

**Storage**: Git-ignored `targets/win11-amd64-hd/doom3/` source copy, `build-hd/` build tree, and `_data/` run/data tree. No game data in git.

**Testing**: Existing run/verify scripts plus a new HD staging script and HD evidence verifier; screenshot comparison with Pillow.

**Target Platform**: Windows 11 x64, desktop OpenGL (ARB2 path).

**Project Type**: Desktop game engine port plus workflow automation.

**Performance Goals**: The demo map must load and run for the configured evidence window without crashing; with DDS disabled the TGA path is slower, and that cost is accepted for local testing.

**Constraints**: Pristine submodule read-only; one copy per target; no new RHI; no tracked game data; no machine-specific absolute paths in tracked files; pack is proprietary and untracked.

**Scale/Scope**: One target, one executable, one demo map, one texture pack.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Result | Evidence |
|---|---|---|
| Pristine submodule untouched | PASS | `git -C doom3_orig/doom3 status --porcelain` is empty; target copy is separate. |
| One copy per target | PASS | `targets/win11-amd64-hd/doom3` created by `scripts/new-target.ps1`; no other target uses it. |
| No game data or build output committed | PASS | Pack and demo live under `_data/`; `.gitignore` covers `_data/`, `*.pk4`, `targets/*/doom3/`, `targets/*/build/`. |
| No direct commits to `main` | PASS | Work is committed on `feat/hd-assets` and opened as a PR; no direct `main` commit. |
| Evidence before victory | PASS | Every acceptance claim has a command output, log line, or screenshot comparison. |
| New RHI gate | PASS | No RHI/Vulkan/D3D12 symbol under `neo/`; run log must still show `ARB2 path ENABLED`. |
| One writer per file | PASS | This feature edits only new `targets/win11-amd64-hd`, `specs/006-hd-assets`, new scripts, `CMakePresets.json`, and the existing run script's optional evidence switches. |

## Project Structure

### Documentation (this feature)

```text
specs/006-hd-assets/
├── spec.md
├── plan.md
└── tasks.md
```

### Source Code (repository root)

```text
CMakePresets.json                       # add win11-amd64-hd configure/build presets
targets/win11-amd64-hd/
├── CMakeLists.txt                      # copied from win11-amd64
├── README.md                           # build, stage, run, licence warning
├── sources.cmake                       # copied from win11-amd64
├── src/eaxguid_x64.cpp                 # target-owned x64 shim
├── patches/all-changes.patch           # copied from win11-amd64
├── patches/all-changes.manifest.txt
└── doom3/                              # git-ignored copy, patched at configure time
scripts/
├── prepare-hd-data.ps1                 # demo shims + HD pk4 hard link
├── verify-hd-run.ps1                   # HD-specific evidence checks
└── compare-screenshots.py              # baseline-vs-HD pixel difference
build-hd/                               # git-ignored build output
```

**Structure Decision**: Reuse the existing per-target copy model. The HD target is a build-path
variant, not a code variant; all HD behaviour comes from data loaded at runtime.

## Phase 0 — RHI audit (complete)

1. Confirm submodule SHA and cleanliness: `git -C doom3_orig/doom3 rev-parse HEAD` → `a9c49da...`.
2. Search the target copy: `grep -rIlE '\bRHI\b|Vulkan|D3D12|ID3D12|IDirect3DDevice' targets/win11-amd64-hd/doom3/neo` → no files.
3. Confirm classic renderer symbols: `r_rendererArgs[] = { "best", "arb", "arb2", ... }` in `RenderSystem_init.cpp`.
4. Decision: proceed; no new RHI present.

## Phase 1 — Target and build (complete)

1. `scripts/new-target.ps1 -Name win11-amd64-hd`
2. Copy target-owned `CMakeLists.txt`, `sources.cmake`, `src/eaxguid_x64.cpp`, and `patches/` from `win11-amd64`.
3. `scripts/apply-target-patches.ps1 -Name win11-amd64-hd` → 66 files verified, 0 mismatches.
4. `cmake --preset win11-amd64-hd` and `cmake --build --preset win11-amd64-hd-release` → exit 0.
5. Verify PE machine 0x8664 and record SHA-256 of both `DOOM3.exe` and `DOOM3XP.exe`.

## Phase 2 — Asset acquisition and staging

1. Download `wulfen_texture_pack_2.0.pk4` (853,775,687 bytes) to `_data/doom3-hd/`; verify ZIP integrity and record SHA-256.
2. Inspect archive members: texture formats, `materials/`, `glprogs/`, shader files, README/licence.
3. `scripts/prepare-hd-data.ps1 -DemoPk4 <demo00.pk4> -HdPk4 <wulfen.pk4> -RunDir <run>`:
   - calls `prepare-demo-data.ps1` for `demo00.pk4` and the two shims;
   - hard-links the HD pack as `base/zzz_wulfen_texture_pack_2.0.pk4` (falls back to copy);
   - writes a manifest with sizes and SHA-256; refuses missing/zero-length inputs.
4. If the pack ships a shader subtree that breaks the vanilla ARB2 path, stage an exclusion list or a repacked local override; never edit the downloaded file.

## Phase 3 — Run and evidence

1. Baseline: demo-only run with `scripts/run-win11-amd64.ps1 -Exe build-hd/bin/Release/DOOM3.exe -DataDir <demo-run> -OutDir <out-base> -Clean -HdTextures -GfxInfo -ListImages -WaitSec 150`.
2. HD: same script and **same executable** with `-Exe build-hd/bin/Release/DOOM3.exe -DataDir <hd-run> -OutDir <out-hd> -Clean -HdTextures -GfxInfo -ListImages -WaitSec 168`.
   - `-HdTextures` is mandatory: without it the engine loads `dds/` from `demo00.pk4` and never samples the pack's TGA.
   - `-ListImages` is mandatory: pk4 load order alone does not prove the pack was used.
   - The HD run loads about 20 s slower, so its longer wall-clock wait puts both screenshots about 122 s after map load. Check that both show the same scene before claiming a pixel difference.
3. `scripts/verify-hd-run.ps1` checks:
   - both pk4 files load and the Wulfen pack loads after `demo00.pk4`;
   - `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, `ARB2 path ENABLED`;
   - `entities spawned`, `SpawnPlayer`, screenshot colour count;
   - the baseline log is pack-free and both configs force the same image settings;
   - `compare-screenshots.py` pixel difference threshold;
   - at least 10 common image names upload at a larger resolution in the HD run (SC-008);
   - no `.pk4` or >1 MB file enters git, and staged demo/pack hashes match the manifest.
   Process liveness and the run exit code are recorded by `run-win11-amd64.ps1` and `tasks.md`; SC-007 remains a procedural clean-checkout check.
4. Record the log excerpt, image-dimension example, and screenshot paths in `tasks.md` / final report.

## Phase 4 — Documentation

1. `targets/win11-amd64-hd/README.md`: build commands, RHI verdict, data staging, licence warning, known limits.
2. `specs/006-hd-assets/tasks.md`: checked tasks with evidence.
3. Commit on `feat/hd-assets` and open a PR; do not merge or close an issue without the user's decision.

## Risks

| Risk | Mitigation |
|---|---|
| Pack includes shaders for a different source port | Inspect `glprogs/`/materials; exclude or repack locally; test with vanilla ARB2 log. |
| Demo map has partial texture coverage | Report coverage as partial; do not claim full-game HD. |
| ModDB Cloudflare blocks automated download | Use a real browser session to obtain the CDN link, or accept a manually downloaded file; never commit cookies. |
| 814 MiB download is slow or interrupted | `curl -C -` resume; verify ZIP integrity and size before staging. |
| Screenshot comparison noisy due to animations | Compare same map/start; require a conservative 1% pixel threshold and record the actual percentage. |
| Demo DDS shadows the TGA texture pack | Force `image_usePrecompressedTextures 0` in both runs; prove usage with `listImages` upload dimensions, not just pk4 load lines. |
| ARB2 unavailable on the test GPU | Report actual `GL_RENDERER`; do not substitute a different backend silently. |

## Complexity Tracking

No constitution violations. The only deviation from a pure source feature is that HD assets are
data, not code; this is intentional and is the user's requested design.
