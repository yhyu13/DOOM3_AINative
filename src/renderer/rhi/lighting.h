#pragma once

// Classic lighting and shadow plan (spec 004 T082).
//
// The classic path classifies every viewLight_t into the backend-neutral plan below: the
// interaction pass and its ordering (CP-02/CP-04/CP-05), the ambient pass (CP-06), the
// fog/blend passes that are not surface interactions (CP-11/CP-12), and the shadow chains
// with their exclusions (CP-07/CP-08/CP-09).
//
// The rules are the contract's reading of Interaction.cpp:1159-1241, draw_common.cpp:1176-1231
// and draw_arb2.cpp:69-135,243-323. They are a classification, not a claim that the stencil
// path renders; that half is backend work and is recorded separately.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>

namespace doom3 {
namespace rhi {

enum class LightKind { Ambient = 0, Point, Projected, Fog, Blend };

const char* ToString(LightKind value);

enum class ShadowPolicy { None = 0, Global, Local };

const char* ToString(ShadowPolicy value);

enum class ShadowSuppression {
    None = 0,
    MaterialNoShadows,
    LightNoShadows,
    MaterialNoSelfShadow,
};

const char* ToString(ShadowSuppression value);

struct LightInput {
    LightKind kind = LightKind::Ambient;
    bool pointLight = false;
    bool hasProjection = false;
    float falloffRadius = 0.0f;
    bool noShadows = false;
    // A projected light that also has a point falloff needs the multi-frustum split.
    bool multiFrustum = false;
};

// Per-material shadow declarations (Material.h:392,405,468).
struct MaterialShadowInput {
    bool noShadows = false;
    bool noSelfShadow = false;
};

struct LightPlan {
    // A shadow-casting light participates in the interaction pass; ambient, fog and blend
    // are separate passes and never accumulate surface interactions.
    bool interactionPass = true;
    bool multiFrustum = false;
    bool ambientPass = false;
    bool fogOrBlendPass = false;
    ShadowPolicy firstChain = ShadowPolicy::None;
    ShadowPolicy secondChain = ShadowPolicy::None;
    ShadowSuppression suppressed = ShadowSuppression::None;
};

// `sameEntity` is true when the shadow caster is the lit surface's own entity. A suppressed
// light emits no chains at all; a self-shadowing combination keeps only the local chain.
Status PlanLight(const LightInput& light, const MaterialShadowInput& material, bool sameEntity,
                 LightPlan* plan, std::string* error);

}  // namespace rhi
}  // namespace doom3
