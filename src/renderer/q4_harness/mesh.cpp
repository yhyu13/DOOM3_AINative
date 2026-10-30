#include "mesh.h"

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

}  // namespace

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

}  // namespace q4
