# 08 — The retail expansion: *Doom 3: Resurrection of Evil*

Read-only analysis. No file under `doom3_orig/doom3/` or
`.kilo/worktrees/feat-1-win11-amd64-native-build/targets/win11-amd64/` was modified; this
document is the only file written.

Paths are relative to `doom3_orig/doom3/` unless stated otherwise. Port evidence is from the
read-only worktree `targets/win11-amd64` in
`.kilo/worktrees/feat-1-win11-amd64-native-build/`.

Confidence labels used below:

* **[V]** verified against this repository's source (`file:line`) or a cited public page.
* **[J]** judgement/estimate, not measurable here.
* **[U]** unverified — stated as a possibility, not a fact.

---

## 0. Short answer: is it "like DLC"?

**No.** *Resurrection of Evil* (RoE) is a **2005 retail expansion pack**, sold as its own
boxed/retail product that **requires an installed copy of DOOM 3** to install and run; it is a
full second campaign (12 single-player levels), not a small add-on item. DLC as a distribution
model barely existed on PC in April 2005. **[V]** (release, dependency)

The only "DLC-shaped" phase was **digital distribution**: Valve listed RoE as a separate Steam
app (9070) that could not be bought or played without base DOOM 3 (app 9050). That is an
add-on *purchase*, but the content is still a full expansion. Since **10 August 2022** the
separate listing was retired and RoE ships inside the consolidated DOOM 3 package. **[V]**

`The Lost Mission`, often called "the BFG DLC", is **not DLC either**: it is 8 levels built into
*DOOM 3: BFG Edition* (2012) and was never sold separately. **[V]**

So the accurate description is:

| Thing | What it is | Sold separately? |
|---|---|---|
| RoE (2005) | Full expansion pack, own retail SKU, requires DOOM 3 | Yes, at retail; later as Steam add-on 9070 |
| Lost Mission (2012) | 8-level mini-campaign inside BFG Edition | No — bundled with BFG only |
| RoE in BFG (2012) | Remastered RoE inside a different engine build | No — part of BFG |
| RoE today (Steam/GOG) | Original RoE bundled with DOOM 3 (and BFG) | No — part of the DOOM 3 package |

---

## 1. The product: *Doom 3: Resurrection of Evil* (2005)

### 1.1 Facts

| Item | Value |
|---|---|
| Developer / publisher | Nerve Software / Activision |
| Windows release | 4 April 2005; Linux 24 May 2005 |
| Xbox release | 5 October 2005 — **standalone**, does not require DOOM 3, and includes Ultimate Doom, Doom II and Master Levels |
| PC dependency | Requires DOOM 3 to install |
| Campaign | 12 new single-player levels, set two years after DOOM 3 |
| New enemies | Six, including the three Hunters (bosses); press/manual name Vulgars, Bruisers, The Forgotten and the Hunters |
| New weapons/items | Double-barrelled shotgun, the **Grabber** (physics tool), the **Artifact** (three demonic powers) |
| Multiplayer | Up to 8 players; DM, Team DM, Last Man Standing, Tournament and **CTF**; new MP maps; CTF developed by ThreeWave |
| Other | Haste and Invulnerability power-ups; environment suits with mounted flashlights |

**[V]** for release/dependency/campaign/enemies/CTF: Wikipedia and the retail manual text
(archive.org). **[V]** for the manual's five listed MP modes; Wikipedia's "eight multiplayer game
modes" is a count we did not reproduce. **[V]** for ThreeWave's involvement: GameSpot review.

### 1.2 The Artifact is "bloodstone" in the code

The Artifact is not a hard-coded C++ class; it is implemented as inventory weapons named
`weapon_bloodstone_passive`, `weapon_bloodstone_active1..3` plus `ammo_bloodstone`:

* `neo/d3xp/Player.cpp:1489-1492` — weapon slots looked up by those names.
* `neo/d3xp/Player.cpp:1954-1961` — given to the player as the artifact powers unlock.
* `neo/d3xp/Game_local.cpp:101-104` — the four bloodstone weapon names in the "keep on fast
  timeline" list, i.e. the artifact's slow-time power must not slow itself down.

This is why `grep Artifact` over `neo/d3xp` returns **nothing** while
`grep weapon_bloodstone` returns 53 hits. Anyone looking for "artifact" in the game code is
looking for the wrong string.

### 1.3 How it ships, over time

1. **2005, retail:** a physical expansion requiring DOOM 3. The PC version installs content into
   the DOOM 3 tree; the engine's RoE asset folder is `d3xp/` (the retail RoE installs
   `d3xp/pak000.pk4`). **[V]** (engine check at `neo/framework/FileSystem.cpp:4021`; modwiki
   folder-structure page states RoE's default base folder is `/d3xp`)
2. **2007, Steam:** app **9070** "DOOM 3: Resurrection of Evil", `MustOwnAppToPurchase` = DOOM 3
   (9050), released 3 August 2007, later retired. **[V]** (SteamDB app 9070)
3. **2012, BFG Edition:** RoE is remastered and included in *DOOM 3: BFG Edition* (see §2). **[V]**
4. **2022, consolidation:** Steam merged DOOM 3, RoE and BFG into one package; the store page
   now says owners receive "The original DOOM 3 and Resurrection of Evil Expansion" and "The
   remastered DOOM 3: BFG Edition". **[V]**
5. **GOG:** the GOG "DOOM 3" product likewise contains original DOOM 3 + original RoE + BFG,
   DRM-free. **[V]**

Consequence for this project: **there is no legal way to buy RoE alone today**; the purchase path
is the DOOM 3 bundle on Steam/GOG (or a second-hand retail disc). RoE data is still under the
original EULA and is not covered by GPLv3 — same rule as the rest of the game data.

---

## 2. *DOOM 3: BFG Edition* (2012) and *The Lost Mission*

BFG Edition is **not** "DOOM 3 + DLC on the same engine". It is a different engine build
("id Tech 4 BFG", id Tech 5 ideas folded in) and a different source release:

| Aspect | This repository (2011 source release) | BFG Edition (2012 source release) |
|---|---|---|
| Source | `doom3_orig/doom3` (fork of `id-Software/DOOM-3`) | `id-Software/DOOM-3-BFG` (separate GitHub repo) |
| Game code | Two modules: `neo/game` (base), `neo/d3xp` (RoE), built as `gamex86.dll` | `neo/d3xp` retained, but the game builds as a **static library** linked into one executable |
| Engine | Classic id Tech 4; metrics/`RenderWorld` as in 2004–2006 | Threading/jobs, resource system, different renderer paths (BFG does not ship stencil-shadow depth-fail), Flash-based UI, embedded Doom Classic |
| Content | Base game + RoE (if installed) | Base game + remastered RoE + **The Lost Mission** (8 levels) + Doom + Doom II |
| Gameplay defaults | 2004/2005 behaviour | Checkpoint saves, armour-mounted flashlight, 3D support |

**[V]** for source separation and the BFG feature list: GitHub repo, Fabien Sanglard's source
review, Wikipedia/Steam. **[V]** for BFG's static game library: Sanglard's review (the two game
projects become one `Game-d3xp.lib`).

*The Lost Mission*: 8 single-player levels, built from content cut from DOOM 3, included only in
BFG (and later BFG-derived console re-releases). It was **never sold as separate DLC**. **[V]**

Practical note for us: BFG data is not a drop-in replacement for the 2011 engine, and BFG's
engine needs its own data layout (`.resources`). We did not test BFG data against our build;
treat cross-loading as unsupported. **[U]**

---

## 3. How the 2011 engine supports RoE

The important structural fact: **the engine is the same; the game code is a second module.** The
2011 source release corresponds to the patched 1.3-era codebase that knows about RoE:

* `neo/framework/FileSystem.cpp` contains the `d3xp` detection and `fs_game` logic.
* `neo/framework/Session.cpp` contains the RoE (`xpkey`) CD-key code.
* `neo/framework/Session_menu.cpp` exposes the "which game" menu entry.
* `neo/renderer/RenderWorld.h:305` already has the extra `startTime` parameter that RoE uses for
  slow-time decals (the base game passes `time` too — `neo/game/Game_local.cpp:3821`).
* `neo/d3xp/` is a **full copy** of `neo/game/` with RoE changes, plus 4 new files.

### 3.1 How `d3xp` is chosen

1. `fs_game` is a system cvar: `neo/framework/FileSystem.cpp:490` —
   `idCVar fs_game("fs_game", "", CVAR_SYSTEM | CVAR_INIT | ...)`.
   It is read from the command line/startup variables at
   `neo/framework/FileSystem.cpp:2848` and must be set at init time (it is `CVAR_INIT`).
2. The cvar's own documentation says what it is for: `FileSystem.cpp:91` —
   "The game directory is set with `+set fs_game myaddon`". `fs_game_base` exists to base a mod
   on D3 + D3XP assets: `FileSystem.cpp:163-165` ("fs_game mymod, fs_game_base d3xp").
3. Search order is built in `SetupGameDirectories` (`FileSystem.cpp:2250-2260`): `fs_game_base`
   first, then `fs_game`, so with `fs_game=d3xp` the engine loads `d3xp/` before `base/`.
4. `RunningD3XP()` is literally "is `fs_game` or `fs_game_base` equal to `d3xp`":
   `FileSystem.cpp:4056-4064`.
5. The mod list hides the `d3xp` entry unless the pak is present:
   `FileSystem.cpp:1745-1747` ("D3 1.3 #31, only list d3xp if the pak is present").
6. `ID_ALLOW_D3XP` gates the whole feature: `neo/framework/BuildDefines.h:128-134` sets it to 1
   on non-Mac builds; `FileSystem.cpp:2854-2860` *removes* `fs_game=d3xp` when it is 0. On
   Windows it is 1, so the expansion is allowed.

### 3.2 Detecting that RoE is installed

`HasD3XP()` (`FileSystem.cpp:3985-4049`) is not a registry lookup; it probes the filesystem:

* On Windows (`#elif ID_ALLOW_D3XP`, `4010-4027`): it opens
  **`d3xp/pak000.pk4`** in each of `fs_savepath`, `fs_devpath`, `fs_basepath`, `fs_cdpath`
  (`4014-4021`). If opened → installed (`4024`).
* Fallback: `d3xp/default.cfg` under `fs_savepath` (`4030-4046`) for unpacked dev installs.
* Result cached in `d3xp` (`-1` unknown/absent, `1` present): `FileSystem.cpp:446`, `514`, `3990-3994`.

So "is RoE installed" is exactly "does `d3xp/pak000.pk4` exist in a search path". This is the
check our port would inherit for free.

### 3.3 The menu gates on it

* `neo/framework/Session_menu.cpp:74-78` — if `HasD3XP()`, set `game_list` string `#str_07202`
  (the multi-game menu label), otherwise `#str_07212`.
* `Session_menu.cpp:324` and `1321` — expose GUI state `visible_hasxp` = `1`/`0`.

This is the source-level evidence that retail RoE is selected inside the same engine/menu ("game
list"), rather than being a different game engine.

### 3.4 The second CD key: `xpkey`

RoE shipped with its **own** CD key. The engine reads and validates two keys when RoE is present:

* File names: `neo/framework/Licensee.h:110-111` — `CDKEY_FILE "doomkey"`, `XPKEY_FILE "xpkey"`.
* Read at startup: `neo/framework/Session.cpp:2985-3016` — reads `../base/doomkey`, then, for
  `xpkey`, `../base/xpkey` (`3004-3016`). Write path: `3042-3048`.
* One key-entry string carries both halves; only the second half is used when RoE is installed:
  `Session.cpp:3123-3149` (`imax = HasD3XP() ? 2 : 1`), `3183-3188` sets `xpkey`/`xpkey_state`
  or `CDKEY_NA`.
* Validation requires a valid `xpkey` only when `HasD3XP()`:
  `Session.cpp:3225-3241`, and the final verdict `3250-3252`.
* `Common.cpp:2836-2843` and `Session.cpp:1155` guard key enforcement with `ID_ENFORCE_KEY`.

For our port this is moot but worth knowing: the port defines `ID_ENFORCE_KEY=0`
(`targets/win11-amd64/CMakeLists.txt:63`), so the RoE key prompt/validation path is compiled out,
and single-player does not need `base/xpkey`. The auth servers behind the online check are gone
anyway.

### 3.5 What exists only in `d3xp` (code evidence)

Directory comparison of `neo/game` (136 files) vs `neo/d3xp` (140 files), by relative path and
SHA-256:

| Result | Count |
|---|---:|
| Files in both, byte-identical | 72 |
| Files in both, different | 64 |
| Only in `neo/game` | 0 |
| Only in `neo/d3xp` | 4 |

The four d3xp-only files:

* `neo/d3xp/Grabber.cpp`, `neo/d3xp/Grabber.h` — `class idGrabber : public idEntity` at
  `Grabber.h:41`; 30 references to `idGrabber` across the module.
* `neo/d3xp/physics/Force_Grab.cpp`, `neo/d3xp/physics/Force_Grab.h` — the grab force applied to
  held objects.

Other RoE-only code is inside the 64 modified shared files, behind `_D3XP` / `CTF`:

| Feature | Evidence |
|---|---|
| Grabber weapon glue (`weapon_grabber`) | 5 references in `neo/d3xp` |
| Artifact ("bloodstone") | `Player.cpp:1489-1492`, `1954-1961`; 53 `weapon_bloodstone`/`ammo_bloodstone` hits |
| Slow-time world (`slow.time`, fast-timeline entity list) | `Game_local.h:244-258` (`timeState_t`, `slowmoState_t`); `Game_local.cpp:72-104` (`fastEntityList`) |
| CTF | 380 `CTF` occurrences in `neo/d3xp`; `GAME_CTF` added to the enum at `MultiplayerGame.h:48-55`; selected from `si_gameType == "CTF"` at `Game_local.cpp:4686-4690`; CTF player class `player_doommarine_ctf` at `Game_local.cpp:2016-2020`; flag-based scoring at `MultiplayerGame.cpp:52-...` |
| Other RoE-only gameplay | `Game_local.cpp:1167`, `4400-4608` (CTF spawn/score paths), `Game_local.cpp:342-356` (`_D3XP` player/medkit changes) |

Note: **`GAME_VERSION` is `"baseDOOM-1"` in both modules** (`neo/game/Game_local.h:62` and
`neo/d3xp/Game_local.h:62`). There is no "d3xp" game-name constant; `d3xp` is a directory and an
`fs_game` value, not a build-time identity string. The identity is the *module* (`neo/d3xp`) plus
the `_D3XP`/`CTF` defines.

### 3.6 The retail runtime shape: one EXE, two `gamex86.dll`

* The base game project builds `gamex86.dll` (`neo/_Game.props:9,15`: `GAME_DLL`, output
  `gamex86.dll`, `game/game.def`).
* The expansion project builds **also `gamex86.dll`** (`neo/_Game-d3xp.props:9,13-14`:
  `GAME_DLL;_D3XP;CTF`, output `gamex86.dll`, module-def `d3xp/game.def`).
* Both files have the same name because the name is computed: `neo/sys/sys_local.cpp:123-133`
  formats `"%s" CPUSTRING ".dll"` from the base name `"game"` → `gamex86.dll`.
* `idCommonLocal::LoadGameDLL` asks the filesystem for `"game"`
  (`neo/framework/Common.cpp:2637`) and `FindDLL` searches the **current** search paths
  (`FileSystem.cpp:3864-3896`), which start with `fs_game` (`SetupGameDirectories`,
  `FileSystem.cpp:2250-2260`). With `fs_game=d3xp` it finds `d3xp/gamex86.dll`; with no
  `fs_game` it finds `base/gamex86.dll`.
* The DLL exports exactly `GetGameAPI` (`neo/d3xp/Game.def`: `EXPORTS GetGameAPI`).

So retail "switches games" by **switching which directory is searched first**, not by changing
executables. That is the design our static-link port gave up (see §4.2).

---

## 4. Our Windows x64 port today (`targets/win11-amd64`)

### 4.1 What it builds

`targets/win11-amd64/CMakeLists.txt:41-46`:

```cmake
add_executable(DOOM3
    ${IDLIB_SOURCES}
    ${ENGINE_SOURCES}
    ${GAME_SOURCES}
    src/eaxguid_x64.cpp
)
```

* `sources.cmake` has three lists: `IDLIB_SOURCES` (`5-56`), `ENGINE_SOURCES` (`58-292`),
  `GAME_SOURCES` (`294-363`). `GAME_SOURCES` has **69** translation units, i.e. every
  `game/*.cpp` that `game.vcxproj` compiles (`neo/game` has 71 `.cpp`; `EndLevel.cpp` and
  `gamesys/Callbacks.cpp` are in neither project's compile list).
* The target defines neither `GAME_DLL` nor `__DOOM_DLL__`
  (`CMakeLists.txt:55-66`), so `LoadGameDLL`'s DLL body is compiled out
  (`Common.cpp:2629-2691`), and the game is reached through the compiled-in globals
  `idGameLocal gameLocal; idGame *game = &gameLocal;` (`neo/game/Game_local.cpp:64-65`).
  This matches the README statement `targets/win11-amd64/README.md:47-48` ("statically linked
  (no `gamex86.dll` lookup)").
* There is **no** `neo/d3xp` file in the build, and no `DOOM3XP` target.

### 4.2 Why a runtime switch is not available in this build

`neo/d3xp/Game_local.cpp:64-65` declares the **same globals** as the base module:

```cpp
idGameLocal gameLocal;
idGame *game = &gameLocal;
```

If both modules were linked into one executable, the linker would see duplicate `game`,
`gameLocal`, `gameAnimationLib`/`animationLib`, `gameEdit`, `gameSoundWorld` and every
same-named class (`idPlayer`, `idWeapon`, `idMoveable`, …) with **different layouts** — a
guaranteed ODR violation, not a warning. The retail engine avoided this by loading exactly one
module at a time as a DLL. Our port cannot, because it deliberately removed the DLL boundary.

Therefore the realistic choices are:

* **A. Two executables** — `DOOM3.exe` (base) and `DOOM3XP.exe` (engine + `neo/d3xp` game code).
  Same engine TUs compiled twice; selection is by which EXE you run (plus `fs_game=d3xp`).
* **B. Restore the DLL model** — rebuild the engine with `__DOOM_DLL__`, build two
  `gamex86.dll`s (`GAME_DLL`, one with `_D3XP;CTF`), and keep one EXE. This recovers retail
  behaviour (menu switch) but re-introduces the x64 DLL/CRT/heap boundary the port was built to
  avoid.
* C. Namespace-rename the d3xp module and link both statically with a runtime function-pointer
  switch. This is the most invasive option and we do not recommend it.

### 4.3 Option A, concretely (recommended)

1. **Source list.** Generate a fourth list from `neo/game-d3xp.vcxproj` instead of globbing.
   That project lists 72 `ClCompile` entries = **71 d3xp TUs** + `idlib/precompiled.cpp`
   (already in `IDLIB_SOURCES`; drop it). The 71 = the 69 base TUs + `Grabber.cpp` +
   `physics/Force_Grab.cpp`. `scripts/gen-sources-win11-amd64.ps1` already parses arbitrary
   vcxproj files (`Get-ProjectSources`), so this is a few lines, not a new tool.
2. **Include order.** The d3xp TUs include `"Game_local.h"`, `"Player.h"`, etc. and must resolve
   them to **`neo/d3xp/...`**, not `neo/game/...`. Do this per-target, not globally:
   compile the d3xp game sources as a CMake `OBJECT` library with
   `target_include_directories(d3xp_game PRIVATE "${NEO_DIR}/d3xp" "${NEO_DIR}" ...)` and then
   `add_executable(DOOM3XP ${IDLIB_SOURCES} ${ENGINE_SOURCES} $<TARGET_OBJECTS:d3xp_game> src/eaxguid_x64.cpp)`.
   Engine TUs then keep `NEO_DIR` first and there is no header shadowing risk. (No engine `.cpp`
   includes a game header — verified by grep for `#include "Game...` in `framework/`, `renderer/`
   and `ui/`: the only hits are the unrelated `ui/Game*Window.*` classes.)
3. **Defines.** For the d3xp objects only: `_D3XP` and `CTF` (exactly what
   `neo/_Game-d3xp.props:9` adds on top of `GAME_DLL`). Do **not** define `GAME_DLL` or
   `__DOOM_DLL__`: we want the static globals (`Game_local.cpp:64-65`) and no DLL load.
   Keep the port-wide defines (`__DOOM__`, `WIN32`, `ID_ENFORCE_KEY=0`, …).
4. **Output name / launch.** `OUTPUT_NAME "DOOM3XP"`. The engine must start with
   `fs_game=d3xp`; `fs_game` is `CVAR_INIT`, so it has to be set before init from the command
   line (`+set fs_game d3xp`) or by a small startup default in the port (inject the pair into
   the command line before the filesystem initialises, or ship a launcher). A shortcut with
   `+set fs_game d3xp` is enough and keeps the engine unpatched.
5. **What you get.** With `d3xp/pak000.pk4` present, `HasD3XP()` is true, `fs_game=d3xp` makes
   `d3xp/` the first search path, `RunningD3XP()` is true, and the `DOOM3XP.exe` game module is
   the RoE one. The base `DOOM3.exe` is unchanged.
6. **Compile risk.** The shared 64 files already carry the port's x64 edits: the patch manifest
   lists `neo/d3xp/Game_local.cpp`, `gamesys/Event.{h,cpp}`, `gamesys/Class.cpp`,
   `script/*.cpp`, etc., and e.g. `neo/d3xp/gamesys/Event.h` in the target copy uses
   `intptr_t` for event args (`E_EVENT_SIZEOF_VEC`, `CopyArgs`, x64 comment). The two d3xp-only
   files (`Grabber.cpp`, `Force_Grab.cpp`) are **not** in the manifest — they have never been
   compiled on x64 — but a quick read shows no obvious pointer→int truncation. Expect a small
   number of compile fixes, not a rewrite. **[J]**
7. **No engine changes expected for gameplay features.** Slow-time decals use the
   `ProjectDecalOntoWorld(..., startTime)` parameter that the engine already has
   (`renderer/RenderWorld.h:305`); CTF is game-side; the RoE key is handled by `Session.cpp`.
   Engine work is limited to build plumbing. **[V]** for the API; **[J]** for "nothing else
   appears".

### 4.4 Effort estimate

Judgement, assuming the existing x64 baseline keeps working and no RoE data is available:

| Work | Estimate |
|---|---|
| Generator + `d3xp_sources.cmake` + CMake object-library target | 2–4 h |
| First full compile of 71 d3xp TUs; fix x64 issues in `Grabber.cpp`/`Force_Grab.cpp` and any `_D3XP`-only path | 0.5–2 days |
| Bringing up `DOOM3XP.exe` against real RoE data (menu, first map, save/load) | 0.5–2 days, **blocked on data** |
| CTF/multiplayer verification | days, and effectively untestable (servers gone; port networking unexercised) |

So: **build plumbing is small (hours); a trustworthy RoE port is not verifiable by us today.**
That is the honest headline.

### 4.5 Data requirements — and what we cannot test

To run RoE you need **both**:

1. **Base DOOM 3 retail data** — `base/pak000.pk4` … `pak004.pk4` (the expansion reuses base
   assets; `fs_game=d3xp` only *prepends* `d3xp/` to the search path,
   `FileSystem.cpp:2250-2260`).
2. **RoE data** — at minimum `d3xp/pak000.pk4` (that exact file is what `HasD3XP()` probes,
   `FileSystem.cpp:4021`). A fully patched retail RoE install may contain additional paks; only
   `pak000.pk4` is load-bearing for detection.

We have neither:

* The worktree contains **no `.pk4` at all** (verified by recursive `-Filter *.pk4` over
  `targets/win11-amd64`); the demo runs used data outside the worktree.
* The only data the port is allowed to use in tests is the **2004 DOOM 3 demo**
  (`targets/win11-amd64/README.md`, "Game data is never committed"), which predates RoE
  (demo 2004, RoE April 2005) and contains 3 Mars City levels — no RoE assets by construction.
* The demo also cannot substitute for retail base paks: it is a cut-down subset, and RoE expects
  the full base assets.

Therefore: **we cannot test d3xp without buying the game.** That is a hard blocker, not a
tooling problem. Buying DOOM 3 on Steam/GOG today includes RoE; do not obtain data any other
way. **[V]** for the data facts; the purchase route is the legal one.

What we *can* do without RoE data:

* Build and link `DOOM3XP.exe` and check for x64 compile/link errors — this is the bulk of the
  engineering risk and needs no assets. **[V]** (feasible)
* Verify `HasD3XP()` returns false and that `+set fs_game d3xp` is accepted but finds no pak
  (expected: no expansion menu entry, missing-asset errors if forced). **[J]**
* Build a synthetic 1-map test mod in `d3xp/` with a hand-made `default.cfg`? The engine accepts
  `d3xp/default.cfg` (`FileSystem.cpp:4037`), but the RoE game module still needs RoE defs,
  models, weapons and monsters; a synthetic pak cannot stand in for the campaign. Useful only
  for "does the module load" smoke tests, not gameplay. **[J]**

### 4.6 Is there any free d3xp content?

Checked and found none:

* The free **DOOM 3 demo** is the 2004 base-game demo (3 levels) — no RoE. **[V]**
* The free **GPL source release** (this repo) contains **code only**, no assets. **[V]**
  (`AGENTS.md`, source release README.)
* The RoE-era **SDK** was free and shipped the RoE gamecode — again code only. **[V]**
  (contemporary release listing)
* We found **no official free RoE demo or asset pack** in the sources searched. We cannot prove
  one never existed; treat "free RoE data" as unavailable. **[U]** for non-existence, **[V]** for
  "nothing found".
* BFG data is commercial as well, and is the wrong engine generation (§2). Not a free path.

---

## 5. BFG vs. us: one-line comparison for the port

| | This repo (2011 engine) | BFG (2012 engine) |
|---|---|---|
| What you build | `DOOM3.exe` + optionally a second static game module | One `Doom3BFG.exe` with the game as a static lib |
| RoE | separate `d3xp` module, selected by `fs_game=d3xp` | included in the unified game library |
| Lost Mission | **not present** (no code, no data) | included (8 levels) |
| Data | `base/*.pk4`, `d3xp/*.pk4` | BFG resource files (different layout) |
| Porting state here | x64 static baseline, base game only | out of scope; separate source tree |

If the goal ever becomes "play RoE and Lost Mission on Windows x64 with the least work", **BFG**
(or an existing BFG-based source port) is the cheaper path. If the goal is "extend *this* 2011
port honestly", Option A above is the minimal step, and it stops at the data wall.

---

## 6. Open questions / unverified

1. Whether retail RoE also shipped extra paks besides `d3xp/pak000.pk4` in all regions/patches —
   the engine only checks `pak000.pk4`, but a retail install may include patch paks. **[U]**
2. Whether an official free RoE demo ever existed; none found. **[U]**
3. BFG data compatibility with the 2011 engine — assumed incompatible (different resource
   system); not tested. **[U]**
4. Exact RoE map count labelled "four new multiplayer maps" vs. a separate "CTF pack with four
   maps" in contemporary press; we cite the press wording, we did not inspect the data. **[U]**
5. Whether the PC retail RoE used the same `DOOM3.exe` with a game-selection menu or its own
   launcher executable; the source has the menu hooks (`Session_menu.cpp:74-78`), the packaging
   is not in this repo. **[U]**

---

## 7. References

Code (this repo):

* `neo/framework/FileSystem.cpp` — `fs_game` (91, 490, 2848), `fs_game_base` (163-165),
  setup order (2250-2260), mod list (1745-1747), `ID_ALLOW_D3XP` guard (2854-2860),
  `FindDLL` (3864-3896), `HasD3XP` (3985-4049), `RunningD3XP` (4056-4064).
* `neo/framework/BuildDefines.h:128-134` — `ID_ALLOW_D3XP`.
* `neo/framework/Common.cpp:2629-2691` — `LoadGameDLL`; `2836-2843` — key gate.
* `neo/framework/Session.cpp:2985-3016`, `3123-3252` — `doomkey`/`xpkey`.
* `neo/framework/Licensee.h:110-111` — `CDKEY_FILE`, `XPKEY_FILE`.
* `neo/framework/Session_menu.cpp:74-78`, `324`, `1321` — expansion menu gating.
* `neo/sys/sys_local.cpp:123-133` — `gamex86.dll` name computation.
* `neo/_Game.props:9,15`; `neo/_Game-d3xp.props:9,13-14` — `GAME_DLL`, `_D3XP;CTF`, output names.
* `neo/d3xp/Game.def` — `EXPORTS GetGameAPI`.
* `neo/game/Game_local.cpp:64-65`, `257-272`; `neo/d3xp/Game_local.cpp:64-65` — static game globals.
* `neo/d3xp/Game_local.cpp:72-104`, `2016-2020`, `4686-4690`; `neo/d3xp/Game_local.h:244-258`;
  `neo/d3xp/MultiplayerGame.h:48-55`; `neo/d3xp/Player.cpp:1489-1492`;
  `neo/d3xp/Grabber.h:41` — RoE features.
* `neo/renderer/RenderWorld.h:305` — `ProjectDecalOntoWorld(..., startTime)`.

Port (read-only worktree):

* `targets/win11-amd64/CMakeLists.txt:41-46,55-66`; `sources.cmake:5,58,294-363`;
  `README.md:47-48`.
* `targets/win11-amd64/patches/all-changes.manifest.txt` — d3xp files already x64-patched.

Web:

* Wikipedia, *Doom 3: Resurrection of Evil* —
  https://en.wikipedia.org/wiki/Doom_3:_Resurrection_of_Evil
* Wikipedia, *Doom 3: BFG Edition* — https://en.wikipedia.org/wiki/Doom_3:_BFG_Edition
* Steam, DOOM 3 (consolidated package) — https://store.steampowered.com/app/208200/DOOM_3/
* SteamDB, app 9070 (retired RoE add-on, depends on 9050) —
  https://steamdb.info/app/9070/info/
* Steam, DOOM 3 BFG news / 2022 consolidation —
  https://steamcommunity.com/app/208200/allnews/
* GOG, DOOM 3 (original + RoE + BFG) — https://www.gog.com/en/game/doom_3
* GameSpot, *Doom 3: Resurrection of Evil Review* (Grabber, Artifact, CTF/ThreeWave) —
  https://www.gamespot.com/reviews/doom-3-resurrection-of-evil-review/1900-6121555/
* Retail manual text (weapons, MP modes) —
  https://archive.org/stream/DOOM_3_RESURRECTION_OF_EVIL/DOOM_3_RESURRECTION_OF_EVIL_djvu.txt
* Retail disc item ("Doom 3 is required to install this expansion") —
  https://archive.org/details/doom-3-expansion-pack
* DOOM 3 demo (2004, base game) — https://archive.org/details/Doom3_201405
* ModWiki, DOOM 3 folder structure / `d3xp` base folder —
  https://modwiki.dhewm3.org/Folder_structure_(Doom_3)
* id Software, DOOM-3-BFG GPL source — https://github.com/id-Software/DOOM-3-BFG
* Fabien Sanglard, *Doom3 BFG Source Code Review* (unified static game lib, engine changes) —
  https://fabiensanglard.net/doom3_bfg/
* *The Lost Mission* (8 levels, BFG-only) —
  https://doom.fandom.com/wiki/Doom_3:_The_Lost_Mission

---

## Appendix — how the counts were produced

Run from the repository root (PowerShell, read-only):

```powershell
$neo = 'doom3_orig\doom3\neo'

# file census and per-file diff
Get-ChildItem "$neo\game"  -Recurse -File | ForEach-Object { $_.FullName.Substring((Resolve-Path "$neo\game").Path.Length+1) }  | Sort-Object > $env:TEMP\game.txt
Get-ChildItem "$neo\d3xp" -Recurse -File | ForEach-Object { $_.FullName.Substring((Resolve-Path "$neo\d3xp").Path.Length+1) } | Sort-Object > $env:TEMP\d3xp.txt
Compare-Object (Get-Content $env:TEMP\game.txt) (Get-Content $env:TEMP\d3xp.txt)

# identical vs differing (SHA-256 over the 136 shared paths): 72 / 64

# feature symbol counts
Select-String -Path "$neo\d3xp\*.cpp","$neo\d3xp\*.h" -Pattern 'idGrabber'   | Measure-Object   # 30
Select-String -Path "$neo\d3xp\*.cpp","$neo\d3xp\*.h" -Pattern 'CTF'         | Measure-Object   # 380
Select-String -Path "$neo\d3xp\*.cpp","$neo\d3xp\*.h" -Pattern 'weapon_bloodstone' | Measure-Object

# build lists
([xml](Get-Content "$neo\game.vcxproj")).GetElementsByTagName('ClCompile').Count       # 69
([xml](Get-Content "$neo\game-d3xp.vcxproj")).GetElementsByTagName('ClCompile').Count  # 72 (= 71 d3xp TUs + idlib/precompiled.cpp)
```

No game data, binaries or build output were written. This document is the only repository change.
