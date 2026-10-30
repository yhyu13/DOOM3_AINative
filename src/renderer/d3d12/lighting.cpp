// Classic lighting and shadow plan (spec 004 T082).
//
// The plan classifies a viewLight_t into the passes and shadow chains the classic path
// submits. It is deliberately a classification with no stencil state: the backend half
// (stencil ops, two-sided volume, depth clamp) belongs behind the RHI and is not implemented
// by this slice. Recording that separation is the point — a plan is not a shadow pass.
//
// Shadow membership follows Interaction.cpp:1159-1241 as read by classic-parity.md CP-08/CP-09:
// the global chain holds casters that are not the lit surface's own entity, the local chain
// holds the self-caster, `noShadows` removes both, and `noSelfShadow` keeps only the local
// chain.

#include "rhi/lighting.h"

#include <cmath>

namespace doom3 {
namespace rhi {

const char* ToString(LightKind value) {
    switch (value) {
        case LightKind::Ambient: return "Ambient";
        case LightKind::Point: return "Point";
        case LightKind::Projected: return "Projected";
        case LightKind::Fog: return "Fog";
        case LightKind::Blend: return "Blend";
    }
    return "Unknown";
}

const char* ToString(ShadowPolicy value) {
    switch (value) {
        case ShadowPolicy::None: return "None";
        case ShadowPolicy::Global: return "Global";
        case ShadowPolicy::Local: return "Local";
    }
    return "Unknown";
}

const char* ToString(ShadowSuppression value) {
    switch (value) {
        case ShadowSuppression::None: return "None";
        case ShadowSuppression::MaterialNoShadows: return "MaterialNoShadows";
        case ShadowSuppression::LightNoShadows: return "LightNoShadows";
        case ShadowSuppression::MaterialNoSelfShadow: return "MaterialNoSelfShadow";
    }
    return "Unknown";
}

Status PlanLight(const LightInput& light, const MaterialShadowInput& material, bool sameEntity,
                 LightPlan* plan, std::string* error) {
    if (plan == nullptr) {
        if (error != nullptr) {
            *error = "PlanLight needs an output plan";
        }
        return Status::InvalidArgument;
    }
    *plan = LightPlan();

    if (light.kind == LightKind::Fog || light.kind == LightKind::Blend) {
        // CP-11/CP-12: fog and blend are their own passes and never accumulate surface
        // interactions.
        plan->fogOrBlendPass = true;
        plan->interactionPass = false;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    if (light.kind == LightKind::Ambient) {
        // CP-06: the ambient path is the non-light pass, with no specular accumulation.
        plan->ambientPass = true;
        plan->interactionPass = false;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    if (light.kind != LightKind::Point && light.kind != LightKind::Projected) {
        if (error != nullptr) {
            *error = "unknown light kind";
        }
        return Status::InvalidArgument;
    }

    plan->interactionPass = true;
    plan->multiFrustum = light.multiFrustum;

    if (light.kind == LightKind::Projected) {
        // CP-04: a projected light without a cookie is malformed, and a zero falloff would
        // make the light pool unbounded.
        if (!light.hasProjection) {
            if (error != nullptr) {
                *error = "projected light has no projection cookie";
            }
            return Status::InvalidArgument;
        }
        if (!(light.falloffRadius > 0.0f)) {
            if (error != nullptr) {
                *error = "projected light has no positive falloff radius";
            }
            return Status::InvalidArgument;
        }
    }

    if (material.noShadows) {
        plan->suppressed = ShadowSuppression::MaterialNoShadows;
        plan->firstChain = ShadowPolicy::None;
        plan->secondChain = ShadowPolicy::None;
    } else if (light.noShadows) {
        plan->suppressed = ShadowSuppression::LightNoShadows;
        plan->firstChain = ShadowPolicy::None;
        plan->secondChain = ShadowPolicy::None;
    } else if (sameEntity) {
        // The lit surface's own entity shadows itself through the local chain only.
        plan->firstChain = ShadowPolicy::Local;
        plan->secondChain = ShadowPolicy::None;
    } else if (material.noSelfShadow) {
        plan->suppressed = ShadowSuppression::MaterialNoSelfShadow;
        plan->firstChain = ShadowPolicy::Local;
        plan->secondChain = ShadowPolicy::None;
    } else {
        plan->firstChain = ShadowPolicy::Global;
        plan->secondChain = ShadowPolicy::Local;
    }

    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

const char* ToString(SurfaceStage value) {
    switch (value) {
        case SurfaceStage::Opaque: return "Opaque";
        case SurfaceStage::AlphaTested: return "AlphaTested";
        case SurfaceStage::Translucent: return "Translucent";
        case SurfaceStage::Blend: return "Blend";
        case SurfaceStage::Fog: return "Fog";
        case SurfaceStage::Glasswarp: return "Glasswarp";
        case SurfaceStage::Skybox: return "Skybox";
        case SurfaceStage::Decal: return "Decal";
        case SurfaceStage::PostProcess: return "PostProcess";
    }
    return "Unknown";
}

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

bool IsOpaqueStage(SurfaceStage stage) {
    return stage == SurfaceStage::Opaque || stage == SurfaceStage::AlphaTested;
}

bool UnitRange(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }

Status Fail(std::string* error, Status status, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return status;
}

void ClearError(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
}

float SpotFactor(float cosAngle, float cosInner, float cosOuter) {
    if (cosAngle <= cosOuter) {
        return 0.0f;
    }
    if (cosAngle >= cosInner || cosInner <= cosOuter) {
        return 1.0f;
    }
    const float t = (cosAngle - cosOuter) / (cosInner - cosOuter);
    // Smoothstep between the two cone edges.
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float ComputeDistanceAttenuation(float distance, float range) {
    if (!(range > 0.0f)) {
        return 1.0f;
    }
    if (!std::isfinite(distance) || distance < 0.0f) {
        return 0.0f;
    }
    // The light and the receiver coincide: no inverse-square singularity is
    // reported as an astronomical value.
    if (distance == 0.0f) {
        return 1.0f;
    }
    if (distance >= range) {
        return 0.0f;
    }
    const float x = distance / range;
    const float x2 = x * x;
    const float window = 1.0f - x2 * x2;
    return window * window / (distance * distance);
}

Status EvaluateDirectLight(const DirectLightInput& light, const DirectSurfaceInput& surface,
                           const ShadowInput& shadow, DirectLightResult* result,
                           std::string* error) {
    if (result == nullptr) {
        return Fail(error, Status::InvalidArgument, "EvaluateDirectLight needs an output");
    }
    *result = DirectLightResult();

    // Forward/compatibility classes have no direct opaque term. This is the
    // explicit unsupported result T052 asks for, not a silent zero.
    if (!IsOpaqueStage(surface.stage)) {
        result->status = Status::Unsupported;
        result->error = std::string("surface stage ") + ToString(surface.stage) +
                        " has no direct opaque term in the hybrid opaque pass";
        if (error != nullptr) {
            *error = result->error;
        }
        return Status::Unsupported;
    }

    if (!IsNonNegativeFinite(light.radiance)) {
        return Fail(error, Status::InvalidArgument,
                    "light radiance must be finite and non-negative");
    }
    if (!IsNonNegativeFinite(surface.baseColor)) {
        return Fail(error, Status::InvalidArgument,
                    "surface base colour must be finite and non-negative");
    }
    if (!UnitRange(surface.roughness) || !UnitRange(surface.metalness)) {
        return Fail(error, Status::InvalidArgument, "roughness and metalness must be in [0,1]");
    }

    LightVec3 normal;
    LightVec3 view;
    if (!Normalize(surface.normal, &normal)) {
        return Fail(error, Status::InvalidArgument, "surface normal is zero");
    }
    if (!Normalize(surface.view, &view)) {
        return Fail(error, Status::InvalidArgument, "surface view direction is zero");
    }

    if (surface.stage == SurfaceStage::AlphaTested && !surface.alphaTestPass) {
        result->discarded = true;
        result->shaded = false;
        ClearError(error);
        return Status::Ok;
    }

    LightVec3 toLight;
    float attenuation = 1.0f;
    if (light.range > 0.0f) {
        const LightVec3 delta{light.position.x - surface.position.x,
                              light.position.y - surface.position.y,
                              light.position.z - surface.position.z};
        const float distance = std::sqrt(Dot(delta, delta));
        attenuation = ComputeDistanceAttenuation(distance, light.range);
        if (!(attenuation > 0.0f) || !Normalize(delta, &toLight)) {
            // Beyond the range (or coincident): the sample is lit by this light
            // zero times. That is a valid result, not an error.
            result->attenuation = 0.0f;
            ClearError(error);
            return Status::Ok;
        }
    } else {
        const LightVec3 travel{-light.direction.x, -light.direction.y, -light.direction.z};
        if (!Normalize(travel, &toLight)) {
            return Fail(error, Status::InvalidArgument, "directional light has no direction");
        }
    }

    // A spot cone is expressed entirely in cosine space. A zero pair means the
    // light is not a spot.
    if (light.cosInner != 0.0f || light.cosOuter != 0.0f) {
        if (!(light.cosInner >= light.cosOuter) || !UnitRange(light.cosInner) ||
            !UnitRange(light.cosOuter)) {
            return Fail(error, Status::InvalidArgument,
                        "spot cone cosines must satisfy 1 >= inner >= outer >= 0");
        }
        LightVec3 axis{-light.direction.x, -light.direction.y, -light.direction.z};
        if (!Normalize(axis, &axis)) {
            return Fail(error, Status::InvalidArgument, "spot light has no axis");
        }
        const float spot = SpotFactor(Dot(toLight, axis), light.cosInner, light.cosOuter);
        if (!(spot > 0.0f)) {
            result->attenuation = 0.0f;
            ClearError(error);
            return Status::Ok;
        }
        attenuation *= spot;
    }

    const float ndotl = Dot(normal, toLight);
    if (!(ndotl > 0.0f)) {
        result->attenuation = attenuation;
        result->ndotl = 0.0f;
        ClearError(error);
        return Status::Ok;
    }

    float shadowFactor = 1.0f;
    if (light.castsShadow) {
        if (shadow.taps == 0 || shadow.blockers > shadow.taps) {
            return Fail(error, Status::InvalidArgument,
                        "shadow taps must be >= 1 and blockers must not exceed taps");
        }
        if (!std::isfinite(shadow.constantBias) || !std::isfinite(shadow.slopeBias) ||
            shadow.constantBias < 0.0f || shadow.slopeBias < 0.0f) {
            return Fail(error, Status::InvalidArgument,
                        "shadow bias must be finite and non-negative");
        }
        const float bias = shadow.constantBias + shadow.slopeBias * (1.0f - ndotl);
        const bool occluded = shadow.lightDepth < shadow.surfaceDepth - bias;
        if (occluded) {
            const float blocked = static_cast<float>(shadow.blockers) /
                                  static_cast<float>(shadow.taps);
            shadowFactor = 1.0f - blocked;
        }
    }

    const float metalness = surface.metalness;
    const float diffuseWeight = 1.0f - metalness;
    const float gloss = (1.0f - surface.roughness) * (1.0f - surface.roughness);
    const float common = ndotl * attenuation * shadowFactor;

    const LinearRgb f0{0.04f * (1.0f - metalness) + surface.baseColor.r * metalness,
                       0.04f * (1.0f - metalness) + surface.baseColor.g * metalness,
                       0.04f * (1.0f - metalness) + surface.baseColor.b * metalness};

    result->diffuse.r = light.radiance.r * surface.baseColor.r * diffuseWeight * common;
    result->diffuse.g = light.radiance.g * surface.baseColor.g * diffuseWeight * common;
    result->diffuse.b = light.radiance.b * surface.baseColor.b * diffuseWeight * common;

    result->specular.r = light.radiance.r * f0.r * gloss * common;
    result->specular.g = light.radiance.g * f0.g * gloss * common;
    result->specular.b = light.radiance.b * f0.b * gloss * common;

    result->radiance.r = result->diffuse.r + result->specular.r;
    result->radiance.g = result->diffuse.g + result->specular.g;
    result->radiance.b = result->diffuse.b + result->specular.b;

    // `view` participates so a caller cannot pass a degenerate view and still
    // receive a shaded result; the bounded lobe does not otherwise use it.
    (void)view;

    result->status = Status::Ok;
    result->shaded = true;
    result->attenuation = attenuation;
    result->ndotl = ndotl;
    result->shadowFactor = shadowFactor;
    ClearError(error);
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
