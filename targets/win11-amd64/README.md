# targets/win11-amd64

Native Windows 11 x64 build of DOOM 3 (VS2022 v143, Windows SDK 10.0.26100, static CRT).
The engine tree is a git-ignored copy (`doom3/`, created by `scripts/new-target.ps1`); this
directory and the patch files are tracked.

## Prerequisites

- Visual Studio 2022 Professional with the **Desktop development with C++** workload
  (toolset v143) and Windows SDK **10.0.26100.0**. No MFC, DirectX June 2010 SDK, OpenAL SDK,
  or VS2010 is required.
- The CMake and Ninja bundled with VS2022 (CMake 3.31 in the tested install).
- The target copy: from the repository root,
  `powershell -ExecutionPolicy Bypass -File scripts/new-target.ps1 -Name win11-amd64`.

## Configure and build

From this directory, with the bundled CMake (or any CMake >= 3.20 on `PATH`):

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake --preset win11-amd64 .
& $cmake --build build --config Release -- /m
```

Output: `build/Release/DOOM3.exe` (x64, `/MT`, 16 MB stack). The build directory is
git-ignored by the repository-level `.gitignore` (`targets/*/build/`).

## Source lists

`sources.cmake` is generated from the copy's own `idlib.vcxproj`, `doomdll.vcxproj` and
`game.vcxproj`, so the build covers exactly the translation units the original project builds.
Regenerate it after a submodule bump or a target-copy refresh:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/gen-sources-win11-amd64.ps1
```

Excluded: `tools/**` except `tools/compilers/aas/AASFile.cpp` and `AASFileManager.cpp` (the
`AASFileManager` service the engine and game use), `curl/**`, `MayaImport/**`, `openal/**`,
`typeinfo/**`, and `idlib/math/Simd_AltiVec.cpp` (excluded in every configuration by the
project itself). The MSVC SIMD asm TUs (`Simd_MMX/3DNow/SSE/SSE2/SSE3.cpp`) stay in the list;
the x64 guards inside the target copy make them empty translation units.

## Build shape

- One executable: engine TUs plus every `game/*.cpp` compiled with neither `__DOOM_DLL__` nor
  `GAME_DLL`, so the game is statically linked (no `gamex86.dll` lookup).
- Defines: `WIN32 _WINDOWS _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE __DOOM__
  ID_ENABLE_CURL=0 ID_NO_TOOLS`; `NDEBUG` in Release. `_USE_32BIT_TIME_T` is never defined.
- Per-file: `sys/win32/win_input.cpp` is compiled with `/source-charset:.1252`.
- Libraries: `dbghelp dinput8 dsound dxguid glu32 iphlpapi opengl32 winmm ws2_32 wsock32
  comctl32 version imm32`. Not linked: `DxErr`, `eaxguid`, `odbccp32`, `openal32`, MFC.

## MFC removal (this target's edits)

Part of the single exported patch `patches/all-changes.patch`:

- `idlib/precompiled.h` - drop the `tools/comafx/StdAfx.h` include from the precompiled header.
- `framework/BuildDefines.h` - `ID_ALLOW_TOOLS` is not defined when `ID_NO_TOOLS` is set.
- `sys/win32/win_taskkeyhook.cpp` - include `<windows.h>` instead of `<afxwin.h>`.

## Known state

Source edits owned by the other workers (SIMD guards, `_alloca16`, Heap, `sys/win32/*` asm,
pointer-width fixes, game event args) land in the same target copy; until they are all in, the
build reports their files and they are listed in the PR instead of being patched here.

## Run and verify

Game data is never committed. The only data used for tests is the official free DOOM 3 demo
(`D3Demo.exe`, id Software, 2004; licence: free non-commercial use, no modification or
redistribution of the demo data). Extract `demo00.pk4` from the installer yourself, then:

```powershell
# builds a run directory outside git: base/demo00.pk4 (hard link) + 2 local compatibility shims
powershell -ExecutionPolicy Bypass -File scripts/prepare-demo-data.ps1 -Pk4 <path>\demo00.pk4 -RunDir <path>\run
# build + start the demo's first map + screenshot
powershell -ExecutionPolicy Bypass -File scripts/run-win11-amd64.ps1 -DataDir <path>\run -Build
# all success criteria (PASS / FAIL / SKIPPED)
powershell -ExecutionPolicy Bypass -File scripts/verify-win11-amd64.ps1 -DataDir <path>\run
```

The two shims exist because the 2011 GPL code expects `guis/mainmenu.gui` and a
`WEAPON_NETFIRING` script field that the 2004 demo does not ship. They are local test files.

## Patches

`scripts/export-target-patches.ps1 -Name win11-amd64` writes `patches/all-changes.patch` (one
`git apply -p2`-able patch against the pristine tree) and `all-changes.manifest.txt`
(status, path, SHA-256). `verify-win11-amd64.ps1` re-applies it to a fresh copy and compares hashes.

## Known open defects (x64 port is a baseline, not finished)

- Save games: `sound/snd_world.cpp` `WriteInt((int)ch->soundShader)` style pointer writes and other
  pointer-to-int serialisation are not x64-safe; saving/loading is untested.
- `Sys_GetCallStack`, x87 FPU state queries and the crash dialog are x64 stubs.
- Networking/multiplayer and the retail `d3xp` expansion were not exercised (see `doom3_orig/doc/07_*` and `08_*`).
- Sound: DirectSound stereo output verified audible with `scripts/audio-peak.ps1`; it only plays while the window has focus.
- Fonts/GUI cache (`.gui` binary cache) round-trip of 8-byte `wexpOp_t` is untested.
- Demo recording/playback (`recordDemo`/`playDemo`): `renderer/RenderWorld_demo.cpp` and `renderer/GuiModel.cpp`
  serialise pointers as 32-bit and read them into 8-byte members; playback will corrupt them. Not exercised.
- Event save format: `idEventArg` slots widened to pointer size without a save-game version bump; x64 saves are
  incompatible with 32-bit saves.
- Cached binary `.gui` expression ops (`wexpOp_t.a`) are read as 32-bit and re-fixed after load; untested.