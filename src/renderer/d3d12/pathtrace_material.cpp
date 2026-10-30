// Bounded material sampling for the Q6 reference integrator (spec 004 T067).
//
// Every distribution here is stated in the header and is a documented
// approximation. The tests compare the sampled estimator against a formula
// written independently in the test, so a change in the model that keeps the
// implementation self-consistent still fails a changed expectation.

#include "rhi/pathtrace_material.h"

#include <algorithm>
#include <cmath>

namespace doom3 {
namespace rhi {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float ClampUnit(float value) {
    if (!(value > 0.0f)) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

bool UnitVector(const RtVec3& value) {
    const float lengthSquared = RtDot(value, value);
    return std::isfinite(lengthSquared) && std::fabs(lengthSquared - 1.0f) < 1.0e-4f;
}

}  // namespace

const char* ToString(PathMaterialKind value) {
    switch (value) {
        case PathMaterialKind::Diffuse: return "Diffuse";
        case PathMaterialKind::Conductor: return "Conductor";
        case PathMaterialKind::Dielectric: return "Dielectric";
        case PathMaterialKind::Emissive: return "Emissive";
    }
    return "Unknown";
}

float EvaluateAlphaCoverage(const PathMaterial& material, const RtVec2& uv) {
    if (!material.alphaTest) {
        return 1.0f;
    }
    const float u = uv.u < 0.0f ? 0.0f : (uv.u >= 1.0f ? 0.999999f : uv.u);
    const float v = uv.v < 0.0f ? 0.0f : (uv.v >= 1.0f ? 0.999999f : uv.v);
    const int cellU = static_cast<int>(u * 2.0f);
    const int cellV = static_cast<int>(v * 2.0f);
    return ((cellU + cellV) & 1) == 0 ? 1.0f : 0.0f;
}

bool AlphaTestDiscards(const PathMaterial& material, const RtVec2& uv) {
    if (!material.alphaTest) {
        return false;
    }
    return EvaluateAlphaCoverage(material, uv) < material.alphaThreshold;
}

float FresnelSchlick(float f0, float cosine) {
    const float c = ClampUnit(cosine);
    const float m = 1.0f - c;
    const float m2 = m * m;
    return f0 + (1.0f - f0) * m2 * m2 * m;
}

float DielectricFresnel(float cosine, float ior) {
    const float c = ClampUnit(cosine);
    if (!std::isfinite(ior) || !(ior > 1.0f)) {
        return 1.0f;
    }
    const float sinI2 = std::max(0.0f, 1.0f - c * c);
    const float sinT2 = sinI2 / (ior * ior);
    if (sinT2 >= 1.0f) {
        return 1.0f;
    }
    const float cosT = std::sqrt(1.0f - sinT2);
    const float rs = (c - ior * cosT) / (c + ior * cosT);
    const float rp = (cosT - ior * c) / (cosT + ior * c);
    return 0.5f * (rs * rs + rp * rp);
}

bool RefractDirection(const RtVec3& incidence, const RtVec3& normal, float eta, RtVec3* out) {
    if (out == nullptr || !std::isfinite(eta) || !(eta > 0.0f)) {
        return false;
    }
    if (!UnitVector(incidence) || !UnitVector(normal)) {
        return false;
    }
    const float cosI = -RtDot(incidence, normal);
    if (!(cosI > 0.0f)) {
        return false;
    }
    const float sinT2 = eta * eta * std::max(0.0f, 1.0f - cosI * cosI);
    if (sinT2 >= 1.0f) {
        return false;
    }
    const float cosT = std::sqrt(1.0f - sinT2);
    const RtVec3 direction =
        RtAdd(RtScale(incidence, eta), RtScale(normal, eta * cosI - cosT));
    RtVec3 normalized;
    if (!RtNormalize(direction, &normalized)) {
        return false;
    }
    *out = normalized;
    return true;
}

RtVec3 ReflectDirection(const RtVec3& incidence, const RtVec3& normal) {
    return RtSub(incidence, RtScale(normal, 2.0f * RtDot(incidence, normal)));
}

RtVec3 SampleCosineHemisphere(const RtVec3& normal, float u1, float u2, float* pdf) {
    const float radius = std::sqrt(ClampUnit(u1));
    const float phi = 2.0f * kPi * ClampUnit(u2);
    const float x = radius * std::cos(phi);
    const float y = radius * std::sin(phi);
    const float z = std::sqrt(std::max(0.0f, 1.0f - radius * radius));

    const RtVec3 up = std::fabs(normal.z) < 0.999f ? RtVec3{0.0f, 0.0f, 1.0f}
                                                   : RtVec3{1.0f, 0.0f, 0.0f};
    RtVec3 tangent;
    if (!RtNormalize(RtCross(up, normal), &tangent)) {
        tangent = RtVec3{1.0f, 0.0f, 0.0f};
    }
    const RtVec3 bitangent = RtCross(normal, tangent);
    const RtVec3 direction = RtAdd(
        RtAdd(RtScale(tangent, x), RtScale(bitangent, y)), RtScale(normal, z));
    if (pdf != nullptr) {
        *pdf = z / kPi;
    }
    return direction;
}

LinearRgb EvaluateLambertian(const PathMaterial& material, const RtVec3& normal,
                             const RtVec3& toLight) {
    LinearRgb result;
    if (material.kind != PathMaterialKind::Diffuse) {
        return result;
    }
    const float cosine = std::max(0.0f, RtDot(normal, toLight));
    const float scale = cosine / kPi;
    result.r = material.albedo.r * scale;
    result.g = material.albedo.g * scale;
    result.b = material.albedo.b * scale;
    return result;
}

bool SampleConductorDirection(const PathMaterial& material, const RtVec3& incidence,
                              const RtVec3& normal, float u1, float u2, RtVec3* direction) {
    if (direction == nullptr || material.kind != PathMaterialKind::Conductor) {
        return false;
    }
    const float roughness = ClampUnit(material.roughness);
    const RtVec3 mirror = ReflectDirection(incidence, normal);
    if (RtDot(mirror, normal) <= 0.0f) {
        return false;
    }
    if (ClampUnit(u1) < roughness) {
        // Conditional on the diffuse branch, u1 / roughness is uniform in
        // [0, 1); the direction is a cosine-weighted sample around the normal.
        const float conditional = ClampUnit(u1) / (roughness > 0.0f ? roughness : 1.0f);
        *direction = SampleCosineHemisphere(normal, conditional, u2, nullptr);
        return true;
    }
    *direction = mirror;
    return true;
}

float NormalIncidenceReflectance(const PathMaterial& material) {
    switch (material.kind) {
        case PathMaterialKind::Conductor:
            return ClampUnit((material.albedo.r + material.albedo.g + material.albedo.b) / 3.0f);
        case PathMaterialKind::Dielectric:
            return 0.04f;
        case PathMaterialKind::Diffuse:
        case PathMaterialKind::Emissive:
            return 0.0f;
    }
    return 0.0f;
}

}  // namespace rhi
}  // namespace doom3
