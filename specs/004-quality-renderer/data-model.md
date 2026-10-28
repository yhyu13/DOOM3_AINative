# Design model: Quality-first rendering

**Specification**: [spec.md](./spec.md)

**Research basis**: [the reviewed renderer research](../003-render-modernization/research.md)

**Status**: Design contract, not evidence that the structures or backend exist.

## Identity and ownership

| Entity | Fields / relationships | Validation and lifecycle |
|---|---|---|
| Capability profile | Backend/target identity, adapter class, requested/advertised/enabled features, format/use combinations, limits, execution status and explicit fallback reason | Probe the actual device; validate every requested operation against enabled features and limits. A software adapter is labeled separately; advertised capability is not execution evidence. No exact browser feature floor is assumed. |
| Tier / extension lease | Core or named native extension, backend/device identity, authorized operation, resource/subresource accesses, initial/final state, queue, synchronization and lease window | Native handles are absent from core entities. An opt-in native bridge retains resources and invalidates/restores cached state; reject an unavailable extension and do not infer provider permission from interop. |
| Shader build record | Source origin/hash and permissions, inputs/includes, compiler/validator identity/version, target/profile/options, entry points, binding layout, commands, stage outputs/diagnostics and target execution | Synthetic or otherwise authorized source only; unresolved translation/distribution permission blocks that operation. Native compilation is not browser validation. |
| Operation result | Operation/stage, target/device, status and stable error category, diagnostic, optional explicit fallback decision | Async failures belong to the request/submission. Unsupported does not silently change backend or rendering mode; the caller chooses any separately implemented fallback. |
| Render frame | Monotonic rendered-frame identity, render/display extents, ordered views and submission identities | Main-view history advances once per rendered main frame, not per simulation tick, shadow or subview. Backend-private fence values are not part of this model. |
| View identity | World/map generation, view kind, stable instance identity, render and display extents, camera/projection and jitter | Main, mirror, camera and shadow views have separate histories. Invalidate when identity or convention changes. |
| Scene packet | Stable geometry/material/light identities, generation, immutable frame-owned data, resource references, explicit ordering/scissor/projection policy | No pointer into frame-recycled storage is used as persistent identity. Packets do not expose Windows or D3D12 types. |
| GPU resource | Device and resource generation, backend-neutral extent/format/subresources, permitted usages, declared accesses, backend state journal and last submitted use | Validate views and usages against the enabled profile and conservative usage scopes in the RHI contract: entire render pass, each compute dispatch, whole buffers and underlying texture subresources. Destroy invalidates the client handle; recorded/in-flight references retain backing storage. Q1a records `RENDER_TARGET` for clear and `COPY_SOURCE` for transfer, privately to its backend. |
| Binding allocation | Layout entries (group index, binding index, type, access, stage visibility, minimum range/alignment, count), resource/views, allocation and last-use token | Pair is unique per pipeline layout; validate group and binding indices, per-group/per-stage counts, offsets, usages and enabled limits. Native mapping must be deterministic and collision-free. An in-flight allocation is immutable and cannot be recycled before backend-proven end of its last submitted use. |
| Command submission | Device, logical queue, recording ownership, declared resource accesses/dependencies, submitted/aborted status, completion observation and separate retirement state | Retain recorded references until reset or submission. Submission is in flight until the backend proves all submitted uses have ended; a successful completion token may prove retirement, but failure, cancellation of observation or device-loss notification alone does not. An allocator, upload page, resource or descriptor is not reused or released while GPU use may remain. An aborted or never-submitted list has no token and cannot retire submitted work. First implementation uses one direct queue. |
| Readback request/result | Source/subresource, staging allocation, producing token, async mapping/copy result, extent/format and CPU stride; caller-owned bytes or bounded mapped view | Completion of the producing submission alone is not an immediate mapping guarantee. Q1a returns caller-owned bytes after its producing submission completes and the copy is available (the D3D12 backend's private fence proves completion). If a mapped view is exposed, define and test the exact mapping-to-unmap/reset window. Account separately for staging pitch and padding; failure/cancellation publishes no incomplete bytes. |
| Frame resource | Scene colour, shader-readable depth, motion, validity/mask, overlay and optional material attachments | Extents, format, sample count and access are explicit. Overlay is not part of the reconstruction/provider scene input. |
| Geometry history | Stable world/entity/surface identity and generation, previous rendered transform, previous posed positions or verified previous pose, validity reason | Snapshot private skinned positions before shared cache/geometry can be overwritten or deleted; commit current main pose once after extraction. |
| Provider evaluation | Provider/backend/version/authorization, resource contract, input and output states, history generation, status | Provider code cannot invisibly alter graph ownership. No provider is assumed present in the default build. |
| Evidence record | Build/source/data identities, effective configuration, scene/view, adapter and driver, command, exit result, captures, metrics, tolerance, exclusions and review | PASS requires the actual check. Missing prerequisites produce SKIPPED or NOT RUN, not success. |

## Frame and resource lifecycle

1. Explicitly select a backend/adapter and request features/limits; probe and create a device or return an actionable unsupported/error result. Choose a fallback explicitly, never silently.
2. Acquire a frame slot only after its last submission completes. Create or acquire frame-owned and persistent resources with explicit intended usages and valid descriptors.
3. Extract immutable scene/view data and determine whether prior rendered history is valid. Subviews do not commit main history.
4. Validate and record declared resource accesses, binding visibility, pass dependencies and logical ordering; the backend lowers necessary barriers. Execute scene, reconstruction and overlay passes in dependency order. Native-provider calls, if authorized, use the separately negotiated lease and invalidate or restore affected cached state.
5. Submit and associate a monotonic per-queue completion token with every referenced object/allocation. If a recording is abandoned before submission, reset it and release only its recorded references; it has no token and cannot retire GPU work. Completion and readback mapping are asynchronous in the portable contract; browser clients yield rather than wait synchronously.
6. Commit the main-view history once, after current private pose capture. A cut, teleport, level or generation change, resolution/mode change, topology change or backend/provider switch makes incompatible history invalid.
7. Reclaim allocators, descriptor ranges, uploads and resources only after backend-proven end of all corresponding submitted uses. On resize or shutdown, retire all references to the affected resources before recreation/release. A client release while work is still outstanding invalidates the handle but defers backend destruction; Q1a deliberately tests this, the public seam observing only an incomplete completion token and the D3D12 hook asserting the outstanding fence values. Failure, cancellation or loss is reported separately and is not retirement evidence. If retirement cannot be proved after loss, quarantine the affected allocations from reuse until a backend-defined, documented teardown guarantees their use has ended; otherwise report cleanup incomplete. Neither teardown nor loss manufactures successful completion or readback.

Aliasing and multiple queues are deferred until their synchronization and lifetime tests exist; a C++ object's lexical lifetime alone is not evidence of GPU retirement.

## Motion and composition contract

- Internal motion is **previous unjittered UV minus current unjittered UV**, top-left origin. Provider adapters convert sign, scale, jitter and depth conventions to the actual authorized SDK contract.
- Current and previous positions are from rendered frames, not assumed simulation ticks. Camera, object transform and posed geometry all contribute as appropriate.
- Validity/disocclusion is separate from the vector: zero is a valid stationary vector and cannot stand in for missing history.
- Static, rigid, skinned and viewmodel classes have numeric reprojection fixtures. Particles, transparency, deformations, reflections and topology changes have explicit correspondence or rejection/mask policies.
- The world GUI is scene content. The display HUD/overlay is a separate display-resolution composition after reconstruction and any authorized display-stage provider.

## Scope of the first slice

The first data-independent slice is an incremental subset of this model, not the full graph, engine adapter or playable renderer. Its public test seam and exact supported operations are fixed in [contracts/rhi-foundation.md](./contracts/rhi-foundation.md); unimplemented properties remain future tasks. A standalone fixture and a software-adapter pass do not prove game parity, hardware performance, GPU-based validation, DXR or DLSS.
