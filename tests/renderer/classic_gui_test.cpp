// T038 — world-GUI versus display-overlay and ordered-capture fixtures (spec 004).
//
// Covers the structural half of CP-17, CP-18 and CP-28/O-11/O-12:
//
//   * an in-world GUI surface is drawn inside the current 3D view and creates no view;
//   * the display HUD is a separate 2D view at display resolution, appended as a view after
//     the scene, with the classic 640x480 ortho mapping;
//   * the overlay does not move the scene's colour target;
//   * a mid-frame CopyToImage that captures the world-GUI view executes inside that view,
//     before the display overlay begins, and the ordering predicate rejects the opposite.
//
// The display-resolution crispness (T-05/T-09) and HUD image comparison need a measured
// control and legal data; they are recorded as pending in evidence-q2.md, not claimed here.

#include "q2_test_support.h"

#include <vector>

namespace {

using doom3::rhi::ColorTargetSelector;
using doom3::rhi::SceneAdapter;
using doom3::rhi::SceneAdapterDesc;
using doom3::rhi::SceneCommandKind;
using doom3::rhi::SceneCommandQueue;
using doom3::rhi::SceneExecuteResult;
using doom3::rhi::Status;
using doom3::rhi::ViewDesc;

int RunOverlayViewMapping() {
    const ViewDesc overlay = doom3::rhi::MakeDisplayOverlayView(77, 1280, 720);
    if (!q2_test::Require(overlay.overlay, "display overlay is flagged as an overlay")) return 1;
    if (!q2_test::Require(!overlay.worldGui, "display overlay is not a world GUI")) return 1;
    if (!q2_test::RequireEq(overlay.identity, 77, "overlay keeps its identity")) return 1;
    if (!q2_test::RequireEq(overlay.width, 1280, "overlay uses the display width")) return 1;
    if (!q2_test::RequireEq(overlay.height, 720, "overlay uses the display height")) return 1;
    // 640x480 ortho mapping onto the display, top-left origin.
    if (!q2_test::RequireNear(overlay.projection[0], 2.0f / 640.0f, 1e-7f, "overlay x scale")) {
        return 1;
    }
    if (!q2_test::RequireNear(overlay.projection[5], -2.0f / 480.0f, 1e-7f, "overlay y scale")) {
        return 1;
    }
    if (!q2_test::RequireNear(overlay.projection[12], -1.0f, 1e-6f, "overlay x origin")) return 1;
    if (!q2_test::RequireNear(overlay.projection[13], 1.0f, 1e-6f, "overlay y origin")) return 1;
    q2_test::Log("OVERLAY 640x480 ortho mapped onto 1280x720 display");
    return 0;
}

int RunHudAfterScene(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(1, 1280, 720));
    queue.Draw(q2_test::MakePacket(10, 3));
    queue.EndView();
    queue.BeginView(doom3::rhi::MakeDisplayOverlayView(2, 1280, 720));
    queue.Draw(q2_test::MakePacket(11, 1));
    queue.EndView();

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC scene+overlay failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView,
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView};
    if (!q2_test::RequireSequence(result.executed, expected, "scene then overlay")) return 1;
    if (!q2_test::RequireEq(result.views, 2, "two views executed")) return 1;
    if (!q2_test::RequireEq(result.overlays, 1, "exactly one overlay view")) return 1;

    // The overlay must be appended after the scene. The kind sequence alone cannot prove
    // that, because both views begin with BeginView, so the recorded view identities are
    // compared in order.
    if (!q2_test::RequireEq(static_cast<uint32_t>(result.viewIds.size()), 2,
                            "two view identities recorded")) {
        return 1;
    }
    if (!q2_test::Require(result.viewIds[0] == 1, "scene view executes first")) return 1;
    if (!q2_test::Require(result.viewIds[1] == 2, "overlay view executes second")) return 1;

    // Negative control: the same comparison must reject the overlay-first order.
    const bool overlayFirstAccepted = result.viewIds.size() == 2 && result.viewIds[0] == 2 &&
                                      result.viewIds[1] == 1;
    if (overlayFirstAccepted) {
        q2_test::Log("CONTROL comparator accepted an overlay drawn before the scene");
        return 1;
    }
    q2_test::Log("CONTROL overlay-before-scene is rejected");
    return 0;
}

int RunWorldGuiStaysInView(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    ViewDesc scene = q2_test::MakeSceneView(5, 1280, 720);
    scene.worldGui = true;
    queue.BeginView(scene);
    doom3::rhi::DrawSurfPacket gui = q2_test::MakePacket(20, 1);
    gui.worldGui = true;
    queue.Draw(gui);
    queue.EndView();

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC world-gui failed: %s", error.c_str());
        return 1;
    }
    if (!q2_test::RequireEq(result.views, 1, "world GUI creates no extra view")) return 1;
    if (!q2_test::RequireEq(result.overlays, 0, "world GUI is not a display overlay")) return 1;
    if (!q2_test::RequireEq(result.drawsExtracted, 1, "world GUI is a draw in the 3D view")) {
        return 1;
    }
    q2_test::Log("WORLD GUI stays in the 3D view");
    return 0;
}

int RunOverlayKeepsTarget(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(31, 1280, 720));
    queue.SetColorTarget(ColorTargetSelector::Offscreen);
    queue.Draw(q2_test::MakePacket(30, 2));
    queue.EndView();
    queue.BeginView(doom3::rhi::MakeDisplayOverlayView(32, 1280, 720));
    queue.Draw(q2_test::MakePacket(31, 1));
    queue.EndView();

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC overlay target failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::SetColorTarget,
        SceneCommandKind::DrawPacket, SceneCommandKind::EndView,
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket, SceneCommandKind::EndView};
    if (!q2_test::RequireSequence(result.executed, expected, "overlay does not retarget")) {
        return 1;
    }
    // The only set-buffer command is the scene's, before the overlay begins.
    uint32_t setCount = 0;
    for (size_t i = 0; i < result.executed.size(); ++i) {
        if (result.executed[i] == SceneCommandKind::SetColorTarget) {
            ++setCount;
        }
    }
    if (!q2_test::RequireEq(setCount, 1, "overlay adds no colour-target command")) return 1;
    if (!q2_test::Require(result.viewIds.size() == 2 && result.viewIds[0] == 31 &&
                              result.viewIds[1] == 32,
                          "overlay view follows the scene view")) {
        return 1;
    }
    q2_test::Log("OVERLAY keeps the scene colour target");
    return 0;
}

// The copy that captures the world-GUI view must be recorded before the second (overlay)
// view begins. The stream has exactly one scene view and one overlay view, so the second
// BeginView is the overlay boundary.
bool CopyPrecedesOverlay(const std::vector<SceneCommandKind>& executed, size_t* copyIndex,
                         size_t* overlayIndex) {
    size_t copy = executed.size();
    size_t overlay = executed.size();
    uint32_t beginViews = 0;
    for (size_t i = 0; i < executed.size(); ++i) {
        if (executed[i] == SceneCommandKind::BeginView) {
            ++beginViews;
            if (beginViews == 2) {
                overlay = i;
            }
        } else if (executed[i] == SceneCommandKind::CopyToImage && copy == executed.size()) {
            copy = i;
        }
    }
    *copyIndex = copy;
    *overlayIndex = overlay;
    return copy != executed.size() && overlay != executed.size() && copy < overlay;
}

int RunWorldGuiCaptureOrder(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    ViewDesc scene = q2_test::MakeSceneView(61, 1280, 720);
    scene.worldGui = true;
    queue.BeginView(scene);
    doom3::rhi::DrawSurfPacket gui = q2_test::MakePacket(60, 1);
    gui.worldGui = true;
    queue.Draw(gui);
    queue.CopyToImage(9);  // capture the in-world GUI view, not the composed frame
    queue.EndView();
    queue.BeginView(doom3::rhi::MakeDisplayOverlayView(62, 1280, 720));
    queue.Draw(q2_test::MakePacket(61, 1));
    queue.EndView();
    queue.SwapBuffers();

    // The queue carried the destination identity; execution then preserves its position.
    const std::vector<doom3::rhi::SceneCommand>& recorded = queue.commands();
    if (!q2_test::RequireEq(static_cast<uint32_t>(recorded.size()), 8,
                            "capture stream has eight commands")) {
        return 1;
    }
    if (!q2_test::Require(recorded[2].kind == SceneCommandKind::CopyToImage &&
                              recorded[2].imageId == 9,
                          "the copy names its destination image")) {
        return 1;
    }

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC world-gui capture failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView,    SceneCommandKind::DrawPacket,
        SceneCommandKind::CopyToImage,  SceneCommandKind::EndView,
        SceneCommandKind::BeginView,    SceneCommandKind::DrawPacket,
        SceneCommandKind::EndView,      SceneCommandKind::SwapBuffers};
    if (!q2_test::RequireSequence(result.executed, expected,
                                  "world-gui capture before the overlay")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.views, 2, "capture stream has two views")) return 1;
    if (!q2_test::RequireEq(result.overlays, 1, "capture stream has one overlay")) return 1;
    if (!q2_test::Require(result.viewIds.size() == 2 && result.viewIds[0] == 61 &&
                              result.viewIds[1] == 62,
                          "the world-GUI view executes before the overlay")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.drawsExtracted, 2,
                            "both the GUI and HUD draws are extracted")) {
        return 1;
    }

    size_t copyIndex = 0;
    size_t overlayIndex = 0;
    if (!q2_test::Require(CopyPrecedesOverlay(result.executed, &copyIndex, &overlayIndex),
                          "the copy executes inside the world-GUI view")) {
        return 1;
    }
    q2_test::Log("CAPTURE copy at %u precedes the overlay at %u",
                 static_cast<uint32_t>(copyIndex), static_cast<uint32_t>(overlayIndex));

    // Negative control: the predicate must reject a copy recorded after the overlay begins.
    SceneCommandQueue misordered;
    misordered.BeginView(scene);
    misordered.Draw(gui);
    misordered.EndView();
    misordered.BeginView(doom3::rhi::MakeDisplayOverlayView(62, 1280, 720));
    misordered.CopyToImage(9);
    misordered.Draw(q2_test::MakePacket(61, 1));
    misordered.EndView();
    misordered.SwapBuffers();
    SceneExecuteResult misorderedResult;
    if (adapter.Execute(&misordered, &misorderedResult, &error) != Status::Ok) {
        return 1;
    }
    size_t misorderedCopy = 0;
    size_t misorderedOverlay = 0;
    if (CopyPrecedesOverlay(misorderedResult.executed, &misorderedCopy,
                            &misorderedOverlay)) {
        q2_test::Log("CONTROL the predicate accepted a copy after the overlay began");
        return 1;
    }
    q2_test::Log("CONTROL copy-after-overlay is rejected");
    return 0;
}

}  // namespace

int main() {
    if (!q2_test::SequenceControlRejectsReorder()) {
        return 1;
    }

    SceneAdapterDesc desc;
    desc.displayWidth = 1280;
    desc.displayHeight = 720;
    desc.structuralOnly = true;

    SceneAdapter adapter;
    std::string error;
    if (adapter.Initialize(desc, &error) != Status::Ok) {
        q2_test::Log("INIT adapter failed: %s", error.c_str());
        return 1;
    }

    if (RunOverlayViewMapping() != 0) return 1;
    if (RunHudAfterScene(adapter) != 0) return 1;
    if (RunWorldGuiStaysInView(adapter) != 0) return 1;
    if (RunOverlayKeepsTarget(adapter) != 0) return 1;
    if (RunWorldGuiCaptureOrder(adapter) != 0) return 1;

    q2_test::Log("T038 PASS (structure only; HUD image metrics NOT RUN)");
    return 0;
}
