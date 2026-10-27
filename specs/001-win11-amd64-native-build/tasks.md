# Tasks: Native Windows 11 x64 build of DOOM 3

Legend: [P] parallel-safe (disjoint files). Status: TODO / DOING / DONE / BLOCKED. Each task needs evidence.

## Phase 0 - Setup
- [x] T000 Data: obtain official demo data into untracked `_data/` (SHA256 + license note) -> `_data/` listing
- [x] T001 [P] Scripts skeleton: `scripts/export-target-patches.ps1` (diff target vs pristine -> `targets/win11-amd64/patches/`)

## Phase 1 - Core compile (US1)
- [x] T010 [P] (W-build) CMakeLists + preset: idlib static + doom3 exe, x64 v143, defines per doc 06 T1/T12
- [x] T011 [P] (W-build) Remove MFC from core, `ID_NO_TOOLS`, keep AASFile*.cpp (doc06 T2)
- [x] T012 [P] (W-idlib) Generic SIMD only on x64 (T3)
- [x] T013 [P] (W-idlib) `_alloca16` / Heap / Matrix / Vector pointer width (T4)
- [x] T014 [P] (W-sys) win32 platform layer x64: cpu, main, shared, syscon (T5)
- [x] T015 [P] (W-sys) Dependencies: DxErr, libs, curl off; win_input charset (T6, T11)
- [x] T016 [P] (W-game) `idEventArg` intptr_t (T7); format/size_t sweep (T8); drop TypeInfo prebuild (T10)
- [x] T017 (Coord) Static link of game module; configure+build; fix link errors (T9)
- [x] T018 PE check: exit 0, machine 0x8664, size > 1 MB

## Phase 2 - Run (US2)
- [x] T020 Boot with demo data: reach menu, log captured
- [x] T021 Screenshot 1280x720 non-flat
- [x] T022 Load demo map via console; record result
- [x] T023 Renderer fallback decision if GL fails - not needed: ARB2 path runs on this GPU (log: 'using ARB2 renderSystem')

## Phase 3 - Verify (US3)
- [x] T030 `scripts/verify-win11-amd64.ps1` (PASS/FAIL/SKIPPED per criterion)
- [x] T031 `scripts/smoke-win11-amd64.ps1`
- [x] T032 Pristine porcelain empty + `diff-target.ps1` shows only intended files
- [x] T033 Patches reproduce build from fresh copy (SC-006)

## Phase 4 - Review and ship
- [x] T040 Self-review of spec/plan/tasks consistency
- [x] T041 Independent code-review (subagent, `code-review` skill pinned origin/main...HEAD)
- [x] T042 PR #4 opened as draft with evidence block; since squash-merged into main as ea2e02e
- [ ] T043 Independent review of everything after PR #4: T060, T061, T064, T070-T075, T050, T062, T063, T065, T066 (round-1/round-2 reviewers in this session were agents of the same workflow, recorded in review.md "Re-review"; a fresh review of the committed diff is still required)

## Phase 5 - Developer setup stage (US4)
- [x] T070 Root `CMakeLists.txt` + `CMakePresets.json`: `add_subdirectory(targets/win11-amd64)`, exe to `build/bin/<Config>/`, `verify` / `smoke` targets
- [x] T071 Configure-time bootstrap: submodule init, `new-target.ps1`, `apply-target-patches.ps1` when the copy is missing (`DOOM3_BOOTSTRAP_TARGET`)
- [x] T072 `scripts/_common.ps1` `Resolve-Doom3Exe`; verify/smoke/run/capture-showcase use it, verify passes `-Exe` to smoke
- [x] T073 Root build of the current tree to `build/`: exit 0, PE 0x8664, size > 1 MB (evidence: `scripts/verify-win11-amd64.ps1` output; the #1 status comment predates the root build and is refreshed by T076)
- [x] T074 Bootstrap proof (SC-007, partial by design): root `cmake --preset` recreated and patched a missing copy (`diff -rq` vs the original: no difference); a full build of an independently recreated copy (stand-alone CMake, `-DNEO_DIR`) exited 0 with PE 0x8664; smoke PASS on the root-built exe. The single unbroken run "empty clone -> root configure -> root build" is T077
- [x] T075 Docs: README "Build it yourself", CONTRIBUTING build step, `targets/win11-amd64/README.md` pointer to the root build, AGENTS.md repository facts
- [ ] T076 Update the PR/issue #1 comment with the setup-stage and T050-T066 evidence (needs commit + PR first)
- [ ] T077 One unbroken SC-007 run in a fresh clone of the merged branch: `cmake --preset` + `cmake --build --preset` + `verify`, plus the interrupted-copy replacement path (implemented, never executed)
- [ ] T078 Merge `main` (PR #7 changed the skills layout: `.claude/skills` tracked, `.kilo/skills` ignored mirror) and resolve AGENTS.md / README / CONTRIBUTING / skills conflicts

## Phase 6 - Engine follow-ups (done in this session)
- [x] T050 Real x64 FPU state / stack traces (`scripts/test-cpu-fpu-trace.ps1`: CPU line, FPU dump, 7 resolved frames with file:line, fatal-error trace, PDB-less module+offset fallback, single-instance mutex). CPUID itself was already real. Not covered: unmasking real FPU exceptions, Debug build, x86 crash dialog; HTT flag is the CPUID bit (cosmetic on CPUs without HT)
- [x] T062 Demo record/playback pointer serialisation (render demo presence flags, GuiModel material slot, sound demo). Evidence: `scripts/test-savegame-demo.ps1 -Sound` on the final exe passes; the pre-fix exe fails the same test with `ERROR: Bad render demo token` during playback with sound on
- [x] T063 Save-game pointer serialisation, `SAVEGAME_VERSION` 17 -> 18 (old file rejected with a warning, exit 0). Evidence: save, load in a new process, view origin/yaw round trip delta 0.00. Not covered: player health (no console command), sound channels that are mid-playback
- [x] T065 Loopback two-instance join (`scripts/test-net-loopback.ps1`; baseline exe reproduces the `idPlayer::ExitCinematic` crash after map restart, fixed build survives). Gameplay not verified: the demo has no multiplayer map, a test-only entityDef stub is used, 2 clients, `si_pure 0`
- [x] T066a `DOOM3XP.exe` builds (x64, d3xp game statically linked, shared engine object library), starts, d3xp-only cvars present, clean fallback without d3xp data (`scripts/test-d3xp.ps1`)
- [ ] T066b Resurrection of Evil gameplay, RoE menu entry, xpkey, CTF: needs retail d3xp data (not available)

## Deferred (P2, optional)
- [ ] T079 Unverified edges to close when convenient: FPU exception unmasking, Debug configuration build, `sys_fpuState` on x87 code paths, sound-channel restore of a channel that is mid-playback, MP content (retail), 3+ clients, x64<->x86 interop

## Added during implementation
- [x] T060 Debugger-driven x64 fixes found by running the demo: findhandle, GUI expr op pointers, sound mix buffer, font file size, CPU_EASYARGS, OP_ADDRESS temp, Cinematic mcomp, ID_ENFORCE_KEY
- [x] T061 Review fixes: apply-target-patches.ps1, client-area 1280x720 capture, menu+map smoke with log assertions, ulVal VRAM, round-to-nearest FtoiFast/FtolFast
- [x] T064 Sound: DirectSound path verified audible (audio-session peak 0.92-1.21 while focused); smoke now asserts audio. Silence seen earlier was an unfocused window, not an engine bug
- [x] T067 Root README with in-game showcase (docs/media), CONTRIBUTING.md, .claude/skills/win64-port-playbook (+ kilo.json skills.paths)
