# Optional neural-provider boundary contract (T058)

**Status**: contract only. No provider is implemented, authorized or tested, and no SDK function,
structure, enum, version or header name appears in this document. Where the actual integration
contract is unknown, it is written as `UNKNOWN` in §7 together with the evidence that would settle
it. Writing the boundary is not permission to code against an SDK that is not in hand.

**Scope**: [spec.md](./spec.md) FR-014, FR-015, SC-006 and US6; [plan.md](./plan.md) Q5;
[tasks.md](./tasks.md) T058–T062. The capability row is `native-provider` in
[contracts/rhi-foundation.md](./contracts/rhi-foundation.md), whose §Native interop and provider
boundary this document refines rather than replaces.

## 1. What this contract decides, and what it does not

Decides:

- where provider code may live, and what may name it;
- who owns each resource the provider touches, and for how long;
- the semantic requirements on inputs, masks and reset;
- what "off" means, and that it is the reference;
- which records must exist before any provider claim.

Does not decide: the SDK's API surface, its version, whether the chosen backend is supported, the
device capability set, distribution terms, performance, or whether a DLSS 5 product exists for
D3D12 at all. Those are `UNKNOWN` (§7) and no text here may be read as a prediction of them.

## 2. Placement and tier

| Rule | Detail |
|---|---|
| Tier | `native-provider`, a native extension. Never a core operation, never an implicit device feature. |
| Header location | Provider interface headers live only under `src/renderer/ext/` (the directory does not exist yet; T059 creates it). |
| What may name it | `src/renderer/ext/**`, `src/renderer/d3d12/**` where the backend bridges the lease, and tests under `tests/renderer/ext/**` or `tests/renderer/d3d12/**`. |
| What may not name it | Public core headers under `src/renderer/rhi/`, public test units in `tests/renderer/` outside `d3d12/` and `ext/`, engine scene/material code, and every web/profile path. |
| Default build | Links, bundles and downloads nothing proprietary (FR-014). No automatic fetch at configure, build or run time. |
| Selection | Explicit request only. Absent, denied, unsupported or unauthorized resolves to `unsupported`; nothing switches automatically. |

## 3. Selection, creation and liveness

1. A provider instance exists only after an explicit request that names it, plus a recorded
   authorization (T060). A remembered preference is not an authorization.
2. Creation is separable from the default device path: the default configure, build and run must
   succeed with no provider present and no attempted download.
3. Creation failure is reported as an actionable result and leaves the frame identical to the off
   path (§8). A half-created provider must not consume inputs, write the scene target, or hold a
   lease.
4. Destruction releases every lease, provider-owned history and provider-owned resource before the
   device is destroyed; outstanding GPU work is retired first (FR-005).
5. The provider is per-device and per-renderer-instance. It is not global state, and it is not
   reachable from engine, scene or material code.

## 4. Resource ownership and lifetime

The bridge retains ownership throughout the lease and any outstanding submitted work; the recipient
must not retain a handle outside the agreed window (rhi-foundation §Native interop).

| Object | Owner | Provider access | Lifetime rule |
|---|---|---|---|
| Input colour target | Renderer frame resources | Read-only lease for one evaluation | Released before the frame slot is reused; no retention across frames |
| Input depth | Renderer frame resources | Read-only lease | Same as colour; format and read/write state declared before the lease |
| Motion vectors and validity | Renderer frame resources | Read-only lease | Must be the previous **rendered** state, not a simulation-tick delta (FR-008) |
| Provider output | Renderer-owned target at display extent | Write-only lease | The provider never allocates or presents the backbuffer |
| Upscaler history / internal state | Provider instance | Internal | Destroyed with the instance; reset per §6; never written into renderer history |
| Display overlay (HUD) | Renderer, after reconstruction | Not visible | Composed at display resolution after evaluation (FR-011) |
| Descriptors, heaps, native handles | Bridge | Lease-scoped | Never retained; actual state/ownership reported on return |
| Command submission | Single core queue | Ordered, not a new queue | Retirement through the core mechanism; no release while work is outstanding |

Rules that follow from the table:

- No resource is reused or released while submitted work still references it (FR-005).
- If a mask or output cannot be produced for a frame, the provider is skipped for that frame and the
  off path is used; a partial input set is never passed.
- Device loss during evaluation is explicit: the frame fails with the core error, the provider is
  torn down, and the off path stays usable.

## 5. Inputs (semantic requirements, no API)

Each row states what the renderer must guarantee; the concrete parameter names belong to the SDK and
are `UNKNOWN` (§7).

| Input | Requirement |
|---|---|
| Colour | Render extent, documented colour space and whether it is pre- or post-tonemap. The answer for the actual SDK is `UNKNOWN`; the record must state which was supplied. |
| Depth | Documented convention (near/far, reversed or not, linear or projection) and the buffer's accessibility at lease start. |
| Motion vectors | Previous rendered position minus current rendered position, with the sign, units (pixels or NDC) and origin documented, and the jitter offset reported separately. |
| Validity | A separate signal from zero motion. A zero vector is never evidence of valid history (FR-008). |
| Jitter | The frame's jitter offset, and the documented convention for whether the provider must apply or expect it. |
| Exposure | Documented whether exposure was applied before the evaluation; changing the convention between frames is a reset trigger. |
| Identity and generation | Stable frame/view identity plus generation, so provider history can be invalidated on the triggers in §6. |
| Render vs display extent | Both extents recorded, because the overlay is composed afterwards at display extent. |
| Overlay | Excluded from the input set. Reconstruction must not see HUD or display overlays (FR-011). |

## 6. Masks, reset and invalidation

**Masks.** The default is an opaque-only path with no mask supplied. Any reactive/transparency mask
is optional, enabled explicitly, and must have a recorded producer (renderer or authored content),
semantic, format and scale. An unspecified mask semantic means the mask is not enabled; the renderer
does not infer one.

**Reset triggers.** Provider history is invalidated on every trigger already required by FR-008, plus
the provider's own lifecycle:

- cut, teleport, level change, resize or scale change, topology change, entity reuse;
- renderer or provider switch, and the first frame after creation;
- device loss, device reset or a failed evaluation;
- exposure, jitter or colour-space discontinuity;
- a frame that skipped evaluation for any reason.

**Reset behaviour.** Reset is explicit, recorded and observable; it must not silently reuse
incompatible history. Whether reset requires recreating a provider object, and its cost, is `UNKNOWN`
(§7). No reset path may make the off path unavailable.

## 7. UNKNOWN register

Each row is a fact this repository does not have. A row is settled only by the named evidence, never
by inference from a public sample on another API, a vendor blog, or a dynamic-loading trick.

| # | Unknown | Why it blocks | What would settle it |
|---|---|---|---|
| U1 | SDK name, API surface, version, header identities | Any implementation or claim | Authorized SDK access with headers/docs and a build that consumes them |
| U2 | Whether the selected backend (D3D12) is supported at all | A sample on another API is not this backend's evidence (FR-015) | The provider's own supported-backend documentation, or a successful create/evaluate on D3D12 |
| U3 | Required resource state, ownership and initial/final state at lease boundaries | Unsafe transitions or validation errors | SDK documentation plus a validation-clean create/evaluate |
| U4 | Required queue and synchronization model (and whether a second queue is required) | Cross-queue hazard; the core contract has one queue | SDK documentation plus a measured frame |
| U5 | Exact colour input set: pre- or post-tonemap, HDR format, exposure application point | Wrong output, or a systematically dim/over-bright image | SDK documentation plus an on/off comparison at recorded exposure |
| U6 | Expected motion-vector convention (sign, units, previous vs current) | Ghosting that looks like a renderer defect | SDK documentation plus an analytic fixture with known vectors |
| U7 | Jitter coupling: who jitters, what offset the provider receives | Temporal instability | SDK documentation plus a measured static scene |
| U8 | Mask set, formats, scales and who authors each mask | Artifacts on transparency and thin geometry | SDK documentation plus captures with and without the mask |
| U9 | Reset semantics and cost; whether a device or object recreation is implied | Stalls, or stale history | SDK documentation plus a measured reset |
| U10 | Device-loss and device-removal behaviour during evaluation | Crash or a silently wrong frame | SDK documentation plus the fault-injection path already used by `rhi.q1b.device_removal` |
| U11 | Hardware capability query, minimum driver and required feature level | An unannounced runtime failure | SDK documentation plus a run on named hardware with driver identity |
| U12 | Distribution and licence terms for the combined work; whether out-of-tree dynamic loading changes them | GPLv3 exposure for this repository | Written SDK terms plus a legal review of the combined work; the loading technique alone settles nothing (FR-015) |
| U13 | Whether a DLSS 5 product exists for this backend, and what its quality/performance claims are | Any DLSS 5 claim | Vendor statement and an executed create/evaluate; no claim before |
| U14 | Cost and benefit on named hardware (frame time, VRAM, latency) | No budget can be set | Measurement on named hardware with recorded configuration |
| U15 | Whether the provider's output may be composed with a display overlay it must not see | Ordering constraint that could be violated | SDK documentation plus on/off captures with the HUD present |

## 8. Off behaviour (non-negotiable)

- **Absent is normal.** With no provider, no authorization or no supported device, the renderer uses
  the non-proprietary reconstruction path: `PlanReconstruction: Off` is the identity path and is
  byte-exact (T055, `tests/renderer/reconstruction_test.cpp`, 35 checks).
- **Off is the reference.** Every on/off comparison is against the off path, not against a
  previously captured image from another build.
- **Off is always selectable**, including when a working provider is present.
- **Off is never a fallback with a silent switch.** The selected mode is reported; an unavailable
  provider is `NOT RUN`/`unsupported`, not a quiet downgrade.
- **A provider is never a build, test, configure or run prerequisite** for any other stage.

## 9. Evidence gates before any provider claim

| Gate | Record required | Current state |
|---|---|---|
| T059 | Off/missing-provider behavioural test; clean default configure, build and run observed for attempted NVIDIA downloads (including failed/blocked attempts and child processes); dependency and artifact inspection as a separate check | **PARTIAL** — `src/renderer/ext/provider.cpp` and an 84-check off/missing-provider test pass (exit 0), and the separate artifact/dependency inspection passes. Clean default configure and build exit 0. The three-phase no-download observation is **NOT RUN** (no complete process trace or packet capture without elevation) and the default engine run is **NOT RUN** (no legal game data); see `specs/004-quality-renderer/evidence-q5.md` |
| T060 | Actual SDK access, backend, version, device/driver, authorization and distribution review | NOT RUN — `specs/004-quality-renderer/provider-clearance.md` does not exist (referenced by `traceability.md` before this revision) |
| T061 | Authorized adapter outside the default dependency path plus create/evaluate/reset test | NOT RUN |
| T062 | Selected-backend inputs, masks, reset, on/off evidence and independent distribution decision | NOT RUN |

Until every gate for a claimed backend holds, any provider statement is `NOT RUN` and no DLSS 5
support, availability or legal compatibility is claimed (FR-015, SC-006).

## 10. Non-claims

This document does not claim: that a provider can be obtained; that D3D12 is a supported backend;
that the resources above are the SDK's expected set; that masks, reset or jitter will behave as
described once the real contract is read; that the boundary compiles; or that any image quality,
performance or legal position is acceptable. It defines what a future implementation must prove and
records what is currently unknown.
