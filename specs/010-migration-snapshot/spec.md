# Feature specification: snapshot and restore a checkout with the state git does not carry

**Feature**: `010-migration-snapshot`
**Status**: specification (2026-10-30). Implementation is proposed in the same issue; the scripts are
new files, no engine or build-system change.
**Input**: "迁移也有 snapshot 的意义，size 是次要的，关键数据需要保留备份" — a repository move must also
be a backup, and a restore must be provable.

---

## 1. Problem

`git clone` moves commits, not a working project. Five classes of state are lost today:

1. **Commits no remote reaches.** Local-only branches, the `backup/*` safety snapshot, and commits
   orphaned by rebase or by removing a worktree. `git clone` cannot restore them, and `git bundle`
   in git 2.25 refuses to pack unreachable commits by raw SHA ("Refusing to create empty bundle").
2. **The pinned submodule.** `doom3_orig/doom3` is 34 MB of history that a default clone does not
   fetch; on an offline or rate-limited host it is the difference between restoring and not.
3. **Ignored but stateful files.** `.specify/feature.json`, Agent Manager recovery state, untracked
   work-in-progress files, and uncommitted diffs.
4. **Ignored evidence and expensive artifacts.** Captured showcase evidence (only reproducible with
   game data), test output, the pinned DXC toolchain cache (~104 MB), texture-encoder cache, built
   executables and build/test logs.
5. **The definition of the engine working copies.** `targets/*/doom3` are git-ignored scratch trees;
   copying them moves whatever drifted state they hold, while rebuilding them from pristine + patch
   is reproducible and verifiable.

A restore also has to be *provable*. "I copied it" is not evidence; a tree hash, a checksum file, a
patch-verification exit code, and a clean `git status` are.

This spec treats a migration as a **snapshot**: completeness of key data wins over package size.

---

## 2. What this feature is, and is not

**Is**

- `scripts/migrate-repo.ps1`: writes a self-contained, checksummed snapshot directory
  (`repo-all.bundle`, `doom3-submodule.bundle`, `dangling-commits.bundle`, `archive/`, `local-state/`,
  `machine-facts.json`, `IGNORED-INVENTORY.txt`, `MIGRATION-NOTES.md`, `checksums.sha256`) and a
  bootstrap copy of the restore script.
- `scripts/deploy-repo.ps1`: restores that snapshot into a fresh directory — repository, refs,
  upstreams, submodule (offline from the bundle), targets rebuilt from pristine + patch, archive data,
  skills gate — and fails closed when a gate does not hold.
- `scripts/test-migration.ps1`: one command that proves both directions end to end in a temporary
  directory and cleans up after itself.
- Two profiles: `snapshot` (default, key data archived) and `portable` (git + stateful files only).

**Is not**

- A remote backup service, encryption, deduplication, or incremental snapshots.
- A way to move Agent Manager linked worktrees: their gitdir metadata contains absolute paths.
- A substitute for game data, which is not redistributable and stays out of every artifact.
- A build system change: no CMake, engine, or submodule edit.

---

## 3. User stories and acceptance scenarios

### US1 — Snapshot before leaving a machine (P1)

**Independent test**: run migrate into an empty directory; every bundle verifies, the checksum file
lists every produced file, and the inventory names every ignored path with its size and status.

1. **Given** a clean checkout at `main`, **when** `migrate-repo.ps1 -Profile snapshot -OutDir <tmp>`
   runs, **then** it exits 0, writes `repo-all.bundle` and `doom3-submodule.bundle`, and both pass
   `git bundle verify`.
2. **Given** unreachable commits exist, **when** the snapshot is written, **then**
   `dangling-commits.bundle` exists, lists one ref per commit, and verifies; the 142-commit case on
   the recorded host produces a working bundle despite git 2.25 refusing raw-SHA bundling.
3. **Given** key ignored data exists, **when** the snapshot is written, **then** `archive/` holds
   showcase evidence, `out/`, the DXC and encoder caches, the skills mirror, both target working
   copies, built binaries (`build*/bin`) and build logs, and `IGNORED-INVENTORY.txt` marks each path
   `ARCHIVED`, `PARTIAL`, or `excluded` with its size.
4. **Given** a portable move is wanted, **when** `-Profile portable` is used, **then** no `archive/`
   directory is produced and `MIGRATION-NOTES.md` still names what is missing and how to recreate it.

### US2 — Restore on a new machine, offline (P1)

**Independent test**: deploy the snapshot into a fresh directory with `-NoNetwork`; the tree hash
matches the source, targets verify against their patches, and the skills gate is green.

1. **Given** a snapshot directory, **when** `deploy-repo.ps1 -Bundle <dir> -Dest <fresh>` runs,
   **then** it exits 0 and prints `checksums verified`.
2. **Given** the submodule bundle is present, **when** `-NoNetwork` is set, **then** the submodule is
   cloned from the bundle, checked out at the recorded gitlink SHA, and reported clean.
3. **Given** the restore completes, **then** `git rev-parse HEAD^{tree}` equals the source tree hash,
   every local branch exists, upstreams point at `origin/<same-name>`, and `git status` is clean
   except untracked files intentionally restored from `local-state/untracked/`.
4. **Given** target copies, **when** the restore runs, **then** each `targets/<name>/doom3` is created
   from pristine + its tracked patch and `apply-target-patches.ps1` reports `0 mismatches`; the
   archived target tree is kept for inspection but never copied over the rebuilt one.
5. **Given** `-Build`, **when** VS2022 + SDK 10.0.26100 are present, **then** `DOOM3.exe` and
   `DOOM3XP.exe` exist afterwards and the build log is retained; when the toolchain is missing the
   script fails with a named prerequisite rather than a silent skip.
6. **Given** a snapshot taken while the worktree was dirty, **when** `-AllowDirty` was used, **then**
   the untracked files are restored without clobbering existing files and the tracked diff is
   reported but not applied automatically.

### US3 — One command proves both directions (P1)

1. **Given** a clean checkout, **when** `scripts/test-migration.ps1` runs, **then** it exits 0 and
   prints each gate (migrate, bundle verify, checksum check, restore, tree hash, patch verify,
   skills) with its result.
2. **Given** any gate fails, **then** the script exits 1, prints the failing gate, and still removes
   its temporary directories unless `-KeepTemp` was passed.

### US4 — Safety properties (P2)

1. **Given** a dirty worktree, **when** migrate runs without `-AllowDirty`, **then** it exits 1 and
   names the dirty paths.
2. **Given** a non-empty output directory or destination, **when** the scripts run without `-Force`
   (migrate) or at all (deploy), **then** they refuse with exit 2 and do not overwrite.
3. **Given** any snapshot, **then** it contains no `*.pk4`/`*.pk3`/`*.pak`, no `CMakeFiles`, no
   `*.obj`, and no absolute path recorded in a tracked file.
4. **Given** an unknown or failed copy, **then** the script exits non-zero; it never reports success
   on a partial snapshot.

---

## 4. Functional requirements

- **FR-1** Snapshot completeness. The `snapshot` profile archives the key ignored data listed in US1.3
  and records every other ignored path in `IGNORED-INVENTORY.txt` with size, file count, and reason.
- **FR-2** Ref completeness. Every local branch, tag, and HEAD is bundled; unreachable commits are
  bundled through temporary refs and the refs are removed afterwards.
- **FR-3** Offline submodule. A bundle of the submodule is written and used by the restore when
  present; the submodule's own `origin` is reset to the URL in `.gitmodules`.
- **FR-4** Reproducible target copies. Restore never seeds a target from the archive; it runs
  `new-target.ps1` + `apply-target-patches.ps1` and fails if any manifest hash mismatches.
- **FR-5** Integrity. `checksums.sha256` covers every file in the snapshot; restore verifies it before
  touching the destination and fails closed on a mismatch (`-SkipChecksum` opts out explicitly).
- **FR-6** Provenance. `machine-facts.json` records OS, PowerShell, git/node/uv/py/gh, Visual Studio
  and MSVC toolsets, Windows SDKs, emsdk and Emscripten version, Chrome, git config, refs, submodule
  SHA, archived trees, and the number of excluded bytes.
- **FR-7** Fail-loud behaviour. Non-zero exit codes: 1 for a rejected or failed gate, 2 for an
  environment error (missing bundle, missing git, non-empty destination).
- **FR-8** No host-specific paths in tracked files. The scripts derive every path from
  `$PSScriptRoot`, environment variables, or explicit parameters.
- **FR-9** PowerShell 5.1 compatibility: no `&&`, no ternary, no `Start-Process` redirection for
  MSBuild (node-reuse workers keep the pipe open and hang the caller; use `cmd /c` plus `-nr:false`).
- **FR-10** A single verification command (`scripts/test-migration.ps1`) that a reviewer can rerun
  without game data.

---

## 5. Key entities

| Entity | Meaning |
|---|---|
| Snapshot directory | One self-contained set: bundles, archive, state, facts, checksums, notes |
| Profile | `snapshot` (key data archived) or `portable` (git + stateful files) |
| Archive entry | An ignored tree copied with its repository-relative path |
| Inventory row | Ignored path, size, file count, `ARCHIVED`/`PARTIAL`/`excluded` |
| Gate | A command with an exit code that the restore must satisfy |

---

## 6. Success criteria

- `scripts/test-migration.ps1` exits 0 on a machine with git, PowerShell 5.1, and no game data.
- A snapshot is restorable with `-NoNetwork` and yields an identical `HEAD^{tree}`.
- Every ignored path on the source appears in the inventory with a size; nothing is silently omitted.
- The reviewer can reproduce the gate from the PR body alone.

---

## 7. Out of scope

- Worktree re-linking, Agent Manager state installation, remote/cloud storage, retention policy.
- Game data acquisition, retail asset layout, or any EULA-bound content.
- Engine, renderer, shader, CMake, or submodule changes.

---

## 8. Evidence on record (source machine, 2026-10-30)

| Fact | Value | How measured |
|---|---|---|
| Tracked content | 454 files, 12.2 MB | `git ls-files` |
| Snapshot size | 569.8 MB, 4,766 files | directory scan |
| Bundles | repo 40.4 MB, submodule 8.7 MB, dangling 44.4 MB (142 refs) | `git bundle verify`, `list-heads` |
| Archive | 475.6 MB across 12 trees | `IGNORED-INVENTORY.txt` |
| Excluded | 4,844.1 MB (worktrees 3,305 MB; build intermediates) | inventory |
| Restore | exit 0, `HEAD^{tree}` equal, targets 72/72 and 66/66, skills 25/25 | deploy run |
| Build (optional) | `DOOM3.exe` 17,304,576 B, `DOOM3XP.exe` 17,488,384 B, PE 0x8664 | `-Build` run |
