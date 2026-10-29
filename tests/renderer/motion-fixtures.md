# Motion fixture contract (T041)

**Status**: Q3 design **and analytic fixture vectors**, delivered at
`tests/renderer/motion-fixtures.md` and mirrored here byte-for-byte. No GPU fixture, renderer code or
engine integration exists. T041 is **fixture-document complete, not accepted**: the C++ consumer
tests (T042/T044/T046) and an independent reviewer still have to check the math and mutations, and
Q3 execution (T048) has not run. The generator
`specs/004-quality-renderer/tools/motion_fixtures.py` reproduces every number below. The execution
record is `specs/004-quality-renderer/evidence-q2-design.md`.

**Scope owner**: T041. Consumers: T042 (camera/rigid reprojection), T043 (view identity/history),
T044 (skinned previous pose), T045 (reset cases), T046 (validity/correspondence policy), T085
(real-scene wiring), T048 (Q3 evidence/review). Requirements: FR-008, FR-009, FR-010, SC-004.

---

## 1. Location and authority

T041's delivery path is `tests/renderer/motion-fixtures.md`. Q1a is now merged on `main`
(`src/renderer/rhi/` and `tests/renderer/` exist), so that path is available and this file is
delivered there. The `specs/004-quality-renderer/motion-fixtures.md` copy is a byte-identical mirror
for the design record; `tools/verify-q2-design.py` fails if the two copies diverge. The generator
stays in `specs/004-quality-renderer/tools/motion_fixtures.py` and is run against both copies.

The S4 session was originally barred from `tests/renderer/**` before Q1a; that constraint is now
satisfied, not ignored. The vectors did not change when the file moved.

The vectors are **analytic**. They do not require the engine, game data, a GPU, or a shader. T042+
turns them into a failing test, observes the failure, implements the minimum, and reruns green.

---

## 2. Normative conventions

These conventions are the test contract. A fixture that uses a different sign, origin, unit or
jitter treatment is a different fixture.

| Item | Convention | Source / rationale |
|---|---|---|
| World axes | `viewaxis[0]` = forward, `[1]` = left, `[2]` = up | DOOM 3 `renderView_t` (`RenderWorld.h:206-216`); `viewEntity_t`/`viewDef_t` (`tr_local.h:338-362,367-431`) |
| Camera basis | `depth = dot(forward, p-eye)`, `x_view = -dot(left, p-eye)`, `y_view = dot(up, p-eye)` | `R_SetViewMatrix` flips DOOM 3's "looking down +X" to OpenGL's "-Z"; `tr_main.cpp:838-886` |
| Projection | Infinite-far form; the engine's 16:9 pair from `idGameLocal::CalcFov` (base 90, 640×480 seed): `fov_x=106.260205°`, `fov_y=73.739795°`, `tan_x=4/3`, `tan_y=3/4` | `Game_local.cpp:2376-2435`; `tr_main.cpp:895-958` |
| zNear | Fixture camera records `zNear=3` (the `r_znear` default) but the UV formula is independent of it; do not treat zNear as a vector input | `RenderSystem_init.cpp:79`; `tr_main.cpp:915-958` |
| UV | `u = 0.5 + 0.5*x_view/(depth*tan_x)`, `v = 0.5 - 0.5*y_view/(depth*tan_y)`, **top-left origin** | `data-model.md` motion contract; `research.md` D4 |
| Internal motion | `velocity_uv = uv(previous rendered) - uv(current rendered)` | `data-model.md`; `research.md` D4 |
| Previous state | The last frame that actually **rendered the main view**, not the last simulation tick, not a shadow/mirror/camera subview | `tr_local.h:367-431`; `data-model.md` frame/view history |
| Jitter | Internal history uses the **unjittered** projection; a renderer may jitter the raster projection, but jitter is not part of the vector | `r_jitter` default 0, `RenderSystem_init.cpp:90`; applied in `tr_main.cpp:905-934` |
| Jitter effect | For a symmetric frustum, a one-sided pixel shift `(jx, jy)` changes the unjittered UV by `u -= jx/width`, `v += jy/height`; it is not a centred ±0.5 px offset | Derived from `tr_main.cpp:929-934`; `RandomFloat()` is [0,1) (`Random.h:47,70,82`) |
| Storage | RG16F, two components, in UV units; the table prints the **16-bit half word**, not a little-endian byte string | `data-model.md` frame resource |
| Validity | Separate from the vector; `VALID` + (0,0) is legal; `INVALID` carries a reason and no vector | `data-model.md`; FR-008/FR-010 |
| Provider conversion | Any sign/scale/jitter conversion happens in the provider adapter, after this internal value; the fixture does not define a provider convention | `data-model.md`; `research.md` D4 |

### 2.1 Why the engine's 16:9 pair, not a generic 90° fov

The engine does **not** use `fov_x=90, fov_y=58.7` for 16:9. `idGameLocal::CalcFov` starts from a
640×480 seed and, for `r_aspectRatio=1` (16:9), produces `fov_x=106.260205°`,
`fov_y=73.739795°`, i.e. `tan_x=4/3`, `tan_y=3/4` (`Game_local.cpp:2376-2435`). Using a generic
90° horizontal fov would scale every `du`/`dv` and make the fixture disagree with the engine at
1280×720. The fixture pins the engine pair. If the Q2 scene uses a different `fov_x`/`fov_y`, T042
must either recompute the vectors or record a scale factor in the test; it may not silently reuse
these numbers.

The UV formula itself is independent of `zNear`; `zNear=3` is recorded only so the fixture camera is
fully specified. A test that changes `r_znear` and expects the UV to change is wrong.

### 2.2 Camera-basis derivation

The engine builds a viewer matrix from the camera axes and multiplies it by the OpenGL flip
(`tr_main.cpp:838-886`). For a point `p` relative to the eye, the resulting camera coordinates are
`(-left·delta, up·delta, -forward·delta)`. The fixture uses that closed form so the expected values
do not depend on the matrix storage order; the GPU implementation may build the matrix differently as
long as the projected UV matches.

If the new renderer changes the projection convention (reversed depth, different fov, a separate
viewmodel camera), the fixture must be updated in the same PR that changes it, with a new
pre-registration. Do not silently reinterpret the numbers.

---

## 3. Fixture inputs

All cases use the camera at `eye=(0,0,0)`, `axis = (forward=(1,0,0), left=(0,1,0), up=(0,0,1))`,
1280×720, the engine 16:9 fov pair, and `zNear=3` unless the row says otherwise. `P` is the
point/vertex being projected. The previous and current states are rendered-frame states, not
simulation-tick states.

| Case | Class | Previous state | Current state | Previous P | Current P | What it isolates |
|---|---|---|---|---|---|---|
| `CAM-01` | camera translation | eye `(0,2,0)`, same axis | eye `(0,0,0)` | `(100,0,0)` | `(100,0,0)` | camera translation; positive `du` |
| `CAM-02` | camera rotation | axis yaw +2° (left) | identity | `(100,0,0)` | `(100,0,0)` | camera rotation; positive `du` |
| `CAM-03` | camera / vertical | identity | identity | `(100,0,1)` | `(100,0,0)` | top-left sign; negative `dv` |
| `CAM-04` | camera / zero | identity | identity | `(100,0,0)` | `(100,0,0)` | zero is valid |
| `CAM-05` | camera / jitter exclusion | identity, jitter `(0.7,0.3)` px | identity, jitter `(0.1,0.9)` px | `(100,0,0)` | `(100,0,0)` | history must use unjittered projection |
| `CAM-06` | invalid / behind camera | identity | identity | `(-10,0,0)` | `(-10,0,0)` | current `depth <= 0`; invalid, not zero |
| `CAM-07` | invalid / previous behind camera | identity | identity | `(-10,0,0)` | `(100,0,0)` | previous `depth <= 0`; invalid |
| `RIG-01` | rigid translation | object +Y 0.5 | object identity | `(100,0.5,0)` | `(100,0,0)` | object transform |
| `RIG-02` | rigid rotation | object yaw +1° about Z | object identity | `(100cos1°,100sin1°,0)` | `(100,0,0)` | object rotation |
| `RIG-03` | rigid depth / off-axis | object +X 1 | object identity | `(101,1,0)` | `(100,1,0)` | depth/scale on an off-axis ray; a depth-ignoring renderer gives 0 |
| `RIG-04` | rigid depth / on-axis | object +X 1 | object identity | `(101,0,0)` | `(100,0,0)` | center ray; zero UV is valid |
| `SKN-01` | skinned blend | bone 0 identity, bone 1 +Z 1 | both identity | `(100,0,1)` (0.5/0.5 posed) | `(100,0,0.5)` | previous posed position, not current |
| `SKN-02` | skinned stationary | both identity | both identity | `(100,0,0)` | `(100,0,0)` | bone 0; zero is valid |
| `SKN-03` | skinned rotation | bone 1 yaw +2° | both identity | `(100cos2°,100sin2°,1)`, **weight 1 on bone 1** | `(100,0,1)` | previous pose rotation; slight depth change |
| `DEF-01` | supported deformation | same vertex, z=+0.3 | same vertex, z=-0.2 | `(100,0,0.3)` | `(100,0,-0.2)` | supported deformation, same topology |
| `VM-01` | viewmodel | depth 20, z=+0.1 | depth 20, z=0 | `(20,0,0.1)` | `(20,0,0)` | viewmodel x/y; depth hack only changes z |
| `VM-02` | viewmodel / zero | depth 20, z=0 | depth 20, z=0 | `(20,0,0)` | `(20,0,0)` | zero is valid |
| `SEQ-01` | rendered-frame history | last rendered pose z=+0.5 | current z=+1.0 | `(100,0,0.5)` | `(100,0,1.0)` | correct previous render (`B`) |
| `SEQ-02` | rendered-frame history / negative | last tick pose z=0 | current z=+1.0 | `(100,0,0)` | `(100,0,1.0)` | wrong tick history (`A`); must fail against `SEQ-01` |
| `SEQ-03` | rendered-frame history / same pose | previous render z=+1.0 | current z=+1.0 | `(100,0,1.0)` | `(100,0,1.0)` | same pose rendered twice; expected zero even if a tick advanced |

`CAM-01`'s `du` is positive because the camera moved left and the fixed point is right of the new
camera center. `CAM-03`'s `dv` is negative because the point is higher in the previous frame and the
UV origin is top-left. These two signs are the most common implementation errors.

---

## 4. Expected vectors

Generated by `py -3 -I specs/004-quality-renderer/tools/motion_fixtures.py --emit`. The tolerance is
`max(1 RG16F ULP at |expected|, 1e-7)` per component; §5 explains why. Do not edit this table by hand;
run `--check` after any change.

| Case | Class | uv previous | uv current | du | dv | RG16F word (du, dv) | tolerance (du, dv) |
|---|---|---|---:|---:|---:|---|---|
| `CAM-01` | camera translation | (0.507500000, 0.500000000) | (0.500000000, 0.500000000) | 0.007500000 | 0.000000000 | (0x1fae, 0x0000) | (3.81e-06, 1.00e-07) |
| `CAM-02` | camera rotation | (0.513095289, 0.500000000) | (0.500000000, 0.500000000) | 0.013095289 | 0.000000000 | (0x22b4, 0x0000) | (7.63e-06, 1.00e-07) |
| `CAM-03` | camera / vertical | (0.500000000, 0.493333333) | (0.500000000, 0.500000000) | 0.000000000 | -0.006666667 | (0x0000, 0x9ed4) | (1.00e-07, 3.81e-06) |
| `CAM-04` | camera / zero | (0.500000000, 0.500000000) | (0.500000000, 0.500000000) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |
| `CAM-05` | camera / jitter exclusion | (0.500000000, 0.500000000) | (0.500000000, 0.500000000) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |
| `CAM-06` | invalid / behind camera | — | — | — | — | — | invalid: `BEHIND_CAMERA` |
| `CAM-07` | invalid / previous behind camera | — | — | — | — | — | invalid: `BEHIND_CAMERA` |
| `RIG-01` | rigid translation | (0.498125000, 0.500000000) | (0.500000000, 0.500000000) | -0.001875000 | 0.000000000 | (0x97ae, 0x0000) | (9.54e-07, 1.00e-07) |
| `RIG-02` | rigid rotation | (0.493454351, 0.500000000) | (0.500000000, 0.500000000) | -0.006545649 | 0.000000000 | (0x9eb4, 0x0000) | (3.81e-06, 1.00e-07) |
| `RIG-03` | rigid depth / off-axis | (0.496287129, 0.500000000) | (0.496250000, 0.500000000) | 0.000037129 | 0.000000000 | (0x026f, 0x0000) | (1.00e-07, 1.00e-07) |
| `RIG-04` | rigid depth / on-axis | (0.500000000, 0.500000000) | (0.500000000, 0.500000000) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |
| `SKN-01` | skinned blend | (0.500000000, 0.493333333) | (0.500000000, 0.496666667) | 0.000000000 | -0.003333333 | (0x0000, 0x9ad4) | (1.00e-07, 1.91e-06) |
| `SKN-02` | skinned stationary | (0.500000000, 0.500000000) | (0.500000000, 0.500000000) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |
| `SKN-03` | skinned rotation | (0.486904711, 0.493329270) | (0.500000000, 0.493333333) | -0.013095289 | -0.000004064 | (0xa2b4, 0x8044) | (7.63e-06, 1.00e-07) |
| `DEF-01` | supported deformation | (0.500000000, 0.498000000) | (0.500000000, 0.501333333) | 0.000000000 | -0.003333333 | (0x0000, 0x9ad4) | (1.00e-07, 1.91e-06) |
| `VM-01` | viewmodel | (0.500000000, 0.496666667) | (0.500000000, 0.500000000) | 0.000000000 | -0.003333333 | (0x0000, 0x9ad4) | (1.00e-07, 1.91e-06) |
| `VM-02` | viewmodel / zero | (0.500000000, 0.500000000) | (0.500000000, 0.500000000) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |
| `SEQ-01` | rendered-frame history | (0.500000000, 0.496666667) | (0.500000000, 0.493333333) | 0.000000000 | 0.003333333 | (0x0000, 0x1ad4) | (1.00e-07, 1.91e-06) |
| `SEQ-02` | rendered-frame history / negative | (0.500000000, 0.500000000) | (0.500000000, 0.493333333) | 0.000000000 | 0.006666667 | (0x0000, 0x1ed4) | (1.00e-07, 3.81e-06) |
| `SEQ-03` | rendered-frame history / same pose | (0.500000000, 0.493333333) | (0.500000000, 0.493333333) | 0.000000000 | 0.000000000 | (0x0000, 0x0000) | (1.00e-07, 1.00e-07) |

### 4.1 Notes on individual rows

- `CAM-01`: camera moves +Y (left) 2 units; `x_view=+2`, so `du=0.0075` with `tan_x=4/3`. A sign
  inversion gives `-0.0075`, which the ±3.81e-06 tolerance rejects.
- `CAM-02`: `du = 0.5*tan(2°)/(4/3) = 0.013095289`; not `tan(2°)`. The factor of 0.5 comes from the
  NDC→UV mapping.
- `CAM-03`: `v = 0.5 - 0.5*y_view/(depth*tan_y)`; the point is 1 unit up, so `dv=-0.006666667`.
  A bottom-left origin gives `+0.006666667`; the fixture rejects one of them.
- `CAM-04`/`RIG-04`/`SKN-02`/`VM-02`/`SEQ-03`: zero is a **valid** vector. A policy that treats
  `(0,0)` as "no history" is wrong.
- `CAM-05`: expected is `(0,0)`. A history that uses the jittered projection for the previous frame
  and the current jittered projection produces `(-0.00046875, -0.000833333)` (the jitter shift is
  independent of the fov), which is far outside the tolerance. This is a deliberate synthetic jitter
  pair; it does not define the renderer's jitter sequence.
- `CAM-07`: the current point is in front, but the previous depth is negative, so there is no
  previous correspondence; invalid, not a zero vector.
- `RIG-01`: previous object +Y 0.5 → `x_view=-0.5` → `du=-0.001875`.
- `RIG-02`: previous object yaw 1° → `x_view=-100 sin1°` → `du=-0.006545649`.
- `RIG-03`: off-axis depth motion `(101,1,0)→(100,1,0)` gives `du=+3.7129e-05`; a renderer that
  ignores depth or scale returns 0 and fails. `RIG-04` is the on-axis zero control.
- `SKN-01`: previous posed vertex `(100,0,1)`, current `(100,0,0.5)`; `dv=-0.003333333`. Using the
  current pose for both gives `(0,0)` and fails.
- `SKN-03`: **single-bone** vertex (weight 1 on bone 1), previous bone yaw 2°; `du=-0.013095289`
  and the depth change makes `dv=-4.064e-06`. This row catches an implementation that ignores the
  previous bone transform but keeps the current depth. It is not a 0.5/0.5 blend; that is `SKN-01`.
- `DEF-01`: same vertex identity, explicit previous/current deformation samples; a topology change
  would be `INVALID`, not this vector.
- `VM-01`: `dv=-0.003333333` at depth 20 for a 0.1 unit rise. `weaponDepthHack` changes the
  projection z term and depth range, not x/y (`tr_render.cpp:176-188`), so this x/y expectation is
  unchanged; a depth-derived history policy is invalid for the viewmodel.
- `SEQ-01` vs `SEQ-02`: correct previous rendered pose `B(0.5)` gives `+0.003333333`; the wrong last
  **tick** pose `A(0.0)` gives `+0.006666667`. The difference is 0.003333, far outside tolerance.
- `SEQ-03`: the same pose rendered twice must be `(0,0)` even if a simulation tick advanced; using the
  tick pose `z=0.5` would give `+0.003333333` and fail.

---

## 5. Tolerances and storage

### 5.1 Analytic tolerance

- Compare the decoded RG16F value to the double-precision analytic value.
- `tolerance = max(1.0 * ULP_half(|expected|), 1e-7)` per component.
- Rationale: IEEE binary16 round-to-nearest has error ≤ 0.5 ULP; 1 ULP covers float→half conversion
  and the last arithmetic step. The `1e-7` floor covers exact zero and subnormal values.
- The table's tolerance column is the predeclared value. Do not use a relative epsilon that grows
  with the camera distance; that would hide a wrong scale.

### 5.2 RG16F word

The table prints the **16-bit half word** (`0x1fae`), not the little-endian byte string (`0xae1f`).
The generator uses `struct.unpack('<H', struct.pack('e', x))[0]`. A reader who decodes the printed
word as little-endian bytes will get a wildly wrong value; the test must use the word. `--check`
verifies the word column.

### 5.3 GPU raster nuance

The analytic table is a **numeric** assertion. If T042 rasterizes a point into a larger motion target,
the sample location has a half-pixel ambiguity. Then:

- use a 1×1 target per fixture point, or sample the pixel center and document it; the vector
  tolerance stays §5.1; or
- if a larger target is used, locate the sample with a documented half-pixel rule, but do not widen
  the **vector** tolerance to half a pixel. A half-pixel locator tolerance applies to finding the
  sample, not to the value compared there.
- Use a point primitive or a constant-interpolated attribute. A triangle sampled at a pixel center
  returns an interpolated value, not the vertex's value; that is not this fixture.
- Never compare a rendered colour buffer for these fixtures; read the motion/validity target.

### 5.4 Why not a pixel tolerance

A pixel is 1/1280 = 0.00078125 in u and 1/720 = 0.0013889 in v. The smallest fixture signal
(`SKN-03` `dv=-4.064e-06`, `RIG-03` `du=3.7129e-05`) is far below a pixel, and the jitter signal is
about 0.6 px. A pixel-sized tolerance would pass the jitter mutation and lose the point of the
fixture. Keep the half-ULP rule and make the sample location exact.

---

## 6. Validity matrix

The vector and the validity are separate outputs. `VALID` with `(0,0)` is a stationary surface;
`INVALID` means no correspondence, and any vector value must not be consumed. Reasons are distinct at
the level T046 needs; some rows group a family (topology/identity, reset causes, resize causes) and the
policy may split them further without changing the fixture.

| Case | Condition | Expected | Reason | Notes |
|---|---|---|---|---|
| `VAL-01` | stationary | VALID | `—` | `(0,0)` with VALID |
| `VAL-02` | no previous rendered record | INVALID | `NO_HISTORY` | no vector; never a valid `(0,0)` |
| `VAL-03` | previous UV outside [0,1] | INVALID | `DISOCCLUSION` | newly visible pixel; no vector |
| `VAL-04` | previous or current depth <= 0 | INVALID | `BEHIND_CAMERA` | no vector |
| `VAL-05` | topology or vertex identity change | INVALID | `TOPOLOGY` | no vector; identity included |
| `VAL-06` | renderer/provider switch | INVALID | `PROVIDER_SWITCH` | no vector |
| `VAL-07` | history generation mismatch | INVALID | `GENERATION` | no vector |
| `VAL-08` | cut / teleport / level change | INVALID | `RESET` | T045; one reason for the reset family |
| `VAL-09` | resize / resolution-mode change | INVALID | `RESIZE` | T045; one reason for the resize family |
| `VAL-10` | entity slot reuse | INVALID | `ENTITY_REUSE` | T045 |

T046 owns the correspondence/rejection policy for particles, transparency, reflections, supported
deformation and newly visible pixels. The policy rows below are **inputs** to that policy, not numeric
vectors; T046 must implement or explicitly reject each one.

| Policy input | Requirement | Expected handling |
|---|---|---|
| `POL-01` particle | A particle vertex is camera-facing; its previous position is the previous rendered particle position, not the emitter origin | Valid only with the same particle identity and no respawn; respawn is `ENTITY_REUSE`/`NO_HISTORY` |
| `POL-02` transparent surface | Motion is the surface's motion, not the refracted background's motion | Same surface identity; no special vector; alpha coverage does not change validity |
| `POL-03` reflection / subview | A reflection or security view must not commit the main-view history | The main history is unchanged; a provider/view switch is `PROVIDER_SWITCH` |
| `POL-04` decal / overlay | Decal/overlay vertices use the same previous/current surface identity | Topology or projection change invalidates; no fabricated zero |
| `POL-05` supported deformation | Same vertex count/identity with changed positions | Valid and uses previous posed positions; topology change is `TOPOLOGY` |
| `POL-06` newly visible pixel | Previous UV outside [0,1] or no previous coverage | `DISOCCLUSION`; no vector |

A policy that returns `VALID` with a fabricated zero for any INVALID row fails.

---

## 7. Rendered-frame history and view identity

- The previous state is the previous **rendered** main view. A simulation tick that is rendered twice
  advances history once per rendered main frame; a shadow, mirror or security-camera render must not
  commit the main history (`data-model.md`; `tr_local.h:367-431`).
- `SEQ-01`/`SEQ-02` are the numeric guard for tick-vs-rendered history. `SEQ-03` is the same-pose
  two-render guard: the second render must be `(0,0)`, not a tick-derived motion.
- Skinned previous positions are a **private persistent copy**, not an alias into the shared
  `cachedDynamicModel`. `idMD5Mesh::UpdateSurface` overwrites/reuses vertex storage
  (`Model_md5.cpp:275-347`), and `InstantiateDynamicModel` can delete the cache
  (`Model_md5.cpp:722-745`). Snapshot before the shared geometry is regenerated or deleted. The
  fixture's `SKN-01`/`SKN-03` use the previous posed positions; using the current pose for both gives
  zero and fails.
- Viewmodel: `weaponDepthHack` changes `qglDepthRange` and the projection z term only
  (`tr_render.cpp:176-188`); x/y is unchanged. Use the same camera-basis formula with the viewmodel's
  own depth. If the new renderer gives the viewmodel a separate view/projection, the fixture must use
  that view's `P`/`V` and be updated before use; do not assume a separate camera without evidence.
- `modelDepthHack` is the same z-only change for particles/effects (`tr_render.cpp:195-207`;
  `RenderWorld.h:138`). It does not move the sample in x/y.

---

## 8. Mutation controls

The fixture is only useful if it detects the common wrong implementations. The generator's
`--self-test` runs the numeric mutations below; T042/T044/T046 must add the non-numeric ones as
assertions.

| Control | Mutation | Expected detection |
|---|---|---|
| M1 sign | Compute `uv(current) - uv(previous)` by swapping the states | `CAM-01`/`CAM-03` sign flips; fails tolerance |
| M2 origin | Use bottom-left `v = 1-v` in history | `CAM-03` `dv` flips; fails |
| M3 jitter | Include `r_jitter` in the history projection | `CAM-05` becomes `(-0.00046875,-0.000833333)`; fails vs 0 |
| M4 pose | Use the current skinned pose as the previous pose | `SKN-01` becomes `(0,0)`; fails vs `-0.003333333` |
| M5 tick | Use the last simulation tick instead of the last rendered frame | `SEQ-01` becomes `SEQ-02` (0.003333 vs 0.006667); fails |
| M6 zero | Treat `(0,0)` as invalid | `VAL-01`/`CAM-04` contract fails |
| M7 depth | Reconstruct viewmodel history from depth instead of x/y | `VM-01` x/y cannot be reproduced by a z-only change; T046 policy fails |
| M8 views | Commit history from a mirror/shadow render | T043 view isolation fails; `SEQ-03`/`POL-03` guard |

M1–M5 are numeric and run by `--self-test`; M6 is a contract assertion in the same run; M7/M8 are
T043/T046 assertions and are explicitly deferred, not claimed as passed. The self-test result is
recorded in `evidence-q2-design.md`. A mutation that does not fail is a test gap, not a pass.

---

## 9. Consumers and required tests

| Consumer | Fixture | Required behavior |
|---|---|---|
| T042 | `CAM-01..07`, `RIG-01..04` | Public scene seam computes previous/current UV and reprojects; valid zero stays valid |
| T043 | `SEQ-01..03`, `VAL-06/07`, `POL-03` | History keyed by rendered main-view identity; subviews do not commit; generation changes invalidate |
| T044 | `SKN-01..03` | Private previous pose survives shared-cache regeneration/deletion and subviews |
| T045 | `VAL-08..10` | Cut/teleport/level/resize/topology/entity reuse invalidate |
| T046 | `VAL-01..07`, `VM-01/02`, `DEF-01`, `POL-01..06` | Correspondence/rejection policy; no fabricated dense motion; viewmodel/deformation handled |
| T085 | All | Q2 scene current/previous pose and per-pixel motion/validity wired end to end; consumes these vectors |

T042/T044 must first observe a real assertion failure against the current code, then implement only
the slice, rerun the unchanged test green, and deliberately break the behavior to prove sensitivity.
A compile error or a skipped test is not red evidence.

---

## 10. Status and blockers

| Item | Status |
|---|---|
| Analytic vectors and tolerances | **Done** (this document + generator) |
| Mutation self-test | **Done** (5 numeric mutations + zero-valid contract; M7/M8 deferred) |
| Fixture document at `tests/renderer/motion-fixtures.md` | **Delivered** (byte-identical mirror in `specs/004-quality-renderer/`); Q1a is merged |
| Executable C++ fixture/test consuming the vectors | **Pending**: T042/T044/T046 own the code, not T041 |
| Independent review of the math | **Pending**; this is author self-review, not sign-off |
| T042–T048, T085 | **Not started** |
| Real-scene motion/validity | **Not started**; synthetic vectors alone are insufficient for Q3 |
| FR-010 policy rows for particles/transparency/reflections | **Design input only**; T046 owns the implementation |

T041 is ticked as a design deliverable: the fixture document is delivered and mirrored. The C++
consumer tests (T042/T044/T046) and T048 remain open; the design is usable now, the execution claim
is not.

---

## 11. Traceability

| Requirement | Where |
|---|---|
| FR-008 previous rendered state, conventions, validity separate, reset | §2, §6, §7 |
| FR-009 main/subview isolation, private skinned pose, once per main frame | §7; `data-model.md` |
| FR-010 policies for static/rigid/skinned/viewmodel/particle/transparent/deformed/new | §3, §6, §7; T046 |
| SC-004 analytic reprojection, invalidation, isolation, overlay ordering | §4, §6, §7; T047/T048 |
| T041 output | this document + `tools/motion_fixtures.py` |

**Self-check**: every expected value comes from `tools/motion_fixtures.py`; the table is checked by
`--check`; the tolerance is derived from binary16 precision, not from a renderer result. No GPU,
engine or game data was used.
