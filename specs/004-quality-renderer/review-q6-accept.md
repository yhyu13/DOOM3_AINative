# Q6 engine-seam acceptance review — PT-S1..S5 on the Q2 ordered draw and Q3 motion seam

Status: **ACCEPTED WITH LIMITS** for the PT-S1..S5 *engine-seam* evidence. Q6 is **not** accepted as
a playable showcase, a retail `DOOM3.exe` frame, an independently converged reference or a
cross-vendor result. This file is the independent acceptance verdict; the implementer did not write
or paraphrase it.

## 1. Reviewer identity and reviewed revisions

- Reviewer: a fresh independent AI reviewer. I did not author Q6, the five PT identities, the Q2/Q3
  seam, the Q4 content cells, `tests/dxr-capture`, `src/renderer/dxr`, or PR #138. I did not share
  the authoring session or worktree.
- Review worktree: `.kilo/worktrees/review-q6-accept-e544da28108947c4`, branch `review/q6-accept`,
  based on `main` at `0d9a313` (the Q2 T080–T082 doc tick after PR #139).
- Reviewed Q6 merge: `cafe7b6` — `feat(renderer): PT-S1..S5 on the Q2 ordered draw and Q3 motion
  seam (refs #137) (#138)`, merged 2026-11-02. The earlier Q6 review `review-q6.md` (PR #131,
  `86e4eb3`) rejected Q6 before any PT-S1..S5 engine-seam capture existed; that review is not
  superseded for the rows it still finds NOT RUN.
- Inputs read: `path-tracing-rubric.md` (T063), `evidence-q6.md` §7, `evidence/t070/*` (nine PNGs,
  `report-320x180.json`, `report-640x360.json`), `tasks.md` T063–T070 and the Q6 ledger note,
  PR #138's body, and the merged sources `src/renderer/dxr/q6_{content,frame,motion_texture,
  pt_scene}.{h,cpp}`, `tests/dxr/q6_engine_frame_test.cpp`, `tests/dxr-capture/{pt_scene,pt_content,
  dxr_capture,pathtrace_capture.*}`.
- This review writes one new file. It changes no code, image, CMake file, submodule, game data,
  rubric threshold, prior review or `tasks.md` row.

## 2. What the reviewed claim is

The Q6 change claims that the five declared identities are rendered from the production Q2 ordered
`SceneCommandQueue` and the production Q3 `FrameIntegration` (current/previous pose, per-pixel
motion and validity), with the authored Q4 content cells, and that this is an *engine-seam* frame,
not a retail `DOOM3.exe` frame. The evidence is nine committed PNGs plus two JSON reports: four
1024-spp stills against 4096-spp references, a nine-frame PT-S3 sequence with a cut and a
history-blend control, an HDR/alpha negative control, frame pacing at two extents, and a fallback
selection check.

## 3. Commands rerun and observed

All commands ran in this review worktree. Build directories are git-ignored; full logs are retained
there.

| Command | Exit | Observed |
|---|---:|---|
| configure `tests/dxr` with VS 2022 x64 | 0 | MSVC 14.44.35207, Windows SDK 10.0.26100.0 |
| build `q6_engine_frame_test` Release | 0 | device-free seam target linked |
| run `q6_engine_frame_test.exe` | 0 | `SUMMARY q6_engine_frame_test checks=63 failures=0` |
| configure + build `tests/dxr-capture` Release (`dxc` 10.0.26100.0) | 0 | `q6_dxr_capture.exe` + `pathtrace_capture.cso` |
| `q6_dxr_capture.exe --suite pt --repo . --width 320 --height 180` | 0 | `PT_RESULT=PASS`, `EXIT_CODE=0` |
| `q6_dxr_capture.exe --suite pt --repo . --width 640 --height 360` | 0 | `PT_RESULT=PASS`, `EXIT_CODE=0` |
| compare rerun 320 report to committed `report-320x180.json` | — | all five row MAE/scaling/NaN/identity-hash values equal |
| compare rerun 640 report to committed `report-640x360.json` | — | all five row MAE/scaling/NaN/identity-hash values equal |
| SHA-256 of the nine rerun 320 PNGs against the nine committed PNGs | — | 9/9 identical, 0 mismatches |
| SHA-256 of the four rerun 640 stills against `report-640x360.json` | — | 4/4 identical; PT-S3 frame hashes are not in that report |
| compare content atlas SHA-256 values | — | base / pbr / emissive all equal |
| `git -C doom3_orig/doom3 status --porcelain` | 0 | empty |
| bounded `*.pk4` / `*.pk3` / `*.pak` scan | — | 0 files |

The adapter matched the evidence: `Intel(R) Graphics`, LUID `0x000000000001725F`, UMD
`32.0.101.8331`, DXR tier `1_1`, shader model `6.6`, debug layer unavailable.

The pacing figures did not reproduce exactly, as the evidence says is expected for one host sample:
320x180 recorded 11.3561 ms median / 14.3841 ms p95; this rerun measured 11.0598 / 13.6423 ms.
640x360 recorded 28.3582 / 32.5904 ms; this rerun measured 28.8001 / 35.0331 ms. Both reruns stay
inside the `p95 <= 1.5 x median` ratio (1.23 and 1.22), and both are still one 60-frame sample,
not a gate and not a hybrid comparison.

Not rerun by this reviewer: a retail `DOOM3.exe` frame, input-to-displayed-change latency, VRAM
residency, a same-scene hybrid image or latency ratio, 960x540/1280x720 pacing, and any
cross-vendor/driver portability. No second adapter family was present on this host.

## 4. Seam verification in code

The seam is real, not a caption on a synthetic image.

- Q2 order: `q6cap::BuildQ2Queue` emits a real `rhi::SceneCommandQueue` through
  `BeginView` / `Draw` / `DrawLight` / `EndView`. `Q6EngineFrame::Consume` walks
  `queue.commands()` in stream order, resolves each draw through `SceneGeometrySource`, keeps the
  order, and refuses a display overlay before the scene view, a draw before the scene view, a
  surface with no content assignment, and a draw with no resolvable geometry. It counts refusals
  instead of substituting an empty mesh.
- Q3 commit: `Q6EngineFrame::BeginFrame` commits the rendered view and the private pose through
  `FrameIntegration::CommitView`; the identity records draw order, cells, pose identity/generation,
  the reset reason and the history flag. `PreviousPrivatePose` supplies the skinned surface's
  previous vertices in the PT-S3 sequence.
- Q3 motion: `BuildMotion` blends the current/previous posed triangles by the device hit
  barycentrics, calls `rhi::ReprojectPoint`, and stores one `MotionSample` per pixel through
  `FrameIntegration::StorePerPixelMotion`. Invalid reasons are `NoHistory`, `Topology`,
  `Disocclusion` and `BehindCamera`, never a sentinel motion vector. `PackMotionTexture` emits the
  documented RGBA8 binary16 plus R8 validity contract.
- PT-S3 drives the seam per frame: `PtGeometrySource::SetFrame(frameIndex)`, previous pose from the
  Q3 integration, frame 6 is the `Cut` with `Teleport`, `historyValid=false` on the cut, and the
  control re-renders the cut while blending the pre-cut history.
- The device-free `q6_engine_frame_test` asserts the Q2 order, the three refusals, the Q3
  current/previous pose, the per-pixel motion/validity, disocclusion, the packed byte layout and the
  identity-hash sensitivity to draw order. I reran it: 63 checks, 0 failures.

## 5. Reproduced results

At 320x180 (1024 spp against a 4096-spp reference), rerun by this reviewer:

| Row | Content identity | MAE (<=0.01) | scaling (1.5..2.5) | NaN | deterministic | Identity hash |
|---|---|---|---:|---:|---:|---|---|
| PT-S1 | environment + environment + metal + dielectric | 0.000341 | 1.524203 | 0 | yes, 0 px differ | `442cad18e6649efa` |
| PT-S2 | environment + emissive sign + grating | 0.000773 | 1.507982 | 0 | yes | `37b9e7ecd188d48f` |
| PT-S4 | environment + environment + glass + glass | 0.000070 | 1.531330 | 0 | yes | `252f1104448f5671` |
| PT-S5 | environment + environment + metal | 0.000197 | 1.516607 | 0 | yes | `cbc6450f42a8017` |
| PT-S3 | environment + metal + character skin | sequence | — | 0 | repeatable frame hashes | `2b6c0184de69cb76` |

At 640x360 the same rows reproduced with the committed values: MAE 0.000318 / 0.000623 / 0.000069 /
0.000186, scaling 1.525740 / 1.543484 / 1.516658 / 1.526574, 0 NaN, and matching identity hashes.

The PT-S3 sequence reproduced at both extents: 28,597 valid motion pixels on frame 1 at 320x180
(114,372 at 640x360), 0 ghost pixels after the cut against 1,721 in the blend control at 320x180
(6,776 at 640x360), 0 disoccluded pixels at this composition, and 0.244392 world units of skinned
displacement. The PT-S2 HDR control reproduced: 180 pixels above 1.0 at 320x180 (625 at 640x360),
max radiance 16.0, and 0 above 1.0 with the authored alpha test disabled.

I viewed the committed images. PT-S1 shows the dielectric and metal panels in the authored
environment; PT-S2 shows the HDR sign through the authored grating cutout; PT-S3 shows the metal
prop and the deforming skinned sheet, with the prop teleported at frame 6; PT-S4 shows the two glass
panes; PT-S5 shows the large environment surface with the metal panel. The images are genuine DXR
outputs, not placeholders.

## 6. Findings

### Accepted claims

- A-1. The Q2 ordered stream is consumed in order, and the Q3 current/previous pose and per-pixel
  motion/validity drive the PT-S3 reprojection and history reset. This is the claimed seam, and it
  is exercised by both the device-free test and the image-level sequence.
- A-2. The five identities name the authored Q4 content cells; the capture loads and hashes the
  atlas bytes (`PT_CONTENT_CELLS=7/7 COMPLETE`), and the atlas hashes match the committed record.
- A-3. The still metrics and images are reproducible. Both extents reproduced the committed
  MAE/scaling/NaN/identity values, and the nine committed 320x180 PNGs reproduced byte-for-byte
  (9/9 SHA-256).
- A-4. PT-S3's history reset is now measured at image level, not only in CPU fixtures: 0 ghost
  pixels after the cut against 1,721 in the blend control at 320x180. The control discriminates,
  because the same cut with the pre-cut history blended ghosts.
- A-5. The fallback selection refuses a silent switch and keeps an explicit hybrid fallback at unit
  level.

### Limits (these are the conditions of acceptance)

- L-1. Engine-seam, not retail. The pixels come from `tests/dxr-capture` consuming the Q2/Q3
  production objects; T044's engine-fork call sites are absent, so no `DOOM3.exe` process produced
  any of these frames. This is the largest scope limit and is why this verdict is not Q6
  acceptance.
- L-2. The scenes are declared primitives (quad, box, sphere, sheet) plus authored content cells,
  not a game map or a retail scene. The content identity is real; the composition is authored for
  the test.
- L-3. Scene-level convergence independence is not met. The 4096-spp reference is a higher-sample
  accumulation of the same estimator, so MAE and scaling demonstrate Monte Carlo self-consistency,
  not an independently produced reference. The CPU analytic fixtures cover the estimator's
  expectation and variance, not these five scenes. The rubric's flat-patch `p99-relative` row is
  NOT MEASURED; the whole-image p99 (PT-S2 0.301) is recorded as a diagnostic and is not the gate.
- L-4. Input-to-displayed-change latency is NOT RUN. There is no window, input path or same-scene
  hybrid display, so the showcase `<= 2x hybrid` row is not measured.
- L-5. VRAM residency is NOT MEASURED. The D3D12 debug layer was unavailable on this host, so there
  is also no debug-error count for these runs.
- L-6. One host only. The run is Intel Graphics, LUID `0x000000000001725F`, tier 1.1, SM 6.6, UMD
  `32.0.101.8331`. No AMD RX 9070 XT is present, and no cross-vendor or cross-driver claim is made.
- L-7. Frame pacing is one 60-frame sample per extent, including the depth/motion prepass and the Q3
  motion build. The `p95 <= 1.5 x median` ratio passes (1.23 and 1.22), but there is no hybrid
  comparison, 960x540/1280x720 was not measured, and the sample is not a playability gate. The JSON
  `PASS` is the ratio verdict only; the evidence prose correctly says it is not a playability
  statement.
- L-8. The per-capture configuration identity is incomplete against T063 §1. The report names the
  adapter, extent, sample counts and content atlas hashes, but not the camera pose/FOV, maximum path
  depth, per-light radiance/radius, environment radiance, seed/jitter sequence or denoise
  parameters. Those values exist in `q6_pt_scene.h`/`.cpp` and the shader, but a reader of the
  artifact alone cannot reconstruct the capture.
- L-9. The `sample-scaling` band is mis-centred relative to the ideal. For a Monte Carlo error the
  ideal `mae(512)/mae(1024)` is `sqrt(2) ~ 1.414`; the observed 1.508..1.543 passes the predeclared
  `1.5..2.5` band, but a correct rare-emitter estimator at 1.37 would fail. This is an owner
  decision; the band is not widened here.
- L-10. `showcase_selectable=true` is a selection-unit check in which the three quality gates are
  supplied as true. It is not evidence that the visual, performance and motion gates were measured
  against a hybrid path.
- L-11. The engine-seam PT suite does not measure `flat-variance` or `edge-bleed`; the earlier
  flat-field 0.0810 ratio was the synthetic fixture, not these identities. PT-S3 now measures
  `history-ghost` at image level, which is progress over the prior review.
- L-12. Art direction is not reviewed. There is no ACES display path, HUD or authored art review;
  the Q4 §4 dimensions remain outside this evidence.
- L-13. Documentation defects, recorded rather than hidden:
  - `evidence-q6.md` §7.4 cites `evidence/t070/run-320x180.log` and §7.8 says the directory holds
    two run logs, but only the nine PNGs and two JSON reports are tracked; no run log is committed.
  - `tasks.md`'s Q6 note records pacing as 11.24 ms median / 14.75 ms p95 at 320x180 and 26.64 /
    29.01 ms at 640x360. The committed reports record 11.3561 / 14.3841 and 28.3582 / 32.5904, and
    this rerun measured 11.0598 / 13.6423 and 28.8001 / 35.0331. The tasks summary is stale.
  - The 640x360 PNGs are not committed; only their hashes are in `report-640x360.json`.
  - `report-320x180.json` and `report-640x360.json` carry per-row `status: "ACCEPT"` and
    `all_rows_passed: true`. Those are the capture tool's metric verdicts, not this review's
    verdict, and should not be read as Q6 acceptance.
  - The PT-S3 row JSON uses `-1` sentinels for MAE/scaling/p99 and `deterministic_rerun: false`
    because it is a sequence. That is not a failure, but `-1` must not be read as a metric.
- L-14. PT-S4's clear/tinted split is not independently measured in the artifact. The shader samples
  the dielectric tint from the authored glass base map, and the report does not record the per-pane
  tint ratios, so the "clear and tinted" identity cannot be checked from the report alone.
- L-15. Disocclusion pixels are 0 at this composition (the mover stays on screen at both extents).
  The disocclusion reason path is exercised by the device-free test, not by the image sequence.

## 7. Verdict

**ACCEPTED WITH LIMITS** — for the PT-S1..S5 engine-seam evidence only, at the declared 320x180 and
640x360 extents on the single Intel Graphics host, with the limits in §6. The acceptance means:

- the five declared identities are rendered from the Q2 ordered `SceneCommandQueue` and the Q3
  `FrameIntegration`/`HistoryBuffer` seam, with the authored Q4 content cells;
- the committed images and metrics are genuine and reproducible (I reproduced both extents and the
  nine 320x180 PNG hashes); and
- the PT-S3 cut/history-reset claim is supported by a discriminating image-level control.

It does **not** mean Q6 is complete, playable, retail, independently converged, cross-vendor or
art-approved. The following remain blockers for full Q6 acceptance.

## 8. Remaining blockers for Q6 acceptance

| # | Blocker | Affected rows | Unblock condition |
|---|---|---|---|
| Q6-A1 | No retail `DOOM3.exe` frame; this is the engine seam (T044 absent) | §2 all five; §5 showcase | Engine-fork call sites that emit the Q2/Q3 stream from a running game, then a named-content retail capture |
| Q6-A2 | Scene-level convergence has no independent reference | §3 `mae`, `p99-flat`, `sample-scaling` | An independently produced higher-sample renderer, or analytic transport checks for these five scenes |
| Q6-A3 | No playable showcase | §5 input latency, hybrid same-scene latency, motion gate | A window/input path, a same-scene hybrid image and latency ratio, and the §4 motion rows |
| Q6-A4 | VRAM residency not measured | §5 residency | A VRAM query on a named adapter and content set |
| Q6-A5 | One host, no cross-vendor result | §1 adapter, §5 pacing | A second vendor/driver run with the same identities and hashes |
| Q6-A6 | Per-capture configuration is incomplete and the cited run logs are uncommitted | §1 identity | Camera/FOV, max depth, light/environment radiance, seed/jitter, denoise parameters and the run logs in the artifact |
| Q6-A7 | Art direction not reviewed | Q4 §4 applied to Q6 | Authored PT-S1..S5 content under the project display path (ACES, HUD), then an art review |

## 9. What this review does not establish

- It does not accept Q6, a playable showcase, a retail frame, an art-direction result, an
  independent convergence result, input latency, VRAM or a cross-vendor claim.
- It does not claim the 4096-spp reference is independent; it is not.
- It does not claim the capture is a game frame, a map or a retail scene.
- It does not change a code file, image, CMake file, submodule, rubric threshold, prior review or
  `tasks.md` row.
- It does not close issue #68 or any other issue.

## 10. Process record

- Reviewed base: `main` at `0d9a313`; reviewed merge `cafe7b6` (PR #138). The Q6 sources and
  captures are merged, not working-tree changes.
- The submodule is clean and no game data is present. The build directories are git-ignored.
- The authoring session did not write or paraphrase this verdict; it was produced by this reviewer
  from the merged artifacts and the commands in §3. This is a process control, not cryptographic
  identity.
- This file is the verdict. The PR that carries it remains open for the coordinator and is not
  merged by this reviewer. The rubric and ledger status rows are left to the coordinator so that
  this review does not rewrite a prior record.

This output is an independent engineering review record, not a product or art acceptance.
