# Tasks: HD Assets on the Legacy OpenGL Build

**Input**: `specs/006-hd-assets/spec.md`, `specs/006-hd-assets/plan.md`

**Prerequisites**: `doom3_orig/doom3` submodule initialized; VS2022 v143 + Windows SDK
10.0.26100; free demo `demo00.pk4`; Wulfen Texture Pack 2.0 pk4 (local, untracked).

**Evidence rule**: a task is checked only when a command output, log line, or screenshot
comparison proves it. Paths below use `<repo>`, `<data>` placeholders; no machine paths are
committed.

## Phase 0 — RHI gate (P1)

- [x] **T001** Record the pristine submodule SHA and clean status.
  - Evidence: `git -C doom3_orig/doom3 rev-parse HEAD` → `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`; `status --porcelain` empty.
- [x] **T002** Search the target engine tree for new-RHI symbols.
  - Evidence: `grep -rIlE '\bRHI\b|Vulkan|D3D12|ID3D12|IDirect3DDevice' targets/win11-amd64-hd/doom3/neo` → 0 files.
- [x] **T003** Confirm the classic renderer selection is present.
  - Evidence: `r_rendererArgs[] = { "best", "arb", "arb2", "Cg", "exp", "nv10", "nv20", "r200" }` in `neo/renderer/RenderSystem_init.cpp`.
- [x] **T004** Run the engine and capture the actual GL identity.
  - Evidence (baseline log): `GL_VENDOR: Intel`, `GL_RENDERER: Intel(R) Graphics`, `GL_VERSION: 4.6.0 - Build 32.0.101.8331`, `ARB2 path ENABLED (ACTIVE)`.
  - Gate result: **PASS** — proceed.

## Phase 1 — Target and build (P2 prerequisite)

- [x] **T005** Create the target copy: `scripts/new-target.ps1 -Name win11-amd64-hd`.
- [x] **T006** Add tracked build files (`CMakeLists.txt`, `sources.cmake`, `src/eaxguid_x64.cpp`, `patches/`) copied from `win11-amd64`.
- [x] **T007** Apply the shared x64 patch: `scripts/apply-target-patches.ps1 -Name win11-amd64-hd` → `Applied patch: 66 files verified, 0 mismatches`.
- [x] **T008** Configure and build Release with the bundled CMake.
  - Evidence: `cmake --build --preset win11-amd64-hd-release` → exit 0; `DOOM3.exe` linked.
- [x] **T009** Verify the executable is x64 and record size/hash.
  - Evidence: `DOOM3.exe` PE machine `0x8664`, 16,710,656 bytes, SHA-256 `635c02c2a7de20c46b566ad875501f0b77660f4130aba6796c754546aa9b7db0`; `DOOM3XP.exe` PE machine `0x8664`, 16,893,952 bytes, SHA-256 `866aea71fc7195b57ac86cc5236f2080d5ee8bbdce95ebf06714e873e1cc1bfe`.

## Phase 2 — Asset acquisition and staging

- [x] **T010** Download `wulfen_texture_pack_2.0.pk4` (853,775,687 bytes) to `<data>/doom3-hd/`.
  - Evidence: file present at `<data>/doom3-hd/wulfen_texture_pack_2.0.pk4`, length 853,775,687 bytes (ModDB download; Cloudflare challenge passed in a normal browser session, file fetched from the CDN).
- [x] **T011** Verify archive integrity (ZIP open, entry count) and record SHA-256.
  - Evidence: `ZipFile.OpenRead` succeeds; 933 entries; .NET `testzip` equivalent passed; SHA-256 `616774f64382473ab9af5d5de3baa3ef0a333adfc782f9a41b5a88a61e21ee26`.
- [x] **T012** Inspect members for texture/material/shader subtrees and choose any local exclusions.
  - Evidence: 933 entries (27 directories), **906 files**; 898 `.tga` (874 under `textures/`, 24 under `models/`), 8 `.mtr`, 0 `glprogs`/`.vfp`/`.vp`/`.fp`; 1,997,989,757 bytes uncompressed. The 8 `.mtr` files override material definitions and produce 957 "previously defined" warnings; they are not texture-only. No shader/prog subtree, so no exclusion was needed.
- [x] **T013** Stage `<data>/run-hd` with `scripts/prepare-hd-data.ps1`; confirm `zzz_` ordering and manifest.
  - Evidence: manifest reports `hd.length=853775687`, `hd.entries=933`, `hd.files=906`, `hd.textures=874`, `hd.materials=8`, `hd.models=24`, `hd.glprogs=0` (file-only grouping; 874 + 24 = 898 TGA), plus `hd.staged_sha256=616774f6...`; staged file `base/zzz_wulfen_texture_pack_2.0.pk4`.

## Phase 3 — Run and evidence

- [x] **T014** Baseline run without the pack; capture log and screenshot.
  - Evidence: `run-win11-amd64.ps1 -Exe build-hd\bin\Release\DOOM3.exe -DataDir <data>\run-base -OutDir <data>\out-base-timed -Clean -HdTextures -GfxInfo -ListImages -WaitSec 150` → exit 0, `spawned=True`, `distinctColours=617`; log loads only `demo00.pk4`; map load 25,914 ms, so the capture is about 124 s after map load.
  - Note: `-HdTextures` is required for a fair control: it forces the TGA path and disables `image_downSize*`, matching the HD run. `build-hd`'s executable is used for both runs; `run-base` has the same demo shims as `run-hd`, and only the pack differs. The first baseline at the default 80 s captured the intro cinematic; it was rerun at 150 s so both screenshots show the same hangar gameplay view.
- [x] **T015** HD run with the pack; capture log and screenshot.
  - Evidence: `-Exe build-hd\bin\Release\DOOM3.exe -DataDir <data>\run-hd -OutDir <data>\out-hd-timed -Clean -HdTextures -GfxInfo -ListImages -WaitSec 168` → exit 0, `spawned=True`, `distinctColours=651`; log line 3 `Loaded pk4 ...demo00.pk4`, line 4 `Loaded pk4 ...zzz_wulfen_texture_pack_2.0.pk4`, line 813 `using ARB2 renderSystem`, line 1463 `1969 entities spawned`, line 1475 `SpawnPlayer: 0`, line 1595 `ARB2 path ENABLED (ACTIVE)`; map load 45,791 ms, so the capture is about 122 s after map load.
  - Pack-sampling evidence: with `fs_debug 1`, `textures/base_door/delelev1_d.tga (found in '...zzz_wulfen_texture_pack_2.0.pk4')` (diagnostic run `out-hd-tga`, log line 7650).
  - Critical finding: the demo ships `dds/` precompressed textures and sets `image_downSize 1`. With defaults the pack is mounted but the engine loads demo DDS and clamps diffuse maps to 256; earlier "HD" evidence was invalid. `-HdTextures` sets `image_usePrecompressedTextures 0` and disables all image downsampling before map load.
- [x] **T016** Run `scripts/verify-hd-run.ps1`; require every required check PASS.
  - Evidence: SC-001, SC-002a, SC-002b, SC-003, SC-004a, SC-004b, SC-005, SC-006, SC-008 all PASS; `SUMMARY fails=0`; exit 0.
  - SC-008 result: **121 textures uploaded larger**; example `textures/base_door/delelev1_s 256x256 -> 1024x1024`.
  - SC-007 (clean-checkout reproducibility) is procedural and is not asserted by the verifier; this record does not claim it passed.
- [x] **T017** Compare baseline and HD screenshots with `scripts/compare-screenshots.py`; record DIFF_PERCENT.
  - Evidence (valid controlled run): `PIXELS=307200`, `DIFF_PIXELS=33373`, `DIFF_PERCENT=10.8636`, `MEAN_ABS=2.5532`, delta 8/255, threshold 1.0, `RESULT=PASS`. Both screenshots show the same Mars City hangar view and HUD; only the pack differs.
  - The superseded 72.2422% figure came from frames captured at different in-game times and different camera views, and is not used as pack evidence. The earlier 22.6774% figure came from a run that still used demo DDS/downsampling and is also superseded.
  - Caveat: the pixel difference is qualitative confirmation; the quantitative sampling proof is SC-008's larger uploads (and the `fs_debug` pk4 hit). Only 8.7% of demo TGA names are covered by the pack (327 of 3,771), and 1,222 warnings were logged, mostly duplicate material definitions and missing `_local`/`_s` maps.

## Phase 4 — Documentation and handoff

- [x] **T018** Write `specs/006-hd-assets/spec.md` and `plan.md`.
- [x] **T019** Write `targets/win11-amd64-hd/README.md` and the staging/verification scripts.
- [x] **T020** Update this file with final evidence and report the RHI verdict to the user.
  - Verdict: **no new RHI**; the build is the 2011 classic OpenGL engine and the run used ARB2.
- [ ] **T021** If the user authorizes a commit: branch, commit, PR; never commit `_data/`, `build-hd/`, or the pack.

## Dependencies

- T010–T013 depend on lawful local access to the demo and pack.
- T015–T017 depend on T008, T013, and T014.
- T020 depends on T017.

## Implementation Strategy

MVP is P1 (gate) + P2 (HD demo run). P3 (reproducibility) is satisfied by the target presets,
`prepare-hd-data.ps1`, and `verify-hd-run.ps1`; no engine code is expected to change.
