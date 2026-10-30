// T052 direct-light and shadow fixtures.
//
// Expected values follow the formulas documented in `rhi/lighting.h`: the
// windowed inverse-square attenuation, `N.L`, and the bias/PCF shadow rule.
// Unsupported legacy stages are asserted as Unsupported with a reason naming
// the stage; they are not silently shaded with zero radiance.

#include "q4_test_support.h"
#include "rhi/lighting.h"

#include <cmath>
#include <string>

namespace {

namespace rhi = doom3::rhi;

constexpr float kPi = 3.14159265358979323846f;

rhi::DirectSurfaceInput Surface() {
    rhi::DirectSurfaceInput surface;
    surface.stage = rhi::SurfaceStage::Opaque;
    surface.position = rhi::LightVec3{0.0f, 0.0f, 0.0f};
    surface.normal = rhi::LightVec3{0.0f, 0.0f, 1.0f};
    surface.view = rhi::LightVec3{0.0f, 0.0f, 1.0f};
    surface.baseColor = rhi::LinearRgb{0.5f, 0.5f, 0.5f};
    surface.roughness = 1.0f;
    surface.metalness = 0.0f;
    surface.alphaTestPass = true;
    return surface;
}

rhi::DirectLightInput Directional(rhi::LightVec3 travel) {
    rhi::DirectLightInput light;
    light.radiance = rhi::LinearRgb{1.0f, 1.0f, 1.0f};
    light.direction = travel;
    light.range = 0.0f;
    light.castsShadow = true;
    return light;
}

void RunAttenuation(q4_test::Runner* runner) {
    // Documented window: (1 - (d/r)^4)^2 / d^2.
    runner->Near(rhi::ComputeDistanceAttenuation(0.0f, 6.0f), 1.0f, 1.0e-7f,
                "T052 attenuation at zero distance is 1");
    runner->Near(rhi::ComputeDistanceAttenuation(3.0f, 6.0f), 0.09765625f, 1.0e-7f,
                "T052 attenuation at half range");
    runner->Near(rhi::ComputeDistanceAttenuation(6.0f, 6.0f), 0.0f, 1.0e-7f,
                "T052 attenuation at the range edge is zero");
    runner->Near(rhi::ComputeDistanceAttenuation(9.0f, 6.0f), 0.0f, 1.0e-7f,
                "T052 attenuation beyond the range is zero");
    runner->Near(rhi::ComputeDistanceAttenuation(9.0f, 0.0f), 1.0f, 1.0e-7f,
                "T052 directional attenuation is 1");
    runner->Check(std::isinf(rhi::ComputeDistanceAttenuation(-1.0f, 6.0f)) == false,
                 "T052 negative distance is not accepted as a value");
}

void RunDirectLight(q4_test::Runner* runner) {
    rhi::DirectLightResult result;
    std::string error;

    // Facing light: N.L = 1, dielectric albedo 0.5, roughness 1 -> no gloss.
    runner->Check(rhi::EvaluateDirectLight(Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f}),
                                          Surface(), rhi::ShadowInput{}, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 facing directional light evaluates", error);
    runner->Near(result.ndotl, 1.0f, 1.0e-6f, "T052 facing N.L");
    runner->Near(result.attenuation, 1.0f, 1.0e-6f, "T052 facing attenuation");
    runner->Near(result.diffuse.r, 0.5f, 1.0e-6f, "T052 facing diffuse radiance");
    runner->Near(result.specular.r, 0.0f, 1.0e-6f, "T052 fully rough specular is zero");

    // 60 degrees: N.L = 0.5.
    const float sin60 = std::sin(kPi / 3.0f);
    const float cos60 = std::cos(kPi / 3.0f);
    runner->Check(rhi::EvaluateDirectLight(
                     Directional(rhi::LightVec3{-sin60, 0.0f, -cos60}), Surface(),
                     rhi::ShadowInput{}, &result, &error) == rhi::Status::Ok,
                 "T052 60-degree light evaluates", error);
    runner->Near(result.ndotl, 0.5f, 1.0e-6f, "T052 60-degree N.L");
    runner->Near(result.diffuse.r, 0.25f, 1.0e-6f, "T052 60-degree diffuse");

    // Back-facing: zero, but still a valid evaluation.
    runner->Check(rhi::EvaluateDirectLight(
                     Directional(rhi::LightVec3{0.0f, 0.0f, 1.0f}), Surface(),
                     rhi::ShadowInput{}, &result, &error) == rhi::Status::Ok,
                 "T052 back-facing light is a valid zero", error);
    runner->Check(!result.shaded && result.ndotl == 0.0f,
                 "T052 back-facing sample is unlit");

    // Point light at half range.
    rhi::DirectLightInput point = Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f});
    point.position = rhi::LightVec3{0.0f, 0.0f, 3.0f};
    point.range = 6.0f;
    runner->Check(rhi::EvaluateDirectLight(point, Surface(), rhi::ShadowInput{}, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 point light evaluates", error);
    runner->Near(result.attenuation, 0.09765625f, 1.0e-6f,
                "T052 point light attenuation at half range");
    runner->Near(result.diffuse.r, 0.5f * 0.09765625f, 1.0e-6f,
                "T052 point light diffuse radiance");

    // A metal keeps its specular tint and loses the diffuse lobe.
    rhi::DirectSurfaceInput metal = Surface();
    metal.baseColor = rhi::LinearRgb{1.0f, 1.0f, 1.0f};
    metal.metalness = 1.0f;
    metal.roughness = 0.0f;
    runner->Check(rhi::EvaluateDirectLight(Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f}), metal,
                                          rhi::ShadowInput{}, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 metal evaluates", error);
    runner->Near(result.diffuse.r, 0.0f, 1.0e-7f, "T052 metal has no diffuse term");
    runner->Near(result.specular.r, 1.0f, 1.0e-6f, "T052 mirror metal preserves radiance");
}

void RunShadow(q4_test::Runner* runner) {
    rhi::DirectLightResult result;
    std::string error;
    const rhi::DirectLightInput light = Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f});

    rhi::ShadowInput insideBias;
    insideBias.surfaceDepth = 10.0f;
    insideBias.lightDepth = 9.995f;
    insideBias.constantBias = 0.01f;
    insideBias.slopeBias = 0.0f;
    runner->Check(rhi::EvaluateDirectLight(light, Surface(), insideBias, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 inside-bias sample evaluates", error);
    runner->Near(result.shadowFactor, 1.0f, 1.0e-6f, "T052 inside-bias sample is lit");

    rhi::ShadowInput occluded = insideBias;
    occluded.lightDepth = 9.98f;
    occluded.taps = 4;
    occluded.blockers = 1;
    runner->Check(rhi::EvaluateDirectLight(light, Surface(), occluded, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 occluded sample evaluates", error);
    runner->Near(result.shadowFactor, 0.75f, 1.0e-6f, "T052 one of four taps occludes");
    runner->Near(result.diffuse.r, 0.5f * 0.75f, 1.0e-6f, "T052 shadow scales diffuse");

    rhi::ShadowInput fullyOccluded = occluded;
    fullyOccluded.blockers = 4;
    runner->Check(rhi::EvaluateDirectLight(light, Surface(), fullyOccluded, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 fully occluded sample evaluates", error);
    runner->Near(result.shadowFactor, 0.0f, 1.0e-6f, "T052 all taps occlude");

    // The shadow cast declaration is honoured: a light that does not cast
    // shadows ignores the depth comparison.
    rhi::DirectLightInput noCaster = light;
    noCaster.castsShadow = false;
    runner->Check(rhi::EvaluateDirectLight(noCaster, Surface(), fullyOccluded, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 non-casting light evaluates", error);
    runner->Near(result.shadowFactor, 1.0f, 1.0e-6f, "T052 non-casting light is unshadowed");

    rhi::ShadowInput badTaps = insideBias;
    badTaps.taps = 0;
    runner->Check(rhi::EvaluateDirectLight(light, Surface(), badTaps, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T052 zero taps is InvalidArgument");
    rhi::ShadowInput badBlockers = insideBias;
    badBlockers.taps = 2;
    badBlockers.blockers = 3;
    runner->Check(rhi::EvaluateDirectLight(light, Surface(), badBlockers, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T052 blockers above taps is InvalidArgument");
}

void RunUnsupportedStages(q4_test::Runner* runner) {
    const rhi::DirectLightInput light = Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f});
    const rhi::SurfaceStage stages[] = {
        rhi::SurfaceStage::Translucent, rhi::SurfaceStage::Blend, rhi::SurfaceStage::Fog,
        rhi::SurfaceStage::Glasswarp, rhi::SurfaceStage::Skybox, rhi::SurfaceStage::Decal,
        rhi::SurfaceStage::PostProcess,
    };
    for (rhi::SurfaceStage stage : stages) {
        rhi::DirectSurfaceInput surface = Surface();
        surface.stage = stage;
        rhi::DirectLightResult result;
        std::string error;
        const rhi::Status status =
            rhi::EvaluateDirectLight(light, surface, rhi::ShadowInput{}, &result, &error);
        runner->Check(status == rhi::Status::Unsupported &&
                         result.status == rhi::Status::Unsupported,
                     std::string("T052 unsupported legacy stage ") + rhi::ToString(stage),
                     error);
        runner->Check(!result.shaded && result.diffuse.r == 0.0f &&
                         error.find(rhi::ToString(stage)) != std::string::npos,
                     std::string("T052 unsupported stage reports a reason: ") +
                         rhi::ToString(stage),
                     error);
    }

    // Alpha-tested geometry is a valid receiver; a failed coverage test
    // discards the sample instead of shading it.
    rhi::DirectSurfaceInput alpha = Surface();
    alpha.stage = rhi::SurfaceStage::AlphaTested;
    alpha.alphaTestPass = false;
    rhi::DirectLightResult result;
    std::string error;
    runner->Check(rhi::EvaluateDirectLight(light, alpha, rhi::ShadowInput{}, &result, &error) ==
                     rhi::Status::Ok,
                 "T052 alpha-tested miss is a valid result", error);
    runner->Check(result.discarded && !result.shaded,
                 "T052 alpha-tested miss is discarded, not shaded");
}

void RunInvalidInputs(q4_test::Runner* runner) {
    rhi::DirectLightResult result;
    std::string error;
    const rhi::DirectLightInput light = Directional(rhi::LightVec3{0.0f, 0.0f, -1.0f});

    rhi::DirectSurfaceInput zeroNormal = Surface();
    zeroNormal.normal = rhi::LightVec3{0.0f, 0.0f, 0.0f};
    runner->Check(rhi::EvaluateDirectLight(light, zeroNormal, rhi::ShadowInput{}, &result,
                                          &error) == rhi::Status::InvalidArgument,
                 "T052 zero normal is InvalidArgument");

    rhi::DirectLightInput negative = light;
    negative.radiance = rhi::LinearRgb{-1.0f, 0.0f, 0.0f};
    runner->Check(rhi::EvaluateDirectLight(negative, Surface(), rhi::ShadowInput{}, &result,
                                          &error) == rhi::Status::InvalidArgument,
                 "T052 negative radiance is InvalidArgument");

    rhi::DirectLightInput spot = light;
    spot.range = 10.0f;
    spot.position = rhi::LightVec3{0.0f, 0.0f, 1.0f};
    spot.cosInner = 0.2f;
    spot.cosOuter = 0.8f;
    runner->Check(rhi::EvaluateDirectLight(spot, Surface(), rhi::ShadowInput{}, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T052 inner cone below outer cone is InvalidArgument");

    rhi::DirectLightInput outsideSpot = spot;
    outsideSpot.position = rhi::LightVec3{1.0f, 0.0f, 0.0f};
    outsideSpot.cosInner = 0.9f;
    outsideSpot.cosOuter = 0.5f;
    runner->Check(rhi::EvaluateDirectLight(outsideSpot, Surface(), rhi::ShadowInput{}, &result,
                                          &error) == rhi::Status::Ok,
                 "T052 outside a spot cone is a valid zero", error);
    runner->Check(!result.shaded && result.attenuation == 0.0f,
                 "T052 outside a spot cone is unlit");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    RunAttenuation(&runner);
    RunDirectLight(&runner);
    RunShadow(&runner);
    RunUnsupportedStages(&runner);
    RunInvalidInputs(&runner);
    return runner.Finish("lighting");
}
