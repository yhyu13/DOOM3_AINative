# Implementation evidence: classic shader library and resolver core

**Date**: 2026-10-29
**Scope**: tasks T-020 … T-030 of [`../tasks.md`](../tasks.md), issue #29.
**What this file is**: the commands that were run, their real output, and the decisions that the
documents did not settle. A check that did not run is listed as not run.

Nothing here says the classic renderer works. It says the library compiles for both targets, the
resolver core behaves as the contract says, and the gates that claim those things actually fail when
they should.

---

## 1. What was created

| Area | Paths |
|---|---|
| ABI header | `shaders/classic/include/d3_abi.hlsli` |
| Inter-stage structures | `shaders/classic/include/d3_input.hlsli` |
| ARB opcode emulation | `shaders/classic/include/d3_arb.hlsli` |
| Classic programs | `shaders/classic/programs/{interaction,environment,bumpyenvironment,shadow,arbfp_glasswarp,test}.hlsl` |
| Tier B stage | `shaders/classic/stages/generic_texgen.hlsl`, `stages/variants.json` |
| Manifest | `shaders/manifest/classic-programs.json` (14 keys), `shaders/manifest/provenance.json` (9 records), `shaders/manifest/compile-goldens.json` |
| Shader tooling | `shaders/tools/{pins.json,get-toolchain.ps1,compile.ps1,reflect.ps1,manifest-check.ps1,provenance-record.ps1,verify-golden.ps1}` |
| Resolver core | `src/renderer/shader/{shader_resolver.h,shader_resolver.cpp,shader_json.h,shader_json.cpp}` |
| Resolver tests | `tests/shader/{CMakeLists.txt,shader_resolver_test.cpp}` |

Generated output lives under `out/shaders/` and `build-shader/`; both are git-ignored, so no compiled
blob is committed (decision D-P3).

---

## 2. Toolchain pin (T-023)

```powershell
powershell -NoProfile -File shaders/tools/get-toolchain.ps1
```

```text
TOOLCHAIN OK: dxc 1.9.2609.5 at shaders/tools/.cache/dxc/bin/x64/dxc.exe
  asset sha256 verified: AD31B1FC8443175D204F77A611FDB3EF2EC42759BDC2F1167368DE24A4A7E7F1
  reported: dxcompiler.dll: 1.9(5512-01b62ad4)(1.9.2609.5) - 1.9.2609.5 (01b62ad47)
```

The release asset hash is verified before extraction and the compiler's own version string must match
the pin, so a substituted compiler fails closed with exit 3. Tint is pinned the same way from a Dawn
release; it has no `--version`, so its identity is the asset SHA-256 plus the Dawn commit (see §11).
One tool stays unpinned and is recorded as a gap: a SPIR-V disassembler.

---

## 3. Compile for both targets (T-020, T-030, T-025)

```powershell
powershell -NoProfile -File shaders/tools/compile.ps1 -All
```

```text
  OK    interaction          vs  dxil=0 spv=0
  OK    interaction          ps  dxil=0 spv=0
  OK    environment          vs  dxil=0 spv=0
  OK    environment          ps  dxil=0 spv=0
  OK    bumpyenvironment     vs  dxil=0 spv=0
  OK    bumpyenvironment     ps  dxil=0 spv=0
  OK    shadow               vs  dxil=0 spv=0
  OK    arbfp_glasswarp      ps  dxil=0 spv=0
  OK    test                 vs  dxil=0 spv=0
  OK    test                 ps  dxil=0 spv=0
  OK    generic_texgen       vs  dxil=0 spv=0
COMPILE OK: 6 programs, 0 Tier B variants, 11 stage compiles, dxc 1.9.2609.5
```

Every stage is compiled twice: DXIL (`vs_6_0`/`ps_6_0`) and SPIR-V
(`-spirv -fvk-use-dx-layout -fspv-target-env=vulkan1.1`). `generic_texgen` is compiled unconditionally
because `arbFP_glasswarp` is a fragment-only program in the engine and needs a vertex stage to pair
with; that is an ABI artifact, not a shipped Tier B variant.

### 3.1 The 42-variant Tier B bound is real

```powershell
powershell -NoProfile -File shaders/tools/compile.ps1 -ProveBound
```

```text
BOUND PROOF OK: 42 variants x 2 stages = 84 compiles, 0 failures
```

7 texgen modes × 2 alpha-test states × 3 vertex-colour modes. `stages/variants.json` ships **zero**
variants, because the rule is "only the combinations the material census observes" and the census is
blocked (T-031). The bound is proven; the shipping set is honestly empty.

---

## 4. Reflection and ABI conformance (T-026)

```powershell
powershell -NoProfile -File shaders/tools/reflect.ps1
```

```text
  interaction          vs  inputs=6 outputs=10 cbuffers=1 resources=0
  interaction          ps  inputs=10 outputs=1 cbuffers=1 resources=10
  environment          vs  inputs=6 outputs=7 cbuffers=1 resources=0
  environment          ps  inputs=7 outputs=1 cbuffers=1 resources=2
  bumpyenvironment     vs  inputs=6 outputs=7 cbuffers=1 resources=0
  bumpyenvironment     ps  inputs=7 outputs=1 cbuffers=1 resources=4
  shadow               vs  inputs=1 outputs=1 cbuffers=1 resources=0
  arbfp_glasswarp      ps  inputs=7 outputs=1 cbuffers=1 resources=4
  generic_texgen       vs  inputs=5 outputs=7 cbuffers=1 resources=0
REFLECT OK: 11 stages, 5 declared cbuffers, 26 declared resources, 7 declared semantics, 0 violations
```

The gate proves four things from the DXIL disassembly: no undeclared constant buffer, no undeclared
texture or sampler, no undeclared semantic, and — the check that catches real drift — **every
fragment input signature equals its vertex output signature**. `shadow` shows one input and one output
because the shadow path carries only `POSITION0`; that is the ABI, not a truncation.

Limit, stated rather than hidden: this is the DXIL side only. The WGSL side is validated separately by
`compile.ps1` (Tint translation plus a re-parse), but cross-backend constant offsets still cannot be
compared until a SPIR-V disassembler is pinned.

---

## 5. Manifest and provenance (T-005, T-025, FR-009, FR-013)

```powershell
powershell -NoProfile -File shaders/tools/manifest-check.ps1
```

```text
MANIFEST OK: keys=14 owned=0 stub=6 unsupported=1 absent=7
```

The check proves the key set equals the census key set plus the data-forced `megaTexture.vfp`, that
`absent` is exactly the dead + removed-backend set, that every declared source exists, that every
declared entry point appears in that source, and that no synthetic `PV-Txx` id leaks into the shipping
manifest.

```powershell
powershell -NoProfile -File shaders/tools/provenance-record.ps1
```

```text
PROVENANCE RECORD OK: 9 records, 10 hashes match, every sourced program has exactly one record
```

---

## 6. Determinism and goldens

```powershell
powershell -NoProfile -File shaders/tools/verify-golden.ps1
```

```text
GOLDEN OK: 11 stage hashes deterministic across two runs and equal to the recorded goldens (dxc 1.9.2609.5)
```

Two independent compiles into separate directories produce byte-identical DXIL and SPIR-V, so the
recorded goldens mean something. A toolchain bump invalidates every golden on purpose.

---

## 7. Resolver core (T-021, T-022, T-024, T-029)

```powershell
cmake -S tests/shader -B build-shader -G "Visual Studio 17 2022" -A x64
cmake --build build-shader --config Release
cmake --build build-shader --config Release --target RUN_TESTS
```

```text
1/6 Test #1: shader.resolver.vectors .............   Passed
2/6 Test #2: shader.resolver.manifest_shipping ...   Passed
3/6 Test #3: shader.resolver.phase2 ..............   Passed
4/6 Test #4: shader.resolver.diagnostics .........   Passed
5/6 Test #5: shader.resolver.manifest_invalid ....   Passed
6/6 Test #6: shader.resolver.material_atomic .....   Passed
100% tests passed, 0 tests failed out of 6
```

The build is warning-free at `/W4 /permissive-`. The tests drive the same 14 vector cases as the
PowerShell harness, so the C++ core and the reference model cannot drift silently: the C++ suite reads
`specs/008-classic-shaders/tools/resolver-vectors.json` and asserts the same codes, decisions and draw
gates.

Mutation control (run by the implementing agent, reproduced here):

```text
flip R-004 expected UNRESOLVED -> OK  ->  FAIL: R-004 expected code=OK ... got code=UNRESOLVED
```

The repository fixture was not modified; the copy lived in the temp directory and its hash was checked
before and after.

### 7.1 Deliberate scope limit

`tests/shader` tests the **core**. The renderer call sites were added on 2026-10-29 (§12) as modified
engine files delivered through `targets/win11-amd64/patches/all-changes.patch`; the core tests still do
not prove the engine wiring, which is why §12 has its own in-engine test and map run.

---

## 8. Decisions taken during implementation

Each is a place the documents did not decide, recorded so a reviewer can disagree with a specific
choice rather than re-deriving it.

| Id | Decision | Why |
|---|---|---|
| D-I1 | Interaction lighting is evaluated in model space | `c[4]` and `c[5]` are written in model space (`tr_render.cpp:732-735`) |
| D-I2 | The bump/diffuse/specular matrices transform the model-space position | the engine writes them as S/T row pairs for the texgen path |
| D-I3 | Diffuse uses the half-Lambert wrap | any Lambert choice is a parity decision; this one is named in `d3_arb.hlsli` |
| D-I4 | Specular is analytic Blinn-Phong, not a lookup in the specular table at `t6` | the 2D table's axes cannot be recovered without retail text; the gate does not require it |
| D-I5 | Attenuation = falloff S lookup × light cube along the light→fragment direction | the two bound images are the only attenuation inputs the ABI exposes |
| D-E1 | Environment reflection is unlit and computed in model space | the environment stage is a reflection stage, not an interaction |
| D-B1 | The bump map is tangent-space; the basis is orthonormalised from both engine tangents | `tangents[0]`/`tangents[1]` are S/T directions, and a non-unit tangent must not skew the light vector |
| D-G1 | The glasswarp offset is the heat-haze image remapped to [-1,1] × `WarpStrength` | the two bound scratch images are all the ABI gives |
| D-G2 | `scratchImage` (unit 2) is bound but unused | recorded as a parity gap instead of a dummy read that would hide it |
| D-S1 | Screen texgen flips Y | matches the GL convention the classic renderer assumed |
| D-S2 | Alpha-test reference is `VertexParms[0].y` | the material passes `alphaTest` through local parameters, and local parameters are their own namespace |
| D-S3 | Fog is linear between `FogParams.x` and `.y` | fixed-function fog does not exist in HLSL, so the port has to pick a model |
| D-M1 | A program that compiles but is not `owned` ships as `stub` | `ownership.md` §5 offers no other status; the `notes` field names the unmet criterion |

### 8.1 Specification defects found by implementing it

| Id | Defect | Resolution |
|---|---|---|
| E-1 | `resolver.md` §4.2 said "13 keys" above a 14-row table, and §8.2's sample said `manifest=13 … absent=6` | corrected to 14 keys / 7 absent; confirmed by `manifest-check.ps1` |
| E-2 | `abi.md` claimed `c[4].w` was undefined for interactions; the engine sets it to 0 | corrected, including the ABI header comment |
| E-3 | `provenance.md`'s status ladder had no state between `planned` and `authored`, so an implemented-but-unowned program had no truthful status | added `compiled` with its own requirements |
| E-4 | `census.md` cited only the fragment bind lines for the two environment programs | widened to `213-215` and `222-224` |
| E-5 | `resolver.md` §8.1's log-once identity omits the target, but a `program` request produces two distinct stage failures under one stage index | the core includes the target in the identity; stated in `shader_resolver.h` |

---

## 9. Not done, and why

| Item | State | Why |
|---|---|---|
| Engine wiring of the resolver (call sites, draw gate, material parse, init order) | done 2026-10-29 | delivered as tracked modified-file hunks in `targets/win11-amd64/patches/all-changes.patch`; evidence §12 |
| `shader_status` / `shader_reload` as console commands | done 2026-10-29 | registered in `R_InitCommands`; observed in `qconsole.log` (§12.2, §12.5) |
| Tolerant-mode visible error material | not done | the draw gate skips the stage in both modes and counts the decision, but the magenta substitution required by I-5 is not drawn; T-021 is therefore `BLOCKED` on a frame it cannot get here (§13.3) |
| WGSL validation | done in §11 | pinned Tint translates every SPIR-V stage and re-parses the emitted WGSL |
| `owned` status for any program | none | WGSL is validated, but `ownership.md` §5 criterion 5 needs a rendered comparison (T-027); `stub` is the honest status |
| Parity capture (T-027) | blocked | needs authorized retail data and a classic backend |
| Runtime census (T-031) | blocked | no `*.pk4`; data is EULA-bound |
| Tier B shipping variants | none | populated only from the census, which is blocked |
| Shadow / glasswarp / interaction behaviour in a running engine | unverified | the resolver gates those binds and the demo map runs, but the authored HLSL is not bound by the GL backend and no parity capture exists |

---

## 10. Negative controls, re-run on the PR branch

`tasks.md` T-006 claims two negative controls for the provenance gate. Both were re-run here; the
fixture was removed afterwards, and the green run below is the one that ships.

```powershell
# a tracked path with a shader-source extension; the file need not exist, the path is rejected first
powershell -NoProfile -File specs/008-classic-shaders/tools/provenance-check.ps1 -TrackedFiles 'temp/leak.vfp'
```

```text
PROVENANCE CHECK FAILED (1)
 - tracked shader source: temp/leak.vfp
exit 1
```

```powershell
# an ARB opcode body pasted without its header into a documentation file
powershell -NoProfile -File specs/008-classic-shaders/tools/provenance-check.ps1 -TrackedFiles '.tmp-negative-controls/leak.md'
```

```text
PROVENANCE CHECK FAILED (1)
 - ARB opcode body (4 opcode lines) in tracked file: .tmp-negative-controls/leak.md
exit 1
```

Clean-up, then the same gate on the staged tree:

```text
PROVENANCE OK: 304 tracked files, no shader source, no ARB program bodies
```

The resolver mutation control from §7 was re-run too, against a temp copy of `resolver-vectors.json`
written as UTF-8 **without** BOM — a BOM makes the parser fail for the wrong reason, which would have
proved nothing:

```text
FAIL: R-004 expected code=OK decision=reject-material draw=false; got code=UNRESOLVED decision=reject-material draw=false
FAIL: R-012 expected code=OK decision=reject-material draw=false; got code=UNRESOLVED decision=reject-material draw=false
exit 1
```

R-012 flips with R-004 because those two vectors share the same expected tuple. The repository fixture
was hashed before and after and did not change; the unmutated run is green (`14/14 passed`).
---

## 11. Tint pin and WGSL validation (T-023, T-030)

### 11.1 The pin

```powershell
powershell -NoProfile -File shaders/tools/get-toolchain.ps1
```

```text
TOOLCHAIN OK: dxc 1.9.2609.5 at shaders/tools/.cache/dxc/bin/x64/dxc.exe
  asset sha256 verified: AD31B1FC8443175D204F77A611FDB3EF2EC42759BDC2F1167368DE24A4A7E7F1
  reported: dxcompiler.dll: 1.9(5512-01b62ad4)(1.9.2609.5) - 1.9.2609.5 (01b62ad47)
TOOLCHAIN OK: tint 106704a9ac3f at shaders/tools/.cache/tint/bin/tint.exe
  asset sha256 verified: 2B082C284F078B9F91A91A6B2CF805FA42C18F60B8DCAAB70476A5E5E24B74EE
  version: no --version in this build; the pin is the asset hash plus the Dawn commit
```

Tint ships inside Dawn's release tarball (~118 MB). `get-toolchain.ps1` verifies the tarball hash, then
extracts exactly one member, `<root>/bin/tint.exe`, and checks that it answers `--help`. This build
rejects `--version`, so the identity is the asset SHA-256 plus the Dawn commit in the asset name; a
substituted binary fails before it can translate anything.

### 11.2 Every stage now has three artefacts

```powershell
powershell -NoProfile -File shaders/tools/compile.ps1 -All
```

```text
  OK    interaction          vs  dxil=0 spv=0 wgsl=0
  OK    interaction          ps  dxil=0 spv=0 wgsl=0
  OK    environment          vs  dxil=0 spv=0 wgsl=0
  OK    environment          ps  dxil=0 spv=0 wgsl=0
  OK    bumpyenvironment     vs  dxil=0 spv=0 wgsl=0
  OK    bumpyenvironment     ps  dxil=0 spv=0 wgsl=0
  OK    shadow               vs  dxil=0 spv=0 wgsl=0
  OK    arbfp_glasswarp      ps  dxil=0 spv=0 wgsl=0
  OK    test                 vs  dxil=0 spv=0 wgsl=0
  OK    test                 ps  dxil=0 spv=0 wgsl=0
  OK    generic_texgen       vs  dxil=0 spv=0 wgsl=0
COMPILE OK: 6 programs, 0 Tier B variants, 11 stage compiles (DXIL + SPIR-V + WGSL), dxc 1.9.2609.5, tint 106704a9
```

`wgsl=0` means two things ran: `tint --format wgsl <stage>.spv -o <stage>.wgsl`, and then
`tint --format wgsl <stage>.wgsl -o <stage>.wgsl.validate`, a re-parse and regeneration of the emitted
module. The second step is the validation: a module that parses cannot carry an unknown identifier, a
type mismatch or a missing binding. The intermediate file is deleted.

The bound proof covers the whole Tier B cross product through the same three tools:

```text
BOUND PROOF OK: 42 variants x 2 stages = 84 compiles (DXIL + SPIR-V + WGSL), 0 failures
```

### 11.3 Goldens now cover WGSL, and the pin is recorded

```powershell
powershell -NoProfile -File shaders/tools/verify-golden.ps1
```

```text
GOLDEN OK: 11 stage hashes (DXIL, SPIR-V, WGSL) deterministic across two runs and equal to the recorded goldens (dxc 1.9.2609.5, tint 106704a9ac3f)
```

`compile-goldens.json` gained a `wgsl` hash per stage and the Tint commit; the source hash is now computed
on canonical bytes (CRLF collapsed to LF), so it agrees with `provenance.json` instead of recording the
checkout convention. Spot-check: `interaction/ps` source `B9FB5BD2…` in both files.

### 11.4 Negative controls

| Mutation | Result |
|---|---|
| feed tint a WGSL file that is not WGSL (`not wgsl`) | exit 1, `error: unexpected token` |
| replace the `interaction/ps` WGSL golden with zeros, run `verify-golden.ps1` | exit 1, `interaction/ps: WGSL hash drift` |
| restore the golden, re-run | exit 0 |

The second control matters: before this change the golden file had no WGSL column, so a translation that
silently produced the wrong module would still have been "green".

### 11.5 What this does not prove

- **No program is `owned`.** `ownership.md` §5 criterion 5 needs a rendered comparison; T-027 is blocked
  on authorized data and a running classic backend. The manifest still says `stub`, `owned=0`.
- **No behavioural evidence.** Nothing here says a program draws correctly; it says the same source
  compiles to three targets and the WGSL parses.
- **Emscripten is pinned but not exercised by this file.** The identity is recorded in `pins.json` and
  checked by `verify-emscripten-pin.ps1` (§13); the wasm build itself is the web target's, so this
  library never invokes `emcc`.
- **SPIR-V constants are not compared.** No SPIR-V disassembler is pinned, so `reflect.ps1` still reads
  the DXIL side only.
---

## 12. Engine wiring (T-021, T-022, T-024, T-029), 2026-10-29

**Delivery.** The engine is a read-only submodule. The call sites are modified engine files exported
as one tracked patch, `targets/win11-amd64/patches/all-changes.patch` (72 modified files, 0 added,
0 deleted), and reconstructed by `scripts/apply-target-patches.ps1`. The WIP commit `d3b3dd4` from the
stalled `feat/29-resolver-wiring` branch was cherry-picked first; the facade and the patch were then
completed in this branch. The old worktree and branch were not edited or reused.

What changed in the engine:

| File | Change |
|---|---|
| `neo/renderer/draw_arb2.cpp` | `R_LoadARBProgram` returns a Code and publishes an id only after a successful compile; `R_FindARBProgram` is replaced by `R_FindProgramSlot` + phase-1 handles; `R_InitShaderResolver`, `R_CheckProgramTableCoverage`, `R_LoadAllProgramBlobs`, `R_LoadNewProgramBlobs`, `shader_status`, `shader_reload`, `shader_selftest`; interaction/test/shadow binds gate on `MayDraw` and use `ProgramId` |
| `neo/renderer/Material.h` | `newShaderStage_t` stores two `shaderProgramHandle_t` values (key, code, target, requested, blocked) instead of raw ints; `ProgramsDrawable()` gates the pair |
| `neo/renderer/Material.cpp` | `program` resolves as an atomic pair; `vertexProgram`/`fragmentProgram`/`megaTexture` resolve through phase 1; a failed request still allocates a blocked stage so the draw cannot silently fall back; diagnostics carry the material name and stage index |
| `neo/renderer/draw_common.cpp` | new-style and texgen stages gate on both phases; glasswarp/environment/bumpyenvironment binds use resolver keys |
| `neo/renderer/RenderSystem_init.cpp` | `r_shaderMode`; manifest loaded in `R_InitShaderResolver` before `R_InitMaterials`; commands registered in `R_InitCommands`; resolver shut down with the renderer |
| `neo/renderer/RenderSystem.cpp` | `R_LoadNewProgramBlobs()` at `BeginFrame` binds phase 2 for materials parsed after the context exists |
| `neo/renderer/tr_local.h` | declarations for the resolver entry points; `R_FindARBProgram` removed |

**Default mode.** `r_shaderMode` defaults to `off` while `owned=0`. The contract's evidence default stays
`strict` (set explicitly with `+set r_shaderMode strict`); `off` is the documented escape hatch and is
never evidence. This is a deliberate deviation from `plan.md` D-011 for this tree: making `strict` the
default now would refuse every program in the shipping manifest, because all six authored programs are
`stub` until T-027 lands. The mode is recorded in every `shader_status` line.

### 12.1 Build

```powershell
cmake --build --preset win11-amd64-release -- -m
```

```text
BUILD_EXIT=0
  doom3_engine.vcxproj -> build/targets/win11-amd64/doom3_engine.dir/Release/doom3_engine.lib
  DOOM3.vcxproj -> build/bin/Release/DOOM3.exe
  DOOM3XP.vcxproj -> build/bin/Release/DOOM3XP.exe
```

The only new compiler diagnostics are the pre-existing C5208 in `neo/sys/win32/win_local.h`
(not caused by this change). A first build failed at `shader_engine.cpp` on unqualified
`Target`/`Decision` names in the self-test; the fix qualified them and the next build exited 0.

### 12.2 In-engine contract test (T-024)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/shader/run-engine-selftest.ps1 `
  -Exe build/bin/Release/DOOM3.exe -DataDir <demo data root> -OutDir build/run-engine-selftest
```

```text
ENGINE SELFTEST OK: 8/8 markers
  shader_status: manifest=14 owned=0 stub=6 unsupported=1 absent=7 requests=18 ok=0
  unresolved=0 stage_missing=0 ambiguous=0 materials_rejected=0 materials_error=0 mode=off
EXIT=0
```

The engine log contains the five behaviours, each run against a local resolver instance so the
shipping resolver is untouched:

```text
SHADER MANIFEST COVERAGE: 18/18 engine table keys covered
SHADER SELFTEST: PASS missing -> UNRESOLVED, material rejected, no draw
SHADER SELFTEST: PASS one-sided -> both stages blocked (STAGE_MISSING/STAGE_MISSING)
SHADER SELFTEST: PASS compile-failed -> phase 2 blocks the draw
SHADER SELFTEST: PASS tolerant -> error material, no draw
SHADER SELFTEST: PASS reload -> before/after report
SHADER SELFTEST: PASS 5/5
```

The one-sided case is the atomic `program` rule: `ResolveProgramPair` returned false and **both** legs
were non-OK, so neither stage was installable. The compile-failed case resolves an `owned` entry, then
reports `COMPILE_FAILED` as the phase-2 blob and `MayDraw` returns false.

### 12.3 Core tests still green

```powershell
cmake -S tests/shader -B build-shader -G "Visual Studio 17 2022" -A x64
cmake --build build-shader --config Release
ctest --test-dir build-shader -C Release
```

```text
100% tests passed, 0 tests failed out of 6
CTEST_EXIT=0
```

One core change accompanied the wiring: `RecordResolution` no longer inserts the empty material name
used by stationary-table requests into `materials_rejected`/`materials_error`. Before the fix, a
strict-mode boot with no material failures reported `materials_rejected=1`; the ctest row that asserts
`materials_rejected` uses a named material and still passes.

### 12.4 Off mode reproduces the legacy render

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-win11-amd64.ps1 `
  -Exe build/bin/Release/DOOM3.exe -DataDir <demo run dir> `
  -OutDir build/run-render-off -Map game/demo_mars_city1 -WaitSec 60
```

```text
CAPTURE title='DOOM 3' 640x480 distinctColours=877 file=build/run-render-off/screenshot.png
RUN map='game/demo_mars_city1' spawned=True alive=True colours=877
EXIT=0
```

With `r_shaderMode off` the engine boots, spawns and renders the demo map. The log shows
`requests=33 unresolved=15` after material parsing: the demo references program names outside the
manifest, which is the data-forced `UNRESOLVED` class, and `off` reproduces the legacy draw anyway.

### 12.5 Strict mode refuses the broken programs

```powershell
DOOM3.exe +set fs_basepath <demo run dir> +set r_shaderMode strict `
  +devmap game/demo_mars_city1 +wait 90 +shader_status +quit
```

```text
EXIT=0
SPAWNED=True
SHADER_ERROR_LINES=10
shader_status: manifest=14 owned=0 stub=6 unsupported=1 absent=7 requests=33 ok=0
  unresolved=15 stage_missing=0 ambiguous=0 materials_rejected=6 materials_error=0 mode=strict
SHADER ERROR: material="textures/glass/glass1" stage=0 keyword=program target=ps
SHADER ERROR: material="textures/glass/glass2" stage=0 keyword=program target=ps
```

Six unique materials were rejected, each record names material, stage, keyword, target, program, key,
code and mode, and the process still spawned and exited cleanly: the failure is loud, not a crash and
not a silent unlit draw. The demo materials use the atomic `program` keyword, so the pair path is
exercised by real data.

### 12.6 Patch delivery is reproducible

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/export-target-patches.ps1 -Name win11-amd64
```

```text
Modified 72, Added 0, Removed 0
EXPORT_EXIT=0
```

A fresh copy was created from the pristine submodule, the exported patch applied to it, and every
modified row's SHA-256 checked:

```text
Applied patch: 72 files verified, 0 mismatches
APPLY_EXIT=0
```

### 12.7 Spec gates re-run on this branch

| Command | Exit | Last line |
|---|---:|---|
| `specs/.../census.ps1 -Verify` | 0 | `CENSUS OK: 18 entries, 13 files, 8 live-default entries, draw_exp compiled=False` |
| `specs/.../resolver-conformance.ps1` | 0 | `RESOLVER VECTORS: 14/14 passed (manifest validated: 12 entries)` |
| `specs/.../verify-docs.ps1` | 0 | `DOCS OK: 125 anchors, 70 content assertions, 40 links` |
| `specs/.../provenance-check.ps1` | 0 | `PROVENANCE OK: 402 tracked files, no shader source, no ARB program bodies` |
| `specs/.../integration-check.ps1` | 0 | `INTEGRATION OK: 18 requests (10 OK, 8 ABSENT), 14 plan keys, 6 authored, 9 provenance ids; megaTexture=UNSUPPORTED` |
| `specs/.../fuzz-resolver.ps1` | 0 | `FUZZ OK: 10162 distinct requests (24000 calls) over 6000 names, 0 failures` |
| `shaders/tools/manifest-check.ps1` | 0 | `MANIFEST OK: keys=14 owned=0 stub=6 unsupported=1 absent=7` |
| `shaders/tools/provenance-record.ps1` | 0 | `PROVENANCE RECORD OK: 9 records, 10 hashes match` |

### 12.8 Shader authoring gates re-run (T-025, T-026, T-030, T-028)

The pinned toolchain was fetched in this worktree and the unchanged programs re-checked:

```powershell
powershell -NoProfile -File shaders/tools/get-toolchain.ps1
powershell -NoProfile -File shaders/tools/compile.ps1 -All
powershell -NoProfile -File shaders/tools/reflect.ps1
powershell -NoProfile -File shaders/tools/verify-golden.ps1
powershell -NoProfile -File shaders/tools/compile.ps1 -ProveBound
```

```text
TOOLCHAIN OK: dxc 1.9.2609.5 ... asset sha256 verified
TOOLCHAIN OK: tint 106704a9ac3f ... asset sha256 verified
COMPILE OK: 6 programs, 0 Tier B variants, 11 stage compiles (DXIL + SPIR-V + WGSL)
REFLECT OK: 11 stages, 5 declared cbuffers, 26 declared resources, 7 declared semantics, 0 violations
GOLDEN OK: 11 stage hashes (DXIL, SPIR-V, WGSL) deterministic ... (dxc 1.9.2609.5, tint 106704a9ac3f)
BOUND PROOF OK: 42 variants x 2 stages = 84 compiles (DXIL + SPIR-V + WGSL), 0 failures
```

Exit codes: all 0. This re-confirms the authoring artifacts; it does **not** make any program `owned`
(ownership criterion 5, the rendered comparison, is T-027).

### 12.9 Not run here, and why

- No rendered parity capture exists; T-027 is still `BLOCKED` on authorized data and a classic backend.
- The GL backend binds retail ARB text from data. The authored HLSL programs are compiled and gated but
  cannot be bound by this backend, so no claim is made that the port's shader code draws.
- Tolerant mode still does not draw the magenta error material (I-5); it logs the `ErrorMaterial`
  decision, counts it and skips the stage. T-021 is therefore `BLOCKED`, and §13.3 shows why the
  remaining half cannot be produced on a host without game data.

---

## 13. Emscripten pin and the data blocker (T-023, T-021/T-025/T-027/T-028/T-030), 2026-11-01

### 13.1 The pin

`shaders/tools/pins.json` gained an `emscripten` block: version `6.0.13`, commit
`e13ee988ce3d87a738dd8f3661d02223308c412a`, the `emdawnwebgpu` port at Dawn `v20260423.175430`. The
identity is not invented here — it is the accepted run recorded in `tests/webgpu/emscripten/README.md`
(2026-10-29), which came from that host's `emcc --version`. What is new is that the identity is now
machine-checked instead of prose.

```powershell
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1
```

```text
EMSCRIPTEN PIN SKIP: no emcc found (checked -EmccPath, PATH, EMSDK, %USERPROFILE%\emsdk)
  pinned identity: emcc 6.0.13 (e13ee988ce3d87a738dd8f3661d02223308c412a)
  SKIP is not a pass: the web target owns the emsdk install; nothing was verified here.
EXIT=2
```

Exit 2 is this host's truth: there is no emsdk on it. The script fails closed — a host whose `emcc`
reports a different version or commit gets exit 1, and only the pinned tool gets exit 0 — so a machine
that builds the wasm leg cannot silently use a different compiler.

### 13.2 Mutation controls

The comparison logic was exercised without installing an emsdk, using three `cmd` shims and three
mutated pin files written under the git-ignored `build/_mut/`. Nothing here is tracked; the shims only
print an `emcc --version` line.

```powershell
# B: pinned tool, unmodified pins
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-right.cmd
# C: tool reports 6.0.12
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-wrong.cmd
# D: tool reports 6.0.13 with a foreign commit
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-wrongcommit.cmd
# E: pinned tool, pin version mutated to 6.0.14
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-right.cmd -PinsPath build/_mut/pins-version.json
# F: pinned tool, pin commit mutated
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-right.cmd -PinsPath build/_mut/pins-commit.json
# G: pin says 6.0.130; the tool's 6.0.13 must not satisfy it
powershell -NoProfile -File shaders/tools/verify-emscripten-pin.ps1 -EmccPath build/_mut/fake-emcc-right.cmd -PinsPath build/_mut/pins-boundary.json
```

| Control | Mutation | Exit | Decisive output |
|---|---|---:|---|
| B | none (pinned tool) | 0 | `EMSCRIPTEN PIN OK: emcc 6.0.13 (e13ee988ce3d)` |
| C | tool version 6.0.12 | 1 | `version matched: False; commit matched: False` |
| D | tool commit foreign | 1 | `version matched: True; commit matched: False` |
| E | pin version 6.0.14 | 1 | `expected version 6.0.14` … `version matched: False` |
| F | pin commit mutated | 1 | `version matched: True; commit matched: False` |
| G | pin version 6.0.130 | 1 | `version matched: False` (the token boundary holds) |
| A | no emcc on this host | 2 | `EMSCRIPTEN PIN SKIP` |

E and F are the controls that matter: they prove the check reads the pin file rather than comparing a
hardcoded string with itself. G proves `6.0.13` cannot be satisfied by a `6.0.130` build. A is the
honest limit — the real `emcc` was never present, so the pin's *identity* rests on the recorded
accepted run, and this file does not claim a wasm build.

### 13.3 Why the remaining implementation half is blocked

Every remaining implementation item needs a frame: the tolerant substitution (T-021), the shadow and
interaction comparisons (T-025, T-030), Tier B shipping variants (T-028) and the parity capture
(T-027). This host has no game data, so no frame is possible. The probe below used an existing Release
build of this target at the same commit (not a build produced by this branch) with `fs_basepath`
pointed at the target copy, which contains only `base/default.cfg`:

```powershell
DOOM3.exe +set logFile 2 +set r_fullscreen 0 `
  +set fs_basepath <target copy> +set fs_savepath <temp> `
  +set r_shaderMode off +shader_selftest +shader_status +quit
```

```text
SHADER MANIFEST COVERAGE: 18/18 engine table keys covered
shader_status: manifest=14 owned=0 stub=6 unsupported=1 absent=7 requests=18 ok=0 unresolved=0
  stage_missing=0 ambiguous=0 materials_rejected=0 materials_error=0 mode=off
Stack trace at fatal error: ... -> idRenderSystemLocal::Init+0x127 (RenderSystem_init.cpp:2168)
EXIT=0xC0000005
```

The resolver itself is fine: coverage and `shader_status` both answer before the abort. The engine dies
later, inside renderer init, because there is no game data to build materials from. A search for
`demo00.pk4` in the repository, the agent temp root, the Kilo worktree pool and the usual
download/install directories found nothing, so this is a missing-input blocker, not a code defect. **Exact input needed:** an official demo
`base/demo00.pk4` (prepared with `scripts/prepare-demo-data.ps1`) or a retail install, plus a
GL-capable desktop that can present. No build was attempted in this worktree: this branch changes no
C++, CMake or target patch file, so there is nothing for a build to prove that §12.1 does not already
cover.

---

## 14. Gate reruns and the stale report (2026-11-01)

### 14.1 Every gate on the staged tree

Each command was run from the repository root and its exit code captured immediately after the
process; the last non-empty output line is recorded below. Nothing in this pass edits the checks
themselves except `report.ps1` (§14.2).

| Command | Exit | Last line |
|---|---:|---|
| `specs/008-classic-shaders/tools/census.ps1 -Verify` | 0 | `CENSUS OK: 18 entries, 13 files, 8 live-default entries, draw_exp compiled=False` |
| `specs/008-classic-shaders/tools/resolver-conformance.ps1` | 0 | `RESOLVER VECTORS: 14/14 passed (manifest validated: 12 entries)` |
| `specs/008-classic-shaders/tools/verify-docs.ps1` | 0 | `DOCS OK: 125 anchors, 70 content assertions, 40 links` |
| `specs/008-classic-shaders/tools/provenance-check.ps1` | 0 | `PROVENANCE OK: 602 tracked files, no shader source, no ARB program bodies` |
| `specs/008-classic-shaders/tools/integration-check.ps1` | 0 | `INTEGRATION OK: 18 requests (10 OK, 8 ABSENT), 14 plan keys, 6 authored, 9 provenance ids; megaTexture=UNSUPPORTED` |
| `specs/008-classic-shaders/tools/fuzz-resolver.ps1` | 0 | `FUZZ OK: 10162 distinct requests (24000 calls) over 6000 names (seed 20261028), 0 failures` |
| `shaders/tools/manifest-check.ps1` | 0 | `MANIFEST OK: keys=14 owned=0 stub=6 unsupported=1 absent=7` |
| `shaders/tools/provenance-record.ps1` | 0 | `PROVENANCE RECORD OK: 9 records, 10 hashes match, every sourced program has exactly one record` |
| `shaders/tools/verify-emscripten-pin.ps1` | 2 | `EMSCRIPTEN PIN SKIP: no emcc found` (§13.1 — a skip, not a pass) |

`verify-docs` still reports 125 anchors / 70 content assertions / 40 links: this pass added prose and
one script, not anchors or relative links. `provenance` moved from 402 to 602 tracked files because the
package, the shader tools and the tests landed after that run; its subject (no shader source, no ARB
bodies) is unchanged.

### 14.2 The committed report contradicted the task list

The `evidence/report.html` committed on 2026-10-29 still said "Engine resolver, ABI header, shaders:
**not started** (T-020..T-030)" and showed `22/32` tasks, while `tasks.md` said the wiring had landed.
That is the same class of defect as gate G-10 in `review.md` §3.1.2: a review artifact that reads as a
current pass (or, here, a current state) while the tree says otherwise.

The gap line is now derived from the task table — `report.ps1` collects every row whose status cell is
`BLOCKED` and prints the ids — so it cannot drift by being typed twice. Regenerated on 2026-11-01:

```text
gates: 6/6 passing
  census       exit=0
  resolver     exit=0
  integration  exit=0
  fuzz         exit=0
  docs         exit=0
  provenance   exit=0
```

Browser assertions on the regenerated page (Playwright, 1440 × 1100, full page):

```json
{ "passBadges": 6, "failBadges": 0, "hasSkipped": true, "hasBlocked": true, "staleClaim": false }
```

with `staleClaim` testing for the literal string "not started", which must not reappear. The KPI strip
now reads `6/6`, `25/32`, `18`, `5`, `70`, and the gaps list reads
`Implementation: 6/11 tasks done. Blocked tasks: T-021, T-025, T-027, T-028, T-030, T-031, T-032`.
`evidence/report.png` was recaptured from the regenerated page (1440 × 2672). What these artifacts
still do **not** prove is any shader behaviour.

Mutation control for the derived line: with `tasks.md` temporarily marking T-020 `BLOCKED` (the change
was not committed), `report.ps1 -Out <temp>` printed
`Blocked tasks: T-020, T-021, T-025, T-027, T-028, T-030, T-031, T-032` — eight ids, so the line
follows the table rather than a typed sentence. The file was restored afterwards and
`git status --porcelain` was empty; the committed report still lists the seven real ids.


---

## 15. Demo-data run: census, tolerant substitution, Tier B (2026-11-01)

The official free DOOM 3 demo (`demo00.pk4`, 12,234 entries, three maps) was staged outside git with
`scripts/prepare-demo-data.ps1`. This unblocked the parts of T-021, T-028 and T-031 that needed
authorised data; it does **not** unblock retail parity (T-025, T-027, T-030).

### 15.1 Commands and exit codes

| # | Command | Exit | Result |
|---|---|---|---|
| 1 | `cmake --preset win11-amd64` | 0 | target copy created and patched |
| 2 | `cmake --build --preset win11-amd64-release -- -m` | 0 | `build/bin/Release/DOOM3.exe`, `DOOM3XP.exe` |
| 3 | `scripts/prepare-demo-data.ps1 -Pk4 <demo00.pk4> -RunDir _data\run` | 0 | `base\demo00.pk4` staged, GUI/script shims written |
| 4 | `specs/008-classic-shaders/tools/runtime-census.ps1 -Pk4 _data\run\base\demo00.pk4` | 0 | 67 `.mtr`, 6,341 materials, 73 program tokens → 106 target requests, 12 Tier B variants |
| 5 | `shaders/tools/get-toolchain.ps1` | 0 | DXC 1.9.2609.5 and Tint `106704a9ac3f`, asset hashes verified |
| 6 | `shaders/tools/compile.ps1 -All` | 0 | `COMPILE OK: 6 programs, 12 Tier B variants, 35 stage compiles, 0 failures` |
| 7 | `run-win11-amd64.ps1 ... -Map game/demo_mars_city1 -ExtraArgs ('strict','+shader_status')` | 0 | spawned, 800×600 capture, 309 distinct colours, 0 magenta pixels |
| 8 | same, `game/demo_mars_city2` | 0 | 38 requests / 20 unresolved / 9 rejected |
| 9 | same, `game/demo_mc_underground` | 0 | 31 requests / 13 unresolved / 6 rejected |
| 10 | same map 1, `r_shaderMode tolerant` | 0 | 6 `materials_error`, 23.76% magenta pixels |
| 11 | same map 1, `r_shaderMode off + r_showShadows 1` | 0 | control frame; legacy ARB text, not evidence |
| 12 | `scripts/export-target-patches.ps1 -Name win11-amd64` | 0 | M-only patch regenerated |
| 13 | `scripts/apply-target-patches.ps1 -Name win11-amd64` on a fresh copy | 0 | 0 mismatches |

Capture size note: at 125% display scaling the engine's client size is 800×600 for a requested
640×480, and `smoke-win11-amd64.ps1` exits 8 on that size check even when the menu renders. These runs
therefore assert on the `qconsole.log` status/diagnostics and on the captured frame, not on a size
match; no size mismatch is reported as a render failure.

### 15.2 Code change

| File | Change |
|---|---|
| `neo/renderer/Image.h`, `neo/renderer/Image_init.cpp` | add the generated `_shaderError` magenta/black checkerboard image |
| `neo/renderer/tr_local.h`, `neo/renderer/draw_common.cpp` | `RB_DrawShaderErrorMaterial`; substitute it for a non-OK stage in `RB_STD_T_RenderShaderPasses` |
| `neo/renderer/draw_arb2.cpp` | substitute it for a non-OK interaction pass; `off` never reaches the branch |
| `src/renderer/shader/engine/shader_engine.{h,cpp}` | `shaderport::Tolerant()` (mode query used by the draw gates) |

The engine changes live in `targets/win11-amd64/patches/all-changes.patch`; the pristine submodule is
untouched. No program was promoted to `owned`.

### 15.3 Evidence

- `evidence/runtime-census.md` — full census, skip semantics, variant set, NOT OBSERVED list.
- `evidence/runtime-census-demo.json` — machine-readable names/counts.
- `evidence/tolerant-error-material-demo-city1.png` — tolerant frame, checkerboard visible.
- `evidence/strict-refused-demo-city1.png` — strict frame, no magenta (`materials_rejected=6`).
- `evidence/demo-off-city1-control.png` — control frame with legacy programs bound.
- Pixel test (R > 100, B > 100, G < min(R,B)/2): tolerant 114,031/480,000 = 23.76% magenta;
  strict 0; off 0. Frames recaptured at base `04c6180`.

Limits: strict/tolerant frames are mostly unlit or error material because every manifest program is
still `stub`; the nine-authoring-set parity comparison and all `owned` promotion remain blocked on
retail data.
