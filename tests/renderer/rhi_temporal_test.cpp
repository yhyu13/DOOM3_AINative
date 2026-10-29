// Portable motion/validity and composition fixture. No device is required.

#include "q1b_test_support.h"

#include <cmath>

namespace {

using doom3::rhi::Camera;
using doom3::rhi::Float2;
using doom3::rhi::HistoryBuffer;
using doom3::rhi::LayerDeclaration;
using doom3::rhi::LayerKind;
using doom3::rhi::MotionSample;
using doom3::rhi::PlanComposition;
using doom3::rhi::Status;

bool NearlyEqual(float a, float b) { return std::fabs(a - b) < 0.0001f; }

int RunTemporal() {
    Camera still;
    still.center = Float2{0.0f, 0.0f};
    still.focal = 100.0f;
    Camera moved = still;
    moved.center = Float2{0.5f, 0.0f};
    const Float2 world{0.0f, 0.0f};

    const MotionSample analytic = doom3::rhi::ProjectMotion(still, moved, world, 2.0f,
                                                            64, 64);
    q1b_test::Log("MOTION analytic x=%.4f y=%.4f valid=%s", analytic.x, analytic.y,
                  analytic.valid ? "true" : "false");
    if (!analytic.valid || !NearlyEqual(analytic.x, 25.0f) ||
        !NearlyEqual(analytic.y, 0.0f)) {
        q1b_test::Require(false, "analytic motion is not 25,0");
        return 1;
    }

    const MotionSample zero = doom3::rhi::ProjectMotion(still, still, world, 2.0f, 64, 64);
    if (!zero.valid || !NearlyEqual(zero.x, 0.0f) || !NearlyEqual(zero.y, 0.0f)) {
        q1b_test::Require(false, "a static camera did not produce a valid zero motion");
        return 1;
    }
    const MotionSample invalid;
    if (zero.valid == invalid.valid) {
        q1b_test::Require(false, "validity is not separate from a valid zero vector");
        return 1;
    }
    q1b_test::Log("MOTION valid zero is distinct from an invalid sample");

    if (doom3::rhi::ProjectMotion(still, moved, world, 0.0f, 64, 64).valid) {
        q1b_test::Require(false, "zero depth was accepted as valid");
        return 1;
    }
    if (doom3::rhi::ProjectMotion(still, moved, world, -1.0f, 64, 64).valid) {
        q1b_test::Require(false, "negative depth was accepted as valid");
        return 1;
    }
    if (doom3::rhi::ProjectMotion(still, moved, Float2{1000.0f, 0.0f}, 2.0f, 64, 64)
            .valid) {
        q1b_test::Require(false, "an off-frame projection was accepted as valid");
        return 1;
    }
    q1b_test::Log("MOTION invalid depth and off-frame samples rejected");

    HistoryBuffer history;
    history.Resize(8, 8);
    if (history.generation() != 1 || history.empty()) {
        q1b_test::Require(false, "history resize did not allocate a generation");
        return 1;
    }
    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            if (history.At(x, y).valid) {
                q1b_test::Require(false, "fresh history is not invalid");
                return 1;
            }
        }
    }
    MotionSample stored;
    stored.valid = true;
    if (!history.Store(0, 0, stored)) {
        return 1;
    }
    if (!history.At(0, 0).valid || history.At(1, 0).valid) {
        q1b_test::Require(false, "history store did not preserve validity per pixel");
        return 1;
    }
    history.Reset();
    if (history.generation() != 2) {
        q1b_test::Require(false, "history reset did not advance the generation");
        return 1;
    }
    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            if (history.At(x, y).valid) {
                q1b_test::Require(false, "history reset did not invalidate every sample");
                return 1;
            }
        }
    }
    q1b_test::Log("HISTORY resize/reset advance the generation and invalidate samples");

    std::vector<LayerDeclaration> layers;
    layers.push_back(LayerDeclaration{LayerKind::Scene, "scene"});
    layers.push_back(LayerDeclaration{LayerKind::DisplayOverlay, "display"});
    layers.push_back(LayerDeclaration{LayerKind::WorldGui, "world-gui"});
    const doom3::rhi::CompositionPlan plan = PlanComposition(layers);
    if (plan.status != Status::Ok || plan.order.size() != 3 ||
        plan.order[0] != 0 || plan.order[1] != 2 || plan.order[2] != 1 ||
        plan.reconstructionInputs != 2) {
        q1b_test::Require(false, "composition order or reconstruction input count is wrong");
        return 1;
    }
    q1b_test::Log("COMPOSITION scene -> world gui -> display overlay, inputs=%u",
                  plan.reconstructionInputs);

    const doom3::rhi::CompositionPlan missing =
        PlanComposition({LayerDeclaration{LayerKind::DisplayOverlay, "display"}});
    if (missing.status != Status::InvalidArgument || missing.error.empty()) {
        q1b_test::Require(false, "a composition without a scene was accepted");
        return 1;
    }
    q1b_test::Log("COMPOSITION missing scene rejected: %s", missing.error.c_str());
    return 0;
}

}  // namespace

int main() {
    const int result = RunTemporal();
    q1b_test::Log("TEMPORAL result=%s", result == 0 ? "PASS" : "FAIL");
    return result;
}
