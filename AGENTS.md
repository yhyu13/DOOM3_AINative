# AGENTS.md — DOOM3_AINative

Migrating DOOM 3 (idTech4, C++, OpenGL) to the web, with an agent-driven loop:
spec → worktree → PR → independent review → evidence. The engine source is a **submodule**, not a
copy; this repository owns the port, the automation, the specs, and the workflow.

## Read first

| File | Why |
|---|---|
| `docs/GIT_WORKFLOW.md` | Issue → spec PR → branch → worktree → implementation PR → review → merge. One issue = one spec PR plus one branch/worktree/PR. |
| `docs/SKILLS.md` | Which project skill to load for which stage, and where each skill came from. |
| `docs/ROADMAP.md` | Milestones, exit criteria, and what is actually proven vs suggested. |

This repository's own content is **GPL-3.0-or-later** (`LICENSE`); third-party works retain their
own licences (see `THIRD_PARTY_NOTICES.md`). Game data is not covered and must not be committed.

## Repository facts

- `doom3_orig/doom3/` — **pristine** submodule, `https://github.com/yhyu13/DOOM-3.git` (fork of
  `id-Software/DOOM-3`), the DOOM 3 GPL source release of 2011-11-22 (GPLv3,
  `doom3_orig/doom3/COPYING.txt`). Source only — **no game data**; assets stay under the retail
  EULA (`doom3_orig/doom3/README.txt`). Native build reference: `neo/doom.sln` (VS2010, DirectX SDK).
- `doom3_orig/doc/` — engine analysis against the pristine tree (evidence for the port; line
  numbers drift when the submodule is bumped).
- `targets/<name>/doom3/` — one **copy** of the pristine source per target, created by
  `scripts/new-target.ps1`, git-ignored. A target never edits the submodule or another target's
  copy. See `targets/README.md`.
- `CMakeLists.txt`, `CMakePresets.json` — `cmake --preset win11-amd64` (creates and patches the
  target copy on first run), then `cmake --build --preset win11-amd64-release`. Output
  `build/bin/Release/DOOM3.exe` and `DOOM3XP.exe` (d3xp game code statically linked; needs retail
  RoE data, base game runs via `DOOM3.exe`; retail RoE gameplay unverified). `build/` is per
  worktree and git-ignored. Pass MSBuild switches as `-- -m` (Git Bash mangles `-- /m`).
- **Redirect build output; show only the summary.** A clean Release build was reported to emit
  ~1.4 MB (~12.9k lines, ~4.2k of them warning lines). See **Context discipline** for shell-specific
  commands that retain the full log and the build exit code.
- Engine diagnostics added by the port: console commands `sys_cpuInfo`, `sys_fpuState`,
  `sys_stackTrace`; `SAVEGAME_VERSION` is 18 (older saves are rejected with a warning). Tests
  (`-Exe -DataDir -OutDir`, net test also `-Port`): `scripts/test-cpu-fpu-trace.ps1`,
  `test-savegame-demo.ps1`, `test-net-loopback.ps1`, `test-d3xp.ps1`.
- `.specify/`, `.kilo/command/speckit.*.md` — GitHub spec-kit (generic integration, PowerShell scripts).
- `.claude/skills/` — 25 tracked skills, source of truth in git, what Claude Code reads natively.
- `.kilo/skills/` — byte-identical, **git-ignored** mirror generated only by
  `scripts/sync-skills.ps1` (also run per worktree by `.kilo/setup-script.ps1`). Never edit it.
- `scripts/` — workflow helpers (worktrees, target copies, skill verification).

## Non-negotiables

1. **The pristine submodule is read-only.** Never edit `doom3_orig/doom3/` in place. Engine changes
   belong on a branch of the fork; land it there, then bump `doom3_orig/doom3` here
   (`git add doom3_orig/doom3`) with the new SHA in the PR body. Migration work happens in
   `targets/<name>/doom3`, which is never committed. Unexplained submodule dirt is a review blocker.
2. **One copy per target.** Do not share a target copy, and do not edit one target from another
   target's session. Create a copy with `scripts/new-target.ps1 -Name <target>`.
3. **Never commit game data or build output.** `.pk4`/`.pk3`/`.pak` assets, binaries, object
   files, and per-worktree build directories stay out of git. GPLv3 covers the source only.
4. **No direct commits to `main`.** No force-push to `main`. No `git stash` — stashes are shared
   across worktrees and silently corrupt parallel work.
5. **Evidence before victory.** A task is done only when a command's output proves it: build exit
   code, test summary, screenshot, or file listing. Load the `verification-before-completion`
   skill before claiming completion.
6. **No secrets or machine-specific absolute paths** in tracked files. Use environment variables
   and paths relative to the repository root.
7. **One writer per file.** Parallel agents must not edit the same file; split by ownership
   (see `docs/GIT_WORKFLOW.md`).
8. **Merge only into a clean, current `main`.** Check the target mainline itself, not the remote
   verdict: `git fetch origin`, then `gh pr view <N> --json mergeable,mergeStateStatus` must report
   `MERGEABLE` / `CLEAN`, **and** `git status --porcelain` in the main worktree must show no
   modified, staged or untracked file that the PR touches — mergeability is computed from the
   remote head and cannot see local dirt. Prove it offline with
   `git merge-tree $(git merge-base origin/main origin/<head>) origin/main origin/<head>` and
   `grep '^changed in both'` (the command exits 0 even on a conflict, and an unanchored grep also
   matches that phrase inside patched file content). Clear the overlap first: a dirty main worktree
   is a merge blocker, not something to work around.
9. **Start from `main`, or stop.** New work starts on `main`. Before touching a file, check
   `git branch --show-current` (and `git worktree list` when the path is under `.kilo/worktrees/`).
   If the checkout is not `main` and the user did not name a branch or worktree, stop and ask. A
   task description is not permission to continue on whatever branch is checked out, nor permission
   to commit to it. Detached HEAD counts as "not `main`".
10. **Never close an issue without the user's permission.** Do not run `gh issue close` or use
    `Closes #N` / `Fixes #N` / `Resolves #N` in a PR body or commit message unless the user
    explicitly approved closing that issue. Otherwise link with `Refs #N`. A merged PR proves the
    work, not the decision to close the issue.
11. **An agent may merge its own PR after independent review.** The author may squash-merge only when
    all of the following hold:
    - The PR contains an `APPROVED` verdict from a reviewer that is not the author and did not share
      the author's session or worktree. A fresh-context subagent may serve only if it runs in its own
      Agent Manager session/worktree, receives only the diff, the relevant specs and the verification
      command, and records its verdict verbatim (PR comment or committed review file) with its session
      ID and the reviewed SHA. The authoring session must not write, rewrite or paraphrase that
      verdict. This is a process control, not cryptographic identity; do not claim more.
    - The PR head at merge equals the reviewed SHA. Any new commit, rebase or force-push after the
      review voids the approval and requires a new review.
    - Submodule bumps (`doom3_orig/doom3`) and build-system changes are excluded from self-merge:
      they require owner sign-off in addition to independent review.
    - The merge gates in §8 hold: `git fetch origin`, `MERGEABLE` / `CLEAN` against current
      `origin/main`, the documented verification command or required checks pass, the main worktree
      is clean and current, `git merge-tree` shows no conflict, no out-of-scope paths, and no game
      data, build output or unexplained submodule dirt.
    - No issue is closed without permission.
    If any condition fails, do not merge: rebase, re-review, or stop. This grants merge authority, not
    permission to bypass evidence, file ownership, or the user's stop instructions.

## Spec-driven flow

```
/speckit.constitution  → establish project principles (once, then edit by PR)
/speckit.specify       → specs/<feature>/spec.md
/speckit.clarify       → resolve ambiguities before planning (optional)
/speckit.plan          → specs/<feature>/plan.md
/speckit.tasks         → specs/<feature>/tasks.md
/speckit.analyze       → cross-artifact consistency check (optional)
/speckit.implement     → execute tasks
/speckit.converge      → append remaining work as tasks
```

Paths and filenames come from `.specify/scripts/powershell/*.ps1`; read `.specify/templates/`
rather than guessing. Do not skip `/speckit.plan` for anything touching the engine build, the
renderer, or the submodule.

Spec artifacts are published by a `docs/<issue>-<slug>` PR and merged to `main` **before**
`scripts/new-worktree.ps1` creates the implementation worktree (`docs/GIT_WORKFLOW.md` §1–§2).
Specs 001/002/006/007 landed inside their implementation PRs instead; that is recorded history, not
the route to follow.

## Toolchain (verified 2026-10-26)

| Tool | State |
|---|---|
| git | 2.25.0.windows.1 — no `git init -b`; PowerShell 5.1 has no `&&` |
| node | v24.19.0 |
| uv / uvx | 0.12.13 — use for spec-kit and Python tooling |
| python / py | bare `python` is 2.7.16; `py` resolves 3.14.4 — use `uv run --python 3.12 ...` or `py -3` |
| specify | `uvx --from git+https://github.com/github/spec-kit.git specify ...` |
| emcc / emsdk | 6.0.13 installed via emsdk, per host, outside the repo (see `tests/webgpu/emscripten/README.md`); its bundled Python 3 must precede a system Python 2 on `PATH` |
| gh | 2.100.0, on `PATH`, authenticated (workspace owner) — see `docs/GIT_WORKFLOW.md` §0 |

## Working in a worktree

Agent Manager runs `.kilo/setup-script.ps1` per worktree, which initialises `doom3_orig/doom3/`;
a hand-made worktree needs `git submodule update --init --recursive`. Target copies are not in git —
recreate them with `scripts/new-target.ps1 -Name <target>` instead of copying between worktrees.
Build directories, caches, and ports derive from `WORKTREE_PATH`; never share them.

## Context discipline

The following figures were reported from 581 sessions: tool output was 56M tokens (201 MB).
The underlying dataset has not been identified in this repository, so the figures have not been
independently verified here. The practices below limit avoidable output without discarding evidence.

1. **Avoid rereading an entire unchanged file.** Re-reads were reported as 30% of all `read` output
   (7.0M tokens). Use `offset`/`limit` to revisit a region; reread when the file has changed or fresh
   evidence is needed.
2. **Do not paste unfiltered test output.** Save the full output to a worktree-local, git-ignored
   log, then show a bounded tail or a tool-specific summary. Capture the test command's exit code
   immediately; inspect the saved traceback or failure details as needed. Do not reuse one filter for
   `unittest`, `ctest`, and `vitest`, and check the test count when tests are expected. Adapt the
   shell-specific logging patterns below with a separate test log.
3. **`git diff --stat` first.** Full `git diff HEAD` was reported to average 43 KB per call. Open
   the full diff only for the files under review.
4. **Bound every listing and search.** `Get-ChildItem -Recurse` was reported as 20% of Bash output.
   Prefer `git ls-files`, `-Depth 2`, or a `-Filter`. For locating, `grep -l` / `-m 1`; read content
   only in the file you have already identified.
5. **Keep build output in a log.** A clean Release build was reported to emit ~1.4 MB (~12.9k lines,
   ~4.2k warning lines). After configuring the build, use one of the following in a dedicated shell
   process: `exit` terminates that shell. Both commands keep the full log in git-ignored `build/`,
   print its last 40 lines, and return the original build exit code.

   Git Bash:

   ```bash
   cmake --build --preset win11-amd64-release -- -m > build/_build.log 2>&1
   s=$?
   tail -40 build/_build.log
   exit "$s"
   ```

   PowerShell 5.1 (redirection occurs in `cmd` so native stderr stays in the log):

   ```powershell
   cmd /c 'cmake --build --preset win11-amd64-release -- -m > build\_build.log 2>&1'
   $s = $LASTEXITCODE
   Get-Content build/_build.log -Tail 40
   exit $s
   ```

6. **Isolate independent work and long-running commands when useful.** For a bounded, independent
   investigation or experiment, use a subagent when the harness supports it; request a concise report
   with evidence, relevant paths, and unresolved questions rather than full tool output. Subagents use
   their own context, but their reports enter the parent context and their work still consumes tokens;
   they do not replace issue, worktree, file-ownership, or review rules. Do small, dependent lookups
   inline instead of delegating just to save context.
7. **Run long builds or tests in the background when the harness supports it.** Redirect full output
   to a worktree-local log as above, preserve the exit code, and inspect a bounded summary on completion.
   Background execution alone does not guarantee output isolation or lower token use; avoid repeatedly
   polling or reading the full log. Report failures and use the actual exit code as evidence.

If output must exceed ~200 lines to be useful, write it to a temp file and `read` the slice you need
instead of letting it enter the transcript.

## Skills

| Path | Tracked | Read by | Produced by |
|---|---|---|---|
| `.claude/skills/<name>/SKILL.md` | **yes** — source of truth in git | Claude Code, subject to personal-scope precedence | hand-edited, copied from the vault, or generated; see `docs/SKILLS.md` |
| `.kilo/skills/<name>/SKILL.md` | no | Kilo, as a project config dir | `scripts/sync-skills.ps1` only |

Both trees must hold the same 25 skills byte-for-byte. `scripts/verify-skills.ps1` validates the
Claude skill format of every tracked skill (`name` == directory, lowercase kebab-case,
`description` <= 1024 chars) and fails when the mirror is missing, stale, or holds extra files.
Follow the update policy in `docs/SKILLS.md`: update vault skills in the vault first and re-copy
into `.claude/skills`; regenerate `speckit-*` rather than hand-editing them. Then sync the mirror;
never edit `.kilo/skills` directly.

`kilo.json` also lists `./.claude/skills` under `skills.paths`, but that was **observed not to take
effect** in a session whose workspace root sat above the project directory — which is why the
generated mirror exists. Treat `skills.paths` as a convenience, not as the mechanism.

Six skills (`prompt-pro`, `tdd`, `code-review`, `webapp-testing`,
`verification-before-completion`, `systematic-debugging`) also exist in the global vault
`~/.kilo/skills`, and Kilo resolves the vault copy for those names. Claude Code also gives a personal
skill precedence over a project skill of the same name, so any of the 12 vault skills can be shadowed
there. The project copy is authoritative for this repo, but when copies exist, verify that they match;
the verifier checks only the project and Kilo mirror. See `docs/SKILLS.md` for the full update and
precedence policy.
Port know-how (`win64-port-playbook`), the stage map, and the "not installed and why" list are in
`docs/SKILLS.md`; contributor rules in `CONTRIBUTING.md`. Do not add a skill that duplicates an
installed one; if a vendored skill needs local changes, copy it under a new name and document the
diff.

## Language

Docs, code, comments, and commit messages in English. Discussion with the user may be Chinese.
