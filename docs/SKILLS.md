# Project skills

Twenty-five agent skills live in `.claude/skills/<name>/SKILL.md` as real files — not junctions or
symlinks — and are mirrored byte-for-byte into the git-ignored `.kilo/skills/` tree that Kilo
reads. A fresh clone tracks only `.claude/skills` and rebuilds the mirror with
`scripts/sync-skills.ps1`, so no session depends on the operator's global skill vault. The set is
14 project skills, `win64-port-playbook`, and the 10 generated `speckit-*` skills.

Skills keep their own licences; the repository's GPL-3.0-or-later does not relicense them. See
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

- **Why local copies:** reproducible for any agent or CI runner. A fresh clone plus
  `git submodule update --init --recursive` is enough; nothing has to be installed under
  `~/.kilo/skills` or `~/.claude/skills` on the machine first. Worktrees created by Agent Manager
  inherit the same skills because they share the tracked files.
- **Cost:** duplicated content. At 2026-10-27 the tracked tree holds **51 files, 334,817 bytes
  (327.0 KiB / 0.32 MiB)**, and `.kilo/skills/` holds an identical local copy that git ignores.
  Every change must be made in the source (the vault, the pinned upstream repo, or the spec-kit
  installer for `speckit-*`) and re-copied, or the copy silently drifts. Two vendored skills also
  need their upstream SHA and license tracked here.

**Why `.claude/skills` is the tracked path:** Claude Code discovers skills from `.claude/skills/` in
the project, `~/.claude/skills/` for the user, and a directory passed with `--add-dir` (which looks
for `<dir>/.claude/skills/`, not `<dir>/skills/`); it has no settings key for extra skill
directories, and `permissions.additionalDirectories` grants file access only. Tracking that path is
the only way a fresh clone reproduces Claude Code's skill set.

**Why Kilo gets a generated second tree:** Kilo discovers project skills inside a `.kilo/` config
directory and also honours `kilo.json -> skills.paths`. In this workspace the config path was
observed **not** to take effect — one session whose workspace root was a parent directory above the
repository (not the repository itself) loaded none of the project skills whether they lived in
`.kilo/skills` or `.claude/skills`, and saw only the global vault at `~/.kilo/skills`. `.kilo/skills`
therefore exists as a local, git-ignored mirror that does not depend on `kilo.json` being applied. It is a copy, never a second source of
truth: edit `.claude/skills`, then run `scripts/sync-skills.ps1`.

The project copy is authoritative: the same vault skills also exist globally at `~/.kilo/skills`
(and, for the 12 vault skills, byte-identically at `~/.claude/skills`). **Policy:** if a harness
lists both a global and a project copy of the same skill name, treat the project copy as the one
that governs work in this repository.

**Caveat, verified against the harness docs:** Claude Code resolves name clashes by precedence
enterprise > personal > project, so for those 12 duplicated names it loads the personal
`~/.claude/skills` copy, not this one. They are byte-identical today, so nothing differs until one
side is edited; when that happens, update both or delete the personal copy.

## What is installed

"Use when" is condensed from each skill's own frontmatter `description`; the source of each row is
recorded in full under [Sources](#sources). Phases are the repository's own loop
(spec → build → test → review → ship).

| Skill | Use when | Phase | Source |
|---|---|---|---|
| `cross-platform-guide` | Native C/C++ must be portable across OS targets: one platform-abstraction interface, porting order, Emscripten/WebAssembly builds, gamepad/evdev input. | spec, build | vendored MIT, `xonovex/platform` @ `97adf3f` |
| `wasm-emscripten` | Compiling C/C++ to WebAssembly with `emcc`: exported functions, memory model, Asyncify, WASI vs browser, `.wasm` debugging. | build | vendored MIT, `mohitmishra786/low-level-dev-skills` @ `bdc5847` |
| `game-dev-loop` | Running a real game build from idea to playable: goal → GDD/TDD → engine + scaffold → implement → test/debug → polish → memory. | spec → build → test | vault |
| `game-gdd-writing` | Turning a game idea into an executable GDD whose sections are contracts for downstream build steps. | spec | vault |
| `game-build-verification` | Verifying and debugging a generated game before shipping: pre-build checklist, build/test/visual order, error triage. | test, ship | vault |
| `game-debug` | A game fails to build/run, or assets, scenes, animations, or config values look inconsistent. | test | vault |
| `software-dev-loop` | Running a non-game software build end to end: goal → plan → implement → test → journey → long-term memory. | spec → build → test | vault |
| `systematic-debugging` | Any bug, failing test, or unexpected behavior — root cause before any fix. | test | vault |
| `verification-before-completion` | About to claim work is complete, fixed, or passing, or before commit/PR — requires fresh command output. | ship | vault |
| `using-git-worktrees` | Starting feature work that needs isolation, or before executing an implementation plan. | build (pre-work) | vault |
| `code-review` | Reviewing a branch, PR, or WIP diff since a fixed point, on two axes: repo standards and the originating spec. | review | vault |
| `prompt-pro` | At the start of any non-trivial request, to rewrite a vague ask into an executable brief before acting. | spec | vault |
| `tdd` | Building a feature or fixing a bug test-first (`red-green-refactor`), or adding integration tests. | test | vault |
| `webapp-testing` | Exercising or debugging a local web app with Playwright: UI behavior, screenshots, console logs. | test | vault |

## Sources

### Vault skills (12)

Vault root: `<AgentSkillsVault>/SKILL` — the vault checkout is local to the operator's machine, so
the absolute prefix is not recorded here; the `SKILL/<path>` part is the stable identifier.

- Vault commit: **`1c23ef1`** — `git -C "<AgentSkillsVault>" rev-parse --short HEAD`
- Working tree: **clean** on 2026-10-26 — `git status --porcelain` produced no output.
- Fidelity: all 12 project copies were compared to their vault source directories by SHA-256 and
  are identical (32 files).

| Skill | Vault-relative path |
|---|---|
| `code-review` | `software-development/mattpocock/skills/engineering/code-review` |
| `game-build-verification` | `game-dev/game-build-verification` |
| `game-debug` | `game-dev/game-debug` |
| `game-dev-loop` | `game-dev/game-dev-loop` |
| `game-gdd-writing` | `game-dev/game-gdd-writing` |
| `prompt-pro` | `software-development/prompt-pro` |
| `software-dev-loop` | `software-development/software-dev-loop` |
| `systematic-debugging` | `software-development/systematic-debugging` |
| `tdd` | `software-development/mattpocock/skills/engineering/tdd` |
| `using-git-worktrees` | `software-development/using-git-worktrees` |
| `verification-before-completion` | `software-development/verification-before-completion` |
| `webapp-testing` | `game-dev/webapp-testing` |

`webapp-testing` carries upstream frontmatter metadata naming
`ComposioHQ/awesome-claude-skills` and ships an Apache-2.0 `LICENSE.txt` in its directory. For this
repository the vault copy is the source of record; the vendored provenance recorded by the skill
is left untouched.

### Vendored skills (2)

Both are MIT-licensed. License was verified at the pinned upstream repository; the copied
directories do not include a `LICENSE` file, so the upstream repository-root license applies.
Retrieved 2026-10-26 by downloading the files from `raw.githubusercontent.com/<owner>/<repo>/<sha>/...`
at the pinned SHA (not from a branch head).

| Skill | Repository | Path in repo | Pinned commit | Local files |
|---|---|---|---|---|
| `cross-platform-guide` | `https://github.com/xonovex/platform` | `packages/runtime/plugin/plugin-native/skills/cross-platform-guide/` | `97adf3fce441cf4e0c5fa03c0f55a803a6bdfbbc` | 6 — `SKILL.md`, `SOURCES.md`, 4 `references/*.md` |
| `wasm-emscripten` | `https://github.com/mohitmishra786/low-level-dev-skills` | `skills/runtimes/wasm-emscripten/` | `bdc58472fa9f309ed1b3f7d985a0d8e9bd8f4608` | 2 — `SKILL.md`, `references/emscripten-linker-flags.md` |

Notes:

- `cross-platform-guide` cross-references sibling skills (`gpu-rendering-guide`,
  `gpu-rendering-vulkan-guide`, `cmake-guide`, `c99-opinionated-guide`) that are **not** installed
  here. Treat those references as pointers, not as available instructions.
- Its `SOURCES.md` is part of the vendored directory and records the archived blog posts and
  Emscripten/kernel docs the guidance was distilled from.
- `wasm-emscripten` is the toolchain-level companion to the web-build material in
  `cross-platform-guide`; load both when planning the Emscripten build.

## Deliberately not installed

| Not installed | Why |
|---|---|
| Phaser, Three.js, `procedural-3d-scenes`, `game-template` and the other JS/Three game skills | Engine mismatch. idTech4 is C++/OpenGL; there is no JS game runtime, no Three.js scene graph, and no template-based web game engine here. |
| Unreal Engine skills (`ue-renderdoc-auto-capture`, `ue4-shader-debug`, …) | Wrong engine. DOOM 3 is idTech4, not UE4/UE5; the APIs, build system, and asset pipeline do not transfer. |
| `technical-design-document` | Roughly 1 MB of templates and guidance. `/speckit.plan` already produces the technical plan for this repository, so the skill would duplicate the artifact at a large size cost. |
| `agent-browser` | Requires an external browser-automation CLI. `webapp-testing` already covers Playwright-based UI checks without adding another toolchain dependency. |

## spec-kit workflow mapping

The mappings below connect spec-kit commands to skills. Spec-kit has no skill hooks, so these are
**suggested** hand-offs to apply while running each command, not automation.

Both spec-kit integrations are installed; they read the same `.specify/` state and differ only in
where the command text lives:

| Harness | Command form | Source | Managed by |
|---|---|---|---|
| Kilo | `/speckit.specify` | `.kilo/command/speckit.*.md` | `generic` integration (default) |
| Claude Code | `/speckit-specify` | `.claude/skills/speckit-*/SKILL.md` | `claude` integration |

The generated `speckit-*` skills are spec-kit output, not vault or vendored skills: do not
hand-edit them. Reinstall with
`uvx --from git+https://github.com/github/spec-kit.git specify integration install claude --force`
and keep both integrations on the same CLI version. The table below uses the Kilo spelling;
Claude Code substitutes `-` for `.`.

| Stage | Skills | Confidence |
|---|---|---|
| `/speckit.specify` | `prompt-pro` (sharpen the request before writing `spec.md`) | suggested |
| `/speckit.plan` | `cross-platform-guide` (portability/WASM constraints), `game-gdd-writing` (design sections as build contracts) | suggested |
| `/speckit.tasks` | `tdd` (shape tasks as testable behaviors) | suggested |
| `/speckit.implement` | `software-dev-loop` (build loop), `systematic-debugging` (when something fails) | suggested |
| Before claiming done / opening a PR | `verification-before-completion` (evidence), `game-build-verification` (build/visual order), `webapp-testing` (browser checks) | suggested |
| PR review | `code-review` (standards + spec axes) | expected — this is the repository's independent-review stage |
| Worktree setup | `using-git-worktrees` | expected for hand-made worktrees; Agent Manager already runs `.kilo/setup-script.ps1` |
| `/speckit.clarify`, `/speckit.analyze`, `/speckit.converge` | none | — |

Applicability caveat: `game-debug` explicitly targets web games, and `game-build-verification` /
`game-dev-loop` assume a generated game with a modern web or template stack. Keep the design,
verification-order, and evidence discipline; ignore the JS/engine specifics when working on the
C++ idTech4 port. `cross-platform-guide` and `wasm-emscripten` are the two most directly relevant
skills for the actual port.

## Update policy

- **Vault skills:** edit the vault first, then re-copy the skill directory into `.claude/skills/`
  and record the new vault commit here. Do not hand-edit the project copy: the two would drift and
  the next re-copy would silently discard the change. After copying, run `scripts/sync-skills.ps1`
  and then the verifier.
- **Never edit `.kilo/skills/`:** it is generated and git-ignored. A change there is overwritten by
  the next sync and is invisible to everyone else.
- **Vendored skills:** re-download at the pinned SHA. Never edit a vendored copy in place. If local
  behavior must change, copy it to a separate local namespace (for example
  `<name>-local`), record the diff and the reason here, and leave the vendored original intact.
- **Bump a vendored SHA deliberately:** review the upstream diff, re-download, update the commit
  in this file, and re-run the verifier. Never track a branch head.
- **Add or remove a skill:** update the table, the source section, and this policy if needed, then
  run the sync and the verifier.

## Verification

```powershell
powershell -NoProfile -File scripts/sync-skills.ps1          # rebuild .kilo/skills from .claude/skills
powershell -NoProfile -File scripts/verify-skills.ps1        # format + mirror parity, exits 0/1
powershell -NoProfile -File scripts/sync-skills.ps1 -Check   # CI: exit 1 if the mirror is stale
```

`verify-skills.ps1` checks every directory under `.claude/skills` for a non-empty `SKILL.md` whose
YAML frontmatter holds a `name` equal to the directory name, lowercase kebab-case and at most 64
characters, plus a non-empty `description` of at most 1024 characters — the format Claude Code and
Kilo both require. It then compares `.claude/skills` with `.kilo/skills`: same skill names, and
byte-identical files (SHA-256 per file). It prints skill, file count, size in KB, status, then the
mirror summary; exits `1` on any failure and `0` when both trees pass. Pass `-NoMirror` to run the
format checks alone.

## Written in this repository (added with PR #4)

| Skill | Use when |
|---|---|
| `win64-port-playbook` | Porting or debugging a 32-bit-era engine on Windows x64: build bring-up, pointer-truncation hunts, crashes that only appear with real data, scripted screenshot/log/audio evidence. Source: this repo's own work (`specs/001-win11-amd64-native-build`). |

It lives in the tracked `.claude/skills/` tree with everything else, and reaches Kilo through the
generated `.kilo/skills/` mirror like every other skill.
