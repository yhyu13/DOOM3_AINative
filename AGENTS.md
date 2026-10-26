# AGENTS.md — DOOM3_AINative

An AI-native workspace for migrating DOOM 3 (idTech4, C++, OpenGL) to the web and for running an
agent-driven engineering loop around it: spec → worktree → PR → independent review → evidence.

The engine source is a **submodule**, not a copy. This repository owns the port, the automation,
the specs, and the workflow.

## Read first

| File | Why |
|---|---|
| `docs/GIT_WORKFLOW.md` | Issue → branch → worktree → PR → review → merge. One issue = one branch = one worktree = one PR. |
| `docs/SKILLS.md` | Which project skill to load for which stage, and where each skill came from. |
| `docs/ROADMAP.md` | Milestones, exit criteria, and what is actually proven vs suggested. |

## Repository facts

- `doom3_orig/doom3/` — **pristine** git submodule: `https://github.com/yhyu13/DOOM-3.git`
  (fork of `id-Software/DOOM-3`), the DOOM 3 GPL source release of 2011-11-22. License: GPLv3
  (`doom3_orig/doom3/COPYING.txt`). The source release contains **no game data**; assets remain
  under the original EULA (`doom3_orig/doom3/README.txt`). Native build reference:
  `doom3_orig/doom3/neo/doom.sln` (Visual Studio 2010, DirectX SDK).
- `doom3_orig/doc/` — engine analysis notes written against the pristine tree (evidence for the
  port; line numbers drift when the submodule is bumped).
- `targets/<name>/doom3/` — one **copy** of the pristine source per migration target, created by
  `scripts/new-target.ps1`. Copies are git-ignored scratch trees; a target never edits the pristine
  submodule or another target's copy. See `targets/README.md`.
- `.specify/`, `.kilo/command/speckit.*.md` — GitHub spec-kit, installed with the generic
  integration and PowerShell scripts.
- `.kilo/skills/` — 14 project-local skills (12 from the user's skill vault, 2 vendored from
  upstream MIT repositories). Real files, not junctions, so worktrees and CI get the same skills.
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

## Spec-driven flow

```
/speckit.constitution  → establish project principles (required once, then edit by PR)
/speckit.specify       → specs/<feature>/spec.md
/speckit.clarify       → resolve ambiguities before planning (optional but recommended)
/speckit.plan          → specs/<feature>/plan.md
/speckit.tasks         → specs/<feature>/tasks.md
/speckit.analyze       → cross-artifact consistency check (optional)
/speckit.implement     → execute tasks
/speckit.converge      → append remaining work as tasks
```

Artifact paths and exact filenames are produced by `.specify/scripts/powershell/*.ps1`; read
`.specify/templates/` rather than guessing. Do not skip `/speckit.plan` for anything that touches
the engine build, the renderer, or the submodule.

## Toolchain (verified 2026-10-26)

| Tool | State |
|---|---|
| git | 2.25.0.windows.1 — no `git init -b`; PowerShell 5.1 has no `&&` |
| node | v24.19.0 |
| uv / uvx | 0.12.13 — use for spec-kit and Python tooling |
| python / py | bare `python` is 2.7.16; the `py` launcher resolves 3.14.4 — for new scripts use `uv run --python 3.12 ...` (or `py -3`) |
| specify | `uvx --from git+https://github.com/github/spec-kit.git specify ...` |
| emcc / emsdk | not installed yet (see `docs/ROADMAP.md` M2) |
| gh | 2.100.0, on `PATH`, authenticated (workspace owner account) — see `docs/GIT_WORKFLOW.md` §0 |

## Working in a worktree

Agent Manager runs `.kilo/setup-script.ps1` for every new worktree, which initialises
`doom3_orig/doom3/`. For a hand-made worktree run `git submodule update --init --recursive` inside
it. Target copies (`targets/<name>/doom3`) are not part of git; recreate them in a new worktree
with `scripts/new-target.ps1 -Name <target>` instead of copying them between worktrees. Build
directories, caches, and ports must be derived from `WORKTREE_PATH`; never share them.

## Skills

Project skills live at `.kilo/skills/<name>/SKILL.md`. They mirror the user's global vault at
`~/.kilo/skills`; the project copy is authoritative inside this repository. The stage map, source
paths, pinned upstream commits, and the "not installed and why" list are in `docs/SKILLS.md`.

Do not add a skill that duplicates an installed one. If a vendored skill needs local changes, copy
it to a new name and document the diff instead of editing it in place.

## Language

Docs, code, comments, and commit messages in English. Discussion with the user may be Chinese.
