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

#include "rhi/linear_color.h"
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

// ---------------------------------------------------------------------------
// Q4 direct light and shadow inputs (T052)
// ---------------------------------------------------------------------------
//
// The classic plan above decides which passes and chains exist. This section is
// the numerical direct-light term the Q4 opaque pass consumes, so the fixture
// can check attenuation, the N.L clamp, the shadow bias and the unsupported
// legacy stages without a device or a shader.
//
// It is a bounded preview term, not a physically based BRDF claim. The specular
// lobe below is deliberately simple and documented; the path-traced reference
// (Q6) is where a full BRDF is judged.

struct LightVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Which surface class this sample belongs to. Only opaque and alpha-tested
// surfaces are direct-light receivers in the Q4 opaque pass. Every other class
// is a forward/compatibility material, and asking for its direct opaque term is
// Unsupported rather than a fabricated result.
enum class SurfaceStage {
    Opaque = 0,
    AlphaTested,
    Translucent,
    Blend,
    Fog,
    Glasswarp,
    Skybox,
    Decal,
    PostProcess,
};

const char* ToString(SurfaceStage value);

struct DirectLightInput {
    // Linear radiance. HDR values are meaningful and are not clamped.
    LinearRgb radiance{1.0f, 1.0f, 1.0f};
    LightVec3 position;
    // Travel direction of the light. For a spot light this is the cone axis;
    // for a directional light (range 0) it is the direction the light travels.
    LightVec3 direction{0.0f, 0.0f, -1.0f};
    // 0 means directional. A positive range bounds a point/spot light with the
    // documented window below; a sample beyond the range is unlit.
    float range = 0.0f;
    // Spot cone in cosine space. `cosInner == cosOuter == 0` means "not a spot".
    float cosInner = 0.0f;
    float cosOuter = 0.0f;
    bool castsShadow = true;
};

struct DirectSurfaceInput {
    SurfaceStage stage = SurfaceStage::Opaque;
    LightVec3 position;
    LightVec3 normal{0.0f, 0.0f, 1.0f};
    LightVec3 view{0.0f, 0.0f, 1.0f};
    // Linear base colour and the two scalar channels.
    LinearRgb baseColor{0.5f, 0.5f, 0.5f};
    float roughness = 0.5f;
    float metalness = 0.0f;
    // Alpha-test coverage at this sample. A failed test discards the sample.
    bool alphaTestPass = true;
};

struct ShadowInput {
    // Light-space depths. A smaller depth is closer to the light.
    float lightDepth = 0.0f;
    float surfaceDepth = 0.0f;
    float constantBias = 0.002f;
    // Added bias proportional to (1 - N.L), which is largest at grazing angles.
    float slopeBias = 0.01f;
    // Number of comparison taps and how many of them occlude. `taps` must be
    // at least 1 and `blockers` must not exceed it.
    uint32_t taps = 1;
    uint32_t blockers = 0;
};

struct DirectLightResult {
    Status status = Status::Ok;
    std::string error;
    bool shaded = false;
    // Alpha test rejected the sample; no radiance is returned.
    bool discarded = false;
    float attenuation = 0.0f;
    float ndotl = 0.0f;
    // 1 = fully lit, 0 = fully occluded.
    float shadowFactor = 1.0f;
    LinearRgb diffuse;
    LinearRgb specular;
    // Diffuse + specular, linear HDR, unclamped.
    LinearRgb radiance;
};

// Documented point/spot window: `x = clamp(distance / range, 0, 1)`,
// `window = (1 - x^4)^2`, and `attenuation = window / max(distance^2, epsilon)`.
// At distance 0 the attenuation is 1. A non-positive range returns 1 (the
// directional case) and an out-of-range distance returns 0.
float ComputeDistanceAttenuation(float distance, float range);

// Evaluates one direct light against one opaque/alpha-tested sample.
//
// Unsupported covers a forward/compatibility stage (translucent, blend, fog,
// glasswarp, skybox, decal, post-process), which has no direct opaque term in
// this pass. InvalidArgument covers a zero normal, a non-positive range with a
// spot cone, out-of-range cone cosines, non-finite or negative radiance, and a
// tap/blocker mismatch.
Status EvaluateDirectLight(const DirectLightInput& light, const DirectSurfaceInput& surface,
                           const ShadowInput& shadow, DirectLightResult* result,
                           std::string* error);

}  // namespace rhi
}  // namespace doom3
