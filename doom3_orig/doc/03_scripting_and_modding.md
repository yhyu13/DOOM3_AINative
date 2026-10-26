# 03 — Scripting and modding surface (Doom 3 GPL, `neo/game`)

Status: read-only source analysis. Scope is the engine copy in `doom3_orig/doom3/neo/game`,
`doom3_orig/doom3/neo/game/gamesys`, `doom3_orig/doom3/neo/framework/Decl*.cpp`, `doom3_orig/doom3/neo/ui/GuiScript.cpp`,
and `doom3_orig/doom3/neo/game/Game.def`. `doom3_orig/doom3/neo/d3xp` contains parallel copies of much of the
same code; it was not used as evidence here except where noted.

Game data is not in this repository. `doom3_orig/doom3/base/` contains only a 14-byte placeholder
`default.cfg` (`# empty file`), with no `.pk4`, `.script`, `.def`, `.fx`, `.prt`, `.af`,
or `.pda` files. Claims about retail script contents are therefore marked **unverified**.

The short answer: a large amount of gameplay can be modded without touching C++ — new
monsters, weapons, items, map logic, UI, FX, particles, skins, ragdolls, PDAs, and AI state
machines composed from the existing event vocabulary. The hard boundary is the event
surface itself. Script cannot add a new native event, a new C++ class, a new network
message, a new save field, an array, or a dynamic container. Anything that needs new
perception, new physics, new rendering, or new persistent state requires C++.

---

## 1. The `.script` language

### 1.1 Program model

There is one `idProgram` per game (`idGameLocal::program`,
`doom3_orig/doom3/neo/game/Game_local.h:267`). All compiled scripts share its globals, types,
functions, and statement array. There is no module isolation and no separate heap per mod.
`idProgram::Startup` frees everything and compiles the default script
(`doom3_orig/doom3/neo/game/script/Script_Program.cpp:1908`); `idGameLocal::Init` calls it with
`SCRIPT_DEFAULT` = `script/doom_main.script` (`doom3_orig/doom3/neo/game/Game.h:42`,
`doom3_orig/doom3/neo/game/Game_local.cpp:300`).

Every `CompileFile` call force-includes `script/doom_defs.script` before the source
(`doom3_orig/doom3/neo/game/script/Script_Compiler.cpp:2596`). That file is the declaration layer for
native events, not an implementation file. The map's own `<map>.script` is compiled later
by `idWorldspawn::Spawn` (`doom3_orig/doom3/neo/game/WorldSpawn.cpp:57`, `:77`).

There is no directory scan for `.script` files. The engine compiles only:

- `script/doom_main.script` at startup;
- the current map script at worldspawn;
- files explicitly compiled by console commands or savegame restore.

Therefore a new script object type must be reachable by `#include` from an already-compiled
script, or the mod must replace/extend `script/doom_main.script`. This is a data-level
constraint, not a C++ one, but it is the first thing a script-only mod must solve.

### 1.2 Types

`etype_t` (`doom3_orig/doom3/neo/game/script/Script_Program.h:47`) defines the language types:

| Type | Meaning | Notes |
|---|---|---|
| `void` | no value | function return only |
| `float` | IEEE float | the only numeric type in practice |
| `vector` | three floats | `idVec3` |
| `string` | fixed 128-byte buffer | `MAX_STRING_LEN` = 128 (`Script_Program.h:41`) |
| `entity` | entity handle | not a pointer in script |
| `boolean` | 0/1 | distinct type but stored as int |
| `object` | record with single inheritance | script object (`Script_Program.cpp:2003`) |
| `namespace` | name scope | global or nested namespace |
| `scriptEvent` | native event declaration | global only |
| `function` | function value | internal/callable |
| `field`, `pointer`, `virtualfunction`, `jumpoffset`, `argsize` | compiler/runtime internals | not user-declarable |

`CheckType` accepts only the source-level names plus previously defined object types
(`doom3_orig/doom3/neo/game/script/Script_Compiler.cpp:852`). There is no pointer type, no array type,
no struct, no enum, no union, and no user-defined numeric type.

Strings are fixed-size, null-terminated buffers copied with `idStr::Copynz`; the interpreter
also has a `PushString` path that reserves 128 bytes on the local stack
(`doom3_orig/doom3/neo/game/script/Script_Interpreter.h:152`). There is no string heap and no growable
string in script.

### 1.3 Grammar and statements

The parser is a conventional recursive-descent compiler. Top-level definitions are
`ParseDefs` (`Script_Compiler.cpp:2485`):

```text
float x;
vector v = '1 2 3';
entity e;
string s;
object my_type : parent_type { float field; void method(...) { ... } };
namespace my_ns { ... }
scriptEvent void eventName(float a, entity b);
void functionName(float a) { ... }
```

Objects are declared with `object <name> : <parent> { ... };` and may inherit only from
another object (`ParseObjectDef`, `Script_Compiler.cpp:2003`). The parser injects an implicit
`self` parameter into object methods (`ParseFunction`, `Script_Compiler.cpp:2100`), and
`init` / `destroy` are the constructor/destructor names (`Script_Program.cpp:1032`, `:1044`).

Statements (`ParseStatement`, `Script_Compiler.cpp:1932`) are limited to:

- blocks `{ ... }`
- `return`
- `while`, `for`, `do ... while`
- `break`, `continue`
- `if` / `else`
- local declarations and initializers
- expression statements

There is no `switch`, no `goto` at source level, no `try`/`catch`, no lambda, no `sizeof`,
no `typedef`, and no preprocessor metaprogramming beyond the parser's `#include`/`#define`
support. Local declarations are attached to the enclosing function scope, not to the inner
block, because `ParseDefs` uses the current function `scope`.

Global variables may only be initialized with immediate values (`ParseVariableDef`,
`Script_Compiler.cpp:2263`). Non-trivial initialization must happen in `init` or a function.

### 1.4 Operators and opcodes

`Script_Compiler.cpp:48` maps operators to opcodes and type signatures. `Script_Compiler.h:47`
defines the runtime opcode enum: 123 opcodes from `OP_RETURN` through `OP_CONTINUE`
(`NUM_OPCODES`, `Script_Compiler.h:194`). The important groups are:

| Group | Opcodes (representative) | Notes |
|---|---|---|
| Control | `RETURN`, `IF`, `IFNOT`, `GOTO`, `CALL`, `THREAD`, `OBJTHREAD`, `EVENTCALL`, `OBJECTCALL`, `SYSCALL`, `BREAK`, `CONTINUE` | `BREAK`/`CONTINUE` are compiler placeholders; loops are patched into jumps |
| Stack | `PUSH_F/V/S/ENT/OBJ`, `STORE_*`, `STOREP_*`, `ADDRESS`, `INDIRECT_*` | `STOREP_*` is pointer store for field/array-like internals |
| Arithmetic | `ADD_*`, `SUB_*`, `MUL_*`, `DIV`, `MOD_F`, unary `U*` | vector/string variants exist (`ADD_S`, `ADD_FS`, `MUL_VF`, etc.) |
| Comparison | `EQ_*`, `NE_*`, `LE`, `GE`, `LT`, `GT` | separate entity/object/string variants |
| Logic/bit | `AND*`, `OR*`, `BITAND`, `BITOR`, `NOT_*` | `&&`/`||` are emitted as jumps plus opcodes |
| Conversion | `INT_F`, `STORE_FTOS`, `STORE_VTOS`, `STORE_FTOBOOL`, `STORE_BOOLTOF` | implicit and explicit coercion paths |
| Unary | `NEG_F`, `NEG_V`, `COMP_F`, `UINC_F`, `UDEC_F`, `UINCP_F`, `UDECP_F` | `++`/`--`/`~` |

Operator support is broad for numbers and vectors: `+ - * / %`, comparisons, `&& || !`,
bitwise `& |`, compound assignments (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`), and
pre/post increment/decrement. Strings support assignment, `+` concatenation, `==`/`!=`,
and a small set of native string helpers (`strLength`, `strLeft`, `strRight`, `strSkip`,
`strMid`, `strToFloat`, `doom3_orig/doom3/neo/game/script/Script_Thread.cpp:96-101`). There is no
string indexing, no `substr` with arbitrary semantics, no formatting language, and no
general string library. `int()` truncates a float; all "integers" are floats at runtime.

`OptimizeOpcode` constant-folds arithmetic and comparisons at compile time
(`Script_Compiler.cpp:497`). The bytecode is a simple statement array
(`statement_t`, `Script_Program.h:414`) executed by a switch in
`idInterpreter::Execute` (`Script_Interpreter.cpp:936`).

### 1.5 Absolute limits

| Limit | Value | Evidence |
|---|---|---|
| String bytes | 128 | `Script_Program.h:41` |
| Global variable space | 196,608 bytes | `Script_Program.h:42` |
| Functions | 3,072 | `Script_Program.h:44` |
| Statements | 81,920 | `Script_Program.h:45` |
| Call stack depth | 64 | `Script_Interpreter.h:32` |
| Local stack | 6,144 bytes | `Script_Interpreter.h:33` |
| Runaway instructions per `Execute` | 5,000,000 | `Script_Interpreter.cpp:957` |
| Native event args | 8 | `doom3_orig/doom3/neo/game/gamesys/Event.h:36` |
| Event definitions | 4,096 | `Event.h:48` |
| Event-processing safety limit per frame (fatal on overflow) | 4,096 | `doom3_orig/doom3/neo/game/gamesys/Event.cpp:40` |

No arrays, no dynamic allocation in script memory (game entities can still be spawned through
events), no pointers, no references, no exceptions, no closures, no first-class coroutines,
no file I/O, no network I/O, no OS calls. Brackets `[` `]` appear in the lexer's punctuation
table (`Script_Compiler.cpp:41`) but no parse path consumes them, so they are not an array
syntax. The interpreter is a
sandbox only in the sense that it has no syscall surface; it is not a security sandbox for
gameplay (it can still call `setcvar`, `spawn`, `error`, damage events, etc.). The bytecode
and globals are saved/recompiled as described in §9.8.

---

## 2. Event system

### 2.1 `idEventDef`: the native event registry

`idEventDef` is a static-registered descriptor (`doom3_orig/doom3/neo/game/gamesys/Event.h:53`).
Every C++ event is declared at file scope:

```cpp
const idEventDef EV_SetOrigin( "setOrigin", "v" );
const idEventDef EV_GetOrigin( "getOrigin", NULL, 'v' );
```

The constructor stores the name, a format string, a return type, argument offsets, and a
unique `eventnum`; it records an error for duplicate names with mismatched signatures, and
`idEvent::Init` aborts the game if any such error was recorded
(`doom3_orig/doom3/neo/game/gamesys/Event.cpp:57`, `:562`). `FindEvent` is a case-sensitive linear name lookup
(`Event.cpp:192`). Arg format characters are:

| Code | Type | Script type |
|---|---|---|
| `d` | integer | `float` |
| `f` | float | `float` |
| `v` | vector | `vector` |
| `s` | string | `string` |
| `e` | entity | `entity` |
| `E` | nullable entity | `entity` |
| `t` | trace | **not exposed to script** |

The trace type is explicitly rejected by `GetTypeForEventArg`
(`Script_Compiler.cpp:2379`) and by the interpreter's marshalling
(`Script_Interpreter.cpp:782`, `:912`). Script cannot declare or call trace events.

There are roughly 479 `idEventDef` instances across 22 `neo/game` `.cpp` files, including
123 in `ai/AI_events.cpp`, 72 in `Entity.cpp`, and the thread/system events in
`script/Script_Thread.cpp`. The registry is global and flat; event names are not namespaced.

### 2.2 How an event becomes callable from script

An event is **not** automatically visible. The script must declare it with `scriptEvent`,
normally in `script/doom_defs.script`:

```text
scriptEvent void    setOrigin(vector origin);
scriptEvent vector  getOrigin();
scriptEvent void    setState(string statename);
```

`ParseEventDef` (`Script_Compiler.cpp:2398`) does all of the following:

1. Looks up the name with `idEventDef::FindEvent`; unknown names are a compile error.
2. Checks the declared return type against the native return type.
3. Checks each declared parameter type against the native format string.
4. Allocates a script function type and a `function_t` whose `eventdef` points at the
   native descriptor.

`scriptEvent` declarations are allowed only in the global namespace
(`Script_Compiler.cpp:898`). This is why `doom_defs.script` is force-included: it is the
single global declaration surface.

At a call site, `entity.event(args)` emits `OP_EVENTCALL`; `sys.event(args)` emits
`OP_SYSCALL`; object methods emit `OP_OBJECTCALL`
(`Script_Compiler.cpp:1112`, `:1139`, `:1094`). `idInterpreter::CallEvent`
(`Script_Interpreter.cpp:683`) then:

- resolves the entity handle to an `idEntity*`;
- checks `RespondsTo(event)`;
- marshals arguments according to the native format string;
- calls `idEntity::ProcessEventArgPtr`;
- copies the return value into the program's return slot;
- pops the arguments, or suspends the thread for a multi-frame event.

If the entity is missing or does not respond, the interpreter writes a safe zero/empty
return and continues (`Script_Interpreter.cpp:693`). A required null entity kills the thread
with a warning (`Script_Interpreter.cpp:776`). This "safe default" behavior is important for
mods: a typo in an event name is caught at compile time, but an event that the target class
does not implement fails silently at runtime (with a developer warning).

### 2.3 Class dispatch

Classes declare event handlers with `CLASS_DECLARATION` and `EVENT` macros
(`doom3_orig/doom3/neo/game/gamesys/Class.h:53`, `:110`). `idClass::Init` builds an `eventMap` indexed
by `eventnum` for every class (`doom3_orig/doom3/neo/game/gamesys/Class.cpp:377`), inheriting the
superclass map and overriding selected entries (`Class.cpp:124`). Dispatch is a direct
member-function pointer call through `ProcessEventArgPtr`
(`Class.cpp:939`). `RespondsTo` is a cheap `eventMap[eventnum] != NULL` test
(`Class.h:349`).

The practical consequence: **adding one new event requires a C++ `idEventDef` and a class
`EVENT` binding.** No amount of `.script` can extend the registry. You can add a new script
function, object type, or state, but every observable/action primitive must already exist.

### 2.4 Key event catalog

The following are the high-value events for gameplay mods. Names are exact and
case-sensitive. Formats are not repeated for every entry; see the source lines cited.

#### Entity / world (`Entity.cpp`, `Class.cpp`)

| Event | Purpose |
|---|---|
| `activate`, `activateTargets` | target activation, map triggers |
| `remove` | safe removal (scheduled) |
| `getOrigin`, `setOrigin`, `getAngles`, `setAngles` | transform |
| `getLinearVelocity`, `setLinearVelocity`, `getAngularVelocity`, `setAngularVelocity` | physics |
| `getSize`, `getMins`, `getMaxs` | bounds |
| `distanceTo`, `distanceToPoint`, `touches` | spatial queries |
| `hide`, `show`, `isHidden` | visibility |
| `bind`, `bindPosition`, `bindToJoint`, `unbind` | attachment |
| `setModel`, `setSkin` | appearance |
| `getKey`, `getFloatKey`, `getVectorKey`, `getEntityKey`, `getNextKey`, `setKey` | spawn-arg access |
| `setShaderParm`, `setShaderParms`, `setColor` | material parameters |
| `startSound`, `startSoundShader`, `stopSound`, `fadeSound` | audio |
| `startFx` | FX declaration |
| `hasFunction`, `callFunction` | dynamic script dispatch |
| `getJointHandle`, `setJointPos`, `setJointAngle`, `getJointPos`, `getJointAngle` | animation joints |

`remove` and the internal `<immediateremove>` are in `Class.cpp:229`; the rest are in
`Entity.cpp` around the `EV_*` definitions near the top of the file.

#### AI / actor (`Actor.cpp`, `ai/AI_events.cpp`, `ai/AI.cpp`)

There are 123 `AI_*` event descriptors. The important groups:

| Group | Events |
|---|---|
| State / script | `setState(string)`, `waitAction(string)`, `stopThinking`, `setTalkState` |
| Perception | `findEnemy`, `findEnemyAI`, `findEnemyInCombatNodes`, `heardSound`, `canSee`, `enemyInCombatCone`, `entityInAttackCone`, `enemyRange`, `enemyRange2D`, `enemyPositionValid`, `canHitEnemy`, `canHitEnemyFromAnim`, `canHitEnemyFromJoint` |
| Enemy | `getEnemy`, `getEnemyPos`, `getEnemyEyePos`, `predictEnemyPos`, `setEnemy`, `clearEnemy` |
| Movement | `moveToPosition`, `moveToEntity`, `moveToEnemy`, `moveToCover`, `moveToAttackPosition`, `moveOutOfRange`, `slideTo`, `wander`, `randomPath`, `stopMove`, `moveStatus`, `waitMove`, `facingIdeal` |
| Facing | `turnTo`, `turnToPos`, `turnToEntity`, `faceEnemy`, `faceEntity`, `getCurrentYaw` |
| Combat | `attackMelee`, `attackMissile`, `launchMissile`, `createMissile`, `fireMissileAtTarget`, `muzzleFlash`, `chargeAttack`, `testMeleeAttack`, `testChargeAttack`, `testAnimAttack` |
| Damage / health | `getHealth`, `setHealth`, `allowDamage`, `ignoreDamage`, `directDamage`, `radiusDamageFromJoint` |
| Ragdoll / burn | `becomeRagdoll`, `stopRagdoll`, `burn`, `clearBurn`, `preBurn`, `shrivel`, `setSmokeVisibility` |
| Chatter | `setTalkTarget`, `getTalkTarget`, `setTalkState` |

`setState` is declared as `AI_SetState` in `Actor.cpp:360` and handled by
`idActor::Event_SetState` (`Actor.cpp:3245`). It sets `idealState`, which
`idActor::UpdateScript` consumes and executes (`Actor.cpp:1327`). State names are arbitrary
object methods; a missing `state_*` function is a fatal script error
(`Actor::SetState`, `Actor.cpp:1296`).

`idAI::Think` calls `UpdateAIScript`, which calls `UpdateScript`
(`ai/AI.cpp:1047`, `:1183`). This is the per-frame AI script budget.

#### Player (`Player.cpp`)

| Event | Purpose |
|---|---|
| `getButtons`, `getMove`, `getViewAngles` | raw input state |
| `getCurrentWeapon`, `getPreviousWeapon`, `getIdealWeapon`, `selectWeapon` | weapon selection |
| `enableWeapon`, `disableWeapon`, `getWeaponEntity` | weapon control |
| `openPDA`, `inPDA`, `exitTeleporter`, `levelTrigger` | player state transitions |
| `getButtons` etc. return values used by player script for animation/weapon logic |

#### Weapon (`Weapon.cpp`)

| Event | Purpose |
|---|---|
| `getOwner`, `nextWeapon` | ownership / switching |
| `weaponState(string, int)` | force a state |
| `useAmmo`, `addToClip`, `ammoInClip`, `ammoAvailable`, `totalAmmoCount`, `clipSize`, `autoReload` | ammo |
| `weaponOutOfAmmo`, `weaponReady`, `weaponReloading`, `weaponHolstered`, `weaponRising`, `weaponLowering` | state queries |
| `launchProjectiles`, `createProjectile`, `ejectBrass`, `melee`, `flashlight` | action primitives |
| `getWorldModel`, `allowDrop`, `netReload`, `netEndReload`, `isInvisible` | presentation/network |

#### Thread / system (`Script_Thread.cpp`)

These are called through `sys.` and are available to every script:

| Event | Purpose |
|---|---|
| `wait(float)`, `waitFrame()`, `waitFor(entity)`, `waitForThread(int)` | cooperative blocking |
| `print(string)`, `println(string)`, `say(string)`, `warning(string)`, `error(string)` | console/log |
| `random(float)`, `getTime()`, `getFrameTime()`, `getTicsPerSecond()` | timing/RNG |
| `getEntity(string)`, `spawn(string)`, `setSpawnArg`, `SpawnString/Float/Vector` | dynamic spawn |
| `trace`, `tracePoint`, `getTraceFraction`, `getTraceEndPos`, `getTraceNormal`, `getTraceEntity`, `getTraceJoint`, `getTraceBody` | ray tests |
| `angToForward/Right/Up`, `sin`, `cos`, `sqrt`, `vecNormalize`, `vecLength`, `DotProduct`, `CrossProduct`, `VecToAngles` | math |
| `fadeIn`, `fadeOut`, `fadeTo`, `music` | presentation |
| `strLength`, `strLeft`, `strRight`, `strSkip`, `strMid`, `strToFloat` | string helpers |
| `radiusDamage`, `isClient`, `isMultiplayer`, `influenceActive` | gameplay/environment |
| `debugLine`, `debugArrow`, `debugCircle`, `debugBounds`, `drawText`, `cacheSoundShader` | debug/asset |

`spawn` is a particularly sharp tool: it can create arbitrary `entityDef` instances and is
therefore both the basis of summoning/projectile logic and a resource-exhaustion/DoS vector
for untrusted AI.

### 2.5 Scheduled events

`idEvent` is the queue element (`Event.h:88`). `idEvent::Alloc` copies arguments into a
per-event byte buffer, resolving strings and entity pointers for save/restore
(`Event.cpp:238`); `Schedule` puts it on the time-ordered queue
(`Event.cpp:382`); `ServiceEvents` runs due events every game frame
(`Event.cpp:460`). `idGameLocal::RunFrame` calls it (`Game_local.cpp:2295`). The queue is
saved and restored as part of the game save (`Event.cpp:614`, `:678`).

Script `wait` and `waitFrame` use this machinery indirectly: the thread posts
`EV_Thread_Execute` at the wake time and returns control to the frame loop
(`Script_Thread.cpp:646`). This is the only "scheduler" script sees.

---

## 3. Thread model

`idThread` is a cooperative script execution context, not an OS thread
(`doom3_orig/doom3/neo/game/script/Script_Thread.h`). Each thread owns:

- an `idInterpreter` with a 64-frame call stack and a 6 KB local stack;
- `waitingForThread`, `waitingFor`, `waitingUntil` state;
- a `spawnArgs` dictionary used by thread spawn helpers;
- a thread number and name;
- save/restore state.

Threads live in a global `idList<idThread*>` and are saved as part of the game object list
(`idGameLocal::Save`, `Game_local.cpp:438`). `idThread::Execute` sets
`currentThread`, runs the interpreter until it blocks or returns, then posts
`EV_Thread_Execute` for the next slice (`Script_Thread.cpp:646`). `idThread::Save` writes the
thread number, wait state, interpreter stack/locals, spawn args, timing, and manual-control
flag (`Script_Thread.cpp:361`). The comment at `:363` notes that restore expects threads in
the same order; thread numbers are not renumbered on load.

Script can create a thread with `thread func(args)`; the compiler emits `OP_THREAD`
(`Script_Compiler.cpp:1058`) and the new thread starts immediately
(`Script_Interpreter.cpp:975`). `idThread::Start` runs until the first block. A thread can be
terminated by `terminate`, `killthread`, or natural return. `wait`, `waitFrame`, `waitFor`,
and `waitForThread` are the cooperative yield points.

Actors use a **persistent manual-control thread**. `idActor::ConstructScriptObject` creates
`scriptThread`, calls `ManualControl`, and calls the object's `init`
(`Actor.cpp:1242`). `idActor::UpdateScript` then runs it each frame, with a 20-iteration cap
to prevent a state-change loop (`Actor.cpp:1327`). The AI's `setState` sequence is therefore
not a new thread per state; it is a function call on the same actor thread. This is why
script state machines can be cheap, and why a badly written state function can stall an
actor for a frame.

The interpreter's multi-frame event support (`BeginMultiFrameEvent` /
`EndMultiFrameEvent`, `Script_Interpreter.cpp:809`) lets an event pause a thread across
frames while remaining inside the event call. It is used by engine code for long actions;
script itself does not declare multi-frame events.

---

## 4. Binding script to entities

### 4.1 `entityDef` → `scriptobject`

`idEntity::Spawn` reads the `scriptobject` key from `spawnArgs`
(`Entity.cpp:444`, `:551`). If present, `idScriptObject::SetType` looks up the type in the
global program, requires it to inherit `object`, allocates memory for its fields, and zeroes
it (`Script_Program.cpp:949`). `ConstructScriptObject` then calls the object's `init`
method in a thread (`Entity.cpp:3097`; actors override this at `Actor.cpp:1242`).

The script object's fields are ordinary script fields and are saved with the entity
(`idScriptObject::Save`, `Script_Program.cpp:900`). `idScriptVariable<T>` is the C++ side of
the contract: it resolves a field by name and type and caches a pointer
(`Script_Program.h:190`). A missing or mistyped field is a fatal `gameError`
(`Script_Program.h:219`).

This is the main data-driven extension point:

```text
entityDef my_monster_variant
{
    "inherit"       "monster_base"
    "spawnclass"    "idZombie"
    "scriptobject"  "my_zombie_variant"
    "model"         "models/monsters/zombie/zombie"
    "health"        "250"
}
```

`spawnclass` must name an existing registered C++ class; `spawnfunc` can instead name a
script function (`Game_local.cpp:3041`, `:3066`). `inherit` copies defaults from another
entityDef, with local keys winning (`DeclEntityDef.cpp:93`). `classname` is set
automatically to the def name (`DeclEntityDef.cpp:90`).

### 4.2 Actor state machines

`idActor::SetState` resolves a function by name in the entity's script object and calls it
on the actor's persistent thread (`Actor.cpp:1296`). The AI's state transitions are driven
by `setState("state_X")`; `state_X` is just an object method. There is no fixed enum of
states in C++.

The C++ side still owns the **state inputs and outputs**. `idAI` links required script
booleans by name at spawn: `AI_TALK`, `AI_DAMAGE`, `AI_PAIN`, `AI_SPECIAL_DAMAGE`,
`AI_DEAD`, `AI_ENEMY_VISIBLE`, `AI_ENEMY_IN_FOV`, `AI_ENEMY_DEAD`, `AI_MOVE_DONE`,
`AI_ONGROUND`, `AI_ACTIVATED`, `AI_FORWARD`, `AI_JUMP`, `AI_ENEMY_REACHABLE`, `AI_BLOCKED`,
`AI_OBSTACLE_IN_PATH`, `AI_DEST_UNREACHABLE`, `AI_HIT_ENEMY`, `AI_PUSHED`
(`ai/AI.h:396-414`, `ai/AI.cpp:1166-1183`). A custom AI script object must declare all of
these fields with the right types or the game aborts at spawn. This is the real cost of
"script-only AI": you get a state machine, but its sensor/actuator interface is fixed.

Actors also link animation-state fields via `idAnimState::SetState`
(`Actor.cpp:149`); weapons and players have their own required field sets (§4.3).

### 4.3 Weapon and player contracts

Weapons are not spawned as ordinary script-object entities. `idWeapon` deliberately returns
`false` from `ShouldConstructScriptObjectAtSpawn` and constructs its object later
(`Weapon.cpp:206`, `:1724`). The weapon entityDef must contain `weapon_scriptobject`
(`Weapon.cpp:977`). The script object must declare seven `boolean` fields:
`WEAPON_ATTACK`, `WEAPON_RELOAD`, `WEAPON_NETRELOAD`, `WEAPON_NETENDRELOAD`,
`WEAPON_NETFIRING`, `WEAPON_RAISEWEAPON`, `WEAPON_LOWERWEAPON`
(`Weapon.h:158-164`; linked at `Weapon.cpp:986`). C++ raises these flags; the script reads
and clears them, then performs the animation/attack sequence. Weapon state functions are
looked up by name from `weaponState` (`Weapon.cpp:1532`).

The player script object is similarly a contract: `Player.cpp:1137` links 19 `AI_*` boolean
fields (`AI_FORWARD`, `AI_BACKWARD`, `AI_STRAFE_LEFT`, `AI_STRAFE_RIGHT`, `AI_ATTACK_HELD`,
`AI_WEAPON_FIRED`, `AI_JUMP`, `AI_DEAD`, `AI_CROUCH`, `AI_ONGROUND`, `AI_ONLADDER`,
`AI_HARDLANDING`, `AI_SOFTLANDING`, `AI_RUN`, `AI_PAIN`, `AI_RELOAD`, `AI_TELEPORT`,
`AI_TURN_LEFT`, `AI_TURN_RIGHT`). A replacement player script must preserve them.

### 4.4 Other script bindings

- **Map scripts:** `idWorldspawn::Spawn` compiles `<map>.script` and starts its `main`
  (`WorldSpawn.cpp:57-80`). It also starts every function named by a `call*` key on
  worldspawn (`WorldSpawn.cpp:88`).
- **`spawnfunc`:** an entityDef can start a global script thread instead of a C++ class
  (`Game_local.cpp:3066`). This is how data-only custom entities are usually built: the
  script calls `spawn()` on an existing class, configures it, and returns.
- **`callFunction`:** a script can call a named method on its own script object with one
  argument (`Entity.cpp:4526`). This is dynamic dispatch inside the object, not a way to
  call arbitrary C++.
- **`$name`:** the compiler resolves `$entityname` to an entity constant at compile time
  (`Script_Compiler.cpp:1257`). This is the main way to reference map entities in script.
- **GUI `runScript`:** a GUI can start a global script function by name
  (`Entity.cpp:3374`). It can also issue a fixed command set (`activate`, `play`,
  `setkeyval`, `setshaderparm`, etc.). This is a UI-to-script bridge, not a general RPC.

---

## 5. What the original pak scripts cover (unverified)

**Verified from the engine:** the runtime expects `script/doom_defs.script`,
`script/doom_main.script`, `doom_main()`, and a per-map `<map>.script` with `main()`
(§1.1, §4.4). It also expects every type named by `scriptobject` or
`weapon_scriptobject` to have been compiled by then.

**Not in this repository:** retail Doom 3 game data. The following coverage is from public
Doom 3 data layout and engine knowledge; treat all filenames and contents as unverified
until a pk4 is available.

| Retail file(s) | Expected coverage |
|---|---|
| `script/doom_defs.script` | `scriptEvent` declarations for native events; the compiler force-includes it |
| `script/doom_main.script` | global entry `doom_main()`, map cycle, game state, difficulty, player spawn, global helpers; likely `#include`s base AI/weapon scripts |
| `script/ai_base.script`, `script/ai_monster_base.script` | shared AI state machine: senses, enemy tracking, movement, attacks, pain/death, chatter, animation states |
| `script/ai_<monster>.script` | per-monster state overrides, attack patterns, animation names, sounds, chatter, special behaviors |
| `script/weapon_base.script`, `script/weapon_<name>.script` | weapon state machine: raise/lower, attack, reload, ammo, muzzle flash, projectile/melee, skin updates |
| `script/player.script` | player script object: input flags, weapon selection, PDA/cinematic transitions, level triggers |
| `script/moveable_base.script`, `script/func_*.script`, `script/trigger_*.script`, `script/target_*.script`, `script/security_camera.script`, `script/lights.script` | map-object behaviors composed from entity events |
| `maps/<map>.script` | per-map sequences, spawns, cinematics, objectives, one-shot set pieces |

The practical implication: retail already demonstrates that a large game can be built on
this event vocabulary. A total-conversion mod can do the same, but it inherits the same
fixed event set and fixed script-object contracts.

---

## 6. The declaration system as a data-driven modding surface

`idDeclManager` registers decl types and folders (`doom3_orig/doom3/neo/framework/DeclManager.cpp:806`,
`doom3_orig/doom3/neo/game/Game_local.cpp:283`). Decls are text files parsed by `idLexer`; they are
looked up by type and name, often lazily on first use. Parse errors generally call
`MakeDefault`, so a malformed decl may not stop the game; check the console and
`decl_show`.

| Decl type | Folder / extension | Role | Modding value |
|---|---|---|---|
| `entityDef` | `def/*.def` | key/value dictionary for an entity class | **primary gameplay surface** |
| `mapDef` | usually map metadata | map-list/server metadata (`idDeclEntityDef` subclass) | server map lists |
| `fx` | `fx/*.fx` | declarative FX actions: particles, lights, sounds, shakes, decals | visual feedback |
| `particle` | `particles/*.prt` | staged particle systems with tables, paths, fade, orientation | visual feedback |
| `articulatedFigure` | `af/*.af` | bodies, joints, constraints, friction, self-collision | ragdolls/physics |
| `skin` | `skins/*.skin` | material remaps per model, optional `model` restriction | cosmetic variants |
| `pda` | `newpdas/*.pda` | PDA entry with emails/videos/audio/security | narrative/data |
| `email`, `video`, `audio` | referenced by PDA | narrative assets | narrative/data |
| `table` | any decl file | float lookup table with `snap`/`clamp` | curves for materials/particles/GUI |
| `material` | `materials/*.mtr` | render material | visuals |
| `sound` | `sound/*.sndshd` | sound shader | audio |
| `model` | `.def` or model decl | model/joint/surface metadata | animation/cosmetics |

### 6.1 `entityDef`

`idDeclEntityDef::Parse` (`DeclEntityDef.cpp:56`) is intentionally schemaless: it reads
quoted key/value pairs, auto-sets `classname`, and processes `inherit*` keys by copying
defaults from other entityDefs (`:93-118`). It precaches referenced media through
`game->CacheDictionaryMedia` (`:128`). There is no validation of key names; a typo is
silently ignored until a C++ `spawnArgs.Get*` call looks for it.

Key groups that matter for gameplay:

- spawn/identity: `classname`, `spawnclass`, `spawnfunc`, `scriptobject`, `inherit`,
  `model`, `health`, `mass`, `skin`, `size`, `mins`, `maxs`, `solid`, `clipmodel`
- AI: `use_aas`, `attack_accuracy`, `attack_cone`, `melee_range`, `turn_rate`,
  `projectile_*`, `def_projectile`, `chatter_*`, `bone_*`, `eye_*`, `fly_*`, `af_*`
- weapon: `weapon_scriptobject`, `ammoType`, `ammoRequired`, `clipSize`, `lowAmmo`,
  `def_projectile`, `def_melee`, `def_ejectBrass`, `gui`, `model_world`, `snd_*`,
  `muzzle_*`, `flash*`, `zoomFov`, `berserk`, `hide_*`
- presentation: `snd_*`, `fx_*`, `mtr_*`, `gui*`, `shaderParm*`, `color`
- map behavior: `target*`, `bind*`, `call*`, `trigger*`, `hide`, `neverDormant`,
  `networkSync`

`spawnclass` selects a C++ class; `spawnfunc` selects a script function. If neither is
present, spawning fails (`Game_local.cpp:3078`). This is the main fork in the road:
`spawnclass` can only name an existing native class, while `spawnfunc` can start arbitrary
script that composes existing classes and events.

### 6.2 FX, particles, skins, AF, PDA, tables

- **FX** (`DeclFX.cpp:411`): a sequence of actions, each with `delay`, `duration`,
  `restart`, `name`, `fire`, `random`, `light`, `shake`, `sound`, `particle`, `decal`,
  `offset`, `axis`, `bindParticles`, etc. It is playback data, not logic.
- **Particles** (`DeclParticle.cpp:423`, stage parser at `:216`): multiple stages with
  material, count, life, distribution/direction/speed/orientation parms, gravity, color,
  fade, animation frames, custom path, bounds expansion. `idParticleParm` can reference an
  `idDeclTable` (`DeclParticle.cpp:206`).
- **Skins** (`DeclSkin.cpp:56`): `model <name>` restrictions plus `from to` material
  remaps, `*` wildcard. Cosmetic only.
- **Articulated figures** (`DeclAF.cpp:1430`): `settings`, `body`, `fixed`,
  `ballAndSocketJoint`, `universalJoint`, `hinge`, `slider`, `spring`; bodies have shape,
  density, friction, contents, self-collision, inertia. Used by ragdoll physics, not AI.
- **PDA** (`DeclPDA.cpp:64`): name/fullname/icon/id/post/title/security plus lists of
  `pda_email`, `pda_audio`, `pda_video`; emails/videos/audio have their own parsers. Pure
  narrative data.
- **Table** (`DeclTable.cpp:112`): a flat float array with optional `snap`/`clamp`; used by
  materials, particles, and GUI. It is not a general data structure and is not exposed to
  `.script`.

### 6.3 Reload behavior

`reloadDecls` reloads existing decl files; the comment at `DeclManager.cpp:1625` explicitly
says it will not find newly created files. `reloadScript` shuts the map down and recompiles
the default script (`SysCmds.cpp:141`), then errors out so the user restarts the map. There
is no in-place script hot reload. For iteration, expect a map restart for script changes and
at least a decl reload (or restart) for new files.

---

## 7. GUI script (`neo/ui/GuiScript.cpp`)

GUI script is a separate, much smaller imperative language attached to `.gui` windows. It
is not the `.script` bytecode VM and does not share its types.

`idGuiScript::Parse` reads a command name and parameters; `idGuiScriptList::Execute` runs
commands in order and supports `if`/`else` on a GUI register (`GuiScript.cpp:410`). The
complete command set is in `commandList` (`GuiScript.cpp:246-259`):

| Command | Effect |
|---|---|
| `set <var> <value>` | set a GUI/window var; `set cmd ...` appends to the window's command string |
| `setFocus <window>` | focus a child window |
| `endGame` | end the game |
| `resetTime [window]` | reset GUI time |
| `showCursor <0/1>` | cursor visibility |
| `resetCinematics` | reset cinematic state |
| `transition <var> <from> <to> <time> [accel] [decel]` | animate a rect/vec4 var |
| `localSound <sound>` | play a local sound |
| `runScript <function>` | start a global script function |
| `evalRegs` | force GUI register evaluation |

GUI variables are `gui::` state, window vars, `$` localized strings, and condition registers.
There are no loops, arrays, user functions, or arithmetic statements beyond GUI expressions.
The command string produced by `set cmd` / `runScript` is handled by
`idEntity::HandleGuiCommands` (`Entity.cpp:3332`), which supports `activate`, `runScript`,
`play`, `setkeyval`, `setshaderparm`, `close`, `print`, plus game-specific commands
(e.g. player `addhealth`, elevator `changefloor`). This is a useful UI trigger surface, but
it is not a general scripting escape hatch.

---

## 8. How much can be done without changing C++

### 8.1 Data-only / script-only capabilities

| Capability | Mechanism | C++ change? |
|---|---|---|
| New monster variant, stats, sounds, skins, drops, projectiles | `entityDef` + `scriptobject` + `inherit` | No |
| New AI state machine/attack patterns | object `state_*` methods + `setState` + existing AI events | No |
| New weapon behavior | `weapon_scriptobject` + `WEAPON_*` flags + existing weapon events | No |
| New item/pickup behavior | existing item class + entityDef + script object | No |
| Map logic, cinematics, spawn waves, objectives | `<map>.script` + entity events + `$name` | No |
| New HUD/menu flow | `.gui` + `runScript` + entityDef `gui*` keys | No |
| New FX/particles/skins/ragdolls/PDA entries | decls | No |
| New global helper functions | `.script` + `#include` from `doom_main` or map script | No |
| New entity built from existing classes | entityDef `spawnfunc` + `spawn()` + script | No |
| New native event, class, net message, save field | — | **Yes** |
| New perception query, spatial iterator, path query | — | **Yes** (unless an existing event covers it) |
| New physics/collision/render pass/UI primitive | — | **Yes** |
| Arrays, dynamic lists, maps, string parsing | — | **Yes** (or encode in fixed fields/strings) |
| Reliable custom persistent state | script fields (fixed, saved) or `persistentLevelInfo` | No for simple primitives; **Yes** for structured state |

### 8.2 The hard limits that matter for AI

1. **Fixed event vocabulary.** No new sensor or action without C++. Existing AI events cover
   a lot (enemy, visibility, movement, melee, missiles, ragdoll), but not arbitrary
   queries such as "all entities within radius", "navmesh path to point", "inventory
   contents", or "line-of-sight to arbitrary point" beyond `trace`/`canSee`.
2. **Fixed script-object layout.** Actors, players, and weapons require specific field
   names/types (`AI_*`, `WEAPON_*`). You can add fields, but you cannot remove or retype the
   contract without C++.
3. **No arrays/containers.** A script object can hold a fixed set of scalars, vectors,
   strings, and entity references. There is no list of targets, no queue, no map, no
   dynamic blackboard. `idList` is C++-only.
4. **No iteration.** There is no "for each entity" event. You can use `getEntity(name)`,
   `getTarget(index)`, `randomTarget`, `findEnemy`, `randomPath`, and `getNextKey`, but not
   a general spatial query.
5. **No string parsing.** You cannot parse a data string into fields. Data must be split
   into separate keys or encoded numerically.
6. **No persistent custom state beyond fields/persistent args.** Script object fields are
   saved automatically; `setPersistantArg`/`getPersistant*` use a global string dictionary
   (`persistentLevelInfo`, saved at `Game_local.cpp:485`, restored at `:1304`), but it is
   global and untyped.
7. **Save compatibility.** `idScriptObject::Restore` rejects a type-size change
   (`Script_Program.cpp:936`), and `idProgram::CalculateChecksum` compares emitted
   statements (opcode + varDef numbers + line/file) on load
   (`Script_Program.cpp:2002`). Changing script structure can invalidate saves; changing
   only a string literal may not be detected. Plan mod versions and save migration.
8. **No hot reload.** Script changes require a map shutdown/reload; new decl files require a
   restart or at least a reload of existing files (`SysCmds.cpp:141`,
   `DeclManager.cpp:1625`).
9. **Trust.** `.script` can call `setcvar`, `spawn`, `remove`, `error`, damage events, and
   `runScript` through GUI. It is not a security boundary. Treat downloaded scripts as
   trusted gameplay code, or gate them (§9).

---

## 9. Proposed AI modding layer

### 9.1 Goal and boundary

The layer is a **one-time C++ investment**. Once present, AI behavior modules are data
files (Lua/WASM/JSON), entityDefs add an `ai_module` key, and no per-mod C++ rebuild is
needed. It does not replace `.script`; it adds a second, sandboxed decision layer on top of
the existing event surface.

The layer must:

- observe a deterministic per-tick snapshot;
- emit only whitelisted actions, in a deterministic order;
- never call arbitrary script or C++ from the AI module;
- be server-authoritative and save/load-safe;
- fail closed if the module, event, or argument is invalid;
- fit the web target (no external process in the browser).

All C++ work lands in the game module; `doom3_orig/doom3/neo/game/Game.def` exports only `GetGameAPI`,
so this is a game-DLL rebuild, not a new engine executable.

### 9.2 Runtime options

| Option | Integration | Determinism | Isolation | Native | Web/WASM | Verdict |
|---|---|---|---|---|---|---|
| **Lua 5.4** embedded | Low–medium: vendor Lua, small C API binding | Good if no wall clock/`math.random`, sorted iteration, float rounding | Good with a restricted environment and instruction hook | Yes | Yes (Lua compiles to WASM) | **Recommended default** |
| **WASM** (wasm3 / wasmtime) | Medium–high: runtime + host ABI + fuel metering | Strong: no ambient I/O, deterministic memory, fuel | Strongest for untrusted modules | Yes | Yes (wasm3) | **Recommended for untrusted/web** |
| **Python** embedded | High: CPython DLL, GIL, version coupling | Weak: GC, hash order, C extensions, libm | Weak unless subinterpreter + audit hooks | Yes | Very hard | External tooling only |
| **External process + JSON IPC** | Low engine code, high ops | Weak unless synchronous and fixed-tick | Strong OS isolation | Yes | **No** (browser cannot spawn) | Authoring/offline only |

If an external process is used anyway, make it a strict request/response sidecar: local
socket or named pipe (never a listening network port), one JSON object per line, a
`hello` handshake carrying protocol version and module hash, one `tick` request per fixed
simulation tick, one `actions` reply, and a 2–5 ms budget. Authenticate the local endpoint,
validate every action in the bridge, and treat timeout/crash as a deterministic no-op. This
is useful for offline authoring, analysis, or a debug observer; it is not suitable for
shipping gameplay, and it cannot run in the browser/WASM target.

Recommendation: implement the ABI once, with a Lua backend first and a WASM backend behind
the same interface. Do not make an external process part of gameplay; it adds latency,
platform coupling, and non-determinism, and it cannot ship to the web target.

### 9.3 Minimal architecture

```text
┌─────────────────────────────┐
│ AI module (Lua/WASM)        │  per-actor environment / coroutine
│  think(state) -> actions[]  │  no I/O, no wall clock, no raw pointers
└──────────────┬──────────────┘
               │ snapshot / action list (handles + scalars)
┌──────────────▼──────────────┐
│ AIBridge (C++ in game DLL)  │
│  catalog + allowlist        │
│  snapshot builder           │
│  action validator/dispatcher│
│  save/load + RNG            │
└──────────────┬──────────────┘
               │ idEventDef / idClass::ProcessEventArgPtr
┌──────────────▼──────────────┐
│ existing game (idAI, idEntity, idWeapon, ...) │
└─────────────────────────────┘
```

Proposed files (illustrative, not created here):

```text
doom3_orig/doom3/neo/game/ai_bridge/AIBridge.h
doom3_orig/doom3/neo/game/ai_bridge/AIBridge.cpp
doom3_orig/doom3/neo/game/ai_bridge/AIEventCatalog.h
doom3_orig/doom3/neo/game/ai_bridge/AIEventCatalog.cpp
doom3_orig/doom3/neo/game/ai_bridge/AISandboxLua.cpp
doom3_orig/doom3/neo/game/ai_bridge/AISandboxWasm.cpp
```

Use `spawnId` (`gameLocal.GetSpawnId(ent)`, `Game_local.cpp:4262`) as the stable AI handle,
not `entityNumber` (which is reused). Build snapshots from `gameLocal.entities`,
filter by distance/team/visibility, and sort by `entityNumber` for determinism.

### 9.4 Hook points

| Hook | File:line | Purpose |
|---|---|---|
| `idInterpreter::CallEvent` | `Script_Interpreter.cpp:683` | intercept script→native event calls; enforce policy for untrusted `.script` |
| `idInterpreter::CallSysEvent` | `Script_Interpreter.cpp:854` | same for `sys.*` events |
| `idCompiler::ParseEventDef` | `Script_Compiler.cpp:2398` | compile-time allowlist/denylist for `scriptEvent` declarations |
| `idThread::Execute` | `Script_Thread.cpp:646` | per-thread instruction/time budget and abort |
| `idInterpreter::Execute` | `Script_Interpreter.cpp:936` | opcode budget/watchdog if the AI drives script |
| `idGameLocal::RunFrame` | `Game_local.cpp:2157` | one deterministic AI tick boundary |
| `idEvent::ServiceEvents` | `Event.cpp:460` | event queue ordering; actions must not race it |
| `idProgram::CompileText` | `Script_Program.cpp:1772` | reject AI-generated source; not a sandbox |
| `idEntity::Spawn` | `Entity.cpp:444` | attach bridge to entities with `ai_module` |
| `idScriptObject::Save/Restore` | `Script_Program.cpp:900`, `:919` | persist script-field state |
| `idInterpreter::Save/Restore` | `Script_Interpreter.cpp:53`, `:103` | persist interpreter state |
| `idGameLocal::Save/Restore` | `Game_local.cpp:447`, `:1224` | persist bridge blob; program checksum |

Do **not** hook `idClass::ProcessEventArgPtr` as the policy point. It is the final
dispatcher and also handles engine-internal calls; it lacks the script/thread context needed
to decide whether a call is mod-originated. Hook `CallEvent`/`CallSysEvent` instead, and
route AI actions through a dedicated `AIBridge::Dispatch` that reuses the same marshalling
code.

### 9.5 Event catalog and whitelist

Build the catalog at startup:

```cpp
for ( int i = 0; i < idEventDef::NumEventCommands(); ++i ) {
    const idEventDef *ev = idEventDef::GetEventCommand( i );
    // store name, format, return type, eventnum
}
```

Resolve allowlist entries by name with `idEventDef::FindEvent`; fail closed if a required
event is missing. Enforce at three points: compile-time (`ParseEventDef`), script runtime
(`CallEvent`/`CallSysEvent`), and AI action dispatch (`AIBridge::Dispatch`).

Suggested policy classes:

| Class | Examples | Policy |
|---|---|---|
| **Observe** | `getOrigin`, `getAngles`, `getLinearVelocity`, `getSize`, `distanceTo`, `distanceToPoint`, `canSee`, `getEnemy`, `getEnemyPos`, `getEnemyEyePos`, `enemyRange`, `moveStatus`, `facingIdeal`, `getHealth`, `ammoInClip`, `ammoAvailable`, `totalAmmoCount`, `clipSize`, `getCurrentWeapon`, `getButtons`, `getMove`, `getViewAngles` | allow; rate-limit; read-only |
| **Act** | `moveToPosition`, `moveToEntity`, `slideTo`, `stopMove`, `turnTo`, `turnToPos`, `turnToEntity`, `faceEnemy`, `faceEntity`, `setEnemy`, `clearEnemy`, `setState`, `waitAction`, `attackMelee`, `attackMissile`, `launchMissile`, `createMissile`, `muzzleFlash`, `startSound`, `startSoundShader`, `startFx`, `setColor`, `setShaderParm`, `hide`, `show`, `allowDamage`, `ignoreDamage`, `becomeRagdoll`, `stopRagdoll` | allow with argument validation and per-actor budget |
| **Guarded** | `spawn`, `remove`, `setHealth`, `setOrigin`, `setLinearVelocity`, `setAngularVelocity`, `setSize`, `bind`, `setModel`, `setSkin`, `setKey`, `setSpawnArg`, `setPersistantArg`, `radiusDamage`, `directDamage`, `activate`, `activateTargets`, `startFx`, `setNeverDormant` | allow only with an explicit capability and caps; default deny |
| **Deny** | `setcvar`, `getcvar`, `error`, `killthread`, `terminate`, `callFunction`, `script`-compiled code, `runScript`, arbitrary console commands | always deny for untrusted AI |
| **Deny (noise)** | `debugLine`, `debugArrow`, `debugCircle`, `debugBounds`, `drawText` | allow only in developer builds; otherwise drop |

Rationale:

- `setcvar`/`getcvar` change global engine state and can leak server config.
- `error` is fatal in many paths; a bad AI module must not be able to crash the server.
- `callFunction`/`runScript` re-enter arbitrary script, bypassing the whitelist and budget.
- `spawn`/`remove`/`setOrigin`/damage events are powerful gameplay actions and need caps.
- `getTime` uses `realClientTime` (`Script_Thread.cpp:1079`), not simulation time; do not
  expose it to AI. Put `gameLocal.time` and the tick number in the snapshot instead.
- `random` advances the global `gameLocal.random` (`Script_Thread.cpp:1064`); let the bridge
  own its RNG so engine RNG consumption is not perturbed by AI.

Validate every argument: finite floats, bounded vectors (map bounds), string length
`< MAX_STRING_LEN`, valid `spawnId`, entity alive, decl exists, state name exists, damage
def in an allowlist, counts/time within caps. Rate-limit actions per actor per tick and
per second. Drop, do not clamp, on invalid input; log with actor id and event name.

### 9.6 Snapshot and action ABI

Keep the ABI scalar/Handle-only:

```text
ActorState {
    u32 spawnId; u32 entityNumber; u32 defHash;
    f32 origin[3]; f32 angles[3]; f32 velocity[3]; f32 health;
    u32 enemySpawnId; f32 enemyOrigin[3]; f32 enemyDistance;
    u32 flags; u32 weaponState; u32 ammoClip; u32 ammoAvail;
}

Action {
    u32 actorSpawnId; u32 eventId; u8 argCount;
    Arg args[8];   // type-tagged: f32 / vec3 / string[128] / entity handle
}
```

The module returns a bounded action list per actor. The bridge resolves `eventId` from the
catalog, checks the whitelist, validates args, then dispatches. For actions that return
values, do not use the event return path; read the value from the snapshot instead. For
actions that need to be delayed, schedule them through `idEvent::Alloc` + `Schedule`
(`Event.cpp:238`, `:382`), which copies strings/entity refs and participates in save/load.

Reuse the `idInterpreter::CallEvent` marshalling logic rather than duplicating it. Extract a
small `idEventMarshal::Dispatch(idEntity*, const idEventDef*, const Arg*, int)` used by both
`CallEvent` and `AIBridge::Dispatch`. Do not build `int data[8]` by hand in multiple places;
the string/entity-pointer cases are easy to get wrong.

### 9.7 Sandbox

For Lua:

- one `lua_State` per game, one environment table per actor; no mutable globals shared
  across actors;
- remove `io`, `os`, `package`, `debug`, `load`, `loadstring`, `dofile`, `require`;
- replace `math.random` with a bridge RNG; do not expose `os.time`/`os.clock`;
- instruction hook (`lua_sethook`) for a per-tick budget; allocator hook for a memory cap;
- no raw `idEntity*`; only integer handles and bridge functions;
- sort table keys before iteration; do not rely on `pairs` order;
- quantize/round floats at the ABI boundary to the engine's 32-bit floats.

For WASM:

- no WASI imports except a host `tick`/`event`/`rand`; deny filesystem, clocks, sockets;
- fuel metering (wasmtime) or an instruction cap (wasm3); memory maximum;
- single-threaded, no shared memory; canonical NaN handling;
- pin the module hash in the save and in the server's mod manifest.

For Python: if used at all, run it out-of-process for authoring/analysis, not in the game
loop. Embedded CPython is too large, too version-coupled, and too non-deterministic for
save/netcode work.

### 9.8 Determinism, save/load, and netcode

**Tick.** Run the AI exactly once per server frame at one fixed point. Do not assume
`ServiceEvents` is drained before entity thinking: `idGameLocal::RunFrame` runs entity
`Think()` first and calls `idEvent::ServiceEvents()` afterwards
(`Game_local.cpp:2157`, `:2295`). Choose an explicit point (for example, immediately after
`ServiceEvents`, accepting a one-frame action delay, or before entity thinking and dispatch
actions synchronously), document it, and never change it between versions. A fixed 10–20 Hz
decimated tick is also valid if movement/attack latency tolerates it. Use `gameLocal.time`
and a monotonic tick counter, not wall clock. Sort actors by `spawnId` and actions by
`(tick, spawnId, actionIndex)` before dispatch. Never run two AI modules in parallel.

**RNG.** Use a bridge-owned PCG/xorshift seeded from the map seed and actor `spawnId`.
Save its state. Do not consume `gameLocal.random` from AI; engine code uses it for spawns,
effects, and damage, and interleaving changes results. `getTime` is not a simulation clock
(`Script_Thread.cpp:1079`); do not expose it.

**Save/load.** Store a versioned bridge blob in `idGameLocal::Save` near `program.Save`
(`Game_local.cpp:447`): `bridgeVersion`, module hash, config hash, RNG state, per-actor
blackboard, pending actions. Restore it near `program.Restore` (`Game_local.cpp:1224`) and
verify the module/config hash against the current one. If it differs, fail the load or
reset AI explicitly; never silently continue with a different policy.

`idProgram::CalculateChecksum` covers emitted statements (opcode + varDef numbers +
line/file), not a full source hash (`Script_Program.cpp:2002`). `idScriptObject::Restore`
also rejects a changed type size (`Script_Program.cpp:936`). Treat script and AI module
changes as save-incompatible unless you provide a migration. `idThread::Save` expects
threads in the same order on restore (`Script_Thread.cpp:361`); do not create/remove
engine threads from AI without accounting for that.

**Netcode.** Run the bridge only when `!gameLocal.isClient`. Client game code explicitly
does not support entity events ("entity events don't work and should not be issued",
`Game_network.cpp:38`). AI decisions must be server-authoritative; clients receive the
resulting entity state through normal snapshot/`networkSync` and C++ network events. Do
not send AI internal state or add a custom network message without C++. If a client-side
cosmetic AI is desired, keep it read-only and never let it issue gameplay events.

**Timeout/failure.** If a module exceeds its budget, returns malformed actions, or crashes,
apply a deterministic no-op for that tick and log. Do not substitute a wall-clock timeout
that changes the action set; use a fixed instruction budget so replays and saves match.

### 9.9 Implementation phases

1. **Catalog + policy (no behavior change).** Enumerate `idEventDef`s, load the allowlist,
   log unknown/denied events, and add the bridge cvars (`ai_enable`, `ai_module`,
   `ai_tick_hz`, `ai_budget_ms`). This is testable without AI.
2. **Read-only snapshot.** Build per-actor observations from existing engine state; verify
   determinism and cost. No actions yet.
3. **Action dispatch.** Add `AIBridge::Dispatch` using the extracted marshaller; start with
   movement/turn/stop/attack only. Add per-actor rate limits.
4. **Sandbox backend.** Add Lua with a restricted environment and instruction hook; then
   add WASM behind the same ABI.
5. **Persistence and netcode.** Add the save blob, module/config hashes, RNG state, and
   server-only guard. Add replay logs for desync diagnosis.
6. **Hardening.** Fuzz invalid actions, test budget exhaustion, test save/load with a
   changed module, and verify no client-side AI actions are issued.

---

## 10. Evidence index

Primary files:

- Language/compiler: `doom3_orig/doom3/neo/game/script/Script_Compiler.h`,
  `Script_Compiler.cpp`, `Script_Program.h`, `Script_Program.cpp`,
  `Script_Interpreter.h`, `Script_Interpreter.cpp`, `Script_Thread.h`,
  `Script_Thread.cpp`.
- Events/classes: `doom3_orig/doom3/neo/game/gamesys/Event.h`, `Event.cpp`, `Class.h`, `Class.cpp`.
- Binding: `doom3_orig/doom3/neo/game/Entity.cpp`, `Actor.cpp`, `Player.cpp`, `Weapon.cpp`,
  `ai/AI.cpp`, `ai/AI_events.cpp`, `WorldSpawn.cpp`, `Game_local.cpp`, `Game.h`.
- Declarations: `doom3_orig/doom3/neo/framework/DeclManager.cpp`, `DeclEntityDef.cpp`, `DeclFX.cpp`,
  `DeclParticle.cpp`, `DeclAF.cpp`, `DeclSkin.cpp`, `DeclPDA.cpp`, `DeclTable.cpp`.
- GUI: `doom3_orig/doom3/neo/ui/GuiScript.cpp`, `doom3_orig/doom3/neo/ui/Window.cpp`,
  `doom3_orig/doom3/neo/game/Entity.cpp` (`HandleGuiCommands`).
- Module export: `doom3_orig/doom3/neo/game/Game.def` (`EXPORTS GetGameAPI`).

Not present in this checkout: any retail `.pk4`, `script/doom_defs.script`,
`script/doom_main.script`, `ai_*.script`, `weapon_*.script`, or map scripts. The only file
under `doom3_orig/doom3/base/` is a 14-byte empty `default.cfg`. All retail-script coverage claims in
§5 are therefore unverified.
