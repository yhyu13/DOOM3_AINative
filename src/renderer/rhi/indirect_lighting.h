#pragma once

// Indirect light, reflection and atmosphere inputs for Q4 (spec 004 T053).
//
// These are the backend-neutral inputs the hybrid path consumes after the
// direct term: an L1 spherical-harmonic irradiance probe, a bounded
// environment reflection with a documented below-horizon fallback, and an
// exponential height atmosphere. The point of the unit is not to claim a
// physically based result; it is to make each artifact case checkable without a
// device so a NaN, a negative radiance or a black below-horizon reflection is
// refused or explicitly handled instead of reaching a frame.
//
// No native type, handle or synchronization value appears here.

#include "rhi/lighting.h"
#include "rhi/linear_color.h"
#include "rhi/rhi.h"

#include <string>

namespace doom3 {
namespace rhi {

// L1 spherical-harmonic irradiance. Coefficient order follows the usual
// real-basis listing: c0 = Y00, c1 = Y(1,-1) on +y, c2 = Y(1,0) on +z,
// c3 = Y(1,1) on +x. Values are linear and may be HDR.
struct Sh4 {
    float c[4][3] = {};
};

// `irradiance(n) = c0 * 0.282095 + (c1 * y + c2 * z + c3 * x) * 0.488603`.
// A negative result is clamped to zero and reported through
// `clampedNegative`, because negative radiance is an artifact rather than a
// valid light value. Returns false for a zero/non-finite normal and leaves the
// outputs untouched.
bool EvaluateIrradiance(const Sh4& probe, const LightVec3& normal, LinearRgb* irradiance,
                        bool* clampedNegative);

struct EnvironmentLighting {
    LinearRgb zenith{0.3f, 0.35f, 0.5f};
    LinearRgb horizon{0.5f, 0.5f, 0.55f};
    LinearRgb ground{0.1f, 0.1f, 0.1f};
};

struct ReflectionInput {
    // Incident direction from the surface toward the viewer or the reflected
    // ray source; the normal is the surface normal. Both are normalized here.
    LightVec3 incident{0.0f, 0.0f, 1.0f};
    LightVec3 normal{0.0f, 0.0f, 1.0f};
    float roughness = 0.0f;
    EnvironmentLighting environment;
};

struct ReflectionResult {
    Status status = Status::Ok;
    std::string error;
    LinearRgb reflection;
    // The reflected direction pointed below the horizon. The result uses the
    // ground/horizon blend instead of a black sample, and this flag says so.
    bool horizonFallback = false;
    // Roughness blended the sky toward its average. A mirror-like value is not
    // blurred.
    bool blurred = false;
};

// Evaluates a bounded environment reflection. InvalidArgument covers a zero
// direction, roughness outside [0,1] and non-finite or negative environment
// radiance. A below-horizon reflection is Ok with `horizonFallback = true`.
Status EvaluateReflection(const ReflectionInput& input, ReflectionResult* result,
                          std::string* error);

struct AtmosphereInput {
    bool enabled = true;
    float distance = 0.0f;
    // Extinction per world unit at the fog reference height, and the height
    // falloff. Both must be finite and non-negative.
    float density = 0.0f;
    float heightFalloff = 0.0f;
    float cameraHeight = 0.0f;
    float fogHeight = 0.0f;
    // Linear in-scatter colour. HDR values are preserved.
    LinearRgb fogColor{0.5f, 0.5f, 0.5f};
};

struct AtmosphereResult {
    Status status = Status::Ok;
    std::string error;
    float transmittance = 1.0f;
    LinearRgb inScatter;
};

// `opticalDepth = density * distance * exp(-heightFalloff * max(cameraHeight -
// fogHeight, 0))`, `transmittance = exp(-opticalDepth)` and
// `inScatter = fogColor * (1 - transmittance)`. A disabled atmosphere is fully
// transparent with zero in-scatter. InvalidArgument covers negative or
// non-finite inputs.
Status EvaluateAtmosphere(const AtmosphereInput& input, AtmosphereResult* result,
                          std::string* error);

}  // namespace rhi
}  // namespace doom3
