# DOOM 3 AI-Native Port — Roadmap

Evidence captured 2026-10-26 from this working tree. Facts are in §1–§2; milestone plans, automation design, decisions, and next actions are `[suggestion]`.

## 1. Evidence: what is actually in the checkout

| Check | Finding | Evidence |
|---|---|---|
| Submodule | `doom3_orig/doom3/` at `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`; commit date 2012-01-31; message says source was released under GPL on 2011-11-22 | `git -C doom3_orig/doom3 log -1` |
| Remote | `https://github.com/yhyu13/DOOM-3.git` | `.gitmodules`; `git -C doom3_orig/doom3 remote -v` |
| Worktree | clean; `git -C doom3_orig/doom3 status --porcelain` produced 0 lines | command |
| Tracked size | 2,156 files, 33.7 MB; 2,151 files under `neo/`; `base/` has 1 file | `git -C doom3_orig/doom3 ls-files`; filesystem |
| Source counts | 563 `.cpp`, 654 `.h`, 170 `.c` (1,387 C/C++ files); 734,180 lines across those files; 236 tracked files have no extension | `git -C doom3_orig/doom3 ls-files`; line count |
| Extension counts | `.h` 654, `.cpp` 563, `.c` 170, `.html` 47, `.pdf` 40, `.3` 38, `.in` 31, `.BMP` 29, `.ico` 25, `.png` 21 | `git -C doom3_orig/doom3 ls-files` |
| Layout | Engine sources and build files are in `neo/`: `cm`, `curl`, `d3xp`, `framework`, `game`, `idlib`, `MayaImport`, `openal`, `renderer`, `sound`, `sys`, `tools`, `TypeInfo`, `ui` | directory listing |
| `base/` contents | `base/default.cfg` only: 14 bytes, content `# empty file` | file read; `git -C doom3_orig/doom3 ls-files base` |
| Game assets | 0 `.pk4`, 0 `.pk3`, 0 `.pak`, 0 `.map`, 0 `.md5mesh`, 0 `.md5anim`, 0 `.roq`, 0 `.tga`, 0 `.dds`; no playable game data | `git -C doom3_orig/doom3 ls-files`; filesystem scan |
| License | GPLv3 (`COPYING.txt`, 36,441 bytes); `README.txt` adds additional terms and excludes third-party code (curl, JPEG, OggVorbis, PropTree, OpenAL SDK, Base64) | `COPYING.txt`; `README.txt:73-75` |
| Build files | 1 `.sln`, 7 `.vcxproj`, 1 `.vcproj`, 1 `.dsp`, 1 `SConstruct`, 5 `neo/sys/scons/SConscript.*`, 6 `.xcodeproj`; **0** `CMakeLists.txt`, **0** Meson, **0** engine `Makefile` (only vendor Makefiles under `neo/curl/**` and `neo/openal/docs/`) | `git -C doom3_orig/doom3 ls-files`; listing |
| Windows solution | `neo/doom.sln` has only `Win32` configurations; `doomdll.vcxproj` uses `ToolsVersion="4.0"` (VS2010), has no `PlatformToolset` element, and hardcodes the DirectX SDK June 2010 include path | `doom.sln`; `doomdll.vcxproj` |
| SCons | `neo/SConstruct` + `SConscript.core/curl/game/gl/idlib`; Python 2 syntax (`print '...'`, imports `commands`, `popen2`, `StringIO`) | `SConstruct:6`, `SConstruct:197` |
| Xcode | `neo/sys/osx/` contains `Doom3`, `game`, `d3xp`, `idlib`, `oggVorbis`, and `SubProjects/curl` Xcode projects | `README.txt:52`; listing |
| Existing web code | 0 `emscripten`/`EMSCRIPTEN`, 0 `webgl`, 0 `wasm` references under `neo/`; no `web/`, `src/`, `port/`, `tests/` top-level directories | search; listing |
| Toolchain (environment, not repo) | git 2.25.0, node 24.19.0, uv 0.12.13, bare python 2.7.16 (`py` launcher 3.14.4); `emcc`, `em++`, `scons`, `cmake` missing; `gh` present; VS2022 Professional and MSBuild present; VS2010 and DirectX SDK June 2010 not found at the checked/default locations | `Get-Command`; `vswhere` |
| Root repo state | scaffold committed on `main`; pristine submodule recorded at `doom3_orig/doom3`; per-target copies live under `targets/` and are git-ignored | `git log --oneline`; `git submodule status` |
| CI | `.github/` absent; no workflow files; `gh` installed but no workflow to list | listing |
| Spec-kit / skills | 14 project skills in `.kilo/skills/`; `.claude/skills/` holds `win64-port-playbook` plus 10 generated `speckit-*` skills; 10 `/speckit.*` commands in `.kilo/command/` (`generic`) and the same flow as `/speckit-*` skills (`claude`); `.specify/memory/constitution.md` is filled | `docs/SKILLS.md`; `specify integration status` |
| Engine analysis notes | `doom3_orig/doc/*.md` (6 files, ~264 KB: overview/metrics, platform, renderer, scripting, AI, build/deps), read-only analysis of the pristine tree; paths normalised to `doom3_orig/doom3/` | directory listing |

**Unknowns, not facts:** peak engine memory and working set in WASM; browser/GPU performance envelope; whether the 2011 VS2010 solution builds under modern MSVC; the exact retail asset/pak layout relative to `fs_basepath`; the current Emscripten GL/Asyncify/pthread support matrix (emcc is not installed); which demo files retail data contains and how deterministic they are across builds.

**Corrections to common assumptions:**
- There is render-thread scaffolding (`GLimp_SpawnRenderThread` at `neo/sys/win32/win_glimp.cpp:1086`, `CreateThread` at `:1102`), but no call site was found; the stub returns false (`neo/sys/stub/stub_gl.cpp:386`). Rendering is driven from `idCommonLocal::Frame` (`neo/framework/Common.cpp:2439`). The live async thread is `Sys_AsyncThread` (`neo/sys/win32/win_main.cpp:873-912`, started at `:1419`) plus FileSystem's background download thread (`neo/framework/FileSystem.cpp:3749`). There is no evidence of a working threaded renderer in this checkout.
- Windows audio defaults to DirectSound (`neo/sys/win32/win_snd.cpp`); OpenAL and EAX are optional (`s_useOpenAL` default `0`, `openal32.dll` loaded on demand, `neo/sound/snd_system.cpp:74-76`). A web audio backend is new work either way.
- The source release has no game data. `base/` is a single empty config; every playable path needs user-supplied retail/Steam data.

## 2. Reality check: constraints, not plans

| Constraint | Evidence | Consequence |
|---|---|---|
| GPLv3 + additional terms | `COPYING.txt` is GPLv3; `README.txt:73-75` adds terms and excludes third-party code | Distributing WASM/JS requires the complete corresponding source under GPLv3, preserved notices, documented modifications, and no extra restrictions. GPLv3 is not AGPLv3: serving the app does not by itself add a network-source obligation, but distribution does. Excluded code keeps its own licence. |
| Game assets are outside the source licence | `README.txt:15`; zero paks in `base/` | Assets remain under the original EULA and cannot be redistributed with this repo. Users must supply a legally owned install; tests need synthetic fixtures or licensed local data. |
| OpenGL + ARB vs WebGL2 | 100 fixed-function occurrences (`qglBegin`/`qglVertexPointer`/`qglMatrixMode`) and 207 ARB-program references in `neo/renderer`; `r_renderer` paths are `best, arb, arb2, Cg, exp, nv10, nv20, r200` (`RenderSystem_init.cpp:45`); no `.vfp/.vpp/.glsl/.frag/.vert` files in the repo | WebGL2 exposes GLSL ES 3.00, not ARB assembly or the fixed pipeline. Shader generation/translation and fixed-function replacement are core renderer work, not a build flag. |
| 32-bit pointers and WASM memory | `doom.sln` has only Win32 configurations; no x64 target; WASM32 uses 32-bit pointers | Pointer-size and struct-padding assumptions matter; linear memory is bounded by the 32-bit address space and browser limits. Peak memory is unknown, and no budget can be set without measurement. |
| Renderer threading and browser main thread | No working render thread call site found; `idCommonLocal::Frame` drives rendering; `Sys_AsyncThread` is a timer-driven packet/async thread | Rendering is main-thread-bound in this checkout. In the default Emscripten GL path, WebGL context calls are made on the main thread; worker/OffscreenCanvas rendering would be a separate design. A pthread build needs COOP/COEP and SharedArrayBuffer, and does not by itself make GL calls safe from workers. |
| Blocking main loop | `while(1) { Win_Frame(); common->Frame(); }` at `neo/sys/win32/win_main.cpp:1443-1444` | A blocking loop hangs the tab. `emscripten_set_main_loop` requires refactoring the loop into a per-frame callback; Asyncify preserves the blocking shape but adds overhead and stack/coroutine constraints. Both are viable shapes with different costs. |
| Synchronous file I/O | `fopen` at `FileSystem.cpp:607,619`; `unzOpen` at `:1317`; `fs_basepath`/`fs_savepath`; paks are zip files named `pak0.pk4` etc. | Browser file access goes through MEMFS, IDBFS, preloaded files, or async fetch. The engine's synchronous read path needs a virtual filesystem layer. Case sensitivity and pak patch order matter. |
| Audio backend | DirectSound in `win_snd.cpp`; OpenAL/EAX optional (`s_useOpenAL=0`, `openal32.dll`, EAX cvars) | Web Audio is a new backend. Emscripten's OpenAL subset may cover basic playback, but EAX reverb and DirectSound behaviour do not map. Audio starts only after a user gesture. |
| Input | `DirectInput8Create` at `neo/sys/win32/win_input.cpp:515`; no `WM_INPUT`/raw-input path found | Browser keyboard/mouse plus Pointer Lock replace DirectInput. Gamepad support is new work, not a port of existing code. Focus loss and pointer-lock exit are new states to handle. |
| Networking | Winsock UDP (`AF_INET`, `SOCK_DGRAM` at `win_net.cpp:256`); dedicated mode is `net_serverDedicated` (`AsyncNetwork.cpp:41`) | WASM has no raw UDP. Browser multiplayer needs a WebSocket/WebRTC bridge or a native dedicated server; a browser-hosted dedicated server is a separate design. |
| Game module loading | `LoadGameDLL` calls `sys->DLL_Load` under `__DOOM_DLL__` (`Common.cpp:2629`); `Game`/`Game-d3xp` are `DynamicLibrary` projects | There is no general `dlopen` in WASM. The WASM build needs a static single-module game link or an equivalent; dynamic game selection is not available as-is. |
| Fidelity limits and performance | `README.txt:59` says the GPL release omits "Carmack's Reverse" depth-fail stencil shadows; no Emscripten/browser baseline exists | Visual parity is not 1:1 with retail. Golden images can only be sourced from this codebase and asset set. Performance numbers are unmeasured; native numbers are not a budget. |

## 3. Milestones M0–M7 [suggestion]

Roles follow `docs/GIT_WORKFLOW.md`: **coordinator** owns integration and the submodule bump, **worker** owns one worktree/issue, **reviewer** is independent and never the author. Every exit criterion is a command result, artifact, or log.

Paths below point at the pristine submodule (`doom3_orig/doom3`) because that is what this evidence was collected from. A worker doing a milestone creates a target copy with `scripts/new-target.ps1 -Name <target>` and edits/builds `targets/<target>/doom3`, never the pristine tree.

### M0 — Repo + CI baseline [suggestion]

| Field | Plan |
|---|---|
| Goal | Make the scaffold reproducible and gate it in CI without touching engine source. |
| Exit criteria | `git submodule status` shows `a9c49da...`; `git -C doom3_orig/doom3 status --porcelain` is empty; `main` has an initial commit; CI runs `scripts/verify-skills.ps1`, a submodule-integrity check, and an asset check (`git -C doom3_orig/doom3 ls-files '*.pk4' '*.pk3' '*.pak'` is empty); missing `emcc`/VS prerequisites fail with a named prerequisite message, not a silent skip. |
| Main risks | Unborn `main`; no CI provider or workflow; worktree-local state leaking into commits; secrets in logs; `git stash` is forbidden. |
| First command | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify-skills.ps1` |
| Spec-kit driver | `/speckit.specify` → `/speckit.tasks` (fill `/speckit.constitution` first) |
| Installed skill | `verification-before-completion`; supporting `using-git-worktrees` |
| Owner | coordinator |

### M1 — Reproduce the native Windows build [suggestion]

| Field | Plan |
|---|---|
| Goal | Produce the original client/game binaries from a documented Windows toolchain, or prove exactly why the 2011 build cannot be reproduced and record the fallback. |
| Exit criteria | One documented native build command exits 0 and produces DoomDLL (Application), Game, and Game-d3xp (DynamicLibrary) artifacts; the log and toolchain versions are archived; `doom3_orig/doom3/` is clean or points at a recorded fork commit; the build is 32-bit. |
| Main risks | VS2010/v100 toolset and DirectX SDK June 2010 are not found; modern MSVC rejects 2011 C++; hardcoded SDK include paths; Python 2 SCons; 32-bit dependencies; MayaImport is optional but may break whole-solution builds. |
| First command | `$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"; $msbuild = @(& $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe")[0]; & $msbuild doom3_orig\doom3\neo\doom.sln /p:Configuration=Release /p:Platform=Win32 /m` — expected to fail until prerequisites or a CMake port exist; archive the exact errors. |
| Spec-kit driver | `/speckit.plan` → `/speckit.tasks` |
| Installed skill | `game-build-verification`; supporting `cross-platform-guide` |
| Owner | worker |

### M2 — Headless dedicated-server build to WASM, no renderer [suggestion]

| Field | Plan |
|---|---|
| Goal | Link the dedicated server path to WASM without GL, audio, or a window, and boot it under Node. |
| Exit criteria | `emcc` links a dedicated target (`ID_DEDICATED` plus stub GL/sys) with the game module statically linked; `wasm-validate` passes; `wasm-objdump -x` shows no GL/audio imports; a Node run reaches the dedicated startup marker (`Common.cpp:2851`) or a named missing-asset failure; artifact and log are archived. Stub sources exist under `neo/sys/stub/` and `neo/sys/linux/dedicated.cpp`; no Emscripten target exists yet. |
| Main risks | Build graph is not ported (VS2010/SCons); Python 2 SCons; `dlopen` game module; Winsock UDP; missing assets; 32-bit assumptions; unknown Emscripten version and flags. |
| First command | `emcc --version` (currently missing; install/activate emsdk first) |
| Spec-kit driver | `/speckit.specify` → `/speckit.plan` |
| Installed skill | `wasm-emscripten`; supporting `cross-platform-guide`, `systematic-debugging` |
| Owner | worker |

### M3 — Asset pipeline + virtual filesystem [suggestion]

| Field | Plan |
|---|---|
| Goal | Load legally supplied DOOM 3 data in the browser through a deterministic VFS, without committing any asset. |
| Exit criteria | A local manifest lists pak name/size/hash; no `.pk4/.pk3/.pak` is tracked (`git -C doom3_orig/doom3 ls-files` is empty for those extensions); the wasm opens a known file from a pak via the Emscripten FS; missing assets produce an actionable error; pak search order, patch override, and case handling are documented; a synthetic fixture works with no retail data. |
| Main risks | EULA/licensing; pak zip handling and case sensitivity; preload size and startup time; IDBFS quota; CORS; lazy range fetch; user-supplied paths. |
| First command | `Get-ChildItem -Path $env:DOOM3_ASSETS -Recurse -Filter *.pk4` (set `DOOM3_ASSETS` to a legal local install; commit nothing) |
| Spec-kit driver | `/speckit.plan` → `/speckit.tasks` |
| Installed skill | `wasm-emscripten`; supporting `cross-platform-guide` |
| Owner | worker |

### M4 — Renderer via Emscripten GL → WebGL2 [suggestion]

| Field | Plan |
|---|---|
| Goal | Render a deterministic DOOM 3 scene in WebGL2 from the client build. |
| Exit criteria | A WebGL2 context is created; translated shaders compile; `r_renderer arb2` or a documented fallback renders the scene; a Playwright canvas screenshot is non-blank and within the threshold recorded with the first human-reviewed golden; no GL error storm; every fixed-function/ARB replacement is listed. |
| Main risks | ARB assembly and fixed-function translation; GLSL ES 3.00 precision/limits; missing GL extensions; material/shader data lives in assets; the GPL release omits depth-fail stencil shadows; GPU/driver variance. |
| First command | `emcc -v; findstr /s /c:"glProgramStringARB" doom3_orig\doom3\neo\renderer\*.cpp` (probe the SDK and the ARB dependency), then serve the built client. |
| Spec-kit driver | `/speckit.plan` → `/speckit.analyze` |
| Installed skill | `wasm-emscripten`; supporting `cross-platform-guide`, `game-debug` |
| Owner | worker |

### M5 — Input, audio, UI [suggestion]

| Field | Plan |
|---|---|
| Goal | Make the browser client operable: keyboard, mouse look, gamepad, audio, and menus. |
| Exit criteria | Pointer Lock acquires and releases cleanly; keyboard/mouse mappings match the native defaults; gamepad polling works; audio resumes after a user gesture and plays a known cue; menus navigate by keyboard and mouse; a Playwright end-to-end test passes with no console errors. |
| Main risks | DirectInput mapping; browser gesture/autoplay rules; EAX has no Web Audio equivalent; latency; focus loss; DPI/scaling; pointer-lock UX. |
| First command | `uv run --python 3.12 .kilo/skills/webapp-testing/scripts/with_server.py --help` |
| Spec-kit driver | `/speckit.tasks` → `/speckit.implement` |
| Installed skill | `webapp-testing`; supporting `cross-platform-guide`, `game-debug` |
| Owner | worker |

### M6 — Performance: pthreads, SIMD, memory [suggestion]

| Field | Plan |
|---|---|
| Goal | Establish a measured performance/memory envelope and only then add threads/SIMD where they pay. |
| Exit criteria | A baseline is recorded on named hardware/browser; `timeDemo`/`timeDemoQuit` metrics are captured; a regression budget gate exists; a pthread build works with COOP/COEP or a no-pthread decision is documented; the SIMD path is selected or a scalar fallback is documented; no OOM; memory-growth behaviour is recorded. |
| Main risks | SharedArrayBuffer/COOP/COEP deployment; 32-bit memory ceiling; memory growth and allocator fragmentation; SIMD feature detection; browser/GPU variance; no baseline today. |
| First command | `emcc -pthread -c tests\pthread_smoke.c -o tests\pthread_smoke.o` (after adding the smoke fixture; verifies the installed SDK accepts `-pthread`). |
| Spec-kit driver | `/speckit.analyze` → `/speckit.tasks` |
| Installed skill | `wasm-emscripten`; supporting `systematic-debugging`, `game-debug` |
| Owner | worker |

### M7 — Automated AI test harness + coding loop [suggestion]

| Field | Plan |
|---|---|
| Goal | Make the issue → worktree → spec → implement → test → review → PR loop run with evidence gates. |
| Exit criteria | A PR gate runs the build, headless smoke, deterministic demo playback, screenshot diff, perf budget, and log triage; a failing gate blocks merge; the PR carries the evidence artifact; the reviewer is independent; an agent runbook documents the loop; no agent merges its own work. |
| Main risks | Flaky screenshots; GPU/browser variance; asset licensing in CI; agents editing `doom3_orig/doom3/` directly; false positives; CI cost; silent skips. |
| First command | `gh workflow list` (currently empty; this is the baseline). |
| Spec-kit driver | `/speckit.implement` → `/speckit.analyze` |
| Installed skill | `code-review`, `verification-before-completion`, `webapp-testing`, `using-git-worktrees` |
| Owner | coordinator |

## [suggestion] 4. Automation design: what "AI testing" means here

| Gate | What it proves | Concrete mechanism | Cheap? |
|---|---|---|---|
| Build gate | The change compiles/links and exports only allowed imports | Native: documented MSBuild command exit 0 + artifact presence. WASM: `emcc` exit 0, `wasm-validate`, `wasm-objdump -x` import allowlist (no GL/audio in dedicated), artifact hash and size. | Yes, per PR |
| Headless smoke | The wasm boots and reaches a known state without a browser | Node runner with MEMFS fixture; assert the dedicated marker (`Common.cpp:2851`) or a named asset error; hard timeout; capture exit code and console. | Yes |
| Deterministic demo playback | Gameplay frames advance reproducibly | Use the engine's own commands: `+playDemo <name>`, `+playCmdDemo <name>`, `+timeDemo <name>`, `+timeDemoQuit <name>` (registered at `Session.cpp:2867-2880`); same build, same assets, same demo file. `timeDemo` prints `N frames rendered in S seconds = F fps` (`Session.cpp:868`). | Medium |
| Frame capture + image diff | The rendered result did not regress | Playwright screenshots the canvas at a fixed viewport and `deviceScaleFactor` after a fixed demo frame; compare against a human-reviewed golden with a threshold recorded at baseline; ignore explicitly nondeterministic regions. Engine `TakeScreenshot`/`aviDemo` can provide a second view. | Medium; inherently flaky |
| Perf budget regression | Frame time/throughput/memory stay within an agreed envelope | Parse `timeDemo` output plus browser performance/memory metrics; compare to a baseline recorded on named hardware/browser; fail on regression beyond the recorded tolerance. No numeric target is assumed before the baseline. | Medium; hardware variance |
| Crash/log triage | Failures are classified, not just red | Capture console, WASM trap stack, exit code, GL errors, idLib errors, and network errors; map to known signatures; attach a short triage artifact to the PR; deduplicate. | Yes |

**Not cheaply automatable:** cross-GPU/browser visual and performance parity; audio quality, latency, and EAX reverb behaviour; gameplay feel and input latency; legal retail-asset provisioning in CI (use synthetic fixtures for code, licensed local data for content); shader-compiler and driver-specific differences; full-game completion/quest logic beyond a demo slice; long demo runs that are inherently timing-sensitive.

## [suggestion] 5. Open decisions, with a recommended default

| Decision | Recommended default | Rationale | Revisit |
|---|---|---|---|
| SDL2 vs raw Emscripten APIs | SDL2 behind the existing `idSys`/sound interfaces for window, input, pointer lock, gamepad, and audio; raw Emscripten for FS, main loop, and GL context where SDL2 does not fit | The codebase already isolates platform code; one backend can serve native and web and avoids bespoke browser API code. Cost: new dependency and build integration. Fall back to a single `sys/emscripten` backend if the dependency cost dominates. | M5 |
| Single-thread first vs pthreads immediately | Single-thread first through M5; add pthreads only in M6 after a measured need | COOP/COEP and SharedArrayBuffer deployment complexity; Asyncify already complicates control flow; rendering is main-thread-bound here anyway. | M6 |
| WebGL2 first vs WebGPU | WebGL2 first | The source is OpenGL/ARB and Emscripten GL maps to WebGL2; WebGPU is a new renderer, not a translation target. | After M6 |
| Pak preload vs lazy fetch | Preload the minimal boot pak set into MEMFS with an IDBFS cache; add lazy/range fetch only after boot works | The engine reads files synchronously through `fopen`/pak unzip; preloading preserves that shape. Cost: initial download size is unknown and must be measured. | M3/M4 |
| Where the port lives | Engine changes as commits on the `yhyu13/DOOM-3` fork, then a submodule bump here; web shell, build scripts, CI, and specs live in this repo | Matches `AGENTS.md` non-negotiable #1 and avoids patch drift; the submodule SHA is the reviewable contract. Cost: fork maintenance. | Every engine PR |

## [suggestion] 6. Next 3 actions

1. **Coordinator:** record M0 evidence and land the scaffold — run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify-skills.ps1`, `git submodule status`, and `git -C doom3_orig/doom3 status --porcelain`; commit the reviewed tree, then add the CI workflow and the prerequisite check.
2. **Worker:** run the M1 native-build probe exactly as written, archive the failure text, and decide via `/speckit.plan` between restoring VS2010 + DirectX SDK June 2010 and adding a modern CMake build.
3. **Worker:** install/activate emsdk, run `emcc --version`, then `/speckit.specify` the M2 dedicated-WASM slice with a synthetic asset fixture and the no-GL/no-audio import check.
