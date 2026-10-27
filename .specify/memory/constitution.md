# DOOM3_AINative Constitution

## Core Principles

### I. Pristine Source Is Read-Only
`doom3_orig/doom3` is never edited, built in, or formatted. Work happens in `targets/<name>/doom3`
and is exported as patches under `targets/<name>/patches/`. Engine changes land on a fork branch
and arrive here as a reviewed submodule bump.

### II. Evidence Before Victory (NON-NEGOTIABLE)
A task is done only when a command result, file listing, byte count, or screenshot proves it.
Acceptance criteria that cannot be checked mechanically are rewritten until they can. Data-dependent
checks without data are reported SKIPPED, never PASS.

### III. Spec-Driven, One Issue One Worktree One PR
Changes follow spec -> plan -> tasks -> implement -> independent review. One issue, one branch,
one worktree, one PR. No direct commits to `main`, no `git stash`.

### IV. Legal Hygiene
Repository content is GPL-3.0-or-later. Game data, binaries, and build output are never committed.
Only legally obtained data (official demo or user-owned install) is used for tests.

### V. Portable Fallbacks Over Silent Removal
Code disabled for a platform needs a documented fallback or a recorded reason and an open task.
Reproducibility beats cleverness: scripted builds, derived paths, no machine-specific absolute paths in tracked files.

## Constraints

- Docs, code, comments, and commit messages in English.
- Toolchain facts live in `AGENTS.md`; new tool requirements are recorded there by PR.
- No secrets in tracked files.

## Workflow and Quality Gates

- Parallel agents own disjoint files (one writer per file).
- A reviewer who is not the author reruns the verification command before merge.
- Submodule bumps and build-system changes require an independent reviewer.

## Governance

This constitution supersedes ad hoc practice. Amendments are made by PR with a rationale and a
version bump. PR reviews verify compliance with `AGENTS.md` and `docs/GIT_WORKFLOW.md`.

**Version**: 1.0.0 | **Ratified**: 2026-10-26 | **Last Amended**: 2026-10-26
