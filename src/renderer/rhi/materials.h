#pragma once

// Classic material-stage plan (spec 004 T081).
//
// The front end still parses `materialDef_t` stage keywords. This header decides the
// backend-neutral plan the classic path submits: the canonical bump -> diffuse -> specular
// order (Material.cpp:1742-1771; tr_render.cpp:689-842), the coverage class that drives the
// alpha-test draw (CP-03), the translucent split (CP-10), and the sort class for decals and
// post-process (CP-21/CP-25).
//
// Program availability is not decided here. The merged spec 008 resolver is the only owner
// of "may this stage be drawn"; this plan asks it through a gate and refuses the material
// when the gate returns no program. A stub or absent program is never replaced by another
// program and is never drawn.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>

namespace doom3 {
namespace rhi {

enum class StageClass {
    None = 0,
    Bump,
    Diffuse,
    Specular,
    Ambient,
    Fog,
    Blend,
    PostProcess,
    Skybox,
    Glasswarp,
    Decal,
};

const char* ToString(StageClass value);

enum class CoverageClass { Opaque = 0, AlphaTested, Translucent };
enum class DrawClass { Opaque = 0, Decal, Translucent, PostProcess };

const char* ToString(CoverageClass value);
const char* ToString(DrawClass value);

// Sort key for the post-process stage that samples the current frame (CP-21).
constexpr uint32_t kPostProcessSort = 100;

// The spec 008 draw gate. `resolve` returns the backend program id for one stage key, or 0
// when the stage must not be drawn; the plan never substitutes a program for a refused one.
//
// `resolve == nullptr` means "program availability is out of scope for this plan". It exists
// only so a classification fixture can exercise the ordering and coverage rules without a
// resolver; a backend must always supply a resolver-backed gate.
struct ProgramGate {
    void* user = nullptr;
    uint32_t (*resolve)(void* user, const char* stageKey) = nullptr;
};

// The material facts the front end already knows after parsing. No engine type appears here.
struct MaterialInput {
    const char* name = nullptr;
    bool hasBumpMap = false;
    bool hasDiffuseMap = false;
    bool hasSpecularMap = false;
    bool alphaTest = false;
    bool blend = false;
    bool glasswarp = false;
    bool skybox = false;
    bool postProcess = false;
    bool decal = false;
    bool megaTexture = false;
};

struct MaterialStage {
    StageClass kind = StageClass::None;
    uint32_t programId = 0;
};

struct MaterialPlan {
    MaterialStage stages[3];
    uint32_t stageCount = 0;
    uint32_t sortKey = 0;
    CoverageClass coverage = CoverageClass::Opaque;
    DrawClass drawClass = DrawClass::Opaque;
    bool usesMegaTexture = false;
    // Parallel to `stages`: the stage's classic map must be uploaded before the
    // stage can be drawn. A declared map with no texture is refused by
    // BindMaterialTextures rather than sampled as an implicit white texel.
    bool needsTexture[3] = {false, false, false};
    // Non-null means "must not be drawn"; the string is a stable literal naming the reason.
    const char* refusal = nullptr;
};

// Builds the plan. Returns InvalidArgument only for a malformed input; a refused material
// is Ok with a non-null `refusal` and stageCount 0, so the caller can report it per material.
Status PlanMaterial(const MaterialInput& input, const ProgramGate& gate, MaterialPlan* plan,
                    std::string* error);

// ---------------------------------------------------------------------------
// Classic stage texture binding (T081)
// ---------------------------------------------------------------------------
//
// The classic interaction program samples one map per stage: bump, then
// diffuse, then specular. Uploading a texture is the RHI's job; this decides
// which uploaded texture a stage binds and refuses a declared map that was not
// uploaded. No implicit white/black substitute is produced.

struct MaterialStageTextures {
    Texture bump;
    Texture diffuse;
    Texture specular;
};

struct BoundMaterialStage {
    StageClass kind = StageClass::None;
    uint32_t programId = 0;
    // Zero means the stage runs untextured (the caller's program owns that).
    Texture texture;
};

struct BoundMaterialPlan {
    BoundMaterialStage stages[3];
    uint32_t stageCount = 0;
    CoverageClass coverage = CoverageClass::Opaque;
    DrawClass drawClass = DrawClass::Opaque;
    // Non-null exactly when the material plan refused the draw.
    const char* refusal = nullptr;
};

// A refused plan binds nothing and keeps its refusal. A plan whose stage
// declares a map but receives no texture is InvalidArgument: a declared map is
// never sampled through a substitute.
Status BindMaterialTextures(const MaterialPlan& plan,
                            const MaterialStageTextures& textures,
                            BoundMaterialPlan* bound, std::string* error);

// ---------------------------------------------------------------------------
// Q2 engine-seam stage selection (T081)
// ---------------------------------------------------------------------------
//
// The engine fork sends up to eight material stages, each with an optional
// image identity, in the material's sorted stage order (bump, then diffuse,
// then specular within a bump-delimited group; Material.cpp:1742-1771). The
// portable DrawItem samples one texture, so the consumer cannot bind the whole
// canonical plan yet. This selects the stage the classic interaction's colour
// comes from and records which stage index was chosen.
//
// The rule is deliberately narrow and testable:
//   * every declared stage image must have an uploaded texture, otherwise the
//     draw is InvalidArgument and no implicit substitute is sampled;
//   * with two or more stages the diffuse slot is stage 1 (the canonical
//     bump -> diffuse -> specular order); with one stage it is stage 0;
//   * if the preferred slot declares no image, the first declared stage is
//     used instead.
//
// This is a bounded one-texture binding, not a claim that multi-stage
// bump/diffuse/specular evaluation is complete. `MaterialStageImage` is
// parallel to the engine's stage facts already converted by the registration
// unit; nothing engine-specific crosses this header.

struct MaterialStageImage {
    bool hasImage = false;
    Texture texture;
};

Status SelectStageTextureForDraw(const MaterialStageImage* stages,
                                 uint32_t stageCount, Texture* texture,
                                 uint32_t* stageIndex, std::string* error);

// ---------------------------------------------------------------------------
// Q4 modern material contract (T050/T051)
// ---------------------------------------------------------------------------
//
// A hybrid deferred/forward renderer needs declared linear-HDR inputs. The
// classic stage keywords do not describe them: a legacy specular map is not a
// roughness map, and an RGB (RXGB) normal map is not a linear tangent-space
// normal. This section classifies what a material actually declared so the
// opaque path can refuse an unspecified channel instead of synthesising one.
//
// The states mirror spec 009 C-05 (authored / derived / unavailable). `Absent`
// is the classic material that never had the channel at all.

enum class ChannelState {
    Authored = 0,
    Derived,
    Unavailable,
    Absent,
};

const char* ToString(ChannelState value);

enum class MaterialRoute {
    // Linear-HDR opaque inputs are declared and the material may enter the
    // hybrid opaque pass.
    HybridOpaque = 0,
    // The classic forward/compatibility route stays available for special and
    // transparent materials, and for materials whose PBR channels are not
    // declared. This is a route, not a refusal of the material.
    ForwardCompatibility,
};

const char* ToString(MaterialRoute value);

struct ModernMaterialInput {
    const char* name = nullptr;
    ChannelState baseColor = ChannelState::Absent;
    ChannelState normal = ChannelState::Absent;
    ChannelState roughness = ChannelState::Absent;
    ChannelState metalness = ChannelState::Absent;
    ChannelState ao = ChannelState::Absent;
    ChannelState emissive = ChannelState::Absent;

    // Base colour was authored in the sRGB encoding and must be decoded.
    bool baseColorSrgb = true;
    // The authored emissive channel is HDR (values above 1.0 are meaningful).
    bool emissiveHdr = false;
    // The normal map's handedness and Z reconstruction were declared (spec 009
    // FR-008). An RGB/RXGB normal without that declaration is not consumed.
    bool normalHandednessDeclared = false;
    // The classic material carries a specular map. It is evidence of legacy
    // shading, never ground-truth roughness or metalness.
    bool legacySpecularMap = false;
    // The material declares itself dielectric, so metalness 0 is a declared
    // fact rather than a synthesised default.
    bool declaredDielectric = false;
    // Glasswarp, skybox, decal, post-process and similar special stages.
    bool specialStage = false;
    CoverageClass coverage = CoverageClass::Opaque;
};

struct ModernMaterialPlan {
    MaterialRoute route = MaterialRoute::ForwardCompatibility;
    bool linearHdrOpaque = false;
    bool srgbBaseColor = false;
    bool hdrEmissive = false;
    bool metalnessDeclared = false;
    // Stable literal naming why the material stays on the forward route. Null
    // exactly when `route == HybridOpaque`.
    const char* forwardReason = nullptr;
};

// Classifies one material. A forward route is Status::Ok with a non-null
// `forwardReason`; a malformed input is InvalidArgument. No channel is invented:
// a missing roughness, an undeclared normal handedness or a legacy-only
// specular map routes the material to the forward path with the reason stated.
Status PlanModernMaterial(const ModernMaterialInput& input, ModernMaterialPlan* plan,
                          std::string* error);

}  // namespace rhi
}  // namespace doom3
