# Q4 hybrid-quality evidence (T049–T055, T056, T057, T086)

Status: author evidence for issue [#65](https://github.com/yhyu13/DOOM3_AINative/issues/65), written
2026-10-30. This is **not** Q4 acceptance, not an engine frame and not an independent review. It records
what the portable slices do on this host and what could not run.

**Headline: T049–T055 and the T086 off-path branch are implemented as CPU-testable, backend-neutral
slices and their tests are green in Debug and Release (32/32 CTest, 0 failures). No device frame, image,
capture, frame-time or VRAM measurement exists. T056 is BLOCKED on missing authored content and on the
absence of a Q4 renderer frame; T057 is NOT RUN for the same reason; the T086 provider branch is NOT
RUN because no authorized provider exists. Numeric budgets are unset by design.**

## Identity and scope

- **Stage and claim**: Q4 portable slices. Claim: the material route, linear-HDR opaque inputs, direct
  light/shadow arithmetic, indirect/reflection/atmosphere arithmetic, residency policy and
  non-proprietary reconstruction/off ordering behave as the tests specify. **No** frame, art, parity,
  capture, budget, engine-integration or provider claim.
- **Issue and branch**: #65, branch `feat/65-q4-hybrid-quality`, based on `main` at `dae9431`.
- **Submodule**: `doom3_orig/doom3` @ `a9c49da5afb18201d31e3f0a429a037e56ce2b9a`,
  `git -C doom3_orig/doom3 status --porcelain` empty. No submodule or fork change in this PR.
- **Data identity and rights**: synthetic CPU fixtures only. No `.pk4`/`.pk3`/`.pak` is present and no
  game data is committed. The only authored content cited is `content/hero/dielectric/` (CC0-1.0,
  `retail_text_consulted: no`), and it is not read by any test.
- **Exclusions and limitations**: the engine fork (renderer dispatch into the hybrid passes), the target
  build (`targets/win11-amd64/CMakeLists.txt` is untouched), all image comparison, all captures,
  provider work, and the Q6 path tracer.

## Environment and configuration

| Item | Value |
|---|---|
| OS / arch | Windows 10.0.22631, x64 |
| Compiler | MSVC 19.44.35228.0 (VS 2022 Professional, toolset v143), Windows SDK 10.0.26100.0 |
| Build tool | CMake 3.31.6 (VS-bundled), generator `Visual Studio 17 2022 -A x64` |
| Build root | `build-q4/` (git-ignored) |
| Adapter and driver | N/A — no device, window, shader or GPU call |
| Shader provenance | N/A — no shader is loaded or compiled |
| Provider | None. No proprietary SDK is present, linked or downloaded; the provider branch is NOT RUN |

## What was implemented

| Task | Files | Behaviour |
|---|---|---|
| T049 | `specs/004-quality-renderer/quality-rubric.md` | Pre-review rubric (materials, lighting, darkness/readability, identity, motion, HUD), hero provenance/permissions, four scene categories, measurement plan. No numeric budget. |
| T050 | `tests/renderer/material_contract_test.cpp` | Independent sRGB decode table, channel order, route fixtures (legacy specular, missing roughness, undeclared normal/metalness, transparent/special, missing base), opaque input refusals. |
| T051 | `src/renderer/rhi/{linear_color,materials,hybrid_opaque}.*`, `src/renderer/d3d12/hybrid_opaque.cpp` | Linear-HDR opaque inputs; sRGB decoded once; HDR emissive preserved; forward/compatibility route retained for special/transparent and undeclared-PBR materials. |
| T052 | `src/renderer/rhi/lighting.h`, `src/renderer/d3d12/lighting.cpp`, `tests/renderer/lighting_test.cpp` | Windowed inverse-square attenuation, `N·L`, bias + PCF shadow factor, metal dielectric split, alpha-test discard, `Unsupported` for every legacy/forward stage. |
| T053 | `src/renderer/rhi/indirect_lighting.h`, `src/renderer/d3d12/indirect_lighting.cpp`, `tests/renderer/indirect_atmosphere_test.cpp` | L1 irradiance with negative clamp, environment reflection with below-horizon fallback and roughness blur, exponential height atmosphere preserving HDR fog colour. |
| T054 | `src/renderer/rhi/residency.*`, `tests/renderer/residency_test.cpp` | Mip/quality selection against remaining budget, per-frame upload throttle, completion-epoch release (deferred while in flight), no eviction to satisfy an upload. |
| T055 | `src/renderer/rhi/reconstruction.*`, `tests/renderer/reconstruction_test.cpp` | Identity off path and bilinear spatial upscale; identity is byte-exact; provider never required. |
| T086 | `tests/renderer/reconstruction_test.cpp` | Real `PlanPostReconstruction` ordering around the reconstruction plan: scene → world GUI → display overlay, overlay excluded from reconstruction, one-to-one HUD texels, refused render-extent HUD, identical order on the off path. |
| T056 | — | **BLOCKED** (see §5). |
| T057 | — | **NOT RUN** (see §5). |

Design boundaries worth stating:

- The `d3d12/` path holds arithmetic only. No `ID3D12*`, device, descriptor or queue operation was added;
  the Q4 tests link no native library.
- `materials.h` keeps the Q2 classic plan intact. `PlanModernMaterial` is additive and never promotes a
  material whose PBR channels are not declared.
- The light model is a bounded preview term, documented as such. It is not a claim of a full BRDF; Q6 is
  where a physically based reference is judged.

## Procedure and retained artifacts

All commands ran from the worktree root. Full logs are retained in the git-ignored `build-q4/`.

```text
cmake -S tests/renderer -B build-q4 -G "Visual Studio 17 2022" -A x64   -> 0
cmake --build build-q4 --config Release -- -m                           -> 0 (no error/warning lines)
cmake --build build-q4 --config Debug   -- -m                           -> 0 (no error/warning lines)
ctest --test-dir build-q4 -C Release --no-tests=error --output-on-failure -> 0, 32/32
ctest --test-dir build-q4 -C Debug   --no-tests=error --output-on-failure -> 0, 32/32
q4_material_contract_test.exe    -> 0, 41 checks / 0 failures
q4_lighting_test.exe             -> 0, 54 checks / 0 failures
q4_indirect_atmosphere_test.exe  -> 0, 32 checks / 0 failures
q4_residency_test.exe            -> 0, 37 checks / 0 failures
q4_reconstruction_test.exe       -> 0, 35 checks / 0 failures
py -3 -I scripts/validate-rhi-contract.py -> 0
```

`validate-rhi-contract.py` reports `header scan: RUN over 19 header(s) and 29 public test file(s)`; the
new headers and public tests contain no private backend vocabulary. The Q4 additions are five CTest
entries labelled `q4`.

**Defect found and fixed during verification.** The first Debug run of `q4_residency_test` hung after
the in-flight release check. The cause was a use-after-erase in `ResidencyPolicy::Tick`: the freed byte
count was read from a `std::map` iterator after `erase`. The bytes are now captured before the erase.
Release had passed by chance; Debug exposed it. The evidence above is from the rebuilt tree.

**Logs** (git-ignored): `build-q4-configure.log`, `build-q4-build-Release.log`,
`build-q4-build-Debug.log`, `build-q4/ctest-Release-final.log`, `build-q4/ctest-Debug-final.log`,
`build-q4/q4_residency_debug.log`.

## 4. T086 result

| Check | Result | Evidence |
|---|---|---|
| overlay/reconstruction ordering with the independent off path | **EXECUTED** | `q4_reconstruction_test.exe` 35/35; order scene → world GUI → display overlay; 2 reconstruction inputs; HUD one-to-one at display extent; render-extent HUD refused |
| provider-specific display-stage position | **NOT RUN** | no authorized provider (T060 not met); T062 has not executed; no API signature was invented |

The provider branch is not a pass and does not block the off-path check.

## 5. T056 and T057

| Task | State | Exact reason |
|---|---|---|
| T056 | **BLOCKED** | Two independent facts. (1) Authored content covers 1 of 7 matrix cells: only `opaque_dielectric` has a source, provenance, sidecar and package; `opaque_metal`, `alpha_tested`, `emissive`, `skinned_character`, `transmissive_glass` and `large_environment` are `NOT STARTED` in `specs/009-rhi-hd-content/tasks.md`. (2) No Q4 renderer frame can be produced: the hybrid passes are portable arithmetic with no engine call site, and `content/tools/capture-q4.ps1` still exits 3 with `BLOCKED: spec 004 has not reached Q4`. Captures, frame times, VRAM and accepted/rejected art examples therefore do not exist and were not fabricated. All seven budget rows remain `blocked` in `content/manifest/budget-rows.json`; numeric budgets are unset. |
| T057 | **NOT RUN** | The independent art review needs T056's captures as input, and none exist. No reviewer has recorded a verdict on `quality-rubric.md`. This file is authored by the implementer and cannot substitute for that review. |

## 6. What is not claimed

- Q4 is **not complete**. T056 and T057 are open; the provider branch of T086 is NOT RUN.
- No modern-look acceptance, no classic parity, no DXR, no DLSS, no frame time and no VRAM figure.
- No capture from spec 006 is cited or reused; no `-HdTextures` switch is involved.
- The portable slices do not prove the engine/fork wiring. That needs its own fork PR, submodule bump
  and independent review.
