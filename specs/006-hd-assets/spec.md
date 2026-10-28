# Feature Specification: HD Assets on the Legacy OpenGL Build

**Feature Branch**: `006-hd-assets`

**Created**: 2026-10-27

**Status**: Draft

**Input**: User description: "build a version of game with hd assets (not sure if our source build already include new RHI?) if new RHI then notify me we stop, I expect old GL with hd assets; target: `targets/win11-amd64-hd`"

## Clarifications

### Session 2026-10-27

- Q: Which HD asset source should be used? → A: Wulfen Texture Pack 2.0 (`wulfen_texture_pack_2.0.pk4`, ModDB file 45604, 853,775,687 bytes).
- Q: Which base game data is available? → A: The official free DOOM 3 demo only (`demo00.pk4`); no retail `pak000.pk4`–`pak008.pk4`.
- Q: What happens if the source build contains the project's new RHI? → A: Stop and report; do not continue the HD build.
- Q: How is a TGA texture pack made effective when the demo ships precompressed DDS? → A: Force `image_usePrecompressedTextures 0` before map load and disable the demo's image downsampling (`-HdTextures`). Without the first, the engine loads `dds/` from `demo00.pk4` and never reads the higher-priority TGA; without the second, `image_downSize 1` clamps diffuse maps to 256. Verified with `listImages` upload dimensions.
- RHI gate result: **PASS** — the target copy at submodule `a9c49da` contains only the 2011 OpenGL renderer (`neo/renderer`, ARB/ARB2 paths). No `RHI`, Vulkan, D3D12, or `IDirect3DDevice` symbol exists under `neo/`. The project RHI is design-only in the merged `specs/003-render-modernization` and `specs/004-quality-renderer` specifications.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Confirm the legacy OpenGL renderer before adding assets (Priority: P1)

A maintainer must be able to prove that the build under test is still the 2011 OpenGL renderer before spending time on HD assets. If a new RHI is present, the work stops and the finding is reported.

**Why this priority**: The user explicitly made this a gate. Building an "HD" variant on an unrequested new renderer would invalidate the whole exercise.

**Independent Test**: Inspect the pristine submodule SHA and the target copy for RHI/Vulkan/D3D12 symbols, then run the executable and read `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, and the ARB2 path line from its log.

**Acceptance Scenarios**:

1. **Given** the target copy is created from submodule `a9c49da`, **When** the engine tree is searched for `RHI`, `Vulkan`, `D3D12`, `ID3D12`, or `IDirect3DDevice` under `neo/`, **Then** no match is found.
2. **Given** the built executable starts with the demo data, **When** the log is read, **Then** it contains `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, and `ARB2 path ENABLED`, and the renderer is the classic ARB2 path rather than a D3D12/Vulkan backend.
3. **Given** the RHI audit finds a new renderer, **When** the maintainer reaches this gate, **Then** the HD build is abandoned and the evidence is reported instead of proceeding.

---

### User Story 2 - Run the demo with HD textures on the old GL engine (Priority: P2)

A player can launch the x64 build, load `game/demo_mars_city1`, and see the Wulfen HD textures replacing the demo textures, without any renderer replacement.

**Why this priority**: This is the requested deliverable: old GL plus HD assets, running.

**Independent Test**: Stage `demo00.pk4` plus the Wulfen pk4 in an untracked run directory, start the HD executable on the demo map with the TGA path forced, and capture the log and a screenshot. Repeat without the Wulfen pk4 using the **same executable and the same data layout** (only the pack differs), capturing both screenshots at the same point after map load; compare both pixels and `listImages` dimensions.

**Acceptance Scenarios**:

1. **Given** a run directory with `base/demo00.pk4` and `base/zzz_wulfen_texture_pack_2.0.pk4`, **When** the engine starts with `image_usePrecompressedTextures 0`, **Then** the log lists both pk4 files as loaded and the Wulfen pack is found after `demo00.pk4` in the search order.
2. **Given** the demo map is loaded with the pack, **When** the engine has run for the configured wait, **Then** the process is alive, the log contains `entities spawned` and `SpawnPlayer`, and the screenshot is non-blank.
3. **Given** baseline and HD screenshots from the same map and start conditions, **When** they are compared, **Then** a measurable fraction of pixels differs, and at least 10 image names present in both runs upload at a larger size in the HD run.
4. **Given** the HD run, **When** the log is inspected, **Then** the engine still reports the classic OpenGL renderer and no new RHI backend initializes.

---

### User Story 3 - Reproduce the HD run from a clean checkout (Priority: P3)

A contributor can recreate the target, build it, stage the untracked data, and rerun the evidence with documented commands, without committing game data or machine-specific paths.

**Why this priority**: The result is only useful if it survives the next worktree and the next contributor.

**Independent Test**: From a clean checkout, run the documented target-build and data-staging commands, then run the evidence script; it exits 0 only when all HD criteria pass.

**Acceptance Scenarios**:

1. **Given** a clean clone with the submodule initialized, **When** `cmake --preset win11-amd64-hd` and `cmake --build --preset win11-amd64-hd-release` run, **Then** the build exits 0 and produces `build-hd/bin/Release/DOOM3.exe` as an x64 PE.
2. **Given** legally obtained demo and Wulfen pk4 files outside git, **When** the staging script runs, **Then** it creates an untracked run directory and never copies the pk4 files into a tracked path.
3. **Given** the staged run directory and a built executable, **When** the HD evidence script runs, **Then** it reports PASS for the RHI gate, pk4 order, map load, screenshot, and base-vs-HD difference, and exits 0.

---

### Edge Cases

- The pack contains `glprogs/` or material definitions written for a source port: if the vanilla ARB2 path renders incorrectly or the log shows shader/material errors, the offending subtree is excluded from a local staged override; the downloaded pack is never edited in place.
- The demo ships precompressed `dds/` textures and the engine prefers them over a higher-priority TGA: force `image_usePrecompressedTextures 0` and disable `image_downSize*` for both runs; without it, pk4 load order alone is not evidence that the pack was sampled.
- The pack filename sorts before `demo00.pk4`: stage it under a name that sorts after (for example `zzz_wulfen_texture_pack_2.0.pk4`) so the intended override order is deterministic.
- A texture exceeds `GL_MAX_TEXTURE_SIZE`: the engine downsizes it; the run must still load and the log must be checked for upload errors.
- The demo data does not contain every texture the pack replaces: partial coverage is expected and is reported, not treated as failure.
- The pack is missing or truncated: staging fails with a non-zero exit and does not produce a run directory that looks valid.
- The pack download is blocked by Cloudflare: the maintainer supplies a locally downloaded file; the build does not depend on a particular download route.
- The RHI audit finds a new backend: stop at User Story 1 and report; do not patch the target copy to work around it.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The repository MUST provide a `win11-amd64-hd` target whose engine source is an unmodified, patched copy of the pristine submodule, created by the existing target scripts.
- **FR-002**: The HD build MUST use the same 2011 OpenGL renderer and the same engine translation units as `win11-amd64`; HD assets MUST be a runtime pk4 override, not a renderer or engine change. Because the demo ships precompressed DDS and downsizing defaults, the run MUST force the TGA path and disable image downsampling (`-HdTextures`) or prove an equivalent override.
- **FR-003**: The build MUST produce `build-hd/bin/Release/DOOM3.exe` as an x64 PE (`IMAGE_FILE_MACHINE_AMD64`, 0x8664).
- **FR-004**: A staging script MUST create a run directory containing `base/demo00.pk4` and the Wulfen pk4 under a name that sorts after `demo00.pk4`.
- **FR-005**: The staging script MUST accept source paths as parameters and MUST NOT embed machine-specific absolute paths in tracked files.
- **FR-006**: Neither the demo data nor the Wulfen pack may enter git; both MUST remain under the ignored `_data/` tree or an equivalent untracked path.
- **FR-007**: The run MUST capture a log and a screenshot, and the log MUST show the loaded pk4 files, the OpenGL identity, the map load, and the player spawn.
- **FR-008**: The evidence MUST include a baseline run without the Wulfen pk4 and a comparison showing the HD run differs in both pixels and at least 10 larger texture uploads.
- **FR-009**: Any engine defect discovered while running with HD assets MUST be fixed through the engine fork/submodule path, not by editing the pristine submodule or by adding a target-copy-only hack that cannot be reproduced from the patch.
- **FR-010**: The repository MUST document the build, staging, run, and verification commands, including the licence/provenance warning for the proprietary pack.
- **FR-011**: The evidence MUST include `listImages` output; "pk4 loaded" alone is not proof that the pack was sampled.

### Key Entities *(include if feature involves data)*

- **HD target**: `targets/win11-amd64-hd/`, containing tracked build files and a git-ignored `doom3/` source copy.
- **HD executable**: `build-hd/bin/Release/DOOM3.exe`, x64, classic OpenGL engine, statically linked base game.
- **Demo data**: `demo00.pk4` from the official free demo, plus two local compatibility shims created by the staging script.
- **Wulfen pack**: `wulfen_texture_pack_2.0.pk4`, 853,775,687 bytes, proprietary, untracked, loaded as the last pk4.
- **Run directory**: untracked `base/` tree with demo data, shims, and the HD pk4; also holds `save/base/qconsole.log`.
- **Evidence directory**: log, screenshot, and comparison output from a run; untracked build/out path.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The HD configure and Release build exit 0; `DOOM3.exe` is an x64 PE (`IMAGE_FILE_MACHINE_AMD64`, 0x8664), and its byte size and SHA-256 are recorded.
- **SC-002**: The RHI search returns 0 matches under `neo/` for `\bRHI\b`, `Vulkan`, `D3D12`, `ID3D12`, and `IDirect3DDevice`; the run log contains `ARB2 path ENABLED`. The verifier reports these as SC-002a (search) and SC-002b (OpenGL/ARB2).
- **SC-003**: The run log lists both `demo00.pk4` and the Wulfen pk4 as loaded, with the Wulfen name sorting after the demo name.
- **SC-004**: The demo map reaches `entities spawned` and `SpawnPlayer`, the process stays alive for the configured wait, and the screenshot has at least 8 distinct sampled colours. The verifier reports the log and screenshot parts as SC-004a and SC-004b.
- **SC-005**: The baseline run is pack-free and uses the same forced image settings as the HD run; with both screenshots from the same scene and point after map load, the HD screenshot differs over at least 1% of pixels by an absolute per-channel threshold of 8/255, or an equivalent documented image-difference metric.
- **SC-006**: `git status --porcelain` shows no `.pk4` file and no file larger than 1 MB added by this feature; the pack remains outside git or under an ignored `_data/` path; the staged demo and pack hashes match the staging manifest.
- **SC-007**: From a clean checkout, the documented staging and verification commands exit 0 without editing a tracked file by hand. This is a procedural check; the verifier does not clone the repository.
- **SC-008**: With `+listImages` in both runs, at least 10 common image names upload at a strictly larger resolution in the HD run; a representative example is recorded.

## Assumptions

- The user supplies a lawfully obtained Wulfen Texture Pack 2.0 and the official free demo; this repository does not redistribute either.
- The Wulfen pack is marked Proprietary on ModDB; it is for local testing only and must never be committed, packaged, or published.
- The free demo's `game/demo_mars_city1` is the only playable map available, so HD coverage is partial and retail-map behaviour is out of scope.
- A modern GPU/driver exposes the ARB2 path; if it does not, the run reports the actual renderer rather than pretending HD rendering succeeded.
- The demo's precompressed `dds/` tree is bypassed for the comparison; retail data may have a different DDS layout, so the same switch must be rechecked before claiming retail coverage.
- The project's new RHI remains design-only; if that changes, this specification's P1 gate must be rerun before any HD work.
- No retail DOOM 3 or Resurrection of Evil data is used.

## Out of Scope

- Implementing or integrating a new RHI, Vulkan, D3D12, or the quality renderer from `specs/003-render-modernization` / `specs/004-quality-renderer`.
- Retail campaign, RoE, multiplayer, or save-game compatibility testing with HD assets.
- Redistributing, repacking for publication, or relicensing the Wulfen pack or demo data.
- Changing texture compression defaults or engine limits unless a documented, reproducible defect requires it.
