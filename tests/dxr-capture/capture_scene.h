// Synthetic scenes for the Q6 DXR capture (spec 004 T069, issue #68).
//
// These are capture fixtures, not the Q4 hero content cells. Every scene is
// built in code from triangles and named materials so the capture is
// reproducible and carries no game data. The evidence file must say so: a
// capture of these fixtures does not satisfy the PT-S1..S5 content identity in
// path-tracing-rubric.md and is not Q6 acceptance.
//
// Coordinate system matches the Q6 CPU slice: z is up, the camera looks toward
// +y, and the environment is split at world z = 0 into an upper and a lower
// hemisphere.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace q6cap {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 Add(const Vec3& a, const Vec3& b);
Vec3 Sub(const Vec3& a, const Vec3& b);
Vec3 Scale(const Vec3& a, float s);
float Dot(const Vec3& a, const Vec3& b);
Vec3 Cross(const Vec3& a, const Vec3& b);
Vec3 Normalize(const Vec3& a);

enum MaterialKind : int {
    kDiffuse = 0,
    kConductor = 1,
    kDielectric = 2,
    kEmissive = 3,
};

// Mirrors `doom3::rhi::PathMaterial` for the fields a capture uses. The GPU
// layout is declared in pathtrace_capture.hlsl and is 80 bytes:
//   float4 albedoRoughness;  // rgb albedo, roughness
//   float4 tintIor;          // rgb transmission tint, ior
//   float4 emissionKind;     // rgb emission, kind
//   float4 alphaUv;          // alphaTest, alphaThreshold, uvMode, unused
//   float4 uvOriginScale;    // originX, originZ, scaleX, scaleZ
struct Material {
    float albedo[3] = {0.5f, 0.5f, 0.5f};
    float roughness = 1.0f;
    float tint[3] = {1.0f, 1.0f, 1.0f};
    float ior = 1.5f;
    float emission[3] = {0.0f, 0.0f, 0.0f};
    int kind = kDiffuse;
    bool alphaTest = false;
    float alphaThreshold = 0.5f;
    // 0: no uv, 1: planar xz (origin/scale from uvOriginScale).
    int uvMode = 0;
    float uvOriginX = 0.0f;
    float uvOriginZ = 0.0f;
    float uvScaleX = 1.0f;
    float uvScaleZ = 1.0f;
};

struct Object {
    std::string name;
    // Rest-pose object-space vertices, non-indexed triangles (3 per triangle).
    std::vector<Vec3> positions;
    int material = 0;
    // Translation-only object-to-world placement. The capture does not claim
    // rotation or scale; normals need no inverse-transpose.
    Vec3 translation;
    // When true the BLAS is built with ALLOW_UPDATE and refit per frame.
    bool skinned = false;
    // Frame requested by BuildScene. SceneRun builds the rest pose first and
    // then refits a skinned object to this frame.
    int poseFrame = 0;
};

struct Camera {
    Vec3 position;
    Vec3 forward{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 0.0f, 1.0f};
    float tanHalfFovX = 0.75f;
    float tanHalfFovY = 0.421875f;
    uint32_t width = 320;
    uint32_t height = 180;
};

struct Environment {
    bool enabled = true;
    float upper[3] = {0.35f, 0.42f, 0.55f};
    float lower[3] = {0.10f, 0.09f, 0.08f};
};

struct Light {
    bool enabled = true;
    Vec3 position{0.0f, 0.2f, 3.0f};
    float radiance[3] = {10.0f, 10.0f, 10.0f};
};

struct Scene {
    std::string name;
    Camera camera;
    Environment environment;
    Light light;
    std::vector<Material> materials;
    std::vector<Object> objects;
    uint32_t maxDepth = 6;
};

// Builds one named fixture at `frame`. Unknown names fail instead of returning
// an empty scene. `alphaControl` disables alpha testing for the cutout fixture
// (a negative control), and is ignored by other scenes.
bool BuildScene(const std::string& name, uint32_t width, uint32_t height, int frame,
                bool alphaControl, Scene* scene, std::string* error);

// Rest-pose vertices transformed for `frame`. `frame == 0` returns the rest
// positions. Linear blend skinning over two z-rotating bones for `skinned`
// objects; a translation for others.
std::vector<Vec3> PoseObject(const Object& object, int frame);

// Camera projection used by the analytic region checks. Returns false when the
// point is behind the camera.
bool ProjectPoint(const Camera& camera, const Vec3& world, float* pixelX, float* pixelY);

// Every fixture name, in capture order.
const char* const* FixtureNames(size_t* count);

}  // namespace q6cap
