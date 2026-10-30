# Q2 classic-path classification fixtures

Status: fixture definitions and observed values for spec 004 Q2 (T034/T037/T038). This document
does **not** close Q2, issue #53, T039 or T040, and it does not claim classic parity. The fixtures
below are **classification fixtures** that run on the CPU through the backend-neutral adapter; the
scene captures they name (`fixture-interaction`, `fixture-shadow`, `fixture-subview`, `fixture-post`,
`fixture-effects`, `fixture-viewmodel`, `hud-control`) are **NOT RUN** because no legal demo data is
present on the host and no classic program is `owned` in the shipping manifest.

The executable implementations are `tests/renderer/engine_command_order_test.cpp` (T034),
`tests/renderer/classic_compatibility_test.cpp` (T037) and `tests/renderer/classic_gui_test.cpp`
(T038). Every expected value is a literal from `classic-parity.md` §4/§6 or from the classic source
anchors named there; no expectation is derived from an adapter output.

## Prerequisites

| Item | Requirement | Missing outcome |
|---|---|---|
| Compiler | MSVC v143, `/W4 /permissive- /EHsc`, C++17 | Configure or build fails; no fixture runs |
| Shader manifest | `shaders/manifest/classic-programs.json` read at run time through the merged spec 008 resolver | Fixture FAILs with an open/parse error; it is not SKIPPED |
| Game data | `base/demo00.pk4` or another legal demo | Scene/image rows are **SKIPPED**; classification rows still run |
| Device | None | These fixtures allocate no device, surface or shader |

## Fixture C — ordered command stream (T034)

Input: a hand-built neutral stream, not an engine capture.

| Case | Stream | Expected executed sequence |
|---|---|---|
| `fifo` | begin view, draw, set target, copy to image 7, swap | `BeginView > DrawPacket > SetColorTarget > CopyToImage > SwapBuffers` |
| `capture-to-file` | begin view, draw, `CaptureToFile(9)`, draw | first execution `BeginView > DrawPacket > CopyToImage`; one command remains |
| `capture-to-image` | begin view, draw, `CopyToImage(13)`, draw | all four execute in one pass; no flush |
| `rejections` | unbalanced `EndView`; copy with destination 0; set-buffer `0x1234` | `EndView` alone is `InvalidArgument` and consumes nothing; the other two are refused at the queue boundary |

Postconditions: the executed prefix is removed from the queue; `FlushMark()` covers the copy, so a
command recorded after the capture is not part of the same execution.

## Fixture M — material plan (T037 / T081)

| Case | Input flags | Expected plan |
|---|---|---|
| `order` | bump + diffuse + specular | `Bump > Diffuse > Specular`, opaque, not refused |
| `alpha` | diffuse + `alphaTest` | coverage `AlphaTested`, draw class `Opaque` |
| `blend` | diffuse + `blend` | coverage `Translucent`, draw class `Translucent` |
| `post` | `postProcess` | draw class `PostProcess`, sort key 100 |
| `megatexture` | `megaTexture` | `usesMegaTexture`, refusal non-null, zero stages |
| `resolver` | bump + diffuse + specular, manifest gate | refused: `interaction` and `bumpyenvironment` are `stub`; zero stages |

The resolver case reads the shipping manifest: 6 `stub`, 1 `unsupported` (`megatexture`), 7 `absent`,
0 `owned`. A material whose stage program is refused is refused whole; no program is substituted.

## Fixture L — light and shadow plan (T037 / T082)

| Case | Input | Expected plan |
|---|---|---|
| ambient | `Ambient` | ambient pass, no interaction pass |
| projected | `Projected`, cookie, falloff 128 | interaction pass |
| point | `Point`, multi-frustum | interaction pass, multi-frustum split |
| projected, no falloff | falloff 0 | `InvalidArgument`; nothing is planned |
| fog / blend | `Fog`, `Blend` | separate pass, no interaction |
| global+local | not self, no suppression | chains `Global`, `Local` |
| self | same entity | chain `Local` only |
| `noShadows` | material flag | suppressed `MaterialNoShadows`, no chain |
| `noSelfShadow` | material flag | suppressed `MaterialNoSelfShadow`, `Local` only |
| light `noShadows` | light flag | suppressed `LightNoShadows`, no chain |

This is a classification, not a stencil-shadow pass. The stencil path is backend work and is
`NOT RUN`.

## Fixture D — depth hacks (T037)

Projection with `x=0.5, y=0.7, z=1.2, z-offset=3.4`:

| Hack | x/y | Expected z |
|---|---|---|
| weapon | unchanged | scale `0.300`, offset `0.850` |
| model, 0.5 | unchanged | scale `1.200`, offset `2.900` |

## Fixture G — overlay and world GUI (T038)

| Case | Expected |
|---|---|
| overlay view | `overlay`, display `1280x720`, projection `2/640`, `-2/480`, origin `(-1, 1)`, identity preserved |
| HUD after scene | scene view then overlay view; exactly one overlay view; recorded view identities `[1, 2]` |
| world GUI | no extra view; one draw in the 3D view; zero overlays |
| overlay target | exactly one colour-target command, before the overlay begins |

## Not run

| Fixture | Reason |
|---|---|
| `fixture-interaction`, `fixture-shadow`, `fixture-subview`, `fixture-post`, `fixture-effects`, `fixture-viewmodel`, `hud-control` captures | No legal demo data on the host; no `owned` classic program; no engine stream |
| Image metrics T-01…T-04, T-05/T-06/T-08/T-09 | Require a GL-vs-GL control and a new-path capture, neither of which can run here |
| Stencil shadow execution, translucent blend order, HUD crispness | Backend passes are not implemented in this slice |

A NOT RUN row is not parity, and a classification PASS is not a rendered frame.
