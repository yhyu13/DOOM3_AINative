// T053 indirect-light, reflection and atmosphere fixtures.
//
// The cases target the artifact modes a hybrid path actually fails on: negative
// irradiance, a reflection that points below the horizon, an over-rough mirror,
// a fogged HDR colour and a NaN height input. Each expected value is the
// formula documented in `rhi/indirect_lighting.h`.

#include "q4_test_support.h"
#include "rhi/indirect_lighting.h"

#include <cmath>
#include <string>

namespace {

namespace rhi = doom3::rhi;

void RunIrradiance(q4_test::Runner* runner) {
    rhi::Sh4 probe;
    probe.c[0][0] = 1.0f;
    probe.c[0][1] = 1.0f;
    probe.c[0][2] = 1.0f;

    rhi::LinearRgb irradiance;
    bool clamped = true;
    runner->Check(rhi::EvaluateIrradiance(probe, rhi::LightVec3{0.0f, 0.0f, 1.0f}, &irradiance,
                                         &clamped),
                 "T053 constant probe evaluates");
    runner->Near(irradiance.r, 0.282095f, 1.0e-6f, "T053 constant probe irradiance");
    runner->Check(!clamped, "T053 constant probe is not clamped");

    rhi::Sh4 directional;
    directional.c[1][0] = 1.0f;
    runner->Check(rhi::EvaluateIrradiance(directional, rhi::LightVec3{0.0f, 1.0f, 0.0f},
                                         &irradiance, &clamped),
                 "T053 +Y probe evaluates");
    runner->Near(irradiance.r, 0.488603f, 1.0e-6f, "T053 +Y probe irradiance");
    runner->Check(rhi::EvaluateIrradiance(directional, rhi::LightVec3{1.0f, 0.0f, 0.0f},
                                         &irradiance, &clamped) &&
                     irradiance.r == 0.0f,
                 "T053 +Y probe does not leak into +X");

    rhi::Sh4 negative;
    negative.c[0][0] = -1.0f;
    runner->Check(rhi::EvaluateIrradiance(negative, rhi::LightVec3{0.0f, 0.0f, 1.0f}, &irradiance,
                                         &clamped),
                 "T053 negative probe evaluates");
    runner->Check(clamped && irradiance.r == 0.0f,
                 "T053 negative irradiance is clamped and reported");

    runner->Check(!rhi::EvaluateIrradiance(probe, rhi::LightVec3{0.0f, 0.0f, 0.0f}, &irradiance,
                                          &clamped),
                 "T053 zero normal is refused");
}

void RunReflection(q4_test::Runner* runner) {
    rhi::ReflectionInput input;
    input.incident = rhi::LightVec3{0.0f, -1.0f, 0.0f};
    input.normal = rhi::LightVec3{0.0f, 1.0f, 0.0f};
    input.roughness = 0.0f;
    input.environment.zenith = rhi::LinearRgb{2.0f, 2.0f, 2.0f};
    input.environment.horizon = rhi::LinearRgb{0.5f, 0.5f, 0.5f};
    input.environment.ground = rhi::LinearRgb{0.1f, 0.1f, 0.1f};

    rhi::ReflectionResult result;
    std::string error;
    runner->Check(rhi::EvaluateReflection(input, &result, &error) == rhi::Status::Ok,
                 "T053 mirror reflection evaluates", error);
    runner->Near(result.reflection.r, 2.0f, 1.0e-6f,
                "T053 mirror reflection preserves HDR zenith");
    runner->Check(!result.horizonFallback && !result.blurred,
                 "T053 mirror reflection is neither clamped nor blurred");

    input.normal = rhi::LightVec3{0.0f, 1.0f, 0.0f};
    input.incident = rhi::LightVec3{0.0f, 1.0f, 0.0f};
    runner->Check(rhi::EvaluateReflection(input, &result, &error) == rhi::Status::Ok,
                 "T053 below-horizon reflection evaluates", error);
    runner->Check(result.horizonFallback,
                 "T053 below-horizon reflection is reported");
    runner->Near(result.reflection.r, 0.1f, 1.0e-6f,
                "T053 below-horizon reflection falls back to ground, not black");

    input.normal = rhi::LightVec3{0.0f, 0.0f, 1.0f};
    input.incident = rhi::LightVec3{0.0f, 0.0f, -1.0f};
    input.roughness = 1.0f;
    runner->Check(rhi::EvaluateReflection(input, &result, &error) == rhi::Status::Ok,
                 "T053 fully rough reflection evaluates", error);
    runner->Near(result.reflection.r, (2.0f + 0.5f + 0.1f) / 3.0f, 1.0e-6f,
                "T053 fully rough reflection is the sky average");
    runner->Check(result.blurred, "T053 rough reflection reports blur");

    rhi::ReflectionInput bad = input;
    bad.roughness = 1.5f;
    runner->Check(rhi::EvaluateReflection(bad, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T053 roughness outside [0,1] is refused");
    rhi::ReflectionInput negative = input;
    negative.environment.zenith = rhi::LinearRgb{-1.0f, 0.0f, 0.0f};
    runner->Check(rhi::EvaluateReflection(negative, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T053 negative environment radiance is refused");
}

void RunAtmosphere(q4_test::Runner* runner) {
    rhi::AtmosphereInput input;
    input.distance = 1.0f;
    input.density = 1.0f;
    input.heightFalloff = 0.0f;
    input.fogColor = rhi::LinearRgb{4.0f, 2.0f, 1.0f};

    rhi::AtmosphereResult result;
    std::string error;
    runner->Check(rhi::EvaluateAtmosphere(input, &result, &error) == rhi::Status::Ok,
                 "T053 atmosphere evaluates", error);
    const float expected = std::exp(-1.0f);
    runner->Near(result.transmittance, expected, 1.0e-6f, "T053 exp(-1) transmittance");
    runner->Near(result.inScatter.r, 4.0f * (1.0f - expected), 1.0e-6f,
                "T053 HDR fog colour is preserved in scatter");
    runner->Near(result.inScatter.b, 1.0f * (1.0f - expected), 1.0e-6f,
                "T053 fog blue channel is preserved");

    input.distance = 0.0f;
    runner->Check(rhi::EvaluateAtmosphere(input, &result, &error) == rhi::Status::Ok,
                 "T053 zero distance atmosphere evaluates", error);
    runner->Near(result.transmittance, 1.0f, 1.0e-7f, "T053 zero distance is fully transparent");

    input.distance = 1.0f;
    input.enabled = false;
    runner->Check(rhi::EvaluateAtmosphere(input, &result, &error) == rhi::Status::Ok,
                 "T053 disabled atmosphere evaluates", error);
    runner->Check(result.transmittance == 1.0f && result.inScatter.r == 0.0f,
                 "T053 disabled atmosphere adds nothing");

    input.enabled = true;
    input.heightFalloff = 1.0f;
    input.cameraHeight = 10.0f;
    input.fogHeight = 2.0f;
    runner->Check(rhi::EvaluateAtmosphere(input, &result, &error) == rhi::Status::Ok,
                 "T053 height falloff evaluates", error);
    const float scaled = std::exp(-std::exp(-8.0f));
    runner->Near(result.transmittance, scaled, 1.0e-6f,
                "T053 height falloff reduces extinction");

    rhi::AtmosphereInput negative = input;
    negative.distance = -1.0f;
    runner->Check(rhi::EvaluateAtmosphere(negative, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T053 negative distance is refused");
    rhi::AtmosphereInput nanHeight = input;
    nanHeight.cameraHeight = std::nanf("");
    runner->Check(rhi::EvaluateAtmosphere(nanHeight, &result, &error) ==
                     rhi::Status::InvalidArgument,
                 "T053 non-finite height is refused");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    RunIrradiance(&runner);
    RunReflection(&runner);
    RunAtmosphere(&runner);
    return runner.Finish("indirect_atmosphere");
}
