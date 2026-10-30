// T050 material conversion and legacy-special-material fallback fixtures.
//
// The expected values are independent of the implementation: the sRGB literals
// come from the documented piecewise transfer function, and the channel order
// and route expectations come from spec 009 C-05/C-06 and spec 004 FR-012. No
// expectation is derived from `PlanModernMaterial` output.
//
// The central rule this fixture pins: legacy specular content is NOT
// ground-truth PBR, and an RGB/RXGB normal map is not consumed before its
// handedness and Z reconstruction are declared.

#include "q4_test_support.h"
#include "rhi/hybrid_opaque.h"
#include "rhi/linear_color.h"
#include "rhi/materials.h"

#include <cmath>
#include <string>

namespace {

namespace rhi = doom3::rhi;

rhi::ModernMaterialInput DeclaredDielectric() {
    rhi::ModernMaterialInput material;
    material.name = "hero_dielectric";
    material.baseColor = rhi::ChannelState::Authored;
    material.normal = rhi::ChannelState::Authored;
    material.roughness = rhi::ChannelState::Authored;
    material.metalness = rhi::ChannelState::Unavailable;
    material.ao = rhi::ChannelState::Authored;
    material.emissive = rhi::ChannelState::Unavailable;
    material.baseColorSrgb = true;
    material.normalHandednessDeclared = true;
    material.declaredDielectric = true;
    return material;
}

void RunConversionTable(q4_test::Runner* runner) {
    // Literal values of the sRGB piecewise curve, computed independently.
    runner->Near(rhi::SrgbChannelToLinear(0.0f), 0.0f, 1.0e-7f, "T050 sRGB 0.0 -> 0.0");
    runner->Near(rhi::SrgbChannelToLinear(0.04045f), 0.0031308f, 1.0e-7f,
                "T050 sRGB 0.04045 -> linear segment end");
    runner->Near(rhi::SrgbChannelToLinear(0.25f), 0.05087609f, 1.0e-6f,
                "T050 sRGB 0.25 -> 0.050876");
    runner->Near(rhi::SrgbChannelToLinear(0.5f), 0.21404114f, 1.0e-6f,
                "T050 sRGB 0.5 -> 0.214041");
    runner->Near(rhi::SrgbChannelToLinear(0.75f), 0.52252155f, 1.0e-6f,
                "T050 sRGB 0.75 -> 0.522522");
    runner->Near(rhi::SrgbChannelToLinear(1.0f), 1.0f, 1.0e-7f, "T050 sRGB 1.0 -> 1.0");

    // Channel order is preserved: no red/blue or axis swap.
    const rhi::LinearRgb decoded =
        rhi::SrgbToLinear(rhi::LinearRgb{0.5f, 0.25f, 0.75f});
    runner->Near(decoded.r, 0.21404114f, 1.0e-6f, "T050 decoded red channel");
    runner->Near(decoded.g, 0.05087609f, 1.0e-6f, "T050 decoded green channel");
    runner->Near(decoded.b, 0.52252155f, 1.0e-6f, "T050 decoded blue channel");
}

void RunRouteFixtures(q4_test::Runner* runner) {
    rhi::ModernMaterialPlan plan;
    std::string error;

    // Happy path: the dielectric hero declares its missing metalness as a
    // dielectric fact, so nothing is synthesised.
    runner->Check(rhi::PlanModernMaterial(DeclaredDielectric(), &plan, &error) ==
                     rhi::Status::Ok,
                 "T050 dielectric plan is Ok", error);
    runner->Check(plan.route == rhi::MaterialRoute::HybridOpaque,
                 "T050 dielectric enters the hybrid opaque route");
    runner->Check(plan.linearHdrOpaque && plan.srgbBaseColor,
                 "T050 dielectric declares linear-HDR opaque and sRGB base colour");
    runner->Check(plan.forwardReason == nullptr,
                 "T050 hybrid material has no forward reason");

    // Legacy specular-only content: forward, with the reason naming the rule.
    rhi::ModernMaterialInput legacy = DeclaredDielectric();
    legacy.name = "legacy_specular";
    legacy.roughness = rhi::ChannelState::Absent;
    legacy.legacySpecularMap = true;
    runner->Check(rhi::PlanModernMaterial(legacy, &plan, &error) == rhi::Status::Ok,
                 "T050 legacy specular plan is Ok", error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 legacy specular routes forward");
    runner->Check(!plan.linearHdrOpaque, "T050 legacy specular is not opaque PBR input");
    runner->Check(plan.forwardReason != nullptr &&
                     std::string(plan.forwardReason).find("specular") != std::string::npos,
                 "T050 legacy specular reason names specular",
                 plan.forwardReason != nullptr ? plan.forwardReason : "null");

    // A missing roughness without a legacy specular map is still not invented.
    rhi::ModernMaterialInput missingRoughness = DeclaredDielectric();
    missingRoughness.roughness = rhi::ChannelState::Unavailable;
    rhi::PlanModernMaterial(missingRoughness, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 missing roughness routes forward");

    // An authored normal without a handedness declaration is not consumed.
    rhi::ModernMaterialInput undeclaredNormal = DeclaredDielectric();
    undeclaredNormal.normalHandednessDeclared = false;
    rhi::PlanModernMaterial(undeclaredNormal, &plan, &error);
    runner->Check(plan.forwardReason != nullptr &&
                     std::string(plan.forwardReason).find("handedness") != std::string::npos,
                 "T050 undeclared normal handedness routes forward",
                 plan.forwardReason != nullptr ? plan.forwardReason : "null");

    // Metalness is not assumed.
    rhi::ModernMaterialInput metal = DeclaredDielectric();
    metal.declaredDielectric = false;
    metal.metalness = rhi::ChannelState::Unavailable;
    rhi::PlanModernMaterial(metal, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 undeclared metalness routes forward");

    // Special and transparent materials keep the compatibility route.
    rhi::ModernMaterialInput translucent = DeclaredDielectric();
    translucent.coverage = rhi::CoverageClass::Translucent;
    rhi::PlanModernMaterial(translucent, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 translucent material keeps the forward route");
    runner->Check(plan.forwardReason != nullptr &&
                     std::string(plan.forwardReason).find("transparent") != std::string::npos,
                 "T050 translucent reason names transparency");

    rhi::ModernMaterialInput glass = DeclaredDielectric();
    glass.specialStage = true;
    rhi::PlanModernMaterial(glass, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 special stage keeps the forward route");

    // A missing base colour is not invented.
    rhi::ModernMaterialInput noBase = DeclaredDielectric();
    noBase.baseColor = rhi::ChannelState::Absent;
    rhi::PlanModernMaterial(noBase, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 missing base colour routes forward");

    // HDR emissive without a channel is inconsistent.
    rhi::ModernMaterialInput hdrWithoutChannel = DeclaredDielectric();
    hdrWithoutChannel.emissive = rhi::ChannelState::Unavailable;
    hdrWithoutChannel.emissiveHdr = true;
    rhi::PlanModernMaterial(hdrWithoutChannel, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::ForwardCompatibility,
                 "T050 HDR emissive declaration without a channel routes forward");

    // Alpha-tested geometry remains an opaque pass and keeps its coverage.
    rhi::ModernMaterialInput alpha = DeclaredDielectric();
    alpha.coverage = rhi::CoverageClass::AlphaTested;
    rhi::PlanModernMaterial(alpha, &plan, &error);
    runner->Check(plan.route == rhi::MaterialRoute::HybridOpaque,
                 "T050 alpha-tested material stays on the opaque route");

    rhi::ModernMaterialInput unnamed;
    runner->Check(rhi::PlanModernMaterial(unnamed, &plan, &error) ==
                     rhi::Status::InvalidArgument,
                 "T050 unnamed material is InvalidArgument");
}

void RunOpaqueInputs(q4_test::Runner* runner) {
    rhi::HybridOpaqueDesc desc;
    desc.material = DeclaredDielectric();
    desc.baseColor = rhi::LinearRgb{0.5f, 0.25f, 0.75f};
    desc.roughness = 0.4f;
    desc.metalness = 0.0f;
    desc.ao = 0.8f;
    desc.emissive = rhi::LinearRgb{4.0f, 2.0f, 1.0f};
    desc.material.emissive = rhi::ChannelState::Authored;
    desc.material.emissiveHdr = true;
    desc.alphaTest = true;
    desc.alpha = 0.4f;
    desc.alphaCutoff = 0.5f;

    rhi::HybridOpaqueInput input;
    std::string error;
    runner->Check(rhi::BuildHybridOpaqueInput(desc, &input, &error) == rhi::Status::Ok,
                 "T050 opaque input builds", error);
    runner->Check(input.linearHdrOpaque, "T050 opaque input is linear-HDR");
    runner->Check(input.srgbDecoded, "T050 base colour is decoded once");
    runner->Check(input.hdrEmissivePreserved, "T050 HDR emissive is preserved");
    runner->Near(input.material.baseColor.r, 0.21404114f, 1.0e-6f,
                "T050 opaque base colour red is linear");
    runner->Near(input.material.emissive.r, 4.0f, 1.0e-7f,
                "T050 opaque emissive is not clamped");
    runner->Check(input.alphaTestActive && input.alphaDiscarded,
                 "T050 alpha below cutoff is discarded, not an invalid material");

    desc.alpha = 0.7f;
    runner->Check(rhi::BuildHybridOpaqueInput(desc, &input, &error) == rhi::Status::Ok &&
                     !input.alphaDiscarded,
                 "T050 alpha above cutoff is shaded", error);

    // Out-of-range input is refused rather than clamped.
    rhi::HybridOpaqueDesc bad = desc;
    bad.baseColor = rhi::LinearRgb{1.5f, 0.0f, 0.0f};
    runner->Check(rhi::BuildHybridOpaqueInput(bad, &input, &error) ==
                     rhi::Status::InvalidArgument,
                 "T050 encoded base colour above 1 is refused", error);

    rhi::HybridOpaqueDesc unclampedEmissive = desc;
    unclampedEmissive.material.emissiveHdr = false;
    unclampedEmissive.emissive = rhi::LinearRgb{2.0f, 0.0f, 0.0f};
    runner->Check(rhi::BuildHybridOpaqueInput(unclampedEmissive, &input, &error) ==
                     rhi::Status::InvalidArgument,
                 "T050 emissive above 1 without HDR declaration is refused", error);

    rhi::HybridOpaqueDesc badRoughness = desc;
    badRoughness.roughness = 1.5f;
    runner->Check(rhi::BuildHybridOpaqueInput(badRoughness, &input, &error) ==
                     rhi::Status::InvalidArgument,
                 "T050 roughness outside [0,1] is refused", error);

    // A forward material produces no opaque values, even with valid numbers.
    rhi::HybridOpaqueDesc forward = desc;
    forward.material.legacySpecularMap = true;
    forward.material.roughness = rhi::ChannelState::Absent;
    runner->Check(rhi::BuildHybridOpaqueInput(forward, &input, &error) == rhi::Status::Ok,
                 "T050 forward material builds without error", error);
    runner->Check(!input.linearHdrOpaque && input.plan.forwardReason != nullptr,
                 "T050 forward material supplies no opaque input");
    runner->Near(input.material.baseColor.r, 0.0f, 1.0e-7f,
                "T050 forward material leaves opaque values untouched");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    RunConversionTable(&runner);
    RunRouteFixtures(&runner);
    RunOpaqueInputs(&runner);
    return runner.Finish("material_contract");
}
