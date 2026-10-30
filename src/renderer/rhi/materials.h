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
    // Non-null means "must not be drawn"; the string is a stable literal naming the reason.
    const char* refusal = nullptr;
};

// Builds the plan. Returns InvalidArgument only for a malformed input; a refused material
// is Ok with a non-null `refusal` and stageCount 0, so the caller can report it per material.
Status PlanMaterial(const MaterialInput& input, const ProgramGate& gate, MaterialPlan* plan,
                    std::string* error);

}  // namespace rhi
}  // namespace doom3
