// T047 consumer test: scene/world-GUI versus display-resolution HUD ordering.
//
// Two parts:
//  1. The Q1b composition planner (src/renderer/rhi/frame_resources.h) already
//     classifies scene, world GUI and display overlay. Those checks exercise
//     merged production code and pass today.
//  2. The T047 production slice (src/renderer/rhi/overlay_composition.cpp) is
//     absent from this branch, so the post-reconstruction assertions are RED
//     by design. The seam comes from tests/renderer/q3_overlay_seam.h plus a
//     build-only stub.
//
// The T047 contract this test pins:
//  * display order is scene, world GUI, display overlay;
//  * scene and world GUI are the reconstruction inputs and are resampled when
//    the display extent is larger;
//  * a display overlay is composed AFTER reconstruction at display resolution,
//    texel for texel, so it is never resampled with the scene and never feeds
//    reconstruction or motion history;
//  * a layer declared at the wrong extent, a missing or duplicate scene layer
//    and a zero extent are refused with an actionable message.

#include "q3_overlay_seam.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;

// Local runner instead of tests/renderer/motion_test_support.h: the merged
// public headers src/renderer/rhi/frame_resources.h (Q1b) and
// src/renderer/rhi/motion_history.h (Q3) both define doom3::rhi::MotionSample,
// so the two cannot be included in one translation unit. The collision is
// recorded in evidence-q3-tests.md as a blocker for the T047 production slice;
// this test needs only the composition types, so it avoids the motion header.
class Runner {
public:
    void Check(bool condition, const std::string& label,
               const std::string& detail = std::string()) {
        ++checks_;
        if (condition) {
            std::printf("PASS %s\n", label.c_str());
            return;
        }
        ++failures_;
        if (detail.empty()) {
            std::printf("FAIL %s\n", label.c_str());
        } else {
            std::printf("FAIL %s: %s\n", label.c_str(), detail.c_str());
        }
    }

    int Finish(const char* name) const {
        std::printf("SUMMARY %s checks=%d failures=%d\n", name, checks_,
                    failures_);
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

constexpr uint32_t kRenderWidth = 640;
constexpr uint32_t kRenderHeight = 360;
constexpr uint32_t kDisplayWidth = 1280;
constexpr uint32_t kDisplayHeight = 720;

rhi::FrameExtent RenderExtent() {
    return rhi::FrameExtent{kRenderWidth, kRenderHeight};
}

rhi::FrameExtent DisplayExtent() {
    return rhi::FrameExtent{kDisplayWidth, kDisplayHeight};
}

rhi::ComposeLayer Layer(rhi::LayerKind kind, const char* name,
                        rhi::FrameExtent extent) {
    rhi::ComposeLayer layer;
    layer.kind = kind;
    layer.name = name;
    layer.extent = extent;
    return layer;
}

// Part 1: merged Q1b planner. This is production code, not the proposal.
void RunMergedPlanner(Runner* runner) {
    const std::vector<rhi::LayerDeclaration> layers = {
        {rhi::LayerKind::DisplayOverlay, "hud"},
        {rhi::LayerKind::Scene, "world"},
        {rhi::LayerKind::WorldGui, "pda"},
    };
    const rhi::CompositionPlan plan = rhi::PlanComposition(layers);
    runner->Check(plan.status == rhi::Status::Ok,
                  "T047 merged planner accepts one scene");
    runner->Check(plan.order.size() == 3u && plan.order[0] == 1u &&
                      plan.order[1] == 2u && plan.order[2] == 0u,
                  "T047 merged planner orders scene, world GUI, display HUD",
                  "actual size " + std::to_string(plan.order.size()));
    runner->Check(plan.reconstructionInputs == 2u,
                  "T047 merged planner excludes the HUD from reconstruction");

    const std::vector<rhi::LayerDeclaration> twoScenes = {
        {rhi::LayerKind::Scene, "world"},
        {rhi::LayerKind::Scene, "second world"},
    };
    const rhi::CompositionPlan rejected = rhi::PlanComposition(twoScenes);
    runner->Check(rejected.status == rhi::Status::InvalidArgument,
                  "T047 merged planner refuses two scene layers");
    runner->Check(!rejected.error.empty(),
                  "T047 merged planner reports the two-scene refusal");
}

void ExpectStatus(Runner* runner, const std::string& label,
                  const rhi::PostReconstructionPlan& plan,
                  rhi::Status expected) {
    // The portable Status name table lives in the backend translation unit, so
    // this CPU-only test reports the numeric value instead of pulling that
    // library in.
    runner->Check(plan.status == expected, label,
                  "status " + std::to_string(static_cast<int>(plan.status)) +
                      " error '" + plan.error + "'");
}

void RunOrdering(Runner* runner) {
    const std::vector<rhi::ComposeLayer> layers = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
        Layer(rhi::LayerKind::WorldGui, "pda", RenderExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
    };
    const rhi::PostReconstructionPlan plan =
        rhi::PlanPostReconstruction(layers, RenderExtent(), DisplayExtent());
    ExpectStatus(runner, "T047 post-reconstruction plan accepted", plan,
                 rhi::Status::Ok);
    runner->Check(plan.order.size() == 3u && plan.order[0] == 0u &&
                      plan.order[1] == 1u && plan.order[2] == 2u,
                  "T047 scene, world GUI, then display HUD",
                  "actual size " + std::to_string(plan.order.size()));
    runner->Check(plan.reconstructionInputs == 2u,
                  "T047 only scene and world GUI are reconstruction input");
    runner->Check(plan.displayOverlays == 1u,
                  "T047 one display overlay is counted");

    // Declaration order must not move the HUD before the scene.
    const std::vector<rhi::ComposeLayer> shuffled = {
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
        Layer(rhi::LayerKind::WorldGui, "pda", RenderExtent()),
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
    };
    const rhi::PostReconstructionPlan reordered =
        rhi::PlanPostReconstruction(shuffled, RenderExtent(), DisplayExtent());
    ExpectStatus(runner, "T047 shuffled declaration accepted", reordered,
                 rhi::Status::Ok);
    runner->Check(reordered.order.size() == 3u && reordered.order[0] == 2u &&
                      reordered.order[1] == 1u && reordered.order[2] == 0u,
                  "T047 HUD is composed last whatever the declaration order");

    // Two overlays keep their relative order and both stay last.
    const std::vector<rhi::ComposeLayer> twoOverlays = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud-a", DisplayExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud-b", DisplayExtent()),
    };
    const rhi::PostReconstructionPlan overlays =
        rhi::PlanPostReconstruction(twoOverlays, RenderExtent(), DisplayExtent());
    ExpectStatus(runner, "T047 two overlays accepted", overlays,
                 rhi::Status::Ok);
    runner->Check(overlays.order.size() == 3u && overlays.order[0] == 0u &&
                      overlays.order[1] == 1u && overlays.order[2] == 2u,
                  "T047 overlay order is preserved");
    runner->Check(overlays.reconstructionInputs == 1u &&
                      overlays.displayOverlays == 2u,
                  "T047 neither overlay feeds reconstruction");

    // Without reconstruction the scene is already at display resolution, but
    // the HUD is still composed last and stays outside reconstruction.
    const std::vector<rhi::ComposeLayer> direct = {
        Layer(rhi::LayerKind::Scene, "world", DisplayExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
    };
    const rhi::PostReconstructionPlan noReconstruction =
        rhi::PlanPostReconstruction(direct, DisplayExtent(), DisplayExtent());
    ExpectStatus(runner, "T047 render extent equal to display accepted",
                 noReconstruction, rhi::Status::Ok);
    runner->Check(noReconstruction.order.size() == 2u &&
                      noReconstruction.order[0] == 0u &&
                      noReconstruction.order[1] == 1u,
                  "T047 HUD still last without reconstruction");
}

void RunRejections(Runner* runner) {
    // A display HUD declared at the render extent would be resampled with the
    // scene; refuse it instead of blurring the HUD.
    const std::vector<rhi::ComposeLayer> hudAtRenderExtent = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", RenderExtent()),
    };
    ExpectStatus(
        runner, "T047 HUD at render extent is refused",
        rhi::PlanPostReconstruction(hudAtRenderExtent, RenderExtent(),
                                    DisplayExtent()),
        rhi::Status::InvalidArgument);

    const std::vector<rhi::ComposeLayer> sceneAtDisplayExtent = {
        Layer(rhi::LayerKind::Scene, "world", DisplayExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
    };
    ExpectStatus(
        runner, "T047 scene at display extent is refused",
        rhi::PlanPostReconstruction(sceneAtDisplayExtent, RenderExtent(),
                                    DisplayExtent()),
        rhi::Status::InvalidArgument);

    const std::vector<rhi::ComposeLayer> guiAtDisplayExtent = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
        Layer(rhi::LayerKind::WorldGui, "pda", DisplayExtent()),
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
    };
    ExpectStatus(
        runner, "T047 world GUI at display extent is refused",
        rhi::PlanPostReconstruction(guiAtDisplayExtent, RenderExtent(),
                                    DisplayExtent()),
        rhi::Status::InvalidArgument);

    const std::vector<rhi::ComposeLayer> noScene = {
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent()),
    };
    ExpectStatus(runner, "T047 missing scene is refused",
                 rhi::PlanPostReconstruction(noScene, RenderExtent(),
                                             DisplayExtent()),
                 rhi::Status::InvalidArgument);

    const std::vector<rhi::ComposeLayer> twoScenes = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
        Layer(rhi::LayerKind::Scene, "second world", RenderExtent()),
    };
    ExpectStatus(runner, "T047 two scene layers are refused",
                 rhi::PlanPostReconstruction(twoScenes, RenderExtent(),
                                             DisplayExtent()),
                 rhi::Status::InvalidArgument);

    const std::vector<rhi::ComposeLayer> oneScene = {
        Layer(rhi::LayerKind::Scene, "world", RenderExtent()),
    };
    ExpectStatus(runner, "T047 zero render extent is refused",
                 rhi::PlanPostReconstruction(
                     oneScene, rhi::FrameExtent{0u, kRenderHeight},
                     DisplayExtent()),
                 rhi::Status::InvalidArgument);
    ExpectStatus(runner, "T047 zero display extent is refused",
                 rhi::PlanPostReconstruction(
                     oneScene, RenderExtent(), rhi::FrameExtent{kDisplayWidth, 0u}),
                 rhi::Status::InvalidArgument);
    ExpectStatus(runner, "T047 empty layer list is refused",
                 rhi::PlanPostReconstruction({}, RenderExtent(), DisplayExtent()),
                 rhi::Status::InvalidArgument);
}

void RunTexelMapping(Runner* runner) {
    const rhi::ComposeLayer scene =
        Layer(rhi::LayerKind::Scene, "world", RenderExtent());
    const rhi::ComposeLayer gui =
        Layer(rhi::LayerKind::WorldGui, "pda", RenderExtent());
    const rhi::ComposeLayer hud =
        Layer(rhi::LayerKind::DisplayOverlay, "hud", DisplayExtent());

    uint32_t x = 0;
    uint32_t y = 0;

    // Scene and world GUI are resampled from the render extent: a 2x display
    // maps display texel 100,50 to render texel 50,25.
    const bool sceneOk = rhi::MapDisplayTexel(scene, RenderExtent(),
                                              DisplayExtent(), 100u, 50u, &x, &y);
    runner->Check(sceneOk && x == 50u && y == 25u,
                  "T047 scene texel is resampled",
                  "actual " + std::to_string(x) + "," + std::to_string(y));
    const bool guiOk = rhi::MapDisplayTexel(gui, RenderExtent(), DisplayExtent(),
                                            100u, 50u, &x, &y);
    runner->Check(guiOk && x == 50u && y == 25u,
                  "T047 world GUI texel is resampled",
                  "actual " + std::to_string(x) + "," + std::to_string(y));

    // The display HUD is composed at display resolution, one texel to one
    // texel. Any scaling here is the defect this test exists for.
    const bool hudOk = rhi::MapDisplayTexel(hud, RenderExtent(), DisplayExtent(),
                                            100u, 50u, &x, &y);
    runner->Check(hudOk && x == 100u && y == 50u,
                  "T047 display HUD texel is not resampled",
                  "actual " + std::to_string(x) + "," + std::to_string(y));

    // The HUD mapping must not depend on the render extent.
    const bool hudIgnoredRender =
        rhi::MapDisplayTexel(hud, rhi::FrameExtent{320u, 180u}, DisplayExtent(),
                             1279u, 719u, &x, &y);
    runner->Check(hudIgnoredRender && x == 1279u && y == 719u,
                  "T047 HUD mapping ignores the render extent",
                  "actual " + std::to_string(x) + "," + std::to_string(y));

    // Scene mapping of the last display texel stays inside the render extent.
    const bool lastScene =
        rhi::MapDisplayTexel(scene, RenderExtent(), DisplayExtent(), 1279u,
                             719u, &x, &y);
    runner->Check(lastScene && x < kRenderWidth && y < kRenderHeight,
                  "T047 last scene texel stays inside the render extent",
                  "actual " + std::to_string(x) + "," + std::to_string(y));

    // A non-power-of-two scale is a floor division, not a rounding error.
    const bool oddScale =
        rhi::MapDisplayTexel(scene, rhi::FrameExtent{1280u, 720u},
                             rhi::FrameExtent{1920u, 1080u}, 100u, 50u, &x, &y);
    runner->Check(oddScale && x == 66u && y == 33u,
                  "T047 scene texel uses a bounded floor scale",
                  "actual " + std::to_string(x) + "," + std::to_string(y));

    // Out-of-range texels and zero extents are refused, never clamped.
    x = 9999u;
    y = 9999u;
    const bool offRight = rhi::MapDisplayTexel(scene, RenderExtent(),
                                               DisplayExtent(), kDisplayWidth,
                                               50u, &x, &y);
    runner->Check(!offRight && x == 9999u && y == 9999u,
                  "T047 out-of-range display texel is refused untouched");
    const bool offBottom = rhi::MapDisplayTexel(hud, RenderExtent(),
                                                DisplayExtent(), 10u,
                                                kDisplayHeight, &x, &y);
    runner->Check(!offBottom && x == 9999u && y == 9999u,
                  "T047 out-of-range HUD texel is refused untouched");
    const bool zeroExtent = rhi::MapDisplayTexel(
        scene, rhi::FrameExtent{0u, 0u}, DisplayExtent(), 10u, 10u, &x, &y);
    runner->Check(!zeroExtent, "T047 zero extent mapping is refused");
}

}  // namespace

int main() {
    Runner runner;
    RunMergedPlanner(&runner);
    RunOrdering(&runner);
    RunRejections(&runner);
    RunTexelMapping(&runner);
    return runner.Finish("q3.overlay_composition");
}
