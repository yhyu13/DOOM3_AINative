# 07 — Networking & Multiplayer Assessment (DOOM 3 GPL, idTech4)

**Scope.** Read-only investigation of the networking and multiplayer stack of `doom3_orig/doom3/neo`
(the 2011-11-22 GPL source release of DOOM 3 1.3.1), the demo game data in
`_data/doom3-demo/demo00.pk4`, and the existing Windows x64 port at
`.kilo/worktrees/feat-1-win11-amd64-native-build/targets/win11-amd64/doom3/neo`. Nothing was built
and nothing was run for this note; all statements are either read from source/data or labelled
**judgement**. Measured 2026-10-27.

**Questions answered.** What online features DOOM 3 has; whether LAN play works without CD keys;
how much 64-bit work the network path needs; whether the widened event-argument slots break the
wire format; whether a dedicated server is possible; and a minimal, verifiable path to two
instances joining each other on one PC.

**Method.** PowerShell 5.1 + `System.IO.Compression` for the pk4 inventory, `Select-String` for the
source, `git diff --no-index` between the pristine tree and the port copy. The evidence commands
are in Appendix A so the numbers can be reproduced. Where the demo data and the engine disagree,
that is called out explicitly.

---

## 1. Headline findings

| # | Finding | Evidence | Consequence |
|---|---|---|---|
| 1 | Multiplayer is **fully implemented in the engine**: deathmatch, team deathmatch, tournament and last-man-standing, 32 clients max | `game/MultiplayerGame.h:42-48`, `game/Game_local.h:81`, `framework/async/AsyncNetwork.h:44` | No protocol or game-code writing is required for LAN play |
| 2 | LAN play is **explicitly exempt from CD-key authorisation** | `framework/async/AsyncServer.cpp:1533-1535` | A LAN game needs no key, no master server, no auth traffic |
| 3 | The **internet server browser and CD-key auth depend on id's master server**, whose protocol is long dead | `framework/Licensee.h:57-65`, `framework/async/AsyncNetwork.cpp:59`, `framework/async/AsyncClient.cpp:382-406` | Internet "Play" is dead as shipped; LAN is not |
| 4 | The demo pk4 contains **no multiplayer map**: 3 maps, all single-player, each with **0 `info_player_deathmatch`** | `demo00.pk4` inventory (§2.4) | DM can start only via the `info_player_start` fallback; no real DM gameplay |
| 5 | The demo data also lacks the **MP GUI**: `guis/netmenu.gui` and `guis/mphud.gui` are absent, and the connect path dereferences the netmenu pointer **unguarded** | `framework/async/AsyncClient.cpp:213-214`; pk4 inventory | `connect` will **crash** with demo-only data until a stub GUI or a 2-line guard exists |
| 6 | The port already **compiles and links the whole network stack** (`ws2_32`, `iphlpapi`), and the port's patch does not touch it | `targets/win11-amd64/sources.cmake:90-95,232`; `CMakeLists.txt:97`; port diff (59 files) | Networking is shipped but **never executed**, so it is untested 2011 code in an x64 build |
| 7 | One genuine 64-bit defect sits under everything: **`SOCKET` is stored in `int`** | `sys/win32/win_net.cpp:243,306`; `sys/sys_public.h:435,455` | Must be fixed before networking can be called correct on Win64 |
| 8 | The wire format is **field-by-field and pointer-free**; the event-argument widening does **not** reach the wire | `framework/async/AsyncNetwork.cpp:189-248`, `game/Game_network.cpp:625-660,1027-1081`, `game/Entity.cpp:4723-4760` | x64↔x86 play on the same source is not blocked by the port's event changes |
| 9 | A dedicated server exists **twice**: a compile-time `ID_DEDICATED` exe and a runtime `net_serverDedicated 1` mode | `framework/BuildDefines.h:88,112-118`, `framework/Common.cpp:3002-3014`, `sys/win32/win_main.cpp:1163,1414-1424` | Runtime dedicated needs **zero** build work in the port |
| 10 | **No voice chat (VoIP) exists.** "Voice" is a menu of pre-recorded taunts relayed by index | `game/MultiplayerGame.cpp:3243-3286,3293-3320`; no capture code in `sound/` | Microphone chat is a new feature, not a port fix |

**Answer to the user's headline question: yes — DOOM 3 supports LAN room battles, and the code path
for them is intact.** What is missing in this repository is not the feature but the *data* (an MP
map and two GUI files) and one x64 socket-width fix.

---

## 2. What multiplayer DOOM 3 actually contains

### 2.1 Game modes

`game/MultiplayerGame.h:42-48` defines the base game's mode enum:

```
GAME_SP, GAME_DM, GAME_TOURNEY, GAME_TDM, GAME_LASTMAN
```

The mode is not chosen by a number on the wire. It is a *server-info string* mapped to the enum in
`game/Game_local.cpp:4215-4236` (`idGameLocal::UpdateServerInfoFlags`):

| `si_gameType` (case-insensitive) | enum | Notes |
|---|---|---|
| *(absent)* / `singleplayer` | `GAME_SP` | default |
| `deathmatch` | `GAME_DM` | free-for-all |
| `Tourney` | `GAME_TOURNEY` | 1v1 queue, `currentTourneyPlayer[2]` |
| `Team DM` | `GAME_TDM` | two teams, `teamFragCount` |
| `Last Man` | `GAME_LASTMAN` | forced warm-up + frag limit ≥ 1 (`Game_local.cpp:4226-4235`) |

The 1.3.1 expansion tree adds CTF to the enum (`d3xp/MultiplayerGame.h:47-57`) and maps
`"CTF"` → `GAME_CTF` (`d3xp/Game_local.cpp:4677-4688`), but CTF is behind `#ifdef CTF`, and **`CTF`
is never defined anywhere** in this source drop (no `#define CTF` in any header, project, or
`SConstruct`). CTF code is therefore compiled out and is not a build option today.

Mode selection at runtime: `idSessionLocal::UpdateUI` copies GUI state into cvars, the host button
runs `startMultiplayer` (`framework/Session_menu.cpp:745-808`), which sets `net_LANServer` from the
GUI's `server_type` toggle and `net_serverDedicated` from a checkbox, then buffers `SpawnServer`.

### 2.2 Player counts

| Limit | Value | Where |
|---|---:|---|
| `MAX_CLIENTS` (game side) | 32 | `game/Game_local.h:81` |
| `MAX_ASYNC_CLIENTS` (engine side) | 32 | `framework/async/AsyncNetwork.h:44` |
| `MAX_CHALLENGES` (connection slots before auth) | 1024 | `framework/async/AsyncServer.h:42` |
| GUI cap for an internet **listen** server | 4 clients (`si_maxPlayers ≤ 4`) | `framework/Session_menu.cpp:790-795` |
| GUI cap for a LAN listen server | `min(8, maxclients+1)` | `framework/Session_menu.cpp:785` |
| Dedicated server | up to `si_maxPlayers` = 32 | `si_maxPlayers` is a plain server cvar |

Two practical caveats, both visible in source:

* `si_maxPlayers` is **not** a compiled-in cvar. It is created by the GUI (or by `sets` on the
  console) and must be flagged `CVAR_SERVERINFO` to reach the server info dict
  (`framework/CVarSystem.cpp:576,971-980`). A bare `set si_maxPlayers 4` does not work; `sets` does.
* A server refuses all clients while `si_maxPlayers == 0`
  (`game/Game_network.cpp:253-260`, returning `ALLOW_NOTYET` with `#str_07140`). This is the most
  likely first failure for a console-launched server.

### 2.3 Player-facing features that exist

| Feature | Implementation | Notes |
|---|---|---|
| Text chat, global and team | `game/Game_network.cpp:764-807`, `guis/chat.gui` | `say` / `sayTeam`, `GAME_RELIABLE_MESSAGE_CHAT/TCHAT` |
| Pre-recorded "voice" taunts | `game/MultiplayerGame.cpp:3243-3286`, `3293-3320` | `clientVoiceChat <snd_voc_name>` → server relays an **index**; audio is a sound shader on every client |
| Vote-to-* (map, restart, kick, …) | `game/MultiplayerGame.cpp:2734-2832` | vote state lives in `mpPlayerState_t.vote` (`MultiplayerGame.h:50-55,57-65`) |
| Teams, ready state, warm-up | `MultiplayerGame.h`, `g_spectatorChat` at `MultiplayerGame.cpp:37` | warm-up state machine `gameState` in `MultiplayerGame` |
| Spectating, scoreboard | `guis/spectate.gui`, `guis/scoreboard.gui`; `MultiplayerGame.cpp:159-171` | |
| Kick / rcon | `framework/async/AsyncServer.cpp:2020-2100`, `AsyncNetwork.cpp:105-107` | `rcon` via UDP, password cvar |
| Ping / rate / packet-loss report | `AsyncServer.cpp:2109-2130`, `AsyncClient.cpp:1196` | |
| PK4 auto-download | `AsyncClient.cpp:1440-1560`, `net_clientDownload` | **unavailable in the port**: `ID_ENABLE_CURL=0` and `curl/` is excluded from the build (`targets/win11-amd64/README.md`) |
| PunkBuster | `framework/Session_menu.cpp:1041` only | a menu command string; **no anti-cheat code exists** in the GPL drop — dead |
| Voice chat (microphone / VoIP) | — | **does not exist**; `sound/` has no capture path (only the comment "where is the camera/microphone" at `sound/sound.h:214`) |

### 2.4 What the demo pk4 actually ships

`demo00.pk4` is 483,534,533 bytes, 12,234 entries. Relevant inventory:

**Maps** — three, all single-player demo maps. There is no `maps/game/mp/` directory at all:

| Map file | `info_player_start` | `info_player_deathmatch` | `item_weapon_*` |
|---|---:|---:|---:|
| `maps/game/demo_mars_city1.map` | 2 | **0** | 0 |
| `maps/game/demo_mars_city2.map` | 2 | **0** | 0 |
| `maps/game/demo_mc_underground.map` | 2 | **0** | 0 |

`def/maps.def` still *declares* five retail MP maps (`mapDef game/mp/d3dm1` … `d3dm5`), and
`sound/levels/d3dm5/` exists, but the BSPs/entities are not in the pk4.

**GUIs** — `guis/mpmain.gui`, `guis/mpmsgmode.gui`, `guis/scoreboard.gui`, `guis/chat.gui`,
`guis/spectate.gui`, `guis/msg.gui` are **present**; `guis/netmenu.gui`, `guis/mphud.gui`,
`guis/mainmenu.gui` and `guis/map/loading.gui` are **absent**. The working data set the port's smoke
test uses (`_data/doom3-demo/run2/base/`) adds a loose `guis/mainmenu.gui` (529,866 bytes, a demo
menu: its only `cmd` values are single-player ones such as `startgame game/demo_mars_city1`) plus a
loose `script/weapon_base.script`. Neither contains server-browser wiring.

**Definitions** — `def/mp.def` **is** present and complete enough for MP: MP damage variants
(`damage_*_mp`), MP items/ammo (`item_armor_*_mp`, `ammo_*_mp`), and `entityDef player_doommarine_mp`
with the `snd_voc_*` voice-taunt keys (`def/mp.def:352+`). The MP player class is selected in code
(`game/Game_local.cpp:1833`: `isMultiplayer ? "player_doommarine_mp" : "player_doommarine"`).

Two data-driven consequences:

* **Spawning still works.** With no `info_player_deathmatch`, `RandomizeInitialSpawns` only warns
  (`Game_local.cpp:4099-4130`), and `SelectInitialSpawnPoint` falls back to
  `info_player_start` when `spawnSpots` is empty (`Game_local.cpp:4149-4162`). Both players spawn on
  the same single-player start point — good enough to prove a connection, not to play.
* **Voice taunts do not resolve with demo defs.** `VoiceChat` looks the sound up in
  `player_doommarine` (`MultiplayerGame.cpp:3265`), but the demo's `def/player.def` has **no
  `snd_voc_` keys** (they exist only in `def/mp.def`, on `player_doommarine_mp`). The command prints
  "Voice command not found".

---

## 3. The engine network stack

### 3.1 Layers

| Layer | Files | Lines | Role |
|---|---|---:|---|
| Transport | `sys/win32/win_net.cpp` | 1,200 | Winsock 1.1, UDP `idPort`, `idTCP`, interface enumeration, LAN classification |
| Async networking | `framework/async/Async{Server,Client,Network}.cpp`, `MsgChannel.cpp`, `ServerScan.cpp` | 8,448 in 12 files | connectionless handshake, challenge, channels, fragmentation, snapshots, server list |
| Game binding | `framework/async/NetworkSystem.cpp`, `game/Game_network.cpp` | 215 + 1,753 | thin `networkSystem` indirection; snapshots, usercmds, reliable game messages |
| Game rules | `game/MultiplayerGame.cpp` | 3,410 | modes, teams, votes, chat, scoring, warm-up |
| UI | `framework/Session*.cpp` | — | `startMultiplayer`, `LANScan`, `connect`, CD-key prompts |

The split matters for the estimate: the **transport** is tiny (1,200 lines) and is where the 64-bit
defect is; the **protocol** is 8,448 lines that need no changes; the **game rules** are 3,410 lines
that are architecture-independent but untested on x64.

Constants that define the protocol (`framework/Licensee.h:57-77`, `AsyncNetwork.h:40-57`):

| Constant | Value |
|---|---|
| `ASYNC_PROTOCOL_MAJOR` / `MINOR` | 1 / 41 → version 1.41 |
| `PORT_SERVER` | 27666 |
| `NUM_SERVER_PORTS` (server-side port scan) | 4 → 27666-27669 |
| `MAX_SERVER_PORTS` (client-side broadcast scan) | 8 → 27666-27673 |
| `IDNET_HOST` / `IDNET_MASTER_PORT` | `idnet.ua-corp.com` / 27650 |
| `MAX_MESSAGE_SIZE` | 16,384 |
| `MAX_ENTITY_STATE_SIZE` / `MAX_EVENT_PARAM_SIZE` | 512 / 128 |

### 3.2 Transport

* `Sys_InitNetworking` (`win_net.cpp:637-700`) calls `WSAStartup(MAKEWORD(1,1))` and enumerates
  IPv4 interfaces with `GetAdaptersInfo` into `netint[]` (ip + mask). **Winsock 1.1 is fine on
  Windows 11**; `iphlpapi` is already linked by the port.
* `NET_IPSocket` (`win_net.cpp:243-307`) creates `AF_INET`/`SOCK_DGRAM`/`IPPROTO_UDP`, sets
  `FIONBIO` (non-blocking) and `SO_BROADCAST`, binds to `INADDR_ANY` unless `net_ip` names an
  interface, and — when binding to `PORT_ANY` — queries the assigned port with `getsockname`.
* `net_ip` defaults to `"localhost"` and `net_port` to `0` (`win_net.cpp:40-41`). With
  `net_ip == "localhost"` the socket binds `INADDR_ANY`, so **127.0.0.1 works without any
  configuration**. A server started with `net_port 0` walks 27666, 27667, 27668, 27669
  (`AsyncServer.cpp:105-130`) — which is exactly what makes two servers on one PC possible.
* The client always binds an ephemeral port (`clientPort.InitForPort(PORT_ANY)`,
  `AsyncClient.cpp:111-123`), so a client and a server on one PC never fight over a port.
* Everything is **IPv4 only**. There is no `AF_INET6`, no dual-stack socket, no `getaddrinfo`
  (`Sys_StringToNetAdr` → `gethostbyname` via `Net_StringToSockaddr`, `win_net.cpp:736-746`).
* Packet I/O is `recvfrom`/`sendto` against the caller's buffer (the async layer passes a
  16,384-byte message buffer; `win_net.cpp:529-580,589-613`), plus an optional artificial
  latency/drop shim whose queue is capped at `MAX_UDP_MSG_SIZE` 1400
  (`win_net.cpp:837-845,936-995`, `net_forceLatency`, `net_forceDrop`) that is useful for testing.
* `idMsgChannel` (`MsgChannel.cpp`) adds sequencing, duplicate suppression, reliable-message
  piggybacking, and fragmentation, and compresses with **run-length, zero-based**
  (`MsgChannel.cpp:263,353-358`) — all integer/byte arithmetic, architecture-neutral.

### 3.3 Connection sequence (as implemented)

```
client                                   server
  |  challenge (clientId)          ->     |  ProcessChallengeMessage    AsyncServer.cpp:1484
  |  <- challengeResponse(challenge, serverId, fs_game_base, fs_game)
  |                                       |  if Sys_IsLANAddress(from): authState = CDK_OK   :1533
  |                                       |  else: send "srvAuth" to master                  :1541
  |  connect(clientDataChecksum, challenge, clientId, rate, guid, ...) ->
  |                                       |  ValidateChallenge            :1636
  |                                       |  authState switch (CDK_WAIT / CDK_ONLYLAN / ...)  :1709
  |                                       |  ServerAllowClient (si_maxPlayers, password)      Game_network.cpp:245
  |  <- connectResponse(clientNum, gameInitId, gameFrame, gameTime, serverInfo dict)
  |                                       |  InitClient, gameInitSequence = 1                  :1878
  |  pure checksums (if si_pure) ...      |
  |  usercmd / ping                       |
  |  <- snapshot / gameInit               |
```

Evidence for each step: `AsyncServer.cpp:1484-1553` (challenge), `1673-1890` (connect),
`framework/async/AsyncClient.cpp:1050-1130` (client side); `ServerAllowClient` at
`game/Game_network.cpp:245-280`.

The server enforces: protocol version, `clientDataChecksum == serverDataChecksum` when non-pure
(`AsyncServer.cpp:1696-1700`), pure-pak checksums, gametype/mod identity
(`AsyncClient.cpp:1077-1082`), password when `si_usepass`, and `si_maxPlayers`.

### 3.4 LAN discovery

Two independent mechanisms, neither needing the master:

1. **Broadcast scan.** `idAsyncClient::GetLANServers` (`AsyncClient.cpp:351-375`) sends a
   connectionless `getInfo` packet to `NA_BROADCAST` on ports 27666-27673, then
   `idServerScan::SetupLANScan` (`ServerScan.cpp:111-118`) matches replies by challenge. The server
   replies to any `getInfo` with an `infoResponse` containing the full server-info dict and the
   client list (`AsyncServer.cpp:2059-2100`). Results are printed by `listServers`
   (`AsyncClient.cpp:413-419`).
2. **Direct console connect.** `connect <host>[:port]` → `idAsyncClient::ConnectToServer`
   (`AsyncNetwork.cpp:346-357`, `AsyncClient.cpp:232-290`). With no port given it uses 27666
   (`AsyncClient.cpp:339-341`). This is the only reliable option on a single PC, because
   broadcasting to 255.255.255.255 is not guaranteed to loop back to the local host.

A client can scan only 8 ports per address and the server only opens 4, so a 5th server on one
machine is reachable by explicit `connect` but invisible to `LANScan`.

### 3.5 Master server, internet browser, CD-key auth — dead as shipped

The pieces exist and are wired:

* `net_master0` defaults to `IDNET_HOST ":" IDNET_MASTER_PORT` and is `CVAR_ROM`
  (`AsyncNetwork.cpp:59`) — it cannot be changed from the console.
* `idAsyncNetwork::GetNETServers` sends `getServers` to master 0 only
  (`AsyncClient.cpp:382-406`); the answer fills the browser list, after which the client sends
  `getInfo` per server.
* Heartbeats: `MasterHeartbeat` sends to every configured master (`AsyncServer.cpp:2588-2615`).
* CD-key authorisation is **not** a separate service in the GPL source: the *server* forwards a
  `srvAuth` packet to `GetMasterAddress()` and waits for an `auth` reply
  (`AsyncServer.cpp:1541-1550`, `1398-1476`, timeout `AUTHORIZE_TIMEOUT = 5000`,
  `AsyncServer.h:45`). There is no `AuthSystem.cpp`; the "auth server" *is* the master.

Status today:

* `idnet.ua-corp.com` still resolves (A → 20.25.16.228 as of 2026-10-27), but that address is not
  an id master (the domain was let go years ago). **Inference, not a protocol probe**: no
  `getServers` reply will come back, so the internet list stays empty, and `checkNewVersion` /
  heartbeat / CD-key auth all time out. Treat the internet path as **dead**.
* If no auth reply arrives, the server **does not reject** the client: after 5 s it logs
  `Authorize server timed out` and continues the connect (`AsyncServer.cpp:1717-1721`). So internet
  play is not *blocked* by the dead master — it is unverified, and the browser cannot find servers.
* `ID_ENFORCE_KEY` is `1` for a normal Windows build (`framework/BuildDefines.h:112-118`), which
  makes the engine read `doomkey`/`xpkey` from the save path (`Session.cpp:2985-3017`) and prompt
  when the key is absent. The check is offline-only (length + charset + optional CRC,
  `Session.cpp:3123-3195`); `StartNewGame` still proceeds after buffering the prompt
  (`Session.cpp:1151-1176`). The port's existing runs already carry a `doomkey` in the save dirs
  (`targets/win11-amd64/build/*/save/base/doomkey`), which is why SP boots without a dialog.
* **LAN is exempt by construction**: `Sys_IsLANAddress(from)` → `CDK_OK`, with the comment
  "no CD Key check for LAN clients" (`AsyncServer.cpp:1533-1535`). `ID_NOLANADDRESS` is 0 by
  default (`BuildDefines.h:56-59`) and is only a debugging switch.

### 3.6 Why a LAN game needs nothing external

`Sys_IsLANAddress` (`win_net.cpp:778-805`) returns **true for `NA_LOOPBACK`** (first branch) and
otherwise compares the peer against every local interface/mask pair from `GetAdaptersInfo`. So
`127.0.0.1`, the machine's own LAN IP, and any host on the same subnet all take the keyless path.
Combined with a listen/dedicated server that never contacts the master unless it is a pure server or
an internet client connects, a LAN match is fully self-contained.

### 3.7 Dedicated server

Two supported shapes, one of them free:

| Shape | Mechanism | Work in the port |
|---|---|---|
| **Compile-time** dedicated exe | `ID_DEDICATED` (`BuildDefines.h:88`) makes `net_serverDedicated` a ROM cvar fixed at 1 (`AsyncNetwork.cpp:39-41`), skips renderer/UI setup (`Common.cpp:3002-3014`), skips tool commands (`Common.cpp:2337-2389`), and prints "Type 'help'…" (`Common.cpp:2850-2852`). Windows reference project: `neo/_Dedicated.props:9`; scons: `sys/scons/SConscript.core:310-311` | **New build target** (defines + TUs + link audit); the 2011 project notes an unresolved-link history with MFC removed (`idlib/precompiled.h:47-58`) |
| **Runtime** dedicated mode | `net_serverDedicated 1` in the *normal* exe: skips OpenGL and sound and shows the console instead (`Common.cpp:3006-3008`, `win_main.cpp:1163,1414-1424`, `AsyncServer.cpp:307-323`) | **None** — this already works in any build |

Also relevant: `Sys_AlreadyRunning()` uses a `"DOOM3"` mutex and refuses a second instance unless
`win_allowMultipleInstances 1` (`win_main.cpp:63,937-948`), and `Common.cpp:2818-2820` skips that
check entirely when `net_serverDedicated != 0`. That combination is what makes the two-instance test
possible.

---

## 4. 64-bit audit of the network path

The port's 59-file patch does **not** touch `framework/async/*` or `sys/win32/win_net.cpp`; it only
changes the surrounding systems (`sys_public.h`, `Event.cpp/.h`, `Class.h`, `Callbacks.cpp`,
`win_shared.cpp`, …). Everything below is therefore 2011 code compiled as x64, largely unexercised.

### 4.1 Socket handles are truncated (real defect)

| Code | Problem |
|---|---|
| `win_net.cpp:243` `int NET_IPSocket(...)` with `SOCKET newsocket` inside, `return newsocket;` (`:306`) | `SOCKET` is `UINT_PTR` (64-bit) on Win64; the return type narrows it to 32 bits |
| `sys/sys_public.h:435` `int netSocket;` in `idPort` | same narrowing, stored in the object |
| `win_net.cpp:490,529,589` `Net_WaitForUDPPacket/GetUDPPacket/SendUDPPacket( int netSocket, ... )` | `recvfrom`/`sendto` receive an `int` where `SOCKET` is expected |
| `sys/sys_public.h:455` `int fd;` in `idTCP` (`win_net.cpp:1112-1193`) | same, for the download path |

Winsock handles happen to be small integers in current Windows versions, so this will *appear* to
work — which is worse than a crash, because it is undefined and can break silently.
**Fix:** `typedef` a handle type (`SOCKET`/`UINT_PTR`) and use it for `NET_IPSocket`, `idPort::netSocket`,
`idTCP::fd`, `Net_GetUDPPacket`, `Net_SendUDPPacket`, `Net_WaitForUDPPacket`. This is mechanical but
touches every call site in 1,200 lines. **Judgement: 3-6 h including rebuild and the loopback test.**

### 4.2 No packed or pointer-shaped wire structures

* `PACKED` is defined empty for Win32 (`sys/sys_public.h:70`) and no `#pragma pack` exists in the
  async code, `win_net.cpp`, or `Session*.cpp` (grep for `pragma pack` / `PACKED` returns nothing in
  those files).
* `netadr_t` is `{ netadrtype_t type; unsigned char ip[4]; unsigned short port; }`
  (`sys_public.h:399-408`) — 12 bytes with 2 bytes of tail padding on **both** x86 and x64, so no
  layout change; it is **not** sent raw: `idBitMsg::WriteNetadr` copies the 4 IP bytes and writes
  the port as a `uint16` (`idlib/BitMsg.cpp:219-224`).
* `usercmd_t` (`framework/UsercmdGen.h:89-108`) contains no pointers and is serialised field by
  field, including the delta variant (`AsyncNetwork.cpp:189-248`).
* No `sizeof(struct)`-derived message lengths exist in the network path: a grep for
  `WriteData(...sizeof`, `ReadData(...sizeof`, `WriteBits(...sizeof`, `WriteLong(sizeof` across
  `game/`, `framework/`, `framework/async/`, `d3xp/` returns **no matches**. All `sizeof` uses in
  `framework/async/*.cpp` are local buffer sizes (`sizeof(msgBuf)`, `sizeof(userCmds)`), which is
  correct on any pointer width.

### 4.3 Snapshots and entity state

Server: `idGameLocal::ServerWriteSnapshot` (`Game_network.cpp:550-687`) writes
`spawnId` (32 bits), type number, entityDef index, then the class-specific
`WriteToSnapshot(idBitMsgDelta&)`. Client: `ClientReadSnapshot` (`Game_network.cpp:957-1096`) reads
the same. `entityState_t`/`snapshot_t` (`Game_local.h:118-130`) *do* contain pointers (`idBitMsg state`,
`next`), but they are **local allocator objects** — only `stateBuf[512]` bytes ever go on the wire
(`Game_network.cpp:620-623, 1022-1027`). Same for `entityNetEvent_t` (`Game_local.h:132-142`): a
128-byte opaque buffer plus a 32-bit spawnId.

`memcmp` on `netadr_t` for duplicate-server detection (`ServerScan.cpp:154`) compares two padding
bytes that are not always initialised — a pre-existing latent bug on x86 too, not an x64
regression. **Judgement: leave it, or zero-init `netadr_t` at every construction site.**

### 4.4 The event-system widening does **not** change the wire

This was the specific worry in the brief. The premise ("`Event.cpp` `WriteToMsg`/`ReadFromMsg`")
does not hold: **those functions do not exist** in this source (grep for `WriteToMsg|ReadFromMsg`
in `game/gamesys/Event.*` and `d3xp/gamesys/Event.*` returns nothing). What the port changed is the
in-process event queue:

| Change | Port file | Pristine |
|---|---|---|
| `idEventArg::value` `int` → `intptr_t` | `game/gamesys/Class.h:57-70` | `int` + `reinterpret_cast<int>(&data)` |
| per-arg slot size `sizeof(int)`/`sizeof(idVec3)` → `sizeof(intptr_t)`/aligned | `game/gamesys/Event.cpp:87-125` | `sizeof(int)` etc. |
| `CopyArgs`/`ServiceEvents` arrays `int[8]` → `intptr_t[8]` | `Event.h:104-110`, `Event.cpp:328,453` | `int` |
| generated ABI callbacks take `const intptr_t` | `game/gamesys/Callbacks.cpp` (all cases) | `const int` |
| save/restore archives entity args via `idEntityPtr` and strings via `idStr` | `Event.cpp:611-760` | raw `int`/`float` |

None of that is a message format. Networked entity events are built by the *caller* as opaque bytes
and relayed verbatim:

* `idEntity::ServerSendEvent` writes `GAME_RELIABLE_MESSAGE_EVENT`, a 32-bit spawnId, an event id,
  the time, and then `msg->GetSize()` bytes of the caller's buffer (`game/Entity.cpp:4723-4760`).
* The server echoes those bytes to clients and stores them for late joiners
  (`Game_local.h:132-142`, `Game_network.cpp:832-860`).
* Every call site writes fixed-width fields: `msg.WriteBits(spectating,1)` followed by
  `ServerSendEvent(EVENT_SPECTATE, &msg)` (`Player.cpp:5444-5451`),
  `msg.WriteShort(powerup)` + 1 bit (`Player.cpp:3024-3031`), and events with no payload at all
  (`ServerSendEvent(EVENT_PICKUP, NULL, …)`, `Item.cpp:364`).

**Verdict:** the widening changes *savegame* compatibility (old x86 saves will not restore) and the
in-memory layout of `idEvent`, but **not** the multiplayer wire format. An x64 client and an x86
server built from the same source, with the same game data, can interoperate on protocol 1.41.
Caveats: the compile-time protocol switches must stay identical (`ID_CLIENTINFO_TAGS`,
`ASYNC_WRITE_TAGS`, `ASYNC_WRITE_PVS` are all 0 — `BuildDefines.h:80-84`,
`game/Game_network.cpp:44-48`), and the decl checksum / pure-pak checksums must match
(`AsyncServer.cpp:1696-1700`), which they do for identical data. Mixing an x64 port with an x86
build that has different game data or a mod will be refused — correctly.

### 4.5 Other spots checked and found safe

| Path | Why it is safe |
|---|---|
| `idBitMsg`/`idBitMsgDelta` reads/writes | pure byte/bit arithmetic on `int`, explicit widths |
| `idMsgChannel` | sequencing, RLE compression, fragment buffer — no pointer values |
| `idAsyncServer::clients[][]`, `challenges[]`, `userCmds[][]` | contain `netadr_t`, ints, `idStr`, `idMsgChannel` (with an allocator pointer) but are **never serialised** as a block |
| `Sys_IsLANAddress` `unsigned long *p_ip = (unsigned long *)&adr.ip[0]` (`win_net.cpp:793-796`) | `unsigned long` is 32-bit under LLP64, so the 4-byte read is the right size; unaligned/alising punt is ugly but works on MSVC/x86-64. **Could be replaced with `memcpy` for cleanliness** |
| `LittleRevBytes(clientInPVS, sizeof(int), …)` (`Game_network.cpp:686`) | `sizeof(int) == 4` on both targets |
| `udpPorts[65536]` (`win_net.cpp:867`) | an array of pointers → 512 KB static on x64; harmless |
| `intptr_t`/`sysDLLHandle_t`/`address_t` fixes already in the port | `sys/sys_public.h`, `win_shared.cpp` — outside the net path but adjacent |

### 4.6 Summary table

| Hazard | Severity | x64-specific? | Action |
|---|---|---|---|
| `SOCKET` narrowed to `int` (3 places + 3 signatures) | **High** (undefined; may appear to work) | yes | Fix before any net claim |
| Missing `guis/netmenu.gui` → unguarded `guiNetMenu->` deref (`AsyncClient.cpp:213-214`) | **High** with demo data (crash on connect) | no | 2-line guard, or stub gui in the data dir |
| `si_maxPlayers` unset → `ALLOW_NOTYET` (`Game_network.cpp:253-260`) | Medium (test setup) | no | `sets si_maxPlayers N` |
| No MP map → spawn fallback / no DM layout | Medium (content, not code) | no | Test map or accept fallback for a transport test |
| `memcmp` of `netadr_t` incl. padding (`ServerScan.cpp:154`) | Low | no | Optional zero-init |
| `unsigned long*` punting in `Sys_IsLANAddress` | Low | no (LLP64) | Optional `memcpy` |
| Event-slot widening vs savegames | — (savegames only) | yes | Already handled by the port; MP unaffected |

---

## 5. Disposition: what the x64 port must change, what works, what is dead

| Item | Status | Evidence |
|---|---|---|
| UDP transport, broadcast, non-blocking sockets | **works as-is** (after the `SOCKET` fix) | `win_net.cpp:243-613` |
| 27666..27669 server ports, ephemeral client port | **works as-is** | `Licensee.h:68-73`, `AsyncServer.cpp:105-130`, `AsyncClient.cpp:111-123` |
| Connectionless challenge/connect handshake | **works as-is** | `AsyncServer.cpp:1484-1890` |
| Reliable/unreliable channels, fragmentation, compression | **works as-is** | `MsgChannel.cpp` |
| Snapshots, usercmds, prediction, delta compression | **works as-is on the wire**; **needs runtime verification on x64** | §4.3 |
| Game modes DM / TDM / Tourney / Last Man | **works as-is** | `MultiplayerGame.h:42-48` |
| Chat, voice taunts, votes, teams, spectate, scoreboard | **works as-is** (voice data caveat in demo) | §2.3 |
| LAN discovery (`LANScan` / `listServers`) | **works as-is**; **GUI missing** in demo | `AsyncClient.cpp:351-419` |
| Direct `connect` / `spawnServer` console path | **works as-is** | `AsyncNetwork.cpp:296-357` |
| Listen server (host + play) | **works as-is** | `AsyncServer.cpp:151-183` |
| Runtime dedicated (`net_serverDedicated 1`) | **works as-is, no build change** | `Common.cpp:3002-3014` |
| Compile-time `ID_DEDICATED` exe | **would need a new target + link work** | `_Dedicated.props`, `precompiled.h:47-58` |
| Missing `guis/netmenu.gui` / `guis/mphud.gui` | **must be supplied or guarded** | pk4 inventory; `AsyncClient.cpp:213-214`; `Player.cpp:1445+` guards the HUD |
| MP maps | **absent from demo data; must be authored/imported** | §2.4 |
| Internet server browser | **dead** (master) | §3.5 |
| CD-key authorisation | **dead/soft** (master; auth timeout lets clients in) | `AsyncServer.cpp:1717-1721` |
| PunkBuster | **dead** (string only) | `Session_menu.cpp:1041` |
| CTF (d3xp) | **dead** (compiled out) | `d3xp/MultiplayerGame.h:47-57` |
| PK4 auto-download | **disabled in the port** (no curl) | port README |
| IPv6 / NAT punch / UPnP | **absent** | §3.2 |
| VoIP / microphone chat | **absent** | §2.3 |

---

## 6. Effort estimate

**These are judgement estimates, not measurements.** They assume one engineer familiar with the
codebase, the existing CMake/MSVC 2022 build, and the scripts in `scripts/`. They exclude content
creation time for real DM maps (art/layout), which is a separate discipline, and exclude any
server-side service (master/relay).

| # | Task | Files | Risk | Hours (estimate) |
|---|---|---|---|---|
| 1 | Socket-handle width fix (`SOCKET`-typed handles end to end) | `sys/sys_public.h`, `sys/win32/win_net.cpp` | Medium (mechanical, wide blast radius) | 3-6 |
| 2 | Two-process loopback test script + log assertions + wiring into CI | new `scripts/net-2proc-test.ps1`, build dirs only | Low | 2-4 |
| 3 | Guard the missing netmenu pointer (and/or stub `guis/netmenu.gui`, `guis/mphud.gui` in the data dir) | `framework/async/AsyncClient.cpp:213-214` (2 lines) + data | Low | 1-3 |
| 4 | Exercise MP on x64: 2 clients, snapshots, prediction, respawn, weapon fire; fix what breaks | `game/*`, `framework/async/*` (diagnosis first) | Medium-High (unknown-unknowns) | 8-24 |
| 5 | A DM-capable test map (`info_player_deathmatch` × 4+, MP weapon pickups, levelshot) | new `maps/game/mp/*` content; needs a tools-enabled build for `dmap` (the port sets `ID_NO_TOOLS`) | Medium | 6-16 |
| 6 | Optional: real `ID_DEDICATED` exe target | `targets/win11-amd64/CMakeLists.txt`, `sources.cmake`, `BuildDefines.h`, `win_main.cpp`, `win_syscon.cpp` | Medium | 4-10 |
| 7 | LAN browser UI for demo data (server list pane wired to `LANScan`/`RefreshServers`) | `guis/*.gui` content + `Session_menu.cpp` wiring | Medium | 4-10 |
| 8 | Internet browser revival (community master or static list; `master0` is ROM, `GetNETServers` uses index 0) | `AsyncClient.cpp:382-406`, `AsyncNetwork.cpp:59`, `ServerScan.cpp` | High (plus a service to run) | 12-24 + service |
| 9 | Dev-build key hygiene (e.g. `ID_ENFORCE_KEY=0` for the port, or document `doomkey`) | `BuildDefines.h`, `CMakeLists.txt` | Low | 1-2 |
| 10 | pk4 auto-download (re-enable curl / replace with libcurl) | `CMakeLists.txt`, curl TUs, `AsyncClient.cpp` download path | Medium | 8-16 |
| 11 | IPv6 + NAT traversal / UPnP | `win_net.cpp`, `AsyncServer/Client` | High | 30-60 |
| 12 | VoIP voice chat (new feature: capture, codec, jitter buffer, per-client mixing) | new sound engine work + a net channel | High | 40-80 |

**Critical path to the narrow goal ("two instances on one PC can LAN-join each other"):**
tasks 1 + 3 + 2 = **6-13 h**, with task 4's first pass (snapshot/prediction smoke) adding
**8-24 h** before the result can be called a playable MP build rather than a handshake demo.

**Critical path to a usable two-player LAN match** (correct spawns, HUD, weapons):
1 + 3 + 2 + 5 + minimal task 7 = **16-49 h**, plus the debugging in task 4.

Everything from the master server onward (internet matchmaking, download, IPv6, VoIP) is a
separate project and should not be bundled with the port.

---

## 7. Minimal path: two instances on one PC, LAN join over loopback

### 7.1 Preconditions (all four are hard requirements)

1. **Socket fix** (task 1) — otherwise every result is built on undefined behaviour.
2. **netmenu guard/stub** (task 3) — `connect` dereferences a NULL `guiNetMenu` when
   `guis/netmenu.gui` is absent (demo data), which is a crash before any packet is sent.
3. **`sets si_maxPlayers`** on the server, or `ServerAllowClient` returns `ALLOW_NOTYET`
   (`#str_07140`, "server is not ready/full").
4. **`win_allowMultipleInstances 1`** on the non-dedicated instance, or the `"DOOM3"` mutex kills it
   (`win_main.cpp:937-948`, `Common.cpp:2818-2820`).

Optional but recommended for the first run: `sets si_pure 0` (skip the pure checksum exchange) and
`sets si_usepass 0`.

### 7.2 Server instance (runtime dedicated, no renderer)

```
DOOM3.exe ^
  +set logFile 2 +set r_fullscreen 0 ^
  +set fs_basepath <DATA_DIR> +set fs_savepath <OUT>\srv\save ^
  +set net_serverDedicated 1 ^
  +set net_verbose 1 ^
  +sets si_gameType deathmatch ^
  +sets si_maxPlayers 4 ^
  +sets si_pure 0 ^
  +spawnServer game/demo_mars_city1
```

* `+set net_serverDedicated 1` also bypasses `Sys_AlreadyRunning` (`Common.cpp:2818`).
* `SpawnServer_f` copies `args.Argv(1)` into `si_map` and forces `si_gameType` away from
  `singleplayer` (`AsyncNetwork.cpp:296-330`).
* Expected first line: `Server spawned on port 27666.` (`AsyncServer.cpp:182`).
* If a loose `doomkey` is missing from `fs_savepath/base`, the engine prints
  `Couldn't read ../base/doomkey.` at startup but still runs; reuse the existing
  `build/*/save/base/doomkey` from previous runs.

### 7.3 Client instance

```
DOOM3.exe ^
  +set logFile 2 +set r_fullscreen 0 +set r_mode -1 ^
  +set r_customWidth 1280 +set r_customHeight 720 ^
  +set fs_basepath <DATA_DIR> +set fs_savepath <OUT>\cli\save ^
  +set win_allowMultipleInstances 1 ^
  +set net_verbose 1 ^
  +connect localhost
```

`connect localhost` resolves through `Sys_StringToNetAdr` (DNS/`gethostbyname` handles the name,
`win_net.cpp:737-746`); `connect 127.0.0.1` also works because the server binds `INADDR_ANY`
(`win_net.cpp:276-278`). The client binds an ephemeral port (`AsyncClient.cpp:111-123`), so there
is no port conflict with the server's 27666.

For a *LAN browser* test instead of a direct connect, use `+LANScan` then `+listServers`
(`AsyncNetwork.cpp:103-104,382-393`) after the server has been up for ~2 s. On a single host this is
less reliable than a direct connect because broadcast-to-self depends on the interface.

### 7.4 Expected log lines (the pass criteria)

Server (`<OUT>\srv\save\base\qconsole.log`):

```
Server spawned on port 27666.
sending challenge 0x........ to localhost:......
challenge from localhost:...... connecting with 0 ping
sending connect response to localhost:......
```

Client (`<OUT>\cli\save\base\qconsole.log`):

```
received challenge response 0x........ from localhost:......
received connect response from localhost:......
received gameinit, gameInitId = ..., gameFrame = ..., gameTime = ...
received first snapshot, gameInitId = ..., gameFrame ... gameTime ...
```

Optional strict checks: `net_clientShowSnapshot 1` and `net_clientLagOMeter 1`
(`game/Game_network.cpp:50-55`) add snapshot/lag diagnostics; `net_verbose 2`
(`AsyncNetwork.cpp:37`) adds per-message detail.

**`net_verbose` is not optional for this test.** `received first snapshot` is inside
`if ( idAsyncNetwork::verbose.GetInteger() )` (`AsyncClient.cpp:834-837`); the challenge/connect
lines are ungated, but the "we are actually in the game" line is not. Both command lines above set
`net_verbose 1` for that reason.

**Caveat for log-based verification:** `Sys_InitNetworking` runs *before* the file system opens the
log (`Common.cpp:2810-2826`), so `Winsock Initialized` and `Found interface: ...` appear on the
console **but not in `qconsole.log`**. Capture stdout (`Start-Process -RedirectStandardOutput`) if
the Winsock/interface lines are part of the evidence. The lines in the table above are printed
after the log is open and are therefore reliable.

### 7.5 Scripted two-process test (sketch — untested, derived from `scripts/smoke-win11-amd64.ps1`)

```powershell
#Requires -Version 5.1
# scripts/net-2proc-test.ps1 — start a dedicated server, join it over loopback, assert both logs.
param(
    [string]$Exe     = '<worktree>\targets\win11-amd64\build\Release\DOOM3.exe',
    [string]$DataDir = $env:D3_DATA_DIR,
    [string]$OutDir  = '<worktree>\targets\win11-amd64\build\net',
    [string]$Map     = 'game/demo_mars_city1',
    [int]$TimeoutSec = 90
)
$ErrorActionPreference = 'Stop'
Get-Process DOOM3 -ErrorAction SilentlyContinue | Stop-Process -Force

function Start-Engine([string]$Tag, [string[]]$Extra) {
    $dir = Join-Path $OutDir $Tag
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $a = @('+set','logFile','2','+set','r_fullscreen','0','+set','net_verbose','1',
           '+set','fs_basepath',$DataDir,'+set','fs_savepath',(Join-Path $dir 'save')) + $Extra
    Start-Process -FilePath $Exe -ArgumentList $a -PassThru |
        Add-Member -NotePropertyName LogPath -NotePropertyValue (Join-Path $dir 'save\base\qconsole.log') -PassThru
}

$srv = Start-Engine 'srv' @('+set','net_serverDedicated','1',
    '+sets','si_gameType','deathmatch','+sets','si_maxPlayers','4',
    '+sets','si_pure','0','+spawnServer',$Map)

# wait for "Server spawned on port"
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec -and
       -not ((Test-Path $srv.LogPath) -and ((Get-Content $srv.LogPath -Raw) -match 'Server spawned on port 2766\d'))) {
    if ($srv.HasExited) { throw ("server died, exit 0x{0:X}" -f $srv.ExitCode) }
    Start-Sleep -Milliseconds 500
}

$cli = Start-Engine 'cli' @('+set','win_allowMultipleInstances','1','+connect','localhost')

$sw.Restart()
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    $cli.Refresh(); if ($cli.HasExited) { break }
    if ((Test-Path $cli.LogPath) -and ((Get-Content $cli.LogPath -Raw) -match 'received first snapshot')) { break }
    Start-Sleep -Milliseconds 500
}

$s = Get-Content $srv.LogPath -Raw
$c = Get-Content $cli.LogPath -Raw
$srvAlive = -not $srv.HasExited
$cliAlive = -not $cli.HasExited
Stop-Process -Id $srv.Id,$cli.Id -Force -ErrorAction SilentlyContinue

$checks = [ordered]@{
    'srv:port'        = $s -match 'Server spawned on port 27666\.'
    'srv:challenge'   = $s -match 'sending challenge 0x[0-9a-f]+ to localhost:27666'
    'srv:response'    = $s -match 'sending connect response to localhost:27666'
    'cli:challenge'   = $c -match 'received challenge response 0x[0-9a-f]+ from localhost:27666'
    'cli:connect'     = $c -match 'received connect response from localhost:27666'
    'cli:snapshot'    = $c -match 'received first snapshot'
    'no-auth-deny'    = ($s + $c) -notmatch 'auth deny|not a lan client|will be rejected'
    'no-crash'        = $srvAlive -and $cliAlive
}
$fail = $checks.GetEnumerator() | Where-Object { -not $_.Value }
$checks.GetEnumerator() | ForEach-Object { '{0,-14} {1}' -f $_.Key, $(if ($_.Value) {'ok'} else {'FAIL'}) }
if ($fail) { exit 1 } else { exit 0 }
```

Notes on the sketch: it uses `sets` (not `set`) for server-info cvars; it waits on the authoritative
log line rather than a fixed sleep; it asserts on both sides so a half-open connection cannot pass.
The exact `localhost:27666` port text depends on `net_port`; make the regex tolerate `2766\d` if
port fallback occurs.

### 7.6 Failure modes and what they mean

| Symptom in log | Likely cause | Fix |
|---|---|---|
| `Unable to open server network port.` | all of 27666-27669 taken | stop stale `DOOM3.exe`; or `net_port 27700` |
| Client dies at/around `ConnectToServer` with no packet | NULL `guiNetMenu` (`AsyncClient.cpp:213-214`) | task 3 (guard or stub `guis/netmenu.gui`) |
| `client ... not a lan client` / `#str_04843` | peer classified as internet while `net_LANServer 1` | expected; do not set `net_LANServer` for a loopback test |
| Server prints a `#str_07140` refusal / "server full" | `si_maxPlayers` is 0 | `sets si_maxPlayers 4` on the server |
| `Authorize server timed out` | peer was not classified as LAN (e.g. a non-RFC1918 address) | use `127.0.0.1`/LAN address; the server continues anyway after 5 s |
| Client never sees a snapshot | pure-server checksum mismatch | `sets si_pure 0`, and make sure both sides load the same data dir |
| Both players spawn on one spot | no `info_player_deathmatch` in the demo map (by design) | task 5 (real MP map) |
| No HUD in game | `guis/mphud.gui` missing | guarded (`Player.cpp:1445+`) — cosmetic only |

---

## 8. What would invalidate this assessment

* **Actually running the loopback test.** Everything in §4 and §7 is static. If a 2-process test
  passes before the `SOCKET` fix, that does not mean the fix is unnecessary — only that today's
  handle values fit in 32 bits.
* **Retail data instead of the demo.** With `pak000.pk4` (5 `game/mp/d3dm*` maps, `guis/netmenu.gui`,
  `guis/mphud.gui`) items 3 and 5 largely disappear from the effort table; only the socket fix and a
  verification pass remain.
* **A community master / patched `master0`.** If the internet path is revived (e.g. by pointing
  `GetNETServers` at `master1`, which *is* writable — `AsyncNetwork.cpp:60`), the "dead" verdict for
  the browser changes; the CD-key verdict does not, because no auth service exists.
* **Engine changes between the GPL drop and any later port.** Line numbers are pinned to the
  submodule commit used for `00_overview_and_metrics.md`; a submodule bump will drift them.

---

## Appendix A — Evidence commands

```powershell
$b = 'doom3_orig\doom3\neo'
$p = '_data\doom3-demo\demo00.pk4'

# 1. pk4 inventory (needs .NET, not 7-Zip)
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::OpenRead($p)
$z.Entries.Count
$z.Entries | Where-Object { $_.FullName -match '^maps/|^guis/[^/]+\.gui$|^def/mp\.def$' } |
    Select-Object -ExpandProperty FullName
$z.Dispose()

# 2. do the demo maps contain MP spawns? (the .map is plain text inside the pk4)
$z = [System.IO.Compression.ZipFile]::OpenRead($p)
foreach ($n in 'maps/game/demo_mars_city1.map','maps/game/demo_mars_city2.map','maps/game/demo_mc_underground.map') {
    $e = $z.Entries | Where-Object { $_.FullName -eq $n }
    $sr = New-Object System.IO.StreamReader($e.Open()); $t = $sr.ReadToEnd(); $sr.Close()
    '{0}: deathmatch={1} start={2}' -f $n,
        ([regex]::Matches($t,'info_player_deathmatch')).Count,
        ([regex]::Matches($t,'info_player_start')).Count
}
$z.Dispose()

# 3. source facts used above
Select-String -Path "$b\game\MultiplayerGame.h" -Pattern 'GAME_|typedef enum' -Context 0,6
Select-String -Path "$b\game\Game_local.h"      -Pattern 'define\s+MAX_CLIENTS'
Select-String -Path "$b\framework\async\AsyncNetwork.h" -Pattern 'MAX_ASYNC_CLIENTS|ASYNC_PROTOCOL'
Select-String -Path "$b\framework\Licensee.h"   -Pattern 'PORT_SERVER|IDNET_|ASYNC_PROTOCOL_MAJOR|NUM_SERVER_PORTS'
Select-String -Path "$b\framework\async\AsyncServer.cpp" -Pattern 'Sys_IsLANAddress|srvAuth|Authorize server timed out'
Select-String -Path "$b\framework\async\AsyncClient.cpp" -Pattern 'netmenu|GetLANServers|GetNETServers|received first snapshot'
Select-String -Path "$b\framework\BuildDefines.h" -Pattern 'ID_ENFORCE_KEY|ID_NOLANADDRESS|ID_DEDICATED|ID_DEMO_BUILD'
Select-String -Path "$b\sys\win32\win_net.cpp" -Pattern 'WSAStartup|AF_INET|SO_BROADCAST|int NET_IPSocket|int\s+netSocket'
Select-String -Path "$b\sys\sys_public.h" -Pattern 'netSocket|int\s+fd;'
Select-String -Path "$b\game\gamesys\Event.cpp","$b\game\gamesys\Event.h" -Pattern 'WriteToMsg|ReadFromMsg'   # expect: nothing

# 4. what the port changed (and did not)
git diff --no-index --stat `
  doom3_orig\doom3\neo `
  .kilo\worktrees\feat-1-win11-amd64-native-build\targets\win11-amd64\doom3\neo
# no framework/async/* or sys/win32/win_net.cpp entries appear
Select-String -Path '.kilo\worktrees\feat-1-win11-amd64-native-build\targets\win11-amd64\sources.cmake' `
  -Pattern 'async/|win_net'
Select-String -Path '.kilo\worktrees\feat-1-win11-amd64-native-build\targets\win11-amd64\CMakeLists.txt' `
  -Pattern 'ws2_32|iphlpapi'
```

Measured sizes for context: `framework/async/*` = **8,448 lines in 12 files**;
`sys/win32/win_net.cpp` = **1,200**; `game/Game_network.cpp` = **1,753**;
`game/MultiplayerGame.cpp` = **3,410**.
