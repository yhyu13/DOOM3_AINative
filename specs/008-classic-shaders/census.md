# Program census: classic ARB2 shader programs

**Status**: static census **complete and machine-verified** (2026-10-28). Runtime census on
retail data **NOT RUN — SKIPPED, not passed**.

**Scope**: every shader program the classic ARB2 renderer can request, as referenced by the
pristine engine source. Fixed-function-equivalent stage state (blend, depth, texgen) is covered by
the HLSL library plan, not by this census.

**Source of truth**: `doom3_orig/doom3` (GPL release, read-only submodule). No game data was read
or is required to reproduce the table below.

---

## 1. What this census answers

1. Which program files does the engine name in code?
2. Which of them are actually bound on the ARB2 path, which are debug-only, which belong to removed
   backends, and which are dead?
3. Which program names can arrive from data (materials) and therefore cannot be enumerated from
   source alone?
4. Which of those questions are proven, and which remain blocked on legal local data?

The answer to (3) is why this is a *census*, not a file list: the engine accepts arbitrary program
names from `.mtr` files, so a source-only count is a lower bound.

---

## 2. Method and reproducibility

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/census.ps1 -Verify
```

`census.ps1` parses `neo/renderer/draw_arb2.cpp` for the `progDef_t` table, scans every
`neo/renderer/*.cpp` for `qglBindProgramARB` sites, reads the material program keywords from
`neo/renderer/Material.cpp`, and checks `targets/win11-amd64/sources.cmake` for whether
`draw_exp.cpp` is compiled. It then compares the result with `census.json` and with the two
generated blocks below; any drift fails with exit code 1.

Program ids are the `program_t` enum values (`neo/renderer/tr_local.h:1316`); the static table spans `neo/renderer/draw_arb2.cpp:337-358`. Classification is mechanical:

The rules are applied **in this order**; precedence matters because `test.vfp` is also bound from
`draw_arb2.cpp`:

| Order | Label | Rule |
|---:|---|---|
| 1 | `debug-only` | enum is `VPROG_TEST`/`FPROG_TEST`, bound only under `r_testARBProgram` (default `0`) |
| 2 | `live-default` | bound from `draw_arb2.cpp` or `draw_common.cpp` (the ARB2 path) |
| 3 | `non-ARB2` | bound only from `draw_nv20.cpp` / `draw_r200.cpp` / `draw_nv10.cpp` |
| 4 | `dead` | no bind in any compiled translation unit |

`draw_exp.cpp` is not compiled, so its binds are recorded in `boundIn` but do not influence the
label. The extractor fails closed if `draw_exp.cpp` becomes compiled or if a renderer file with a
bind site is not in the known backend map; both would invalidate this table rather than silently
change it.

`boundIn` in `census.json` lists raw bind sites including `draw_exp.cpp`; classification already
accounts for that file not being compiled.

---

## 3. Static table

<!-- census:table:begin -->
| # | line | target | enum | `glprogs/` file | classification |
|---:|---:|---|---|---|---|
| 1 | 338 | VP | `VPROG_TEST` | `test.vfp` | debug-only |
| 2 | 339 | FP | `FPROG_TEST` | `test.vfp` | debug-only |
| 3 | 340 | VP | `VPROG_INTERACTION` | `interaction.vfp` | live-default |
| 4 | 341 | FP | `FPROG_INTERACTION` | `interaction.vfp` | live-default |
| 5 | 342 | VP | `VPROG_BUMPY_ENVIRONMENT` | `bumpyEnvironment.vfp` | live-default |
| 6 | 343 | FP | `FPROG_BUMPY_ENVIRONMENT` | `bumpyEnvironment.vfp` | live-default |
| 7 | 344 | VP | `VPROG_AMBIENT` | `ambientLight.vfp` | dead |
| 8 | 345 | FP | `FPROG_AMBIENT` | `ambientLight.vfp` | dead |
| 9 | 346 | VP | `VPROG_STENCIL_SHADOW` | `shadow.vp` | live-default |
| 10 | 347 | VP | `VPROG_R200_INTERACTION` | `R200_interaction.vp` | non-ARB2 |
| 11 | 348 | VP | `VPROG_NV20_BUMP_AND_LIGHT` | `nv20_bumpAndLight.vp` | non-ARB2 |
| 12 | 349 | VP | `VPROG_NV20_DIFFUSE_COLOR` | `nv20_diffuseColor.vp` | non-ARB2 |
| 13 | 350 | VP | `VPROG_NV20_SPECULAR_COLOR` | `nv20_specularColor.vp` | non-ARB2 |
| 14 | 351 | VP | `VPROG_NV20_DIFFUSE_AND_SPECULAR_COLOR` | `nv20_diffuseAndSpecularColor.vp` | non-ARB2 |
| 15 | 352 | VP | `VPROG_ENVIRONMENT` | `environment.vfp` | live-default |
| 16 | 353 | FP | `FPROG_ENVIRONMENT` | `environment.vfp` | live-default |
| 17 | 354 | VP | `VPROG_GLASSWARP` | `arbVP_glasswarp.txt` | dead |
| 18 | 355 | FP | `FPROG_GLASSWARP` | `arbFP_glasswarp.txt` | live-default |
<!-- census:table:end -->

---

## 4. Counts

<!-- census:counts:begin -->
- `static_entries` = 18
- `static_distinct_files` = 13
- `live_default_entries` = 8
- `live_default_distinct_files` = 5
- `debug_only_entries` = 2
- `debug_only_distinct_files` = 1
- `non_arb2_entries` = 5
- `non_arb2_distinct_files` = 5
- `dead_entries` = 3
- `dead_distinct_files` = 2
- `max_glprogs` = 200
- `free_slots` = 182
- `material_resolver_calls` = 6
- `material_keywords` = 4
- `draw_exp_compiled` = False
<!-- census:counts:end -->

---

## 5. Reading the census

### 5.1 The authoring set is five files, not thirteen

Only five distinct files are bound on the default ARB2 path:

| File | Stages | Bound from | Why it is needed |
|---|---|---|---|
| `interaction.vfp` | VP + FP | `neo/renderer/draw_arb2.cpp:157` | per-light surface interaction (normal, diffuse, specular, falloff) |
| `bumpyEnvironment.vfp` | VP + FP | `neo/renderer/draw_common.cpp:213-215` | per-pixel cube reflection with a bump stage |
| `environment.vfp` | VP + FP | `neo/renderer/draw_common.cpp:222-224` | per-pixel cube reflection without a bump stage |
| `shadow.vp` | VP | `neo/renderer/draw_arb2.cpp:290` | stencil-volume projection to infinity |
| `arbFP_glasswarp.txt` | FP | `neo/renderer/draw_common.cpp:155` | heat-haze / glass warp fragment stage |

The fragment-only glasswarp program is real: the vertex side of `TG_GLASSWARP` uses fixed-function
texgen (`neo/renderer/draw_common.cpp:153`), so there is no `arbVP_glasswarp` bind anywhere.

`test.vfp` is debug-only (`r_testARBProgram`, default `0`; binds at `neo/renderer/draw_arb2.cpp:153-158`). Authoring it is optional but cheap and
gives the library a second, independent normal-mapping implementation to test against.

### 5.2 What must not be authored

| File | Entries | Reason |
|---|---|---|
| `ambientLight.vfp` | VP + FP | no bind in any compiled translation unit; the only bind is `draw_exp.cpp`, which is not built |
| `arbVP_glasswarp.txt` | VP | no bind anywhere; the glasswarp path binds only the fragment program |
| `R200_interaction.vp` | VP | `draw_r200.cpp` only; the R200 backend is scheduled for deletion |
| `nv20_bumpAndLight.vp`, `nv20_diffuseColor.vp`, `nv20_specularColor.vp`, `nv20_diffuseAndSpecularColor.vp` | 4 × VP | `draw_nv20.cpp` only; the NV20 backend is scheduled for deletion |

These still get loaded at startup: `R_ReloadARBPrograms_f` walks the whole table regardless of the
selected backend (`neo/renderer/RenderSystem_init.cpp:696`). In the port they must be classified as
`absent`/`unsupported` in the manifest so that a data reference to them is reported instead of
silently binding nothing.

### 5.3 Programs outside the static table

| Source | Name | Status |
|---|---|---|
| Material keyword `program <file>` | arbitrary; both VP and FP requested from one file (`neo/renderer/Material.cpp:1490`) | cannot be enumerated from source |
| Material keyword `vertexProgram <file>` | arbitrary (`neo/renderer/Material.cpp:1503`) | cannot be enumerated from source |
| Material keyword `fragmentProgram <file>` | arbitrary (`neo/renderer/Material.cpp:1497`) | cannot be enumerated from source |
| Material keyword `megaTexture <file>` | forced `megaTexture.vfp` (`neo/renderer/Material.cpp:1517`) | not in the table; see decision D-003 in `plan.md` |
| `draw_exp.cpp` | 13 files / 20 calls (`neo/renderer/draw_exp.cpp:672`) | not compiled; out of scope |

The table has 182 free slots after the 18 static entries (`MAX_GLPROGS = 200`,
`neo/renderer/draw_arb2.cpp:334`). Material-declared programs are appended there at load time, so
a data set can exhaust the table. Counts alone cannot prove completeness.

---

## 6. Silent-failure facts that shape the resolver

These are the reasons a resolver is required rather than an error string. Full contract:
[`resolver.md`](resolver.md).

| # | Fact | Evidence | Consequence today |
|---|---|---|---|
| C-1 | `ident` is assigned **before** the file is parsed or compiled | `neo/renderer/draw_arb2.cpp:398` | a nonzero program id does not mean the program loaded |
| C-2 | Static entries are pre-seeded with the enum value, so they are nonzero even when the file is missing | `neo/renderer/draw_arb2.cpp:338` | a missing stock program is indistinguishable from a loaded one at the call site |
| C-3 | A missing file prints `: File not found` and returns | `neo/renderer/draw_arb2.cpp:379` | console noise, no error, draw continues |
| C-4 | Lookup strips the last extension and compares case-insensitively | `neo/renderer/draw_arb2.cpp:470`, `neo/idlib/Str.cpp:761` | `foo.vp`, `foo.vfp`, `Foo.vfp` alias; first request wins |
| C-5 | Names are copied into a 64-byte field | `neo/renderer/draw_arb2.cpp:493` | names longer than 63 bytes are truncated/aliased |
| C-6 | The table walk tests `progs[i].name[0]` before the `i == MAX_GLPROGS` check | `neo/renderer/draw_arb2.cpp:473`, `neo/renderer/draw_arb2.cpp:486` | out-of-bounds read when all 200 slots are named |
| C-7 | `program` resolves the same token for both targets | `neo/renderer/Material.cpp:1492` | a one-sided file yields a valid id for a stage it does not contain |
| C-8 | `newStage` is allocated when **either** program id is nonzero | `neo/renderer/Material.cpp:1542` | the other stage is bound as 0 and the draw still runs |
| C-9 | Draw binds and draws with no validity check | `neo/renderer/draw_common.cpp:807`, `neo/renderer/draw_common.cpp:833`, `neo/renderer/draw_common.cpp:837` | unlit/black/undefined output, no actionable error |
| C-10 | Materials are parsed before GL is initialized | `neo/renderer/RenderSystem_init.cpp:2158`, `neo/renderer/RenderSystem_init.cpp:683`, `neo/framework/Common.cpp:2949`, `neo/framework/Common.cpp:3012` | a dynamically named program resolved early gets id 0 and the stage is permanently built without it |
| C-11 | `reloadARBprograms` prints the same banner on total failure and total success | `neo/renderer/draw_arb2.cpp:505-512` | no aggregate health signal exists |

C-6 and C-10 are latent in the retail engine; C-1 through C-5, C-7 through C-9, and C-11 are the
normal failure modes this port must remove.

---

## 7. Runtime census: procedure, status, and what would count as evidence

### 7.1 Status

**BLOCKED / SKIPPED.** No `*.pk4` exists in the worktree, `targets/win11-amd64/doom3/base` contains
only `default.cfg`, and no retail install was found at the default Steam path on this machine on
2026-10-28.
Game data is EULA-bound and must not be committed, so the runtime census cannot be reproduced from
the repository alone. This is recorded as a skip, not a pass.

### 7.2 Procedure (to run when authorized local data exists)

1. Build the census build with resolver logging enabled (implementation task T-021).
2. Enumerate `base/*.pk4` and list `glprogs/*` entry names and `materials/*.mtr` names. Record
   **names and counts only**; do not extract or commit shader text.
3. Parse the `.mtr` files for `program`, `vertexProgram`, `fragmentProgram` and `megaTexture`
   tokens. Each occurrence becomes a request. `postProcess` is a sort key
   (`neo/renderer/Material.cpp:358`), not a program request; record it separately if post-process
   coverage matters.
4. Run `DOOM3.exe +set r_shaderMode strict +map <representative map>` for the parity scene set.
5. Dump `shader_status` (requested, resolved, unresolved, unsupported, per-material counts).
6. Compare step 3 with step 5. Every requested name must be in the manifest or explicitly
   `unsupported` with a reason.
7. Record in `evidence/runtime-census.md` (created when this procedure is first run): data-set label and version, date, machine, map list,
   counts, and the full list of unresolved/unsupported names. No shader text, no pak contents.

### 7.3 Acceptance

- Zero `UNRESOLVED` names on the parity map set.
- Every `UNSUPPORTED` name has a documented decision and a visible fallback.
- The requested set is a subset of the manifest; any new name is added by PR with a provenance
  record.
- A skipped run is reported as SKIPPED with the reason, never as a pass.

---

## 8. What is proven, and what is not

| Claim | Status | Evidence |
|---|---|---|
| The static table has 18 entries over 13 files | **proven** | `census.ps1 -Verify`, `census.json` |
| 5 files are bound on the default ARB2 path | **proven** | bind sites in the table above |
| `ambientLight.vfp` and `arbVP_glasswarp.txt` are unbound | **proven** | no compiled bind site |
| `draw_exp.cpp` is not compiled | **proven** | absent from `targets/win11-amd64/sources.cmake` |
| Materials can request arbitrary programs | **proven** | `neo/renderer/Material.cpp:1490` |
| The retail data requests exactly this set | **not proven** | runtime census not run |
| The five live-default programs (plus optional `test.vfp`) are the whole authoring set | **not proven** | `megaTexture.vfp` and material-declared programs are outside the table |

The last two rows are why the runtime census remains an open gate. Do not claim "the port covers
all shaders" from this document.

---

## 9. Open questions

1. Does retail data actually reference `megaTexture.vfp`, or only the editor/test maps? (Needs the
   runtime census.)
2. Are there material stages that request a program file by a name that collides under the
   extension-stripping rule? (Census step 3, compared by normalized key.)
3. Does any shipped material name a program without a file extension? (The lookup supports it;
   whether data uses it is unknown.)
