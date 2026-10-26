# DOOM3_AINative

An AI-native workspace for migrating **DOOM 3** (idTech4, C++, OpenGL) to the web, and for running
the port as an agent-driven engineering loop: spec → worktree → PR → independent review → evidence.

**Status: scaffold.** No port code yet. The engine source is vendored as a submodule; milestone
plan and exit criteria live in [`docs/ROADMAP.md`](docs/ROADMAP.md).

## What is in the box

| Path | Contents |
|---|---|
| `doom3/` | Git submodule — DOOM 3 GPL source release (2011-11-22), GPLv3, **no game assets** |
| `.specify/` | GitHub spec-kit infrastructure (templates, PowerShell scripts, constitution) |
| `.kilo/command/` | `/speckit.*` slash commands (constitution, specify, clarify, plan, tasks, analyze, implement, converge, checklist, taskstoissues) |
| `.kilo/skills/` | 14 project-local skills (game dev, debugging, review, browser testing, C++→WASM) |
| `.kilo/setup-script.ps1` | Initialises the submodule in every Agent Manager worktree |
| `docs/` | Workflow, skills inventory, roadmap |
| `scripts/` | Worktree helpers and the skill verifier |

## Quick start

```powershell
# clone with the engine submodule
git clone --recurse-submodules <repo-url> DOOM3_AINative

# already cloned without it:
git submodule update --init --recursive

# verify the local skill install
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify-skills.ps1

# spec-kit (no global install needed)
uvx --from git+https://github.com/github/spec-kit.git specify check
```

Open the repo in Kilo and start with `/speckit.constitution`, then `/speckit.specify`.

## Workflow in one paragraph

One issue = one branch = one worktree = one PR. A coordinator breaks the issue down and owns
integration; each worker gets one worktree under `.kilo/worktrees/` and one ticket; a reviewer who
did not write the code runs the `code-review` skill against the PR. Only squash merges into
`main`, and the PR body must carry the evidence (command + output) that the change works. Full
mechanics, scripts, and red lines: [`docs/GIT_WORKFLOW.md`](docs/GIT_WORKFLOW.md).

## Skills

The project ships real copies of the skills it depends on, so a fresh worktree or a CI agent gets
the same behaviour without the user's global vault. Each skill is a directory with a `SKILL.md`.

- Migration: `cross-platform-guide`, `wasm-emscripten`
- Game loop: `game-dev-loop`, `game-gdd-writing`, `game-build-verification`, `game-debug`
- Engineering: `software-dev-loop`, `tdd`, `systematic-debugging`, `code-review`, `prompt-pro`
- Verification and delivery: `verification-before-completion`, `webapp-testing`, `using-git-worktrees`

Provenance, pinned commits, and the "deliberately not installed" list are in
[`docs/SKILLS.md`](docs/SKILLS.md).

## Engine submodule

`doom3/` points at a fork of `id-Software/DOOM-3`. Changes to the engine are made on a fork branch,
never as unexplained working-tree dirt here; this repository only records submodule bumps. The
source release has no game data — `.pk4`/`.pk3`/`.pak` files are ignored and must not be committed.

## Licensing

- Engine source in `doom3/`: GPLv3 (`doom3/COPYING.txt`).
- Game data: not included, still governed by the original DOOM 3 EULA (`doom3/README.txt`).
- Skills under `.kilo/skills/`: each carries its upstream licence; see `docs/SKILLS.md`.
- This repository's own code and docs: licence not chosen yet.
