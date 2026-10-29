# Q7a: field-to-WebGPU mapping for the portable RHI surface

**Status**: Design and conformance evidence for spec 004 task T088 (Q7a). This document and
`evidence-q7a.md` are the Q7a gate output. They do **not** claim that a WebGPU backend exists, that
the header compiles for `wasm32`, or that Q2/Q7 are satisfied. Q2 remains blocked until the
Emscripten-encoder leg listed under [NOT RUN](#not-run-and-limits) actually executes.

**Authority**: the three-tier contract in [contracts/rhi-foundation.md](./contracts/rhi-foundation.md).
The portable core may only name operations that have a WebGPU-shaped lowering; anything without one
belongs to the D3D12-private tier or a native extension. This document is the semantic counterpart of
the lexical header gate: the gate catches D3D12 nouns, this mapping catches a portable-looking field
that no browser target can implement.

**Scope**: every public declaration of `src/renderer/rhi/rhi.h` as it exists on `main` at the time of
this run. `scripts/validate-webgpu-mapping.py` parses the header and fails if a declaration is
missing, a row is stale, a status is unknown, or a non-`mapped` row carries no justification.

Status vocabulary:

| Status | Meaning |
|---|---|
| `mapped` | A direct WebGPU operation implements the field with the same observable contract. |
| `mapped-with-gap` | Implementable, but only after a documented adaptation such as promise tracking, a de-pitch copy, or an error scope. |
| `unsupported` | No WebGPU operation exists. The portable core must reject the request or the backend must keep the state itself; a silently fabricated value is a contract violation. |

### What this gate cannot decide

The validator is a **structural completeness gate**. It proves that every declaration has a row, that
no row is stale, that statuses and check references use the allowed vocabulary, that gap rows carry a
non-placeholder reason, and that the check table still names every required check. It cannot tell
whether a status is *true*: flipping a genuinely `unsupported` row to `mapped` with a plausible
sentence would pass it. Check references are also not validated semantically, so a row can cite the
wrong check. Those are review items, recorded in `review-q7a.md`, not machine gates. The lexical
header gate has the complementary limit documented in the contract: it catches declared spellings and
identifier-embedded forms, not every native synonym (`CommittedResource`, `CmdQueue`,
`GPUDescriptorHandle`, `stagingPitch`), a transitive include, or a header placed outside the scan roots.

## Mapping table

| Declaration | WebGPU operation | Status | Conformance check | Justification |
|---|---|---|---|---|
| `AdapterClass` | Derived from `GPUAdapterInfo.isFallbackAdapter` and the vendor string; WebGPU exposes no adapter-class enum | mapped-with-gap | C1 | The class is inferred, so a driver that reports neither a fallback flag nor a known vendor cannot be classified without a caller-visible heuristic |
| `AdapterClass.Hardware` | `requestAdapter()` then require `adapter.info.isFallbackAdapter === false` | mapped | C1 | Observed: `intel` / `xe-lpg`, `isFallbackAdapter=false` |
| `AdapterClass.Software` | `requestAdapter({ forceFallbackAdapter: true })`; a null result is an actionable `Unsupported` | mapped-with-gap | — | No software WebGPU adapter was exposed on this host, so this leg is NOT RUN and is not counted as a pass |
| `AdapterInfo` | `GPUAdapterInfo` plus `GPUSupportedLimits` | mapped | C1 | `info` is optional in the spec; an empty object is legal |
| `AdapterInfo.adapterClass` | Inference from the same fields as `AdapterClass` | mapped-with-gap | C1 | Same inference gap as `AdapterClass` |
| `AdapterInfo.dedicatedVideoMemory` | No WebGPU field or limit reports dedicated memory size | unsupported | — | A backend must report `0` and mark the capability unsupported; inventing a value from `maxBufferSize` would be a false claim |
| `AdapterInfo.deviceId` | WebGPU reports no numeric PCI device id | unsupported | — | `GPUAdapterInfo.device` is a driver-defined string, not an id; a numeric id cannot be produced |
| `AdapterInfo.name` | `GPUAdapterInfo.description`, falling back to `vendor` plus `architecture` | mapped-with-gap | C1 | Chrome 156 returned an empty `description`; the fallback is required for a non-empty name |
| `AdapterInfo.vendorId` | `GPUAdapterInfo.vendor` is a lowercase string such as `intel`, not a PCI id | mapped-with-gap | C1 | Requires a documented string-to-id table; an unknown vendor maps to `0` and must not be guessed |
| `ClearReadbackDesc` | `GPUTextureDescriptor` for a `width x height` RGBA8 texture plus a copy layout | mapped-with-gap | C6 | The descriptor itself is direct; the row semantics are not (see `ClearReadbackDesc.rows`) |
| `ClearReadbackDesc.height` | `GPUExtent3D.height` and `rowsPerImage` | mapped | C6 | |
| `ClearReadbackDesc.rows` | No per-row clear exists: `clearValue` is one value per attachment. Use `queue.writeTexture` with a packed per-row buffer, or a fullscreen draw that reads `@builtin(position)` | mapped-with-gap | C6 | The Q1a per-row pattern is preserved by `writeTexture` in this spike; a "clear" call has no per-row form |
| `ClearReadbackDesc.width` | `GPUExtent3D.width` and `bytesPerRow` before de-pitching | mapped | C6 | |
| `ClearRow` | Four UNORM8 channels of one row | mapped-with-gap | C6 | No WebGPU type; it is a row of texel bytes |
| `ClearRow.alpha` | Alpha byte of the row texel, or `clearValue.a` for a uniform clear | mapped | C6 | |
| `ClearRow.blue` | Blue byte of the row texel, or `clearValue.b` for a uniform clear | mapped | C6 | |
| `ClearRow.green` | Green byte of the row texel, or `clearValue.g` for a uniform clear | mapped | C6 | |
| `ClearRow.red` | Red byte of the row texel, or `clearValue.r` for a uniform clear | mapped | C6 | |
| `Device` | `GPUDevice` | mapped | C1 | Obtained from `adapter.requestDevice()` |
| `Device.Initialize` | `requestAdapter()` then `requestDevice()`, inside a `pushErrorScope('validation')` to classify the failure | mapped | C1 | Rejections and null adapters become `Unsupported`; validation errors become `InvalidArgument` |
| `Device.IsComplete` | `queue.onSubmittedWorkDone()` or the resolution of `mapAsync`; WebGPU exposes no pollable fence | mapped-with-gap | C8 | A poll becomes a cached promise state; there is no synchronous "is it done" query, and no way to observe a single submission separately from the queue |
| `Device.IsInitialized` | Backend-local state | mapped | C1 | No GPU operation |
| `Device.Readback` | `mapAsync(READ)` on a `MAP_READ` buffer, then copy `getMappedRange()` into caller memory and `unmap()` | mapped | C2 | Observed byte-exact `[1,2,3,5,8,13,21,34,55,89,144,233,0,127,128,255]` |
| `Device.Shutdown` | Await outstanding `onSubmittedWorkDone`, then `device.destroy()`; drop references | mapped-with-gap | C8 | `destroy()` is device-wide and immediate; proving retirement first depends on the tracked promises |
| `Device.SubmitClearReadback` | `createCommandEncoder`, begin a render pass with a `clearValue` (uniform) or a row shader, `copyTextureToBuffer`, `queue.submit` | mapped-with-gap | C6 | The submission is direct; the per-row pattern needs the `rows` adaptation |
| `Device.TryRelease` | No per-submission release exists in WebGPU | unsupported | — | The refusal semantics must be backend bookkeeping over tracked promises. A backend that simply calls `destroy()` would violate the contract by tearing down other submissions |
| `Device.adapter` | `GPUAdapterInfo` snapshot taken at `requestDevice` time | mapped | C1 | |
| `DeviceOptions` | `GPURequestAdapterOptions` | mapped | C1 | |
| `DeviceOptions.adapterClass` | `forceFallbackAdapter: true` for software, absent or `false` for hardware | mapped-with-gap | C1 | See `AdapterClass.Software`: the requested class may be unavailable |
| `DeviceOptions.adapterIndex` | WebGPU returns one adapter per request; no enumeration or index exists | unsupported | — | A non-zero index must return `Unsupported`, not silently use adapter 0 |
| `ReadbackResult` | Mapped-range copy into caller-owned bytes | mapped | C2 | |
| `ReadbackResult.adapter` | `GPUAdapterInfo` at readback time | mapped | C1 | |
| `ReadbackResult.height` | `GPUExtent3D.height` of the copy | mapped | C6 | |
| `ReadbackResult.pixels` | `new Uint8Array(getMappedRange()).slice()` before `unmap()` | mapped | C2 | The slice is what makes the bytes caller-owned; returning the mapped view would violate the contract |
| `ReadbackResult.rowStride` | Tightly packed `width * 4`, while WebGPU requires `bytesPerRow % 256 == 0` for texture copies | mapped-with-gap | C6 | A backend must de-pitch row by row; for a 4-pixel RGBA8 row that is a 16x staging amplification, which the spike makes visible |
| `ReadbackResult.width` | `GPUExtent3D.width` of the copy | mapped | C6 | |
| `ReleaseStatus` | Backend-local release bookkeeping; WebGPU has no release result | unsupported | — | The enum must stay portable and be produced by the backend, never derived from a GPU result |
| `ReleaseStatus.InvalidSubmission` | Local token validation against the device's token table | mapped | — | Purely local |
| `ReleaseStatus.RefusedOutstanding` | `onSubmittedWorkDone()` has not resolved for that token's submission | mapped-with-gap | — | The queue-wide promise requires an ordered per-submission chain; two submits in flight cannot be distinguished by WebGPU itself |
| `ReleaseStatus.Released` | Drop the recording and staging references after their promise resolves | mapped-with-gap | — | Nothing is released on the GPU timeline; only JavaScript references drop |
| `Status` | Aggregate of promise rejection, `GPUError` from an error scope, and `device.lost` | mapped-with-gap | E | There is no status object; the classification is a backend decision that must be consistent |
| `Status.DeviceLost` | `device.lost` resolving, with reason `destroyed` or `unknown` | mapped | C8 | Observed reason `destroyed` after `destroy()` |
| `Status.Error` | Any uncaptured or scoped `GPUError` that is not validation or out-of-memory | mapped-with-gap | E | The mapping depends on scopes being pushed around every call |
| `Status.InvalidArgument` | `GPUValidationError` from a `pushErrorScope('validation')` / `popErrorScope()` pair | mapped | E | An uncaptured validation error is itself a failure: `E-uncaptured` must stay PASS |
| `Status.NotReady` | A pending promise (`mapAsync`, `onSubmittedWorkDone`) | mapped | C2 | |
| `Status.Ok` | The promise resolved with no scoped error | mapped | C1 | |
| `Status.OutOfMemory` | `GPUOutOfMemoryError` from an error scope | mapped | — | Not exercised: inducing real OOM is outside a bounded spike, so this row is mapping-only |
| `Status.Unsupported` | `requestAdapter()` returning null, `requestDevice()` rejecting, or a required feature missing from `adapter.features` | mapped | C1 | |
| `Submission` | Backend-allocated token mapped to an ordered `onSubmittedWorkDone` promise | mapped-with-gap | C8 | WebGPU has no submission object |
| `Submission.completionToken` | Backend counter; never a GPU value | mapped-with-gap | — | The token is opaque by contract; WebGPU cannot supply it, so the backend must |
| `Submission.id` | Backend counter | mapped | — | Purely local |
| `ToString` | Local string table | mapped | — | No GPU operation |

## Conformance checks

Run by `node tests/webgpu/run-conformance.mjs` in the browser named in `evidence-q7a.md`. Each check
compares a readback against literals written in `tests/webgpu/conformance.html`; no expected value is
derived from a readback.

| Check | What it proves | Observed on the recorded host |
|---|---|---|
| C1 | Adapter and device creation, adapter identity | `intel` / `xe-lpg`, `isFallbackAdapter=false` |
| C2 | Buffer write and `mapAsync(READ)` byte-exact round trip | 16 literals equal |
| C3 | One `GPUBindGroup` reused across two compute dispatches (immutable binding) | `[1,4,7,10]` both times |
| C4 | One render pass with two colour attachments (MRT) | A=`[64,128,191,255]`, B=`[255,0,128,64]` |
| C5 | One compute dispatch over 64 u32 elements | first eight `[1,4,7,10,13,16,19,22]` |
| C6 | `copyTextureToBuffer` at `bytesPerRow=256` for a 16-byte row, plus `mapAsync`, padding untouched, packed offset rejected | rows 0 and 3 exact, padding `0xAA` |
| C7 | Canvas surface configure, acquire, clear, commit, readback | `bgra8unorm`, pixel `[0,128,255,255]` |
| C8 | `destroy()` resolves `device.lost` with reason `destroyed` | `destroyed` |
| N1 | A one-unit mutation of the expected compute result is rejected | mismatch at byte 20 |
| N2 | Packed-offset control derived from the C6 readback: row 1 is only correct at offset 256, not at offset 16. It does not attempt an invalid packed-pitch copy, because that would raise a validation error and defeat `E` | packed offset rejected |
| N3 | A live device's `lost` promise does not resolve, so C8 is not vacuous | pending after 200 ms |
| E | No uncaptured validation or out-of-memory error during the whole run | count 0 |

## Gaps and deltas

These are contract-relevant, not implementation trivia. Each one changes what a WebGPU backend has to
do, and therefore what the portable core may promise.

- **G1 — "clear" is not a WebGPU clear.** `GPURenderPassColorAttachment.clearValue` is one value for
  the whole attachment. The Q1a per-row distinct pattern needs `writeTexture` (used here) or a draw
  that reads the fragment position. The portable contract must keep saying "produce these row bytes",
  never "call clear".
- **G2 — 256-byte row pitch.** Every texture-to-buffer copy requires `bytesPerRow % 256 == 0`. The
  core's `rowStride` is tightly packed, so a WebGPU backend must de-pitch. For the 4-pixel Q1a row
  that is 16 bytes of payload per 256 bytes staged; the spike proves both that the padding is not
  payload and that reading row 1 at the packed offset is wrong (N2).
- **G3 — completion is queue-wide and promise-only.** There is no fence value and no polling. A
  per-submission token is possible only as backend bookkeeping over an ordered chain of
  `onSubmittedWorkDone()` promises. `TryRelease` refusing while outstanding remains implementable,
  but only if the backend keeps that chain; `device.destroy()` is not a substitute.
- **G4 — adapter identity is thinner than the native one.** `vendorId`, `deviceId`,
  `dedicatedVideoMemory` and `adapterIndex` have no honest WebGPU source. The backend must report
  `Unsupported` for an index request and must not fabricate memory or PCI ids.
- **G5 — errors are scoped, not returned.** `Status.InvalidArgument` is only obtainable by wrapping
  each operation in `pushErrorScope('validation')`. A backend that relies on `uncapturederror` alone
  cannot attribute a failure to the request that caused it.
- **G6 — device loss has two reasons.** `destroyed` is deliberate teardown; `unknown` is a real loss.
  The contract's quarantine rule applies to `unknown`: outstanding work cannot be proven retired, so
  allocations must not be reused until a documented teardown.
- **G7 — the header is portable but unproven on `wasm32`.** A lexical scan and this mapping do not
  compile the header for WebAssembly, and a browser JavaScript spike is not a C++ encoder. That gap is
  the NOT RUN leg below.
- **G8 — five core declarations have no WebGPU-shaped operation, so the header is wider than the
  contract's core rule.** `AdapterInfo.deviceId`, `AdapterInfo.dedicatedVideoMemory` and
  `DeviceOptions.adapterIndex` are adapter facts that WebGPU does not expose; `ReleaseStatus` and
  `Device.TryRelease` are client-side lifetime bookkeeping with no GPU counterpart. The contract says a
  field with no WebGPU-shaped operation belongs to the private tier or a native extension, and the
  header currently expresses no capability signal for "this backend cannot answer". A WebGPU backend
  must therefore return `Unsupported` for the index and report `0`/empty for the identity fields, which
  is behaviour the header does not yet describe. Remediation task T089 owns the header/contract
  reconciliation; it blocks any Q7/WebGPU claim but not Q1b on D3D12.

## NOT RUN and limits

- **Emscripten encoder leg: NOT RUN.** T088 requires the same checks in a named encoder
  implementation: Dawn native, or a pinned browser plus the Emscripten WebGPU library. This host has
  no `emcc`, no `emsdk` installation, and no Dawn build, and this session did not download a
  toolchain. The executed browser checks therefore prove that the WebGPU operations exist and behave
  as mapped in a real implementation; they do **not** prove that the C++ header compiles or marshals
  through Emscripten. Q2 stays blocked on this leg, exactly as T088 requires.
- **Software adapter: NOT RUN.** `requestAdapter({ forceFallbackAdapter: true })` returned no adapter
  in the probe; the recorded C1 run is hardware only. A software-adapter run is a functional check,
  not performance evidence, and its absence is not a pass.
- **Out-of-memory classification: mapping only.** No OOM was induced.
- **Performance, presentation pacing, resize and multi-adapter behaviour: not measured.** The spike
  is a conformance probe, not a renderer.
- **No WebGPU backend exists** in this repository, and nothing here changes `src/renderer/**`.

To close the remaining leg once a toolchain is authorized, record its version, licence, acquisition
path and storage first, then run the same twelve checks in the encoder build and add its output to
`evidence-q7a.md`. The browser harness stays the reference for expected values.
