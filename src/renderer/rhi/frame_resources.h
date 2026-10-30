#pragma once

// Portable motion/history and composition resources.
//
// Validity is a separate field, never encoded as a sentinel vector: a static
// camera legitimately produces a valid zero motion vector, while a point that
// cannot be projected produces an invalid sample whose components are
// meaningless. Persistent history is bounded by the surface size and is
// invalidated by a generation counter instead of being silently reused.
//
// Composition classifies 2D layers by purpose: the scene first, world GUI over
// the scene, and the display overlay last. Only scene and world-GUI layers are
// input to reconstruction; the display overlay is excluded.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

struct Float2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Per-pixel history sample. Deliberately not named `MotionSample`:
// motion_history.h owns that name for the reprojection result, and both public
// headers must be includable in one translation unit (issue #58).
struct PerPixelMotionSample {
    float x = 0.0f;
    float y = 0.0f;
    // False means "no usable projection"; x and y must not be consumed.
    bool valid = false;
};

struct Camera {
    // Camera centre in world units on the image plane, plus a pixel focal
    // length. This is a deliberately small analytic model for fixtures.
    Float2 center;
    float focal = 1.0f;
};

// Projects one world point at a given view depth in the previous and current
// camera and returns the previous-minus-current pixel displacement. Both
// projections must land inside the frame; otherwise the sample is invalid.
inline PerPixelMotionSample ProjectMotion(const Camera& previous, const Camera& current,
                                  const Float2& world, float depth, uint32_t width,
                                  uint32_t height) {
    PerPixelMotionSample sample;
    if (!(depth > 0.0f) || width == 0 || height == 0) {
        return sample;
    }
    const float halfWidth = static_cast<float>(width) * 0.5f;
    const float halfHeight = static_cast<float>(height) * 0.5f;
    const float previousX =
        halfWidth + (world.x - previous.center.x) * previous.focal / depth;
    const float previousY =
        halfHeight + (world.y - previous.center.y) * previous.focal / depth;
    const float currentX =
        halfWidth + (world.x - current.center.x) * current.focal / depth;
    const float currentY =
        halfHeight + (world.y - current.center.y) * current.focal / depth;
    const bool inside = previousX >= 0.0f && previousX < static_cast<float>(width) &&
                        previousY >= 0.0f && previousY < static_cast<float>(height) &&
                        currentX >= 0.0f && currentX < static_cast<float>(width) &&
                        currentY >= 0.0f && currentY < static_cast<float>(height);
    if (!inside) {
        return sample;
    }
    sample.x = previousX - currentX;
    sample.y = previousY - currentY;
    sample.valid = true;
    return sample;
}

// Bounded per-pixel history. Resize and Reset both advance the generation and
// invalidate every stored sample, so a consumer cannot mistake stale data for a
// valid zero vector.
class HistoryBuffer {
public:
    void Resize(uint32_t width, uint32_t height) {
        width_ = width;
        height_ = height;
        samples_.assign(static_cast<size_t>(width) * height, PerPixelMotionSample{});
        ++generation_;
    }

    void Reset() {
        samples_.assign(samples_.size(), PerPixelMotionSample{});
        ++generation_;
    }

    bool Store(uint32_t x, uint32_t y, const PerPixelMotionSample& sample) {
        if (x >= width_ || y >= height_) {
            return false;
        }
        samples_[static_cast<size_t>(y) * width_ + x] = sample;
        return true;
    }

    PerPixelMotionSample At(uint32_t x, uint32_t y) const {
        if (x >= width_ || y >= height_) {
            return PerPixelMotionSample{};
        }
        return samples_[static_cast<size_t>(y) * width_ + x];
    }

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint64_t generation() const { return generation_; }
    bool empty() const { return samples_.empty(); }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint64_t generation_ = 0;
    std::vector<PerPixelMotionSample> samples_;
};

enum class LayerKind {
    Scene,
    WorldGui,
    DisplayOverlay,
};

struct LayerDeclaration {
    LayerKind kind = LayerKind::Scene;
    std::string name;
};

struct CompositionPlan {
    Status status = Status::Ok;
    std::string error;
    // Layer indices in display order.
    std::vector<uint32_t> order;
    // Number of leading entries that are scene-reconstruction input.
    uint32_t reconstructionInputs = 0;
};

// Validates exactly one scene layer and produces a stable display order:
// scene, then world GUI, then display overlay, preserving input order within a
// kind. Display overlays are excluded from reconstruction input.
inline CompositionPlan PlanComposition(const std::vector<LayerDeclaration>& layers) {
    CompositionPlan plan;
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
        plan.error = "composition requires exactly one scene layer, found " +
                     std::to_string(sceneCount);
        return plan;
    }
    plan.order.reserve(layers.size());
    plan.order.insert(plan.order.end(), scene.begin(), scene.end());
    plan.order.insert(plan.order.end(), gui.begin(), gui.end());
    plan.order.insert(plan.order.end(), overlay.begin(), overlay.end());
    plan.reconstructionInputs = static_cast<uint32_t>(scene.size() + gui.size());
    return plan;
}

}  // namespace rhi
}  // namespace doom3
