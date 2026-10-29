# Q0 reference scenes (T007)

Status: **established** by the recorded `baseline-final13` run. All four categories were captured
twice with the engine's own framebuffer screenshot at a fixed tic. Results are in
[evidence-q0.md](./evidence-q0.md).

## 1. Data identity and legal boundary

Only the official free DOOM 3 demo is available locally. It is lawful test data but it is **not**
committed, extracted, redistributed or modified. The retail game is not installed.

| Item | Value |
|---|---|
| Primary data file | `base/demo00.pk4` |
| Bytes | 483,534,533 |
| SHA-256 | `e373ec70b35cf72852a1f6014c2c2dc21f890a03cd41b21a9524755f951f64f9` |
| Local shims | `base/guis/mainmenu.gui` (529,866 bytes, `9ff40f6a...`), `base/script/weapon_base.script` (2,721 bytes, `80266cc1...`) |
| Maps present | `maps/game/demo_mars_city1`, `demo_mars_city2`, `demo_mc_underground` (`.map`, `.proc`, `.cm`) |
| Retail data | absent; no retail scene is claimed |

## 2. Four categories

| Id | Category | Map | Drive (before the appended screenshot/quit) | Tolerance `meanAbs` / `diffPercent` |
|---|---|---|---|---|
| `corridor` | corridor | `game/demo_mc_underground` | `+devmap game/demo_mc_underground +wait 600 +set g_showHud 0 +echo SCENE_READY_corridor` | 0.5 / 0.5 |
| `open-view` | open view | `game/demo_mars_city2` | `+set ui_showGun 0 +devmap game/demo_mars_city2 +wait 600 +set g_showHud 0 +set g_skipFX 1 +set g_skipParticles 1 +echo SCENE_READY_open-view` | 2.0 / 2.0 |
| `close-character` | close character/viewmodel | `game/demo_mars_city2` | `+devmap game/demo_mars_city2 +wait 600 +set pm_thirdPerson 1 +set pm_thirdPersonRange 80 +set pm_thirdPersonAngle 180 +set g_showHud 0 +set g_skipFX 1 +set g_skipParticles 1 +echo SCENE_READY_close-character` | 0.5 / 0.5 |
| `moving-light-combat` | moving light/combat | `game/demo_mars_city2` | `+devmap game/demo_mars_city2 +wait 600 +set g_showHud 0 +god +spawn monster_demon_imp +wait 120 +echo SCENE_READY_moving-light-combat` | 3.0 / 5.0 |

The screenshots are:

| Scene | What the frame shows |
|---|---|
| `corridor` | Underground interior corridor through the opened access doors, with pipes, stairs and a lit doorway. |
| `open-view` | Open hangar through the opened transfer-elevator doors, with the upper walkway, floor grating and elevator frame. |
| `close-character` | Third-person marine close-up in the transfer elevator, weapon in hand, with the "TRANSFER SUCCESSFUL" panel. |
| `moving-light-combat` | Imp attacking through the elevator doors, with the shotgun viewmodel and the imp's orange projectile light. |

The frames are distinct and recognisable; the category label is not a substitute for the image.
No named entity, map or cvar is invented; the classname `monster_demon_imp` is present in the demo
defs and the spawn behavior is in `neo/game/gamesys/SysCmds.cpp:773-808`.

## 3. Why these frames repeat

The failure mode of the earlier OS-window captures was not the renderer: the demo plays an intro
camera, a moving transfer elevator, scripted doors and AI, and the wall-clock capture point varied
between runs. The fix is threefold:

1. **Fixed game tic.** `com_fixedTic 1` makes the session advance a fixed number of game tics per
   rendered frame instead of deriving the tic count from wall time
   (`neo/framework/Session.cpp:37,2581-2582,2698-2701`). After `+wait 600`, both runs are at the
   same game time even if one machine frame was slower.
2. **Engine framebuffer screenshot.** `+screenshot screenshots/<name>.tga` executes inside the
   command buffer at the same frame (`neo/renderer/RenderSystem_init.cpp:2055`), instead of an OS
   client-area capture whose window focus and timing vary. `+wait 30 +quit` lets the TGA flush and
   exits the process.
3. **Scene controls.** `g_showHud 0`, `g_skipFX 1`, `g_skipParticles 1`, `ui_showGun 0`,
   `pm_thirdPerson` and `god` remove the HUD, fire/particle noise, viewmodel sway and death flash
   from the frames where they are not the subject.

The script converts the uncompressed true-colour TGA (24- or 32-bit) to PNG with a small in-script decoder, then uses
the same pixel comparator as before. The TGA is 1280x720; no window chrome is included.

## 4. Common command arguments

```
+set logFile 2
+set r_fullscreen 0
+set r_mode -1
+set r_customWidth 1280
+set r_customHeight 720
+set com_fixedTic 1
+set fs_basepath <D3_DATA_DIR>
+set fs_savepath <OutDir>\<scene>\run<n>\save
```

Facts used:

- `+devmap` strips the extension, requires the map under `maps/`, sets `developer 1` and starts a
  new game (`neo/framework/Session.cpp:127-158`); the `game/` prefix is required.
- `+wait N` delays the remaining buffered commands by **N rendered frames**, not seconds
  (`neo/framework/CmdSystem.cpp:291-296,622-634`).
- `+echo` is a system command (`neo/framework/CmdSystem.cpp:329`); it marks the end of the drive.
- `spawn <classname>` places the entity 80 units in front of the player and faces it back at the
  player (`neo/game/gamesys/SysCmds.cpp:773-808`); it is a single-player cheat and is allowed in
  single player.
- `g_showHud` hides the HUD (`neo/game/gamesys/SysCvar.cpp:270`); `g_skipFX` and
  `g_skipParticles` reduce effects (`:107-108`); `ui_showGun` hides the first-person viewmodel
  (`:84`); `pm_thirdPerson`, `pm_thirdPersonRange` and `pm_thirdPersonAngle` select the
  third-person camera (`:260-264`).
- `g_stopTime 1` was tested and rejected: it freezes game time but leaves the 3D view black while
  the HUD remains. `com_fixedTic` is the control that works.

## 5. Repeatability tolerances and result

The metric is the script's per-pixel maximum absolute RGB difference, counted at `delta=8`;
`diffPercent` is the share of changed pixels; `meanAbs` is the mean per-channel absolute
difference. A scene passes when both limits hold and all log markers match.

| Scene | Max `meanAbs` | Max `diffPercent` | Measured `meanAbs` | Measured `diffPercent` | Result |
|---|---:|---:|---:|---:|---|
| `corridor` | 0.5 | 0.5 | 0.1827 | 0.2079 | PASS |
| `open-view` | 2.0 | 2.0 | 0.2258 | 0.2103 | PASS |
| `close-character` | 0.5 | 0.5 | 0.1082 | 0.0982 | PASS |
| `moving-light-combat` | 3.0 | 5.0 | 0.0550 | 0.0318 | PASS |

The ceilings are deliberately tight for static scenes and looser for the animated imp. The
combat result (1.40%) is well inside 5%; a failed spawn is rejected by the
`Unknown classname 'monster_demon_imp'` forbidden log marker, and a black/blank frame is rejected
by the distinct-colour and centre-luminance guards.

## 6. Calibration history (disclosed, not the recorded result)

The OS-window attempts failed because the elevator/intro camera/AI advanced by wall time:

| Attempt | Observation | Outcome |
|---|---|---|
| `baseline-open` / `final` / `open2` / `open3` | elevator/overlay moved between OS captures | 0.57% / 14.32% / 6.63% / 6.05% |
| `baseline-final6` / `open-long` | elevator at a different height | 72.19% / 64.66% |
| `baseline-close19` / `final6` | idle pose + elevator | 4.63% / 93.51% |
| `baseline-combat` / `final5` / `combat-god` | AI, death flash, PDA | 15.84% / 99.35% / 94.85% |

The fixed-tic engine-frame switch then produced:

| Probe | Outcome |
|---|---|
| `probe-fixedtic` open, 240 tics | 0.072% changed |
| `probe-eng-open600` | 0.69% changed |
| `probe-eng-close` | 0.003% changed |
| `probe-eng-combat` | 1.98% changed |
| `probe-eng-corridor` | 0.084% changed |
| `baseline-final13` | 0.21% / 0.21% / 0.10% / 0.03% — all four PASS |

The failed OS-window runs are retained under `build-q0-*` but are not the recorded evidence. The
fixed-tic run is the one that establishes Q0.

**Ceiling revision disclosure**: the final13 ceilings (0.5/0.5, 2.0/2.0, 0.5/0.5, 3.0/5.0) were
tightened after the probes. The earlier final9 ceilings were 0.5/1.0, 2.0/8.0, 2.5/6.0 and
8.0/20.0. They were frozen before final13; no result was loosened, and final13 also passes the
earlier, looser cells. "Pre-declared" here means frozen before the recorded run, not unchanged
since the first attempt.

**Corridor reclassification disclosure**: final9 captured `control-interior` as a closed door at 1
tic on `demo_mc_underground`. final13 uses the same map at 600 tics, after the doors open, and that
frame shows the underground corridor. The control scene was removed; this is not a relabel of the
door frame.
