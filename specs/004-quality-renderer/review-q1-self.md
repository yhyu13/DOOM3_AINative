# Q1b / Q1 self-review

Author: the S5 implementer session.
Date: 2026-10-29.
Worktree: `feat/22-q1b-q1-rhi`, rebased onto `origin/main` at `29a1e37`.

**This is not T031.** T031 requires a different reviewer who reruns the work.
This document records the author's own inspection and deliberately does not
count as independent review, approval or a Q1 pass. Issue #22's fourth
acceptance criterion (an independent reviewer) and T031 remain open.

## Scope checked

| Source | Check |
|---|---|
| `tasks.md` T022–T030 | Every named fixture, file and control exists |
| Issue #22 | Fixture document, literal values, tool prerequisites, NOT RUN rules and validator are present; independent review is not |
| `contracts/rhi-foundation.md` "Foundation growth required for full Q1" | Draw/pipeline, compute, MRT, sampled depth, motion/validity, overlay, declared uses, frame slots, resize, history and diagnostics are covered |
| Q1a public seam | No Q1a test or behaviour regressed (17/17 including the seven Q1a cases) |

## Axes reviewed

### 1. Public boundary and contract

- The validator scans three core headers and eleven public test units and
  reports no prohibited noun (`build-q1b/validator.log`). The private backend
  files live under `src/renderer/d3d12/` and the private tests under
  `tests/renderer/d3d12/`, both excluded by contract.
- `rhi.h` exposes opaque shader, pipeline and surface handles; no native type,
  state value, queue value or handle crosses it.
- The frame graph consumes declared reads and writes. It does not expose a
  state change and does not alias resources.
- **Caveat**: the shader module carries opaque bytes, but the C++ fixtures embed
  a program in the backend's native language. The Q7a mapping gate still owns
  the portable shader-module format. This is recorded in `evidence-q1.md` and
  is not claimed as portability evidence.

### 2. Lifetime and ownership

- Every submission owns its colour attachments, depth attachment, readback
  buffers and view heaps until its completion token is reached.
- A frame slot is reused only after `GetCompletedValue() >= slot.serial`.
  Four slots are bound; the fifth concurrent frame returns NotReady.
- The surface path copies the composed backbuffer to a readback buffer before
  the display call and keeps that buffer with the submission.
- **Finding F1 (High, fixed during this review)**: `DestroyAll` released
  surfaces before waiting for in-flight slots, and `DestroySurface` did not wait
  at all. A surface frame still in flight could therefore reference a released
  chain buffer. Both now wait (bounded 10–30 s) and return NotReady when
  ownership cannot be proved. `DestroyAll` waits every slot before destroying
  any surface.
- **Finding F2 (Medium, accepted)**: a failed attachment or record allocation
  leaves the acquired slot with serial 0. The next `AcquireSlot` observes
  completion 0 and resets it; no in-flight memory is reused. This is deliberate
  and safe but relies on serial 0 never being a valid submission value (the
  counter starts at 1).

### 3. Correctness of the worked fixtures

- The pattern is recoverable from the pixel coordinate; expected bytes are
  literals, and the tint table is independently checked.
- Compute expects sixteen literal 32-bit values; transposing the table must
  fail.
- The MRT fixture writes three distinct colours and a left/right depth split.
  Pass 2 samples the depth texture and must produce 64 or 191.
- The fixture document's tables and the C++ literals were reconciled by
  `scripts/validate-q1b-fixtures.py`; the validator's self-test rejects altered
  pattern and compute values.
- **Finding F3 (Medium, accepted)**: the public fifth-frame slot probe can be
  NOT APPLICABLE because 8×8 frames finish before the probe. The private gated
  test is the deterministic control and fails when in-flight slots are reused.
  The public test records the NOT APPLICABLE outcome instead of claiming a pass.

### 4. Failure behaviour

- Missing adapters are SKIPPED; a missing shader compiler or a rejected program
  is an actionable Error with the compiler message.
- A minimized surface returns NotReady without recording work.
- The private removal hook makes completion, readback, release and submission
  fail with DeviceLost/RefusedOutstanding; teardown returns in 0 ms and a fresh
  device renders afterwards.
- Debug layer and GPU-based validation are attempted and SKIPPED
  (`0x887A002D`). No unwaived-error claim is made.

### 5. Tests and evidence

- Debug and Release: 17/17, 0 failed, both adapter classes attempted.
- Red/mutation controls for draw, compute, depth, slots, motion, frame graph and
  surface each produced a real behavioural failure and were restored.
- `git diff --check` exits 0.
- Captures: the composed backbuffer BMPs are pixel-checked; window-level
  `PrintWindow` BMPs are written but not pixel-asserted.
- **Finding F4 (Low)**: `PrintWindow` returns a platform-composed image whose
  pixels were not verified. It remains a supplementary artifact, and the
  backbuffer readback is the evidence.

### 6. Performance and resource shape

- **Finding F5 (Low, open)**: RTV/DSV heaps and attachment textures are created
  per submission rather than per slot. This is safe and simple, but Q2 will need
  bounded pooling and measurement. No performance claim is made.
- **Finding F6 (Low, open)**: the compute element bound uses
  `D3D12_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP`, a conservative constant.
  It rejects pathologically large structured buffers rather than sizing them.
- **Finding F7 (Low, open)**: one 256-byte uniform block per pass. A production
  frame needs bounded growth; this is Q2+ work.

### 7. Documentation

- `tests/renderer/q1b-fixtures.md` states scope, prerequisites, expected values,
  tolerance, order, negative controls and NOT RUN rules.
- `evidence-q1.md` records commands, exits, adapters, executed versus skipped
  checks, red/green/mutation logs, artifacts and limitations.
- `quickstart.md` now has runnable Q1b commands.
- `traceability.md` marks Q1b partial and keeps T030/T031 open.

## Findings summary

| ID | Severity | Status | Evidence |
|---|---|---|---|
| F1 | High | Fixed | `frame_resources.cpp` waits slots before surfaces; `rhi_present.cpp` DestroySurface waits; 17/17 rerun after the fix |
| F2 | Medium | Accepted | serial 0 is never issued; next acquisition resets the slot |
| F3 | Medium | Accepted | private gated test is the deterministic bound; public probe logs NOT APPLICABLE |
| F4 | Low | Accepted | backbuffer BMPs are the asserted images |
| F5 | Low | Open (Q2) | per-submission view heaps; no performance claim |
| F6 | Low | Open (Q2) | conservative compute element bound |
| F7 | Low | Open (Q2) | single 256-byte uniform block |

## Second-pass audit

A separate read-only audit pass reviewed the backend lifetime, the public seam
and the fixtures. It found two blocking defects and several rigour gaps; all
were fixed before the final rerun:

| Finding | Severity | Disposition |
|---|---|---|
| A failed `Signal` left the slot at serial 0, so a later acquisition could reset an allocator whose list was already executing | High | Fixed: the real serial is published before `ExecuteCommandLists` |
| A failed surface resize left null back buffers that the next submit dereferenced | High | Fixed: explicit check with an actionable Error |
| A gate-fence failure after record insertion can quarantine the device | Medium | Accepted: rare OOM path, no memory unsafety; recorded as an open teardown limitation |
| `CreateQ1bState` could leave Q1b state over released device objects | Medium | Fixed: state reset and `RequireQ1b` checks `initialized` |
| A rejected submission burned a serial that a surface wait would observe | Medium | Mitigated: `LastSerial` now reports the highest in-use slot serial, not the allocation counter |
| Duplicate serial allocation per submission | Low | Fixed |
| Public tests used native window handles | High (contract) | Fixed: moved to the private surface test; the gate now rejects Win32 vocabulary |
| The draw expectation recomputed the shader formula | Medium | Fixed: literal RGBA8 tables |
| The public fifth-frame probe could pass with an unbounded allocator | Medium | Fixed: probe removed; private gated test is the bound |
| The depth test never saw a failing fragment | Medium | Fixed: the `x = 0` fragment is discarded and expected values include it |
| In-flight readback was not checked for the new entry points | Medium | Fixed in the private gated test |
| A graphics pipeline could be passed to a dispatch | Low | Fixed: `InvalidArgument` control |
| Fixture doc said four programs and tinted alpha | Low | Fixed: five programs, alpha independent of tint |
| `reuseCount` was logged but not asserted | Low | Renamed to `readAfterWriteCount`; the graph test asserts the first value implicitly through order |

The audit is still author-side: it used the same repository and the same model
family, and it cannot satisfy T031.

## Rerun checklist

| Check | Result |
|---|---|
| Debug configure/build | exit 0 |
| Debug CTest | 17/17, 0 failed |
| Release build/CTest | exit 0, 17/17 |
| Contract validator and self-test | exit 0; 3 headers and 11 public tests scanned |
| Q1b fixture validator and self-test | exit 0; altered values rejected |
| `git diff --check` | exit 0 |
| Deliberate mutations | seven mutation slices, each failed as intended, restored |

## Verdict

The Q1b implementation is ready for an independent reviewer to inspect and
rerun. The author's checks do not close T030 or T031, do not prove a real
device-removal path, and do not enable the debug layer or GPU-based validation.
Full Q1 remains blocked on those components.
