# Independent verification — T043 (post-PR-#4 diff) and T077 (fresh-clone SC-007)

- **Role**: independent verification worker. I did not author this code. Read-only apart from this
  report: nothing else in the worktree was edited, and the engine submodule and target copy were not
  touched outside a throw-away clone under `<TEMP>`.
- **Session**: `ses_e6d25911fffervWKai0AgSkEDq`
- **Reviewed SHAs**
  - T043 (committed code): `0f386a7153dfdbb4127dfccd66c2f81fab12dd10` — PR #8, merged to `main`.
  - T077 (executed tree): `f9e84c8698727faeba55e19684fa3c8befa8f5d2` — `origin/main` at run time.
- **Date**: 2026-11-01
- **Host toolchain**: Windows 11 x64, CMake 4.1.2, MSVC 19.38.33145.0 (VS2022 Community, v143),
  Windows SDK 10.0.26100; git 2.25.
- **Game data**: none. `_data/` is absent and `D3_DATA_DIR` is empty, so every data-dependent check
  is **NOT RUN**, never PASS.

## Verdict

**T043 — APPROVED WITH FINDINGS.** No BLOCKER in the reviewed diff. The x64 fixes in the listed tasks
are present, coherent and correct on inspection; the setup stage works. One MAJOR finding (the
verification script of the reviewed SHA wrote the tracked patch set) is **already fixed on `main`** by a
later commit and is therefore not live. Three MINOR findings remain live, all documentation or narrow
detection issues; no functional defect found.

**T077 — PASS for the build/bootstrap/verify path, with SC-003 NOT RUN.** One unbroken fresh-clone run
(config → build → verify) and the interrupted-copy replacement path both exited 0 and produced a
17,771,008-byte x64 executable; SC-001/002/004/005/006/007 PASS. SC-003 and every runtime engine test
(T050/T062/T063/T064/T065/T066a runtime, T066b) are **NOT RUN** because this host has no DOOM 3 data.

This PR records the verdict and evidence. It was **not merged** and issue #1 was **not closed**.

## T043 — method

Scope: the committed diff `ea2e02e..0f386a7` for the tasks named in `review.md`'s Re-review section:
T060, T061, T064, T070–T075, T050, T062, T063, T065, T066 (29 files, +1272/−99; the engine part is
carried by `targets/win11-amd64/patches/all-changes.patch`, 731,842 bytes at that SHA because
`.gitattributes` marks `targets/*/patches/*.patch` binary).

What I did:
- Read the full diff for the scripts, root/target CMake, presets, specs and docs.
- Re-applied the patch in the fresh clone and read the hunks for every fix claimed by the tasks:
  `PtrFlag_Write`/`PtrFlag_Read`, sound-channel save/restore, `RenderWorld_demo`, `GuiModel`,
  `SAVEGAME_VERSION 18`, `Sys_FPU_*`/stack-trace code, `idPlayer::ExitCinematic`,
  `OP_ADDRESS`/`type_pointer`, `Cinematic.cpp` `mcomp`, `findhandle`, `FONT_FILE_BYTES`,
  `CPU_EASYARGS`, `_alloca16`, `FtoiFast`/`FtolFast`, `AdapterRAM` (`ulVal`).
- Checked the manifest: 72 rows, all `M`, 0 `A`, 0 `D`; every patched file hash matches (72/72).
- Checked hygiene: no tracked `.exe/.dll/.pk4/.pk3/.pak/.obj/.lib/.pdb` in the diff; the only
  absolute-path-like strings are removed lines (the old review.md) plus a prose grep pattern.

## T077 — command log (exit codes captured immediately)

Run in a clone of `origin/main` at `f9e84c8`, deliberately cloned **without** `--recurse-submodules`
so the root configure had to initialise the submodule itself.

| # | Command (run from the fresh clone) | Exit | Key evidence |
|---|---|---|---|
| 1 | `git clone --no-recurse-submodules <origin> <fresh-clone>` | 0 | HEAD `f9e84c8`; submodule dir empty; no target copy |
| 2 | `cmake --preset win11-amd64` | 0 | log: `Initialising submodule doom3_orig/doom3`, `new-target: exit 0`, `apply-target-patches: exit 0`, `Configuring done`, `Generating done` |
| 3 | `cmake --build --preset win11-amd64-release -- -m` | 0 | 0 `error` lines, 761 `warning` lines; `DOOM3.exe` and `DOOM3XP.exe` linked |
| 4 | `powershell -File scripts\verify-win11-amd64.ps1` (no `-Exe`, no `-DataDir`) | 0 | `SUMMARY pass=5 fail=0 skipped=1` |
| 5 | PE/size probe on both executables | 0 | `DOOM3.exe 17,771,008 B machine=0x8664`; `DOOM3XP.exe 17,966,080 B machine=0x8664` |
| 6 | `git status --porcelain` (clone) / `git -C doom3_orig/doom3 status --porcelain` | 0 | 0 lines / 0 lines |
| 7 | Interrupted-copy simulation: delete `targets/win11-amd64/doom3/neo/idlib/precompiled.h`, add a stray file | — | marker `True → False`; copy directory retained |
| 8 | `cmake --preset win11-amd64` (again, through the replacement branch) | 0 | `new-target: exit 0`, `apply-target-patches: exit 0`, `Configuring done`; copy recreated with 2156 files, marker restored, stray file gone |
| 9 | `powershell -File scripts\apply-target-patches.ps1 -Name win11-amd64` | 0 | `Target already patched (72 files match)` |
| 10 | `cmake --build --preset win11-amd64-release -- -m` (after replacement) | 0 | 0 `error` lines; both executables relinked; `DOOM3.exe` 17,771,008 B, machine `0x8664` |
| 11 | `powershell -File scripts\verify-win11-amd64.ps1` (after replacement) | 0 | `SUMMARY pass=5 fail=0 skipped=1`; SC-006b `git apply exit 0, ... mismatches: 0` |
| 12 | Manifest hash check against the patched copy | — | 72 / 72 files match |

Verify output (step 4 and 11, identical):

```
SC-001   PASS     PE machine 0x8664
SC-002   PASS     size 17771008 bytes
SC-004   PASS     pristine git status exit 0, porcelain lines: 0
SC-006a  PASS     export exit 0, patch manifest entries: 72
SC-006b  PASS     git apply exit 0, patched copy hash mismatches on modified files: 0
SC-003   SKIPPED  no game data dir (set -DataDir or D3_DATA_DIR)
SUMMARY pass=5 fail=0 skipped=1
```

SC-007 is satisfied for the executable/verify part: the two root commands exit 0,
`build/bin/Release/DOOM3.exe` is x64 and 17,771,008 B (> 1,048,576), and `verify` without `-Exe`
found the root build and reported the same PASS set as the stand-alone build would (SC-003 is
data-dependent and SKIPPED in both layouts without data).

## PASS / NOT RUN matrix

`PASS` = executed or read with concrete evidence. `PASS (static)` = the committed code contains and
implements the claim, but the runtime behaviour was not reproduced here. `NOT RUN` = the check
requires game data or retail data that this host does not have.

| Item | Status | Evidence |
|---|---|---|
| T060 debugger-driven x64 fixes | PASS (static) | all eight sites present in the patch: `findhandle intptr_t`, GUI expr `intptr_t`, mix-buffer `uintptr_t` align, `FONT_FILE_BYTES`, `CPU_EASYARGS 0`, `OP_ADDRESS` pointer-sized temp, `Cinematic mcomp int`, `ID_ENFORCE_KEY=0` in CMake |
| T061 review fixes | PASS / PASS (static) | `apply-target-patches.ps1` fails closed (`exit 2` missing inputs); capture uses `GetClientRect`; smoke asserts menu+map log lines; `ulVal` and `_mm_cvtss_si32` hunks verified |
| T064 sound audible + smoke audio assertion | **NOT RUN** (runtime); script PASS (static) | `audio-peak.ps1` (WASAPI session peak) is wired into the map phase; no data on this host |
| T070 root CMake + presets | PASS | step 2/3/8/10 |
| T071 configure-time bootstrap | PASS | step 2 (clone without submodules → submodule init + copy + patch) |
| T072 shared executable resolver | PASS (static) | `scripts/_common.ps1 Resolve-Doom3Exe`; verify runs without `-Exe` and finds the root build (step 4) |
| T073 root build evidence | PASS | step 3/5/10/11 |
| T074 bootstrap proof | SUPERSEDED by T077 | step 2/8/10/11 are the unbroken run T074 deferred |
| T075 docs | PASS with MINOR accuracy findings | B1 docs corrected; R2/R3 below |
| T050 FPU state / stack traces | **NOT RUN** (runtime); PASS (static) | `sys_cpuInfo`/`sys_fpuState`/`sys_stackTrace` and `test-cpu-fpu-trace.ps1` present; script needs `-DataDir` |
| T062 demo record/playback serialisation | **NOT RUN** (runtime); PASS (static) | render/GUI/sound pointer fields are presence flags; `test-savegame-demo.ps1` phases 4–5 are non-vacuous |
| T063 savegame serialisation, version 18 | **NOT RUN** (runtime); PASS (static) | `SAVEGAME_VERSION 18`, old versions rejected; sound pointers read as flags, re-resolved on restore |
| T065 loopback join / map-restart crash | **NOT RUN** (runtime); PASS (static) | `idPlayer::ExitCinematic` returns when `!scriptThread`; `test-net-loopback.ps1` has a real negative control |
| T066a `DOOM3XP.exe` builds | PASS (build); **NOT RUN** (runtime) | `DOOM3XP.exe` 17,966,080 B, machine `0x8664`; `test-d3xp.ps1` not run (no data) |
| T066b retail RoE gameplay | **NOT RUN** | retail `d3xp/pak000.pk4` not available |
| SC-001 build exits 0, PE 0x8664 | PASS | steps 3/5 |
| SC-002 size > 1,048,576 | PASS | 17,771,008 B |
| SC-003 menu ≤120 s, 1280×720 non-flat | **NOT RUN / SKIPPED** | no game data; `verify` reports SKIPPED |
| SC-004 pristine porcelain empty | PASS | step 6 |
| SC-005 verify exits 0, PASS/SKIPPED per criterion | PASS | step 4/11 |
| SC-006 every change in a patch, fresh copy rebuilds | PASS | 72 `M` / 0 `A` / 0 `D`; `git apply` exit 0; 72/72 hashes; rebuilt tree builds (step 10) |
| SC-007 fresh clone, two root commands, verify | PASS (build/verify), SC-003 NOT RUN | steps 2–5, 11 |

## Findings

Severities follow `CONTRIBUTING.md`. "Live" = still present on `origin/main` `f9e84c8` at review time.

### MAJOR (not live — fixed after the reviewed SHA)

**M-R1 — the verification script of the reviewed SHA rewrote tracked patch files.**
At `0f386a7`, `scripts/verify-win11-amd64.ps1` calls
`& scripts/export-target-patches.ps1 -Name win11-amd64` with no output directory, and that version of
the export script writes to `targets/win11-amd64/patches/` (there is no `-OutDir` parameter). A
verification run therefore rewrites tracked files and can silently adopt uncommitted edits from the
target copy into the published patch — the evidence step is not read-only. *Evidence*: `git show
0f386a7:scripts/verify-win11-amd64.ps1` (SC-006 block) and `git show
0f386a7:scripts/export-target-patches.ps1` (`$out = Join-Path $repo "targets\$Name\patches"`).
*Current state*: fixed on `main`; the verifier now exports to `%TEMP%\d3exp-<guid>` (observed in this
run: `Wrote ...\Temp\d3exp-...\all-changes.patch`). No action needed beyond recording it.

### MINOR (live)

**M-R2 — `targets/win11-amd64/README.md:62-66` understates the compiled `tools/` set.** It says
`tools/**` is excluded "except `tools/compilers/aas/AASFile.cpp` and `AASFileManager.cpp`", but
`sources.cmake` compiles **33** `tools/compilers/**` TUs (including `dmap/*`, `renderbump`,
`roqvq`) and `gen-sources` excludes only `tools/` outside `tools/compilers/`. The README contradicts
the generator header and the actual build. Fix the README or exclude the extra TUs.
*Evidence*: `Select-String sources.cmake -Pattern 'tools/'` → 33; `gen-sources-win11-amd64.ps1:8,68`.

**M-R3 — stale status line in the specification.** `specs/001-win11-amd64-native-build/spec.md:5`
still says "pending commit, PR and independent review of the post-PR-#4 work" although PR #8
(`0f386a7`) is merged. It should say the post-PR-#4 work is merged and this review is the outstanding
independent review (or record its outcome).

**M-R4 — interrupted-copy detection is narrower than the failure it guards.** The root configure
treats a copy directory as interrupted only when
`targets/win11-amd64/doom3/neo/idlib/precompiled.h` is missing
(`CMakeLists.txt:23,46,64-69`). A robocopy interrupted after that file was copied leaves the marker
present; configure then skips the replacement branch and the build fails later on missing sources.
*Evidence*: the replacement path I executed only triggers on the marker; `new-target.ps1`'s own
file-count check (lines 70-75) is never reached in that case. Suggest a completion marker written
last, or a file-count check in configure.

### NIT

- **N-R1** `sys/sys_public.h` still defines `BUILD_STRING "win-x86"` on x64 (round-1 finding n1);
  `CPUSTRING` is `"x64"`, so this is cosmetic.
- **N-R2** Mixed line endings remain in two patched files: `GuiModel.cpp` (1 LF-only line) and
  `snd_world.cpp` (16 LF-only lines). Re-review item 10; cosmetic patch noise.
- **N-R3** `plan.md`'s tracked source list now contains the build-critical inputs, but omits the test
  scripts, `audio-peak.ps1`, `capture-showcase.ps1` and `diff-target.ps1` (M3 was about build inputs,
  so this is only completeness).

## Limits of this verification

- No game data: T050/T062/T063/T064/T065/T066a runtime, SC-003 and T066b are NOT RUN, not PASS.
- The engine hunks were reviewed statically and the patched tree was built; I did not attach a
  debugger or execute save/load, demo playback, networking, audio or FPU probes.
- The T043 review covers `0f386a7` only. Later `main` commits (spec 004/008 work) changed
  `CMakeLists.txt`, the scripts and the patch set; those are out of scope except where I re-ran the
  current tree for T077.
- `T076` (issue comment), `T078` (merge `main` and resolve shared-file conflicts) and `T079`
  (deferred edges) remain open; none is closed by this PR.

## Residual blockers

1. Data-dependent verification (SC-003 and the four engine test scripts) needs the official demo data
   prepared with `scripts/prepare-demo-data.ps1`; without it those criteria stay NOT RUN.
2. Retail Resurrection of Evil data is absent, so T066b stays NOT RUN.
3. M-R2/M-R3 are documentation corrections in shared files (`targets/win11-amd64/README.md`,
   `specs/001.../spec.md`) that T078 also touches; this PR does not edit them.
