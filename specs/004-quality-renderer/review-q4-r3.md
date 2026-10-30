# Q4 round-3 independent review — T057 r3 (VRAM addendum)

**Status: ACCEPTED WITH LIMITS (Q4, T057 r3).**
**Reviewed SHA:** `d9b3cb7ef9cd5dbc7a9ab7015e39d025278b3509` (PR #86 merge; `origin/main` at review start).
**Reviewer:** independent; did not author T056, T057, B-1…B-6 or the VRAM probe/measurement. Separate
worktree `review/q4-t057-r3`; no shared session with the authors. This review writes one new file and
does not edit prior reviews, code or images.

## 1. Verdict

**Q4: ACCEPTED WITH LIMITS.** No remaining Q4 blocker. The only round-2 blocker was B4: the process
VRAM peak had not been measured. It is now measured and recorded at **107,110,400 bytes** LOCAL
`CurrentUsage`, reached at `materials_loaded`, in all **12/12 runs**. The three round-2 record defects
(environment residency, "sky" wording, B1/B2 pointers) are fixed. The round-2 art verdicts are
unchanged: 4.1–4.7 `accept`, with 4.3, 4.5 and 4.6 limited and 4.7 carrying a structural note.

This is **not** an unrestricted Q4-green claim. The limits in §6 remain open and are **not upgraded to
passes**.

## 2. VRAM peak: verified as a measurement

`specs/004-quality-renderer/evidence/t056/vram.json` holds 12 run records (4 scenes × 3 runs) and
`complete: true`. Each run has 33 samples in the order `harness_ready`, `materials_loaded`,
`scene_start`, then 30 `frame` samples. Every run exited 0 with 30 frames.

| Scene | Runs | Peak LOCAL `CurrentUsage` | Peak phase | Per-frame usage | NON_LOCAL peak |
|---|---:|---:|---|---:|---:|
| `static_opaque` | 3 | 107,110,400 | `materials_loaded` | 106,971,136 | 0 |
| `alpha_emissive` | 3 | 107,110,400 | `materials_loaded` | 106,971,136 | 0 |
| `moving` | 3 | 107,110,400 | `materials_loaded` | 106,971,136 | 0 |
| `transmissive_environment` | 3 | 107,110,400 | `materials_loaded` | 106,971,136 | 0 |

The peak is 12/12 identical, at `peakFrame: 0`, and is 139,264 bytes above the flat post-upload frame
usage. It is therefore an upload-time process peak, not a frame-time allocation and not an inference
from adapter capacity. `measurements.json` `vram` reproduces the same per-scene peaks, phase and
`peak` block.

**Why this is a measurement, not a record of a guess.**

- `src/renderer/q4_harness/vram_probe.cpp:108-190` resolves the DXGI adapter that owns the RHI device
  by `GetAdapterLuid()` + `IDXGIFactory4::EnumAdapterByLuid` and queries `IDXGIAdapter3`.
- `vram_probe.cpp:213-241` calls `QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL)` and
  `(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)` for the calling process. The value recorded is process
  `CurrentUsage`, not adapter memory.
- `src/renderer/q4_harness/harness.cpp:744-751,781-783,794-796,809-810` samples exactly after
  initialization, after the material upload, at scene start and after every rendered frame.
- `specs/004-quality-renderer/evidence/t056/tools/vram_measure.py:81-181` runs each scene three times
  for 30 frames, parses the one `VRAM_JSON` line, and takes the maximum `CurrentUsage`.

The reported **134,217,728 bytes is the adapter's dedicated memory**, recorded as identity only and
explicitly **not substituted** for the peak. The LOCAL budget of 37,979,832,729 bytes on this UMA host
is also a budget, not a use figure. B4 in `quality-rubric.md:260` is set to the measured
`≤ 107,110,400 bytes`, with the stated condition that a driver, content or allocation change that raises
it requires a new T056 measurement. That is a regression ceiling for this host and these scene
identities, not a portable VRAM budget.

**Limit of this verification.** The worktree has no harness binary or packaged content, so I did not
rebuild and re-measure; the binary is build output and is not tracked, and its recorded `exe_sha256`
cannot be recomputed from git. I verified the complete 12-run record against the probe and harness
source. This closes the round-2 blocker because the blocker was the absence of a process counter, not
the absence of a second host; it is one recorded measurement, not a second independent measurement.

## 3. The three record defects are fixed

| Defect (round 2) | Current record | Verification |
|---|---|---|
| Environment sidecar still said `residency.state: "blocked"` / "no Q4 frame exists" | `content/materials/hero_environment.mat.json:7-16` now says `state: "recorded"`, `measured_resident_bytes: 33,554,512`, `budget_bytes: 268,435,456`, `deferred_uploads: 0`, `over_budget: false`, with the frame and the measurements artifact. The scope string says it is `ResidencyPolicy` accounting, not the process VRAM peak. | Values agree with `measurements.json` `residency` (`resident_bytes` and `uploaded_bytes` 33,554,512, `deferred` 0) and with `quality-rubric.md:218-219`. |
| `evidence-q4.md` called the backdrop "sky" | `evidence-q4.md:263-266` now says the grating shows the **clear-colour** background `(0.012, 0.014, 0.020)`, "No sky is drawn", and the Fresnel reflection lifts "the clear colour, not a drawn sky". | Code agrees: `harness_render.cpp` clears to `(0.012, 0.014, 0.020)` and uses the analytic environment only as a reflection/probe source. The 4.3 sky/atmosphere clause remains **NOT TESTED**, not a pass. |
| B1/B2 pointers still described the historical session frame | `evidence-q4.md:264` points at the B1/B2-session measurement **and** `measurements.json b1.current`; `:319-321` labels the current `02` as `b1.current`; `:359-369` labels the current `01` as `b2.current`. `measurements.json b1.scope,b2.scope` say the historical rows do not describe the committed frames and provide `current`. | I recomputed both. B1 current: 1 dark component (1,895 px), `sd` 15.39, `lit_share` 5.95 %, against the rejected solid shadow's 1 / 2,385 / 1.53 / 0.0 %. B2 current: 0.947 / 3.354 / 0.402 / 1.151 high-pass `sd` with **0 isolated spikes** on all four ROIs. Both match `measurements.json`. |

The historical rows are retained as history, not deleted; the current pointers are explicit. The
sidecar's `upload_per_frame_bytes: 67,108,864` is the 64 MiB/frame **throttle**, not the 33,554,512-byte
upload recorded in `measurements.json`; its `scope` string says "throttle", so it is not a residency
claim. Renaming that field would be clearer, but it does not affect B2 or B4.

## 4. Image and metric re-verification

No image changed after round 2: `git diff --name-status 691b79a..d9b3cb7 -- '*.png'` is empty, and
`review-q4.md` / `review-q4-r2.md` are unchanged by PR #86. I re-checked the committed evidence:

- **15/15** image and crop SHA-256 values match `measurements.json` `images`; byte counts also match.
  The 11 capture records name the same `Intel(R) Graphics` or WARP adapter and the same image hashes.
- `coverage_metrics.py` reproduces the record exactly: glass corr **0.9336**, variance ratio
  **0.3929**, 58.51 % of pane pixels within 20/255, best shift **−9 px** (13.46 % SAD improvement);
  emissive 16× **254.948 ± 0.282** with 5,920/5,920 saturated, 2× control **242.989 ± 5.326** with 0
  saturated; HUD border **2 px**, 20-px cell pitch; skinned pair **5,758** px inside the character box
  and **47,732** outside; WARP parity **4 px** (static) and **1 px** (transmissive).
- `defect_metrics.py --only b2` reproduces the b2 values above; `--only b1` with the rejected
  `50efd1a` frame reproduces the b1 current values above.
- I inspected `01`, `02`, `05`, `08`, `crops/skinned-pose-2x.png` and
  `crops/glass-refraction-2x.png`. The round-2 observations hold: no dielectric speckle, the cutout
  shadow carries the hole pattern (soft, foreshortened), the glass is a bounded refractive slab, the
  HUD is texel-sharp at display extent, and the character is visibly a procedural mannequin.

## 5. Art verdicts (unchanged)

| Dimension | Verdict | Recorded limit |
|---|---|---|
| 4.1 Materials | accept | SDR emissive container; authored 16× display saturates; analytic-env-only metal reflection; screen-space glass. |
| 4.2 Lighting and shadows | accept | Cutout holes are soft/foreshortened (B-1 residual); glass casts no caustic or attenuated shadow because the model does not exist. |
| 4.3 Indirect light, reflections, atmosphere | **accept (limited)** | No drawn sky and no scene-content reflection. The backdrop is the clear colour; the analytic environment is only a reflection/probe source. Atmospheric-range and scene-reflection clauses are **NOT TESTED**. |
| 4.4 Darkness and readability | accept | Visual verdict with sampled values; no numeric contrast threshold is invented. |
| 4.5 Identity | **accept (limited)** | Procedural mannequin, not an authored character; large-environment identity is not proven at this tiling; glass's upper half is a grey card over the void. |
| 4.6 Motion | **accept (limited)** | No history, reprojection, ghosting, cut or teleport path exists, so those clauses are **NOT TESTED**. The committed `03`/`04` pair supports the conclusion; the eight-frame adjacent-pair table comes from an uncommitted `out/motion2` series (N-4). |
| 4.7 HUD / overlay | accept | Display-resolution overlay and ordering verified; history exclusion is structural/untested because the path has no history; T086's provider branch remains **NOT RUN**. |

## 6. Limits that remain open (not passes)

1. **Procedural rig.** 12-bone, 805-vertex procedural humanoid with 2-bone linear-blend posing. It
   proves the skinning path and material/silhouette stability, not a retail mesh or game animation.
2. **Screen-space glass.** Single thin slab, one refraction, five-tap frost, no dispersion, caustics,
   multiple internal reflections or per-layer depth rejection; the pane is excluded from the shadow
   pass. This is not engine-side transmission.
3. **SDR emissive container.** Radiance above 1.0 is the sidecar `emissiveScale` applied after sRGB
   decode into the HDR scene target; the packaged texture remains an SDR mask. The authored 16× display
   is fully saturated; the above-1.0 detail case is the pre-tonemap probe plus the 2× control.
4. **Untested sky / atmosphere clause.** No sky is drawn. The backdrop is the clear colour
   `(0.012, 0.014, 0.020)`; 4.3's atmospheric-range and scene-content-reflection clauses are not
   exercised.
5. **Untested history / reprojection clause.** No temporal history exists, so 4.6's ghosting and
   cut/teleport clauses and 4.7's "not in motion history" clause are not image-tested.
6. **Single-host coverage.** One `Intel(R) Graphics` adapter, driver 32.0.101.8331, one UMA host; WARP
   only for the static and transmissive categories. B3 and B4 are regression ceilings on that host,
   not cross-vendor or device-wide claims. The VRAM figure is process LOCAL `CurrentUsage`, not total
   device memory, and the UMA LOCAL budget is shared system memory, so "over budget" is not a
   meaningful test here.

Further coverage limits, also not passes: no engine or fork frame, no retail character or animation,
one pose per category and no camera path, no second host, the pre-tonemap probe values are the record's
rather than an independent readback, the eight-frame motion table is not reproducible from the
repository (N-4), the environment identity is not proven (N-9), the cutout shadow is heavily
foreshortened (N-6), and the glass `mean_abs` 25.888 is Fresnel-dominated rather than a transmission
measurement (N-7).

## 7. What this review does not establish

No engine/fork frame, no retail content, no drawn sky or atmospheric HDR range, no scene-content
reflection, no history/reprojection or cut/teleport behaviour, no dispersion/caustics/multiple internal
reflections, no second host, no camera path and no device-wide or cross-vendor VRAM figure. It does not
establish Q5/Q6/Q7, classic parity, DXR or DLSS. The Q4 verdict is limited to the committed harness
frames, the recorded measurements and the limits above.

## 8. Process

This review adds `specs/004-quality-renderer/review-q4-r3.md` only. It does not edit `review-q4.md`,
`review-q4-r2.md`, code or images. PR #86 changed no image and left the prior reviews untouched; the
frame hashes and metrics above are recomputed from the committed files. The PR carries `Refs #65` and
is not merged by the reviewer.

`specs/004-quality-renderer/tasks.md:169` still says "T057 r3/addendum must record the final Q4
verdict". This review supplies that verdict; the coordinator should tick the T057 row after the review
PR merges. That is a bookkeeping follow-up, not a quality blocker.

## 9. Recomputed numbers

| Quantity | Value | Source |
|---|---|---|
| VRAM peak | 107,110,400 bytes LOCAL `CurrentUsage`, 12/12 runs, `materials_loaded` | `vram.json`; this review |
| VRAM per-frame | 106,971,136 bytes, flat; NON_LOCAL 0 | `vram.json`; this review |
| adapter dedicated memory | 134,217,728 bytes, identity only | `vram.json`; not used as the peak |
| B2 high-pass sd / spikes | 0.947 / 3.354 / 0.402 / 1.151; 0 / 0 / 0 / 0 | `defect_metrics.py --only b2` |
| B1 current | 1 component (1,895 px), sd 15.39, lit 5.95 % | `defect_metrics.py --only b1` |
| glass | corr 0.9336, variance 0.3929, 58.51 % visible, shift −9 px, 13.46 % SAD improvement | `coverage_metrics.py` |
| HUD | 1280×720, 2-px border, 20-px cells | `coverage_metrics.py` |
| skinned pair | 5,758 px inside (10.6 %), 47,732 outside (5.2 %) | `coverage_metrics.py` |
| WARP parity | 4 px static, 1 px transmissive | `coverage_metrics.py` |
| emissive | 254.948 ± 0.282 (16×) vs 242.989 ± 5.326 (2×) | `coverage_metrics.py` |

## 10. Final decision

**ACCEPTED WITH LIMITS.** The Q4 measurement gap is closed, the record defects are corrected, and the
art dimensions are accepted at the round-2 level with the same limits. None of the limits in §6 is a
pass, and the Q4 verdict should not be quoted as unrestricted green.
