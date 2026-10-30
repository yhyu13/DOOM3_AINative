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

}  // namespace rhi
}  // namespace doom3
