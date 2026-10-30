#pragma once

// Linear-HDR opaque inputs for the Q4 hybrid path (spec 004 T051).
//
// This unit turns one material's declared channels into the values an opaque
// lighting pass consumes: base colour and emissive in linear light, a
// [0,1] roughness/metalness pair, ambient occlusion and the alpha-test result.
// It is backend-neutral; the backend translation unit that lowers these values to\n// a pipeline is separate, and this slice does not claim a drawn frame.
//
// Two rules matter more than the arithmetic:
//  * the material route is decided by `materials.h`; this unit never promotes a
//    forward/compatibility material into the opaque path;
//  * an HDR emissive may exceed 1.0 and is preserved, while an emissive above
//    1.0 on a material that did not declare HDR is refused rather than silently
//    clamped.
//
// No native type, handle or synchronization value appears here.

#include "rhi/linear_color.h"
#include "rhi/materials.h"
#include "rhi/rhi.h"

#include <string>

namespace doom3 {
namespace rhi {

struct HybridOpaqueDesc {
    // The declared channel states and coverage class. The route is derived from
    // this input, never supplied by the caller.
    ModernMaterialInput material;

    // Channel values as authored. When `material.baseColorSrgb` is true these
    // are sRGB-encoded in [0,1]; otherwise they are already linear. Emissive is
    // authored in sRGB unless `material.emissiveHdr` is set, in which case the
    // values are linear HDR and are not decoded again.
    LinearRgb baseColor{0.5f, 0.5f, 0.5f};
    LinearRgb emissive{0.0f, 0.0f, 0.0f};
    float roughness = 0.5f;
    float metalness = 0.0f;
    float ao = 1.0f;

    bool alphaTest = false;
    float alpha = 1.0f;
    float alphaCutoff = 0.5f;
};

struct HybridOpaqueMaterial {
    // Linear light. A base colour is always [0,1] in linear light; emissive is
    // HDR and unclamped.
    LinearRgb baseColor;
    LinearRgb emissive;
    float roughness = 0.5f;
    float metalness = 0.0f;
    float ao = 1.0f;
    float alphaCutoff = 0.5f;
    bool alphaTest = false;
};

struct HybridOpaqueInput {
    Status status = Status::Ok;
    std::string error;
    // Mirrors the classification. `ForwardCompatibility` means "do not consume
    // this struct as opaque shading input"; the caller sends the material down
    // the forward path with `forwardReason`.
    ModernMaterialPlan plan;
    bool linearHdrOpaque = false;
    bool srgbDecoded = false;
    bool hdrEmissivePreserved = false;
    bool alphaTestActive = false;
    // True when the alpha test rejected this sample. The material is valid; the
    // sample is not shaded.
    bool alphaDiscarded = false;
    HybridOpaqueMaterial material;
};

// Validates the declared channels and produces linear-HDR opaque inputs.
//
// Status::InvalidArgument covers malformed values (non-finite, negative,
// out-of-range roughness/metalness/ao/alpha, an encoded base colour outside
// [0,1], or an HDR emissive without the HDR declaration). A forward-routed
// material is Status::Ok with `linearHdrOpaque == false`; it is not an error.
Status BuildHybridOpaqueInput(const HybridOpaqueDesc& desc, HybridOpaqueInput* input,
                              std::string* error);

}  // namespace rhi
}  // namespace doom3
