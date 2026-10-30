# Fail-loud shader resolver: contract

**Status**: contract **complete**; executable vectors **14/14 passing** (2026-10-28). The engine call
sites are wired as of 2026-10-29 (modified engine files delivered through the tracked target patch);
the tolerant-mode visible error material is not drawn yet. See `evidence/implementation.md` §12.

**Goal**: no draw may ever run with an unresolved shader program, and every failure must name the
material, the stage, the program, and the reason.

**Companion**: [`census.md`](census.md) establishes the program set; this file defines how the port
resolves it. [`abi.md`](abi.md) defines what a resolved program must implement.

---

## 1. Why the current behaviour is not acceptable

The retail engine has no resolver; it has a string-keyed cache whose failure mode is invisible.
The evidence is in [`census.md`](census.md) §6. The three facts that drive this design:

1. A nonzero program id does **not** mean the program compiled
   (`neo/renderer/draw_arb2.cpp:396`). Static entries are nonzero even when the file is missing.
2. A missing program is followed by a normal draw
   (`neo/renderer/draw_common.cpp:837`), so the failure surfaces as unlit geometry, a black
   surface, or driver-defined output — never as an error.
3. A material can bake the failure permanently at parse time, before GL exists
   (`neo/renderer/RenderSystem_init.cpp:2158`).

A resolver that only replaces `Printf` with `Warning` would leave (2) and (3) intact. The contract
below changes the decision point: **resolution happens once, produces a status, and the draw is
gated on that status.**

---

## 2. Invariants

| ID | Invariant | Why |
|---|---|---|
| I-1 | A draw with an unresolved, unsupported, stubbed, absent, ambiguous, compile-failed or not-compiled program must not execute. | The visible failure is currently indistinguishable from a lighting bug. |
| I-2 | Resolution never probes the filesystem or compiles a shader at draw time. | Draw-time compilation hides latency and cannot be counted. |
| I-3 | `id != 0` is never used as a health signal. Health is an explicit status field. | Evidence C-1, C-2. |
| I-4 | Every non-OK record carries: material, stage index, keyword, program name, normalized key, target, code, mode. | Actionable without a debugger. |
| I-5 | Both strict and tolerant modes are loud. Tolerant substitutes a visible error material; it does not draw the broken stage. | "Tolerant" must not mean "silent". |
| I-6 | Resolution is idempotent and repeatable: reload re-resolves every request and re-validates every program. | Shader iteration is the main workflow. |
| I-7 | The manifest is the single source of truth for ownership and status; the engine table is checked against it, not hand-maintained in parallel. | Two lists drift; one must win. |
| I-8 | Data compatibility is preserved at the boundary: extension-insensitive, case-insensitive, path-sensitive keys. | Retail `.mtr` files must keep resolving. |

---

## 3. Vocabulary

| Term | Meaning |
|---|---|
| **request** | one `(keyword, name, target)` from the engine table or a material |
| **key** | the normalized lookup key: last extension stripped, lowercased, path preserved |
| **manifest** | versioned JSON listing every program the port knows, with stage coverage, status and provenance |
| **record** | the resolution result for one request: status, code, detail, manifest entry |
| **owned** | an authored program exists, compiles for every required backend, and has tests |
| **stub** | a deliberate placeholder that renders a recognisable pattern; not parity |
| **unsupported** | known, decided out of scope for this milestone, with a reason |
| **absent** | a name the engine or data may request, with no implementation planned (dead/NV20/R200) |
| **error material** | a generated, unmistakable material substituted in tolerant mode |

---

## 4. Manifest

### 4.1 Schema

```json
{
  "schema": "doom3-classic-programs/1",
  "programs": [
    {
      "key": "interaction",
      "stages": ["vs", "ps"],
      "status": "owned",
      "provenance": "PV-001",
      "source": "shaders/classic/programs/interaction.hlsl",
      "entry": { "vs": "vs_main", "ps": "ps_main" },
      "arity": { "vertexParms": 0, "fragmentMaps": 7 },
      "notes": ""
    }
  ]
}
```

| Field | Required | Rule |
|---|---|---|
| `key` | yes | unique unless deliberately testing ambiguity; lowercase; no extension |
| `stages` | yes | subset of `["vs","ps"]`, non-empty |
| `status` | yes | `owned` \| `stub` \| `unsupported` \| `absent` |
| `provenance` | yes | id resolvable in [`provenance.md`](provenance.md) |
| `source` | for `owned` | path under `shaders/`, tracked |
| `entry` | for `owned` | entry-point names per stage |
| `arity` | recommended for `owned` | local-parameter and fragment-map arity a stage expects (`abi.md` §5) |
| `notes` | for non-owned | one sentence, shown in the error detail |

The live example is [`tools/resolver-vectors.json`](tools/resolver-vectors.json); its `manifest`
array is the schema in use by the harness. That fixture also contains synthetic keys
(`one_sided`, `stubprogram`, `dupkey`) and `PV-Txx` provenance ids. They exist to exercise the
decision table and must never appear in the shipping manifest.

### 4.2 Required entries for the static table

The 18 static entries collapse to 13 keys; `megatexture` is data-forced and is added here because the engine can require it, so the shipping manifest holds **14 keys**: 6 authored-or-pending, 1 unsupported, 7 absent. Required status in milestone 1:

| Key | Stages | Status | Why |
|---|---|---|---|
| `interaction` | vs, ps | owned | live-default interaction program |
| `environment` | vs, ps | owned | live-default cube reflection |
| `bumpyenvironment` | vs, ps | owned | live-default bumpy cube reflection |
| `shadow` | vs | owned | live-default stencil-volume projection |
| `arbfp_glasswarp` | ps | owned | live-default warp fragment stage |
| `test` | vs, ps | owned | debug-only; authoring is optional (PV-006) |
| `megatexture` | vs, ps | unsupported | decision D-003 |
| `ambientlight` | vs, ps | absent | dead entry |
| `arbvp_glasswarp` | vs | absent | dead entry |
| `r200_interaction` | vs | absent | backend scheduled for deletion |
| `nv20_bumpandlight` | vs | absent | backend scheduled for deletion |
| `nv20_diffusecolor` | vs | absent | backend scheduled for deletion |
| `nv20_specularcolor` | vs | absent | backend scheduled for deletion |
| `nv20_diffuseandspecularcolor` | vs | absent | backend scheduled for deletion |

Material-declared programs cannot be pre-listed. They resolve against the manifest at material
parse time; a name with no entry is `UNRESOLVED`, which is the intended loud failure.

---

## 5. Resolution algorithm

```text
resolve(keyword, name, target):
    if len(name) > 63:                      -> NAME_TOO_LONG
    key = normalize(name)                   # last '.' stripped, lowercase, path kept
    entries = manifest[key]
    if entries is empty:                    -> UNRESOLVED
    if entries.size > 1:                    -> AMBIGUOUS
    e = entries[0]
    if target not in e.stages:              -> STAGE_MISSING
    switch e.status:
        owned       -> OK(e)
        stub        -> STUB(e)
        unsupported -> UNSUPPORTED(e)
        absent      -> ABSENT(e)
        default     -> MANIFEST_INVALID

request(keyword, name):
    if keyword == "program":  r_vs = resolve(..., "vs"); r_ps = resolve(..., "ps")
                              return both, atomically
    else:                     one resolve for the keyword's target

load(manifest):
    validate every entry; any defect -> MANIFEST_INVALID, refuse to start (all modes)

apply(records, mode):
    if all OK:                    draw
    else if mode == "strict":     reject material, log once, count
    else if mode == "tolerant":   substitute error material, log once, count
    else if mode == "fatal":      common->Error(record)         # CI only
    # "off" is a debugging escape hatch; it still logs, it never counts as evidence
```

`program` is atomic: if either stage is non-OK, the stage pair is not installed. This closes
evidence C-7 and C-8 (a one-sided file bound as a valid program on the other stage).

### 5.1 Two phases, because blobs do not exist at material parse time

The engine parses materials before GL exists (`neo/renderer/RenderSystem_init.cpp:2158`). A single
"resolve everything at load" step cannot work: name resolution is possible there, blob validation is
not. Split it:

| Phase | When | Inputs | Produces | Codes possible |
|---|---|---|---|---|
| 1 — name resolution | material parse, no device | request, manifest | key + entry + stage coverage; material stores the key, not a raw id | `OK`, `UNRESOLVED`, `STAGE_MISSING`, `STUB`, `UNSUPPORTED`, `ABSENT`, `AMBIGUOUS`, `NAME_TOO_LONG`, `MANIFEST_INVALID` |
| 2 — blob binding | device init and every reload | phase-1 key, active backend | valid program handle for this backend | `OK`, `COMPILE_FAILED`, `NOT_COMPILED` |

Rules:

- A material never stores a phase-2 handle at parse time; it stores the phase-1 key and a validity
  flag. This removes the C-10 ordering hazard without needing a device.
- The draw gates on **both** phases: phase-1 OK and phase-2 OK.
- A reload re-runs phase 1 for every request and phase 2 for every blob, then reports the counts.
- Phase 1 is idempotent and cheap; phase 2 is the only part that touches the backend.

This is why `COMPILE_FAILED` and `NOT_COMPILED` are described as runtime states in §6: they cannot
be produced by the vectors, which model phase 1 only.

---

## 6. Codes and decisions

| Code | Trigger | `draw` | `strict` | `tolerant` |
|---|---|---|---|---|
| `OK` | owned entry, stage covered | yes | draw | draw |
| `UNRESOLVED` | no manifest entry for the key | no | reject material | error material |
| `STAGE_MISSING` | entry exists, stage absent | no | reject material | error material |
| `STUB` | status `stub` | no | reject material | error material |
| `UNSUPPORTED` | status `unsupported` | no | reject material | error material |
| `ABSENT` | status `absent` | no | reject material | error material |
| `AMBIGUOUS` | duplicate manifest key | no | reject material | error material |
| `NAME_TOO_LONG` | request > 63 bytes | no | reject material | error material |
| `MANIFEST_INVALID` | missing/invalid field, unknown status | no | refuse to start | refuse to start |
| `COMPILE_FAILED` | owned entry's blob failed validation | no | reject material | error material |
| `NOT_COMPILED` | no blob for the active backend | no | reject material | error material |

`MANIFEST_INVALID` is a build/packaging defect, not a data defect: it is fatal in every mode. A
malformed manifest must never degrade to "everything unresolved", because that would look like a
data problem.

The harness covers nine of the eleven codes: it validates the manifest first (`MANIFEST_INVALID`)
and then runs R-001 … R-014 in [`tools/resolver-vectors.json`](tools/resolver-vectors.json) for the
eight request-level codes. `COMPILE_FAILED` and `NOT_COMPILED` are phase-2 runtime states and will
be pinned by the engine test in T-024.

---

## 7. Modes

| Mode | When | Behaviour on non-OK | Evidence status |
|---|---|---|---|
| `off` | local debugging only | logs, draws anyway | **not evidence** |
| `tolerant` | shipping fallback, mod tolerance | error material, one log line, counter | valid with a recorded count |
| `strict` | **default for Q1/Q2 evidence builds** | material rejected before it can be drawn | valid when the count is zero |
| `fatal` | automated gates | `common->Error` with the full record | valid, exit non-zero |

`r_shaderMode` selects the mode; `r_shaderMode` changes take effect on `shader_reload`, not
mid-frame. The default is `strict` until classic parity is measured; the release default is
revisited with evidence, not assumed.

**Implementation note (2026-10-29).** In the current tree every authored program is `stub`
(`owned=0`), so a `strict` default would refuse every program in the shipping manifest. The engine
therefore ships with `r_shaderMode off` as the default until the first program is `owned`; `off` is the
debugging escape hatch above and is never evidence. Evidence builds set `+set r_shaderMode strict`
(or `tolerant`) explicitly, and every `shader_status` line names the mode. Restoring `strict` as the
default is part of promoting the first program to `owned`.

The error material is a generated magenta/black checkerboard with the program key in the material
name shown by `shader_status`. It is deliberately ugly: a black surface is a bug report nobody
files.

---

## 8. Diagnostics contract

### 8.1 One log line per unique failure

```text
SHADER ERROR: material="textures/base/wall" stage=2 keyword=vertexProgram target=vs
  program="foo.vfp" key="foo" code=UNRESOLVED mode=strict
  detail="no manifest entry for 'foo'"
  action="material rejected; stage not drawn"
```

Rules:

- Prefix `SHADER ERROR:` is stable and machine-parseable.
- The record is emitted once per `(material, stage, program, code)`; repeats increment a counter.
- The material name is the resolved material name, not the file path.
- `action` states the visible consequence.

### 8.2 Aggregate status

`shader_status` prints one line:

```text
shader_status: manifest=14 owned=0 stub=6 unsupported=1 absent=7
  requests=1234 ok=1230 unresolved=4 stage_missing=0 ambiguous=0
  materials_rejected=2 materials_error=0 mode=strict
```

`shader_reload` (kept as an alias of the existing `reloadARBprograms`) re-resolves everything,
re-validates blobs, and prints the same status line before and after. A reload that changes the
non-OK count must say so explicitly; the current implementation prints the same banner either way
(evidence C-11).

### 8.3 Counters

Counters are monotonic per run and reset on `shader_reload`. They are the only accepted proof that
a scene rendered without unresolved programs, because a screenshot cannot show a missing stage that
happens to be black.

---

## 9. Data compatibility

The retail key semantics are preserved because materials depend on them:

| Behaviour | Source | Resolver rule |
|---|---|---|
| last extension stripped anywhere in the string | `neo/idlib/Str.cpp:761` | same normalization |
| case-insensitive comparison | `neo/renderer/draw_arb2.cpp:481` | lowercase key |
| only the extension is stripped; no path normalisation | `neo/renderer/draw_arb2.cpp:470` (strips the suffix), `:481` (compares the whole remaining string) | keep the prefix in the key |
| 63-byte name field | `neo/renderer/draw_arb2.cpp:493` | reject longer names instead of truncating |
| first match wins | `neo/renderer/draw_arb2.cpp:481` | **not preserved**: duplicate keys are `AMBIGUOUS` |

The last row is the one deliberate incompatibility. If retail data relies on two files whose keys
collide under extension stripping, the runtime census must reveal it; silently keeping first-wins
would reproduce the bug the resolver exists to remove.

---

## 10. Engine integration

| # | Change | Anchor |
|---|---|---|
| 1 | Replace `R_LoadARBProgram` with a manifest-driven load that returns a record and never assigns an id before success | `neo/renderer/draw_arb2.cpp:365` |
| 2 | Replace `R_FindARBProgram` with `R_ResolveProgram(target, name, record)`; keep the key rules of §9 | `neo/renderer/draw_arb2.cpp:466` |
| 3 | Make `program` atomic at parse time; never allocate `newStage` from a partial pair | `neo/renderer/Material.cpp:1490`, `neo/renderer/Material.cpp:1542` |
| 4 | Store a validated handle (or an explicit status) in `newShaderStage_t`; a raw `int` that can be 0 is not sufficient | `neo/renderer/Material.h:192` |
| 5 | Gate every bind and draw on the status; bind 0 is a programming error, not a fallback | `neo/renderer/draw_common.cpp:807`, `neo/renderer/draw_common.cpp:833`, `neo/renderer/draw_arb2.cpp:157` |
| 6 | Load the manifest before `R_InitMaterials`, or make resolution deferred and re-run after GL init | `neo/renderer/RenderSystem_init.cpp:2158`, `neo/renderer/RenderSystem_init.cpp:683` |
| 7 | Generated table check: a test asserts the compiled `progs[]` keys equal the manifest's engine-side set | `neo/renderer/draw_arb2.cpp:337` |
| 8 | Startup logs one summary line, not one line per dead entry | `neo/renderer/RenderSystem_init.cpp:696` |

Items 1–5 are the resolver core; 6 is the ordering defect; 7 keeps the two lists honest; 8 is
operator ergonomics.

Delivery (2026-10-29): items 1–8 are implemented in the engine copy and exported as modified-file
hunks in `targets/win11-amd64/patches/all-changes.patch`; the pristine submodule is untouched. The
classic GL backend still loads ARB text from data, so phase 2 binds that text; the authored HLSL is
not consumed by this backend. Tolerant mode currently skips the broken stage instead of drawing the
error material, so I-5 is only partly met.

---

## 11. Verification

| Layer | What it proves | Command |
|---|---|---|
| vectors | decision table, codes, draw gating, key semantics | `powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/resolver-conformance.ps1` |
| census | the engine-side key set the manifest must cover | `powershell -NoProfile -ExecutionPolicy Bypass -File specs/008-classic-shaders/tools/census.ps1 -Verify` |
| engine test (future, T-024) | a real material with a missing program produces the record and no draw; fault-injected compile failure produces `COMPILE_FAILED` | `tests/renderer/shader_resolver_test.cpp` (to be created) |
| scene gate (future) | a full map renders with `unresolved=0` | `scripts/test-render-baseline.ps1` (owned by the Q0 baseline workstream) |

The vectors are a reference model, not a claim that the engine contains this code.

---

## 12. Non-goals

- Not a shader translator. It never reads retail ARB text.
- Not a pipeline-variant system. Variant keys and PSO caching belong to the renderer.
- Not a hot-reload implementation. It defines what a reload must report.
- Not a licence decision. Provenance is [`provenance.md`](provenance.md).

---

## 13. Open questions

1. Does the engine's `program` keyword ever appear in retail data, or is it only `vertexProgram` /
   `fragmentProgram`? (Runtime census; changes how often atomic-pair handling is exercised.)
2. Is a 63-byte rejection safe, or does retail data contain long names that currently truncate to a
   working file? (Runtime census, unresolved-name list.)
