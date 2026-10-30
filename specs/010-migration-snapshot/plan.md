# Implementation plan: 010-migration-snapshot

**Feature**: `010-migration-snapshot`
**Branch**: `docs/77-migration-snapshot` (spec) → `feat/77-migration-snapshot` (implementation)
**Scope**: three new PowerShell scripts; no engine, build-system, or submodule change.

---

## 1. Technical context

| Item | Value |
|---|---|
| Language | PowerShell 5.1 (Windows), no `&&`, no ternary |
| Runtime deps | `git` (2.25 verified), `robocopy`, `cmd` |
| Optional deps | Visual Studio 2022 v143 + Windows SDK 10.0.26100 + bundled CMake, for `-Build` |
| Data deps | none (no game assets needed for any gate) |
| Temp footprint | ~1 GB during `test-migration.ps1`, removed on success and failure |
| Output | a self-contained snapshot directory + a restore report under `out/` |

The scripts follow the repository conventions: PowerShell 5.1, exits 0/1/2, English messages,
`$PSScriptRoot`-relative paths, no secrets, no machine-specific absolute paths in tracked files.

---

## 2. Architecture

### 2.1 `scripts/migrate-repo.ps1`

Pipeline, each step fail-closed:

1. **Preflight** — repo root, git present, output empty or `-Force`, worktree/submodule cleanliness
   (`-AllowDirty` saves a byte-exact diff via `git diff --binary --output` and copies untracked files).
2. **Bundles** — `git bundle create repo-all.bundle --branches --tags HEAD`; submodule bundle;
   `git bundle verify` on both.
3. **Local-only refs** — per-branch upstream, ahead/behind, and commits not reachable from any remote
   (`<sha> --not --remotes`, positive revision first; the reverse order silently returns 0 in git 2.25).
4. **Dangling commits** — `git fsck --unreachable`; write the list; bundle by pinning each commit to a
   temporary `refs/migration/dangling/<n>`, bundling those refs, then deleting them in `finally`.
5. **Local state** — spec feature pointer, Agent Manager reference, worktree list, untracked files.
6. **Archive** (`snapshot` only) — robocopy ignored trees with repository-relative paths.
7. **Inventory** — every `git status --ignored` path with size, file count, and
   `ARCHIVED`/`PARTIAL`/`excluded`; totals printed.
8. **Machine facts** — JSON, schema `doom3-migration/2`.
9. **Notes, bootstrap script, checksums** — `MIGRATION-NOTES.md`, `deploy-repo.ps1`, SHA-256 of every
   produced file.

Archive set: `showcase/evidence`, `out`, `shaders/tools/.cache`, `content/tools/.cache`,
`.specify/integrations/.cache`, `.kilo/skills`, `targets/*/doom3`, `build*/bin`, all `*.log` under the
root and build dirs, and optionally `.kilo/worktrees` (`-IncludeWorktrees`). Build intermediates,
`targets/*/build`, and game data are never archived; the inventory records their size and reason.

### 2.2 `scripts/deploy-repo.ps1`

Order matters:

1. Verify `checksums.sha256` (fail closed; `-SkipChecksum` is explicit).
2. Prerequisite table (`-CheckOnly` stops here): git, node, uv, py, gh, VS + SDK + CMake (required
   only with `-Build`), emsdk (warn).
3. `git init` in an empty destination, `core.longpaths=true`, `core.autocrlf=false`, fetch all heads
   and tags from the bundle, set HEAD to the recorded default branch.
4. Add `origin` (URL from `machine-facts.json`), optional fetch (`-NoNetwork` skips), restore branch
   upstreams from the recorded ref table.
5. Submodule: pinned SHA from `ls-tree`, clone from `doom3-submodule.bundle` with
   `protocol.file.allow=always`, verify HEAD equals the gitlink and the tree is clean, reset the
   submodule's own `origin` to the `.gitmodules` URL.
6. Restore `local-state` (feature pointer; untracked files without clobbering; diff reported only).
7. Restore `archive/` working parts: evidence, out, caches, skills mirror, `build*/bin`, logs. Archived
   target trees are reported but never copied.
8. `sync-skills.ps1` then `verify-skills.ps1` (must exit 0).
9. For every `targets/*/CMakeLists.txt`: `new-target.ps1` + `apply-target-patches.ps1`, exit 0
   required; then `test-target-patches.ps1` negative controls.
10. Optional `-Build`: `cmd /c` redirection, `-- -m -nr:false`, artifact existence checks.
11. Final proof: `git status` clean except restored untracked files; write `out/deploy-report.txt`.

### 2.3 `scripts/test-migration.ps1`

Self-contained gate:

1. Create `<temp>/snap` and `<temp>/restore`.
2. Run migrate `-Profile portable` (fast path) and assert: exit 0, no `archive/`, bundles verify.
3. Run migrate `-Profile snapshot -NoDanglingBundle` over the same tree; assert archive, inventory,
   notes, facts, checksums exist.
4. Assert no `*.pk4`/`*.pk3`/`*.pak` or `CMakeFiles` inside the snapshot.
5. Run `deploy-repo.ps1 -CheckOnly` (checksums) and a full restore with `-NoNetwork -SkipTargets`
   (fast), then assert `HEAD^{tree}` equality. A second restore with targets is out of the gate's
   fast path; the PR evidence records the full-target restore separately.
6. `finally`: remove both directories unless `-KeepTemp`.
7. Exit 0/1 with one summary line per gate.

Decision: the gate deliberately skips target recreation and builds to stay under ~2 minutes; the
acceptance evidence for those paths lives in the issue and the PR body, produced by the same scripts.

---

## 3. Design decisions and rejected alternatives

| Decision | Why |
|---|---|
| Bundle all refs instead of a mirror clone | A mirror clone cannot be created from a dirty worktree and carries absolute paths in `config`; a bundle is a single verifiable file. |
| Temporary refs for dangling commits | git 2.25 refuses raw-SHA bundles; verified: a temp ref bundles 7 MB, raw SHA yields "empty bundle". |
| Archive with relative paths | Restore can map `archive/<rel>` back to `<dest>/<rel>` without metadata. |
| Rebuild targets instead of restoring them | The source copy can drift from its patch (the recorded host had 65/72 matching); only pristine + patch is reproducible. |
| Do not restore `agent-manager.json` | Absolute paths from another host; the file is kept as a reference only. |
| `cmd /c` + `-nr:false` for MSBuild | `Start-Process -RedirectStandardOutput` hangs on node-reuse workers (observed: 15-minute hang after artifacts were already written). |
| Checksums over the whole snapshot | A restore must not silently accept a truncated archive. |

---

## 4. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Snapshot is large (hundreds of MB to GB with worktrees) | Size is not the priority; the inventory makes the cost explicit, and `portable` exists for lean moves. |
| Long paths on Windows | `core.longpaths=true` on restore; destination length warning above 150 chars. |
| Concurrent work on the source repo | The bundle is a point-in-time ref set; facts record HEAD; the PR says to re-run migrate at the actual cutover. |
| Partial copy reported as success | robocopy exit >= 8 and every gate exit code are fatal. |
| Line-ending drift between source worktree and checkout | Equality is defined by `HEAD^{tree}`, not directory bytes; documented in the notes. |
| Reviewer cannot run the gate (disk/time) | The gate uses `-Profile portable` plus a snapshot-metadata check; ~1 GB temp is documented. |

**Rollback**: the PR adds three new scripts and one spec directory. Reverting the squash commit removes
them; nothing else references them. No data migration, no engine state.

---

## 5. Verification

One command for the reviewer:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-migration.ps1
```

Full-path evidence recorded in the PR body:

```powershell
# snapshot (about 570 MB)
.\scripts\migrate-repo.ps1 -Profile snapshot -OutDir D:\tmp\snap -EmsdkPath C:\emsdk
# restore (about 1 GB)
powershell -ExecutionPolicy Bypass -File D:\tmp\snap\deploy-repo.ps1 -Bundle D:\tmp\snap -Dest D:\tmp\restore -Build
```

Expected: exit 0, `checksums verified`, `0 mismatches` for both targets, `25 skills checked, 25 OK`,
equal `HEAD^{tree}`, clean status, two PE 0x8664 executables.

---

## 6. Task order

See `tasks.md`. Phase 1 publishes this spec; phase 2 implements the scripts; phase 3 runs the gate and
records evidence; phase 4 updates the notes if the gate exposes a gap.
