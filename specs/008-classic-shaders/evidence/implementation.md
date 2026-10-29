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
the pin, so a substituted compiler fails closed with exit 3. `pins.json` also records the two tools
that are **not** pinned — Tint and a SPIR-V disassembler — with the reason, rather than implying that
WGSL validation happens.

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

Limit, stated rather than hidden: this is the DXIL side only. Cross-backend constant offsets cannot be
compared until a SPIR-V disassembler is pinned, and WGSL cannot be produced without Tint.

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

`tests/shader` tests the **core**, not the renderer. `R_LoadARBProgram`, `R_FindARBProgram`,
`Material.cpp`'s parse and the console-command registration are engine files; `plan.md` §5 keeps them
out of this workstream. That is why T-021/T-022/T-024/T-029 are `PARTIAL` in `tasks.md` even though
their logic exists and is tested.

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
| Engine wiring of the resolver (draw gate, material parse, init order) | not done | engine files; `plan.md` §5 assigns them to a separate renderer issue |
| `shader_status` / `shader_reload` as console commands | core only | registration is an engine file; the counters and status line exist and are tested |
| WGSL validation | not done | no Tint binary; recorded in `pins.json` as a gap |
| `owned` status for any program | none | needs WGSL validation and parity; `stub` is the honest status |
| Parity capture (T-027) | blocked | needs authorized retail data and a classic backend |
| Runtime census (T-031) | blocked | no `*.pk4`; data is EULA-bound |
| Tier B shipping variants | none | populated only from the census, which is blocked |
| Shadow / glasswarp / interaction behaviour in a running engine | unverified | no engine wiring and no data; this file makes no such claim |

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
