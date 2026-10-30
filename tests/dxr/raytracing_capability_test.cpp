// Q6 explicit mode selection (spec 004 T064).
//
// The checks are the documented rules, not the implementation's current branch
// order: an unavailable device is Unsupported and never silently becomes a
// pass; a failed showcase gate keeps the hybrid renderer available but requires
// an explicit fallback approval; a capable device with every gate is Showcase.

#include "dxr_test_support.h"

#include "rhi/raytracing_capability.h"

#include <string>

using doom3::rhi::PlanRaytracingSelection;
using doom3::rhi::QualityGate;
using doom3::rhi::RaytracingCapability;
using doom3::rhi::RaytracingRequest;
using doom3::rhi::RaytracingSelection;
using doom3::rhi::RaytracingSelectionDesc;
using doom3::rhi::RaytracingTier;
using doom3::rhi::Status;

namespace {

RaytracingCapability CapableDevice() {
    RaytracingCapability capability;
    capability.deviceSupportsRaytracing = true;
    capability.tier = RaytracingTier::Tier11;
    capability.shaderModelMajor = 6;
    capability.shaderModelMinor = 5;
    return capability;
}

RaytracingCapability UnsupportedDevice() {
    return RaytracingCapability();
}

QualityGate PassedGates() {
    QualityGate gates;
    gates.visualGatePassed = true;
    gates.performanceGatePassed = true;
    gates.motionGatePassed = true;
    return gates;
}

bool HasLimitation(const RaytracingSelection& selection, const std::string& needle) {
    for (const std::string& limitation : selection.limitations) {
        if (limitation.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Hybrid;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok, "hybrid request is available on an unsupported device");
        runner.Check(selection.effective == RaytracingRequest::Hybrid,
                     "hybrid request keeps the hybrid mode");
        runner.Check(!selection.pathTracingAvailable,
                     "hybrid request does not claim path tracing");
        runner.Check(selection.fallbackAvailable,
                     "hybrid renderer is reported available");
        runner.Check(selection.limitations.empty(),
                     "a hybrid request has no limitation");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Reference;
        desc.capability = CapableDevice();
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok, "reference request on a capable device is Ok");
        runner.Check(selection.effective == RaytracingRequest::Reference,
                     "reference request selects the reference mode");
        runner.Check(selection.pathTracingAvailable,
                     "capable device reports path tracing available");
        runner.Check(!selection.fallbackApproved,
                     "no fallback is approved when the request succeeds");
        runner.Check(selection.limitations.empty(),
                     "a successful reference plan has no limitation");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Reference;
        desc.capability = UnsupportedDevice();
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Unsupported,
                     "reference request on an unsupported device is Unsupported");
        runner.Check(selection.fallbackAvailable,
                     "the hybrid renderer remains available after an unsupported request");
        runner.Check(!selection.fallbackApproved,
                     "the plan does not approve a fallback by itself");
        runner.Check(HasLimitation(selection, "ray-tracing support"),
                     "the unsupported device limitation is recorded");
        runner.Check(!error.empty(), "an Unsupported plan reports an actionable error");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Reference;
        desc.capability = UnsupportedDevice();
        desc.allowFallback = true;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok,
                     "an explicit fallback approval makes the plan runnable");
        runner.Check(selection.effective == RaytracingRequest::Hybrid,
                     "an approved fallback selects the hybrid mode");
        runner.Check(selection.fallbackApproved,
                     "an approved fallback is recorded");
        runner.Check(HasLimitation(selection, "ray-tracing support"),
                     "an approved fallback keeps the limitation visible");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Reference;
        desc.capability = CapableDevice();
        desc.capability.shaderModelMinor = 2;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Unsupported,
                     "shader model 6.2 is refused for ray tracing");
        runner.Check(HasLimitation(selection, "6.3"),
                     "the shader-model requirement is named");
    }

    {
        QualityGate gates;
        gates.visualGatePassed = true;
        gates.motionGatePassed = true;
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Showcase;
        desc.capability = CapableDevice();
        desc.gates = gates;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Unsupported,
                     "a failed performance gate keeps the showcase Unsupported");
        runner.Check(HasLimitation(selection, "performance gate"),
                     "the failed performance gate is named");
        runner.Check(!selection.fallbackApproved,
                     "a failed gate does not approve a fallback");
    }

    {
        QualityGate gates;
        gates.visualGatePassed = true;
        gates.motionGatePassed = true;
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Showcase;
        desc.capability = CapableDevice();
        desc.gates = gates;
        desc.allowFallback = true;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok,
                     "an approved fallback runs after a failed showcase gate");
        runner.Check(selection.effective == RaytracingRequest::Hybrid,
                     "the approved fallback is the hybrid mode");
        runner.Check(selection.fallbackApproved,
                     "the approved fallback is recorded for the failed gate");
        runner.Check(HasLimitation(selection, "performance gate"),
                     "the gate limitation survives an approved fallback");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Showcase;
        desc.capability = CapableDevice();
        desc.gates = PassedGates();
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok, "a showcase with every gate is Ok");
        runner.Check(selection.effective == RaytracingRequest::Showcase,
                     "a gated showcase selects the showcase mode");
        runner.Check(selection.pathTracingAvailable,
                     "the gated showcase reports path tracing");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Showcase;
        desc.capability = CapableDevice();
        desc.gates = PassedGates();
        desc.allowFallback = true;
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::Ok && selection.effective == RaytracingRequest::Showcase,
                     "fallback approval does not override a passing showcase selection");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Reference;
        desc.capability = CapableDevice();
        RaytracingSelection selection;
        std::string error;
        PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(selection.effective != RaytracingRequest::Hybrid,
                     "a capable device never silently selects the hybrid mode");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = static_cast<RaytracingRequest>(42);
        RaytracingSelection selection;
        std::string error;
        const Status status = PlanRaytracingSelection(desc, &selection, &error);
        runner.Check(status == Status::InvalidArgument,
                     "an unknown request is an InvalidArgument");
    }

    {
        RaytracingSelectionDesc desc;
        desc.requested = RaytracingRequest::Hybrid;
        const Status status = PlanRaytracingSelection(desc, nullptr, nullptr);
        runner.Check(status == Status::InvalidArgument,
                     "a null selection is an InvalidArgument");
    }

    runner.Check(std::string(doom3::rhi::ToString(RaytracingRequest::Reference)) == "Reference",
                 "request names are stable");
    runner.Check(std::string(doom3::rhi::ToString(RaytracingTier::Tier11)) == "Tier11",
                 "tier names are stable");

    return runner.Finish("q6_raytracing_capability_test");
}
