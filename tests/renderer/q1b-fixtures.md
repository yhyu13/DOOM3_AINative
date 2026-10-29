# Q1b synthetic graphics fixtures

Status: fixtures and worked expected values for spec 004 Q1b (T022). This
document does not close Q1b, Q1, issue #22 or issue #11. Full Q1 remains
blocked until the debug layer and GPU-based validation actually execute with
zero unwaived errors; on the current machine both are unavailable
(`D3D12GetDebugInterface` returns `0x887A002D`), so they are recorded as
SKIPPED rather than PASS.

The fixtures are implemented by the public-seam tests under
`tests/renderer/*.cpp` and the D3D12-private companion under
`tests/renderer/d3d12/`. The independent values below are literals; no value is
derived from a readback.

## Shader and tool prerequisites

| Item | Requirement | Missing or unsupported outcome |
|---|---|---|
| Host API | Installed Windows SDK D3D12 and DXGI headers and libraries | Configure or build fails; no fixture runs |
| Shader build | `D3DCompile` from the installed `d3dcompiler_47` runtime, targets `vs_5_0`, `ps_5_0`, `cs_5_0`, `D3DCOMPILE_ENABLE_STRICTNESS` | `CreateShaderModule` returns `Status::Error` with the compiler message; fixture is FAIL, not SKIPPED |
| Program text | The five embedded programs in `tests/renderer/q1b_test_support.h`: fullscreen vertex, pattern pixel, MRT+depth pixel, depth-sample pixel, compute | No external shader file, include path or game data is read |
| Adapters | Hardware (`Intel(R) Graphics`, vendor `0x8086`, device `0x7D67`) and Microsoft Basic Render Driver (WARP, vendor `0x1414`, device `0x008C`) | A missing adapter class is SKIPPED, never PASS |
| Debug layer | `ID3D12Debug` from the Graphics Tools optional feature | `0x887A002D`; recorded SKIPPED by `rhi.q1a.diagnostics`; not claimed |
| GPU-based validation | `ID3D12Debug1::SetEnableGPUBasedValidation` | Same missing component; recorded SKIPPED; full Q1 stays BLOCKED |

No fixture uses retail data, a proprietary SDK, a network download or a
machine-specific absolute path.

## Fixture P — programmable draw

Frame: 8×8 RGBA8, one colour attachment, one pass, no depth. The vertex program
covers the viewport with one triangle; the pixel program writes

```
base(x,y) = (x*32, y*32, 128, 1.0)
output    = base * (tint.rgb, 1.0)
```

with 8-bit UNORM conversion. Expected bytes for tint 1.0:

| y \ x | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| 0 | 0,0,128,255 | 32,0,128,255 | 64,0,128,255 | 96,0,128,255 | 128,0,128,255 | 160,0,128,255 | 192,0,128,255 | 224,0,128,255 |
| 1 | 0,32,128,255 | 32,32,128,255 | 64,32,128,255 | 96,32,128,255 | 128,32,128,255 | 160,32,128,255 | 192,32,128,255 | 224,32,128,255 |
| 2 | 0,64,128,255 | 32,64,128,255 | 64,64,128,255 | 96,64,128,255 | 128,64,128,255 | 160,64,128,255 | 192,64,128,255 | 224,64,128,255 |
| 3 | 0,96,128,255 | 32,96,128,255 | 64,96,128,255 | 96,96,128,255 | 128,96,128,255 | 160,96,128,255 | 192,96,128,255 | 224,96,128,255 |
| 4 | 0,128,128,255 | 32,128,128,255 | 64,128,128,255 | 96,128,128,255 | 128,128,128,255 | 160,128,128,255 | 192,128,128,255 | 224,128,128,255 |
| 5 | 0,160,128,255 | 32,160,128,255 | 64,160,128,255 | 96,160,128,255 | 128,160,128,255 | 160,160,128,255 | 192,160,128,255 | 224,160,128,255 |
| 6 | 0,192,128,255 | 32,192,128,255 | 64,192,128,255 | 96,192,128,255 | 128,192,128,255 | 160,192,128,255 | 192,192,128,255 | 224,192,128,255 |
| 7 | 0,224,128,255 | 32,224,128,255 | 64,224,128,255 | 96,224,128,255 | 128,224,128,255 | 160,224,128,255 | 192,224,128,255 | 224,224,128,255 |

Per-pass uniform variants (the upload path must not leak between frames):

| tint | (0,0) | (3,5) | (7,7) |
|---|---|---|---|
| 1.0 | 0,0,128,255 | 96,160,128,255 | 224,224,128,255 |
| 0.5 | 0,0,64,255 | 48,80,64,255 | 112,112,64,255 |
| 0.25 | 0,0,32,255 | 24,40,32,255 | 56,56,32,255 |
| 0.75 | 0,0,96,255 | 72,120,96,255 | 168,168,96,255 |

Tolerance: absolute per-channel difference ≤ 1 UNORM step. Every expected
channel is an integer derived from the published literal rule, so a conforming
run is exact.

Negative controls:

| Control | Method | Required observation |
|---|---|---|
| Adjacent pixel | Compare each pixel with its right neighbour before verification | The comparator must see at least one difference; the pattern varies with x |
| Wrong tint | Read a 0.5-tint frame and verify it against tint 1.0 | Verification must fail |
| Out-of-range attachment | Read attachment 1 of a one-attachment frame | `Status::InvalidArgument` |
| Pipeline/attachment mismatch | Submit a two-colour pass with a one-colour pipeline | `Status::InvalidArgument` |

## Fixture C — compute and UAV readback

Dispatch: 4×4 elements, one `numthreads(4,4,1)` group. The program writes

```
result[y*4+x] = ((x*16 + y*3 + 7) ^ 0xA5)
```

Expected 32-bit values, little-endian in the readback buffer:

| y \ x | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| 0 | 162 | 178 | 130 | 146 |
| 1 | 175 | 191 | 143 | 159 |
| 2 | 168 | 184 | 136 | 152 |
| 3 | 181 | 133 | 149 | 229 |

Buffer shape: 16 elements × 4 bytes = 64 bytes, tightly packed, element 0
first. There is no row pitch in the public result.

Negative controls:

| Control | Method | Required observation |
|---|---|---|
| Transposed expectation | Compare the result with `expected[x*4+y]` | Must differ |
| Zero workgroup count | Submit `groupsX = 0` | `Status::InvalidArgument` |
| Wrong pipeline kind | Submit a graphics pipeline to a dispatch | `Status::InvalidArgument` |

## Fixture D — multiple attachments and sampled depth

Frame: 8×8, four colour attachments, one D32_FLOAT depth attachment cleared to
1.0. Pass 0 writes attachments 0–2 and `SV_Depth = 0.25` for `x < 4`, else
0.75. Pass 1 samples the depth attachment as a point-sampled texture and
writes `depth*255` into the red channel of attachment 3.

Expected colours:

| Attachment | Expected RGBA | Note |
|---|---|---|
| 0, x > 0 | 10,20,30,255 | Pass 0, first output |
| 1, x > 0 | 40,50,60,255 | Pass 0, second output |
| 2, x > 0 | 70,80,90,255 | Pass 0, third output |
| 0–2, x = 0 | 0,0,0,255 | Depth 1.0 fails `LESS` against the 1.0 clear, so the fragment is discarded and the clear survives |
| 3, x = 0 | 255,0,0,255 | Discarded fragment leaves depth at the 1.0 clear |
| 3, 1 ≤ x < 4 | 64,0,0,255 | 0.25 converted to UNORM |
| 3, x ≥ 4 | 191,0,0,255 | 0.75 converted to UNORM |

Depth conversion:

| Written depth | Expected byte |
|---|---|
| 1.0 (discarded by `LESS`) | 255 |
| 0.25 | 64 |
| 0.75 | 191 |

The depth test is `LESS` against the 1.0 clear, depth writing is enabled in
pass 0 and disabled in pass 1, and pass 1 reads the depth attachment after
pass 0 wrote it. The `x = 0` fragment writes depth 1.0, so it is discarded: its
colour attachments keep the clear and the sampled depth is 255. That pixel is
the depth-test control. Attachment order and index are part of the expectation.

Negative controls:

| Control | Method | Required observation |
|---|---|---|
| Cross-attachment | Verify attachment 1 against attachment 0's colour | Must fail |
| Depth sample on a colour attachment | Verify attachment 2 with the depth expectation | Must fail |
| Depth use without a depth attachment | Submit the sampling pass with `depthEnabled = false` | `Status::InvalidArgument` |
| Duplicate colour write | Two passes write attachment 0 | `Status::InvalidArgument` |

## Fixture F — frame slots and upload ownership

Four draw frames with tints 1.0, 0.5, 0.25 and 0.75 are submitted without
waiting. The public test proves that every completed frame keeps its own tint
and that the slots are reusable after completion. A private gated companion
holds the four frames on a backend-private gate, so the fifth submission is
deterministically refused with `Status::NotReady`; it also records four distinct
slot indices and checks that an in-flight readback is `NotReady`.

Negative controls:

| Control | Method | Required observation |
|---|---|---|
| In-flight reuse | Ignore the slot's in-flight state in the allocator | The fifth submission succeeds and the private test fails |
| Wait-idle shortcut | Wait for completion before every submission | The fifth submission is accepted; the private test fails |
| Stale upload | Overwrite a slot's uniform storage while it is in flight | A completed frame shows another frame's tint |

## Fixture M — motion, validity and composition (portable, no device)

Analytic model: pixel = frame centre + (world − camera centre) × focal / depth.
Camera previous centre (0,0), current centre (0.5,0), focal 100, depth 2, frame
64×64, world point (0,0).

| Case | Expected |
|---|---|
| Previous-minus-current motion | x = 25, y = 0, valid = true |
| Static camera | x = 0, y = 0, valid = true (distinct from invalid) |
| Depth 0 | valid = false |
| Depth −1 | valid = false |
| Off-frame projection (world x = 1000) | valid = false |
| History resize 8×8 | generation 1, every sample invalid |
| Store a valid zero at (0,0) | (0,0) valid, (1,0) invalid |
| History reset | generation 2, every sample invalid again |

Composition order for layers `[scene, display overlay, world gui]` is
`[scene, world gui, display overlay]`; reconstruction input count is 2 and the
display overlay is excluded. A plan without exactly one scene layer is
`Status::InvalidArgument`.

## Fixture G — declared accesses and pass order (portable, no device)

| Case | Expected |
|---|---|
| Write colour 0 and 1, then read 1 | Ok, order 0 then 1 |
| Read a transient before any write | `Status::InvalidArgument` |
| Read an imported resource first | Ok |
| Use an undeclared resource | `Status::InvalidArgument` |
| Declare a resource twice | `Status::InvalidArgument` |
| Declare one resource twice in one pass | `Status::InvalidArgument` |
| Empty pass name | `Status::InvalidArgument` |
| Three declared passes | Order 0, 1, 2 preserved |

## Fixture S — window surface

| Case | Expected |
|---|---|
| Create 64×48 surface | Ok, or `Status::Unsupported` on a display stack without a window surface (SKIPPED) |
| Compose a solid frame | Readback shows the same colour at every pixel; dimensions match |
| Resize to 96×64 | Readback dimensions become 96×64; no stale 64×48 content |
| Minimize and submit | `Status::NotReady` with an actionable message |
| Restore, resize, submit | Ok; readback matches the new colour and size |
| Destroy and restart the device | A new surface can be created and composed |
| Window capture | `PrintWindow` content is written when the platform permits it; otherwise NOT RUN, never PASS |

## Reproducible validation

From a fresh worktree-local build directory:

```powershell
py -3 -I scripts/validate-q1b-fixtures.py
py -3 -I scripts/validate-q1b-fixtures.py --self-test
```

The validator parses the Fixture P/C/D tables and compares them with literal
expected data held in the script; it also checks that the Fixture G/S required
controls are present. It exits 0. `--self-test` alters one value in memory and
requires the altered table to fail. An altered tracked document must make the
first command exit non-zero.

Fixture execution commands and retained logs are recorded in
[`evidence-q1.md`](../../specs/004-quality-renderer/evidence-q1.md). A skipped
GPU fixture, a missing adapter or an unavailable validation component is not a
pass.
