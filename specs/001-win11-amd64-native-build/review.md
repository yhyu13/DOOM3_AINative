# Independent review — 001 win11-amd64 native build

- **Reviewer role**: independent (did not write this change). Read-only: no file in the worktree was
  edited; the only write is this report.
- **Worktree**: `D:\git-repo-ai\DOOM3_AINative\.kilo\worktrees\feat-1-win11-amd64-native-build`
  (branch `feat/1-win11-amd64-native-build`, HEAD `09b6e7d docs: x64 port findings (#1)`).
- **Pristine**: `D:\git-repo-ai\DOOM3_AINative\doom3_orig\doom3`
- **Target copy**: `targets\win11-amd64\doom3` (git-ignored, created by `scripts/new-target.ps1`)
- **Date**: 2026-10-26

## What was inspected / run

- Read `AGENTS.md`, `specs/001-win11-amd64-native-build/{spec,plan,tasks}.md`,
  `doom3_orig/doc/06_win11_amd64_port_findings.md`.
- `git diff --no-index` on every changed file: 59 modified, 0 added, 0 removed
  (`scripts/diff-target.ps1` → `Pristine files 2156 / Target files 2156 / Modified 59 / Added 0 /
  Removed 0`).
- Real hunks read for: `sys/win32/*`, `sys/sys_public.h`, `sys/sys_local.*`, `idlib/Heap.cpp`,
  `idlib/math/{Math.h,Simd*.cpp,Matrix.cpp,Vector.cpp}`, `game/gamesys/{Class,Event}.cpp`,
  `game/gamesys/{Class,Event}.h`, `game/gamesys/Callbacks.cpp`, `game/script/*`,
  `ui/Window.{cpp,h}`, `ui/BindWindow.cpp`, `renderer/{Cinematic.cpp,RenderSystem.h,RenderSystem_init.cpp,tr_font.cpp}`,
  `sound/{snd_system,snd_world}.cpp`, `framework/{Common,FileSystem,BuildDefines}.cpp/.h`,
  `idlib/{Lib.cpp,precompiled.h}`, `sys/win32/win_taskkeyhook.cpp`, and the 12 `d3xp/*` mirrors.
- Target metadata: `targets/win11-amd64/{CMakeLists.txt,CMakePresets.json,README.md,sources.cmake,patches/*}`,
  `src/eaxguid_x64.cpp`, all new `scripts/*win11-amd64*.ps1`, `capture-window.ps1`,
  `export-target-patches.ps1`, `prepare-demo-data.ps1`, `diff-target.ps1`, `gen-sources-win11-amd64.ps1`.
- Executed: `scripts/verify-win11-amd64.ps1 -SkipSmoke` → `SUMMARY pass=5 fail=0 skipped=1`, exit 0;
  `scripts/diff-target.ps1 -Name win11-amd64` (read-only).
- Inspected artifacts: `build/Release/DOOM3.exe` (16,690,688 B, 22:43:49; PE machine `0x8664`),
  `build/run/{screenshot.png,qconsole.log}`, `build/smoke/{screenshot.png,qconsole.log,save/base/*}`.
- The 15-minute build was **not** re-run.

## Verdict

The x64 port is coherent and the engine demonstrably runs the demo map on x64 (1969 entities spawned,
`SpawnPlayer: 0`, in-game screenshot). The pointer-width work is mostly correct and careful. What is
**not** ready is the *evidence and reproducibility layer*: the documented setup never applies the
patch, SC-003 is not actually asserted, and demos are still pointer-truncated. Nothing found forces a
change to the successful build.

**Counts: BLOCKER 1 · MAJOR 4 · MINOR 6 · NIT 5.**

---

## 1. AGENTS.md non-negotiables

| # | Requirement | Result | Evidence |
|---|---|---|---|
| 1 | Pristine submodule untouched | **PASS** | `git -C doom3_orig/doom3 status --porcelain` → empty; target copy has no `.git` |
| 2 | One copy per target | **PASS** | only `targets/win11-amd64/doom3`, 2156 files, no shared gitdir |
| 3 | No game data / binaries in tracked paths | **PASS** | `git ls-files` matches none of `pk4 pk3 pak exe dll obj lib pdb`; `*.log`, `build*/`, `*.exe`, `*.pk4` are git-ignored |
| 4 | No direct commits to main / no stash | **PASS** | branch `feat/1-...`, no stashes |
| 5 | Evidence before victory | **PARTIAL** | build artifact + run logs exist, but see M1/M2/M3 (SC-003 not asserted, stale log copy, no final build log) |
| 6 | No secrets / machine-specific absolute paths in tracked files | **PASS (1 NIT)** | `git grep` for `D:\`/`C:\Users` → none; one example path in `smoke-win11-amd64.ps1:11` help text (n4) |
| 7 | One writer per file | **PASS** | changes respect plan.md:46-54 ownership by area |

Note: **none of the 59 file changes, the target metadata, or the new scripts are committed** — the
branch has only the two `docs(...)` commits; `git status -uall` shows 14 untracked entries. That is
folded into M3.

## 2. Correctness of the x64 fixes (spot verdicts)

| Area | Change | Verdict |
|---|---|---|
| `sys/sys_public.h` | `address_t`/`sysDLLHandle_t` pointer-sized; `CPU_EASYARGS 0` on x64; `_alloca16` via `uintptr_t` (all 3 platform variants) | **Correct.** 32-bit values unchanged. `BUILD_STRING` left `"win-x86"` (n1) |
| `sys/win32/win_shared.cpp` | raw-COM WMI rewrite; `SymLoadModule64`/`SymGetSymFromAddr64`; `DWORD64`; `address_t` module base; x64 call-stack stub | **Correct** for the debug symbol path. `AdapterRAM` still read as `intVal` (m1) |
| `sys/win32/win_main.cpp` | SEH/chkstk gated to x86; `_findfirst` handle `intptr_t`; `_fstat64`/`ID_TIME_T=time_t` | **Correct**; `ID_TIME_T` is `time_t` (`precompiled.h:36`), so the cast is safe |
| `sys/win32/win_cpu.cpp` | `__rdtsc`/`__cpuidex`/`_mm_getcsr`, `DWORD_PTR` affinity, `DWORD ProcSpeed`, no-op FPU ops | **Correct.** `Sys_FPU_EnableExceptions` is a no-op on x64 (only `TEST_FPU_EXCEPTIONS=0` is used) |
| `idlib/Heap.cpp` | `Allocate16`/`Free16` store a `void*` trailer; `Large*`/`Msize` page pointer; `uintptr_t` page align; `Msize` header math (`LARGE_HEADER_SIZE = sizeof(dword*)+1` → aligned 16) | **Correct** (header offsets verified to not overlap the `d[-1]` type byte). `USE_LIBC_MALLOC=1` on 64-bit additionally bypasses the small-heap `dword` free-list, which remains unsafe if forced to 0 (acknowledged in the comment) |
| `idlib/math/*` | `tempPtr` via `uintptr_t`; `SinCos` portable; SIMD generic only; `__asm` TUs empty on x64 | **Correct.** `FtoiFast` semantics differ from x86 (m2) |
| `game/gamesys/{Class,Event}.*`, `Callbacks.cpp` | `intptr_t value`/arg slots; `GetArgSize` slot sizes; float args via `*(float*)&data[i]`; entity via `idEntityPtr`; trace material flag + name | **Correct and ABI-sound.** Each non-float slot is passed as `intptr_t` in the same positional class as the real parameter, so x64 register assignment matches; 510 generated cases preserved. Entity slot is 8 B for a 4 B `idEntityPtr` (n3) |
| `game/script/Script_Compiler.cpp` (+d3xp) | `OP_ADDRESS` rewrites `statement.c` to a fresh `AllocDef(&type_pointer, RESULT_STRING, scope, false)` temp | **Correct.** `type_pointer` is `sizeof(void*)`; the fresh def is allocated at 8 B so the 8-byte pointer store no longer overruns a 4-byte neighbour. Costs one permanent 8 B local per address-of expression (acceptable). `OP_INDIRECT_*` loads values only, not pointers, so no sibling bug |
| `game/script/Script_Program.cpp` | `type_entity/object/field/function/scriptevent/namespace = sizeof(int)`, `type_pointer = sizeof(void*)` | **Correct.** Function/scriptevent defs get their `value` overwritten by `SetFunction`, so a 4-byte slot is not truncated |
| `ui/Window.{h,cpp}` | `wexpOp_t.a` `intptr_t`; `EmitOp`/`ParseTerm` pointer-carrying; read path reads int into `wa` | **Runtime-correct** (registers vs. pointer cases are handled in `EvaluateRegisters`). The `ReadFromDemoFile` path is dead code (`WRITE_GUIS` off, `assert(false)`) and its comment is wrong (n2) |
| `renderer/Cinematic.cpp` | `mcomp[256]` `unsigned int` → `int` | **Correct and necessary** — values are `normalBuffer0 - (temp2+temp)` (ptrdiff) and are used as pointer deltas; unsigned gave +4 GiB on x64 |
| `renderer/tr_font.cpp`, `RenderSystem.h` | `FONT_FILE_BYTES`/`FONT_FILE_GLYPH_BYTES` (80 B/glyph) instead of `sizeof(fontInfo_t)` | **Correct** — the loader reads exactly 7×int + 4×float + int + 32 B per glyph; x64 `glyphInfo_t` grew only because of the pointer |
| `framework/{Common,FileSystem}.cpp` | `gameDLL` `intptr_t`; `_fileno(FILE*)` instead of private `FILE::_file` | **Correct** |
| `framework/BuildDefines.h`, `idlib/precompiled.h`, `win_taskkeyhook.cpp` | `ID_NO_TOOLS`, drop `StdAfx.h`, `windows.h` instead of `afxwin.h` | **Correct**; no MFC TUs remain |
| `idlib/Lib.cpp` | `__debugbreak()` instead of `int 3` | **Correct** |
| `d3xp/*` mirrors | 12 files, same changes | **Consistent** with `game/*` (spot-checked `Event.h/Class.cpp/Event.cpp/Script_Compiler.cpp`) |
| CMake | single exe, `/MT`, 16 MB stack, `ID_NO_TOOLS`, `ID_ENFORCE_KEY=0` (`#if ID_ENFORCE_KEY`, so 0 is right) | **Correct**; `sources.cmake` is a hard requirement (CMakeLists.txt:27-33) but is not in the plan's tracked list (M3) |

**32-bit behaviour:** the changes that are `#if _WIN64`-gated (Heap, CPU, SEH, SIMD, `$`-casts) leave
x86 paths intact; `E_EVENT_SIZEOF_VEC`/`idEventArg`/`wexpOp_t` collapse back to their old x86 sizes.
Two intentional x86-visible edits: `sprintf("%016llx")` in logs (cosmetic) and the new
`D_EVENT_STRING` savegame case (format change, m3).

## 3. Remaining 64-bit truncation hazards not fixed

| Severity | Site | Effect / fix |
|---|---|---|
| MAJOR | `renderer/RenderWorld_demo.cpp:94,143,493-498,634-659` (`ReadInt((int&)ptr)`), `renderer/GuiModel.cpp:93,143`, `ui/Window.cpp:3277` | `recordDemo`/`playDemo` serialise pointers as 32-bit and read them back into 8-byte members (`light.shader`, `ent.hModel`, `ent.gui[i]`, `ent.joints`, surface material) → truncated pointers on playback. Either fix with a string/spawn-id encoding or list demo playback explicitly in the "open defects" paragraph (FR-004) |
| MINOR | `sound/snd_world.cpp:1200,1202,1203` write / `1371-1374` read | 3 pointer fields archived as int; the reader leaves the high 4 bytes stale. Mitigated because the caller immediately re-resolves decoder/shader/sample from strings (`snd_world.cpp:1306-1360`), but it is UB and can spuriously allocate a decoder when the field was NULL. Read into an `int` temp and use it only as a presence flag |
| MINOR | `sys/sys_public.h:455` (`int netSocket`), `sys/win32/win_net.cpp:243,306` | `SOCKET` is `UINT_PTR`; truncated to `int`. Out of scope per spec, but this is a pointer-width cast |
| MINOR | `idlib/Heap.cpp:1242-1253` | `debugMemory_t` is 88 B on x64, not "a multiple of 16"; `Mem_Alloc16` under `ID_DEBUG_MEMORY` returns an 8-mod-16 pointer (assert at `Heap.cpp:1126`/`1699`). Dead today (`ID_DEBUG_MEMORY` commented out) — add `alignas(16)`/padding |
| NIT | `ui/Window.cpp:1721/1726`, `2105-2117`, `SimpleWindow.cpp:261/266` | `(int)&((idWindow*)0)->rect` — member offsets, not pointers; safe |

Clean: no `reinterpret_cast<int>(ptr)` remains; no active inline asm remains (the build proves it).

## 4. Spec success criteria — evidence

| SC | Verdict | Evidence |
|---|---|---|
| SC-001 build exits 0 from the target copy | **PASS (partial evidence)** | `build/Release/DOOM3.exe`, 16,690,688 B, 22:43:49; `verify` SC-002 PASS. **No captured log/exit code for the final successful build** — the only `*.log` files are older runs, and `build-release-3.log` contains 23 `error C` lines (superseded; git-ignored) |
| SC-002 exe > 1 MB, PE `0x8664` | **PASS** | header check `machine=0x8664`, 16.69 MB |
| SC-003 menu ≤120 s, 1280×720 non-flat screenshot | **FAIL (literal)** | menu is rendered (image) and non-flat, but the artifact is **1296×759** (window incl. border; client is 1280×720), smoke never asserts the menu/log, its default `+devmap` loads a map, and the copied log is stale (M1/M2) |
| SC-004 pristine porcelain empty after build+run | **PASS** | verified now: 0 lines; `diff-target` 59 M / 0 A / 0 D |
| SC-005 verify exits 0 and prints PASS/SKIPPED | **PASS** | ran `-SkipSmoke`: `pass=5 fail=0 skipped=1`, exit 0 |
| SC-006 every change in a patch; fresh copy rebuilds | **PARTIAL** | 59 M rows; `git apply -p2` reproduces all modified-file hashes (`SC-006b PASS`); no added/removed files today. **Not proven**: no apply step in setup, no build of the patched copy, `git apply` exit code ignored (B1) |

## 5. Task evidence (tasks.md is entirely unchecked)

| Task | Evidence |
|---|---|
| T000 demo data prep (`prepare-demo-data.ps1`, licence/SHA note) | script present; README has a short licence note; **no SHA-256 of the demo archive anywhere** |
| T001/T010/T011/T012/T013/T016/T017 build scaffold, MFC out, sources list, RC/version, TypeInfo drop | present and building. T010 says "idlib static + doom3 exe" but the build is one exe (no idlib target); T016 needs no change (Release uses `NoGameTypeInfo.h`, `TypeInfo.cpp` unchanged) |
| T002/T003/T005/T007/T008/T009/T014/T015 x64 host layer, static game link, SIMD generic, CMake config | evidence = successful build + `qconsole.log: "game DLL: 0x0 in pak: 0x0"`. No separate per-task artifact |
| T018 PE/report checks | `verify` SC-002 PASS |
| T019/T020/T021/T022/T023 menu, log, screenshot, demo map, GL fallback | menu image; `build/run/qconsole.log` (opened 22:45:48) shows GL renderer init, `1969 entities spawned`, `SpawnPlayer: 0`; `build/run/screenshot.png` (656×519) is the in-game view. GL fallback not needed (not recorded as a decision) |
| T030/T031 verify/smoke scripts | present, run |
| T032 pristine untouched + only intended files | PASS (porcelain empty; 59 M/0 A/0 D over 2156 files) |
| T033 export patch + reproduce | export PASS; reproduction only by hash, not by build |
| T034-T036 cleanup/ignore/log hygiene | PASS (logs/builds ignored, 2560×1440 note N/A) |
| T040 self-review, T041 independent review, T042 PR | T040 no artifact found; T041 = this review; **T042 not started — nothing is committed** |

## 6. Findings

### BLOCKER

**B1 — SC-006: the documented setup cannot produce the tree that builds.**
`targets/win11-amd64/README.md:4,14,22` says "create the copy with `new-target.ps1`, then configure".
`scripts/new-target.ps1:64` copies pristine and stops; no script applies
`patches/all-changes.patch` in place, and the README never mentions applying it. A fresh reader gets a
pristine x64-incapable tree. `verify-win11-amd64.ps1:40-50` applies the patch only inside a throwaway
temp dir, ignores `git apply`'s exit code (line 46; `$ErrorActionPreference='Continue'`) and only
compares hashes of `M` rows (lines 53-59) — the `A`-row comment at line 48 is unimplemented (no added
files today, but it will silently pass once there are).
*Fix:* add `scripts/apply-target-patches.ps1` (or `new-target.ps1 -Patch`) with
`git apply -p2 --whitespace=nowarn`, check `$LASTEXITCODE`, add the step to README §Setup, and make
`verify` fail when `git apply` fails; ideally configure+build the patched copy (or state in the spec
that SC-006 is satisfied by source identity and amend SC-006 — that needs a spec change record).

### MAJOR

**M1 — SC-003 is not actually asserted.**
`smoke-win11-amd64.ps1:81-90` exits 0 when a window wider than 100 px contains ≥8 distinct colours; it
never reads the log and never checks for the main menu. `verify:65-73` turns that exit code into
`SC-003 PASS`. The script's default `-ExtraArgs '+devmap game/demo_mars_city1'` (line 18) loads a map,
which contradicts "reaches the main menu". Today the menu image only exists because the capture at
22:50:31 happened before the map appeared.
*Fix:* default `ExtraArgs` to empty for SC-003; assert `qconsole.log` contains `GL_RENDERER`,
`reloading guis/mainmenu.gui` (or `--------- Initializing ...` + no `Fatal`/`ERROR`), and capture
`GetClientRect` (see m-screenshot) so the PNG is exactly 1280×720.

**M2 — smoke's captured log is from a different run.**
`smoke-win11-amd64.ps1:86` does `Get-ChildItem -Recurse -Filter qconsole.log | Select -First 1`, i.e.
the first file in directory order, and `:87` skips the copy when that happens to be the existing
`OutDir\qconsole.log`. Result: `build/smoke/qconsole.log` (5,318 B) was opened at **22:47:47** and is a
menu-only log from an earlier run, while the run behind `screenshot.png` (22:50:31) wrote
`build/smoke/save/base/qconsole.log` (24,502 B, opened **22:50:14**, map load). Screenshot and log
cannot be tied together.
*Fix:* read the known path `Join-Path $OutDir 'save\base\qconsole.log'` (the script sets
`fs_savepath=$OutDir\save`), or sort by `LastWriteTime -Descending` and always copy.

**M3 — traceability: nothing is committed, tasks are unchecked, and the tracked-file list omits
build inputs.**
`tasks.md:5-37` is all `- [ ]`; HEAD only contains the two docs commits; all 59 source edits and the
new scripts/target metadata are untracked. `plan.md:38-44` lists only
`CMakeLists.txt, CMakePresets.json, README.md, patches/*.patch` and the three scripts — it omits
`targets/win11-amd64/sources.cmake` (hard requirement, `CMakeLists.txt:27-33`),
`targets/win11-amd64/src/eaxguid_x64.cpp` (`CMakeLists.txt:45`), `patches/all-changes.manifest.txt`,
and the helpers `run`, `capture-window`, `prepare-demo-data`, `gen-sources` and `diff-target`
(referenced by `tasks.md:19`). A PR staging only the documented list will not build.
*Fix:* mark tasks with their evidence, update the plan's tracked list, and commit the whole set on the
branch before the PR.

**M4 — demo record/playback is still pointer-truncated (FR-004 not fully satisfied).**
See §3 first row. FR-004 allows "recorded as open defects", but `README.md:63-72` names only save
games; `playDemo` is a normal engine command and will dereference truncated pointers.
*Fix:* encode `renderEntity_t`/`light` references by name/spawn-id (like `trace.c.material` already
does), or add demo playback to the open-defect list with a one-line rationale.

### MINOR

- **m1** `sys/win32/win_shared.cpp:168` — `retSize = varSize.intVal / (1024*1024)` on a `VT_UI4`
  property. With ≥2 GB VRAM `intVal` is negative, so the `retSize == 0 → 64` fallback is bypassed and
  a negative MB count is reported. Use `varSize.ulVal` (guard `varSize.vt == VT_UI4`) and clamp.
- **m2** `idlib/math/Math.h:799-804` — x64 `FtoiFast`/`FtolFast` are `(int)f` / `(unsigned long)f`
  (truncate) whereas x86 uses `fistp` with round-to-nearest. Used by `ANGLE2SHORT`/`ANGLE2BYTE`,
  `DeclParticle`/`ModelDecal` colours, AAS routing distances and player heart rate → 1-unit divergence
  from the 32-bit baseline. Use `_mm_cvtss_si32(_mm_set_ss(f))` (honours MXCSR, matches `fistp`).
- **m3** `game/gamesys/Event.cpp:95,101,294` (+ `Event.h`) — event arg slots widen (int 4→8, entity 4→8,
  vector 12→16) and `D_EVENT_STRING` args are now archived (`:650-656`, `:753-759`), with no savegame
  version bump. Self-consistent within x64, but savegames are x64-only and the x86 format changes too.
  Document the format change (README known state) or version `idSaveGame`.
- **m4** `targets/win11-amd64/README.md:39-41` — claims `tools/**` is excluded except two AAS files;
  in fact 33 `tools/compilers/**` TUs are compiled (e.g. `sources.cmake:253 dmap/dmap.cpp`; `dmap.obj`
  and `AASBuild.obj` are in the link). Exclude them or correct the README (the generator header says
  "except tools/compilers/**", so the README is the wrong one).
- **m5** `sys/sys_public.h:455` — `int netSocket` truncates `SOCKET` (see §3).
- **m6** `idlib/Heap.cpp:1242-1253` — `debugMemory_t` alignment (see §3).

### NIT

- **n1** `sys/sys_public.h:46` — `BUILD_STRING` is still `"win-x86"` on x64 (`CPUSTRING` is `"x64"`).
- **n2** `ui/Window.cpp:3277` — comment claims "pointer ops are re-fixed up after load"; there is no
  fixup, and the function is dead (`WRITE_GUIS` off, `assert(false)` at `:3168`). Misleading.
- **n3** `game/gamesys/Event.cpp:101,294` — 8-byte slot for a 4-byte `idEntityPtr`; use
  `sizeof(idEntityPtr<idEntity>)` (4 on x86, 4 on x64) to keep the arg table tight.
- **n4** `scripts/smoke-win11-amd64.ps1:11` — help text embeds `D:\data\doom3`; use `<data-root>` like
  the other scripts (AGENTS #6).
- **n5** Stale docs: `targets/README.md:22` documents `diff-target.ps1 -NameStatus`, which does not
  exist (only `-Detailed`); `targets/win11-amd64/README.md:63` "Known state" predates the smoke flow.

## 7. Things done well (do not regress)

- The `OP_ADDRESS`/`type_pointer` analysis is right and the fresh-def fix is safe; `type_pointer =
  sizeof(void*)` with the other script value types kept at 4 bytes is the correct split.
- Container-aware `_alloca16` and `tempPtr` fixes cover the 236 call sites from doc06 §5.
- `Callbacks.cpp` regeneration preserves all 510 formatspec cases and correctly separates float XMM
  args from `intptr_t` integer args; `CopyArgs`/`ServiceEvents`/`CallArgs` keep one consistent
  representation.
- Heap large-block header math and the `Allocate16` trailer are symmetric and were checked against
  `LARGE_HEADER_SIZE = sizeof(dword*)+1`.
- MFC removal is minimal (3 files) and the `d3xp` mirrors are exact.

## 8. Limits of this review

- No rebuild was performed (per instructions); `run`/`smoke` evidence was taken as-is.
- Save/load and demo paths were analysed statically (no savegame was produced in the worktree).
- `Sys_FPU_SetRounding` relies on `_controlfp_s` also updating MXCSR on x64; not probed at runtime.
