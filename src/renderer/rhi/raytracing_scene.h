#pragma once

// CPU reference acceleration structure for the Q6 path-traced slice
// (spec 004 T066).
//
// This unit is the portable half of a ray-tracing scene: triangles, a bounding
// volume hierarchy built from them, a per-instance affine placement, and a
// nearest-hit query. It names no native API type, handle or device object. A
// native ray-tracing implementation is a separate backend concern; this unit is
// what its arithmetic is checked against.
//
// Two update paths are deliberately distinct:
//
//   * `SetGeometry`  builds the hierarchy from scratch. Topology, triangle order
//                    and counts may all change.
//   * `RefitGeometry` keeps the existing hierarchy and order, recomputes every
//                    leaf bound from the new posed vertices and then every
//                    internal bound bottom-up. It is the skinned/rigid pose path
//                    and refuses a changed triangle count instead of guessing.
//
// A refit is correct, not necessarily balanced: moving a triangle far outside
// its old node makes traversal visit more nodes, but every triangle stays inside
// an ancestor bound, so no hit is lost. A caller that grows a hierarchy badly
// through repeated large deformations rebuilds with `SetGeometry`.
//
// A per-instance affine placement stands in for a top-level instance entry: the
// query maps the world ray into object space, traverses there and maps the hit
// back. One instance is enough to test that rigid motion does not require a
// rebuild.

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

struct RtVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RtVec2 {
    float u = 0.0f;
    float v = 0.0f;
};

struct RtRay {
    RtVec3 origin;
    // A parameter direction. The query does not normalize it: `t` is measured
    // in units of `|direction|`.
    RtVec3 direction{0.0f, 0.0f, -1.0f};
};

struct RtTriangle {
    RtVec3 v0;
    RtVec3 v1;
    RtVec3 v2;
    RtVec2 uv0;
    RtVec2 uv1;
    RtVec2 uv2;
    // Index into the caller's material table.
    uint32_t material = 0;
    // Stable identity for diagnostics. Not required to be dense.
    uint32_t primitive = 0;
};

struct RtHit {
    bool hit = false;
    // Ray parameter of the nearest accepted intersection.
    float t = 0.0f;
    RtVec3 position;
    // Geometric normal from the triangle winding, normalized. It is not flipped
    // toward the ray; `backFace` says which side was hit.
    RtVec3 normal;
    RtVec2 uv;
    // Barycentric coordinates: `position = v0 * (1 - u - v) + v1 * u + v2 * v`.
    float u = 0.0f;
    float v = 0.0f;
    uint32_t triangle = 0;
    uint32_t material = 0;
    uint32_t primitive = 0;
    bool backFace = false;
};

// Row-major affine transform:
//   x' = m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3]
// and likewise for y' and z'. The last row is implicitly (0, 0, 0, 1).
struct RtAffine {
    float m[3][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
};

RtVec3 RtAdd(const RtVec3& a, const RtVec3& b);
RtVec3 RtSub(const RtVec3& a, const RtVec3& b);
RtVec3 RtScale(const RtVec3& a, float s);
float RtDot(const RtVec3& a, const RtVec3& b);
RtVec3 RtCross(const RtVec3& a, const RtVec3& b);
float RtLength(const RtVec3& a);
bool RtNormalize(const RtVec3& a, RtVec3* out);

bool RtIsFinite(const RtVec3& a);

// Affine helpers used by the tests and the instance placement.
RtAffine RtAffineTranslation(const RtVec3& offset);
RtAffine RtAffineScale(const RtVec3& scale);
// Rotation about +z by `radians`.
RtAffine RtAffineRotationZ(float radians);
RtAffine RtAffineMultiply(const RtAffine& a, const RtAffine& b);
// Returns false for a singular linear part.
bool RtAffineInvert(const RtAffine& value, RtAffine* inverse);
RtVec3 RtTransformPoint(const RtAffine& value, const RtVec3& point);
RtVec3 RtTransformVector(const RtAffine& value, const RtVec3& vector);
// Normal transform for a transform whose inverse is `worldToObject`:
// `normalize(transpose(worldToObject.linear) * objectNormal)`.
RtVec3 RtTransformNormalToWorld(const RtAffine& worldToObject, const RtVec3& objectNormal);

class RaytracingScene {
public:
    // Builds the hierarchy. A non-finite or zero-area triangle is refused and
    // leaves the previous geometry untouched. An empty triangle list is valid
    // and produces a scene every ray misses.
    bool SetGeometry(const std::vector<RtTriangle>& triangles, std::string* error);

    // Refits an existing hierarchy in place after a pose change. The triangle
    // count and order must match the last `SetGeometry`; a different count or a
    // non-finite/zero-area triangle is refused and leaves the previous geometry
    // untouched. `triangles[i].material` and `triangles[i].primitive` must match
    // too, because the hierarchy order is keyed by triangle index.
    bool RefitGeometry(const std::vector<RtTriangle>& posedTriangles, std::string* error);

    // Object-to-world affine placement. A singular transform is refused and the
    // previous placement is kept. Identity by default.
    bool SetInstanceTransform(const RtAffine& objectToWorld, std::string* error);
    RtAffine instanceTransform() const { return objectToWorld_; }

    // Nearest hit in [tMin, tMax]. `out` is cleared first, so a miss is
    // observable. `out` must not be null.
    bool Intersect(const RtRay& ray, float tMin, float tMax, RtHit* hit) const;

    const std::vector<RtTriangle>& triangles() const { return triangles_; }
    uint32_t triangleCount() const {
        return static_cast<uint32_t>(triangles_.size());
    }
    uint32_t nodeCount() const { return static_cast<uint32_t>(nodes_.size()); }
    uint32_t leafCount() const { return leafCount_; }
    uint32_t maxDepth() const { return maxDepth_; }

    // Total node visits during the most recent Intersect call. Diagnostics for
    // the tests and the evidence file; not part of the hit result.
    uint32_t lastVisitCount() const { return lastVisitCount_; }

private:
    struct Node {
        RtVec3 boundsMin{0.0f, 0.0f, 0.0f};
        RtVec3 boundsMax{0.0f, 0.0f, 0.0f};
        // Internal nodes: children. Leaf: `count > 0`, triangles are
        // `order[first .. first + count)`.
        uint32_t left = 0;
        uint32_t right = 0;
        uint32_t first = 0;
        uint32_t count = 0;
    };

    int32_t BuildRange(std::vector<uint32_t>* indices, uint32_t begin, uint32_t end,
                       uint32_t depth);
    void RefitNode(uint32_t index);
    void ComputeTriangleBounds(const RtTriangle& triangle, RtVec3* boundsMin,
                               RtVec3* boundsMax) const;

    std::vector<RtTriangle> triangles_;
    std::vector<Node> nodes_;
    std::vector<uint32_t> order_;
    int32_t root_ = -1;
    uint32_t leafCount_ = 0;
    uint32_t maxDepth_ = 0;
    RtAffine objectToWorld_;
    RtAffine worldToObject_;
    mutable uint32_t lastVisitCount_ = 0;
};

}  // namespace rhi
}  // namespace doom3
