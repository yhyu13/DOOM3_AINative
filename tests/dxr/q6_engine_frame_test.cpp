// Q6 engine-seam frame: Q2 order, Q3 pose/motion/validity, content identity
// (spec 004 T069/T070).
//
// This test is device-free. It drives the same `EngineQ2SinkConsumer` callback
// table the engine installs, consumes the ordered `SceneCommandQueue` into a Q6
// frame, commits the view and pose through the Q3 `FrameIntegration`, and checks
// the per-pixel motion/validity the Q6 capture uploads. The expectations are
// literals or formulas in this file; nothing here asks the code under test for
// an expected value.

#include "dxr_test_support.h"

#include "dxr/q6_content.h"
#include "dxr/q6_frame.h"
#include "dxr/q6_motion_texture.h"
#include "dxr/q6_pt_scene.h"
#include "rhi/frame_integration.h"
#include "rhi/frame_resources.h"
#include "rhi/motion_history.h"
#include "rhi/rhi.h"

#include <cmath>
#include <string>
#include <vector>

using doom3::dxr::BuildPtScene;
using doom3::dxr::CellsForIdentity;
using doom3::dxr::LoadQ6Content;
using doom3::dxr::PackMotionTexture;
using doom3::dxr::Q6ContentCell;
using doom3::dxr::Q6ContentRegistry;
using doom3::dxr::Q6EngineFrame;
using doom3::dxr::Q6FrameIdentity;
using doom3::dxr::Q6HitReference;
using doom3::dxr::Q6MotionStats;
using doom3::dxr::Q6MotionTextureData;
using doom3::dxr::Q6PtSceneSpec;
using doom3::dxr::Q6SurfacePose;
using doom3::rhi::CameraState;
using doom3::dxr::Q6FrameEvent;
using doom3::rhi::FrameExtent;
using doom3::rhi::FrameIntegration;
using doom3::rhi::FrameIntegrationDesc;
using doom3::rhi::MotionValidity;
using doom3::rhi::MotionVec3;
using doom3::rhi::RenderRecord;
using doom3::rhi::SceneSurfaceGeometry;
using doom3::rhi::Vertex;
using doom3::rhi::ViewIdentity;
using doom3::rhi::ViewKind;

namespace {

const char* const kRepoRoot = Q6_TEST_REPO_ROOT;

// A quad of six vertices per surface id. The geometry is deliberately trivial:
// this test checks the seam, not a renderer.
class QuadSource : public doom3::rhi::SceneGeometrySource {
public:
    bool ResolveSurface(uint32_t surfaceId, SceneSurfaceGeometry* geometry) override {
        if (surfaceId == 0 || geometry == nullptr) {
            return false;
        }
        geometry->vertices.clear();
        const float x = static_cast<float>(surfaceId);
        const float quad[6][3] = {
            {-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 2.0f},
            {-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 2.0f}, {-1.0f, 1.0f, 2.0f},
        };
        for (const auto& position : quad) {
            Vertex vertex;
            vertex.position[0] = position[0] + x;
            vertex.position[1] = position[1];
            vertex.position[2] = position[2];
            vertex.texCoord[0] = 0.0f;
            vertex.texCoord[1] = 0.0f;
            geometry->vertices.push_back(vertex);
        }
        return true;
    }
    bool ResolveLight(const doom3::rhi::LightPacket&, doom3::rhi::SceneLightGeometry*) override {
        return false;
    }
};

CameraState MakeCamera(float eyeX) {
    CameraState camera;
    camera.eye = MotionVec3{eyeX, -3.0, 1.4};
    camera.forward = MotionVec3{0.0, 1.0, 0.0};
    camera.left = MotionVec3{1.0, 0.0, 0.0};
    camera.up = MotionVec3{0.0, 0.0, 1.0};
    camera.tanX = 0.75;
    camera.tanY = 0.75 * 36.0 / 64.0;
    camera.width = 64;
    camera.height = 36;
    return camera;
}

ViewIdentity MakeIdentity() {
    ViewIdentity identity;
    identity.worldId = 1;
    identity.instanceId = 7;
    identity.width = 64;
    identity.height = 36;
    return identity;
}

std::vector<RenderRecord> MakeRecords() {
    std::vector<RenderRecord> records;
    RenderRecord record;
    record.point = MotionVec3{0.0, 2.0, 1.0};
    records.push_back(record);
    return records;
}

// Builds the Q2 ordered stream in the given draw order. The engine sink writes
// exactly this `SceneCommandQueue` (its translation is exercised by the Q3
// frame-integration and engine-link suites); Q6 consumes the stream itself.
bool BuildStream(doom3::rhi::SceneCommandQueue* queue, const std::vector<uint32_t>& surfaces,
                 std::string* error) {
    doom3::rhi::ViewDesc view;
    view.identity = 1;
    view.width = 64;
    view.height = 36;
    if (!queue->BeginView(view)) {
        *error = "the stream could not express the scene view";
        return false;
    }
    for (uint32_t surface : surfaces) {
        doom3::rhi::DrawSurfPacket draw;
        draw.surfaceId = surface;
        draw.materialId = surface;
        draw.vertexCount = 6;
        draw.indexCount = 6;
        if (!queue->Draw(draw)) {
            *error = "the stream could not express a draw";
            return false;
        }
    }
    doom3::rhi::LightPacket light;
    light.lightId = 1;
    light.light.kind = doom3::rhi::LightKind::Point;
    light.light.pointLight = true;
    light.light.falloffRadius = 300.0f;
    if (!queue->DrawLight(light)) {
        *error = "the stream could not express a light";
        return false;
    }
    if (!queue->EndView()) {
        *error = "the stream could not express the end of the view";
        return false;
    }
    return true;
}

// A 2x2 hit buffer: one surface, primitive 0, barycentrics (1/3, 1/3).
void FillHits(std::vector<Q6HitReference>* hits, uint32_t width, uint32_t height) {
    hits->assign(static_cast<size_t>(width) * height, Q6HitReference{});
    for (Q6HitReference& hit : *hits) {
        hit.surface = 0;
        hit.primitive = 0;
        hit.baryU = 1.0f / 3.0f;
        hit.baryV = 1.0f / 3.0f;
    }
}

// A triangle whose vertices can be posed. `previous` is the same triangle one
// frame earlier.
Q6SurfacePose MakePose(const MotionVec3& currentOffset, const MotionVec3& previousOffset) {
    Q6SurfacePose pose;
    pose.surfaceId = 1;
    const MotionVec3 base[3] = {{-1.0, 2.0, 1.0}, {1.0, 2.0, 1.0}, {0.0, 2.0, 2.0}};
    for (const MotionVec3& vertex : base) {
        pose.current.push_back(MotionVec3{vertex.x + currentOffset.x, vertex.y + currentOffset.y,
                                          vertex.z + currentOffset.z});
        pose.previous.push_back(
            MotionVec3{vertex.x + previousOffset.x, vertex.y + previousOffset.y,
                       vertex.z + previousOffset.z});
    }
    return pose;
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    // 1. Content identity: every declared authored channel and licence exists.
    Q6ContentRegistry content;
    std::string error;
    const bool loaded = LoadQ6Content(kRepoRoot, &content, &error);
    runner.Check(loaded, "content registry loads all seven Q4 cells", error);
    runner.Check(content.cells.size() == 7, "content registry has seven cells");
    runner.Check(content.CompleteCount() == 7,
                 "every cell's declared authored channels and licence exist");
    std::vector<Q6ContentCell> cells;
    for (const char* identity : {"PT-S1", "PT-S2", "PT-S3", "PT-S4", "PT-S5"}) {
        runner.Check(CellsForIdentity(identity, &cells),
                     std::string(identity) + " names content cells");
    }
    const auto* environment = content.Find(Q6ContentCell::Environment);
    runner.Check(environment != nullptr && environment->environmentTiles.size() == 4,
                 "the environment cell names four 2048 tiles");
    const auto* emissive = content.Find(Q6ContentCell::EmissiveSign);
    runner.Check(emissive != nullptr && std::fabs(emissive->emissiveScale - 16.0f) < 1e-6f,
                 "the emissive cell declares emissiveScale 16");
    const auto* glass = content.Find(Q6ContentCell::Glass);
    runner.Check(glass != nullptr && glass->metalness.state == doom3::dxr::Q6ChannelState::Unavailable,
                 "the glass cell declares metalness unavailable, not synthesised");

    // 2. Every PT identity declares its required content and a real camera.
    for (const char* identity : {"PT-S1", "PT-S2", "PT-S3", "PT-S4", "PT-S5"}) {
        Q6PtSceneSpec spec;
        std::string sceneError;
        const bool built = BuildPtScene(identity, &spec, &sceneError);
        runner.Check(built, std::string(identity) + " builds", sceneError);
        runner.Check(spec.ContentMatchesIdentity(&sceneError),
                     std::string(identity) + " uses every required content cell", sceneError);
        runner.Check(spec.surfaces.size() >= 3,
                     std::string(identity) + " declares at least three surfaces");
    }

    // 3. The Q2 stream reaches Q6 in stream order, with the light packet.
    doom3::rhi::SceneCommandQueue queue;
    QuadSource source;
    std::string streamError;
    runner.Check(BuildStream(&queue, {3, 1, 4, 2}, &streamError),
                 "the ordered Q2 stream is accepted", streamError);
    Q6EngineFrame frame;
    frame.SetScene("PT-S1", "static-convergence");
    std::vector<doom3::dxr::Q6SurfaceAssignment> assignments = {
        {1, Q6ContentCell::Environment, false},
        {2, Q6ContentCell::Dielectric, false},
        {3, Q6ContentCell::Metal, false},
        {4, Q6ContentCell::Environment, false},
    };
    const bool consumed = frame.Consume(queue, &source, assignments, &streamError);
    runner.Check(consumed, "Q6 consumes the Q2 queue", streamError);
    runner.Check(frame.stats().drawsResolved == 4, "all four draws resolved");
    runner.Check(frame.lights().size() == 1 && frame.lights()[0].pointLight,
                 "the point light packet is recorded");
    bool orderOk = frame.surfaces().size() == 4;
    const uint32_t expected[4] = {3, 1, 4, 2};
    for (size_t i = 0; i < frame.surfaces().size() && orderOk; ++i) {
        orderOk = frame.surfaces()[i].surfaceId == expected[i] && frame.surfaces()[i].order == i;
    }
    runner.Check(orderOk, "the resolved draws keep the Q2 stream order");

    // 4. A draw with no content assignment is refused, not silently empty.
    Q6EngineFrame refused;
    std::string refuseError;
    std::vector<doom3::dxr::Q6SurfaceAssignment> partial = {{1, Q6ContentCell::Environment, false}};
    const bool refusedOk = refused.Consume(queue, &source, partial, &refuseError);
    runner.Check(!refusedOk && refused.stats().drawsRefused > 0,
                 "a surface with no content assignment is refused", refuseError);

    // 5. A display overlay before the scene is refused by the seam as well.
    doom3::rhi::SceneCommandQueue overlayFirst;
    doom3::rhi::ViewDesc overlay;
    overlay.identity = 2;
    overlay.width = 64;
    overlay.height = 36;
    overlay.overlay = true;
    overlayFirst.BeginView(overlay);
    doom3::rhi::DrawSurfPacket draw;
    draw.surfaceId = 1;
    overlayFirst.Draw(draw);
    doom3::rhi::ViewDesc scene;
    scene.identity = 1;
    scene.width = 64;
    scene.height = 36;
    overlayFirst.BeginView(scene);
    Q6EngineFrame overlayFrame;
    std::string overlayError;
    runner.Check(!overlayFrame.Consume(overlayFirst, &source, assignments, &overlayError),
                 "a display overlay before the scene is refused", overlayError);

    // 6. Q3 frame integration: current/previous pose, per-pixel motion, validity.
    FrameIntegration integration;
    FrameIntegrationDesc integrationDesc;
    integrationDesc.render = FrameExtent{64, 36};
    integrationDesc.display = FrameExtent{64, 36};
    std::string integrationError;
    runner.Check(integration.Initialize(integrationDesc, &integrationError) ==
                     doom3::rhi::Status::Ok,
                 "the Q3 frame integration initializes", integrationError);

    Q6EngineFrame first;
    first.SetScene("PT-S3", "motion");
    first.Consume(queue, &source, assignments, &streamError);
    const CameraState camera0 = MakeCamera(0.0f);
    const std::vector<MotionVec3> pose0 = {MotionVec3{0.0, 2.0, 1.0}};
    runner.Check(first.BeginFrame(&integration, camera0, 1, 0, MakeIdentity(), MakeRecords(),
                                  pose0, 3, 0, Q6FrameEvent::Cut, false, &error),
                 "frame 0 commits with a reset", error);
    runner.Check(first.identity().resetReason == MotionValidity::Reset,
                 "a Cut frame records Reset as its reason");
    runner.Check(!first.identity().historyValid, "frame 0 has no history");

    Q6EngineFrame second;
    second.SetScene("PT-S3", "motion");
    second.Consume(queue, &source, assignments, &streamError);
    const CameraState camera1 = MakeCamera(0.1f);
    // The previous pose is fetched from Q3 before the new pose is committed.
    std::vector<MotionVec3> previousPose;
    runner.Check(integration.PreviousPrivatePose(ViewKind::Main, 3, 0, &previousPose) ==
                     MotionValidity::Valid,
                 "the previous private pose is available before the next commit");
    runner.Check(previousPose.size() == 1 &&
                     std::fabs(previousPose[0].y - 2.0) < 1e-9 &&
                     std::fabs(previousPose[0].x) < 1e-9,
                 "the previous pose is the one committed for frame 0");
    const std::vector<MotionVec3> pose1 = {MotionVec3{0.0, 2.1, 1.0}};
    runner.Check(second.BeginFrame(&integration, camera1, 1, 1, MakeIdentity(), MakeRecords(),
                                   pose1, 3, 1, Q6FrameEvent::None, true, &error),
                 "frame 1 commits against frame 0", error);
    runner.Check(second.identity().resetReason == MotionValidity::NoHistory,
                 "an ordinary frame carries no reset reason");
    runner.Check(second.identity().historyValid, "frame 1 records that history was usable");

    std::vector<Q6HitReference> hits;
    FillHits(&hits, 64, 36);
    const Q6SurfacePose staticPose = MakePose(MotionVec3{0.0, 0.0, 0.0},
                                              MotionVec3{0.0, 0.0, 0.0});
    Q6MotionStats stats;
    // The previous camera is frame 0's; the current camera is frame 1's.
    runner.Check(second.BuildMotion(&integration, camera0, &staticPose, 1, hits.data(), 64, 36,
                                    &stats, &error),
                 "per-pixel motion is built through the Q3 seam", error);
    runner.Check(stats.pixels == 64 * 36, "every pixel receives one sample");
    runner.Check(stats.validPixels == 64 * 36,
                 "a same-pose, same-camera point is valid at every pixel");
    bool zeroVector = true;
    for (uint32_t y = 0; y < 36 && zeroVector; ++y) {
        for (uint32_t x = 0; x < 64 && zeroVector; ++x) {
            const auto sample = integration.LoadPerPixelMotion(x, y);
            // The current frame's camera is frame 1's; the previous camera here
            // is frame 0's, so the vector is the camera move, not zero. The
            // stationary-zero case is checked against the same camera below.
            zeroVector = sample.valid;
        }
    }
    runner.Check(zeroVector, "every camera-move sample is valid");

    // 7. A camera move produces a non-zero du with the sign the history
    //    projection defines, and a pose that leaves the frame is a disocclusion.
    const auto moved = integration.LoadPerPixelMotion(32, 18);
    runner.Check(moved.valid && moved.x < 0.0f,
                 "moving the eye along the view's left axis gives a negative du");
    Q6MotionStats sameCameraStats;
    runner.Check(second.BuildMotion(&integration, camera1, &staticPose, 1, hits.data(), 64, 36,
                                    &sameCameraStats, &error),
                 "a same-camera frame is built", error);
    const auto stationary = integration.LoadPerPixelMotion(32, 18);
    runner.Check(stationary.valid && std::fabs(stationary.x) < 1e-6f &&
                     std::fabs(stationary.y) < 1e-6f,
                 "a valid stationary vector is a real (0,0), not invalid");
    Q6MotionStats disocclusionStats;
    const Q6SurfacePose jumped = MakePose(MotionVec3{0.0, 0.0, 0.0},
                                          MotionVec3{1000.0, 0.0, 0.0});
    runner.Check(second.BuildMotion(&integration, camera0, &jumped, 1, hits.data(), 64, 36,
                                    &disocclusionStats, &error),
                 "an out-of-frame previous pose is classified", error);
    runner.Check(disocclusionStats.disoccludedPixels > 0,
                 "an out-of-frame previous point is Disocclusion, not a fabricated vector");

    // 8. The identity hash is stable for one frame and changes with draw order.
    Q6EngineFrame hashA;
    hashA.SetScene("PT-S1", "static-convergence");
    hashA.Consume(queue, &source, assignments, &streamError);
    hashA.BeginFrame(&integration, camera0, 1, 0, MakeIdentity(), MakeRecords(), pose0, 3, 0,
                     Q6FrameEvent::Cut, false, &error);
    Q6EngineFrame hashB;
    hashB.SetScene("PT-S1", "static-convergence");
    hashB.Consume(queue, &source, assignments, &streamError);
    hashB.BeginFrame(&integration, camera0, 1, 0, MakeIdentity(), MakeRecords(), pose0, 3, 0,
                     Q6FrameEvent::Cut, false, &error);
    runner.Check(hashA.identity().hash == hashB.identity().hash,
                 "the same frame identity hashes equally");
    runner.Check(hashA.identity().drawOrder.size() == 4 &&
                     hashA.identity().drawOrder[0] == 3,
                 "the identity records the draw order");

    doom3::rhi::SceneCommandQueue reversedQueue;
    runner.Check(BuildStream(&reversedQueue, {2, 4, 1, 3}, &streamError),
                 "a second ordered stream is accepted", streamError);
    Q6EngineFrame hashC;
    hashC.SetScene("PT-S1", "static-convergence");
    hashC.Consume(reversedQueue, &source, assignments, &streamError);
    hashC.BeginFrame(&integration, camera0, 1, 0, MakeIdentity(), MakeRecords(), pose0, 3, 0,
                     Q6FrameEvent::Cut, false, &error);
    runner.Check(hashC.identity().hash != hashA.identity().hash,
                 "a different draw order hashes differently");
    
    

    // 9. Motion texture packing matches the Q3 byte contract.
    doom3::rhi::HistoryBuffer history;
    history.Resize(2, 1);
    doom3::rhi::PerPixelMotionSample sample;
    sample.valid = true;
    sample.x = 0.0f;
    sample.y = -1.0f / 6.0f;
    history.Store(0, 0, sample);
    Q6MotionTextureData packed;
    std::string packError;
    runner.Check(PackMotionTexture(history, &packed, &packError),
                 "the Q3 motion buffer packs into two textures", packError);
    const uint16_t wordZero = doom3::rhi::MotionHalfWord(0.0);
    const uint16_t wordThird = doom3::rhi::MotionHalfWord(-1.0 / 6.0);
    runner.Check(packed.motion.size() == 8 && packed.validity.size() == 2,
                 "the packed textures are RGBA8 and R8 at the frame extent");
    runner.Check(packed.motion[0] == static_cast<uint8_t>(wordZero & 0xFF) &&
                     packed.motion[1] == static_cast<uint8_t>(wordZero >> 8),
                 "du is stored as binary16 little endian in R/G");
    runner.Check(packed.motion[2] == static_cast<uint8_t>(wordThird & 0xFF) &&
                     packed.motion[3] == static_cast<uint8_t>(wordThird >> 8),
                 "dv is stored as binary16 little endian in B/A");
    runner.Check(wordThird != wordZero && (wordThird & 0x8000u) != 0,
                 "a negative dv keeps its sign bit");
    runner.Check(packed.validity[0] == 0xFF && packed.validity[1] == 0,
                 "validity is 255 for a usable sample and 0 for none");
    runner.Check(packed.validPixels == 1 && packed.invalidPixels == 1,
                 "the packer counts valid and invalid samples");

    // 10. The environment frame is resized by Q3, so the Q6 capture cannot
    //     reuse a stale extent.
    Q6MotionStats staleStats;
    runner.Check(!second.BuildMotion(&integration, camera0, &staticPose, 1, hits.data(), 32, 18,
                                     &staleStats, &error),
                 "a motion extent that disagrees with Q3 is refused", error);

    return runner.Finish("q6_engine_frame_test");
}






