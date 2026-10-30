// Linear-HDR opaque inputs for the Q4 hybrid path (spec 004 T051).
//
// The stage boundary is deliberate: this translation unit has no device, no
// pipeline and no native call. It prepares and validates the values a hybrid
// opaque pass would bind, so the same code runs in the CPU fixtures and can be
// lowered by a backend without re-deciding the material route.
//
// Decoding rules:
//  * base colour is decoded from sRGB once, here; a material that declared a
//    linear base colour is not decoded a second time;
//  * emissive is either sRGB (decoded) or explicitly HDR (linear, unclamped);
//  * roughness, metalness and ambient occlusion are linear scalars in [0,1];
//  * a forward-routed material produces no opaque values at all.

#include "rhi/hybrid_opaque.h"

#include <cmath>

namespace doom3 {
namespace rhi {

namespace {

bool InUnitRange(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }

bool EncodedInRange(const LinearRgb& value) {
    return IsFinite(value) && value.r >= 0.0f && value.r <= 1.0f && value.g >= 0.0f &&
           value.g <= 1.0f && value.b >= 0.0f && value.b <= 1.0f;
}

void ClearError(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
}

Status Fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return Status::InvalidArgument;
}

}  // namespace

Status BuildHybridOpaqueInput(const HybridOpaqueDesc& desc, HybridOpaqueInput* input,
                              std::string* error) {
    if (input == nullptr) {
        return Fail(error, "BuildHybridOpaqueInput needs an output");
    }
    *input = HybridOpaqueInput();

    input->status = PlanModernMaterial(desc.material, &input->plan, error);
    if (input->status != Status::Ok) {
        input->error = error != nullptr ? *error : std::string();
        return input->status;
    }

    if (input->plan.route != MaterialRoute::HybridOpaque) {
        // The forward/compatibility route is a valid outcome. Do not fabricate
        // opaque inputs for it; the reason is already on the plan.
        input->linearHdrOpaque = false;
        ClearError(error);
        return Status::Ok;
    }

    if (desc.material.baseColorSrgb) {
        if (!EncodedInRange(desc.baseColor)) {
            return Fail(error,
                        "sRGB base colour is outside [0,1]; authored colour is not clamped");
        }
        input->material.baseColor = SrgbToLinear(desc.baseColor);
        input->srgbDecoded = true;
    } else {
        if (!IsNonNegativeFinite(desc.baseColor)) {
            return Fail(error, "linear base colour must be finite and non-negative");
        }
        input->material.baseColor = desc.baseColor;
    }

    if (!InUnitRange(desc.roughness) || !InUnitRange(desc.metalness) ||
        !InUnitRange(desc.ao) || !InUnitRange(desc.alpha)) {
        return Fail(error,
                    "roughness, metalness, ambient occlusion and alpha must be finite in [0,1]");
    }
    if (!(desc.alphaCutoff >= 0.0f && desc.alphaCutoff <= 1.0f)) {
        return Fail(error, "alpha cutoff must be in [0,1]");
    }

    if (desc.material.emissiveHdr) {
        if (!IsNonNegativeFinite(desc.emissive)) {
            return Fail(error, "HDR emissive must be finite and non-negative");
        }
        input->material.emissive = desc.emissive;
        input->hdrEmissivePreserved = true;
    } else {
        if (!EncodedInRange(desc.emissive)) {
            return Fail(error,
                        "emissive exceeds the sRGB range but the material is not declared HDR");
        }
        input->material.emissive = SrgbToLinear(desc.emissive);
    }

    input->material.roughness = desc.roughness;
    input->material.metalness = desc.metalness;
    input->material.ao = desc.ao;
    input->material.alphaCutoff = desc.alphaCutoff;
    input->material.alphaTest = desc.alphaTest;

    input->linearHdrOpaque = true;
    input->alphaTestActive = desc.alphaTest;
    // An alpha-tested sample below the cutoff is not shaded. It is not an
    // invalid material, so this stays Ok with the discard flagged.
    input->alphaDiscarded = desc.alphaTest && desc.alpha < desc.alphaCutoff;

    ClearError(error);
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
