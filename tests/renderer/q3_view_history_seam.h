#pragma once

// PROPOSED portable seam for Q3 T043/T045 - stable rendered-view history.
//
// STATUS: proposal, not production. This header is test-owned on purpose: the
// branch that adds these tests owns tests/renderer/** only, and T043/T045 name
// src/renderer/rhi/view_history.cpp as the production location. When that
// slice lands it must:
//   1. move this header to src/renderer/rhi/view_history.h with the same shape;
//   2. implement it in src/renderer/rhi/view_history.cpp;
//   3. delete tests/renderer/q3_unimplemented_seam_stub.cpp and point
//      tests/renderer/CMakeLists.txt at the production source.
// Until then the red tests in tests/renderer/view_history_test.cpp and
// tests/renderer/history_reset_test.cpp fail at runtime against the stub; that
// red state is recorded in specs/004-quality-renderer/evidence-q3-tests.md.
//
// Contract:
//  * History is the last state that was ACTUALLY RENDERED for one view
//    identity. A simulation tick that was not rendered must not commit, and a
//    mirror, camera or shadow view has its own slot.
//  * The lookup key is (ViewKind, instanceId). World identity, extent,
//    topology, entity generation, provider generation and view generation are
//    compared against the stored record and only ever produce an explicit
//    reason, never a silent reuse.
//  * A rejected lookup never yields a stored state: the caller's output is
//    left untouched, so stale history cannot be consumed as a valid zero.
//  * A rejected lookup never yields a usable vector: it returns a reason, and
//    the caller must not consume the previous state.
//
// This header names no backend type, handle or synchronization value.

#include "rhi/motion_history.h"
#include "rhi/skinned_history.h"

#include <cstdint>
#include <vector>

namespace doom3 {
namespace rhi {

// Stable identity of one rendered view.
//
//  worldId     level/map identity. A different world is a reset (level load).
//  instanceId  the engine's stable view instance. Two security cameras differ.
//  generation  advances when the view's camera/projection convention changes.
//  topology    advances when the view's geometry identity set changes.
//  entity      advances when an entity slot is reused or a model respawns.
//  provider    advances on a renderer/provider switch.
//  width/height the rendered extent. A different extent is a resize.
struct ViewIdentity {
    uint32_t worldId = 0;
    uint32_t instanceId = 0;
    uint32_t generation = 0;
    uint32_t topology = 0;
    uint32_t entity = 0;
    uint32_t provider = 0;
    uint32_t width = 1280;
    uint32_t height = 720;
};

// One rendered state of one view: the camera used for the draw plus the
// points that were rendered in that view. A point without a rendered record
// is a point the view did not draw.
struct RenderedViewState {
    CameraState camera;
    std::vector<RenderRecord> records;
};

class RenderedViewHistory {
public:
    // Commits the state of a view that was actually drawn. Call exactly once
    // per rendered frame of that view, after the frame used the previous
    // state. Never call it for a simulation tick that was not rendered.
    // An empty record list is rejected and commits nothing.
    bool CommitRendered(ViewKind kind, const ViewIdentity& identity,
                        const RenderedViewState& state);

    // Copies the last rendered state of the same view to `previous`.
    //
    // Reasons, in precedence order (highest first). The first failing check
    // is the reported reason:
    //   1. ProviderSwitch  provider generation differs from the stored record
    //   2. Reset           an explicit reset is pending (cut, teleport) or the
    //                      world identity differs (level load)
    //   3. Resize          an explicit resize is pending or the extent differs
    //   4. Topology        the topology generation differs
    //   5. EntityReuse     the entity generation differs
    //   6. Generation      the view generation differs
    //   7. NoHistory       this view has never been committed
    //   8. Valid           `previous` now holds the usable rendered state
    //
    // On anything but Valid the output is not modified.
    MotionValidity Previous(ViewKind kind, const ViewIdentity& identity,
                            RenderedViewState* previous) const;

    // Drops the stored state for this view. `reason` must be Reset, Resize,
    // Topology, EntityReuse or ProviderSwitch; any other value is a no-op.
    // The reason is reported by Previous until the next commit.
    void Invalidate(ViewKind kind, const ViewIdentity& identity,
                    MotionValidity reason);

    // Drops every stored view for a cut, teleport, level load or provider
    // switch. A view that was never committed still reports NoHistory.
    void InvalidateAll(MotionValidity reason);

    void Clear();
};

}  // namespace rhi
}  // namespace doom3
