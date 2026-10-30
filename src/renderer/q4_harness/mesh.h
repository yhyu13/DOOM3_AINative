#pragma once

// Procedural meshes for the Q4 capture harness. They are deliberately simple
// and deterministic: the capture is about the PBR route, not mesh authoring.

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

namespace q4 {

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv;
};

enum class MeshKind : uint32_t {
    Plane = 0,
    Cube = 1,
    Sphere = 2,
    Quad = 3,
};

std::vector<Vertex> MakePlaneVertices(float size, float uvScale);
std::vector<uint32_t> MakePlaneIndices();

std::vector<Vertex> MakeCubeVertices();
std::vector<uint32_t> MakeCubeIndices();

std::vector<Vertex> MakeSphereVertices(uint32_t segments, uint32_t rings);
std::vector<uint32_t> MakeSphereIndices(uint32_t segments, uint32_t rings);

std::vector<Vertex> MakeQuadVertices();
std::vector<uint32_t> MakeQuadIndices();

}  // namespace q4
