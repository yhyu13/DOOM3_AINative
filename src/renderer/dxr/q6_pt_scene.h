#pragma once

// Declared PT-S1..S5 scenes for the Q6 path-traced reference/showcase (spec 004
// T069/T070).
//
// The rubric fixes five identities so two people can reproduce them. This unit
// is that fixed description: geometry primitives, content cell per surface, the
// key emitter, the environment radiance, the camera keys and the motion/cut
// schedule. It carries no device type and no texture bytes, so both the portable
// test and the DXR capture build the same scene from the same declaration.
//
// Geometry primitives are named rather than hidden in the capture tool: the tool
// tessellates `Quad`, `Box`, `Sphere` and `Sheet` deterministically. A change to
// a declared number changes the identity hash of every capture of that scene.

#include "dxr/q6_content.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace dxr {

enum class Q6Primitive : uint32_t {
    Quad = 0,
    Box,
    Sphere,
    Sheet,
};

const char* ToString(Q6Primitive primitive);

// A quad's normal axis and its two in-plane axes are derived from `axis`:
//   axis 0 (+x):  u = +y, v = +z   (uv mode yz)
//   axis 1 (+y):  u = +x, v = +z   (uv mode xz, normal -y so it faces the camera)
//   axis 2 (+z):  u = +x, v = +y   (uv mode xy)
struct Q6SurfaceSpec {
    uint32_t surfaceId = 0;
    Q6ContentCell cell = Q6ContentCell::Dielectric;
    Q6Primitive primitive = Q6Primitive::Quad;
    float center[3] = {0.0f, 0.0f, 0.0f};
    // Quad/sheet half extents (`half[0]`, `half[1]`), box half sizes (all
    // three), or sphere radius in `half[0]`.
    float half[3] = {1.0f, 1.0f, 1.0f};
    int axis = 1;
    bool alphaTest = false;
    bool skinned = false;
    bool moving = false;
    // 1 = xz, 2 = xy, 3 = yz. Planar texture projection, declared per surface.
    int uvMode = 2;
    float uvScale = 0.25f;
    float uvOrigin[2] = {0.0f, 0.0f};
    // Rigid translation applied at `frame` for a moving surface.
    float moveDelta[3] = {0.0f, 0.0f, 0.0f};
    // A teleport applied from `teleportFrame` on, so a cut frame has a real
    // discontinuity to test the history reset against.
    uint32_t teleportFrame = 0xFFFFFFFFu;
    float teleportDelta[3] = {0.0f, 0.0f, 0.0f};
};

struct Q6CameraSpec {
    float position[3] = {0.0f, -3.0f, 1.4f};
    float forward[3] = {0.0f, 1.0f, 0.0f};
    float tanHalfFovX = 0.75f;
};

struct Q6PointEmitter {
    bool enabled = true;
    float position[3] = {0.0f, 0.0f, 3.0f};
    float radiance[3] = {10.0f, 10.0f, 10.0f};
    float radius = 0.0f;
};

struct Q6PtSceneSpec {
    std::string identity;   // PT-S1..PT-S5
    std::string name;       // static-convergence, ...
    std::vector<Q6SurfaceSpec> surfaces;
    Q6PointEmitter key;
    float environmentUpper[3] = {0.10f, 0.12f, 0.16f};
    float environmentLower[3] = {0.03f, 0.03f, 0.03f};
    Q6CameraSpec camera0;
    // Used by the slow camera move / dolly identities. Equal to camera0 when the
    // identity is still.
    Q6CameraSpec camera1;
    uint32_t maxDepth = 6;
    // Frame schedule: frames 0..lastFrame inclusive. `cutFrame` is the frame
    // that receives a cut/teleport event (UINT32_MAX means none).
    uint32_t lastFrame = 0;
    uint32_t cutFrame = 0xFFFFFFFFu;
    // Camera interpolation between camera0 (frame 0) and camera1 (lastFrame).
    bool cameraMoves = false;

    const Q6SurfaceSpec* FindSurface(uint32_t surfaceId) const;
    std::vector<Q6SurfaceAssignment> Assignments() const;
    // True when every surface names one of the cells `CellsForIdentity` requires.
    bool ContentMatchesIdentity(std::string* error) const;
};

// Builds one declared identity. Unknown names fail instead of returning an empty
// scene.
bool BuildPtScene(const std::string& identity, Q6PtSceneSpec* scene, std::string* error);

// Every identity name, in capture order.
const char* const* PtIdentities(size_t* count);

// Camera at `frame`, linearly interpolated between the two keys when the
// identity moves. `tanY` is derived from the render extent (the engine's
// 16:9 seed scales with the height/width ratio).
void CameraAt(const Q6PtSceneSpec& scene, uint32_t frame, uint32_t width, uint32_t height,
              float position[3], float forward[3], float* tanHalfFovX, float* tanHalfFovY);

}  // namespace dxr
}  // namespace doom3

