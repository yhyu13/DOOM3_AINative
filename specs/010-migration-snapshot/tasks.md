# Tasks: 010-migration-snapshot

**Input**: `spec.md`, `plan.md`
**Verification command**: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-migration.ps1`

Legend: `[P]` = can run in parallel with its neighbours; `[USn]` = user story from `spec.md`.

## Phase 1 — Publish the specification (docs PR)

- [ ] T001 Write `specs/010-migration-snapshot/spec.md` (problem, stories, FR-1..FR-10, evidence) [US1]
- [ ] T002 Write `specs/010-migration-snapshot/plan.md` (pipeline, decisions, risks, rollback) [US1]
- [ ] T003 Write `specs/010-migration-snapshot/tasks.md` (this file) [US1]
- [ ] T004 Open the docs PR with `Refs #77`, evidence block and risk/rollback; merge after independent review

## Phase 2 — Snapshot script `scripts/migrate-repo.ps1`

- [ ] T005 [P] Preflight: repo check, git check, non-empty `-OutDir` refusal (`-Force`), dirty-tree
      refusal (`-AllowDirty` saves `git diff --binary --output` plus untracked copies) [US4]
- [ ] T006 [P] Bundles: `--branches --tags HEAD`, submodule bundle, `git bundle verify` on both [US1]
- [ ] T007 Local-only refs: upstream/ahead/behind and `<sha> --not --remotes` (positive revision first) [US1]
- [ ] T008 Dangling commits: `git fsck --unreachable`, list file, bundle via temporary
      `refs/migration/dangling/<n>` and delete the refs in `finally` [US1]
- [ ] T009 Local state: `.specify/feature.json`, Agent Manager reference, worktree list, untracked
      files, uncommitted patch [US1]
- [ ] T010 Archive (`snapshot` only): evidence, `out`, DXC and encoder caches, spec cache, skills
      mirror, `targets/*/doom3`, `build*/bin`, all logs; `-IncludeWorktrees` opt-in [US1]
- [ ] T011 Inventory: every ignored path with size, files, `ARCHIVED`/`PARTIAL`/`excluded`; print totals [US1][US4]
- [ ] T012 Machine facts JSON (schema 2), `local-only-refs.txt`, `MIGRATION-NOTES.md`,
      bootstrap copy of `deploy-repo.ps1` [US1]
- [ ] T013 Checksums over every produced file; summary with profile, HEAD, submodule, archive totals [US1]

## Phase 3 — Restore script `scripts/deploy-repo.ps1`

- [ ] T014 [P] Checksum verification (fail closed; `-SkipChecksum` explicit) and prerequisite table
      with `-CheckOnly` [US2][US4]
- [ ] T015 [P] `git init`, `core.longpaths`, `core.autocrlf=false`, fetch all heads/tags, default
      branch from facts [US2]
- [ ] T016 Origin + upstreams: set the recorded remote URL, optional fetch (`-NoNetwork`), restore
      each branch's upstream [US2]
- [ ] T017 Submodule: pinned SHA check, offline clone from the bundle, `gitlink` equality, clean tree,
      reset the submodule origin to `.gitmodules` [US2]
- [ ] T018 Local state restore: feature pointer; untracked files without clobbering; diff reported only [US2]
- [ ] T019 Archive restore: evidence, out, caches, skills mirror, `build*/bin`, logs; archived target
      trees reported but not copied [US2]
- [ ] T020 Skills: `sync-skills.ps1` then `verify-skills.ps1`, exit 0 required [US2]
- [ ] T021 Targets: `new-target.ps1` + `apply-target-patches.ps1` per target, exit 0 required; then
      `test-target-patches.ps1` controls [US2]
- [ ] T022 Optional `-Build`: bundled CMake or PATH, `cmd /c` redirection, `-- -m -nr:false`,
      artifact existence [US2]
- [ ] T023 Final proof: `git status` clean except restored untracked paths; `out/deploy-report.txt`;
      exact exit codes 0/1/2 [US2][US4]

## Phase 4 — Verification gate and evidence

- [ ] T024 `scripts/test-migration.ps1`: portable + snapshot metadata, no-game-data check, checksum
      check, offline restore, `HEAD^{tree}` equality, `finally` cleanup, `-KeepTemp` [US3]
- [ ] T025 Run the gate on the recorded host; paste commands, exit codes and counts into the PR body [US3]
- [ ] T026 Run the full snapshot + restore with targets (and `-Build` when the toolchain is present);
      record `checksums verified`, `0 mismatches`, `25/25`, tree hash, artifact sizes [US2]
- [ ] T027 Update `MIGRATION-NOTES.md` and `IGNORED-INVENTORY.txt` wording if the run exposes a gap [US1]
- [ ] T028 Open the implementation PR with `Refs #77`, evidence, risk/rollback; merge after
      independent review

## Dependencies

- Phase 1 must merge before Phase 2's worktree is created (`docs/GIT_WORKFLOW.md` §1).
- T008 requires T006 (bundle file path); T011 requires T010; T013 requires T006–T012.
- T019 requires T018; T021 requires T017; T024 requires T013 and T023.

## Out of scope (do not implement here)

- Worktree re-linking, Agent Manager state installation, cloud storage, encryption, retention.
- Game data, engine/CMake/submodule changes, renderer work.
