// T055 non-proprietary reconstruction/off path and the T086 ordering check.
//
// Part 1 proves the off path and the spatial upscale arithmetic.
// Part 2 runs the real post-reconstruction composition plan
// (`rhi/overlay_composition.h`) around that path: scene and world GUI are the
// reconstruction inputs, the display overlay is composed after reconstruction
// at display resolution, and the same order holds when reconstruction is off.
// The provider-specific display-stage position is NOT RUN here; there is no
// authorized provider, and this fixture never pretends otherwise.

#include "q4_test_support.h"
#include "rhi/overlay_composition.h"
#include "rhi/reconstruction.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;

rhi::FrameExtent Extent(uint32_t width, uint32_t height) {
    return rhi::FrameExtent{width, height};
}

rhi::ComposeLayer Layer(rhi::LayerKind kind, const char* name, rhi::FrameExtent extent) {
    rhi::ComposeLayer layer;
    layer.kind = kind;
    layer.name = name;
    layer.extent = extent;
    return layer;
}

void RunPlan(q4_test::Runner* runner) {
    rhi::ReconstructionDesc desc;
    desc.renderWidth = 640;
    desc.renderHeight = 360;
    desc.displayWidth = 1280;
    desc.displayHeight = 720;

    rhi::ReconstructionOptions options;
    rhi::ReconstructionPlan plan;
    std::string error;
    runner->Check(rhi::PlanReconstruction(desc, options, &plan, &error) == rhi::Status::Ok,
                 "T055 upscale plan is Ok", error);
    runner->Check(plan.mode == rhi::ReconstructionMode::SpatialUpscale && !plan.identity,
                 "T055 larger display extent selects spatial upscale");
    runner->Check(!plan.providerRequired,
                 "T055 the portable path needs no proprietary provider");

    options.preferProvider = true;
    options.providerAvailable = false;
    runner->Check(rhi::PlanReconstruction(desc, options, &plan, &error) == rhi::Status::Ok,
                 "T055 missing provider still plans", error);
    runner->Check(plan.providerFellBack && !plan.providerRequired,
                 "T055 missing provider falls back explicitly");

    desc.displayWidth = 640;
    desc.displayHeight = 360;
    runner->Check(rhi::PlanReconstruction(desc, rhi::ReconstructionOptions{}, &plan, &error) ==
                     rhi::Status::Ok,
                 "T055 equal extents plan is Ok", error);
    runner->Check(plan.mode == rhi::ReconstructionMode::Off && plan.identity,
                 "T055 equal extents select the off path");

    desc.displayWidth = 320;
    runner->Check(rhi::PlanReconstruction(desc, rhi::ReconstructionOptions{}, &plan, &error) ==
                     rhi::Status::InvalidArgument,
                 "T055 a smaller display extent is refused");
    desc.displayWidth = 0;
    runner->Check(rhi::PlanReconstruction(desc, rhi::ReconstructionOptions{}, &plan, &error) ==
                     rhi::Status::InvalidArgument,
                 "T055 a zero extent is refused");
}

void RunImagePaths(q4_test::Runner* runner) {
    std::string error;
    // Off path: texel for texel, no resampling.
    float identity[16];
    for (int index = 0; index < 16; ++index) {
        identity[index] = static_cast<float>(index);
    }
    float copy[16];
    runner->Check(rhi::ReconstructRgba(identity, 2, 2, copy, 2, 2, rhi::ReconstructionMode::Off,
                                      &error) == rhi::Status::Ok,
                 "T055 off path copies", error);
    runner->Check(std::memcmp(identity, copy, sizeof(identity)) == 0,
                 "T055 off path is byte-identical");

    // 2x1 -> 4x1: source centres at -0.25, 0.25, 0.75, 1.25.
    const float horizontal[8] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float widened[16] = {};
    runner->Check(rhi::ReconstructRgba(horizontal, 2, 1, widened, 4, 1,
                                      rhi::ReconstructionMode::SpatialUpscale,
                                      &error) == rhi::Status::Ok,
                 "T055 horizontal upscale runs", error);
    runner->Near(widened[0], 0.0f, 1.0e-6f, "T055 left edge is clamped");
    runner->Near(widened[4], 0.25f, 1.0e-6f, "T055 first interior sample is 0.25");
    runner->Near(widened[8], 0.75f, 1.0e-6f, "T055 second interior sample is 0.75");
    runner->Near(widened[12], 1.0f, 1.0e-6f, "T055 right edge is clamped");

    // 2x2 -> 4x4: texel (1,1) samples 0.25/0.25 between {0,1;1,0} = 0.375.
    const float square[16] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                              1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float upscaled[64] = {};
    runner->Check(rhi::ReconstructRgba(square, 2, 2, upscaled, 4, 4,
                                      rhi::ReconstructionMode::SpatialUpscale,
                                      &error) == rhi::Status::Ok,
                 "T055 2D upscale runs", error);
    runner->Near(upscaled[(1 * 4 + 1) * 4], 0.375f, 1.0e-6f,
                "T055 bilinear centre is 0.375");

    // Artifact guards.
    float bad[4] = {0.0f, 0.0f, 0.0f, std::nanf("")};
    runner->Check(rhi::ReconstructRgba(bad, 1, 1, copy, 1, 1, rhi::ReconstructionMode::Off,
                                      &error) == rhi::Status::InvalidArgument,
                 "T055 non-finite input is refused");
    runner->Check(rhi::ReconstructRgba(identity, 2, 2, copy, 3, 3, rhi::ReconstructionMode::Off,
                                      &error) == rhi::Status::InvalidArgument,
                 "T055 off path with mismatched extents is refused");
    runner->Check(rhi::ReconstructRgba(nullptr, 2, 2, copy, 2, 2, rhi::ReconstructionMode::Off,
                                      &error) == rhi::Status::InvalidArgument,
                 "T055 null input is refused");
    runner->Check(rhi::ReconstructRgba(identity, 4, 4, copy, 2, 2,
                                      rhi::ReconstructionMode::SpatialUpscale,
                                      &error) == rhi::Status::InvalidArgument,
                 "T055 downscale is refused");
}

void RunOrdering(q4_test::Runner* runner) {
    // T086: render 640x360, display 1280x720, with a display-resolution HUD.
    const std::vector<rhi::ComposeLayer> layers = {
        Layer(rhi::LayerKind::Scene, "world", Extent(640, 360)),
        Layer(rhi::LayerKind::WorldGui, "pda", Extent(640, 360)),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", Extent(1280, 720)),
    };

    rhi::PostReconstructionPlan plan =
        rhi::PlanPostReconstruction(layers, Extent(640, 360), Extent(1280, 720));
    runner->Check(plan.status == rhi::Status::Ok, "T086 composition plan is Ok", plan.error);
    runner->Check(plan.order.size() == 3u && plan.order[0] == 0u && plan.order[1] == 1u &&
                     plan.order[2] == 2u,
                 "T086 scene, then world GUI, then display overlay");
    runner->Check(plan.reconstructionInputs == 2u,
                 "T086 the display overlay is not a reconstruction input");
    runner->Check(plan.displayOverlays == 1u, "T086 exactly one display overlay");

    // The real reconstruction plan for the same extents is the spatial upscale;
    // the overlay is composed afterwards, so it is not part of that call.
    rhi::ReconstructionDesc desc;
    desc.renderWidth = 640;
    desc.renderHeight = 360;
    desc.displayWidth = 1280;
    desc.displayHeight = 720;
    rhi::ReconstructionPlan reconstruction;
    std::string error;
    runner->Check(rhi::PlanReconstruction(desc, rhi::ReconstructionOptions{}, &reconstruction,
                                         &error) == rhi::Status::Ok,
                 "T086 reconstruction plan is Ok", error);
    runner->Check(reconstruction.mode == rhi::ReconstructionMode::SpatialUpscale,
                 "T086 reconstruction runs before the overlay");

    // Texel mapping: the scene is resampled, the overlay is one-to-one.
    uint32_t sourceX = 0;
    uint32_t sourceY = 0;
    runner->Check(rhi::MapDisplayTexel(layers[0], Extent(640, 360), Extent(1280, 720), 100, 50,
                                      &sourceX, &sourceY) &&
                     sourceX == 50u && sourceY == 25u,
                 "T086 scene texel is resampled from the render extent");
    runner->Check(rhi::MapDisplayTexel(layers[2], Extent(640, 360), Extent(1280, 720), 100, 50,
                                      &sourceX, &sourceY) &&
                     sourceX == 100u && sourceY == 50u,
                 "T086 HUD texel is one-to-one after reconstruction");

    // A HUD declared at the render extent is refused: it must not be blurred
    // with the scene.
    std::vector<rhi::ComposeLayer> blurredHud = {
        Layer(rhi::LayerKind::Scene, "world", Extent(640, 360)),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", Extent(640, 360)),
    };
    rhi::PostReconstructionPlan rejected =
        rhi::PlanPostReconstruction(blurredHud, Extent(640, 360), Extent(1280, 720));
    runner->Check(rejected.status == rhi::Status::InvalidArgument && !rejected.error.empty(),
                 "T086 a render-extent HUD is refused with a reason");

    // Off path ordering: when the extents match, the order is unchanged and the
    // identity reconstruction replaces the upscale.
    const std::vector<rhi::ComposeLayer> nativeLayers = {
        Layer(rhi::LayerKind::Scene, "world", Extent(640, 360)),
        Layer(rhi::LayerKind::WorldGui, "pda", Extent(640, 360)),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", Extent(640, 360)),
    };
    rhi::PostReconstructionPlan nativePlan =
        rhi::PlanPostReconstruction(nativeLayers, Extent(640, 360), Extent(640, 360));
    runner->Check(nativePlan.status == rhi::Status::Ok && nativePlan.order.size() == 3u &&
                     nativePlan.order[0] == 0u && nativePlan.order[1] == 1u &&
                     nativePlan.order[2] == 2u,
                 "T086 off-path order is scene, world GUI, display overlay");
    runner->Check(nativePlan.reconstructionInputs == 2u,
                 "T086 off path still excludes the overlay");
    desc.displayWidth = 640;
    desc.displayHeight = 360;
    runner->Check(rhi::PlanReconstruction(desc, rhi::ReconstructionOptions{}, &reconstruction,
                                         &error) == rhi::Status::Ok &&
                     reconstruction.mode == rhi::ReconstructionMode::Off,
                 "T086 off path selects identity reconstruction");
    runner->Check(rhi::MapDisplayTexel(nativeLayers[2], Extent(640, 360), Extent(640, 360), 7, 9,
                                      &sourceX, &sourceY) &&
                     sourceX == 7u && sourceY == 9u,
                 "T086 off-path HUD is one-to-one");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    RunPlan(&runner);
    RunImagePaths(&runner);
    RunOrdering(&runner);
    return runner.Finish("reconstruction");
}
