# Q6 independent review — T070 (path-traced reference/showcase against T063)

Status: Q6 is NOT ACCEPTED. This file is the independent T070 review artifact. It records that
T069 can be ticked as an *evidence record* with the limits in §7, and that ticking T069 does not accept
Q6, a playable showcase or the art direction. The synthetic DXR captures are real, reproducible and
internally consistent; they are not PT-S1..S5, not a Q6 engine frame and not a showcase.

## 1. Reviewer identity and reviewed revisions

- Reviewer: an AI subagent acting as the independent reviewer. I did not author Q6, T069, the CPU
  slice, the capture tool, the probe, the denoiser repair or the earlier PR #118 capture-scope
  approval. I did not share the authoring session or worktree.
- Review worktree: `.kilo/worktrees/review-q6-t070-e21fd798a35fc0f4`, branch `review/q6-t070`, based
  on `origin/main` at `2d11cd22ac41dcbb7d6b9d5eef475177c03ae100`.
- Reviewed Q6 merges: `1aa5324` (CPU slice, PR #73), `1fafb81` (probe, PR #102), `bf28295` (probe
  pass contract, PR #108), `4fd59d9` (denoise repair, PR #110), `c63f57a` (synthetic DXR captures,
  PR #118, reviewed head `8b19b5b6a3c0e54080e64e0ff0375937b818f85f`).
- Inputs read: `path-tracing-rubric.md` (T063), `evidence-q6.md`, `evidence/t069/*` including
  `report.json` and `determinism.txt`, `tasks.md` T063–T070, issue #68, `quality-rubric.md` §4 (the
  Q4 art-direction dimensions this review maps to), `ledger-q6-q7-reconcile.md`, and the merged
  sources under `src/renderer/d3d12/`, `tests/dxr/`, `tests/dxr-probe/` and `tests/dxr-capture/`.
- This review writes one new file. It changes no code, image, target CMake, submodule, game data,
  rubric threshold or evidence record. It does not edit `tasks.md`; the ledger action is in §7.

## 2. Commands rerun

All commands ran from this worktree. Build directories are git-ignored (`build-dxr/`,
`build-dxr-capture/`); full logs are retained there.

| Command | Exit | Observed |
|---|---:|---|
| `cmake -S tests/dxr -B build-dxr -G "Visual Studio 17 2022" -A x64` | 0 | MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 |
| `cmake --build build-dxr --config Release -- -m` | 0 | five Q6 executables built |
| `cmake --build build-dxr --config Debug -- -m` | 0 | five Q6 executables built |
| `ctest --test-dir build-dxr -C Release --no-tests=error --output-on-failure` | 0 | 5/5 tests passed |
| `ctest --test-dir build-dxr -C Debug --no-tests=error --output-on-failure` | 0 | 5/5 tests passed |
| each `build-dxr/Release/q6_*.exe` | 0 | 37 + 68 + 38 + 45 + 61 = 249 checks, 0 failures |
| each `build-dxr/Debug/q6_*.exe` | 0 | the same 249 checks, 0 failures |
| `cmake -S tests/dxr-capture ... -DDXC_EXE=<SDK dxc>` + Release build | 0 | capture tool and `pathtrace_capture.cso` built |
| `q6_dxr_capture.exe --out build-dxr-capture/_review-capture --width 320 --height 180` | 0 | all declared checks accepted, `RESULT=PASS` |
| SHA-256 of the 13 rerun PNGs against the 13 committed PNGs | — | 13/13 identical, 0 mismatches |
| `uv run --python 3.12 python scripts/validate-rhi-contract.py` | 0 | 25 headers and 30 test files scanned, zero prohibited hits |
| `powershell -NoProfile -ExecutionPolicy Bypass -File content/tools/matrix-report.ps1` | 0 | 7/7 cells authored, 0/7 captured, 7/7 budget rows |
| `git -C doom3_orig/doom3 status --porcelain` | 0 | empty |
| bounded `*.pk4` / `*.pk3` / `*.pak` scan | — | 0 files |

Two host facts differ from the earlier evidence and are recorded, not smoothed over:

- The RHI contract gate now exits 0. `evidence-q6.md` §4.4 recorded exit 1 at `b244458` from
  pre-existing `src/renderer/q4_harness` headers. That failure has since been repaired; the gate
  result in this review is a pass. The evidence sentence is stale.
- The capture timing p95 did not reproduce exactly: the committed report records median 5.3880 ms and
  p95 6.6128 ms; this rerun records median 5.3862 ms and p95 5.4345 ms at 640x360, 1 spp/pass,
  32 passes. Both are one 32-pass sample; the median agrees to 0.0018 ms and the p95 spread is host
  state. It is correctly not a gate.

## 3. What the merged code actually does (verified)

- `raytracing_capability.cpp` is a pure selection function. `Hybrid` is always available; `Reference`
  requires a reported device support, a non-unsupported tier and shader model 6.3 or newer;
  `Showcase` additionally requires the visual, performance and motion gates. A failed requirement
  returns `Unsupported` and keeps `effective = Hybrid` only as the available alternative. A silent
  switch is impossible without `allowFallback = true`, and even then the limitation list is retained.
  This is the T064 behaviour and the 37 capability checks exercise both the refusal and the explicit
  approval.
- `pathtrace.cpp` is an iterative integrator with camera rays, deterministic per-pixel/per-sample
  seeds, next-event estimation against a point light, a two-hemisphere environment and alpha-aware
  shadow rays. The static fixture checks its expectation and variance analytically.
- `raytracing_scene.cpp` uses a median-split triangle hierarchy, bottom-up refit for posed or skinned
  vertices, an affine instance placement for rigid motion without a rebuild, a rebuild on topology
  change and a refusal on an invalid pose. The dynamic fixture checks that no stale hit remains and
  every triangle is reachable.
- `pathtrace_material.cpp` implements cosine-weighted Lambertian sampling, a bounded two-lobe
  conductor, Schlick/Snell dielectric transmission with a tint, emission and a deterministic checker
  alpha mask. It is a documented bounded model, not a physically exact BRDF claim.
- `pathtrace_denoise.cpp` accumulates reference samples, rejects history on any non-`Valid`
  `MotionValidity` or on generation/topology/reset/resize/provider-switch/new-pixel flags, and applies
  an edge-aware bilateral filter with an optional depth guide. The MSVC 19.38 codegen defect recorded
  at `1fafb81` is repaired by the `double sumRgb[3]` channel-indexed accumulation; this review's
  61-check denoise run passes in both configurations.
- `tests/dxr-capture` is a standalone D3D12/DXR tool that mirrors the CPU transport model, builds one
  BLAS per object with `ALLOW_UPDATE`, refits skinned frames with `PERFORM_UPDATE`, moves the rigid
  instance in the TLAS descriptor, and renders synthetic scenes with a real `DispatchRays`. It is not
  the engine and it loads no `content/` cell.

The code-level slice is coherent and the tests are discriminating where they claim to be. That does
not make it a renderer: no engine call site, no window, no input path and no Q6 frame exist.

## 4. T063 rubric rows

Verdict vocabulary below follows T063 §6. `PARTIAL` is a review summary for a row whose measurable
part passes but whose declared scope is not met.

| T063 row | Predeclared threshold | Observed on the reviewed captures | Review verdict |
|---|---|---|---|
| §1 configuration identity | every field named; an unnameable field is `BLOCKED` | `report.json` names adapter/LUID/vendor/device/driver/tier/SM/extent/spp/hashes. Camera pose and FOV, maximum path depth, per-fixture light radiance and radius, per-fixture environment radiance, seed/jitter sequence and denoise parameters are in `capture_scene.h`/`README.md` but not in the per-capture record | PARTIAL: not a §1-conformant capture record |
| §2 PT-S1 `static-convergence` | named content identity + Q6 frame | synthetic dielectric/metal room, no `content/hero` cell, no engine frame | NOT RUN |
| §2 PT-S2 `alpha-emissive` | authored cutout + HDR sign | synthetic checker cutout + synthetic 4.0/3.2/2.2 emissive; not authored content | NOT RUN |
| §2 PT-S3 `motion` | rigid + skinned keys, cut/teleport | synthetic rigid translation and two-bone sheet; no cut/teleport frame | NOT RUN |
| §2 PT-S4 `transmission` | authored glass cell, then camera move | synthetic single-plane clear/tinted dielectric, still camera, constant backdrop | NOT RUN |
| §2 PT-S5 `large-environment` | large environment + residency surface, dolly | no fixture, no frame | NOT RUN |
| §3 `mae` at 1024 vs 4096 spp | ≤ 0.01 | 0.001309 static, 0.000102 alpha-emissive, 0.000625 transmission (linear radiance, all pixels) | numeric pass; reference is the same estimator, so PARTIAL |
| §3 `p99-relative` | ≤ 5% on flat patches | 2.87% static back wall, 4.10% alpha sign, 1.11% transmission backdrop | PARTIAL: only the backdrop patch is flat; the back wall carries a point-light gradient and the sign patch includes cutout edges |
| §3 `nan-pixels` | 0 | 0 NaN/inf pixels in all 13 captures | PASS |
| §3 `sample-scaling` | 1.5..2.5 | 1.5271 static, 1.5895 alpha-emissive, 1.5286 transmission | numeric pass; the rubric's "halves the error" rationale is wrong (see M-3) |
| §4 `flat-variance` | denoised ≤ 25% of raw | 0.002203 → 0.000178, ratio 0.0810 on the uniform calibration wall | PASS |
| §4 `edge-bleed` | ≤ 1% of step height | no fixture declares a step edge | NOT MEASURED |
| §4 `history-ghost` / `history-reset` | 0 pixels beyond one frame; reset replaces | capture tool has no temporal history, cut, teleport or reprojection path | NOT RUN at image level; the CPU `DecideHistory` and accumulator checks pass (61 checks) |
| §5 `frame-median` / `p95` | p95 ≤ 1.5 × median | 5.3880 / 6.6128 ms, ratio 1.23, at 640x360, 1 spp/pass, 32 passes | RECORDED, NOT A GATE; not a converged frame and not a hybrid comparison |
| §5 `input-latency` | showcase ≤ 2 × hybrid | no window or input path | NOT RUN |
| §5 `motion-gate` | PT-S3 §4 rows | no PT-S3 and no temporal path | NOT RUN |
| §5 `fallback` | always true; no automatic switch | 37 capability checks pass; no engine/device selection was run | PASS at CPU unit level; NOT RUN at engine/device level |
| §6 showcase | PT-S3 plus §5 | static/fixture images only | NOT RUN; §6.3 forbids a static image as a showcase |

The three `mae` values are below the predeclared 0.01 threshold, the 0 NaN result is clean, and the
flat-field denoise ratio is well inside 0.25. Those are real measurements on a real DXR device. The
row-level problem is scope: the 4096-spp reference is a higher-sample accumulation of the same
estimator (`evidence-q6.md` §4.5.2 says so), so it proves Monte Carlo self-consistency, not the
rubric's "independently produced" reference. Independence is supplied only by the calibration wall's
analytic mean and variance. There is no analytic expectation for the static room, the alpha scene or
the transmission scene, so a systematic bias in those transport paths would not be detected by the
`mae`/`p99` comparison.

The alpha and transmission fixtures do carry analytic transport checks: 1671 HDR sign pixels through
the holes versus 0 with the alpha test disabled; tinted G/R 0.5513 and B/R 0.3024 against the declared
0.55 and 0.30; clear G/R 1.0003. Those check the material model and the alpha mask, not the whole
estimator's bias, so they do not satisfy §3's independent-reference row.

## 5. Art direction

The Q4 art-direction dimensions in `quality-rubric.md` §4 cannot be judged from these fixtures:

| Dimension | Why it is not reviewable here |
|---|---|
| 4.1 Materials | synthetic Lambertian/conductor/dielectric only; no authored channel, sidecar or package identity |
| 4.2 Lighting and shadows | one point light and a simple room; no authored light rig, bias fixture or compatibility-stage case |
| 4.3 Indirect light, reflections, atmosphere | two-hemisphere environment; no fog, atmosphere or named environment map |
| 4.4 Darkness and readability | the images are readable, but there is no stated exposure/tonemap comparison and no numeric contrast metric |
| 4.5 Identity | no `content/hero` cell appears in any capture; identity is explicitly disclaimed |
| 4.6 Motion | rigid and skinned images exist, but there is no history/reprojection/ghost image and no cut |
| 4.7 HUD / overlay | no HUD and no display-overlay path |

The display transform is also different from Q4: `tests/dxr-capture/README.md` records linear →
Reinhard `x/(1+x)` → sRGB, while the Q4 measurement used ACES at exposure 1.0. A Q6 art-direction
verdict would need the authored cells and the project display path, not only a transport fixture.

I viewed the committed images. They are genuine: the static room shows a tan conductor and a faceted
clear dielectric sphere with shadows; the alpha/emissive frame shows the checker cutout with HDR sign
blocks behind it; the transmission frame shows distinct tinted and clear panels; the rigid frames
move the red box across the room; the skinned frames deform the hanging sheet. The faceting is the
documented 24x12 sphere approximation, not a smoothing defect. The transmission backdrop is a
constant 0.8 emissive plane, so the capture can prove the tint ratio and Fresnel expectation but
cannot show refraction of structured background content. These are limits of the fixture, stated in
the tool README, not hidden failures.

## 6. Findings

### BLOCKING for Q6 acceptance

- B-1. No Q6 engine frame and no PT-S1..S5 capture. All five identities are `NOT RUN`; PT-S5 has no
  fixture at all. T063 §2 marks all five `BLOCKED`, and §6.3 says a static converged image alone is
  not a playable showcase. The synthetic fixtures declare a different identity and cannot be
  substituted.
- B-2. No playable showcase evidence. Input latency is `NOT RUN`; the 640x360, 1 spp/pass timing is
  an offline reference-renderer sample, not a converged frame, not a hybrid comparison and not a
  p50/p95 gate. The motion gate, the frame-pacing gate and the showcase selection are all `NOT RUN`.
- B-3. Scene-level convergence is self-referential. The 4096-spp reference is the same estimator; the
  analytic checks cover only the calibration wall. The rubric's §3 independent-reference condition is
  therefore not met for the static room, alpha scene or transmission scene.
- B-4. The art direction is not reviewable. No authored content, no HUD, no ACES display path and no
  camera path are present, so the Q4 rubric §4 dimensions cannot be judged and no Q6 look is claimed.

### MAJOR

- M-1. The `p99-relative` gate is applied outside its declared domain. T063 §3 says "≤ 5% on flat
  patches"; the static patch is a back wall with a point-light shading gradient, and the alpha patch
  includes the emissive sign and its cutout edges. The numeric values pass, but two of the three
  patches are not flat, so the row is not a §3-conformant measurement.
- M-2. The per-capture configuration identity is incomplete against T063 §1. `report.json` records
  the adapter and sampling, but not camera pose/FOV, maximum path depth, per-fixture light radiance or
  radius, per-fixture environment radiance, the seed/jitter sequence, or the denoise parameters. Those
  values exist in `capture_scene.h`, `pathtrace_capture.hlsl` and the README, but a reader of the
  artifact alone cannot reconstruct the capture.
- M-3. The T063 `sample-scaling` rationale is mathematically wrong. A Monte Carlo estimator reduces
  error as `1/sqrt(N)`, so 2x samples give about 1.41x error reduction, not 2x. The predeclared band
  of 1.5..2.5 barely admits the observed 1.53–1.59. The threshold was predeclared and is not changed
  here; the rationale sentence should be corrected in a future T063 revision, not after a rejection.
- M-4. The artifact status lines are stale. `evidence-q6.md`'s headline still says there is no
  capture, image or frame time, while §4.5 records all three; `path-tracing-rubric.md` line 7 says no
  Q6 capture exists while its §7 records the fixtures. `ledger-q6-q7-reconcile.md` already records
  both staleness defects, but they remain in the cited files.

### MINOR

- m-1. The timing p95 is a single 32-pass sample and did not reproduce exactly (6.6128 ms recorded
  vs 5.4345 ms here). The evidence correctly treats timing as recorded, not a gate.
- m-2. The alpha `p99` at 4.10% is close to the 5% threshold and is measured on the edge-rich sign
  patch. It should be read as marginal, not a comfortable pass.
- m-3. The gated flat-field denoise check uses a uniform wall with a constant depth guide, so it
  exercises the depth-guided code path but not a depth discontinuity. The static image's depth-guided
  denoise (ratio 0.3547) is reported but not gated because its patch carries a shading gradient; the
  depth-edge behaviour is covered by the 61 CPU checks, not by a gated image.
- m-4. The evidence §4.4 RHI gate statement is stale: the gate now exits 0 over 25 headers and
  30 test files. This does not change the Q6 result.
- m-5. No cross-vendor or cross-driver claim is made, and none can be made from this host: the
  captures are on `Intel(R) Graphics`, tier 1.1, SM 6.6, UMD 32.0.101.8331, not the RX 9070 XT
  named in the probe record.

## 7. T069 decision

T069's row says: "Record static convergence, dynamic/skinned captures, material limitations, frame
pacing and input latency in `evidence-q6.md`; unavailable RT hardware is SKIPPED and preserves the
fallback." Under that wording the deliverable is the record, not a pass, and T063 §6.1 explicitly
admits `NOT RUN` rows with an exact reason.

Decision: T069 can be ticked as an evidence record with the limits below. The record contains:

- static convergence on the calibration and static fixtures (analytic mean/variance plus 1024 vs
  4096 spp metrics);
- rigid and skinned captures with frame numbers, a BLAS refit and a TLAS translation;
- alpha, HDR-emissive and transmission captures with a negative alpha control and analytic tint
  ratios;
- frame pacing, recorded as a reference-only 640x360, 1 spp/pass sample and explicitly not a gate;
- input latency, recorded as `NOT RUN` with the exact reason "no window or input path";
- the fallback selection at CPU unit level, with 37 checks and no silent switch;
- unavailability handling in the capture tool (exit 2 when DXR is not supported) and the documented
  limit that no no-DXR device run occurred on this host.

Ticking T069 is task completion for the record, not Q6 acceptance. The limits that must accompany the
tick are: PT-S1..S5 `NOT RUN` (0/5), synthetic fixtures only; no Q6 engine frame; input latency
`NOT RUN`; frame pacing not a gate and no hybrid comparison; `edge-bleed` and image-level
`history-ghost`/`history-reset` `NOT RUN`; scene-level convergence reference is the same estimator;
the capture configuration is incomplete against T063 §1; no art-direction verdict; fallback is
CPU-unit only. If the owner intends T069 to require executed PT-S1..S5 captures and a measured input
latency, the row must be reworded first; under its current wording the record is complete.

Ledger-vocabulary note: `ledger-q6-q7-reconcile.md` defines `DONE` as every deliverable and stated
condition evidenced, and `PARTIAL` as at least one absent. Under that definition T069 remains
`PARTIAL`, because input latency is not a measurement and PT-S1..S5 are absent. This review's tick
recommendation rests on T069's literal wording ("Record ... in `evidence-q6.md`") plus T063 §6.1's
`NOT RUN` row. The coordinator should either adopt that reading or keep T069 `PARTIAL`; neither choice
accepts Q6, and the evidence record is complete and accurate either way.

Suggested `tasks.md` annotation for T069:

> T069 DONE (evidence record, 2026-11-02) after independent review: synthetic DXR fixtures for
> static, rigid, skinned, alpha/emissive and transmission cases, 13/13 PNG hashes reproduced, 13/13
> declared checks accepted, flat-field denoise 0.0810. Limits: PT-S1..S5 NOT RUN, no Q6 engine frame,
> input latency NOT RUN, frame pacing not a gate, edge-bleed and image-level history NOT RUN,
> convergence reference is the same estimator, capture configuration incomplete against T063 §1, no
> art-direction verdict. Q6 is not accepted. See `review-q6.md`.

T070 is now reviewable: this file is the independent verdict and it rejects Q6 acceptance. Whether to
tick T070 as review-complete or keep it blocked until a Q6 engine frame exists is a ledger decision;
either way the tick, if made, means the review was performed, not that Q6 was accepted. Suggested
annotation if the coordinator ticks it:

> T070 DONE (independent review, 2026-11-02): `review-q6.md` records Q6 NOT ACCEPTED. T069 is
> ticked only as an evidence record with the limits quoted there. PT-S1..S5, the Q6 engine frame, the
> playable showcase, input latency, edge-bleed and image-level history remain NOT RUN. Q6 acceptance
> stays blocked; no issue is closed.

## 8. Blockers that remain for Q6 acceptance

| # | Blocker | Affected rows | Unblock condition |
|---|---|---|---|
| Q6-B1 | No Q6 engine frame and no PT-S1..S5 capture; the capture tool is standalone and synthetic | §2 all five identities; §3 scene-level independence; §5 showcase | Engine/fork wiring for a Q6 frame, then a named-content capture at a named adapter |
| Q6-B2 | No playable showcase | §5 input latency, frame pacing vs hybrid, motion gate, showcase selection | A window/input path, hybrid comparison, converged frame pacing and PT-S3 motion rows |
| Q6-B3 | Scene-level convergence has no independent reference | §3 `mae`, `p99`, `sample-scaling` | An independent higher-sample renderer or analytic transport checks for the static, alpha and transmission scenes |
| Q6-B4 | No art-direction review | Q4 §4 dimensions applied to Q6 | Authored PT-S1..S5 content under the project display path (ACES, HUD), then an art review |
| Q6-B5 | Capture configuration is incomplete | §1 identity | Per-capture camera/FOV, max depth, light/environment radiance, seed/jitter and denoise parameters in the artifact |
| Q6-B6 | Edge-bleed and image-level history are unmeasured | §4 | A declared step-edge fixture and a temporal/cut/teleport capture |

## 9. What this review does not establish

- It does not accept Q6, PT-S1..S5, a playable showcase, an art-direction result or a path-traced
  engine integration.
- It does not claim an AMD/RX 9070 XT result; the captures are Intel Graphics on this host, and no
  cross-vendor or cross-driver byte identity is claimed.
- It does not claim the 4096-spp reference is independent at scene level; it is not.
- It does not claim input latency, VRAM, debug-layer errors, edge bleed or image-level history.
- It does not reuse a Q4 capture, link DLSS/NR or change a threshold, a code file, an image or the
  submodule.

## 10. Process record

- Reviewed base: `origin/main` at `2d11cd2`; the Q6 code and captures are merged, not working-tree
  changes. The submodule is clean and no game data is present.
- The review is documentation only: one new file, `specs/004-quality-renderer/review-q6.md`. No source,
  image, CMake, rubric threshold or `tasks.md` edit is made here.
- The authoring session did not write or paraphrase this verdict; it was produced by this reviewer
  from the merged artifacts and the commands in §2. This is a process control, not cryptographic
  identity.
- Merge is not claimed: the PR that carries this file stays open for the coordinator.

This output is an independent engineering review record, not a product or art acceptance.
