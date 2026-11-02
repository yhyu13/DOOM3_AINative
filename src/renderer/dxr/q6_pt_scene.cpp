#include "dxr/q6_pt_scene.h"

#include <cmath>
#include <cstring>

namespace doom3 {
namespace dxr {
namespace {

Q6SurfaceSpec Quad(uint32_t surfaceId, Q6ContentCell cell, float cx, float cy, float cz,
                   float halfU, float halfV, int axis, int uvMode, float uvScale) {
    Q6SurfaceSpec surface;
    surface.surfaceId = surfaceId;
    surface.cell = cell;
    surface.primitive = Q6Primitive::Quad;
    surface.center[0] = cx;
    surface.center[1] = cy;
    surface.center[2] = cz;
    surface.half[0] = halfU;
    surface.half[1] = halfV;
    surface.axis = axis;
    surface.uvMode = uvMode;
    surface.uvScale = uvScale;
    return surface;
}

Q6SurfaceSpec Box(uint32_t surfaceId, Q6ContentCell cell, float cx, float cy, float cz,
                  float hx, float hy, float hz) {
    Q6SurfaceSpec surface;
    surface.surfaceId = surfaceId;
    surface.cell = cell;
    surface.primitive = Q6Primitive::Box;
    surface.center[0] = cx;
    surface.center[1] = cy;
    surface.center[2] = cz;
    surface.half[0] = hx;
    surface.half[1] = hy;
    surface.half[2] = hz;
    return surface;
}

Q6SurfaceSpec Sheet(uint32_t surfaceId, Q6ContentCell cell, float y, float x0, float x1,
                    float z0, float z1) {
    Q6SurfaceSpec surface;
    surface.surfaceId = surfaceId;
    surface.cell = cell;
    surface.primitive = Q6Primitive::Sheet;
    surface.center[0] = 0.5f * (x0 + x1);
    surface.center[1] = y;
    surface.center[2] = 0.5f * (z0 + z1);
    surface.half[0] = 0.5f * (x1 - x0);
    surface.half[1] = 0.5f * (z1 - z0);
    surface.skinned = true;
    surface.uvMode = 2;
    surface.uvScale = 0.5f;
    return surface;
}

Q6SurfaceSpec Sphere(uint32_t surfaceId, Q6ContentCell cell, float cx, float cy, float cz,
                     float radius) {
    Q6SurfaceSpec surface;
    surface.surfaceId = surfaceId;
    surface.cell = cell;
    surface.primitive = Q6Primitive::Sphere;
    surface.center[0] = cx;
    surface.center[1] = cy;
    surface.center[2] = cz;
    surface.half[0] = radius;
    surface.uvScale = 0.5f;
    return surface;
}

Q6CameraSpec Camera(float x, float y, float z, float fx, float fy, float fz,
                    float tanHalfFovX) {
    Q6CameraSpec camera;
    camera.position[0] = x;
    camera.position[1] = y;
    camera.position[2] = z;
    camera.forward[0] = fx;
    camera.forward[1] = fy;
    camera.forward[2] = fz;
    camera.tanHalfFovX = tanHalfFovX;
    return camera;
}

void SetEnvironment(Q6PtSceneSpec* scene, float upR, float upG, float upB, float lowR,
                    float lowG, float lowB) {
    scene->environmentUpper[0] = upR;
    scene->environmentUpper[1] = upG;
    scene->environmentUpper[2] = upB;
    scene->environmentLower[0] = lowR;
    scene->environmentLower[1] = lowG;
    scene->environmentLower[2] = lowB;
}

void SetKey(Q6PtSceneSpec* scene, float x, float y, float z, float r, float g, float b) {
    scene->key.enabled = true;
    scene->key.position[0] = x;
    scene->key.position[1] = y;
    scene->key.position[2] = z;
    scene->key.radiance[0] = r;
    scene->key.radiance[1] = g;
    scene->key.radiance[2] = b;
}

void BuildStaticConvergence(Q6PtSceneSpec* scene) {
    scene->identity = "PT-S1";
    scene->name = "static-convergence";
    scene->maxDepth = 6;
    // Environment cell: 4096x4096 assembled from four authored 2048 tiles.
    scene->surfaces.push_back(Quad(1, Q6ContentCell::Environment, 0.0f, 2.0f, 0.0f, 5.2f,
                                   5.2f, 2, 1, 2.0f));
    // The wall bottom sits below the floor so the two surfaces do not share an
    // edge; a coincident edge makes the primary-hit winner implementation-
    // dependent and breaks the deterministic-rerun row.
    scene->surfaces.push_back(Quad(2, Q6ContentCell::Environment, 0.0f, 5.0f, 1.70f, 5.0f,
                                   1.75f, 1, 1, 1.0f));
    scene->surfaces.push_back(Quad(3, Q6ContentCell::Metal, -1.3f, 4.2f, 1.2f, 0.9f, 0.9f, 1,
                                   2, 0.5f));
    scene->surfaces.push_back(Quad(4, Q6ContentCell::Dielectric, 1.3f, 4.2f, 1.2f, 0.9f, 0.9f,
                                   1, 1, 1.0f));
    SetKey(scene, 0.0f, 0.2f, 3.0f, 10.0f, 10.0f, 10.0f);
    SetEnvironment(scene, 0.35f, 0.42f, 0.55f, 0.10f, 0.09f, 0.08f);
    scene->camera0 = Camera(0.0f, -3.2f, 1.4f, 0.0f, 1.0f, -0.12f, 0.75f);
    scene->camera1 = scene->camera0;
}

void BuildAlphaEmissive(Q6PtSceneSpec* scene) {
    scene->identity = "PT-S2";
    scene->name = "alpha-emissive";
    scene->maxDepth = 5;
    scene->surfaces.push_back(Quad(1, Q6ContentCell::Environment, 0.0f, 2.0f, 0.0f, 4.0f,
                                   4.0f, 2, 1, 2.0f));
    scene->surfaces.push_back(Quad(2, Q6ContentCell::EmissiveSign, 0.0f, 3.2f, 1.3f, 1.0f,
                                   0.6f, 1, 1, 1.0f));
    // The grating is narrower than the sign, so the sign is visible around it
    // as well as through the authored cutout; a fully covering plate would hide
    // the emissive surface and make the HDR row unmeasurable.
    Q6SurfaceSpec cutout = Quad(3, Q6ContentCell::Grating, 0.0f, 1.2f, 1.2f, 0.8f, 0.5f, 1,
                                1, 1.0f);
    cutout.alphaTest = true;
    scene->surfaces.push_back(cutout);
    SetKey(scene, 0.0f, 0.0f, 2.5f, 3.0f, 3.0f, 3.0f);
    SetEnvironment(scene, 0.05f, 0.05f, 0.06f, 0.02f, 0.02f, 0.02f);
    scene->camera0 = Camera(0.0f, -2.4f, 1.4f, 0.0f, 1.0f, -0.05f, 0.75f);
    scene->camera1 = scene->camera0;
}

void BuildMotion(Q6PtSceneSpec* scene) {
    scene->identity = "PT-S3";
    scene->name = "motion";
    scene->maxDepth = 5;
    scene->surfaces.push_back(Quad(1, Q6ContentCell::Environment, 0.0f, 2.0f, 0.0f, 5.0f,
                                   5.0f, 2, 1, 2.0f));
    Q6SurfaceSpec mover = Box(2, Q6ContentCell::Metal, 0.0f, 0.0f, 0.0f, 0.35f, 0.30f, 0.50f);
    mover.moving = true;
    mover.moveDelta[0] = 0.25f;
    mover.teleportFrame = 6;
    mover.teleportDelta[0] = 3.0f;
    mover.center[0] = -1.0f;
    mover.center[1] = 1.2f;
    mover.center[2] = 0.55f;
    scene->surfaces.push_back(mover);
    scene->surfaces.push_back(Sheet(3, Q6ContentCell::CharacterSkin, 1.2f, -0.9f, 0.9f, 0.4f,
                                    2.2f));
    SetKey(scene, 0.0f, 0.0f, 3.0f, 10.0f, 10.0f, 10.0f);
    SetEnvironment(scene, 0.30f, 0.34f, 0.42f, 0.08f, 0.08f, 0.08f);
    scene->camera0 = Camera(0.0f, -3.0f, 1.4f, 0.0f, 1.0f, -0.05f, 0.75f);
    scene->camera1 = scene->camera0;
    scene->lastFrame = 8;
    scene->cutFrame = 6;
}

void BuildTransmission(Q6PtSceneSpec* scene) {
    scene->identity = "PT-S4";
    scene->name = "transmission";
    scene->maxDepth = 6;
    scene->surfaces.push_back(Quad(1, Q6ContentCell::Environment, 0.0f, 2.0f, 0.0f, 4.0f,
                                   4.0f, 2, 1, 2.0f));
    scene->surfaces.push_back(Quad(2, Q6ContentCell::Environment, 0.0f, 3.5f, 1.3f, 3.0f,
                                   2.0f, 1, 1, 2.0f));
    scene->surfaces.push_back(Quad(3, Q6ContentCell::Glass, -0.8f, 2.0f, 1.3f, 0.7f, 0.9f, 1,
                                   2, 0.5f));
    scene->surfaces.push_back(Quad(4, Q6ContentCell::Glass, 0.8f, 2.0f, 1.3f, 0.7f, 0.9f, 1,
                                   2, 0.5f));
    SetKey(scene, 0.0f, 1.0f, 3.2f, 4.0f, 4.0f, 4.0f);
    SetEnvironment(scene, 0.06f, 0.07f, 0.09f, 0.03f, 0.03f, 0.03f);
    scene->camera0 = Camera(0.0f, -2.0f, 1.3f, 0.0f, 1.0f, 0.0f, 0.8f);
    scene->camera1 = Camera(0.35f, -2.0f, 1.3f, 0.0f, 1.0f, 0.0f, 0.8f);
    scene->lastFrame = 8;
    scene->cameraMoves = true;
}

void BuildLargeEnvironment(Q6PtSceneSpec* scene) {
    scene->identity = "PT-S5";
    scene->name = "large-environment";
    scene->maxDepth = 5;
    scene->surfaces.push_back(Quad(1, Q6ContentCell::Environment, 0.0f, 4.0f, 0.0f, 8.5f,
                                   8.5f, 2, 1, 4.0f));
    scene->surfaces.push_back(Quad(2, Q6ContentCell::Environment, 0.0f, 12.0f, 3.95f, 8.0f,
                                   4.0f, 1, 1, 4.0f));
    // The residency test surface: the authored 4096x4096 environment cell is
    // reused on a second, differently scaled surface.
    scene->surfaces.push_back(Quad(3, Q6ContentCell::Metal, -4.0f, 8.0f, 1.5f, 1.5f, 1.2f, 1,
                                   2, 0.5f));
    SetKey(scene, 0.0f, 2.0f, 4.0f, 14.0f, 14.0f, 14.0f);
    SetEnvironment(scene, 0.28f, 0.30f, 0.36f, 0.07f, 0.07f, 0.07f);
    scene->camera0 = Camera(0.0f, -4.0f, 1.5f, 0.0f, 1.0f, -0.02f, 0.75f);
    scene->camera1 = Camera(0.0f, -1.0f, 1.5f, 0.0f, 1.0f, -0.02f, 0.75f);
    scene->lastFrame = 8;
    scene->cameraMoves = true;
}

}  // namespace

const char* ToString(Q6Primitive primitive) {
    switch (primitive) {
        case Q6Primitive::Quad: return "quad";
        case Q6Primitive::Box: return "box";
        case Q6Primitive::Sphere: return "sphere";
        case Q6Primitive::Sheet: return "sheet";
        default: return "unknown";
    }
}

const Q6SurfaceSpec* Q6PtSceneSpec::FindSurface(uint32_t surfaceId) const {
    for (const Q6SurfaceSpec& surface : surfaces) {
        if (surface.surfaceId == surfaceId) {
            return &surface;
        }
    }
    return nullptr;
}

std::vector<Q6SurfaceAssignment> Q6PtSceneSpec::Assignments() const {
    std::vector<Q6SurfaceAssignment> assignments;
    for (const Q6SurfaceSpec& surface : surfaces) {
        Q6SurfaceAssignment assignment;
        assignment.surfaceId = surface.surfaceId;
        assignment.cell = surface.cell;
        assignment.alphaTested = surface.alphaTest;
        assignments.push_back(assignment);
    }
    return assignments;
}

bool Q6PtSceneSpec::ContentMatchesIdentity(std::string* error) const {
    std::vector<Q6ContentCell> required;
    if (!CellsForIdentity(identity, &required)) {
        if (error != nullptr) *error = "unknown identity " + identity;
        return false;
    }
    for (Q6ContentCell cell : required) {
        bool used = false;
        for (const Q6SurfaceSpec& surface : surfaces) {
            if (surface.cell == cell) {
                used = true;
                break;
            }
        }
        if (!used) {
            if (error != nullptr) {
                *error = identity + " does not use the required content cell " + ToString(cell);
            }
            return false;
        }
    }
    if (error != nullptr) error->clear();
    return true;
}

bool BuildPtScene(const std::string& identity, Q6PtSceneSpec* scene, std::string* error) {
    if (scene == nullptr) {
        if (error != nullptr) *error = "BuildPtScene needs an output";
        return false;
    }
    *scene = Q6PtSceneSpec{};
    if (identity == "PT-S1") {
        BuildStaticConvergence(scene);
    } else if (identity == "PT-S2") {
        BuildAlphaEmissive(scene);
    } else if (identity == "PT-S3") {
        BuildMotion(scene);
    } else if (identity == "PT-S4") {
        BuildTransmission(scene);
    } else if (identity == "PT-S5") {
        BuildLargeEnvironment(scene);
    } else {
        if (error != nullptr) *error = "unknown PT identity '" + identity + "'";
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

const char* const* PtIdentities(size_t* count) {
    static const char* const kIdentities[] = {"PT-S1", "PT-S2", "PT-S3", "PT-S4", "PT-S5"};
    if (count != nullptr) {
        *count = sizeof(kIdentities) / sizeof(kIdentities[0]);
    }
    return kIdentities;
}

void CameraAt(const Q6PtSceneSpec& scene, uint32_t frame, uint32_t width, uint32_t height,
              float position[3], float forward[3], float* tanHalfFovX, float* tanHalfFovY) {
    const float t = scene.lastFrame == 0
                        ? 0.0f
                        : static_cast<float>(frame) / static_cast<float>(scene.lastFrame);
    const Q6CameraSpec& a = scene.camera0;
    const Q6CameraSpec& b = scene.cameraMoves ? scene.camera1 : scene.camera0;
    position[0] = a.position[0] + (b.position[0] - a.position[0]) * t;
    position[1] = a.position[1] + (b.position[1] - a.position[1]) * t;
    position[2] = a.position[2] + (b.position[2] - a.position[2]) * t;
    forward[0] = a.forward[0] + (b.forward[0] - a.forward[0]) * t;
    forward[1] = a.forward[1] + (b.forward[1] - a.forward[1]) * t;
    forward[2] = a.forward[2] + (b.forward[2] - a.forward[2]) * t;
    const float length = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] +
                                   forward[2] * forward[2]);
    if (length > 0.0f) {
        forward[0] /= length;
        forward[1] /= length;
        forward[2] /= length;
    }
    if (tanHalfFovX != nullptr) {
        *tanHalfFovX = a.tanHalfFovX + (b.tanHalfFovX - a.tanHalfFovX) * t;
    }
    if (tanHalfFovY != nullptr) {
        const float tanX = a.tanHalfFovX + (b.tanHalfFovX - a.tanHalfFovX) * t;
        *tanHalfFovY = tanX * static_cast<float>(height) / static_cast<float>(width);
    }
}

}  // namespace dxr
}  // namespace doom3
