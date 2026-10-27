# Design model: Quality-first rendering

**Specification**: [spec.md](./spec.md)

**Research basis**: [the reviewed renderer research](../003-render-modernization/research.md)

**Status**: Design contract, not evidence that the structures or backend exist.

## Identity and ownership

| Entity | Fields / relationships | Validation and lifecycle |
|---|---|---|
| Capability profile | Backend identity, adapter class, format/feature capabilities, enabled checks, fallback reasons | Probe the actual device. A software adapter is labeled separately; advertised capability is not execution evidence. |
| Render frame | Monotonic rendered-frame identity, render/display extents, submission fence values, ordered views | Main-view history advances once per rendered main frame, not per simulation tick, shadow or subview. |
| View identity | World/map generation, view kind, stable instance identity, render and display extents, camera/projection and jitter | Main, mirror, camera and shadow views have separate histories. Invalidate when identity or convention changes. |
| Scene packet | Stable geometry/material/light identities, generation, immutable frame-owned data, resource references, explicit ordering/scissor/projection policy | No pointer into frame-recycled storage is used as persistent identity. Packets do not expose Windows or D3D12 types. |
| GPU resource | Backend-neutral descriptor, resource generation, permitted usages, current state per relevant subresource, state journal, last submitted use | Caller-visible handles are invalidated on destruction. The backend retains actual resources until all recorded uses retire. Q1a explicitly records `RENDER_TARGET` for the clear and `COPY_SOURCE` for the transfer. |
| Binding allocation | Layout, resource/view references, descriptor range and last-use fence | An in-flight range is immutable and cannot be recycled before its last use completes. |
| Command submission | Queue, allocator/list ownership, recorded resource accesses, submitted/aborted status, fence signal | A recorded use is **in flight** from submission until its fence value is signalled. An allocator, upload page, resource or descriptor may be reset/reused only when its last GPU use has completed. An aborted or never-submitted command list discards its retained references on reset; that reset is not a GPU retirement and must not satisfy a fence wait. First implementation uses one direct queue. |
| Readback result | Caller-owned byte buffer or explicitly bounded mapped view, row pitch/stride, producing fence, validity state | Q1a returns caller-owned bytes after the producing fence signals. If a mapped view is exposed, it is valid only from fence signal until unmap/reset; the backend documents and tests that window. |
| Frame resource | Scene colour, shader-readable depth, motion, validity/mask, overlay and optional material attachments | Extents, format, sample count and access are explicit. Overlay is not part of the reconstruction/provider scene input. |
| Geometry history | Stable world/entity/surface identity and generation, previous rendered transform, previous posed positions or verified previous pose, validity reason | Snapshot private skinned positions before shared cache/geometry can be overwritten or deleted; commit current main pose once after extraction. |
| Provider evaluation | Provider/backend/version/authorization, resource contract, input and output states, history generation, status | Provider code cannot invisibly alter graph ownership. No provider is assumed present in the default build. |
| Evidence record | Build/source/data identities, effective configuration, scene/view, adapter and driver, command, exit result, captures, metrics, tolerance, exclusions and review | PASS requires the actual check. Missing prerequisites produce SKIPPED or NOT RUN, not success. |

## Frame and resource lifecycle

1. Probe capabilities and create a device; do not silently select an unsupported path.
2. Acquire a frame slot only after its last submission fence has completed. Create or acquire frame-owned and persistent resources with explicit intended usages.
3. Extract immutable scene/view data and determine whether prior rendered history is valid. Subviews do not commit main history.
4. Compile/record declared resource accesses and barriers; execute scene, reconstruction and overlay passes in dependency order. Native-provider calls, if any, declare resource inputs/outputs and invalidate or restore affected cached state.
5. Submit, signal a monotonically increasing fence and associate every referenced object/allocation with that submission. If a recorded command list is abandoned before submission, reset it and release only its recorded references; it has no fence and cannot retire GPU work.
6. Commit the main-view history once, after current private pose capture. A cut, teleport, level or generation change, resolution/mode change, topology change or backend/provider switch makes incompatible history invalid.
7. Reclaim allocators, descriptor ranges, uploads and resources only after the corresponding completed fence. On resize or shutdown, retire all references to the affected resources before recreation/release. A release while a fence is still outstanding is a deliberate test case, not a valid normal-path shortcut; the test asserts the outstanding value at release.

Aliasing and multiple queues are deferred until their synchronization and lifetime tests exist; a C++ object's lexical lifetime alone is not evidence of GPU retirement.

## Motion and composition contract

- Internal motion is **previous unjittered UV minus current unjittered UV**, top-left origin. Provider adapters convert sign, scale, jitter and depth conventions to the actual authorized SDK contract.
- Current and previous positions are from rendered frames, not assumed simulation ticks. Camera, object transform and posed geometry all contribute as appropriate.
- Validity/disocclusion is separate from the vector: zero is a valid stationary vector and cannot stand in for missing history.
- Static, rigid, skinned and viewmodel classes have numeric reprojection fixtures. Particles, transparency, deformations, reflections and topology changes have explicit correspondence or rejection/mask policies.
- The world GUI is scene content. The display HUD/overlay is a separate display-resolution composition after reconstruction and any authorized display-stage provider.

## Scope of the first slice

The first data-independent slice is an incremental subset of this model, not the full graph, engine adapter or playable renderer. Its public test seam and exact supported operations are fixed in [contracts/rhi-foundation.md](./contracts/rhi-foundation.md); unimplemented properties remain future tasks. A standalone fixture and a software-adapter pass do not prove game parity, hardware performance, GPU-based validation, DXR or DLSS.
