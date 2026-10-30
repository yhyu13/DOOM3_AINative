// Q6 static geometry, camera and convergence (spec 004 T065).
//
// The convergence target is computed independently: a diffuse wall whose
// normal is horizontal sees a two-hemisphere environment (1 above, 0 below).
// Half of the cosine-weighted directions see 1, so the expected radiance is
// albedo * 0.5 and the single-sample variance is albedo^2 * 0.25. The direct
// light target is albedo / pi * radiance * cos / distance^2, written here from
// the documented formula.

#include "dxr_test_support.h"

#include "rhi/pathtrace.h"
#include "rhi/pathtrace_denoise.h"
#include "rhi/raytracing_scene.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using doom3::rhi::AccumulateSample;
using doom3::rhi::EvaluateEnvironment;
using doom3::rhi::GenerateCameraRay;
using doom3::rhi::PathCamera;
using doom3::rhi::PathEnvironment;
using doom3::rhi::PathMaterial;
using doom3::rhi::PathMaterialKind;
using doom3::rhi::PathTraceCounters;
using doom3::rhi::PathTraceSample;
using doom3::rhi::PathTraceSettings;
using doom3::rhi::PointLight;
using doom3::rhi::RaytracingScene;
using doom3::rhi::ReferenceAccumulator;
using doom3::rhi::RenderReference;
using doom3::rhi::ResetAccumulator;
using doom3::rhi::ResolveAccumulator;
using doom3::rhi::RtRay;
using doom3::rhi::Status;
using doom3::rhi::TotalSamples;
using doom3::rhi::TraceRadiance;

namespace {

constexpr float kPi = 3.14159265358979323846f;

PathMaterial Diffuse(float albedo) {
    PathMaterial material;
    material.kind = PathMaterialKind::Diffuse;
    material.albedo = doom3::rhi::LinearRgb{albedo, albedo, albedo};
    material.roughness = 1.0f;
    return material;
}

struct Statistics {
    float mean = 0.0f;
    float variance = 0.0f;
    float error = 0.0f;
};

Statistics SampleMean(const RaytracingScene& scene, const std::vector<PathMaterial>& materials,
                      const RtRay& ray, const PathTraceSettings& settings, uint64_t baseSeed,
                      uint32_t samples, float expected) {
    Statistics statistics;
    std::vector<float> values;
    values.reserve(samples);
    double sum = 0.0;
    for (uint32_t index = 0; index < samples; ++index) {
        PathTraceSample sample;
        std::string error;
        const Status status = TraceRadiance(scene, materials, ray, nullptr, 0, settings,
                                            doom3::rhi::BuildSampleSeed(baseSeed, 0, 0, index),
                                            &sample, nullptr, &error);
        if (status != Status::Ok) {
            continue;
        }
        const float value = sample.radiance.r;
        values.push_back(value);
        sum += static_cast<double>(value);
    }
    statistics.mean = static_cast<float>(sum / static_cast<double>(values.size()));
    double variance = 0.0;
    for (float value : values) {
        const double difference = static_cast<double>(value) - statistics.mean;
        variance += difference * difference;
    }
    statistics.variance = static_cast<float>(variance / static_cast<double>(values.size()));
    statistics.error = std::fabs(statistics.mean - expected);
    return statistics;
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    // --- Camera rays -------------------------------------------------------
    {
        PathCamera camera;
        camera.position = dxr_test::V(0.0f, 0.0f, 0.0f);
        camera.width = 2;
        camera.height = 2;
        camera.tanHalfFovX = 1.0f;
        camera.tanHalfFovY = 1.0f;

        const RtRay topLeft = GenerateCameraRay(camera, 0, 0, 0.5f, 0.5f);
        const float length = std::sqrt(0.25f + 0.25f + 1.0f);
        runner.Near(topLeft.direction.x, -0.5f / length, 1.0e-5f, "camera ray left x");
        runner.Near(topLeft.direction.y, 0.5f / length, 1.0e-5f, "camera ray up y");
        runner.Near(topLeft.direction.z, -1.0f / length, 1.0e-5f, "camera ray forward z");
        runner.Near(topLeft.origin.x, 0.0f, 0.0f, "camera ray origin x");
        runner.Near(topLeft.origin.z, 0.0f, 0.0f, "camera ray origin z");

        const RtRay topRight = GenerateCameraRay(camera, 1, 0, 0.5f, 0.5f);
        runner.Check(topRight.direction.x > 0.0f, "top-right pixel points right");
        const RtRay jitterLeft = GenerateCameraRay(camera, 1, 0, 0.0f, 0.5f);
        const RtRay jitterRight = GenerateCameraRay(camera, 1, 0, 0.999f, 0.5f);
        runner.Check(jitterLeft.direction.x < jitterRight.direction.x,
                     "jitter moves inside the pixel and keeps order");
        runner.Near(jitterLeft.origin.x, 0.0f, 0.0f, "jitter does not move the camera");
    }

    // --- One triangle -------------------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        triangles.push_back(dxr_test::MakeTriangle(
            dxr_test::V(0.0f, 0.0f, 0.0f), dxr_test::V(1.0f, 0.0f, 0.0f),
            dxr_test::V(0.0f, 1.0f, 0.0f), dxr_test::UV(0.0f, 0.0f), dxr_test::UV(1.0f, 0.0f),
            dxr_test::UV(0.0f, 1.0f), 3, 7));
        RaytracingScene scene;
        std::string error;
        runner.Check(scene.SetGeometry(triangles, &error), "one-triangle scene builds");
        runner.Check(scene.triangleCount() == 1, "one triangle is recorded");
        runner.Check(scene.nodeCount() == 1, "one-triangle scene has a single leaf");
        runner.Check(scene.leafCount() == 1, "one-triangle scene has one leaf");

        doom3::rhi::RtHit hit;
        const RtRay fromAbove{dxr_test::V(0.25f, 0.25f, 1.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(scene.Intersect(fromAbove, 0.0f, 100.0f, &hit), "ray from above hits");
        runner.Near(hit.t, 1.0f, 1.0e-5f, "hit distance");
        runner.Near(hit.position.x, 0.25f, 1.0e-5f, "hit position x");
        runner.Near(hit.position.y, 0.25f, 1.0e-5f, "hit position y");
        runner.Near(hit.position.z, 0.0f, 1.0e-5f, "hit position z");
        runner.Near(hit.uv.u, 0.25f, 1.0e-5f, "hit uv u");
        runner.Near(hit.uv.v, 0.25f, 1.0e-5f, "hit uv v");
        runner.Near(hit.normal.z, 1.0f, 1.0e-5f, "hit normal z");
        runner.Check(hit.material == 3, "hit material is the triangle's material");
        runner.Check(hit.primitive == 7, "hit primitive identity is preserved");
        runner.Check(!hit.backFace, "front face is reported");

        const RtRay fromBelow{dxr_test::V(0.25f, 0.25f, -1.0f), dxr_test::V(0.0f, 0.0f, 1.0f)};
        runner.Check(scene.Intersect(fromBelow, 0.0f, 100.0f, &hit), "ray from below hits");
        runner.Check(hit.backFace, "back face is reported");

        const RtRay miss{dxr_test::V(2.0f, 2.0f, 1.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(!scene.Intersect(miss, 0.0f, 100.0f, &hit), "a ray outside the triangle misses");
        runner.Check(!hit.hit, "a miss clears the hit record");
        runner.Check(scene.lastVisitCount() > 0, "traversal visited at least one node");
    }

    // --- Grid hierarchy -----------------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        for (int row = 0; row < 10; ++row) {
            for (int column = 0; column < 10; ++column) {
                const float x = static_cast<float>(column) - 4.5f;
                const float y = static_cast<float>(row) - 4.5f;
                dxr_test::AddQuad(&triangles, dxr_test::V(x, y, 0.0f),
                                  dxr_test::V(x + 1.0f, y, 0.0f),
                                  dxr_test::V(x + 1.0f, y + 1.0f, 0.0f),
                                  dxr_test::V(x, y + 1.0f, 0.0f), 0,
                                  static_cast<uint32_t>(row * 10 + column));
            }
        }
        RaytracingScene scene;
        std::string error;
        runner.Check(scene.SetGeometry(triangles, &error), "grid scene builds");
        runner.Check(scene.triangleCount() == 200, "grid scene has 200 triangles");
        runner.Check(scene.leafCount() > 1, "grid scene has more than one leaf");
        runner.Check(scene.nodeCount() >= scene.leafCount(), "internal nodes are counted");
        runner.Check(scene.maxDepth() <= 16, "median split keeps the hierarchy bounded");

        std::set<uint32_t> primitives;
        uint32_t hits = 0;
        for (int row = 0; row < 10; ++row) {
            for (int column = 0; column < 10; ++column) {
                const float x = static_cast<float>(column) - 4.0f;
                const float y = static_cast<float>(row) - 4.0f;
                const RtRay ray{dxr_test::V(x, y, 2.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
                doom3::rhi::RtHit hit;
                if (scene.Intersect(ray, 0.0f, 10.0f, &hit)) {
                    ++hits;
                    primitives.insert(hit.primitive);
                }
            }
        }
        runner.Check(hits == 100, "every grid cell centre hits the hierarchy");
        runner.Check(primitives.size() == 100, "each grid cell reports its own primitive");
    }

    // --- Nearest hit --------------------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 5.0f, 0.0f, 0, 0);
        dxr_test::AddFloor(&triangles, 5.0f, 1.0f, 0, 1);
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);
        doom3::rhi::RtHit hit;
        const RtRay ray{dxr_test::V(0.0f, 0.0f, 2.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(scene.Intersect(ray, 0.0f, 100.0f, &hit), "stacked quads are hit");
        runner.Near(hit.t, 1.0f, 1.0e-5f, "nearest quad wins");
        runner.Check(hit.primitive == 1, "nearest primitive is reported");
    }

    // --- Environment --------------------------------------------------------
    {
        PathEnvironment environment;
        environment.enabled = true;
        environment.upper = doom3::rhi::LinearRgb{1.0f, 2.0f, 3.0f};
        environment.lower = doom3::rhi::LinearRgb{4.0f, 5.0f, 6.0f};
        const doom3::rhi::LinearRgb above = EvaluateEnvironment(environment, dxr_test::V(0, 0, 1));
        const doom3::rhi::LinearRgb below = EvaluateEnvironment(environment, dxr_test::V(0, 0, -1));
        runner.Near(above.g, 2.0f, 0.0f, "upper hemisphere radiance");
        runner.Near(below.g, 5.0f, 0.0f, "lower hemisphere radiance");
        environment.enabled = false;
        const doom3::rhi::LinearRgb disabled =
            EvaluateEnvironment(environment, dxr_test::V(0, 0, 1));
        runner.Near(disabled.r, 0.0f, 0.0f, "a disabled environment is black");
    }

    // --- Convergence --------------------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddQuad(&triangles, dxr_test::V(0.0f, -2.0f, -2.0f),
                          dxr_test::V(0.0f, 2.0f, -2.0f), dxr_test::V(0.0f, 2.0f, 2.0f),
                          dxr_test::V(0.0f, -2.0f, 2.0f), 0, 1);
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);

        std::vector<PathMaterial> materials{Diffuse(0.8f)};
        PathTraceSettings settings;
        settings.maxDepth = 2;
        settings.nextEventEstimation = false;
        settings.environment.enabled = true;
        settings.environment.upper = doom3::rhi::LinearRgb{1.0f, 1.0f, 1.0f};
        settings.environment.lower = doom3::rhi::LinearRgb{0.0f, 0.0f, 0.0f};

        const RtRay ray{dxr_test::V(2.0f, 0.0f, 0.0f), dxr_test::V(-1.0f, 0.0f, 0.0f)};
        const float expected = 0.8f * 0.5f;
        const Statistics small = SampleMean(scene, materials, ray, settings, 0x1234ull, 64, expected);
        const Statistics large =
            SampleMean(scene, materials, ray, settings, 0x1234ull, 4096, expected);

        const float standardErrorSmall = 0.8f * 0.5f / std::sqrt(64.0f);
        const float standardErrorLarge = 0.8f * 0.5f / std::sqrt(4096.0f);
        runner.Check(small.error <= 3.0f * standardErrorSmall,
                     "64-sample estimate is within three standard errors");
        runner.Check(large.error <= 3.0f * standardErrorLarge,
                     "4096-sample estimate is within three standard errors");
        runner.Check(large.error <= small.error + standardErrorSmall,
                     "the larger sample is not worse than the smaller one by a standard error");
        runner.Near(large.variance, 0.8f * 0.8f * 0.25f, 0.02f,
                    "single-sample variance matches albedo^2 * p * (1-p)");
        runner.Near(large.mean, expected, 0.02f, "converged mean matches the analytic target");

        // Reference accumulation over the same estimator.
        ReferenceAccumulator accumulator;
        runner.Check(ResetAccumulator(&accumulator, 1, 1, &error), "1x1 accumulator resets");
        bool accumulated = true;
        for (uint32_t index = 0; index < 4096; ++index) {
            PathTraceSample sample;
            TraceRadiance(scene, materials, ray, nullptr, 0, settings,
                          doom3::rhi::BuildSampleSeed(0x77u, 0, 0, index), &sample, nullptr,
                          &error);
            const float rgb[3] = {sample.radiance.r, sample.radiance.g, sample.radiance.b};
            accumulated = accumulated && AccumulateSample(&accumulator, rgb, &error);
        }
        runner.Check(accumulated, "accumulator accepts every finite sample");
        float resolved[3] = {0.0f, 0.0f, 0.0f};
        runner.Check(ResolveAccumulator(accumulator, resolved, &error),
                     "accumulator resolves a mean");
        runner.Check(TotalSamples(accumulator) == 4096, "accumulator counts every sample");
        runner.Near(resolved[0], expected, 0.02f, "accumulated reference matches the analytic mean");
    }

    // --- Direct light and shadow --------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 5.0f, 0.0f, 0, 0);
        RaytracingScene unshadowed;
        std::string error;
        unshadowed.SetGeometry(triangles, &error);

        PathMaterial floor;
        floor.kind = PathMaterialKind::Diffuse;
        floor.albedo = doom3::rhi::LinearRgb{0.6f, 0.5f, 0.4f};
        std::vector<PathMaterial> materials{floor};

        PointLight light;
        light.position = dxr_test::V(0.0f, 0.0f, 3.0f);
        light.radiance = doom3::rhi::LinearRgb{4.0f, 4.0f, 4.0f};

        PathTraceSettings settings;
        settings.maxDepth = 2;
        settings.nextEventEstimation = true;
        settings.environment.enabled = false;

        const RtRay ray{dxr_test::V(0.0f, -3.0f, 2.0f), dxr_test::V(0.0f, 0.8320503f, -0.5547002f)};
        PathTraceSample sample;
        PathTraceCounters counters;
        runner.Check(TraceRadiance(unshadowed, materials, ray, &light, 1, settings, 0x55u, &sample,
                                   &counters, &error) == Status::Ok,
                     "unshadowed direct-light trace succeeds");
        runner.Near(sample.firstHitDistance, 3.0f / 0.8320503f, 1.0e-3f,
                    "primary hit is the floor at the expected distance");
        const float scaled = 4.0f / 9.0f / kPi;
        runner.Near(sample.radiance.r, 0.6f * scaled, 1.0e-4f, "direct light red channel");
        runner.Near(sample.radiance.g, 0.5f * scaled, 1.0e-4f, "direct light green channel");
        runner.Near(sample.radiance.b, 0.4f * scaled, 1.0e-4f, "direct light blue channel");
        runner.Check(counters.shadowRays == 1, "one shadow ray is traced");
        runner.Check(counters.shadowOcclusions == 0, "the unshadowed light is not occluded");

        std::vector<doom3::rhi::RtTriangle> shadowedTriangles = triangles;
        dxr_test::AddQuad(&shadowedTriangles, dxr_test::V(-0.5f, -0.5f, 1.5f),
                          dxr_test::V(0.5f, -0.5f, 1.5f), dxr_test::V(0.5f, 0.5f, 1.5f),
                          dxr_test::V(-0.5f, 0.5f, 1.5f), 1, 99);
        RaytracingScene shadowed;
        shadowed.SetGeometry(shadowedTriangles, &error);
        std::vector<PathMaterial> shadowedMaterials{floor, floor};
        PathTraceSample shadowedSample;
        PathTraceCounters shadowedCounters;
        runner.Check(TraceRadiance(shadowed, shadowedMaterials, ray, &light, 1, settings, 0x55u,
                                   &shadowedSample, &shadowedCounters, &error) == Status::Ok,
                     "shadowed direct-light trace succeeds");
        runner.Near(shadowedSample.radiance.r, 0.0f, 1.0e-6f,
                    "an occluder removes the direct term");
        runner.Check(shadowedCounters.shadowOcclusions == 1,
                     "the shadow ray is reported as occluded");
    }

    // --- RenderReference depth ----------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 10.0f, 0.0f, 0, 0);
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);
        std::vector<PathMaterial> materials{Diffuse(0.5f)};

        PathCamera camera;
        camera.position = dxr_test::V(0.0f, 0.0f, 2.0f);
        camera.width = 2;
        camera.height = 2;
        camera.tanHalfFovX = 0.5f;
        camera.tanHalfFovY = 0.5f;

        PathTraceSettings settings;
        settings.maxDepth = 2;
        float rgb[2 * 2 * 3] = {};
        float depth[2 * 2] = {};
        runner.Check(RenderReference(scene, materials, camera, nullptr, 0, settings, 0x9u, 1, rgb,
                                     depth, &error) == Status::Ok,
                     "RenderReference succeeds");
        bool allHit = true;
        for (float value : depth) {
            if (!(value > 1.9f && value < 2.5f)) {
                allHit = false;
            }
        }
        runner.Check(allHit, "every floor pixel reports a primary hit distance");

        RaytracingScene empty;
        std::vector<doom3::rhi::RtTriangle> none;
        empty.SetGeometry(none, &error);
        // One depth value per pixel: the camera is 2x2.
        float emptyDepth[2 * 2] = {};
        runner.Check(RenderReference(empty, materials, camera, nullptr, 0, settings, 0x9u, 1, rgb,
                                     emptyDepth, &error) == Status::Ok,
                     "an empty scene renders a miss");
        runner.Check(emptyDepth[0] < 0.0f && emptyDepth[3] < 0.0f,
                     "a missed primary ray reports a negative depth");
    }

    // --- Invalid inputs -----------------------------------------------------
    {
        std::vector<doom3::rhi::RtTriangle> triangles;
        dxr_test::AddFloor(&triangles, 5.0f, 0.0f, 0, 0);
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);
        std::vector<PathMaterial> materials{Diffuse(0.5f)};
        PathTraceSettings settings;
        settings.maxDepth = 0;
        PathTraceSample sample;
        const RtRay ray{dxr_test::V(0.0f, 0.0f, 2.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(TraceRadiance(scene, materials, ray, nullptr, 0, settings, 1, &sample, nullptr,
                                   &error) == Status::InvalidArgument,
                     "maxDepth zero is an InvalidArgument");
        settings.maxDepth = 2;
        runner.Check(TraceRadiance(scene, materials, ray, nullptr, 0, settings, 1, nullptr, nullptr,
                                   &error) == Status::InvalidArgument,
                     "a null sample is an InvalidArgument");

        std::vector<doom3::rhi::RtTriangle> badMaterial = {dxr_test::MakeTriangle(
            dxr_test::V(0.0f, 0.0f, 0.0f), dxr_test::V(1.0f, 0.0f, 0.0f),
            dxr_test::V(0.0f, 1.0f, 0.0f), dxr_test::UV(0, 0), dxr_test::UV(1, 0),
            dxr_test::UV(0, 1), 7, 1)};
        RaytracingScene badScene;
        badScene.SetGeometry(badMaterial, &error);
        runner.Check(TraceRadiance(badScene, materials, ray, nullptr, 0, settings, 1, &sample,
                                   nullptr, &error) == Status::Error,
                     "an out-of-range material index is an Error");
    }

    return runner.Finish("q6_pathtrace_static_test");
}
