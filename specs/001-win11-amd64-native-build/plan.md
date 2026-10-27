# Implementation Plan: Native Windows 11 x64 build of DOOM 3

**Branch**: `feat/1-win11-amd64-native-build` | **Spec**: [spec.md](spec.md) | **Issue**: #1
**Evidence**: `doom3_orig/doc/06_win11_amd64_port_findings.md` (compile probe: idlib 44/44 and exe 197/197 TUs compile on x64 after 6 edits)

## Summary
Build the target copy `targets/win11-amd64/doom3` with VS2022 (v143), Windows SDK 10.0.26100, x64, via CMake.
Disable MFC tools, force the generic SIMD path, fix pointer truncation, drop DirectX SDK (June 2010) / TypeInfo prebuild,
statically link the game module. Test with the official free demo data (legal, kept outside git).

## Technical Context
- Language: C++ (2004-era, MSVC dialect), CMake >= 3.20 (bundled with VS2022), Ninja or VS generator
- Platform: Windows 11 (10.0.22631) x64, Intel integrated GPU, legacy OpenGL via system driver
- Test: PowerShell smoke script + Playwright-less screenshot (desktop window capture via .NET `CopyFromScreen`/`PrintWindow`)
- Constraints: pristine submodule read-only; changes exported as patches; no game data in git

## Constitution Check
| Principle | Status |
|---|---|
| I Pristine read-only | PASS: all edits in target copy, exported to `targets/win11-amd64/patches/` |
| II Evidence | PASS: verify script prints PASS/FAIL/SKIPPED |
| III One issue/worktree/PR | PASS: issue #1, this worktree |
| IV Legal | PASS: demo data under untracked `_data/`, never committed |
| V Fallbacks | PASS: generic SIMD replaces asm; recorded in tasks |

## Design decisions
| Decision | Choice | Why |
|---|---|---|
| Build system | New CMake in `targets/win11-amd64/` (outside engine tree, trackable) | No CMake exists upstream; keeps engine tree edits minimal |
| Game module | Static link into exe | Avoids hard-coded `gamex86.dll` name and DLL handle truncation (doc 06 §7.3) |
| SIMD | Generic only | 1,613 asm tokens are x86 MSVC-only; generic path exists |
| Tools | `ID_NO_TOOLS`, keep `AASFile*.cpp` | MFC not installed; game needs AASFileManager |
| curl/OpenAL/EAX | disabled | not needed for boot |
| Renderer | ARB2 path first; fallback decision if `glprogs` missing | `glprogs/*.vfp` are not in the GPL release but ship inside pak (`pak000.pk4`) |
| Data | official demo | free, legal; license recorded |
| Entry point | Root `CMakeLists.txt` + `CMakePresets.json` that `add_subdirectory(targets/<name>)` | One documented command for every developer (US4); `targets/<name>` stays buildable alone |
| Bootstrap | Root configure runs `new-target.ps1` + `apply-target-patches.ps1` (and `git submodule update`) when the copy is missing | The copy is git-ignored, so a fresh clone has nothing to build otherwise; patches are hash-verified |
| Output dir | `CMAKE_RUNTIME_OUTPUT_DIRECTORY=<build>/bin` | Same path for every generator: `build/bin/Release/DOOM3.exe` |
| Exe discovery | `scripts/_common.ps1` `Resolve-Doom3Exe` (`D3_EXE` > newest of root / stand-alone output) | Scripts keep working with either build layout |
| Patch export | `scripts/export-target-patches.ps1` diff vs pristine | reproducible, reviewable (FR-007) |

## Source layout (tracked)
```
specs/001-win11-amd64-native-build/{spec,plan,tasks}.md
CMakeLists.txt, CMakePresets.json (repo root), targets/win11-amd64/CMakeLists.txt, CMakePresets.json, sources.cmake, README.md, src/eaxguid_x64.cpp, patches/all-changes.{patch,manifest.txt}
scripts/_common.ps1, scripts/{verify,smoke,run}-win11-amd64.ps1, scripts/{export,apply}-target-patches.ps1, scripts/{capture-window,prepare-demo-data,gen-sources-win11-amd64}.ps1
```
Untracked: `targets/win11-amd64/doom3/` (copy), `build/` (root build), `targets/win11-amd64/build/` (stand-alone build), `_data/`.

## File ownership (one writer per file)
| Worker | Owns (write) |
|---|---|
| W-build | `targets/win11-amd64/CMakeLists.txt`, `CMakePresets.json`; target copy: `idlib/precompiled.h`, `framework/BuildDefines.h`, `sys/win32/win_taskkeyhook.cpp` |
| W-idlib | target copy: `idlib/math/Simd*.cpp`, `idlib/Heap.cpp`, `idlib/math/{Matrix,Vector}.cpp`, `sys/sys_public.h` |
| W-sys | target copy: `sys/win32/*` (except taskkeyhook), `sys/sys_local.*`, `framework/Common.cpp` |
| W-game | target copy: `game/**`, `d3xp/**` (T7,T8,T10) |
| W-setup | root `CMakeLists.txt`, `CMakePresets.json`, `scripts/_common.ps1` |
| W-scripts | `scripts/*-win11-amd64.ps1`, `scripts/export-target-patches.ps1`, `targets/win11-amd64/README.md` |
| Coordinator | spec/plan/tasks, integration, PR |

## Risks
| Risk | Mitigation |
|---|---|
| Legacy GL on Intel driver fails | record failure; try `r_renderer arb`/`best`; software GL (Mesa llvmpipe) as fallback; documented as open defect not PASS |
| Demo lacks needed pak content / expects x86 gamex86.dll | static game link; dhewm3-style layout check |
| Pointer bugs surface only at runtime | T7 + startup asserts; run demo map |
