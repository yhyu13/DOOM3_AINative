# 01 — Engine Core & Platform Layer (DOOM 3 GPL, idTech4)

Read-only analysis of the DOOM 3 GPL source tree. All paths are relative to
`doom3_orig/doom3/neo/` unless stated otherwise; the tree is the 2011-11-22 source release
(`doom3_orig/doom3/COPYING.txt`, GPLv3), no game data.

Purpose of this document: give a migration team and an AI-assisted modding
pipeline a precise map of *what the engine core is*, *where it touches the
operating system*, and *which seams are safe to cut*. Line numbers are evidence
and were taken from this tree; they will drift if the submodule is bumped.

Sibling documents in this directory: `02_renderer.md`,
`04_game_logic_and_ai.md`, `05_build_deps_data_tools.md`. Renderer and game
details are only cross-referenced here, not duplicated.

---

## 1. Layering and boot sequence

The engine is a classic idTech4 stack. The important fact for any port: **the OS
dependency is already concentrated behind a small, explicit seam** (`sys_public.h`
free functions + the `idSys` vtable). The renderer and OpenAL are the two places
where that discipline breaks down.

```
            game module (game/*.cpp, GAME_DLL)          <- GetGameAPI / gameImport_t
                        |
        +---------------+------------------+
        |                                  |
   framework/ (Common, Session,        renderer/ (RenderSystem, tr_*,
   FileSystem, DeclManager,            draw_arb2, Model, Image)
   Cmd/CVar, Console, UsercmdGen)             |
        |                                    |
   idlib/ (Str, Dict, Lexer, Parser,     qgl.h  -> platform GL entry points
   containers, math, geometry)                |
        |                                    |
        +---------------+--------------------+
                        |
                 sys/sys_public.h          <- Sys_* free functions + idSys vtable
                        |
        sys/win32 | sys/linux | sys/posix | sys/osx | (missing: emscripten)
```

### 1.1 Boot and frame loop (verified)

| # | Step | Evidence |
|---|---|---|
| 1 | `WinMain` is the process entry; no portable `main` on Windows | `sys/win32/win_main.cpp:1365` |
| 2 | `Sys_Init()` — OS/CPU probing, FPU setup | `sys/win32/win_main.cpp:964` (called `sys/win32/win_main.cpp:1405-1406`) |
| 3 | `common->Init( 0, NULL, lpCmdLine )` — argc/argv are *not* used on Win32, only the raw command line string | `sys/win32/win_main.cpp:1407` |
| 4 | `idCommonLocal::Init`: inject `idLib` service pointers, `idLib::Init()`, parse command line, `cmdSystem->Init()`, `cvarSystem->Init()`, key input, console, `Sys_Init()`, `Sys_InitNetworking()`, `InitSIMD()`, `InitCommands()`, `InitGame()` | `framework/Common.cpp:2764-2833` |
| 5 | `InitGame()` → `session->Init()` → `LoadGameDLL()` | `framework/Common.cpp:2629-2691` |
| 6 | Frame pump: `common->Frame()` loops forever | `sys/win32/win_main.cpp:1501` |
| 7 | A dedicated async thread drives 60 Hz user commands and sound mixing | `sys/win32/win_main.cpp:923`; `framework/Common.cpp:2581-2622`; `sound/snd_system.cpp:622-626` |

`idCommon::Init`, `Frame`, `Async`, `GUIFrame` are the four engine-tick entry
points (`framework/Common.h:119-138`). `Async` is the one that assumes a real
thread; everything else is main-thread only.

---

## 2. Module map (measured, `*.cpp/*.h/*.c`, `Measure-Object -Line`)

| Area | Files | ≈LOC | Role | Portability |
|---|---:|---:|---|---|
| `framework/` | 50 | 32,700 | Engine core: `Common`, `Session`, `FileSystem`, decls, console, Cmd/CVar, usercmd | B |
| `framework/async/` | 12 | 7,300 | Client/server network state machine, snapshots, msg channel | D |
| `idlib/` (root) | 28 | 15,400 | `idLib`, `idStr`, `idDict`, `Lexer`, `Parser`, `Token`, `CmdArgs`, `BitMsg`, `MapFile`, `Heap`, `Base64` | A |
| `idlib/containers/` | 15 | 3,900 | `idList`, `idHashTable`, `idStrList`, `idStrPool`, `idBTree`, `idHierarchy`, `idStaticList` | A |
| `idlib/geometry/` | 18 | 6,700 | `idBounds`, `idWinding`, `idDrawVert`, `idTraceModel`, `idSurface` | A |
| `idlib/hashing/` | 6 | 600 | MD5, CRC32 | A |
| `idlib/bv/` | 9 | 4,800 | Bounding volumes: sphere/box/frustum/polytope | A |
| `idlib/math/` | 44 | 55,300 | `idVec*`, `idMat*`, `idAngles`, `idQuat`, `idCurve`, `idLcp`, `idODE`, `idSIMD` | A− / C (SIMD) |
| `sys/` (root) | 3 | 665 | `sys_public.h` seam, `idSysLocal` adapter | A |
| `sys/win32/` | 16 | 14,700 | Win32 main, GL context, WGL loader, input, net, sound, console | E |
| `sys/linux/` | 23 | 7,100 | POSIX main, GLX, ALSA, dedicated | E |
| `sys/posix/` | 6 | 2,200 | Shared POSIX threads/net/signal/main | D |
| `sys/osx/` | 13 | 1,800 | Partial legacy Mac port | E |
| `cm/` | 10 | 8,100 | Collision model: load, trace, contacts, rotate, translate | B+ |
| `sound/` | 10 | 7,000 | `idSoundWorld`, `idSoundEmitter`, `idSoundShader`, mixer, wave/ogg | B− |
| `ui/` | 44 | 16,200 | `idUserInterface`, `idWindow`, `idDeviceContext`, `.gui` parser, bundled mini-games | B+ |
| `openal/` | 3 | 310 | Hard-coded AL/ALC function table loader | E |
| `renderer/` (xref) | 83 | 62,100 | GL renderer, 5 vendor back ends, model/image/material loading | D/E |
| `game/` (xref) | 60 + subdirs | ≈114,600 | Game DLL: entities, AI, physics, script VM | A−/B |

Largest single files (useful for effort estimation):
`renderer/…` excluded; `framework/Unzip.cpp` 3,857; `framework/FileSystem.cpp`
3,740; `framework/Session.cpp` 2,839; `framework/Common.cpp` 2,600;
`idlib/math/Simd_SSE.cpp` 16,102; `idlib/math/Matrix.cpp` 6,881;
`ui/Window.cpp` 3,799; `cm/CollisionModel_load.cpp` 3,285;
`sys/win32/win_qgl.cpp` 2,708; `sound/snd_world.cpp` 1,789.

In-scope total is ≈ 320k LOC; the remaining ~200k of the ~520k tree is
`tools/`, `d3xp/`, `MayaImport/`, `curl/`, `TypeInfo/`.

---

## 3. Key interfaces

All engine services are pure-virtual singletons with a global pointer, created as
file-scope objects. That makes them easy to stub, but it also means the ABI is
vtable-order-sensitive (relevant to the game DLL boundary, §4).

### 3.1 `idCommon` — `framework/Common.h:112`

| Group | Methods | Notes |
|---|---|---|
| Life cycle | `Init/Shutdown/Quit/IsInitialized` | `Init` takes `(argc, argv, cmdline)`; Win32 passes only `cmdline` |
| Tick | `Frame`, `GUIFrame`, `Async` | `Async` is called from a background thread at 60 Hz |
| Console | `Printf`, `VPrintf`, `DPrintf`, `Warning`, `DWarning`, `PrintWarnings`, `Error`, `FatalError` | All output funnels through here; good single choke point for a browser console |
| Config | `StartupVariable`, `WriteConfigToFile`, `WriteFlaggedCVarsToFile` | `+set` handling |
| Editors | `InitTool`, `ActivateTool` | `toolFlag_t` (`Common.h:40-54`) |
| Input sampling | `KeysFromBinding`, `BindingFromKey`, `ButtonState`, `KeyState` | Polling API on top of `sysEvent_t` |
| Localisation | `GetLanguageDict` | `idLangDict` (`idlib/LangDict.h`) |

Win32 types leak into this otherwise portable header:
`HWND com_hwndMsg` under `#ifdef _WIN32` (`framework/Common.h:86-91`).

### 3.2 `idSession` — `framework/Session.h:63`

Owns the level life cycle and is the only thing that talks to the game object
between levels. Public methods: `Init`, `Shutdown`, `Stop`, `UpdateScreen`,
`PacifierUpdate`, `Frame`, `ProcessEvent`, `StartMenu`, `SetGUI`,
`GuiFrameEvents`, `MessageBox`, `DownloadProgressBox`, `TimeHitch`, CD-key calls.

Public data (not accessors) — `Session.h:152-160`:
`rw` (`idRenderWorld*`), `sw` (`idSoundWorld*`), `readDemo`/`writeDemo`
(`idDemoFile*`), `renderdemoVersion`. Demos are written by the renderer and sound
system directly, which is one reason the renderer is hard to swap.

Map start is a `sessionCommand` string (`game/Game.h:45-54`, `gameReturn_t`).
`Session.cpp:100/141/177` shows the `maps/<name>.map` and `dmap` paths.

### 3.3 `idFileSystem` — `framework/FileSystem.h:148`

Contract (`FileSystem.h:32-52`): no game code may call stdio; forward slashes
only; all names case-insensitive. Four roots: `fs_basepath` (read-only install),
`fs_savepath` (read-write user data), `fs_cdpath` (read-only media),
`fs_devpath` (writable dev tree).

| Concern | API | Evidence |
|---|---|---|
| Listings | `ListFiles`, `ListFilesTree` | case-insensitive dir cache, `ClearDirCache` |
| Bulk read | `ReadFile`/`FreeFile` (may be cached) | returns length, `0`-terminated |
| Streaming | `OpenFileRead/Write/Append/ByMode`, `OpenExplicitFile*` | `idFile` subclasses: `idFile_Memory`, `idFile_BitMsg`, `idFile_Permanent` (`framework/File.h:116/160/186`) |
| PAK overlay | `FileIsInPAK`, `AddZipFile`, pure-server checksums | zip handling is `framework/Unzip.cpp` (3,857 LOC) |
| DLL lookup | `FindDLL` | used by `LoadGameDLL` |
| Mods | `ListMods`, `FindFile`, `GetMapDecl` | `fs_game` addon paks |

The concrete class is `idFileSystemLocal fileSystemLocal` (`framework/FileSystem.cpp:499`).
`idFile_Permanent` still holds a `FILE *o` (`framework/File.h:205,212`) and uses
`fread/fwrite/fseek` (`framework/File.cpp:1068-1221`) — the stdio ban is a
convention, not an enforcement.

`MAX_OSPATH 256` and `MAX_STRING_CHARS 1024` are hard limits that AI-generated
content must respect (`FileSystem.h:57`, `idlib/Lib.h:95`).

### 3.4 `idCmdSystem` — `framework/CmdSystem.h:72`

Command registry + text buffer. `AddCommand(name, fn, flags, desc, completion)`,
`BufferCommandText(CMD_EXEC_NOW|INSERT|APPEND, text)`,
`ExecuteCommandBuffer()` (called once per frame), plus a large family of
`ArgCompletion_*` helpers (`CmdSystem.h:110-125`).
Flags (`CMD_FL_SYSTEM/RENDERER/SOUND/GAME/TOOL/CHEAT`) allow bulk removal on
module reload — this is how the game DLL unregisters its commands.
Concrete: `idCmdSystemLocal cmdSystemLocal` (`framework/CmdSystem.cpp:115`).

### 3.5 `idCVarSystem` / `idCVar` — `framework/CVarSystem.h:114`

`idCVar` is a static, name-keyed scalar with type flag (`CVAR_BOOL/INTEGER/FLOAT`),
min/max or allowed-string set, completion function, and a linked list of
duplicates that all resolve to one `internalVar` (`CVarSystem.h:151,166-167`).
Registration is via static construction + `idCVar::RegisterStaticVars()`
(`framework/Common.cpp:2798`). Flags of interest: `CVAR_ARCHIVE`, `CVAR_ROM`,
`CVAR_INIT`, `CVAR_CHEAT`, `CVAR_USERINFO`, `CVAR_SERVERINFO`, `CVAR_NETWORKSYNC`.
Concrete: `idCVarSystemLocal localCVarSystem` (`framework/CVarSystem.cpp:486`).

### 3.6 `idDeclManager` — `framework/DeclManager.h:250`

The uniform registry for all "small text" data. Contract (`DeclManager.h:32-63`):

* a decl pointer, once returned, never goes away and is unique per (type, name);
* pointers *inside* a decl may be invalidated by a level change (`EnsureNotPurged`);
* parse errors must warn and call `MakeDefault()`, never `Error()`.

`declType_t` (`DeclManager.h:65-85`) enumerates the 15 types. Registration is
`RegisterDeclType` + `RegisterDeclFolder` in `idDeclManagerLocal::Init`
(`framework/DeclManager.cpp:806-823`):

| Type string | Enum | Allocator | Folder / ext |
|---|---|---|---|
| `table` | `DECL_TABLE` | `idDeclTable` | — |
| `material` | `DECL_MATERIAL` | `idMaterial` | `materials/*.mtr` |
| `skin` | `DECL_SKIN` | `idDeclSkin` | `skins/*.skin` |
| `sound` | `DECL_SOUND` | `idSoundShader` | `sound/*.sndshd` |
| `entityDef` | `DECL_ENTITYDEF` | `idDeclEntityDef` | `def/*.def` |
| `mapDef` | `DECL_MAPDEF` | `idDeclEntityDef` | map-specific |
| `fx` | `DECL_FX` | `idDeclFX` | `fx/*.fx` |
| `particle` | `DECL_PARTICLE` | `idDeclParticle` | `particles/*.prt` |
| `articulatedFigure` | `DECL_AF` | `idDeclAF` | `af/*.af` |
| `pda` | `DECL_PDA` | `idDeclPDA` | `pda/*.pda` |
| `email` / `video` / `audio` | `DECL_EMAIL/VIDEO/AUDIO` | `idDeclEmail` / `idDeclVideo` / `idDeclAudio` (`framework/DeclPDA.h:41,69,95`) | `pda/*.pda` |
| `modelexport` | `DECL_MODELEXPORT` | — | — |

Decl parsing uses a dedicated lexer flag set (`DeclManager.h:93-98`):
no string concat, no escape chars, path names allowed, multi-char literals,
backslash concatenation, **non-fatal errors**.

### 3.7 `sys_public.h` — the platform abstraction — `sys/sys_public.h`

This single header is the port seam, and it is worth reading in full. It is
split between: per-OS macros (42-134), CPU/FPU enums (142-180), events (192-221),
free `Sys_*` functions (239-387), networking types (397-469), threading (480-527),
and the `idSys` vtable (537-571).

| Concern | Symbols | Line |
|---|---|---|
| Compiler/OS macros | `BUILD_STRING`, `CPUSTRING`, `ALIGN16`, `PACKED`, `_alloca16`, `PATHSEPERATOR_*`, `ID_INLINE` | 42-134 |
| CPU feature bits | `CPUID_MMX/SSE/SSE2/SSE3/3DNOW/ALTIVEC/FTZ/DAZ` | 142-158 |
| FPU control | precision, rounding, FTZ/DAZ, exception mask | 175-180, 277-299 |
| Events | `sysEvent_t` = key / char / mouse / joystick / console (`SE_*`) | 192-221 |
| Platform services | clipboard, sleep, ms clock, clock ticks, RAM/VRAM, drive space, memory lock, call stack, DLL load, show window/console, mkdir, timestamps, default paths, list files | 246-387 |
| Networking | `netadr_t` (4-byte IP + `short` port), `idPort` (UDP), `idTCP` | 397-469 |
| Threads | `xthread_t`, `Sys_CreateThread`, 4 named critical sections, 4 trigger events | 480-527 |
| `idSys` | vtable mirror of the above + `OpenURL`, `StartProcess` | 537-571 |
| OpenAL | `Sys_LoadOpenAL`, `Sys_FreeOpenAL` | 575-576 |

`sys/sys_local.cpp:40-41` shows the adapter pattern: `idSysLocal sysLocal; idSys *sys = &sysLocal;`
with every method forwarding to a free `Sys_*` function.

Two portability landmines in this header:

* `typedef unsigned long address_t` (`sys_public.h:234`) — 32-bit on Win32 and
  wasm32, 64-bit on LP64 Linux. Call-stack code assumes it fits an address.
* `_alloca16` casts through `int` (`sys_public.h:52,87,118`), and `_Common.props`
  sets `_USE_32BIT_TIME_T`. The engine is 32-bit-clean but not 64-bit-clean.

### 3.8 `idLib` — `idlib/`

`idLib` is a service locator plus string/exception helpers
(`idlib/Lib.h:50-63`, `95`):

```cpp
static class idSys         *sys;
static class idCommon      *common;
static class idCVarSystem  *cvarSystem;
static class idFileSystem  *fileSystem;
```

These four pointers are injected once in `idCommonLocal::Init`
(`framework/Common.cpp:2768-2771`) and then used by `idLib::Error/Warning`,
`idStr::va`, `idStr::Format`, file helpers, etc. Any port that replaces the
platform layer must keep this injection intact or reimplement `idLib`.

| Component | File | Shape |
|---|---|---|
| `idStr` | `idlib/Str.h:116` (1,515 LOC impl) | growable `char*` with static buffer, `c_str()`/`operator const char*`, `Cmp` (case-sensitive, `Str.h:259`) / `Icmp` (case-insensitive, `Str.h:261`), `Format`, `va()` |
| `idDict` | `idlib/Dict.h:65` | ordered key/value list (`idKeyValue`, `Dict.h:48`), used for entityDefs, GUI state, spawn args; `MatchPrefix`, `GetString/Bool/Int/Float`, `Set` |
| `idLexer` | `idlib/Lexer.h:137` | tokeniser over memory/file, `LEXFL_*` flags, `ReadToken`, `ParseInt/Float/1DMatrix`, source-file + line tracking |
| `idParser` | `idlib/Parser.h:76` | lexer + operator/expression engine (`MAX_EXPRESSION_OPS/REGISTERS`, `idlib/precompiled.h:138-139`), used by `.gui` scripts and material expressions |
| `idToken` | `idlib/Token.h` | token buffer with punctuation table |
| `idCmdArgs` | `idlib/CmdArgs.h` | argv-style tokenizer for console commands |
| `idBitMsg` | `idlib/BitMsg.h` | bit-packed network/file writer with delta ops |
| `idMapFile` | `idlib/MapFile.cpp:720` | `.map` text parser → entities/brushes/patchDefs |
| `idHeap` | `idlib/Heap.cpp:55` (class), `Heap.cpp:33` (`USE_LIBC_MALLOC 0`) | optional custom small/medium/large allocator; libc `malloc` is behind a compile-time switch |
| containers | `idlib/containers/` | `idList` (int-indexed), `idHashTable`/`idHashIndex`, `idStrList`, `idStrPool`, `idStaticList`, `idLinkList`, `idQueue`, `idStack`, `idBTree`, `idHierarchy`, `idVectorSet` |
| geometry | `idlib/geometry/` | `idBounds`, `idWinding`, `idDrawVert`, `idTraceModel`, `idSurface*` |
| math | `idlib/math/` | `idVec2/3/4/5/6`, `idMat2/3/4/5/6`, `idAngles`, `idQuat`, `idRotation`, `idPlane`, `idCurve*`, `idPolynomial`, `idComplex`, `idLcp`, `idODE`, `idSIMD` |
| SIMD | `idlib/math/Simd.cpp` | `idSIMD` abstract ops (`Simd.h:104+`); runtime dispatch by CPUID at `Simd.cpp:64-86` |

`idStr`/`idList` use `int` indices and no exceptions; that is fine for wasm32 but
means large mods (>2 GB asset index) are out of scope.

---

## 4. Engine ↔ game boundary

### 4.1 The contract — `game/Game.h`

| Symbol | Line | Meaning |
|---|---|---|
| `GAME_API_VERSION = 8` | `game/Game.h:321` | hard ABI version; mismatch is fatal |
| `gameImport_t` | `game/Game.h:323-340` | engine → game: `sys`, `common`, `cmdSystem`, `cvarSystem`, `fileSystem`, `networkSystem`, `renderSystem`, `soundSystem`, `renderModelManager`, `uiManager`, `declManager`, `AASFileManager`, `collisionModelManager` |
| `gameExport_t` | `game/Game.h:342-348` | game → engine: `game` (`idGame*`), `gameEdit` (`idGameEdit*`) |
| `GetGameAPI_t` | `game/Game.h:350-352` | `extern "C" gameExport_t *(*)(gameImport_t*)` |
| `idGame` | `game/Game.h:72-193` | 40+ virtuals: map load/save, run frame, draw, ESC/menu, server/client snapshots |
| `idGameEdit` | `game/Game.h:231-308` | editor-facing: spawn args → `renderLight_t`/`renderEntity_t`/`refSound_t`, anim frame baking, AF, map editing |

`d3xp/Game.h:323` declares the same `GAME_API_VERSION 8` and the same import
struct, so the two game modules are interchangeable at the binary level.

### 4.2 Loading — `framework/Common.cpp:2629-2691`

```cpp
fileSystem->FindDLL( "game", dllPath, true );          // Common.cpp:2637
gameDLL = sys->DLL_Load( dllPath );                     // 2644
GetGameAPI = (GetGameAPI_t) Sys_DLL_GetProcAddress(...); // 2650
gameImport.version = GAME_API_VERSION;                  // 2658
... all service pointers ...                            // 2659-2671
gameExport = *GetGameAPI( &gameImport );                // 2673
if ( gameExport.version != GAME_API_VERSION ) FatalError(...); // 2675
game = gameExport.game; gameEdit = gameExport.gameEdit;  // 2682-2683
```

The whole block is inside `#ifdef __DOOM_DLL__` (`Common.cpp:2630`), and the
unload path is `Common.cpp:2698-2709`. Build-system proof of the two variants:
`_DoomDLL.props` defines `__DOOM__;__DOOM_DLL__`, `_Game.props` defines
`__DOOM__;GAME_DLL`; `_Common.props` adds `WIN32;_WINDOWS;_USE_32BIT_TIME_T`.

### 4.3 Why this boundary matters for a port

* The interface is a **vtable ABI**, not a C ABI. It works only when both sides
  are built by a compatible compiler. That is fine for one MSVC version, and it
  is fine for one wasm toolchain — but it rules out mixing a prebuilt game DLL
  with a newer engine.
* **The game module is the intended extension point.** Everything in `idGame`
  is virtual, and `idGameEdit` exists so tools (Radiant, AF editor) can drive
  the game without linking it.
* `#ifdef __DOOM_DLL__` already provides a **static-link mode**: if the macro is
  undefined, `LoadGameDLL` does nothing and the engine expects `game`/`gameEdit`
  to be defined elsewhere (`game/Game.h:195`, `310`; `Common.cpp:2687-2690`).
  This is the cheapest path to a single-binary WASM build.
* Anything new (a browser console, a JS scripting bridge, an AI tool API) should
  be added *behind* `idCommon`/`idSession`/`idGameEdit`, not by patching the
  game. Otherwise mods stop being data-only.

---

## 5. Platform / OS / GL / OpenAL / SIMD coupling register

Each row is a concrete blocker with its evidence. "Fix" is deliberately one line;
the staged plan is §9.

### 5.1 OS and build system

| # | Coupling | Evidence | Blocks |
|---|---|---|---|
| O1 | Win32 types in public engine headers | `framework/Common.h:86-91` (`HWND com_hwndMsg`), `sys/sys_public.h:42-62` | compiling `framework/` on any non-Win32 target |
| O2 | `windows.h`, `winsock2.h`, `dinput.h`, `dsound.h` pulled into **every** translation unit via the precompiled header | `idlib/precompiled.h:65-73,84` | any non-Windows build; also makes compile times and namespace pollution huge |
| O3 | Renderer header (and therefore GL) is in the precompiled header | `idlib/precompiled.h:142` | headless/dedicated builds, test doubles |
| O4 | `WinMain` is the only entry point; no `main()` fallback in-tree | `sys/win32/win_main.cpp:1365`; compare `sys/posix/posix_main.cpp`, `sys/linux/main.cpp` | Emscripten/SDL entry |
| O5 | Threads = `CreateThread`, 4 fixed critical sections, 4 fixed events | `sys/sys_public.h:480-527`; `sys/win32/win_main.cpp:93-94`; `sys/win32/win_glimp.cpp:1102` (render thread) | single-threaded web main loop; SharedArrayBuffer requirements |
| O6 | 60 Hz async tick + sound mixer on a second thread | `framework/Common.cpp:2581-2622`; `sound/snd_system.cpp:622-626` | deterministic frame stepping in a browser tab |
| O7 | FPU control (precision, rounding, FTZ/DAZ, x87 stack) | `sys/sys_public.h:277-299`; `sys/win32/win_main.cpp:1405` | x87 does not exist in wasm; no-op shims required |
| O8 | CPUID probing drives SIMD and denormal handling | `sys/sys_public.h:142-158`; `sys/win32/win_main.cpp:1137`; `framework/Common.cpp:2823` | must return `CPUID_GENERIC`-safe values |
| O9 | Directory listing via CRT `_findfirst/_findnext` | `sys/win32/win_main.cpp:542-579` (esp. 565) | browser has no directory enumeration; needs a pak manifest |
| O10 | `FILE*` + `fread/fwrite/fseek/fflush` | `framework/File.h:205,212`; `framework/File.cpp:1068,1120,1221` | browser FS is async; needs MEMFS/OPFS bridge or async VFS |
| O11 | `ID_TIME_T` is `time_t`, with `_USE_32BIT_TIME_T` on Win32 | `idlib/precompiled.h:36`; `_Common.props` | 2038 rollover, and a type-width mismatch vs Emscripten's 64-bit `time_t` in timestamp comparisons |
| O12 | 32-bit pointer/`int` assumptions | `sys/sys_public.h:52,234`; `_Common.props` | wasm32 is fine; wasm64/LP64 is not |
| O13 | Clipboard via Win32 API | `sys/win32/win_main.cpp:587-598` | no-op or Async Clipboard API |
| O14 | Win32 console window for `Sys_ShowConsole`/notify lines | `framework/Console.cpp`; `sys/win32/win_syscon.cpp` (467 LOC) | replace with DOM overlay |
| O15 | DLL loading is a first-class OS service | `sys/sys_public.h:328-331`; `sys/win32/win_main.cpp:660-666`; `common->LoadGameDLL` | no `dlopen` in a browser; see §4.3 |
| O16 | UDP/TCP sockets + 4-byte IPv4 `netadr_t` | `sys/sys_public.h:397-469`; `sys/win32/win_net.cpp:638` (`WSAStartup`) | browsers expose neither UDP nor raw TCP |
| O17 | DirectInput 8 for keyboard/mouse | `sys/win32/win_input.cpp:502-587` (515 `DirectInput8Create`) | replace with DOM events → `sysEvent_t` |
| O18 | Message pump / window proc | `sys/win32/win_wndproc.cpp` (386 LOC) | SDL or Emscripten HTML5 backend |
| O19 | High-resolution timers via QPC | `sys/sys_public.h:265-271`; `sys/win32/win_main.cpp` | `performance.now()`; sub-ms granularity is clamped in browsers |
| O20 | CD-key / auth / MAPI / shell-exec paths | `framework/Session.h:122-146`; `sys/win32/win_main.cpp:1247` | dead code for a web port; delete early |

### 5.2 OpenGL

| # | Coupling | Evidence | Blocks |
|---|---|---|---|
| G1 | GL entry points are a **function-pointer table** filled at runtime by loading `opengl32.dll` | `renderer/qgl.h:181,444,497`; `sys/win32/win_qgl.cpp:1696-1698` (`LoadLibrary`), `2050` (`wglGetProcAddress`) | WebGL has no `GetProcAddress`; table must be statically bound |
| G2 | Context creation is WGL-only on Win32 | `sys/win32/win_glimp.cpp:468` (`SetPixelFormat`), `477` (`wglCreateContext`), `608` (`CreateWindowEx`) | needs EGL/canvas (Emscripten) or SDL |
| G3 | GLX path is X11-only | `sys/linux/glimp.cpp:281` (`XOpenDisplay`), `294` (`GLX_Init`) | not usable in-browser |
| G4 | Renderer uses GL 1.x fixed function + client arrays | `renderer/tr_backend.cpp:55-72`; `qgl.h:181,444,497`; 21 `qglBegin` call sites in `renderer/tr_rendertools.cpp` | WebGL 1/2 have **no** fixed-function pipeline, no `glBegin/glEnd`, no display lists |
| G5 | Five vendor back ends selected at runtime | enum `BE_ARB, BE_NV10, BE_NV20, BE_R200, BE_ARB2, BE_BAD` (`renderer/tr_local.h:673-678`); selection `renderer/RenderSystem.cpp:536-607` | only an ARB2-equivalent GLSL ES backend is viable; NV/R200/ARB1 are dead weight |
| G6 | ARB vertex/fragment **assembly** programs uploaded at runtime | `renderer/draw_arb2.cpp:69-80` (`qglProgramEnvParameter4fvARB`, `GL_VERTEX_PROGRAM_ARB`) | ARB asm does not exist in WebGL; must be rewritten as GLSL ES 3.0 shaders |
| G7 | Extension probing by string, incl. `GL_ARB_*` and `GL_NV_register_combiners` | `renderer/RenderSystem_init.cpp:334-518` (`R_CheckPortableExtensions`), `669-671` | many extensions absent; feature flags must be forced |
| G8 | Stencil-shadow volume path | `renderer/tr_stencilshadow.cpp` (1,157 LOC), `tr_backend.cpp` state setup | WebGL2 supports stencil + `INCR_WRAP`/`DECR_WRAP` + separate front/back ops, so this survives, but needs a depth-stencil format and careful `glStencilOpSeparate` mapping |
| G9 | "Portable extensions" includes ARB2 shader paths that are actually optional | `renderer/RenderSystem_init.cpp:330-334` | naming is misleading: it checks *optional* ARB features, not portability |
| G10 | Renderer writes demo files directly from the back end | `framework/Session.h:156-160`; renderer demo code | demos become coupled to backend selection |

### 5.3 OpenAL

| # | Coupling | Evidence | Blocks |
|---|---|---|---|
| A1 | AL/ALC symbols are loaded by hand into a global function table | `openal/idal.cpp:28-60` (`GetProcAddress` per symbol); `openal/idal.h:21,36-42` (`#define alSourceStop idalSourceStop`) | the indirection is good news: a new backend only has to fill the table |
| A2 | The loader is **Win32-typed** (`HMODULE`, `ALAPIENTRY`) | `openal/idal.cpp:36`; `sys/win32/win_snd.cpp:512` (`Sys_LoadOpenAL`) | must be replaced, but the call sites are already abstracted |
| A3 | EAX 2/3/4 reverb is used if present | `sound/snd_system.cpp:350-363` (`alEAXSet`, `EAXSetBufferMode`) | EAX is Windows-only and gone; guard or delete |
| A4 | Sound mix runs on the async thread and talks to hardware | `sound/snd_system.cpp:596-626,742-748`; `Sys_Sleep(100)` on shutdown `536-537` | WebAudio/AudioWorklet model differs (pull-based render quantum) |
| A5 | `idSoundSystem` is a hard dependency of `gameImport_t` | `game/Game.h:333` | a stub `idSoundSystem` is required for headless/silent builds |
| A6 | PCM/OGG decoding is in-engine | `sound/snd_wavefile.cpp:83-85` (`.ogg`), `snd_decoder.cpp` | fine to keep, but decoding should move to a worker |

### 5.4 SIMD

| # | Coupling | Evidence | Blocks |
|---|---|---|---|
| S1 | Runtime dispatch over 6 implementations | `idlib/math/Simd.cpp:64-86` (`CPUID_ALTIVEC/SSE3/SSE2/SSE/3DNOW/MMX`) | wasm has SIMD128, not SSE asm |
| S2 | MSVC inline assembly, ~1,433 `__asm` blocks in one file | `idlib/math/Simd_SSE.cpp:633` (sample); 16,102 LOC total | will not compile outside x86 MSVC |
| S3 | Additional asm engines | `Simd_SSE2.cpp` (65 `__asm`), `Simd_MMX.cpp` (6), `Simd_3DNow.cpp` (1), `Simd_AltiVec.cpp` (389 KB, PPC) | all dead on wasm/ARM |
| S4 | A generic C++ fallback already exists | `Simd_Generic.cpp` (84 KB), `Simd.cpp:53` (`generic->cpuid = CPUID_GENERIC`) | **the port does not need SIMD**; compile generic only |
| S5 | FPU FTZ/DAZ toggled from CPUID result | `Simd.cpp:102-107`; `sys/sys_public.h:295-299` | no-op on wasm; keep values consistent |
| S6 | Scalar math is huge but ordinary C++ | `idlib/math/Matrix.cpp` (6,881 LOC), `Vector.cpp`, `Angles.cpp`, `Quat.cpp` | portable as-is |

---

## 6. Portability ranking

Score: **A** = portable as-is, **B** = portable with a platform shim,
**C** = needs a targeted rewrite, **D** = replace the implementation,
**E** = platform-locked / delete.

| Rank | Component | Score | Rationale | Cheapest action |
|---|---|---|---|---|
| 1 | `idlib/containers`, `idlib/hashing`, `idlib/geometry`, `idlib/bv` | A | pure C++, no OS calls | none |
| 2 | `idlib/{Str,Dict,Lexer,Parser,Token,CmdArgs,Base64,MapFile}` | A | pure C++; `idLib` service pointers must be initialised | none |
| 3 | `idlib/math` (scalar) | A | pure C++ | compile with `Simd_Generic` |
| 4 | `ui/` | B+ | pure data/GUI logic; only `sysEvent_t` in `HandleEvent` | keep; feed DOM events |
| 5 | `cm/` | B+ | pure geometry/algorithms; reads `idFile`, `idLexer` only | keep |
| 6 | `framework/` minus async | B | `idFileSystem`/`File` touch stdio and `Sys_*`; everything else is interface-driven | shim `Sys_*`; VFS |
| 7 | `sound/` (world/emitter/shader) | B− | API clean (`sound.h:150,197,283`); mixer is AL-specific | keep API, replace back end |
| 8 | `idlib/math/Simd_*` | C | inline asm, vendor ISAs | drop, or add a wasm SIMD128 engine |
| 9 | `framework/async/` | D | UDP/TCP, snapshots, pure-server checksums | drop for SP-only |
| 10 | `openal/` | D | hand-rolled `HMODULE` loader | replace with Emscripten AL or WebAudio |
| 11 | `renderer/` front end (`Interaction`, `tr_light`, `tr_shadowbounds`, `tr_stencilshadow`) | C | platform-neutral scene logic, but tied to `qgl` types and backend enum | keep and retarget |
| 12 | `renderer/` back ends (`draw_arb/nv10/nv20/r200/exp`) | D | vendor GL extensions | delete all but a new GLES3 backend |
| 13 | `sys/win32`, `sys/linux`, `sys/osx` | E | OS-locked | replace with `sys/emscripten` (+`sys/sdl` for desktop) |
| 14 | `game/` | B | portable C++, but is a DLL with `GAME_API_VERSION`/`__DOOM_DLL__` | link statically in wasm, keep the struct |

**Bottom line:** ~200k LOC of the in-scope tree (idlib + framework + ui + cm +
sound + game) is ordinary C++03 and survives a port almost unchanged. The work
is concentrated in ≈35k LOC of platform code, ≈16k LOC of SIMD asm, and the
renderer's GL back ends.

---

## 7. Data formats parsed

| Format | Extension | Parser / owner | Encoding | Producer | Migration note |
|---|---|---|---|---|---|
| Declaration (generic) | `*.mtr`, `*.def`, `*.skin`, `*.sndshd`, `*.fx`, `*.prt`, `*.af`, `*.pda`, `*.table` | `idDeclManager` (`framework/DeclManager.cpp:806-823`) + per-type `Parse` | text, `idLexer` with `DECL_LEXER_FLAGS` | authored | **primary AI modding surface** |
| Entity definition | `def/*.def` | `idDeclEntityDef` (`framework/DeclEntityDef.cpp`) → `idDict` | text; supports `inherit` | authored | class hierarchy = key/value inheritance |
| Material | `materials/*.mtr` | `idMaterial` (`renderer/Material.cpp`) | text; expression ops via `idParser` | authored | references stage/blend/lighting keywords tied to GL paths |
| Sound shader | `sound/*.sndshd` | `idSoundShader` (`sound/sound.h:78`, impl `sound/snd_shader.cpp`) | text | authored | playback flags are AL-neutral |
| FX / particle | `fx/*.fx`, `particles/*.prt` | `idDeclFX`, `idDeclParticle` | text | authored | renderer-facing; not GL-specific |
| Articulated figure | `af/*.af` | `idDeclAF` (`framework/DeclAF.cpp`) | text | AF editor | pure physics data |
| PDA / email / video / audio | `pda/*.pda` | `idDeclPDA` (`framework/DeclPDA.cpp`) | text | authored | gui/cinematic hooks |
| GUI | `guis/*.gui` | `idUserInterfaceLocal::InitFromFile` (`ui/UserInterface.cpp:269-302`), `idParser` at 290; script blocks `ui/GuiScript.cpp:345` | text | authored | **primary UI modding surface** |
| Game script | `script/*.script` | `game/script/Script_Compiler.cpp` (75 KB) → bytecode in `Script_Program` | text → bytecode | authored | inside the game module, so data-only mods work without an engine build |
| Map (source) | `maps/*.map` | `idMapFile::Parse` (`idlib/MapFile.cpp:717-740`) | text (Quake brushes/patchDef) | authored / Radiant | needs `dmap` to run |
| Proc (compiled map) | `maps/*.proc` | `idLexer`; magic `mapProcFile003` (`renderer/RenderWorld.h:40-41`), read at `renderer/RenderWorld_load.cpp:496-534` and `cm/CollisionModel_load.cpp:119-128` | **text token stream** written with `WriteFloatString` (`tools/compilers/dmap/output.cpp:644-653`) | `dmap` | precompile in CI; the browser must not run `dmap` |
| Collision model | `maps/*.cm` | `cm/CollisionModel_load.cpp` (lexer), writer `cm/CollisionModel_files.cpp:65-200` (`WriteFloatString`) | **text token stream** | `dmap` | precompile in CI |
| MD5 mesh / anim | `*.md5mesh`, `*.md5anim` | `renderer/Model_md5.cpp` | text | exporter | portable |
| ASE / LWO / MA | `*.ase`, `*.lwo`, `*.ma` | `Model_ase.cpp`, `Model_lwo.cpp` (104 KB), `Model_ma.cpp` | text / binary | DCC | portable but heavy; prune |
| Audio | `*.wav`, `*.ogg` | `snd_wavefile.cpp:83-85`, `snd_decoder.cpp` | binary | authored | decode off main thread |
| PAK | `*.pk4` | `framework/Unzip.cpp`; `idFileSystemLocal` | zip | packaged | replace with JS/zip VFS or preload to MEMFS |
| Save game | `*.save` | `idFile` binary reads/writes, `Session.cpp` | binary structs | runtime | not needed for a browser port |
| Demo | `*.demo` | `framework/DemoFile.cpp` | binary | runtime | backend-coupled; drop |

Note: **`.proc` and `.cm` are text, not binary.** That is a real advantage — they
can be generated or inspected by tooling, and endianness is a non-issue. The
binary formats are save games and demos.

---

## 8. AI-friendly modding surfaces (no engine source changes)

The engine already exposes a data-only extension system. An AI pipeline should
target these in order of safety:

| Surface | What can change | Mechanism | Risk |
|---|---|---|---|
| `def/*.def` entityDefs | new entities, weapon/monster stats, spawn args, inheritance | `idDict` + `idDeclEntityDef`; `inherit` key | low; parse errors default the decl (`DeclManager.h:59-60`) |
| `script/*.script` | new behaviours, state machines, weapon logic, AI hooks | compiled by the game module at runtime | medium; runtime errors are game-side |
| `guis/*.gui` | HUD, menus, PDA screens, bindings | `idParser` + `idGuiScript` | low; parser is non-fatal for GUIs (`LEXFL_NOFATALERRORS`, `ui/UserInterface.cpp:290`) |
| `materials/*.mtr` | surface look, stage/blend, light interaction | `idMaterial` | medium; invalid stage keywords warn and default |
| `sound/*.sndshd`, `fx/*.fx`, `particles/*.prt`, `af/*.af` | audio, effects, physics rigs | decl types | low |
| `pda/*.pda` | story text, audio logs, video | decl types | low |
| `maps/*.map` | geometry, entity placement | `idMapFile`; needs `dmap` for `.proc`/`.cm` | high; requires the native compiler |
| `*.pk4` + `fs_game` | packaging and mod isolation/overlay | `ListMods`/`FindFile`/pure checksums | low |

Practical constraints for generated content:

* **Names are the API.** Lookups are case-insensitive string matches
  (`idStr::Icmp`, `FileSystem.h:283-284`), so generated names must be stable and
  collision-free; prefer a namespace prefix.
* **Hard limits**: `MAX_STRING_CHARS 1024` (`idlib/Lib.h:95`), `MAX_OSPATH 256`
  (`FileSystem.h:57`), `MAX_PURE_PAKS 128`.
* **Decl identity is permanent** for the process lifetime (`DeclManager.h:44-49`),
  so redefining the same name live requires `Reload`/`Invalidate`, not rename.
* **Read-only cataloguing is possible without parsing media**:
  `idDeclManager::DeclByIndex(type, i, forceParse=false)` (`DeclManager.h:294`)
  plus `ListType`/`PrintType` (`DeclManager.h:297-298`) give an AI a complete
  index of what exists. That is the cleanest hook for a "what can I change?"
  query, and it needs no source change.
* Runtime knobs are `idCVar` + console commands (`CmdSystem.h:80`,
  `CVarSystem.h:114`); a web port that keeps the command buffer keeps the whole
  existing debugging surface for free.
* **Do not** extend behaviour by patching `idGame` vtables from a mod: that
  breaks `GAME_API_VERSION 8` and every other mod.

---

## 9. Migration recommendations

### 9.1 Target shape

```
main (Emscripten/SDL2)
  └─ sys/emscripten/   Sys_* + GLimp_* + idSysLocal          [new, ~2k LOC]
  └─ framework/        unchanged, minus async/net/auth        [trim]
  └─ idlib/            generic SIMD only, no asm              [compile flags]
  └─ cm/ ui/ sound/    unchanged; new audio back end          [shim]
  └─ renderer/         new draw_gles3 back end + GLSL ES 3.0  [new, largest task]
  └─ game/             statically linked, __DOOM_DLL__ off    [build config]
```

Recommended order (each step buildable and testable on its own):

1. **De-Windows the core (1–2 weeks).** Remove `windows.h`/`dinput.h`/`dsound.h`
   from `idlib/precompiled.h` (O2) and move `qgl.h` out of it (O3). Move `HWND`
   out of `framework/Common.h` (O1). Compile `idlib` + `framework` with a stub
   `sys/stub/` (581 LOC already present in `sys/stub`) — this alone proves the
   core is portable.
2. **New platform module.** Implement `sys/emscripten/` against `sys_public.h`:
   `Sys_Milliseconds`/`GetClockTicks` → `performance.now()`, `Sys_ListFiles` →
   VFS manifest (O9), clipboard → no-op, DLL → static table, threads → no-op
   or `pthread` when `SharedArrayBuffer` is available, FPU/CPUID → fixed
   `CPUID_GENERIC` (O7/O8). Keep the `idSysLocal` adapter pattern
   (`sys/sys_local.cpp:40`) — it is the cheapest correct shape.
3. **Collapse the async thread.** Make `idCommon::Async` a function the main
   loop calls once per frame (`framework/Common.cpp:2584`), and drive the sound
   mixer from `AudioWorklet` render quanta. This removes O5/O6 and the
   `Sys_Sleep(100)` shutdown hack (`sound/snd_system.cpp:536-537`).
4. **Replace GL.** Two sub-steps, in this order:
   * Rewrite `qgl.h`'s table as a thin static binder (on web every function
     exists; on desktop keep `SDL_GL_GetProcAddress`). Keep the *signatures* so
     the renderer compiles unchanged.
   * Delete `draw_nv10`, `draw_nv20`, `draw_r200`, `draw_exp`, `draw_arb` and
     `BE_NV*`/`BE_R200` (`renderer/tr_local.h:673-678`). Keep the ARB2 stage
     logic (`renderer/draw_arb2.cpp`) as the reference and port its ARB assembly
     to GLSL ES 3.0. Keep the front end (`Interaction`, `tr_light`,
     `tr_stencilshadow`) — it is the valuable, platform-neutral part.
   * Target **WebGL2 / GLES3** (`-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`,
     `-sFULL_ES3`). WebGL1 is not enough: no `glStencilOpSeparate`, no
     `INCR_WRAP`/`DECR_WRAP`, no integer textures, no UBOs.
   * Stencil shadows survive: WebGL2 has a real depth-stencil buffer and
     two-sided stencil ops. The failure mode to watch is `r_useDepthBoundsTest`
     and `GL_EXT_depth_bounds_test`, which does not exist — force it off.
5. **VFS over PAK.** Do not preload 500 MB into MEMFS. Implement
   `idFileSystemLocal` on top of a JS zip reader over `fetch`, with an index
   built from the zip central directory; keep `OpenFileRead` synchronous by
   pre-reading small text files (decls, GUIs) during startup and streaming only
   large media. Writes go to OPFS/IndexedDB. Keep `MAX_OSPATH 256` as the
   virtual-path limit.
6. **Audio.** Either link Emscripten's OpenAL (it implements on top of WebAudio)
   and keep `sound/` untouched, or write a native `idSoundSystem` back end.
   Either way, delete EAX (`sound/snd_system.cpp:350-363`) and the `idal.cpp`
   `HMODULE` loader (A1/A2). `idSoundEmitter`/`idSoundWorld`/`idSoundShader`
   (`sound/sound.h:150,197,283`) are good interfaces — keep them.
7. **Game module.** Build with `__DOOM_DLL__` undefined and statically link the
   `game/` sources; `LoadGameDLL` (`framework/Common.cpp:2629`) is already
   `#ifdef`-guarded, and `game`/`gameEdit` are declared `extern`
   (`game/Game.h:195,310`). If mod loading from separate WASM modules is ever
   required, mirror `gameImport_t` with a hand-written C dispatch table instead
   of relying on vtable layout — that is the only way to make the ABI stable
   across toolchains.
8. **Networking.** For single-player, compile out `framework/async/`,
   `sys/win32/win_net.cpp`, and the CD-key/auth paths
   (`framework/Session.h:122-146`). If multiplayer is in scope, bridge
   `idPort`/`idTCP` (`sys_public.h:412-457`) to WebSocket/WebRTC in JS; the
   snapshot code itself (`idBitMsg`, `ServerWriteSnapshot`) is portable.
9. **Content pipeline.** Keep `dmap` as a native/container build step producing
   `.proc` and `.cm` (both text — see §7). The browser never needs the compilers,
   only the outputs; that is what makes maps moddable without shipping a toolchain.

### 9.2 Build flags / toolchain notes

* Use **wasm32**, not wasm64: `address_t`, `_alloca16`, `int` pointer casts, and
  `MAX_OSPATH` all assume 32-bit (O12). wasm32 is the path of least resistance.
* **Enable C++ exceptions** or convert error handling. `idCommonLocal::Init`
  wraps everything in `try/catch (idException&)` (`framework/Common.cpp:2765,
  2862`), `idException` is in `idlib/Lib.h:150`, and `common->Error` throws.
  Emscripten's default is `-fno-exceptions`; use `-sDISABLE_EXCEPTION_CATCHING=0`
  (or refactor to return codes — more invasive than it looks, because `Error`
  is called from deep inside parsers).
* **`setjmp`-free, but not `longjmp`-free in spirit**: any engine `Error`
  unwinds to the frame loop. In a browser that means "reload the level" is the
  only recovery — do not try to resume.
* The tree is C++03. Compiling with a modern standard works, but `idStr`'s
  implicit `operator const char*`, `TypeInfo` (`neo/TypeInfo/`, generated
  `idList` instantiation) and `_USE_32BIT_TIME_T` are the usual breakages.
* Prefer SCons/CMake over the `.vcxproj` set (`neo/SConstruct` exists); the
  project files hard-code 32-bit Win32 and the `_*` props split.

### 9.3 What *not* to port

| Thing | Why |
|---|---|
| `sys/osx/` (13 files, 1.8k LOC) | partial legacy port; superseded by SDL on every target |
| `Simd_MMX`, `Simd_3DNow`, `Simd_SSE2`, `Simd_SSE3`, `Simd_AltiVec` | dead ISAs; `Simd_Generic` covers correctness |
| EAX reverb (`snd_efxfile.cpp`, EAX calls) | Windows-only, absent from WebAudio/OpenAL-soft defaults |
| CD key / auth / `WaitingForGameAuth` | no backend, no point |
| DirectInput, Win32 raw input, `win_syscon` | DOM events / overlay |
| `draw_exp`, `draw_nv10`, `draw_nv20`, `draw_r200` | vendor extensions from 2004 |
| Demo recording | backend-coupled binary format; low value for web |

---

## 10. Gotchas checklist (ordered by likelihood of biting)

1. `idStr` implicit conversions + `va()` are not thread-safe; the async tic used
   to rely on a single writer. When you collapse threads, audit any `va()` use
   in the sound path.
2. `MAX_STRING_CHARS 1024` truncates long AI-generated descriptions silently in
   some `idStr` constructors — validate before writing decls.
3. `idDecl` pointers are permanent but *contents* are not; `EnsureNotPurged`
   (`DeclManager.h:161-162`) is mandatory after `BeginLevelLoad`.
4. Binary search/caches assume the `idStr` case-insensitive comparator; a
   case-sensitive VFS will change mod loading order and pure checksums.
5. `renderer/qgl.h` is included by the precompiled header (O3), so a headless
   test binary must either provide `qgl` stubs or split the PCH.
6. `Sys_GetCallStack`/`address_t` is best-effort; on wasm leave it empty rather
   than pretending.
7. `ID_TIME_T` widened by Emscripten changes file-timestamp comparisons used by
   `DeclManager` reload logic (`SourceFileChanged`) — verify after the port.
8. `.proc`/`.cm` are text but **generated**; never hand-edit them, regenerate
   from `.map` with `dmap`.
9. `gameImport_t` struct order is the ABI. Adding a member breaks every
   prebuilt game module; bump `GAME_API_VERSION` and keep old layouts behind a
   version check if binary mods matter.
10. The engine assumes a **single** `game` object for the process; there is no
    context/session isolation. A browser tab = one game instance.

---

## 11. One-paragraph summary

`framework/` is a set of vtable-defined services (`idCommon`, `idSession`,
`idFileSystem`, `idCmdSystem`, `idCVarSystem`, `idDeclManager`) that sit on
`idlib/` (strings, dicts, lexers, containers, geometry, math) and call the OS
only through `sys_public.h`'s `Sys_*` functions and the `idSys` vtable. The game
is a DLL implementing `idGame`/`idGameEdit`, negotiated through
`gameImport_t`/`gameExport_t` at `GAME_API_VERSION 8` and loaded by
`idCommonLocal::LoadGameDLL` (`framework/Common.cpp:2629`), guarded by
`__DOOM_DLL__`. Roughly 200k LOC of the in-scope tree ports without structural
change; the migration cost is concentrated in `sys/win32` (14.7k LOC), the SIMD
asm (16k LOC in one file), the OpenAL shim (310 LOC, but Win32-typed), and the
renderer's five vendor GL back ends. A WebGL2/GLES3 target needs a new
`draw_gles3` back end, GLSL ES rewrites of the ARB assembly programs, a static
`qgl` binder, a pak-only VFS, a single-threaded tick with AudioWorklet, and a
statically linked game module. The declarative data layer (`.def`, `.mtr`,
`.skin`, `.sndshd`, `.fx`, `.prt`, `.af`, `.pda`, `.table`, `.gui`, `.script`)
already provides a safe, source-free surface for AI-authored mods, and
`idDeclManager::DeclByIndex(..., forceParse=false)` is the read-only index an AI
tool should use to discover it.
