// CPU reference acceleration structure (spec 004 T066).
//
// This is backend-neutral arithmetic: a median-split bounding volume hierarchy
// over triangles, a bottom-up refit for pose changes, one affine instance
// placement and a nearest-hit query. It compiles and runs without a device and
// is the independent reference the Q6 tests and a future D3D12 implementation
// are checked against. It is not a vendor acceleration-structure builder and
// makes no performance claim.

#include "rhi/raytracing_scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace doom3 {
namespace rhi {

namespace {

constexpr uint32_t kLeafSize = 4;
// Median split keeps the depth close to log2(triangles / kLeafSize); the cap is
// a guard, not a target.
constexpr int32_t kMaxStack = 96;
constexpr float kMinTriangleAreaSquared = 1.0e-12f;

bool IsValidTriangle(const RtTriangle& triangle) {
    if (!RtIsFinite(triangle.v0) || !RtIsFinite(triangle.v1) || !RtIsFinite(triangle.v2)) {
        return false;
    }
    const RtVec3 e1 = RtSub(triangle.v1, triangle.v0);
    const RtVec3 e2 = RtSub(triangle.v2, triangle.v0);
    const RtVec3 n = RtCross(e1, e2);
    const float lengthSquared = RtDot(n, n);
    return std::isfinite(lengthSquared) && lengthSquared > kMinTriangleAreaSquared;
}

void Expand(RtVec3* boundsMin, RtVec3* boundsMax, const RtVec3& point) {
    boundsMin->x = std::min(boundsMin->x, point.x);
    boundsMin->y = std::min(boundsMin->y, point.y);
    boundsMin->z = std::min(boundsMin->z, point.z);
    boundsMax->x = std::max(boundsMax->x, point.x);
    boundsMax->y = std::max(boundsMax->y, point.y);
    boundsMax->z = std::max(boundsMax->z, point.z);
}

RtVec3 MinVec(const RtVec3& a, const RtVec3& b) {
    return RtVec3{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

RtVec3 MaxVec(const RtVec3& a, const RtVec3& b) {
    return RtVec3{std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

bool SlabTest(const RtVec3& boundsMin, const RtVec3& boundsMax, const RtVec3& origin,
              const RtVec3& direction, float tMin, float tMax) {
    float near = tMin;
    float far = tMax;
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {direction.x, direction.y, direction.z};
    const float lo[3] = {boundsMin.x, boundsMin.y, boundsMin.z};
    const float hi[3] = {boundsMax.x, boundsMax.y, boundsMax.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (d[axis] == 0.0f) {
            if (o[axis] < lo[axis] || o[axis] > hi[axis]) {
                return false;
            }
            continue;
        }
        const float inverse = 1.0f / d[axis];
        float t0 = (lo[axis] - o[axis]) * inverse;
        float t1 = (hi[axis] - o[axis]) * inverse;
        if (t0 > t1) {
            const float swap = t0;
            t0 = t1;
            t1 = swap;
        }
        near = std::max(near, t0);
        far = std::min(far, t1);
        if (near > far) {
            return false;
        }
    }
    return true;
}

// Moller-Trumbore intersection of `ray` with one triangle. Two-sided.
bool IntersectTriangle(const RtTriangle& triangle, const RtVec3& origin, const RtVec3& direction,
                       float tMin, float tMax, float* tOut, float* uOut, float* vOut) {
    const RtVec3 e1 = RtSub(triangle.v1, triangle.v0);
    const RtVec3 e2 = RtSub(triangle.v2, triangle.v0);
    const RtVec3 pvec = RtCross(direction, e2);
    const float determinant = RtDot(e1, pvec);
    if (std::fabs(determinant) < 1.0e-12f) {
        return false;
    }
    const float inverseDeterminant = 1.0f / determinant;
    const RtVec3 tvec = RtSub(origin, triangle.v0);
    const float u = RtDot(tvec, pvec) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const RtVec3 qvec = RtCross(tvec, e1);
    const float v = RtDot(direction, qvec) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    const float t = RtDot(e2, qvec) * inverseDeterminant;
    if (!(t >= tMin) || !(t <= tMax)) {
        return false;
    }
    *tOut = t;
    *uOut = u;
    *vOut = v;
    return true;
}

}  // namespace

RtVec3 RtAdd(const RtVec3& a, const RtVec3& b) {
    return RtVec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

RtVec3 RtSub(const RtVec3& a, const RtVec3& b) {
    return RtVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

RtVec3 RtScale(const RtVec3& a, float s) { return RtVec3{a.x * s, a.y * s, a.z * s}; }

float RtDot(const RtVec3& a, const RtVec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

RtVec3 RtCross(const RtVec3& a, const RtVec3& b) {
    return RtVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float RtLength(const RtVec3& a) { return std::sqrt(RtDot(a, a)); }

bool RtNormalize(const RtVec3& a, RtVec3* out) {
    const float lengthSquared = RtDot(a, a);
    if (out == nullptr || !std::isfinite(lengthSquared) || !(lengthSquared > 1.0e-20f)) {
        return false;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    *out = RtVec3{a.x * inverse, a.y * inverse, a.z * inverse};
    return true;
}

bool RtIsFinite(const RtVec3& a) {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

RtAffine RtAffineTranslation(const RtVec3& offset) {
    RtAffine result;
    result.m[0][3] = offset.x;
    result.m[1][3] = offset.y;
    result.m[2][3] = offset.z;
    return result;
}

RtAffine RtAffineScale(const RtVec3& scale) {
    RtAffine result;
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

RtAffine RtAffineRotationZ(float radians) {
    RtAffine result;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    return result;
}

RtAffine RtAffineMultiply(const RtAffine& a, const RtAffine& b) {
    RtAffine result;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            float value = 0.0f;
            for (int k = 0; k < 3; ++k) {
                value += a.m[row][k] * b.m[k][column];
            }
            if (column == 3) {
                value += a.m[row][3];
            }
            result.m[row][column] = value;
        }
    }
    return result;
}

bool RtAffineInvert(const RtAffine& value, RtAffine* inverse) {
    if (inverse == nullptr) {
        return false;
    }
    const float a00 = value.m[0][0];
    const float a01 = value.m[0][1];
    const float a02 = value.m[0][2];
    const float a10 = value.m[1][0];
    const float a11 = value.m[1][1];
    const float a12 = value.m[1][2];
    const float a20 = value.m[2][0];
    const float a21 = value.m[2][1];
    const float a22 = value.m[2][2];

    const float c00 = a11 * a22 - a12 * a21;
    const float c01 = a12 * a20 - a10 * a22;
    const float c02 = a10 * a21 - a11 * a20;
    const float determinant = a00 * c00 + a01 * c01 + a02 * c02;
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12f) {
        return false;
    }
    const float inverseDeterminant = 1.0f / determinant;

    RtAffine result;
    result.m[0][0] = c00 * inverseDeterminant;
    result.m[0][1] = (a02 * a21 - a01 * a22) * inverseDeterminant;
    result.m[0][2] = (a01 * a12 - a02 * a11) * inverseDeterminant;
    result.m[1][0] = c01 * inverseDeterminant;
    result.m[1][1] = (a00 * a22 - a02 * a20) * inverseDeterminant;
    result.m[1][2] = (a02 * a10 - a00 * a12) * inverseDeterminant;
    result.m[2][0] = c02 * inverseDeterminant;
    result.m[2][1] = (a01 * a20 - a00 * a21) * inverseDeterminant;
    result.m[2][2] = (a00 * a11 - a01 * a10) * inverseDeterminant;

    const RtVec3 translation{value.m[0][3], value.m[1][3], value.m[2][3]};
    const RtAffine linear = result;
    const RtVec3 shifted = RtTransformVector(linear, translation);
    result.m[0][3] = -shifted.x;
    result.m[1][3] = -shifted.y;
    result.m[2][3] = -shifted.z;

    *inverse = result;
    return true;
}

RtVec3 RtTransformPoint(const RtAffine& value, const RtVec3& point) {
    return RtVec3{
        value.m[0][0] * point.x + value.m[0][1] * point.y + value.m[0][2] * point.z + value.m[0][3],
        value.m[1][0] * point.x + value.m[1][1] * point.y + value.m[1][2] * point.z + value.m[1][3],
        value.m[2][0] * point.x + value.m[2][1] * point.y + value.m[2][2] * point.z + value.m[2][3],
    };
}

RtVec3 RtTransformVector(const RtAffine& value, const RtVec3& vector) {
    return RtVec3{
        value.m[0][0] * vector.x + value.m[0][1] * vector.y + value.m[0][2] * vector.z,
        value.m[1][0] * vector.x + value.m[1][1] * vector.y + value.m[1][2] * vector.z,
        value.m[2][0] * vector.x + value.m[2][1] * vector.y + value.m[2][2] * vector.z,
    };
}

RtVec3 RtTransformNormalToWorld(const RtAffine& worldToObject, const RtVec3& objectNormal) {
    // transpose(worldToObject.linear) * objectNormal
    return RtVec3{
        worldToObject.m[0][0] * objectNormal.x + worldToObject.m[1][0] * objectNormal.y +
            worldToObject.m[2][0] * objectNormal.z,
        worldToObject.m[0][1] * objectNormal.x + worldToObject.m[1][1] * objectNormal.y +
            worldToObject.m[2][1] * objectNormal.z,
        worldToObject.m[0][2] * objectNormal.x + worldToObject.m[1][2] * objectNormal.y +
            worldToObject.m[2][2] * objectNormal.z,
    };
}

void RaytracingScene::ComputeTriangleBounds(const RtTriangle& triangle, RtVec3* boundsMin,
                                            RtVec3* boundsMax) const {
    const float infinity = std::numeric_limits<float>::infinity();
    RtVec3 low{infinity, infinity, infinity};
    RtVec3 high{-infinity, -infinity, -infinity};
    Expand(&low, &high, triangle.v0);
    Expand(&low, &high, triangle.v1);
    Expand(&low, &high, triangle.v2);
    *boundsMin = low;
    *boundsMax = high;
}

int32_t RaytracingScene::BuildRange(std::vector<uint32_t>* indices, uint32_t begin, uint32_t end,
                                    uint32_t depth) {
    const uint32_t nodeIndex = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();
    maxDepth_ = std::max(maxDepth_, depth);

    const float infinity = std::numeric_limits<float>::infinity();
    RtVec3 boundsMin{infinity, infinity, infinity};
    RtVec3 boundsMax{-infinity, -infinity, -infinity};
    RtVec3 centroidMin{infinity, infinity, infinity};
    RtVec3 centroidMax{-infinity, -infinity, -infinity};

    for (uint32_t i = begin; i < end; ++i) {
        const RtTriangle& triangle = triangles_[(*indices)[i]];
        const RtVec3 low = RtVec3{std::min(triangle.v0.x, std::min(triangle.v1.x, triangle.v2.x)),
                                  std::min(triangle.v0.y, std::min(triangle.v1.y, triangle.v2.y)),
                                  std::min(triangle.v0.z, std::min(triangle.v1.z, triangle.v2.z))};
        const RtVec3 high = RtVec3{std::max(triangle.v0.x, std::max(triangle.v1.x, triangle.v2.x)),
                                   std::max(triangle.v0.y, std::max(triangle.v1.y, triangle.v2.y)),
                                   std::max(triangle.v0.z, std::max(triangle.v1.z, triangle.v2.z))};
        boundsMin = MinVec(boundsMin, low);
        boundsMax = MaxVec(boundsMax, high);
        const RtVec3 centroid = RtScale(RtAdd(RtAdd(triangle.v0, triangle.v1), triangle.v2),
                                        1.0f / 3.0f);
        Expand(&centroidMin, &centroidMax, centroid);
    }

    if (end - begin <= kLeafSize) {
        Node& node = nodes_[nodeIndex];
        node.boundsMin = boundsMin;
        node.boundsMax = boundsMax;
        node.first = begin;
        node.count = end - begin;
        ++leafCount_;
        return static_cast<int32_t>(nodeIndex);
    }

    const float extentX = centroidMax.x - centroidMin.x;
    const float extentY = centroidMax.y - centroidMin.y;
    const float extentZ = centroidMax.z - centroidMin.z;
    int axis = 0;
    float extent = extentX;
    if (extentY > extent) {
        axis = 1;
        extent = extentY;
    }
    if (extentZ > extent) {
        axis = 2;
    }

    const uint32_t middle = begin + (end - begin) / 2;
    const std::vector<RtTriangle>& triangles = triangles_;
    std::nth_element(indices->begin() + begin, indices->begin() + middle,
                     indices->begin() + end,
                     [&triangles, axis](uint32_t leftIndex, uint32_t rightIndex) {
                         const RtTriangle& left = triangles[leftIndex];
                         const RtTriangle& right = triangles[rightIndex];
                         const float leftCentroid =
                             (axis == 0 ? left.v0.x + left.v1.x + left.v2.x
                                        : (axis == 1 ? left.v0.y + left.v1.y + left.v2.y
                                                     : left.v0.z + left.v1.z + left.v2.z));
                         const float rightCentroid =
                             (axis == 0 ? right.v0.x + right.v1.x + right.v2.x
                                        : (axis == 1 ? right.v0.y + right.v1.y + right.v2.y
                                                     : right.v0.z + right.v1.z + right.v2.z));
                         return leftCentroid < rightCentroid;
                     });

    const int32_t left = BuildRange(indices, begin, middle, depth + 1);
    const int32_t right = BuildRange(indices, middle, end, depth + 1);

    Node& node = nodes_[nodeIndex];
    node.boundsMin = boundsMin;
    node.boundsMax = boundsMax;
    node.left = static_cast<uint32_t>(left);
    node.right = static_cast<uint32_t>(right);
    node.first = 0;
    node.count = 0;
    return static_cast<int32_t>(nodeIndex);
}

bool RaytracingScene::SetGeometry(const std::vector<RtTriangle>& triangles, std::string* error) {
    for (const RtTriangle& triangle : triangles) {
        if (!IsValidTriangle(triangle)) {
            if (error != nullptr) {
                *error = "triangle " + std::to_string(triangle.primitive) +
                         " is non-finite or degenerate";
            }
            return false;
        }
    }

    triangles_ = triangles;
    nodes_.clear();
    order_.resize(triangles_.size());
    for (uint32_t i = 0; i < order_.size(); ++i) {
        order_[i] = i;
    }
    leafCount_ = 0;
    maxDepth_ = 0;
    if (triangles_.empty()) {
        root_ = -1;
    } else {
        nodes_.reserve(triangles_.size() * 2);
        root_ = BuildRange(&order_, 0, static_cast<uint32_t>(order_.size()), 0);
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void RaytracingScene::RefitNode(uint32_t index) {
    Node& node = nodes_[index];
    if (node.count > 0) {
        const float infinity = std::numeric_limits<float>::infinity();
        RtVec3 low{infinity, infinity, infinity};
        RtVec3 high{-infinity, -infinity, -infinity};
        for (uint32_t i = node.first; i < node.first + node.count; ++i) {
            RtVec3 lowTriangle;
            RtVec3 highTriangle;
            ComputeTriangleBounds(triangles_[order_[i]], &lowTriangle, &highTriangle);
            low = MinVec(low, lowTriangle);
            high = MaxVec(high, highTriangle);
        }
        node.boundsMin = low;
        node.boundsMax = high;
        return;
    }
    node.boundsMin = MinVec(nodes_[node.left].boundsMin, nodes_[node.right].boundsMin);
    node.boundsMax = MaxVec(nodes_[node.left].boundsMax, nodes_[node.right].boundsMax);
}

bool RaytracingScene::RefitGeometry(const std::vector<RtTriangle>& posedTriangles,
                                   std::string* error) {
    if (posedTriangles.size() != triangles_.size()) {
        if (error != nullptr) {
            *error = "refit requires the same triangle count; rebuild for a topology change";
        }
        return false;
    }
    for (const RtTriangle& triangle : posedTriangles) {
        if (!IsValidTriangle(triangle)) {
            if (error != nullptr) {
                *error = "posed triangle " + std::to_string(triangle.primitive) +
                         " is non-finite or degenerate";
            }
            return false;
        }
    }

    triangles_ = posedTriangles;
    for (int32_t index = static_cast<int32_t>(nodes_.size()) - 1; index >= 0; --index) {
        RefitNode(static_cast<uint32_t>(index));
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool RaytracingScene::SetInstanceTransform(const RtAffine& objectToWorld, std::string* error) {
    RtAffine inverse;
    if (!RtAffineInvert(objectToWorld, &inverse)) {
        if (error != nullptr) {
            *error = "instance transform is singular";
        }
        return false;
    }
    objectToWorld_ = objectToWorld;
    worldToObject_ = inverse;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool RaytracingScene::Intersect(const RtRay& ray, float tMin, float tMax, RtHit* hit) const {
    lastVisitCount_ = 0;
    if (hit == nullptr) {
        return false;
    }
    *hit = RtHit();
    if (root_ < 0) {
        return false;
    }

    const RtVec3 origin = RtTransformPoint(worldToObject_, ray.origin);
    const RtVec3 direction = RtTransformVector(worldToObject_, ray.direction);
    if (!RtIsFinite(origin) || !RtIsFinite(direction)) {
        return false;
    }

    int32_t stack[kMaxStack];
    int32_t stackSize = 0;
    stack[stackSize++] = root_;

    float best = tMax;
    RtHit bestHit;
    bool found = false;

    while (stackSize > 0) {
        const Node& node = nodes_[static_cast<uint32_t>(stack[--stackSize])];
        ++lastVisitCount_;
        if (!SlabTest(node.boundsMin, node.boundsMax, origin, direction, tMin, best)) {
            continue;
        }
        if (node.count > 0) {
            for (uint32_t i = node.first; i < node.first + node.count; ++i) {
                const uint32_t triangleIndex = order_[i];
                const RtTriangle& triangle = triangles_[triangleIndex];
                float t = 0.0f;
                float u = 0.0f;
                float v = 0.0f;
                if (!IntersectTriangle(triangle, origin, direction, tMin, best, &t, &u, &v)) {
                    continue;
                }
                best = t;
                found = true;
                bestHit.hit = true;
                bestHit.t = t;
                bestHit.u = u;
                bestHit.v = v;
                bestHit.triangle = triangleIndex;
                bestHit.material = triangle.material;
                bestHit.primitive = triangle.primitive;
                bestHit.uv.u = triangle.uv0.u * (1.0f - u - v) + triangle.uv1.u * u +
                               triangle.uv2.u * v;
                bestHit.uv.v = triangle.uv0.v * (1.0f - u - v) + triangle.uv1.v * u +
                               triangle.uv2.v * v;
                const RtVec3 objectPosition = RtAdd(origin, RtScale(direction, t));
                bestHit.position = RtTransformPoint(objectToWorld_, objectPosition);
                RtVec3 objectNormal;
                if (!RtNormalize(
                        RtCross(RtSub(triangle.v1, triangle.v0), RtSub(triangle.v2, triangle.v0)),
                        &objectNormal)) {
                    continue;
                }
                bestHit.normal = RtTransformNormalToWorld(worldToObject_, objectNormal);
                RtNormalize(bestHit.normal, &bestHit.normal);
                const RtVec3 worldDirection = RtTransformVector(objectToWorld_, direction);
                bestHit.backFace = RtDot(bestHit.normal, worldDirection) > 0.0f;
            }
        } else {
            if (stackSize + 2 <= kMaxStack) {
                stack[stackSize++] = static_cast<int32_t>(node.left);
                stack[stackSize++] = static_cast<int32_t>(node.right);
            }
        }
    }

    if (found) {
        *hit = bestHit;
    }
    return found;
}

}  // namespace rhi
}  // namespace doom3
