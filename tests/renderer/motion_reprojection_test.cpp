// T042 consumer test: analytic camera/rigid reprojection (motion-fixtures.md
// rows CAM-01..07 and RIG-01..04).
//
// Public seam only. No GPU, engine or game data is used. A valid zero is
// distinct from an invalid result, and the history path ignores raster jitter.

#include "motion_test_support.h"

#include <cstdint>

namespace {

using motion_test::CameraAt;
using motion_test::HalfValue;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;
namespace rhi = doom3::rhi;

struct NumericRow {
    const char* label;
    rhi::CameraState previousCamera;
    rhi::CameraState currentCamera;
    rhi::MotionVec3 previousPoint;
    rhi::MotionVec3 currentPoint;
    double previousU;
    double previousV;
    double currentU;
    double currentV;
    double du;
    double dv;
    uint16_t wordU;
    uint16_t wordV;
    double toleranceU;
    double toleranceV;
};

void RunNumericRow(Runner* runner, const NumericRow& row) {
    rhi::MotionUv previousUv;
    rhi::MotionUv currentUv;
    const bool previousOk =
        rhi::ProjectHistoryPoint(row.previousCamera, row.previousPoint, &previousUv);
    const bool currentOk =
        rhi::ProjectHistoryPoint(row.currentCamera, row.currentPoint, &currentUv);
    runner->Check(previousOk, std::string(row.label) + " previous projects");
    runner->Check(currentOk, std::string(row.label) + " current projects");
    if (!previousOk || !currentOk) {
        return;
    }

    runner->Uv(std::string(row.label) + " previous uv", previousUv,
               row.previousU, row.previousV, row.toleranceU > row.toleranceV
                                                  ? row.toleranceU
                                                  : row.toleranceV);
    runner->Uv(std::string(row.label) + " current uv", currentUv, row.currentU,
               row.currentV, row.toleranceU > row.toleranceV ? row.toleranceU
                                                             : row.toleranceV);

    const rhi::MotionSample sample =
        rhi::ReprojectPoint(row.previousCamera, Record(row.previousPoint),
                            row.currentCamera, Record(row.currentPoint));
    runner->Validity(std::string(row.label) + " validity", sample.validity,
                     rhi::MotionValidity::Valid);
    runner->Check(sample.HasVector(), std::string(row.label) + " vector usable");
    runner->Near(std::string(row.label) + " du", sample.vector.du, row.du,
                 row.toleranceU);
    runner->Near(std::string(row.label) + " dv", sample.vector.dv, row.dv,
                 row.toleranceV);

    // The published word is a 16-bit pattern; one word of rounding slack.
    runner->HalfWord(std::string(row.label) + " du word", sample.vector.du,
                     row.wordU);
    runner->HalfWord(std::string(row.label) + " dv word", sample.vector.dv,
                     row.wordV);

    // Decode the expected word independently and re-check the value. This
    // catches an encoder that agrees with the wrong published number.
    runner->Near(std::string(row.label) + " du decoded",
                 HalfValue(row.wordU), row.du, row.toleranceU);
    runner->Near(std::string(row.label) + " dv decoded",
                 HalfValue(row.wordV), row.dv, row.toleranceV);
}

void RunCameraRows(Runner* runner) {
    const rhi::CameraState identity = CameraAt(0.0, 0.0, 0.0);

    {
        NumericRow row{"CAM-01", CameraAt(0.0, 2.0, 0.0), identity,
                       P(100.0, 0.0, 0.0), P(100.0, 0.0, 0.0),
                       0.5075, 0.5, 0.5, 0.5, 0.0075, 0.0, 0x1fae, 0x0000,
                       3.81e-06, 1.00e-07};
        RunNumericRow(runner, row);
    }
    {
        NumericRow row{"CAM-02", CameraAt(0.0, 0.0, 0.0, 2.0), identity,
                       P(100.0, 0.0, 0.0), P(100.0, 0.0, 0.0),
                       0.513095289, 0.5, 0.5, 0.5, 0.013095289, 0.0, 0x22b4,
                       0x0000, 7.63e-06, 1.00e-07};
        RunNumericRow(runner, row);
    }
    {
        NumericRow row{"CAM-03", identity, identity, P(100.0, 0.0, 1.0),
                       P(100.0, 0.0, 0.0), 0.5, 0.493333333, 0.5, 0.5, 0.0,
                       -0.006666667, 0x0000, 0x9ed4, 1.00e-07, 3.81e-06};
        RunNumericRow(runner, row);
    }
    {
        NumericRow row{"CAM-04", identity, identity, P(100.0, 0.0, 0.0),
                       P(100.0, 0.0, 0.0), 0.5, 0.5, 0.5, 0.5, 0.0, 0.0,
                       0x0000, 0x0000, 1.00e-07, 1.00e-07};
        RunNumericRow(runner, row);
    }
}

void RunJitterExclusion(Runner* runner) {
    rhi::CameraState previousCamera = CameraAt(0.0, 0.0, 0.0);
    previousCamera.jitterPixelsX = 0.7;
    previousCamera.jitterPixelsY = 0.3;
    rhi::CameraState currentCamera = CameraAt(0.0, 0.0, 0.0);
    currentCamera.jitterPixelsX = 0.1;
    currentCamera.jitterPixelsY = 0.9;
    const rhi::MotionVec3 point = P(100.0, 0.0, 0.0);

    const rhi::MotionSample sample =
        rhi::ReprojectPoint(previousCamera, Record(point), currentCamera,
                            Record(point));
    runner->Validity("CAM-05 validity", sample.validity,
                     rhi::MotionValidity::Valid);
    runner->Near("CAM-05 du", sample.vector.du, 0.0, 1.00e-07);
    runner->Near("CAM-05 dv", sample.vector.dv, 0.0, 1.00e-07);
    runner->HalfWord("CAM-05 du word", sample.vector.du, 0x0000);
    runner->HalfWord("CAM-05 dv word", sample.vector.dv, 0x0000);

    // The raster path does carry jitter. This proves the two paths are not
    // accidentally the same, so the zero above is an exclusion, not a no-op.
    rhi::MotionUv previousRaster;
    rhi::MotionUv currentRaster;
    runner->Check(rhi::ProjectRasterPoint(previousCamera, point, &previousRaster),
                  "CAM-05 previous raster projects");
    runner->Check(rhi::ProjectRasterPoint(currentCamera, point, &currentRaster),
                  "CAM-05 current raster projects");
    runner->Uv("CAM-05 previous raster", previousRaster, 0.499453125,
               0.5004166666666666, 1.0e-12);
    runner->Uv("CAM-05 current raster", currentRaster, 0.499921875, 0.50125,
               1.0e-12);
}

void RunBehindCameraRows(Runner* runner) {
    const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(-10.0, 0.0, 0.0)), camera,
            Record(P(-10.0, 0.0, 0.0)));
        runner->Validity("CAM-06 validity", sample.validity,
                         rhi::MotionValidity::BehindCamera);
        runner->Check(!sample.HasVector(),
                      "CAM-06 invalid carries no vector");
    }
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(-10.0, 0.0, 0.0)), camera,
            Record(P(100.0, 0.0, 0.0)));
        runner->Validity("CAM-07 validity", sample.validity,
                         rhi::MotionValidity::BehindCamera);
        runner->Check(!sample.HasVector(),
                      "CAM-07 invalid carries no vector");
    }
}

void RunRigidRows(Runner* runner) {
    const rhi::CameraState identity = CameraAt(0.0, 0.0, 0.0);
    {
        NumericRow row{"RIG-01", identity, identity, P(100.0, 0.5, 0.0),
                       P(100.0, 0.0, 0.0), 0.498125, 0.5, 0.5, 0.5,
                       -0.001875, 0.0, 0x97ae, 0x0000, 9.54e-07, 1.00e-07};
        RunNumericRow(runner, row);
    }
    {
        const double radians = 1.0 * motion_test::kPi / 180.0;
        NumericRow row{"RIG-02", identity, identity,
                       P(100.0 * std::cos(radians), 100.0 * std::sin(radians),
                         0.0),
                       P(100.0, 0.0, 0.0), 0.493454351, 0.5, 0.5, 0.5,
                       -0.006545649, 0.0, 0x9eb4, 0x0000, 3.81e-06, 1.00e-07};
        RunNumericRow(runner, row);
    }
    {
        NumericRow row{"RIG-03", identity, identity, P(101.0, 1.0, 0.0),
                       P(100.0, 1.0, 0.0), 0.496287129, 0.5, 0.49625, 0.5,
                       0.000037129, 0.0, 0x026f, 0x0000, 1.00e-07, 1.00e-07};
        RunNumericRow(runner, row);
    }
    {
        NumericRow row{"RIG-04", identity, identity, P(101.0, 0.0, 0.0),
                       P(100.0, 0.0, 0.0), 0.5, 0.5, 0.5, 0.5, 0.0, 0.0,
                       0x0000, 0x0000, 1.00e-07, 1.00e-07};
        RunNumericRow(runner, row);
    }
}

}  // namespace

int main() {
    Runner runner;
    RunCameraRows(&runner);
    RunJitterExclusion(&runner);
    RunBehindCameraRows(&runner);
    RunRigidRows(&runner);
    return runner.Finish("q3-motion-reprojection");
}
