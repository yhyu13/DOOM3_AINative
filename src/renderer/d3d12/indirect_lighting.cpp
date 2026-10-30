// Indirect light, reflection and atmosphere inputs (spec 004 T053).
//
// The implementation is deliberately arithmetic-only: no device, pipeline or
// native call. That keeps the artifact cases cheap to exercise and keeps the
// degradation rules reviewable. In particular a below-horizon reflection is
// never silently black, and an atmosphere cannot multiply an already linear HDR
// value by a display-encoded colour.

#include "rhi/indirect_lighting.h"

#include <cmath>

namespace doom3 {
namespace rhi {

namespace {

constexpr float kEpsilon = 1.0e-6f;

bool Normalize(const LightVec3& value, LightVec3* out) {
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (!(lengthSquared > kEpsilon) || !std::isfinite(lengthSquared)) {
        return false;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    out->x = value.x * inverse;
    out->y = value.y * inverse;
    out->z = value.z * inverse;
    return true;
}

float Dot(const LightVec3& a, const LightVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bool UnitRange(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }

Status Fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return Status::InvalidArgument;
}

void ClearError(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
}

LinearRgb SampleSky(const EnvironmentLighting& environment, float elevation) {
    if (elevation >= 0.0f) {
        return Lerp(environment.horizon, environment.zenith, elevation);
    }
    // Below the horizon: ground toward the horizon, never a black hole.
    return Lerp(environment.ground, environment.horizon, 1.0f + elevation);
}

LinearRgb AverageSky(const EnvironmentLighting& environment) {
    LinearRgb sum;
    sum.r = environment.zenith.r + environment.horizon.r + environment.ground.r;
    sum.g = environment.zenith.g + environment.horizon.g + environment.ground.g;
    sum.b = environment.zenith.b + environment.horizon.b + environment.ground.b;
    sum.r /= 3.0f;
    sum.g /= 3.0f;
    sum.b /= 3.0f;
    return sum;
}

bool EnvironmentValid(const EnvironmentLighting& environment) {
    return IsNonNegativeFinite(environment.zenith) && IsNonNegativeFinite(environment.horizon) &&
           IsNonNegativeFinite(environment.ground);
}

}  // namespace

bool EvaluateIrradiance(const Sh4& probe, const LightVec3& normal, LinearRgb* irradiance,
                        bool* clampedNegative) {
    LightVec3 direction;
    if (irradiance == nullptr || !Normalize(normal, &direction)) {
        return false;
    }
    float values[3];
    for (int channel = 0; channel < 3; ++channel) {
        values[channel] = probe.c[0][channel] * 0.282095f +
                          (probe.c[1][channel] * direction.y +
                           probe.c[2][channel] * direction.z +
                           probe.c[3][channel] * direction.x) *
                              0.488603f;
    }
    bool clamped = false;
    for (int channel = 0; channel < 3; ++channel) {
        if (!std::isfinite(values[channel])) {
            return false;
        }
        if (values[channel] < 0.0f) {
            values[channel] = 0.0f;
            clamped = true;
        }
    }
    irradiance->r = values[0];
    irradiance->g = values[1];
    irradiance->b = values[2];
    if (clampedNegative != nullptr) {
        *clampedNegative = clamped;
    }
    return true;
}

Status EvaluateReflection(const ReflectionInput& input, ReflectionResult* result,
                          std::string* error) {
    if (result == nullptr) {
        return Fail(error, "EvaluateReflection needs an output");
    }
    *result = ReflectionResult();

    LightVec3 normal;
    LightVec3 incident;
    if (!Normalize(input.normal, &normal)) {
        result->status = Status::InvalidArgument;
        result->error = "surface normal is zero";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::InvalidArgument;
    }
    if (!Normalize(input.incident, &incident)) {
        result->status = Status::InvalidArgument;
        result->error = "incident direction is zero";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::InvalidArgument;
    }
    if (!UnitRange(input.roughness)) {
        result->status = Status::InvalidArgument;
        result->error = "roughness must be in [0,1]";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::InvalidArgument;
    }
    if (!EnvironmentValid(input.environment)) {
        result->status = Status::InvalidArgument;
        result->error = "environment radiance must be finite and non-negative";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::InvalidArgument;
    }

    // R = I - 2 (I.N) N
    const float scale = 2.0f * Dot(incident, normal);
    LightVec3 reflected{incident.x - scale * normal.x, incident.y - scale * normal.y,
                        incident.z - scale * normal.z};
    if (!Normalize(reflected, &reflected)) {
        result->status = Status::InvalidArgument;
        result->error = "reflected direction is undefined";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::InvalidArgument;
    }

    const float elevation = reflected.y;
    LinearRgb sharp = SampleSky(input.environment, elevation);
    if (elevation < 0.0f) {
        result->horizonFallback = true;
    }

    if (input.roughness > 0.0f) {
        sharp = Lerp(sharp, AverageSky(input.environment), input.roughness);
        result->blurred = true;
    }
    result->reflection = sharp;
    ClearError(error);
    return Status::Ok;
}

Status EvaluateAtmosphere(const AtmosphereInput& input, AtmosphereResult* result,
                          std::string* error) {
    if (result == nullptr) {
        return Fail(error, "EvaluateAtmosphere needs an output");
    }
    *result = AtmosphereResult();

    if (!input.enabled) {
        result->transmittance = 1.0f;
        result->inScatter = LinearRgb{};
        ClearError(error);
        return Status::Ok;
    }

    if (!(input.distance >= 0.0f) || !std::isfinite(input.distance) ||
        !(input.density >= 0.0f) || !std::isfinite(input.density) ||
        !(input.heightFalloff >= 0.0f) || !std::isfinite(input.heightFalloff) ||
        !std::isfinite(input.cameraHeight) || !std::isfinite(input.fogHeight)) {
        return Fail(error, "atmosphere distance, density and height inputs must be finite; "
                           "distance, density and height falloff must be non-negative");
    }
    if (!IsNonNegativeFinite(input.fogColor)) {
        return Fail(error, "fog colour must be finite and non-negative");
    }

    const float heightAboveFog =
        input.cameraHeight > input.fogHeight ? input.cameraHeight - input.fogHeight : 0.0f;
    const float densityScale = std::exp(-input.heightFalloff * heightAboveFog);
    const float opticalDepth = input.density * input.distance * densityScale;
    if (!std::isfinite(opticalDepth)) {
        return Fail(error, "atmosphere optical depth is not finite");
    }
    const float transmittance = std::exp(-opticalDepth);
    result->transmittance = transmittance;
    result->inScatter.r = input.fogColor.r * (1.0f - transmittance);
    result->inScatter.g = input.fogColor.g * (1.0f - transmittance);
    result->inScatter.b = input.fogColor.b * (1.0f - transmittance);
    ClearError(error);
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
