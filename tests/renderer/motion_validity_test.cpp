// T046 consumer test: correspondence/rejection policy and the numeric
// viewmodel/deformation/particle rows (motion-fixtures.md section 6 and rows
// DEF-01, VM-01/02).
//
// A rejected input never yields a usable vector, and a valid zero stays valid.

#include "motion_test_support.h"

#include <string>

namespace {

using motion_test::CameraAt;
using motion_test::P;
using motion_test::Record;
using motion_test::Runner;
namespace rhi = doom3::rhi;

void ExpectCorrespondence(Runner* runner, const std::string& label,
                          rhi::SurfaceClass surface,
                          const rhi::CorrespondenceDesc& desc,
                          rhi::MotionValidity expected) {
    const rhi::CorrespondenceResult result =
        rhi::EvaluateCorrespondence(surface, desc);
    runner->Validity(label, result.validity, expected);
    runner->Check(result.canUseVector ==
                      (expected == rhi::MotionValidity::Valid),
                  label + " vector usability");
}

rhi::CorrespondenceDesc GoodDesc() { return rhi::CorrespondenceDesc{}; }

void RunValidityMatrix(Runner* runner) {
    const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);

    // VAL-01: a stationary surface is VALID with (0,0), not "no history".
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(100.0, 0.0, 0.0)), camera,
            Record(P(100.0, 0.0, 0.0)));
        runner->Validity("VAL-01 validity", sample.validity,
                         rhi::MotionValidity::Valid);
        runner->Check(sample.HasVector(), "VAL-01 zero is usable");
        runner->Near("VAL-01 du", sample.vector.du, 0.0, 1.00e-07);
        runner->Near("VAL-01 dv", sample.vector.dv, 0.0, 1.00e-07);
        ExpectCorrespondence(runner, "VAL-01 policy", rhi::SurfaceClass::Static,
                             GoodDesc(), rhi::MotionValidity::Valid);
    }

    // VAL-02: no previous rendered record.
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(100.0, 0.0, 0.0), false), camera,
            Record(P(100.0, 0.0, 0.0)));
        runner->Validity("VAL-02 validity", sample.validity,
                         rhi::MotionValidity::NoHistory);
        runner->Check(!sample.HasVector(), "VAL-02 no usable vector");
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.hasPreviousRenderedRecord = false;
        ExpectCorrespondence(runner, "VAL-02 policy",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::NoHistory);
    }

    // VAL-03: previous UV outside [0,1].
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(100.0, 300.0, 0.0)), camera,
            Record(P(100.0, 0.0, 0.0)));
        runner->Validity("VAL-03 validity", sample.validity,
                         rhi::MotionValidity::Disocclusion);
        runner->Check(!sample.HasVector(), "VAL-03 no usable vector");
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.newPixel = true;
        ExpectCorrespondence(runner, "VAL-03 policy",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::Disocclusion);
    }

    // VAL-04: either depth <= 0.
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(-10.0, 0.0, 0.0)), camera,
            Record(P(-10.0, 0.0, 0.0)));
        runner->Validity("VAL-04 validity", sample.validity,
                         rhi::MotionValidity::BehindCamera);
        runner->Check(!sample.HasVector(), "VAL-04 no usable vector");
    }

    // VAL-05: topology or identity change.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.sameTopology = false;
        ExpectCorrespondence(runner, "VAL-05 topology",
                             rhi::SurfaceClass::Skinned, desc,
                             rhi::MotionValidity::Topology);
        rhi::CorrespondenceDesc identity = GoodDesc();
        identity.sameSurfaceIdentity = false;
        ExpectCorrespondence(runner, "VAL-05 identity",
                             rhi::SurfaceClass::Static, identity,
                             rhi::MotionValidity::Topology);
    }

    // VAL-06: provider switch.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.providerSwitched = true;
        ExpectCorrespondence(runner, "VAL-06 policy",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::ProviderSwitch);
    }

    // VAL-07: history generation mismatch.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.generationMismatch = true;
        ExpectCorrespondence(runner, "VAL-07 policy",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::Generation);
    }

    // VAL-08..10 are owned by T045 at the engine-event level; here they guard
    // the policy precedence only.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.resetOccurred = true;
        ExpectCorrespondence(runner, "VAL-08 precedence",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::Reset);
        desc = GoodDesc();
        desc.resized = true;
        ExpectCorrespondence(runner, "VAL-09 precedence",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::Resize);
        desc = GoodDesc();
        desc.entitySlotReused = true;
        ExpectCorrespondence(runner, "VAL-10 precedence",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::EntityReuse);
    }

    // Precedence: a provider switch outranks a reset; no history outranks a
    // stale generation.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.providerSwitched = true;
        desc.resetOccurred = true;
        ExpectCorrespondence(runner, "VAL precedence provider",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::ProviderSwitch);
        desc = GoodDesc();
        desc.hasPreviousRenderedRecord = false;
        desc.generationMismatch = true;
        ExpectCorrespondence(runner, "VAL precedence no history",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::NoHistory);
    }
}

void RunNumericPolicyRows(Runner* runner) {
    const rhi::CameraState camera = CameraAt(0.0, 0.0, 0.0);

    // DEF-01: same vertex identity with changed positions.
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(100.0, 0.0, 0.3)), camera,
            Record(P(100.0, 0.0, -0.2)));
        runner->Validity("DEF-01 validity", sample.validity,
                         rhi::MotionValidity::Valid);
        runner->Near("DEF-01 du", sample.vector.du, 0.0, 1.00e-07);
        runner->Near("DEF-01 dv", sample.vector.dv, -0.003333333, 1.91e-06);
        runner->HalfWord("DEF-01 dv word", sample.vector.dv, 0x9ad4);
        ExpectCorrespondence(runner, "DEF-01 policy",
                             rhi::SurfaceClass::Deformation, GoodDesc(),
                             rhi::MotionValidity::Valid);
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.sameTopology = false;
        ExpectCorrespondence(runner, "DEF-01 topology",
                             rhi::SurfaceClass::Deformation, desc,
                             rhi::MotionValidity::Topology);
    }

    // VM-01/02: weaponDepthHack changes z only; x/y history uses the same
    // camera basis at the viewmodel's own depth.
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(20.0, 0.0, 0.1)), camera,
            Record(P(20.0, 0.0, 0.0)));
        runner->Validity("VM-01 validity", sample.validity,
                         rhi::MotionValidity::Valid);
        runner->Near("VM-01 du", sample.vector.du, 0.0, 1.00e-07);
        runner->Near("VM-01 dv", sample.vector.dv, -0.003333333, 1.91e-06);
        runner->HalfWord("VM-01 dv word", sample.vector.dv, 0x9ad4);
        ExpectCorrespondence(runner, "VM-01 policy",
                             rhi::SurfaceClass::Viewmodel, GoodDesc(),
                             rhi::MotionValidity::Valid);
    }
    {
        const rhi::MotionSample sample = rhi::ReprojectPoint(
            camera, Record(P(20.0, 0.0, 0.0)), camera,
            Record(P(20.0, 0.0, 0.0)));
        runner->Validity("VM-02 validity", sample.validity,
                         rhi::MotionValidity::Valid);
        runner->Near("VM-02 du", sample.vector.du, 0.0, 1.00e-07);
        runner->Near("VM-02 dv", sample.vector.dv, 0.0, 1.00e-07);
        runner->HalfWord("VM-02 dv word", sample.vector.dv, 0x0000);
    }

    // POL-01: a particle uses its previous rendered position, not the emitter
    // origin. The two produce opposite signs, so an origin-derived history is
    // detected even though both are small.
    {
        const rhi::MotionSample previousPosition = rhi::ReprojectPoint(
            camera, Record(P(100.0, 0.0, 1.0)), camera,
            Record(P(100.0, 0.0, 0.5)));
        runner->Near("POL-01 previous position dv",
                     previousPosition.vector.dv, -0.003333333, 1.91e-06);
        const rhi::MotionSample emitterOrigin = rhi::ReprojectPoint(
            camera, Record(P(100.0, 0.0, 0.0)), camera,
            Record(P(100.0, 0.0, 0.5)));
        runner->Near("POL-01 emitter origin dv", emitterOrigin.vector.dv,
                     0.003333333, 1.91e-06);
        runner->Check(
            std::fabs(previousPosition.vector.dv - emitterOrigin.vector.dv) >
                0.006,
            "POL-01 origin and previous position differ");

        rhi::CorrespondenceDesc desc = GoodDesc();
        ExpectCorrespondence(runner, "POL-01 particle",
                             rhi::SurfaceClass::Particle, desc,
                             rhi::MotionValidity::Valid);
        desc.respawned = true;
        ExpectCorrespondence(runner, "POL-01 respawn",
                             rhi::SurfaceClass::Particle, desc,
                             rhi::MotionValidity::EntityReuse);
        desc.hasPreviousRenderedRecord = false;
        ExpectCorrespondence(runner, "POL-01 respawn without history",
                             rhi::SurfaceClass::Particle, desc,
                             rhi::MotionValidity::NoHistory);
    }

    // POL-02: transparency does not change validity; identity and topology do.
    {
        ExpectCorrespondence(runner, "POL-02 transparent",
                             rhi::SurfaceClass::Transparent, GoodDesc(),
                             rhi::MotionValidity::Valid);
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.sameSurfaceIdentity = false;
        ExpectCorrespondence(runner, "POL-02 identity",
                             rhi::SurfaceClass::Transparent, desc,
                             rhi::MotionValidity::Topology);
    }

    // POL-03: a reflection/subview never supplies the main-view vector.
    {
        ExpectCorrespondence(runner, "POL-03 reflection",
                             rhi::SurfaceClass::Reflection, GoodDesc(),
                             rhi::MotionValidity::ProviderSwitch);
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.subview = true;
        ExpectCorrespondence(runner, "POL-03 subview",
                             rhi::SurfaceClass::Static, desc,
                             rhi::MotionValidity::ProviderSwitch);
    }

    // POL-04: decal/overlay keeps surface identity; a projection or topology
    // change invalidates.
    {
        ExpectCorrespondence(runner, "POL-04 decal",
                             rhi::SurfaceClass::Decal, GoodDesc(),
                             rhi::MotionValidity::Valid);
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.sameTopology = false;
        ExpectCorrespondence(runner, "POL-04 decal topology",
                             rhi::SurfaceClass::Decal, desc,
                             rhi::MotionValidity::Topology);
    }

    // POL-05: supported deformation is valid; topology change is not.
    {
        ExpectCorrespondence(runner, "POL-05 deformation",
                             rhi::SurfaceClass::Deformation, GoodDesc(),
                             rhi::MotionValidity::Valid);
    }

    // POL-06: newly visible pixel.
    {
        rhi::CorrespondenceDesc desc = GoodDesc();
        desc.newPixel = true;
        ExpectCorrespondence(runner, "POL-06 newly visible",
                             rhi::SurfaceClass::Transparent, desc,
                             rhi::MotionValidity::Disocclusion);
    }
}

}  // namespace

int main() {
    Runner runner;
    RunValidityMatrix(&runner);
    RunNumericPolicyRows(&runner);
    return runner.Finish("q3-motion-validity");
}
