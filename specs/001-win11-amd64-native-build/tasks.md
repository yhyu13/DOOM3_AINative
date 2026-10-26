# Tasks: Native Windows 11 x64 build of DOOM 3

Legend: [P] parallel-safe (disjoint files). Status: TODO / DOING / DONE / BLOCKED. Each task needs evidence.

## Phase 0 - Setup
- [ ] T000 Data: obtain official demo data into untracked `_data/` (SHA256 + license note) -> `_data/` listing
- [ ] T001 [P] Scripts skeleton: `scripts/export-target-patches.ps1` (diff target vs pristine -> `targets/win11-amd64/patches/`)

## Phase 1 - Core compile (US1)
- [ ] T010 [P] (W-build) CMakeLists + preset: idlib static + doom3 exe, x64 v143, defines per doc 06 T1/T12
- [ ] T011 [P] (W-build) Remove MFC from core, `ID_NO_TOOLS`, keep AASFile*.cpp (doc06 T2)
- [ ] T012 [P] (W-idlib) Generic SIMD only on x64 (T3)
- [ ] T013 [P] (W-idlib) `_alloca16` / Heap / Matrix / Vector pointer width (T4)
- [ ] T014 [P] (W-sys) win32 platform layer x64: cpu, main, shared, syscon (T5)
- [ ] T015 [P] (W-sys) Dependencies: DxErr, libs, curl off; win_input charset (T6, T11)
- [ ] T016 [P] (W-game) `idEventArg` intptr_t (T7); format/size_t sweep (T8); drop TypeInfo prebuild (T10)
- [ ] T017 (Coord) Static link of game module; configure+build; fix link errors (T9)
- [ ] T018 PE check: exit 0, machine 0x8664, size > 1 MB

## Phase 2 - Run (US2)
- [ ] T020 Boot with demo data: reach menu, log captured
- [ ] T021 Screenshot 1280x720 non-flat
- [ ] T022 Load demo map via console; record result
- [ ] T023 Renderer fallback decision if GL fails

## Phase 3 - Verify (US3)
- [ ] T030 `scripts/verify-win11-amd64.ps1` (PASS/FAIL/SKIPPED per criterion)
- [ ] T031 `scripts/smoke-win11-amd64.ps1`
- [ ] T032 Pristine porcelain empty + `diff-target.ps1` shows only intended files
- [ ] T033 Patches reproduce build from fresh copy (SC-006)

## Phase 4 - Review and ship
- [ ] T040 Self-review of spec/plan/tasks consistency
- [ ] T041 Independent code-review (subagent, `code-review` skill pinned origin/main...HEAD)
- [ ] T042 PR (draft -> ready) with evidence block

## Deferred (P2, optional)
- [ ] T050 Real x64 CPUID/FPU flags and stack traces (doc06 T14)
