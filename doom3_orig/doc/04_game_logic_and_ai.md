# 04 — Game Logic and AI (Doom 3 GPL source)

Read-only analysis of `doom3_orig/doom3/neo` (Doom 3 GPL release, 2011-11-22). All paths are relative to
`doom3_orig/doom3/neo/`. Citations use `path:function` or `path:line`.

**Purpose:** make the game module portable (WASM / other runtime) and make its AI drivable by an
LLM/agent without reverse-engineering the engine.

**Scope note:** the AI *state machine itself is script*, not C++. `script/ai_base.script`,
`script/monster_*.script` and `script/doom_main.script` are **not in this tree** — `doom3_orig/doom3/base/`
contains only `default.cfg` (no pk4, no scripts; assets remain under the original EULA, per
`doom3_orig/doom3/README.txt`). Everything below about "states" describes the C++ support layer plus the exact
event vocabulary the scripts drive. That event vocabulary is the de-facto action API and is fully
visible in `game/ai/AI_events.cpp`.

---

## 1. Module boundary — the game is already a plugin

| Fact | Evidence |
|---|---|
| Engine hands the game a struct of 12 engine interfaces and receives back two pointers | `game/Game.h:323` `gameImport_t`, `game/Game.h:342` `gameExport_t` |
| C ABI entry point (no C++ types cross the boundary except the two structs) | `game/Game.h:350` `GetGameAPI_t`, `extern "C"` |
| API version gate | `game/Game.h:321` `GAME_API_VERSION = 8` |
| Imported systems: sys, common, cmd, cvar, file, network, render, sound, renderModel, UI, decl, AASFile, collisionModel | `game/Game.h:323-340` |
| Base and expansion are *separate* DLL projects with the same sources | `game.vcxproj`, `game-d3xp.vcxproj`, `Game.def` / `d3xp/Game.def` |
| `idGame` is a pure virtual interface with 40+ methods | `game/Game.h:72` `class idGame` |

Implication: the game is already a sealed module. Porting is "implement 12 interfaces", not "cut the
renderer out of the game". The two things that make that hard are (a) `idRenderWorld` /
`idSoundWorld` are used *everywhere* (§11) and (b) the game links `idlib` math/SIMD/containers.

---

## 2. Class hierarchy

`idClass` is the RTTI/reflection/spawn root: every game class registers an `idTypeInfo`
(name, superclass, event map, `CreateInstance`), which is what makes entityDef spawning and
save/load possible without a switch statement.

```
idClass (gamesys/Class.h:174)
└── idEntity (game/Entity.h:107)                      ABSTRACT
    ├── idAnimatedEntity (game/Entity.h:477)
    │   ├── idAFAttachment, idAFEntity_Base → _Gibbable → _Generic, _WithAttachedHead, _Vehicle…
    │   ├── idActor (game/Actor.h:107)
    │   │   ├── idAI (game/ai/AI.h:239)  → idAI_Vagary
    │   │   └── idPlayer (game/Player.h:197)
    │   ├── idWeapon (game/Weapon.h:59)
    │   └── idTestModel
    ├── idProjectile → idGuidedProjectile → idSoulCubeMissile; idBFGProjectile
    ├── idDebris
    ├── idMoveable → idBarrel → idExplodingBarrel; idItem → idItemPowerup / idObjective / idPDAItem / idMoveableItem…
    ├── idMover, idMover_Binary → idDoor / idPlat, idMover_Periodic → idRotater/idBobber/idPendulum/idRiser
    ├── idTrigger → idTrigger_Multi / _EntityName / _Timer / _Count / _Hurt / _Fade / _Touch
    ├── idLight, idCamera → idCameraView / idCameraAnim, idSecurityCamera
    ├── idTarget (25 subtypes), idBrittleFracture, idEntityFx, idSmokeParticles family
    ├── idWorldspawn, idSpawnableEntity, idPlayerStart, idStaticEntity, idAnimated
    ├── idCombatNode (game/ai/AI.cpp:4900)              AI attack-position volume
    ├── idFuncAASPortal, idFuncAASObstacle              AAS topology at runtime
    └── idFuncPortal, idFuncEmitter, idBeam, idLiquid, idEarthQuake…
idClass
├── idPhysics (game/physics/Physics.h:76)  ABSTRACT
├── idForce (game/physics/Force.h)
└── idThread (game/script/Script_Thread.cpp:114)        script coroutine, not an entity
```

Key facts:

- **`idActor` derives from `idAFEntity_Gibbable`**, not directly from `idAnimatedEntity`
  (`game/Actor.cpp:364` `CLASS_DECLARATION( idAFEntity_Gibbable, idActor )`). So every actor carries
  an `idAF` articulated-figure/ragdoll and a gib path.
- `idPlayer` and `idAI` are siblings (`game/Actor.cpp:364`, `game/ai/AI_events.cpp:164`).
- `idWeapon` is an *entity*, owned by a player, with its own script thread and animations
  (`game/Weapon.h:59`, `game/Weapon.cpp:74`).
- `idPhysics` objects are `idClass`s but **not entities**; an entity owns them by value
  (`idEntity::physicsObj`, `idEntity::defaultPhysicsObj`).
- Expansion-only classes: `idGrabber`, `idHarvestable`, `idAFEntity_Harvest`, `idItemTeam`,
  `idShockwave`, `idFuncMountedObject`, `idFuncMountedWeapon`, `idPortalSky`, `idTrigger_Flag`,
  `idForce_Grab`, `idAFEntity_VehicleAutomated` (see §10).

---

## 3. The frame loop

`game/Game_local.cpp:2157` `idGameLocal::RunFrame( const usercmd_t *clientCmds )`

| # | Step | Evidence |
|---|---|---|
| 0 | If single-player and `g_stopTime`, only `player->Think()` runs (menus/pause) | 2174-2183 |
| 1 | Advance clock: `framenum++`, `previousTime = time`, `time += msec` | 2186-2189 |
| 2 | `random.RandomInt()` — burns one PRNG value per frame so player actions perturb the stream | 2198-2200 |
| 3 | `gameRenderWorld->SetRenderView( player->GetRenderView() )`; clear debug lines/polygons | 2202-2214 |
| 4 | `memcpy` this frame's `usercmd_t` per client into `gameLocal.usercmds[]` | 2217 |
| 5 | `smokeParticles->FreeSmokes()` | 2220 |
| 6 | `ServerProcessEntityNetworkEventQueue()` | 2223 |
| 7 | `UpdateGravity()` (gravity vector from map/worldspawn) | 2226 |
| 8 | `SetupPlayerPVS()` — merged PVS of all players | 2229 |
| 9 | `SortActiveEntityList()` — pushers first, team masters before slaves | 2232 |
| 10 | **`for ( ent = activeEntities … ) ent->Think();`** — the entire simulation | 2240-2273 |
| 11 | Remove entities whose `thinkFlags == 0` from the active list | 2276-2288 |
| 12 | **`idEvent::ServiceEvents();`** — timed/deferred event queue drains here | 2295 |
| 13 | `FreePlayerPVS()` | 2300 |
| 14 | `if ( isMultiplayer ) mpGame.Run()` | 2303-2305 |
| 15 | Fill `gameReturn_t` (health, heartRate, stamina, combat 0-100, sessionCommand) | 2314-2337 |
| 16 | Debug render (`RunDebugInfo`, `D_DrawDebugLines`) | 2354-2355 |

Clock granularity: `msec = USERCMD_MSEC`, `USERCMD_HZ = 60`, `USERCMD_MSEC = 1000/60 = 16`
(`game/Game_local.h:294`, `framework/UsercmdGen.h:40-41`). The simulation is a **fixed 16 ms tick**,
one think pass per tick, single-threaded, deterministic order = order in `activeEntities`.

Think gating uses bit flags (`game/Entity.h:63`):

| Flag | Meaning |
|---|---|
| `TH_THINK` | run the entity's Think |
| `TH_PHYSICS` | run physics |
| `TH_ANIMATE` | advance animation |
| `TH_UPDATEVISUALS` | rebuild renderEntity |
| `TH_UPDATEPARTICLES` | particles |

`idEntity::Think` is just `RunPhysics(); Present();` (`game/Entity.cpp:822`). Subclasses extend the
chain rather than replacing it (`idAI::Think` → `RunPhysics/UpdateAnimation/Present/LinkCombat`,
`game/ai/AI.cpp:1047`; `idPlayer::Think` → controls → physics → view, `game/Player.cpp:6180`).

**Dormancy** is the main CPU saver and a determinism hazard: `idEntity::DoDormantTests`
(`game/Entity.cpp:835`) removes monsters from the active list when their AAS area is not connected
to any player and they are outside the player PVS. Dormant entities do not think, so behavior
depends on PVS/connectivity — fine for a game, awkward for headless batch simulation (§12).

---

## 4. Entity spawning: map → entityDef → C++ object

Three spawn paths exist; all end in `idClass::CallSpawn` → virtual `Spawn()`.

| Path | Mechanism | Evidence |
|---|---|---|
| `spawnclass` (native) | entityDef key names an `idTypeInfo`; `CreateInstance()` → `CallSpawn()` | `game/Game_local.cpp:3041-3062` |
| `spawnfunc` (script) | entityDef names a script function; a new `idThread` is `DelayedStart(0)`-ed | `game/Game_local.cpp:3066-3075` |
| Direct `SpawnEntityDef(args)` | used by map load, `spawn()` console cmd, script `sys.spawn` | `game/Game_local.cpp:3018` |

Order of map load:

1. `idGameLocal::LoadMap` — `idMapFile::Parse( mapName + ".map" )`, then
   `collisionModelManager->LoadMap( mapFile )`, reset `entities[]`, `random.SetSeed( isMultiplayer ? randseed : 0 )`
   (`game/Game_local.cpp:833-886`). **Single-player seed is always 0.**
2. `idGameLocal::MapPopulate` — `SpawnMapEntities()`, `SpreadLocations()`,
   `RandomizeInitialSpawns()`, compute `mapSpawnCount`, then `idEvent::ServiceEvents()` so the map
   script's `main()` runs before the first physics step (`game/Game_local.cpp:1129`).
3. `idGameLocal::SpawnMapEntities` — entity 0 is forced to `ENTITYNUM_WORLD` and must instantiate
   `idWorldspawn`; then each map entity is `InhibitEntitySpawn`-filtered (skill/MP keys) and spawned
   (`game/Game_local.cpp:3184-3233`).
4. `idEntity::Spawn` — `RegisterEntity`, cache `entityDefNumber`, parse render entity and refSound
   from spawnArgs, set flags (`neverDormant`, `hide`, `networkSync`), then subclass spawn runs
   (`game/Entity.cpp:444`).

Important invariants for a port:

- `entityNumber` is the array index in `entities[MAX_GENTITIES]`; slot `< MAX_CLIENTS` is reserved
  for players, so map entities start at `MAX_CLIENTS` (`game/Game_local.cpp:881-883`).
- `spawnIds[]` is the generation counter behind `idEntityPtr` — pointers across respawn/save must go
  through `idEntityPtr` (`game/Game_local.h:217`), never raw pointers.
- Entity defs are **declarations** (`DECL_ENTITYDEF`, `declManager->FindType`), with `_mp` override
  lookup in multiplayer (`game/Game_local.cpp:3087`). EntityDef inheriting via `SetDefaults`.
- Map entities are the authoritative entity list; a port must keep `.map` parsing + entityDef
  inheritance to stay compatible with existing mods.

---

## 5. Event dispatch (the real scripting ABI)

Two layers:

| Layer | What it is | Evidence |
|---|---|---|
| Static event defs | `idEventDef( "name", "argformat", 'returntype' )` registered in a global table at startup | `game/gamesys/Event.cpp:60`, `idEventDef::FindEvent` 189 |
| Per-class event map | `CLASS_DECLARATION` builds a `eventMap[eventNum] → member fn ptr` table | `game/gamesys/Class.h:110`, `game/ai/AI_events.cpp:167` `EVENT(...)` |
| Dispatch | `ProcessEventArgPtr` resolves `GetType()->eventMap[num]` and calls the member through a generated 0-8-arg thunk | `game/gamesys/Class.cpp:939-1040`, thunk table `Callbacks.cpp` |
| Deferred queue | `PostEventMS/PostEventSec` → `idEvent` list sorted by time; drained by `ServiceEvents` | `game/gamesys/Class.cpp:628`, `game/gamesys/Event.cpp:460` |
| Script | `idThread` runs script functions; `idThread::CallFunction`/`ReturnEntity` bridge script ↔ C++ | `game/script/Script_Thread.cpp` |

`idEvent::ServiceEvents` is strictly "pop while `event->time <= gameLocal.time`", insertion-ordered
within a tick, and `event->object->ProcessEventArgPtr( ev, args )` (`game/gamesys/Event.cpp:474-532`).
The queue is serialized in savegames (`idEvent::Save`, `Event.cpp:614`), so pending timers survive
load.

**Targets / triggers.** `idEntity::ActivateTargets` walks the `target*` key/values, and for each
target that `RespondsTo(EV_Activate)` calls `Signal(SIG_TRIGGER)` then
`ProcessEvent(&EV_Activate, activator)` (`game/Entity.cpp:3545`). This is the entire "level logic"
graph: movers, doors, triggers, lights, targets, and `target_*` helper entities all work through it.

**Why this matters for porting:** the game logic is not a hand-written FSM in C++. It is
`entityDef` data + script threads + this event bus. A re-hosting effort must either (a) keep the
`idProgram`/`idThread` script VM in C++, or (b) re-implement dozens of scripted entity behaviors.
(a) is strongly recommended; the VM is ~180 KB of self-contained code (`game/script/`).

---

## 6. AI (`idAI`)

### 6.1 Where the FSM actually lives

| Layer | Role |
|---|---|
| `idThread` + `script/ai_base.script`, `monster_*.script` | the state machine (`state_Idle`, `state_WakeOnFlashlight`, `state_Combat`, …), calls C++ only through events |
| `idAI` (C++) | perception, movement primitives, turning, attacks, animation, muzzle flash |
| `idMoveState move` (`game/ai/AI.h:170`) | the current movement order + status, and the only thing a controller needs to write |
| Script variables (`idScriptBool` / `idScriptFloat`, `game/ai/AI.h:395-414`) | the observation channel C++ → script, linked by name in `LinkScriptVariables` (`game/ai/AI.cpp:1165`) |

The script-visible observations are exactly: `AI_TALK, AI_DAMAGE, AI_PAIN, AI_SPECIAL_DAMAGE,
AI_DEAD, AI_ENEMY_VISIBLE, AI_ENEMY_IN_FOV, AI_ENEMY_DEAD, AI_MOVE_DONE, AI_ONGROUND,
AI_ACTIVATED, AI_FORWARD, AI_JUMP, AI_ENEMY_REACHABLE, AI_BLOCKED, AI_OBSTACLE_IN_PATH,
AI_DEST_UNREACHABLE, AI_HIT_ENEMY, AI_PUSHED` (`game/ai/AI.cpp:1165-1183`). State transitions read
these; `state = GetScriptFunction("state_Killed"); SetState(state)` on death
(`game/ai/AI.cpp:3390`).

### 6.2 Movement types and commands

| Enum | Values | Evidence |
|---|---|---|
| `moveType_t` | `MOVETYPE_DEAD, ANIM, SLIDE, FLY, STATIC` | `game/ai/AI.h:54` (comment: mirrored in `script/ai_base.script`) |
| `moveCommand_t` | `MOVE_NONE, FACE_ENEMY, FACE_ENTITY, TO_ENEMY, TO_ENEMYHEIGHT, TO_ENTITY, OUT_OF_RANGE, TO_ATTACK_POSITION, TO_COVER, TO_POSITION, TO_POSITION_DIRECT, SLIDE_TO_POSITION, WANDER` | `game/ai/AI.h:63` |
| `moveStatus_t` | `DONE, MOVING, WAITING, DEST_NOT_FOUND, DEST_UNREACHABLE, BLOCKED_BY_WALL/_OBJECT/_ENEMY/_MONSTER` | `game/ai/AI.h:96` |
| attack flags | `ATTACK_IGNORE / ON_DAMAGE / ON_ACTIVATE / ON_SIGHT` | `game/ai/AI.h:48` |

Per-tick dispatch by move type (`game/ai/AI.cpp:1079-1121`):

| moveType | Per-tick function | Motion model |
|---|---|---|
| `DEAD` | `DeadMove` | ragdoll / AF physics |
| `ANIM` | `AnimMove` (calls `GetMovePos` → `PathToGoal`; feeds `physicsObj.SetDelta`) | root-motion animation + monster physics |
| `SLIDE` | `SlideMove` | velocity-commanded slide |
| `FLY` | `FlyMove` (+ `AddFlyBob`, `AdjustFlyHeight`, `FlySeekGoal`, `FlyTurn`) | sinusoidal fly |
| `STATIC` | `StaticMove` | turret-like, turns only |

Every branch runs `UpdateEnemyPosition()` → `UpdateAIScript()` (script tick) → move → chatter/blink.

### 6.3 Perception

| Sense | Implementation | Evidence |
|---|---|---|
| Sight (geometric) | `idActor::CanSee(ent, useFov)` — eye point → target eye/origin, `MASK_OPAQUE` trace via `gameLocal.clip.TracePoint`, then `GetTraceEntity` | `game/Actor.cpp:1452-1479` |
| FOV | `idActor::CheckFOV` — horizontal dot against `viewAxis[0]` with infinite vertical vision | `game/Actor.cpp:1425-1445` |
| Derived flags | `AI_ENEMY_VISIBLE` / `AI_ENEMY_IN_FOV` set in `UpdateEnemyPosition`; also sets `lastVisibleEnemyPos`, `lastVisibleReachableEnemyPos` | `game/ai/AI.cpp:3756-3815` |
| Hearing | *global* alert: `idGameLocal::AlertAI` stores `lastAIAlertEntity` for one frame; AI checks `GetAlertEntity()` + `AI_HEARING_RANGE = 2048` + `InPlayerPVS` | `game/Game_local.cpp:3517`, `game/ai/AI_events.cpp:491`, `game/ai/AI.h:45` |
| Reachability | `PointReachableAreaNum` + `PathToGoal` sets `lastReachableEnemyPos` | `game/ai/AI.cpp:3788-3794` |
| Combat-node volumes | `idCombatNode::EntityInView` — cone/height/range volumes authoring attack spots | `game/ai/AI.h:662`, `AI.cpp:4900` |
| Flashlight | `idAI::TouchedByFlashlight` + `wakeOnFlashlight` | `game/ai/AI.cpp:3605` |

Note the asymmetry: **hearing is not per-entity raycast/mixing**; it is a single "who made noise this
frame" global (`gameLocal.AlertAI`) plus a squared-distance test. Cheap, but it means a port that
wants real audio occlusion must add it.

### 6.4 Combat

| Stage | C++ | Notes |
|---|---|---|
| Pick/validate target | `CheckForEnemy`, `ReactionTo`, `SetEnemy`, `EnemyDead`, `ClearEnemy` | `game/ai/AI.cpp:3171, 3313, 3553, 3616, 3829` |
| Aim test | `CanHitEnemy` → `TestMelee` / trajectory prediction | `AI_events.cpp:1427` region, `AI.cpp:4277` |
| Windup | `BeginAttack(name)` / `Event_AttackBegin` | `game/ai/AI.cpp:4026` |
| Melee | `TestMelee` → `AttackMelee(meleeDef)` → `DirectDamage`, `DamageFeedback` | `AI.cpp:4277, 4334, 4235, 4215` |
| Projectile | `CreateProjectile` → `LaunchProjectile(joint, target, clampToAttackCone)`; `GetAimDir`; ballistic solve | `AI.cpp:4045, 4085, 3979`; `AI_pathing.cpp:1438` `PredictTrajectory` |
| Recovery | `EndAttack` | `AI.cpp:4036` |
| Death | `Killed` → `state_Killed` script + gibs/ragdoll | `AI.cpp:3313` |

Melee/projectile hit test is animation-frame driven (`CalculateAttackOffsets`, `lastHitCheckTime`,
`AI_HIT_ENEMY`), so attacks are timed to animation frames, not to a cooldown timer — important when
re-targeting AI updates to a different tick rate.

### 6.5 Movement/nav helpers relevant to a controller

`StopMove`, `FaceEnemy`, `FaceEntity`, `DirectMoveToPosition`, `MoveToEnemyHeight`,
`MoveOutOfRange`, `MoveToAttackPosition`, `MoveToEnemy`, `MoveToEntity`, `MoveToPosition`,
`MoveToCover`, `SlideToPosition`, `WanderAround`, `TurnToward` — all in `game/ai/AI.cpp:1492-2010,
2459`. Each just mutates `move` and returns bool; this is the clean, side-effect-light seam.

---

## 7. AAS navigation

### 7.1 File format

`AAS` = "Area Awareness System": a BSP of convex areas with precomputed reachabilities, generated
offline by `neo/tools/compilers/aas`. The runtime never builds it.

| Element | Struct | Evidence |
|---|---|---|
| Magic/version | `AAS_FILEID "DewmAAS"`, `AAS_FILEVERSION "1.07"` | `tools/compilers/aas/AASFile.h:40-41` |
| BSP nodes | `aasNode_t { planeNum, children[2] }` (0 = solid leaf, negative = -(area)) | `AASFile.h:170` |
| Areas | `aasArea_t { faces, bounds, center, flags, contents, cluster, clusterAreaNum, travelFlags, reach, rev_reach }` | `AASFile.h:155` |
| Faces | `aasFace_t { planeNum, flags, edges, areas[2] }` (two-sided portal between areas) | `AASFile.h:146` |
| Reachability | `idReachability { travelType, toAreaNum, start, end, edgeNum, travelTime, next, rev_next }` + subclasses Walk / BarrierJump / WaterJump / WalkOffLedge / Swim / Fly / Special(dict) | `AASFile.h:93-132` |
| Clusters/portals | `aasCluster_t`, `aasPortal_t` (routing acceleration) | `AASFile.h:176-189` |
| Travel flags | `TFL_WALK, CROUCH, WALKOFFLEDGE, BARRIERJUMP, JUMP, LADDER, SWIM, WATERJUMP, TELEPORT, ELEVATOR, FLY, SPECIAL, WATER, AIR` | `AASFile.h:44-58` |
| Area flags | `AREA_FLOOR, GAP, LEDGE, LADDER, LIQUID, CROUCH, REACHABLE_WALK, REACHABLE_FLY` | `AASFile.h:68-75` |
| Settings | `idAASSettings`: up to 4 bounding boxes, gravity, `maxStepHeight`, `maxBarrierHeight`, `maxWaterJumpHeight`, `maxFallHeight`, `minFloorCos`, fixed travel times | `AASFile.h:211-238` |

Loading is **engine-side**: `AASFileManager->LoadAAS( mapName, mapFileCRC )`
(`game/ai/AAS.cpp:82`); the game only talks to `idAAS`. This is good news for a port: AAS parsing can
stay wherever the collision model lives.

### 7.2 Routing API (what a bot/agent needs)

`idAAS` abstract interface (`game/ai/AAS.h:75`), implemented by `idAASLocal`
(`game/ai/AAS_local.h:86`):

| Query | Use |
|---|---|
| `PointAreaNum`, `PointReachableAreaNum`, `BoundsReachableAreaNum` | world position → AAS area |
| `AreaCenter`, `AreaFlags`, `AreaTravelFlags` | area metadata |
| `TravelTimeToGoalArea` | scalar cost (100ths of a second) — usable as an RL distance feature |
| `RouteToGoalArea` | first hop reachability + travel time |
| `WalkPathToGoal` / `FlyPathToGoal` → `aasPath_t { type, moveGoal, moveAreaNum, secondaryGoal, reachability }` | the per-frame "steer here" point; **this is what `idAI` uses** |
| `WalkPathValid` / `FlyPathValid` | straight-line shortcut test (path smoothing) |
| `FindNearestGoal( aasGoal_t&, …, idAASCallback& )` | cover / out-of-range / attack-position search |
| `Trace( aasTrace_t&, start, end )` | area-graph trace (num areas, last area, blocking area) |
| `AddObstacle` / `RemoveObstacle` / `SetAreaState` | dynamic obstacles and door/portal closure |

Routing performance comes from `areaCacheIndex` (per area × cluster travel times) and
`portalCacheIndex`, with LRU eviction (`game/ai/AAS_local.h:127-160`). AAS pathing is incremental:
`idAI::PathToGoal` (`AI.cpp:1392`) simply calls `aas->WalkPathToGoal` and falls through to
`FindPathAroundObstacles` (`AI_pathing.cpp:924`) when the straight AAS path is blocked by a dynamic
obstacle (barrels, other monsters).

`idAI::SetAAS` binds an AI to an AAS by the entityDef key `use_aas` and validates the AI's bbox
against `idAASSettings::boundingBoxes`; mismatch is a hard error (`game/ai/AI.cpp:1295-1313`).
**Consequence:** a new agent character needs a matching AAS bbox size in the compiled `.aas`, or it
must be authored as one of the existing sizes.

---

## 8. Physics layer

`idPhysics` is a *position/orientation driver* plus a container for `idClipModel`s, not a solver
library (`game/physics/Physics.h:37-60` documents the contract). All collision queries ultimately go
to `idClip`, which uses `idCollisionModelManager` (engine) over `.cm`/`.map` geometry.

| Class | Base | Used by | Notes |
|---|---|---|---|
| `idPhysics` | `idClass` | — | pure interface; `Evaluate(timeStep, endTime)`, `SaveState/RestoreState`, `WriteToSnapshot` |
| `idPhysics_Base` | `idPhysics` | — | shared clip-model/contact bookkeeping |
| `idPhysics_Actor` | `idPhysics_Base` | actors | adds ground/gravity handling |
| `idPhysics_Player` | `idPhysics_Actor` | `idPlayer` | Quake-lineage `PM_NORMAL/PM_DEAD/PM_SPECTATOR/PM_FREEZE/PM_NOCLIP` state machine (`Physics_Player.h:44`), step/slide/water/ladder handling |
| `idPhysics_Monster` | `idPhysics_Actor` | `idAI` | step height, floor cosine, `monsterMoveResult_t`, fly vs delta vs velocity move |
| `idPhysics_AF` | `idPhysics_Base` | ragdolls, vehicles | articulated figure, constraints — the biggest file in the tree (~210 KB) |
| `idPhysics_Parametric` | `idPhysics_Base` | `idMover`, `idMisc` movers | scripted/parametric motion (pos1/pos2, splines) with blocking + pushers |
| `idPhysics_RigidBody` | `idPhysics_Base` | `idMoveable`, `idItem`, `idProjectile`, debris | impulse/contact response |
| `idPhysics_Static` | `idPhysics` | static map geometry | no motion |
| `idPhysics_StaticMulti` | `idPhysics` | multi-clipmodel statics | world brushes |
| `idForce*` | `idClass` | `idPhysics_AF`, grabber | `Constant, Drag, Field, Spring, Grab` |

`idClipModel`/`idClip` (`game/physics/Clip.h:54`, `:248`) provide `Translation`, `Rotation`,
`Motion`, `Contacts`, `Contents`, `TracePoint`, `TraceBounds`, `EntitiesTouchingBounds`. Contents
masks are the engine's `CONTENTS_*` / `MASK_*` bitfields.

Entity ↔ physics contract (`game/Entity.cpp:2524` `idEntity::RunPhysics`):

1. skip if `!TH_PHYSICS` (but still run animation controllers);
2. team slaves do nothing — the team master simulates the whole team;
3. save state of every team part, disable their clip if `!solidForTeam`;
4. `part->physics->Evaluate( endTime - startTime, endTime )` per part;
5. detect blocking, re-run/restore as needed, then `BecomeActive(TH_ANIMATE|TH_UPDATEVISUALS)`.

So **the entity never sets its own origin**; it reads `GetPhysics()->GetOrigin()/GetAxis()` for
presentation. That inversion is the cleanest seam for a port: a replacement physics backend only has
to satisfy the `idPhysics` interface.

---

## 9. Save / load and determinism

`idGameLocal::SaveGame` (`game/Game_local.cpp:408-527`) writes, in order:

| Section | Contents |
|---|---|
| build number | `BUILD_NUMBER` (rejects incompatible saves) |
| object table | every entity (team-linked) + every `idThread`, then `WriteObjectList` |
| script | `program.Save` — script state space |
| session | skill, `serverInfo`, per-client `userInfo`/`usercmd`/persistent info |
| entities | for each of `MAX_GENTITIES`: object ref + `spawnIds[i]` |
| globals | `firstFreeIndex`, `num_entities`, `world`, spawned list, active list, `numEntitiesToDeactivate`, sort flags, `persistentLevelInfo`, `globalShaderParms`, **`random.GetSeed()`**, `frameCommandThread`, `sessionCommand` |
| cinematics/time | skip timers, `inCinematic`, `framenum`, `previousTime`, `time` |
| net | `isMultiplayer`, `gameType`, `isServer/isClient`, `localClientNum` |

Per-entity: `idEntity::Save` (`game/Entity.cpp:617`) stores `entityNumber`, `entityDefNumber`,
`spawnArgs`, `thinkFlags`, `dormantStart`, `renderEntity`, `modelDefHandle`, `refSound`, bind/team
links, **`defaultPhysicsObj` via `WriteStaticObject`**, PVS areas, signal table. Pending events are
saved separately by `idEvent::Save` (`Event.cpp:614`) with object + `idEventDef` name (not a raw
pointer), and `trace_t` payloads are name-resolved on load (`Event.cpp:774`).

Restore is `idGameLocal::InitFromSaveGame` (`Game_local.cpp:1199`): `ReadBuildNumber` →
`CreateObjects` → `program.Restore` (aborts and falls back to level restart if the script changed) →
`RestoreObjects`.

**Determinism posture:**

| Property | Status |
|---|---|
| Fixed timestep | Yes — 16 ms, `msec` is a compile-time constant |
| Simulation order | Deterministic: `activeEntities` list order + insertion-ordered event queue |
| RNG | `idRandom` only; SP seeded 0 (`Game_local.cpp:886`), seed saved (`:491`); no `rand()`/`srand()` in `neo/game` |
| Float determinism | **No guarantee.** `idSIMD::InitProcessor` / `com_forceGenericSIMD` (`Game_local.cpp:2194`) allow switching SIMD kernels at runtime; float ops differ across SSE/NEON/wasm |
| Dormancy | PVS/connectivity-dependent, so a headless camera-less run diverges from a player run |
| Physics substepping | `idPhysics_Player`/`AF`/`RigidBody` use `USERCMD_MSEC`-quantized time (`Physics.cpp:76` `SnapTimeToPhysicsFrame`) |
| Multiplayer | Client prediction re-runs `Think` for the local player (`ClientPrediction`), so "one tick = one think" is not globally true in MP |

For an agent/RL harness, rely on *fixed tick + seeded RNG + event log*, not on float equality; record
`usercmd` + event stream for replay.

---

## 10. Network (`game/Game_network.cpp`)

The network layer is a snapshot/delta system layered on the same entity list.

| Function | Role |
|---|---|
| `ServerWriteSnapshot` / `ClientReadSnapshot` | per-client delta snapshot of `networkSync` entities (`Game_network.cpp:550`, `957`) |
| `ServerApplySnapshot` / `ClientApplySnapshot` | patch entity states from a snapshot (`:694`, `1242`) |
| `WriteGameStateToSnapshot` / `ReadGameStateFromSnapshot` | game-level bits (`:518`, `:533`) |
| `ClientPrediction` | re-simulates local player from `usercmd`s (`:1465`) |
| `ServerProcessEntityNetworkEventQueue` / `ClientProcess…` | reliable per-entity events (`:725`, `:1251`) |
| `InitClientDeclRemap` / `ServerSendDeclRemapToClient` | maps server decl indices to client decls (`:117`, `:149`) |
| `idPlayer::WritePlayerStateToSnapshot` | player-specific state (`game/Player.cpp:478`) |
| `idEntity::WriteToSnapshot` / `ReadFromSnapshot` | per-class state; e.g. `idWeapon`, `idProjectile` |

Only entities with `networkSync` set are replicated (`game/Entity.cpp:501-504`). Single-player still
runs `isMultiplayer == false`, and `RunFrame` vs `ClientPrediction` are distinct code paths.

For a WASM/headless port: the network layer can be compiled out (it is confined to
`Game_network.cpp` + `MultiplayerGame.cpp` + per-class snapshot methods), but those snapshot methods
are also handy as a compact per-entity state serializer.

---

## 11. d3xp (Resurrection of Evil) delta

Same source tree duplicated under `neo/d3xp/`, guarded by `#ifdef _D3XP`, built as
`game-d3xp.vcxproj`. It is **not** a diff-friendly build: whole files are forked (e.g.
`d3xp/Player.cpp` is 274 KB vs `game/Player.cpp` 230 KB) and the `_D3XP` guards are dense in place.

Guard density (`#ifdef _D3XP` / `#ifdef CTF` occurrences, top files):

| File | Count |
|---|---|
| `d3xp/Player.cpp` | 111 |
| `d3xp/MultiplayerGame.cpp` | 47 |
| `d3xp/Game_local.cpp` | 40 |
| `d3xp/Weapon.cpp` | 38 |
| `d3xp/Projectile.cpp` | 22 |
| `d3xp/Moveable.cpp`, `Entity.cpp` | 18 each |
| `d3xp/ai/AI.cpp` | 17 |
| `d3xp/Player.h`, `MultiplayerGame.h` | 17, 16 |
| `d3xp/Item.cpp`, `Mover.cpp`, `ai/AI_events.cpp` | 13 each |

New classes (not in base):

| Class | Base | Purpose |
|---|---|---|
| `idGrabber` | `idEntity` | gravity-gun weapon entity |
| `idForce_Grab` | `idForce` | grab constraint physics |
| `idHarvestable`, `idAFEntity_Harvest` | `idEntity`, `idAFEntity_WithAttachedHead` | soul-harvest corpses |
| `idItemTeam` | `idItem` | team-based pickup (MP) |
| `idShockwave` | `idEntity` | expanding damage volume |
| `idFuncMountedObject`, `idFuncMountedWeapon` | `idEntity` | mounted turrets |
| `idPortalSky` | `idEntity` | portal sky box |
| `idTrigger_Flag` | `idTrigger_Multi` | CTF flag trigger |
| `idAFEntity_VehicleAutomated` | vehicle | scripted vehicle |

Behavioral additions worth knowing when porting:

| Area | Change | Evidence |
|---|---|---|
| Time | dual time groups `fast`/`slow` (`timeState_t`, `slowmoState_t SLOWMO_STATE_*`) plus `SetTimeState` RAII push/pop; `SelectTimeGroup`/`GetTimeGroupTime` are real | `d3xp/Game_local.h` (`timeState_t`, `slow`, `fast`), `d3xp/Game_local.cpp` `RunTimeGroup2`, `ComputeSlowMsec` |
| Entities | `timeGroup`, `noGrab`, x-ray render entity; `DetermineTimeGroup` | `d3xp/Entity.h` |
| Player | powerups `INVULNERABILITY, HELLTIME, ENVIROSUIT, ENVIROTIME`, rechargeable ammo, `weapon_bloodstone`, bloom, mounted objects | `d3xp/Player.h` |
| Items | ammo regen, `CanGive`, empty-clip handling | `d3xp/Item.cpp`, `d3xp/Player.h` |
| AI | new events `launchProjectile`, `triggerFX`, `startEmitter/stopEmitter/getEmitter`, `moveToPositionDirect`, `avoidObstacles`; `spawnClearMoveables`, `funcEmitters`, `harvestEnt` | `d3xp/ai/AI.h` diff, `d3xp/ai/AI_events.cpp` |
| Rendering/PVS | portal-sky PVS merge in the snapshot path; `GetPlayerPVS()` accessor | `d3xp/Game_network.cpp`, `d3xp/Game_local.h` |
| Net | `newServerInfo` flag lets the server push a new `idDict` serverinfo mid-game | `d3xp/Game_network.cpp` |

Porting recommendation: treat base as the reference and d3xp as an optional compile-time flavor.
Do not try to "merge" them file-by-file; pick one, or parametrize the 6 heavy files
(`Player`, `Game_local`, `Weapon`, `MultiplayerGame`, `Entity`, `AI`).

---

## 12. Renderer / sound coupling (evidence)

The game module never touches OpenGL, but it does drive the render and sound *worlds* directly.
Reference counts across `neo/game/**/*.cpp`:

| Symbol | Occurrences |
|---|---|
| `gameRenderWorld` | 392 |
| `declManager->` | 204 |
| `renderModelManager->` | 29 |
| `renderEntity_t` | 28 |
| `uiManager->` | 20 |
| `gameSoundWorld` | 32 |
| `soundSystem->` | 2 |

Concrete choke points:

| Coupling | Site |
|---|---|
| Entity → render entity registration | `idEntity::UpdateModel` / `Present`: `gameRenderWorld->AddEntityDef(&renderEntity)`, `UpdateEntityDef` (`game/Entity.cpp:764, 1407-1409, 4931`) |
| Entity → sound emitter | `refSound.referenceSound = gameSoundWorld->AllocSoundEmitter()` (`game/Entity.cpp:1600`), `idEntity::UpdateSound` (`:1657`) |
| AI muzzle flash | raw `renderLight_t worldMuzzleFlash` + `AddLightDef`/`UpdateLightDef` (`game/ai/AI.cpp:4501-4527`) |
| Animation → render model | `idAnimator` reads `modelHandle->GetJoints()` and drives `renderEntity` (`game/anim/Anim_Blend.cpp:2179, 2645, 2953`) |
| Player view / HUD | `CalculateRenderView`, `DrawHUD(idUserInterface*)`, `renderSystem->DrawStretchPic`, `gameSoundWorld->PlaceListener` (`game/PlayerView.cpp:438-472`) |
| FX / particles | `idEntityFx`, `idSmokeParticles`, decl particles (`game/Fx.cpp`, `SmokeParticles.cpp`) |
| Media precache | `CacheDictionaryMedia` resolves materials/models/sounds/GUIs from spawnArgs (`game/Game_local.cpp:1640`) |
| Decl dependency | entityDef/modelDef/material/skin/particle/soundShader are all `idDecl*` from `declManager` — including inside save games (`idSaveGame::WriteMaterial`, `SaveGame.h:70-77`) |

Also coupled, though not to graphics: `cmdSystem` / `cvarSystem` (`gamesys/SysCmds.cpp`,
`SysCvar.cpp` — ~100 KB of console commands and cvars registered from the game),
`sys->FPU_*` (`CalcFov` sanity check, `Game_local.cpp:2382`), and `idLib` (`idStr`, `idDict`,
`idList`, `idVec3`, `idMat3`, `idRandom`, `idLexer`, `idParser`).

Cleanly *not* coupled: AAS, collision (`idClip` on `idCollisionModelManager`), `idClass`/event
system, `idPhysics_*`, `idSaveGame`, `idProgram`/`idThread`, `idAI` decision logic (except
`UpdateMuzzleFlash`).

---

## 13. Recommended seams

### 13.1 Porting the module

| Option | Work | Assessment |
|---|---|---|
| **A. Recompile C++ → wasm32-emscripten** | implement `gameImport_t` vtables as JS/WASM glue; stub or port `idRenderWorld`/`idSoundWorld`; keep `idlib`, CM, AAS | **Recommended.** Preserves entityDef compatibility, the script VM, all class behavior, savegames. Cost: struct/vtable ABI, float determinism, `idSys`/file system. |
| B. Rewrite game logic in TS/Rust | reimplement 130+ classes, entityDef semantics, event bus, script VM | Rejected for fidelity: scripting + entityDef data are the product. |
| C. Keep native game DLL, drive it through the existing C ABI | lowest risk, but no web target | Useful as a reference oracle for differential testing of A. |

Concrete seam plan for A:

1. **Null backends first.** `idRenderWorld` and `idSoundWorld` are pure-virtual engine interfaces
   (`framework/RenderWorld.h`, `framework/sound/…`). Implement `NullRenderWorld` (AddEntityDef →
   handle, UpdateEntityDef → no-op, DebugLine → ring buffer, RenderScene → no-op) and
   `NullSoundWorld`. That alone yields a headless, deterministic game loop; all 392 `gameRenderWorld`
   uses still compile.
2. **Keep `idCollisionModelManager` + `idAASFileManager`.** They are pure geometry and are the only
   things standing between the game and a working simulation.
3. **Split presentation from state.** `idEntity::Present/UpdateVisuals` and `idActor`'s
   `idAnimator` are the only render-facing code paths in the entity. Everything else
   (`idPhysics`, `idAI`, `idMover`, `idTrigger`, `idTarget`, `idSaveGame`) is engine-agnostic C++.
   A WebGL front end can consume a serialized `renderEntity_t`/`renderLight_t` stream instead of
   calling `idRenderWorld` directly.
4. **Replace `idLib` selectively.** `idDict`/`idStr`/`idList`/`idRandom`/`idVec3`/`idMat3` must keep
   exact semantics (they are in savegame format and in entityDef parsing). `idSIMD` should be forced
   to the generic path (`com_forceGenericSIMD 1`) for cross-platform reproducibility.
5. **Keep the script VM.** `game/script/` is self-contained; dropping it means reimplementing
   `doom_main.script`, `ai_base.script` and every monster's states — the largest single risk.

### 13.2 Agent / LLM / RL API

The existing AI event table is already a typed tool schema. Suggested layering:

```
LLM / policy  ── JSON tool call ──▶  AgentBridge  ──▶  idAI event (PostEventMS / direct call)
                      ▲                                   │
                      └── observation JSON ───────────────┘  (read-only getters + AAS queries)
```

**Action space** — map directly onto existing events (`game/ai/AI_events.cpp:40-162`), which already
have format strings suitable for schema generation:

| Verb | Event / call | Args |
|---|---|---|
| move_to | `moveToPosition` (`AI.cpp:1883`) | `idVec3` |
| move_to_entity | `moveToEntity` | entity handle |
| move_to_enemy | `moveToEnemy` | — |
| move_to_cover | `moveToCover` | hide-from entity (implicit) |
| stop | `stopMove` / `Event_StopMove` | — |
| face | `turnTo` / `turnToPos` / `turnToEntity` | yaw / pos / entity |
| attack_begin | `attackBegin(name)` | attack name |
| melee | `attackMelee(def)` | melee def |
| shoot | `createMissile(joint)` / `attackMissile(joint)` / `launchProjectile` (d3xp) | joint name |
| reload/idle/wait | `waitAction(state)` | state name |
| set move mode | `setMoveType(int)` `MOVETYPE_*` | enum |
| talk/look | `setTalkTarget`, `lookAt(entity,dur)` | — |

Two levels are worth exposing:

- **Primitive layer (12 move commands + 6 attack verbs)** — one call per 16 ms tick, closest to the
  script's own vocabulary. Deterministic and cheap.
- **Intent layer (`MoveToPosition` + `WaitMove` / `WaitAction`)** — blocking-until-predicate, matching
  how `ai_base.script` actually sequences behavior. Better for LLM planning; needs the event queue's
  timed callbacks (`waitMove` blocks the *script thread*, so an external controller needs its own
  completion callback on `AI_MOVE_DONE`).

**Observation space** — all values already exist as fields; no new state needed:

| Group | Fields | Source |
|---|---|---|
| self kinematics | `physicsObj.GetOrigin/GetAxis`, linear/angular velocity, `viewAxis`, `current_yaw`, `move.moveType` | `game/ai/AI.h` |
| self status | `health`, `AI_DEAD`, `AI_ONGROUND`, `AI_BLOCKED`, `AI_PUSHED`, `AI_OBSTACLE_IN_PATH` | `AI.cpp:1165` |
| move status | `move.moveCommand`, `move.moveStatus`, `move.moveDest`, `move.toAreaNum`, `move.blockTime` | `AI.h:170` |
| enemy | `enemy` handle, `lastVisibleEnemyPos`, `lastVisibleReachableEnemyPos`, `lastReachableEnemyPos`, `AI_ENEMY_VISIBLE`, `AI_ENEMY_IN_FOV`, `AI_ENEMY_REACHABLE`, `AI_ENEMY_DEAD` | `AI.h:387-414`, `AI.cpp:3756` |
| nav | `PointReachableAreaNum(self)`, `TravelTimeToGoalArea`, `aasPath_t.moveGoal`, `moveAreaNum` | `AAS.h:112-127` |
| local geometry | N short `clip.TracePoint` fan (e.g. 8 × 512 uu) or `clip.Contacts` | `Clip.h:271` |
| world | entity list within radius (`gameLocal.EntitiesTouchingBounds`), alert entity | `Clip.h:298`, `Game_local.cpp:3530` |

**Design rules that follow from the code:**

1. Do **not** let the agent write `move` structs directly *and* call `MoveTo*` — pick one. The
   `MoveTo*` calls are idempotent-ish and maintain `blockTime`, `AI_MOVE_DONE`, and AAS state;
   writing `move` bypasses all of that (`AI.cpp:1631-1930`).
2. Run the agent at the tick boundary, inside `RunFrame` **after** step 10 (`Think`) and before
   step 12 (`ServiceEvents`) — otherwise posted actions land one tick late and event ordering becomes
   ambiguous.
3. Persist agent state *outside* `idSaveGame`. `idAI::Save/Restore` (`AI.cpp:405, 537`) will not know
   about an external policy's memory; keep a sidecar keyed by `entityNumber + spawnIds` (the
   `idEntityPtr` identity).
4. Determinism: seed `idRandom` explicitly (SP hardcodes 0), disable dormancy per agent
   (`spawnArgs["neverDormant"] = 1`) so PVS-dependent wakeups do not change behavior between a
   headless observer and a rendered run, and record `usercmd`+events for replay.
5. For RL, `msec` is fixed, so `step()` = one `RunFrame`, and `TravelTimeToGoalArea` /
   `PathToGoal` success are good intrinsic reward / termination signals. `moveStatus_t` gives a
   ready-made discrete failure taxonomy (`DEST_UNREACHABLE`, `BLOCKED_BY_*`).

---

## 14. Coupling summary (what a port must earn)

| Subsystem | Portability | Blocker |
|---|---|---|
| `gamesys/Class`, `Event`, `idTypeInfo` | High | none — pure C++/reflection |
| `script/` VM | High | self-contained; depends on `idLexer`/`idParser` |
| `idEntity` state machine | High | `renderEntity_t`/`refSound_t` fields in savegames; `ParseSpawnArgsTo*` in `idGameEdit` |
| `idAI` decisions | High | `idAnimator` (joint handles) for look/aim/attack offsets |
| `idActor`/animation | Medium | `idRenderModel` joints, `idMD5Anim`, IK (`game/IK.cpp`) |
| Physics | Medium-High | `idCollisionModelManager` geometry + float determinism |
| AAS | High | file parsing lives in engine (`idAASFileManager`) |
| Save/load | High | declaration names embedded in the format (`WriteMaterial`, `WriteModelDef`, …) |
| Network | Medium | per-class `WriteToSnapshot`; can be omitted |
| Rendering / audio | **Low** | 392 + 32 direct call sites; `idRenderWorld`/`idSoundWorld` stubs or a stream are mandatory |

---

## 15. Open questions / risks

1. `idGameEdit::ParseSpawnArgsToRenderEntity` (`game/GameEdit.cpp`) is called from `idEntity::Spawn`
   even in a headless run — it is the *only* place spawnArgs become render/sound structs, and it is an
   `idGameEdit` (exported) method. A null render backend must still run it (it also sets the initial
   `origin`/`axis` used by everything downstream, `game/Entity.cpp:471-472`).
2. `idAI` behavior depends on **animation frame callbacks** (`ServiceAnims`, `AI_JumpFrame`,
   attack-frame offsets from `CalculateAttackOffsets`). A headless or non-MD5 animation path changes
   combat timing, not just visuals.
3. Hearing is global (`AlertAI`) and PVS-gated (`InPlayerPVS`), so agents that are never near the
   player will not hear scripted noise. Any "agent noise" API must set `gameLocal.AlertAI()` itself.
4. d3xp's slowmo time groups make `gameLocal.time` non-monotonic relative to wall time; agent
   step-counting must use `fast.time`/`framenum`, not `time`.
5. The GPL release lacks scripts and assets; any behavioral claim about a specific monster's state
   machine cannot be verified from this tree, only the C++ event surface it consumes.
