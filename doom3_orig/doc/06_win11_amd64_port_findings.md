# 06 — Windows 11 / x64 (VS 2022, MSVC 14.44, v143) port findings

Read-only analysis of `doom3_orig/doom3/neo` plus **empirical compile probes** against the toolchain
actually installed on this machine. All paths are relative to `doom3_orig/doom3/neo/` unless noted.
This document is the only file written to the repository.

Target toolchain (verified on this host):

| Item | Value |
|---|---|
| cl.exe | `19.44.35228.0` (MSVC 14.44.35207), `Hostx64\x64` |
| Windows SDK | `10.0.26100.0` and `10.0.20348.0` both installed |
| Target | `x64` (`_M_X64`, `_WIN64`, LLP64: `long` stays 32-bit, pointer/`size_t` are 64-bit) |
| MFC/ATL | **not installed** (`VC\Tools\MSVC\14.44.35207\atlmfc\include\afxwin.h` absent, `nafxcw.lib` absent) |
| DirectX June 2010 SDK | not installed — and not required (see §6) |

---

## 0. Method

* Census over `*.c/*.cpp/*.h/*.hpp/*.inl`, excluding vendored `curl`, `renderer/jpeg-6`,
  `sound/OggVorbis`, `openal` where noted: **1,340 engine files**.
* Every count below comes from a script (§13), not from memory.
* A **disposable copy** of `neo/` was made under
  `<TEMP>\kilo\d3x64\` and patched there; the repository and the pristine
  submodule were not modified. The probe compiled with the real `cl.exe` above.
* Probe flags mirror the project: `/EHsc /MT /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_DEPRECATE
  /D_CRT_NONSTDC_NO_DEPRECATE /DNDEBUG`, plus `__DOOM__ __DOOM_DLL__ ID_ENABLE_CURL=0`.

---

## 1. Verdict

The port is **feasible and smaller than doc 01/05 implied**, because the failures cluster into four
mechanical buckets and the x64 replacement for almost every one of them is already in the source as
a `#else` branch:

1. **MFC is pulled into every engine translation unit** by `idlib/precompiled.h:61`, not just the
   tools. No engine file outside `tools/` uses an MFC type, so the fix is to delete that include.
   (The tools themselves genuinely need MFC and must be excluded.)
2. **Inline x86 assembly** exists in 11 Windows-relevant files; only 6 need a code change, the other
   5 (`Simd_*.cpp`) just need to be excluded — `Simd_Generic.cpp` is a complete fallback.
3. **Pointer truncation** is concentrated in one macro (`_alloca16`) plus a handful of named sites.
   `_alloca16` alone is used **236 times in 49 files** and truncates the pointer on x64, so it is a
   P0 crash, not a warning to ignore.
4. **Dependencies**: the Windows SDK covers DirectSound/DirectInput/GUIDs; only `DxErr` (DirectX SDK)
   is missing, used by 9 `DXTRACE_ERR` calls. `openal32.lib`/`eaxguid.lib` are not needed at all
   (OpenAL is `LoadLibrary`-ed at runtime and defaults off).

**Proof of feasibility:** with 6 source edits + 1 per-file charset flag (all listed in §10), **idlib
(44 TUs) and 197 of 197 exe TUs compile with 0 errors on x64** — leaving only the four
`sys/win32` files that contain asm and therefore need real x64 code, and the MFC tools.

---

## 2. Compiler facts probed on this host

| Probe | Result | Consequence |
|---|---|---|
| `_USE_32BIT_TIME_T` + x64 | `fatal error C1189: You cannot use 32-bit time_t (_USE_32BIT_TIME_T) with _WIN64` (`ucrt\corecrt.h(634)`) | `_Common.props:11` is a hard build blocker; must be removed |
| `__asm { mov eax, … }` | `error C4235` + `C2065` cascade | every asm site fails |
| `__declspec(naked)` | `error C2485: unrecognized extended attribute` | `win_main.cpp:1516` `clrstk` fails |
| `#include <DxErr.h>` | `fatal error C1083: cannot open include file` | 9 `DXTRACE_ERR` sites fail |
| `dsound.h / dinput.h / mmsystem.h / mmreg.h / ks.h / ksmedia.h` | compile clean | no DirectX SDK needed |
| `WINVER 0x501` + SDK 26100 | compiles clean | no need to raise WINVER |
| `reinterpret_cast<int>(ptr)`, `(int)ptr` | warnings `C4311`+`C4302`, **not errors** | pointer truncations compile silently — must be fixed by review, not by the compiler |

---

## 3. Inline assembly that fails on x64

Census including `_asm` (single underscore), which the old doc-05 count of “1,565 `__asm`” missed:
**1,613 asm tokens in 13 files** (`__asm` alone: 1,565 in 14 files).

### 3.1 Windows x64 build — exact list and fallback

| File | asm tokens | Sites | Why it fails | Fallback |
|---|---:|---|---|---|
| `idlib/math/Simd_SSE.cpp` | 1,433 | whole body under `#elif defined(_WIN32)` (line 624) | MSVC x64 has no inline asm | **Exclude TU**; `idSIMD_Generic` |
| `idlib/math/Simd_SSE2.cpp` | 65 | body under `#elif defined(_WIN32)` (247) | same | Exclude TU |
| `idlib/math/Simd_SSE3.cpp` | 53 (51 are `_asm _emit`) | body under `#elif defined(_WIN32)` (56) | same | Exclude TU |
| `idlib/math/Simd_MMX.cpp` | 8 | body under `#elif defined(_WIN32)` (52) | same | Exclude TU |
| `idlib/math/Simd_3DNow.cpp` | 1 | body under `#ifdef _WIN32` (43) | same | Exclude TU |
| `idlib/math/Simd.cpp` | 14 | RDTSC macros 152–168 under `#ifdef _WIN32` (144) | asm in test/bench code | `#if defined(_WIN32) && defined(_M_IX86)` → falls to the existing `start=0/end=1` stub |
| `idlib/math/Math.h` | 6 | 390 `SinCos` (`_asm`), 443 `SinCos64` (`_asm`), 802–803 `FtoiFast`, 835–836 `FtolFast` | asm in inline functions | 4 × `#if defined(_WIN32) && defined(_M_IX86)`; C fallbacks already exist (`sinf/cosf`, `(int)f`) |
| `idlib/Lib.cpp` | 1 | 580 `__asm int 0x03` | `AssertFailed` trap | `#if _M_IX86 … #else __debugbreak(); #endif` |
| `sys/win32/win_cpu.cpp` | 22 | 60 `rdtsc`, 133 `HasCPUID`, 177–185 `CPUID`, 348 `LogicalProcPerPhysicalProc`, 366 `GetAPIC_ID`, 520 `FXSAVE`, 684/703/733/832/856/878 x87 env, 898/919 `_asm STMXCSR/LDMXCSR` | CPUID + x87 control words | x64 branch: `__rdtsc()`, `__cpuidex()`, `HasCPUID→true`, `GetLogicalProcessorInformation` or `1`/`0`, `_controlfp_s`/`_mm_getcsr`/`_mm_setcsr`; or minimal `Sys_GetCPUId()→CPUID_GENERIC` + FPU no-ops |
| `sys/win32/win_main.cpp` | 2 | 1516 `__declspec(naked) clrstk`, 1375 (inside `#if 0`) | naked + asm | delete `clrstk`/`HackChkStk`/`TestChkStk` under `#if _M_IX86` (never called) |
| `sys/win32/win_shared.cpp` | 2 | 670 `GetCallerAddr`, 697 `Sys_GetCallStack` (both x86 `ebp` frame walking) | asm + 32-bit frame layout | x64: zero-fill `callStack[]` (Sym code is already a no-op in release); optional `RtlCaptureStackBackTrace` later |
| `sound/OggVorbis/vorbissrc/os.h` | 1 | 117 `vorbis_ftoi` under `#if defined(_WIN32) && !defined(__GNUC__) && !defined(__BORLANDC__)` | vendored libvorbis asm | add `&& !defined(_M_X64)` → existing `(int)(f+.5)` fallback (compiled as C: 154 errors without it) |

Non-Windows-only asm (not compiled here): `sys/osx/macosx_utils.mm` (6), `sys/osx/misc/ftoi.patch`
(4), `sys/linux/main.cpp` (2).

### 3.2 The one-line guard for the SIMD TUs

Each `Simd_*.cpp` already reads `#if defined(MACOS_X) && defined(__i386__) … #elif defined(_WIN32) …
#endif`. Changing `#elif defined(_WIN32)` to `#elif defined(_WIN32) && defined(_M_IX86)` turns the
whole file into an empty TU on x64 without touching a single asm instruction. Preferred, however, is
to drop the five TUs from the project entirely (they are dead weight: 16k LOC that nothing needs).

---

## 4. How SIMD is selected, and how to force `Simd_Generic` on x64

`idlib/math/Simd.cpp`:

* `idSIMD::Init()` (51–56) allocates `idSIMD_Generic`, sets `generic->cpuid = CPUID_GENERIC`, and
  points both `processor` and `SIMDProcessor` at it.
* `idSIMD::InitProcessor(module, forceGeneric)` (63–111):
  1. `cpuid = idLib::sys->GetProcessorId()` (`win_cpu.cpp:Sys_GetCPUId`);
  2. if `forceGeneric` → `newProcessor = generic`;
  3. else, first call picks AltiVec → SSE3 → SSE2 → SSE → 3DNow → MMX → generic and caches it;
  4. prints `"<module> using <GetName()> for SIMD processing"`;
  5. sets FTZ/DAZ from `CPUID_FTZ`/`CPUID_DAZ`.
* Call sites: `framework/Common.cpp:2430` (`idSIMD::InitProcessor("doom", com_forceGenericSIMD…)`,
  cvar declared at `Common.cpp:70`) and `game/Game_local.cpp:270` in DLL builds.

There are three ways to force generic; **only the third works on x64**, because the other two still
require the asm TUs to compile:

| Approach | Works on x64? | Notes |
|---|---|---|
| `+set com_forceGenericSIMD 1` | runtime yes, build no | already exists; keeps asm TUs compiled |
| `Sys_GetCPUId()` returns `CPUID_GENERIC` | runtime yes, build no | same problem |
| **Compile-time**: exclude the 5 `Simd_*.cpp`, guard dispatch | **yes** | wrap lines 76–90 in `#if defined(_M_IX86) || defined(__i386__) || defined(__ppc__) … #else processor = generic; #endif` |

Probe: with that guard, `Simd.cpp` + `Simd_Generic.cpp` compile clean and `idSIMD_Generic` is the only
implementation in the binary. `com_forceGenericSIMD 1` remains useful as a belt-and-braces runtime
switch but is no longer required.

---

## 5. Pointer-width audit (LLP64)

### 5.1 Measured patterns

| Pattern | Hits | Files | Comment |
|---|---:|---:|---|
| `reinterpret_cast<int>` | 10 | 2 | all in `game/gamesys/Class.h:65-69` + `d3xp` copy |
| `address_t` | 29 | 7 | `typedef unsigned long address_t` (`sys/sys_public.h:234`) is **32-bit on Win64** |
| `sizeof(void*)` | 14 | 4 | `Script_Program.cpp:36-45` (×5), `TypeInfo.cpp:578,1139` — self-consistent, no change |
| `unsigned long` | 320 | 54 | **not** a width change on Win64 (`long` stays 32); only a problem where it holds a pointer |
| `DWORD_PTR` / `INT_PTR` / `intptr_t` / `ULONG_PTR` | **0** | 0 | the tree has never been 64-bit-aware |
| `_alloca16` macro uses | **236** | 49 | macro itself casts through `(int)` (`sys_public.h:52`) |
| pointer→int warnings (`C4311`/`C4302`) | 227 exe + 191 idlib | — | 254 of the exe ones are the `_alloca16` macro instantiated from `Matrix.h:2385` |
| int→pointer warnings (`C4312`) | 562 exe + 322 idlib | — | mostly `_alloca16`; plus the `CVarSystem.h:292` sentinel |
| `size_t`→`int` (`C4267`) | 1057 exe + 336 idlib | — | 896 from `idlib/Str.h` (string lengths < 2 GB, benign) |

### 5.2 Real truncations, ranked

| # | Site | What breaks | Fix |
|---|---|---|---|
| 1 | `sys/sys_public.h:52` `_alloca16(x)` = `((void *)((((int)_alloca((x)+15)) + 15) & ~15))` | **every** stack allocation in math/render/game is truncated and mis-aligned → instant crash | `((void *)((((uintptr_t)_alloca((x)+15)) + 15) & ~(uintptr_t)15))` |
| 2 | `idlib/Heap.cpp:336,340` (`Allocate16`) | stores the malloc pointer in 4 bytes (`*((int*)(alignedPtr-4)) = (int)ptr`) → `Free16` frees garbage. The custom heap is the default (`USE_LIBC_MALLOC 0`, `Heap.cpp:32`) | store `void*` at `alignedPtr - sizeof(void*)`, size `bytes+16+sizeof(void*)`; or build with `USE_LIBC_MALLOC=1` |
| 3 | `idlib/Heap.cpp:492` | page data pointer aligned via `(int)` | `uintptr_t` |
| 4 | `idlib/math/Matrix.cpp:2939`, `idlib/math/Vector.cpp:387` | static temp buffer alignment truncates `idMatX::temp`/`idVecX::temp` | `uintptr_t` |
| 5 | `game/gamesys/Class.h:57-69` (`idEventArg`) | `int value` stores pointers for VECTOR/STRING/ENTITY/TRACE; consumed as pointers in `Event.cpp:283-312` → truncated entity/string/trace on every event | `intptr_t value` + `reinterpret_cast<intptr_t>`; cast back to `int` for INTEGER/FLOAT in `Event.cpp:283,353`; do the same in `d3xp` |
| 6 | `sys/win32/win_main.cpp:658-690` + `sys/sys_public.h:329-331` + `sys/sys_local.h` | `Sys_DLL_Load` returns `int`; `HMODULE` is 8 bytes → `GetProcAddress` on a truncated handle | `intptr_t`/`void*` in the `Sys_*` API and in `idCommonLocal::gameDLL` (`Common.cpp:196`) |
| 7 | `sys/win32/win_main.cpp:100` + `sys/sys_public.h:490-493` | `xthreadInfo::threadHandle` is `int`; `HANDLE` is 8 bytes; `SetThreadPriority((HANDLE)threadHandle)` fails | `intptr_t threadHandle`; fix the three `(HANDLE)` casts |
| 8 | `sys/win32/win_main.cpp:482` | `return (long) st.st_mtime;` truncates 64-bit `time_t`; `ID_TIME_T` is 64-bit on x64 → decl reload / timestamp compare wrong | `return (ID_TIME_T) st.st_mtime;` |
| 9 | `sys/win32/win_shared.cpp:363,478` + `win_shared` module list (`int address`) | symbol module lookup by truncated base address | widen `address` to `uintptr_t`/`address_t` (and `address_t` → `uintptr_t`, see #10); debug-only |
| 10 | `sys/sys_public.h:234` `typedef unsigned long address_t` | cannot hold a pointer on Win64; used by the call-stack API | `typedef uintptr_t address_t` (needed together with #9/#11) |
| 11 | `sys/win32/win_shared.cpp:648-718` | x86 `ebp` chain walk and prologue signature (`0x00FFFFFF` mask) | x64 stub that zero-fills; optional `RtlCaptureStackBackTrace` |
| 12 | `framework/CVarSystem.h:292` | `staticVars != (idCVar *)0xFFFFFFFF` — sentinel becomes `0x00000000FFFFFFFF` | benign today; use `(idCVar *)~(uintptr_t)0` when touching the file |
| 13 | `game/script/Script_Program.cpp:36-45`, `TypeInfo.cpp:578,1139` | `sizeof(void*)` becomes 8 | **no change**: offsets/sizes are computed with the same value on both sides |
| 14 | `ui/Window.cpp:1721-1749,3779-3807`, `ui/SimpleWindow.cpp:261-285` | `(int)&((idWindow*)0)->member` offset tables | benign (small offsets), but the casts are why the warning count is high; leave or switch to `offsetof` |

**No pointer hashing / vtable serialization problem exists.** `idHashTable`/`idHashIndex` key on
`int`/`idStr`; `HashFileName` folds into `FILE_HASH_SIZE` (`FileSystem.cpp:...HashFileName`), so `long`
being 32-bit is fine. `SaveGame.cpp:44-46` writes an **object index**, not a pointer. The game ABI is a
raw-pointer struct (`gameImport_t`) but it never leaves the process, so one compiler/target is enough.

---

## 6. Dependencies

### 6.1 Verified against the installed SDK (x64)

| Requirement | Present? | Evidence | Action |
|---|---|---|---|
| `dsound.h` | yes (`um`) | compiled `t_sdk.c` | keep |
| `dinput.h` | yes (`um`) | compiled | keep |
| `mmreg.h` | yes (`shared`) | compiled | keep |
| `ks.h`, `ksmedia.h` (win_snd.cpp) | yes (`shared`) | compiled | keep |
| `dsound.lib`, `dinput8.lib`, `dxguid.lib` (x64) | yes | `Lib\10.0.26100.0\um\x64` | keep |
| `glu32, opengl32, winmm, wsock32, ws2_32, dbghelp, iphlpapi, odbc32, odbccp32` (x64) | yes | dir listing | keep |
| `DxErr.h` / `DxErr.lib` | **no** | `C1083` | remove `#include <DxErr.h>` (`win_snd.cpp:32`) and the 9 `DXTRACE_ERR(...)` uses; replace with `hr` or a local log macro |
| `eax.h` | exists (`sys/win32/eax.h`) but **never included** by any TU | `#include … eax` = 0 hits | `eaxguid.lib` is unnecessary; remove from `AdditionalDependencies` |
| `openal32.lib` | 32-bit only, and **never linked** — OpenAL is `LoadLibrary(s_libOpenAL)` + `GetProcAddress` (`win_snd.cpp:512-535`, `openal/idal.cpp`) | runtime loader | delete `openal\lib` from `AdditionalLibraryDirectories`; nothing to rebuild |
| libcurl | vendored `neo/curl` 7.11.1, linked via `curllib.vcxproj` | project ref in `doomdll.vcxproj` | **disable**: `ID_ENABLE_CURL=0` and drop the `curllib` project reference (removes ~1 MB of 2004-era code and its x64 risk) |
| IJG JPEG 6b | vendored `renderer/jpeg-6/**`, sources listed in `doomdll.vcxproj` | compiled in | nothing to install |
| Ogg Vorbis | vendored `sound/OggVorbis/**`, sources listed | compiled in | one asm guard (§3.1) |
| zlib | embedded in `framework/Unzip.cpp` | — | nothing |
| MFC (`nafxcw.lib`, `afxwin.h`) | **not installed on this host** | `Test-Path` false | see §8 |

Notes:

* `_DoomDLL.props:16-17` still points `AdditionalLibraryDirectories` at
  `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x86`, `openal\lib` and
  `sys\win32\dongle` (the last does not exist). All three must go.
* `_DoomDLL.props:16` also lists `DxErr.lib`, `eaxguid.lib`, `odbccp32.lib`. Only `DxErr` is a
  blocker; `odbccp32` exists in the SDK.
* `_DoomDLL.props:9` sets `sound/vorbis/include`; that directory does not exist. It is harmless
  because `snd_decoder.cpp:33` includes `"OggVorbis/vorbis/codec.h"` relative to `sound/`.

### 6.2 How to disable each optional dependency

| Feature | Switch | Where |
|---|---|---|
| curl / HTTP download | `ID_ENABLE_CURL=0` | `framework/BuildDefines.h:71`, used at `framework/FileSystem.cpp:47,3650` |
| OpenAL | already off by default (`s_useOpenAL "0"`, `snd_system.cpp:75`); `ID_OPENAL=0` compiles the DirectSound-only path | `framework/BuildDefines.h:120-126` |
| EAX | no code path includes the EAX headers; nothing to disable | — |
| Tools/editors | new `ID_NO_TOOLS` (see §8) | `framework/BuildDefines.h:95-97` |
| TypeInfo generation | don't define `ID_DEBUG_MEMORY`, drop the PreBuildEvent | `game/gamesys/TypeInfo.cpp:40-44` |
| D3XP game module | don't build `game-d3xp.vcxproj` | — |

---

## 7. Project structure and the game module

### 7.1 What the existing projects do

`neo/doom.sln` has 7 configurations, **all `|Win32`**, `ToolsVersion 4.0`, and **no
`<PlatformToolset>`** (defaults to v100). `OutDir` = `..\build\$(PlatformName)\$(Configuration)\`.

| Project | Output | Key defines |
|---|---|---|
| `doomdll.vcxproj` | `DOOM3.exe` | `__DOOM__;__DOOM_DLL__` + `WIN32;_WINDOWS;_USE_32BIT_TIME_T` |
| `game.vcxproj` | **`gamex86.dll`** (hard-coded in `_Game.props:15`) | `__DOOM__;GAME_DLL`, module def `game/game.def` |
| `game-d3xp.vcxproj` | also `gamex86.dll` | `+ _D3XP;CTF` |
| `idlib.vcxproj` | `idlib.lib` | `__IDLIB__` |
| `typeinfo.vcxproj` | `TypeInfo.exe` | `ID_ENABLE_CURL=0;ID_TYPEINFO;__DOOM_DLL__` |
| `curllib.vcxproj` | `curllib.lib` | — |
| `MayaImport.vcxproj` | Maya 5.0 exporter | needs Autodesk Maya SDK — do not build |

`doomdll.vcxproj` also compiles the **entire MFC tool suite** (Radiant, MaterialEditor, GUIEd,
AF/PDA/Sound/Script/Decl editors, script debugger, `tools/comafx`, PropTree) and references
`curllib.vcxproj`. That is why MFC is linked into the client exe.

### 7.2 How `gamex86.dll` is loaded

`framework/Common.cpp:2629-2690` (inside `#ifdef __DOOM_DLL__`):

```
fileSystem->FindDLL( "game", dllPath, true );
gameDLL = sys->DLL_Load( dllPath );
GetGameAPI = (GetGameAPI_t) Sys_DLL_GetProcAddress( gameDLL, "GetGameAPI" );
gameImport.version = GAME_API_VERSION; … fill 13 service pointers …
gameExport = *GetGameAPI( &gameImport );
game = gameExport.game; gameEdit = gameExport.gameEdit;
```

The name comes from `idSysLocal::DLL_GetFileName` (`sys/sys_local.cpp:123-133`):
`"%s" CPUSTRING ".dll"` with `CPUSTRING "x86"` (`sys/sys_public.h:46`). `FindDLL`
  (`FileSystem.cpp:3864-3960`) looks next to the exe first, then inside paks under the same name;
`SetPureServerChecksums` hashes that name too.

Consequences for x64:

* Keeping `"x86"` in the name works but collides with a retail/32-bit `gamex86.dll`; rename
  `CPUSTRING` to `"x64"` **and** change `_Game.props` `OutputFile` to `gamex64.dll`.
* `Sys_DLL_Load` returns `int` → HMODULE truncation (see §5.2 #6). Fix required before DLL mode works.
* `GAME_API_VERSION` stays 8; the struct is raw pointers, fine within one compiler/target.

### 7.3 Recommendation: static-link the game into the exe

`__DOOM_DLL__` is already the only thing that guards `LoadGameDLL`; with it undefined the whole body
compiles away and the engine expects `game`/`gameEdit` to exist elsewhere
(`framework/Common.cpp:106-109` guards the NULL definitions with `#ifdef __DOOM_DLL__`;
`game/Game_local.cpp:65` defines `idGame *game = &gameLocal;`, `game/GameEdit.cpp:668` defines
`gameEdit`).

Build shape:

* `DOOM3.exe` = all exe TUs **plus** all `game/*.cpp`, compiled with **neither `__DOOM_DLL__` nor
  `GAME_DLL`**.
* With `GAME_DLL` undefined, `game/Game_local.cpp:34-50` does not re-define `sys`, `common`,
  `renderSystem`, … so the engine's definitions are used (they are declared `extern` in the normal
  headers: `sys/sys_public.h:573`, `framework/Common.h:213`, `renderer/RenderSystem.h:261`, …).
* `idCVar::staticVars` is then owned by the engine (not the game TU) — consistent.
* Side effect to delete: `Game_local.cpp:257` calls `TestGameAPI()` when `GAME_DLL` is undefined.
  It fills a fake `gameImport_t` from the real globals and re-checks the version; harmless, but
  removing the call keeps init deterministic.

Probe: `game/gamesys/Class.cpp`, `game/Game_local.cpp`, `game/gamesys/Event.cpp` compile clean both
with and without `GAME_DLL` (0 errors). Static link also removes the name/arch problem, the
HMODULE-handle problem, and the separate-ABI risk in one step. Keep the DLL shape as an optional
follow-up if binary mods matter.

---

## 8. MFC tools exclusion

The tools are not optional in the current exe project: `doomdll.vcxproj` lists every editor, and
`idlib/precompiled.h:61` includes `tools/comafx/StdAfx.h` → `<afxwin.h>` for **all** TUs.

Facts:

* MFC type usage outside `tools/`: **0** (`CString`/`CDialog`/`CWnd`/`CRect`/… only in `sys/osx/*.mm`).
  So the engine does not need MFC, only the include chain does.
* `tools/comafx/StdAfx.h:38` is the `#include <afxwin.h>` that fails; `win_taskkeyhook.cpp:37`
  includes `<afxwin.h>` directly.
* `framework/BuildDefines.h:95-97` auto-defines `ID_ALLOW_TOOLS` on `_WIN32` unless `ID_DEDICATED`
  or `ID_DEMO_BUILD`. It gates `Common.cpp` (777, 987, 1139, 1226, 2348), `Console.cpp:854`,
  `win_main.cpp:778,1455`, `ui/Window.cpp` (10 blocks).

Minimal change set:

1. `idlib/precompiled.h`: replace the active `#include "../tools/comafx/StdAfx.h"` (line 61) with
   nothing (keep `#define WINVER 0x501`). *This single edit removed MFC from 44 idlib + 197 exe TUs
   in the probe.*
2. `framework/BuildDefines.h:95`: add `&& !defined( ID_NO_TOOLS )`; build with `/DID_NO_TOOLS`.
3. `sys/win32/win_taskkeyhook.cpp:37`: `<afxwin.h>` → `<windows.h>` (it only uses `HHOOK`, `BOOL`,
   `LPCTSTR`, `HINSTANCE`).
4. Remove every `tools\*` `<ClCompile>` from the exe **except**
   `tools/compilers/aas/AASFile.cpp` and `tools/compilers/aas/AASFileManager.cpp` — the latter defines
   the `AASFileManager` service that `Common.cpp:2670` and the game read (`.aas` loading).
   `dmap`, `aas` builders, `roqvq`, `renderbump` are not referenced by the client at runtime.
5. Drop `MayaImport` from the build (needs the Maya 5.0 SDK).
6. Do **not** use `ID_DEDICATED` to disable tools: it changes networking/dedicated behavior as well.

Alternative: install “C++ MFC for latest v143 build tools” in the VS installer and keep the includes.
That makes the tools compile, but they still cost build time and are not needed to run the game.

---

## 9. TypeInfo / `GameTypeInfo.h` prebuild

* `_Game.props` PreBuildEvent: `..\build\Win32\"$(Configuration)"\TypeInfo.exe` — no arguments, and
  the path is Win32-specific (breaks the moment `$(PlatformName)` becomes `x64`).
* `TypeInfo/main.cpp:272-281` writes `../neo/game/gamesys/GameTypeInfo.h` by default — i.e. **into the
  source tree**.
* `game/gamesys/TypeInfo.cpp:40-44` includes `GameTypeInfo.h` **only** under `ID_DEBUG_MEMORY`;
  otherwise it includes `NoGameTypeInfo.h`, which supplies empty tables (83 lines, all sentinels).
* `GameTypeInfo.h` is **not committed** (`game/gamesys/` contains `NoGameTypeInfo.h` only).

Therefore:

* For a Release/Debug x64 build: **delete the PreBuildEvent** and do not build `typeinfo.vcxproj`.
  Nothing is missing.
* If `ID_DEBUG_MEMORY` is ever wanted: build `TypeInfo.exe` for x64, run it with an explicit output
  argument pointing outside the source tree, and add that directory to the include path.
* `TypeInfo` itself is x64-clean (no asm; it links `idlib` + `FileSystem` + `Unzip`), but it pulls in
  `FileSystem.cpp`, which needs the same `_file` fix (§10).

---

## 10. Empirical compile probe (the proof)

A temp copy of `neo/` was patched with only these edits, then compiled for x64 with MSVC 14.44:

| # | Edit | Files |
|---|---|---|
| 1 | Remove the MFC include from the PCH chain | `idlib/precompiled.h:61` |
| 2 | Guard `__asm`/`_asm` with `_M_IX86` | `idlib/math/Math.h` (390, 443, 802, 835), `idlib/Lib.cpp:580` |
| 3 | Guard SIMD test harness + dispatch; exclude 5 SIMD TUs | `idlib/math/Simd.cpp` (144, 76-90) |
| 4 | Guard vendored vorbis asm | `sound/OggVorbis/vorbissrc/os.h:117` |
| 5 | `fp->_file` → `_fileno(fp)` | `framework/FileSystem.cpp:3598,3644` |
| 6 | UDL: `"..."S_COLOR_RED` → `"..." S_COLOR_RED "..."` | `idlib/math/Simd.cpp:1501,1527` |
| 7 | `/source-charset:.1252` for one legacy-encoded file | `sys/win32/win_input.cpp` |

Results:

| Target set | TUs | Errors | Notes |
|---|---:|---:|---|
| `idlib` (all project TUs except the 5 SIMD asm files + `precompiled.cpp`) | 44 | **0** | 1,040 warnings (C4311/C4302 191, C4312 322, C4267 336) |
| exe TUs (all `doomdll.vcxproj` TUs minus `tools\*`, minus `win_cpu/win_main/win_shared/win_snd`, plus the 2 AAS TUs) | 197 | **0** | 60 errors in the batch were all `win_input.cpp`; it compiles clean with edit #7 |

Errors that appeared and are now understood (all reproducible):

| Code | Message | Site |
|---|---|---|
| C1189 | `You cannot use 32-bit time_t (_USE_32BIT_TIME_T) with _WIN64` | props / `corecrt.h` |
| C1083 | `afxwin.h` not found | `precompiled.h:61`, `win_taskkeyhook.cpp:37` |
| C4235 + cascade | `__asm` unsupported | Math.h, Lib.cpp, Simd*, vorbis `os.h` |
| C3688 | invalid literal suffix `S_COLOR_RED` | `Simd.cpp:1501,1527` (C++11 UDL) |
| C2039 | `_file` is not a member of `_iobuf` | `FileSystem.cpp:3598,3644` |
| C2015/C2017/C2001/C2059 | char-constant errors | `win_input.cpp:158+` (CP1252 source under a CJK ANSI code page) |
| C2065 | `GWL_WNDPROC` undeclared | `win_syscon.cpp:396` (`winuser.h` `#undef`s it under `_WIN64`) |
| C2485 | `__declspec(naked)` | `win_main.cpp:1516` |

The only files **not** covered by the probe are the four that need real x64 code:
`sys/win32/win_cpu.cpp`, `sys/win32/win_main.cpp`, `sys/win32/win_shared.cpp`,
`sys/win32/win_snd.cpp` (DxErr), plus `win_syscon.cpp` (one-line GWLP fix) and `win_input.cpp`
(one charset flag).

---

## 11. Prioritized task list

Each task is self-contained and verifiable. “Build” means the x64 configuration from §12.
Changes belong in a branch/target copy, never in the pristine `doom3_orig/doom3`.

### T1 — P0 — Build harness (x64, v143, SDK 26100)
* **Files:** new `CMakeLists.txt` (+ `cmake/` presets) at the repo or target root; no engine edits.
* **Change:** three targets mirroring the project: `idlib` (static), `doom3` (exe), and optionally
  `game` (shared). `set(CMAKE_SYSTEM_VERSION 10.0.26100.0)`, `-A x64`, toolset `v143`, C++14,
  `/MT`, `/EHsc`, `/D WIN32 /D _WINDOWS /D _CRT_SECURE_NO_DEPRECATE /D _CRT_NONSTDC_NO_DEPRECATE
  /D NDEBUG /D __DOOM__ /D __DOOM_DLL__ /D ID_ENABLE_CURL=0 /D ID_NO_TOOLS`.
  **Do not** define `_USE_32BIT_TIME_T`. Source lists are the `<ClCompile>` lists already in
  `idlib.vcxproj`/`doomdll.vcxproj` (they are the authoritative “what builds today”).
* **Verify:** `cmake --build out --config Release` fails only in the files named in T3/T4/T5 —
  i.e. no configure errors and no missing-source errors.

### T2 — P0 — Remove MFC from the core; exclude the tools
* **Files:** `idlib/precompiled.h` (line 61), `framework/BuildDefines.h` (95), `sys/win32/win_taskkeyhook.cpp`
  (37), exe source list (all `tools\*` except `tools/compilers/aas/AASFile.cpp` and `AASFileManager.cpp`),
  `doomdll.vcxproj`/CMake project references (`curllib`, `MayaImport`).
* **Change:** delete the `StdAfx.h` include; add `ID_NO_TOOLS` to the `ID_ALLOW_TOOLS` condition;
  `<afxwin.h>` → `<windows.h>`; remove the tool TUs and the `curllib`/`MayaImport` deps.
* **Verify:** `cl /c idlib\Str.cpp` succeeds with no `afxwin.h` in the include trace
  (`/showIncludes`); grep the build log for `afx` = 0 hits; `dumpbin /dependents DOOM3.exe` has no
  `mfc*.dll`.

### T3 — P0 — SIMD: generic only on x64
* **Files:** `idlib/math/Simd.cpp`, `idlib/math/Simd_SSE.cpp`, `Simd_SSE2.cpp`, `Simd_SSE3.cpp`,
  `Simd_MMX.cpp`, `Simd_3DNow.cpp`, project source list.
* **Change:** (a) drop the five asm TUs, or change their `#elif defined(_WIN32)` to
  `#elif defined(_WIN32) && defined(_M_IX86)`; (b) wrap the selection block in `Simd.cpp:76-90` with
  `#if defined(_M_IX86) || defined(__i386__) || defined(__ppc__) … #else processor = generic; #endif`;
  (c) guard the RDTSC test macros at `Simd.cpp:144`.
* **Verify:** link a test that calls `idSIMD::Init(); idSIMD::InitProcessor("t", false);` and assert
  `SIMDProcessor->GetName()` prints the generic name; `dumpbin /symbols idlib.lib | findstr idSIMD_SSE`
  is empty; `DOOM3.exe +set com_forceGenericSIMD 1` never prints “using idSIMD_SSE3”.

### T4 — P0 — `_alloca16` and the allocator
* **Files:** `sys/sys_public.h:52` (and the mac/linux copies at 87/118 for consistency),
  `idlib/Heap.cpp` (32, 336-340, 492, 1119, 1140), `idlib/math/Matrix.cpp:2939`,
  `idlib/math/Vector.cpp:387`.
* **Change:** `_alloca16` via `uintptr_t`; `Allocate16` stores/loads `void*` with
  `sizeof(void*)` trailer; `page_s` alignment and the `assert(((int)ptr)&15)` via `uintptr_t`;
  static `temp` alignment via `uintptr_t`. *(Fastest unblock: also `/D USE_LIBC_MALLOC=1` for the
  first runnable build; still fix the macro.)*
* **Verify:** add a startup assert `(uintptr_t)Mem_Alloc16(64) % 16 == 0` and run a map; also
  `grep -n "_alloca16" ` and confirm no `(int)` remains in the macro. Watch for `C4311/C4312` at
  `Matrix.h:2385` dropping to 0 in the build log.

### T5 — P0 — `sys/win32` x64 platform layer
* **Files:** `sys/win32/win_cpu.cpp`, `win_main.cpp`, `win_shared.cpp`, `win_syscon.cpp`,
  `sys/sys_public.h` (`address_t`, `xthreadInfo`, `Sys_DLL_*`), `sys/sys_local.{h,cpp}`,
  `framework/Common.cpp:196` (`gameDLL`).
* **Change:**
  * `win_cpu.cpp`: `#if defined(_M_X64)` branch with `__rdtsc()` for `Sys_GetClockTicks`,
    `__cpuidex` for `CPUID`, `HasCPUID→true`, `Sys_GetCPUId→CPUID_GENERIC` (minimal) or full flags
    with `_mm_getcsr/_mm_setcsr` for DAZ/FTZ and FPU no-ops/`_controlfp_s`.
  * `win_main.cpp`: delete `clrstk`/`HackChkStk`/`TestChkStk` under `#if _M_IX86`; `Sys_DLL_Load`
    returns `intptr_t`; `threadHandle` → `intptr_t`; `(ID_TIME_T)st.st_mtime`.
  * `win_shared.cpp`: `Sys_GetCallStack` x64 stub zero-fills; widen `module->address`; drop the
    `(DWORD)mbi.AllocationBase` casts (debug only).
  * `win_syscon.cpp:396`: `SetWindowLongPtr(…, GWLP_WNDPROC, (LONG_PTR)…)`.
* **Verify:** `cl /c sys\win32\win_cpu.cpp sys\win32\win_shared.cpp sys\win32\win_main.cpp
  sys\win32\win_syscon.cpp` → 0 errors; run `+set com_developer 1 +error` and confirm the log shows a
  clean “ASSERTION/ERROR” path and no crash in `Sys_GetCallStack`.

### T6 — P0 — Dependencies (DxErr, libs, curl)
* **Files:** `sys/win32/win_snd.cpp` (32, 9 `DXTRACE_ERR`), `_DoomDLL.props` (16-17), project refs.
* **Change:** remove `<DxErr.h>`; replace `DXTRACE_ERR(TEXT("…"), hr)` with `hr` (or
  `common->DPrintf`); delete `DxErr.lib`, `eaxguid.lib`, `odbccp32.lib` if unused, the DirectX
  `Lib\x86` path, `openal\lib`, `sys\win32\dongle`; `ID_ENABLE_CURL=0` + drop `curllib`.
* **Verify:** link succeeds with no `LNK1104`/`LNK2019`; `dumpbin /dependents` shows only SDK DLLs
  plus `OpenAL32.dll` if `s_useOpenAL 1`.

### T7 — P1 — `idEventArg` pointer storage
* **Files:** `game/gamesys/Class.h:57-69`, `game/gamesys/Event.cpp:283,353`, `d3xp` copies.
* **Change:** `intptr_t value`; `reinterpret_cast<intptr_t>` in the 5 pointer constructors; cast back
  to `int` for `D_EVENT_INTEGER`/`FLOAT`; leave `type` as `int`.
* **Verify:** unit-ish check in game: fire an event with a string and an entity argument on a map and
  assert the received pointer equals the sent pointer (e.g. `EV_Touch` / `EV_Activate`); no `C4312`
  in `Class.h`/`Event.cpp`; `sizeof(idEventArg)` == 16 on x64.

### T8 — P1 — Format-string / `size_t` sweep
* **Files:** the 37 `%d`+`sizeof` sites (top: `game/script/Script_Program.cpp:1323,1762,1764`,
  `game/Game_network.cpp:711`, `ui/BindWindow.cpp:73`, `renderer/RenderSystem_init.cpp:770`),
  `sys/win32/win_main.cpp:482`.
* **Change:** `%d`→`%zu` for `size_t`, `%d`→`%lld`/`(long long)` for `ID_TIME_T`; prefer
  `idStr::snPrintf` with explicit casts.
* **Verify:** build with `/W4` and count `C4477` (format) warnings before/after; run `where`, `dir`,
  `listDecls` and check numbers are not truncated.

### T9 — P1 — Game module linkage decision
* **Files:** exe source list, `framework/Common.cpp` (no edit needed), `game/Game_local.cpp:257`.
* **Change:** static link (compile `game/*.cpp` with neither `__DOOM_DLL__` nor `GAME_DLL`) **or**
  keep the DLL with `CPUSTRING "x64"` + `_Game.props` `gamex64.dll` + the T5 handle fix.
* **Verify (static):** `dumpbin /dependents DOOM3.exe` has no `game*.dll`; `LoadGameDLL` is absent
  from the map; the game boots and `game->Init()` runs (log shows “--------- Initializing Game
  ----------”). **Verify (DLL):** `gamex64.dll` is produced and `+set fs_basepath …` loads it; log
  shows `Loading game DLL: '…gamex64.dll'`.

### T10 — P1 — Drop the TypeInfo prebuild
* **Files:** `_Game.props` PreBuildEvent, `typeinfo.vcxproj` (optional), `game/gamesys/TypeInfo.cpp`
  (no edit).
* **Change:** remove the PreBuildEvent; do not build/run TypeInfo for Release; if `ID_DEBUG_MEMORY`
  is enabled later, run it with an out-of-tree output path.
* **Verify:** a clean build leaves `game/gamesys/GameTypeInfo.h` absent (`git status` clean in the
  source tree) and the exe still builds; `TypeInfo.cpp` still includes `NoGameTypeInfo.h`.

### T11 — P1 — Text encoding for the legacy source
* **Files:** `sys/win32/win_input.cpp` only.
* **Change:** set `/source-charset:.1252` on that TU (CMake: `set_source_files_properties(...)`) or
  convert the 6 non-ASCII char literals (lines 158–160) to `'\xE4'`-style escapes.
* **Verify:** `cl /c /source-charset:.1252 sys\win32\win_input.cpp` → 0 errors (verified); in-game
  keyboard layout for German/French keys still maps correctly.

### T12 — P1 — MFC-free build config in the repo
* **Files:** new `_x64.props` (if staying on MSBuild) or the CMake toolchain file.
* **Change:** `<PlatformToolset>v143</PlatformToolset>`, `<WindowsTargetPlatformVersion>10.0.26100.0`,
  x64 platform, `/MT`, no `_USE_32BIT_TIME_T`, no MFC libs, no DirectX include/lib path, no
  `LargeAddressAware` (ignored on x64), keep 16 MB stack.
* **Verify:** `msbuild doom.sln /p:Configuration=Release /p:Platform=x64` (or the CMake build)
  produces `DOOM3.exe` with `dumpbin /headers` showing `machine (x64)`.

### T13 — P2 — Runtime smoke + regression evidence
* **Files:** none (CI script).
* **Change:** boot test with the user-supplied `base/` pk4s:
  `DOOM3.exe +set com_skipIntro 1 +set com_skipRenderer 1 +set fs_basepath <path> +quit` for CI, and a
  windowed run + `+map` for manual verification.
* **Verify:** exit code 0 and the log contains `--------- Initializing Game ----------` and
  `using idSIMD_Generic` (or no SIMD line if the build has only generic); screenshot of the menu;
  `+map testmaps/test_box` (or any retail map) reaches “SERVER STARTED”.

### T14 — P2 (optional) — Real x64 CPUID/FPU and stack traces
* **Files:** `win_cpu.cpp`, `win_shared.cpp`.
* **Change:** full `__cpuidex` feature flags, `_mm_setcsr` FTZ/DAZ, `RtlCaptureStackBackTrace` +
  `SymFromAddr` for release symbols.
* **Verify:** log prints the real CPU string/flags; `Sys_GetCallStackCurStr(4)` in a forced
  `common->Error` shows frames (debug build).

---

## 12. Build configuration reference

```
Platform        x64
Toolset         v143 (MSVC 14.44)
Windows SDK     10.0.26100.0   (10.0.20348.0 also verified)
Language        C++14 (/std:c++14); C files as C
Runtime         /MT (Release), /MTd (Debug)
Defines         WIN32 _WINDOWS _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE NDEBUG
                __DOOM__ __DOOM_DLL__          (exe + separate game DLL)
                ID_ENABLE_CURL=0
                ID_NO_TOOLS                    (new, see T2)
                # NOT _USE_32BIT_TIME_T
                # NOT GAME_DLL in the exe when static-linking
Includes        <neo>  (the PCH include of tools/comafx is removed)
Libs (x64)      dbghelp dinput8 dsound dxguid glu32 iphlpapi odbc32 opengl32 winmm wsock32
                # no DxErr.lib, no eaxguid.lib, no openal32.lib, no nafxcw.lib
Link            /SUBSYSTEM:WINDOWS /STACK:16777216
                (drop /LARGEADDRESSAWARE — meaningless on x64)
Files excluded  tools/** except compilers/aas/{AASFile,AASFileManager}.cpp
                idlib/math/Simd_{3DNow,MMX,SSE,SSE2,SSE3,AltiVec}.cpp
                MayaImport/**
                curllib/**
                typeinfo (Release; optional Debug-memory only)
```

Per-file exception: `sys/win32/win_input.cpp` → `/source-charset:.1252`.

---

## 13. Evidence / reproduction commands

All counts in this document come from these (PowerShell 5.1, repo root `doom3_orig/doom3/neo`):

```powershell
# inline asm, counting _asm and __asm but not __asm__ (GCC) or asm in comments-only patch files
$root='...\neo'
$files = Get-ChildItem $root -Recurse -File -Include *.c,*.cpp,*.h,*.hpp,*.inl |
         Where-Object { $_.FullName -notmatch '\\curl\\|\\jpeg-6\\|\\OggVorbis\\|\\openal\\' }
$files | ForEach-Object {
  $m = Select-String -LiteralPath $_.FullName -Pattern '(?<![A-Za-z0-9])_+asm\b' -AllMatches
  if ($m) { [pscustomobject]@{ File=$_.FullName.Substring($root.Length+1);
             Count=($m|ForEach-Object{$_.Matches.Count}|Measure-Object -Sum).Sum } }
} | Sort-Object Count -Descending

# pointer-width patterns
foreach ($p in 'reinterpret_cast\s*<\s*int\s*>','\baddress_t\b','sizeof\s*\(\s*void\s*\*\s*\)',
                '\bunsigned\s+long\b','\bDWORD_PTR\b|INT_PTR|intptr_t|ULONG_PTR','_alloca16\s*\(') { ... }

# real compiler probes (temp copy, not the repo)
cl /nologo /c /EHsc /MT /DWIN32 /D_WINDOWS /D__DOOM__ /D__DOOM_DLL__ /DID_ENABLE_CURL=0 ^
   /I<neo> idlib\math\Simd.cpp
cl /nologo /c /EHsc /MT /DWIN32 /D_WINDOWS /D__DOOM__ /D__DOOM_DLL__ /DID_ENABLE_CURL=0 ^
   /I<neo> sys\win32\win_cpu.cpp        # expected: C4235 cascade until T5
```

Probe result logs (temp, not committed):
`<TEMP>\kilo\d3x64\idlib_build4.log` (44 TUs, 0 errors) and
`doomdll_build2.log` (197 TUs; 60 errors, all `win_input.cpp` before edit #7).

---

## 14. Risks and open items

1. **Runtime layout of `idHeap`** — T4 fixes the 16-byte path; the small/medium allocators use
   `byte` headers and are width-independent, but `idDynamicBlock` arithmetic should be re-read after
   the pointer fixes.
2. **`idStr`/`idList` remain `int`-indexed.** Fine below 2 GB per string/list; a large mod index is
   out of scope.
3. **`GAME_API_VERSION` and raw-pointer ABI** — safe only when engine and game are built together
   (always true in static-link; also true for a same-compiler DLL).
4. **`ID_ALLOW_TOOLS` is not the only editor coupling**: `ui/Window.cpp` has 10
   `#ifdef ID_ALLOW_TOOLS` blocks tied to `idUserInterfaceLocal` editor state; verify the UI still
   loads `.gui` files after T2 (the blocks are all editor hooks, but confirm with a menu screenshot).
5. **`win_input.cpp` encoding** — `/source-charset:.1252` compiles it, but the resulting character
   literals are Latin-1; if a non-Western keyboard layout is required, convert to `K_*`/Unicode
   mapping instead (separate task).
6. **OpenAL** stays off by default; if enabled, ship OpenAL Soft's **x64** `OpenAL32.dll`. The
   in-tree `openal32.lib`/`eaxguid.lib` must not be linked.
7. **`_iobuf::_file`** also exists in `idFile_Permanent`-based curl paths; T6 removes curl, but the
   two `_fileno` edits in T1 are still required for the non-curl `BackgroundDownload` path.
8. **`typeinfo.vcxproj`** links `FileSystem.cpp`, so it needs the same `_file` fix even though it is
   excluded from Release.

---

## 15. Corrections to earlier docs

* `05_build_deps_data_tools.md` §3 counts “1,565 `__asm`”. The full census including `_asm` is
  **1,613 tokens**, and `idlib/math/Math.h` has **two `_asm` blocks (SinCos/SinCos64)** that the
  `__asm` pattern does not see. `05` §3 also implies only separate `Simd_*.cpp` TUs are affected;
  `Math.h`, `Lib.cpp`, `win_cpu.cpp`, `win_main.cpp`, `win_shared.cpp` and vendored libvorbis
  `os.h` are equally affected.
* `05` §3 says “`idlib.vcxproj` also lists `Simd_*` as separate TUs” — correct, but omits that
  `doomdll.vcxproj` compiles the entire MFC tool suite and links `curllib`, which is why MFC appears
  in the client.
* `01_engine_core_platform.md` §5/O2 says `windows.h`/`dinput.h`/`dsound.h` come from the PCH — true,
  and additionally **`tools/comafx/StdAfx.h` (MFC)** is in the same PCH block, which is the actual
  blocker on a machine without MFC.
* `01` §4.1 says the game DLL is loaded via `FindDLL("game")` — correct; add that the name is
  `game` + `CPUSTRING` (currently `"x86"`) and that `Sys_DLL_Load` returns `int` (HMODULE truncation).
* `05` §5 step 2 says tools are excluded “by build defines” — the repo has no such define; the
  `ID_ALLOW_TOOLS` gate must be extended with a new macro (T2), and the tool TUs removed from the
  exe project.
