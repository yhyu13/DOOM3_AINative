// T044 consumer test: private skinned previous pose (motion-fixtures.md rows
// SKN-01..03) plus the retention rules from section 7.
//
// The shared dynamic model is simulated: the source vector is overwritten and
// deleted after the commit. The previous pose must survive because it is a
// private copy, and a subview commit must not touch the main-view record.

#include "motion_test_support.h"

#include "rhi/skinned_history.h"

#include <cstdint>
#include <vector>

namespace {

using motion_test::CameraAt;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;
namespace rhi = doom3::rhi;

struct SkinnedRow {
    const char* label;
    rhi::MotionVec3 previousPosed;
    rhi::MotionVec3 currentPosed;
    double du;
    double dv;
    uint16_t wordU;
    uint16_t wordV;
    double toleranceU;
    double toleranceV;
};

bool SamePoint(const rhi::MotionVec3& a, const rhi::MotionVec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

void RunNumericRows(Runner* runner) {
    const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);
    const SkinnedRow rows[] = {
        {"SKN-01", P(100.0, 0.0, 1.0), P(100.0, 0.0, 0.5), 0.0, -0.003333333,
         0x0000, 0x9ad4, 1.00e-07, 1.91e-06},
        {"SKN-02", P(100.0, 0.0, 0.0), P(100.0, 0.0, 0.0), 0.0, 0.0, 0x0000,
         0x0000, 1.00e-07, 1.00e-07},
        {"SKN-03", P(100.0 * std::cos(2.0 * motion_test::kPi / 180.0),
                     100.0 * std::sin(2.0 * motion_test::kPi / 180.0), 1.0),
         P(100.0, 0.0, 1.0), -0.013095289, -0.000004064, 0xa2b4, 0x8044,
         7.63e-06, 1.00e-07},
    };

    for (const SkinnedRow& row : rows) {
        rhi::SkinnedPoseHistory history;
        const std::vector<rhi::MotionVec3> shared{row.previousPosed};
        const bool committed =
            history.CommitPose(rhi::ViewKind::Main, 41, 3, shared);
        runner->Check(committed, std::string(row.label) + " commit accepted");

        std::vector<rhi::MotionVec3> previous;
        const rhi::MotionValidity validity =
            history.PreviousPose(rhi::ViewKind::Main, 41, 3, &previous);
        runner->Validity(std::string(row.label) + " previous pose", validity,
                         rhi::MotionValidity::Valid);
        if (validity != rhi::MotionValidity::Valid || previous.size() != 1u) {
            continue;
        }
        runner->Check(SamePoint(previous[0], row.previousPosed),
                      std::string(row.label) + " previous point retained");

        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(previous[0]), camera, Record(row.currentPosed));
        runner->Validity(std::string(row.label) + " validity", sample.validity,
                         rhi::MotionValidity::Valid);
        runner->Near(std::string(row.label) + " du", sample.vector.du, row.du,
                     row.toleranceU);
        runner->Near(std::string(row.label) + " dv", sample.vector.dv, row.dv,
                     row.toleranceV);
        runner->HalfWord(std::string(row.label) + " du word", sample.vector.du,
                         row.wordU);
        runner->HalfWord(std::string(row.label) + " dv word", sample.vector.dv,
                         row.wordV);
    }
}

void RunSharedCacheLifetime(Runner* runner) {
    rhi::SkinnedPoseHistory history;
    std::vector<rhi::MotionVec3> shared{P(1.0, 2.0, 3.0), P(4.0, 5.0, 6.0)};
    const bool committed =
        history.CommitPose(rhi::ViewKind::Main, 7, 1, shared);
    runner->Check(committed, "shared-cache commit accepted");

    // Regenerate/overwrite the shared storage, then delete it. A private copy
    // must be unaffected; an alias would now hold the new or freed values.
    shared[0] = P(9.0, 9.0, 9.0);
    shared[1] = P(8.0, 8.0, 8.0);
    shared.clear();
    shared.shrink_to_fit();

    std::vector<rhi::MotionVec3> previous;
    const rhi::MotionValidity validity =
        history.PreviousPose(rhi::ViewKind::Main, 7, 1, &previous);
    runner->Validity("shared-cache previous pose", validity,
                     rhi::MotionValidity::Valid);
    if (validity == rhi::MotionValidity::Valid && previous.size() == 2u) {
        runner->Check(SamePoint(previous[0], P(1.0, 2.0, 3.0)) &&
                          SamePoint(previous[1], P(4.0, 5.0, 6.0)),
                      "shared-cache previous pose is the committed copy");
    } else {
        runner->Check(false, "shared-cache previous pose count",
                      "expected two retained vertices");
    }

    // Copy-on-read: mutating one caller's result must not change the record.
    std::vector<rhi::MotionVec3> second;
    if (history.PreviousPose(rhi::ViewKind::Main, 7, 1, &second) ==
        rhi::MotionValidity::Valid) {
        previous[0] = P(0.0, 0.0, 0.0);
        runner->Check(second.size() == 2u &&
                          SamePoint(second[0], P(1.0, 2.0, 3.0)),
                      "previous pose is copied per read");
    } else {
        runner->Check(false, "previous pose second read");
    }
}

void RunRegenerationAndDeletion(Runner* runner) {
    rhi::SkinnedPoseHistory history;

    // First render of surface 11: the shared posed array is owned elsewhere.
    std::vector<rhi::MotionVec3> shared{P(1.0, 2.0, 3.0), P(4.0, 5.0, 6.0)};
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 11, 1, shared),
                  "regeneration: first pose committed");

    // Regenerate the shared geometry: free the old array, then allocate and
    // fill a new one. The private copy must not alias either array.
    shared.clear();
    shared.shrink_to_fit();
    std::vector<rhi::MotionVec3> previous;
    runner->Validity("regeneration: previous pose survives source deletion",
                     history.PreviousPose(rhi::ViewKind::Main, 11, 1, &previous),
                     rhi::MotionValidity::Valid);
    if (previous.size() == 2u) {
        runner->Check(SamePoint(previous[0], P(1.0, 2.0, 3.0)) &&
                          SamePoint(previous[1], P(4.0, 5.0, 6.0)),
                      "regeneration: deleted source did not dangle");
    } else {
        runner->Check(false, "regeneration: deleted source pose count",
                      "expected two retained vertices");
    }

    // Regenerated geometry, same stable surface identity, new generation.
    std::vector<rhi::MotionVec3> regenerated{P(11.0, 12.0, 13.0)};
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 11, 2, regenerated),
                  "regeneration: regenerated pose committed");
    runner->Validity("regeneration: superseded generation is rejected",
                     history.PreviousPose(rhi::ViewKind::Main, 11, 1, &previous),
                     rhi::MotionValidity::Generation);
    if (history.PreviousPose(rhi::ViewKind::Main, 11, 2, &previous) ==
        rhi::MotionValidity::Valid) {
        runner->Check(previous.size() == 1u &&
                          SamePoint(previous[0], P(11.0, 12.0, 13.0)),
                      "regeneration: current generation is the new pose");
    } else {
        runner->Check(false, "regeneration: current generation readable");
    }

    // Delete the regenerated shared geometry; the new private copy survives.
    regenerated.clear();
    regenerated.shrink_to_fit();
    runner->Validity("deletion: regenerated pose survives source deletion",
                     history.PreviousPose(rhi::ViewKind::Main, 11, 2, &previous),
                     rhi::MotionValidity::Valid);

    // A subview regenerates with its own surface identity while the main view
    // advances; neither record may alias or replace the other.
    const std::vector<rhi::MotionVec3> mainPose{P(21.0, 22.0, 23.0)};
    const std::vector<rhi::MotionVec3> mirrorPose{P(31.0, 32.0, 33.0)};
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 11, 3, mainPose),
                  "regeneration: main generation 3 committed");
    runner->Check(history.CommitPose(rhi::ViewKind::Mirror, 11, 3, mirrorPose),
                  "regeneration: mirror generation 3 committed");
    const std::vector<rhi::MotionVec3> mainNewer{P(41.0, 42.0, 43.0)};
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 11, 4, mainNewer),
                  "regeneration: main generation 4 committed");
    if (history.PreviousPose(rhi::ViewKind::Mirror, 11, 3, &previous) ==
        rhi::MotionValidity::Valid) {
        runner->Check(previous.size() == 1u &&
                          SamePoint(previous[0], P(31.0, 32.0, 33.0)),
                      "regeneration: main commit does not replace the mirror");
    } else {
        runner->Check(false, "regeneration: mirror pose readable");
    }
    if (history.PreviousPose(rhi::ViewKind::Main, 11, 4, &previous) ==
        rhi::MotionValidity::Valid) {
        runner->Check(previous.size() == 1u &&
                          SamePoint(previous[0], P(41.0, 42.0, 43.0)),
                      "regeneration: main keeps its own newer pose");
    } else {
        runner->Check(false, "regeneration: main newer pose readable");
    }
    runner->Validity("regeneration: main generation 3 superseded",
                     history.PreviousPose(rhi::ViewKind::Main, 11, 3, &previous),
                     rhi::MotionValidity::Generation);

    history.Clear();
    runner->Check(!history.HasPose(rhi::ViewKind::Main, 11) &&
                      !history.HasPose(rhi::ViewKind::Mirror, 11),
                  "deletion: clear drops every view");
    runner->Validity("deletion: cleared view has no history",
                     history.PreviousPose(rhi::ViewKind::Main, 11, 4, &previous),
                     rhi::MotionValidity::NoHistory);
}

void RunSubviewIsolation(Runner* runner) {
    rhi::SkinnedPoseHistory history;
    const std::vector<rhi::MotionVec3> mainPose{P(1.0, 0.0, 0.0)};
    const std::vector<rhi::MotionVec3> mirrorPose{P(2.0, 0.0, 0.0)};
    const std::vector<rhi::MotionVec3> cameraPose{P(3.0, 0.0, 0.0)};
    const std::vector<rhi::MotionVec3> shadowPose{P(4.0, 0.0, 0.0)};

    runner->Check(history.CommitPose(rhi::ViewKind::Main, 5, 1, mainPose),
                  "main pose committed");
    runner->Check(history.CommitPose(rhi::ViewKind::Mirror, 5, 1, mirrorPose),
                  "mirror pose committed");
    runner->Check(history.CommitPose(rhi::ViewKind::Camera, 5, 1, cameraPose),
                  "camera pose committed");
    runner->Check(history.CommitPose(rhi::ViewKind::Shadow, 5, 1, shadowPose),
                  "shadow pose committed");

    std::vector<rhi::MotionVec3> stored;
    if (history.PreviousPose(rhi::ViewKind::Main, 5, 1, &stored) ==
            rhi::MotionValidity::Valid &&
        stored.size() == 1u) {
        runner->Check(SamePoint(stored[0], mainPose[0]),
                      "subviews do not overwrite the main pose");
    } else {
        runner->Check(false, "main pose still readable after subviews");
    }
    if (history.PreviousPose(rhi::ViewKind::Mirror, 5, 1, &stored) ==
            rhi::MotionValidity::Valid &&
        stored.size() == 1u) {
        runner->Check(SamePoint(stored[0], mirrorPose[0]),
                      "mirror keeps its own pose");
    } else {
        runner->Check(false, "mirror pose readable");
    }
    if (history.PreviousPose(rhi::ViewKind::Camera, 5, 1, &stored) ==
            rhi::MotionValidity::Valid &&
        stored.size() == 1u) {
        runner->Check(SamePoint(stored[0], cameraPose[0]),
                      "camera keeps its own pose");
    } else {
        runner->Check(false, "camera pose readable");
    }
    if (history.PreviousPose(rhi::ViewKind::Shadow, 5, 1, &stored) ==
            rhi::MotionValidity::Valid &&
        stored.size() == 1u) {
        runner->Check(SamePoint(stored[0], shadowPose[0]),
                      "shadow keeps its own pose");
    } else {
        runner->Check(false, "shadow pose readable");
    }
}

void RunGenerationAndRejection(Runner* runner) {
    rhi::SkinnedPoseHistory history;
    std::vector<rhi::MotionVec3> previous;

    runner->Validity("missing pose is no history",
                     history.PreviousPose(rhi::ViewKind::Main, 1, 1, &previous),
                     rhi::MotionValidity::NoHistory);

    const std::vector<rhi::MotionVec3> empty;
    runner->Check(!history.CommitPose(rhi::ViewKind::Main, 1, 1, empty),
                  "empty pose rejected");

    const std::vector<rhi::MotionVec3> oldPose{P(1.0, 1.0, 1.0)};
    const std::vector<rhi::MotionVec3> newPose{P(2.0, 2.0, 2.0),
                                               P(2.0, 2.0, 3.0)};
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 1, 4, oldPose),
                  "generation 4 committed");
    runner->Check(history.CommitPose(rhi::ViewKind::Main, 1, 5, newPose),
                  "generation 5 committed");
    runner->Validity("stale generation rejected",
                     history.PreviousPose(rhi::ViewKind::Main, 1, 4, &previous),
                     rhi::MotionValidity::Generation);
    if (history.PreviousPose(rhi::ViewKind::Main, 1, 5, &previous) ==
        rhi::MotionValidity::Valid) {
        runner->Check(previous.size() == 2u &&
                          SamePoint(previous[0], newPose[0]),
                      "current generation returns the newest pose");
    } else {
        runner->Check(false, "current generation readable");
    }

    history.Clear();
    runner->Check(!history.HasPose(rhi::ViewKind::Main, 1),
                  "clear drops the record");
}

}  // namespace

int main() {
    Runner runner;
    RunNumericRows(&runner);
    RunSharedCacheLifetime(&runner);
    RunRegenerationAndDeletion(&runner);
    RunSubviewIsolation(&runner);
    RunGenerationAndRejection(&runner);
    return runner.Finish("q3-skinned-history");
}
