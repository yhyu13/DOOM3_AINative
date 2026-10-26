# DOOM3_AINative

An AI-native workspace for migrating **DOOM 3** (idTech4, C++, OpenGL) to the web, and for running
the port as an agent-driven engineering loop: spec → worktree → PR → independent review → evidence.

**Status: scaffold.** No port code yet. The engine source is a pristine submodule; each migration
target works on its own copy under `targets/`. Milestones and exit criteria live in
[`docs/ROADMAP.md`](docs/ROADMAP.md).

## What is in the box

| Path | Contents |
|---|---|
| `doom3_orig/doom3/` | **Pristine** git submodule — DOOM 3 GPL source (2011-11-22), GPLv3, **no game assets** |
| `doom3_orig/doc/` | Engine analysis notes (overview, platform, renderer, scripting, AI, build/deps) |
| `targets/<name>/doom3/` | One working **copy** per migration target (ignored scratch tree; see `targets/README.md`) |
| `.specify/` | GitHub spec-kit infrastructure (templates, PowerShell scripts, constitution) |
| `.kilo/command/` | `/speckit.*` slash commands (constitution, specify, clarify, plan, tasks, analyze, implement, converge, checklist, taskstoissues) |
| `.kilo/skills/` | 14 project-local skills (game dev, debugging, review, browser testing, C++→WASM) |
| `.kilo/setup-script.ps1` | Initialises the submodule in every Agent Manager worktree |
| `docs/` | Workflow, skills inventory, roadmap |
| `scripts/` | Worktree helpers, target copies, skill verifier |

## Quick start

```powershell
# clone with the engine submodule
git clone --recurse-submodules <repo-url> DOOM3_AINative

# already cloned without it:
git submodule update --init --recursive

# create the working copy for a migration target (pristine stays untouched)
.\scripts\new-target.ps1 -Name web

# see how far a target has drifted from the pristine source
.\scripts\diff-target.ps1 -Name web

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

## Engine submodule and target copies

`doom3_orig/doom3/` is a pristine clone of a fork of `id-Software/DOOM-3`; it is pinned by commit
and treated as read-only evidence. Engine changes are made on a fork branch, never as unexplained
working-tree dirt here; this repository only records submodule bumps.

Migration work happens in `targets/<name>/doom3/`, a plain copy created by
`scripts/new-target.ps1` with no shared git metadata. Each target gets exactly one copy, so ports
cannot leak into each other; `scripts/diff-target.ps1` shows the divergence from pristine. The
source release has no game data — `.pk4`/`.pk3`/`.pak` files are ignored and must not be committed.

## Licensing

This repository's own content — specs, docs, scripts, workflow, automation — is
**GPL-3.0-or-later** ([`LICENSE`](LICENSE)), matching the engine's licence so the port stays a
single, compatible work.

- Engine source in `doom3_orig/doom3/`: GPLv3 or later (`doom3_orig/doom3/COPYING.txt`).
- Game data: not included, still governed by the original DOOM 3 EULA
  (`doom3_orig/doom3/README.txt`).
- Spec Kit, MIT and Apache-2.0 skills: their own licences, recorded in
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and `docs/SKILLS.md`.

SPDX identifier for this repository: `GPL-3.0-or-later`.
