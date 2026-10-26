# 05 — Build System, Dependencies, Game Data and Tools

Read-only analysis of `doom3_orig/doom3/neo` + `doom3_orig/doom3/base`. All paths are relative to `doom3_orig/doom3/` unless stated.
Every claim below is anchored to a file; counts were produced by the command recorded in
[§9 Evidence](#9-evidence).

---

## 1. Build targets and what each one produces

Windows: `neo/doom.sln` (Format 11.00, `# Visual Studio 2010`) with 7 solution configurations, all
`|Win32` only — `Debug`, `Debug with inlines`, `Debug with inlines and memory log`, `Release`,
`Dedicated Debug`, `Dedicated Debug with inlines`, `Dedicated Release` (`neo/doom.sln:24-31`).
Output root is `..\build\$(PlatformName)\$(Configuration)\` (`neo/_Common.props:6-7`).

| Project | Kind (`ConfigurationType`) | Output | Notes |
|---|---|---|---|
| `doomdll.vcxproj` | Application | `DOOM3.exe` (`neo/_DoomDLL.props:18`) | Win32 GUI subsystem, 16 MB stack reserve+commit, `LargeAddressAware`, preprocessor `__DOOM__;__DOOM_DLL__` (`_DoomDLL.props:10,21-22,23`). Historically named "DLL" because the game logic is a separate DLL. |
| `game.vcxproj` | DynamicLibrary | `gamex86.dll` (`neo/_Game.props:15`) | Module def `game\game.def`; defines `__DOOM__;GAME_DLL`; pre-build step runs `TypeInfo.exe` (`_Game.props:9,15-16,19`). |
| `game-d3xp.vcxproj` | DynamicLibrary | `gamex86.dll` — same filename (`neo/_Game-d3xp.props:9,13-14`) | Adds `_D3XP;CTF`, def `d3xp\game.def`. Two projects emit the same name, so they are built/installed one at a time (`Resurrection of Evil` variant). |
| `idlib.vcxproj` | StaticLibrary | `idlib.lib` | Precompiled header `Use`, define `__IDLIB__` (`neo/_idlib.props:9-10`). No `TargetName`, so the default name is used. |
| `typeinfo.vcxproj` | Application (Console) | `TypeInfo.exe` | Code generator for `game/gamesys/TypeInfo.cpp`; defines `ID_ENABLE_CURL=0;ID_TYPEINFO;__DOOM_DLL__` (`neo/_TypeInfo.props:9-10,13`); references `idlib` (`neo/typeinfo.vcxproj`). |
| `curllib.vcxproj` | StaticLibrary | `curllib.lib` | Despite `USRDLL;CURLLIB_EXPORTS` defines (`neo/_Curl.props:10`), the project is a static lib. |
| `MayaImport.vcxproj` | DynamicLibrary | Maya exporter DLL | Needs the **Autodesk Maya 5.0 SDK**: include paths `mssdk/include;MayaImport/Maya5.0`, libs `Foundation.lib;OpenMaya.lib;OpenMayaAnim.lib` (`neo/_MayaImport.props:9,15-16`); def `MayaImport/mayaimport.def`. |
| `neo/tools/*` | — | — | **No project file exists in the repo.** `doom.sln` contains no tool project (`neo/doom.sln:3-22` lists only libs/exes/dlls). Radiant, MaterialEditor, GUIEd, AF editor, Decl browser, Particle/PDA/Sound/Script editors and the script debugger are source-only. |

Linux/macOS: `neo/SConstruct` + `neo/sys/scons/SConscript.{core,game,idlib,curl,gl}` (and
`neo/sys/osx/` Xcode 3.2 projects). SCons targets: `doom.<cpu>`, `doomded.<cpu>`,
`game<cpu>-base.so`, `game<cpu>-d3xp.so`, `doom-mon.<cpu>` (monolithic), `doom-demo.<cpu>`,
`game<cpu>-demo.so`, `setup`, `setup-demo`, `sdk`, plus optional `game01-base.pk4`
(`neo/SConstruct:428-517`).

`SConstruct` is **Python 2 only** — `print` statement, `commands`, `popen2`, `dict.has_key`,
`pickle` (`neo/SConstruct:6,191,197`). It cannot run under Python 3 or a current SCons.

Note: `neo/doom.suo`, `neo/doomdll.vcxproj.user`, `neo/*.vcxproj.user` and hard-coded install
paths (`neo/installdebug.bat:1-2`: `f:\doom\doom.exe`, `g:\doom`) are committed machine state.

---

## 2. Third-party libraries and licenses

Licenses are enumerated by id in `README.txt` under **EXCLUDED CODE** — that code is *not* covered
by the GPL and carries its own terms.

| Component | Location | Version / license | Used by |
|---|---|---|---|
| libcurl | `neo/curl/**` | **7.11.1** (`neo/curl/include/curl/curl.h`); MIT/X-style (`README.txt` "Curl library") | `ID_ENABLE_CURL`, HTTP/FTP download of pk4s; built by `sys/scons/SConscript.curl`; disabled with `NOCURL=1` |
| IJG JPEG | `neo/renderer/jpeg-6/**` | **6b** (README note: bundled README is from 6b); IJG terms, requires attribution "based in part on the work of the Independent JPEG Group" | `Image_load.cpp`; sources listed in `SConscript.core` |
| Ogg Vorbis | `neo/sound/OggVorbis/{ogg,vorbissrc}/**` | Xiph.org BSD-3 | `.ogg` sound decoding, compiled as `libogg` (`SConscript.core`) |
| OpenAL SDK | `neo/openal/{include,lib,docs,osx}/**` | LGPL-2+ headers; **prebuilt 32-bit `openal32.lib` + `eaxguid.lib`** | Sound backend; `_DoomDLL.props:16-17` |
| zlib (embedded) | `neo/framework/Unzip.cpp` | zlib licence (Gailly/Adler) + Gilles Vollant's minizip code | `.pk4` reading |
| PropTree | `neo/tools/common/PropTree/**` | Scott Ramsay, permissive "as is" | MFC property grid in the tools |
| Base64 | `neo/idlib/Base64.cpp` | Lars Wirzenius BSD-2 | net/base64 |
| MD4 / MD5 / CRC32 | `neo/idlib/hashing/` | RSA MD4 notice / public domain MD5 / Mark Adler | hashing, pk4 integrity |
| OpenGL headers | `neo/renderer/glext.h`, `wglext.h` | SGI Free Software License B | GL extension declarations |
| DirectX SDK | external, **June 2010**, path hard-coded `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x86` | Microsoft EULA | `dinput8`, `dsound`, `dxguid`, `DxErr` (`neo/_DoomDLL.props:16-17`) |
| MFC | MSVC static libs `nafxcw.lib` / `nafxcwd.lib` | MSVC runtime EULA | **all Windows tools** (`neo/_Release.props:22`, `neo/_Debug.props:18`) |

Everything else in `neo/{framework,renderer,cm,ui,sound,sys,idlib,game,d3xp,TypeInfo}` is id
Software code under GPLv3 (`COPYING.txt`).

**MFC consequence:** the editors are not portable. `neo/tools/comafx/StdAfx.h` pulls in the MFC
headers; 15 tool files include `<afx...>` directly (Radiant dialogs, MaterialEditor views,
`QE3.H`, `WIN_QE3.RC2`). There is no non-Windows path for these tools.

---

## 3. Compiler and toolchain assumptions

- **MSVC v100 / Visual Studio 2010.** `ToolsVersion="4.0"`, `_ProjectFileVersion 10.0.40219.1`,
  **no `<PlatformToolset>` element anywhere** → defaults to v100; `doom.sln` is VS2010 format.
  Only `|Win32` platforms exist.
- **Windows-only client on the MSVC path**: `WIN32;_WINDOWS;_USE_32BIT_TIME_T`
  (`neo/_Common.props:11`); libs `dbghelp, dinput8, dsound, dxguid, DxErr, eaxguid, glu32,
  iphlpapi, odbc32, opengl32, winmm, wsock32` (`_DoomDLL.props:16`).
- **32-bit by construction**: DirectX `Lib\x86`, prebuilt 32-bit `openal32.lib`, output name
  `gamex86.dll`, `_USE_32BIT_TIME_T`, `LargeAddressAware` (to use >2 GB on 32-bit).
- **GCC/Linux path** pins 32-bit too: `-m32` for compile and link, `-march=pentium3`,
  `-ffast-math`, `-fno-strict-aliasing`, `-fpermissive`
  (`neo/SConstruct:296-297,316`).
- **Inline assembly is pervasive and x86-only** — 1,565 `__asm` occurrences across 14 of the 1,400 engine
  files, concentrated in:
  `neo/idlib/math/Simd_{3DNow,MMX,SSE,SSE2,SSE3}.cpp`, `neo/idlib/math/Math.h`,
  `neo/idlib/Lib.cpp`, `neo/sys/win32/win_cpu.cpp` (CPUID / 3DNow! / SSE detection),
  `neo/sys/win32/win_main.cpp`, `neo/sys/win32/win_shared.cpp`, `neo/sys/linux/main.cpp`,
  `neo/sys/osx/macosx_utils.mm`.
- `alloca` is used 1,498 times (of which `_alloca` 457) — stack-allocation patterns combined with
  the 16 MB stack reserve.
- SIMD dispatch files: `idlib/math/Simd.cpp` + `Simd_Generic.cpp` are the ones the SCons build
  compiles (`sys/scons/SConscript.idlib`), while `idlib.vcxproj` also lists
  `Simd_3DNow/MMX/SSE/SSE2/SSE3/AltiVec` as separate translation units.
- Mac path is legacy: `MACOS_X` 86 hits, `__APPLE__` only 2 — the OS X code uses the custom
  macro, not the compiler's. `Simd_AltiVec.cpp` targets PowerPC and is dead on modern Macs.

---

## 4. 64-bit portability hazards

| # | Hazard | Evidence | Impact |
|---|---|---|---|
| 1 | Inline `__asm` in MSVC/GCC x86 syntax; no x64 assembly path | 1,565 `__asm`; `neo/sys/win32/win_cpu.cpp:60-185`; `neo/idlib/math/Simd_SSE*.cpp` | MSVC x64 rejects `__asm` outright; every SIMD file needs an intrinsics rewrite or exclusion |
| 2 | Pointer ↔ `int` round-trips | 10 × `reinterpret_cast<int>`, 3 × `(int)…Ptr`, 1 × `(int)this` (engine files) | Truncation on x64 |
| 3 | `sizeof(void*)`-dependent code | 14 hits | Any struct that stores a pointer + `int` changes size |
| 4 | 32-bit `time_t` baked into the ABI | `_USE_32BIT_TIME_T` in `neo/_Common.props:11` | 2038 ceiling; removing it changes time-struct layout |
| 5 | Prebuilt 32-bit import libraries | `neo/openal/lib/openal32.lib`, `eaxguid.lib`; DirectX `Lib\x86` | Cannot link x64 without new libs |
| 6 | Hard-coded x86 artifact names | `gamex86.dll` (`_Game.props:15`), `game<cpu>-base.so` | Install scripts/paths assume x86 |
| 7 | Savegame/demo/network serialization | `neo/game/gamesys/SaveGame.cpp:44-46` saves an **object index**, not a raw pointer, then `WriteInt` | This is the *good* case: no raw pointer is persisted, so save format is more portable than it looks |
| 8 | `long`-width and `%d`-on-`size_t` assumptions | widespread `sprintf` (395) — not audited line-by-line | Format-string UB on LP64 |
| 9 | Big/little-endian code paths | `Simd_AltiVec`, `idlib/Base64.cpp` endian fixes | Irrelevant for x86-64/wasm; blocks any BE target |

Mitigating: the renderer already loads GL entry points dynamically (`neo/renderer/qgl.h`,
`qgl_linked.h`) and `GL_HARDLINK` is a build option (`SConstruct:323-324`), so swapping the
platform GL layer is feasible.

---

## 5. Modern-toolchain build plan

Goal: one CMake build that produces a portable client + game module, then an Emscripten target.

1. **CMake 3.20+ superbuild** replacing `doom.sln` + `SConstruct`. Targets mirror the table in §1:
   `idlib` (static), `typeinfo` (host tool), `game` / `game_d3xp` (shared modules), `doom3`
   (executable). Keep per-target `__DOOM__`, `GAME_DLL`, `_D3XP`, `ID_ENABLE_CURL`.
2. **Drop the tools from the default build.** They require MFC (`nafxcw.lib`) and have no project
   files. Build only the headless compilers that matter: `dmap`, `aas`, `roqvq`, `renderbump`
   (`neo/tools/compilers/**`, already compiled into the core in `SConscript.core`).
3. **Compilers**: MSVC 2022 (`/std:c++14`-ish, C++98-era code) or clang-cl; provide an
   `-DID_DISABLE_SIMD` path that selects `Simd_Generic.cpp` only, then reintroduce
   SSE/AVX via intrinsics. `__asm` blocks are guarded per variant already, so exclusion is
   mechanical.
4. **x64**: delete `_USE_32BIT_TIME_T`, replace `alloca` hot spots, fix the pointer/`int` casts
   listed in §4, rebuild OpenAL against OpenAL-Soft.
5. **Platform layer**: SDL2 for window/input/GL context (`neo/sys/win32/*`, `neo/sys/linux/*`,
   `neo/sys/posix/*` collapse into one backend); OpenAL-Soft replaces the fixed-function
   `openal32.lib` ABI; GL 1.4 + ARB2 path is already the primary renderer
   (`neo/renderer/draw_arb2.cpp`), so a GL 3.3 core or WebGL2 backend is a shader-path port,
   not a renderer rewrite.
6. **Web**: Emscripten + WebGL2, `-pthread` for the async network thread, and a virtual FS that
   mounts user-supplied pk4s (assets never in git).
7. **CI evidence**: build exit code + a headless smoke run (`+set com_skipIntro 1 +quit`,
   `+map` on a test map) because no assets are in the repo.

---

## 6. Game data: what is and is not in the repository

- `doom3_orig/doom3/base/` contains **exactly one file**: `default.cfg`, 14 bytes, content `# empty file`.
- Repo-wide count of `*.pk4` / `*.pk3` / `*.pak` files: **0**.
- `pak0.pk4 … pak4.pk4` are **not** in the repo and must never be committed. They come from a
  retail/Steam install; `README.txt` states the source release contains no game data and that
  the data remains under the original EULA.
- Archive semantics: `.pk4` is a plain ZIP renamed to discourage unzipping; search order is
  descending (`pak4` … `pak0`) so a patch pk4 overrides earlier ones; demo data may live under
  `demo/pak0.pk4` (`neo/framework/FileSystem.cpp:97-111`). Reading is implemented by
  `neo/framework/Unzip.cpp` (zlib + minizip).

### 6.1 File formats

| Format | Kind | Produced by | Consumed by |
|---|---|---|---|
| `.pk4` | ZIP archive | packaging (`sys/scons/Setup.py`, `sys/linux/pk4/`) | `framework/FileSystem.cpp`, `framework/Unzip.cpp` |
| `.map` | idTech4 text map (brush/entity/patch) | Radiant (`neo/tools/radiant/`) | `idlib/MapFile.cpp`, `tools/compilers/dmap/map.cpp` |
| `.proc` | compiled BSP/portalized world | `dmap` (`tools/compilers/dmap/output.cpp`) | `renderer/RenderWorld_load.cpp`, `cm/CollisionModel_load.cpp:3299` |
| `.cm` | collision model, binary, file id `CM`, version `1.00` | `dmap` | `cm/CollisionModel_files.cpp:42-44`, `CollisionModel_load.cpp:3516` |
| `.aas` | AI area navigation, file id `DewmAAS`, version `1.07` | `tools/compilers/aas/` | `game/ai/AAS*.cpp`, loaded via `game/Game_local.cpp` / `framework/Common.cpp` |
| `.md5mesh` / `.md5anim` | skeletal mesh / animation (text) | Maya exporter (`neo/MayaImport/`), third-party exporters | `renderer/Model_md5.cpp`, `game/anim/Anim_Import.cpp` |
| `.md3`, `.ase`, `.lwo`, `.ma` | legacy mesh formats | 3ds Max / LightWave / Maya | `Model_md3.cpp`, `Model_ase.cpp`, `Model_lwo.cpp`, `Model_ma.cpp` |
| `.mtr` | material decl | text (`materials/`) | `framework/DeclManager.cpp:821` |
| `.skin` | skin decl | text (`skins/`) | `DeclManager.cpp:822` |
| `.sndshd` | sound shader decl | text (`sound/`) | `DeclManager.cpp:823`, `sound/snd_shader.cpp` |
| `.def` | entity definition (`entityDef`) | text | `DECL_ENTITYDEF` (`DeclManager.cpp:811`), `DeclEntityDef.cpp` |
| `.fx` | FX system decl | text | `DECL_FX` (`DeclManager.cpp:813`), `DeclFX.cpp` |
| `.particle` | particle system decl | Particle Editor | `DECL_PARTICLE` (`DeclManager.cpp:814`) |
| `.af` | articulated figure / ragdoll decl | AF editor | `DECL_AF` (`DeclManager.cpp:815`), `DeclAF.cpp` |
| `.pda` | PDA/email/audio/video decls | PDA editor | `DECL_PDA/EMAIL/AUDIO/VIDEO` (`DeclManager.cpp:816-819`) |
| `.table` | lookup table decl | text | `DECL_TABLE` (`DeclManager.cpp:806`) |
| `.script` | idScript (C-like) | Script editor | `game/script/Script_Compiler.cpp`, `Script_Interpreter.cpp` |
| `.gui` | GUI layout/script | GUIEd | `ui/GuiScript.cpp`, `ui/UserInterface.cpp` |
| `.lang` | localized string table | text | `idlib/LangDict.cpp`, `framework/Common.cpp` |
| `.roq` | RoQ video | `tools/compilers/roqvq` | `renderer/Cinematic.cpp` |
| `.tga` / `.dds` | textures | image tools | `renderer/Image_load.cpp` |
| `.wav` / `.ogg` | audio | — | `sound/snd_wavefile.cpp`, `snd_decoder.cpp` |

Only three folder→extension mappings are hard-coded in the engine (`materials`→`.mtr`,
`skins`→`.skin`, `sound`→`.sndshd`); all other decl types are resolved by type name plus
path/name convention (`neo/framework/DeclManager.cpp:806-823`). Any asset pipeline must encode
those conventions explicitly.

---

## 7. License and distribution constraints

- **Engine: GPLv3.** `doom3_orig/doom3/COPYING.txt` is GPLv3 (29 June 2007); every `neo/**` file carries the
  GPLv3 header plus the id additional-terms paragraph (e.g. `neo/TypeInfo/main.cpp:4-26`).
- **Additional terms** exist but their text is *not* in this repo — `README.txt` says they
  accompanied the release and must be requested from id/ZeniMax. Treat that as an open legal
  item before redistribution.
- **Excluded code** (own licences, see §2): curl, JPEG, Ogg Vorbis, OpenAL SDK, PropTree, Base64,
  zlib/minizip, MD4/MD5/CRC32, GL headers.
- **Missing feature, not missing licence:** stencil-shadow "depth fail" (Carmack's Reverse) is
  deliberately absent (`README.txt`, "Back End Rendering of Stencil Shadows").
- **Game assets are proprietary.** No data in the repo; retail/Steam assets are governed by the
  original EULA. A web port may ship source + engine binaries (GPLv3: complete corresponding
  source, including the wasm build scripts) but **must not** bundle `pak*.pk4`, and must require
  the user to supply their own data.
- GPLv3 + web distribution: the Emscripten output is a covered work, so the exact build
  toolchain, CMake files and any engine patches must be published.

---

## 8. AI-native migration roadmap

Effort is engineer-days for one experienced engineer; risk is technical uncertainty, not
schedule risk.

| Phase | Scope | Effort | Risk | Exit evidence |
|---|---|---|---|---|
| P0 — Freeze & instrument | Record baseline: file inventory, macro/asm counts, target graph, licence manifest. No code changes. | 0.5 | Low | This document + reproducible count script |
| P1 — CMake skeleton (x86, clang/MSVC) | Port `idlib` + `typeinfo` to CMake; keep `__DOOM__`/`GAME_DLL`; exclude `Simd_*` asm via generic path | 3-5 | Medium | `idlib.lib` + `TypeInfo.exe` build; `TypeInfo` output diffed against committed `game/gamesys/TypeInfo.cpp` |
| P2 — Engine + game modules on SDL2/OpenAL-Soft | Replace win32/posix sys layer; link `doom3` + `game`/`game_d3xp`; keep GL 1.4/ARB2 | 5-10 | Medium-High | Client boots to console with user-supplied pk4; `+map` smoke test |
| P3 — x64 | Drop `_USE_32BIT_TIME_T`, rewrite asm SIMD as intrinsics or generic, fix pointer→int, new OpenAL x64 | 10-20 | High | 64-bit binary runs the same smoke test; no truncation warnings |
| P4 — WebAssembly / WebGL2 | Emscripten, `-pthread`, virtual FS mounting user pk4, shader-path port | 10-20 | High | Browser loads pk4 from local file, renders a map, input works |
| P5 — Headless asset toolchain | `dmap`/`aas`/`roqvq`/`renderbump` as CLI targets; **drop** MFC editors (Radiant, MaterialEditor, GUIEd, AF/PDA/Sound/Script/debugger) or replace with web/AI tooling | 8-15 | Medium | `dmap` compiles a `.map` → `.proc`/`.cm`/`.aas` in CI on all 3 OSes |
| P6 — AI-native loop | Spec-driven tasks; validators for `.def/.mtr/.md5mesh/.gui`; automated build→run→screenshot evidence; asset-licence gate that fails on any `*.pk4` in git | continuous | Low-Medium | Every PR carries machine-checked evidence; licence gate green |

Sequencing note: P3 (x64) is *not* a prerequisite for P4 (wasm32 is 32-bit) — the web track can
run in parallel with the x64 track, and P5 can start as soon as P1 lands.

---

## 9. Evidence

Macro/asm census over engine sources, excluding vendored `neo/curl`, `neo/renderer/jpeg-6`,
`neo/sound/OggVorbis` (1,400 files, `*.c/*.cpp/*.h/*.hpp/*.inl`):

```
__asm      1565      asm {      196
_WIN32      111      _MSC_VER    95
MACOS_X      86      __declspec  50
__i386__     23      __linux__   20
__GNUC__      8      __APPLE__    2
alloca     1498 (_alloca 457)
_3DNow       41      SSE-token  4244 (mostly idSIMD_SSE* names/comments)
```

Other measured facts: engine source files 1,400; `base/` file count 1; `*.pk4|*.pk3|*.pak`
count 0; solution configurations 7, all `Win32`; `.sln` files in repo 1 (`neo/doom.sln`);
`<PlatformToolset>` elements 0; tool `.vcxproj` files 0.
