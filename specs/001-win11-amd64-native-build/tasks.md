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
- [x] T042 PR #4 opened as draft with evidence block

## Deferred (P2, optional)
- [ ] T050 Real x64 CPUID/FPU flags and stack traces (doc06 T14)

## Added during implementation
- [x] T060 Debugger-driven x64 fixes found by running the demo: findhandle, GUI expr op pointers, sound mix buffer, font file size, CPU_EASYARGS, OP_ADDRESS temp, Cinematic mcomp, ID_ENFORCE_KEY
- [x] T061 Review fixes: apply-target-patches.ps1, client-area 1280x720 capture, menu+map smoke with log assertions, ulVal VRAM, round-to-nearest FtoiFast/FtolFast
- [ ] T062 Demo record/playback pointer serialisation (follow-up spec)
- [ ] T063 Save-game pointer serialisation and version bump (follow-up spec)
- [x] T064 Sound: DirectSound path verified audible (audio-session peak 0.92-1.21 while focused); smoke now asserts audio. Silence seen earlier was an unfocused window, not an engine bug
- [ ] T065 Networking: loopback 2-instance join (see doom3_orig/doc/07_networking_assessment.md)
- [ ] T066 d3xp executable (see doom3_orig/doc/08_expansion_resurrection_of_evil.md); needs retail data to verify
- [x] T067 Root README with in-game showcase (docs/media), CONTRIBUTING.md, .claude/skills/win64-port-playbook (+ kilo.json skills.paths)
