#pragma once

// Procedural meshes for the Q4 capture harness. They are deliberately simple
// and deterministic: the capture is about the PBR route, not mesh authoring.
//
// Every vertex carries four bone indices and four weights so one vertex layout
// and one pipeline serve both rigid and skinned draws. Rigid vertices use bone
// 0 with weight 1; the harness writes an identity palette for a non-skinned
// draw, so the same shader path is an exact identity for them.

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

namespace q4 {

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv;
    uint8_t boneIndices[4] = {0, 0, 0, 0};
    DirectX::XMFLOAT4 boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
};

enum class MeshKind : uint32_t {
    Plane = 0,
    Cube = 1,
    Sphere = 2,
    Quad = 3,
    SkinnedCharacter = 4,
};

// One procedural character rig. The skeleton is a 12-bone hierarchy authored
// here, not a retail rig; the skin weights and the per-frame pose are generated
// in this module so the moving category has a real skinned draw without a
// third-party asset. Bone 11 is unused and kept for palette padding.
constexpr uint32_t kCharacterBoneCount = 12;

struct CharacterBone {
    DirectX::XMFLOAT3 head;
    int parent;  // -1 for a root
};

extern const CharacterBone kCharacterSkeleton[kCharacterBoneCount];

std::vector<Vertex> MakePlaneVertices(float size, float uvScale);
std::vector<uint32_t> MakePlaneIndices();

std::vector<Vertex> MakeCubeVertices();
std::vector<uint32_t> MakeCubeIndices();

std::vector<Vertex> MakeSphereVertices(uint32_t segments, uint32_t rings);
std::vector<uint32_t> MakeSphereIndices(uint32_t segments, uint32_t rings);

std::vector<Vertex> MakeQuadVertices();
std::vector<uint32_t> MakeQuadIndices();

// A stylised humanoid in bind pose: torso, head, two arms and two legs, with
// smooth two-bone weights across the elbows, knees and the torso/hip seam.
std::vector<Vertex> MakeCharacterVertices();
std::vector<uint32_t> MakeCharacterIndices();

// Linear-blend skinning palette for `phase` (a frame index in [0, 8) used to
// drive the pose). skin[i] maps bind-pose model space to posed model space for
// bone i, i.e. p' = sum_i w_i * (p * skin[i]) with row vectors. At phase 0 the
// pose is the bind pose up to the small authored offset documented in mesh.cpp.
void BuildCharacterSkinMatrices(float phase,
                                DirectX::XMFLOAT4X4* skin,
                                uint32_t count = kCharacterBoneCount);

}  // namespace q4
