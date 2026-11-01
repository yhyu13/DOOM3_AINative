# Runtime census — official free DOOM 3 demo

**Status**: **RUN on the official free demo (2026-11-01)**. Retail data is still **NOT RUN**.
**Acceptance at `census.md` §7.3 is not met**: the demo's three maps request four program names that
the manifest does not contain. They are recorded below; the manifest was not edited to make the gate
green.

This file records names and counts only. No shader text, material text or pak content is stored here
or in `runtime-census-demo.json`.

## 1. Data set and provenance

| Item | Value |
|---|---|
| Data set | Official free DOOM 3 demo, `demo00.pk4` (id Software / Activision demo release) |
| `demo00.pk4` size | `483,534,533` bytes |
| `demo00.pk4` SHA-256 | `E373EC70B35CF72852A1F6014C2C2DC21F890A03CD41B21A9524755F951F64F9` |
| Installer MD5 (as supplied) | `5172537e1ea114ec857766f6af06a211` |
| Engine-reported checksum | `0x851c03dd` (`Loaded pk4 ... with checksum 0x851c03dd`) |
| Entry count | `12,234` files |
| Maps | `game/demo_mars_city1`, `game/demo_mars_city2`, `game/demo_mc_underground` |
| Staging | `scripts/prepare-demo-data.ps1 -Pk4 <path> -RunDir <worktree>\_data\run` (exit 0) |
| Committed? | **No.** `_data/` is git-ignored; only this derived census is committed |

The demo is free to obtain and use. Its `.pk4` and extracted material/shader text are not committed.

## 2. Procedure (census.md §7)

| Step | What was run | Result |
|---|---|---|
| 1 | `cmake --preset win11-amd64`, `cmake --build --preset win11-amd64-release`; resolver logging on | configure exit 0, build exit 0 |
| 2 | `specs/008-classic-shaders/tools/runtime-census.ps1 -Pk4 _data\run\base\demo00.pk4` | exit 0; enumerates entries, `glprogs/*`, `materials/*.mtr` |
| 3 | Same script parses `program`, `vertexProgram`, `fragmentProgram`, `megaTexture` | 73 occurrences → 106 target requests |
| 4 | `scripts/run-win11-amd64.ps1 ... -Map <map> -ExtraArgs @('+set','r_shaderMode','strict','+shader_status')` | 3 maps, 3 × exit 0 |
| 5 | `shader_status` from each `qconsole.log` | table in §3 |
| 6 | Compare §2.3 with §3 | 4 runtime names unresolved; see §4 |
| 7 | This file | names/counts only |

All logs are under `build/` (git-ignored). The full machine-readable observation is
`evidence/runtime-census-demo.json`.

## 3. Static observation (step 3)

| Metric | Value |
|---|---|
| `.mtr` files | 67 |
| Material declarations | 6,341 |
| `program` occurrences | 32 |
| `vertexProgram` occurrences | 17 |
| `fragmentProgram` occurrences | 23 |
| `megaTexture` occurrences | 1 (`area3.mega` → forced `megaTexture.vfp`, D-003) |
| Target requests (engine semantics: `program` = vs+ps) | 106 |
| Distinct normalized keys | 7 |
| Key collisions after last-extension stripping | 0 |
| Names longer than 63 characters | 0 |
| `postProcess` tokens | 0 |
| `glprogs/*` files in data | 15 |
| `glassWarp` stages | 0 |

Distinct requested keys (static):

| Key | Names | Requests | Materials |
|---|---|---|---|
| `bumpyenvironment` | `bumpyEnvironment.vfp` | 2 | 1 |
| `colorprocess` | `colorProcess.vfp` | 7 | 7 |
| `heathaze` | `heatHaze.vfp` | 12 | 6 |
| `heathazewithmask` | `heatHazeWithMask.vfp` | 28 | 15 |
| `heathazewithmaskandvertex` | `heatHazeWithMaskAndVertex.vfp` | 21 | 21 |
| `megatexture` | `megaTexture.vfp` (from `area3.mega`) | 1 | 1 |
| `pinch` | `pinch.cg` | 2 | 1 |

The data also ships four `glprogs` files outside the 18-entry static table (`colorProcess.vfp`,
`heatHaze.vfp`, `heatHazeWithMask.vfp`, `heatHazeWithMaskAndVertex.vfp`). That confirms §5.3: material
programs are not enumerable from the engine source.

## 4. Runtime observation (steps 4–6)

All three maps were loaded in strict mode, spawned a player, and reported a `shader_status` line after
map initialisation. "Requests" includes the 18 engine-table requests made at init; `unresolved` counts
per-target records from map-parsed materials.

| Map | requests | ok | unresolved | materials_rejected | diagnostic lines | Runtime keys |
|---|---|---|---|---|---|---|
| `game/demo_mars_city1` | 33 | 0 | 15 | 6 | 10 | `heathaze`, `heathazewithmask`, `heathazewithmaskandvertex` |
| `game/demo_mars_city2` | 38 | 0 | 20 | 9 | 13 | above + `colorprocess` |
| `game/demo_mc_underground` | 31 | 0 | 13 | 6 | 9 | `heathaze`, `heathazewithmask`, `heathazewithmaskandvertex` |

`ok=0` because every engine-table entry is `stub` in this milestone. A failed `program` pair prints
one diagnostic but increments both targets, so diagnostic lines are fewer than `unresolved`.

Runtime-observed union (4 keys): `colorprocess`, `heathaze`, `heathazewithmask`,
`heathazewithmaskandvertex`.

Static keys **NOT OBSERVED** in any of the three demo maps (recorded as not observed, not guessed):

| Key | Where it is declared | Map list searched |
|---|---|---|
| `pinch` | `materials/shaderDemo.mtr` (`shaderDemos/pinch`) | all three; no stage was parsed or drawn |
| `megatexture` | `materials/shaderDemo.mtr` (`shaderDemos/megaTexture`) | all three; the map set never calls the mega-texture path |
| `bumpyenvironment` | `materials/shaderDemo.mtr` (`shaderDemos/bumpyReflect`) | all three; no environment stage reached it |

**Unresolved names (the acceptance failure):** `colorProcess.vfp`, `heatHaze.vfp`,
`heatHazeWithMask.vfp`, `heatHazeWithMaskAndVertex.vfp`. They are demo-data programs with no manifest
entry. No manifest change was made here: adding them would be a provenance/scope decision, and none of
them was promoted to `owned`.

## 5. Skip semantics observed

| Gate | State | What happens in strict/tolerant |
|---|---|---|
| Material stage `program`/`vertexProgram`/`fragmentProgram` | UNRESOLVED for the four names above | Stage not bound. Strict skips it (`material rejected; stage not drawn`); tolerant substitutes the error material. |
| `interaction` (both phases) | manifest `stub` | Strict skips every interaction draw; tolerant draws the error material for the affected surfaces. `off` binds the data ARB text and is not evidence. |
| Environment / `bumpyenvironment` texgen stages | manifest `stub` | Unreachable: the per-stage gate fails first and takes the same skip/substitute path. |
| `arbfp_glasswarp` | manifest `stub` | Unreachable in this data set: zero `glassWarp` stages were observed. |
| `shadow` (vertex) | manifest `stub` | The engine takes its fixed-function stencil-shadow fallback (`r_useShadowVertexProgram` branch is not entered). This is an alternate implementation, not a silent skip. |
| `off` mode | legacy data ARB text bound | Runs, but `census.md` §0 / `resolver.md` exclude it as parity evidence. |

## 6. Tier B variant observation (feeds T-028)

`runtime-census.ps1` also parses every ordinary stage block (a stage with no program keyword) for the
three axes in `ownership.md` §3.1: texgen mode, `alphaTest`, `vertexColor`/`inverseVertexColor`.

| Metric | Value |
|---|---|
| Ordinary stages observed | 5,358 |
| `glassWarp` stages (Tier A, excluded) | 0 |
| `postProcess` stages (excluded) | 0 |
| Observed variants | **12** (of the 42 upper bound) |

| texgen | alphatest | vertexcolor | Stages |
|---|---|---|---|
| 0 explicit | 0 | 0 ignore | 3,966 |
| 0 explicit | 0 | 1 modulate | 447 |
| 0 explicit | 0 | 2 inverse | 164 |
| 0 explicit | 1 | 0 ignore | 654 |
| 0 explicit | 1 | 1 modulate | 2 |
| 1 diffuse cube | 0 | 0 ignore | 3 |
| 2 reflect cube | 0 | 0 ignore | 78 |
| 2 reflect cube | 0 | 1 modulate | 11 |
| 3 skybox cube | 0 | 0 ignore | 8 |
| 4 wobble-sky cube | 0 | 0 ignore | 9 |
| 5 screen | 0 | 0 ignore | 14 |
| 5 screen | 0 | 1 modulate | 2 |

Texgen mode 6 (`screen2`) and the remaining alpha/vertex-colour cross-products are **NOT OBSERVED** in
this data set. The 42-variant figure remains an upper bound, not a shipping target. The twelve
observed variants are listed in `shaders/classic/stages/variants.json` and compile:
`COMPILE OK: 6 programs, 12 Tier B variants, 35 stage compiles (DXIL + SPIR-V + WGSL), 0 failures`.

## 7. Tolerant substitution evidence (T-021 / I-5)

| Evidence | Path |
|---|---|
| Tolerant frame, demo `game/demo_mars_city1` | `evidence/tolerant-error-material-demo-city1.png` |
| Strict frame, same map/pose | `evidence/strict-refused-demo-city1.png` |
| Off-mode control frame (legacy programs bound; not parity evidence) | `evidence/demo-off-city1-control.png` |

Same map, same `+devmap` start, same capture path:

| Mode | `shader_status` after map init | Diagnostics | Frame |
|---|---|---|---|
| strict | `unresolved=15 materials_rejected=6 materials_error=0` | 10 × `action="material rejected; stage not drawn"` | near-black with self-illuminated surfaces, **0 magenta pixels**, 309 distinct colours |
| tolerant | `unresolved=15 materials_rejected=0 materials_error=6` | 10 × `action="error material substituted; stage not drawn"` | magenta/black checkerboard on world surfaces, **114,031 / 480,000 = 23.76% magenta pixels**, 622 distinct colours |

The magenta test is objective, not a visual guess: a pixel counts when `R > 100`, `B > 100` and
`G < min(R,B)/2`. Strict **0/480,000**, off-mode control **0/480,000**, tolerant **114,031/480,000**.
The frames in `evidence/` were recaptured at base `04c6180`.

The tolerant frame shows the generated `_shaderError` checkerboard drawn by
`RB_DrawShaderErrorMaterial` for both the `interaction` stub and the six unresolved material stages.
The strict frame is the counter-evidence: the same non-OK stages are refused, and no magenta is drawn.
The off-mode control shows the demo itself renders correctly when the legacy ARB programs are bound,
so the strict black frame is a refusal result, not a broken build.

## 8. T-025 shadow status

The demo exercises the shadow pass: the three maps contain 466 / 639 / 616 light entities, of which
147 / 134 / 320 carry `noshadows 1`, leaving at least 319 / 505 / 296 shadow-capable lights.

**T-025 is still NOT RUN**, for two independent reasons:

1. The classic GL backend binds the data's `shadow.vp` ARB text; the port's authored `shadow.hlsl`
   is not consumed by this backend, so there is no authored shadow output to compare.
2. Retail parity (T-027) needs retail data, which this machine does not have and must not obtain.

## 9. What remains blocked

| Item | Why |
|---|---|
| Zero-unresolved acceptance (`census.md` §7.3) | four demo programs are outside the manifest; resolving them is a scope/provenance decision |
| Retail census completeness | needs retail data; the demo is not the retail map set |
| T-027 parity | needs retail data and the same rendered comparison |
| T-030 promotion to `owned` | needs the T-027 comparison |
| T-025 shadow comparison | authored program not bound by this backend + retail data |
| T-032 | legal review has not happened |

## 10. Reproduction

```powershell
scripts/prepare-demo-data.ps1 -Pk4 <demo00.pk4> -RunDir <worktree>\_data\run
cmake --preset win11-amd64
cmake --build --preset win11-amd64-release -- -m
specs/008-classic-shaders/tools/runtime-census.ps1 -Pk4 _data\run\base\demo00.pk4 -OutDir build\_census
scripts/run-win11-amd64.ps1 -Exe build\bin\Release\DOOM3.exe -DataDir _data\run `
  -OutDir build\run-strict-city1 -Map game/demo_mars_city1 -WaitSec 70 -Clean -StopExisting `
  -ExtraArgs @('+set','r_shaderMode','strict','+shader_status')
scripts/run-win11-amd64.ps1 -Exe build\bin\Release\DOOM3.exe -DataDir _data\run `
  -OutDir build\run-tolerant-city1 -Map game/demo_mars_city1 -WaitSec 70 -Clean -StopExisting `
  -ExtraArgs @('+set','r_shaderMode','tolerant','+shader_status')
```
