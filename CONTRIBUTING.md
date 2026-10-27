# Contributing

DOOM3_AINative ports DOOM 3 (idTech4, C++, OpenGL) as a loop: issue → worktree → PR → independent
review → merge. This is the front door; [`AGENTS.md`](AGENTS.md) holds repository facts,
non-negotiables, and toolchain, and [`docs/GIT_WORKFLOW.md`](docs/GIT_WORKFLOW.md) holds the
branch/worktree/PR/review/merge detail. Read both first; where this file and those disagree, they win.

## Who this is for

Human developers and AI agents under the same rules: same evidence bar, same review, same red lines.

**TL;DR — five lines**

1. One issue = one branch = one worktree = one PR.
2. `doom3_orig/doom3/` is a read-only pristine submodule; work happens in `targets/<name>/doom3`.
3. Evidence before victory: a claim without command output is not a result.
4. Never commit game data, binaries, build output, secrets, or absolute machine paths.
5. Docs, code, comments, and commit messages in English.

## Setup on Windows 11

Two pinned versions shape every command below (full toolchain: `AGENTS.md`):

- **git 2.25** has no `git init -b`. Use `git init` then `git checkout -b <branch>`.
- **PowerShell 5.1** has no `&&`. Chain with `;` plus `if ($?) { ... }`, or run commands separately.

```powershell
git clone --recurse-submodules <repo-url> <repo-root>
cd <repo-root>
git submodule update --init --recursive   # if you cloned without --recurse-submodules
gh auth status                            # must show an active account with repo scope
```

Native build prerequisites for the win11-amd64 target: Visual Studio 2022 with the **Desktop
development with C++** workload (toolset v143), Windows SDK **10.0.26100**, and the CMake 3.20+
bundled with VS2022. No VS2010, MFC, DirectX June 2010 SDK, or OpenAL SDK — see
`targets/win11-amd64/README.md`.

**Credential caveat.** `gh` and Git Credential Manager are separate logins. GCM may be signed in as a
different GitHub account than `gh`; that shows up as a `403` on push even though `gh auth status`
looks healthy. Check `git config --get credential.helper` and `gh auth status`, then push once with
a token without changing stored config:

```powershell
$tok = gh auth token --user <name>
git -c "credential.helper=" -c "credential.helper=!f() { echo username=<name>; echo password=$tok; }; f" push -u origin <branch>
Remove-Variable tok
```

Do not rewrite the global credential helper; never put a token in a tracked file.

## The loop (`scripts/`)

1. **Confirm the issue contract.** Every acceptance criterion must be checkable: command + exit code,
   byte count, file listing, or screenshot. Fix the issue before branching if it is not.
2. **Create the worktree:** `.\scripts\new-worktree.ps1 -Issue <N> -Slug <slug> -Kind feat` (also
   `fix`, `chore`, `docs`), branching from `origin/main` into `.kilo/worktrees/<kind>-<N>-<slug>`.
   Exit `2` = submodule init failed; run `git -C <worktree> submodule update --init --recursive`.
3. **Enter it:** `git status --short` and `git rev-parse --show-toplevel` must agree on the worktree.
4. **Create the target copy:** `.\scripts\new-target.ps1 -Name win11-amd64` (git-ignored, never shared).
5. **Apply the tracked patch:** `.\scripts\apply-target-patches.ps1 -Name win11-amd64`. A fresh copy
   is pristine and will not build for x64 until this runs.
6. **Build** from `targets/win11-amd64` with the CMake preset: `cmake --preset win11-amd64 .` then
   `cmake --build build --config Release`; output is `build/Release/DOOM3.exe`.
7. **Run and check:** `run-win11-amd64.ps1 -DataDir <data-root>\run -Build`,
   `smoke-win11-amd64.ps1 -DataDir <data-root>\run`, `verify-win11-amd64.ps1 -DataDir <data-root>\run`
   (all under `.\scripts\`). Supporting probes: `capture-window.ps1 -ProcessName DOOM3 -Out <file>.png`,
   `audio-peak.ps1 -ProcessName DOOM3`, `capture-showcase.ps1 -DataDir <data-root>\run -OutDir docs\media`.
8. **Export the source change:** `.\scripts\export-target-patches.ps1 -Name win11-amd64` rewrites
   `targets/win11-amd64/patches/all-changes.patch` and `all-changes.manifest.txt`.
9. **Commit** only your owned paths, push the branch, open a **draft** PR with the body template in
   `docs/GIT_WORKFLOW.md` §8.
10. **Review** by someone who is not the author, then mark ready and squash-merge.
11. **Clean up** from the parent checkout: `.\scripts\remove-worktree.ps1 -Path .kilo\worktrees\<slug>`
    then `git worktree prune`.

Demo data is prepared once per machine, outside git:
`.\scripts\prepare-demo-data.ps1 -Pk4 <data-root>\demo00.pk4 -RunDir <data-root>\run`.

## Definition of done

A PR body must carry, per claim: the exact command, its exit code, one key output line quoted
verbatim, and for visual claims the screenshot path. `docs/GIT_WORKFLOW.md` §8 (`## Evidence`,
`## Risk / rollback`, issue reference) is the required shape; machine-specific artifacts stay
untracked, the evidence block is text.

- **Squash merge only.** No merge commits, no direct commits to `main`.
- Write **`Closes #N`** only when the issue is genuinely finished. GitHub closes an issue on merge
  whenever the PR body or a commit message contains `Closes`, `Fixes`, or `Resolves` with the issue
  reference, and a squash commit inherits those messages. For partial work use **`Refs #N`**.
- Merge only when verification is green, review is approved, no out-of-scope path was touched, and
  no uncommitted submodule dirt remains.

## Engine and port rules

- **Edit only the target copy.** `doom3_orig/doom3/` is never edited, formatted, or built in.
- **Export changes as patches.** A target change is tracked as
  `targets/<name>/patches/all-changes.patch` plus its manifest, produced by the export script and
  re-applied by the apply script; `verify-win11-amd64.ps1` reapplies it to a fresh copy and compares
  hashes.
- **Patch files are binary in `.gitattributes`** (`targets/*/patches/*.patch binary`) because the
  sources are CRLF; any text conversion changes bytes and `git apply` then fails.
- **Build with the CMake preset**, never by hand-editing the generated `build/` tree.
- **Never commit `targets/*/build/`** (or any `build/`, `out/`, `*.exe`, `*.obj`, `*.log`).
- **Game data stays outside git**, and tests use legally obtained data only. The free demo licence
  forbids modifying or redistributing demo data; the shims `prepare-demo-data.ps1` writes are local
  test files, never committed.
- **Engine-level changes go upstream:** make them on a branch of the `yhyu13/DOOM-3` fork, then land
  them here as a reviewed submodule bump whose PR body records the fork PR and the old/new SHA.

## Testing and review

- The **reviewer is never the author**, reruns the issue's verification command (or the closest
  reproducible subset, stating what was not rerun and why), and keeps the review read-only apart from
  the report.
- Reviews use the `code-review` skill against `origin/main...HEAD`; comments cite `file:line`.
- Severity vocabulary, as used in `specs/001-win11-amd64-native-build/review.md`:
  **BLOCKER** (cannot merge: the artifact or the reviewable evidence is wrong),
  **MAJOR** (fix, or accept with a tracked follow-up), **MINOR** (fix if cheap, else record),
  **NIT** (cosmetic, no gate). Report counts, e.g. `BLOCKER 1 · MAJOR 4 · MINOR 6 · NIT 5`.

## For AI agents

1. Read `AGENTS.md`, then this file, then the stage of `docs/GIT_WORKFLOW.md` you are in.
2. Load skills from `.kilo/skills/<name>/SKILL.md` (the tree Kilo reads) or from
   `.claude/skills/<name>/SKILL.md` (the tree Claude Code reads). They are byte-identical mirrors and
   only `.claude/skills` is tracked in git: change a skill there, then run
   `scripts/sync-skills.ps1` and `scripts/verify-skills.ps1`.
3. Use subagents only with **disjoint file ownership** agreed before dispatch. One writer per file;
   if two agents need the same file, split the file or serialise the work.
4. Use the shared board for material findings that change another agent's decisions — not for
   progress narration, and never as a substitute for your final report.
5. Never `git stash`: stashes are shared across worktrees and silently corrupt parallel work.
6. Never push, merge, or close an issue unless the user asked for it in this task.
7. Verify by running the command and quoting real output (exit code plus the line). If it could not
   be run, say `not run` and why.
8. Do not write absolute machine paths into tracked files. Use `<repo-root>`, `<data-root>`,
   `<worktree>` placeholders or environment variables.

**PowerShell traps already hit here**

- Aliases shadow names: `R` is `Invoke-History`, not a short function name; check `Get-Alias <name>`
  before defining one.
- `Try` is reserved (`try`/`catch`), so a function or variable with that name fails to define.
- `[IO.File]::ReadAllText('x')` resolves `x` against the **process** working directory, not the
  shell's current location — use `$PSScriptRoot`, an absolute path, or `Resolve-Path` first.
- PowerShell text pipelines strip `CR` from native output; use `-Raw` or
  `[IO.File]::ReadAllBytes` when CRLF matters (patches, hashes).

## Where things live

| Path | What it holds |
|---|---|
| `AGENTS.md` | Repository facts, non-negotiables, toolchain, spec-kit flow |
| `docs/` | `GIT_WORKFLOW.md`, `SKILLS.md`, `ROADMAP.md`, media |
| `doom3_orig/` | Pristine read-only submodule (`doom3/`) and engine analysis notes (`doc/`); no game data |
| `targets/<name>/` | Tracked target metadata (CMake, presets, README, patches) |
| `targets/<name>/doom3/` | Git-ignored working copy; never shared, never committed |
| `scripts/` | Worktree, target, patch, build/run/verify, capture helpers |
| `specs/<feature>/`, `.specify/` | `spec.md`, `plan.md`, `tasks.md`, `review.md`; spec-kit templates, scripts, constitution |
| `.claude/` | Tracked project skills — the source of truth for every harness |
| `.kilo/` | Slash commands, setup script, generated skill mirror (ignored), worktrees (ignored) |
| `showcase/` | Static showcase page built from captured media |
| `LICENSE`, `THIRD_PARTY_NOTICES.md` | GPL-3.0-or-later here; third-party notices |
