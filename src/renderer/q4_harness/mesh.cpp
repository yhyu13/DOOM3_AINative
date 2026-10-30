#include "mesh.h"

#include <algorithm>
#include <cmath>

namespace q4 {
namespace {

using namespace DirectX;

XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b) {
    return XMFLOAT3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x);
}

XMFLOAT3 Normalize(const XMFLOAT3& value) {
    const float length =
        std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0f) {
        return XMFLOAT3(0.0f, 1.0f, 0.0f);
    }
    return XMFLOAT3(value.x / length, value.y / length, value.z / length);
}

XMFLOAT3 Subtract(const XMFLOAT3& a, const XMFLOAT3& b) {
    return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) {
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

XMFLOAT3 Scale(const XMFLOAT3& a, float s) {
    return XMFLOAT3(a.x * s, a.y * s, a.z * s);
}

float SmoothStep(float edge0, float edge1, float x) {
    if (edge1 <= edge0) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Appends a capped-free tapered tube from `a` to `b` with `rings` rings and
// `sides` sides. Bone weights blend from boneA at t=0 to boneB at t=1 with the
// smooth window [blendStart, blendEnd], so the elbow/knee seam deforms instead
// of shearing.
void AppendTube(std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
                const XMFLOAT3& a, const XMFLOAT3& b, float radiusA,
                float radiusB, uint32_t rings, uint32_t sides, int boneA,
                int boneB, float blendStart, float blendEnd, float uvScale) {
    const uint32_t base = static_cast<uint32_t>(vertices->size());
    const XMFLOAT3 axis = Normalize(Subtract(b, a));
    const XMFLOAT3 reference =
        std::fabs(axis.y) > 0.9f ? XMFLOAT3(1, 0, 0) : XMFLOAT3(0, 1, 0);
    const XMFLOAT3 u = Normalize(Cross(reference, axis));
    const XMFLOAT3 v = Cross(axis, u);

    for (uint32_t ring = 0; ring <= rings; ++ring) {
        const float t = static_cast<float>(ring) / static_cast<float>(rings);
        const XMFLOAT3 center = Add(a, Scale(Subtract(b, a), t));
        const float radius = radiusA + (radiusB - radiusA) * t;
        const float wB = SmoothStep(blendStart, blendEnd, t);
        for (uint32_t side = 0; side < sides; ++side) {
            const float angle = XM_2PI * static_cast<float>(side) /
                                static_cast<float>(sides);
            const float cs = std::cos(angle);
            const float sn = std::sin(angle);
            const XMFLOAT3 radial =
                Add(Scale(u, cs), Scale(v, sn));
            Vertex vertex;
            vertex.position = Add(center, Scale(radial, radius));
            vertex.normal = radial;
            const XMFLOAT3 tangent =
                Add(Scale(u, -sn), Scale(v, cs));
            vertex.tangent = XMFLOAT4(tangent.x, tangent.y, tangent.z, 1.0f);
            vertex.uv = XMFLOAT2(static_cast<float>(side) /
                                     static_cast<float>(sides) * uvScale,
                                 t * uvScale);
            vertex.boneIndices[0] = static_cast<uint8_t>(boneA);
            vertex.boneIndices[1] = static_cast<uint8_t>(boneB);
            vertex.boneIndices[2] = 0;
            vertex.boneIndices[3] = 0;
            vertex.boneWeights = XMFLOAT4(1.0f - wB, wB, 0.0f, 0.0f);
            vertices->push_back(vertex);
        }
    }

    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t side = 0; side < sides; ++side) {
            const uint32_t next = (side + 1) % sides;
            const uint32_t i0 = base + ring * sides + side;
            const uint32_t i1 = base + ring * sides + next;
            const uint32_t i2 = base + (ring + 1) * sides + side;
            const uint32_t i3 = base + (ring + 1) * sides + next;
            indices->push_back(i0);
            indices->push_back(i1);
            indices->push_back(i2);
            indices->push_back(i1);
            indices->push_back(i3);
            indices->push_back(i2);
        }
    }
}

void AppendSphere(std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
                  const XMFLOAT3& center, float radius, int bone,
                  uint32_t segments, uint32_t rings) {
    const uint32_t base = static_cast<uint32_t>(vertices->size());
    const std::vector<Vertex> local = MakeSphereVertices(segments, rings);
    for (Vertex vertex : local) {
        vertex.position = Add(center, Scale(vertex.position, radius * 2.0f));
        vertex.normal = Normalize(vertex.normal);
        vertex.boneIndices[0] = static_cast<uint8_t>(bone);
        vertex.boneIndices[1] = 0;
        vertex.boneIndices[2] = 0;
        vertex.boneIndices[3] = 0;
        vertex.boneWeights = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
        vertices->push_back(vertex);
    }
    const std::vector<uint32_t> localIndices = MakeSphereIndices(segments, rings);
    for (uint32_t index : localIndices) {
        indices->push_back(base + index);
    }
}

XMMATRIX LocalRotation(uint32_t bone, float phase) {
    const float p = 0.6f + phase * 0.8f;
    switch (bone) {
        case 0:  // hips
            return XMMatrixRotationY(
                XMConvertToRadians(3.0f * std::sin(p * 0.7f)));
        case 1:  // spine
            return XMMatrixRotationY(
                XMConvertToRadians(7.0f * std::sin(p * 0.7f)));
        case 2:  // head
            return XMMatrixRotationY(
                XMConvertToRadians(-6.0f * std::sin(p * 0.7f)));
        case 3:  // left upper arm
            return XMMatrixRotationZ(
                XMConvertToRadians(15.0f * std::sin(p * 0.8f)));
        case 4:  // left forearm
            return XMMatrixRotationZ(
                XMConvertToRadians(-15.0f * std::sin(p * 0.8f + 0.6f)));
        case 5:  // right upper arm: lifts and falls across the sequence
            return XMMatrixRotationZ(
                XMConvertToRadians(-(55.0f + 25.0f * std::sin(p))));
        case 6:  // right forearm: elbow bend relative to the upper arm
            return XMMatrixRotationZ(
                XMConvertToRadians(-(35.0f + 25.0f * std::sin(p + 1.0f))));
        default:
            return XMMatrixIdentity();
    }
}

}  // namespace

const CharacterBone kCharacterSkeleton[kCharacterBoneCount] = {
    {{0.0f, 0.90f, 0.0f}, -1},   // 0 hips
    {{0.0f, 1.10f, 0.0f}, 0},    // 1 spine
    {{0.0f, 1.62f, 0.0f}, 1},    // 2 head
    {{0.26f, 1.54f, 0.0f}, 1},   // 3 left upper arm
    {{0.50f, 1.22f, 0.0f}, 3},   // 4 left forearm
    {{-0.26f, 1.54f, 0.0f}, 1},  // 5 right upper arm
    {{-0.50f, 1.22f, 0.0f}, 5},  // 6 right forearm
    {{0.16f, 0.88f, 0.0f}, 0},   // 7 left thigh
    {{0.18f, 0.46f, 0.0f}, 7},   // 8 left shin
    {{-0.16f, 0.88f, 0.0f}, 0},  // 9 right thigh
    {{-0.18f, 0.46f, 0.0f}, 9},  // 10 right shin
    {{0.0f, 0.0f, 0.0f}, -1},    // 11 unused palette padding
};

std::vector<Vertex> MakePlaneVertices(float size, float uvScale) {
    const float h = size * 0.5f;
    return {
        {{-h, 0.0f, -h}, {0, 1, 0}, {1, 0, 0, 1}, {0, 0}},
        {{-h, 0.0f, h}, {0, 1, 0}, {1, 0, 0, 1}, {0, uvScale}},
        {{h, 0.0f, h}, {0, 1, 0}, {1, 0, 0, 1}, {uvScale, uvScale}},
        {{h, 0.0f, -h}, {0, 1, 0}, {1, 0, 0, 1}, {uvScale, 0}},
    };
}

std::vector<uint32_t> MakePlaneIndices() {
    return {0, 1, 2, 0, 2, 3};
}

std::vector<Vertex> MakeCubeVertices() {
    const float h = 0.5f;
    struct Face {
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT3 corners[4];
    };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 0, -1},
         {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}},
        {{-1, 0, 0}, {0, 0, 1},
         {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}},
        {{0, 1, 0}, {1, 0, 0},
         {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}}},
        {{0, -1, 0}, {1, 0, 0},
         {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}},
        {{0, 0, 1}, {1, 0, 0},
         {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}}},
        {{0, 0, -1}, {-1, 0, 0},
         {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
    };
    const XMFLOAT2 uv[4] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    std::vector<Vertex> vertices;
    vertices.reserve(24);
    for (const Face& face : faces) {
        for (uint32_t i = 0; i < 4; ++i) {
            Vertex vertex;
            vertex.position = face.corners[i];
            vertex.normal = face.normal;
            vertex.tangent =
                XMFLOAT4(face.tangent.x, face.tangent.y, face.tangent.z, 1.0f);
            vertex.uv = uv[i];
            vertices.push_back(vertex);
        }
    }
    return vertices;
}

std::vector<uint32_t> MakeCubeIndices() {
    std::vector<uint32_t> indices;
    indices.reserve(36);
    for (uint32_t face = 0; face < 6; ++face) {
        const uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    return indices;
}

std::vector<Vertex> MakeSphereVertices(uint32_t segments, uint32_t rings) {
    std::vector<Vertex> vertices;
    vertices.reserve((segments + 1) * (rings + 1));
    for (uint32_t y = 0; y <= rings; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rings);
        const float theta = v * XM_PI;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (uint32_t x = 0; x <= segments; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(segments);
            const float phi = u * XM_2PI;
            XMFLOAT3 normal{sinTheta * std::cos(phi), cosTheta,
                            sinTheta * std::sin(phi)};
            XMFLOAT3 tangent = Cross(XMFLOAT3(0, 1, 0), normal);
            tangent = Normalize(tangent);
            if (std::fabs(normal.y) > 0.99f) {
                tangent = XMFLOAT3(1, 0, 0);
            }
            Vertex vertex;
            vertex.position =
                XMFLOAT3(normal.x * 0.5f, normal.y * 0.5f, normal.z * 0.5f);
            vertex.normal = normal;
            vertex.tangent = XMFLOAT4(tangent.x, tangent.y, tangent.z, 1.0f);
            vertex.uv = XMFLOAT2(u, v);
            vertices.push_back(vertex);
        }
    }
    return vertices;
}

std::vector<uint32_t> MakeSphereIndices(uint32_t segments, uint32_t rings) {
    std::vector<uint32_t> indices;
    indices.reserve(segments * rings * 6);
    for (uint32_t y = 0; y < rings; ++y) {
        for (uint32_t x = 0; x < segments; ++x) {
            const uint32_t i0 = y * (segments + 1) + x;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + (segments + 1);
            const uint32_t i3 = i2 + 1;
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    return indices;
}

std::vector<Vertex> MakeQuadVertices() {
    return {
        {{-0.5f, -0.5f, 0.0f}, {0, 0, 1}, {1, 0, 0, 1}, {0, 1}},
        {{0.5f, -0.5f, 0.0f}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1}},
        {{0.5f, 0.5f, 0.0f}, {0, 0, 1}, {1, 0, 0, 1}, {1, 0}},
        {{-0.5f, 0.5f, 0.0f}, {0, 0, 1}, {1, 0, 0, 1}, {0, 0}},
    };
}

std::vector<uint32_t> MakeQuadIndices() {
    return {0, 1, 2, 0, 2, 3};
}

namespace {

void BuildCharacter(std::vector<Vertex>* vertices,
                    std::vector<uint32_t>* indices) {
    vertices->clear();
    indices->clear();
    vertices->reserve(4096);
    // Torso: hips at the bottom, spine at the top.
    AppendTube(vertices, indices, XMFLOAT3(0.0f, 0.86f, 0.0f),
               XMFLOAT3(0.0f, 1.60f, 0.0f), 0.21f, 0.25f, 8, 12, 0, 1, 0.05f,
               0.40f, 2.0f);
    // Head.
    AppendSphere(vertices, indices, XMFLOAT3(0.0f, 1.79f, 0.0f), 0.17f, 2, 16,
                 10);
    // Arms: shoulder to wrist, smooth blend across the elbow.
    AppendTube(vertices, indices, XMFLOAT3(0.26f, 1.54f, 0.0f),
               XMFLOAT3(0.56f, 0.92f, 0.04f), 0.075f, 0.055f, 12, 8, 3, 4,
               0.45f, 0.55f, 2.0f);
    AppendTube(vertices, indices, XMFLOAT3(-0.26f, 1.54f, 0.0f),
               XMFLOAT3(-0.56f, 0.92f, 0.04f), 0.075f, 0.055f, 12, 8, 5, 6,
               0.45f, 0.55f, 2.0f);
    // Hands.
    AppendSphere(vertices, indices, XMFLOAT3(0.57f, 0.90f, 0.04f), 0.065f, 4, 8,
                 6);
    AppendSphere(vertices, indices, XMFLOAT3(-0.57f, 0.90f, 0.04f), 0.065f, 6,
                 8, 6);
    // Legs: hips to ankle, smooth blend across the knee.
    AppendTube(vertices, indices, XMFLOAT3(0.16f, 0.88f, 0.0f),
               XMFLOAT3(0.18f, 0.06f, 0.03f), 0.105f, 0.070f, 10, 8, 7, 8,
               0.45f, 0.55f, 2.0f);
    AppendTube(vertices, indices, XMFLOAT3(-0.16f, 0.88f, 0.0f),
               XMFLOAT3(-0.18f, 0.06f, 0.03f), 0.105f, 0.070f, 10, 8, 9, 10,
               0.45f, 0.55f, 2.0f);
}

}  // namespace

std::vector<Vertex> MakeCharacterVertices() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BuildCharacter(&vertices, &indices);
    return vertices;
}

std::vector<uint32_t> MakeCharacterIndices() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BuildCharacter(&vertices, &indices);
    return indices;
}

void BuildCharacterSkinMatrices(float phase, XMFLOAT4X4* skin, uint32_t count) {
    if (skin == nullptr) {
        return;
    }
    const uint32_t bones = std::min(count, kCharacterBoneCount);
    XMMATRIX posed[kCharacterBoneCount];
    XMMATRIX rotation[kCharacterBoneCount];
    for (uint32_t i = 0; i < kCharacterBoneCount; ++i) {
        posed[i] = XMMatrixIdentity();
        rotation[i] = XMMatrixIdentity();
    }
    for (uint32_t i = 0; i < bones; ++i) {
        const CharacterBone& bone = kCharacterSkeleton[i];
        const XMMATRIX local = LocalRotation(i, phase);
        XMVECTOR head = XMVectorSet(bone.head.x, bone.head.y, bone.head.z, 1.0f);
        if (bone.parent < 0) {
            rotation[i] = local;
        } else {
            const uint32_t parent = static_cast<uint32_t>(bone.parent);
            head = XMVector3TransformCoord(head, posed[parent]);
            rotation[i] = local * rotation[parent];
        }
        XMFLOAT4 hf;
        XMStoreFloat4(&hf, head);
        const XMMATRIX toOrigin = XMMatrixTranslation(-hf.x, -hf.y, -hf.z);
        const XMMATRIX fromOrigin = XMMatrixTranslation(hf.x, hf.y, hf.z);
        posed[i] = toOrigin * rotation[i] * fromOrigin;
    }
    for (uint32_t i = 0; i < bones; ++i) {
        XMStoreFloat4x4(&skin[i], posed[i]);
    }
    for (uint32_t i = bones; i < count; ++i) {
        XMStoreFloat4x4(&skin[i], XMMatrixIdentity());
    }
}

}  // namespace q4
