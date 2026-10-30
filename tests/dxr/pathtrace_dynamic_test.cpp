// Q6 moving rigid and skinned geometry (spec 004 T066).
//
// The checks are geometric: after a refit, the old position must not keep a
// stale hit and every triangle must still be reachable; a rigid placement moves
// the hit without rebuilding; a topology change is refused instead of guessed;
// an invalid pose leaves the previous geometry intact.

#include "dxr_test_support.h"

#include "rhi/raytracing_scene.h"

#include <cmath>
#include <string>
#include <vector>

using doom3::rhi::RtAffine;
using doom3::rhi::RtAffineRotationZ;
using doom3::rhi::RtAffineScale;
using doom3::rhi::RtAffineTranslation;
using doom3::rhi::RtHit;
using doom3::rhi::RtRay;
using doom3::rhi::RtTriangle;
using doom3::rhi::RaytracingScene;

namespace {

std::vector<RtTriangle> Grid() {
    std::vector<RtTriangle> triangles;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const float x = static_cast<float>(column);
            const float y = static_cast<float>(row);
            dxr_test::AddQuad(&triangles, dxr_test::V(x, y, 0.0f),
                              dxr_test::V(x + 1.0f, y, 0.0f),
                              dxr_test::V(x + 1.0f, y + 1.0f, 0.0f),
                              dxr_test::V(x, y + 1.0f, 0.0f), 0,
                              static_cast<uint32_t>(row * 4 + column));
        }
    }
    return triangles;
}

std::vector<RtTriangle> DeformedGrid() {
    std::vector<RtTriangle> triangles = Grid();
    for (RtTriangle& triangle : triangles) {
        triangle.v0.z = 0.1f * std::sin(triangle.v0.x * 2.0f);
        triangle.v1.z = 0.1f * std::sin(triangle.v1.x * 2.0f);
        triangle.v2.z = 0.1f * std::sin(triangle.v2.x * 2.0f);
    }
    return triangles;
}

bool RayHitsTriangle(const RaytracingScene& scene, uint32_t triangleIndex, RtHit* hit) {
    const RtTriangle& triangle = scene.triangles()[triangleIndex];
    const doom3::rhi::RtVec3 centroid{
        (triangle.v0.x + triangle.v1.x + triangle.v2.x) / 3.0f,
        (triangle.v0.y + triangle.v1.y + triangle.v2.y) / 3.0f,
        (triangle.v0.z + triangle.v1.z + triangle.v2.z) / 3.0f};
    const RtRay ray{doom3::rhi::RtAdd(centroid, dxr_test::V(0.0f, 0.0f, 5.0f)),
                    dxr_test::V(0.0f, 0.0f, -1.0f)};
    return scene.Intersect(ray, 0.0f, 100.0f, hit) && hit->triangle == triangleIndex;
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    // --- Refit a moved triangle ---------------------------------------------
    {
        std::vector<RtTriangle> triangles;
        triangles.push_back(dxr_test::MakeTriangle(
            dxr_test::V(0.0f, 0.0f, 0.0f), dxr_test::V(1.0f, 0.0f, 0.0f),
            dxr_test::V(0.0f, 1.0f, 0.0f), dxr_test::UV(0, 0), dxr_test::UV(1, 0),
            dxr_test::UV(0, 1), 0, 1));
        RaytracingScene scene;
        std::string error;
        runner.Check(scene.SetGeometry(triangles, &error), "single triangle builds");

        RtHit hit;
        const RtRay oldRay{dxr_test::V(0.2f, 0.2f, 1.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(scene.Intersect(oldRay, 0.0f, 10.0f, &hit), "triangle starts at the old place");

        std::vector<RtTriangle> moved = triangles;
        moved[0].v0.x += 100.0f;
        moved[0].v1.x += 100.0f;
        moved[0].v2.x += 100.0f;
        runner.Check(scene.RefitGeometry(moved, &error), "a rigid move refits");
        runner.Check(!scene.Intersect(oldRay, 0.0f, 10.0f, &hit),
                     "the old position no longer reports a stale hit");
        const RtRay newRay{dxr_test::V(100.2f, 0.2f, 1.0f), dxr_test::V(0.0f, 0.0f, -1.0f)};
        runner.Check(scene.Intersect(newRay, 0.0f, 10.0f, &hit),
                     "the moved triangle is reachable");
        runner.Near(hit.position.x, 100.2f, 1.0e-4f, "the moved hit reports the new position");
    }

    // --- Small deformation keeps every triangle reachable -------------------
    {
        RaytracingScene scene;
        std::string error;
        runner.Check(scene.SetGeometry(Grid(), &error), "grid builds for refit");
        const uint32_t triangleCount = scene.triangleCount();
        const uint32_t nodeCount = scene.nodeCount();

        runner.Check(scene.RefitGeometry(DeformedGrid(), &error), "a pose refit succeeds");
        runner.Check(scene.triangleCount() == triangleCount, "refit keeps the triangle count");
        runner.Check(scene.nodeCount() == nodeCount, "refit keeps the hierarchy shape");

        uint32_t reachable = 0;
        for (uint32_t index = 0; index < triangleCount; ++index) {
            RtHit hit;
            if (RayHitsTriangle(scene, index, &hit)) {
                ++reachable;
            }
        }
        runner.Check(reachable == triangleCount,
                     "every triangle is reachable after a pose refit");
    }

    // --- Topology change is refused -----------------------------------------
    {
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(Grid(), &error);
        std::vector<RtTriangle> fewer = Grid();
        fewer.pop_back();
        runner.Check(!scene.RefitGeometry(fewer, &error),
                     "a changed triangle count is refused by refit");
        runner.Check(!error.empty(), "the topology refusal explains itself");
        RtHit hit;
        runner.Check(scene.Intersect(RtRay{dxr_test::V(0.5f, 0.5f, 2.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "a refused refit leaves the previous geometry usable");

        std::vector<RtTriangle> rebuilt = Grid();
        rebuilt.push_back(rebuilt[0]);
        runner.Check(scene.SetGeometry(rebuilt, &error),
                     "a topology change is handled by a rebuild");
        runner.Check(scene.triangleCount() == rebuilt.size(), "the rebuild records the new count");
    }

    // --- Invalid pose leaves the geometry intact ----------------------------
    {
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(Grid(), &error);
        std::vector<RtTriangle> invalid = Grid();
        invalid[3].v1.x = std::nanf("");
        runner.Check(!scene.RefitGeometry(invalid, &error),
                     "a non-finite pose is refused");
        RtHit hit;
        runner.Check(scene.Intersect(RtRay{dxr_test::V(0.5f, 0.5f, 2.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "an invalid refit does not damage the scene");

        std::vector<RtTriangle> degenerate = Grid();
        degenerate[0].v1 = degenerate[0].v0;
        degenerate[0].v2 = degenerate[0].v0;
        runner.Check(!scene.RefitGeometry(degenerate, &error),
                     "a degenerate pose is refused");
    }

    // --- Rigid instance placement -------------------------------------------
    {
        std::vector<RtTriangle> triangles;
        triangles.push_back(dxr_test::MakeTriangle(
            dxr_test::V(0.0f, 0.0f, 0.0f), dxr_test::V(1.0f, 0.0f, 0.0f),
            dxr_test::V(0.0f, 1.0f, 0.0f), dxr_test::UV(0, 0), dxr_test::UV(1, 0),
            dxr_test::UV(0, 1), 0, 1));
        RaytracingScene scene;
        std::string error;
        scene.SetGeometry(triangles, &error);

        runner.Check(scene.SetInstanceTransform(RtAffineTranslation(dxr_test::V(10.0f, 0.0f, 0.0f)),
                                                &error),
                     "a translation placement is accepted");
        RtHit hit;
        runner.Check(!scene.Intersect(RtRay{dxr_test::V(0.2f, 0.2f, 1.0f),
                                            dxr_test::V(0.0f, 0.0f, -1.0f)},
                                      0.0f, 10.0f, &hit),
                     "the object-space position misses under the placement");
        runner.Check(scene.Intersect(RtRay{dxr_test::V(10.2f, 0.2f, 1.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "the placed triangle is hit");
        runner.Near(hit.position.x, 10.2f, 1.0e-4f, "the placed hit is in world space");
        runner.Near(hit.normal.z, 1.0f, 1.0e-4f, "the placed normal is in world space");

        runner.Check(scene.SetInstanceTransform(RtAffineScale(dxr_test::V(2.0f, 2.0f, 2.0f)),
                                                &error),
                     "a scale placement is accepted");
        runner.Check(scene.Intersect(RtRay{dxr_test::V(1.5f, 0.1f, 1.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "the scaled triangle is hit");
        runner.Near(hit.position.x, 1.5f, 1.0e-4f, "the scaled hit x is correct");
        runner.Near(hit.position.y, 0.1f, 1.0e-4f, "the scaled hit y is correct");

        // The unit triangle rotates into the second quadrant:
        // (0,0), (0,1), (-1,0). (-0.2, 0.5) is inside it.
        runner.Check(scene.SetInstanceTransform(RtAffineRotationZ(1.57079632679f), &error),
                     "a rotation placement is accepted");
        runner.Check(scene.Intersect(RtRay{dxr_test::V(-0.2f, 0.5f, 1.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "the rotated triangle is hit");
        runner.Near(hit.position.x, -0.2f, 1.0e-4f, "the rotated hit x is correct");
        runner.Near(hit.position.y, 0.5f, 1.0e-4f, "the rotated hit y is correct");

        RtAffine singular;
        singular.m[0][0] = 0.0f;
        singular.m[1][1] = 0.0f;
        singular.m[2][2] = 0.0f;
        runner.Check(!scene.SetInstanceTransform(singular, &error),
                     "a singular placement is refused");
        runner.Check(scene.Intersect(RtRay{dxr_test::V(-0.2f, 0.5f, 1.0f),
                                           dxr_test::V(0.0f, 0.0f, -1.0f)},
                                     0.0f, 10.0f, &hit),
                     "a refused placement keeps the previous one");
    }

    // --- Identity and empty scenes ------------------------------------------
    {
        RaytracingScene scene;
        std::string error;
        std::vector<RtTriangle> none;
        runner.Check(scene.SetGeometry(none, &error), "an empty scene is valid");
        runner.Check(scene.triangleCount() == 0, "an empty scene has no triangles");
        RtHit hit;
        runner.Check(!scene.Intersect(RtRay{dxr_test::V(0.0f, 0.0f, 0.0f),
                                            dxr_test::V(0.0f, 0.0f, -1.0f)},
                                      0.0f, 10.0f, &hit),
                     "an empty scene misses");
        runner.Check(scene.RefitGeometry(none, &error), "an empty scene refits trivially");
    }

    return runner.Finish("q6_pathtrace_dynamic_test");
}
