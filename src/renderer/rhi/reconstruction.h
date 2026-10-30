#pragma once

// Non-proprietary reconstruction and off path (spec 004 T055, T086).
//
// Scene and world-GUI layers arrive at the render extent and are reconstructed
// to the display extent before the HUD is composed. This unit owns the
// provider-free half: the off path (identity when the extents match) and a
// documented spatial upscale. The optional proprietary provider is a separate
// module, and it is never required here.
//
// The ordering rule is checked by the consumer, not this unit: the display
// overlay is composed after reconstruction and is never an input to it. A
// caller that feeds an overlay into `ReconstructRgba` is making a composition
// mistake that the composition plan already refuses.
//
// No native type, handle or synchronization value appears here.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>

namespace doom3 {
namespace rhi {

enum class ReconstructionMode {
    // Identity: render and display extents match. The non-proprietary path is
    // always usable in this mode.
    Off = 0,
    // Bilinear spatial upscale from the render extent to the display extent.
    SpatialUpscale,
};

const char* ToString(ReconstructionMode value);

struct ReconstructionDesc {
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
};

struct ReconstructionOptions {
    // The caller asked for the proprietary display-stage provider. It is never
    // required: when it is unavailable the portable path still runs.
    bool preferProvider = false;
    bool providerAvailable = false;
};

struct ReconstructionPlan {
    Status status = Status::Ok;
    std::string error;
    ReconstructionMode mode = ReconstructionMode::Off;
    bool identity = true;
    // Always false: this path needs no proprietary component.
    bool providerRequired = false;
    // The caller asked for the provider and it is not available. The plan still
    // succeeds with the portable path.
    bool providerFellBack = false;
};

// A zero extent, a display extent smaller than the render extent in either
// axis, or equal extents combined with the upscale request is InvalidArgument.
Status PlanReconstruction(const ReconstructionDesc& desc, const ReconstructionOptions& options,
                          ReconstructionPlan* plan, std::string* error);

// CPU reference used by the fixture. `input` and `output` are tightly packed
// RGBA floats, straight alpha. Off requires equal extents and copies texel for
// texel; SpatialUpscale is bilinear with clamped edges. A non-finite input
// sample is refused rather than propagated.
Status ReconstructRgba(const float* input, uint32_t inputWidth, uint32_t inputHeight,
                       float* output, uint32_t outputWidth, uint32_t outputHeight,
                       ReconstructionMode mode, std::string* error);

}  // namespace rhi
}  // namespace doom3
