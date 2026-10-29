// T043 consumer test: rendered-frame history and main/mirror/camera/shadow
// isolation (motion-fixtures.md section 7, rows SEQ-01..03).
//
// The numeric rows are re-derived from the merged T042 seam and pass; the
// history store is the T043 production slice named in tasks.md
// (src/renderer/rhi/view_history.cpp). That slice is absent from this branch,
// so the store assertions are RED by design and the seam comes from
// tests/renderer/q3_view_history_seam.h plus a build-only stub.
//
// The store contract this test pins:
//  * previous state is the last ACTUALLY RENDERED state, so an interval with
//    no render contributes nothing (the simulation tick is never committed);
//  * a mirror/camera/shadow view never replaces the main-view record, and a
//    subview that was never rendered has no history of its own;
//  * two views with different stable instance identities never share a record;
//  * a changed view generation is an explicit reason, not silent reuse.

#include "motion_test_support.h"
#include "q3_view_history_seam.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using motion_test::CameraAt;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;
namespace rhi = doom3::rhi;

constexpr double kTolU = 1.00e-07;
constexpr double kTolVWord = 1.91e-06;
constexpr double kTolVTwoSteps = 3.81e-06;

bool SamePoint(const rhi::MotionVec3& a, const rhi::MotionVec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

rhi::ViewIdentity MainIdentity(uint32_t instance = 1) {
    rhi::ViewIdentity identity;
    identity.worldId = 1;
    identity.instanceId = instance;
    identity.generation = 1;
    identity.topology = 1;
    identity.entity = 1;
    identity.provider = 1;
    identity.width = 1280;
    identity.height = 720;
    return identity;
}

// One rendered view state at depth 100 with the point raised by `z`. The
// camera is the engine 16:9 pair from motion_test::CameraAt.
rhi::RenderedViewState StateAt(double z) {
    rhi::RenderedViewState state;
    state.camera = CameraAt(0.0, 0.0, 0.0);
    state.records.push_back(Record(P(100.0, 0.0, z)));
    return state;
}

rhi::RenderedViewState StateWithoutRecords(double z) {
    rhi::RenderedViewState state;
    state.camera = CameraAt(0.0, 0.0, 0.0);
    (void)z;
    return state;
}

void RunNumericBaseline(Runner* runner) {
    // These three rows use only the merged T042 seam. They show the signal the
    // history store must feed: the rendered previous pose and the last-tick
    // pose differ by 0.003333333 in dv, far outside the predeclared tolerance.
    const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);
    const rhi::MotionVec3 previousRendered = P(100.0, 0.0, 0.5);
    const rhi::MotionVec3 previousTaggedTick = P(100.0, 0.0, 0.0);
    const rhi::MotionVec3 current = P(100.0, 0.0, 1.0);

    const rhi::MotionSample seq01 = rhi::ReprojectPoint(
        camera, Record(previousRendered), camera, Record(current));
    runner->Validity("SEQ-01 baseline validity", seq01.validity,
                     rhi::MotionValidity::Valid);
    runner->Near("SEQ-01 baseline du", seq01.vector.du, 0.0, kTolU);
    runner->Near("SEQ-01 baseline dv", seq01.vector.dv, 0.003333333, kTolVWord);

    const rhi::MotionSample seq02 = rhi::ReprojectPoint(
        camera, Record(previousTaggedTick), camera, Record(current));
    runner->Near("SEQ-02 baseline dv", seq02.vector.dv, 0.006666667,
                 kTolVTwoSteps);

    const rhi::MotionSample seq03 = rhi::ReprojectPoint(
        camera, Record(current), camera, Record(current));
    runner->Validity("SEQ-03 baseline validity", seq03.validity,
                     rhi::MotionValidity::Valid);
    runner->Near("SEQ-03 baseline dv", seq03.vector.dv, 0.0, kTolU);
}

void RunRenderedFrameSequence(Runner* runner) {
    rhi::RenderedViewHistory history;
    const rhi::ViewIdentity identity = MainIdentity();
    rhi::RenderedViewState previous;

    // First render: nothing to reproject against yet.
    runner->Validity("T043 first render has no history",
                     history.Previous(rhi::ViewKind::Main, identity, &previous),
                     rhi::MotionValidity::NoHistory);

    // Render A at simulation tick 1.
    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.0)),
                  "T043 first rendered commit accepted");
    const rhi::MotionValidity afterFirst =
        history.Previous(rhi::ViewKind::Main, identity, &previous);
    runner->Validity("T043 previous after first render", afterFirst,
                     rhi::MotionValidity::Valid);
    if (afterFirst == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 0.0)),
                      "T043 previous is the rendered pose, not the tick pose");

        // Tick 2 raised the point to z=0.5 but was never rendered, so it must
        // not become history. The reprojection must use A (0.0), giving
        // SEQ-02 rather than SEQ-01.
        const rhi::MotionSample fromHistory = rhi::ReprojectPoint(
            previous.camera, previous.records[0], CameraAt(0.0, 0.0, 0.0),
            Record(P(100.0, 0.0, 1.0)));
        runner->Validity("T043 unrendered tick keeps history valid",
                         fromHistory.validity, rhi::MotionValidity::Valid);
        runner->Near("T043 unrendered tick is not the previous pose",
                     fromHistory.vector.dv, 0.006666667, kTolVTwoSteps);
    } else {
        runner->Check(false, "T043 previous record retained",
                      "expected one rendered record");
        runner->Check(false, "T043 unrendered tick keeps history valid",
                      "no usable previous state");
        runner->Check(false, "T043 unrendered tick is not the previous pose",
                      "no usable previous state");
    }

    // Render C: the same pose rendered twice must reproject to a valid zero.
    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(1.0)),
                  "T043 second rendered commit accepted");
    const rhi::MotionValidity afterSecond =
        history.Previous(rhi::ViewKind::Main, identity, &previous);
    runner->Validity("T043 previous after second render", afterSecond,
                     rhi::MotionValidity::Valid);
    if (afterSecond == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        const rhi::MotionSample samePose = rhi::ReprojectPoint(
            previous.camera, previous.records[0], CameraAt(0.0, 0.0, 0.0),
            Record(P(100.0, 0.0, 1.0)));
        runner->Near("T043 same pose rendered twice is a valid zero",
                     samePose.vector.dv, 0.0, kTolU);
    } else {
        runner->Check(false, "T043 second rendered record retained",
                      "expected one rendered record");
    }
}

void RunSeq01ThroughHistory(Runner* runner) {
    rhi::RenderedViewHistory history;
    const rhi::ViewIdentity identity = MainIdentity();
    rhi::RenderedViewState previous;

    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.5)),
                  "SEQ-01 rendered commit accepted");
    const rhi::MotionValidity validity =
        history.Previous(rhi::ViewKind::Main, identity, &previous);
    runner->Validity("SEQ-01 history validity", validity,
                     rhi::MotionValidity::Valid);
    if (validity == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            previous.camera, previous.records[0], CameraAt(0.0, 0.0, 0.0),
            Record(P(100.0, 0.0, 1.0)));
        runner->Near("SEQ-01 history du", sample.vector.du, 0.0, kTolU);
        runner->Near("SEQ-01 history dv", sample.vector.dv, 0.003333333,
                     kTolVWord);
    } else {
        runner->Check(false, "SEQ-01 history record retained");
    }
}

void RunViewIsolation(Runner* runner) {
    rhi::RenderedViewHistory history;
    const rhi::ViewIdentity identity = MainIdentity();
    rhi::RenderedViewState previous;

    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.0)),
                  "T043 main render committed");
    runner->Check(history.CommitRendered(rhi::ViewKind::Mirror, identity,
                                         StateAt(0.25)),
                  "T043 mirror render committed");
    runner->Check(history.CommitRendered(rhi::ViewKind::Camera, identity,
                                         StateAt(0.5)),
                  "T043 camera render committed");
    runner->Check(history.CommitRendered(rhi::ViewKind::Shadow, identity,
                                         StateAt(0.75)),
                  "T043 shadow render committed");

    struct ViewRow {
        const char* label;
        rhi::ViewKind kind;
        double z;
    };
    const ViewRow rows[] = {
        {"main history survives subviews", rhi::ViewKind::Main, 0.0},
        {"mirror keeps its own history", rhi::ViewKind::Mirror, 0.25},
        {"camera keeps its own history", rhi::ViewKind::Camera, 0.5},
        {"shadow keeps its own history", rhi::ViewKind::Shadow, 0.75},
    };
    for (const ViewRow& row : rows) {
        const rhi::MotionValidity validity =
            history.Previous(row.kind, identity, &previous);
        runner->Validity(row.label, validity, rhi::MotionValidity::Valid);
        if (validity == rhi::MotionValidity::Valid && previous.records.size() == 1) {
            runner->Check(SamePoint(previous.records[0].point,
                                    P(100.0, 0.0, row.z)),
                          std::string(row.label) + " point");
        } else {
            runner->Check(false, std::string(row.label) + " point",
                          "expected one retained record");
        }
    }

    // A view that was never rendered has no history, even when a subview was.
    rhi::RenderedViewHistory subviewOnly;
    runner->Check(subviewOnly.CommitRendered(rhi::ViewKind::Mirror, identity,
                                             StateAt(0.25)),
                  "T043 mirror-only commit accepted");
    runner->Validity("T043 subview commit does not create main history",
                     subviewOnly.Previous(rhi::ViewKind::Main, identity,
                                          &previous),
                     rhi::MotionValidity::NoHistory);
    runner->Validity("T043 shadow never rendered",
                     subviewOnly.Previous(rhi::ViewKind::Shadow, identity,
                                          &previous),
                     rhi::MotionValidity::NoHistory);
}

void RunInstanceIdentity(Runner* runner) {
    rhi::RenderedViewHistory history;
    const rhi::ViewIdentity first = MainIdentity(7);
    const rhi::ViewIdentity second = MainIdentity(8);
    rhi::RenderedViewState previous;

    runner->Check(history.CommitRendered(rhi::ViewKind::Camera, first,
                                         StateAt(0.25)),
                  "T043 first camera instance committed");
    runner->Check(history.CommitRendered(rhi::ViewKind::Camera, second,
                                         StateAt(0.75)),
                  "T043 second camera instance committed");

    const rhi::MotionValidity firstValidity =
        history.Previous(rhi::ViewKind::Camera, first, &previous);
    runner->Validity("T043 first camera instance readable", firstValidity,
                     rhi::MotionValidity::Valid);
    if (firstValidity == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 0.25)),
                      "T043 first camera instance is not the second's record");
    } else {
        runner->Check(false, "T043 first camera instance record");
    }

    const rhi::MotionValidity secondValidity =
        history.Previous(rhi::ViewKind::Camera, second, &previous);
    runner->Validity("T043 second camera instance readable", secondValidity,
                     rhi::MotionValidity::Valid);
    if (secondValidity == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 0.75)),
                      "T043 second camera instance is not the first's record");
    } else {
        runner->Check(false, "T043 second camera instance record");
    }

    // A stable identity that was never committed is NoHistory, never a share.
    const rhi::ViewIdentity third = MainIdentity(9);
    runner->Validity("T043 unrendered instance has no history",
                     history.Previous(rhi::ViewKind::Camera, third, &previous),
                     rhi::MotionValidity::NoHistory);
}

void RunGenerationAndRejection(Runner* runner) {
    rhi::RenderedViewHistory history;
    const rhi::ViewIdentity identity = MainIdentity();
    rhi::RenderedViewState previous;

    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.5)),
                  "T043 generation commit accepted");

    rhi::ViewIdentity newer = identity;
    newer.generation = identity.generation + 1;
    runner->Validity("T043 generation change is invalid",
                     history.Previous(rhi::ViewKind::Main, newer, &previous),
                     rhi::MotionValidity::Generation);

    // A rejected lookup must not overwrite the caller's state with stale data.
    rhi::RenderedViewState sentinel;
    sentinel.records.push_back(Record(P(-1.0, -1.0, -1.0)));
    const rhi::RenderedViewState sentinelCopy = sentinel;
    const rhi::MotionValidity rejected =
        history.Previous(rhi::ViewKind::Main, newer, &sentinel);
    runner->Check(rejected == rhi::MotionValidity::Generation &&
                      sentinel.records.size() == sentinelCopy.records.size() &&
                      SamePoint(sentinel.records[0].point,
                                sentinelCopy.records[0].point),
                  "T043 rejected lookup leaves the output untouched");

    // An empty render commits nothing; the previous state is unchanged.
    runner->Check(!history.CommitRendered(rhi::ViewKind::Main, identity,
                                          StateWithoutRecords(1.0)),
                  "T043 empty render is rejected");
    const rhi::MotionValidity stillStored =
        history.Previous(rhi::ViewKind::Main, identity, &previous);
    runner->Validity("T043 rejected commit keeps the last rendered state",
                     stillStored, rhi::MotionValidity::Valid);
    if (stillStored == rhi::MotionValidity::Valid && previous.records.size() == 1) {
        runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 0.5)),
                      "T043 rejected commit did not replace the record");
    } else {
        runner->Check(false, "T043 rejected commit record retention");
    }
}

}  // namespace

int main() {
    Runner runner;
    RunNumericBaseline(&runner);
    RunRenderedFrameSequence(&runner);
    RunSeq01ThroughHistory(&runner);
    RunViewIsolation(&runner);
    RunInstanceIdentity(&runner);
    RunGenerationAndRejection(&runner);
    return runner.Finish("q3.view_history");
}
