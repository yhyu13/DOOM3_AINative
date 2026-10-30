#pragma once

// Shared Q6 fixture support (spec 004 T063-T068).
//
// Every expected value in the Q6 tests is an independent literal or a formula
// written in the test. Nothing here calls into the code under test to obtain an
// expectation. This header names no backend type or native handle.

#include "rhi/raytracing_scene.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace dxr_test {

class Runner {
public:
    void Check(bool condition, const std::string& label,
               const std::string& detail = std::string()) {
        ++checks_;
        if (condition) {
            std::printf("PASS %s\n", label.c_str());
        } else {
            ++failures_;
            if (detail.empty()) {
                std::printf("FAIL %s\n", label.c_str());
            } else {
                std::printf("FAIL %s: %s\n", label.c_str(), detail.c_str());
            }
        }
        // Flushed immediately so a hung Debug run still shows the last check.
        std::fflush(stdout);
    }

    void Near(float actual, float expected, float tolerance, const std::string& label) {
        const bool finite = std::isfinite(actual) && std::isfinite(expected);
        const float delta = finite ? std::fabs(actual - expected) : 0.0f;
        Check(finite && delta <= tolerance, label,
              "expected " + std::to_string(expected) + ", observed " +
                  std::to_string(actual));
    }

    int Finish(const char* name) const {
        std::printf("SUMMARY %s checks=%d failures=%d\n", name, checks_, failures_);
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

inline doom3::rhi::RtVec3 V(float x, float y, float z) {
    return doom3::rhi::RtVec3{x, y, z};
}

inline doom3::rhi::RtVec2 UV(float u, float v) {
    return doom3::rhi::RtVec2{u, v};
}

inline doom3::rhi::RtTriangle MakeTriangle(const doom3::rhi::RtVec3& v0,
                                           const doom3::rhi::RtVec3& v1,
                                           const doom3::rhi::RtVec3& v2,
                                           const doom3::rhi::RtVec2& uv0,
                                           const doom3::rhi::RtVec2& uv1,
                                           const doom3::rhi::RtVec2& uv2, uint32_t material,
                                           uint32_t primitive) {
    doom3::rhi::RtTriangle triangle;
    triangle.v0 = v0;
    triangle.v1 = v1;
    triangle.v2 = v2;
    triangle.uv0 = uv0;
    triangle.uv1 = uv1;
    triangle.uv2 = uv2;
    triangle.material = material;
    triangle.primitive = primitive;
    return triangle;
}

// A quad in counter-clockwise order as seen from its front, split into two
// triangles. `uvScale` maps the unit square onto the requested uv range so a
// test can place the whole quad inside one alpha-mask cell.
inline void AddQuad(std::vector<doom3::rhi::RtTriangle>* triangles,
                    const doom3::rhi::RtVec3& p0, const doom3::rhi::RtVec3& p1,
                    const doom3::rhi::RtVec3& p2, const doom3::rhi::RtVec3& p3,
                    uint32_t material, uint32_t primitive,
                    const doom3::rhi::RtVec2& uvMin = UV(0.0f, 0.0f),
                    const doom3::rhi::RtVec2& uvMax = UV(1.0f, 1.0f)) {
    const doom3::rhi::RtVec2 uv0{uvMin.u, uvMin.v};
    const doom3::rhi::RtVec2 uv1{uvMax.u, uvMin.v};
    const doom3::rhi::RtVec2 uv2{uvMax.u, uvMax.v};
    const doom3::rhi::RtVec2 uv3{uvMin.u, uvMax.v};
    triangles->push_back(MakeTriangle(p0, p1, p2, uv0, uv1, uv2, material, primitive));
    triangles->push_back(MakeTriangle(p0, p2, p3, uv0, uv2, uv3, material, primitive));
}

// Horizontal quad at z = `height` facing up.
inline void AddFloor(std::vector<doom3::rhi::RtTriangle>* triangles, float halfExtent,
                     float height, uint32_t material, uint32_t primitive) {
    AddQuad(triangles, V(-halfExtent, -halfExtent, height), V(halfExtent, -halfExtent, height),
            V(halfExtent, halfExtent, height), V(-halfExtent, halfExtent, height), material,
            primitive);
}

// Horizontal quad at z = `height` facing down.
inline void AddCeiling(std::vector<doom3::rhi::RtTriangle>* triangles, float halfExtent,
                       float height, uint32_t material, uint32_t primitive) {
    AddQuad(triangles, V(-halfExtent, -halfExtent, height), V(-halfExtent, halfExtent, height),
            V(halfExtent, halfExtent, height), V(halfExtent, -halfExtent, height), material,
            primitive);
}

inline float Luma(const float* rgb) {
    return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
}

}  // namespace dxr_test
