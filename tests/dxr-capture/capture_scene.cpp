#include "capture_scene.h"

#include <algorithm>
#include <cmath>

namespace q6cap {

Vec3 Add(const Vec3& a, const Vec3& b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Scale(const Vec3& a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }
float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 Normalize(const Vec3& a) {
    const float length = std::sqrt(Dot(a, a));
    if (!(length > 0.0f)) {
        return Vec3{0.0f, 1.0f, 0.0f};
    }
    return Scale(a, 1.0f / length);
}

namespace {

// A quad with normal `cross(u, v)`, split into two triangles.
void AddQuad(std::vector<Vec3>& out, const Vec3& center, const Vec3& u, const Vec3& v,
             float halfU, float halfV) {
    const Vec3 a = Sub(Sub(center, Scale(u, halfU)), Scale(v, halfV));
    const Vec3 b = Add(Sub(center, Scale(v, halfV)), Scale(u, halfU));
    const Vec3 c = Add(Add(center, Scale(u, halfU)), Scale(v, halfV));
    const Vec3 d = Add(Sub(center, Scale(u, halfU)), Scale(v, halfV));
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
    out.push_back(a);
    out.push_back(c);
    out.push_back(d);
}

void AddTriangleOriented(std::vector<Vec3>& out, const Vec3& a, const Vec3& b, const Vec3& c,
                         const Vec3& inside) {
    const Vec3 normal = Cross(Sub(b, a), Sub(c, a));
    const Vec3 centroid = Scale(Add(Add(a, b), c), 1.0f / 3.0f);
    if (Dot(normal, Sub(centroid, inside)) < 0.0f) {
        out.push_back(a);
        out.push_back(c);
        out.push_back(b);
    } else {
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
    }
}

void AddBox(std::vector<Vec3>& out, const Vec3& center, const Vec3& half) {
    AddQuad(out, Add(center, Vec3{0.0f, 0.0f, half.z}), Vec3{1, 0, 0}, Vec3{0, 1, 0}, half.x,
            half.y);
    AddQuad(out, Sub(center, Vec3{0.0f, 0.0f, half.z}), Vec3{1, 0, 0}, Vec3{0, -1, 0}, half.x,
            half.y);
    AddQuad(out, Sub(center, Vec3{0.0f, half.y, 0.0f}), Vec3{1, 0, 0}, Vec3{0, 0, 1}, half.x,
            half.z);
    AddQuad(out, Add(center, Vec3{0.0f, half.y, 0.0f}), Vec3{1, 0, 0}, Vec3{0, 0, -1}, half.x,
            half.z);
    AddQuad(out, Add(center, Vec3{half.x, 0.0f, 0.0f}), Vec3{0, 1, 0}, Vec3{0, 0, 1}, half.y,
            half.z);
    AddQuad(out, Sub(center, Vec3{half.x, 0.0f, 0.0f}), Vec3{0, 1, 0}, Vec3{0, 0, -1}, half.y,
            half.z);
}

void AddSphere(std::vector<Vec3>& out, const Vec3& center, float radius, int segments,
               int rings) {
    auto vertex = [&](int i, int j) {
        const float theta = 3.14159265358979323846f * static_cast<float>(i) /
                            static_cast<float>(rings);
        const float phi = 6.28318530717958647692f * static_cast<float>(j) /
                          static_cast<float>(segments);
        return Vec3{center.x + radius * std::sin(theta) * std::cos(phi),
                    center.y + radius * std::sin(theta) * std::sin(phi),
                    center.z + radius * std::cos(theta)};
    };
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const Vec3 a = vertex(i, j);
            const Vec3 b = vertex(i, j + 1);
            const Vec3 c = vertex(i + 1, j + 1);
            const Vec3 d = vertex(i + 1, j);
            if (i == 0) {
                AddTriangleOriented(out, a, c, d, center);
            } else if (i == rings - 1) {
                AddTriangleOriented(out, a, b, c, center);
            } else {
                AddTriangleOriented(out, a, b, c, center);
                AddTriangleOriented(out, a, c, d, center);
            }
        }
    }
}

// Vertical sheet in the xz plane at y, normal -y. Used by the synthetic
// skinned fixture; the deformation is applied by PoseObject.
void AddSheet(std::vector<Vec3>& out, float y, float x0, float x1, float z0, float z1,
              int cellsX, int cellsZ) {
    for (int i = 0; i < cellsX; ++i) {
        for (int j = 0; j < cellsZ; ++j) {
            const float xa = x0 + (x1 - x0) * static_cast<float>(i) / static_cast<float>(cellsX);
            const float xb =
                x0 + (x1 - x0) * static_cast<float>(i + 1) / static_cast<float>(cellsX);
            const float za = z0 + (z1 - z0) * static_cast<float>(j) / static_cast<float>(cellsZ);
            const float zb =
                z0 + (z1 - z0) * static_cast<float>(j + 1) / static_cast<float>(cellsZ);
            const Vec3 a{xa, y, za};
            const Vec3 b{xb, y, za};
            const Vec3 c{xb, y, zb};
            const Vec3 d{xa, y, zb};
            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
            out.push_back(a);
            out.push_back(c);
            out.push_back(d);
        }
    }
}

Material Diffuse(float r, float g, float b) {
    Material m;
    m.kind = kDiffuse;
    m.albedo[0] = r;
    m.albedo[1] = g;
    m.albedo[2] = b;
    return m;
}

Material Emissive(float r, float g, float b) {
    Material m;
    m.kind = kEmissive;
    m.emission[0] = r;
    m.emission[1] = g;
    m.emission[2] = b;
    m.albedo[0] = 0.0f;
    m.albedo[1] = 0.0f;
    m.albedo[2] = 0.0f;
    return m;
}

void AddRoom(Scene* scene, float floorAlbedo, float wallR, float wallG, float wallB) {
    Object floor;
    floor.name = "floor";
    AddQuad(floor.positions, Vec3{0.0f, 2.0f, 0.0f}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, 5.0f,
            5.0f);
    floor.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(Diffuse(floorAlbedo, floorAlbedo, floorAlbedo));
    scene->objects.push_back(floor);

    Object back;
    back.name = "back-wall";
    AddQuad(back.positions, Vec3{0.0f, 5.0f, 1.75f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 5.0f,
            1.75f);
    back.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(Diffuse(wallR, wallG, wallB));
    scene->objects.push_back(back);

    Object side;
    side.name = "side-wall";
    AddQuad(side.positions, Vec3{-4.0f, 2.0f, 1.75f}, Vec3{0, 1, 0}, Vec3{0, 0, 1}, 3.0f,
            1.75f);
    side.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(Diffuse(0.35f, 0.33f, 0.30f));
    scene->objects.push_back(side);
}

void SetCamera(Scene* scene, const Vec3& position, const Vec3& forward, float tanHalfFovX,
               uint32_t width, uint32_t height) {
    scene->camera.position = position;
    scene->camera.forward = Normalize(forward);
    scene->camera.right = Normalize(Cross(scene->camera.forward, Vec3{0.0f, 0.0f, 1.0f}));
    scene->camera.up = Cross(scene->camera.right, scene->camera.forward);
    scene->camera.tanHalfFovX = tanHalfFovX;
    scene->camera.tanHalfFovY =
        tanHalfFovX * static_cast<float>(height) / static_cast<float>(width);
    scene->camera.width = width;
    scene->camera.height = height;
}

void BuildCalibration(Scene* scene, uint32_t width, uint32_t height) {
    scene->name = "calibration";
    scene->maxDepth = 2;
    scene->environment.enabled = true;
    scene->environment.upper[0] = 1.0f;
    scene->environment.upper[1] = 1.0f;
    scene->environment.upper[2] = 1.0f;
    scene->environment.lower[0] = 0.0f;
    scene->environment.lower[1] = 0.0f;
    scene->environment.lower[2] = 0.0f;
    scene->light.enabled = false;
    Object wall;
    wall.name = "calibration-wall";
    AddQuad(wall.positions, Vec3{0.0f, 2.0f, 0.0f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 7.0f, 7.0f);
    wall.material = 0;
    scene->materials.push_back(Diffuse(0.6f, 0.4f, 0.2f));
    scene->objects.push_back(wall);
    SetCamera(scene, Vec3{0.0f, -1.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, 0.6f, width, height);
}

void BuildStatic(Scene* scene, uint32_t width, uint32_t height) {
    scene->name = "static";
    scene->maxDepth = 6;
    AddRoom(scene, 0.55f, 0.42f, 0.45f, 0.50f);

    Material glass;
    glass.kind = kDielectric;
    glass.ior = 1.5f;
    glass.tint[0] = 1.0f;
    glass.tint[1] = 1.0f;
    glass.tint[2] = 1.0f;
    Object sphere;
    sphere.name = "dielectric-sphere";
    AddSphere(sphere.positions, Vec3{0.65f, 2.0f, 0.75f}, 0.75f, 24, 12);
    sphere.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(glass);
    scene->objects.push_back(sphere);

    Material metal;
    metal.kind = kConductor;
    metal.roughness = 0.35f;
    metal.albedo[0] = 0.95f;
    metal.albedo[1] = 0.80f;
    metal.albedo[2] = 0.45f;
    Object metalSphere;
    metalSphere.name = "metal-sphere";
    AddSphere(metalSphere.positions, Vec3{-1.0f, 1.3f, 0.6f}, 0.6f, 24, 12);
    metalSphere.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(metal);
    scene->objects.push_back(metalSphere);

    scene->light.enabled = true;
    scene->light.position = Vec3{0.0f, 0.2f, 3.0f};
    SetCamera(scene, Vec3{0.0f, -3.2f, 1.4f}, Vec3{0.0f, 1.0f, -0.12f}, 0.75f, width,
              height);
}

void BuildRigid(Scene* scene, uint32_t width, uint32_t height, int frame) {
    scene->name = "rigid";
    scene->maxDepth = 5;
    AddRoom(scene, 0.5f, 0.40f, 0.44f, 0.52f);

    Material red;
    red.kind = kDiffuse;
    red.albedo[0] = 0.85f;
    red.albedo[1] = 0.15f;
    red.albedo[2] = 0.10f;
    Object mover;
    mover.name = "moving-box";
    AddBox(mover.positions, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.35f, 0.30f, 0.50f});
    mover.material = static_cast<int>(scene->materials.size());
    mover.translation = Vec3{-1.0f + 0.25f * static_cast<float>(frame), 1.2f, 0.55f};
    scene->materials.push_back(red);
    scene->objects.push_back(mover);

    Material blue;
    blue.kind = kDiffuse;
    blue.albedo[0] = 0.15f;
    blue.albedo[1] = 0.25f;
    blue.albedo[2] = 0.70f;
    Object anchor;
    anchor.name = "anchor-box";
    AddBox(anchor.positions, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.30f, 0.30f, 0.30f});
    anchor.material = static_cast<int>(scene->materials.size());
    anchor.translation = Vec3{0.0f, 4.0f, 0.35f};
    scene->materials.push_back(blue);
    scene->objects.push_back(anchor);

    scene->light.enabled = true;
    scene->light.position = Vec3{0.0f, 0.0f, 3.0f};
    SetCamera(scene, Vec3{0.0f, -3.0f, 1.4f}, Vec3{0.0f, 1.0f, -0.10f}, 0.75f, width,
              height);
}

void BuildSkinned(Scene* scene, uint32_t width, uint32_t height) {
    scene->name = "skinned";
    scene->maxDepth = 5;
    AddRoom(scene, 0.5f, 0.40f, 0.44f, 0.52f);

    Material cloth;
    cloth.kind = kDiffuse;
    cloth.albedo[0] = 0.65f;
    cloth.albedo[1] = 0.35f;
    cloth.albedo[2] = 0.20f;
    Object sheet;
    sheet.name = "skinned-sheet";
    AddSheet(sheet.positions, 1.2f, -0.9f, 0.9f, 0.4f, 2.2f, 16, 8);
    sheet.material = static_cast<int>(scene->materials.size());
    sheet.skinned = true;
    scene->materials.push_back(cloth);
    scene->objects.push_back(sheet);

    scene->light.enabled = true;
    scene->light.position = Vec3{0.0f, 0.0f, 3.0f};
    SetCamera(scene, Vec3{0.0f, -3.0f, 1.4f}, Vec3{0.0f, 1.0f, -0.05f}, 0.75f, width,
              height);
}

void BuildAlphaEmissive(Scene* scene, uint32_t width, uint32_t height, bool alphaControl) {
    scene->name = alphaControl ? "alpha-emissive-control" : "alpha-emissive";
    scene->maxDepth = 4;
    scene->environment.enabled = true;
    scene->environment.upper[0] = 0.05f;
    scene->environment.upper[1] = 0.05f;
    scene->environment.upper[2] = 0.06f;
    scene->environment.lower[0] = 0.02f;
    scene->environment.lower[1] = 0.02f;
    scene->environment.lower[2] = 0.02f;

    Object sign;
    sign.name = "emissive-sign";
    AddQuad(sign.positions, Vec3{0.0f, 3.2f, 1.3f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 1.0f,
            0.6f);
    sign.material = 0;
    scene->materials.push_back(Emissive(4.0f, 3.2f, 2.2f));
    scene->objects.push_back(sign);

    Material cutout;
    cutout.kind = kDiffuse;
    cutout.albedo[0] = 0.10f;
    cutout.albedo[1] = 0.55f;
    cutout.albedo[2] = 0.20f;
    cutout.alphaTest = !alphaControl;
    cutout.alphaThreshold = 0.5f;
    cutout.uvMode = 1;
    cutout.uvOriginX = -1.3f;
    cutout.uvOriginZ = 0.5f;
    cutout.uvScaleX = 1.0f / 2.6f;
    cutout.uvScaleZ = 1.0f / 1.6f;
    Object quad;
    quad.name = "alpha-cutout";
    AddQuad(quad.positions, Vec3{0.0f, 1.2f, 1.3f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 1.3f,
            0.8f);
    quad.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(cutout);
    scene->objects.push_back(quad);

    scene->light.enabled = true;
    scene->light.position = Vec3{0.0f, 0.0f, 2.5f};
    scene->light.radiance[0] = 3.0f;
    scene->light.radiance[1] = 3.0f;
    scene->light.radiance[2] = 3.0f;
    SetCamera(scene, Vec3{0.0f, -2.4f, 1.4f}, Vec3{0.0f, 1.0f, -0.05f}, 0.75f, width,
              height);
}

void BuildTransmission(Scene* scene, uint32_t width, uint32_t height) {
    scene->name = "transmission";
    scene->maxDepth = 4;
    scene->environment.enabled = true;
    scene->environment.upper[0] = 0.06f;
    scene->environment.upper[1] = 0.07f;
    scene->environment.upper[2] = 0.09f;
    scene->environment.lower[0] = 0.03f;
    scene->environment.lower[1] = 0.03f;
    scene->environment.lower[2] = 0.03f;

    Object backdrop;
    backdrop.name = "backdrop";
    AddQuad(backdrop.positions, Vec3{0.0f, 3.5f, 1.3f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 3.0f,
            2.0f);
    backdrop.material = 0;
    scene->materials.push_back(Emissive(0.8f, 0.8f, 0.8f));
    scene->objects.push_back(backdrop);

    Material tinted;
    tinted.kind = kDielectric;
    tinted.ior = 1.5f;
    tinted.tint[0] = 1.0f;
    tinted.tint[1] = 0.55f;
    tinted.tint[2] = 0.30f;
    Object left;
    left.name = "tinted-glass";
    AddQuad(left.positions, Vec3{-0.8f, 2.0f, 1.3f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 0.7f,
            0.9f);
    left.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(tinted);
    scene->objects.push_back(left);

    Material clear;
    clear.kind = kDielectric;
    clear.ior = 1.5f;
    clear.tint[0] = 1.0f;
    clear.tint[1] = 1.0f;
    clear.tint[2] = 1.0f;
    Object right;
    right.name = "clear-glass";
    AddQuad(right.positions, Vec3{0.8f, 2.0f, 1.3f}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, 0.7f,
            0.9f);
    right.material = static_cast<int>(scene->materials.size());
    scene->materials.push_back(clear);
    scene->objects.push_back(right);

    scene->light.enabled = false;
    SetCamera(scene, Vec3{0.0f, -2.0f, 1.3f}, Vec3{0.0f, 1.0f, 0.0f}, 0.8f, width, height);
}

}  // namespace

bool BuildScene(const std::string& name, uint32_t width, uint32_t height, int frame,
                bool alphaControl, Scene* scene, std::string* error) {
    if (scene == nullptr) {
        if (error != nullptr) {
            *error = "BuildScene needs an output scene";
        }
        return false;
    }
    *scene = Scene();
    if (name == "calibration") {
        BuildCalibration(scene, width, height);
    } else if (name == "static") {
        BuildStatic(scene, width, height);
    } else if (name == "rigid") {
        BuildRigid(scene, width, height, frame);
    } else if (name == "skinned") {
        BuildSkinned(scene, width, height);
    } else if (name == "alpha-emissive") {
        BuildAlphaEmissive(scene, width, height, alphaControl);
    } else if (name == "transmission") {
        BuildTransmission(scene, width, height);
    } else {
        if (error != nullptr) {
            *error = "unknown capture fixture '" + name + "'";
        }
        return false;
    }
    scene->camera.width = width;
    scene->camera.height = height;
    scene->camera.tanHalfFovY =
        scene->camera.tanHalfFovX * static_cast<float>(height) / static_cast<float>(width);
    for (Object& object : scene->objects) {
        object.poseFrame = frame;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

std::vector<Vec3> PoseObject(const Object& object, int frame) {
    if (!object.skinned || frame == 0) {
        std::vector<Vec3> posed;
        posed.reserve(object.positions.size());
        for (const Vec3& p : object.positions) {
            posed.push_back(Add(p, object.translation));
        }
        return posed;
    }
    const Vec3 pivotA{-0.45f, 1.2f, 1.2f};
    const Vec3 pivotB{0.45f, 1.2f, 1.2f};
    const float angleA = 0.55f * static_cast<float>(frame) / 6.0f;
    const float angleB = -0.55f * static_cast<float>(frame) / 6.0f;
    auto rotateAbout = [](const Vec3& p, const Vec3& pivot, float angle) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const Vec3 local = Sub(p, pivot);
        return Vec3{pivot.x + c * local.x - s * local.y, pivot.y + s * local.x + c * local.y,
                    pivot.z + local.z};
    };
    std::vector<Vec3> posed;
    posed.reserve(object.positions.size());
    for (const Vec3& p : object.positions) {
        float weightA = 0.5f - p.x / 1.8f;
        weightA = std::min(1.0f, std::max(0.0f, weightA));
        const float weightB = 1.0f - weightA;
        const Vec3 a = rotateAbout(p, pivotA, angleA);
        const Vec3 b = rotateAbout(p, pivotB, angleB);
        posed.push_back(Add(Scale(a, weightA), Scale(b, weightB)));
    }
    return posed;
}

bool ProjectPoint(const Camera& camera, const Vec3& world, float* pixelX, float* pixelY) {
    const Vec3 relative = Sub(world, camera.position);
    const float depth = Dot(relative, camera.forward);
    if (!(depth > 0.0f)) {
        return false;
    }
    const float ndcX = Dot(relative, camera.right) / (depth * camera.tanHalfFovX);
    const float ndcY = Dot(relative, camera.up) / (depth * camera.tanHalfFovY);
    if (pixelX != nullptr) {
        *pixelX = (ndcX + 1.0f) * 0.5f * static_cast<float>(camera.width);
    }
    if (pixelY != nullptr) {
        *pixelY = (1.0f - ndcY) * 0.5f * static_cast<float>(camera.height);
    }
    return true;
}

const char* const* FixtureNames(size_t* count) {
    static const char* const kNames[] = {"calibration", "static",  "rigid",
                                         "skinned",     "alpha-emissive", "transmission"};
    if (count != nullptr) {
        *count = sizeof(kNames) / sizeof(kNames[0]);
    }
    return kNames;
}

}  // namespace q6cap
