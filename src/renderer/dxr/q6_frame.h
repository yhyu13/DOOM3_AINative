#pragma once

// Q6 engine-seam frame: the Q2 ordered draw stream, the Q3 current/previous pose
// and the Q3 per-pixel motion/validity, bound to one reproducible frame identity
// (spec 004 T069/T070).
//
// This is the superproject-side integration the Q6 capture consumes. It is not a
// new renderer and it does not replace the Q2/Q3 core:
//
//   * `Consume` walks the exact `SceneCommandQueue` the engine sink produced, in
//     stream order, and resolves each draw through the caller's
//     `SceneGeometrySource`. A display overlay declared before the scene is
//     refused, and a draw whose geometry is unavailable is counted, never
//     substituted with an empty mesh.
//   * `BeginFrame` commits the rendered view and the private pose through
//     `FrameIntegration`, so the current/previous correspondence is the Q3 one.
//   * `BuildMotion` turns the current/previous posed vertices and the device's
//     per-pixel hit reference into one `MotionSample` per pixel through
//     `ReprojectPoint`, and stores it in the Q3 `HistoryBuffer`. Validity stays a
//     reason, never a sentinel vector.
//   * `identity` records every field a reviewer needs to attribute a capture to a
//     frame: view id, frame index, extents, camera, draw order, pose and motion
//     generation, reset reason and content cells.
//
// No native type, handle or device value appears in this header, so the seam is
// compiled and tested without a DXR device.

#include "dxr/q6_content.h"
#include "rhi/engine_scene_adapter.h"
#include "rhi/frame_integration.h"
#include "rhi/motion_history.h"
#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace dxr {

// One draw from the Q2 stream, after geometry resolution, in stream order.
struct Q6OrderedSurface {
    uint32_t order = 0;
    uint32_t surfaceId = 0;
    uint32_t materialId = 0;
    Q6ContentCell cell = Q6ContentCell::Dielectric;
    bool alphaTested = false;
    bool translucent = false;
    // Object-space triangle list: three Vertex per triangle. `transform` is the
    // packet's model transform, row-major like `SceneSurfaceGeometry`.
    std::vector<rhi::Vertex> vertices;
    float transform[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f};
};

// One light packet. The classic packet carries identity, order and shadow flags;
// it does not carry a radiometric position or radiance, so the emitter values
// come from the declared PT scene rig. That gap is recorded, not hidden.
struct Q6OrderedLight {
    uint32_t order = 0;
    uint32_t lightId = 0;
    bool pointLight = false;
    bool noShadows = false;
    float falloffRadius = 0.0f;
};

// The caller's surface -> content assignment comes from `q6_content.h`; a
// surface with no assignment is refused, because a capture must name the
// content cell it renders.

struct Q6SeamStats {
    uint32_t views = 0;
    uint32_t overlays = 0;
    uint32_t draws = 0;
    uint32_t drawsResolved = 0;
    uint32_t drawsRefused = 0;
    uint32_t lights = 0;
    bool orderValid = false;
};

// The Q3 invalidation events, plus an explicit "this frame has no event". The
// Q3 `FrameEvent` has no none-value, so the seam names it here instead of
// inventing a numeric sentinel.
enum class Q6FrameEvent : uint32_t {
    None = 0,
    Cut,
    Teleport,
    LevelLoad,
    ProviderSwitch,
};

rhi::MotionValidity ReasonForEvent(Q6FrameEvent event);

// One frame a capture can cite. `hash` is FNV-1a over the named fields, so two
// runs can be compared without re-rendering.
struct Q6FrameIdentity {
    std::string scene;       // PT-S1..PT-S5
    std::string sceneName;   // static-convergence, alpha-emissive, ...
    uint32_t viewId = 0;
    uint32_t frameIndex = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    rhi::CameraState camera;
    uint32_t poseIdentity = 0;
    uint32_t poseGeneration = 0;
    uint32_t motionGeneration = 0;
    rhi::MotionValidity resetReason = rhi::MotionValidity::NoHistory;
    std::vector<uint32_t> drawOrder;
    std::vector<Q6ContentCell> cells;
    uint64_t hash = 0;
    bool historyValid = false;

    std::string Describe() const;
};

// The current and previous world-space posed triangle list for one surface.
struct Q6SurfacePose {
    uint32_t surfaceId = 0;
    std::vector<rhi::MotionVec3> current;
    std::vector<rhi::MotionVec3> previous;
};

struct Q6MotionStats {
    uint32_t pixels = 0;
    uint32_t validPixels = 0;
    uint32_t invalidPixels = 0;
    uint32_t disoccludedPixels = 0;
    uint32_t behindCameraPixels = 0;
};

// Per-pixel device readback: the instance (surface index in `surfaces`), the
// primitive index and the two barycentric weights. Barycentric weights are
// `(u, v)` such that the hit is `v0*(1-u-v) + v1*u + v2*v`.
struct Q6HitReference {
    uint32_t surface = 0;
    uint32_t primitive = 0;
    float baryU = 0.0f;
    float baryV = 0.0f;
};

class Q6EngineFrame {
public:
    // Names the PT identity this frame belongs to (PT-S1..PT-S5). Called before
    // BeginFrame so the identity hash and the content record carry it.
    void SetScene(const std::string& identity, const std::string& name) {
        identity_.scene = identity;
        identity_.sceneName = name;
    }

    // Walks the queue in stream order. A display overlay before the scene view,
    // a draw with no geometry, or a surface with no content assignment is
    // refused and counted; `error()` says which one.
    bool Consume(const rhi::SceneCommandQueue& queue, rhi::SceneGeometrySource* geometry,
                 const std::vector<Q6SurfaceAssignment>& assignments, std::string* error);

    // Commits the view and the private pose through the Q3 integration, then
    // captures the identity. `event` is the frame's invalidation reason (a cut,
    // a teleport, a level load, a provider switch or none). `historyValid` says
    // whether the previous frame produced usable history (false on the first
    // frame and after a reset).
    bool BeginFrame(rhi::FrameIntegration* integration, const rhi::CameraState& camera,
                    uint32_t viewId, uint32_t frameIndex, const rhi::ViewIdentity& identity,
                    const std::vector<rhi::RenderRecord>& records,
                    const std::vector<rhi::MotionVec3>& currentPose, uint32_t poseIdentity,
                    uint32_t poseGeneration, Q6FrameEvent event, bool historyValid,
                    std::string* error);

    // One `MotionSample` per pixel from the current/previous posed surfaces and
    // the device hit reference. A pixel with no hit, no finite barycentrics or a
    // previous point outside the frame is stored invalid with its reason. The
    // stores go through `FrameIntegration::StorePerPixelMotion`, so the Q3
    // validity vocabulary and the Q3 per-pixel generation are the ones used.
    bool BuildMotion(rhi::FrameIntegration* integration, const rhi::CameraState& previousCamera,
                     const Q6SurfacePose* surfaces, uint32_t surfaceCount,
                     const Q6HitReference* hits, uint32_t width, uint32_t height,
                     Q6MotionStats* stats, std::string* error);

    // Hashes the consumed stream and the camera without committing a view, so a
    // still reference capture can cite a frame identity. The hash is the same
    // FNV-1a input BeginFrame uses.
    uint64_t ComputeIdentityHash(const rhi::CameraState& camera, uint32_t frameIndex) const;

    const Q6FrameIdentity& identity() const { return identity_; }
    const Q6SeamStats& stats() const { return stats_; }
    const std::vector<Q6OrderedSurface>& surfaces() const { return surfaces_; }
    const std::vector<Q6OrderedLight>& lights() const { return lights_; }
    const std::string& error() const { return error_; }

private:
    bool Refuse(const std::string& reason);

    std::vector<Q6OrderedSurface> surfaces_;
    std::vector<Q6OrderedLight> lights_;
    Q6SeamStats stats_;
    Q6FrameIdentity identity_;
    std::string error_;
    bool consumed_ = false;
};

// The content cell a PT identity names last (the hero cell) and the draw-order
// label used by the identity hash. Exposed so a test can check the mapping
// without building a device frame.
const char* PtSceneName(const std::string& identity);

}  // namespace dxr
}  // namespace doom3

