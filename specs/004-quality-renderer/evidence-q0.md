# Q0 baseline evidence (T009–T011)

Status: **Q0 established** on 2026-10-28 in the `feat/21-q0-safe-baseline` worktree. All four
reference categories were captured twice with the engine's own framebuffer screenshot at a fixed
game tic, and pass the ceilings frozen before the recorded run. The ceilings were tightened after the fixed-tic probes; the earlier, looser cells are disclosed in `reference-scenes.md` §6, and final11 also passes them. This is evidence for the existing
OpenGL renderer only; it does not claim classic parity, modern-renderer quality, web support,
retail scene coverage, or performance.

## 1. Environment

Identity was collected to the git-ignored `build-q0-20261028/host-identity.txt` before the final
capture, and from `build-q0-verify-configure.log` for the compiler/SDK.

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.22631, build 22631 |
| CPU | Intel Core Ultra 7 265K, 20 cores / 20 logical |
| RAM | 64,892 MB reported by Windows; 64,896 MB reported by the engine |
| Graphics adapters | Intel Graphics (driver 32.0.101.8331); GameViewer virtual display (15.6.5.199); Todesk virtual display (16.44.2.509); OrayIdd (17.50.19.949) |
| Renderer reported by engine | `using ARB renderSystem` and later ARB2 lines; engine reports 64 MB video memory and "qualifies for Low quality" |
| PowerShell | 5.1.22621.6133 |
| CMake | 3.31.6-msvc6 (Visual Studio 2022 bundled) |
| Compiler | MSVC 19.44.35228.0 (cl 14.44.35207) |
| Windows SDK | 10.0.26100.0 |

The machine runs through virtual display adapters. The frames are valid for this host, but they are
**not** a discrete-GPU performance or quality baseline. The engine's own "Low quality" line is
retained rather than hidden.

## 2. Source, submodule and patch identity

| Item | Value |
|---|---|
| Repository commit | `ca4280144505a3b48d6880d73757d211eff6187c` |
| Branch | `feat/21-q0-safe-baseline` |
| Q0 working-tree change set | new Q0 script, tests and three spec documents; no tracked file modified |
| Submodule commit | `a9c49da5afb18201d31e3f0a429a037e56ce2b9a` |
| Submodule dirt | `git -C doom3_orig/doom3 status --porcelain` empty |
| Target patch | `targets/win11-amd64/patches/all-changes.patch`, 731,842 bytes, SHA-256 `bf1ad1e06da8198d0404ded8829d4ef6efcd35666378e91cd633e787c85ded80` |
| Patch manifest | SHA-256 `05824d894423ebef07c2d8451e7373746e3569683527e54270a3af0d0aa68abe` |
| Output tree | `build-q0-20261028/baseline-final11` (git-ignored) |
| Captured executable | `build-q0-verify-20261028/bin/Release/DOOM3.exe` |

The target copy is a git-ignored copy of the pristine submodule plus the patch above. The submodule
itself was not edited. The working tree was not committed; the untracked change set is named here
and its hashes are in §2.1.

### 2.1 Captured-code identity

The final11 manifest records the hash of the exact script, test and launcher used:

| File | SHA-256 in final11 manifest |
|---|---|
| `scripts/test-render-baseline.ps1` | `7b5dc84cdf6b4df59f6d1441a7be09ef89e81aee580f04520368365a72ab0a9a` |
| `tests/baseline/test-render-baseline.ps1` | `57de9506050183f0f8c945b7a0f245b68cb99cba29004341e02438e45ab6d552` |
| `tests/renderer/test-render-baseline.ps1` | `3419e30fecfe48ec3df0bac7db9991f9d3a0b567a9cca492c38c7b9f53a6c9bf` |

The current files match those hashes. The two spec documents are not read by the capture script;
their hashes identify the reviewed change set:

| File | SHA-256 |
|---|---|
| `specs/004-quality-renderer/baseline-procedure.md` | `0d54a1f38d06dbbbe4baabffaf1a4084291bf7153e2884724910200d0d5d05d4` |
| `specs/004-quality-renderer/reference-scenes.md` | `804387377784fc6172471669e5c9187da9f42076129f8d6957938f88091d3037` |

## 3. Fresh build

Commands (run from the worktree root; output redirected to git-ignored logs):

```powershell
$cmake = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S . -B build-q0-verify-20261028 -G 'Visual Studio 17 2022' -A x64 `
  -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DDOOM3_TARGET=win11-amd64 -DCMAKE_CONFIGURATION_TYPES=Release `
  *> build-q0-verify-configure.log
$configureExit = $LASTEXITCODE
& $cmake --build build-q0-verify-20261028 --config Release -- -m *> build-q0-verify-build.log
$buildExit = $LASTEXITCODE
Set-Content -Path build-q0-verify-20261028\build-q0-verify.exit -Value "configure=$configureExit build=$buildExit"
```

Recorded result file `build-q0-verify-20261028/build-q0-verify.exit`: `configure=0 build=0`. The
build log has 12,850 lines, 4,243 lines matching `warning ` and 0 matching `error `. Both
`DOOM3.exe` and `DOOM3XP.exe` were linked. CMake printed
`Ignoring extra path from command line: ".0.26100.0"` but selected SDK 10.0.26100.0; configure
still exited 0.

| Artifact | Bytes | SHA-256 | PE |
|---|---:|---|---|
| `build-q0-verify-20261028/bin/Release/DOOM3.exe` (captured) | 16,710,656 | `c5c45b7fbc776445123aea1a52d2ce30ffd8a8ea94390e5215a9e3031991bc0b` | 0x8664 |
| `build-q0-20261028/bin/Release/DOOM3.exe` (first pass) | 16,710,656 | `f0902ff77066589940ee55bfd9b2044c250312f268a9ee1df13c3fbc84796fa7` | 0x8664 |
| `build-q0-verify-20261028/bin/Release/DOOM3XP.exe` | 16,893,952 | `2ca3a3a2f2ec781e33621f72d27ab509773edac9464fb2948699ccaee51e915b` | 0x8664 |

The two `DOOM3.exe` hashes differ even though both builds used the same target copy and source.
PE timestamps or other link-time metadata are the likely cause; the build is **not** proven
byte-reproducible. The report uses the second build's executable because that build has a captured
exit code. No FileVersion resource is present (`FileVersion` and `ProductVersion` are empty).

`build-q0-20261028` hosts the final output tree; its own `DOOM3.exe` was used only for exploratory
captures, not for this report. The captured executable is from `build-q0-verify-20261028`.

## 4. Data identity

| File | Bytes | SHA-256 |
|---|---:|---|
| `base/demo00.pk4` | 483,534,533 | `e373ec70b35cf72852a1f6014c2c2dc21f890a03cd41b21a9524755f951f64f9` |
| `base/guis/mainmenu.gui` | 529,866 | `9ff40f6a86e6703c0d297b10867ce8e5fef46c9621f308f1eb81b83a110aa476` |
| `base/script/weapon_base.script` | 2,721 | `80266cc1ad443bcf32299eddc5aa985a523ad87934db568e3f4e91d125626387` |

The two loose shims are local compatibility files created by `scripts/prepare-demo-data.ps1`; they
are not redistributed. The data root is external to the repository and is read-only input. No
`.pk4`, `.pk3`, `.pak`, save, screenshot or log from the run is tracked.

## 5. Capture

Command (relative paths; the external data root is the local prepared demo run):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-render-baseline.ps1 `
  -Mode All -CaptureMethod Engine -Exe build-q0-verify-20261028\bin\Release\DOOM3.exe `
  -DataDir <D3_DATA_DIR> `
  -OutDir build-q0-20261028\baseline-final11 -OutRoot build-q0-20261028 `
  -SettleSec 0 -TimeoutSec 180 -Repeat 2
```

The manifest's `commandLine` is reconstructed from the bound parameters and records the capture phase as `mode=Capture`; the replay command is the block above. The script exits 0 and prints `result=PASS` (no skips). Each scene has two engine framebuffer
screenshots at 1280x720. The engine runs with `com_fixedTic 1`; `+wait 600` (or 720 for combat)
places both runs at the same game tic; `+screenshot` captures inside the command buffer and `+quit`
exits the process (recorded `processExit=0`, no force-stop). The script converts the uncompressed
true-colour TGA (24- or 32-bit) to PNG and compares the PNG pair. The OS window capture path is not used for Q0.

| Scene | Status | Runs | `meanAbs` | `diffPercent` | Ceiling | Centre luma | Distinct colours |
|---|---|---|---:|---:|---|---:|---:|
| `corridor` | PASS | 2 | 0.0848 | 0.0905 | 0.5 / 0.5 | 14.20 / 14.20 | 17,939 / 17,889 |
| `open-view` | PASS | 2 | 0.1767 | 0.1430 | 2.0 / 2.0 | 13.94 / 13.85 | 21,134 / 20,950 |
| `close-character` | PASS | 2 | 0.1665 | 0.2088 | 0.5 / 0.5 | 24.20 / 24.20 | 24,604 / 24,694 |
| `moving-light-combat` | PASS | 2 | 0.4853 | 0.7490 | 3.0 / 5.0 | 23.93 / 23.95 | 91,938 / 91,920 |

Screenshot SHA-256 (run1 / run2):

| Scene | run1 | run2 |
|---|---|---|
| `corridor` | `50cbb573d120701e3448acc4210f05e9629fa1bae86eee6da72069354b89c5e2` | `615a6dbd9f092ed2b8988ec3b0104daa7de088bbe9fcc6653d7c4db5fed56457` |
| `open-view` | `033fc4ae72ced7a566b7f425a21a82f1edb159f971da852c321f3f8b722bc6e5` | `14fe37cdb2a6abee84647291d1b08916b561dad903679228cfec764ed5105bfc` |
| `close-character` | `2ea57c59e5b705a0f36c196fb2f16a2345c3856f3c050c988e566d3ea79c41cf` | `5c046d486f44bef64e04cc067a9af498eef16698fe65211613b60b02b8508c2c` |
| `moving-light-combat` | `cf911f1fe6e78baf027dbf493bf04bf50da4c6eb122e6c2a7c430d6da9a4e6fc` | `33cfc530cc49847c8a081db9db9518fbaac0288eef6c18c4f452c2020574b666` |

The frames are visually distinct and match the categories: underground corridor, open hangar,
third-person marine, and imp attack. final9 had a `control-interior` closed-door frame on the same
`demo_mc_underground` map; final11 uses that map at 600 tics after the doors open, and the corridor
frame shows the interior. The control scene was removed; this is not a relabel of the door frame.
`SC-001` is met: all four categories were captured twice, no category is SKIPPED.

## 6. Synthetic tests and mutation checks

Command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\baseline\test-render-baseline.ps1
```

Result: exit 0, `SUMMARY pass=26 fail=0`. Cases: ListScenes; valid Check; missing exe (3); missing
data (4); empty data (4); non-empty output refused with an untouched sentinel (5);
refusal-before-exe-check (5); Capture refusal (5); Capture fake-PE rejection (3); repo-root output
refused (2); unknown scene (6); missing map entry (6); exe hash mismatch (7); data hash mismatch
(4); identical comparison (0); Compare overwrite refusal (5); different comparison (9); forbidden
log rejected (9); missing comparison run (6); Compare inside the data tree (2); Compare equals the
data root (2); Compare contains the data root (2); Compare inside the exe tree (2); and two
refusal mutations plus the mutation-anchor check. Each mutant gets a fresh fixture and reports
`sentinel=True`; detection is the deviation from the safe result (exit 5 + sentinel intact).

The issue-compatible launcher `tests/renderer/test-render-baseline.ps1` reruns the same suite:
exit 0, `SUMMARY pass=26 fail=0`. It is a thin delegation to `tests/baseline/**`, not a second test
implementation; this keeps the Q0 tests out of the Q1a-owned `tests/renderer/CMakeLists.txt`.

The suite uses a synthetic executable (never executed), a tiny zip as `demo00.pk4`, and generated
PNGs. It reads no game data and starts no engine. The `Engine` capture path is exercised by the
real final11 run, not by the synthetic suite; process start/stop and the real `-Clean` archive
remain not behavior-tested.

## 7. Verification of the change set

| Check | Result |
|---|---|
| `git diff --check` | exit 0, no output |
| Submodule cleanliness | `git -C doom3_orig/doom3 status --porcelain` empty |
| Tracked game data / builds | none; Q0 outputs are under `build-q0-*`, which is git-ignored |
| Q0 files in `git status` | only the new script, tests and spec documents; no tracked file modified |
| Exit file | moved into `build-q0-verify-20261028/`, so it is under the ignored build tree |
| Absolute machine paths in tracked evidence | none; the external data root is `<D3_DATA_DIR>` and the CMake path uses `$env:ProgramFiles` |
| Compare overwrite | second `Compare` on `baseline-final11` would exit 5 and preserve `comparison.json` |

## 8. Independent review (T011)

Review 1 (2026-10-28) was an independent subagent with a fresh context; verdict **BLOCKED**.
Review 2 (same day) was a second independent pass; verdict **BLOCKED** with new findings. Review 3
was a third pass; verdict **PASS-WITH-FINDINGS**. Those reviews were on the OS-window/blocked
state. After review 3, the fixed-tic engine-framebuffer method changed the capture and produced
`baseline-final10`; review 4 read that state. A comment-only script change then altered the hash, so the same capture was rerun as `baseline-final11` with the current script; final11 is the recorded tree.

| Finding | Disposition |
|---|---|
| R1-B1: Compare overwrote `comparison.json`. | Fixed: Compare refuses an existing report (exit 5); test checks the hash is unchanged. |
| R1-B2: Mutation tests reused a moved fixture. | Fixed: fresh fixture per mutant; safe result = exit 5 + sentinel; archive gated on `Capture/All` and `-Clean`. |
| R1-B3/B4: Wrong recorded values / hidden calibration. | Fixed: the current result is final11; the scene spec discloses the failed OS-window runs. |
| R1-B5: Review section self-attested. | Fixed: all reviews and verdicts are recorded. |
| R1-M1: OutDir inside data/exe not refused. | Fixed: both directions; tests cover inside/equals/parent data and inside exe. |
| R1-M2: Refusal after the 483 MB hash. | Fixed: preflight before exe/data. |
| R1-M3: Capture raced the drive; black+HUD could pass. | Fixed: `SCENE_READY` and centre-luminance; the final method removes the OS-window race entirely. |
| R1-M4: Capture mode untested. | Partly fixed: Capture refusal and fake-PE rejection are tested; real process/`-Clean` remain untested. |
| R1-M5–M10: output-tree wording, absolute VS path, citation, corridor evidence, host identity, command line. | Fixed or disclosed in the current documents. |
| R2-B4: Calibration table misattributed the open pass. | Fixed: the scene spec lists the OS-window failures and the fixed-tic switch separately. |
| R2-M1: data-tree test false positive. | Fixed: no `-OutRoot`; added equals/parent/exe-tree cases. |
| R2-M2: capture not tied to a script revision. | Fixed: manifest records the script/test/launcher hashes; final11 matches. |
| R2-M3/M4: stale launcher log / unignored exit file. | Fixed: launcher rerun 26/26; exit file moved into the ignored build tree. |
| R3: host timestamp, contract wording, resolution enforcement, commandLine, calibration, bookkeeping. | Fixed or disclosed; resolution enforcement remains a gap (§9). |

Review 4 (2026-10-28) was a fourth independent pass over the fixed-tic final10 state. After review 4, a comment-only script change altered the hash; final11 is the rerun with the current hash. Verdict:
**PASS-WITH-FINDINGS**. Findings and dispositions:

| Review 4 finding | Disposition |
|---|---|
| F1: the corridor/control reclassification was not disclosed. | Disclosed in the scene spec §6 and §5 above: final9 used a closed door on the same map; final11 uses 600 tics after the doors open and shows the corridor. The control scene was removed, not relabeled. |
| F2: the final11 ceilings were tightened after the probes, so "pre-declared" was overstated. | Disclosed: the earlier final9 ceilings and the final11 ceilings are both listed; final11 also passes the earlier looser cells. The wording is now "frozen before the recorded run". |
| F3: the TGA bit depth was documented as 24-bit; the engine can write 32-bit. | Corrected to "24- or 32-bit"; the decoder supports both and the recorded files decode correctly. |
| F4: `tasks.md` still said Q0 was not established and SC-001 not met. | Fixed: `tasks.md` and `plan.md` now record the final11 result and the SC-001 status. |
| F5: the guards are weak as an automated regression gate. | Disclosed: this is a host-specific human-reviewed baseline, not a gate; a future regression gate should tighten the thresholds. |
| F6: the manifest `commandLine` is reconstructed and says `mode=Capture` for `-Mode All`. | Disclosed in §5 and the procedure; the replay command is the block above. |
| F7: stale review launcher log / ARB vs ARB2. | The stale log is not cited; `build-q0-test-launcher.log` is the 26/26 run. The renderer line is corrected to ARB/ARB2. |
| F8: known gaps (resolution enforcement, engine path unit test, process/`-Clean`). | Already in §9; not claimed as passing. |

T011 is complete with these residual gaps: the baseline is valid for this host/demo/GL renderer,
not a retail or performance claim; resolution enforcement and real-process/`-Clean` tests remain
future work.

## 9. Not proven / next work

- **Retail coverage**: none. The demo is the only lawful data; four category frames exist but not
  the full retail maps.
- **Classic parity, modern quality, performance**: out of scope. The engine reports Low quality on
  a virtual adapter; no FPS or GPU timing claim is made.
- **Fixed-tic dependence**: `com_fixedTic 1` is what makes the frames repeat; a different tic
  count or a different engine build may move the scene. The recorded tic counts and executable
  hash are part of the identity.
- **Resolution enforcement**: `Compare` records 1280x720 but does not yet reject a pair whose size
  differs; the synthetic compare fixtures use 64x64. The recorded final11 is 1280x720.
- **Engine capture path testing**: the synthetic suite does not run the engine; TGA decoding and
  the `+screenshot`/`+quit` path are exercised by final11, not by a unit test.
- **Process start/stop and real `-Clean` archive**: not behavior-tested.
- **Binary reproducibility**: two builds differ in hash. Record the captured hash; do not use a
  hash to identify "the same build" across rebuilds.
- **Evidence schema**: the issue #20 evidence template is not yet on `main`; reconcile this file
  when it lands.
