#pragma once

// Q3 frame integration: connects the Q2 ordered scene stream to the Q3
// rendered-frame history, private skinned poses, per-pixel motion resources and
// the post-reconstruction display-overlay order.
//
// The engine front end reports what it actually rendered through CommitView and
// reports invalidation through NotifyEngineEvent/Resize. ExecuteFrame consumes
// the Q2 SceneAdapter stream and refuses an order that would draw the display
// HUD before the scene. This unit names no backend type, handle or
// synchronization value.
//
// What this slice does not include: the engine-fork call sites
// (neo/renderer/Model_md5.cpp private pose snapshot, sink/RC_* dispatch into the
// adapter). They need a Q3 fork branch, a reviewed submodule bump with owner
// sign-off and authorized data; recorded as blocked in
// specs/004-quality-renderer/evidence-q3.md §6.

#include "rhi/engine_scene_adapter.h"
#include "rhi/frame_resources.h"
#include "rhi/overlay_composition.h"
#include "rhi/skinned_history.h"
#include "rhi/view_history.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

// Engine events that invalidate rendered history. A resize carries a new
// extent, so it goes through Resize() instead of this enum.
enum class FrameEvent : uint32_t {
    Cut,
    Teleport,
    LevelLoad,
    ProviderSwitch,
};

// Cut, Teleport and LevelLoad map to Reset; ProviderSwitch maps to
// ProviderSwitch; an unknown value maps to NoHistory, which is a no-op.
MotionValidity ReasonForEvent(FrameEvent event);

// One view this frame actually rendered. `displayOverlay` marks a display HUD,
// which is composed after reconstruction and never committed to history.
struct RenderedViewCommit {
    ViewKind kind = ViewKind::Main;
    ViewIdentity identity;
    CameraState camera;
    std::vector<RenderRecord> records;
    bool displayOverlay = false;
    // Optional private skinned pose. `poseIdentity` is the stable surface
    // identity; an empty `posedPositions` means this view has no skinned pose.
    uint32_t poseIdentity = 0;
    uint32_t poseGeneration = 0;
    std::vector<MotionVec3> posedPositions;
};

struct FrameIntegrationDesc {
    FrameExtent render;
    FrameExtent display;
};

struct FrameIntegrationStats {
    uint32_t viewsCommitted = 0;
    uint32_t displayViewsRefused = 0;
    uint32_t overlaysOrdered = 0;
    uint32_t eventsApplied = 0;
    uint32_t motionSamplesStored = 0;
    uint32_t motionSamplesInvalid = 0;
    uint32_t motionWritesRefused = 0;
};

// Builds the post-reconstruction layer description from the Q2 queue's
// BeginView records: a display overlay view at the display extent, a world-GUI
// view at the render extent, and a scene view at the render extent. Everything
// else in the queue is not a layer.
std::vector<ComposeLayer> ComposeLayersFromQueue(const SceneCommandQueue& queue,
                                                 FrameExtent render,
                                                 FrameExtent display);

class FrameIntegration {
public:
    Status Initialize(const FrameIntegrationDesc& desc, std::string* error);
    bool initialized() const { return initialized_; }
    const FrameIntegrationDesc& desc() const { return desc_; }

    // Applies an engine invalidation event to every stored view. Returns false
    // for an uninitialized integration or an unknown event.
    bool NotifyEngineEvent(FrameEvent event);

    // New render/display extents: invalidates every stored view with Resize and
    // reallocates the per-pixel motion resource at the display extent. A zero
    // extent is refused and changes nothing.
    Status Resize(FrameExtent render, FrameExtent display, std::string* error);

    // Commits one view that was actually rendered. A display overlay is
    // refused (it is composed after reconstruction). Empty records are refused.
    // The private pose, when supplied, is copied before the view commit.
    bool CommitView(const RenderedViewCommit& commit);

    MotionValidity Previous(ViewKind kind, const ViewIdentity& identity,
                            RenderedViewState* previous) const;

    bool CommitPrivatePose(ViewKind kind, uint32_t surfaceId, uint32_t generation,
                           const std::vector<MotionVec3>& posedPositions);
    MotionValidity PreviousPrivatePose(ViewKind kind, uint32_t surfaceId,
                                       uint32_t generation,
                                       std::vector<MotionVec3>* positions) const;

    // Stores one display-space per-pixel sample. A sample whose validity is not
    // Valid is stored with `valid = false` and its vector is not consumed; an
    // out-of-range coordinate is refused.
    bool StorePerPixelMotion(uint32_t x, uint32_t y, const MotionSample& sample);
    PerPixelMotionSample LoadPerPixelMotion(uint32_t x, uint32_t y) const;

    // Consumes the Q2 stream only when its declared view order equals the
    // post-reconstruction plan. A display overlay declared before the scene (or
    // any other reordering) is InvalidArgument and consumes nothing.
    Status ExecuteFrame(SceneAdapter* adapter, SceneCommandQueue* queue,
                        SceneExecuteResult* result, std::string* error);

    // The plan for the queue's declared layers, without executing it.
    PostReconstructionPlan PlanQueue(const SceneCommandQueue& queue) const;

    void InvalidateView(ViewKind kind, const ViewIdentity& identity,
                        MotionValidity reason);
    void InvalidateAll(MotionValidity reason);

    RenderedViewHistory& history() { return history_; }
    const RenderedViewHistory& history() const { return history_; }
    const SkinnedPoseHistory& poses() const { return poses_; }
    const HistoryBuffer& motion() const { return motion_; }
    const FrameIntegrationStats& stats() const { return stats_; }

private:
    FrameIntegrationDesc desc_;
    bool initialized_ = false;
    RenderedViewHistory history_;
    SkinnedPoseHistory poses_;
    HistoryBuffer motion_;
    FrameIntegrationStats stats_;
};

}  // namespace rhi
}  // namespace doom3
