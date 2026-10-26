# Migration targets

`doom3_orig/doom3` is the **pristine** DOOM 3 submodule (GPLv3 source, pinned commit). It is a
reference: never edit it, never commit build output next to it.

Every migration target gets its **own copy** of that source:

```
doom3_orig/doom3/          pristine submodule, read-only reference
targets/web/doom3/         working copy for the web target
targets/<other>/doom3/     working copy for another target
```

The copies are plain trees: no `.git` file, no shared gitdir with the pristine submodule, so each
target can diverge without touching the others. They are git-ignored by design (`targets/*/doom3/`)
because a copy is tens of megabytes and diverges immediately.

## Create / inspect

```powershell
# copy pristine -> targets/web/doom3 (refuses to overwrite without -Force)
.\scripts\new-target.ps1 -Name web

# diff a target against pristine (exit 1 = differences, 0 = identical)
.\scripts\diff-target.ps1 -Name web
.\scripts\diff-target.ps1 -Name web -NameStatus
```

## Rules

1. **Pristine stays intact.** Do not edit, format, or build inside `doom3_orig/doom3`. If it shows
   as dirty, restore it before copying.
2. **One copy per target.** Do not share a copy between targets; a change for one must not leak
   into another.
3. **Never commit a copy.** `targets/*/doom3/` is ignored. Keep per-target notes in
   `targets/<name>/README.md` and patches in `targets/<name>/patches/` — those two paths *are*
   trackable.
4. **Engine changes go upstream.** If a change must live in the engine itself, export it from the
   target, apply it on a branch of the `yhyu13/DOOM-3` fork, then bump the pristine submodule here
   (one reviewable commit SHA). Do not let fork work exist only inside a target copy.
5. **Copies are scratch.** `git clean -xfd` deletes them. If a target must survive that, turn it
   into its own repository deliberately, or export a patch under `patches/`.

## Current targets

| Target | Copy | Notes |
|---|---|---|
| _(none yet)_ | — | create one with `scripts/new-target.ps1 -Name web` |
