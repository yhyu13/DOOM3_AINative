# Portable profile contract (T071)

**Status**: Draft contract for spec 004 US8 / T071. It defines what a future profile must declare
and prove. It does not claim that any target, including the browser, is supported.

This contract exists so that a target is selected by an explicit id and a published capability
table, never inferred from a Windows pass, a header scan or a shader translation. It does not
cancel the browser roadmap in `specs/005-web-renderer/research.md`; it states the evidence that
roadmap requires before a support claim.

## 1. Profile identity

A profile must be named and selected explicitly:

| Field | Requirement |
|---|---|
| `targetId` | Stable id, for example `win11-amd64` or `webgpu-browser`. A missing or different id is an UNSUPPORTED result. |
| `backend` | Backend implementation and version, not just the host OS. |
| `browser` / `driver` | Named browser build or driver/adapter identity where applicable. |
| `contractVersion` | Version of this contract the profile was evaluated against. |
| `evidence` | Path to the target's own executed evidence record. |

A native D3D12 pass is not evidence for `webgpu-browser`. A browser page that only loads is not
evidence for a renderer profile.

## 2. Result vocabulary

Every capability row and every check uses exactly one of these results:

| Result | Meaning | May support a claim? |
|---|---|---|
| `PASS` | Executed on the named target and met the declared requirement. | Yes, for that row only. |
| `FALLBACK` | The preferred capability is absent; the named fallback was selected and must be exercised or explicitly listed as not exercised. | Only when the fallback itself is recorded. |
| `UNSUPPORTED` | The target cannot provide a required capability or the target identity is wrong. The profile must refuse the target. | No. |
| `NOT RUN` | The check did not execute. A specific reason is mandatory. | No. A skipped check is never a pass. |
| `FAIL` | Executed and did not meet the requirement. | No. |

A profile's overall status is `READY`, `DEGRADED` or `UNSUPPORTED`:

- `READY`: every required row passed and no quality fallback was selected.
- `DEGRADED`: every required row passed but at least one quality fallback was selected; the
  fallback and its limits are published.
- `UNSUPPORTED`: at least one required row failed or is missing, or the target identity is wrong.

## 3. Required limits

These rows are required. A missing or short value is `UNSUPPORTED`; it is never silently lowered.
`at-least` means the advertised value must be greater than or equal to the requirement; `at-most`
means the advertised value must be less than or equal to the requirement.

| Limit | Requirement | Direction | Why |
|---|---:|---|---|
| `maxBufferSize` | 268435456 (256 MiB) | at-least | chunked staging and uploads |
| `maxStorageBufferBindingSize` | 134217728 (128 MiB) | at-least | GI/reflection buffers must fit or be split |
| `maxTextureDimension2D` | 8192 | at-least | display and offscreen targets |
| `maxBindGroups` | 4 | at-least | frame / view-light / material / per-draw split |
| `maxSampledTexturesPerShaderStage` | 16 | at-least | seven interaction textures plus headroom |
| `maxStorageBuffersPerShaderStage` | 8 | at-least | budgeted compute jobs |
| `maxUniformBuffersPerShaderStage` | 12 | at-least | classic path uniform blocks |
| `maxUniformBufferBindingSize` | 65536 (64 KiB) | at-least | per-light and per-draw packing |
| `minUniformBufferOffsetAlignment` | 256 | at-most | uniform ring slots are 256-byte aligned |
| `maxVertexBuffers` | 8 | at-least | one interleaved DOOM 3 vertex stream |
| `maxVertexAttributes` | 16 | at-least | position/ST/normal/tangents/colour |
| `maxColorAttachments` | 8 | at-least | G-buffer plus auxiliary targets |
| `maxColorAttachmentBytesPerSample` | 32 | at-least | 28-byte G-buffer plus headroom |
| `maxComputeInvocationsPerWorkgroup` | 256 | at-least | compute tile sizes |

The executable table is `web/profile/capability.mjs` (`REQUIRED_LIMITS`). The unit tests fail when
this document and that table drift apart.

## 4. Optional features and explicit fallbacks

An optional feature that is absent produces a `FALLBACK` row. A `quality` fallback degrades the
profile; an `info` fallback is recorded but does not by itself change the overall status.

| Feature | Fallback id | Impact | Declared behaviour when absent |
|---|---|---|---|
| `timestamp-query` | `cpu-frame-timing` | info | use CPU frame timing; make no GPU performance claim |
| `texture-compression-bc` | `bc-transcode-required` | quality | transcode BC textures before upload; no BC asset is exercised today |
| `depth-clip-control` | `no-clip-plane` | info | DOOM 3 does not require clip planes; alphaNotch is shader-side |
| `float32-filterable` | `nearest-or-half-float` | info | use nearest filtering or a half-float format |

These fallbacks are declared, not proven. Each one must be exercised on its own target before it
can be counted as preserved behavior.

## 5. Required host pieces for `webgpu-browser`

The browser profile owns these pieces and tests them with its own harness under `tests/webgpu/`:

| Piece | Contract |
|---|---|
| Adapter/device | Request an adapter and device explicitly; record adapter identity, limits and features; record a hidden `0x0` adapter id without using it for selection. |
| Capability table | Emit the rows in sections 3 and 4, with an overall `READY`/`DEGRADED`/`UNSUPPORTED` status. |
| VFS/file abstraction | Async, mount-based, typed misses and traversal refusal; game data is `NOT RUN` unless a legal user-supplied mount exists; pak parsing is an explicit `UNSUPPORTED` until implemented. |
| Cooperative main loop | One host frame callback, fixed 60 Hz tic, bounded catch-up, pause/resume, no synchronous infinite loop. |
| Canvas/swapchain | Configure once, clamp the drawing buffer to `maxTextureDimension2D` and a declared DPR fallback, acquire a frame texture, and remember that WebGPU has no explicit present call. |
| Input bridge | Keyboard/mouse state with pointer lock requested only from a user gesture; focus loss clears state. |
| Audio bridge | Web Audio stays locked until a user gesture; a refused or still-suspended context is `NOT RUN`/`BLOCKED`, not a pass. |
| Pinned-browser harness | Explicit browser path or `WGPU_BROWSER`; pinned major; nonce echo; per-check report; screenshot only as a local artifact; no auto-substitution of another browser. |

The current implementation is `web/profile/**`; the harness is
`tests/webgpu/run-profile.mjs` with `tests/webgpu/browser-pin.json`.

## 6. Explicitly outside this profile

The following are unsupported for `webgpu-browser` and must be reported as such if requested:
ray tracing / acceleration structures, the native neural upscaler, bindless binding arrays,
multiple queues, sparse resources, resource aliasing, depth bounds, polygon mode,
fixed-function alpha test and texgen, L8/LA8/A8 formats, guaranteed BC, and native provider
interop. Naming them here is a refusal, not an implementation.

## 7. Evidence requirements

A profile may be claimed only when it has:

1. a published capability table for the named target version;
2. target-specific build/shader tests;
3. a **real-scene** run on that target (spec 004 T074), with the scene, inputs, captures and
   any unsupported cases recorded;
4. an independent review that did not author the profile (spec 004 T075).

A procedural fixture proves host plumbing only. It does not satisfy the real-scene requirement,
and a desktop compile or shader translation is not a browser pass. When legal game data is absent,
the real-scene row is `NOT RUN` with that exact reason.

## 8. Current status (2026-10-30)

- `targetId: webgpu-browser` host slice: implemented under `web/profile/**`.
- Capability table, VFS, cooperative loop, canvas/swapchain, input/audio bridge, procedural scene
  and pinned-browser harness: unit-tested and exercised in the pinned browser.
- Real-scene acceptance (T074): `NOT RUN` because no legal DOOM 3 game data is present in this
  repository; retail pk4/pk3 assets are EULA-bound and were not supplied.
- Independent profile review (T075): `NOT RUN` at the time of writing; the author does not write
  the approval.
- Therefore: **no browser support claim is made.** The Windows profile and the D3D12 flagship are
  unaffected.
