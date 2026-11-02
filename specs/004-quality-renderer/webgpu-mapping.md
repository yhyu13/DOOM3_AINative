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
| `AdapterFact` | Bitmask vocabulary for optional adapter facts; a clear bit is the portable "unavailable" signal | mapped | C1 | No WebGPU type is needed; the bits describe what the adapter info actually contains |
| `AdapterFact.VendorId` | Set only by a backend that can produce a numeric vendor id | mapped | C1 | The D3D12 backend sets it; a WebGPU-shaped backend cannot |
| `AdapterFact.DeviceId` | Set only by a backend that can produce a numeric device id | mapped | C1 | Same split as `VendorId` |
| `AdapterFact.DedicatedMemory` | Set when the backend can report a memory size, including a real 0 | mapped | C1 | Distinguishes a reported zero (software adapter) from an unavailable fact |
| `AdapterInfo.reportedFacts` | The only authority for which adapter fields are meaningful | mapped | C1 | Required addition from T089; tests assert `0x7` on D3D12 and a browser backend would report `0` |
| `AdapterInfo` | `GPUAdapterInfo` plus `GPUSupportedLimits` | mapped | C1 | `info` is optional in the spec; an empty object is legal |
| `AdapterInfo.adapterClass` | Inference from the same fields as `AdapterClass` | mapped-with-gap | C1 | Same inference gap as `AdapterClass` |
| `AdapterInfo.dedicatedVideoMemory` | Defined as meaningful only when `reportedFacts` includes `DedicatedMemory`; WebGPU exposes no memory size | mapped-with-gap | C1 | A browser backend leaves the bit clear and the value 0; a client that needs the fact must treat a clear bit as unavailable, not as a zero-byte budget (contract §Optional adapter facts) |
| `AdapterInfo.deviceId` | Defined as meaningful only when `reportedFacts` includes `DeviceId`; WebGPU reports no numeric PCI device id | mapped-with-gap | C1 | A browser backend sets no bit and leaves the value 0; `GPUAdapterInfo.device` is a driver-defined string, so it must not be parsed into an id |
| `AdapterInfo.name` | `GPUAdapterInfo.description`, falling back to `vendor` plus `architecture` | mapped-with-gap | C1 | Chrome 156 returned an empty `description`; the fallback is required for a non-empty name |
| `AdapterInfo.vendorId` | Defined as meaningful only when `reportedFacts` includes `VendorId`; WebGPU exposes a lowercase vendor string such as `intel`, not a PCI id | mapped-with-gap | C1 | A browser backend sets no bit and leaves 0; mapping the string to an id by guesswork is a contract violation |
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
| `Device.TryRelease` | Core release rule lowered to the backend's completion observation (`onSubmittedWorkDone`), not a GPU operation | mapped-with-gap | C8 | WebGPU has no per-submission release: the backend keeps the token-to-promise bookkeeping and must not call `destroy()` (that would tear down other submissions) |
| `Device.adapter` | `GPUAdapterInfo` snapshot taken at `requestDevice` time | mapped | C1 | |
| `DeviceOptions` | `GPURequestAdapterOptions` | mapped | C1 | |
| `DeviceOptions.adapterClass` | `forceFallbackAdapter: true` for software, absent or `false` for hardware | mapped-with-gap | C1 | See `AdapterClass.Software`: the requested class may be unavailable |
| `DeviceOptions.adapterIndex` | Core addressable index 0; a backend that cannot enumerate returns `Unsupported` for any other index | mapped-with-gap | C1 | WebGPU returns one adapter per request, so index 0 is the only portable value; silently substituting adapter 0 for index N is forbidden (contract §Optional adapter facts) |
| `ReadbackResult` | Mapped-range copy into caller-owned bytes | mapped | C2 | |
| `ReadbackResult.adapter` | `GPUAdapterInfo` at readback time | mapped | C1 | |
| `ReadbackResult.height` | `GPUExtent3D.height` of the copy | mapped | C6 | |
| `ReadbackResult.pixels` | `new Uint8Array(getMappedRange()).slice()` before `unmap()` | mapped | C2 | The slice is what makes the bytes caller-owned; returning the mapped view would violate the contract |
| `ReadbackResult.rowStride` | Tightly packed `width * 4`, while WebGPU requires `bytesPerRow % 256 == 0` for texture copies | mapped-with-gap | C6 | A backend must de-pitch row by row; for a 4-pixel RGBA8 row that is a 16x staging amplification, which the spike makes visible |
| `ReadbackResult.width` | `GPUExtent3D.width` of the copy | mapped | C6 | |
| `ReleaseStatus` | Result of the core release rule, produced by the backend from its completion bookkeeping | mapped-with-gap | C8 | Not a GPU result; the contract defines the three values and forbids deriving them from a native value |
| `ReleaseStatus.InvalidSubmission` | Local token validation against the device's token table | mapped | — | Purely local |
| `ReleaseStatus.RefusedOutstanding` | `onSubmittedWorkDone()` has not resolved for that token's submission | mapped-with-gap | C8 | The queue-wide promise requires an ordered per-submission chain; two submits in flight cannot be distinguished by WebGPU itself |
| `ReleaseStatus.Released` | Drop the recording and staging references after their promise resolves | mapped-with-gap | C8 | Nothing is released on the GPU timeline; only JavaScript references drop |
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
| `BufferResult` | `mapAsync(READ)` on the result buffer, then copy `getMappedRange()` into caller bytes | mapped-with-gap | C5 | A compute result buffer needs `STORAGE` for the dispatch and a separate `MAP_READ \| COPY_DST` staging buffer for readback, because WebGPU forbids a storage buffer that is also mappable |
| `BufferResult.adapter` | `GPUAdapterInfo` snapshot taken at readback time | mapped | C1 | Same availability rule as `ReadbackResult.adapter` |
| `BufferResult.bytes` | `new Uint8Array(getMappedRange()).slice()` before `unmap()` | mapped | C2 | The slice makes the bytes caller-owned; the mapped view must not escape |
| `BufferResult.elementCount` | Caller-declared element count, checked against `GPUBufferDescriptor.size / elementSize` | mapped | C5 | Pure metadata; it is the dispatch contract, not a GPU read |
| `BufferResult.elementSize` | Element size in bytes; `4` for the Q1b `u32` result | mapped | C5 | Pure metadata; no GPU operation |
| `ComputeDispatchDesc` | `beginComputePass()`, `setPipeline` / `setBindGroup` / `dispatchWorkgroups`, `copyBufferToBuffer`, `queue.submit` | mapped-with-gap | C5 | WebGPU has no single "dispatch and read back" call; the backend records the pass and the staging copy, and completion is the submission promise |
| `ComputeDispatchDesc.elementCount` | `GPUBufferDescriptor.size = elementCount * 4` for the result buffer | mapped | C5 | |
| `ComputeDispatchDesc.groupsX` | `dispatchWorkgroups(x, y, z)` X argument | mapped | C5 | |
| `ComputeDispatchDesc.groupsY` | `dispatchWorkgroups(x, y, z)` Y argument | mapped | C5 | |
| `ComputeDispatchDesc.groupsZ` | `dispatchWorkgroups(x, y, z)` Z argument | mapped | C5 | |
| `ComputeDispatchDesc.pipeline` | `GPUComputePipeline` bound with `setPipeline` | mapped | C5 | |
| `ComputePipelineDesc` | `GPUComputePipelineDescriptor` | mapped | C5 | |
| `ComputePipelineDesc.computeShader` | `compute: { module, entryPoint }` | mapped-with-gap | C5 | The core stores an opaque `ShaderModule` id; the backend resolves it to the `GPUShaderModule` it created and supplies the entry point at pipeline creation, because WebGPU takes both together |
| `Device.CreateComputePipeline` | `device.createComputePipeline()` or `createComputePipelineAsync()` | mapped-with-gap | C5 | The core passes a `ShaderModule` handle; WebGPU needs `{ module, entryPoint }` resolved at creation time |
| `Device.CreateGraphicsPipeline` | `device.createRenderPipeline()` or `createRenderPipelineAsync()` | mapped-with-gap | C4 | `pixelShader` becomes `fragment`, `vertexShader` becomes `vertex`, and the depth fields become `depthStencil`; the core's single flat descriptor has no direct one-call equivalent |
| `Device.CreateShaderModule` | `device.createShaderModule({ code, label })` | mapped-with-gap | C5 | WebGPU accepts source text and a browser backend must supply WGSL; the core's `source` is deliberately language-undefined, so a backend that cannot compile it returns `Unsupported` instead of claiming success |
| `Device.CreateSurface` | `HTMLCanvasElement.getContext('webgpu')` and `GPUCanvasContext.configure({ device, format })` | mapped-with-gap | C7 | WebGPU has no device-created surface: the canvas or platform window owns it. The core `title` has no WebGPU source, and the backend must bind the platform object rather than fabricate one |
| `Device.DestroySurface` | Drop the `GPUCanvasContext` and its size state | mapped-with-gap | C7 | WebGPU exposes no surface destruction; the canvas owner destroys the element. The backend releases its references and must report success only for a surface it owns |
| `Device.ReadbackAttachment` | `copyTextureToBuffer` from the attachment texture, then `mapAsync(READ)` | mapped-with-gap | C6 | Attachment textures need `COPY_SRC`; the copy must be de-pitched from the 256-byte `bytesPerRow`, and the attachment must still be live when the readback submission runs |
| `Device.ReadbackBuffer` | `mapAsync(GPUMapMode.READ)` on the staging buffer | mapped | C2 | The producing submission must be complete first; the core's `NotReady` is that check |
| `Device.ResizeSurface` | Set `canvas.width` / `canvas.height` and reconfigure the `GPUCanvasContext` | mapped-with-gap | C7 | WebGPU has no resize call; the canvas size is the surface size, so the backend mutates the platform object and reconfigures |
| `Device.SubmitCompute` | `beginComputePass`, dispatch, `copyBufferToBuffer` to a `MAP_READ` staging buffer, `queue.submit` | mapped-with-gap | C5 | "Submit and read back" is two GPU operations plus a staging copy; the core token is backend bookkeeping over the submit |
| `Device.SubmitDrawFrame` | Encode one render pass per `DrawPassDesc`, then `queue.submit(...)` | mapped-with-gap | C4 | WebGPU has no frame object and no ordered-pass list; the backend expands `passes[]` into sequential `beginRenderPass` calls and keeps the submission-token bookkeeping |
| `Device.SubmitSurfaceFrame` | `context.getCurrentTexture()`, a render pass with `clearValue`, `queue.submit`; the compositor presents at the end of the task | mapped-with-gap | C7 | Presentation is implicit in the web API; the backend cannot choose a present mode or a present time beyond the end-of-task rule |
| `DrawFrameDesc` | One `GPUTexture` per colour attachment, one depth texture, and a sequence of render passes | mapped-with-gap | C4 | The core's frame is an ordered pass list; WebGPU has no frame object, so the backend expands it into passes and attachment load/store decisions |
| `DrawFrameDesc.attachmentCount` | Number of colour attachment textures, 1..4 | mapped | C4 | |
| `DrawFrameDesc.depthClear` | `GPURenderPassDepthStencilAttachment.depthClearValue` | mapped | — | No conformance check exercises depth; mapping only |
| `DrawFrameDesc.depthEnabled` | Presence of a `depthStencil` attachment and pipeline depth state | mapped | — | No conformance check exercises depth; mapping only |
| `DrawFrameDesc.height` | `GPUTextureDescriptor.size.height` and `setViewport` | mapped | C4 | |
| `DrawFrameDesc.passCount` | Number of sequential `beginRenderPass` calls | mapped-with-gap | — | WebGPU has no pass-count field; the backend iterates the array, and each pass must declare the attachments it uses. Multi-pass ordering is mapping-only |
| `DrawFrameDesc.passes` | Host array of `DrawPassDesc`, looped into `beginRenderPass` calls | mapped-with-gap | — | The pointer is host-only and has no GPU form; WebGPU requires each pass to carry its own attachment list, so the backend fully expands the array. Multi-pass ordering is mapping-only |
| `DrawFrameDesc.width` | `GPUTextureDescriptor.size.width` and `setViewport` | mapped | C4 | |
| `DrawPassDesc` | `GPURenderPassDescriptor` plus the commands inside one pass | mapped-with-gap | C4 | The core's `colorIndices` / `colorClears` are parallel arrays; WebGPU's `colorAttachments` is a positional slot array and the backend must translate the indices |
| `DrawPassDesc.colorClears` | `GPURenderPassColorAttachment.clearValue` per selected slot | mapped | C4 | |
| `DrawPassDesc.colorCount` | Number of populated `colorAttachments` entries | mapped | C4 | |
| `DrawPassDesc.colorIndices` | Slot indices into `GPURenderPassDescriptor.colorAttachments` | mapped-with-gap | C4 | WebGPU's array is positional and the backend must map each core index to a slot; duplicate or out-of-range indices are a contract error the backend must reject |
| `DrawPassDesc.pipeline` | `setPipeline(GPURenderPipeline)` | mapped | C4 | |
| `GraphicsPipelineDesc` | `GPURenderPipelineDescriptor` | mapped-with-gap | C4 | One flat core descriptor maps to the nested `vertex` / `fragment` / `depthStencil` WebGPU descriptor; the pixel stage becomes `fragment` |
| `GraphicsPipelineDesc.colorAttachmentCount` | Length of `fragment.targets`, 1..4 | mapped | C4 | |
| `GraphicsPipelineDesc.depthTest` | `depthStencil.depthCompare` (`less` when enabled, `always` when disabled) | mapped | — | No conformance check exercises depth; mapping only |
| `GraphicsPipelineDesc.depthWrite` | `depthStencil.depthWriteEnabled` | mapped | — | No conformance check exercises depth; mapping only |
| `GraphicsPipelineDesc.pixelShader` | `fragment: { module, entryPoint, targets }` | mapped-with-gap | C4 | "Pixel" is WebGPU's `FRAGMENT` stage; the targets array must match `colorAttachmentCount` |
| `GraphicsPipelineDesc.sampleDepth` | Depth texture with `TEXTURE_BINDING` plus a `texture_depth_2d` and sampler in a bind group | mapped-with-gap | — | WebGPU has no pipeline field for "sample the depth attachment"; the backend must create the depth texture with the extra usage and bind it. Sampled depth is mapping-only |
| `GraphicsPipelineDesc.vertexShader` | `vertex: { module, entryPoint }` | mapped-with-gap | C4 | The core stores an opaque `ShaderModule`; the backend resolves its stage and entry point into the WebGPU stage object |
| `Pipeline` | `GPURenderPipeline` or `GPUComputePipeline`, held by the backend | mapped-with-gap | — | WebGPU has two distinct pipeline classes and no shared handle; the core id maps to a backend record tagged with its kind |
| `Pipeline.id` | Backend handle; never a GPU value | mapped | — | Purely local; zero means none |
| `ShaderModule` | `GPUShaderModule` plus its declared stage | mapped-with-gap | C5 | WebGPU's shader module carries no stage; the core handle must retain the stage separately for `GraphicsPipelineDesc` resolution |
| `ShaderModule.id` | Backend handle; never a GPU value | mapped | — | Purely local; zero means none |
| `ShaderModuleDesc` | `GPUShaderModuleDescriptor` plus the core stage | mapped-with-gap | C5 | The descriptor carries source and entry point; WebGPU takes `code` there but the entry point and stage at pipeline creation, so those are backend metadata |
| `ShaderModuleDesc.entryPoint` | `GPUProgrammableStage.entryPoint` at pipeline creation | mapped | C5 | |
| `ShaderModuleDesc.source` | `GPUShaderModuleDescriptor.code`; WGSL for a browser backend | mapped-with-gap | C5 | The core deliberately does not define the language or compiler; a WebGPU backend supplies WGSL, and a source it cannot compile is `Unsupported`, not a fabricated success |
| `ShaderModuleDesc.stage` | `GPUShaderStage` at pipeline creation (Vertex / Fragment / Compute) | mapped-with-gap | C5 | WebGPU has no stage on the module; the backend stores it and maps `Pixel` to `FRAGMENT` |
| `ShaderStage` | `GPUShaderStage` flags plus the render/compute pipeline split | mapped-with-gap | — | WebGPU uses bit flags and separate pipeline stages; the core's scoped enum is a backend-side classification |
| `ShaderStage.Compute` | `GPUShaderStage.COMPUTE` / `compute` stage | mapped | C5 | |
| `ShaderStage.Pixel` | `GPUShaderStage.FRAGMENT` / `fragment` stage | mapped | C4 | "Pixel" is named `fragment` in WebGPU |
| `ShaderStage.Vertex` | `GPUShaderStage.VERTEX` / `vertex` stage | mapped | C4 | |
| `Surface` | `GPUCanvasContext` on the web, bound to a platform canvas or window | mapped-with-gap | C7 | WebGPU has no surface object created by the device; the context is obtained from the platform object, and the backend keeps the association |
| `Surface.id` | Backend handle; never a GPU value | mapped | — | Purely local; zero means none |
| `SurfaceDesc` | Canvas or platform surface parameters | mapped-with-gap | C7 | WebGPU's surface descriptor is `configure({ device, format, alphaMode })`; title and size come from the platform object, not the GPU device |
| `SurfaceDesc.height` | `canvas.height` / `GPUTextureDescriptor.size.height` | mapped | C7 | |
| `SurfaceDesc.title` | No WebGPU operation; the canvas or DOM embedding owns the title | unsupported | — | WebGPU exposes no title. A browser backend must not fabricate one or claim it set it, so the core field is platform-only and belongs to the embedding, not the portable GPU surface |
| `SurfaceDesc.width` | `canvas.width` / `GPUTextureDescriptor.size.width` | mapped | C7 | |

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
  `dedicatedVideoMemory` and `adapterIndex` have no honest WebGPU source. Since T089 the core says so
  explicitly: `reportedFacts` leaves those bits clear and the values at 0, `adapterIndex` 0 is the only
  portable value (a native backend that enumerates may honour others; a backend that cannot must return
  `Unsupported`), and no memory size or PCI id is fabricated.
  A browser backend is therefore compliant; it is the *caller* that must treat a clear bit as
  unavailable.
- **G5 — errors are scoped, not returned.** `Status.InvalidArgument` is only obtainable by wrapping
  each operation in `pushErrorScope('validation')`. A backend that relies on `uncapturederror` alone
  cannot attribute a failure to the request that caused it.
- **G6 — device loss has two reasons.** `destroyed` is deliberate teardown; `unknown` is a real loss.
  The contract's quarantine rule applies to `unknown`: outstanding work cannot be proven retired, so
  allocations must not be reused until a documented teardown.
- **G7 — the header is portable but unproven on `wasm32`.** A lexical scan and this mapping do not
  compile the header for WebAssembly, and a browser JavaScript spike is not a C++ encoder. That gap is
  the NOT RUN leg below.
- **G8 — resolved by T089 (issue #36).** Five declarations originally had no WebGPU-shaped
  operation: `AdapterInfo.vendorId`, `AdapterInfo.deviceId`, `AdapterInfo.dedicatedVideoMemory`,
  `DeviceOptions.adapterIndex`, and `ReleaseStatus`/`Device.TryRelease`. T089 added the missing
  availability rule instead of moving the fields: `AdapterFact` plus `AdapterInfo.reportedFacts` make
  "this backend cannot report the fact" expressible, the contract gained §Optional adapter facts and
  adapter selection, and the rows above were reclassified from `unsupported` to `mapped-with-gap`. A
  browser backend now has defined behaviour: `reportedFacts = 0`, identity fields left at 0, a nonzero
  `adapterIndex` returns `Unsupported`, and release stays backend bookkeeping. Any future core field
  without a WebGPU-shaped operation needs the same treatment or belongs in `ext/`.

- **G9 — Q1b pipelines, passes and surfaces are classified, not newly exercised.** The Q1b
  declarations (`BufferResult`, `ShaderModule`, `GraphicsPipelineDesc`/`ComputePipelineDesc`,
  `DrawFrameDesc`/`DrawPassDesc`, `Surface`/`SurfaceDesc`) landed after the T088 browser run. Their
  rows classify a WebGPU-shaped lowering; they are not a rerun of C1–C8 for those paths. Multi-pass
  ordering, sampled depth, per-pass tint, surface resize/destroy and presentation pacing are
  mapping-only here. The Q1b execution evidence is D3D12 (`evidence-q1.md`), not WebGPU, and a
  WebGPU backend must rerun the checks before any of these rows is cited as observed.

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
