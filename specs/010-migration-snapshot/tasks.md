# Tasks: 010-migration-snapshot

**Input**: `spec.md`, `plan.md`
**Verification command**: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-migration.ps1`

**Status (2026-11-01, branch `spec-010-impl` @ `a1dc01c`)**: the scripts landed in `a314ef6`
(PR #79) with these boxes unticked. This pass audits every task against the shipped code, fixes the
two defects the audit proved, and records what was **not** run. A task is `DONE` only when a command
in [§ Evidence](#evidence-2026-11-01) produced the quoted exit code; nothing is ticked on inspection.

Legend: `[P]` = can run in parallel with its neighbours; `[USn]` = user story from `spec.md`.

## Phase 1 — Publish the specification (docs PR)

- [x] T001 Write `specs/010-migration-snapshot/spec.md` (problem, stories, FR-1..FR-10, evidence) — **DONE** (added by `b21afc9`, PR #78) [US1]
- [x] T002 Write `specs/010-migration-snapshot/plan.md` (pipeline, decisions, risks, rollback) — **DONE** (same commit) [US1]
- [x] T003 Write `specs/010-migration-snapshot/tasks.md` (this file) — **DONE** (same commit) [US1]
- [x] T004 Open the docs PR with `Refs #77`, evidence block and risk/rollback; merge after independent review — **DONE** (`b21afc9` is on `main`; the review record lives in the PR history and was not re-verified here) [US1]

## Phase 2 — Snapshot script `scripts/migrate-repo.ps1`

- [x] T005 [P] Preflight: repo check, git check, non-empty `-OutDir` refusal (`-Force`), dirty-tree
      refusal (`-AllowDirty` saves `git diff --binary --output` plus untracked copies) — **DONE** (E2: non-empty → exit 2, dirty → exit 1 naming the path, `-AllowDirty` → exit 0 with the patch and untracked copies; exit code corrected from 1 to 2, see D2) [US4]
- [x] T006 [P] Bundles: `--branches --tags HEAD`, submodule bundle, `git bundle verify` on both — **DONE** (E1: both `bundle verify` gates exit 0 on snapshot and portable) [US1]
- [x] T007 Local-only refs: upstream/ahead/behind and `<sha> --not --remotes` (positive revision first) — **DONE** (E3: `local-only-refs.txt` lists the unpushed branch with a count > 0; `main` row records `upstream=origin/main`, `ahead`/`behind`) [US1]
- [x] T008 Dangling commits: `git fsck --unreachable`, list file, bundle via temporary
      `refs/migration/dangling/<n>` and delete the refs in `finally` — **DONE** (E3: orphan commit → bundle written, `bundle verify` exit 0, `list-heads` shows `refs/migration/dangling/0`, `git for-each-ref refs/migration` empty afterwards) [US1]
- [x] T009 Local state: `.specify/feature.json`, Agent Manager reference, worktree list, untracked
      files, uncommitted patch — **DONE for 4 of 5** (E2: feature pointer, worktree list, untracked file, 1,536-byte patch); `agent-manager.json.reference` is **NOT RUN**: the file is absent on this host and fabricating live harness state is not safe [US1]
- [x] T010 Archive (`snapshot` only): evidence, `out`, DXC and encoder caches, spec cache, skills
      mirror, `targets/*/doom3`, `build*/bin`, all logs; `-IncludeWorktrees` opt-in — **DONE for the trees present on this host** (E1, E7: `out`, `.kilo/skills`, `targets/*/doom3` 33.8 MB / 2,156 files each, `build/bin`, `build-logs`); `showcase/evidence`, `shaders/tools/.cache`, `content/tools/.cache`, `.specify/integrations/.cache`, `.playwright-mcp`, `-IncludeWorktrees` are **NOT RUN** (absent on this host; same `Copy-ArchiveTree` path as the archived trees) [US1]
- [x] T011 Inventory: every ignored path with size, files, `ARCHIVED`/`PARTIAL`/`excluded`; print totals — **DONE** (E3: no malformed rows; E7 shows `[PARTIAL] 388.6 MB build`, `[ARCHIVED] 33.8 MB 2156 files targets/…`) [US1][US4]
- [x] T012 Machine facts JSON (schema 2), `local-only-refs.txt`, `MIGRATION-NOTES.md`,
      bootstrap copy of `deploy-repo.ps1` — **DONE** (E3: `schema=doom3-migration/2`, `danglingBundled=true`, submodule SHA recorded, bootstrap copy byte-equal to `scripts/deploy-repo.ps1`) [US1]
- [x] T013 Checksums over every produced file; summary with profile, HEAD, submodule, archive totals — **DONE** (E1: 61 entries for 62 files; E3: 11 entries for 12 files — the manifest excludes only itself) [US1]

## Phase 3 — Restore script `scripts/deploy-repo.ps1`

- [x] T014 [P] Checksum verification (fail closed; `-SkipChecksum` explicit) and prerequisite table
      with `-CheckOnly` — **DONE** (E4: tampered snapshot → exit 2 naming the file; `-SkipChecksum` → exit 0; clean → exit 0 with `checksums verified`; `-CheckOnly` writes nothing) [US2][US4]
- [x] T015 [P] `git init`, `core.longpaths`, `core.autocrlf=false`, fetch all heads/tags, default
      branch from facts — **DONE** (E1: 5 branches restored, `HEAD^{tree}` equal on both sides, offline) [US2]
- [x] T016 Origin + upstreams: set the recorded remote URL, optional fetch (`-NoNetwork`), restore
      each branch's upstream — **DONE** (E5: `origin -> https://github.com/yhyu13/DOOM3_AINative.git`, `upstreams restored: 2`, `branch.main.merge=refs/heads/main`) [US2]
- [x] T017 Submodule: pinned SHA check, offline clone from the bundle, `gitlink` equality, clean tree,
      reset the submodule origin to `.gitmodules` — **DONE** (E1: gitlink `a9c49da5afb1`, clone from bundle, tree clean; E6 restores it again during `-Build`) [US2]
- [x] T018 Local state restore: feature pointer; untracked files without clobbering; diff reported only — **DONE** (E5: `.specify/feature.json` restored, `restored 1, skipped 0`, tracked diff printed as `not applied automatically`) [US2]
- [x] T019 Archive restore: evidence, out, caches, skills mirror, `build*/bin`, logs; archived target
      trees reported but not copied — **DONE** (E7: `archived target copies kept … (not restored; targets are rebuilt from pristine + patch)`, `archive restored: out, .kilo/skills, build/bin, build-logs/build`) [US2]
- [x] T020 Skills: `sync-skills.ps1` then `verify-skills.ps1`, exit 0 required — **DONE** (E1: `25 skills checked, 25 OK, byte-identical`; the explicit restored-tree run exits 0) [US2]
- [x] T021 Targets: `new-target.ps1` + `apply-target-patches.ps1` per target, exit 0 required; then
      `test-target-patches.ps1` controls — **DONE** (E1/E6/E7: `72 files verified, 0 mismatches` and `66 files verified, 0 mismatches`; controls `8 PASS`) [US2]
- [x] T022 Optional `-Build`: bundled CMake or PATH, `cmd /c` redirection, `-- -m -nr:false`,
      artifact existence — **DONE** (E6: exit 0 with VS2022 Community + SDK 10.0.26100.0; both artifacts PE `0x8664`; `build/_build.log` retained) [US2]
- [x] T023 Final proof: `git status` clean except restored untracked paths; `out/deploy-report.txt`;
      exact exit codes 0/1/2 — **DONE for 0 and 2** (E1/E5/E6/E7 exit 0, E4 exit 2, report written, `clean except 1 restored untracked file(s)`); the exit-1 gate path (−submodule mismatch, skills/patch/status failure) is **NOT RUN**, no injection point without tampering [US2][US4]

## Phase 4 — Verification gate and evidence

- [x] T024 `scripts/test-migration.ps1`: portable (no `archive/`) + snapshot metadata, no-game-data
      check, checksum check, offline full restore with targets, `HEAD^{tree}` equality, `0 mismatches`
      for both target patches, `verify-skills.ps1` 25/25, `finally` cleanup, `-KeepTemp` — **DONE** (E1: exit 0, `22 gate(s) passed, 0 failed`; the marker file and temp root are removed by `finally`) [US3]
- [x] T025 Run the gate on the recorded host; paste commands, exit codes and counts into the PR body — **DONE** (E1; the PR body repeats the gate line and counts) [US3]
- [x] T026 Run the snapshot + restore with `-Build` when the toolchain is present (targets are covered by T024);
      record `checksums verified`, `0 mismatches`, `25/25`, tree hash, artifact sizes — **DONE** (E6) [US2]
- [x] T027 Update `MIGRATION-NOTES.md` and `IGNORED-INVENTORY.txt` wording if the run exposes a gap — **DONE, no wording change needed** (the two gaps were code, D1/D2; the run exposed no missing or misleading note) [US1]
- [x] T028 Open the implementation PR with `Refs #77`, evidence, risk/rollback; merge after
      independent review — **DONE for opening**; merge is left to the reviewer session, per the task brief [US1]

## Dependencies

- Phase 1 must merge before Phase 2's worktree is created (`docs/GIT_WORKFLOW.md` §1).
- T008 requires T006 (bundle file path); T011 requires T010; T013 requires T006–T012.
- T019 requires T018; T021 requires T017; T024 requires T013 and T023.

## Out of scope (do not implement here)

- Worktree re-linking, Agent Manager state installation, cloud storage, encryption, retention.
- Game data, engine/CMake/submodule changes, renderer work.

---

## Evidence (2026-11-01)

Host: Windows, git 2.55.0.windows.3, PowerShell 5.1, VS2022 Community, Windows SDK 10.0.26100.0,
CMake on PATH, emsdk absent. No game data used. Full logs were written outside the repository; the
lines below are quoted verbatim from them.

### E1 — end-to-end gate

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-migration.ps1
```

exit **0**, last line `22 gate(s) passed, 0 failed`, with:

```
PASS migrate portable        exit 0
PASS migrate snapshot        exit 0
PASS snapshot layout         8/8 present
PASS checksum coverage       61 entries for 62 files
PASS no game data in snapshot   0 pk4/pk3/pak
PASS no build intermediates     0 CMakeFiles/__pycache__
PASS deploy -CheckOnly       exit 0, checksums verified=True
PASS deploy restore          exit 0
PASS target patches verified 2 target(s) reported 0 mismatches
PASS skills gate in restore  25/25, byte-identical
PASS HEAD tree equality      03deef90e3ce4d95ed232b48b5f768db3473666a / 03deef90e3ce4d95ed232b48b5f768db3473666a
PASS restored status clean   0 dirty path(s)
PASS submodule gitlink       a9c49da5afb1
PASS archive restored        out/gate-marker.txt byte-equal
PASS detached migrate        exit 0, branch=null=True, head matches=True
PASS detached restore tree   exit 0, 03deef90e3ce4d95ed232b48b5f768db3473666a / 03deef90e3ce4d95ed232b48b5f768db3473666a
PASS verify-skills (restored) exit 0
```

### E2 — preflight and `-AllowDirty`

```
# non-empty -OutDir, no -Force
powershell -File scripts/migrate-repo.ps1 -Profile portable -OutDir <tmp>\nonempty
  → exit 2, "Output directory is not empty: … (use -Force to overwrite files in it)", keep.txt intact

# dirty worktree, one untracked file
powershell -File scripts/migrate-repo.ps1 -Profile portable -OutDir <tmp>\x
  → exit 1, names _probe_dirty.txt

# same dirty worktree, -AllowDirty
powershell -File scripts/migrate-repo.ps1 -Profile portable -AllowDirty -OutDir <tmp>\y
  → exit 0; local-state/worktree-untracked.txt, local-state/untracked/_probe_dirty.txt,
    local-state/specify-feature.json, local-state/worktree-list.txt, local-state/worktree-uncommitted.patch

# one tracked file modified, -AllowDirty
  → exit 0; worktree-uncommitted.patch 1,536 bytes; restore prints
    "the source had uncommitted tracked changes … (not applied automatically)"
```

### E3 — dangling commits, refs, facts, inventory, checksums

```
git commit-tree "HEAD^{tree}" -p HEAD -m 'probe'        → 3c1eb3c4a625 (unreachable, no ref touched)
powershell -File scripts/migrate-repo.ps1 -Profile portable -OutDir <tmp>\snap-a   → exit 0
  dangling-commits.bundle written
  git bundle verify <bundle>                           → exit 0
  git bundle list-heads <bundle>                       → refs/migration/dangling/0
  git for-each-ref refs/migration                      → empty (temp refs deleted in finally)
  unreachable-commits.txt                              → names the probe commit
  machine-facts.json   schema=doom3-migration/2  danglingBundled=True  submodule.sha=a9c49da5afb1
  deploy-repo.ps1 in the snapshot                      → SHA-256 equal to scripts/deploy-repo.ps1
  local-only-refs.txt                                  → spec-010-impl, local-only-commits > 0; main upstream=origin/main
  IGNORED-INVENTORY.txt                                → rows only in the form "[STATUS] <size> MB <n> files <path>"
  checksums.sha256                                     → 11 entries for 12 files
# snapshot profile with two unreachable probes present
  summary line: "dangling  : 2 (bundled)"
```

### E4 — checksum fail-closed

```
# one byte appended to <snap>/MIGRATION-NOTES.md after the manifest was written
powershell -File <snap>/deploy-repo.ps1 -Bundle <snap> -Dest <tmp>/check-x -CheckOnly
  → exit 2, "MISMATCH MIGRATION-NOTES.md"
powershell -File <snap>/deploy-repo.ps1 -Bundle <snap> -Dest <tmp>/check-x -CheckOnly -SkipChecksum
  → exit 0
# file restored to its recorded bytes
powershell -File <snap>/deploy-repo.ps1 -Bundle <snap> -Dest <tmp>/check-ok -CheckOnly
  → exit 0, "checksums verified"
```

### E5 — restore details (no build, no targets)

```
powershell -File <snap>/deploy-repo.ps1 -Bundle <snap> -Dest <tmp>/restore-a -NoNetwork -SkipTargets
  → exit 0
  origin -> https://github.com/yhyu13/DOOM3_AINative.git
  upstreams restored: 2
  branch.main.remote=origin, branch.main.merge=refs/heads/main
  .specify/feature.json restored
  untracked files from the source worktree: restored 1, skipped 0 that already exist
  the source had uncommitted tracked changes … (not applied automatically)
  out/deploy-report.txt written
  git status  : clean except 1 restored untracked file(s) from the source
```

### E6 — snapshot + restore with `-Build` (T026)

```
powershell -File scripts/migrate-repo.ps1 -Profile snapshot -OutDir <tmp>\snap
  → exit 0; 114.5 MB in 62 files; HEAD spec-010-impl @ a1dc01c; submodule a9c49da5afb1
powershell -File <tmp>\snap\deploy-repo.ps1 -Bundle <tmp>\snap -Dest <tmp>\restore -NoNetwork -Build
  → exit 0
  checksums verified
  win11-amd64     copy + patch verified (Applied patch: 72 files verified, 0 mismatches)
  win11-amd64-hd  copy + patch verified (Applied patch: 66 files verified, 0 mismatches)
  patch-apply controls: 8 PASS
  25 skills checked, 25 OK, 0 failed.  /  25 skills in both trees, byte-identical.
  DOOM3.exe   17,771,520 bytes  sig=PE machine=0x8664
  DOOM3XP.exe 17,966,080 bytes  sig=PE machine=0x8664
  HEAD^{tree} 03deef90e3ce4d95ed232b48b5f768db3473666a on both sides
  git status  : clean
```

### E7 — target copies archived, then rebuilt (T010/T019)

```
# snapshot taken inside the E6 restore, whose targets/*/doom3 exist
powershell -File <restore>\scripts\migrate-repo.ps1 -Profile snapshot -OutDir <tmp>\snap2
  → exit 0; 249.6 MB in 4,380 files; archive 6 trees, 187.7 MB; excluded 270.4 MB
  IGNORED-INVENTORY.txt:
    [PARTIAL]     388.6 MB    607 files  build
    [ARCHIVED]     33.8 MB   2156 files  targets/win11-amd64/doom3
    [ARCHIVED]     33.8 MB   2156 files  targets/win11-amd64-hd/doom3
    [ARCHIVED]      0.3 MB     51 files  .kilo/skills
    [ARCHIVED]      0.0 MB      1 files  out
powershell -File <tmp>\snap2\deploy-repo.ps1 -Bundle <tmp>\snap2 -Dest <tmp>\restore2 -NoNetwork
  → exit 0
  archived target copies kept under … (not restored; targets are rebuilt from pristine + patch)
  archive restored: out (test evidence), .kilo/skills (mirror), build/bin, build-logs/build
  72 files verified, 0 mismatches / 66 files verified, 0 mismatches
  git status  : clean
```

### E8 — defects found and fixed by this pass

**D1 (blocking).** `migrate-repo.ps1` aborted on any git stderr warning. PowerShell 5.1 promotes
each stderr line of a native command to `NativeCommandError`, and the script runs with
`ErrorActionPreference = 'Stop'`; `git count-objects -v` exits 0 but warns
`"garbage found: …/.git/worktrees/<stale>/refs"`, which killed the run in "Recording machine facts"
*after* the bundles and archive were written. First gate run: exit 1, `FAIL migrate portable
exit 1`, no `machine-facts.json`, `checksums.sha256` or `deploy-repo.ps1`. Same class fixed in
`deploy-repo.ps1` (`git cat-file`, `git remote set-url`, `vswhere`, and the child-PowerShell runner).
Fix: an `Invoke-Native` helper captures stderr as data and lets the caller decide from the exit code.
After: E1 exit 0.

**D2.** A non-empty `-OutDir` exited 1, while `spec.md` US4.2/FR-7 require 2 (and `deploy-repo.ps1`
already exits 2 for a non-empty `-Dest`). Observed before: exit 1. After: exit 2 (E2).

### Not run, and why

| Item | Reason |
|---|---|
| `local-state/agent-manager.json.reference` | `.kilo/agent-manager.json` absent on this host; creating a fake one would corrupt live harness state |
| `showcase/evidence`, `shaders/tools/.cache`, `content/tools/.cache`, `.specify/integrations/.cache`, `.playwright-mcp` archive entries | absent on this host; same code path as the archived trees (E1, E7) |
| `-IncludeWorktrees` archive | multi-GB opt-in; not needed to prove the mechanism |
| deploy exit-1 gate path | needs an injected failure (submodule SHA mismatch, skills/patch/status); 0 and 2 were observed, 1 was not |
| US1.2's 142-commit case | this host had 0–2 unreachable commits; the bundling mechanism is proven, the volume is not |
