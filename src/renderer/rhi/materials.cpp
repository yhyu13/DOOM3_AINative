// Classic material-stage plan (spec 004 T081).
//
// Stage order is the normalised bump -> diffuse -> specular run the classic renderer submits
// per light (Material.cpp:1742-1771; tr_render.cpp:689-842). The plan fixes the order even
// when the material declares its stages in another order, because a reordered run changes the
// accumulated result. Coverage decides the alpha-test draw (CP-03) and the translucent split
// (CP-10); decal and post-process choose the draw class and the sort key (CP-21/CP-25).
//
// Program availability is the merged spec 008 resolver's decision, asked through ProgramGate.
// A stage the gate refuses refuses the whole material: the classic path fails loud rather
// than drawing a stub program or substituting another one.

#include "rhi/materials.h"

namespace doom3 {
namespace rhi {

const char* ToString(StageClass value) {
    switch (value) {
        case StageClass::None: return "None";
        case StageClass::Bump: return "Bump";
        case StageClass::Diffuse: return "Diffuse";
        case StageClass::Specular: return "Specular";
        case StageClass::Ambient: return "Ambient";
        case StageClass::Fog: return "Fog";
        case StageClass::Blend: return "Blend";
        case StageClass::PostProcess: return "PostProcess";
        case StageClass::Skybox: return "Skybox";
        case StageClass::Glasswarp: return "Glasswarp";
        case StageClass::Decal: return "Decal";
    }
    return "Unknown";
}

const char* ToString(CoverageClass value) {
    switch (value) {
        case CoverageClass::Opaque: return "Opaque";
        case CoverageClass::AlphaTested: return "AlphaTested";
        case CoverageClass::Translucent: return "Translucent";
    }
    return "Unknown";
}

const char* ToString(DrawClass value) {
    switch (value) {
        case DrawClass::Opaque: return "Opaque";
        case DrawClass::Decal: return "Decal";
        case DrawClass::Translucent: return "Translucent";
        case DrawClass::PostProcess: return "PostProcess";
    }
    return "Unknown";
}

namespace {

// The manifest key a classic stage program resolves through. Only the five required classic
// programs appear here; a stage with no key is not submitted.
const char* StageKey(StageClass kind) {
    switch (kind) {
        case StageClass::Bump: return "bumpyenvironment";
        case StageClass::Diffuse: return "interaction";
        case StageClass::Specular: return "interaction";
        case StageClass::Ambient: return "environment";
        case StageClass::Skybox: return "environment";
        case StageClass::Glasswarp: return "arbfp_glasswarp";
        case StageClass::PostProcess: return "interaction";
        case StageClass::Decal: return "interaction";
        case StageClass::Fog:
        case StageClass::Blend:
        case StageClass::None:
            return nullptr;
    }
    return nullptr;
}

}  // namespace

Status PlanMaterial(const MaterialInput& input, const ProgramGate& gate, MaterialPlan* plan,
                    std::string* error) {
    if (plan == nullptr) {
        if (error != nullptr) {
            *error = "PlanMaterial needs an output plan";
        }
        return Status::InvalidArgument;
    }
    *plan = MaterialPlan();

    if (input.name == nullptr || input.name[0] == '\0') {
        if (error != nullptr) {
            *error = "material has no name; the classic path cannot report or resolve it";
        }
        return Status::InvalidArgument;
    }

    // Coverage and draw class do not depend on program availability, so a refused material
    // still reports what it would have been.
    if (input.blend) {
        plan->coverage = CoverageClass::Translucent;
        plan->drawClass = DrawClass::Translucent;
    } else if (input.alphaTest) {
        plan->coverage = CoverageClass::AlphaTested;
        plan->drawClass = DrawClass::Opaque;
    } else {
        plan->coverage = CoverageClass::Opaque;
        plan->drawClass = DrawClass::Opaque;
    }
    if (input.decal) {
        plan->drawClass = DrawClass::Decal;
    }
    if (input.postProcess) {
        plan->drawClass = DrawClass::PostProcess;
        plan->sortKey = kPostProcessSort;
    }

    // D-003: the milestone-1 path does not model tiled/sparse feedback. A material that
    // requests it is reported, never drawn with a silent fallback.
    if (input.megaTexture) {
        plan->usesMegaTexture = true;
        plan->refusal = "megatexture is unsupported in milestone 1 (spec 008 D-003)";
        return Status::Ok;
    }

    StageClass canonical[3];
    uint32_t stageCount = 0;
    if (input.postProcess) {
        canonical[stageCount++] = StageClass::PostProcess;
    } else if (input.glasswarp) {
        canonical[stageCount++] = StageClass::Glasswarp;
    } else if (input.skybox) {
        canonical[stageCount++] = StageClass::Skybox;
    } else if (input.decal) {
        canonical[stageCount++] = StageClass::Decal;
    } else {
        // Canonical order regardless of declaration order.
        if (input.hasBumpMap) {
            canonical[stageCount++] = StageClass::Bump;
        }
        if (input.hasDiffuseMap) {
            canonical[stageCount++] = StageClass::Diffuse;
        }
        if (input.hasSpecularMap) {
            canonical[stageCount++] = StageClass::Specular;
        }
    }

    if (stageCount == 0) {
        plan->refusal = "material has no classic stage to submit";
        return Status::Ok;
    }

    for (uint32_t i = 0; i < stageCount; ++i) {
        const char* key = StageKey(canonical[i]);
        if (key == nullptr) {
            plan->refusal = "classic stage has no resolvable program key";
            plan->stageCount = 0;
            return Status::Ok;
        }
        uint32_t programId = 1;
        if (gate.resolve != nullptr) {
            programId = gate.resolve(gate.user, key);
        }
        if (programId == 0) {
            // Fail loud: one refused stage refuses the material, and no other program is
            // substituted for it.
            plan->stageCount = 0;
            plan->refusal = "classic program refused by the resolver gate (not owned)";
            return Status::Ok;
        }
        plan->stages[i].kind = canonical[i];
        plan->stages[i].programId = programId;
    }
    plan->stageCount = stageCount;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

const char* ToString(ChannelState value) {
    switch (value) {
        case ChannelState::Authored: return "Authored";
        case ChannelState::Derived: return "Derived";
        case ChannelState::Unavailable: return "Unavailable";
        case ChannelState::Absent: return "Absent";
    }
    return "Unknown";
}

const char* ToString(MaterialRoute value) {
    switch (value) {
        case MaterialRoute::HybridOpaque: return "HybridOpaque";
        case MaterialRoute::ForwardCompatibility: return "ForwardCompatibility";
    }
    return "Unknown";
}

namespace {

bool IsDeclared(ChannelState state) {
    return state == ChannelState::Authored || state == ChannelState::Derived;
}

// A forward route is not a failure: the material stays drawable through the
// classic/forward path, and the reason is recorded so a reviewer can see which
// channel was missing instead of discovering a silent default.
Status Forward(ModernMaterialPlan* plan, std::string* error, const char* reason) {
    plan->route = MaterialRoute::ForwardCompatibility;
    plan->linearHdrOpaque = false;
    plan->forwardReason = reason;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace

Status PlanModernMaterial(const ModernMaterialInput& input, ModernMaterialPlan* plan,
                          std::string* error) {
    if (plan == nullptr) {
        if (error != nullptr) {
            *error = "PlanModernMaterial needs an output plan";
        }
        return Status::InvalidArgument;
    }
    *plan = ModernMaterialPlan();

    if (input.name == nullptr || input.name[0] == '\0') {
        if (error != nullptr) {
            *error = "modern material has no name; the route cannot be reported";
        }
        return Status::InvalidArgument;
    }

    // T051: special and transparent materials keep the forward/compatibility
    // route. Alpha-tested geometry is an opaque pass and stays eligible.
    if (input.specialStage || input.coverage == CoverageClass::Translucent) {
        return Forward(plan, error,
                       "special or transparent material uses the forward/compatibility route");
    }

    if (!IsDeclared(input.baseColor)) {
        return Forward(plan, error,
                       "no authored base colour; a missing channel is not synthesised");
    }

    if (!IsDeclared(input.roughness)) {
        // T050: a legacy specular map is not roughness. The classic path is
        // still available; the modern path must not reinterpret it.
        if (input.legacySpecularMap) {
            return Forward(plan, error,
                           "legacy specular map is not ground-truth PBR roughness");
        }
        return Forward(plan, error,
                       "no authored roughness; a missing channel is not synthesised");
    }

    if (IsDeclared(input.normal) && !input.normalHandednessDeclared) {
        return Forward(plan, error,
                       "normal map handedness is not declared; convert the RGB/RXGB map first");
    }

    const bool metalnessDeclared = IsDeclared(input.metalness);
    if (!metalnessDeclared && !input.declaredDielectric) {
        return Forward(plan, error,
                       "metalness is neither authored nor declared dielectric; it is not assumed");
    }

    if (input.emissiveHdr && !IsDeclared(input.emissive)) {
        return Forward(plan, error,
                       "HDR emissive declared without an authored emissive channel");
    }

    plan->route = MaterialRoute::HybridOpaque;
    plan->linearHdrOpaque = true;
    plan->srgbBaseColor = input.baseColorSrgb;
    plan->hdrEmissive = input.emissiveHdr && IsDeclared(input.emissive);
    plan->metalnessDeclared = metalnessDeclared || input.declaredDielectric;
    plan->forwardReason = nullptr;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
