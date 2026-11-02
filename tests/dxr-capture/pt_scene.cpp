#include "pt_scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using doom3::dxr::Q6ContentCell;
using doom3::dxr::Q6Primitive;
using doom3::dxr::Q6SurfaceSpec;

namespace q6cap {
namespace {

Vec3 RotateAboutZ(const Vec3& point, const Vec3& pivot, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float x = point.x - pivot.x;
    const float y = point.y - pivot.y;
    return Vec3{pivot.x + c * x - s * y, pivot.y + s * x + c * y, point.z};
}

void AddQuad(std::vector<Vec3>* out, const Vec3& center, const Vec3& u, const Vec3& v,
             float halfU, float halfV) {
    const Vec3 a = Sub(Sub(center, Scale(u, halfU)), Scale(v, halfV));
    const Vec3 b = Add(Sub(center, Scale(v, halfV)), Scale(u, halfU));
    const Vec3 c = Add(Add(center, Scale(u, halfU)), Scale(v, halfV));
    const Vec3 d = Add(Sub(center, Scale(u, halfU)), Scale(v, halfV));
    out->push_back(a);
    out->push_back(b);
    out->push_back(c);
    out->push_back(a);
    out->push_back(c);
    out->push_back(d);
}

void TessellateQuad(const Q6SurfaceSpec& surface, std::vector<Vec3>* out) {
    const Vec3 center{surface.center[0], surface.center[1], surface.center[2]};
    Vec3 u;
    Vec3 v;
    switch (surface.axis) {
        case 0:
            // Normal +x, facing the -x side.
            u = Vec3{0.0f, 1.0f, 0.0f};
            v = Vec3{0.0f, 0.0f, 1.0f};
            break;
        case 1:
            // Normal -y (faces a camera at lower y): u = +x, v = +z, so
            // u x v = -y and DXR's default back-face culling keeps the quad.
            u = Vec3{1.0f, 0.0f, 0.0f};
            v = Vec3{0.0f, 0.0f, 1.0f};
            break;
        default:
            u = Vec3{1.0f, 0.0f, 0.0f};
            v = Vec3{0.0f, 1.0f, 0.0f};
            break;
    }
    AddQuad(out, center, u, v, surface.half[0], surface.half[1]);
}

void TessellateBox(const Q6SurfaceSpec& surface, std::vector<Vec3>* out) {
    const Vec3 c{surface.center[0], surface.center[1], surface.center[2]};
    const Vec3 h{surface.half[0], surface.half[1], surface.half[2]};
    AddQuad(out, Add(c, Vec3{0.0f, 0.0f, h.z}), Vec3{1, 0, 0}, Vec3{0, 1, 0}, h.x, h.y);
    AddQuad(out, Sub(c, Vec3{0.0f, 0.0f, h.z}), Vec3{1, 0, 0}, Vec3{0, -1, 0}, h.x, h.y);
    AddQuad(out, Sub(c, Vec3{0.0f, h.y, 0.0f}), Vec3{1, 0, 0}, Vec3{0, 0, 1}, h.x, h.z);
    AddQuad(out, Add(c, Vec3{0.0f, h.y, 0.0f}), Vec3{1, 0, 0}, Vec3{0, 0, -1}, h.x, h.z);
    AddQuad(out, Add(c, Vec3{h.x, 0.0f, 0.0f}), Vec3{0, 1, 0}, Vec3{0, 0, 1}, h.y, h.z);
    AddQuad(out, Sub(c, Vec3{h.x, 0.0f, 0.0f}), Vec3{0, 1, 0}, Vec3{0, 0, -1}, h.y, h.z);
}

void AddTriangleOriented(std::vector<Vec3>* out, const Vec3& a, const Vec3& b, const Vec3& c,
                         const Vec3& inside) {
    const Vec3 normal = Cross(Sub(b, a), Sub(c, a));
    const Vec3 centroid = Scale(Add(Add(a, b), c), 1.0f / 3.0f);
    if (Dot(normal, Sub(centroid, inside)) < 0.0f) {
        out->push_back(a);
        out->push_back(c);
        out->push_back(b);
    } else {
        out->push_back(a);
        out->push_back(b);
        out->push_back(c);
    }
}

void TessellateSphere(const Q6SurfaceSpec& surface, std::vector<Vec3>* out) {
    const Vec3 center{surface.center[0], surface.center[1], surface.center[2]};
    const float radius = surface.half[0];
    const int segments = 24;
    const int rings = 12;
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

// The sheet is a 16x8 grid in the xz plane at y = center.y, deformed by two
// z-rotating bones. This is the same pose model as the synthetic skinned
// fixture, now carrying the authored character-skin cell.
void TessellateSheet(const Q6SurfaceSpec& surface, uint32_t frame, std::vector<Vec3>* out) {
    constexpr int cellsX = 16;
    constexpr int cellsZ = 8;
    const float x0 = surface.center[0] - surface.half[0];
    const float x1 = surface.center[0] + surface.half[0];
    const float z0 = surface.center[2] - surface.half[1];
    const float z1 = surface.center[2] + surface.half[1];
    const float y = surface.center[1];
    const float t = std::min(1.0f, static_cast<float>(frame) / 6.0f);
    const Vec3 pivotA{-0.45f, 1.2f, 1.2f};
    const Vec3 pivotB{0.45f, 1.2f, 1.2f};
    const float angleA = 0.55f * t;
    const float angleB = -0.55f * t;
    auto pose = [&](const Vec3& p) {
        float weightA = 0.5f - p.x / 1.8f;
        weightA = std::min(1.0f, std::max(0.0f, weightA));
        const float weightB = 1.0f - weightA;
        const Vec3 a = RotateAboutZ(p, pivotA, angleA);
        const Vec3 b = RotateAboutZ(p, pivotB, angleB);
        return Add(Scale(a, weightA), Scale(b, weightB));
    };
    for (int i = 0; i < cellsX; ++i) {
        for (int j = 0; j < cellsZ; ++j) {
            const float xa = x0 + (x1 - x0) * static_cast<float>(i) / cellsX;
            const float xb = x0 + (x1 - x0) * static_cast<float>(i + 1) / cellsX;
            const float za = z0 + (z1 - z0) * static_cast<float>(j) / cellsZ;
            const float zb = z0 + (z1 - z0) * static_cast<float>(j + 1) / cellsZ;
            const Vec3 a{xa, y, za};
            const Vec3 b{xb, y, za};
            const Vec3 c{xb, y, zb};
            const Vec3 d{xa, y, zb};
            out->push_back(pose(a));
            out->push_back(pose(b));
            out->push_back(pose(c));
            out->push_back(pose(a));
            out->push_back(pose(c));
            out->push_back(pose(d));
        }
    }
}

}  // namespace

void TessellateSurface(const Q6SurfaceSpec& surface, uint32_t frame, std::vector<Vec3>* out) {
    out->clear();
    Vec3 offset{0.0f, 0.0f, 0.0f};
    if (surface.moving) {
        offset = Scale(Vec3{surface.moveDelta[0], surface.moveDelta[1], surface.moveDelta[2]},
                       static_cast<float>(frame));
    }
    if (frame >= surface.teleportFrame) {
        offset = Add(offset, Vec3{surface.teleportDelta[0], surface.teleportDelta[1],
                                  surface.teleportDelta[2]});
    }
    switch (surface.primitive) {
        case Q6Primitive::Quad: TessellateQuad(surface, out); break;
        case Q6Primitive::Box: TessellateBox(surface, out); break;
        case Q6Primitive::Sphere: TessellateSphere(surface, out); break;
        case Q6Primitive::Sheet: TessellateSheet(surface, frame, out); break;
    }
    if (offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f) {
        for (Vec3& vertex : *out) {
            vertex = Add(vertex, offset);
        }
    }
}

bool BuildQ2Queue(const doom3::dxr::Q6PtSceneSpec& spec, uint32_t width, uint32_t height,
                  doom3::rhi::SceneCommandQueue* queue, std::string* error) {
    if (queue == nullptr) {
        if (error != nullptr) *error = "the PT stream needs a queue";
        return false;
    }
    queue->Clear();
    doom3::rhi::ViewDesc view;
    view.identity = 1;
    view.width = width;
    view.height = height;
    if (!queue->BeginView(view)) {
        if (error != nullptr) *error = "the stream could not express the scene view";
        return false;
    }
    for (const Q6SurfaceSpec& surface : spec.surfaces) {
        doom3::rhi::DrawSurfPacket draw;
        draw.surfaceId = surface.surfaceId;
        draw.materialId = surface.surfaceId;
        draw.vertexCount = 6;
        draw.indexCount = 6;
        draw.alphaTested = surface.alphaTest;
        if (!queue->Draw(draw)) {
            if (error != nullptr) *error = "the stream could not express a draw";
            return false;
        }
    }
    doom3::rhi::LightPacket light;
    light.lightId = 1;
    light.light.kind = doom3::rhi::LightKind::Point;
    light.light.pointLight = spec.key.enabled;
    light.light.falloffRadius = spec.key.radius;
    if (!queue->DrawLight(light)) {
        if (error != nullptr) *error = "the stream could not express the light";
        return false;
    }
    if (!queue->EndView()) {
        if (error != nullptr) *error = "the stream could not express the end of the view";
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

bool PtGeometrySource::ResolveSurface(uint32_t surfaceId,
                                      doom3::rhi::SceneSurfaceGeometry* geometry) {
    if (geometry == nullptr) {
        return false;
    }
    const Q6SurfaceSpec* surface = spec_.FindSurface(surfaceId);
    if (surface == nullptr) {
        return false;
    }
    std::vector<Vec3> positions;
    TessellateSurface(*surface, frame_, &positions);
    if (positions.empty()) {
        return false;
    }
    geometry->vertices.clear();
    geometry->vertices.reserve(positions.size());
    for (const Vec3& position : positions) {
        doom3::rhi::Vertex vertex;
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.position[2] = position.z;
        vertex.texCoord[0] = 0.0f;
        vertex.texCoord[1] = 0.0f;
        geometry->vertices.push_back(vertex);
    }
    std::memset(geometry->transform, 0, sizeof(geometry->transform));
    geometry->transform[0] = 1.0f;
    geometry->transform[5] = 1.0f;
    geometry->transform[10] = 1.0f;
    geometry->transform[15] = 1.0f;
    return true;
}

bool BuildCaptureScene(const doom3::dxr::Q6PtSceneSpec& spec,
                       const doom3::dxr::Q6EngineFrame& frame, uint32_t sceneFrame,
                       uint32_t width, uint32_t height, uint32_t atlasColumns, Scene* scene,
                       std::string* error) {
    if (scene == nullptr) {
        if (error != nullptr) *error = "the capture scene needs an output";
        return false;
    }
    *scene = Scene();
    scene->name = spec.identity + "-" + spec.name;
    scene->maxDepth = spec.maxDepth;
    scene->useContent = true;
    (void)atlasColumns;

    float position[3];
    float forward[3];
    float tanX = 0.75f;
    float tanY = 0.5f;
    doom3::dxr::CameraAt(spec, sceneFrame, width, height, position, forward, &tanX, &tanY);
    scene->camera.position = Vec3{position[0], position[1], position[2]};
    scene->camera.forward = Normalize(Vec3{forward[0], forward[1], forward[2]});
    scene->camera.right = Normalize(Cross(scene->camera.forward, Vec3{0.0f, 0.0f, 1.0f}));
    scene->camera.up = Cross(scene->camera.right, scene->camera.forward);
    scene->camera.tanHalfFovX = tanX;
    scene->camera.tanHalfFovY = tanY;
    scene->camera.width = width;
    scene->camera.height = height;

    scene->environment.enabled = true;
    for (int i = 0; i < 3; ++i) {
        scene->environment.upper[i] = spec.environmentUpper[i];
        scene->environment.lower[i] = spec.environmentLower[i];
    }
    scene->light.enabled = spec.key.enabled;
    scene->light.position =
        Vec3{spec.key.position[0], spec.key.position[1], spec.key.position[2]};
    for (int i = 0; i < 3; ++i) {
        scene->light.radiance[i] = spec.key.radiance[i];
    }

    for (const doom3::dxr::Q6OrderedSurface& ordered : frame.surfaces()) {
        const Q6SurfaceSpec* surface = spec.FindSurface(ordered.surfaceId);
        if (surface == nullptr) {
            if (error != nullptr) {
                *error = "the ordered stream names an undeclared surface " +
                         std::to_string(ordered.surfaceId);
            }
            return false;
        }
        if (ordered.vertices.empty() || ordered.vertices.size() % 3 != 0) {
            if (error != nullptr) *error = "an ordered surface is not a triangle list";
            return false;
        }
        Material material;
        material.kind = kDiffuse;
        material.contentCell = static_cast<int>(surface->cell);
        material.uvMode = surface->uvMode;
        // The planar projection is centred on the surface so uv 0.5 maps to the
        // surface centre; an origin of 0 would put the texture seam through the
        // middle of a centred quad.
        if (surface->uvMode == 1) {
            material.uvOriginX = surface->center[0];
            material.uvOriginZ = surface->center[2];
        } else if (surface->uvMode == 2) {
            material.uvOriginX = surface->center[0];
            material.uvOriginZ = surface->center[1];
        } else {
            material.uvOriginX = surface->center[1];
            material.uvOriginZ = surface->center[2];
        }
        // uvScale is the number of texture repeats across the surface; the
        // shader divides by the surface extent so the full authored image maps
        // onto the quad instead of a truncated corner of it.
        material.uvScaleX = surface->uvScale / (2.0f * surface->half[0]);
        material.uvScaleZ = surface->uvScale / (2.0f * surface->half[1]);
        material.emissiveScale = 1.0f;
        if (surface->cell == Q6ContentCell::EmissiveSign) {
            material.kind = kEmissive;
            material.emissiveScale = 16.0f;
        } else if (surface->cell == Q6ContentCell::Glass) {
            material.kind = kDielectric;
            material.ior = 1.5f;
            material.tint[0] = 1.0f;
            material.tint[1] = 1.0f;
            material.tint[2] = 1.0f;
        }
        material.alphaTest = surface->alphaTest;
        material.alphaThreshold = 0.5f;
        const int materialIndex = static_cast<int>(scene->materials.size());
        scene->materials.push_back(material);

        Object object;
        object.name = "surface-" + std::to_string(surface->surfaceId);
        object.positions.reserve(ordered.vertices.size());
        for (const doom3::rhi::Vertex& vertex : ordered.vertices) {
            object.positions.push_back(Vec3{vertex.position[0], vertex.position[1],
                                            vertex.position[2]});
        }
        object.material = materialIndex;
        object.translation = Vec3{0.0f, 0.0f, 0.0f};
        object.skinned = false;
        object.poseFrame = 0;
        scene->objects.push_back(std::move(object));
    }
    if (error != nullptr) error->clear();
    return true;
}

void PosesForFrame(const doom3::dxr::Q6PtSceneSpec& spec, uint32_t frame,
                   std::vector<doom3::dxr::Q6SurfacePose>* poses) {
    poses->clear();
    for (const Q6SurfaceSpec& surface : spec.surfaces) {
        doom3::dxr::Q6SurfacePose pose;
        pose.surfaceId = surface.surfaceId;
        std::vector<Vec3> positions;
        TessellateSurface(surface, frame, &positions);
        for (const Vec3& position : positions) {
            pose.current.push_back(
                doom3::rhi::MotionVec3{position.x, position.y, position.z});
        }
        poses->push_back(std::move(pose));
    }
}

}  // namespace q6cap





