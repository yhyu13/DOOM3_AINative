// Explicit ray-tracing mode selection (spec 004 T064).
//
// The plan is a pure function of the caller's capability record, the quality
// gates and an explicit fallback approval. It performs no device query and
// never turns a missing capability into a pass.

#include "rhi/raytracing_capability.h"

namespace doom3 {
namespace rhi {

namespace {

bool SupportsShaderModel(const RaytracingCapability& capability) {
    if (capability.shaderModelMajor > 6) {
        return true;
    }
    return capability.shaderModelMajor == 6 && capability.shaderModelMinor >= 3;
}

std::string ShaderModelText(const RaytracingCapability& capability) {
    return std::to_string(capability.shaderModelMajor) + "." +
           std::to_string(capability.shaderModelMinor);
}

}  // namespace

const char* ToString(RaytracingTier value) {
    switch (value) {
        case RaytracingTier::Unsupported: return "Unsupported";
        case RaytracingTier::Tier10: return "Tier10";
        case RaytracingTier::Tier11: return "Tier11";
        case RaytracingTier::Tier12: return "Tier12";
    }
    return "Unknown";
}

const char* ToString(RaytracingRequest value) {
    switch (value) {
        case RaytracingRequest::Hybrid: return "Hybrid";
        case RaytracingRequest::Reference: return "Reference";
        case RaytracingRequest::Showcase: return "Showcase";
    }
    return "Unknown";
}

Status PlanRaytracingSelection(const RaytracingSelectionDesc& desc,
                               RaytracingSelection* selection, std::string* error) {
    if (selection == nullptr) {
        if (error != nullptr) {
            *error = "PlanRaytracingSelection needs an output plan";
        }
        return Status::InvalidArgument;
    }
    *selection = RaytracingSelection();
    selection->requested = desc.requested;
    selection->fallbackAvailable = true;

    if (desc.requested != RaytracingRequest::Hybrid &&
        desc.requested != RaytracingRequest::Reference &&
        desc.requested != RaytracingRequest::Showcase) {
        selection->status = Status::InvalidArgument;
        selection->error = "unknown ray-tracing request";
        if (error != nullptr) {
            *error = selection->error;
        }
        return Status::InvalidArgument;
    }

    const bool capable = desc.capability.deviceSupportsRaytracing &&
                         desc.capability.tier != RaytracingTier::Unsupported &&
                         SupportsShaderModel(desc.capability);
    selection->pathTracingAvailable = capable;

    if (desc.requested == RaytracingRequest::Hybrid) {
        selection->effective = RaytracingRequest::Hybrid;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    if (!desc.capability.deviceSupportsRaytracing) {
        selection->limitations.push_back("device does not report ray-tracing support");
    } else if (desc.capability.tier == RaytracingTier::Unsupported) {
        selection->limitations.push_back("ray-tracing tier is unsupported");
    }
    if (!SupportsShaderModel(desc.capability)) {
        selection->limitations.push_back("shader model 6.3 or newer is required; backend reported " +
                                         ShaderModelText(desc.capability));
    }
    if (desc.requested == RaytracingRequest::Showcase) {
        if (!desc.gates.motionGatePassed) {
            selection->limitations.push_back("motion gate has not passed");
        }
        if (!desc.gates.visualGatePassed) {
            selection->limitations.push_back("visual gate has not passed");
        }
        if (!desc.gates.performanceGatePassed) {
            selection->limitations.push_back("performance gate has not passed");
        }
    }

    if (selection->limitations.empty()) {
        selection->effective = desc.requested;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    selection->effective = RaytracingRequest::Hybrid;
    if (desc.allowFallback) {
        selection->fallbackApproved = true;
        selection->status = Status::Ok;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    selection->fallbackApproved = false;
    selection->status = Status::Unsupported;
    selection->error = std::string("requested mode ") + ToString(desc.requested) +
                       " cannot run; the hybrid renderer is available only with explicit "
                       "fallback approval";
    if (error != nullptr) {
        *error = selection->error;
    }
    return Status::Unsupported;
}

}  // namespace rhi
}  // namespace doom3
