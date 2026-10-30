# Q6 path-tracing rubric, scene identities and pre-review thresholds (T063)

Status: **pre-review record**, written 2026-10-30 for issue #68. This file fixes what a Q6 review
will look at and how it will be measured. It is **not** a measurement and not a review. No capture,
frame time, input-latency sample, VRAM figure or hardware ray-tracing result exists at this commit;
every row below is `NOT RUN` until the named hardware, legal content and a Q6 engine frame exist.

The rubric is separate from Q4. A frame that looks like the hybrid renderer is not a Q6 pass: Q6 asks
whether a path-traced reference converges to an independently computed image, whether the material
transport (alpha, transmission, emission) behaves, and whether a playable showcase meets the motion
and latency gates **without** removing the non-path-traced renderer.

---

## 1. Scene and configuration identity

A capture may only be reviewed when it names every field below. A field that cannot be named is a
`BLOCKED` row, not a default.

| Field | Requirement |
|---|---|
| `scene` | one of the stable identities in §2 |
| `mode` | `reference` (converged still or offline accumulation) or `showcase` (playable) |
| `renderExtent` / `displayExtent` | both stated; the reference may render and display at the same extent |
| `camera` | position, direction and FOV, or the path and key frames |
| `content` | material and geometry identity, with the Q4 content-matrix cell and its licence state |
| `lighting` | emitters (kind, linear radiance, radius if any), the environment (upper/lower radiance or a named map), and the shadow treatment |
| `transport` | alpha rule, transmission tint/IOR, emission range, and any case declared unsupported |
| `sampling` | samples per pixel, maximum path depth, seed, jitter sequence, accumulation count |
| `denoise` | filter radius, colour/depth tolerances, history rule and reset policy |
| `backend` / `adapter` / `driver` | required by spec 004 FR-018 and spec 009 FR-013; an unreported id is recorded as unreported, never as a real zero |
| `dependencies` | absence of DLSS/NR or any proprietary provider in the default path (FR-014) |

## 2. Scene identities

The five identities are fixed now so two people can reproduce them. Geometry and content are named;
the numeric budgets are not set by this file.

| Id | Scene | Content | Motion | Status |
|---|---|---|---|---|
| PT-S1 `static-convergence` | dielectric and metal under one direct key plus a named environment, camera still | `content/hero/dielectric/` (CC0, authored) and the `opaque_metal` cell | none | **BLOCKED**: metal cell not authored; no Q6 frame |
| PT-S2 `alpha-emissive` | alpha-tested cutout in front of an HDR emissive sign | `alpha_tested` and `emissive` cells | none | **BLOCKED**: cells not authored |
| PT-S3 `motion` | rigid prop and skinned character crossing a lit floor | `skinned_character` cell | rigid and skinned keys, with a cut/teleport frame | **BLOCKED**: cell not authored |
| PT-S4 `transmission` | clear and tinted glass with an environment behind it | `transmissive_glass` cell | none, then a slow camera move | **BLOCKED**: cell not authored |
| PT-S5 `large-environment` | the large environment surface with the residency test surface | `large_environment` cell | camera dolly | **BLOCKED**: cell not authored |

The Q4 content matrix is 1 of 7 authored (`specs/009-rhi-hd-content/tasks.md`), and no Q6 engine frame
exists. That is why all five rows are blocked, not a scheduling preference.

### 2.1 CPU fixture identities (implemented, not a visual capture)

The Q6 code slice is checked on CPU fixtures that are named here so the review can map code evidence
to a scene. They are not substitutes for PT-S1..S5.

| Fixture | Scene | What it proves |
|---|---|---|
| `cpu-static-wall` | horizontal-normal diffuse wall, two-hemisphere environment (upper 1, lower 0) | the estimator's mean is `albedo * 0.5` and its single-sample variance is `albedo^2 * 0.25` |
| `cpu-direct-shadow` | floor, one point light, an optional occluder | the direct term is `albedo / pi * radiance * cos / distance^2`, and an occluder removes it |
| `cpu-emissive-box` | diffuse floor inside an emissive ceiling and four emissive walls | the first bounce returns `albedo * emission` with zero variance |
| `cpu-alpha-primary` / `cpu-alpha-shadow` | one alpha-masked quad mapped wholly to a solid or a hole cell | a hole passes the primary ray and the shadow ray; a solid cell does not |
| `cpu-transmission` | one clear plane, upper environment 0, lower environment 2, tint (1, 0.5, 0.25) | the mean is `(1 - F0) * lower * tint` with `F0 = 0.04` |
| `cpu-refit-grid` | a 4x4 grid, rigid and deformed refits, one far move | no stale hit remains, every triangle is reachable, a topology change rebuilds |
| `cpu-denoise-edge` | flat noisy field, colour step, depth step | variance falls on a flat region and neither a colour nor a depth edge is smeared |

## 3. Convergence

A converged reference is compared with an **independently produced** higher-sample reference of the
same scene and configuration. The higher-sample image is not the only source of truth: the analytic
fixtures in §2.1 check the estimator's expectation and variance directly.

| Metric | Definition | Pre-review threshold |
|---|---|---|
| `mae` | mean absolute error per channel in linear radiance between an N-sample and a 4096-sample reference, over the evaluated pixels | at 1024 spp: **≤ 0.01** |
| `p99-relative` | 99th percentile of `abs(error) / max(reference, 0.01)` | **≤ 5%** on flat patches |
| `nan-pixels` | pixels with NaN or infinity | **0** |
| `sample-scaling` | `mae(512) / mae(1024)` | in **1.5 .. 2.5** (a converging estimator halves the error when it doubles the samples) |

A result that misses a threshold is a rejection, not a rounding matter. A result from a scene that
does not match its declared identity is invalid.

## 4. Noise and denoising

| Metric | Definition | Pre-review threshold |
|---|---|---|
| `flat-variance` | luminance variance over a declared flat patch | denoised ≤ 25% of raw (a 4x reduction) |
| `edge-bleed` | maximum luminance change across a declared edge | ≤ 1% of the step height |
| `history-ghost` | pixels whose error versus the reference exceeds twice the raw error on the first frame after a cut, teleport or disocclusion | **0** pixels beyond one frame |
| `history-reset` | a rejected correspondence (invalid, generation, topology, reset, resize, provider switch, new pixel) | must replace history, not blend it |

The denoiser is the documented bilateral filter plus the Q3 history rule. A learned or vendor
denoiser is out of scope until Q5's gates are met.

## 5. Latency, frame pacing and the showcase gate

Reference mode is an offline reference; its frame time is recorded but is **not** a playability gate.
Showcase mode is only selectable when the visual, performance and motion gates have passed.

| Metric | Definition | Pre-review threshold |
|---|---|---|
| `frame-median` / `p95` | frame time over a stated sampling window, same scene as the hybrid path | `p95 ≤ 1.5 × median` |
| `input-latency` | input event to displayed change, same scene and display | showcase **≤ 2 ×** the hybrid path |
| `motion-gate` | ghosting and reprojection rows in §4 on PT-S3 | must pass |
| `fallback` | with ray tracing unavailable or a gate failed, the hybrid renderer is still selectable | **always true**; an automatic mode switch is not accepted |

Absolute frame-time budgets are deliberately unset. They are recorded after the first measurement on
named hardware, in the same change as the evidence, before the review verdict. A number added after a
rejection is not a threshold.

## 6. Verdict rules

1. Every row is `accept`, `reject`, `SKIPPED` or `NOT RUN`, with an artifact path or an exact reason.
2. `SKIPPED` for unavailable ray-tracing hardware is legitimate and keeps the fallback claim; it is
   never a pass.
3. A static converged image alone is **not** a playable showcase. PT-S3 and §5 are required for the
   showcase verdict.
4. A missing fallback, a silently switched mode or a claimed device result without a device query is
   an automatic reject.
5. The reviewer is not the implementer and records the reviewed commit and the commands rerun.

## 7. Current state

- The Q6 code slice (acceleration structures, ray dispatch, material/lighting integration,
  alpha/transmission, reference accumulation plus bilateral denoise and history rejection, explicit
  fallback selection) is implemented and its CPU fixtures pass; see `evidence-q6.md`.
- PT-S1..S5 are **BLOCKED** on authored content and on a Q6 engine frame. No capture exists.
- No DXR device path is implemented in this slice, so no hardware tier, frame time or latency is
  measured or claimed.
- Every numeric threshold above is predeclared and **unmeasured**.
