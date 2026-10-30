// T085 superproject integration test: Q2 ordered stream -> Q3 rendered-frame
// history, private skinned pose, per-pixel motion and post-reconstruction HUD
// order.
//
// This is the integration half of Q3 that the portable seams can prove. It
// consumes:
//   * the merged Q2 SceneAdapter (src/renderer/rhi/engine_scene_adapter.cpp),
//     not a test double;
//   * the T041 fixture rows SKN-01..03 and DEF-01 from motion-fixtures.md, not
//     synthetic vectors invented here;
//   * the Q3 production slices view_history.cpp and overlay_composition.cpp.
//
// It does not claim an engine frame: the fork call sites (private pose snapshot
// in neo/renderer/Model_md5.cpp and RC_* dispatch) are blocked on a fork PR and
// a submodule bump, recorded in evidence-q3.md.

#include "rhi/frame_integration.h"
#include "motion_test_support.h"
#include "q2_test_support.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;
using motion_test::CameraAt;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;

constexpr double kPi = 3.14159265358979323846;
constexpr double kTolPixel = 1.0e-6;  // float storage after the double fixture

rhi::FrameExtent Render() { return rhi::FrameExtent{640u, 360u}; }
rhi::FrameExtent Display() { return rhi::FrameExtent{1280u, 720u}; }

rhi::ViewIdentity Identity(uint32_t instance = 1) {
    rhi::ViewIdentity identity;
    identity.worldId = 1;
    identity.instanceId = instance;
    identity.generation = 1;
    identity.topology = 1;
    identity.entity = 1;
    identity.provider = 1;
    identity.width = 640;
    identity.height = 360;
    return identity;
}

rhi::RenderedViewCommit View(rhi::ViewKind kind, const rhi::ViewIdentity& identity,
                             double z) {
    rhi::RenderedViewCommit commit;
    commit.kind = kind;
    commit.identity = identity;
    commit.camera = CameraAt(0.0, 0.0, 0.0);
    commit.records.push_back(Record(P(100.0, 0.0, z)));
    return commit;
}

bool SamePoint(const rhi::MotionVec3& a, const rhi::MotionVec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// T085: the T041 skinned/deformation rows, driven through the integration's
// rendered history and stored into the per-pixel resource.
void RunFixtureVectors(Runner* runner) {
    struct Row {
        const char* label;
        rhi::MotionVec3 previousPosed;
        rhi::MotionVec3 currentPosed;
        double du;
        double dv;
        double toleranceU;
        double toleranceV;
    };
    const double yaw2 = 2.0 * kPi / 180.0;
    const Row rows[] = {
        {"SKN-01 blend", P(100.0, 0.0, 1.0), P(100.0, 0.0, 0.5), 0.0,
         -0.003333333, 1.00e-07, 1.91e-06},
        {"SKN-02 stationary", P(100.0, 0.0, 0.0), P(100.0, 0.0, 0.0), 0.0, 0.0,
         1.00e-07, 1.00e-07},
        {"SKN-03 rotation",
         P(100.0 * std::cos(yaw2), 100.0 * std::sin(yaw2), 1.0),
         P(100.0, 0.0, 1.0), -0.013095289, -0.000004064, 7.63e-06, 1.00e-07},
        {"DEF-01 deformation", P(100.0, 0.0, 0.3), P(100.0, 0.0, -0.2), 0.0,
         -0.003333333, 1.00e-07, 1.91e-06},
    };

    for (const Row& row : rows) {
        rhi::FrameIntegration frame;
        std::string error;
        const rhi::FrameIntegrationDesc desc{Render(), Display()};
        runner->Check(frame.Initialize(desc, &error) == rhi::Status::Ok,
                      std::string(row.label) + " integration initialized");

        const rhi::ViewIdentity identity = Identity();
        rhi::RenderedViewCommit previous = View(rhi::ViewKind::Main, identity, 0.0);
        previous.records.clear();
        previous.records.push_back(Record(row.previousPosed));
        previous.poseIdentity = 41;
        previous.poseGeneration = 3;
        previous.posedPositions.push_back(row.previousPosed);
        runner->Check(frame.CommitView(previous),
                      std::string(row.label) + " previous render committed");

        rhi::RenderedViewState stored;
        const rhi::MotionValidity validity =
            frame.Previous(rhi::ViewKind::Main, identity, &stored);
        runner->Validity(std::string(row.label) + " previous state", validity,
                         rhi::MotionValidity::Valid);
        if (validity != rhi::MotionValidity::Valid || stored.records.size() != 1) {
            runner->Check(false, std::string(row.label) + " previous record");
            continue;
        }
        runner->Check(SamePoint(stored.records[0].point, row.previousPosed),
                      std::string(row.label) + " previous point");

        std::vector<rhi::MotionVec3> privatePose;
        const rhi::MotionValidity poseValidity = frame.PreviousPrivatePose(
            rhi::ViewKind::Main, 41, 3, &privatePose);
        runner->Validity(std::string(row.label) + " private pose", poseValidity,
                         rhi::MotionValidity::Valid);
        if (poseValidity == rhi::MotionValidity::Valid && privatePose.size() == 1) {
            runner->Check(SamePoint(privatePose[0], row.previousPosed),
                          std::string(row.label) + " private pose retained");
        }

        const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            stored.camera, stored.records[0], camera, Record(row.currentPosed));
        runner->Validity(std::string(row.label) + " reprojection valid",
                         sample.validity, rhi::MotionValidity::Valid);
        runner->Near(std::string(row.label) + " du", sample.vector.du, row.du,
                     row.toleranceU);
        runner->Near(std::string(row.label) + " dv", sample.vector.dv, row.dv,
                     row.toleranceV);

        runner->Check(frame.StorePerPixelMotion(320u, 180u, sample),
                      std::string(row.label) + " motion stored");
        const rhi::PerPixelMotionSample loaded =
            frame.LoadPerPixelMotion(320u, 180u);
        runner->Check(loaded.valid, std::string(row.label) + " stored valid");
        runner->Near(std::string(row.label) + " stored du", loaded.x, row.du,
                     kTolPixel);
        runner->Near(std::string(row.label) + " stored dv", loaded.y, row.dv,
                     kTolPixel);
    }
}

void RunQueueOrdering(Runner* runner) {
    rhi::FrameIntegration frame;
    std::string error;
    runner->Check(frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()},
                                   &error) == rhi::Status::Ok,
                  "queue ordering integration initialized");

    rhi::SceneAdapterDesc desc;
    desc.displayWidth = 1280;
    desc.displayHeight = 720;
    desc.structuralOnly = true;
    rhi::SceneAdapter adapter;
    runner->Check(adapter.Initialize(desc, &error) == rhi::Status::Ok,
                  "Q2 adapter initialized");

    rhi::SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(1, 640, 360));
    queue.Draw(q2_test::MakePacket(10, 3));
    queue.EndView();
    rhi::ViewDesc gui = q2_test::MakeSceneView(2, 640, 360);
    gui.worldGui = true;
    queue.BeginView(gui);
    queue.Draw(q2_test::MakePacket(11, 1));
    queue.EndView();
    queue.BeginView(rhi::MakeDisplayOverlayView(3, 1280, 720));
    queue.Draw(q2_test::MakePacket(12, 1));
    queue.EndView();

    const rhi::PostReconstructionPlan plan = frame.PlanQueue(queue);
    runner->Check(plan.status == rhi::Status::Ok, "queue plan accepted",
                  plan.error);
    runner->Check(plan.order.size() == 3u && plan.order[0] == 0u &&
                      plan.order[1] == 1u && plan.order[2] == 2u,
                  "queue plan orders scene, world GUI, display HUD");
    runner->Check(plan.reconstructionInputs == 2u && plan.displayOverlays == 1u,
                  "queue plan splits reconstruction inputs from the HUD");

    const std::vector<rhi::ComposeLayer> layers =
        rhi::ComposeLayersFromQueue(queue, Render(), Display());
    runner->Check(layers.size() == 3u &&
                      layers[0].kind == rhi::LayerKind::Scene &&
                      layers[0].extent.width == 640u &&
                      layers[1].kind == rhi::LayerKind::WorldGui &&
                      layers[1].extent.width == 640u &&
                      layers[2].kind == rhi::LayerKind::DisplayOverlay &&
                      layers[2].extent.width == 1280u,
                  "Q2 view declarations become the Q3 layer roles");

    uint32_t sourceX = 0;
    uint32_t sourceY = 0;
    runner->Check(rhi::MapDisplayTexel(layers[0], Render(), Display(), 100u, 50u,
                                       &sourceX, &sourceY) &&
                      sourceX == 50u && sourceY == 25u,
                  "scene layer is resampled from the render extent");
    runner->Check(rhi::MapDisplayTexel(layers[2], Render(), Display(), 100u, 50u,
                                       &sourceX, &sourceY) &&
                      sourceX == 100u && sourceY == 50u,
                  "display HUD layer is one to one at the display extent");

    rhi::SceneExecuteResult result;
    runner->Check(frame.ExecuteFrame(&adapter, &queue, &result, &error) ==
                      rhi::Status::Ok,
                  "Q2 stream executed through the integration", error);
    runner->Check(result.viewIds.size() == 3u && result.viewIds[0] == 1u &&
                      result.viewIds[1] == 2u && result.viewIds[2] == 3u,
                  "executed view order keeps the HUD last");
    runner->Check(result.overlays == 1u, "one display overlay executed");
    runner->Check(frame.stats().overlaysOrdered == 1u,
                  "integration counted the ordered overlay");

    // A HUD declared first is refused before the adapter consumes anything.
    rhi::SceneCommandQueue reordered;
    reordered.BeginView(rhi::MakeDisplayOverlayView(9, 1280, 720));
    reordered.Draw(q2_test::MakePacket(20, 1));
    reordered.EndView();
    reordered.BeginView(q2_test::MakeSceneView(8, 640, 360));
    reordered.Draw(q2_test::MakePacket(21, 1));
    reordered.EndView();
    const size_t before = reordered.Size();
    rhi::SceneExecuteResult refused;
    const rhi::Status status = frame.ExecuteFrame(&adapter, &reordered, &refused,
                                                  &error);
    runner->Check(status == rhi::Status::InvalidArgument,
                  "HUD-before-scene stream is refused",
                  "status " + std::to_string(static_cast<int>(status)));
    runner->Check(!error.empty(), "HUD-before-scene refusal is actionable");
    runner->Check(reordered.Size() == before,
                  "refused stream was not consumed");
}

void RunHistoryAndMotion(Runner* runner) {
    rhi::FrameIntegration frame;
    std::string error;
    runner->Check(frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()},
                                   &error) == rhi::Status::Ok,
                  "history integration initialized");

    const rhi::ViewIdentity identity = Identity();
    runner->Check(frame.CommitView(View(rhi::ViewKind::Main, identity, 0.0)),
                  "main view committed");
    runner->Check(frame.CommitView(View(rhi::ViewKind::Mirror, identity, 0.25)),
                  "mirror view committed");
    runner->Check(frame.CommitView(View(rhi::ViewKind::Camera, identity, 0.5)),
                  "camera view committed");

    struct Row {
        const char* label;
        rhi::ViewKind kind;
        double z;
    };
    const Row rows[] = {
        {"main history", rhi::ViewKind::Main, 0.0},
        {"mirror history", rhi::ViewKind::Mirror, 0.25},
        {"camera history", rhi::ViewKind::Camera, 0.5},
    };
    for (const Row& row : rows) {
        rhi::RenderedViewState previous;
        runner->Validity(row.label,
                         frame.Previous(row.kind, identity, &previous),
                         rhi::MotionValidity::Valid);
        if (previous.records.size() == 1) {
            runner->Check(SamePoint(previous.records[0].point,
                                    P(100.0, 0.0, row.z)),
                          std::string(row.label) + " isolation");
        } else {
            runner->Check(false, std::string(row.label) + " record");
        }
    }

    // A display overlay is composed after reconstruction; it must not create or
    // advance history even when the engine tries to commit it.
    rhi::RenderedViewCommit hud = View(rhi::ViewKind::Main, identity, 9.0);
    hud.displayOverlay = true;
    runner->Check(!frame.CommitView(hud), "display overlay is not committed");
    runner->Check(frame.stats().displayViewsRefused == 1u,
                  "display overlay refusal counted");
    rhi::RenderedViewState afterHud;
    runner->Validity("display overlay leaves main history untouched",
                     frame.Previous(rhi::ViewKind::Main, identity, &afterHud),
                     rhi::MotionValidity::Valid);
    if (afterHud.records.size() == 1) {
        runner->Check(SamePoint(afterHud.records[0].point, P(100.0, 0.0, 0.0)),
                      "display overlay did not replace the rendered pose");
    }

    // An unrendered tick advancing a subview must not move the main record.
    runner->Check(frame.CommitView(View(rhi::ViewKind::Mirror, identity, 0.75)),
                  "later mirror render committed");
    rhi::RenderedViewState mainAgain;
    if (frame.Previous(rhi::ViewKind::Main, identity, &mainAgain) ==
            rhi::MotionValidity::Valid &&
        mainAgain.records.size() == 1) {
        runner->Check(SamePoint(mainAgain.records[0].point, P(100.0, 0.0, 0.0)),
                      "a subview render does not advance main history");
    } else {
        runner->Check(false, "main history after subview");
    }

    // The stored previous pose feeds the per-pixel resource. Last rendered main
    // pose is z=0.0, current z=1.0: SEQ-02 (0.006666667), not a valid zero.
    const rhi::MotionSample sample = rhi::ReprojectPoint(
        mainAgain.camera, mainAgain.records[0], CameraAt(0.0, 0.0, 0.0),
        Record(P(100.0, 0.0, 1.0)));
    runner->Validity("unrendered tick reprojection valid", sample.validity,
                     rhi::MotionValidity::Valid);
    runner->Check(frame.StorePerPixelMotion(10u, 10u, sample),
                  "per-pixel motion stored");
    const rhi::PerPixelMotionSample loaded = frame.LoadPerPixelMotion(10u, 10u);
    runner->Check(loaded.valid, "per-pixel sample is valid");
    runner->Near("per-pixel dv", loaded.y, 0.006666667, kTolPixel);

    rhi::MotionSample missing;
    missing.validity = rhi::MotionValidity::NoHistory;
    runner->Check(frame.StorePerPixelMotion(11u, 10u, missing),
                  "invalid sample is stored as invalid");
    runner->Check(!frame.LoadPerPixelMotion(11u, 10u).valid,
                  "invalid sample is not a valid zero");
    runner->Check(frame.LoadPerPixelMotion(11u, 10u).x == 0.0f &&
                      frame.LoadPerPixelMotion(11u, 10u).y == 0.0f,
                  "invalid sample vector is not consumed");

    runner->Check(!frame.StorePerPixelMotion(1280u, 0u, sample),
                  "out-of-range motion write is refused");
    runner->Check(frame.stats().motionWritesRefused == 1u,
                  "refused motion write counted");
}

void RunInvalidation(Runner* runner) {
    const rhi::ViewIdentity identity = Identity();

    // Cut: every view is invalidated, the caller's output is untouched.
    {
        rhi::FrameIntegration frame;
        std::string error;
        frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()}, &error);
        frame.CommitView(View(rhi::ViewKind::Main, identity, 0.0));
        runner->Check(frame.NotifyEngineEvent(rhi::FrameEvent::Cut),
                      "cut event applied");
        rhi::RenderedViewState sentinel;
        sentinel.records.push_back(Record(P(-1.0, -1.0, -1.0)));
        runner->Validity("cut invalidates",
                         frame.Previous(rhi::ViewKind::Main, identity, &sentinel),
                         rhi::MotionValidity::Reset);
        runner->Check(sentinel.records.size() == 1 &&
                          SamePoint(sentinel.records[0].point, P(-1.0, -1.0, -1.0)),
                      "cut leaves the caller's output untouched");
    }

    struct EventRow {
        const char* label;
        rhi::FrameEvent event;
        rhi::MotionValidity expected;
    };
    const EventRow events[] = {
        {"teleport invalidates", rhi::FrameEvent::Teleport, rhi::MotionValidity::Reset},
        {"level load invalidates", rhi::FrameEvent::LevelLoad,
         rhi::MotionValidity::Reset},
        {"provider switch invalidates", rhi::FrameEvent::ProviderSwitch,
         rhi::MotionValidity::ProviderSwitch},
    };
    for (const EventRow& row : events) {
        rhi::FrameIntegration frame;
        std::string error;
        frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()}, &error);
        frame.CommitView(View(rhi::ViewKind::Main, identity, 0.0));
        runner->Check(frame.NotifyEngineEvent(row.event),
                      std::string(row.label) + " event applied");
        rhi::RenderedViewState previous;
        runner->Validity(row.label,
                         frame.Previous(rhi::ViewKind::Main, identity, &previous),
                         row.expected);
    }

    // Resize: extent change invalidates history and reallocates the per-pixel
    // resource, so an old sample cannot survive as a valid zero.
    {
        rhi::FrameIntegration frame;
        std::string error;
        frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()}, &error);
        frame.CommitView(View(rhi::ViewKind::Main, identity, 0.0));
        rhi::MotionSample sample;
        sample.validity = rhi::MotionValidity::Valid;
        sample.vector.du = 0.25;
        frame.StorePerPixelMotion(5u, 5u, sample);
        const uint64_t before = frame.motion().generation();
        runner->Check(frame.Resize(rhi::FrameExtent{1280u, 720u},
                                   rhi::FrameExtent{1920u, 1080u}, &error) ==
                          rhi::Status::Ok,
                      "resize applied");
        runner->Check(frame.motion().generation() > before,
                      "resize advanced the per-pixel generation");
        runner->Check(!frame.LoadPerPixelMotion(5u, 5u).valid,
                      "resize invalidated the stored sample");
        rhi::RenderedViewState previous;
        runner->Validity("resize invalidates history",
                         frame.Previous(rhi::ViewKind::Main, identity, &previous),
                         rhi::MotionValidity::Resize);
    }

    // Entity reuse and the remaining identity classes.
    {
        rhi::FrameIntegration frame;
        std::string error;
        frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()}, &error);
        frame.CommitView(View(rhi::ViewKind::Main, identity, 0.5));

        rhi::ViewIdentity reused = identity;
        reused.entity = identity.entity + 1;
        rhi::RenderedViewState previous;
        runner->Validity("entity reuse invalidates",
                         frame.Previous(rhi::ViewKind::Main, reused, &previous),
                         rhi::MotionValidity::EntityReuse);

        rhi::ViewIdentity newTopology = identity;
        newTopology.topology = identity.topology + 1;
        runner->Validity("topology change invalidates",
                         frame.Previous(rhi::ViewKind::Main, newTopology,
                                        &previous),
                         rhi::MotionValidity::Topology);

        rhi::ViewIdentity newGeneration = identity;
        newGeneration.generation = identity.generation + 1;
        runner->Validity("view generation change invalidates",
                         frame.Previous(rhi::ViewKind::Main, newGeneration,
                                        &previous),
                         rhi::MotionValidity::Generation);

        runner->Check(frame.CommitView(View(rhi::ViewKind::Main, reused, 0.75)),
                      "recommit after entity reuse accepted");
        if (frame.Previous(rhi::ViewKind::Main, reused, &previous) ==
                rhi::MotionValidity::Valid &&
            previous.records.size() == 1) {
            runner->Check(SamePoint(previous.records[0].point,
                                    P(100.0, 0.0, 0.75)),
                          "entity reuse does not return the old slot's pose");
        } else {
            runner->Check(false, "record after entity reuse");
        }
    }

    // A cut on an unrendered view stays NoHistory; an accepted reason that is
    // not an invalidation is a no-op.
    {
        rhi::FrameIntegration frame;
        std::string error;
        frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()}, &error);
        frame.NotifyEngineEvent(rhi::FrameEvent::Cut);
        rhi::RenderedViewState previous;
        runner->Validity("cut on an unrendered view is no-history",
                         frame.Previous(rhi::ViewKind::Main, identity, &previous),
                         rhi::MotionValidity::NoHistory);

        frame.CommitView(View(rhi::ViewKind::Main, identity, 0.0));
        frame.InvalidateView(rhi::ViewKind::Main, identity,
                             rhi::MotionValidity::Valid);
        runner->Validity("valid reason is a no-op",
                         frame.Previous(rhi::ViewKind::Main, identity, &previous),
                         rhi::MotionValidity::Valid);
    }
}

void RunPrivatePose(Runner* runner) {
    rhi::FrameIntegration frame;
    std::string error;
    runner->Check(frame.Initialize(rhi::FrameIntegrationDesc{Render(), Display()},
                                   &error) == rhi::Status::Ok,
                  "private pose integration initialized");

    std::vector<rhi::MotionVec3> shared;
    shared.push_back(P(100.0, 0.0, 1.0));
    runner->Check(frame.CommitPrivatePose(rhi::ViewKind::Main, 7u, 3u, shared),
                  "private pose committed");
    // The engine's shared dynamic model is rewritten after the commit; the
    // stored pose must be a copy, not an alias.
    shared[0] = P(0.0, 0.0, 0.0);

    std::vector<rhi::MotionVec3> previous;
    runner->Validity("private pose survives shared-cache rewrite",
                     frame.PreviousPrivatePose(rhi::ViewKind::Main, 7u, 3u,
                                               &previous),
                     rhi::MotionValidity::Valid);
    if (previous.size() == 1u) {
        runner->Check(SamePoint(previous[0], P(100.0, 0.0, 1.0)),
                      "private pose is the committed copy");
    } else {
        runner->Check(false, "private pose size");
    }

    std::vector<rhi::MotionVec3> mirrorPose;
    mirrorPose.push_back(P(100.0, 0.0, 0.5));
    frame.CommitPrivatePose(rhi::ViewKind::Mirror, 7u, 3u, mirrorPose);
    if (frame.PreviousPrivatePose(rhi::ViewKind::Main, 7u, 3u, &previous) ==
            rhi::MotionValidity::Valid &&
        previous.size() == 1u) {
        runner->Check(SamePoint(previous[0], P(100.0, 0.0, 1.0)),
                      "mirror pose does not replace the main pose");
    } else {
        runner->Check(false, "main pose after mirror commit");
    }

    runner->Validity("stale pose generation rejected",
                     frame.PreviousPrivatePose(rhi::ViewKind::Main, 7u, 4u,
                                               &previous),
                     rhi::MotionValidity::Generation);
    runner->Check(!frame.CommitPrivatePose(rhi::ViewKind::Main, 7u, 4u, {}),
                  "empty pose is rejected");
}

}  // namespace

int main() {
    Runner runner;
    RunFixtureVectors(&runner);
    RunQueueOrdering(&runner);
    RunHistoryAndMotion(&runner);
    RunInvalidation(&runner);
    RunPrivatePose(&runner);
    return runner.Finish("q3.frame_integration");
}
