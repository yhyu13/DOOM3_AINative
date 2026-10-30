#pragma once

// Portable reference path integrator for Q6 (spec 004 T065).
//
// The integrator is a CPU reference: it traces one path per call, with a
// deterministic seed, and returns linear radiance. It exists so the Q6
// behaviour (a static convergence case, material transport, alpha rejection,
// direct lighting and the environment) is checkable without a device or a
// shader. It is not a claim of interactive performance and it does not
// implement any vendor ray-tracing API.
//
// The environment is a two-hemisphere reference: `upper` for directions with
// world z >= 0 and `lower` otherwise. That is enough to test an interface that
// separates reflection from transmission without inventing an HDR map.
//
// Direct lighting uses next-event estimation against point lights when
// enabled. Shadow rays are occluded by geometry and respect the same alpha mask
// as the primary path, so a cut-out does not cast a solid shadow.

#include "rhi/linear_color.h"
#include "rhi/pathtrace_material.h"
#include "rhi/raytracing_scene.h"
#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

struct PathCamera {
    RtVec3 position;
    RtVec3 forward{0.0f, 0.0f, -1.0f};
    RtVec3 right{1.0f, 0.0f, 0.0f};
    RtVec3 up{0.0f, 1.0f, 0.0f};
    float tanHalfFovX = 1.0f;
    float tanHalfFovY = 0.5625f;
    uint32_t width = 1;
    uint32_t height = 1;
};

// Pixel `x` in [0, width), `y` in [0, height) with y = 0 at the top. `jitterX`
// and `jitterY` are in [0, 1) and move the sample inside the pixel.
RtRay GenerateCameraRay(const PathCamera& camera, uint32_t x, uint32_t y, float jitterX,
                        float jitterY);

struct PathEnvironment {
    bool enabled = false;
    // Linear radiance. HDR values are preserved.
    LinearRgb upper{0.0f, 0.0f, 0.0f};
    LinearRgb lower{0.0f, 0.0f, 0.0f};
};

LinearRgb EvaluateEnvironment(const PathEnvironment& environment, const RtVec3& direction);

struct PointLight {
    RtVec3 position;
    // Linear radiance of the emitter. The irradiance at a surface is
    // `radiance * cos / distance^2`; there is no inverse-square singularity
    // because a shadow ray shorter than the epsilon is skipped.
    LinearRgb radiance{1.0f, 1.0f, 1.0f};
};

struct PathTraceSettings {
    // Number of surface interactions after the primary ray. At least 1.
    uint32_t maxDepth = 8;
    bool nextEventEstimation = true;
    PathEnvironment environment;
};

struct PathTraceCounters {
    uint32_t alphaRejects = 0;
    uint32_t transmissions = 0;
    uint32_t emissiveHits = 0;
    uint32_t environmentHits = 0;
    uint32_t bounces = 0;
    uint32_t shadowRays = 0;
    uint32_t shadowOcclusions = 0;
};

struct PathTraceSample {
    LinearRgb radiance;
    bool hit = false;
    // First surface hit by the primary ray. Zero when the primary ray misses.
    float firstHitDistance = 0.0f;
    uint32_t firstHitTriangle = 0;
    uint32_t firstHitMaterial = 0;
};

// Deterministic generator. The tests fix the seed, so a changed result is a
// real change, not sampling luck.
class RtRandom {
public:
    explicit RtRandom(uint64_t seed);
    float NextUniform();

private:
    uint64_t state_ = 0;
};

// A stable seed from the caller's base seed, pixel and sample index.
uint64_t BuildSampleSeed(uint64_t baseSeed, uint32_t x, uint32_t y, uint32_t sampleIndex);
float HashToUnitFloat(uint64_t hash);

// One path. `materials` is indexed by `RtTriangle::material`; an out-of-range
// index is an Error, never a fallback material. `seed` fixes the generator.
Status TraceRadiance(const RaytracingScene& scene,
                     const std::vector<PathMaterial>& materials, const RtRay& primaryRay,
                     const PointLight* lights, uint32_t lightCount,
                     const PathTraceSettings& settings, uint64_t seed,
                     PathTraceSample* sample, PathTraceCounters* counters, std::string* error);

// Renders every pixel of `camera` with `samplesPerPixel` paths. `rgb` receives
// width * height * 3 linear floats. `depth` may be null; when supplied it
// receives the primary hit distance of the first sample (or a negative value
// for a miss) per pixel. `baseSeed` fixes every sample seed.
Status RenderReference(const RaytracingScene& scene,
                       const std::vector<PathMaterial>& materials, const PathCamera& camera,
                       const PointLight* lights, uint32_t lightCount,
                       const PathTraceSettings& settings, uint64_t baseSeed,
                       uint32_t samplesPerPixel, float* rgb, float* depth, std::string* error);

}  // namespace rhi
}  // namespace doom3
