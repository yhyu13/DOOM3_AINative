# Contract: RHI foundation and first executable seam

**Status**: Design for incremental implementation; no operation is available until implemented and tested.

**Scope**: A backend-neutral RHI boundary and a data-independent D3D12 vertical slice. This does not define a game renderer or claim completion of the full Q1 gate.

## Public boundary

The first executable boundary is a small C++ API that lets a client:

1. Select an explicitly named adapter class (hardware or software), create a device and observe a meaningful unsupported/error result.
2. Create an explicitly described 2D colour resource and record a clear plus a transfer to a readable CPU result.
3. Submit the commands, wait for their fence and inspect the resulting pixels. The caller owns neither native COM objects nor backend descriptor internals.
4. Release client handles and shut down safely, including after incomplete initialization; GPU-owned objects, command allocators and upload/readback storage remain alive until their submitted work completes.

Exact function names are implementation choices; tests must use the same public boundary that an eventual renderer client uses. The backend-specific implementation owns D3D12/DXGI types. Do not add speculative operations merely to make the interface look complete.

## First vertical-slice acceptance

- Configure/build the standalone fixture without the engine target copy, game data or proprietary NVIDIA SDK. The installed Windows SDK and Visual Studio C++ tools are required.
- On an available explicitly selected adapter, clear a synthetic RGBA8 texture using a **per-row distinct pattern** (at least two rows differ, and the row index is recoverable from the expected bytes), read it back, and compare every row against independently specified byte values. Expected values are derived by the test from the pattern definition, never from the renderer's output. Allow only the documented UNORM rounding tolerance.
- Account for D3D12 readback row pitch. A tightly packed CPU output stride of `width * 4` is valid; that is not the same as the GPU staging footprint's row pitch. Include three negative controls that must fail: (a) using `width * 4` as the staging row stride when the returned row pitch is larger, (b) an off-by-one source-row offset, and (c) repeating row 0 for every row. If a control is genuinely inapplicable on a conforming implementation, record it as NOT APPLICABLE with the reason; do not count it as a pass.
- Declare and implement the Q1a state sequence rather than leaving it implicit: the colour resource is created for render-target use, cleared in `RENDER_TARGET`, transitioned to `COPY_SOURCE`, and the readback buffer is in `COPY_DEST`. The backend keeps a state journal for the resource and a test asserts the recorded sequence. A general public transition API remains Q1b work, but the Q1a implementation must perform the correct transitions.
- Submit at least two independent submissions. At least one release/reset attempt must occur while its submission fence is still unsignalled: assert `GetCompletedValue() < signalValue` at the moment of release. A test that waits idle before every release is not evidence. Use a controlled gate or a deliberately long command so the outstanding condition is deterministic, and include a negative control that shows the test fails if the wait-idle shortcut is used.
- Readback results cross the public boundary as caller-owned bytes copied after the producing fence signals. If an implementation instead exposes a mapped view, the public contract must state a single validity window and a test must prove it; a raw pointer with unspecified lifetime is not acceptable.
- Report hardware versus software adapter and the check result. Attempt each adapter class that is present; at least one adapter must actually execute the clear/readback test. Adapter unavailability is SKIPPED/NOT RUN, not PASS. A software run is a functional check, not hardware performance evidence.
- Debug layer and GPU-based validation: the fixture must expose opt-in requests, attempt enablement, capture messages into the retained test log, and record absence/failure distinctly. Reporting "unavailable" without attempting enablement is not evidence. Debug-layer and GPU-based validation are separate from functional execution and must not be marked passing without actually running.
- Retain the full CTest log (`-V` or `--output-junit`/`--output-log`), including successful test output, adapter identity and executed/skipped checks. `--output-on-failure` alone retains only failures.
- Failures expose an actionable stage/error; device creation alone is not counted as a successful rendering or readback test.

This is **Q1a**. Passing it does not complete Q1, User Story 2 or the programme's success criteria.

## Foundation growth required for full Q1

After Q1a, extend the same boundary by independently tested slices for draw/pipeline and shader compilation, compute dispatch, multiple colour attachments, shader-readable depth, motion/validity and overlay resources, a general public transition/UAV-ordering API (beyond Q1a's internal clear/copy transitions), frame slots, resize, persistent history/reset and diagnostics. Before declaring Q1 complete, run zero-unwaived-error debug-layer and GPU-based-validation checks on an environment where those checks actually execute.

The frame graph consumes declared resource accesses and pass dependencies; it is not a facade that hides synchronization or assumes an aliased resource is safe. Device/queue/resources are not tied to deferred shading or an optional provider. Native-provider access is a restricted, explicitly declared boundary with resource states and state-cache invalidation/restoration, and is deferred until an actual authorized SDK contract is available.

## Compatibility and integration boundary

The planned engine adapter consumes the ordered view/set/copy/present command stream, not a wrapper for every historical `gl*` call. Commands can flush within a frame for screenshots/copies as well as at the normal end of frame. Classify display overlays, world GUIs and other 2D views by semantic purpose and preserve ordering/capture behavior; do not assume every 2D view can be moved to the final HUD pass.

The standalone component lives in this repository. Changes to engine-owned code follow the fork/submodule review path; target copies are scratch and are not a durable delivery mechanism. The patch exporter reproduces modified files only: added and deleted files are manifest rows with no hunks, and the apply/verify scripts now fail closed on them. Deliver such files through the fork/submodule path instead of relying on the target-copy patch.

## Later renderer invariants

- Logical resource references carry explicit use and lifetime; native pointer values are not stable entity identities.
- Per-view history uses previous rendered state and a separate validity result. Main-view history cannot be committed by a mirror, shadow or security-camera view.
- A private previous skinned-pose copy survives the engine's shared dynamic-model cache updates and deletion.
- Scene reconstruction/provider input excludes the native-resolution display overlay; world GUI treatment remains a tested scene policy.
- Optional neural evaluation never becomes a hard-coded RHI operation or a dependency of the default renderer.
- Any future backend must implement and test its own capability/fallback table. Backend-neutral descriptions are not evidence of a browser or Vulkan port.
