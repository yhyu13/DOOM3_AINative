#pragma once

// Material sampling for the Q6 path-traced slice (spec 004 T067).
//
// The model is deliberately bounded and documented. It is a reference
// integrator for diagnosing the hybrid renderer, not a claim of a physically
// exact BRDF. Each function states the distribution it samples and the weight
// it returns, so a test can compare against an independently computed formula.
//
// Four kinds are supported:
//
//   Diffuse     Lambertian. Cosine-weighted hemisphere sampling; the returned
//               weight is the albedo, because f * cos / pdf = albedo / pi *
//               cos / (cos / pi) = albedo.
//   Conductor   A bounded two-lobe model: with probability `roughness` a
//               cosine-weighted sample around the normal, otherwise the exact
//               mirror direction. It is not GGX and does not claim a microfacet
//               distribution; the weight is the albedo in both lobes.
//   Dielectric  Fresnel-weighted reflection or Snell refraction. The branch is
//               chosen with the Schlick approximation of the dielectric
//               Fresnel term; the reflect weight is 1 and the transmit weight
//               is `transmissionTint`. The expectation is unbiased for the
//               stated probability model.
//   Emissive    Emission only. The integrator terminates after adding it.
//
// Alpha testing is a deterministic fixture mask: a 2x2 checkerboard in the
// surface uv. It makes "ray passes through a hole and reaches what is behind"
// checkable without an image, and it is not a claim about engine texture
// sampling. The mask is applied by the integrator, which continues the same ray
// past a rejected hit.

#include "rhi/linear_color.h"
#include "rhi/raytracing_scene.h"

#include <cstdint>

namespace doom3 {
namespace rhi {

enum class PathMaterialKind {
    Diffuse = 0,
    Conductor,
    Dielectric,
    Emissive,
};

const char* ToString(PathMaterialKind value);

struct PathMaterial {
    PathMaterialKind kind = PathMaterialKind::Diffuse;
    // Linear base colour. HDR values are allowed but are not a transmission or
    // emission claim; `emission` carries emitted radiance separately.
    LinearRgb albedo{0.5f, 0.5f, 0.5f};
    // Samples the rough lobe of a conductor. In [0, 1]; 0 is a perfect mirror.
    float roughness = 1.0f;
    // Index of refraction for the dielectric interface (air is 1).
    float ior = 1.5f;
    // Multiplied into the throughput of a refracted path. A clear interface
    // uses (1, 1, 1).
    LinearRgb transmissionTint{1.0f, 1.0f, 1.0f};
    // Emitted linear radiance. HDR values are preserved.
    LinearRgb emission{0.0f, 0.0f, 0.0f};
    bool alphaTest = false;
    // Coverage below this value discards the sample.
    float alphaThreshold = 0.5f;
};

// Checkerboard coverage in uv: 1 for two cells, 0 for the other two. A material
// with `alphaTest == false` has full coverage.
float EvaluateAlphaCoverage(const PathMaterial& material, const RtVec2& uv);

bool AlphaTestDiscards(const PathMaterial& material, const RtVec2& uv);

// Schlick's approximation: `f0 + (1 - f0) * (1 - cosine)^5`.
float FresnelSchlick(float f0, float cosine);

// Exact Fresnel reflectance for an unpolarized dielectric interface with the
// surrounding medium at index 1.0 and the other side at `ior`. `cosine` is the
// absolute cosine of the incidence angle.
float DielectricFresnel(float cosine, float ior);

// `incidence` points toward the surface; `normal` faces against it
// (`dot(incidence, normal) < 0`). `eta` is n1 / n2. Returns false on total
// internal reflection and leaves `out` untouched.
bool RefractDirection(const RtVec3& incidence, const RtVec3& normal, float eta, RtVec3* out);

RtVec3 ReflectDirection(const RtVec3& incidence, const RtVec3& normal);

// Cosine-weighted sample in the hemisphere around `normal`. `u1` and `u2` are
// uniform in [0, 1). The sampled direction is normalized and lies in the
// hemisphere. `pdf` receives `cos / pi`.
RtVec3 SampleCosineHemisphere(const RtVec3& normal, float u1, float u2, float* pdf);

// Lambertian reflectance divided by pi, times the clamped cosine. Used by
// next-event estimation, not by the sampled bounce.
LinearRgb EvaluateLambertian(const PathMaterial& material, const RtVec3& normal,
                             const RtVec3& toLight);

// Samples the conductor lobe. `roughness` selects the cosine lobe with that
// probability and the mirror otherwise. Returns false if the material is not a
// conductor or the mirror direction lies below the surface.
bool SampleConductorDirection(const PathMaterial& material, const RtVec3& incidence,
                              const RtVec3& normal, float u1, float u2, RtVec3* direction);

// Reflectance at normal incidence for a dielectric or conductor.
float NormalIncidenceReflectance(const PathMaterial& material);

}  // namespace rhi
}  // namespace doom3
