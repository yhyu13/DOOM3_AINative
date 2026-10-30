#include "rhi/overlay_composition.h"

namespace doom3 {
namespace rhi {

namespace {

bool IsZero(FrameExtent extent) {
    return extent.width == 0 || extent.height == 0;
}

bool SameExtent(FrameExtent a, FrameExtent b) {
    return a.width == b.width && a.height == b.height;
}

const char* RoleName(LayerKind kind) {
    switch (kind) {
        case LayerKind::Scene: return "scene";
        case LayerKind::WorldGui: return "world GUI";
        case LayerKind::DisplayOverlay: return "display overlay";
    }
    return "unknown";
}

bool RequireExtent(const std::vector<ComposeLayer>& layers, LayerKind kind,
                   FrameExtent expected, FrameExtent renderExtent,
                   FrameExtent displayExtent, PostReconstructionPlan* plan) {
    for (const ComposeLayer& layer : layers) {
        if (layer.kind != kind) {
            continue;
        }
        if (!SameExtent(layer.extent, expected)) {
            plan->status = Status::InvalidArgument;
            plan->error = std::string("layer '") + layer.name + "' is a " +
                          RoleName(kind) + " but declares " +
                          std::to_string(layer.extent.width) + "x" +
                          std::to_string(layer.extent.height) + "; the render "
                          "extent is " + std::to_string(renderExtent.width) +
                          "x" + std::to_string(renderExtent.height) +
                          " and the display extent is " +
                          std::to_string(displayExtent.width) + "x" +
                          std::to_string(displayExtent.height);
            return false;
        }
    }
    return true;
}

}  // namespace

PostReconstructionPlan PlanPostReconstruction(
    const std::vector<ComposeLayer>& layers, FrameExtent renderExtent,
    FrameExtent displayExtent) {
    PostReconstructionPlan plan;
    if (IsZero(renderExtent)) {
        plan.status = Status::InvalidArgument;
        plan.error = "post-reconstruction composition needs a non-zero render "
                     "extent";
        return plan;
    }
    if (IsZero(displayExtent)) {
        plan.status = Status::InvalidArgument;
        plan.error = "post-reconstruction composition needs a non-zero display "
                     "extent";
        return plan;
    }

    uint32_t sceneCount = 0;
    std::vector<uint32_t> scene;
    std::vector<uint32_t> gui;
    std::vector<uint32_t> overlay;
    for (uint32_t index = 0; index < layers.size(); ++index) {
        switch (layers[index].kind) {
            case LayerKind::Scene:
                ++sceneCount;
                scene.push_back(index);
                break;
            case LayerKind::WorldGui:
                gui.push_back(index);
                break;
            case LayerKind::DisplayOverlay:
                overlay.push_back(index);
                break;
        }
    }
    if (sceneCount != 1) {
        plan.status = Status::InvalidArgument;
        plan.error = "post-reconstruction composition requires exactly one "
                     "scene layer, found " + std::to_string(sceneCount);
        return plan;
    }
    if (!RequireExtent(layers, LayerKind::Scene, renderExtent, renderExtent,
                       displayExtent, &plan)
            || !RequireExtent(layers, LayerKind::WorldGui, renderExtent,
                              renderExtent, displayExtent, &plan)
            || !RequireExtent(layers, LayerKind::DisplayOverlay, displayExtent,
                              renderExtent, displayExtent, &plan)) {
        return plan;
    }

    plan.order.reserve(layers.size());
    plan.order.insert(plan.order.end(), scene.begin(), scene.end());
    plan.order.insert(plan.order.end(), gui.begin(), gui.end());
    plan.order.insert(plan.order.end(), overlay.begin(), overlay.end());
    plan.reconstructionInputs =
        static_cast<uint32_t>(scene.size() + gui.size());
    plan.displayOverlays = static_cast<uint32_t>(overlay.size());
    return plan;
}

bool MapDisplayTexel(const ComposeLayer& layer, FrameExtent renderExtent,
                     FrameExtent displayExtent, uint32_t x, uint32_t y,
                     uint32_t* sourceX, uint32_t* sourceY) {
    if (sourceX == nullptr || sourceY == nullptr) {
        return false;
    }
    if (IsZero(displayExtent) || x >= displayExtent.width ||
        y >= displayExtent.height) {
        return false;
    }
    if (layer.kind == LayerKind::DisplayOverlay) {
        // Composed after reconstruction at display resolution: no scaling, and
        // the render extent must not influence the mapping.
        *sourceX = x;
        *sourceY = y;
        return true;
    }
    if (IsZero(renderExtent)) {
        return false;
    }
    const uint32_t mappedX =
        static_cast<uint32_t>((static_cast<uint64_t>(x) * renderExtent.width) /
                              displayExtent.width);
    const uint32_t mappedY =
        static_cast<uint32_t>((static_cast<uint64_t>(y) * renderExtent.height) /
                              displayExtent.height);
    if (mappedX >= renderExtent.width || mappedY >= renderExtent.height) {
        return false;
    }
    *sourceX = mappedX;
    *sourceY = mappedY;
    return true;
}

}  // namespace rhi
}  // namespace doom3
