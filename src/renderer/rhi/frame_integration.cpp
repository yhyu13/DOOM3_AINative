#include "rhi/frame_integration.h"

namespace doom3 {
namespace rhi {

namespace {

bool IsZero(FrameExtent extent) {
    return extent.width == 0 || extent.height == 0;
}

}  // namespace

MotionValidity ReasonForEvent(FrameEvent event) {
    switch (event) {
        case FrameEvent::Cut:
        case FrameEvent::Teleport:
        case FrameEvent::LevelLoad:
            return MotionValidity::Reset;
        case FrameEvent::ProviderSwitch:
            return MotionValidity::ProviderSwitch;
    }
    return MotionValidity::NoHistory;
}

std::vector<ComposeLayer> ComposeLayersFromQueue(const SceneCommandQueue& queue,
                                                 FrameExtent render,
                                                 FrameExtent display) {
    (void)render;
    (void)display;
    std::vector<ComposeLayer> layers;
    for (const SceneCommand& command : queue.commands()) {
        if (command.kind != SceneCommandKind::BeginView) {
            continue;
        }
        ComposeLayer layer;
        layer.name = std::to_string(command.view.identity);
        // The declared extent is taken from the stream, not from the
        // integration, so a view that declares the wrong size is refused by the
        // plan instead of being silently coerced.
        layer.extent = FrameExtent{command.view.width, command.view.height};
        if (command.view.overlay) {
            layer.kind = LayerKind::DisplayOverlay;
        } else if (command.view.worldGui) {
            layer.kind = LayerKind::WorldGui;
        } else {
            layer.kind = LayerKind::Scene;
        }
        layers.push_back(layer);
    }
    return layers;
}

Status FrameIntegration::Initialize(const FrameIntegrationDesc& desc,
                                    std::string* error) {
    if (IsZero(desc.render) || IsZero(desc.display)) {
        if (error != nullptr) {
            *error = "frame integration needs non-zero render and display "
                     "extents";
        }
        initialized_ = false;
        return Status::InvalidArgument;
    }
    desc_ = desc;
    history_.Clear();
    poses_.Clear();
    motion_.Resize(desc.display.width, desc.display.height);
    stats_ = FrameIntegrationStats{};
    initialized_ = true;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

bool FrameIntegration::NotifyEngineEvent(FrameEvent event) {
    if (!initialized_) {
        return false;
    }
    const MotionValidity reason = ReasonForEvent(event);
    if (reason == MotionValidity::NoHistory) {
        return false;
    }
    history_.InvalidateAll(reason);
    ++stats_.eventsApplied;
    return true;
}

Status FrameIntegration::Resize(FrameExtent render, FrameExtent display,
                                std::string* error) {
    if (!initialized_) {
        return Status::NotReady;
    }
    if (IsZero(render) || IsZero(display)) {
        if (error != nullptr) {
            *error = "resize needs non-zero render and display extents";
        }
        return Status::InvalidArgument;
    }
    desc_.render = render;
    desc_.display = display;
    history_.InvalidateAll(MotionValidity::Resize);
    // Reallocating advances the per-pixel generation, so stale samples cannot
    // survive a resize as a valid zero.
    motion_.Resize(display.width, display.height);
    ++stats_.eventsApplied;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

bool FrameIntegration::CommitView(const RenderedViewCommit& commit) {
    if (!initialized_) {
        return false;
    }
    if (commit.displayOverlay) {
        ++stats_.displayViewsRefused;
        return false;
    }
    if (commit.records.empty()) {
        return false;
    }
    if (!commit.posedPositions.empty()) {
        if (!poses_.CommitPose(commit.kind, commit.poseIdentity,
                               commit.poseGeneration, commit.posedPositions)) {
            return false;
        }
    }
    if (!history_.CommitRendered(commit.kind, commit.identity,
                                 RenderedViewState{commit.camera,
                                                   commit.records})) {
        return false;
    }
    ++stats_.viewsCommitted;
    return true;
}

MotionValidity FrameIntegration::Previous(ViewKind kind,
                                          const ViewIdentity& identity,
                                          RenderedViewState* previous) const {
    return history_.Previous(kind, identity, previous);
}

bool FrameIntegration::CommitPrivatePose(
    ViewKind kind, uint32_t surfaceId, uint32_t generation,
    const std::vector<MotionVec3>& posedPositions) {
    return poses_.CommitPose(kind, surfaceId, generation, posedPositions);
}

MotionValidity FrameIntegration::PreviousPrivatePose(
    ViewKind kind, uint32_t surfaceId, uint32_t generation,
    std::vector<MotionVec3>* positions) const {
    return poses_.PreviousPose(kind, surfaceId, generation, positions);
}

bool FrameIntegration::StorePerPixelMotion(uint32_t x, uint32_t y,
                                           const MotionSample& sample) {
    PerPixelMotionSample stored;
    if (sample.validity == MotionValidity::Valid) {
        stored.x = static_cast<float>(sample.vector.du);
        stored.y = static_cast<float>(sample.vector.dv);
        stored.valid = true;
    } else {
        // The validity is separate from the vector: an invalid sample keeps
        // no usable vector and is never turned into a valid zero.
        stored.valid = false;
    }
    if (!motion_.Store(x, y, stored)) {
        ++stats_.motionWritesRefused;
        return false;
    }
    if (stored.valid) {
        ++stats_.motionSamplesStored;
    } else {
        ++stats_.motionSamplesInvalid;
    }
    return true;
}

PerPixelMotionSample FrameIntegration::LoadPerPixelMotion(uint32_t x,
                                                          uint32_t y) const {
    return motion_.At(x, y);
}

PostReconstructionPlan FrameIntegration::PlanQueue(
    const SceneCommandQueue& queue) const {
    return PlanPostReconstruction(ComposeLayersFromQueue(queue, desc_.render,
                                                         desc_.display),
                                  desc_.render, desc_.display);
}

Status FrameIntegration::ExecuteFrame(SceneAdapter* adapter,
                                      SceneCommandQueue* queue,
                                      SceneExecuteResult* result,
                                      std::string* error) {
    if (!initialized_) {
        return Status::NotReady;
    }
    if (adapter == nullptr || queue == nullptr || result == nullptr) {
        if (error != nullptr) {
            *error = "execute frame needs an adapter, a queue and a result";
        }
        return Status::InvalidArgument;
    }

    const std::vector<ComposeLayer> layers =
        ComposeLayersFromQueue(*queue, desc_.render, desc_.display);
    const PostReconstructionPlan plan =
        PlanPostReconstruction(layers, desc_.render, desc_.display);
    if (plan.status != Status::Ok) {
        if (error != nullptr) {
            *error = plan.error;
        }
        return plan.status;
    }

    // The Q2 adapter executes FIFO, so the declaration order must already be
    // the display order. Refusing here means the stream is not consumed and a
    // HUD-before-scene frame cannot be drawn even once.
    for (uint32_t position = 0; position < plan.order.size(); ++position) {
        if (plan.order[position] != position) {
            if (error != nullptr) {
                *error = "declared view " +
                         std::to_string(plan.order[position]) +
                         " belongs at position " + std::to_string(position) +
                         ": a display overlay must follow the scene and world "
                         "GUI";
            }
            return Status::InvalidArgument;
        }
    }

    std::vector<uint32_t> declaredIds;
    declaredIds.reserve(layers.size());
    for (const SceneCommand& command : queue->commands()) {
        if (command.kind == SceneCommandKind::BeginView) {
            declaredIds.push_back(command.view.identity);
        }
    }

    const Status executed = adapter->Execute(queue, result, error);
    if (executed != Status::Ok) {
        return executed;
    }
    if (result->viewIds.size() != layers.size()) {
        if (error != nullptr) {
            *error = "executed view count " +
                     std::to_string(result->viewIds.size()) + " does not match " +
                     std::to_string(layers.size()) + " declared views";
        }
        return Status::InvalidArgument;
    }
    for (uint32_t position = 0; position < plan.order.size(); ++position) {
        if (result->viewIds[position] != declaredIds[plan.order[position]]) {
            if (error != nullptr) {
                *error = "executed view order differs from the post-"
                         "reconstruction plan";
            }
            return Status::InvalidArgument;
        }
    }
    if (result->overlays != plan.displayOverlays) {
        if (error != nullptr) {
            *error = "executed overlay count " +
                     std::to_string(result->overlays) + " does not match the " +
                     std::to_string(plan.displayOverlays) + " planned";
        }
        return Status::InvalidArgument;
    }
    stats_.overlaysOrdered += plan.displayOverlays;
    return Status::Ok;
}

void FrameIntegration::InvalidateView(ViewKind kind,
                                      const ViewIdentity& identity,
                                      MotionValidity reason) {
    history_.Invalidate(kind, identity, reason);
}

void FrameIntegration::InvalidateAll(MotionValidity reason) {
    history_.InvalidateAll(reason);
}

}  // namespace rhi
}  // namespace doom3
