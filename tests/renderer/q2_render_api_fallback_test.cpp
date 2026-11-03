// T036 — failed classic-path initialization and reference fallback contract (spec 004).
//
// The engine selector `r_renderApi` and its typed failure live in the reviewed engine fork
// (`neo/renderer/RenderApi.h`, `RenderSystem_init.cpp`): 0 keeps the OpenGL reference path,
// 1 selects the modern classic path, and a failed modern initialization must report an
// actionable result without rewriting the cvar or silently switching paths.
//
// This fixture exercises the portable half of that contract through the same consumer the
// engine installs:
//
//   * a consumer asked for real classic submission without a backend refuses to
//     initialize and says what is missing, instead of drawing an empty frame;
//   * the refusal does not rewrite the request into structural-only mode, and a second
//     attempt with the same request still fails (no automatic switch);
//   * the reference/structural path remains selectable and usable in the same process:
//     it initializes and executes an ordered view, and it still discloses that no frame
//     was drawn.
//
// It does not boot the engine, does not set `r_renderApi`, and does not prove the GL path
// renders. T083 owns the forced-failure launch and remains NOT RUN; the engine selector's
// runtime behaviour is therefore not claimed here.

#include "q2_test_support.h"
#include "rhi/engine_q2_sink.h"

#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;

using rhi::EngineQ2SinkConsumer;
using rhi::EngineQ2SinkDesc;
using rhi::Q2SinkMaterialFacts;
using rhi::SceneGeometryBackend;
using rhi::SceneSurfaceGeometry;
using rhi::Status;

constexpr int kWidth = 1280;
constexpr int kHeight = 720;

const float kIdentityProjection[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                       0.0f, 1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 0.0f, 1.0f};

// A backend that must never be reached: the descriptor fails before any upload or submit.
class UnusedBackend : public SceneGeometryBackend {
public:
    bool Upload(uint32_t, const SceneSurfaceGeometry&, rhi::SceneMeshHandles*,
                std::string*) override {
        return false;
    }
    Status Submit(const rhi::DrawFrameDesc&, rhi::Submission*, std::string*) override {
        return Status::InvalidArgument;
    }
};

bool Contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

int RunFailedModernInit() {
    EngineQ2SinkDesc desc;  // structuralOnly defaults to false: real submission.
    EngineQ2SinkConsumer consumer(desc);

    std::string error;
    if (consumer.Initialize(kWidth, kHeight, &error)) {
        q2_test::Log("CONTROL a classic consumer initialized without a backend");
        return 1;
    }
    if (!q2_test::Require(!error.empty(), "failed init returns an actionable reason")) return 1;
    if (!q2_test::Require(Contains(error, "backend"),
                          "the reason names the missing classic backend")) {
        return 1;
    }
    if (!q2_test::Require(Contains(error, "refusing"),
                          "the reason states the refusal instead of a silent frame")) {
        return 1;
    }
    if (!q2_test::Require(!consumer.initialized(),
                          "a failed initialization leaves the consumer uninitialized")) {
        return 1;
    }

    // No automatic switch: the request still asks for real submission and no backend.
    if (!q2_test::Require(!consumer.desc().structuralOnly,
                          "the failure does not rewrite the request to structural-only")) {
        return 1;
    }
    if (!q2_test::Require(consumer.desc().backend == nullptr,
                          "the failure does not invent a backend")) {
        return 1;
    }

    // A callback before a successful initialization is refused with a reason, not accepted
    // as an empty frame.
    if (consumer.BeginView(1u, 0, 0, kWidth, kHeight, false, false, kIdentityProjection)) {
        q2_test::Log("CONTROL an uninitialized consumer accepted a view");
        return 1;
    }
    if (!q2_test::Require(!consumer.lastError().empty(),
                          "an uninitialized stream reports why it was refused")) {
        return 1;
    }

    // A second attempt with the unchanged request fails the same way.
    std::string retryError;
    if (consumer.Initialize(kWidth, kHeight, &retryError)) {
        q2_test::Log("CONTROL a retry silently switched to the reference path");
        return 1;
    }
    if (!q2_test::Require(Contains(retryError, "backend"),
                          "the retry keeps the actionable refusal")) {
        return 1;
    }
    q2_test::Log("T036 modern init refused: %s", retryError.c_str());
    return 0;
}

int RunMissingPipeline() {
    // The failure is not just "backend is null": a backend without a pipeline is also
    // refused with a specific reason, so an actionable error is not a constant.
    UnusedBackend unused;
    EngineQ2SinkDesc desc;
    desc.structuralOnly = false;
    desc.backend = &unused;
    EngineQ2SinkConsumer consumer(desc);

    std::string error;
    if (consumer.Initialize(kWidth, kHeight, &error)) {
        q2_test::Log("CONTROL a backend with no surface pipeline initialized");
        return 1;
    }
    if (!q2_test::Require(Contains(error, "pipeline"),
                          "the reason names the missing surface pipeline")) {
        return 1;
    }
    if (!q2_test::Require(Contains(error, "refusing"),
                          "the pipeline failure also states the refusal")) {
        return 1;
    }
    q2_test::Log("T036 missing pipeline refused: %s", error.c_str());
    return 0;
}

int RunZeroExtentFailure() {
    EngineQ2SinkDesc desc;
    desc.structuralOnly = true;
    EngineQ2SinkConsumer consumer(desc);

    std::string error;
    if (consumer.Initialize(0, kHeight, &error)) {
        q2_test::Log("CONTROL a zero-width display was accepted");
        return 1;
    }
    if (!q2_test::Require(Contains(error, "extent"),
                          "the reason names the display extent")) {
        return 1;
    }
    if (!q2_test::Require(!consumer.initialized(), "the failed extent is not initialized")) {
        return 1;
    }

    // The same consumer must be recoverable: the failure did not poison it.
    std::string recoverError;
    if (!consumer.Initialize(kWidth, kHeight, &recoverError)) {
        q2_test::Log("T036 recover after a bad extent failed: %s", recoverError.c_str());
        return 1;
    }
    q2_test::Log("T036 bad extent refused, then a valid extent initialized");
    return 0;
}

int RunReferencePathUsable() {
    EngineQ2SinkDesc desc;
    desc.structuralOnly = true;  // the retained reference/structural path.
    EngineQ2SinkConsumer consumer(desc);

    std::string error;
    if (!consumer.Initialize(kWidth, kHeight, &error)) {
        q2_test::Log("T036 reference path failed to initialize: %s", error.c_str());
        return 1;
    }

    Q2SinkMaterialFacts material;
    material.numStages = 1;
    material.hasTexture = true;
    material.hasProgram = true;
    if (!consumer.BeginView(1u, 0, 0, kWidth, kHeight, false, false,
                            kIdentityProjection) ||
        !consumer.DrawPacket(10u, material, 4, 6, 0, 0.0f, false, false) ||
        !consumer.EndView() || !consumer.SwapBuffers()) {
        q2_test::Log("T036 reference path refused a valid stream: %s",
                     consumer.lastError().c_str());
        return 1;
    }
    if (!q2_test::Require(consumer.lastSwapSucceeded(),
                          "the reference path executes and completes a frame")) {
        return 1;
    }
    if (!q2_test::RequireEq(consumer.lastResult().views, 1, "one view executed")) return 1;
    if (!q2_test::RequireEq(consumer.lastResult().drawsExtracted, 1,
                            "one draw extracted on the reference path")) {
        return 1;
    }
    if (!q2_test::RequireEq(consumer.lastResult().overlays, 0,
                            "the reference path creates no overlay")) {
        return 1;
    }
    // Deliberately not a rendered frame: the structural path has no device submission.
    if (!q2_test::Require(consumer.lastResult().drawSubmissionUnavailable,
                          "the reference path still discloses that no frame was drawn")) {
        return 1;
    }
    q2_test::Log("T036 reference path usable after a failed modern init (structure only)");
    return 0;
}

}  // namespace

int main() {
    if (RunFailedModernInit() != 0) return 1;
    if (RunMissingPipeline() != 0) return 1;
    if (RunZeroExtentFailure() != 0) return 1;
    if (RunReferencePathUsable() != 0) return 1;

    q2_test::Log(
        "T036 PASS (portable failed-init/fallback contract; engine r_renderApi launch "
        "and T083 NOT RUN)");
    return 0;
}
