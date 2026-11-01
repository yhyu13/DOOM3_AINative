# Evidence: smoke client-size comparison on a DPI-scaled display (issue #103)

**Status**: executed 2026-11-01 on the 125%-scaled host. `scripts/smoke-win11-amd64.ps1` compared the
physical capture with the requested logical `-Width`/`-Height`, so no value could match and the map
phase never ran. The comparison now uses one coordinate system and prints requested, scale, expected
and observed sizes.

## Identity and scope

- **Claim**: on a scaled display the menu phase reaches exit 0, the map phase executes, and a genuinely
  wrong size still fails (the check is not relaxed into an unconditional pass).
- **Change**: `scripts/smoke-win11-amd64.ps1` only. No engine source, submodule, game data or build
  system change.
- **Scale**: `GetDeviceCaps(LOGPIXELSX)` with the process made DPI-aware returns 120, so the display
  scale is 1.25. `GetDpiForSystem` returns 96 to a DPI-unaware process and is not used.

## Environment

- Windows 10.0.19045 x64, Windows PowerShell 5.1, git 2.25.0.windows.1, CMake 4.1.2 / MSVC 19.38.
- Exe: `build/bin/Release/DOOM3.exe` (Release build, `cmake --build --preset win11-amd64-release -- -m`,
  exit 0; full log in git-ignored `build/_build.log`).
- Data: official demo `demo00.pk4`, staged by `scripts/prepare-demo-data.ps1` into the git-ignored
  `build/_demo_run` (exit 0).

## Procedure and results

All commands were run from the repository root; full output is in the git-ignored logs named below.

| # | Command | Exit | Observed |
|---|---|---:|---|
| 1 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\smoke-win11-amd64.ps1 -SizeSelfTest` | **0** | 6/6 size cases: scaled match PASS, unscaled window FAIL, wrong width FAIL, wrong height FAIL, rounding within tolerance PASS, 100% exact PASS (`build/_smoke_sizetest.log`) |
| 2 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\smoke-win11-amd64.ps1 -DataDir build\_demo_run -OutDir build\smoke-dpi -Width 2000 -Height 1125 -Clean -StopExisting` | **7** | menu **PASS** (`client=2500x1406 requested=2000x1125 scale=1.25 expected=2500x1406 colours=536 logChecks=ok`); the map phase **ran** and matched `entities spawned` / `SpawnPlayer`, then failed the `ERROR:` log guard on `SHADER ERROR` (see below) (`build/_smoke_dpi.log`) |
| 3 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\smoke-win11-amd64.ps1 -DataDir build\_demo_run -OutDir build\smoke-dpi-neg -Width 2000 -Height 1125 -Scale 1 -Clean -StopExisting` | **8** | `client size 2500x1406 != expected 2000x1125 (requested 2000x1125, scale 1)`: the negative control still fails (`build/_smoke_dpi_negative.log`) |

Case 2 proves the #103 symptoms are gone: before the change the menu returned 8 on size and the map
phase never started. Case 3 proves the size check is still load-bearing: forcing the pre-fix coordinate
system (scale 1) fails on the same capture.

## Map-phase observation (not a DPI defect)

The map phase loaded the level (`...1969 entities spawned, 0 inhibited`, `SpawnPlayer: 0`,
`Common Initialization Complete`) and then exited 7 because the log contains
`SHADER ERROR: ... glprogs/arbVP_glasswarp.txt ... file not found`. The official 2004 demo pk4 has
**0** `glasswarp` entries (16 `glprogs/*` entries, none of them glasswarp); those shaders were added
after the demo. This is a demo-data/version gap, unchanged by this fix and outside #103's DPI scope;
fixing it would need game data or engine changes, neither of which is allowed here.

## Limitations

- The size comparison assumes the engine window stays DPI-unaware, so the OS scales by the system DPI.
  If an engine build declares DPI awareness, pass `-Scale 1`; the printed `requested/scale/expected/
  observed` fields show which assumption was used.
- `-SizeTolerance` (default 2 physical pixels) absorbs DPI rounding (1125 x 1.25 = 1406.25 -> 1406).
- The map phase result above is a data observation, not a clearance of the map phase on this host.

## Independent review

- **Independent reviewer**: NOT RUN. The PR reviewer should rerun cases 1-3 or spot-check the logs.
