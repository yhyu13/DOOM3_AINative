// T045 consumer test: invalidation on cut, teleport, level load, resize,
// topology, entity reuse and renderer/provider switch (motion-fixtures.md
// rows VAL-08..10, section 7).
//
// The invalidation contract is part of the T045 production slice in
// src/renderer/rhi/view_history.cpp; this test links it directly.
//
// The store contract this test pins:
//  * a rejected lookup reports one explicit reason and never a usable state;
//  * the caller's previous-state output is untouched on a rejection, so stale
//    history cannot be consumed as a valid zero;
//  * the reason is stable until the next commit, and a recommit with the new
//    identity restores Valid without resurrecting the old record;
//  * precedence is documented and observable: provider switch, then reset
//    (cut/teleport/level), resize, topology, entity reuse, view generation.

#include "motion_test_support.h"
#include "rhi/view_history.h"

#include <cstdint>
#include <string>

namespace {

using motion_test::CameraAt;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;
namespace rhi = doom3::rhi;

bool SamePoint(const rhi::MotionVec3& a, const rhi::MotionVec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

rhi::ViewIdentity BaseIdentity() {
    rhi::ViewIdentity identity;
    identity.worldId = 1;
    identity.instanceId = 1;
    identity.generation = 1;
    identity.topology = 1;
    identity.entity = 1;
    identity.provider = 1;
    identity.width = 1280;
    identity.height = 720;
    return identity;
}

rhi::RenderedViewState StateAt(double z) {
    rhi::RenderedViewState state;
    state.camera = CameraAt(0.0, 0.0, 0.0);
    state.records.push_back(Record(P(100.0, 0.0, z)));
    return state;
}

void Expect(Runner* runner, const std::string& label,
            const rhi::RenderedViewHistory& history, rhi::ViewKind kind,
            const rhi::ViewIdentity& identity, rhi::MotionValidity expected) {
    rhi::RenderedViewState previous;
    runner->Validity(label, history.Previous(kind, identity, &previous),
                     expected);
}

void CommitAllViews(Runner* runner, rhi::RenderedViewHistory* history,
                    const rhi::ViewIdentity& identity) {
    runner->Check(history->CommitRendered(rhi::ViewKind::Main, identity,
                                          StateAt(0.0)),
                  "T045 main commit accepted");
    runner->Check(history->CommitRendered(rhi::ViewKind::Mirror, identity,
                                          StateAt(0.25)),
                  "T045 mirror commit accepted");
    runner->Check(history->CommitRendered(rhi::ViewKind::Camera, identity,
                                          StateAt(0.5)),
                  "T045 camera commit accepted");
    runner->Check(history->CommitRendered(rhi::ViewKind::Shadow, identity,
                                          StateAt(0.75)),
                  "T045 shadow commit accepted");
}

void RunResetFamily(Runner* runner) {
    const rhi::ViewIdentity identity = BaseIdentity();

    // VAL-08: a cut invalidates every rendered view, not only the main one.
    {
        rhi::RenderedViewHistory history;
        CommitAllViews(runner, &history, identity);
        history.InvalidateAll(rhi::MotionValidity::Reset);
        Expect(runner, "T045 cut invalidates main", history,
               rhi::ViewKind::Main, identity, rhi::MotionValidity::Reset);
        Expect(runner, "T045 cut invalidates mirror", history,
               rhi::ViewKind::Mirror, identity, rhi::MotionValidity::Reset);
        Expect(runner, "T045 cut invalidates camera", history,
               rhi::ViewKind::Camera, identity, rhi::MotionValidity::Reset);
        Expect(runner, "T045 cut invalidates shadow", history,
               rhi::ViewKind::Shadow, identity, rhi::MotionValidity::Reset);

        // The reason persists until a recommit, and the recommit is usable.
        Expect(runner, "T045 cut reason is stable until recommit", history,
               rhi::ViewKind::Main, identity, rhi::MotionValidity::Reset);
        runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                             StateAt(1.0)),
                      "T045 render after cut accepted");
        rhi::RenderedViewState previous;
        const rhi::MotionValidity afterCut =
            history.Previous(rhi::ViewKind::Main, identity, &previous);
        runner->Validity("T045 render after cut is valid", afterCut,
                         rhi::MotionValidity::Valid);
        if (afterCut == rhi::MotionValidity::Valid && previous.records.size() == 1) {
            runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 1.0)),
                          "T045 old record is not resurrected after a cut");
        } else {
            runner->Check(false, "T045 record after cut");
        }
    }

    // VAL-08: teleport is the same reset family. It must not reuse the pose
    // captured before the move.
    {
        rhi::RenderedViewHistory history;
        runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                             StateAt(0.0)),
                      "T045 pre-teleport commit accepted");
        history.InvalidateAll(rhi::MotionValidity::Reset);
        rhi::RenderedViewState previous;
        previous.records.push_back(Record(P(-1.0, -1.0, -1.0)));
        const rhi::MotionValidity teleported =
            history.Previous(rhi::ViewKind::Main, identity, &previous);
        runner->Validity("T045 teleport invalidates", teleported,
                         rhi::MotionValidity::Reset);
        runner->Check(previous.records.size() == 1 &&
                          SamePoint(previous.records[0].point,
                                    P(-1.0, -1.0, -1.0)),
                      "T045 teleport leaves the output untouched");
    }

    // Level load: a different world identity is a reset even without an
    // explicit invalidation call.
    {
        rhi::RenderedViewHistory history;
        runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                             StateAt(0.5)),
                      "T045 pre-level commit accepted");
        rhi::ViewIdentity nextLevel = identity;
        nextLevel.worldId = identity.worldId + 1;
        rhi::RenderedViewState previous;
        previous.records.push_back(Record(P(-2.0, -2.0, -2.0)));
        const rhi::MotionValidity loaded =
            history.Previous(rhi::ViewKind::Main, nextLevel, &previous);
        runner->Validity("T045 level load invalidates", loaded,
                         rhi::MotionValidity::Reset);
        runner->Check(previous.records.size() == 1 &&
                          SamePoint(previous.records[0].point,
                                    P(-2.0, -2.0, -2.0)),
                      "T045 level load leaves the output untouched");

        runner->Check(history.CommitRendered(rhi::ViewKind::Main, nextLevel,
                                             StateAt(0.25)),
                      "T045 new-level commit accepted");
        Expect(runner, "T045 new-level history is valid", history,
               rhi::ViewKind::Main, nextLevel, rhi::MotionValidity::Valid);
        Expect(runner, "T045 new level does not answer the old world", history,
               rhi::ViewKind::Main, identity, rhi::MotionValidity::Reset);
    }

    // A cut on a view that was never rendered is NoHistory, not Reset: there
    // is nothing to invalidate and no state to fabricate.
    {
        rhi::RenderedViewHistory history;
        history.InvalidateAll(rhi::MotionValidity::Reset);
        Expect(runner, "T045 reset on an unrendered view is no-history",
               history, rhi::ViewKind::Main, identity,
               rhi::MotionValidity::NoHistory);
    }
}

void RunResizeFamily(Runner* runner) {
    const rhi::ViewIdentity identity = BaseIdentity();
    rhi::RenderedViewHistory history;
    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.5)),
                  "T045 pre-resize commit accepted");

    // VAL-09: an extent change is a resize, whether or not the engine called
    // Invalidate.
    rhi::ViewIdentity smaller = identity;
    smaller.width = 640;
    smaller.height = 360;
    Expect(runner, "T045 extent change invalidates", history,
           rhi::ViewKind::Main, smaller, rhi::MotionValidity::Resize);

    history.Invalidate(rhi::ViewKind::Main, identity,
                       rhi::MotionValidity::Resize);
    Expect(runner, "T045 explicit resize invalidates", history,
           rhi::ViewKind::Main, identity, rhi::MotionValidity::Resize);

    runner->Check(history.CommitRendered(rhi::ViewKind::Main, smaller,
                                         StateAt(0.5)),
                  "T045 render at the new extent accepted");
    Expect(runner, "T045 history after resize is valid", history,
           rhi::ViewKind::Main, smaller, rhi::MotionValidity::Valid);
    Expect(runner, "T045 the old extent is not answered", history,
           rhi::ViewKind::Main, identity, rhi::MotionValidity::Resize);
}

void RunGeometryFamily(Runner* runner) {
    const rhi::ViewIdentity identity = BaseIdentity();

    // VAL-05: a topology/identity change invalidates.
    {
        rhi::RenderedViewHistory history;
        runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                             StateAt(0.5)),
                      "T045 pre-topology commit accepted");
        rhi::ViewIdentity newTopology = identity;
        newTopology.topology = identity.topology + 1;
        Expect(runner, "T045 topology change invalidates", history,
               rhi::ViewKind::Main, newTopology, rhi::MotionValidity::Topology);

        history.Invalidate(rhi::ViewKind::Main, identity,
                           rhi::MotionValidity::Topology);
        Expect(runner, "T045 explicit topology invalidates", history,
               rhi::ViewKind::Main, identity, rhi::MotionValidity::Topology);
    }

    // VAL-10: entity slot reuse invalidates the record that used the slot.
    {
        rhi::RenderedViewHistory history;
        runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                             StateAt(0.5)),
                      "T045 pre-reuse commit accepted");
        rhi::ViewIdentity reused = identity;
        reused.entity = identity.entity + 1;
        Expect(runner, "T045 entity reuse invalidates", history,
               rhi::ViewKind::Main, reused, rhi::MotionValidity::EntityReuse);

        history.Invalidate(rhi::ViewKind::Main, identity,
                           rhi::MotionValidity::EntityReuse);
        Expect(runner, "T045 explicit entity reuse invalidates", history,
               rhi::ViewKind::Main, identity,
               rhi::MotionValidity::EntityReuse);

        runner->Check(history.CommitRendered(rhi::ViewKind::Main, reused,
                                             StateAt(0.75)),
                      "T045 recommit after reuse accepted");
        rhi::RenderedViewState previous;
        const rhi::MotionValidity recovered =
            history.Previous(rhi::ViewKind::Main, reused, &previous);
        runner->Validity("T045 history after reuse is valid", recovered,
                         rhi::MotionValidity::Valid);
        if (recovered == rhi::MotionValidity::Valid &&
            previous.records.size() == 1) {
            runner->Check(SamePoint(previous.records[0].point, P(100.0, 0.0, 0.75)),
                          "T045 reuse does not return the old slot's pose");
        } else {
            runner->Check(false, "T045 record after reuse");
        }
    }
}

void RunProviderAndPrecedence(Runner* runner) {
    const rhi::ViewIdentity identity = BaseIdentity();

    // VAL-06: a provider/renderer switch invalidates every view.
    {
        rhi::RenderedViewHistory history;
        CommitAllViews(runner, &history, identity);
        history.InvalidateAll(rhi::MotionValidity::ProviderSwitch);
        Expect(runner, "T045 provider switch invalidates main", history,
               rhi::ViewKind::Main, identity,
               rhi::MotionValidity::ProviderSwitch);
        Expect(runner, "T045 provider switch invalidates mirror", history,
               rhi::ViewKind::Mirror, identity,
               rhi::MotionValidity::ProviderSwitch);
        Expect(runner, "T045 provider switch invalidates camera", history,
               rhi::ViewKind::Camera, identity,
               rhi::MotionValidity::ProviderSwitch);
        Expect(runner, "T045 provider switch invalidates shadow", history,
               rhi::ViewKind::Shadow, identity,
               rhi::MotionValidity::ProviderSwitch);

        rhi::ViewIdentity switched = identity;
        switched.provider = identity.provider + 1;
        Expect(runner, "T045 provider generation change invalidates", history,
               rhi::ViewKind::Main, switched,
               rhi::MotionValidity::ProviderSwitch);
    }

    // Documented precedence, observed one step at a time.
    rhi::RenderedViewHistory history;
    runner->Check(history.CommitRendered(rhi::ViewKind::Main, identity,
                                         StateAt(0.5)),
                  "T045 precedence commit accepted");

    rhi::ViewIdentity all = identity;
    all.provider += 1;
    all.worldId += 1;
    all.width = 640;
    all.height = 360;
    all.topology += 1;
    all.entity += 1;
    all.generation += 1;
    Expect(runner, "T045 precedence: provider switch wins", history,
           rhi::ViewKind::Main, all, rhi::MotionValidity::ProviderSwitch);

    rhi::ViewIdentity worldThenRest = all;
    worldThenRest.provider = identity.provider;
    Expect(runner, "T045 precedence: reset before resize", history,
           rhi::ViewKind::Main, worldThenRest, rhi::MotionValidity::Reset);

    rhi::ViewIdentity resizeThenRest = worldThenRest;
    resizeThenRest.worldId = identity.worldId;
    Expect(runner, "T045 precedence: resize before topology", history,
           rhi::ViewKind::Main, resizeThenRest, rhi::MotionValidity::Resize);

    rhi::ViewIdentity topologyThenRest = resizeThenRest;
    topologyThenRest.width = identity.width;
    topologyThenRest.height = identity.height;
    Expect(runner, "T045 precedence: topology before entity", history,
           rhi::ViewKind::Main, topologyThenRest,
           rhi::MotionValidity::Topology);

    rhi::ViewIdentity entityThenGeneration = topologyThenRest;
    entityThenGeneration.topology = identity.topology;
    Expect(runner, "T045 precedence: entity reuse before generation", history,
           rhi::ViewKind::Main, entityThenGeneration,
           rhi::MotionValidity::EntityReuse);

    rhi::ViewIdentity generationOnly = entityThenGeneration;
    generationOnly.entity = identity.entity;
    Expect(runner, "T045 precedence: generation last", history,
           rhi::ViewKind::Main, generationOnly,
           rhi::MotionValidity::Generation);

    Expect(runner, "T045 precedence: identical identity is valid", history,
           rhi::ViewKind::Main, identity, rhi::MotionValidity::Valid);

    // An unrecognized reason is a no-op, not a silent reset.
    history.Invalidate(rhi::ViewKind::Main, identity, rhi::MotionValidity::Valid);
    Expect(runner, "T045 valid reason is a no-op", history,
           rhi::ViewKind::Main, identity, rhi::MotionValidity::Valid);
}

}  // namespace

int main() {
    Runner runner;
    RunResetFamily(&runner);
    RunResizeFamily(&runner);
    RunGeometryFamily(&runner);
    RunProviderAndPrecedence(&runner);
    return runner.Finish("q3.history_reset");
}
