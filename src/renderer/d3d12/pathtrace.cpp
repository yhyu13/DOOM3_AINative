// CPU reference path integrator for Q6 (spec 004 T065).
//
// One path is traced iteratively with a deterministic generator. The model is
// the one documented in pathtrace_material.h: cosine-weighted Lambertian,
// a bounded conductor lobe, Fresnel/Snell dielectric transport and emission.
// Direct illumination uses next-event estimation against point lights; shadow
// rays use the same alpha mask as the primary path. No vendor API is called and
// no interactive-performance claim is made.

#include "rhi/pathtrace.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace doom3 {
namespace rhi {

namespace {

constexpr float kEpsilon = 1.0e-4f;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kMaxAlphaRejections = 64;
constexpr uint64_t kSplitMixIncrement = 0x9E3779B97F4A7C15ull;

uint64_t SplitMix64(uint64_t value) {
    value += kSplitMixIncrement;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

LinearRgb Add(const LinearRgb& a, const LinearRgb& b) {
    return LinearRgb{a.r + b.r, a.g + b.g, a.b + b.b};
}

LinearRgb Mul(const LinearRgb& a, const LinearRgb& b) {
    return LinearRgb{a.r * b.r, a.g * b.g, a.b * b.b};
}

LinearRgb Scale(const LinearRgb& a, float s) {
    return LinearRgb{a.r * s, a.g * s, a.b * s};
}

bool IsUsable(const LinearRgb& value) {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           value.r >= 0.0f && value.g >= 0.0f && value.b >= 0.0f;
}

Status Fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return Status::InvalidArgument;
}

// Nearest geometric hit that survives the material alpha mask, starting at
// `tMin`. An out-of-range material index is an error, not a skip.
Status IntersectAlphaAware(const RaytracingScene& scene,
                           const std::vector<PathMaterial>& materials, const RtRay& ray,
                           float tMin, float tMax, RtHit* hit, uint32_t* alphaRejects,
                           bool* found, std::string* error) {
    float currentMin = tMin;
    RtRay currentRay = ray;
    *found = false;
    for (uint32_t attempt = 0; attempt <= kMaxAlphaRejections; ++attempt) {
        RtHit candidate;
        if (!scene.Intersect(currentRay, currentMin, tMax, &candidate)) {
            return Status::Ok;
        }
        if (candidate.material >= materials.size()) {
            if (error != nullptr) {
                *error = "triangle material index " + std::to_string(candidate.material) +
                         " is outside the material table";
            }
            return Status::Error;
        }
        const PathMaterial& material = materials[candidate.material];
        if (material.alphaTest && AlphaTestDiscards(material, candidate.uv)) {
            ++(*alphaRejects);
            // Keep the origin so the next parameter is measured on the same
            // ray; advancing the origin would make `candidate.t` meaningless.
            currentMin = candidate.t + kEpsilon;
            continue;
        }
        *hit = candidate;
        *found = true;
        return Status::Ok;
    }
    // A stack of more than the rejection cap is treated as no hit rather than an
    // unbounded loop. The cap is documented in the header.
    return Status::Ok;
}

Status TraceOccluded(const RaytracingScene& scene,
                     const std::vector<PathMaterial>& materials, const RtRay& ray, float tMax,
                     PathTraceCounters* counters, bool* occluded, std::string* error) {
    RtHit hit;
    bool found = false;
    const Status status = IntersectAlphaAware(scene, materials, ray, kEpsilon, tMax, &hit,
                                              &counters->alphaRejects, &found, error);
    if (status != Status::Ok) {
        return status;
    }
    *occluded = found;
    return Status::Ok;
}

}  // namespace

RtRandom::RtRandom(uint64_t seed) : state_(SplitMix64(seed)) {}

float RtRandom::NextUniform() {
    state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
    const uint32_t bits = static_cast<uint32_t>(state_ >> 40);
    return static_cast<float>(bits) / 16777216.0f;
}

uint64_t BuildSampleSeed(uint64_t baseSeed, uint32_t x, uint32_t y, uint32_t sampleIndex) {
    uint64_t value = baseSeed;
    value ^= static_cast<uint64_t>(x) * 0xD6E8FEB86659FD93ull;
    value ^= static_cast<uint64_t>(y) * 0xA5A3564E27F8862Full;
    value ^= static_cast<uint64_t>(sampleIndex) * 0x9E3779B97F4A7C15ull;
    return SplitMix64(value);
}

float HashToUnitFloat(uint64_t hash) {
    const uint32_t bits = static_cast<uint32_t>(hash >> 40);
    return static_cast<float>(bits) / 16777216.0f;
}

RtRay GenerateCameraRay(const PathCamera& camera, uint32_t x, uint32_t y, float jitterX,
                        float jitterY) {
    RtRay ray;
    ray.origin = camera.position;
    if (camera.width == 0 || camera.height == 0) {
        ray.direction = camera.forward;
        return ray;
    }
    const float u = (static_cast<float>(x) + jitterX) / static_cast<float>(camera.width);
    const float v = (static_cast<float>(y) + jitterY) / static_cast<float>(camera.height);
    const float ndcX = 2.0f * u - 1.0f;
    const float ndcY = 1.0f - 2.0f * v;
    const RtVec3 direction =
        RtAdd(RtAdd(camera.forward, RtScale(camera.right, ndcX * camera.tanHalfFovX)),
              RtScale(camera.up, ndcY * camera.tanHalfFovY));
    RtVec3 normalized;
    if (RtNormalize(direction, &normalized)) {
        ray.direction = normalized;
    } else {
        ray.direction = camera.forward;
    }
    return ray;
}

LinearRgb EvaluateEnvironment(const PathEnvironment& environment, const RtVec3& direction) {
    LinearRgb result;
    if (!environment.enabled) {
        return result;
    }
    result = direction.z >= 0.0f ? environment.upper : environment.lower;
    return result;
}

Status TraceRadiance(const RaytracingScene& scene,
                     const std::vector<PathMaterial>& materials, const RtRay& primaryRay,
                     const PointLight* lights, uint32_t lightCount,
                     const PathTraceSettings& settings, uint64_t seed,
                     PathTraceSample* sample, PathTraceCounters* counters, std::string* error) {
    if (sample == nullptr) {
        return Fail(error, "TraceRadiance needs an output sample");
    }
    *sample = PathTraceSample();
    PathTraceCounters localCounters;
    PathTraceCounters& stats = counters != nullptr ? *counters : localCounters;
    stats = PathTraceCounters();

    if (settings.maxDepth == 0) {
        return Fail(error, "maxDepth must be at least 1");
    }
    if (lights == nullptr && lightCount != 0) {
        return Fail(error, "lightCount is non-zero but lights is null");
    }
    if (!IsUsable(settings.environment.upper) || !IsUsable(settings.environment.lower)) {
        return Fail(error, "environment radiance must be finite and non-negative");
    }
    for (const PathMaterial& material : materials) {
        if (!IsUsable(material.albedo) || !IsUsable(material.transmissionTint) ||
            !IsUsable(material.emission)) {
            return Fail(error, "material colour must be finite and non-negative");
        }
        if (!(material.roughness >= 0.0f && material.roughness <= 1.0f)) {
            return Fail(error, "material roughness must be in [0,1]");
        }
        if (!std::isfinite(material.ior) || !(material.ior > 0.0f)) {
            return Fail(error, "material ior must be finite and positive");
        }
        if (!(material.alphaThreshold >= 0.0f && material.alphaThreshold <= 1.0f)) {
            return Fail(error, "material alpha threshold must be in [0,1]");
        }
    }
    if (!RtIsFinite(primaryRay.origin)) {
        return Fail(error, "primary ray origin is not finite");
    }
    RtVec3 direction;
    if (!RtNormalize(primaryRay.direction, &direction)) {
        return Fail(error, "primary ray direction is zero or not finite");
    }

    RtRandom random(seed);
    LinearRgb radiance;
    LinearRgb throughput{1.0f, 1.0f, 1.0f};
    RtRay ray{primaryRay.origin, direction};
    bool firstHit = true;

    for (uint32_t depth = 0; depth < settings.maxDepth; ++depth) {
        RtHit hit;
        bool found = false;
        const Status hitStatus = IntersectAlphaAware(scene, materials, ray, kEpsilon,
                                                     std::numeric_limits<float>::infinity(), &hit,
                                                     &stats.alphaRejects, &found, error);
        if (hitStatus != Status::Ok) {
            return hitStatus;
        }
        if (!found) {
            if (settings.environment.enabled) {
                ++stats.environmentHits;
                radiance = Add(radiance, Mul(throughput, EvaluateEnvironment(
                                                               settings.environment,
                                                               ray.direction)));
            }
            break;
        }

        if (hit.material >= materials.size()) {
            if (error != nullptr) {
                *error = "triangle material index outside the material table";
            }
            return Status::Error;
        }
        const PathMaterial& material = materials[hit.material];

        if (firstHit) {
            sample->hit = true;
            sample->firstHitDistance = hit.t;
            sample->firstHitTriangle = hit.triangle;
            sample->firstHitMaterial = hit.material;
            firstHit = false;
        }

        if (MaxChannel(material.emission) > 0.0f) {
            ++stats.emissiveHits;
            radiance = Add(radiance, Mul(throughput, material.emission));
            if (material.kind == PathMaterialKind::Emissive) {
                break;
            }
        } else if (material.kind == PathMaterialKind::Emissive) {
            // An emissive material with zero emission still terminates the path;
            // otherwise it would behave as a black diffuse surface by accident.
            break;
        }

        const bool entering = RtDot(hit.normal, ray.direction) < 0.0f;
        const RtVec3 facing = entering ? hit.normal : RtScale(hit.normal, -1.0f);

        if (settings.nextEventEstimation && material.kind == PathMaterialKind::Diffuse &&
            lights != nullptr) {
            for (uint32_t lightIndex = 0; lightIndex < lightCount; ++lightIndex) {
                const PointLight& light = lights[lightIndex];
                if (!IsUsable(light.radiance) || !RtIsFinite(light.position)) {
                    if (error != nullptr) {
                        *error = "point light is not finite";
                    }
                    return Status::InvalidArgument;
                }
                const RtVec3 offset = RtSub(light.position, hit.position);
                const float distanceSquared = RtDot(offset, offset);
                if (!(distanceSquared > kEpsilon * kEpsilon)) {
                    continue;
                }
                const float distance = std::sqrt(distanceSquared);
                const RtVec3 toLight = RtScale(offset, 1.0f / distance);
                if (!(RtDot(facing, toLight) > 0.0f)) {
                    continue;
                }
                ++stats.shadowRays;
                const RtRay shadowRay{RtAdd(hit.position, RtScale(facing, kEpsilon)), toLight};
                bool occluded = false;
                const Status shadowStatus = TraceOccluded(scene, materials, shadowRay,
                                                          distance - kEpsilon, &stats,
                                                          &occluded, error);
                if (shadowStatus != Status::Ok) {
                    return shadowStatus;
                }
                if (occluded) {
                    ++stats.shadowOcclusions;
                    continue;
                }
                const LinearRgb reflectance = EvaluateLambertian(material, facing, toLight);
                const LinearRgb contribution = Scale(
                    Mul(Mul(throughput, reflectance), light.radiance), 1.0f / distanceSquared);
                radiance = Add(radiance, contribution);
            }
        }

        RtVec3 nextDirection;
        LinearRgb weight{1.0f, 1.0f, 1.0f};
        RtVec3 nextOrigin = RtAdd(hit.position, RtScale(facing, kEpsilon));
        bool scattered = true;

        switch (material.kind) {
            case PathMaterialKind::Diffuse: {
                nextDirection = SampleCosineHemisphere(facing, random.NextUniform(),
                                                      random.NextUniform(), nullptr);
                weight = material.albedo;
                break;
            }
            case PathMaterialKind::Conductor: {
                if (!SampleConductorDirection(material, ray.direction, facing,
                                             random.NextUniform(), random.NextUniform(),
                                             &nextDirection)) {
                    scattered = false;
                }
                weight = material.albedo;
                break;
            }
            case PathMaterialKind::Dielectric: {
                const float cosine = std::max(0.0f, -RtDot(ray.direction, facing));
                const float fresnel = FresnelSchlick(0.04f, cosine);
                if (random.NextUniform() < fresnel) {
                    nextDirection = ReflectDirection(ray.direction, facing);
                    weight = LinearRgb{1.0f, 1.0f, 1.0f};
                } else {
                    const float ior = std::isfinite(material.ior) && material.ior > 0.0f
                                          ? material.ior
                                          : 1.5f;
                    const float eta = entering ? 1.0f / ior : ior;
                    if (!RefractDirection(ray.direction, facing, eta, &nextDirection)) {
                        // Total internal reflection: the interface reflects.
                        nextDirection = ReflectDirection(ray.direction, facing);
                        weight = LinearRgb{1.0f, 1.0f, 1.0f};
                    } else {
                        weight = material.transmissionTint;
                        nextOrigin = RtSub(hit.position, RtScale(facing, kEpsilon));
                        ++stats.transmissions;
                    }
                }
                break;
            }
            case PathMaterialKind::Emissive: {
                scattered = false;
                break;
            }
        }

        if (!scattered) {
            break;
        }
        throughput = Mul(throughput, weight);
        if (!IsUsable(throughput) || Luminance(throughput) <= 0.0f) {
            break;
        }
        ray.origin = nextOrigin;
        ray.direction = nextDirection;
        ++stats.bounces;
    }

    sample->radiance = radiance;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status RenderReference(const RaytracingScene& scene,
                       const std::vector<PathMaterial>& materials, const PathCamera& camera,
                       const PointLight* lights, uint32_t lightCount,
                       const PathTraceSettings& settings, uint64_t baseSeed,
                       uint32_t samplesPerPixel, float* rgb, float* depth, std::string* error) {
    if (rgb == nullptr) {
        return Fail(error, "RenderReference needs an output buffer");
    }
    if (camera.width == 0 || camera.height == 0) {
        return Fail(error, "camera extent must be non-zero");
    }
    if (samplesPerPixel == 0) {
        return Fail(error, "samplesPerPixel must be at least 1");
    }

    const uint32_t width = camera.width;
    const uint32_t height = camera.height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            LinearRgb sum;
            uint32_t counted = 0;
            float firstDistance = -1.0f;
            for (uint32_t sampleIndex = 0; sampleIndex < samplesPerPixel; ++sampleIndex) {
                const uint64_t seed = BuildSampleSeed(baseSeed, x, y, sampleIndex);
                const float jitterX = HashToUnitFloat(seed ^ 0x243F6A8885A308D3ull);
                const float jitterY = HashToUnitFloat(seed ^ 0x13198A2E03707344ull);
                const RtRay ray = GenerateCameraRay(camera, x, y, jitterX, jitterY);
                PathTraceSample sample;
                const Status status = TraceRadiance(scene, materials, ray, lights, lightCount,
                                                    settings, seed, &sample, nullptr, error);
                if (status != Status::Ok) {
                    return status;
                }
                sum = Add(sum, sample.radiance);
                ++counted;
                if (sampleIndex == 0) {
                    firstDistance = sample.hit ? sample.firstHitDistance : -1.0f;
                }
            }
            const size_t index = (static_cast<size_t>(y) * width + x) * 3;
            const float inverse = 1.0f / static_cast<float>(counted);
            rgb[index + 0] = sum.r * inverse;
            rgb[index + 1] = sum.g * inverse;
            rgb[index + 2] = sum.b * inverse;
            if (depth != nullptr) {
                depth[static_cast<size_t>(y) * width + x] = firstDistance;
            }
        }
    }
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
