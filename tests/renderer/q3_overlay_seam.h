#pragma once

// PROPOSED portable seam for Q3 T047 - scene/world-GUI versus display-
// resolution HUD ordering.
//
// STATUS: proposal, not production. This header is test-owned on purpose: the
// branch that adds the test owns tests/renderer/** only, and T047 names
// src/renderer/rhi/overlay_composition.cpp as the production location. When
// that slice lands it must:
//   1. move this header to src/renderer/rhi/overlay_composition.h;
//   2. implement it in src/renderer/rhi/overlay_composition.cpp;
//   3. delete tests/renderer/q3_unimplemented_seam_stub.cpp and point
//      tests/renderer/CMakeLists.txt at the production source.
// Until then tests/renderer/overlay_composition_test.cpp fails at runtime
// against the stub; that red state is recorded in
// specs/004-quality-renderer/evidence-q3-tests.md.
//
// Contract:
//  * The scene and world-GUI layers are rendered at the render extent, and
//    they are the only reconstruction inputs. They are resampled when the
//    reconstruction output is larger than the render extent.
//  * A display overlay (HUD) is composed AFTER reconstruction, at the display
//    extent, texel for texel. It is never resampled with the scene and never
//    feeds reconstruction or motion history.
//  * Exactly one scene layer is required. Every layer's declared extent must
//    match its role: scene and world GUI at the render extent, display
//    overlays at the display extent, so a misdeclared HUD is refused instead
//    of silently blurred.
//
// This header names no backend type, handle or synchronization value.

#include "rhi/frame_resources.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

struct FrameExtent {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct ComposeLayer {
    LayerKind kind = LayerKind::Scene;
    std::string name;
    FrameExtent extent;
};

struct PostReconstructionPlan {
    Status status = Status::Ok;
    std::string error;
    // Layer indices in display order: scene, world GUI, display overlays.
    std::vector<uint32_t> order;
    // Number of leading entries that are reconstruction input (scene + world
    // GUI). Display overlays are always excluded.
    uint32_t reconstructionInputs = 0;
    uint32_t displayOverlays = 0;
};

// Validates the declared layers and produces the display order. Requires
// exactly one scene layer, a non-zero render extent and a non-zero display
// extent. `InvalidArgument` carries an actionable message.
PostReconstructionPlan PlanPostReconstruction(
    const std::vector<ComposeLayer>& layers, FrameExtent renderExtent,
    FrameExtent displayExtent);

// Maps one display-space texel to the layer's source texel. Scene and
// world-GUI layers are resampled from the render extent; a display overlay is
// mapped one to one. Returns false for an out-of-range texel or a zero
// extent, so a caller cannot read a fabricated sample.
bool MapDisplayTexel(const ComposeLayer& layer, FrameExtent renderExtent,
                     FrameExtent displayExtent, uint32_t x, uint32_t y,
                     uint32_t* sourceX, uint32_t* sourceY);

}  // namespace rhi
}  // namespace doom3
