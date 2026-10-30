// Q6 alpha, transmission and emissive transport (spec 004 T067).
//
// Expectations:
//   * a clear interface at normal incidence reflects with F0 = 0.04, so the
//     expected radiance through a lower hemisphere of 2 is (1 - 0.04) * 2 *
//     tint;
//   * a closed box whose only surfaces above the floor are emissive returns
//     albedo * emission on the first bounce, with no variance;
//   * an alpha-mask hole passes the ray through and a solid cell does not, for
//     both the primary path and a shadow ray.

#include "dxr_test_support.h"

#include "rhi/pathtrace.h"
#include "rhi/pathtrace_material.h"
#include "rhi/raytracing_scene.h"

#include <cmath>
#include <string>
#include <vector>

using doom3::rhi::AlphaTestDiscards;
using doom3::rhi::DielectricFresnel;
using doom3::rhi::EvaluateAlphaCoverage;
using doom3::rhi::FresnelSchlick;
using doom3::rhi::PathMaterial;
using doom3::rhi::PathMaterialKind;
using doom3::rhi::PathTraceCounters;
using doom3::rhi::PathTraceSample;
using doom3::rhi::PathTraceSettings;
using doom3::rhi::PointLight;
using doom3::rhi::RaytracingScene;
using doom3::rhi::ReflectDirection;
using doom3::rhi::RefractDirection;
using doom3::rhi::RtRay;
using doom3::rhi::SampleConductorDirection;
using doom3::rhi::Status;
using doom3::rhi::TraceRadiance;

namespace {

struct MeanResult {
    doom3::rhi::LinearRgb mean;
    uint32_t samples = 0;
    uint32_t transmissions = 0;
    uint32_t reflections = 0;
    uint32_t alphaRejects = 0;
};

MeanResult RunMean(const RaytracingScene& scene, const std::vector<PathMaterial>& materials,
                   const RtRay& ray, const PathTraceSettings& settings, uint32_t samples) {
    MeanResult result;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    for (uint32_t index = 0; index < samples; ++index) {
        PathTraceSample sample;
        PathTraceCounters counters;
        std::string error;
        const Status status =
            TraceRadiance(scene, materials, ray, nullptr, 0, settings,
                          doom3::rhi::BuildSampleSeed(0xABCDEFu, 0, 0, index), &sample,
                          &counters, &error);
        if (status != Status::Ok) {
            continue;
        }
        r += static_cast<double>(sample.radiance.r);
        g += static_cast<double>(sample.radiance.g);
        b += static_cast<double>(sample.radiance.b);
        result.transmissions += counters.transmissions;
        result.reflections += counters.bounces - counters.transmissions;
        result.alphaRejects += counters.alphaRejects;
        ++result.samples;
    }
    if (result.samples > 0) {
        const double inverse = 1.0 / static_cast<double>(result.samples);
        result.mean.r = static_cast<float>(r * inverse);
        result.mean.g = static_cast<float>(g * inverse);
        result.mean.b = static_cast<float>(b * inverse);
    }
    return result;
}

PathMaterial AlphaMaterial(float albedo) {
    PathMaterial material;
    material.kind = PathMaterialKind::Diffuse;
    material.albedo = doom3::rhi::LinearRgb{albedo, albedo, albedo};
    material.alphaTest = true;
    material.alphaThreshold = 0.5f;
    return material;
}

PathMaterial Emissive(float emission) {
    PathMaterial material;
    material.kind = PathMaterialKind::Emissive;
    material.emission = doom3::rhi::LinearRgb{emission, emission, emission};
    return material;
}

void AddBoxWalls(std::vector<doom3::rhi::RtTriangle>* triangles, uint32_t material) {
    dxr_test::AddQuad(triangles, dxr_test::V(-2.0f, -2.0f, 0.0f),
                      dxr_test::V(-2.0f, 2.0f, 0.0f), dxr_test::V(-2.0f, 2.0f, 2.0f),
                      dxr_test::V(-2.0f, -2.0f, 2.0f), material, 10);
    dxr_test::AddQuad(triangles, dxr_test::V(2.0f, -2.0f, 0.0f),
                      dxr_test::V(2.0f, 2.0f, 0.0f), dxr_test::V(2.0f, 2.0f, 2.0f),
                      dxr_test::V(2.0f, -2.0f, 2.0f), material, 11);
    dxr_test::AddQuad(triangles, dxr_test::V(-2.0f, -2.0f, 0.0f),
                      dxr_test::V(2.0f, -2.0f, 0.0f), dxr_test::V(2.0f, -2.0f, 2.0f),
                      dxr_test::V(-2.0f, -2.0f, 2.0f), material, 12);
    dxr_test::AddQuad(triangles, dxr_test::V(-2.0f, 2.0f, 0.0f),
                      dxr_test::V(2.0f, 2.0f, 0.0f), dxr_test::V(2.0f, 2.0f, 2.0f),
                      dxr_test::V(-2.0f, 2.0f, 2.0f), material, 13);
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    // --- Alpha mask ---------------------------------------------------------
    {
        PathMaterial material = AlphaMaterial(0.9f);
        runner.Near(EvaluateAlphaCoverage(material, dxr_test::UV(0.2f, 0.2f)), 1.0f, 0.0f,
                    "alpha cell (0,0) is solid");
        runner.Near(EvaluateAlphaCoverage(material, dxr_test::UV(0.7f, 0.2f)), 0.0f, 0.0f,
                    "alpha cell (1,0) is a hole");
        runner.Near(EvaluateAlphaCoverage(material, dxr_test::UV(0.2f, 0.7f)), 0.0f, 0.0f,
                    "alpha cell (0,1) is a hole");
        runner.Near(EvaluateAlphaCoverage(material, dxr_test::UV(0.7f, 0.7f)), 1.0f, 0.0f,
                    "alpha cell (1,1) is solid");
        runner.Check(AlphaTestDiscards(material, dxr_test::UV(0.7f, 0.2f)),
                     "a hole discards the sample");
        runner.Check(!AlphaTestDiscards(material, dxr_test::UV(0.2f, 0.2f)),
                     "a solid cell keeps the sample");
        material.alphaTest = false;
        runner.Near(EvaluateAlphaCoverage(material, dxr_test::UV(0.7f, 0.2f)), 1.0f, 0.0f,
                    "a material without alpha test has full coverage");
    }

    // --- Fresnel and refraction --------------------------------------------
    {
        runner.Near(FresnelSchlick(0.04f, 1.0f), 0.04f, 1.0e-6f, "Schlick at normal incidence");
        runner.Near(FresnelSchlick(0.04f, 0.0f), 1.0f, 1.0e-6f, "Schlick at grazing incidence");
        runner.Near(DielectricFresnel(1.0f, 1.5f), 0.04f, 1.0e-5f,
                    "dielectric F0 for ior 1.5 is 0.04");
        runner.Near(DielectricFresnel(0.0f, 1.5f), 1.0f, 1.0e-6f,
                    "dielectric reflectance reaches one at grazing incidence");
        runner.Near(DielectricFresnel(0.70710678f, 1.5f), 0.05024f, 1.0e-4f,
                    "dielectric reflectance at 45 degrees");

        doom3::rhi::RtVec3 refracted;
        runner.Check(RefractDirection(dxr_test::V(0.0f, 0.0f, -1.0f), dxr_test::V(0.0f, 0.0f, 1.0f),
                                      1.0f / 1.5f, &refracted),
                     "normal incidence refracts");
        runner.Near(refracted.z, -1.0f, 1.0e-5f, "normal incidence keeps the direction");

        const float invRootTwo = 0.70710678f;
        runner.Check(RefractDirection(dxr_test::V(0.0f, invRootTwo, -invRootTwo),
                                      dxr_test::V(0.0f, 0.0f, 1.0f), 1.0f / 1.5f, &refracted),
                     "a 45 degree ray refracts");
        runner.Near(refracted.y, invRootTwo / 1.5f, 1.0e-5f, "Snell bends the ray toward the normal");
        runner.Near(doom3::rhi::RtLength(refracted), 1.0f, 1.0e-5f, "the refracted ray is unit length");
        runner.Check(!RefractDirection(dxr_test::V(0.0f, invRootTwo, -invRootTwo),
                                       dxr_test::V(0.0f, 0.0f, 1.0f), 1.5f, &refracted),
                     "total internal reflection is reported");

        const doom3::rhi::RtVec3 reflected =
            ReflectDirection(dxr_test::V(0.0f, 0.0f, -1.0f), dxr_test::V(0.0f, 0.0f, 1.0f));
        runner.Near(reflected.z, 1.0f, 1.0e-6f, "a mirror reflects the ray");
    }

    // --- Conductor sampling -------------------------------------------------
    {
        PathMaterial conductor;
        conductor.kind = PathMaterialKind::Conductor;
        conductor.albedo = doom3::rhi::LinearRgb{1.0f, 1.0f, 1.0f};
        conductor.roughness = 0.0f;
        doom3::rhi::RtVec3 direction;
        runner.Check(SampleConductorDirection(conductor, dxr_test::V(0.0f, 0.0f, -1.0f),
                                              dxr_test::V(0.0f, 0.0f, 1.0f), 0.99f, 0.5f,
                                              &direction),
                     "a smooth conductor samples a direction");
        runner.Near(direction.z, 1.0f, 1.0e-5f, "roughness zero is the exact mirror");
        conductor.roughness = 1.0f;
        runner.Check(SampleConductorDirection(conductor, dxr_test::V(0.0f, 0.0f, -1.0f),
                                              dxr_test::V(0.0f, 0.0f, 1.0f), 0.3f, 0.5f,
                                              &direction),
                     "a rough conductor samples a direction");
        runner.Check(doom3::rhi::RtDot(direction, dxr_test::V(0.0f, 0.0f, 1.0f)) >= 0.0f,
                     "a rough conductor stays in the hemisphere");
        runner.Near(doom3::rhi::RtLength(direction), 1.0f, 1.0e-5f,
                    "a rough conductor direction is unit length");
        PathMaterial diffuse;
        diffuse.kind = PathMaterialKind::Diffuse;
        runner.Check(!SampleConductorDirection(diffuse, dxr_test::V(0.0f, 0.0f, -1.0f),
                                               dxr_test::V(0.0f, 0.0f, 1.0f), 0.5f, 0.5f,
                                               &direction),
                     "a diffuse material has no conductor lobe");
    }

    // --- Emissive transport in a closed box ---------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 2.0f, 0.0f, 0, 1);
        dxr_test::AddCeiling(&triangles, 2.0f, 2.0f, 1, 2);
        AddBoxWalls(&triangles, 1);
        RaytracingScene scene;
        std::string error;
        runner.Check(scene.SetGeometry(triangles, &error), "the emissive box builds");

        PathMaterial floor;
        floor.kind = PathMaterialKind::Diffuse;
        floor.albedo = doom3::rhi::LinearRgb{0.6f, 0.5f, 0.4f};
        std::vector<PathMaterial> materials{floor, Emissive(0.5f)};

        PathTraceSettings settings;
        settings.maxDepth = 3;
        settings.nextEventEstimation = false;

        const RtRay down{dxr_test::V(0.0f, 0.0f, 1.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        const MeanResult bounce = RunMean(scene, materials, down, settings, 64);
        runner.Check(bounce.samples == 64, "every box sample traced");
        runner.Near(bounce.mean.r, 0.6f * 0.5f, 1.0e-5f, "diffuse indirect emission red");
        runner.Near(bounce.mean.g, 0.5f * 0.5f, 1.0e-5f, "diffuse indirect emission green");
        runner.Near(bounce.mean.b, 0.4f * 0.5f, 1.0e-5f, "diffuse indirect emission blue");

        const RtRay up{dxr_test::V(0.0f, 0.0f, 1.0f), dxr_test::V(0.0f, 0.0f, 1.0f)};
        const MeanResult direct = RunMean(scene, materials, up, settings, 16);
        runner.Near(direct.mean.r, 0.5f, 1.0e-6f, "a primary emissive hit returns its emission");
    }

    // --- Alpha on the primary path -----------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> solid;
        dxr_test::AddQuad(&solid, dxr_test::V(-2.0f, -2.0f, 1.0f), dxr_test::V(2.0f, -2.0f, 1.0f),
                          dxr_test::V(2.0f, 2.0f, 1.0f), dxr_test::V(-2.0f, 2.0f, 1.0f), 0, 1,
                          dxr_test::UV(0.1f, 0.1f), dxr_test::UV(0.2f, 0.2f));
        // The same geometry mapped to a hole cell.
        std::vector<doom3::rhi::RtTriangle> holeTriangles;
        dxr_test::AddQuad(&holeTriangles, dxr_test::V(-2.0f, -2.0f, 1.0f),
                          dxr_test::V(2.0f, -2.0f, 1.0f), dxr_test::V(2.0f, 2.0f, 1.0f),
                          dxr_test::V(-2.0f, 2.0f, 1.0f), 0, 1, dxr_test::UV(0.6f, 0.1f),
                          dxr_test::UV(0.7f, 0.2f));

        RaytracingScene solidScene;
        RaytracingScene holeScene;
        std::string error;
        solidScene.SetGeometry(solid, &error);
        holeScene.SetGeometry(holeTriangles, &error);

        PathMaterial alpha = AlphaMaterial(0.9f);
        std::vector<PathMaterial> materials{alpha};

        PathTraceSettings settings;
        settings.maxDepth = 3;
        settings.nextEventEstimation = false;
        settings.environment.enabled = true;
        settings.environment.upper = doom3::rhi::LinearRgb{0.0f, 0.0f, 0.0f};
        settings.environment.lower = doom3::rhi::LinearRgb{5.0f, 5.0f, 5.0f};

        const RtRay ray{dxr_test::V(0.0f, 0.0f, 2.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        const MeanResult solidResult = RunMean(solidScene, materials, ray, settings, 1);
        const MeanResult holeResult = RunMean(holeScene, materials, ray, settings, 1);
        runner.Near(solidResult.mean.r, 0.0f, 1.0e-6f,
                    "an alpha-tested solid surface stays opaque");
        runner.Near(holeResult.mean.r, 5.0f, 1.0e-6f,
                    "an alpha hole lets the primary ray reach the environment");
        runner.Check(holeResult.alphaRejects == 1,
                     "the primary alpha rejection is counted");
    }

    // --- Alpha on a shadow ray ----------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> base;
        dxr_test::AddFloor(&base, 5.0f, 0.0f, 0, 0);
        std::vector<doom3::rhi::RtTriangle> solid = base;
        dxr_test::AddQuad(&solid, dxr_test::V(-0.5f, -0.5f, 1.5f),
                          dxr_test::V(0.5f, -0.5f, 1.5f), dxr_test::V(0.5f, 0.5f, 1.5f),
                          dxr_test::V(-0.5f, 0.5f, 1.5f), 1, 9, dxr_test::UV(0.1f, 0.1f),
                          dxr_test::UV(0.2f, 0.2f));
        std::vector<doom3::rhi::RtTriangle> hole = base;
        dxr_test::AddQuad(&hole, dxr_test::V(-0.5f, -0.5f, 1.5f),
                          dxr_test::V(0.5f, -0.5f, 1.5f), dxr_test::V(0.5f, 0.5f, 1.5f),
                          dxr_test::V(-0.5f, 0.5f, 1.5f), 1, 9, dxr_test::UV(0.6f, 0.1f),
                          dxr_test::UV(0.7f, 0.2f));

        RaytracingScene solidScene;
        RaytracingScene holeScene;
        std::string error;
        solidScene.SetGeometry(solid, &error);
        holeScene.SetGeometry(hole, &error);

        PathMaterial floor;
        floor.kind = PathMaterialKind::Diffuse;
        floor.albedo = doom3::rhi::LinearRgb{0.6f, 0.6f, 0.6f};
        std::vector<PathMaterial> materials{floor, AlphaMaterial(0.9f)};

        PointLight light;
        light.position = dxr_test::V(0.0f, 0.0f, 3.0f);
        light.radiance = doom3::rhi::LinearRgb{4.0f, 4.0f, 4.0f};

        PathTraceSettings settings;
        settings.maxDepth = 2;
        settings.nextEventEstimation = true;
        settings.environment.enabled = false;

        const RtRay ray{dxr_test::V(0.0f, -3.0f, 2.0f), dxr_test::V(0.0f, 0.8320503f, -0.5547002f)};
        PathTraceSample solidSample;
        PathTraceCounters solidCounters;
        runner.Check(TraceRadiance(solidScene, materials, ray, &light, 1, settings, 0x3u,
                                   &solidSample, &solidCounters, &error) == Status::Ok,
                     "the solid shadow trace succeeds");
        runner.Near(solidSample.radiance.r, 0.0f, 1.0e-6f,
                    "a solid alpha cell blocks the point light");

        PathTraceSample holeSample;
        PathTraceCounters holeCounters;
        runner.Check(TraceRadiance(holeScene, materials, ray, &light, 1, settings, 0x3u,
                                   &holeSample, &holeCounters, &error) == Status::Ok,
                     "the alpha-hole shadow trace succeeds");
        const float expected = 0.6f * (4.0f / 9.0f) / 3.14159265358979323846f;
        runner.Near(holeSample.radiance.r, expected, 1.0e-4f,
                    "an alpha hole lets the shadow ray reach the light");
        runner.Check(holeCounters.alphaRejects >= 1,
                     "the shadow alpha rejection is counted");
    }

    // --- Dielectric transmission -------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 2.0f, 1.0f, 0, 1);
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);

        PathMaterial glass;
        glass.kind = PathMaterialKind::Dielectric;
        glass.ior = 1.5f;
        glass.transmissionTint = doom3::rhi::LinearRgb{1.0f, 0.5f, 0.25f};
        std::vector<PathMaterial> materials{glass};

        PathTraceSettings settings;
        settings.maxDepth = 3;
        settings.nextEventEstimation = false;
        settings.environment.enabled = true;
        settings.environment.upper = doom3::rhi::LinearRgb{0.0f, 0.0f, 0.0f};
        settings.environment.lower = doom3::rhi::LinearRgb{2.0f, 2.0f, 2.0f};

        const RtRay ray{dxr_test::V(0.0f, 0.0f, 2.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        const MeanResult result = RunMean(scene, materials, ray, settings, 4096);
        const float transmitted = (1.0f - 0.04f) * 2.0f;
        runner.Near(result.mean.r, transmitted * 1.0f, 0.03f, "transmission red channel");
        runner.Near(result.mean.g, transmitted * 0.5f, 0.03f, "transmission green channel");
        runner.Near(result.mean.b, transmitted * 0.25f, 0.03f, "transmission blue channel");
        runner.Check(result.transmissions > 0, "the transmission branch is sampled");
        runner.Check(result.reflections > 0, "the Fresnel reflection branch is sampled");
        runner.Check(result.mean.r > result.mean.g && result.mean.g > result.mean.b,
                     "the transmission tint orders the channels");
    }

    return runner.Finish("q6_pathtrace_material_test");
}
